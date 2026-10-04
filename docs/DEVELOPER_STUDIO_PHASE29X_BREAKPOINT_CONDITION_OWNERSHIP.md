# Developer Studio Phase 29X — Breakpoint Condition Ownership

## Result

**Outcome B — the editor-owned line-37 condition path is repaired and qualified, but the persistent Breakpoint Manager selection route has no hosted provider to exercise, and one longer-root DWARF run remains path-sensitive.** The editor and controller surfaces now resolve the same project-scoped breakpoint configuration. The remaining qualification boundaries are listed at the end.

The primary defect was **A. View-specific selection defect**: the editor had a configured source breakpoint, but the condition command only admitted a Manager selection backed by `g_debugUiBreakpointValid`. The Manager snapshot is a view/provider result, not the canonical breakpoint registry.

## Repository and protection state

- The duplicate/stale-prompt gate ran before edits. No authoritative phase marker, Phase 29X completion commit, or `docs/DEVELOPER_STUDIO_PHASE29X_*` artifact existed. Phase 29X was next; no changes were made for a stale prompt.
- Standalone repository: `D:\dev\guideXOS_Developer_Studio`, branch `main`, starting HEAD `4ead4cc8b255ea6b674b4124afce551f9b0566b0`. Live initial divergence was **0 ahead / 0 behind** against `origin/main`, rather than the prompt's expected 5/0.
- Server repository: `D:\dev\guideXOSServerV0.5_DEVELOPER_STUDIO`, branch `v0.5_DEVELOPER_STUDIO`, starting HEAD `3b47738fbd4616990c05c11d6e1f88fe08d58b4c`. Live initial divergence was **0/0**, rather than the prompt's expected 1/0.
- No Server source, SDK, ABI, or tracked files were edited. Its only worktree entries stayed protected and their SHA-256 values remained:
  - `Apps/DeveloperStudio/app.json` — `5793567C54ABF22423A8FCDE2F9B32E5DEEB73981E070707BA11E7D23F390401`
  - `Apps/DeveloperStudio/bin/amd64/developerstudio.elf` — `106BD4D1C8827894AB6CE4850E5DC30E55E98A1BAA52BCA149027781B4081FE9`
  - `ESP/Apps/DeveloperStudio/.phase28q-diagnostic` — `967D29A9A500A3108D7331BA47A5E2053A7C2203B81A345BE990084F3F43698A`
- QEMU used an isolated `git archive` staging tree at `C:\Users\guideX\AppData\Local\Temp\guidexos-phase29x-qemu-stage-20261004\guideXOSServerV0.5_DEVELOPER_STUDIO`; protected Server files were neither staged nor overwritten.
- There was no pre-existing Phase 29X marker. This document is the phase record. Physical hardware was not used.

## Protected Phase 29W and Phase 29V baseline

- The optional document-activation host-call remains at tail offset 448; old table size 448, extended size 456. Existing offsets did not change. AMD64/ARM64 ABI layout checks and the 9/9 document-activation helper checks passed. No `gx_host_calls` or SDK header edits were made.
- Phase 29V parser proof remains 413 DIEs, `Ready`, `error=none`, `truncated=0`; line 37 maps to five addresses, primary `0x200016AC`, reverse mapping `src/main.cpp:37`. Capacity probes passed at 511 and 512 DIEs; 513 returned `limit_exceeded`; malformed DWARF was rejected.
- Phase 29W's line-42 unconditional Phase 15 lifecycle was rerun after the Phase 29X changes and passed (see regression table).

## Failure reproduction and identity model

The pre-change fresh line-37 hosted reproduction is preserved at `C:\Users\guideX\AppData\Local\Temp\guidexos-phase29x-repro\baseline-line37-editor.log`. It showed `Manager snapshot unavailable`, an editor-owned selection, and no `debug_condition_editor=OPEN` marker. The failure reproduced before production edits.

The configured, project-scoped source record is `guidexos::developer_studio::DebuggerWorkspaceBreakpoint` in `src/developer_studio_debugger_workspace.h`. Its logical key is normalized `(sourcePath, line)` within the active project workspace. It owns enabled state, action, hit policy/threshold, condition text, and log template. The workspace does not allocate a stable numeric ID for this configuration record. The editor row and Manager/controller rows are projections; the session controller assigns a logical live breakpoint ID after binding. Runtime software-breakpoint binding identity remains separate.

For the reproduced fixture, the hosted product selected `src/main.cpp:37`; DWARF was Ready and untruncated with five addresses, primary `0x200016AC`. The editor row was index `0`, editor row ID `0`; Manager ID was `0` because no Manager snapshot existed. Once bound, the live controller/logical breakpoint ID was `1`. The runtime binding ID was `1` at `0x00000000200016AC`.

