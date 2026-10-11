# guideXOS Developer Studio — Phase 30A
## Persistent Breakpoint Manager Row and Condition Remap Retention

**Current result: Outcome B.** The hosted persistent Manager provider and focused rebuild/remap gates remain qualified at 5/5 each. The historical combined run stopped at 13/25 on cycle 14 because its marker baseline was sampled after a valid OPEN event. That harness freshness defect was corrected and the repeated open/close marker gate passed 10/10. A completely fresh combined run then passed cycles 1–14 and stopped on cycle 15/25 while entering the initial condition in the scheduled remap-retention case. The required 25/25 combined stress remains incomplete.

## Phase gate and starting state

No authoritative phase marker or earlier Phase 30A closeout was found. The standalone history ended with Phase 29Z, so Phase 30A was current. The prompt's expected divergence did not match the live upstream state; live Git state was clean and 0 ahead / 0 behind in both repositories.

| Repository | Starting branch / HEAD | Ahead / behind | Starting worktree |
|---|---|---:|---|
| Standalone | `main` / `eccc4e6f4cff4a59a8f113abbdb006b349b700b9` | 0 / 0 | Clean |
| Server integration | `v0.5_DEVELOPER_STUDIO` / `e205dd6fb05190bd5374bc9d44346fcebd95608b` | 0 / 0 | Clean |

The Phase 29Z QEMU baseline remains frozen: focused diagnostic 1/1, full acceptance 10/10, ownership stress 25/25; no `invalid_project_root` or Phase 28Q failure was reported in that baseline. This phase changed the hosted Manager projection and UI routes. It did not change the canonical workspace breakpoint record, controller, runtime binding, or ownership of workspace/build generations, so no Phase 30A QEMU regression was required or run. Physical hardware was not used.

## Provider audit and architecture

**Classification A — missing hosted implementation.** Hosted `debugUiRefreshBreakpoints` refreshed editor projections but did not publish persistent Manager rows. The bare-metal Manager continued to use its Server snapshot path.

The hosted provider is `DebuggerWorkspaceBreakpointProvider`; its snapshot uses the same type. `debugUiRefreshBreakpoints` refreshes editor rows and calls `DebuggerWorkspaceBreakpointProviderRefresh`. Its source is the canonical `g_debuggerWorkspace.breakpoints` collection. Each row contains project ID, normalized source path, line, column, workspace index, project generation, and workspace generation. The provider contains no editable condition or policy copy.

The logical Manager row identity is the project-scoped source key plus generation stamps:

`com.example.debuggerphase15 / src/main.cpp:37 @ project generation / workspace generation`

There is no durable numeric Manager ID. A row resolves through `DebuggerWorkspaceBreakpointProviderResolve`, which checks both generations and resolves the source path and line back to the same `DebuggerWorkspaceBreakpoint`. The editor projection uses the same source key. Its row index and runtime IDs are view/session details, not configuration identity.

When a target is active, the Manager view joins runtime state from the current controller by source path and line, and accepts it only for the current session generation. It renders the current runtime breakpoint ID, binding ID, and mapped address. When the target is stopped or torn down, those fields become unbound while the workspace row and condition remain. Workspace refresh, breakpoint add/remove, project change, and generation mismatch rebuild or invalidate the provider. A stale provider cannot resolve into a new workspace generation.

## Hosted Manager gate

`tests/smoke-developer-studio-phase30a-manager-gate.ps1` completed **5/5 PASS** using five fresh fixture copies and the production hosted UI path. Evidence was created under `D:\dev\g30a-mgr-final` and later relocated to `D:\dev\bkup\unknown\g30a-mgr-final`.

For each run, the test created one canonical workspace breakpoint at `src/main.cpp:37`, published one Manager row and one editor projection, and verified `logical_count=1`. Before launch the selected row reported runtime ID, session, and binding as zero. The Manager opened the production condition editor (`debug_condition_editor=OPEN`), committed `counter == 2`, and the editor reopened with the same workspace value. The editor then changed it to `counter >= 2`; reopening the Manager showed that value. The run restored `counter == 2` for the runtime condition check.

The test removed the row through the Manager, observed the provider row count fall to zero, recreated it through the supported path, and confirmed the new canonical record had no condition (`condition=<none>`). No duplicate logical record was created. All five hosted lifecycle sessions closed successfully.

## Rebuild and remap evidence

A focused `ManagerRemapRetention` preflight completed successfully at `D:\dev\g30a-rmp-pf8` (now `D:\dev\bkup\unknown\g30a-rmp-pf8`). The fixture source hash changed from `9636FCE10823AD2FF7CD61DB41E7FEB8BE93A79934BC8722695CDD0D3CE9E175` to `73B17D4D1E8AAE4933D328319B19C0DE74202F6F26DA4F61F1D3CABCEDEB8EE1` after changing a preceding statement while preserving the source key `src/main.cpp:37`.

