# Successful upgrade route on my FZ-N1

This is a record of my working configuration, not a generic flashing recommendation. Commands are examples to execute manually only after every prerequisite is satisfied. Image names below are local aliases: obtain and verify your lawful private inputs first.

## 1. Prerequisites

- Device: FZ-N1ESEAZKJ; reported fastboot product FZ-N1EVU; current slot `a`.
- Stable USB, charged battery, battery cover installed and latch fully locked.
- Backups outside the phone. Bootloader unlocking and userdata formatting erase internal data.
- A known way to enter a computer-visible FastBoot Mode, with recovery images ready.
- OEM unlocking and USB debugging enabled where the original system permits them.
- ADB and fastboot; a matching Magisk 31.0 installation for the tested root configuration.

Use a single connected device or explicitly add `-s <your-device-serial>` to commands. Do not copy somebody else's serial number.

## 2. Fastboot access

On the working setup, `adb reboot bootloader` was usable. The original Android 9 setup required a device-specific preparation: after manually entering FastBoot Mode, the following vendor command successfully set NVM item 2556 to 0:

```sh
fastboot getvar oem-9499677799
```

Despite its `getvar` spelling, this is a **vendor-specific write**, not a read-only query. It is not a standard Android unlock command. It was followed by re-entering the bootloader and checking command availability. Do not repeat it when the required commands already work, or assume it applies to other models.

The initial manual key sequence was not reliably established. This guide does not invent one. Confirm FastBoot Mode with the computer before proceeding.

```sh
fastboot devices
fastboot getvar product
fastboot getvar current-slot
fastboot flashing get_unlock_ability
```

Expected on the tested device: `FZ-N1EVU`, slot `a`, unlock ability `1`.

## 3. Android 9 to the factory Android 11 configuration

The selected factory release was **18-03-006-009**. Its published SKU coverage is different from FZ-N1ESEAZKJ. The successful result on my device is evidence of one tested combination, not official model support.

| Partition | Expected capacity, bytes | Private input alias |
| --- | ---: | --- |
| modem_a | 115343360 | modem-android11.img |
| dsp_a | 16777216 | dsp-android11.img |
| system_a | 4026531840 | system-android11.img |
| vendor_a | 2147483648 | vendor-android11.img |
| boot_a | 67108864 | boot-android11-full-64MiB.img |

Query each with `fastboot getvar partition-size:<partition>`. Check the SHA-256 values in [the artifact manifest](ARTIFACTS.json). Stop if product, slot, capacity, or hashes differ.

Run one command at a time. Continue only after both sending and writing succeed; stop on any failure and keep the device connected.

```sh
fastboot -S 256M flash modem_a private/modem-android11.img
fastboot -S 256M flash dsp_a private/dsp-android11.img
fastboot -S 256M flash system_a private/system-android11.img
fastboot -S 256M flash vendor_a private/vendor-android11.img
fastboot -S 256M flash boot_a private/boot-android11-full-64MiB.img
fastboot reboot
```

This does not replace recovery, the B slot, or ABL/XBL/TZ bootloader firmware. The successful record includes flash acknowledgments and subsequent boot/network checks, not a complete five-partition readback audit.

After setup, authorize ADB again and check Android 11, build 18-03-006-009, and `sys.boot_completed=1`. Test Wi-Fi, camera, audio, and the scanner before moving on.

## 4. Bootloader unlock and root

Confirm backups before unlocking. The OEM toggle only allows unlocking; it does not mean the device is already unlocked.

```sh
adb reboot bootloader
fastboot flashing unlock
# Confirm on the device; internal data may be erased.
fastboot getvar unlocked
```

Require `unlocked: yes` before using modified images. The tested root setup used Magisk 31.0 to patch the matching factory Android 11 boot and flashed a verified 64 MiB padded image to `boot_a`.

```sh
fastboot flash boot_a private/magisk-patched-full-64MiB.img
fastboot reboot
adb install -r private/Magisk-31.0.apk
adb shell /debug_ramdisk/su -c id
```

Approve the shell request if shown. On this setup, `su` was under `/debug_ramdisk`; absence from the default shell PATH did not mean root was unavailable. Newly generated Magisk files need independent validation; matching names are insufficient.

