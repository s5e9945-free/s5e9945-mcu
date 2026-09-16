# Memory map

## Candidate ACPM / boot flow
- `0x90028000..0x90074000`: S-Boot `acpm_dump`, confirmed size 0x4C000.
- `0x0203F000..0x0208F000`: 0x50000 region backed up before overwrite.
- `0x1356B000..0x135BB000`: backup/staging.
- file `0xCA000..0x116000`: candidate 0x4C000 ACPM bundle (strong inference).
- file `0xCA000`: Cortex-M23 #1.
- file `0xE0000`: Cortex-M23 #2.
- file `0x11A000`: separate Cortex-M55 image.

## M55 DM relocation
For the DM plugin section:
`runtime = local + 0x7007C000`.

| local | runtime | observation |
|---|---|---|
| 0x8000 | 0x70084000 | plugin start |
| 0x80AC | 0x700840AC | return-zero stub |
| 0x88FC | 0x700848FC | external callback |
| 0x8A18 | 0x70084A18 | constraint registration |
| 0x8D74 | 0x70084D74 | IPC dispatcher |
| 0x9364 | 0x70085364 | strings |
| 0x9410 | 0x70085410 | dm_config |
| 0x9488 | 0x70085488 | descriptor[0] |
| 0xA4B8 | 0x700864B8 | descriptors end |
| 0xA4BC | 0x700864BC | export table |
| 0xA504 | 0x70086504 | current constraint pointer |
| 0xA508 | 0x70086508 | pointer array |
| 0xA584 | 0x70086584 | arena offset |
| 0xA588 | 0x70086588 | count |
| 0xA58C | 0x7008658C | context/global |
| 0xA590 | 0x70086590 | arena base |
| 0xC590 | 0x70088590 | arena end |

Export table at runtime 0x700864BC:
`70085410, 70084D75, 700840AD, 00000000, 700848FD`.
