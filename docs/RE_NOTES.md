# S5E9945 MCU Reverse-Engineering Notes

These are independently derived reverse-engineering observations intended for a clean-room C reconstruction, not original Samsung source.

## Confidence
- **Confirmed**: supported directly by bytes/disassembly/address arithmetic.
- **Strong inference**: multiple observations agree, identity/semantics not fully proven.
- **Provisional**: working name only; do not promote to fact.

## S-Boot
`sboot.bin`: size `0x899330`.
Version: `v2.8(release):CEC1-240326-Android14-6.1-PT57109-s5e9945-1-g664e306cb`.

ACPM-related strings include `acpm_log`, `acpm_ipc_send_data_wait`, `ACPM0 GPR`, `ACPM1 GPR`,
`acpm_dump`, `@ ACPM vclk data`, `acpm ipc timeout`, `FLEXPMU-DBG`,
and `[9945] %s: acpm_dump: base: %x size: %x`.

### ACPM dump — confirmed
AArch64 at `0x354C4C` constructs:
- base `0x90028000`
- size `0x4C000`
for `handle_acpm_dump` / `acpm_dump`.
End exclusive: `0x90074000`.

### Relocation/copy chain — confirmed
- `0x9620C`: `0x0203F000..0x0208F000 -> 0x1356B000`, `0x50000`
- `0x96234`: `0x1356B000..0x135BB000 -> 0x90028000`, `0x50000`
- `0x9625C`: `0x13434000..0x13440000 -> 0x0203F000`, `0xC000`
- `0x96284`: `0x13440000..0x13484000 -> 0x0204B000`, `0x44000`
- `0x962AC`: `0x13484000..0x1348C000 -> 0x0208F000`, `0x8000`
- `0x962D4`: `0x1348C000..0x134AC000 -> 0x90084000`, `0x20000`
- `0x962FC`: `0x0208F000..0x02097000 -> 0x90078000`, `0x8000`
- `0x9634C`: `0x134AC000..0x134D4000 -> 0x90000000`, `0x28000`

The `0x0203F000..0x0208F000` contents are backed up before being overwritten.
A descriptor associates `0x135BB000` with the beginning of a `log_dramtrain` region.

S-Boot conditionally checks the first word at `0x90028000` against `0xC800`.
File offset `0xCA000` begins with exactly `0x0000C800`, followed by a valid Cortex-M vector table.

## Candidate ACPM bundle — strong inference
Candidate file range: `0xCA000..0x116000` (`0x4C000` bytes).
Reasons:
1. exact S-Boot ACPM dump size;
2. first word matches S-Boot `0xC800` check;
3. valid M23 firmware at start;
4. contains M23/framework/plugin-like power-management regions;
5. separate M55 starts at `0x11A000`, after candidate end.

Do not claim M55 == ACPM. Exact top-level identity remains unresolved.

## Cortex-M images
### M23 #1 — confirmed
File `0xCA000`; MSP `0xC800`, Reset `0x6141`, NMI `0x6241`, HardFault `0x6247`.
Mapping: `VA = file - 0xC4000`.
`CORTEXM23` at file `0xCD154` / VA `0x9154`.
Strings include framework IPC/event/debug terms.
Build: `d334506 yulgon`, `20:03:15 Oct 30 2023`.

Plugin-like region begins around `0xD2000`; builds include `bf46199 yulgon` and `4d85991 yulgon`.
Later strings strongly resemble FlexPMU/power management.

### M23 #2 — confirmed
File `0xE0000`; MSP `0x7D00`, Reset `0x171`, NMI `0x259`, HardFault `0x25F`.
Mapping: `VA = file - 0xE0000`.
`CORTEXM23` at VA `0x4494`.
Build `af65594 hjshin`, `14:22:57 Aug 06 2024`.
Following plugin-like regions contain G3D/mailbox/FVP/DVFS/GPADC/DCXO-related strings.

### M55 #3 — confirmed
File `0x11A000`; MSP `0x4C00`, Reset `0x189`, NMI `0x271`, HardFault `0x277`.
Extracted analysis window: `0x28000` bytes from `0x11A000`.
Local offset in this extraction is treated as local VA.
`CORTEXM55` local `0x30F8`.
Build metadata includes:
- `5ff1676 yulgon`, `18:04:17 Nov 13 2023`
- `dc82754 hjshin`, `16:52:42 Jan 03 2024`
- `d9c4b39 hjshin`, `15:11:35 May 27 2024`
- `db47cd4 joonyj7`, `14:51:16 Jul 26 2023`

