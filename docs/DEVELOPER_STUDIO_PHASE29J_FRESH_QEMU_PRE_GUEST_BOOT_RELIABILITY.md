# Developer Studio Phase 29J — Fresh QEMU Pre-Guest Boot Reliability

## Result

Phase 29J adds process, firmware, loader, kernel, UART, and NativeElf evidence to the fresh-QEMU runner. On the new launches, the trace now distinguishes firmware boot, UEFI loader entry, kernel handoff, serial initialization, and NativeElf dispatch from an empty serial file. Four formal boots reached those boundaries and the Developer Studio guest; the first three passed the requested Phase 29I sentinel/request checks. The fourth also accepted the sentinel and handed off one `/P28Q` request, then exposed a later repeated Phase 29E manifest path and a Phase 29C `load_in_progress` failure. The runner stopped there; boot 5 was not run.

The original Phase 29I silent boot is **not conclusively explained or reproduced**. The one Phase 29I `SENTINEL_PATH_INVALID` observation also did not recur after detailed normalization evidence was added. The earliest unresolved boundary is now the repeated manifest/project-open path on formal boot 4, after firmware, loader, kernel, UART, NativeElf, Developer Studio startup, sentinel recognition, and one request handoff all succeeded.

**Classification: Outcome B.** Boot progress is now deterministically classifiable with independent debugcon and serial evidence, but the consecutive 5/5 boot/sentinel acceptance gate was interrupted by a downstream Developer Studio transaction failure. No 10-boot or 25-cycle run was started.

## Repository baseline and preserved files

| Repository | Branch | Starting HEAD |
|---|---|---|
| Standalone Developer Studio | `main` | `aeffdc130557f5d19db6c74c052d20086fe989ad` |
| Server integration | `v0.5_DEVELOPER_STUDIO` | `0df7686c2383551e4303462c9a9ba5bae90b495f` |

Both local heads matched their upstream refs at inspection (`0 ahead / 0 behind`), despite the Phase 29I note describing each as one commit ahead. Branches, remotes, and authentication were not changed. Existing standalone untracked files remain excluded and untouched: the Phase 3B and Phase 15 JSON pairs, `tests/fixtures/debugger-phase15-h29-0927/`, and `tests/fixtures/debugger-phase15-h29-0927-phase29i-hosted-run-1/`. The server’s restored 17-byte `ESP/Apps/DeveloperStudio/.phase28q-diagnostic` remains untracked and excluded; its SHA-256 is `967D29A9A500A3108D7331BA47A5E2053A7C2203B81A345BE990084F3F43698A`.

## Phase 29I evidence retained before code changes

The Phase 29I baseline had one successful fresh boot, followed by boot 2 with no serial-visible guest output. Its failed boot identity was:

| Field | Retained evidence |
|---|---|
| Stage ID | `79023af2867a4d69b0008cbd835ae351` |
| ESP | `C:\Users\guideX\AppData\Local\Temp\guidexos-phase28g-b4fcd2df5b9e457c99da7f716f24741f\esp-boot2-79023af2867a4d69b0008cbd835ae351` |
| Kernel | `kernel.elf`, SHA-256 `3715A42C6E72F311034191671B56659372186F7489A2C332DB3D68A06423DB2C` |
| UEFI loader | `EFI\BOOT\BOOTX64.EFI`, SHA-256 `26B934B38521F014EAF76F54997C46B5CBC368082781E84D3916A467BFFE46C7` (hashed from the retained staged ESP) |
| Sentinel | 17 bytes, exact content, SHA-256 `967D29A9A500A3108D7331BA47A5E2053A7C2203B81A345BE990084F3F43698A` |
| Full tree SHA-256 | `DE4B99DBEDA5A32C27640F420CAA2CE264ABCE4751E2E8CD20E0CBA4A2C6D21B` |
| ESP identity | `9915597C04B1DFDDD28EDBEB5FFB382D7F8D0C38ED2CC5B023E4348992DF60BF` |
| Serial | `boot2.serial.log` does not exist; size unavailable because no file was created |
| Other captures | No retained stdout, stderr, QEMU debug/reset log, PID/exit code, process-at-timeout record, or loader side-channel record |

The exact historical process state is unavailable. An empty/missing serial log cannot establish that no guest code ran. The old runner’s command was reconstructed from its launch code; its actual argument string was not saved:

