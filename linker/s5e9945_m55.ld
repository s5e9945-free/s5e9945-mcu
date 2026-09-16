/* Reference layout only. A loadable firmware/vector map is not established.
 * DM local + 0x7007c000 = observed runtime address. */
MEMORY {
    M55_IMAGE (rx) : ORIGIN = 0x7007c000, LENGTH = 0x28000
    DM_ARENA (rw) : ORIGIN = 0x70086590, LENGTH = 0x2000
}
/* No SECTIONS mapping: this file records evidence, not a replacement image. */