## M55 DM relocation — confirmed for DM section
`runtime = local + 0x7007C000`.

Examples:
- local `0x8000` -> runtime `0x70084000`
- `0x80AC` -> `0x700840AC`
- `0x88FC` -> `0x700848FC`
- `0x8D74` -> `0x70084D74`
- `0x9410` -> `0x70085410`
- `0x9488` -> `0x70085488`
- `0xA4BC` -> `0x700864BC`

## DM strings
Runtime:
`70085364 freqchg+`, `70085370 freq`, `7008537C freqchg-`,
`70085388 memlack`, `70085390 polreq+`, `7008539C min_freq`,
`700853A8 max_freq`, `700853B4 polreq-`, `700853C0 freqreq+`,
`700853CC freqreq-`, `700853D8 dm_ext+`, `700853E0 dm_ext-`,
`700853E8 const_t`, `700853F4 dm_type`, `70085400 ipc_h`, `70085408 plug_st`.

## Globals
- `0x70086504`: current constraint pointer
- `0x70086508`: constraint pointer array
- `0x70086580`: flag
- `0x70086584`: arena allocation offset
- `0x70086588`: count/index
- `0x7008658C`: plugin context/global
- `0x70086590`: arena base
- arena size `0x2000`; end `0x70088590`
Allocation overflow logs `memlack` and reaches a fatal loop.

## dm_config and descriptors — confirmed layout
`dm_config` runtime `0x70085410`, local `0x9410`.
First word = 28. Descriptor[0] at config `+0x78`.
Descriptor size = `0x94`; 28 descriptors; array ends local `0xA4B8`.

Known descriptor offsets:
- `+00`: `0x101` for MIF and INT; observed `1` for others
- `+04`: sequential index
- `+08`: name
- `+18`: secondary/global domain ID
- `+70`: 1 only for CPU CL0/CL1L/CL1H/CL2
- `+74,+7C,+84,+8C`: four 8-byte list heads

Domains `(index, name, +0x18 ID)`:
0 dm_mif 0x00
1 dm_int 0x01
2 dm_cpu_cl0 0x02
3 dm_cpu_cl1l 0x03
4 dm_cpu_cl1h 0x04
5 dm_cpu_cl2 0x05
6 dm_npu 0x06
7 dm_npu 0x07
8 dm_dsu 0x08
9 dm_aud 0x0B
10 dm_gpu 0x0D
11 dm_intcam 0x0E
12 dm_cam 0x0F
13 dm_disp 0x10
14 dm_csis 0x11
15 dm_isp 0x12
16 dm_mfc 0x13
17 dm_mfc1 0x14
18 dm_dsp 0x17
19 dm_dnc 0x18
20 dm_GNSS 0x19
21 dm_ALIVE 0x1A
22 dm_chub 0x1B
23 dm_vts 0x1C
24 dm_hsi0 0x1D
25 dm_ufd 0x1E
26 dm_unpu 0x20
27 dm_icpu 0x16

## Export table — confirmed values
Runtime table `0x700864BC` / local `0xA4BC`:
- +00 `0x70085410` config
- +04 `0x70084D75` Thumb IPC handler -> `0x70084D74`
- +08 `0x700840AD` Thumb return-zero stub -> `0x700840AC`
- +0C `0`
- +10 `0x700848FD` Thumb external callback -> `0x700848FC`

`dm_plugin_start` working name, local `0x8000`: logs `plug_st`, initializes four list heads/domain,
uses config/export data, and establishes plugin context/callback state.
Local `0x80AC` is exactly `movs r0,#0; bx lr`.

## IPC dispatcher — high confidence
Working name `dm_ipc_handler`, local `0x8D74`, runtime `0x70084D74`.
Request is 16 bytes / four words.
`cmd = word0 & 0xff`; `domain = (word0 >> 8) & 0xff`.
Commands `0x00..0x0E` use TBH.

Correct initial targets:
00 8DC6
01 8E0E
02 8E0E
03 8E0E
04 8F6E
05 8F78
06 8DCC
07 8EDA
08 8EDA
09 8EDA
0A 8DE6
0B 8DE6
0C 8DE6
0D 8F3C
0E 8F4C

Literal pool begins at local `0x9348`:
- `0x10624DD3`
- `0x70086504`
- `0x700853E8` (`const_t`)
- `0x700853F4` (`dm_type`)
- `0x70085388` (`memlack`)
- `0x70086588`
- `0x70086508`
Strings begin at `0x9364`.

