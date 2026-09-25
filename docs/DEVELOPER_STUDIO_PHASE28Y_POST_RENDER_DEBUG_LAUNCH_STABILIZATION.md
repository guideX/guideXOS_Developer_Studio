# Developer Studio Phase 28Y — Post-Render Debug Launch Stabilization

Date: 2026-09-24

## Classification

**Outcome B.** The Phase 28Y post-render transition is now observable and the
known boot-3 omission is repaired at the fresh-boot harness boundary, but the
required 10/10 complete fresh-QEMU gate did not complete. A separate
non-deterministic QEMU/Native ELF runtime stall remains after the repaired
transition. The 25-cycle stress gate was not run because its 10/10 prerequisite
was not achieved.

## Repository state

Standalone Developer Studio started on `main` at `b0d1e96a43d1a359b827bdb9ada08daeacb95227`
(`Phase 28X stabilize paused stop ownership`). It was clean and one commit
ahead of `origin/main`.

Server integration started on
`v0.5_DEVELOPER_STUDIO` at
`2d63c75873ec612820a7638d1bb6ee5d621b2bb2`
(`Phase 28X trace NativeElf stop context`). It was clean and synchronized with
its configured upstream at the start of this phase; this differs from the
expected `0 behind / 1 ahead` state recorded in the Phase 28X handoff.

Phase 28X remained intact, including
`DebugControllerStoppedContextIsCurrent`, authoritative stop ownership,
generation rejection, and exit teardown semantics. No branch switch, merge,
rebase, reset, or history rewrite was performed.

## Reproduction and evidence

The unchanged Phase 28X code was first exercised with the existing three-boot
QEMU proof. That run did not reproduce the historical post-render stall: it
reached the debugger and then exposed the already-known Continue-path stall on
boot 1. This was kept separate from the Phase 28Y startup question.

The historical Phase 28X boot-3 evidence was then compared with boot 1 and
boot 2. Boot 3 reached:

```text
DEVELOPER_STUDIO_PHASE28M_APP_DISCOVERY_PASS=PASS
DEVELOPER_STUDIO_PHASE28M_GUEST_APP_PASS=PASS
DEVELOPER_STUDIO_PHASE28M_GUEST_CLEANUP_PASS=PASS
DEVELOPER_STUDIO_PHASE28M=PASS
GUIDEXOS_DEVELOPER_STUDIO_MARKER initial_render=PASS
```

It did not reach `DEVELOPER_STUDIO_PHASE28Q_BEGIN`. The disposable boot-3 ESP
copy also did not contain `Apps/DeveloperStudio/.phase28q-diagnostic`, while
the successful boot copies did. Therefore the diagnostic launch request was
never staged; the normal application rendered and then waited for ordinary UI
input. No debugger stop/context code ran in that failure.

Evidence source: the preserved Phase 28X boot-3 serial log under
`C:\Users\guideX\AppData\Local\Temp\guidexos-phase28g-c9e49ba37cfa45a2a0a695d6c384be4a`.

## Startup-stage map

The bounded trace uses `P28Y STARTUP` markers in the Developer Studio app and
the server launch path. The app-side production sequence is:

```text
01 gx_main_entered
02 initial_sentinel_probe_complete
   EVENT initial_q_sentinel present|absent
03 final_sentinel_probe_complete
   EVENT final_q_sentinel present|absent
   EVENT final_m_sentinel present|absent
   DEVELOPER_STUDIO_PHASE28Q_BEGIN
04 window_create_requested
05 window_created
   DEVELOPER_STUDIO_PHASE28Q_APP_LAUNCH_PASS
   initial_render=PASS
06 initial_render_complete
07 event_loop_entered
08 first_event_loop_iteration
   EVENT debug_launch_mode phase28q_enabled|phase28q_disabled
   EVENT phase28q_stage_N
```

For the diagnostic path, `phase28q_stage_1` is project open, stage 2 is debug
start, and later stages cover target launch, stop/context work, Continue, the
second stop/exit, and cleanup. The stage trace is bounded to avoid continuous
logging.

The server-side map covers the corresponding launch handoff:

```text
21 prepare_entry
22 prepare_rejected
23 deployment_registered
24 start_entry
25 start_launch_requested
26 start_launch_failed
27 start_launch_returned
28 desktop_launch_entry
29 desktop_launch_resolved
30 desktop_launch_returned
31 nativeelf_process_spawn_requested
32 nativeelf_process_entry
33 nativeelf_executable_validated
34 nativeelf_image_loaded
35 nativeelf_runtime_prepared
36 nativeelf_executor_ready
37 nativeelf_process_spawn_returned
38 nativeelf_debug_service_entry
39 debugger_service_ready
40 target_runtime_registered
41 debug_launch_gate_wait_entry
42 debug_launch_gate_released
```