```text
"C:\Program Files\qemu\qemu-system-x86_64.exe" -machine pc,usb=off -drive if=pflash,format=raw,readonly=on,file=D:\dev\guideXOSServerV0.5_DEVELOPER_STUDIO\OVMF.fd -drive file=fat:rw:C:\Users\guideX\AppData\Local\Temp\guidexos-phase28g-b4fcd2df5b9e457c99da7f716f24741f\esp-boot2-79023af2867a4d69b0008cbd835ae351,format=raw,if=ide,index=0 -m 4096M -vga std -serial file:C:\Users\guideX\AppData\Local\Temp\guidexos-phase28g-b4fcd2df5b9e457c99da7f716f24741f\boot2.serial.log -display none -no-reboot -no-shutdown
```

The retained failed ESP still contains the audited kernel, loader, and sentinel. Its input matched Phase 29I boot 1’s kernel/tree identity; only the per-boot stage/ESP identity differed. No retained serial or process evidence locates the first execution divergence.

The immediately preceding Phase 29I boot 1 was successful: stage ID `d47bb6c58bdf4ca6b59cbff2b5ca5baa`, ESP `C:\Users\guideX\AppData\Local\Temp\guidexos-phase28g-b4fcd2df5b9e457c99da7f716f24741f\esp-boot1-d47bb6c58bdf4ca6b59cbff2b5ca5baa`, ESP identity `32B6A606180D33AC94568EC3902BFFFCF4743A7CA230FEBE16321AF4CAB58DF3`, and tree SHA-256 `DE4B99DBEDA5A32C27640F420CAA2CE264ABCE4751E2E8CD20E0CBA4A2C6D21B`. Its kernel SHA-256 matched boot 2 (`3715A42C...`); its AMD64 Developer Studio payload was `C59CBE0E4823C668399284E5867BFB8C35F5440B105910010ADB300783B3E896`. Serial showed OVMF selecting the IDE disk, FAT32 mounted at `/` from `ata0m`, the app ELF loaded, the exact sentinel stat/read accepted, diagnostic mode enabled, one `/P28Q` request, and Phase 29C through `ready`. Boot 2 had the same full tree hash but a distinct ESP identity and no serial-visible guest evidence. Its old command used the same argument form shown above with the boot-2 ESP and `boot2.serial.log` paths.

## Firmware, boot device, and loader marker meaning

QEMU uses OVMF/UEFI, not legacy BIOS. The firmware file is `D:\dev\guideXOSServerV0.5_DEVELOPER_STUDIO\OVMF.fd` (3,653,632 bytes; SHA-256 `2DF617A6FD1BAE41EC7C0E3BCECA96C928A5A88D8DC198468D34733393AB2792`). QEMU is `C:\Program Files\qemu\qemu-system-x86_64.exe`, version `11.0.0 (v11.0.0-12122-ga4bb4b10c9)`. The UEFI pflash is read-only. The only bootable data device is the requested directory-backed FAT drive at IDE index 0; there is no attached CD-ROM, network boot device, or other disk. There is no explicit `-boot` option. In each observed boot OVMF reported `BdsDxe: starting Boot0001 "UEFI QEMU HARDDISK QM00001"` from `Ata(Primary,Master,0x0)`, followed by the guideXOS loader. The selected device therefore matched `if=ide,index=0`; no device-model or boot-order change was justified.

The existing `P28Z BOOT 01 native_loader_entered` is emitted in `kernel/core/native_elf/native_elf_loader.cpp` after ELF validation, application context and dedicated stack setup, and immediately before the `gx_main` trampoline call. It proves the kernel reached the NativeElf dispatch path for an application. It does **not** prove firmware entry, UEFI loader entry, kernel entry, or that the application’s `gx_main` body started. `P28Z APP 00 gx_main_entry_raw` is the later application-entry evidence.

## Phase 29J instrumentation and staging invariants

The runner now records a bounded host trace per boot: unique stage ID and source, staging completion, final tree audit, a pre-spawn tree recheck, exact QEMU command, QEMU PID/start time, process-alive observation, serial/debugcon destinations and first-byte times, process/stop state, capture sizes, CPU reset record count, and one deterministic classification. It stores stdout/stderr and serial/debugcon/QEMU reset logs for Phase 29I-focused runs. The helper module builds arguments using the exact requested ESP path and classifies spawn failure, QEMU exit, absent firmware handoff, firmware-without-loader, loader-without-kernel handoff, UART initialization/output failure, early guest hang/exit, repeated firmware/loader reset entry, and NativeElf reachability. Hosted helper tests cover unique stage IDs, exact drive selection, loader detection, spawn and empty/live/exited outcomes, reset/re-entry cases, and each later marker boundary.

