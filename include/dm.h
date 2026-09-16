#ifndef S5E9945_DM_H
#define S5E9945_DM_H

#include <stddef.h>
#include <stdint.h>
#include "ipc.h"
#include "m55.h"

#define DM_DOMAIN_COUNT 28u
#define DM_MAX_CONSTRAINTS 64u /* Host-model capacity; target array extent unknown. */

struct list_head {
    m55_addr_t next;
    m55_addr_t prev;
};

struct dm_domain_desc {
    uint32_t field00;
    uint32_t index;
    char name[16];
    uint32_t global_id;
    uint8_t unknown1c_3f[0x24];
    uint32_t field40;
    uint8_t unknown44_4f[0x0c];
    uint32_t field50;
    uint8_t unknown54_5f[0x0c];
    uint32_t field60;
    uint32_t field64;
    uint32_t field68;
    uint32_t unknown6c;
    uint32_t field70;
    struct list_head list74;
    struct list_head list7c;
    struct list_head list84;
    struct list_head list8c;
};

struct dm_config {
    uint32_t count;
    uint8_t unknown04_77[0x74];
    struct dm_domain_desc domain[DM_DOMAIN_COUNT];
};

struct dm_constraint {
    uint32_t domain0;
    uint32_t id_or_domain1;
    struct list_head node0;
    struct list_head node1;
    uint8_t flag18;
    uint8_t flag19;
    uint8_t flag1a;
    uint8_t unknown1b;
    uint32_t inner_count;
    uint32_t outer_count;
    uint32_t selected_index;
    uint8_t flag28;
    uint8_t unknown29_3b[0x13];
    m55_addr_t selected_table;
    m55_addr_t tables;
    uint32_t field44;
    uint32_t field48;
    m55_addr_t runtime_object;
};

struct dm_pair { uint32_t value0, value1; };

/* Values observed at runtime 0x700864bc; function entries have Thumb bit set. */
struct dm_export_table {
    m55_addr_t config;
    m55_addr_t ipc_handler_thumb;
    m55_addr_t return_zero_thumb;
    uint32_t field0c;
    m55_addr_t external_callback_thumb;
};

_Static_assert(sizeof(struct list_head) == 8, "target list head");
_Static_assert(sizeof(struct dm_domain_desc) == 0x94, "target descriptor");
_Static_assert(offsetof(struct dm_domain_desc, name) == 0x08, "descriptor name");
_Static_assert(offsetof(struct dm_domain_desc, global_id) == 0x18, "descriptor ID");
_Static_assert(offsetof(struct dm_domain_desc, field40) == 0x40, "descriptor field40");
_Static_assert(offsetof(struct dm_domain_desc, field50) == 0x50, "descriptor field50");
_Static_assert(offsetof(struct dm_domain_desc, field60) == 0x60, "descriptor field60");
_Static_assert(offsetof(struct dm_domain_desc, field64) == 0x64, "descriptor field64");
_Static_assert(offsetof(struct dm_domain_desc, field68) == 0x68, "descriptor field68");
_Static_assert(offsetof(struct dm_domain_desc, field70) == 0x70, "descriptor field70");
_Static_assert(offsetof(struct dm_domain_desc, list74) == 0x74, "descriptor list74");
_Static_assert(offsetof(struct dm_domain_desc, list7c) == 0x7c, "descriptor list7c");
_Static_assert(offsetof(struct dm_domain_desc, list84) == 0x84, "descriptor list84");
_Static_assert(offsetof(struct dm_domain_desc, list8c) == 0x8c, "descriptor list8c");
_Static_assert(offsetof(struct dm_config, domain) == 0x78, "config descriptors");
_Static_assert(sizeof(struct dm_config) == 0x10a8, "config size through descriptors");
_Static_assert(sizeof(struct dm_constraint) == 0x50, "target constraint");
_Static_assert(offsetof(struct dm_constraint, node0) == 0x08, "constraint node0");
_Static_assert(offsetof(struct dm_constraint, node1) == 0x10, "constraint node1");
_Static_assert(offsetof(struct dm_constraint, flag18) == 0x18, "constraint flag18");
_Static_assert(offsetof(struct dm_constraint, flag19) == 0x19, "constraint flag19");
_Static_assert(offsetof(struct dm_constraint, flag1a) == 0x1a, "constraint flag1a");
_Static_assert(offsetof(struct dm_constraint, inner_count) == 0x1c, "constraint inner count");
_Static_assert(offsetof(struct dm_constraint, outer_count) == 0x20, "constraint outer count");
_Static_assert(offsetof(struct dm_constraint, selected_index) == 0x24, "constraint selected index");
_Static_assert(offsetof(struct dm_constraint, flag28) == 0x28, "constraint flag28");
_Static_assert(offsetof(struct dm_constraint, selected_table) == 0x3c, "constraint selected table");
_Static_assert(offsetof(struct dm_constraint, tables) == 0x40, "constraint tables");
_Static_assert(offsetof(struct dm_constraint, field44) == 0x44, "constraint field44");
_Static_assert(offsetof(struct dm_constraint, field48) == 0x48, "constraint field48");
_Static_assert(offsetof(struct dm_constraint, runtime_object) == 0x4c, "constraint runtime object");
_Static_assert(sizeof(struct dm_pair) == 8, "target pair");
_Static_assert(sizeof(struct dm_export_table) == 0x14, "target export table");

enum dm_result { DM_OK = 0, DM_UNKNOWN = -1, DM_INVALID = -2, DM_MEMLACK = -3 };

struct dm_platform {
    void (*log)(void *user, const char *observed_label);
    void (*fatal)(void *user, const char *observed_label);
    /* Unknown framework operation at local 0x8a18. */
    int (*register_constraint)(void *user, uint32_t domain,
                               m55_addr_t constraint);
    void *user;
};

struct dm_state {
    struct dm_config config;
    const struct dm_platform *platform;
    m55_addr_t current_constraint;
    m55_addr_t registered[DM_MAX_CONSTRAINTS];
    uint32_t count;
    uint32_t arena_offset;
    _Alignas(8) uint8_t arena[M55_DM_ARENA_SIZE];
};

extern const struct dm_export_table dm_exports;

void dm_plugin_start(struct dm_state *state, const struct dm_platform *platform);
int dm_return_zero_stub(void);
int dm_external_request(struct dm_state *state, const struct dm_ipc_words *request,
                        struct dm_ipc_words *response);
int dm_register_constraint(struct dm_state *state, m55_addr_t constraint);
int dm_ipc_handler(struct dm_state *state, const struct dm_ipc_words *request,
                   struct dm_ipc_words *response);
m55_addr_t dm_arena_alloc(struct dm_state *state, uint32_t bytes);
void *dm_arena_ptr(struct dm_state *state, m55_addr_t address, uint32_t bytes);

#endif
