#include "dm.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static unsigned fatal_count;
static void fatal_hook(void *user, const char *label)
{
    (void)user;
    assert(strcmp(label, "memlack") == 0);
    ++fatal_count;
}

static struct dm_state state;
static const struct dm_platform platform = { .fatal = fatal_hook };

static void test_layout_and_domains(void)
{
    static const char *const names[DM_DOMAIN_COUNT] = {
        "dm_mif", "dm_int", "dm_cpu_cl0", "dm_cpu_cl1l", "dm_cpu_cl1h",
        "dm_cpu_cl2", "dm_npu", "dm_npu", "dm_dsu", "dm_aud", "dm_gpu",
        "dm_intcam", "dm_cam", "dm_disp", "dm_csis", "dm_isp", "dm_mfc",
        "dm_mfc1", "dm_dsp", "dm_dnc", "dm_GNSS", "dm_ALIVE", "dm_chub",
        "dm_vts", "dm_hsi0", "dm_ufd", "dm_unpu", "dm_icpu"
    };
    static const uint8_t ids[DM_DOMAIN_COUNT] = {
        0,1,2,3,4,5,6,7,8,0x0b,0x0d,0x0e,0x0f,0x10,
        0x11,0x12,0x13,0x14,0x17,0x18,0x19,0x1a,0x1b,0x1c,
        0x1d,0x1e,0x20,0x16
    };
    uint32_t i;
    dm_plugin_start(&state, &platform);
    assert(state.config.count == 28);
    assert(M55_DM_CONFIG_ADDR + offsetof(struct dm_config, domain) ==
           M55_DM_DESCRIPTOR_ADDR);
    assert(M55_DM_CONFIG_ADDR + offsetof(struct dm_config, field10a8) ==
           0x700864b8u);
    assert(M55_DM_CONFIG_ADDR + sizeof(struct dm_config) == M55_DM_EXPORT_ADDR);
    assert(dm_exports.config == M55_DM_CONFIG_ADDR);
    assert(dm_exports.ipc_handler_thumb == 0x70084d75u);
    assert(dm_exports.return_zero_thumb == 0x700840adu);
    assert(dm_exports.field0c == 0);
    assert(dm_exports.external_callback_thumb == 0x700848fdu);
    assert(dm_return_zero_stub() == 0);
    for (i = 0; i < DM_DOMAIN_COUNT; ++i) {
        const struct dm_domain_desc *d = &state.config.domain[i];
        const uint32_t base = M55_DM_DESCRIPTOR_ADDR + i * 0x94u;
        assert(d->field00 == (i < 2 ? 0x101u : 1u));
        assert(d->index == i);
        assert(strcmp(d->name, names[i]) == 0);
        assert(d->global_id == ids[i]);
        assert(d->field70 == (i >= 2 && i <= 5 ? 1u : 0u));
        assert(d->list74.next == base + 0x74u);
        assert(d->list74.prev == base + 0x74u);
        assert(d->list7c.next == base + 0x7cu);
        assert(d->list84.next == base + 0x84u);
        assert(d->list8c.next == base + 0x8cu);
    }
}