| Measurement | First build/session | Rebuild/session |
|---|---|---|
| Application artifact SHA-256 | `ecbb42704c20c9685749e93ec415a56271af82bab1afdacbee6eccd0ae6c1ec9` | `15bf4f0c56ce56cef79c8fa83d7735ec5d1f7c83938e96971b6574b25c381785` |
| `src/main.cpp:37` address mapping | `0x200016AC`, `0x200016AF`, `0x200016B2`, `0x200016B5`, `0x200016B8` | `0x200016BD`, `0x200016C0`, `0x200016C3`, `0x200016C6`, `0x200016C9` |
| Primary address | `0x200016AC` | `0x200016BD` |
| Debug session | 1 | 2 |

The five-address mapping changed. The first session's runtime binding was removed on target stop; the second session created a fresh binding against the rebuilt artifact. Numeric binding IDs were emitted by the live marker path but were not retained in the moved preflight artifact set. The workspace condition remained `counter == 2`; the Manager row and editor both showed it after remap. The false condition continued without stopping, and the true condition stopped at the intended value. The test exercised Manager-to-editor condition editing, Step Into, Step Over, Step Out, refreshed inspection state, target stop, teardown, relaunch, removal, and recreation with an empty condition. The application closed cleanly after targets were stopped.

The first formal gate attempt used five sequential fresh fixtures. Cycles 1–3 passed. During cycle 4, the evidence root was moved from `D:\dev\g30a-remap-final` to `D:\dev\bkup\unknown\g30a-remap-final` while the hosted run was using its original absolute fixture path. It reported `Remap fixture source is missing` for the old `fixture-4\src\main.cpp` path. The file was present in the relocated fixture, confirming the path interruption. Cycle 5 did not start, and that attempt was not retried.

After the interruption was identified, a new formal run started from cycle 1 with five fresh fixtures under `D:\dev\g30a-remap-recovered`. It completed **5/5 PASS**. Per cycle, the pre-build artifact hashes were respectively `16156766B3F3776C24C92797F2C1C56CE902687EBF6EB3D5F16CE64AF831C982`, `3B3DA5B4F7E151B9144DC3694396285F188661DE4E3BD372D7DEACB0416F2EC1`, `EFEFEA5914C51F83A65CCAC079CAEB4632327FFBDA4F3EB1262258FFD3C51C1C`, `C600A8AD1FA648EC752E206B4D59AB58A8455C8FEFFDAF789C035AE524590BAC`, and `A8DC10A5BC12E13FCE0D61C5B484BA07692EA20EF7FE416DAF649B53DB7963AC`; post-build hashes were `94F80321E0EEC2E1AEA89207F53F2FD15B1208852305B24992403BD2D11F18C5`, `DDFB111F1A55916AA5D1050C98888FD75A1BC72F5FC568678AB41FAE6912E5B7`, `84C77FAD5A7E1B0985C47B0B6C9FEB5DA8A9611D8302E566F0565ECC9D3EB5E6`, `40E00BCA9F05AEF51A3A15E50EBE11A703E0866EDFDA90B7169BCC9DFF56CCE6`, and `3F57C91D382310AD3AA2FFCCB069173CD5685395DE91D6AFDDE5E4B967CBFA75`. Every run retained the five-address logical mapping change from `0x200016AC, 0x200016AF, 0x200016B2, 0x200016B5, 0x200016B8` (primary `0x200016AC`) to `0x200016BD, 0x200016C0, 0x200016C3, 0x200016C6, 0x200016C9` (primary `0x200016BD`) and created fresh runtime sessions 1 and 2.

The first combined stress run used a fresh fixture and hosted process per cycle. Its cadence was four `ManagerConditionEditor` cases followed by one `ManagerRemapRetention` case, repeated five times. Cycles 1–13 passed. Historical cycle 14 stopped at `debug_condition_editor=OPEN breakpoint_id=pending source=src/main.cpp:37 origin=manager` with baseline count 1 and actual count 1. Its retained trace contains the Manager-origin OPEN event, the rendered `debug_condition_editor=OPEN breakpoint_id=0 ... identity=workspace_configured` event, and a visible condition modal. The no-retry rule was followed. The first run therefore remains **13/25 PASS, stopped at cycle 14**; cycles 15–25 did not run in that run. Its root cause and the subsequent continuation are recorded below.

## Independent regression and package results

