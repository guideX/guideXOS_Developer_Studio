# Developer Studio Debugger Workspace Persistence Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans (recommended for this single-session implementation) or superpowers:subagent-driven-development to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Persist bounded project-scoped breakpoint configuration and Watch expression order across Developer Studio restart, then rematerialize fresh manager-owned runtime debugger state for each Debug generation.

**Architecture:** Add a UI-independent version-1 JSON model/serializer/store using the existing `WorkspaceFileSystem` callbacks and project-root metadata convention. `main.cpp` owns the active configured model and routes every accepted breakpoint/Watch mutation through save/load; the existing Server source-breakpoint manager remains authoritative for live IDs, mappings, patches, hit counts, and output. The source editor projection renders configured rows before Debug and merges them with current-generation manager rows during Debug.

**Tech Stack:** C++11 bounded fixed-storage model, existing hand-written JSON cursor pattern, Native ABI `file_read_workspace`/`file_write_all`, CMake/CTest host tests, PowerShell package/QEMU harnesses, AMD64 and ARM64 Native ELF builds.

**Spec:** `docs/superpowers/specs/2026-09-18-debugger-workspace-persistence-design.md`

## Global Constraints

- Start standalone work from `77d861f0e00b9d0db1943cdeebded474fac90727` on `phase28o-debugger-workspace-persistence`; do not modify `main` or push.
- Use `D:\dev\guideXOSServerV0.5_DEVELOPER_STUDIO` at live clean `b1ea3d152fe4ca993b83f7ae9d50c8d84d844763`; preserve evidence and do not fetch, pull, merge, rebase, reset, clean, amend, or rewrite history.
- Persist only canonical project-relative source policy and Watch expression strings; never serialize runtime IDs, addresses, opcode bytes, installed state, generation, session, hit counts, results, frames, markers, Locals/Arguments, Output, or stepping state.
- Persist at most 8 breakpoints and 8 Watches; reject over-capacity or malformed documents all-or-nothing.
- Use existing limits: `kMaxProjectPathBytes`, `kDebugWatchMaxExpressionBytes`, `GX_DEVELOPMENT_DEBUG_MAX_LOG_TEMPLATE_BYTES`, and the current backend enum values.
- Use the existing manager commands for every live breakpoint add/configure/enable/disable/remove operation; do not patch target memory from the persistence layer.
- Preserve dirty source buffers and existing Phase 28N source-editor behavior.
- Use `apply_patch` for source/doc edits and run a fresh verification command before every completion or regression claim.

### Task 1: Add the bounded persistence model and failing host tests