static void test_ipc_roundtrip(void)
{
    struct dm_ipc_words request = {{0}}, response = {{0}};
    struct dm_constraint *c;
    struct dm_pair *p;
    request.word[0] = 0x00000301u;
    /* word1: id=7, flags+outer=0x2d, selected=1, inner=3 */
    request.word[1] = 0x03012d07u;
    assert(dm_ipc_command(&request) == 1);
    assert(dm_ipc_domain(&request) == 3);
    assert(dm_ipc_handler(&state, &request, &response) == DM_OK);
    c = dm_arena_ptr(&state, state.current_constraint, sizeof(*c));
    assert(c && c->domain0 == 3 && c->id_or_domain1 == 7);
    assert(c->flag28 == 1 && c->flag18 == 0 && c->flag19 == 1 &&
           c->flag1a == 1);
    assert(c->outer_count == 2 && c->selected_index == 1 && c->inner_count == 3);
    assert(c->selected_table != 0 && c->tables != 0);
    assert(state.arena_offset == 0x50u + 2u * 4u + 2u * 3u * 8u);

    request.word[0] = 0x00000302u;
    request.word[1] = 0x02101207u; /* pair 2, table 1 in byte2 high nibble */
    request.word[2] = 0x12345678u;
    request.word[3] = 0x9abcdef0u;
    assert(dm_ipc_handler(&state, &request, &response) == DM_OK);
    {
        m55_addr_t *tables = dm_arena_ptr(&state, c->tables, 8);
        assert(tables);
        p = dm_arena_ptr(&state, tables[1] + 2u * 8u, 8);
    }
    assert(p && p->value0 == 0x12345678u && p->value1 == 0x9abcdef0u);

    request.word[0] = 0x00000303u;
    assert(dm_ipc_handler(&state, &request, &response) == DM_OK);
    assert((response.word[1] & 0xffu) == 0 && state.count == 1);
    assert(state.registered[0] == state.current_constraint);
    assert(state.config.domain[3].list7c.next == state.current_constraint + 8u);
    assert(state.config.domain[7].list8c.next == state.current_constraint + 0x10u);
    assert(state.config.domain[3].unknown6c == 1u);
    assert(state.config.domain[7].field68 == 1u);

    c->field44 = 0x11223344u;
    c->field48 = 0x55667788u;
    request.word[0] = 0x0000030bu;
    request.word[1] = 0x00000f00u; /* registered index 0; flags overwrite low nibble */
    assert(dm_ipc_handler(&state, &request, &response) == DM_OK);
    assert((response.word[1] & 0xffu) == 7u);
    assert(((response.word[1] >> 8) & 0x0fu) == 0x0du);
    assert((response.word[1] & 0x0000f000u) == 0);
    assert(((response.word[1] >> 16) & 0xffu) == 0x12u);
    assert((response.word[1] >> 24) == 3u);
    assert(response.word[2] == c->field44 && response.word[3] == c->field48);

    request.word[0] = 0x0000030cu;
    request.word[1] = 0x00020100u; /* constraint 0, table 1, pair 2 */
    assert(dm_ipc_handler(&state, &request, &response) == DM_OK);
    assert(response.word[2] == p->value0 && response.word[3] == p->value1);

    request.word[0] = 0x00000004u; /* registered constraint 0 */
    request.word[1] = 0u;
    assert(dm_ipc_handler(&state, &request, &response) == DM_OK);
    assert(c->selected_index == 0);
    {
        m55_addr_t *tables = dm_arena_ptr(&state, c->tables, 8);
        assert(tables && c->selected_table == tables[0]);
    }

    /* A second constraint uses list74. CMD0B traverses list74, then list7c. */
    request.word[0] = 0x00000301u;
    request.word[1] = 0x01001007u;
    assert(dm_ipc_handler(&state, &request, &response) == DM_OK);
    request.word[0] = 0x00000303u;
    assert(dm_ipc_handler(&state, &request, &response) == DM_OK);
    assert(state.count == 2);
    assert(state.config.domain[3].list74.next == state.current_constraint + 8u);
    request.word[0] = 0x0000030bu;
    request.word[1] = 0u;
    assert(dm_ipc_handler(&state, &request, &response) == DM_OK);
    assert(((response.word[1] >> 8) & 0x0fu) == 0u);
    request.word[1] = 0x00001000u; /* second entry in the combined lists */
    assert(dm_ipc_handler(&state, &request, &response) == DM_OK);
    assert(((response.word[1] >> 8) & 0x0fu) == 0x0du);
    request.word[0] = 0x0000030cu;
    request.word[1] = 0x00020101u; /* constraint 1, table 1, pair 2 */
    assert(dm_ipc_handler(&state, &request, &response) == DM_OK);
    assert(response.word[2] == p->value0 && response.word[3] == p->value1);
}

static void test_descriptor_commands(void)
{
    struct dm_ipc_words request = {{0}}, response = {{0}};
    struct dm_domain_desc *d = &state.config.domain[3];
    request.word[0] = 0x00000300u;
    request.word[1] = 123456u;
    request.word[2] = 3000u;
    request.word[3] = 65536u;
    assert(dm_ipc_handler(&state, &request, &response) == DM_OK);
    assert(d->field40 == 123456u && d->field50 == 3000u);
    assert(d->field60 == 123456u && d->field64 == 3000u);
    d->field68 = 0x12345678u;
    request.word[0] = 0x00000307u;
    request.word[3] = 0xabcd0000u;
    assert(dm_ipc_handler(&state, &request, &response) == DM_OK);
    assert(response.word[1] == ((123u << 16) | 0x5678u));
    assert(response.word[2] == (123u << 16 | 3u));
    assert(response.word[3] == (0xabcdu << 16 | 3u));

    request.word[0] = 0x00000308u;
    request.word[2] = 0xabcd0000u;
    assert(dm_ipc_handler(&state, &request, &response) == DM_OK);
    assert(response.word[1] == (65u << 16 | 123u));
    assert(response.word[2] == (0xabcdu << 16 | 65u));

    d->unknown6c = 0xdeadbeefu;
    request.word[0] = 0x0000030au;
    assert(dm_ipc_handler(&state, &request, &response) == DM_OK);
    assert(response.word[1] == 0xdeadbeefu);
    request.word[0] = 0x0000000eu;
    request.word[1] = 0x13572468u;
    assert(dm_ipc_handler(&state, &request, &response) == DM_OK);
    assert(state.config.field10a8 == 0x13572468u);
}

static void test_arena_bounds(void)
{
    struct dm_ipc_words request = {{0}}, response = {{0}};
    uint32_t before;
    dm_plugin_start(&state, &platform);
    assert(dm_arena_ptr(&state, M55_DM_ARENA_ADDR - 1u, 1) == NULL);
    assert(dm_arena_ptr(&state, M55_DM_ARENA_ADDR + 0x2000u, 1) == NULL);
    assert(dm_arena_alloc(&state, 0x1ffcu) == M55_DM_ARENA_ADDR);
    before = fatal_count;
    assert(dm_arena_alloc(&state, 8) == 0);
    assert(fatal_count == before + 1);
    assert(state.arena_offset == 0x1ffcu);
    request.word[0] = 0x00000001u;
    request.word[1] = 0x01001000u; /* one table, one pair */
    before = fatal_count;
    assert(dm_ipc_handler(&state, &request, &response) == DM_MEMLACK);
    assert(fatal_count == before + 1);
    assert(state.arena_offset == 0x1ffcu);
}

int main(void)
{
    test_layout_and_domains();
    test_ipc_roundtrip();
    test_descriptor_commands();
    test_arena_bounds();
    puts("dm tests passed");
    return 0;
}
