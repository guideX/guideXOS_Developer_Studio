# Developer Studio Phase 28S: Native Launch and Debugger Proof

Date: 2026-09-20  
Outcome: **B — useful repairs landed, but complete acceptance remains incomplete**

Phase 28S repaired the native launch stall and removed the hosted access-violation path through project open and build. The complete debugger acceptance is not yet green: two of three final-artifact QEMU boots reached the terminal P28Q proof, but the fixture reports locals, watches, data tips, and live progress as unavailable at the cooperative stop; one final-artifact boot stalled before the running marker. Hosted smoke now reaches project open and a successful real P28Q build, but its end-to-end lifecycle handshake remains incomplete. No physical hardware pass was performed.

## Root cause and evidence

### Native QEMU launch stall

P28Q is a short real compiled ELF. The AMD64 NativeElf scheduler parks it at `native_elf_scheduler_yield` while the Developer Studio owner services its debug request. The old debug `POLL` path unconditionally advanced the target from that parked boundary. For a short fixture, that consumed the last cooperative yield before the UI Pause request could be captured, so the serial trace stopped after ELF open/validation and before the loader/debug-start proof markers. The trace showed a parked cooperative target and a live owner, not a loader fault, deadlock, or blocking file operation.

The repair retains the parked yield context, captures a user Pause directly from that parked context when safe, and publishes one parked running boundary for a poll that precedes a Pause request. Explicit Continue, step, and lifecycle pumps still advance the target normally. The repaired path emits `DEVELOPER_STUDIO_PHASE28S_PARKED_PAUSE_CAPTURE_PASS`.

The final QEMU serial evidence shows real P28Q compilation, ELF validation/open, debug start, running state, Pause capture, Continue, a second distinct stop generation, second Continue, `gx_main returned 0`, teardown, `debug_state=EXITED`, and `DEVELOPER_STUDIO_PHASE28Q_PASS` in the successful boots. The earlier Phase 28R entry-breakpoint restoration and scheduler-yield Boolean-result repairs remain in place. No ELF loading, debugger attachment, fault handling, or timeout was bypassed.

### Hosted `0xC0000005` path

The hosted smoke failure was a separate bounded-NativeElf-stack defect. WER/GDB symbol mapping localized successive access violations to large by-value ABI temporaries in:

- `debugUiResetRuntimeState` (`src/main.cpp`, first crash at line 8733);
- `LoadProject` (`src/developer_studio_projects.cpp`, large result/project/parser frame);
- `loadStorageFile` (`src/developer_studio_debugger_workspace.cpp`, WER frame with approximately `0x5e90` bytes reserved before the failing `memset` at line 170).

The exception was a real Windows access violation, `0xC0000005` / `-1073741819`, not a test timeout. After the fixes, hosted smoke reached project open and completed the real P28Q build with `output=/P28Q/build/bin/amd64/p28q.elf`, `output_bytes=10403`, `output_hash=fnv1a64:285E8C42C5212D02`, and SHA-256 `37babf03a9a1a8799eee90e710fbfd4f72ba98f30c73b5c83393054cecbf7439`.

The remaining hosted smoke run was stopped after it remained in the normal Developer Studio render/event path. Live GDB showed the guest in `drawShell`/`drawExplorer` and the host waiting in `gxos::apps::hostPollEvent`; no new access violation or parser deadlock was observed. The smoke driver therefore did not receive its terminal lifecycle marker and remains an open defect.

## Production changes and regression coverage

- NativeElf AMD64 scheduler: parked-boundary capture and one-shot poll publication in `kernel/core/native_elf/native_elf_run_service.cpp`.
- Developer Studio hosted NativeElf stack discipline: in-place ABI resets and static bounded scratch storage for debugger UI snapshots, project parsing, and debugger-workspace persistence.
- P28Q acceptance script: the Phase 28S parked-capture marker is required in the evidence set.
- Existing standalone debugger model coverage remains green, including the prior entry-breakpoint restoration and scheduler-result behavior.

The static scratch buffers preserve the existing bounded, transactional parsing behavior; they are not a new debugger capability. Generated build trees remain ignored/untracked, and the restored Mbed TLS/TF-PSA provenance remains Mbed TLS 4.1.0 (`0fe989...`, `mbedtls-4.1.0`) and TF-PSA Crypto 1.1.0 (`29160...`, `v1.1.0`) with the three existing project patches.

## Commands and results

Repository roots: standalone `D:\dev\guideXOS_Developer_Studio`, branch `main`; server `D:\dev\guideXOSServerV0.5_DEVELOPER_STUDIO`, branch `v0.5_DEVELOPER_STUDIO`.

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tests\run-developer-studio-validation-fast.ps1 `
  -ServerRoot D:\dev\guideXOSServerV0.5_DEVELOPER_STUDIO -SkipPackage -SkipHosted

mingw32-make.exe -B amd64

powershell.exe -NoProfile -ExecutionPolicy Bypass -File D:\dev\guideXOS_Developer_Studio\build.ps1 `
  -ServerRoot D:\dev\guideXOSServerV0.5_DEVELOPER_STUDIO `
  -SkipModelTest -SkipProjectTest -TargetArchitecture amd64

powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\smoke-compiler-bootstrap.ps1 `
  -Phase28QOnly -BootCount 1 -TimeoutSeconds 120