**Files:**
- Create: `src/developer_studio_debugger_workspace.h`
- Create: `src/developer_studio_debugger_workspace.cpp`
- Create: `tests/debugger_workspace_test.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces `DebuggerWorkspace`, `DebuggerWorkspaceBreakpoint`, `DebuggerWorkspaceErrorCode`, `DebuggerWorkspaceInit`, `DebuggerWorkspaceAddBreakpoint`, `DebuggerWorkspaceAddWatch`, `DebuggerWorkspaceFindBreakpoint`, `SerializeDebuggerWorkspace`, and `ParseDebuggerWorkspace` for later tasks.
- `DebuggerWorkspaceBreakpoint` contains `sourcePath`, `line`, `column`, `enabled`, `action`, `hitPolicy`, `hitThreshold`, `condition`, and `logTemplate`; it contains no runtime fields.
- `DebuggerWorkspace` contains `breakpoints[8]`, `breakpointCount`, `watches[8][kDebugWatchMaxExpressionBytes + 1]`, `watchCount`, `lastError`, and `lastErrorMessage`.
- `SerializeDebuggerWorkspace(const DebuggerWorkspace&, char* output, uint32_t outputSize, uint32_t* outBytes, DebuggerWorkspaceErrorCode* error)` and `ParseDebuggerWorkspace(const char* bytes, uint32_t length, DebuggerWorkspace* output, DebuggerWorkspaceErrorCode* error)` are deterministic and side-effect free.

- [ ] **Step 1: Add the test target and write the first failing round-trip test.**

  Add this target after the existing debugger editor test target in `CMakeLists.txt`:

  ```cmake
  add_executable(developer_studio_debugger_workspace_test
      tests/debugger_workspace_test.cpp
  )
  target_link_libraries(developer_studio_debugger_workspace_test PRIVATE developer_studio_models)
  set_target_properties(developer_studio_debugger_workspace_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
  add_test(NAME developer_studio_debugger_workspace_test COMMAND developer_studio_debugger_workspace_test)
  ```

  Add a test that initializes one BREAK breakpoint at `src/main.cpp:13`, one LOG breakpoint at `tests/main.cpp:4`, and Watches `input + delta` and `adjusted * 2`; serialize, parse, and assert every field and original ordering. Include `<cassert>`, `<cstring>`, and `<iostream>`.

- [ ] **Step 2: Run the focused test to verify the intended missing-symbol failure.**

  Run:

  ```powershell
  cmake --build build-native-gcc --target developer_studio_debugger_workspace_test
  ctest --test-dir build-native-gcc -R developer_studio_debugger_workspace_test --output-on-failure
  ```

  Expected: configure/build fails because the new header, implementation symbols, or target source do not yet exist. If the target is not configured, run the existing CMake configure command used by `tests/run-developer-studio-validation-fast.ps1` first, then rerun the same focused command.

- [ ] **Step 3: Add the model header with bounded fields and error names.**

  Define `kDebuggerWorkspaceMaxBreakpoints = 8`, `kDebuggerWorkspaceMaxWatches = 8`, `kDebuggerWorkspaceMaxFileBytes = 16u * 1024u`, and errors for `None`, `NullInput`, `MalformedJson`, `UnsupportedVersion`, `MissingField`, `UnknownField`, `DuplicateField`, `InvalidPath`, `InvalidLine`, `InvalidColumn`, `InvalidEnum`, `InvalidThreshold`, `OverCapacity`, `DuplicateBreakpoint`, `StringTooLong`, `OutputTooSmall`, and `FileError`. Use `uint32_t action` and `uint32_t hitPolicy` so the model remains independent of the SDK header while matching the existing ABI values.

- [ ] **Step 4: Implement the minimal model operations and serializer/parser.**

  Add the implementation to the library source list. Reuse the existing project parser's bounded cursor style: strict object keys, strict array separators, no trailing data, bounded strings before copying, slash-normalized relative paths through `NormalizePath`/`PathContainsTraversal`, and exact enum names `BREAK`/`LOG` and `NONE`/`EQUAL`/`MULTIPLE`/`AT_LEAST`. Require `hitThreshold == 0` for `NONE` and `hitThreshold > 0` for all other policies. Require `column >= 1` for nonzero source columns. Reject duplicate canonical source path + line records, regardless of column. Emit fields in exactly the schema order from the design spec and preserve array order.

- [ ] **Step 5: Run the focused test and inspect deterministic bytes.**

  Run:

  ```powershell
  cmake --build build-native-gcc --target developer_studio_debugger_workspace_test
  ctest --test-dir build-native-gcc -R developer_studio_debugger_workspace_test --output-on-failure
  ```

  Expected: PASS. Add an assertion that serializing the same model twice produces identical bytes and that the serialized text contains no `breakpointId`, `targetAddress`, `sessionGeneration`, `rawHitCount`, `originalByte`, `installed`, or runtime-result field names.

- [ ] **Step 6: Add parser rejection tests before moving on.**

  Extend `tests/debugger_workspace_test.cpp` with separate assertions for truncated JSON, invalid syntax, version `2`, missing `breakpoints`, unknown fields, invalid action/policy, zero threshold for `EQUAL`, overlong condition/log/Watch/path values, 9 breakpoints, 9 Watches, duplicate canonical `src/main.cpp:13`, absolute/traversal source paths, and trailing bytes. Assert the output workspace remains empty after each failed parse and the expected error is reported.

- [ ] **Step 7: Run the focused test again and commit the model slice.**

  Run the focused CTest command from Step 5 and then:

  ```powershell
  git diff --check
  git add CMakeLists.txt src/developer_studio_debugger_workspace.h src/developer_studio_debugger_workspace.cpp tests/debugger_workspace_test.cpp
  git commit -m "developer studio: add debugger workspace persistence model"
  ```

### Task 2: Add two-slot project storage and project lifecycle loading

**Files:**
- Modify: `src/developer_studio_debugger_workspace.h`
- Modify: `src/developer_studio_debugger_workspace.cpp`
- Modify: `src/main.cpp`
- Modify: `tests/debugger_workspace_test.cpp`

**Interfaces:**
- Produces `DebuggerWorkspaceStorageLoad`, `DebuggerWorkspaceStorageSave`, `DebuggerWorkspaceStoragePath`, `DebuggerWorkspaceStorageBackupPath`, and bounded load/save status text.
- Storage functions consume `const WorkspaceFileSystem&`, project root, project ID, and a `DebuggerWorkspace`; they read/write only the two exact project-root metadata paths.

- [ ] **Step 1: Write failing fake-filesystem storage tests.**

  Extend the host test with an in-memory fake filesystem containing `stat`, `read`, and `write` callbacks. Test that `DebuggerWorkspaceStorageSave` writes `.bak` then primary, that a primary write failure leaves the in-memory model unchanged, that a malformed primary falls back to a valid backup, and that a project-root path for `D:/projects/a` never reads `D:/projects/b`.

- [ ] **Step 2: Run the focused storage tests to verify missing symbols.**

  Run:

  ```powershell
  cmake --build build-native-gcc --target developer_studio_debugger_workspace_test
  ctest --test-dir build-native-gcc -R developer_studio_debugger_workspace_test --output-on-failure
  ```

  Expected: FAIL at compile/link time because storage functions are not implemented yet.

- [ ] **Step 3: Implement exact storage paths and bounded read/write behavior.**

  Build primary and backup paths with `JoinWorkspacePath(projectRoot, "guidexos.debugger.json", ...)` and `JoinWorkspacePath(projectRoot, "guidexos.debugger.json.bak", ...)`. Before reading, `stat` the file and reject non-regular or larger-than-`kDebuggerWorkspaceMaxFileBytes` files. Read into a fixed buffer, parse all-or-nothing, and prefer the primary; only use backup when primary is absent or invalid. For save, serialize into a fixed buffer, write backup first, require `outBytes == serializedBytes`, then write primary and require the same. Return a distinct bounded error for serialization, callback, short-write, and parse failure.

- [ ] **Step 4: Run storage tests to verify the green behavior.**

  Run the focused CTest command from Step 2. Expected: PASS with primary/backup fallback and project isolation covered.

- [ ] **Step 5: Add the active configured workspace to `main.cpp` and load it on project open.**

  Include the new header and add `static DebuggerWorkspace g_debuggerWorkspace = {};` plus bounded status buffers near the existing debugger globals. Add helpers `debuggerWorkspaceReset`, `debuggerWorkspaceLoadForProject`, `debuggerWorkspaceSave`, and `debuggerWorkspaceReportStatus`. Call load after successful `commitProjectOpen` and `openCreatedProject`; clear it before `commitWorkspaceOpen` switches away from the current project. Do not call the storage layer until `g_controller.model.hasProject` is true.

- [ ] **Step 6: Load on application initialization without fabricating a project.**

  Initialize the workspace model empty in `gx_main`; leave it empty until a project opens. Keep the app usable when the file is absent or invalid, write a bounded output/status message, and emit Phase 28O diagnostic markers only when the new sentinel mode is active.

- [ ] **Step 7: Run the host test and existing project tests, then commit the storage slice.**

  Run:

  ```powershell
  ctest --test-dir build-native-gcc -R "developer_studio_(debugger_workspace|project)_test" --output-on-failure
  git diff --check
  git add src/developer_studio_debugger_workspace.h src/developer_studio_debugger_workspace.cpp src/main.cpp tests/debugger_workspace_test.cpp
  git commit -m "developer studio: persist debugger workspace files"
  ```

### Task 3: Make the source-editor and pane model configured-state aware

**Files:**
- Modify: `src/developer_studio_debug_editor.h`
- Modify: `src/developer_studio_debug_editor.cpp`
- Modify: `tests/debug_editor_test.cpp`
- Modify: `src/main.cpp`

**Interfaces:**
- Produces configured-row projection helpers used by `debugEditorRefreshBreakpoints`.
- Adds `DebugEditorBreakpointVisualState::Unresolved` and permits `id == 0` for configured rows while retaining strict project/path/line validation.

- [ ] **Step 1: Write failing editor-model tests for configured rows.**

  Add rows with `id == 0`, one enabled configured row, one disabled configured row, one unresolved row, and one live row with a nonzero ID at the same source identity. Assert configured rows are retained before Debug, unresolved rows return `Unresolved`, live rows replace the configured association without duplicating the source identity, and execution/inspection reset still clears only runtime locations.

- [ ] **Step 2: Run the focused editor test and verify the new assertions fail.**

  Run:

  ```powershell
  cmake --build build-native-gcc --target developer_studio_debug_editor_test
  ctest --test-dir build-native-gcc -R developer_studio_debug_editor_test --output-on-failure
  ```

  Expected: FAIL on the new configured/unresolved assertions.

- [ ] **Step 3: Extend the editor model without weakening Phase 28N identity rules.**

  Add the visual state and an `unresolved` flag/status field to `DebugEditorBreakpoint`. Change row acceptance to allow `id == 0` only for configured rows, preserve canonical project/path/line deduplication, prefer a live row over a configured row at the same identity, and return `Unresolved` before enabled/disabled checks when the flag is set.

- [ ] **Step 4: Run the editor test and existing debugger editor tests.**

  Run the command from Step 2. Expected: PASS, including all prior duplicate, path distinction, marker, and runtime reset assertions.

- [ ] **Step 5: Add a configured projection in `main.cpp`.**

  Replace the current pre-Debug empty/manager-only projection with a helper that converts `g_debuggerWorkspace.breakpoints` into `DebugEditorBreakpoint` rows using `id = 0`, current project ID, persisted path/line/column/enabled/action, and the current unresolved flag. When a valid manager snapshot exists, merge its live rows by canonical identity and set the live ID/action/enabled; retain configured rows that have no manager match. Keep `g_debugEditor` runtime locations untouched by breakpoint projection.

- [ ] **Step 6: Ensure source-file opening updates the gutter from configured state.**

  Call the configured projection after project load and from the existing document-open success path. Verify the projection uses full canonical paths so `src/main.cpp` and `tests/main.cpp` remain distinct. Leave execution markers absent because `DebugEditorModelResetRuntime` remains the only source for those runtime locations.

- [ ] **Step 7: Run focused and existing editor tests, then commit.**

  Run:

  ```powershell
  ctest --test-dir build-native-gcc -R "developer_studio_(debugger_workspace|debug_editor)_test" --output-on-failure
  git diff --check
  git add src/developer_studio_debug_editor.h src/developer_studio_debug_editor.cpp tests/debug_editor_test.cpp src/main.cpp
  git commit -m "developer studio: project configured breakpoint projection"
  ```

### Task 4: Route all breakpoint and Watch mutations through persistence

**Files:**
- Modify: `src/developer_studio_debugger_workspace.h`
- Modify: `src/developer_studio_debugger_workspace.cpp`
- Modify: `src/main.cpp`
- Modify: `tests/debugger_workspace_test.cpp`

**Interfaces:**
- Produces helpers `debuggerWorkspaceAddOrToggleBreakpoint`, `debuggerWorkspaceRemoveBreakpoint`, `debuggerWorkspaceSetBreakpointEnabled`, `debuggerWorkspaceUpdateBreakpointPolicy`, `debuggerWorkspaceAddWatch`, `debuggerWorkspaceEditWatch`, and `debuggerWorkspaceRemoveWatch`.
- These helpers accept the current project context and optional live manager row; they mutate the configured model only after manager acceptance when a live row exists, then call `debuggerWorkspaceSave`.

- [ ] **Step 1: Write failing model-level mutation tests.**

  Add tests that append breakpoints and Watches in order, toggle enabled state, update LOG/condition/hit-policy/threshold/template fields, remove a middle breakpoint and Watch, save/reparse, and assert the remaining order and fields. Assert `id`, address, and raw hit-count-like values are never copied into the persisted model.

- [ ] **Step 2: Run the focused test to verify missing model mutation helpers.**

  Run the focused workspace CTest command. Expected: FAIL because the model-level ordered mutation helpers do not yet exist. The application bridge is added only after these host-testable operations are green.

- [ ] **Step 3: Implement ordered model mutation helpers.**

  Add `DebuggerWorkspaceToggleBreakpoint`, `DebuggerWorkspaceRemoveBreakpoint`, `DebuggerWorkspaceUpdateBreakpoint`, `DebuggerWorkspaceAddWatch`, `DebuggerWorkspaceEditWatch`, and `DebuggerWorkspaceRemoveWatch` to the UI-independent module. Keep duplicate identity, capacity, canonical-path, and bounded-string decisions in this module so the host tests cover the same rules used by `main.cpp`.

- [ ] **Step 4: Implement no-session breakpoint mutations.**

  Update `toggleBreakpointAtCaret` and `debugUiAddBreakpointAtCaret` to call the model helpers. With no active Debug session, create a configured row with canonical project-relative identity, default BREAK/NONE policy, and no runtime ID. Duplicate identity toggles enabled state rather than appending. Refresh the configured editor projection and save immediately.

- [ ] **Step 5: Implement live-session mutation synchronization.**

  For active Debug, continue using `debugUiManagerCommand` for add/remove/enable/disable/configure. After a successful command, find the corresponding configured row by canonical source identity and copy only the source policy fields from the manager/UI inputs; never copy manager ID, target address, installed state, or raw hit count. If a manager command fails, leave the configured row unchanged and report the bounded manager error.

- [ ] **Step 6: Persist Watches in visible order and reset only results.**

  On add/edit/remove, compact `g_debugUiWatches` into persisted order, reject expressions at the existing 256-byte limit, save immediately, and keep runtime result structs transient. On project open, load expressions into the first eight UI slots in persisted order. On generation/session reset, clear only each result while retaining expression strings.

- [ ] **Step 7: Run focused, debugger Watch, and editor tests.**

  Run:

  ```powershell
  ctest --test-dir build-native-gcc -R "developer_studio_(debugger_workspace|debugger_watches|debug_editor)_test" --output-on-failure
  ```

  Expected: PASS with ordering, add/edit/remove, capacity, and runtime-result reset covered.

- [ ] **Step 8: Commit the mutation slice.**

  ```powershell
  git diff --check
  git add -- src/developer_studio_debugger_workspace.h src/developer_studio_debugger_workspace.cpp src/main.cpp tests/debugger_workspace_test.cpp
  git commit -m "developer studio: save debugger intent mutations"
  ```

### Task 5: Materialize persisted configuration for each Debug generation

**Files:**
- Modify: `src/developer_studio_debugger_workspace.h`
- Modify: `src/developer_studio_debugger_workspace.cpp`
- Modify: `src/main.cpp`
- Modify: `tests/debugger_workspace_test.cpp`
- Modify: `tests/debug_editor_test.cpp`

**Interfaces:**
- Produces `DebuggerWorkspaceMaterializationEntry` and `BuildDebuggerWorkspaceMaterializationPlan` in the UI-independent module, plus the `debuggerWorkspaceMaterialize`, `debuggerWorkspaceMergeLiveSnapshot`, `debuggerWorkspaceResetRuntime`, and `debuggerWorkspaceMarkUnresolved` application helpers.
- The host-testable plan contains only ordered source-level policy fields. `main.cpp` consumes that plan through the existing manager calls and keeps IDs, addresses, mappings, hit counts, and generation bindings transient.

- [ ] **Step 1: Write failing materialization-plan tests.**

  Add tests in `tests/debugger_workspace_test.cpp` that build a plan from enabled and disabled configured rows, preserve source order and policy fields, omit disabled rows, and prove the plan contains no runtime ID/address/session/hit-count fields. The manager-command trace and generation-A/B IDs are verified in the real diagnostic path because the existing manager API is owned by `main.cpp`.

- [ ] **Step 2: Run the test and verify the materialization-plan path fails.**

  Run the focused CTest target. Expected: FAIL because the materialization-plan interface is not present.

- [ ] **Step 3: Implement the ordered materialization plan and generation-safe application bridge.**

  Make `BuildDebuggerWorkspaceMaterializationPlan` emit enabled records in persisted order. Invoke the application bridge only after the existing Debug session has a valid handle and source-map readiness. For each plan entry, issue `GX_DEVELOPMENT_DEBUG_ADD_SOURCE_BREAKPOINT`; immediately issue `GX_DEVELOPMENT_DEBUG_CONFIGURE_SOURCE_BREAKPOINT_POLICY` with persisted policy fields. Refresh the authoritative snapshot after each accepted operation, bind the returned live ID only to the in-memory projection, and clear all prior live associations before generation B. For disabled rows, clear any stale live association and retain the configured row only.

- [ ] **Step 4: Implement unresolved retention.**

  When add/mapping returns a source-not-found or line-not-mapped condition, mark only that configured row unresolved for the current generation, report a bounded status, skip policy installation for that row, and continue materializing other rows. Never delete or rewrite the persisted record.

- [ ] **Step 5: Reset runtime state at generation/session boundaries.**

  Extend `debugUiResetRuntimeState`, `completeDebugShutdownIfReady`, and the Debug session-generation transition so all live snapshots, IDs, addresses, manager bindings, raw-hit-derived display, Watch results, selected frame, Output snapshot, execution marker, and inspection marker are cleared. Retain `g_debuggerWorkspace` and its ordered expressions. Ensure the next generation calls materialization again and receives new IDs.

- [ ] **Step 6: Re-run materialization and projection tests.**

  Run:

  ```powershell
  ctest --test-dir build-native-gcc -R "developer_studio_(debugger_workspace|debug_editor)_test" --output-on-failure
  ```

  Expected: PASS, including generation A/B ID freshness, no persisted address/ID, disabled behavior, unresolved retention, and runtime reset.

- [ ] **Step 7: Commit the runtime materialization slice.**

  ```powershell
  git diff --check
  git add src/main.cpp tests/debugger_workspace_test.cpp tests/debug_editor_test.cpp
  git commit -m "developer studio: rematerialize debugger workspace state"
  ```

### Task 6: Add Phase 28O diagnostics and real restart/reopen proof

**Files:**
- Modify: `src/main.cpp`
- Modify: `tests/run-developer-studio-debugger-required.ps1`
- Create: `docs/DEVELOPER_STUDIO_PHASE28O_DEBUGGER_WORKSPACE_PERSISTENCE.md` (initial evidence skeleton is filled in Task 8).

**Interfaces:**
- Produces a real `-Phase28OOnly`-equivalent sentinel-driven app path using the existing Developer Studio window, project open, editor handlers, Breakpoints pane, Watch pane, close event, relaunch, project reopen, and Debug start.
- Emits bounded `DEVELOPER_STUDIO_PHASE28O_*` markers for configure/save/close/relaunch/restore/materialize/hit/watch/reset/unresolved/corrupt/bounds/regression outcomes.

- [ ] **Step 1: Write failing marker assertions in the PowerShell harness.**

  Add a Phase 28O mode parameter/marker set to the existing required debugger harness. Assert the required begin, project-open, save, close, relaunch, restore, no-runtime-ID, Debug-start, materialize, fresh-ID, breakpoint-hit, Watch-reevaluate, hit-count-zero, second-generation, bounds, and PASS markers. Run the harness before adding app behavior and confirm it fails by timing out or missing the new begin marker.

- [ ] **Step 2: Add the Phase 28O sentinel and state machine to the real app.**

  Add `g_phase28oDiagnostic`, `g_phase28oFinished`, `g_phase28oFailed`, stage/deadline fields, and sentinel detection parallel to the Phase 28M/28N diagnostic path. Implement bounded phases: open `/P28O`, open source files, configure four policy variants and two Watches through the real handlers, save, close the app, relaunch the packaged app, reopen the same project, assert configured rows/ordering and no live IDs/markers, start Debug, assert materialization and fresh ID/count state, pause/hit/reevaluate Watch/LOG output, stop, start generation B, assert new IDs and zero raw counts, then cleanly terminate.

- [ ] **Step 3: Add corruption, capacity, unresolved, and isolation checks to the diagnostic state machine.**

  Use the real filesystem callbacks to write one malformed debugger file before an open and assert safe load failure/no arbitrary manager commands; use host tests for over-capacity and project isolation; create one unresolved configured path and assert it remains visible without a live ID while other entries materialize. Restore the valid file/configuration before the main Debug proof.

- [ ] **Step 4: Add marker assertions for Phase 28N regression and dirty-buffer preservation.**

  Keep the existing Phase 28N source-editor marker checks active in Phase 28O mode. Modify an open source buffer without saving, close/reopen debugger configuration through the app lifecycle, and assert source buffer dirty state/text remains authoritative and debugger metadata does not overwrite it.

- [ ] **Step 5: Run PowerShell parse and the focused diagnostic harness.**

  Run:

  ```powershell
  $scripts = Get-ChildItem tests -Filter '*.ps1' -File
  foreach ($script in $scripts) { [System.Management.Automation.Language.Parser]::ParseFile($script.FullName, [ref]$null, [ref]$null) | Out-Null }
  powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests/run-developer-studio-debugger-required.ps1 -Phase28OOnly -ServerRoot D:\dev\guideXOSServerV0.5_DEVELOPER_STUDIO
  ```

  Expected before package rebuild: parse succeeds; the diagnostic command may report missing Phase 28O package artifacts, which is the expected red state for this task.

- [ ] **Step 6: Commit the standalone diagnostic/harness slice.**

  ```powershell
  git diff --check
  git add -- src/main.cpp tests/run-developer-studio-debugger-required.ps1 docs/DEVELOPER_STUDIO_PHASE28O_DEBUGGER_WORKSPACE_PERSISTENCE.md
  git commit -m "developer studio: add phase 28O persistence diagnostics"
  ```

### Task 7: Stage server fixture, package builds, and three fresh QEMU boots

**Files:**
- Server create/modify: `D:\dev\guideXOSServerV0.5_DEVELOPER_STUDIO\ESP\P28O\CMakeLists.txt`, `README.md`, `app\app.json`, `build.ps1`, `guidexos.project`, `src\helper.cpp`, `src\main.cpp`, and `ESP\Apps\DeveloperStudio\.phase28o-diagnostic`.
- Server modify: `D:\dev\guideXOSServerV0.5_DEVELOPER_STUDIO\ESP\Apps\DeveloperStudio\app.json`, `bin\amd64\developerstudio.elf`, and `bin\arm64\developerstudio.elf` when the normal package build produces updated tracked artifacts.

**Interfaces:**
- Produces normal AMD64/ARM64 package artifacts from the standalone Phase 28O branch through the existing build path, with no manual binary patching.
- Produces three isolated final-package QEMU boot traces proving configure → close → relaunch → restore → Debug → hit/evaluate → clean termination.

- [ ] **Step 1: Inspect the existing Phase 28M/28N package and QEMU staging path.**

  Read `build.ps1`, `tests/validate-developer-studio-package.ps1`, `tests/run-developer-studio-debugger-required.ps1`, the server `ESP\P28M` fixture, and package audit rules. Confirm that `ESP\Apps\DeveloperStudio\.phase28o-diagnostic` is the only new sentinel and that the `ESP\P28O` fixture is outside the package compiler discovery path.

- [ ] **Step 2: Add the bounded Phase 28O server fixture/staging changes.**

  Reuse the existing `ESP\P28M` fixture pattern in the exact `ESP\P28O` files listed above, with source lines/functions that exercise BREAK, disabled, conditional/hit-count, LOG/template, and two Watch expressions. Keep the fixture inside the server evidence tree and outside compiler discovery for the Developer Studio package itself. Add the `.phase28o-diagnostic` marker file to the normal package staging.

- [ ] **Step 3: Build and audit AMD64 and ARM64 packages.**

  Run the normal package script twice, once per architecture, followed by the package audit:

  ```powershell
  powershell.exe -NoProfile -ExecutionPolicy Bypass -File build.ps1 -ServerRoot D:\dev\guideXOSServerV0.5_DEVELOPER_STUDIO -Configuration Debug -TargetArchitecture amd64
  powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests/validate-developer-studio-package.ps1 -ServerRoot D:\dev\guideXOSServerV0.5_DEVELOPER_STUDIO
  powershell.exe -NoProfile -ExecutionPolicy Bypass -File build.ps1 -ServerRoot D:\dev\guideXOSServerV0.5_DEVELOPER_STUDIO -Configuration Debug -TargetArchitecture arm64
  ```

  Record exact size, SHA-256, and ELF machine from the staged binaries. Do not manually edit the binaries.

- [ ] **Step 4: Run three fresh isolated packaged boots.**

  Execute the established real Developer Studio debugger-required/QEMU harness three times with independent staging/trace directories. Require each run to observe configuration, save, close, relaunch, restore, Debug materialization with fresh IDs, Watch reevaluation, one real breakpoint hit, and clean termination. Require at least one run to observe LOG output, disabled restore, conditional/hit-policy restore, and raw hit count reset.

- [ ] **Step 5: Run the corrupt-file and unresolved guest controls.**

  Use one focused guest/host run to present truncated/invalid debugger metadata, assert safe load failure, then restore the valid file. In the same or a separate bounded run, assert an unresolved breakpoint remains persisted and does not install a live patch.

- [ ] **Step 6: Commit the server package/validation slice without touching history.**

  First inspect `git status --short --untracked-files=all` and stage only the exact Phase 28O fixture and package paths listed in this task. Then commit in the server repository:

  ```powershell
  git diff --check
  git add -- ESP/P28O/CMakeLists.txt ESP/P28O/README.md ESP/P28O/app/app.json ESP/P28O/build.ps1 ESP/P28O/guidexos.project ESP/P28O/src/helper.cpp ESP/P28O/src/main.cpp ESP/Apps/DeveloperStudio/.phase28o-diagnostic ESP/Apps/DeveloperStudio/app.json ESP/Apps/DeveloperStudio/bin/amd64/developerstudio.elf ESP/Apps/DeveloperStudio/bin/arm64/developerstudio.elf
  git commit -m "developer studio: persist debugger workspace"
  ```

  Do not amend the existing package commit, push, fetch, pull, merge, rebase, reset, or clean. Preserve unrelated generated evidence.

### Task 8: Full verification, documentation, and final provenance

**Files:**
- Modify: `docs/DEVELOPER_STUDIO_PHASE28O_DEBUGGER_WORKSPACE_PERSISTENCE.md`
- No other files are planned for this task; any newly discovered regression must be fixed in the task where it is detected and covered by that task's focused test before this final verification task begins.

**Interfaces:**
- Produces the final Phase 28O evidence record and clean local commits on both repositories, with standalone `main` unchanged and no pushes.

- [ ] **Step 1: Run the full standalone host validation.**

  Run the repository's fast validation script and complete CTest suite from the Phase 28O branch, then run `git diff --check` and PowerShell parse validation. Capture exact test counts and failures; if any fail, add a focused regression test before changing implementation.

- [ ] **Step 2: Run backend and ABI regressions in the server repository.**

  Run the relevant Phase 28I–28N Native ELF debugger/watch/breakpoint/logpoint tests, filesystem contract, ABI layout, validator, trampoline, compiler object ABI, and PowerShell parse checks using the server's aggregate commands. Record object ABI 10 and debugger ABI version 1 results.

- [ ] **Step 3: Complete the final documentation from verified evidence.**

  Fill `docs/DEVELOPER_STUDIO_PHASE28O_DEBUGGER_WORKSPACE_PERSISTENCE.md` with the actual storage paths, schema/version, two-slot write behavior, capacities, persisted/non-persisted fields, configured/live model, close/relaunch proof, restored policy examples, old/new IDs and addresses, raw-hit reset, Watch reset/reevaluation, unresolved/corrupt/over-capacity/isolation results, dirty-buffer result, package hashes, ABI values, regression counts, three boot outcomes, storage lifetime, physical mouse status, clean worktrees, and nothing-pushed status. Do not state an outcome or pass marker not present in captured output.

- [ ] **Step 4: Run final verification commands before claiming completion.**

  Run fresh commands:

  ```powershell
  git -C D:\dev\guideXOS_Developer_Studio status --short --untracked-files=all
  git -C D:\dev\guideXOS_Developer_Studio log -1 --format='%H%n%s'
  git -C D:\dev\guideXOS_Developer_Studio rev-parse main
  git -C D:\dev\guideXOSServerV0.5_DEVELOPER_STUDIO status --short --untracked-files=all
  git -C D:\dev\guideXOSServerV0.5_DEVELOPER_STUDIO log -1 --format='%H%n%s'
  git -C D:\dev\guideXOS_Developer_Studio diff main --stat
  git -C D:\dev\guideXOS_Developer_Studio diff --check main
  ```

  Read the complete output and report actual clean/dirty state, commit hashes, server divergence from its upstream, preserved evidence, package hashes/sizes, and any remaining limitation. Mark Outcome A only if all required persistence, rematerialization, regression, and three-boot evidence is present; otherwise report the exact lower outcome and boundary.

- [ ] **Step 5: Commit the final standalone documentation and report provenance.**

  ```powershell
  git add -- docs/DEVELOPER_STUDIO_PHASE28O_DEBUGGER_WORKSPACE_PERSISTENCE.md
  git commit -m "docs: record phase 28O debugger persistence"
  git status --short --untracked-files=all
  ```

  Report that standalone `main` remains at `33c37e56df6dd70e0963b2caca824e100f5e3d7e`, the Phase 28O branch point is `77d861f0...`, server starting baseline was live `b1ea3d1...`, nothing was pushed, and any preserved untracked evidence remains untouched.
