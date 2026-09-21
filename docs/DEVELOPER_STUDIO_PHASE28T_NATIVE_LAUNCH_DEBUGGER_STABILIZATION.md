# Developer Studio Phase 28T — Native Launch and Debugger Stabilization

Date: 2026-09-20  
Outcome: **Outcome C — Reproducible blocker isolated**

Phase 28T did not add debugger features. It repaired several concrete acceptance defects, added a deterministic regression probe, and narrowed the remaining nondeterminism to the freestanding nested NativeElf terminal-poll handoff. Hosted and fresh-QEMU acceptance is therefore not claimed as complete.

## Starting state

| Repository | Branch | Starting HEAD |
| --- | --- | --- |
| `D:\dev\guideXOS_Developer_Studio` | `main` | `e892bc26ee29622d8438abdfae9c03ccce72aa53` |
| `D:\dev\guideXOSServerV0.5_DEVELOPER_STUDIO` | `v0.5_DEVELOPER_STUDIO` | `01e0330906fc83358563113a95e2f76247f6de30` |

Both worktrees were clean at the recorded starting heads. The Phase 28S AMD64 artifact was `42B7B28C5657BC14FAF1186880FF298D993322F8499544FD6BA1A13D9AE5A3D5C`.

## Defects reproduced and repaired

### Freestanding DWARF capacity mismatch

The hosted Phase 3B DebugSymbols fixture is valid DWARF5 but contains 397 DIEs and 41 abbreviation declarations. The freestanding mapper tables were limited to 256 DIEs and 32 abbreviations, so the exact bare-metal parser reported `malformed_dwarf`/capacity failure while the host parser accepted the same 11,608-byte artifact.

The freestanding limits in `src/developer_studio_debug_symbols.h` are now 512 DIEs and 64 abbreviation declarations. The checked-in probe `tests/debug_symbols_capacity_test.cpp`, driven by `tests/run-debug-symbols-capacity.ps1`, compiles the mapper with `GXOS_DEVELOPER_STUDIO_BARE_METAL` and loads the real fixture. It passes with `dies=397`.

### Server native build recipe omissions

`build-native-experimental.bat` did not provide `kernel/core/include` and omitted `kernel/core/compiler/elf_writer.cpp` and `kernel/core/native_elf/native_elf_debug_watches.cpp`. The recipe now includes those existing dependencies. An optimized (`-O2`) native server build completed successfully.

### Phase 28Q acceptance-gate naming

The QEMU runner required `DEVELOPER_STUDIO_PHASE28Q_PAUSE_UI_REQUEST_PASS`, while the product emits `DEVELOPER_STUDIO_PHASE28Q_UI_PAUSE_REQUEST_PASS`. The runner now checks the emitted marker. The runner also now checks the truthful `DEVELOPER_STUDIO_PHASE28Q_PROGRESS_NOT_LIVE` capability marker established by Phase 28S instead of requiring an unavailable live-progress PASS marker.

### Terminal-state publication ordering

The event pass could observe the target's terminal state after the normal diagnostic pump and then redraw before the existing Phase 28Q terminal markers were published. `src/main.cpp` now calls the existing `phase28qPump` once more after the secondary event polls and suppresses the final shell redraw after an exit request. This is an ordering correction to the existing lifecycle, not a new debugger feature.

## QEMU lifecycle investigation

Bounded one-shot diagnostics were used temporarily to distinguish launch, pause, continue, event, terminal poll, target return, teardown, and EXITED publication. They were removed before the final package build.

The successful traces demonstrated the expected internal sequence:

`APP_LAUNCH_PASS` → `DEBUG_START_PASS` → `RUNNING_PASS` → first Pause/paused state → Continue → second Pause/new stop → second Continue → `gx_main returned 0` → teardown PASS → `debug_state=EXITED`.

The runner evidence was not deterministic enough for acceptance:

| Fresh QEMU evidence | Result |
| --- | --- |
| `guidexos-phase28g-14c67db406844b9cafbd440b78843293` | Serial reached the complete P28Q lifecycle, target return, teardown, and internal `PHASE28Q_PASS`; the wrapper was rejected by the then-stale marker requirements. |
| `guidexos-phase28g-84dcc317d3fe40b68ee07f89aa78fa12` | Reached second Continue, target return, and teardown, but did not publish final markers. |
| `guidexos-phase28g-495e7034aa4347b685cb6245dd9fcc85` | Stalled after the first Continue. |
| `guidexos-phase28g-5584332a72db496588928202679424e0` | Stalled before the target loader after build completion. |
| `guidexos-phase28g-8eecc67b2f3647bd83651a31dbfa0438` | Reproduced the same pre-target-launch pause with a longer timeout. |
| `guidexos-phase28g-deb94b3186164961b608a960e5258b04` | Reached `debug_state=EXITED`; the final markers were absent because the event pass had not repumped the existing Phase 28Q publisher. |
| `guidexos-phase28g-f90e083e9f244def894b20def38a74a8` | After the ordering repair, reached `TERMINAL_POLL_ENTRY` but intermittently stalled before `TERMINAL_POLL_EXIT`. |