| Gate | Result |
|---|---|
| Workspace/provider focused CTest | PASS |
| Full Debug CTest | 33/33 PASS, sequential |
| Full Release CTest | 33/33 PASS, sequential |
| Positive GXSM lifecycle | PASS on a clean copied `debugger-phase29q-positive` fixture: GXSM v2, live counter values, equality condition, refresh/invalidation, relaunch, target exit 0, clean teardown |
| Phase 29V DWARF baseline | PASS: 413 DIEs; `Ready`, `error=none`, `truncated=0`; `src/main.cpp:37` maps to five addresses with primary `0x200016AC` and reverse maps to line 37 |
| DIE boundary | 511 PASS; 512 PASS; 513 `limit_exceeded`; malformed synthetic DWARF rejected as `malformed_dwarf` |
| Long-root malformed-DWARF observation | Preserved separately from Phase 29X; not re-investigated because the controlled remap preflight did not reproduce it |
| QEMU regression | Not run; hosted provider/UI projection change only, with canonical ownership and runtime/controller behavior unchanged |
| Physical hardware | Not tested |

The sequential CTest runs passed after an earlier parallel Debug run encountered a shared temporary-path race in the symbol test. The package builds were made from the standalone production source and audited as ELF64 stripped/sectionless payloads with the expected file set:

| Architecture | Server package size | SHA-256 |
|---|---:|---|
| AMD64 | 1,125,884 bytes | `BAE1D5CF382D66EEC2DEB9AF840E543086EE5DC93D74590E289E3CEDAB7BFAF3` |
| ARM64 | 1,286,380 bytes | `AB0F3BDF55F8660F6542392DE4D9740FC1ADF91327892FFAC5B00847967A804F` |

The Server integration package also received the generated app manifest and the two rebuilt ELF payloads. The incidental `desktop.json` window-state change from the hosted runs was discarded.

## Phase 30A continuation: cycle 14 diagnosis and fresh gate

### Continuation starting state

Live continuation heads were Standalone `main` at `73f44689cc121dc333ee9dcb05768d260fbcd94b` and Server `v0.5_DEVELOPER_STUDIO` at `c237db2e8c484c1adcafa786ee8e8773bd386346`. Both worktrees were clean, each 1 ahead / 0 behind, and neither branch nor remote configuration was changed. At the end of this continuation, only Standalone harness and documentation files were changed; there is no Server package change.

The accepted Manager 5/5 and fresh Remap 5/5 results were preserved. The earlier remap attempt whose evidence directory was externally moved remains an invalidated infrastructure run, not a product failure. No regression suite or package was rebuilt for this harness-only continuation.

### Historical cycle 13 versus cycle 14

Successful-case stdout and UI traces were removed by the Phase 20 runner. The cycle-13 trace directory is empty, so the comparison below separates retained facts from facts established only by the runner/case setup.

| Field | Historical cycle 13 | Historical cycle 14 |
|---|---|---|
| Cycle/run token | No per-cycle token was emitted or retained; outer run identity is not in the cycle trace | No per-cycle token was emitted or retained; fixture and trace use index 14 |
| Workspace generation | Not retained; the identical fresh Manager fixture normally starts at generation 2, but this is not direct cycle-13 evidence | `manager_workspace_generation=2`; provider marker reports project generation 1 / workspace generation 2 |
| Canonical breakpoint identity | Fixture and test case identify `com.example.debuggerphase15/src/main.cpp:37`; no cycle-13 row marker was retained | `com.example.debuggerphase15/src/main.cpp:37`, source `src/main.cpp:37`, one logical row |
| Manager row identity | Stable source-key identity is implied by the case setup, but no row line was retained | `com.example.debuggerphase15/src/main.cpp:37`, `editor_row=0`, `editor_row_id=0`, `manager_id=0` (no active runtime Manager ID) |
| Condition | Cycle-13 fixture records `counter == 2`; no modal text trace was retained | Cycle-14 fixture starts with an empty condition; Manager row shows `condition=<none>` and the visible editor is empty |
| Editor initially closed / open request | The case starts with the Manager view and issues its open actions; exact UI trace was not retained | UI trace shows Manager row before the modal; the `C` open action was issued |
| Modal rendered / current identity | Successful case and teardown imply the modal route ran, but raw render evidence is absent | Yes. “Breakpoint Condition” is drawn; OPEN marker names the Manager source key and workspace generation 2 |
| OPEN marker/count | No count or exact marker sequence retained | Exact Manager OPEN line appears; fresh-wait baseline was 1 and post-action count remained 1 |
| Close result | The case ended with targeted window close while its final modal was active. The outer runner waited for successful hosted lifecycle exit before launching the next fresh process. A modal-specific CLOSED line is not retained | Not applicable; this was the next process and the modal genuinely opened |
| Manager snapshot | Numeric cycle-13 snapshot generation is not retained | `manager_snapshot=workspace`, workspace generation 2; no distinct numeric snapshot generation is emitted |

