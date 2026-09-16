# Ghidra follow-up, 2026-09-16

The inputs were read outside this repository. No firmware bytes are committed.

| Input | Size | SHA-256 |
|---|---:|---|
| `ghidra_12.1.3_PUBLIC_20260817/sboot.bin` | `0x899330` | `2d9001a28c8e6cd1a4316f1c71fe9c223cae613f6cef4df56fa832911873c0eb` |
| `ghidra_12.1.3_PUBLIC_20260817/exynos990-dump/acpm.bin` | `0x4c000` | `5181740cfa775fad5ac9310f0791c35c529b3a2219f3bf318f1bf78eee159b3c` |

The analysis extracted only a temporary `0x28000`-byte M55 window from S-Boot file offset `0x11a000`. Ghidra 12.1.3 imported it as raw `ARM:LE:32:v8-m` at local address zero, with global auto-analysis disabled. `tools/ProbeM55.java` set Thumb mode over the DM code range and seeded only known starts; `tools/DecompileM55.java` decompiled selected functions. The decompiler output was checked against individual instructions from `dm_full.txt`. Inline literal pools and the TBH table must not be treated as code.

## Verified data and image separation

- The M55 vector words at S-Boot `0x11a000` are `0x4c00, 0x189, 0x271, 0x277`. The M55 `CORTEXM55` string is local `0x30f8`.
- The candidate S5E9945 ACPM block is `0xca000..0x116000`, exactly `0x4c000` bytes. It has plausible M23 vector tables at relative `0` and `0x16000`. The M55 begins `0x4000` bytes after this candidate block ends.
- The separate Exynos 990 ACPM dump is also `0x4c000` bytes. Its first four words are `0xe800, 0x3261, 0x3281, 0x3287`; another vector-like header appears at `0x3f000`. Similar size and multiple vector tables support comparing container layout only. They do **not** identify the S5E9945 M55 subsystem.
- The M55 config at local `0x9410` starts with count 28. All 28 static descriptor words `+0x40,+0x50,+0x60,+0x64,+0x68,+0x6c` are zero. The four CPU descriptors alone have `+0x70 = 1`; MIF and INT both have `+0x00 = 0x101`. The export words at `0xa4bc` match `docs/RE_NOTES.md`. The config tail at `0xa4b8` starts at zero.

## DM instruction findings

| Local address | Observation | Model treatment |
|---|---|---|
| `0x89cc` | CMD00 stores three request words across descriptor offsets `+0x24,+0x2c..+0x64`; the final stores overwrite `+0x2c,+0x30` with word3. | Neutral fields updated exactly at observed offsets. |
| `0x8f4e..0x8f6c` | CMD04 reads a constraint from the registration array, checks `flag1a` and `outer_count`, then selects a table. | Implemented for valid arena constraints. |
| `0x8ef8..0x8f3c` | CMD07 serializes descriptor `+0x68` and four divided fields from `+0x40,+0x50,+0x60,+0x64`. | Packed into observed 16-bit response slots. |
| `0x922a..0x9258` | CMD08 serializes divided descriptor `+0x3c,+0x2c,+0x24`. | Packed into observed 16-bit response slots. |
| `0x8e02..0x8e0c` | CMD0A copies descriptor `+0x6c` into reply word1. | Implemented with neutral field name. |
| `0x8f3e..0x8f48` | CMD0E stores request word1 at config `+0x10a8`, the word immediately before the export table. | Implemented as `field10a8`. |
| `0x8a18` | Ordinary registration inserts constraint node `+0x08` into descriptor list `+0x74` or `+0x7c`, and node `+0x10` into the other descriptor's `+0x84` or `+0x8c`, selected by `flag28`. It also clears constraint `+0x44/+0x48` and updates counters. | Ordinary list path modeled. The branch that allocates a second `0x50`-byte object and reverses pair values, plus the ordering traversal, remain unresolved. |
| `0x88fc` | Callback takes three pointers and branches on a selector with observed values 0, 1 and 2. It calls helpers at `0x8544` and `0x886c`. | ABI and helper effects remain unresolved; no invented callback behavior. |

The Ghidra decompiler's variable types and names are provisional. In particular, the second object's purpose and the graph traversal under `0x8a18` need a control-flow review with typed structures. Runtime captures of descriptor fields after CMD00 and registration would be more useful than further static reads of the zero-filled table.
