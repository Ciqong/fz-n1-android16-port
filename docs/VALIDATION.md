# Verification and limitations

## Recorded final acceptance

- Unified module auto-started after reboot.
- Camera IDs 0, 1, and 2 were present.
- Scanner and button integration reported ready.
- The owner confirmed normal repeated side-button scanning.
- Four decoded records were observed in the captured logs and four history entries on screen.
- Monitored application-data hashes matched before/after migration.
- No relevant application crash was found in the final captured crash buffer.

Raw logs, device identifiers, scan contents, account data, and database hashes are not published. This is a sanitized summary of the local records; readers cannot independently validate every private raw trace from this repository alone.

## Not claimed

- Universal support across FZ-N1 SKUs or other Android 16 builds.
- A complete source build of all final APKs, vendor libraries, or the private module.
- Full five-partition readback after the original Android 11 installation.
- Full factory-app restoration: 19 APKs were inspected, four adapted apps were integrated.
- Full testing of a fresh userdata deployment of the unified module.
- Every programmable action, privileged diagnostic switch, or external accessory verified.
- A proven fixed manual key combination for first-time fastboot entry.
- Long-term stability or security certification.

## Known open issue: Google Play services

The installed version 26.32.34 repeatedly threw `RejectedExecutionException` in Nearby Sharing's ReceiveSurfaceChimeraService after a thread pool had shut down. Quick Share receiving visibility was already off. The captured exception identifies the immediate failure but does not prove whether the underlying cause is that Play services version or ROM compatibility. No fix was validated in the project records.
