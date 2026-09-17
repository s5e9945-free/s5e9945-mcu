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
- A raw word sequence near M55 local `0x19e4` contains `0x70084000, 0x5000, 0x70089000, 0xd000, 0x70096000, 0x3000, 0x70099000, 0x4000, 0x7009d000, 0x7000`. Another sequence after `CORTEXM55` at local `0x310c` contains `0x70080000, 0x7000, 0x70084000, 0x20000, 0x700a4000`. Their record format and boot-time use are unproven; these are values only.

## DM instruction findings

| Local address | Observation | Model treatment |
|---|---|---|
| `0x89cc` | CMD00 stores three request words across descriptor offsets `+0x24,+0x2c..+0x64`; the final stores overwrite `+0x2c,+0x30` with word3. | Neutral fields updated exactly at observed offsets. |
| `0x8f4e..0x8f6c` | CMD0E reads a constraint from the registration array, checks `flag1a` and `outer_count`, then selects a table. | Implemented for valid arena constraints. |
| `0x8ef8..0x8f3c` | CMD07 serializes descriptor `+0x68` and four divided fields from `+0x40,+0x50,+0x60,+0x64`. | Packed into observed 16-bit response slots. |
| `0x922a..0x9258` | CMD08 serializes divided descriptor `+0x3c,+0x2c,+0x24`. | Packed into observed 16-bit response slots. |
| `0x8e02..0x8e0c` | CMD0A copies descriptor `+0x6c` into reply word1. | Implemented with neutral field name. |
| `0x8f3e..0x8f48` | CMD0D stores request word1 at config `+0x10a8`, the word immediately before the export table. | Implemented as `field10a8`. |
| `0x8a18` | Registration inserts constraint node `+0x08` into descriptor list `+0x74` or `+0x7c`, and node `+0x10` into the other descriptor's `+0x84` or `+0x8c`, selected by `flag28`. It clears constraint `+0x44/+0x48`, updates counters, and rebuilds the ordering array. | List, ordering, and reverse-object allocation paths modeled. |
| `0x911c..0x920e` | CMD09 treats request word1 as an index across the target domain's `+0x84` then `+0x8c` lists. It serializes the selected constraint's source domain and flags into reply byte1, and six divided-by-1000 values into the remaining halfwords. | Implemented for valid registered constraints. |
| `0x88fc` | Callback takes three pointers and branches on a selector with observed values 0, 1 and 2. It calls helpers at `0x8544` and `0x886c`. | ABI and helper effects remain unresolved; no invented callback behavior. |

The TBH instruction at `0x8da6` uses PC `0x8daa` and 15 little-endian halfwords starting there. Direct calculation gives targets `00=8dc8`, `01/02/03=8e10`, `04=8f70`, `05=8f7a`, `06=8dce`, `07/08/09=8edc`, `0a/0b/0c=8de8`, `0d=8f3e`, `0e=8f4e`. This corrects the earlier target list in `RE_NOTES.md`, whose addresses were two bytes early. CMD04 calls helper `0x8544`; CMD05 calls `0x886c`; CMD06 enters the common response path directly.

Further decompilation shows that `0x80b0` sends an eight-byte object through framework function pointers with selector `0x0b`. Helper `0x8544` conditionally writes descriptor words in the `+0x40` and `+0x50` regions, combines several bounds, and calls `0x82d8`. Helper `0x886c` updates another descriptor field and calls `0x862c`. The last two callees and framework effects are unresolved, so CMD04/05 remain placeholders.

## Additional M55 findings, 2026-09-17

At the tail of `0x8a18`, descriptor `+0x20` is the incoming-edge count for ordinary (`flag28=0`) links. The code scans all 28 descriptors, queues active ones with zero incoming count, walks their `+0x74` source lists, and decrements temporary counts for linked targets. It writes the queue length to config `+0x04`, domain indices to config `+0x08`, and each emitted domain position to descriptor `+0x1c`. This is a topological ordering over the ordinary links. A cycle produces an empty order when every involved domain has a positive incoming count. The host model now reconstructs this path.

When `flag18=1` and `flag28=0`, `0x8a18` additionally allocates a `0x50`-byte reverse constraint and one table of `inner_count` pairs. The reverse object swaps source and target domains, sets `flag18` and `flag28`, sets its `+0x44/+0x48` words to `0xffffffff`, and swaps each selected pair's two values. It links the new object into the original target's `+0x7c` list and original source's `+0x8c` list. Original constraint `+0x4c` records the new object address. This branch is reconstructed for valid arena data; firmware fatal behavior on allocation exhaustion is represented by `DM_MEMLACK`.

CMD09 uses the opposite list nodes, at constraint `+0x10`. For a valid selected constraint, reply byte1 packs its source domain in bits 0..5, `flag18` in bit 6, and `flag28` in bit 7. Reply words 1, 2, and 3 each contain two 16-bit values after unsigned division by 1000: `descriptor[source]+0x3c` and constraint `+0x44`; constraint `+0x48` and descriptor `+0x2c`; then descriptor `+0x60/+0x64` and `+0x40/+0x50`, with the latter pair selected by `flag28`. The disassembly has no index bounds check; the host model rejects out-of-range indices instead of reading a list sentinel as a constraint.

`0x82d8` and `0x862c` are the remaining large data-flow paths under CMD04/05. Both recompute bounds from descriptor `+0x40/+0x50/+0x60/+0x64`, traverse the ordered domain graph, and select entries from constraint pair tables. `0x862c` also invokes framework helpers `0x8060` and `0x8174`; `0x8544` can send through `0x80b0`. The framework calls and propagation rules need further evidence before these paths can be reproduced faithfully.

The Ghidra decompiler's variable types and names are provisional. In particular, graph propagation under `0x82d8`/`0x862c` needs a control-flow review with typed structures. Runtime captures of descriptor fields after CMD04/05 would be more useful than further static reads of the zero-filled table.