For every formal boot, the final ESP audit and pre-spawn tree SHA-256 matched. The copied stage ID/path was unique; kernel, loader, sentinel, and source app hashes were logged; the same absolute ESP path appears in the exact `-drive` argument. The runner performs no host-side ESP mutation after the final audit. Each boot is a private copy so guest writes cannot become the next boot’s input. The QEMU drive remains `fat:rw:` for existing guest lifecycle behavior; guest-side writes can modify that boot’s private staging directory after launch. Thus the proven immutability invariant is **no host-side staging mutation after audit and no cross-boot reuse**, not a read-only guest filesystem.

The runner records whether QEMU is alive or exited and captures its exit code and CPU reset log. It does not use QMP, so a live paused VM cannot be distinguished from a live CPU-spinning VM solely from process state. Those sub-states are not claimed by the classifier.

The original runner rebuilt/restaged the working ESP before copying it to a unique per-boot directory. In Phase 29I-only mode the canonical untracked sentinel is snapshotted and restored around that in-place setup. The final audit occurs after staging writes, package copies, and sentinel restoration/copy, and the exact audited copy is used by QEMU.

## Earliest guest execution and serial audit

The bounded debugcon path uses QEMU ISA debugcon port `0xE9`, independent of COM1 serial output. The sequence is:

| Marker | Meaning |
|---|---|
| `P29J GUEST 01 uefi_loader_entry` | custom UEFI loader entry |
| `02 kernel_image_loaded` | kernel ELF loaded |
| `03 exit_boot_services_complete` | UEFI Boot Services ended |
| `04 kernel_handoff_invoke` | loader is about to hand off to kernel |
| `05 kernel_entry_before_uart` | kernel entry reached before UART initialization |
| `06 kernel_uart_initialized` | kernel COM1 configuration completed |
| `07 native_loader_dispatch_ready` | NativeElf is prepared immediately before `gx_main` |
| `08 gx_main_invoke` | NativeElf trampoline call is next |

The UEFI loader configures COM1 at `0x3F8`, divisor 1 (115200 baud), 8N1. The kernel configures the same COM1 settings. Both formerly unbounded transmitter-ready waits are now bounded at 65,536 probes. A stalled UART byte is sent to debugcon and the guest continues. An audit also found and bounded a dormant diagnostic serial helper and an unused MSVC trampoline serial helper; the active kernel and UEFI paths are the production paths used by this run. The serial timeout fallback did not fire in the successful formal traces. Debugcon markers prove execution even before COM1 is initialized.

## New fresh boot and sentinel evidence

One diagnostic boot after rebuilding the app with detailed sentinel normalization telemetry passed: the sentinel path normalized, stat found a regular 17-byte file, read all 17 bytes, accepted exact content, enabled diagnostic mode, and the project reached Phase 29C `ready`. It was diagnostic evidence and is not counted toward the formal gate.

The formal command was:

```powershell
scripts/smoke-compiler-bootstrap.ps1 -Phase29ISentinelOnly -BootCount 5 -TimeoutSeconds 120
```

The first four unique stages had the same content tree and different ESP identities. Kernel SHA-256 was `7B35E6407FDCFFD3543D26CDE516273838F1E26D26F8B59A0C7918806B554CC9`; UEFI loader SHA-256 was `9F41AE22D34C87A51A1F9591DEE447DD80A97E2706D56D223B38C654960ABF11`; Developer Studio AMD64 payload SHA-256 was `C841ACC41D4B9518B24CA2C0F63129859CDF2E1F1CB39653F6B58D2FDE5D4B50`; the staged sentinel SHA-256 was `967D29A9A500A3108D7331BA47A5E2053A7C2203B81A345BE990084F3F43698A`.

| Boot | Stage ID | Exact ESP path suffix | Full tree SHA-256 | Boot/guest result |
|---|---|---|---|---|
| 1 | `575570344f204fabae8e7b7c7ffb22af` | `esp-boot1-575570344f204fabae8e7b7c7ffb22af` | `281A73BD8DB9629B6FA3DF2EF7018F0B0EDFB37BE4594D02788FFD1CE5FD58F0` | Passed boot, sentinel, one-shot startup request; Phase 29C reached `ready` |
| 2 | `1f71a9f850f34552adce52189808a4d2` | `esp-boot2-1f71a9f850f34552adce52189808a4d2` | same | Passed boot, sentinel, one-shot startup request; Phase 29C reached `ready` |
| 3 | `6f4a61ff775e4fb7b88eb29d3b1d0bb3` | `esp-boot3-6f4a61ff775e4fb7b88eb29d3b1d0bb3` | same | Passed boot, sentinel, one-shot startup request; Phase 29C reached `ready` |
| 4 | `93bc5af1ee1f4e20b64393be97f85909` | `esp-boot4-93bc5af1ee1f4e20b64393be97f85909` | same | Boot and sentinel passed; one `/P28Q` request reached `load_started`; later duplicate manifest validation was followed by Phase 29C `load_in_progress`; wider gate stopped |
| 5 | — | — | — | Not run after boot 4 failed the wider proof |

