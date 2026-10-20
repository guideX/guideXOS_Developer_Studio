# Developer Studio QAC acceptance integration

The focused Developer Studio Phase 28Q and Phase 28O guest acceptance modes use guideXOS QAC to request exclusive admission before QEMU starts. The Studio runner remains `tests/run-developer-studio-debugger-required.ps1`; it delegates the guest proof to the paired Server checkout's `scripts/smoke-compiler-bootstrap.ps1`, where the final QEMU process is created.

## Run

Build the QAC Windows CLI apphost, start the interactive QAC coordinator under the same Windows account, and run either focused acceptance mode:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\run-developer-studio-debugger-required.ps1 `
  -ServerRoot D:\dev\guideXOSServerV0.5_DEVELOPER_STUDIO `
  -Phase28QOnly `
  -QacCliPath D:\dev\guideXOS_QAC\src\GuideXOS.QAC.Cli\bin\Debug\net10.0-windows\GuideXOS.QAC.Cli.exe
```

Use `-Phase28OOnly` for Phase 28O. `-QacCliPath` is optional when `GUIDEXOS_QAC_CLI` points to the apphost or `qac.exe` is on `PATH`. `-QacAdmissionTimeoutSeconds` bounds the queue wait (default 1800 seconds); `-MaxRuntimeSeconds` bounds each guest run after QAC reports the child process. The GUI is useful for observing queue and active state, but the coordinator and CLI do not require the dashboard to remain open.

## Lifecycle and evidence

The launcher resolves the existing QEMU executable and argument array, then invokes `qac run --project DeveloperStudio --mode acceptance -- <qemu-system-*.exe> <arguments>`. QAC starts the direct QEMU child only after granting the exclusive reservation. A Windows command-line quoting helper preserves the argument array, including whitespace, quotes, empty arguments, and trailing backslashes. The existing firmware, machine, CPU, memory, disk, serial, display, and device settings are retained; acceptance mode adds a loopback-only QMP endpoint for graceful guest shutdown.

The QAC CLI's child output remains captured in the existing per-boot stdout and stderr evidence files. Guest serial and debug markers remain the success criteria. On the existing harness stop marker or runtime timeout, the script sends QMP `quit` and waits for QAC to report QEMU's exit code and a verified `Released` reservation. The child exit code is compared with the QAC CLI exit code. Cancellation while queued invokes `qac cancel`; cancellation during a run requests QMP shutdown. Coordinator unavailability, foreign-QEMU blocking, launch failure, cancellation, a timeout, or unverified release cannot become an acceptance pass.

If QEMU exit or reservation release remains unverified after the graceful wait, the script leaves QAC's reservation and child lifecycle in the conservative state, skips changes to the staged ESP while QEMU could still hold it, and preserves the boot evidence directory. The direct QEMU child is never force-killed through this acceptance path. Use the QAC coordinator/dashboard state and the preserved host trace to diagnose and reconcile the reservation.

The WPF dashboard displays these reservations through its general reservation model: `DeveloperStudio`, `acceptance`, queue/block reason, child PID and process identity when verified, foreign-QEMU blocker, and released state. No Developer Studio-specific dashboard code was added.

## Scope and qualification

QAC integration applies to the two focused Phase 28Q/28O guest acceptance launchers in this milestone. It does not change their guest boot or marker criteria, the rest of the hosted validation tiers, or project startup. QAC must be online in the current user session; no service is installed or started automatically, and there is no unmanaged fallback.

The integration-specific PowerShell helper checks are in the paired Server repository at `tests/test-developer-studio-qac-acceptance.ps1`. QAC CLI lifecycle, foreign-QEMU blocking, cancellation, launch failure, direct-child identity, argument handling, exit propagation, and recovery regressions remain in the QAC solution's automated suite. Qualification evidence and any incomplete conditions are recorded in the QAC `docs/phase-validation.md` QAC3A entry.

QAC3B should evaluate the guideXOS Server development QEMU workflow at its final executable spawn point, then assess shared scripts and CI entry points.

## Qualification disposition — Outcome B

The focused Phase 28Q path was run on LEON2_RDT through the QAC-managed launcher. QAC granted exclusive admission and started the installed QEMU executable as its direct child. The real guest boot exceeded the bounded 360-second runtime. QMP shutdown completed, QAC verified child exit and released the reservation, and the serial file contained the existing `DEVELOPER_STUDIO_PHASE28Q_PASS` marker after shutdown. Because the runner reached its runtime timeout before observing that marker, this attempt is incomplete and is not an acceptance pass.

A second Phase 28Q boot was interrupted with Ctrl+C and is also not a pass. Phase 28O was not exercised live. Deterministic QAC and PowerShell helper checks cover the admission and lifecycle paths, including the new rejection of timeout and incomplete release, but they do not replace a successful guest qualification. See the paired QAC repository's `docs/phase-validation.md` and Server integration doc for run IDs and retained evidence.

