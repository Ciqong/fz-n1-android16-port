# Local build inputs and source scope

This is a source/documentation release, not a turnkey build tree. The public code was selected from the working project's implementation snapshots. Chinese inline comments in original implementation files have been preserved except for obsolete introductory ABI labels; the public documentation is English.

## Wi-Fi patch

The most self-contained component is `src/wifi`. Provide your legally obtained matching `libcfg.so` privately. Its SHA-256 must match the manifest.

Using an AArch64 assembler/toolchain, assemble `byte_memcmp.S` and extract only `.text` to `byte_memcmp.bin` (44 bytes). Put the private original and comparator binary beside a local copy of `patch_libcfg.py`, then run Python 3. The script checks the original hash, code cave, PLT instruction, and program-header layout before writing `libcfg-byte-safe.so`.

Keep that build workspace outside the public tree or under ignored `private/`. Do not commit the original or patched proprietary library.

## Camera / scanner / buttons

- Use a matching Android NDK/libc++ and target architecture; private framework layouts are build-specific.
- `src/camera/br_bridge.cpp`, `forward.S`, and `diag.cpp` are snapshots with integration dependencies. Additional adapted APK/JNI wiring, build definitions, and vendor inputs are required.
- `src/scanner/epoch_gate_v2.cpp` documents one implemented lifecycle component; it is not a standalone scanner driver.
- `src/buttons/button_bridge.cpp` implements the native event bridge; it also needs the adapted service, policy/permissions, and startup integration.
- Compilation alone is not proof of ABI compatibility. Test caller layout, return ownership, cleanup, and actual hardware behavior separately.

## Module template

The template intentionally excludes:

- `apps/` and `system/` proprietary APK payloads;
- scanner HAL proxy/backend binary payloads;
- Magisk helper binaries and upstream installer functions;
- compiled native button executable;
- private signing materials and device backups.

Startup scripts reference these missing inputs and are retained for architecture review. Do not install the public template. A complete private package requires matching lawful inputs and a separate device-side validation.

## Public-tree checks

Run from the repository root:

```sh
python tools/audit_public_tree.py
```

This validates the source file manifest and checks for binary/key artifacts and common credential patterns. It does not replace human review and cannot detect every possible secret.