## Constraint object — confirmed size and many fields
Base allocation size `0x50`.

Safe layout:
```c
struct dm_constraint {
    uint32_t domain0;       // +00
    uint32_t id_or_domain1; // +04
    struct list_head node0; // +08
    struct list_head node1; // +10
    uint8_t flag18;         // +18
    uint8_t flag19;         // +19
    uint8_t flag1A;         // +1A
    uint8_t unknown1B;
    uint32_t inner_count;   // +1C
    uint32_t outer_count;   // +20
    uint32_t selected_index;// +24
    uint8_t flag28;         // +28
    uint8_t unknown29_3B[0x13];
    void *selected_table;   // +3C (32-bit target pointers)
    void **tables;          // +40
    uint32_t field44;       // +44
    uint32_t field48;       // +48
    void *runtime_object;   // +4C
}; // target size 0x50
```
When compiling on a 64-bit host, use explicit 32-bit target pointer representations or packed target-layout structs so assertions remain valid.

CMD01 decoding:
- request byte1 bit0 -> +28
- bit1 -> +18
- bit2 -> +19
- bit3 -> +1A
- byte1 high nibble -> +20
- request byte2 low nibble -> +24
- request word1 high byte -> +1C
Allocates `outer_count * 4` pointer array, then `outer_count` tables of `inner_count * 8`.
`+3C = tables[selected_index]`.

Earlier hypothesis that +28 was a name buffer is contradicted; do not use it.

## Pair table
Each table element is exactly 8 bytes:
```c
struct dm_pair { uint32_t value0, value1; };
```
Frequency relationship semantics are plausible but not proven.

CMD02 writes one pair into `current_constraint->tables[table][index]` after domain check.

CMD03 verifies current constraint domain, calls local `0x8A18` (working name `dm_register_constraint`),
stores constraint pointer into global array `70086508[count]`, returns count in response byte0, increments `70086588`.

Local `0x8A18` connects/registers constraints using domain lists. For ordinary links
(`flag28=0`), target descriptor `+0x20` counts incoming links. The routine rebuilds
a topological order: config `+0x04` is the queue length, config `+0x08` stores
domain indices, and descriptor `+0x1c` stores each emitted position. Only the
source-side `+0x74` lists contribute outgoing edges. These roles are supported
by the decompiled reads and writes; the function name is a working name.

When `flag18=1` and `flag28=0`, `0x8A18` allocates a second `0x50`-byte
constraint and `inner_count` reversed pairs from the selected table. The second
constraint swaps the two domains, sets flags `+0x18` and `+0x28`, links into
the original target's `+0x7c` and source's `+0x8c` lists, and is recorded in
the original constraint's `+0x4c` word. Its `+0x44/+0x48` words are `UINT32_MAX`.

CMD0B traverses descriptor list heads +74/+7C and serializes constraint metadata.
It is close to an inverse of CMD01:
- response byte0 <- constraint +04
- byte1 packs flag28, flag18, flag19, flag1A
- byte2 packs +20 low nibble and +24 high nibble
- byte3 <- +1C
- response words <- +44 and +48

CMD0C traverses same family and returns an 8-byte pair.
Indices from request word1:
bits0..7 constraint index; bits8..15 table index; bits16..23 pair index.

CMD09 traverses the target-side lists (+84/+8C), selects by request word1,
and serializes the selected constraint's source domain, two flags, and six
divided-by-1000 values. See `ghidra-followup.md` for the exact field mapping.

CMD07 reads descriptor +40/+50/+60/+64/+68 and uses magic `0x10624DD3` with UMULL/shift,
consistent with division by 1000. Exact field semantics remain unknown.

CMD04 calls `0x8544`, then `0x82d8`; CMD05 calls `0x886c`, then `0x862c`.
Both latter helpers propagate values across descriptor lists and pair tables.
Their framework effects are unresolved. CMD06 reaches the response path
directly; CMD0D stores request word1 at config `+0x10a8`; CMD0E selects a
table on a registered constraint when `flag1a` permits it.

## Analysis caveat
Ghidra was imported raw ARMv8-M with no global auto-analysis. A range script seeded Thumb every 2 bytes,
so literal pools and TBH tables can be falsely decoded as instructions. Trust known entries/control flow and
explicitly classify inline data.

## Legal hygiene
Keep proprietary Samsung binaries out of the reconstructed-source repository.
GPL-2.0-only may cover newly written reconstruction/project material but does not relicense Samsung firmware.
