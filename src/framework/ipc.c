#include "ipc.h"

uint8_t dm_ipc_command(const struct dm_ipc_words *request)
{
    return (uint8_t)(request->word[0] & 0xffu);
}

uint8_t dm_ipc_domain(const struct dm_ipc_words *request)
{
    return (uint8_t)((request->word[0] >> 8) & 0xffu);
}
