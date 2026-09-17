#ifndef S5E9945_DM_INTERNAL_H
#define S5E9945_DM_INTERNAL_H

#include "dm.h"

struct dm_constraint *dm_constraint_at(struct dm_state *state, m55_addr_t address);
struct dm_pair *dm_pair_at(struct dm_state *state,
                           const struct dm_constraint *constraint,
                           uint32_t table_index, uint32_t pair_index);
struct list_head *dm_list_at(struct dm_state *state, m55_addr_t address);
struct dm_constraint *dm_constraint_in_domain_list(struct dm_state *state,
                                                    uint32_t domain,
                                                    uint32_t index);
struct dm_constraint *dm_constraint_in_target_list(struct dm_state *state,
                                                    uint32_t domain,
                                                    uint32_t index);

#endif
