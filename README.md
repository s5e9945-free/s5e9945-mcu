# S5E9945 Cortex-M55 DM reconstruction

This repository contains **reverse-engineered clean-room C source**, not Samsung source or a recovered firmware image. The existing GPL-2.0 license applies to this new project material; it does not license Samsung firmware. No proprietary binaries are included.

The evidence base is the repository's [reverse-engineering notes](docs/RE_NOTES.md) and [memory map](docs/memory-map.md), copied from the adjacent `s5e9945_codex_context` directory. The supplementary `dm_full.txt` disassembly remains in that adjacent directory. The M55 image's subsystem identity is unresolved; it must not be assumed to be ACPM.

## Scope and confidence

Confirmed observations represented here include the 28 descriptors and their observed fields, target structure sizes and offsets, DM export table values, relocation arithmetic, the 0x2000-byte arena, IPC command/domain extraction, CMD00 descriptor writes, CMD01 metadata fields and allocations, CMD02 pair writes, CMD03 registration/count behavior, CMD04 table selection, CMD07/08 serialization, CMD0A descriptor retrieval, CMD0E config-tail write, and CMD0B/C data relationships. The host model connects the two observed list nodes for the ordinary CMD03 path and reads them for CMD0B/C. List traversal order and the full graph bookkeeping remain **partially inferred**. The disassembly refines `RE_NOTES.md`: CMD01 metadata is in word1's bytes (the notes' “request byte1/byte2”), with `domain0` from word0 byte1 and `id_or_domain1` from word1 byte0.

Unknown framework calls, external-request ABI, graph ordering, the additional constraint path, and IPC commands with incomplete control flow stay explicit placeholders. CMD07 and CMD08 pack observed descriptor fields into 16-bit response slots, using the disassembly's division by 1000 where shown. No MMIO is touched. [The Ghidra follow-up](docs/ghidra-followup.md) records the input hashes and new byte-level findings.

`include/dm.h` uses 32-bit target addresses even on a 64-bit host. The simulated arena maps those addresses into host memory. The target's observed `memlack` fatal loop is represented by a platform fatal hook, so host tests return safely.

## Build and test

Run `make test` for host checks. `make` builds the host library. `linker/s5e9945_m55.ld` documents the observed M55 local image and DM relocation address space; it is not a claim that the reconstructed code can replace the firmware image. No cross compiler or device integration is required for host tests.

## Remaining reverse engineering

Highest value next evidence: control-flow-validated disassembly for local `0x88FC` (external ABI), `0x8A18` (list registration), and the CMD0B/C list paths under `0x8D74..0x9348`, followed by runtime descriptor values at `+0x40..+0x68`. The static image has zeros there; runtime behavior is needed to determine the actual values.
