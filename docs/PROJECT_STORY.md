# My experience: keeping an industrial Android handheld useful

## Starting point

I began with a Panasonic FZ-N1ESEAZKJ running Android 9, build 16-03-006-013. The device had a dedicated scanner and programmable buttons, which made it more interesting than an ordinary phone. My aim was to run a newer Android system while retaining those hardware features.

## Establishing a baseline

The working intermediate system used factory Android 11 build 18-03-006-009. Rather than replacing the entire bootloader chain, the successful configuration wrote selected modem, DSP, system, vendor, and boot images to the A slot. I retained recovery and rollback materials and checked the exact files and partition sizes.

This was a device-specific compatibility project, not a supported OTA upgrade for the model printed on my device. That distinction matters when sharing the procedure with other owners.

## Moving to a GSI

The custom system was RestlessOS Android 16, keeping the Android 11 vendor/radio foundation and a verified Magisk-based boot image. A clean userdata format was part of the successful system change. Wi-Fi needed a small native-library compatibility patch before it worked reliably in the tested configuration.

## Restoring the scanner

Opening a regular camera was not enough. The application requested scanner camera ID 2, and a successful connection to camera 0 did not prove scanner functionality. The actual goal required the dedicated scanner's aiming pattern, illumination, decode results, and repeat operation.

The camera bridge required two separate ABI boundaries: the factory application's call into the shim and the shim's call into Android 16. Both needed the correct non-trivial return convention. The new API also required the right target SDK, device policy, and attribution created by the application's framework context.

After connection worked, scan stop/restart behavior and the UI's result handling still needed attention. The final physical tests demonstrated successive scans and one history entry per decoded result.

## Restoring the buttons and persistence

Saving a button mapping and receiving a physical key event are different problems. The implementation bridged the GPIO input device, passed unrelated keys through uinput, and dispatched supported actions to the adapted service. Side-button scanning was then tested on the device.

The final integration combined scanner startup, button support, and four adapted applications into a Magisk module. A reboot followed by physical scans verified that the working setup was persistent.

## What I learned

- A normal process exit is not a successful camera connection.
- A non-null native object is not proof of parameter reads, cleanup, or barcode decoding.
- A symbol name alone does not establish a private C++ ABI.
- A standalone probe can reproduce its own assumptions rather than the real application's calling convention.
- Missing log output is not proof that a code path did not run.
- Hashes, service identity, real hardware actions, and explicit rollback checks are stronger than optimistic status messages.
- Successful tests should be reported with their limits, especially when proprietary components and undocumented interfaces are involved.

This repository preserves the practical outcome and selected implementation work without publishing firmware, personal data, or every intermediate experiment.
