#include "dm_internal.h"
#include <string.h>

struct dm_domain_seed { const char *name; uint8_t global_id; };
static const struct dm_domain_seed domain_seed[DM_DOMAIN_COUNT] = {
    {"dm_mif", 0x00}, {"dm_int", 0x01}, {"dm_cpu_cl0", 0x02},
    {"dm_cpu_cl1l", 0x03}, {"dm_cpu_cl1h", 0x04}, {"dm_cpu_cl2", 0x05},
    {"dm_npu", 0x06}, {"dm_npu", 0x07}, {"dm_dsu", 0x08},
    {"dm_aud", 0x0b}, {"dm_gpu", 0x0d}, {"dm_intcam", 0x0e},
    {"dm_cam", 0x0f}, {"dm_disp", 0x10}, {"dm_csis", 0x11},
    {"dm_isp", 0x12}, {"dm_mfc", 0x13}, {"dm_mfc1", 0x14},
    {"dm_dsp", 0x17}, {"dm_dnc", 0x18}, {"dm_GNSS", 0x19},
    {"dm_ALIVE", 0x1a}, {"dm_chub", 0x1b}, {"dm_vts", 0x1c},
    {"dm_hsi0", 0x1d}, {"dm_ufd", 0x1e}, {"dm_unpu", 0x20},
    {"dm_icpu", 0x16}
};

const struct dm_export_table dm_exports = {
    M55_DM_CONFIG_ADDR, 0x70084d75u, 0x700840adu, 0u, 0x700848fdu
};

static void init_list(struct list_head *head, m55_addr_t address)
{
    head->next = address;
    head->prev = address;
}

/* Working name. Local 0x8000, runtime 0x70084000. */
void dm_plugin_start(struct dm_state *state, const struct dm_platform *platform)
{
    uint32_t i;
    memset(state, 0, sizeof(*state));
    state->platform = platform;
    state->config.count = DM_DOMAIN_COUNT;
    if (platform && platform->log)
        platform->log(platform->user, "plug_st");
    for (i = 0; i < DM_DOMAIN_COUNT; ++i) {
        struct dm_domain_desc *d = &state->config.domain[i];
        const m55_addr_t base = M55_DM_DESCRIPTOR_ADDR + i * 0x94u;
        d->field00 = i < 2u ? 0x101u : 1u;
        d->index = i;
        (void)strncpy(d->name, domain_seed[i].name, sizeof(d->name) - 1u);
        d->global_id = domain_seed[i].global_id;
        d->field70 = i >= 2u && i <= 5u ? 1u : 0u;
        init_list(&d->list74, base + 0x74u);
        init_list(&d->list7c, base + 0x7cu);
        init_list(&d->list84, base + 0x84u);
        init_list(&d->list8c, base + 0x8cu);
    }
}

/* Local 0x80ac, runtime 0x700840ac: movs r0,#0; bx lr. */
int dm_return_zero_stub(void) { return 0; }

/* Local 0x88fc, runtime 0x700848fc. ABI and effects unresolved. */
int dm_external_request(struct dm_state *state, const struct dm_ipc_words *request,
                        struct dm_ipc_words *response)
{
    (void)state;
    (void)request;
    (void)response;
    return DM_UNKNOWN;
}

void *dm_arena_ptr(struct dm_state *state, m55_addr_t address, uint32_t bytes)
{
    uint32_t offset;
    if (!state || address < M55_DM_ARENA_ADDR)
        return NULL;
    offset = address - M55_DM_ARENA_ADDR;
    if (offset > M55_DM_ARENA_SIZE || bytes > M55_DM_ARENA_SIZE - offset)
        return NULL;
    return &state->arena[offset];
}

m55_addr_t dm_arena_alloc(struct dm_state *state, uint32_t bytes)
{
    uint32_t offset;
    if (!state)
        return 0;
    offset = state->arena_offset;
    /* The observed allocation branches on carry: end >= 0x2000 fails. */
    if (offset > M55_DM_ARENA_SIZE || bytes >= M55_DM_ARENA_SIZE - offset) {
        if (state->platform && state->platform->log)
            state->platform->log(state->platform->user, "memlack");
        if (state->platform && state->platform->fatal)
            state->platform->fatal(state->platform->user, "memlack");
        return 0;
    }
    state->arena_offset += bytes;
    return M55_DM_ARENA_ADDR + offset;
}
