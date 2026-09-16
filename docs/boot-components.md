# Additional boot component inventory, 2026-09-16

These files were supplied alongside `sboot.bin` under `ghidra_12.1.3_PUBLIC_20260817`. All analysis was read-only; no proprietary bytes are in this repository. Being in the same directory does not establish that every file belongs to the same SoC or firmware build.

| File | Size | Directly observed marker | SHA-256 |
|---|---:|---|---|
| `dtp.bin` | `0x300310` | `PLUG-IN-SIP`, `PLUG-IN_VER2`; AArch64-looking first words | `115b09026a861915afdcbb66d82f14c2180671f72584fd3272503dde1ca85de3` |
| `fld.bin` | `0x2c330` | `Exynos9815`, USB boot strings | `468732897f0694fc9d420fcc10692321b3e79711e6e7934957e6461f24e9c21a` |
| `harx.bin` | `0x200310` | `Exynos_H-Arx`, EL2/hypervisor strings | `9abe6bb37f5461354411f995ba46494a2881ab99aec6d196b94408f9d531c8d9` |
| `keystorage.bin` | `0x4310` | `SLSI` | `07068dba21475554a8f004bd8ef01055a0e1deed4b886f742ec876e16ef4548a` |
| `ldfw.img` | `0x166310` | `exynos9945 root`, `CryptoManagerV70` | `f973c985ed598c25643089b2edc7c583d3e7ba579f32ef19f18122bd73ef73dc` |
| `ssp.img` | `0xbc000` | No plain-text subsystem marker found | `7b45a045c0a3ef34cbbbcd656f9af720997148706a0686c7224459fce3e0bdc1` |
| `svm.bin` | `0x100000` | `BiEn1`, `svm` | `9df60bb16d17484dcce873b816c2141a1c0be6e956d8a77497889d0af1b4a293` |
| `tzar.img` | `0x580310` | `s5e9945root0`, `/lib64`, ELF content | `73eb665bc3f20b1832c82241ab43ad6d4d7098db5fea58683eed80071b8cf1ae` |
| `tzsw.img` | `0x180310` | `BiEn1`, `teegris` | `c24fef075bb3516557d360813df417349447cfa4a9fcf8068947c64d68c7bfd8` |
| `uh.bin` | `0x52400` | `GREENTEA` | `0027c1585ef86ef9caf8c67bc6386f3992486ed086520fc8385f004e6b47da06` |
| `up_param.bin` | `0x2ffb10` | POSIX tar archive with image names | `42c34b99bf8d44e52084c8fd4fe13682e94a6fda00888576246850a976dbc409` |
| `vbmeta.img` | `0x2390` | `AVB0` | `59ca95ed75b2127fa1c9751f7029f3219537e8dade54adae699c63fde6ea4593` |

## Targeted ACPM/M55 checks

Each additional file was searched case-insensitively for `acpm`, `CORTEXM23`, `CORTEXM55`, `dm_mif`, `dm_cpu_cl0`, and `FLEXPMU`; there were no hits. Exact byte searches also found no copy of the known 32-byte M23/M55 vector prefixes or the 20-byte M55 DM export table from S-Boot. A raw four-byte `0x70084000` pattern occurs once in `tzar.img` at unaligned offset `0x909b1`; without neighboring structure or references it is not evidence of a DM image.

`fld.bin` explicitly names Exynos9815, so it is not treated as evidence for S5E9945. `ldfw.img` and `tzar.img` carry S5E9945/9945 markers, but neither exposes the ACPM/M55 strings or exact image fragments tested here. Several images have high entropy near their starts; absence of a plain-text match does not rule out packing or encryption. The embedded images and relocation observations in `sboot.bin` remain the direct evidence for this reconstruction.
