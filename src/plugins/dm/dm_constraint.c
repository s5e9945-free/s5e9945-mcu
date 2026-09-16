#include "dm_internal.h"
#include <string.h>

struct dm_constraint *dm_constraint_at(struct dm_state *state, m55_addr_t address)
{
    if (address & 3u)
        return NULL;
    return dm_arena_ptr(state, address, sizeof(struct dm_constraint));
}

struct dm_pair *dm_pair_at(struct dm_state *state,
                           const struct dm_constraint *constraint,
                           uint32_t table_index, uint32_t pair_index)
{
    m55_addr_t *tables;
    if (!constraint || table_index >= constraint->outer_count ||
        pair_index >= constraint->inner_count)
        return NULL;
    tables = dm_arena_ptr(state, constraint->tables,
                          constraint->outer_count * sizeof(*tables));
    if (!tables)
        return NULL;
    return dm_arena_ptr(state, tables[table_index] + pair_index * 8u,
                        sizeof(struct dm_pair));
}

struct list_head *dm_list_at(struct dm_state *state, m55_addr_t address)
{
    uint32_t delta, index, offset;
    struct dm_domain_desc *d;
    if (!state)
        return NULL;
    if (address >= M55_DM_DESCRIPTOR_ADDR &&
        address < M55_DM_DESCRIPTOR_ADDR + DM_DOMAIN_COUNT * 0x94u) {
        delta = address - M55_DM_DESCRIPTOR_ADDR;
        index = delta / 0x94u;
        offset = delta % 0x94u;
        d = &state->config.domain[index];
        switch (offset) {
        case 0x74u: return &d->list74;
        case 0x7cu: return &d->list7c;
        case 0x84u: return &d->list84;
        case 0x8cu: return &d->list8c;
        default: return NULL;
        }
    }
    if (address < M55_DM_ARENA_ADDR + 8u)
        return NULL;
    return dm_arena_ptr(state, address, sizeof(struct list_head));
}

static int list_insert_front(struct dm_state *state, m55_addr_t head_addr,
                             m55_addr_t node_addr)
{
    struct list_head *head = dm_list_at(state, head_addr);
    struct list_head *node = dm_list_at(state, node_addr);
    struct list_head *old_next;
    if (!head || !node)
        return DM_INVALID;
    old_next = dm_list_at(state, head->next);
    if (!old_next)
        return DM_INVALID;
    node->next = head->next;
    node->prev = head_addr;
    old_next->prev = node_addr;
    head->next = node_addr;
    return DM_OK;
}

struct dm_constraint *dm_constraint_in_domain_list(struct dm_state *state,
                                                    uint32_t domain,
                                                    uint32_t index)
{
    static const uint32_t family[2] = {0x74u, 0x7cu};
    uint32_t f, visited = 0;
    if (!state || domain >= DM_DOMAIN_COUNT)
        return NULL;
    for (f = 0; f < 2; ++f) {
        const m55_addr_t head_addr = M55_DM_DESCRIPTOR_ADDR + domain * 0x94u +
                                     family[f];
        const struct list_head *head = dm_list_at(state, head_addr);
        m55_addr_t node_addr;
        if (!head)
            return NULL;
        node_addr = head->next;
        while (node_addr != head_addr && visited < DM_MAX_CONSTRAINTS) {
            struct dm_constraint *c;
            if (node_addr < M55_DM_ARENA_ADDR + 8u)
                return NULL;
            c = dm_constraint_at(state, node_addr - 8u);
            if (!c)
                return NULL;
            if (index == 0u)
                return c;
            --index;
            ++visited;
            node_addr = c->node0.next;
        }
    }
    return NULL;
}

/* Working name. Local 0x8a18, runtime 0x70084a18. Normal list insertion
 * follows observed stores; reverse-object and graph ordering remain open. */
int dm_register_constraint(struct dm_state *state, m55_addr_t constraint)
{
    struct dm_constraint *c = dm_constraint_at(state, constraint);
    struct dm_domain_desc *from, *to;
    m55_addr_t from_head, to_head;
    if (!state || !c || c->domain0 >= DM_DOMAIN_COUNT ||
        c->id_or_domain1 >= DM_DOMAIN_COUNT)
        return DM_INVALID;
    /* flag18 with flag28 clear creates an additional 0x50-byte object and
     * reversed pair table. That branch is not reconstructed yet. */
    if (c->flag18 && !c->flag28 &&
        !(state->platform && state->platform->register_constraint))
        return DM_UNKNOWN;
    if (state->platform && state->platform->register_constraint)
        if (state->platform->register_constraint(state->platform->user,
                                                c->domain0, constraint) != DM_OK)
            return DM_UNKNOWN;
    from = &state->config.domain[c->domain0];
    to = &state->config.domain[c->id_or_domain1];
    from_head = M55_DM_DESCRIPTOR_ADDR + c->domain0 * 0x94u +
                (c->flag28 ? 0x7cu : 0x74u);
    to_head = M55_DM_DESCRIPTOR_ADDR + c->id_or_domain1 * 0x94u +
              (c->flag28 ? 0x8cu : 0x84u);
    if (list_insert_front(state, from_head, constraint + 8u) != DM_OK ||
        list_insert_front(state, to_head, constraint + 0x10u) != DM_OK)
        return DM_INVALID;
    c->field44 = 0;
    c->field48 = 0;
    (void)strncpy((char *)c->unknown29_3b, to->name, 16u);
    ++from->unknown6c;
    ++to->field68;
    /* The observed code also traverses these lists to update an ordering
     * array. Exact graph ordering remains unresolved. */
    return DM_OK;
}
