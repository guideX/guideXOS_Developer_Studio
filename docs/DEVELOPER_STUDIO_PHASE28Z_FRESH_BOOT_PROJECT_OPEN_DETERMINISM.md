# Developer Studio Phase 28Z — Fresh-Boot and Project-Open Determinism

## Scope

Phase 28Z makes the fresh-boot and project-open boundary observable and makes project publication transactional. The implementation preserves the existing debugger and Phase 28X/Y behavior; it does not add retries, timing delays, duplicate project opens, or a bypass path.

The repositories were clean at the start of the work:

- Developer Studio: `main` at `34d92e9` (`Phase 28Y document stress prerequisite`)
- guideXOS server: `v0.5_DEVELOPER_STUDIO` at `82ecea29` (`Phase 28Y trace post-render debug launch`)

## Lifecycle finding and repair

The real project-open path is synchronous on the application event-loop pump:

`phase28qPump` → `WorkspaceControllerOpenProject` → `LoadProject` → candidate root/project publication → directory refresh → symbol publication → active project consumers.

There is no worker queue or completion notification involved in this path. Before Phase 28Z, the controller published the new root and project before refresh completed. A refresh failure could therefore leave the controller exposing a partially opened project. Phase 28Z now:

1. assigns a monotonic request id and publishes `load_started`;
2. loads the project and publishes `loaded`;
3. constructs and refreshes a candidate model, publishing `refresh_started`;
4. commits the candidate model, listing state, generation, and symbols only after refresh succeeds;
5. publishes `ready`, or publishes `failed` while retaining the previous root, generation, active document, and project state.

The observer state is `idle`, `load_started`, `loaded`, `refresh_started`, `ready`, or `failed`. Request id and project generation are carried with every state notification. The deterministic host regression test forces a refresh failure and verifies rollback to the prior project/document.

The fresh-boot audit also found two pre-lifecycle bulk clears that could stall an emulated Native ELF launch. Startup now uses the bounded DWARF mapper reset and count-gated debugger-workspace reset. The artifact buffer is consumed only through its valid-byte count and no longer receives an unnecessary full wipe.

## Instrumentation

The server emits fixed boot milestones for native loader entry, kernel entry, early kernel initialization, runtime/scheduler readiness, gx_main invocation, desktop initialization, and gx_main return. Developer Studio emits raw entry, storage readiness, gx_main, first render, request observation, project-open entry/return, project-ready, debugger request, and debugger processing milestones.

Project state and bounded filesystem begin/return traces are emitted from the real controller and filesystem callbacks. The harness prints a bounded `P28Z FINAL_STATE` dump on focused-gate failure.

The harness classifies a focused failure as one of:

- pre-Developer-Studio boot/loader failure;
- loader invoked but application entry unconfirmed;
- post-render/project-open or later application failure.

## Fresh-boot staging audit

`Assert-Phase28ZBootImage` stages every boot into a unique disposable ESP directory and validates the exact kernel, Developer Studio ELF, diagnostic project, app manifest, sentinel, fixture, and configuration identities. It records SHA-256 values in one bounded line:

`P28Z BOOT_IMAGE boot=<n> arch=amd64 kernel=<hash> developer_studio=<hash> project=<hash> config=<hash> sentinel=present project_fixture=present isolated=present`

The staged directory is independently copied for each boot, so stale files and cross-boot carryover are not accepted.

## Validation performed

- Standalone CTest: **31/31 passed**.
- Focused transactional project-open regression: **passed**.
- Hosted Developer Studio build and tests: **passed**.
- LLVM freestanding AMD64 Native ELF package build: **passed**.
- DWARF locals/arguments proof reported **PASS** by the repository build; the freestanding fixture capacity is documented for the 397-DIE fixture.
- Fresh QEMU boots reached the complete Phase 28Z project sequence on the successful evidence boots, including `load_started`, `loaded`, `refresh_started`, `ready`, `project_open_return`, `project_ready`, debugger request, and debugger processing.

The final five-boot focused invocation did not complete 5/5. Its first boot reached project-ready and debugger launch, then stopped in the pre-existing debugger post-refresh stop-mapping path after:

`DEVELOPER_STUDIO_PHASE28V_EVENT_DEBUG_POST_REFRESH_STOP_MAPPING_UNRESOLVED`

This is after the Phase 28Z project-open completion boundary and is classified separately from a project-open failure. The focused proof is therefore **Outcome B**: the project-open determinism boundary is instrumented and transactionally repaired, but the required full debugger gate is not green in this environment.

Because the focused gate did not complete, the 10-boot full debugger gate and the conditional 25-cycle real-QEMU lifecycle stress were not run. Hardware/QEMU-on-target validation was not performed beyond the disposable QEMU evidence above.

Evidence from the bounded fresh-boot runs is retained under:

`C:\Users\guideX\AppData\Local\Temp\guidexos-phase28g-fef429f1ce4e4ec48e07ddde04adada1`

The earlier startup-boundary failure evidence is retained under:

`C:\Users\guideX\AppData\Local\Temp\guidexos-phase28g-8bbf8a3e2e874734a773bca77c883817`

## Changed files

Developer Studio:

- `src/developer_studio_workspace.h/.cpp` — lifecycle state, request/generation ownership, observer, transactional candidate commit.
- `src/developer_studio_debugger_workspace.cpp` — count-gated startup reset.
- `src/main.cpp` — real boot/project/filesystem/debugger milestones and bounded startup reset.
- `tests/project_test.cpp` — observer and rollback regression.

guideXOS server:

- `kernel/core/main.cpp` — boot milestones.
- `kernel/core/native_elf/native_elf_loader.cpp` — loader invoke/return milestones.
- `scripts/smoke-compiler-bootstrap.ps1` — isolated image audit, classifications, focused markers, and bounded final-state dump.

## Reproduction commands

```powershell
cmake --build build --target developer_studio_project_test -j 4
ctest --test-dir build --output-on-failure

powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 `
  -ServerRoot D:\dev\guideXOSServerV0.5_DEVELOPER_STUDIO `
  -ToolchainRoot 'C:\Program Files\LLVM\bin' `
  -Configuration Debug -TargetArchitecture amd64

powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\smoke-compiler-bootstrap.ps1 `
  -Phase28QOnly -BootCount 5 -TimeoutSeconds 180
```
