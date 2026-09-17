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

/* The tail of 0x8a18 rebuilds config +0x04/+0x08 from descriptor +0x20.
 * Its queue starts with active domains of degree zero, then walks list74. */
static void rebuild_domain_order(struct dm_state *state)
{
    uint32_t degrees[DM_DOMAIN_COUNT], queue[DM_DOMAIN_COUNT];
    uint32_t queued = 0, position = 0, i;
    struct dm_config *config = &state->config;
    for (i = 0; i < DM_DOMAIN_COUNT; ++i) {
        const struct dm_domain_desc *d = &config->domain[i];
        const m55_addr_t base = M55_DM_DESCRIPTOR_ADDR + i * 0x94u;
        degrees[i] = d->incoming_count;
        if (d->field00 != 0u &&
            (d->list74.next != base + 0x74u ||
             d->list7c.next != base + 0x7cu ||
             d->list84.next != base + 0x84u ||
             d->list8c.next != base + 0x8cu) &&
            degrees[i] == 0u)
            queue[queued++] = i;
    }
    while (position < queued) {
        uint32_t source = queue[position];
        const m55_addr_t head_addr = M55_DM_DESCRIPTOR_ADDR + source * 0x94u + 0x74u;
        m55_addr_t node_addr = config->domain[source].list74.next;
        config->order[position] = source;
        config->domain[source].order_index = position++;
        for (i = 0; node_addr != head_addr && i < DM_MAX_CONSTRAINTS; ++i) {
            struct dm_constraint *c;
            uint32_t target;
            if (node_addr < M55_DM_ARENA_ADDR + 8u)
                break;
            c = dm_constraint_at(state, node_addr - 8u);
            if (!c)
                break;
            target = c->id_or_domain1;
            if (target < DM_DOMAIN_COUNT && degrees[target] != 0u &&
                --degrees[target] == 0u && queued < DM_DOMAIN_COUNT)
                queue[queued++] = target;
            node_addr = c->node0.next;
        }
    }
    config->order_count = queued;
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

/* CMD09 indexes the target-side lists in +0x84, then +0x8c order. Their
 * nodes are constraint +0x10, so the iteration must follow node1. */
struct dm_constraint *dm_constraint_in_target_list(struct dm_state *state,
                                                    uint32_t domain,
                                                    uint32_t index)
{
    static const uint32_t family[2] = {0x84u, 0x8cu};
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
            if (node_addr < M55_DM_ARENA_ADDR + 0x10u)
                return NULL;
            c = dm_constraint_at(state, node_addr - 0x10u);
            if (!c)
                return NULL;
            if (index == 0u)
                return c;
            --index;
            ++visited;
            node_addr = c->node1.next;
        }
    }
    return NULL;
}

/* Local 0x8a18, runtime 0x70084a18. */
int dm_register_constraint(struct dm_state *state, m55_addr_t constraint)
{
    struct dm_constraint *c = dm_constraint_at(state, constraint), *reverse;
    struct dm_domain_desc *from, *to;
    m55_addr_t from_head, to_head, reverse_addr, pair_addr;
    struct dm_pair *source_pairs, *reverse_pairs;
    uint32_t i;
    if (!state || !c || c->domain0 >= DM_DOMAIN_COUNT ||
        c->id_or_domain1 >= DM_DOMAIN_COUNT)
        return DM_INVALID;
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
    if (!c->flag28)
        ++to->incoming_count;
    ++from->unknown6c;
    ++to->field68;
    if (c->flag18 && !c->flag28) {
        /* 0x8abc..0x8ae2: the inverse has one selected table whose pairs
         * exchange their two words. It is linked, but not registered in the
         * IPC registration array. */
        source_pairs = dm_arena_ptr(state, c->selected_table,
                                    c->inner_count * sizeof(*source_pairs));
        if (!source_pairs)
            return DM_INVALID;
        reverse_addr = dm_arena_alloc(state, sizeof(*reverse));
        if (!reverse_addr)
            return DM_MEMLACK;
        pair_addr = dm_arena_alloc(state,
                                   c->inner_count * sizeof(*reverse_pairs));
        if (!pair_addr)
            return DM_MEMLACK;
        reverse = dm_constraint_at(state, reverse_addr);
        reverse_pairs = dm_arena_ptr(state, pair_addr,
                                     c->inner_count * sizeof(*reverse_pairs));
        if (!reverse || !reverse_pairs)
            return DM_INVALID;
        reverse->domain0 = c->id_or_domain1;
        reverse->id_or_domain1 = c->domain0;
        reverse->flag18 = 1u;
        reverse->flag28 = 1u;
        reverse->inner_count = c->inner_count;
        reverse->selected_table = pair_addr;
        reverse->field44 = UINT32_MAX;
        reverse->field48 = UINT32_MAX;
        (void)strncpy((char *)reverse->unknown29_3b, from->name, 16u);
        for (i = 0; i < c->inner_count; ++i) {
            reverse_pairs[i].value0 = source_pairs[i].value1;
            reverse_pairs[i].value1 = source_pairs[i].value0;
        }
        if (list_insert_front(state,
                M55_DM_DESCRIPTOR_ADDR + c->id_or_domain1 * 0x94u + 0x7cu,
                reverse_addr + 8u) != DM_OK ||
            list_insert_front(state,
                M55_DM_DESCRIPTOR_ADDR + c->domain0 * 0x94u + 0x8cu,
                reverse_addr + 0x10u) != DM_OK)
            return DM_INVALID;
        c->runtime_object = reverse_addr;
        ++to->unknown6c;
        ++from->field68;
    }
    rebuild_domain_order(state);
    return DM_OK;
}
