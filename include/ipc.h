#ifndef S5E9945_IPC_H
#define S5E9945_IPC_H

#include <stdint.h>

struct dm_ipc_words {
    uint32_t word[4];
};

_Static_assert(sizeof(struct dm_ipc_words) == 16, "four IPC words");

uint8_t dm_ipc_command(const struct dm_ipc_words *request);
uint8_t dm_ipc_domain(const struct dm_ipc_words *request);

#endif
