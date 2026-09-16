#include "dm.h"

/* The vector table/reset sequence is not reconstructed. This translation unit
 * deliberately provides no fabricated boot path. The observed local Reset
 * vector is 0x189 (Thumb entry); DM startup is separately at local 0x8000. */
