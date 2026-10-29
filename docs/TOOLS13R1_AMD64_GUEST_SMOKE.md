# TOOLS13R1 — AMD64 Native Guest Smoke

**Disposition: Outcome D — `TOOLS13R1_ENVIRONMENT_BLOCKED`.** This was a preflight and artifact-validation pass only. No guest boot, Developer Studio launch, UI test, or shutdown was attempted. A compatible disposable guest image could not be prepared from the specified Server checkout, and QAC admission could not be verified.

## Repository and qualification baseline

| Repository | Branch | Starting HEAD | Worktree at preflight |
| --- | --- | --- | --- |
| Developer Studio Tools source | `DEVELOPER_STUDIO_TOOLS` | `3d3feba2a460e52a765d713c876eda455e0bcc47` | clean |
| Server integration | `DEVELOPER_STUDIO_TOOLS` | `a13768dea9e5808baadb3c2908a8fca463505f16` | clean |

The TOOLS12 hosted qualification is documented in `docs/TOOLS12_NAVIGATION_HISTORY.md`. It records hosted UI checks, configured Debug and Release suites, and an AMD64 package audit. That hosted evidence is not native guest acceptance.

## AMD64 package identity

The recorded TOOLS12 AMD64 ELF was found at:

`D:\dev\guideXOS_DeveloperStudio_Tools_Source\out\TOOLS12\amd64-acceptance-stage\Apps\DeveloperStudio\bin\amd64\developerstudio.elf`

It is **1,262,228 bytes**, SHA256 **`9CBC32163417A553B38A1F01EF69CDBAF49B99C58B9D63D0F06E2E9A31C1AC16`**, matching the accepted TOOLS12 value. The AMD64 ELF in `out\TOOLS12\dual-architecture-package` has the same size and hash. No rebuild or substitution was made.

The corresponding package manifest identifies `com.guidexos.developerstudio`, version `0.1.0`, `NativeElf`, AMD64 entry `bin/amd64/developerstudio.elf`, entry point `gx_main`, ABI `guidexos-c-abi-v1`, runtime `native-elf`, and `minGuideXOSVersion` **`0.5.0`**. The AMD64 acceptance stage contains the AMD64 ELF. The assembled `dual-architecture-package` contains both architecture ELFs, and its AMD64 bytes match the accepted hash; the ARM64 entry is outside this phase's target.

The tracked production ELF in the Server integration checkout is **1,128,572 bytes**, SHA256 **`1387CB09F2026252E4A277E96256C2BB59D06AE70ACFAA56A84EFEFF2F9CF845`**. It differs from the accepted TOOLS12 artifact and was not overwritten or modified.

The prior TOOLS12 report records the stage directory, hash, package audit, and implementation commit `de921d3d2ba49f630295443d2236dfa7da34fec0`. The staged output has no separate provenance manifest tying its bytes to an exact compiler invocation. This phase reused it solely because its size and hash exactly match the accepted artifact; no new build inputs were introduced.

## Server compatibility and image preparation

The specified Server checkout's `kernel/core/include/kernel/version.h` identifies the kernel as **guideXOS 0.1.0**. The Developer Studio manifest requires guideXOS **0.5.0**. The tracked Server source stores `minGuideXOSVersion` when loading manifests, but this checkout did not expose a separate platform version or a version comparison that establishes this package as compatible. I therefore did not put the TOOLS12 ELF into a Server image, lower or rewrite the manifest minimum, or bypass compatibility checks.

The checkout has OVMF files, QEMU scripts, test disks, and `ESP\ramdisk.img`; these do not establish a compatible, isolated guest image for this package and Server version. No image was built or modified, so there is no guest image path or SHA256. Server build identification is the specified clean checkout at `a13768dea9e5808baadb3c2908a8fca463505f16`, with kernel version 0.1.0.

A bounded fixture was prepared outside the Server checkout at:

`D:\dev\guideXOS_DeveloperStudio_Tools_Source\out\TOOLS13R1\fixture\DeveloperStudioNavigationSmoke`

It contains `main.cpp`, `helpers.cpp`, `helpers.h`, and a README; the sources include a structure, a helper function, TODO and FIXME markers, `TOOLS13R1_KNOWN_SEARCH_ID`, and multiple navigable lines. The fixture was not copied into a guest image. The actual guest workspace root remains unverified, so this host staging path is not being represented as a guest path.

## QAC baton and admission

QAC's documented coordination procedure requires an online coordinator and a QAC-managed acceptance reservation before a participating launcher starts QEMU. The available CLI apphost is:

`D:\dev\guideXOS_QAC\src\GuideXOS.QAC.Cli\bin\Debug\net10.0-windows\GuideXOS.QAC.Cli.exe`

Read-only `qac status` and `qac list` calls both returned **“Coordinator unavailable: Coordinator connection timed out or is unavailable.”** No TOOLS13R1 reservation was requested, and no QEMU command was run.

The newest local coordinator log, `C:\Users\guideX\AppData\Local\guideXOS\QAC\logs\coordinator-20261028-191428-003-1388.jsonl`, records an acceptance reservation for project `guideXOSServer` (`f65b6276455f48bd9cea9e554260b041`) and a registered direct child PID `7992` at 2026-10-29 02:16 UTC. The log ends with an IPC `IOException`; it contains no child-completed or reservation-released event. The recorded coordinator and child PIDs are now absent/reused, while the persisted reservation journal is stale (last modified October 22) and cannot prove release. The log does not identify the responsible human track. Thus neither an explicit baton release nor a currently authorized independent lane could be confirmed. No process or reservation was stopped, released, or otherwise altered.

**QAC lane status: pending/unverified.** This is not guest acceptance. Because the image compatibility gate is also unresolved, this phase is classified Outcome D rather than “ready guest pending baton.”

## Guest results and evidence

No guest was launched. Consequently:

- Kernel, desktop/compositor, input services, package recognition, Developer Studio startup/window, and native stability: **not observed**.
- Tests A–E (Tools menu, Go to File, Go to Line, Task List, and Back/Forward): **not performed**.
- Optional utility checks and replacement safety: **not performed**.
- Serial output, screenshots, application diagnostics, shutdown result, and QAC release evidence: **none generated for TOOLS13R1**.
- No guest failure or application exception was observed; this is not evidence of a pass.
- Shutdown and cleanup were not applicable because this phase started no guest and acquired no reservation.

## Follow-up

Resolve the Server/application version contract and identify a production-compatible Server target whose version satisfies the package minimum, then prepare an isolated image and verify its hash. Separately, obtain current QAC status and an explicit release or an independently authorized lane through the documented coordinator procedure. After both gates are clear, resume the bounded TOOLS13R1 guest smoke using this staged fixture and the exact accepted ELF. Do not begin TOOLS13R2 automatically.

No Compilation, Debugging, Server, or QAC implementation was changed. No push was made.
