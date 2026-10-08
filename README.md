# Bringing the Panasonic FZ-N1 to Android 16

A personal engineering project: upgrading a Panasonic TOUGHBOOK FZ-N1 from Android 9 to a working Android 11 factory-image configuration, then running an Android 16 GSI while recovering the dedicated barcode scanner and programmable hardware buttons.

The goal was more than making a newer Android version boot. I wanted to keep the device useful as a handheld scanner, including its aiming light, illumination, repeat scans, scan history, and side-button operation.

**This repository documents a successful configuration on one FZ-N1ESEAZKJ. It is not an official Panasonic upgrade, a universal ROM, or a flash-ready firmware bundle.**

## Result

| Area | Verified result |
| --- | --- |
| Factory-image baseline | Android 11, build 18-03-006-009, using a selected five-partition combination |
| Custom Android system | RestlessOS arm64-ab 16.0.0-202606230923 GSI |
| Wi-Fi | Working after a build-specific libcfg compatibility patch |
| Dedicated SE4750 scanner | Real barcode/QR decoding, aiming and illumination |
| Repeated scanning | Multiple successive scans, with individual history records retained |
| Hardware buttons | Verified side-button scanning and supported button mappings |
| Persistence | Scanner and button integration survived a reboot through a unified Magisk module |
| Diagnostics | Adapted diagnostic screens and selected tests; some privileged operations remain limited |
| All factory applications | 19 APKs inventoried; only four adapted applications integrated, not all factory functionality restored |

The final module verification recorded four decoded scans and four visible history records, unchanged monitored application-data hashes, and no application crash in the final captured crash buffer. This is a bounded test result, not a guarantee of indefinite stability.

## What I worked on

- Identifying a compatible factory-image baseline while preserving the original bootloader chain and recovery.
- Checking image hashes, physical partition capacities, slot selection, and rollback materials before flashing.
- Bridging legacy native camera calls to the Android 16 framework, including AArch64 hidden-return ABI handling and framework-created attribution.
- Keeping the actual scanner camera ID instead of substituting an ordinary camera.
- Investigating scanner stop/restart behavior and recovering repeat scanning without losing decoded results or scan history.
- Building a GPIO/uinput button bridge and preserving application settings.
- Packaging runtime integration with startup checks, backups, rollback, and SELinux enforcement retained.

The project used ADB/fastboot, native disassembly, APK/smali analysis, C++/assembly, shell scripts, Python tooling, and physical-device verification. It was developed with AI-assisted investigation and implementation; conclusions were corrected when device evidence contradicted earlier assumptions.

## Repository map

- [My project experience](docs/PROJECT_STORY.md)
- [Successful upgrade route](docs/UPGRADE_GUIDE.md)
- [Architecture and engineering details](docs/ARCHITECTURE.md)
- [Verification and current limitations](docs/VALIDATION.md)
- [Source provenance and redistribution notes](NOTICE.md)
- [Local build inputs](docs/LOCAL_BUILD.md)
- `src/`: selected project-written camera, scanner, button, and Wi-Fi implementation snapshots.
- `module-template/`: source-only startup/rollback template; **not an installable module**.
- `tools/`: public-tree audit and source-manifest validation.

## Important boundaries

Factory firmware, proprietary libraries, modified factory APKs, signing keys, raw logs, and device-specific backups are deliberately excluded. A public copy of the complete working module would include proprietary payloads, so no module ZIP is distributed here.

The factory Android 11 images came from a release for another FZ-N1 SKU. Their successful use on my device does not establish official support for FZ-N1ESEAZKJ. The initial physical fastboot entry sequence was not reliably documented, so a verified entry method is a prerequisite, not something this guide promises to solve.

Google Play services was later observed repeatedly crashing in its Nearby Sharing component. That issue remains unresolved; the device should not be described as fully compatible with every Google or factory feature.

Read the guide and limitations before using any code. Do not flash unknown images, bypass the hash checks, or relock the bootloader with modified images installed.
