#include "dm_internal.h"

static uint8_t byte_at(uint32_t word, unsigned shift)
{
    return (uint8_t)(word >> shift);
}

static int cmd01_create(struct dm_state *state,
                        const struct dm_ipc_words *request)
{
    const uint8_t byte1 = byte_at(request->word[1], 8);
    const uint8_t byte2 = byte_at(request->word[1], 16);
    const uint32_t outer = byte1 >> 4;
    const uint32_t selected = byte2 & 0x0fu;
    const uint32_t inner = byte_at(request->word[1], 24);
    m55_addr_t address, tables_address;
    struct dm_constraint *constraint;
    m55_addr_t *tables;
    uint32_t i, needed;

    if (outer == 0u || inner == 0u || selected >= outer)
        return DM_INVALID;
    needed = 0x50u + outer * 4u + outer * inner * 8u;
    if (state->arena_offset > M55_DM_ARENA_SIZE ||
        needed >= M55_DM_ARENA_SIZE - state->arena_offset) {
        (void)dm_arena_alloc(state, needed); /* observed memlack path */
        return DM_MEMLACK;
    }
    address = dm_arena_alloc(state, 0x50u);
    tables_address = dm_arena_alloc(state, outer * 4u);
    constraint = dm_constraint_at(state, address);
    tables = dm_arena_ptr(state, tables_address, outer * 4u);
    if (!constraint || !tables)
        return DM_INVALID;
    /* 0x8e3c and 0x8e4c identify the two source bytes. */
    constraint->domain0 = dm_ipc_domain(request);
    constraint->id_or_domain1 = byte_at(request->word[1], 0);
    constraint->flag28 = byte1 & 1u;
    constraint->flag18 = (byte1 >> 1) & 1u;
    constraint->flag19 = (byte1 >> 2) & 1u;
    constraint->flag1a = (byte1 >> 3) & 1u;
    constraint->outer_count = outer;
    constraint->selected_index = selected;
    constraint->inner_count = inner;
    constraint->tables = tables_address;
    for (i = 0; i < outer; ++i)
        tables[i] = dm_arena_alloc(state, inner * 8u);
    constraint->selected_table = tables[selected];
    state->current_constraint = address;
    return DM_OK;
}

static int cmd02_write_pair(struct dm_state *state,
                            const struct dm_ipc_words *request)
{
    struct dm_constraint *constraint = dm_constraint_at(state,
                                                  state->current_constraint);
    struct dm_pair *pair;
    if (!constraint || constraint->domain0 != dm_ipc_domain(request))
        return DM_INVALID;
    /* 0x927e..0x9294: word1 byte2 high nibble selects table; high byte
     * selects pair. Source values are word2 and word3. */
    pair = dm_pair_at(state, constraint,
                      (byte_at(request->word[1], 16) >> 4),
                      byte_at(request->word[1], 24));
    if (!pair)
        return DM_INVALID;
    pair->value0 = request->word[2];
    pair->value1 = request->word[3];
    return DM_OK;
}

static int cmd03_commit(struct dm_state *state,
                        const struct dm_ipc_words *request,
                        struct dm_ipc_words *response)
{
    struct dm_constraint *constraint = dm_constraint_at(state,
                                                  state->current_constraint);
    int result;
    if (!constraint || constraint->domain0 != dm_ipc_domain(request) ||
        state->count >= DM_MAX_CONSTRAINTS)
        return DM_INVALID;
    result = dm_register_constraint(state, state->current_constraint);
    if (result != DM_OK)
        return result;
    state->registered[state->count] = state->current_constraint;
    response->word[1] = (response->word[1] & ~0xffu) | (state->count & 0xffu);
    ++state->count;
    return DM_OK;
}