These were independently fresh QEMU instances, not fixture restarts in one boot. The investigation did not produce ten accepted fresh boots. The final artifact was rebuilt after diagnostics were removed; the traces above are diagnostic evidence, not a claim that all of them are final-artifact acceptance passes.

The remaining blocker is narrowed to the freestanding nested host-call/image handoff around `DebugControllerPoll`, `NativeElfRunService::poll`, and `native_elf_nested_enter_for_host`/capture/leave. In successful runs the target returns and cleanup can complete; in failing runs the terminal poll can remain nested after the target has returned. This is a concrete backend lifecycle blocker, not a generic timeout problem, so no arbitrary sleep or retry was added.

## Hosted lifecycle findings

The hosted smoke was rerun after the mapper repair:

`logs\developer-studio-debugger-shutdown-trace-287.log`

It no longer reported `malformed_dwarf` or `0xC0000005`. The native app reached `Exited`, cleaned its owned window (`cleaned window count: 1`, remaining owned windows `0`), and emitted `GUIDEXOS_DEVELOPER_STUDIO_MARKER clean_close=PASS`. However, the smoke did not observe `debug_start` or a debugger session and timed out waiting for the expected completion marker. Hosted debugger acceptance is therefore incomplete; clean terminal close is observed, but the full hosted debugger handshake is not accepted.

## Generation and ownership audit

The audit covered the existing generation/owner boundaries rather than redesigning them:

* `DevelopmentRunService` handles encode slot and generation, and owned lookups reject stale generations and owner-runtime mismatches.
* `NativeAppRuntime::hostPollEvent` filters events that are not owned by the active runtime.
* Debug session, stop, and command paths validate the active session/target identity; stale Pause/Continue/POLL requests are rejected rather than applied to a relaunch.
* Target close and NativeElf cleanup showed no persistent owned-window or ordinary event leak in the hosted trace.
* Mapper reset, `RunControllerInit`, debugger teardown, and `debugUiResetRuntimeState` were audited for launch reuse. Existing state is reset at the relevant lifecycle boundaries; the fast model tier stayed green.

No evidence showed an old window event controlling a new launch. The remaining failure occurs later, at nested terminal polling after target return, and is therefore classified as a backend context-handoff problem rather than a simple ownership leak.

## Regression and validation tests

Passed:

* Standalone fast hosted tier: **31/31 CTest tests passed**.
* Bare DWARF capacity regression: **PASS, dies=397**.
* DebugSymbols fixture: SHA-256 `37BABF03A9A1A8799EEE90E710FBFD4F72BA98F30C73B5C83393054CECBF7439`.
* Optimized freestanding/server native build: **PASS**.
* Existing debugger model tests covering breakpoints, conditional breakpoints, stepping, stack, locals/arguments, watches, data tips, editor, workspace, and symbol mapping remained green.

Not accepted:

* Hosted end-to-end debugger terminal handshake: incomplete; no crash, but no observed debugger-start completion in smoke run 287.
* Ten-success fresh-QEMU stress target: not achieved; the independent-boot traces above include intermittent stalls.
* Locals, Watch, Data Tips, and live progress remain unavailable in the particular freestanding P28Q proof environment, as established in Phase 28S. No PASS marker was fabricated for those capabilities.

## Final artifact

AMD64 Developer Studio package:

`D:\dev\guideXOSServerV0.5_DEVELOPER_STUDIO\Apps\DeveloperStudio\bin\amd64\developerstudio.elf`

SHA-256: `5293CD3016CBD54B9526A17969FF6A39434E23C0EC9AADABEBE1E2E12E36587C`  
Size: 936,744 bytes

No physical hardware was tested.

## Classification

**Outcome C — Reproducible blocker isolated.**

Phase 28T repaired the concrete DWARF capacity defect, native build recipe, QEMU acceptance predicates, and terminal-state publication ordering without regressing the 31-test tier. The remaining intermittent stall is isolated to the nested freestanding NativeElf terminal-poll/context handoff, while hosted debugger completion is still unobserved. It is not safe to claim Outcome A until that subsystem is repaired and fresh boots are deterministic.
