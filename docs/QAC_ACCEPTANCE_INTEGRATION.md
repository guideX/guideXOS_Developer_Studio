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
