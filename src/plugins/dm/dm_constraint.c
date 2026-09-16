#include "dm_internal.h"

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

/* Working name. Local 0x8a18, runtime 0x70084a18. Observed code connects
 * constraint/domain lists; exact graph manipulation remains unresolved. */
int dm_register_constraint(struct dm_state *state, m55_addr_t constraint)
{
    if (!state || !dm_constraint_at(state, constraint))
        return DM_INVALID;
    if (state->platform && state->platform->register_constraint)
        return state->platform->register_constraint(state->platform->user,
                                                    dm_constraint_at(state, constraint)->domain0,
                                                    constraint);
    return DM_OK; /* Host model records registration; graph remains unknown. */
}