The cycle-13 and cycle-14 adjacent processes were isolated: the runner completed cycle 13, including its hosted close/lifecycle checks, before cycle 14 started in a fresh fixture/process. The old records do not prove a particular modal instance ID was destroyed because the product has no dialog-generation identity and successful modal-close stdout was discarded. They do rule out a still-running cycle-13 process/modal satisfying cycle 14.

### Marker producer and historical failure classification

The product marker is emitted in `src/main.cpp` by `drawModal` when `g_inputMode == InputMode::BreakpointCondition` and `g_debugConditionEditorOpenMarkerPending` is true. `beginBreakpointConditionPrompt` resolves the selected Manager source row back to the canonical `DebuggerWorkspaceBreakpoint`, copies its condition into `g_prompt`, enters `BreakpointCondition`, and sets the pending flag. The first modal render clears the flag, emits the detailed OPEN line, then calls `logBreakpointConditionUi` for the editor-rendered OPEN line. This is once per accepted open/render transition, not once per breakpoint, session, or process. The state is a real transition gate; an already-open modal does not generate another pending OPEN event. There is no dialog generation number.

The detailed emitted line in the historical cycle-14 trace was:

```text
GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_condition_editor=OPEN breakpoint_id=pending source=src/main.cpp:37 origin=manager editor_row=0 editor_row_id=0 manager_id=0 manager_row_id=com.example.debuggerphase15/src/main.cpp:37 manager_workspace_generation=2 manager_snapshot=workspace
```

The second line was:

```text
GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_condition_editor=OPEN breakpoint_id=0 text= identity=workspace_configured
```

The harness waits for the literal Manager-specific prefix `debug_condition_editor=OPEN breakpoint_id=pending source=src/main.cpp:37 origin=manager`. Before the action, however, the old capture key was only `debug_condition_editor=OPEN breakpoint_id=`. `Wait-MarkerFresh` stores captured baselines under the exact marker string. Because these strings differed, the waiter did not find the pre-action baseline and sampled the full matcher after the `C` action and product emission. That late baseline was 1; the count remained 1, yielding the retained timeout `baseline=1 actualCount=1`. This is **classification C: harness baseline race**, caused by an exact-key mismatch in the capture/wait matcher. It was not a product marker publication defect, log deduplication, event routing, stale Manager row, stale breakpoint, or a modal-close lifecycle defect.

`Get-MarkerCount` uses `Select-String -SimpleMatch` over the captured Server stdout and counts matching lines. The Server logger appends each message to the stdout stream; it does not coalesce identical UI marker lines. Both OPEN lines use the normal product logger/host log path. The event was not routed elsewhere. The historical baseline race was caused by ordering: the product emitted the fresh OPEN after `C`, then the waiter took a new baseline because the pre-action key did not match.

No product diagnostic identity change was justified. There is no existing dialog-generation identity, and the current detailed line already contains workspace generation, stable Manager row identity, source, and origin. The corrected harness now captures the exact same matcher before each action and requires the post-action occurrence count to increase. It records whether a baseline was captured before action or sampled at wait time; it does not add sleeps, retries, double-clicks, or a modal-visibility-only pass condition.

### Focused repeated marker gate

`tests/smoke-developer-studio-phase30a-marker-gate.ps1` completed **10/10 PASS** at `D:\dev\g30a-marker-c6218767`. Outer token: `f50e049c14fd4a5496477a31e2052cfd`; inner gate token: `25b142c722904e36994bd3335d3a7f46`. The artifact is `D:\dev\g30a-marker-c6218767\traces-1\phase30a-marker-gate-10.log`.

The ten measured cycles each required a current Manager row selection after the previous close, captured the exact detailed OPEN matcher before `C`, observed both the new detailed OPEN and rendered editor content, cancelled the editor, and observed the fresh CLOSED marker before the next cycle. The ten opens shared workspace/provider generation 3 and the canonical identity `com.example.debuggerphase15/src/main.cpp:37`; condition remained `counter == 2`, logical row count remained 1, and runtime binding remained absent. Each measured OPEN count advanced by one event occurrence (the detailed plus generic line counts are tracked by their respective matchers); all ten closes were fresh CANCEL events. No duplicate logical breakpoint appeared. This qualifies the historical marker issue and preserves both focused 5/5 baselines.