`DebugBreakpointSelectionIdentity` retains origin, project/workspace/session generations, logical ID when bound, Manager/editor row IDs and index, project ID, source path, line, and column. It stores no raw UI row pointer. Editor, Manager, and controller selection helpers resolve back to the same workspace `(sourcePath, line)` record. A workspace generation change invalidates an old editor identity; removal or stale session rejects the write.

## Manager snapshot and condition editor

`g_debugUiBreakpointValid` was renamed `g_debugUiBreakpointSnapshotValid`. It describes only whether a Manager/provider snapshot is available; it is not global breakpoint validity. For the hosted `NativeAppDebugger` backend, a persistent Manager provider is absent by design, so the Manager view can legitimately say `Manager snapshot unavailable` while the editor and live controller still have the configured breakpoint.

Before the fix, selected-breakpoint condition editing required the Manager-specific snapshot and row. Now editor selection resolves the canonical workspace record directly. The condition editor requires a current project/workspace identity and a configured breakpoint; it does not require a Manager window or Manager snapshot. `debug_condition_editor=OPEN` is emitted only after the modal has rendered and the identity is revalidated.

The hosted line-37 editor route passed without opening the Manager first. Its evidence included:

```text
debug_condition_editor=OPEN breakpoint_id=1 source=src/main.cpp:37 origin=editor editor_row=0 editor_row_id=0 manager_id=0 manager_snapshot=absent
debug_condition_persist=PASS source=src/main.cpp:37
```

The controller-panel selection fallback also passed an editor/controller round trip and persisted one workspace breakpoint. The persistent Manager-row route resolves a Manager row back through project/source/line to the same workspace record in production code, but **could not be hosted-qualified**: the hosted backend does not provide persistent Manager rows. No separate Manager breakpoint was synthesized. Therefore no Manager-provider failure was observed, but direct persistent Manager-row editing remains a qualification item.

Condition text is authoritative in the workspace record. The commit path parses the expression first, validates project/workspace/session identity, updates the canonical workspace once, synchronizes available Manager/controller projections, and rolls projections back if persistence or controller update fails. Editor/controller reopen showed the same condition; the workspace retained exactly one logical breakpoint. Manager projection synchronization was not observable in the hosted backend because its provider was absent.

Stale/deleted selections are rejected by source key and workspace generation before a write. Unit coverage exercises current identity, wrong key/generation, and invalidation after edits/removal. The condition remains attached to its source configuration while build mapping/runtime binding is separately rematerialized. Runtime binding is session-local and was recreated on relaunch. A distinct hosted build-remap-with-condition-retention run was not performed.

## Conditional behavior

The expression syntax/evaluator was not expanded. The accepted Phase 15 expression was `counter == 2`, evaluated against current target-frame DWARF locals; the Phase 15 image had no GXSM metadata (`GXSM result=absent`). The watch UI continued to report authoritative GXSM watch metadata unavailable rather than fabricating values.

The five-session focused gate and 25-session stress gate each used a fresh Phase 15 fixture, line 37, and condition `counter == 2`. In every passing session the runtime trap was resolved to logical ID `1`, binding ID `1`, address `0x200016AC`; the false condition occurred twice and execution continued/rebound without a user-visible breakpoint stop. The subsequent true condition published the normal stop at the expected line. Target exit was code 0 and teardown passed.

The explicit error/recovery smoke used `unknown_value == 2`: parsing accepted the expression, evaluation surfaced `Debug: breakpoint condition error`, and execution paused with a bounded error instead of silently treating missing metadata as false. Replacing it with `counter == 2` committed and stopped on true; clearing the condition then succeeded. On the no-GXSM Phase 15 artifact, GXSM validation reported `absent`; the live variable condition still evaluated from DWARF target-frame state. Thus the condition path does not require GXSM, while the watch metadata limitation remains explicit.

The true stop retained the current source location and session stop ownership. False traps did not emit a visible breakpoint STOPPED event and followed the existing software-breakpoint restore/execute/reinsert/continue path. The persistent configured breakpoint remained armed. A new hosted session created a fresh runtime binding; the workspace condition remained configured.

## Validation results