## Repair

The repair has two parts:

1. `src/main.cpp` now emits a deterministic, bounded stage/event trace from
   `gx_main` through initial render, event-loop entry, and the real Phase 28Q
   launch stages. It uses the existing persistent diagnostic sentinel and does
   not change debugger stop publication, paused-context ownership, or
   `DebugControllerStoppedContextIsCurrent`.
2. `scripts/smoke-compiler-bootstrap.ps1` now validates every disposable
   fresh-boot ESP copy before QEMU starts. In Phase 28Q-only mode it requires
   `Apps/DeveloperStudio/.phase28q-diagnostic` and emits
   `P28Y STARTUP boot=N launch_request_staged=present`. A missing request is
   reported as a staging failure instead of silently producing a rendered but
   non-launching desktop.

This is a persistent, level-triggered request boundary: the sentinel is the
state that causes the application to enter the diagnostic pump on its first
event-loop iteration. There is no one-shot wakeup to recover, no arbitrary
sleep, retry loop, or skipped launch stage. If the state is absent, no
diagnostic request exists; if it is present, the trace proves whether the
application reads it and which stage consumes it.

The server changes add bounded launch handoff, Native ELF pipeline, debugger
service, and launch-gate traces. They do not alter the Phase 28X authoritative
context validation.

## Validation

### Automated and hosted

- Full CTest: **31/31 passed**.
- DWARF capacity: **PASS, dies=397**.
- Hosted Native ELF debugger lifecycle: **PASS**.
- AMD64 acceptance artifact SHA-256:
  `D35DA736082425B9E3FE1033AD4A7C165E989E998AB1691586C5BEE8E2371C6A`.

### Focused fresh-boot proof

The focused Phase 28Q-only proof completed **3/3 fresh boots** with the
staged-request guard and emitted the complete startup trace through debugger
launch/pause/Continue/exit markers. A separate 300-second bounded single-boot
diagnostic also completed successfully.

### Full fresh-boot gate

The required 10-boot run used a 300-second per-boot bound. Boot 1 passed. Boot
2 failed, so the acceptance count was reset and the result is **0/10 complete
acceptance** (one complete boot was observed before the gate reset).

The failed boot was not the original missing-request defect. Its preserved
trace is:

```text
P28Y STARTUP boot=2 launch_request_staged=present
P28Y STARTUP 01 gx_main_entered
P28Y STARTUP 02 initial_sentinel_probe_complete
P28Y STARTUP EVENT initial_q_sentinel present
P28Y STARTUP 03 final_sentinel_probe_complete
P28Y STARTUP EVENT final_q_sentinel present
DEVELOPER_STUDIO_PHASE28Q_BEGIN
P28Y STARTUP 04 window_create_requested
P28Y STARTUP 05 window_created
GUIDEXOS_DEVELOPER_STUDIO_MARKER initial_render=PASS
P28Y STARTUP 06 initial_render_complete
P28Y STARTUP 07 event_loop_entered
P28Y STARTUP 08 first_event_loop_iteration
P28Y STARTUP EVENT debug_launch_mode phase28q_enabled
P28Y STARTUP EVENT phase28q_stage_1
DEVELOPER_STUDIO_PHASE28V_EVENT_PHASE28Q_PROJECT_OPEN_ENTRY
```

There is no project-open return marker and no later debugger launch marker.
The precise statement for this failure is: **the boot stopped after the
post-render diagnostic request was consumed because the project-open operation
did not return; it was not waiting for a missing launch request and it did not
enter Phase 28X stop ownership.**

Earlier 180-second attempts also showed an independent pre-`gx_main` Native
ELF entry stall at `ELF Loader: invoking gx_main with gx_app_context`; one
300-second single boot passed that point. These observations identify a
remaining non-deterministic QEMU/Native ELF runtime limitation rather than
justify a speculative debugger change.

### Stress

The 25-cycle lifecycle stress gate was **not run** because the required 10/10
fresh-boot prerequisite was not achieved. The repository does contain the
hosted `tests/smoke-developer-studio-debugger-soak.ps1` fixture, but running it
before the fresh-QEMU gate would not validate the required fresh-boot lifecycle
and would broaden the failing acceptance scope.

Physical hardware: **Not tested**.

## Remaining limitation

The Phase 28Y post-render transition is now persistent at the test boundary and
fully localized by stage traces. The remaining blocker is the fresh-QEMU
runtime variability after that transition: one run stalled before `gx_main`,
and the 10-boot run stalled inside Phase 28Q project open after
`initial_render=PASS`. Resolving that independent Native ELF/project-open
runtime issue is outside the evidence-supported Phase 28Y repair and must not
weaken Phase 28X context ownership.
