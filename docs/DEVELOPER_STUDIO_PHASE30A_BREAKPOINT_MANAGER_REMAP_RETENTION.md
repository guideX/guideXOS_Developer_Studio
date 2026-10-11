# guideXOS Developer Studio — Phase 30A
## Persistent Breakpoint Manager Row and Condition Remap Retention

**Outcome B.** The missing hosted persistent Manager provider was implemented and its five-session gate passed. A focused rebuild/remap lifecycle also passed, but the formal five-cycle remap gate was interrupted during cycle 4 when its evidence directory was moved. The required 5/5 remap gate and dependent 25/25 hosted stress therefore remain incomplete.

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

The formal gate `tests/smoke-developer-studio-phase30a-remap-gate.ps1` began once with five sequential fresh fixtures. Cycles 1–3 passed. During cycle 4, the evidence root was moved from `D:\dev\g30a-remap-final` to `D:\dev\bkup\unknown\g30a-remap-final` while the hosted run was using its original absolute fixture path. The process then reported `Remap fixture source is missing` for the old `fixture-4\src\main.cpp` path. The file is present in the relocated cycle-4 fixture, confirming the path interruption. Cycle 5 did not start. The gate's no-retry rule was followed; the formal result is **3/5 PASS, interrupted in cycle 4**, not a product remap failure. The focused preflight is supporting evidence and does not turn this into a 5/5 formal result.

Because the required formal remap gate did not complete, the dependent combined hosted projection/retention stress gate was not run. Required formal results are therefore Manager **5/5**, remap **3/5 interrupted**, combined stress **not run**.

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

## Outcome and remaining boundary

The hosted persistent Manager-provider defect is fixed and the Manager route passed 5/5. The focused remap lifecycle demonstrates condition retention, fresh mappings, fresh runtime binding, conditional execution, stepping, inspection refresh, and teardown. The formal remap gate is incomplete because the evidence directory was moved during cycle 4; the no-retry rule prevented another formal run. Outcome **B** applies. The exact remaining qualification boundary is a complete formal remap gate at 5/5 and its dependent combined hosted stress at 25/25. The historical long-root `malformed_dwarf` report remains a separate preserved observation.