### Fresh combined hosted gate and cycle-15 boundary

Only after the 10/10 marker gate passed, a new formal 25-cycle run started from cycle 1. Its unique evidence root is `D:\dev\g30a-stress-4c87c1f2`; manifest/run token is `1451b5e2c99348ebb1cfce99c5872b04`. The manifest was written before execution and records its evidence root, run token, cycle, fixture, trace directory, completion state, and failure state. No evidence directory was moved. Cycles 1–14 passed. The cadence remained four `ManagerConditionEditor` cases followed by one `ManagerRemapRetention` case, repeated five times. Thus cycles 5 and 10 completed remap retention; cycle 15 was the next scheduled remap-retention case.

Cycle 15 stopped at the first failed invariant and no cycle 16 or retry ran. Its fixture and trace are `D:\dev\g30a-stress-4c87c1f2\fixture-15` and `D:\dev\g30a-stress-4c87c1f2\traces-15\phase30a-stress-15.log`. The manifest ends `state=failed`, `completedCycles=14`, `failedCycle=15`, `currentCase=ManagerRemapRetention`.

The first failing check was in `Add-ConditionText('counter == 2')`, before condition commit and before the first build/remap. The exact matcher was ` text=counter == 2`; its captured-before-action baseline was 0 and the final count was 0. The editor had legitimately opened through Manager selection: the trace has the fresh Manager OPEN line for source row `com.example.debuggerphase15/src/main.cpp:37`, workspace generation 2, `manager_snapshot=workspace`; the Manager row began at `condition=<none>`, one logical row, runtime unbound. The modal initially showed the empty condition, as expected for this setup.

The retained stdout at `C:\Users\guideX\AppData\Local\Temp\guidexos-phase20-c342c888de87462283d19ddc89dbc490.out` contains `debug_condition_input` markers through `text=counter =`, then no complete `text=counter == 2` marker. The text-entry helper issued the remaining `gui.keyto` down commands, including key 187 (`=`) twice and key 50 (`2`), and the Server printed `Key queued`; the product input marker stream did not reach the requested value before timeout. The fixture JSON and backup both still contain an empty condition, consistent with stopping before commit. The trace does not establish whether the missing key events were dropped by the hosted input/compositor path or by another input-handling boundary. It does establish that this is not a cycle-14 OPEN freshness failure and not evidence of a stale Manager/breakpoint identity. The first failed invariant is **condition-text input delivery in the hosted Manager condition-editor path**, before remap qualification begins.

The new combined gate therefore records **14/25 PASS, stopped at cycle 15**. This is Outcome **B**: the marker freshness issue is repaired and qualified, but a later combined-stress invariant failed. The remap operation itself was not reached in cycle 15, so this run provides no cycle-15 remap, condition-retention, runtime-rebinding, or post-remap execution result. Cycles 16–25 were not run.

### Continuation changes and retained qualifications

Production UI/model code did not change. The harness changes are limited to exact pre-action matcher baselines, added current-cycle repeated OPEN/CLOSED evidence, improved freshness diagnostics, and unique evidence-directory/manifest checks that fail explicitly if the owned directory disappears. The historical Manager 5/5 and remap 5/5 results remain valid. The fresh combined run validates cycles 1–14 only; it does not replace either focused qualification.

No Debug/Release CTest, GXSM, DWARF, package build, or QEMU run was repeated because the source product and Server package did not change. The previously audited package hashes remain AMD64 `BAE1D5CF382D66EEC2DEB9AF840E543086EE5DC93D74590E289E3CEDAB7BFAF3` and ARM64 `AB0F3BDF55F8660F6542392DE4D9740FC1ADF91327892FFAC5B00847967A804F`. QEMU was not required for these harness-only changes. The failed run's evidence directory remained present throughout and its manifest records the failure rather than falling back to another path.

The remaining boundary is the hosted condition-text input event path in the remap-retention case, followed by completion of a new, fully passing 25/25 combined run after that boundary is resolved. The failed cycle 15 must remain recorded; it is not replaced or counted as a pass. The historical long-root `malformed_dwarf` report remains a separate preserved observation.

## Outcome and remaining boundary

The hosted persistent Manager provider passed its 5/5 gate and the fresh formal remap gate passed 5/5. Historical cycle 14 was classified as a harness baseline race; its exact matcher key was corrected, and the focused repeated marker gate passed 10/10. A fresh combined gate then passed cycles 1–14 and stopped on cycle 15 while the condition text remained incomplete. Outcome **B** applies. The full combined gate remains unqualified at **14/25 PASS**; the earlier run's **13/25 PASS** remains a separate historical failure. No cycle was retried or carried forward.