The exact boot-4 ESP path was:

```text
C:\Users\guideX\AppData\Local\Temp\guidexos-phase28g-806518b997864430bef4386cdaf8c9d0\esp-boot4-93bc5af1ee1f4e20b64393be97f85909
```

Its exact QEMU command was:

```text
"C:\Program Files\qemu\qemu-system-x86_64.exe" -machine pc,usb=off -drive if=pflash,format=raw,readonly=on,file=D:\dev\guideXOSServerV0.5_DEVELOPER_STUDIO\OVMF.fd -drive file=fat:rw:C:\Users\guideX\AppData\Local\Temp\guidexos-phase28g-806518b997864430bef4386cdaf8c9d0\esp-boot4-93bc5af1ee1f4e20b64393be97f85909,format=raw,if=ide,index=0 -m 4096M -vga std -serial file:C:\Users\guideX\AppData\Local\Temp\guidexos-phase28g-806518b997864430bef4386cdaf8c9d0\boot4.serial.log -debugcon file:C:\Users\guideX\AppData\Local\Temp\guidexos-phase28g-806518b997864430bef4386cdaf8c9d0\boot4.debugcon.log -global isa-debugcon.iobase=0xe9 -d cpu_reset -D C:\Users\guideX\AppData\Local\Temp\guidexos-phase28g-806518b997864430bef4386cdaf8c9d0\boot4.qemu-debug.log -display none -no-reboot -no-shutdown
```

Boot 4 ran as PID `25712`; the host observed it alive, then terminated it after the required marker. It had not exited before the harness stop (`exited_before_harness_stop=0`, `alive_at_timeout=0`, `harness_stop=required_marker_observed`, recorded process exit code `-1` after harness termination). The serial file existed and was 105,664 bytes; debugcon was 1,157 bytes; QEMU reset log existed with two initial CPU-reset records; stdout and stderr were empty. The trace classified `NATIVE_LOADER_REACHED`. The two CPU-reset records did not constitute a reset loop: firmware boot/loader entry did not repeat. The guest trace proves UEFI, kernel handoff, UART init, NativeElf dispatch, and `gx_main` invocation. Firmware serial showed Boot0001 on the intended ATA Primary/Master drive.

On this boot, the sentinel path normalized as `/Apps/DeveloperStudio/.phase28q-diagnostic`; stat found a regular file of size 17; read returned 17/17 bytes; exact content was accepted; and diagnostic mode was enabled. Phase 29D emitted one request-created/submitted/accepted/handoff sequence for `/P28Q`, and Phase 29C emitted one `load_started`. Phase 29E then emitted its manifest/read/identity set twice; the later project path ended in `load_in_progress` and `DEVELOPER_STUDIO_PHASE28Q_FAILURE`. This is the first later Developer Studio blocker. The boot harness stopped at boot 4 and did not launch boot 5.

## Sentinel regression follow-up

Before detailed telemetry was added, a separate Phase 29I-focused run reached the guest on boot 4 and emitted `PATH_NORMALIZATION_ERROR reason=SENTINEL_PATH_INVALID`; VFS readiness and mount readiness were already proven. The stage was `3df272bbc9dd46ebbd1e358d342a4988` in `C:\Users\guideX\AppData\Local\Temp\guidexos-phase28g-f602f50b32544a8686def3ba9259df9d`. That guest then returned `-4`. A reducer-level diagnostic was added without changing path-normalization rules: it records input length, first byte, failure category, and failure offset. The standalone sentinel test covers absolute-path rejection, traversal, output-capacity failure, and successful canonical input. The next diagnostic boot and formal boots 1–4 all normalized and accepted the same canonical path. Therefore the isolated PathInvalid event is not reproduced and has no proven cause; no speculative sentinel behavior change was made.

## Release CTest and package validation