## 5. Android 16 GSI

Tested system: **RestlessOS arm64-ab 16.0.0-202606230923**. It kept the factory Android 11 modem, DSP, and vendor. The actually tested GSI boot was `boot-early-diag-full-64MiB.img`, based on the Magisk Android 11 boot and including a startup-log collector. Its kernel/DTB/header were preserved. Use the artifact manifest to distinguish it from other boot files.

```sh
adb reboot bootloader
fastboot getvar product
fastboot getvar current-slot
fastboot getvar unlocked
fastboot flash boot_a private/boot-early-diag-full-64MiB.img
fastboot -S 256M flash system_a private/RestlessOS-arm64-ab-16.0.0-202606230923-sparse.img
```

The next commands **delete internal applications, settings, and files**. Prepare a clean data format only after backing up:

```sh
fastboot format:ext4 userdata
fastboot erase metadata
fastboot reboot
```

Complete setup, authorize ADB, install the full Magisk application again, and verify Android 16, boot completion, and root. This device used physical system/vendor partitions; do not apply unrelated super-resizing or fastbootd instructions.

## 6. Wi-Fi compatibility

The working configuration used the build-specific libcfg patch in `src/wifi`. It redirects the library's memcmp entry to a byte-wise comparator while preserving the surrounding configuration write/check and locking logic.

Build the comparator and patch **your matching private original** as described in [local build inputs](LOCAL_BUILD.md). The deployment script refuses non-matching hashes. It modifies `/vendor/lib64/libcfg.so`; it is not a temporary module overlay.

```sh
adb push private/libcfg.so /data/local/tmp/fzn1-libcfg.so
adb push private/libcfg-byte-safe.so /data/local/tmp/fzn1-libcfg-byte-safe.so
adb push src/wifi/apply-libcfg-fix.sh /data/local/tmp/apply-libcfg-fix.sh
adb shell /debug_ramdisk/su -mm -c 'sh /data/local/tmp/apply-libcfg-fix.sh'
```

Require the patch hash and `PATCH_INSTALLED_VERIFIED`, then reboot. Recheck Wi-Fi connectivity and the read-only vendor mount. The script attempts a read-only remount; an occupancy-related remount failure requires a reboot and verification rather than ignoring all script errors. Restoring factory vendor removes this patch.

## 7. Scanner/buttons integration

The working private package was **FZ-N1 Stock Apps and Scanner Suite v3-validation**, module ID `fz-n1-userbutton-priv`. It included BarcodeReader v51, UserButtonManager v3, UserButtonService v3, Diagnostic v11, runtime camera integration, targeted policy rules, and the button bridge.

The public `module-template` deliberately lacks proprietary APKs, HAL backend libraries, helper binaries, and the installer. **Do not ZIP and install the template.** The complete private module was installed through Magisk and verified after reboot. Migration from existing adapted apps was tested; a blank-userdata first deployment did not receive the same complete acceptance test.

## 8. Acceptance and rollback

Test the physical scanner, aiming/illumination, actual decoded content, multiple scans with separate history records, side-button operation, mapping persistence, Wi-Fi, ordinary cameras, and diagnostic screens. Scanner ID 2 must be used; camera 0 connection is not a substitute.

Save full `dumpsys media.camera` output instead of truncating the device-side pipe with `head`. Verify SELinux is enforcing and inspect scanner/button startup logs.

To return from GSI, restore the verified Android 11 boot, system, and original vendor, plus modem/DSP if those were changed. Prepare for another data format on downgrade; restoring images does not recover deleted user data. Do not relock with modified images installed.

Module disable and uninstall differ: disabling stops the unified integration after reboot; uninstall may restore a backed-up legacy scanner startup hook. Keep that behavior in mind when cleaning a migrated installation.

Sources: [Panasonic release page](https://jp-pc-support.connect.panasonic.com/dldocs/200108), [factory release note](https://dl-pc-support.connect.panasonic.com/public/toughpad/n1/ReleaseNoteforFZ-N1_18-03-006-009_SB.pdf), [tested RestlessOS release](https://github.com/cawilliamson/treble_restlessos/releases/tag/16.0.0-202606230923). These identify the original sources, not guaranteed current downloads or official support for my SKU.