| Gate | Result |
| --- | --- |
| Debug CTest | 33/33 PASS |
| Release CTest | 33/33 PASS |
| Workspace identity/generation and stale edit coverage | PASS |
| Phase 29V parser and 511/512/513 boundaries | PASS; see baseline above |
| Malformed-DWARF rejection | PASS |
| Server SDK document-activation ABI helpers | 9/9 PASS |
| Server ABI layout + AMD64/ARM64 cross-target probes | PASS; prior offsets preserved |
| AMD64 package build/audit | PASS, 1,117,692 bytes, SHA-256 `7605FE1BC4BDF82833032E9381D4678672641F906510290DB99B92D0E744A0DE` |
| ARM64 package build/audit | PASS, 1,277,932 bytes, SHA-256 `D4F5E831129A695A5189D33A7F37F2E53AEC49A82BC592E53F3311742B2FB85E` |
| Hosted line-37 focused sequence | 5/5 PASS; fresh fixture each time |
| Hosted conditional stress | 25/25 PASS; no retries |
| Unconditional Phase 15 stepping | PASS: Step Into, Step Over, Step Out, stack/locals refresh, Continue, exit, teardown |
| Positive Phase 29Q GXSM fixture | PASS: GXSM v2, live values, equality condition, refresh/invalidation, relaunch, teardown |
| Controller-panel/editor condition round trip | PASS; Manager snapshot remained absent |
| Direct condition editor and error recovery | PASS |
| QEMU Phase 28Q full acceptance | 10/10 fresh boots PASS with debugger start, lifecycle, cleanup, and `DEVELOPER_STUDIO_PHASE28Q_PASS`; no `invalid_project_root` |
| QEMU project-load ownership stress | Incomplete: first run stopped at 8/25 due to the transient-token monitor race; corrected run reached 22/25 then timed out before app entry; third run reached 5/25 then hit the same pre-app timeout. No run completed 25/25. |
| Physical hardware | Not used |

The full QEMU run's harness labels each individual QEMU stop `NATIVE_LOADER_REACHED`, although each corresponding serial log contains the complete Phase 28Q lifecycle pass marker; the aggregate harness exited 0 after ten boots.

The first QEMU ownership stress attempt stopped at boot 9/25. Its preserved evidence root is `C:\Users\guideX\AppData\Local\Temp\guidexos-phase28g-03a6e634caef4e41b09dfb2d3c6ecd3d`. Boots 1–8 reached project-load ready. Boot 9 had no `invalid_project_root` or complete non-current ownership tuple; it was killed mid-commit by the harness. The live monitor's original regex matched a transient partial `result=CUR...` token as an ownership failure. This was reproduced directly: the old pattern matches `result=CUR` at end-of-buffer, although complete `result=CURRENT` is valid. Only the isolated QEMU-stage copy of the monitor was changed to require whitespace after a complete result token; no repository harness or product source was changed. A new fresh 25-boot sequence is running from iteration 1 at the time of this report draft.

That second fresh sequence reached 22 project-load-ready boots and then stopped at boot 23. Evidence is preserved at `C:\Users\guideX\AppData\Local\Temp\guidexos-phase28g-b4233528824c4c57829a3f589666e436`. Boot 23 timed out at 120 seconds before Developer Studio app-entry or project-load markers; the serial tail showed the kernel main loop and repeated desktop pump traces. It had no `invalid_project_root` and no ownership mismatch. The run exited 1 because the required application/project-load markers were absent. This is a pre-app QEMU startup boundary, not a failed ownership tuple. The failed iteration was preserved and not replayed.

A third fresh sequence reached five project-load-ready boots and stopped at boot 6. Evidence is preserved at `C:\Users\guideX\AppData\Local\Temp\guidexos-phase28g-dc37c2d324a44c05809c38455833705b`. Boot 6 timed out at 120 seconds with the same kernel-main-loop/desktop-pump-only trace before Developer Studio app-entry. There was no `invalid_project_root` or ownership mismatch. After this repeated pre-app timeout, the 25/25 gate was left incomplete rather than replaying the failed iteration again.

One additional error/recovery attempt under a longer temporary source root built successfully but the hosted parser classified that artifact as `malformed_dwarf` before `debug_start` (421 DIEs, zero functions/variables; stdout `C:\Users\guideX\AppData\Local\Temp\guidexos-phase20-f3f7e51ffa024efd892a47529557f3de.out`). Re-running the same bounded error/recovery case from short root `C:\Users\guideX\AppData\Local\Temp\p29xe` passed. This is recorded as a path-sensitive hosted DWARF boundary; the evidence does not establish its root cause. The canonical 413-DIE Phase 15 parser proof remains passing.

## QEMU `invalid_project_root` status

The ten isolated Phase 28Q full-acceptance boots each reached `debug_start=PASS` and `DEVELOPER_STUDIO_PHASE28Q_PASS`. No serial contained `invalid_project_root`. This closes the previously reported Phase 29W boot-1 boundary for the staged Phase 29X package. QEMU used the copied package with AMD64 hash `7605FE1BC4BDF82833032E9381D4678672641F906510290DB99B92D0E744A0DE` and preserved the sentinel hash.

## Remaining boundary and closeout

Outcome B remains because the hosted persistent Manager provider/row route could not be exercised, condition retention through a separate hosted remap was not independently run, and QEMU ownership stress could not complete 25 consecutive valid app/project-load boots. No QEMU boot in these runs reported `invalid_project_root`; the two stress failures were pre-app timeouts. There is no observed Manager-provider failure: the hosted Manager snapshot is legitimately absent for this backend. The longer-root malformed-DWARF error/recovery attempt also remains a path-sensitive boundary despite the successful short-root recovery and canonical parser proof.

This document closes the Phase 29X technical evidence. The final task report records the resulting commit, push attempt, ending HEADs, ahead/behind counts, and worktree state.
