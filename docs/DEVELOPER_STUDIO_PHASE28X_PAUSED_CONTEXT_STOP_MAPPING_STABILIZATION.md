# guideXOS Developer Studio — Phase 28X

## Paused Context / Stop Mapping Stabilization

Date: 2026-09-23

Final classification: **Outcome B**.

The paused-context/stop-ownership defect was repaired and the targeted hosted
and one-boot Native ELF evidence passes. The required fresh-QEMU gate stopped
at boot 3 of the first 10-boot attempt because the packaged application stalled
after `initial_render=PASS`, before project open or debugger launch. That is an
independent startup/application blocker, not another paused-context or stop
mapping failure. The acceptance result is therefore 2/10 consecutive complete
boots, not 10/10. The 25-cycle stress gate was not run because its prerequisite
was not met and no executable 25-cycle debugger stress sequence is present in
the repositories.

## Starting state

Both repositories were clean on their expected branches before Phase 28X:

| Repository | Branch | Starting HEAD |
| --- | --- | --- |
| `D:\dev\guideXOS_Developer_Studio` | `main` | `3f0c511` — Document hosted Phase 28W control |
| `D:\dev\guideXOSServerV0.5_DEVELOPER_STUDIO` | `v0.5_DEVELOPER_STUDIO` | `885b87c7` — Phase 28W resolve Continue after NativeElf exit |

No branch switch, merge, rebase, reset, history rewrite, or discard of
pre-existing work was performed.

## Reproduction and evidence

The Phase 28W failing trace showed a second paused stop with the same visible
instruction and frame addresses, followed by:

```text
DEBUG_POST_REFRESH_STOP_MAPPING_UNRESOLVED
DEBUG_POST_REFRESH_CALL_STACK_PARTIAL
debug_call_stack=PARTIAL
debug_variables=PARTIAL
PHASE28Q_CAN_CONTINUE_FALSE
PHASE28Q_CONTINUE_REJECT_CONTEXT
PHASE28Q_FAIL_REASON_SECOND_CONTINUE
```

The earlier trace is preserved at:

`C:\Users\guideX\AppData\Local\Temp\guidexos-phase28g-4160120f5e014a5fb90b50065bd2430c\boot1.serial.log`

The successful repaired one-boot trace shows the backend publishing a complete
context for both stops, including `valid=1`, `arch=1`, `native=1`, `thread=1`,
and non-zero `rip`, `rsp`, and `rbp`, followed by
`PHASE28Q_CAN_CONTINUE_TRUE` and the second Continue pass:

`C:\Users\guideX\AppData\Local\Temp\guidexos-phase28g-edac409b1a694cfe8ec72497bccad8ff\boot1.serial.log`

The final 10-boot attempt used fresh QEMU instances and stopped on boot 3 as
required by the acceptance rule. Boot 1 and boot 2 completed the full Phase
28Q sequence. Boot 3 reached:

```text
DEVELOPER_STUDIO_PHASE28M_APP_DISCOVERY_PASS=PASS
... main_window_creation=PASS
... initial_render=PASS
```

It never reached `DEVELOPER_STUDIO_PHASE28Q_BEGIN`, project open, debug start,
or a stopped context. The harness then timed out and preserved the evidence at:

`C:\Users\guideX\AppData\Local\Temp\guidexos-phase28g-c9e49ba37cfa45a2a0a695d6c384be4a\boot3.serial.log`

This distinguishes the remaining failure from the Phase 28X defect: there is
no stop to map, no context to validate, and no Continue command in the failed
boot.

## Root cause

The controller had payload-valid register-context checks, but no single
authoritative ownership predicate tying the stopped context to the exact
controller tuple. Consequently, a context could be individually valid while
being stale, partially published, or owned by a different session/runtime,
process, thread, or stop generation. The existing paused-state consumers then
made independent freshness decisions, allowing the visible stop, context,
stack/source mapping, and Continue path to disagree.

This was a stop-publication and composite-generation validation defect. It was
separate from the Phase 28W EXITED convergence repair: the Phase 28W lifecycle
behavior remains intact, and this change does not revive or wait on an exited
target.

## Ownership model and repair

Phase 28X establishes one reusable predicate:

`DebugControllerStoppedContextIsCurrent(controller)`

For a usable paused context it requires payload validity and exact equality of:

`session generation + process identity + Native ELF runtime identity + current thread + stop generation`

Paused breakpoint and user-pause snapshots are now rejected before controller
publication unless the snapshot itself has the authoritative current tuple,
known architecture, and non-zero register frame (`rip`, `rsp`, `rbp`). The
Native ELF zero-process identity remains an intentional authenticated form; a
non-zero hosted process identity is still checked when present.

The same predicate now gates Continue, CanContinue, all step commands, source
stop resolution, call-stack freshness and frame selection, variables, watches,
breakpoint-condition evaluation, and other paused-state reads. Invalid or late
state fails deterministically with `StaleStopContext`. Publication emits the
bounded trace events `DEBUGGER_STOP_CONTEXT_REJECTED` and
`DEBUGGER_STOP_CONTEXT_PUBLISHED`.

The server's existing `DEBUG_POLL_RESULT` trace was extended with the
authoritative context fields needed to reconstruct each stop without
instruction-level logging: validity, architecture, runtime, thread, `rip`,
`rsp`, and `rbp`.

## Regression coverage

The existing debugger tests were strengthened through production paths to
cover:

- incomplete paused context rejection;
- stale stop-generation rejection;
- current context acceptance;
- stale thread, runtime, and session ownership rejection;
- Continue rejection for stale context;
- the existing hosted source-mapping mismatch with a valid current stop.

The CTest suite remains **31/31 passed**. The total did not increase because
the coverage was added to existing test cases rather than as a new CTest target.

## Validation

| Check | Result |
| --- | --- |
| Full CTest | **31/31 passed** |
| DWARF capacity | **PASS, dies=397** |
| Hosted Native ELF debugger control | **PASS**; launch, breakpoint pause, source navigation, call stack, locals, Continue, target identity, close, teardown, and clean server exit |
| AMD64 packaged artifact | SHA-256 `3AD8C807E1FCEFFD2370ACAB6A12296EF3A2F0C58458D58C7949DF70F5A2C031` |
| Fresh QEMU Phase 28Q | **2/10 complete**; boot 3 failed independently before debugger launch, boots 4–10 were not attempted |
| 25-cycle debugger stress | **Not run**; 10/10 prerequisite was not met and no executable 25-cycle sequence was found |
| Physical hardware | **Not tested** |

The hosted control command was:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\smoke-developer-studio-debugger.ps1 `
  -ServerRoot 'D:\dev\guideXOSServerV0.5_DEVELOPER_STUDIO' `
  -ContinueBreakpoint -TraceDirectory '.\logs\phase28x-hosted' -TraceRunIndex 201
```

The fresh-boot command was:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\smoke-compiler-bootstrap.ps1 `
  -Phase28QOnly -BootCount 10 -TimeoutSeconds 180
```

## Git and remaining blocker

The Phase 28X changes are committed locally in both repositories. Pushes were
attempted normally after commit; the final commit IDs, push results, and
ahead/behind counts are recorded in the completion report accompanying this
document.

The remaining blocker is the pre-debugger fresh-boot startup stall documented
above. It occurs after the packaged Developer Studio reaches initial render and
before the Phase 28Q workflow begins. It is outside paused-context ownership
and stop mapping, so it remains deferred to a startup/application
stabilization phase rather than being hidden by retrying until ten successes.