static int cmd07_serialize(struct dm_state *state,
                           const struct dm_ipc_words *request,
                           struct dm_ipc_words *response)
{
    const uint32_t index = dm_ipc_domain(request);
    const struct dm_domain_desc *d;
    if (index >= DM_DOMAIN_COUNT)
        return DM_INVALID;
    d = &state->config.domain[index];
    /* 0x8ef8..0x8f3c: field68 is copied low 16 bits; the other four
     * fields use UMULL with 0x10624dd3 and high-word shift 6, equivalent
     * to unsigned division by 1000. */
    response->word[1] = (d->field68 & 0xffffu) |
                        (((d->field40 / 1000u) & 0xffffu) << 16);
    response->word[2] = ((d->field50 / 1000u) & 0xffffu) |
                        (((d->field60 / 1000u) & 0xffffu) << 16);
    response->word[3] = (response->word[3] & 0xffff0000u) |
                        ((d->field64 / 1000u) & 0xffffu);
    return DM_OK;
}

static struct dm_constraint *selected_registered(struct dm_state *state,
                                                  uint8_t index)
{
    if (index >= state->count)
        return NULL;
    /* Proxy for observed +74/+7c traversal. Actual list ordering unknown. */
    return dm_constraint_at(state, state->registered[index]);
}

static int cmd0b_metadata(struct dm_state *state,
                          const struct dm_ipc_words *request,
                          struct dm_ipc_words *response)
{
    /* 0x8fa8..0x8fc8: word1 byte1 high nibble selects list entry. */
    struct dm_constraint *c = selected_registered(state,
                                          byte_at(request->word[1], 8) >> 4);
    uint8_t flags;
    if (!c)
        return DM_INVALID;
    flags = (uint8_t)((c->flag28 & 1u) | ((c->flag18 & 1u) << 1) |
                      ((c->flag19 & 1u) << 2) | ((c->flag1a & 1u) << 3));
    response->word[1] = (response->word[1] & 0x0000f000u) |
                        (c->id_or_domain1 & 0xffu) |
                        ((uint32_t)flags << 8) |
                        ((c->outer_count & 0x0fu) << 16) |
                        ((c->selected_index & 0x0fu) << 20) |
                        ((c->inner_count & 0xffu) << 24);
    response->word[2] = c->field44;
    response->word[3] = c->field48;
    return DM_OK;
}

static int cmd0c_read_pair(struct dm_state *state,
                           const struct dm_ipc_words *request,
                           struct dm_ipc_words *response)
{
    const uint32_t indices = request->word[1];
    struct dm_constraint *c = selected_registered(state,
                                           byte_at(indices, 0));
    struct dm_pair *pair = dm_pair_at(state, c, byte_at(indices, 8),
                                      byte_at(indices, 16));
    if (!pair)
        return DM_INVALID;
    /* Pair byte location is unconfirmed. Host model places it in words 2/3. */
    response->word[2] = pair->value0;
    response->word[3] = pair->value1;
    return DM_OK;
}

/* Working name. Local 0x8d74, runtime 0x70084d74. TBH dispatch covers
 * 0x00..0x0e. Incomplete entries deliberately return DM_UNKNOWN. */
int dm_ipc_handler(struct dm_state *state, const struct dm_ipc_words *request,
                   struct dm_ipc_words *response)
{
    if (!state || !request || !response)
        return DM_INVALID;
    /* 0x8d7c..0x8d9e copies all four request words to a local reply. */
    *response = *request;
    switch (dm_ipc_command(request)) {
    case 0x00: /* DM_CMD_00 */ return DM_UNKNOWN;
    case 0x01: return cmd01_create(state, request);
    case 0x02: return cmd02_write_pair(state, request);
    case 0x03: return cmd03_commit(state, request, response);
    case 0x04: /* DM_CMD_04 */ return DM_UNKNOWN;
    case 0x05: /* DM_CMD_05 */ return DM_UNKNOWN;
    case 0x06: /* DM_CMD_06 */ return DM_UNKNOWN;
    case 0x07: return cmd07_serialize(state, request, response);
    case 0x08: /* DM_CMD_08 */ return DM_UNKNOWN;
    case 0x09: /* DM_CMD_09: other list family */ return DM_UNKNOWN;
    case 0x0a: /* DM_CMD_0A */ return DM_UNKNOWN;
    case 0x0b: return cmd0b_metadata(state, request, response);
    case 0x0c: return cmd0c_read_pair(state, request, response);
    case 0x0d: /* DM_CMD_0D */ return DM_UNKNOWN;
    case 0x0e: /* DM_CMD_0E */ return DM_UNKNOWN;
    default: return DM_UNKNOWN;
    }
}