```

The QEMU command was run as three separate fresh final-artifact boots. Boot 1 stalled before `DEVELOPER_STUDIO_PHASE28Q_RUNNING_PASS`; boots 2 and 3 reached `DEVELOPER_STUDIO_PHASE28Q_PASS` but the harness still failed because it requires live progress and contains the stale spelling `DEVELOPER_STUDIO_PHASE28Q_PAUSE_UI_REQUEST_PASS`, while the application emits `DEVELOPER_STUDIO_PHASE28Q_UI_PAUSE_REQUEST_PASS`.

Evidence directories:

- Boot 1: `C:\Users\guideX\AppData\Local\Temp\guidexos-phase28g-24e49d46eabd4a6dbed23f9c025377ba`.
- Boot 2: `C:\Users\guideX\AppData\Local\Temp\guidexos-phase28g-17239bc238be4dab95d89ccffc3caf30`.
- Boot 3: `C:\Users\guideX\AppData\Local\Temp\guidexos-phase28g-de310c6721bb42c8b8ce588d1fd614bf`.

Hosted smoke command:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tests\smoke-developer-studio-debugger.ps1 `
  -ServerRoot D:\dev\guideXOSServerV0.5_DEVELOPER_STUDIO `
  -ContinueBreakpoint -TraceDirectory D:\dev\guideXOS_Developer_Studio\logs `
  -TraceRunIndex 134
```

Result: project open and real P28Q build passed; terminal lifecycle proof did not complete. Exact trace: `D:\dev\guideXOS_Developer_Studio\logs\developer-studio-debugger-shutdown-trace-134.log`.

## Debugger proof matrix

| Scenario | Result | Evidence or limitation |
|---|---|---|
| P28Q compile, reopen, ELF validation/open | Pass | Real ELF, 10403 bytes, `fnv1a64:285E8C42C5212D02` |
| Loader/debug start and running state | Pass in boots 2–3 | Boot 1 remained incomplete |
| Pause while running | Pass in boots 2–3 | Pause capture and parked-capture markers |
| Continue | Pass in boots 2–3 | First and second Continue markers |
| Repeated Pause/Continue | Pass in boots 2–3 | Stop generations 2 and 3; final target exit |
| Registers/context | Pass | Validated nonzero AMD64 context |
| Call Stack | Pass | Call-stack marker; frame values were not independently fixture-asserted |
| Variables/locals | Not live | Fixture reports `LOCALS_NOT_LIVE` |
| Watch | Not live | Fixture reports `WATCH_NOT_LIVE` |
| Data Tips | Not live | Fixture reports `DATA_TIP_NOT_LIVE` |
| Source breakpoints and one-shot entry restoration | Partial | Existing model tests remain green; full P28Q terminal scenario does not independently assert coexistence |
| Duplicate Pause | Partial | Repeated stop identity passed; dedicated duplicate-Pause marker was not separately exercised |
| Pause during stepping / stop ownership | Not proven | No complete terminal stepping-interaction scenario |
| Stop, close, relaunch, teardown | Partial | Native target teardown and EXITED passed; hosted close/relaunch did not complete |
| Developer Studio close/freeze reliability | Open | Physical reproduction was not attempted |

## Artifacts

Final artifact SHA-256 values:

```text
42B7B28C5657BC14FAF1186880FF298D993322F8499544FD6BA1A13D9AE5A3D5C  Apps/DeveloperStudio/bin/amd64/developerstudio.elf
E91B7170B59439C941DE2295DF2188886175EC41D65F28C48B3787D1C8CE57E3  Apps/DeveloperStudio/bin/arm64/developerstudio.elf
5D4E48CD06ECE888BD3CB92723CA2A62782D7799853322E7793402A6D31CB0D3  kernel/build/amd64/bin/kernel.elf
8AF9FEED4AEB4E050CDA3EB55B3A811CD34E8CCBCC5E0CC2A7F5C8A6ED867C79  guideXOSBootLoader/x64/Release/guideXOSBootLoader.exe
6915A5C8EE331F1EBBB8381F808CB23E96BE53A45E0BF140A033D7C4D67DC895  guideXOSServer.experimental.exe
```

P28Q fixture identity: `10403` bytes, `fnv1a64:285E8C42C5212D02`.

## Remaining work and physical validation

The remaining blockers are the live-variable/watch/data-tip/progress limitations at the cooperative parked stop, the one incomplete final-artifact boot, the hosted smoke terminal handshake, and the previously reported Developer Studio close/freeze reliability issue. These require another stabilization phase before debugger development advances.

QEMU evidence here is freestanding virtual-machine evidence only. Physical bare-metal validation is pending and must be run separately; no physical hardware result is implied by Outcome B.
