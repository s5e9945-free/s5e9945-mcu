#include "dm_internal.h"
#include <string.h>

static uint8_t byte_at(uint32_t word, unsigned shift)
{
    return (uint8_t)(word >> shift);
}

static struct dm_constraint *selected_registered(struct dm_state *state,
                                                  uint8_t index);

static void desc_write_word(struct dm_domain_desc *desc, uint32_t offset,
                            uint32_t value)
{
    /* Some confirmed words live inside as-yet-unnamed descriptor regions. */
    memcpy((uint8_t *)desc + offset, &value, sizeof(value));
}

static uint32_t desc_read_word(const struct dm_domain_desc *desc,
                               uint32_t offset)
{
    uint32_t value;
    memcpy(&value, (const uint8_t *)desc + offset, sizeof(value));
    return value;
}

/* Local 0x8dc8 calls 0x89cc. The four IPC words supply the domain and
 * three values; all destination offsets below are observed stores. */
static int cmd00_write_fields(struct dm_state *state,
                              const struct dm_ipc_words *request)
{
    const uint32_t index = dm_ipc_domain(request);
    struct dm_domain_desc *d;
    uint32_t off;
    if (index >= DM_DOMAIN_COUNT)
        return DM_INVALID;
    d = &state->config.domain[index];
    desc_write_word(d, 0x24u, request->word[3]);
    for (off = 0x2cu; off <= 0x38u; off += 4u)
        desc_write_word(d, off, request->word[1]);
    for (off = 0x40u; off <= 0x4cu; off += 4u)
        desc_write_word(d, off, request->word[1]);
    for (off = 0x50u; off <= 0x5cu; off += 4u)
        desc_write_word(d, off, request->word[2]);
    desc_write_word(d, 0x3cu, request->word[1]);
    d->field60 = request->word[1];
    d->field64 = request->word[2];
    desc_write_word(d, 0x2cu, request->word[3]);
    desc_write_word(d, 0x30u, request->word[3]);
    return DM_OK;
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

static int cmd0e_select_table(struct dm_state *state,
                              const struct dm_ipc_words *request)
{
    const uint32_t index = dm_ipc_domain(request);
    struct dm_constraint *c = selected_registered(state, (uint8_t)index);
    m55_addr_t *tables;
    const uint32_t selected = request->word[1];
    if (!c)
        return DM_INVALID;
    if (!c->flag1a || selected >= c->outer_count)
        return DM_OK;
    tables = dm_arena_ptr(state, c->tables, c->outer_count * 4u);
    if (!tables)
        return DM_INVALID;
    c->selected_index = selected;
    c->selected_table = tables[selected];
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

static int cmd08_serialize(struct dm_state *state,
                           const struct dm_ipc_words *request,
                           struct dm_ipc_words *response)
{
    const uint32_t index = dm_ipc_domain(request);
    const struct dm_domain_desc *d;
    if (index >= DM_DOMAIN_COUNT)
        return DM_INVALID;
    d = &state->config.domain[index];
    /* 0x922a..0x9258, all three fields pass through divide-by-1000. */
    response->word[1] = ((desc_read_word(d, 0x2cu) / 1000u) & 0xffffu) << 16 |
                        ((desc_read_word(d, 0x3cu) / 1000u) & 0xffffu);
    response->word[2] = (response->word[2] & 0xffff0000u) |
                        ((desc_read_word(d, 0x24u) / 1000u) & 0xffffu);
    return DM_OK;
}

static int cmd09_serialize(struct dm_state *state,
                           const struct dm_ipc_words *request,
                           struct dm_ipc_words *response)
{
    /* 0x911c..0x920e: word1 is an index across target-side +0x84/+0x8c
     * lists. The selected constraint's source domain supplies descriptor
     * values; the two constraint words occupy the other response halves. */
    struct dm_constraint *c = dm_constraint_in_target_list(state,
                                 dm_ipc_domain(request), request->word[1]);
    const struct dm_domain_desc *d;
    uint32_t flags;
    if (!c || c->domain0 >= DM_DOMAIN_COUNT)
        return DM_INVALID;
    d = &state->config.domain[c->domain0];
    flags = ((c->flag18 & 1u) << 6) | ((c->flag28 & 1u) << 7);
    response->word[0] = (response->word[0] & 0xffff00ffu) |
                        (((c->domain0 & 0x3fu) | flags) << 8);
    response->word[1] = ((desc_read_word(d, 0x3cu) / 1000u) << 16) |
                        ((c->field44 / 1000u) & 0xffffu);
    response->word[2] = ((c->field48 / 1000u) << 16) |
                        ((desc_read_word(d, 0x2cu) / 1000u) & 0xffffu);
    response->word[3] = (((c->flag28 ? d->field64 : d->field60) / 1000u) << 16) |
                        (((c->flag28 ? d->field50 : d->field40) / 1000u) & 0xffffu);
    return DM_OK;
}

static struct dm_constraint *selected_registered(struct dm_state *state,
                                                  uint8_t index)
{
    if (index >= state->count)
        return NULL;
    /* CMD0E addresses the registration array directly. */
    return dm_constraint_at(state, state->registered[index]);
}

static int cmd0b_metadata(struct dm_state *state,
                          const struct dm_ipc_words *request,
                          struct dm_ipc_words *response)
{
    /* 0x8fa8..0x8fc8: word1 byte1 high nibble selects list entry. */
    struct dm_constraint *c = dm_constraint_in_domain_list(state,
                                    dm_ipc_domain(request),
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
    struct dm_constraint *c = dm_constraint_in_domain_list(state,
                                    dm_ipc_domain(request), byte_at(indices, 0));
    struct dm_pair *pair = dm_pair_at(state, c, byte_at(indices, 8),
                                      byte_at(indices, 16));
    if (!pair)
        return DM_INVALID;
    /* 0x9106..0x9116 writes the pair to response words 2 and 3. */
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
    case 0x00: return cmd00_write_fields(state, request);
    case 0x01: return cmd01_create(state, request);
    case 0x02: return cmd02_write_pair(state, request);
    case 0x03: return cmd03_commit(state, request, response);
    case 0x04: /* 0x8f70 calls unresolved helper 0x8544. */ return DM_UNKNOWN;
    case 0x05: /* 0x8f7a calls unresolved helper 0x886c. */ return DM_UNKNOWN;
    case 0x06: /* TBH goes directly to common response path 0x8dce. */
        return DM_OK;
    case 0x07: return cmd07_serialize(state, request, response);
    case 0x08: return cmd08_serialize(state, request, response);
    case 0x09: return cmd09_serialize(state, request, response);
    case 0x0a: /* 0x8e02..0x8e0c: descriptor +0x6c to word1. */
        if (dm_ipc_domain(request) >= DM_DOMAIN_COUNT)
            return DM_INVALID;
        response->word[1] = state->config.domain[dm_ipc_domain(request)].unknown6c;
        return DM_OK;
    case 0x0b: return cmd0b_metadata(state, request, response);
    case 0x0c: return cmd0c_read_pair(state, request, response);
    case 0x0d: /* 0x8f3e..0x8f48: word1 to config +0x10a8. */
        state->config.field10a8 = request->word[1];
        return DM_OK;
    case 0x0e: /* 0x8f4e..0x8f6c: select registered table. */
        return cmd0e_select_table(state, request);
    default: return DM_UNKNOWN;
    }
}
