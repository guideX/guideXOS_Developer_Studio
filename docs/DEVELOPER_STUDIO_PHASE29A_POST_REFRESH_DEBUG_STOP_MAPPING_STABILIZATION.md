# Developer Studio Phase 29A — Post-Refresh Debug Stop Mapping Stabilization

## Result

Phase 29A is **Outcome B**. The post-refresh stop-mapping repair is implemented,
covered by deterministic model tests, and passes the standalone, DWARF, package,
and hosted ELF gates. The focused five-boot QEMU gate was invoked with the rebuilt
artifacts but did not reach a post-fix pause on boot 3: the guest reached the
transactional project-ready and debug-start milestones, then remained in the
in-guest build/debug transition until the bounded timeout. The 10-boot gate and
conditional 25-cycle stress were therefore not run.

The repositories were clean at the start of this phase:

- Developer Studio: `main` at `14275dd` (`Phase 28Z fresh boot project open determinism`)
- guideXOS Server: `v0.5_DEVELOPER_STUDIO` at `745f9dbc` (`Phase 28Z boot staging and lifecycle milestones`)

No branch switch, merge, rebase, reset, or history rewrite was used.

## Baseline finding

The preserved Phase 28Z trace reproduced the original
`DEBUG_POST_REFRESH_STOP_MAPPING_UNRESOLVED` after a valid user pause. The
current-stop ownership predicate was already true: session, process/runtime,
thread, stop generation, architecture, and nonzero register context all matched
the active stop. The captured register RIP was `0x205346`, while the loaded
Developer Studio image occupied the fixed application range beginning at
`0x50000000`. The first unresolved layer was therefore **PC/module selection**,
not stale stop ownership, project generation, source identity, workspace
document selection, or DWARF line lookup.

The raw pause came from the cooperative NativeElf scheduler yield. Its runtime
helper RIP is intentionally outside the application image. Treating that raw RIP
as the source PC caused the stack/source/editor path to fail even though the stop
itself was current and valid.

Baseline evidence:

`C:\Users\guideX\AppData\Local\Temp\guidexos-phase28g-fef429f1ce4e4ec48e07ddde04adada1\boot1.serial.log`

## Repair

### Developer Studio ownership and diagnostics

- `DebugControllerResolveCurrentStop` now verifies current-stop ownership,
  project generation, mapper generation, exact artifact identity, executable
  module range, and DWARF source identity before publishing a location.
- A new bounded error taxonomy distinguishes invalid/out-of-module PCs, missing
  modules, stale module/symbol generations, stale project generation, missing
  DWARF ranges/lines, missing source identity, missing workspace documents, and
  selected-frame binding failures.
- New stops and generation/artifact invalidation clear derived source, stack,
  variable, and editor state so stale mappings cannot survive a refresh.
- Cooperative pauses retain the raw register context for ownership and resume,
  but frame zero may bind to the backend's validated application call-site when
  the raw helper RIP is unmapped.
- Phase 28Q tracing now emits one bounded
  `DEVELOPER_STUDIO_PHASE29A_STOP_MAPPING_DIAGNOSTIC` record containing session,
  stop, PC, reported RIP, module/symbol/project generations, source, workspace,
  and the exact result code. A complete mapping emits
  `DEVELOPER_STUDIO_PHASE29A_STOP_MAPPING_PASS`; failures emit the corresponding
  `..._FAIL` marker.

### NativeElf server boundary

- A cooperative pause now performs a bounded, validated stack scan for an
  application call-site only when the raw RIP is outside the user image.
- A candidate is accepted only when it is inside the loaded application image and
  resolves to an authoritative target address, source path, line, and function
  mapping. The scan is generic and does not depend on the Phase 28Q fixture or on
  timing/retry behavior.
- `call_stack` returns a bounded source-only frame for this case rather than
  rejecting the valid stop or inventing a caller chain. The raw register context
  remains authoritative for control flow.
- The server serial trace records the raw RIP, published mapping address, and
  `mapping=PASS|NONE`.

## Validation

| Gate | Result |
|---|---|
| Standalone CTest | **PASS, 31/31** |
| New stale project/artifact/external-PC model coverage | **PASS** |
| Bare-metal DWARF capacity | **PASS, dies=397** |
| Hosted Developer Studio debugger smoke | **PASS**, real source breakpoint, call stack, locals, editor execution, clean exit |
| Package content/ELF audit | **PASS**, AMD64 and ARM64 sectionless ELF64 ET_EXEC |
| Rebuilt AMD64 package | **PASS**, SHA-256 `4F1B461E1DBC7A0E235507EA4CD39D3B2AB86DE6555C21DB583474D08F3992C0` |
| Rebuilt ARM64 package | **PASS**, SHA-256 `CB3452F1237350146A87E6C85C50498FB3CCD751A09AB3FE6939566B23C4E0E0` |
| Focused QEMU | **BLOCKED, not 5/5**; boot 3 reached project-ready/debug-start but timed out before running/pause |
| Full 10-boot QEMU | **NOT RUN**, focused prerequisite failed |
| 25-cycle QEMU stress | **NOT RUN**, focused prerequisite failed |
| Physical hardware | **NOT RUN** |

Focused QEMU evidence was preserved at:

`C:\Users\guideX\AppData\Local\Temp\guidexos-phase28g-d40202afdede4395bf54f7688a5a49b6`

The boot image audit recorded the rebuilt AMD64 payload hash on each isolated
boot. Boot 3 reached `P28Z PROJECT state=ready`, `P28Z APP 05 project_open_return`,
`DEVELOPER_STUDIO_PHASE28Q_PROJECT_OPEN_PASS`, `P28Z APP 08
debugger_launch_processed`, and `DEVELOPER_STUDIO_PHASE28Q_DEBUG_START_PASS`.
It did not reach `DEVELOPER_STUDIO_PHASE28Q_RUNNING_PASS`, a pause capture, or
the new stop-mapping diagnostic. This is a later in-guest build/debug transition
blocker, not evidence that the repaired PC/module layer remains unresolved.

## Changed files

Developer Studio:

- `src/developer_studio_debugger.h`
- `src/developer_studio_debugger.cpp`
- `src/developer_studio_debugger_stack.cpp`
- `src/main.cpp`
- `tests/debug_symbols_test.cpp`
- `tests/run-developer-studio-debugger-required.ps1`
- `docs/DEVELOPER_STUDIO_PHASE29A_POST_REFRESH_DEBUG_STOP_MAPPING_STABILIZATION.md`

guideXOS Server:

- `kernel/core/native_elf/native_elf_run_service.cpp`
- `scripts/smoke-compiler-bootstrap.ps1`
- `Apps/DeveloperStudio/bin/amd64/developerstudio.elf`
- `Apps/DeveloperStudio/bin/arm64/developerstudio.elf`

## Reproduction commands

```powershell
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure

powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 `
  -ServerRoot D:\dev\guideXOSServerV0.5_DEVELOPER_STUDIO `
  -Configuration Debug -TargetArchitecture amd64

powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\run-debug-symbols-capacity.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\validate-developer-studio-package.ps1 `
  -ServerRoot D:\dev\guideXOSServerV0.5_DEVELOPER_STUDIO

powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\smoke-developer-studio-debugger.ps1 `
  -ServerRoot D:\dev\guideXOSServerV0.5_DEVELOPER_STUDIO `
  -DiagnosticOnly -DebugWaitSeconds 60 -MaxRuntimeSeconds 180

powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\smoke-compiler-bootstrap.ps1 `
  -Phase28QOnly -BootCount 5 -TimeoutSeconds 180
```
