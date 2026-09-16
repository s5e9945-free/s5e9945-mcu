#ifndef S5E9945_M55_H
#define S5E9945_M55_H

#include <stdint.h>

enum {
    M55_DM_RELOCATION = 0x7007c000u,
    M55_DM_CONFIG_ADDR = 0x70085410u,
    M55_DM_DESCRIPTOR_ADDR = 0x70085488u,
    M55_DM_EXPORT_ADDR = 0x700864bcu,
    M55_DM_ARENA_ADDR = 0x70086590u,
    M55_DM_ARENA_SIZE = 0x2000u
};

/* A target pointer is one 32-bit Cortex-M address, never a host pointer. */
typedef uint32_t m55_addr_t;

#endif