Outcome B is recorded for this integration checkpoint. The Developer Studio phase progression and acceptance criteria remain unchanged.

## QAC3A-R1 requalification follow-up — October 19, 2026 (Pacific)

The Phase 28Q wrapper's required UI pause marker had a stale spelling, and its ordering check placed the Phase 29A stop-mapping marker before the first Continue even though the guest emits it at the second stop. Both checks now match the existing guest markers and lifecycle order; no guest code or acceptance marker was changed.

The corrected Phase 28Q wrapper passed with exit 0 across three fresh QAC-managed boots. All 22 required markers were present in order; each `DEVELOPER_STUDIO_PHASE28Q_PASS` marker was observed live about 32.9–33.4 seconds after verified QEMU child start, inside the original 360-second limit. QMP shutdown, QEMU exit 0, and verified reservation release succeeded on each boot. Evidence: `%LOCALAPPDATA%\Temp\guidexos-phase28g-71a6ed320e8d440490c6e653971c490c`.

The required single live Phase 28O workflow was then run through QAC. Its guest emitted `DEVELOPER_STUDIO_PHASE28O_TIMEOUT step=21`, with the debug controller still Running and no runtime breakpoints, followed by `DEVELOPER_STUDIO_PHASE28O_FAIL reason=timeout` and `DEVELOPER_STUDIO_PHASE28O_FAILURE`. Required materialization, breakpoint, Watch, terminal, cleanup, and final O PASS markers were missing; the wrapper exited 1. QMP shutdown and QAC release were safe. The run evidence is `%LOCALAPPDATA%\Temp\guidexos-phase28g-d96f5a768f96473b9493ccb5cbdbac41`.

The Server harness now requires the exact `DEVELOPER_STUDIO_PHASE28O_PASS` marker for QAC shutdown eligibility instead of the generic kernel main-loop text. That selector fix was covered by deterministic helper tests after the live O attempt; the explicit guest failure remains the O blocker. QAC3A is still Outcome B, and Developer Studio's unrelated phase progression remains unchanged.

## QAC3A-R2 requalification — October 20, 2026 (Pacific)

**Outcome A for QAC3A.** The Phase 28O debugger workflow now passes its original guest acceptance gate through QAC. Developer Studio remains on main; its independent phase progression was not advanced. This repository started at 59f58107a1b936be5c606556904e303160b063e6.

The old Phase 28O timeout reported O stage 21. That stage is a supervisory wait while the shared Phase 28M flow runs. Phase 28M case 20 issues the real UI Continue command; case 21 waits for a valid Paused stop and debugger output from the configured logpoint. The historical serial trace records a backend poll error, “Rejected unowned hosted breakpoint trap,” and then remains Running with no runtime breakpoints or output. The controller correctly requires an enabled local breakpoint matching both the backend binding ID and mapped address before publishing a trap. The failure was identity/state propagation, not an operation that needed a longer timeout.

The focused repairs send the generation-scoped backend binding ID for hosted Continue and instruction-step operations, tag a breakpoint added during an active session with that session generation, and map persisted line breakpoints without treating a gutter anchor column as an executable column. The Server-side NativeElf rearm policy also now respects its authoritative enabled/removed state when the local Continue mirror is stale. No guest acceptance markers or timeouts were changed.

Regression coverage passed for the debugger model, source stepping, workspace materialization, and related Developer Studio suites and package build. New tests verify session generation on an active-session breakpoint, backend binding-ID routing for Continue and Step Instruction, and line-based materialization while preserving stored columns.

The fresh QAC-managed Phase 28O wrapper exited 0 in 103.8 seconds. It emitted the existing full Phase 28O marker set, including the formerly missing materialization, breakpoint hit, Watch, log output, terminal reset, second-generation, bounds, cleanup, and final PASS markers. The final PASS marker was observed 64.515 seconds after verified QEMU start, inside the existing 360-second limit and before child exit. QAC used reservation 2d05e3d933124795861aeb0b9032481b and direct QEMU child PID 20856; QMP quit, child exit 0, and verified release all succeeded. Evidence is preserved under %LOCALAPPDATA%\Temp\guidexos-phase28g-3baa9c8e62104233a832c52c17880c94.

The affected debugger and bootstrap paths are shared with Phase 28Q, so the existing Phase 28Q regression was repeated on three fresh QAC-managed boots. All three passed with the existing markers, QMP shutdown, QEMU exit 0, and verified release. Their evidence is under %LOCALAPPDATA%\Temp\guidexos-phase28g-2da271e734064f5e9d39668039abdd56. The Developer Studio phase history remains independent; overall QAC3 is open and QAC3B is next.