| Check | Result |
|---|---|
| Debug CTest | **33/33 passed** |
| Release CTest | **26/33 passed**; failures: model, project, navigation, symbol, relationship, debugger structured variables, and conditional breakpoints |
| Release failure class | Test configuration/assumption issue: the Release Ninja flags contain `-DNDEBUG`, while the failing tests place setup calls inside `assert(...)`. Those calls disappear; dependent test code then segfaults or, for project, throws `std::out_of_range`. This is not evidence of an optimized production package defect; Release tests remain a follow-up and are not reported as passing. |
| Targeted sentinel tests | Debug CTest and the standalone build-script’s 12 focused sentinel cases passed. The Release sentinel executable exited 0, but `NDEBUG` removes its `assert` checks, so that result is not treated as meaningful assertion coverage. |
| Phase 29J PowerShell helper tests | Passed: unique IDs, exact ESP argument, process/classifier boundaries, reset/re-entry evidence, and debugcon marker detection |
| PowerShell parser | Passed for the modified QEMU runner, evidence module, and helper test |
| Bare DWARF capacity | **PASS, dies=397** |
| AMD64 package audit | PASS; ELF64 little-endian AMD64 ET_EXEC, sectionless, no debug sections; 1,055,116 bytes; SHA-256 `C841ACC41D4B9518B24CA2C0F63129859CDF2E1F1CB39653F6B58D2FDE5D4B50` |
| ARM64 package audit | PASS; ELF64 little-endian AArch64 ET_EXEC, sectionless, no debug sections; 1,220,524 bytes; SHA-256 `FDA1D8AB9149E3FA8D71606731E03EFD87A57B254089727F5915E93610DC95C7` |
| Runtime package contents | Exactly `app.json`, `bin/amd64/developerstudio.elf`, and `bin/arm64/developerstudio.elf` |

The AMD64 and ARM64 packages were rebuilt from the instrumented standalone source. The Release CTest failure pattern is independent of the QEMU boot and sentinel path; it was not changed in this phase.

## Later gates and separate hosted boundary

| Gate | Result |
|---|---|
| Focused early guest path | **4/5 reached NativeElf and Developer Studio startup; gate incomplete/failed at boot 4’s later proof failure; boot 5 not run.** |
| Phase 29I sentinel/request checks | **4/5 formal boots observed through sentinel acceptance, one Phase 29D `/P28Q` handoff, and Phase 29C `load_started`; boot 5 not run.** Boot 4 then failed the broader Phase 29E/29C validation. |
| Phase 29D request | One request reached accepted/handoff on formal boots 1–4; duplicate/re-entry marker was absent on boot 4. |
| Phase 29C project transaction | Boots 1–3 reached `ready`; boot 4 later emitted repeated Phase 29E identities and failed with `load_in_progress`. |
| Phase 29H artifact/symbol snapshot | Not reached |
| Phase 29F start / Phase 29B RUNNING | Not reached |
| Phase 29G return | Not reached |
| Phase 29A stop mapping | Not reached in this focused run |
| Hosted breakpoint | Not rerun. Phase 29I’s separate result remains `No symbols`, no source association/mapping, and no `debug_breakpoint=PENDING`; retain as an independent candidate. |
| Full Phase 28Q QEMU acceptance | **0/10; not started** because the focused gate did not pass 5/5 |
| 25-cycle lifecycle stress | **0/25; not started** |
| Physical hardware | **Not tested** |

## Changes and root cause

Server changes add the P29J debugcon milestones in UEFI and kernel execution, bound active COM1 transmit waits with debugcon fallback, bound two audited dormant serial helpers, and add the host trace, per-boot audit/hash capture, exact-ESP argument construction, process/reset classification, and hosted regression tests. Standalone changes add detailed path-normalizer error data and emit it only when the Phase 29I path check fails; the normalization policy is unchanged. Both architecture packages were rebuilt from these sources.

The retained Phase 29I boot-2 failure remains **unclassified at its historical process/guest boundary** because no serial file, QEMU process result, stdout/stderr, firmware/reset log, or loader side-channel was preserved. The Phase 29J diagnostics prove that later QEMU launches progress from process spawn through OVMF boot selection, UEFI loader, kernel, UART, NativeElf, and Developer Studio. The separate PathInvalid event has not recurred with input/failure telemetry enabled. The current unrepaired failure category is **G — Later Developer Studio blocker**, specifically duplicate Phase 29E manifest evidence followed by Phase 29C `load_in_progress` on formal boot 4. No production fix was applied to that later lifecycle boundary in Phase 29J.

The gate records and package/validation evidence are retained under `C:\Users\guideX\AppData\Local\Temp\guidexos-phase28g-806518b997864430bef4386cdaf8c9d0` (formal boots) and `C:\Users\guideX\AppData\Local\Temp\guidexos-phase28g-064153ea1d0e41bfa68211f66cd7eea4` (diagnostic boot). The earlier sentinel failure evidence remains under `C:\Users\guideX\AppData\Local\Temp\guidexos-phase28g-f602f50b32544a8686def3ba9259df9d`.
