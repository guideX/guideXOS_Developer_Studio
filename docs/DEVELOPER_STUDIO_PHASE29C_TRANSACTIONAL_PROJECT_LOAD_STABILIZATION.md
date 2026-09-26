# guideXOS Developer Studio — Phase 29C Transactional Project-Load Stabilization

## Scope and baseline

Phase 29C hardens project loading in the standalone Developer Studio model and
adds bare-metal diagnostics and a focused QEMU gate in the server repository.

The audited starting points were:

- standalone `main`: `54c5ea4afd630e8446dbe94e5a1e581b7b513fe3`
- server `v0.5_DEVELOPER_STUDIO`: `5920b2885051fcecebd2330f4830f9de8d572729`

The preserved Phase 29B evidence at
`C:\Users\guideX\AppData\Local\Temp\guidexos-phase28g-f538288a3ca34290b459ce06208fdbca`
contains the original three-boot failure. Boot 3 reached project
`load_started` and then failed before `loaded`, candidate allocation, refresh,
commit, or `ready`. The old path did not expose the inner load error. There is
no project-load worker, task queue, or polling loop in this architecture, so
the evidence does not indicate a lost wakeup or worker deadlock.

Phase 29B's debugger start, `RUNNING`, pause/stop, and Phase 29A mapping
boundaries remain unchanged. Phase 29C keeps the dependency explicit:
`committed project → ready → debugger launch`.

## Transaction protocol

Project loading is synchronous and follows this bounded state machine:

`idle → load_started → loaded → candidate_allocated → refresh_started → validated → committing → active → ready`

Any load failure emits `failed` with a `ProjectErrorCode`, retires the
candidate, and leaves the previously active model untouched. `ready` is sent
only after the authoritative model has been replaced and its generation has
been published.

Each accepted request carries:

- a monotonic request ID;
- a candidate ID owned by the controller/load transaction;
- a candidate project generation;
- a refresh generation; and
- the active generation observed at each notification.

The candidate model and refresh listing are isolated from the active model.
The active model is changed only at the commit point. Candidate refresh errors
are kept out of the active model's error state. The transaction is checked for
the same controller and request ID before refresh and commit, which prevents a
stale result from publishing.

The implementation uses one statically bounded transaction slot because the
current runtime is synchronous. The slot is owner-bound, request-bound,
monotonic-ID tagged, and explicitly retired on every success and failure path;
it is not an asynchronous queue or an unowned global candidate.

## Re-entrancy and failure semantics

An open request received while a load is active is rejected with
`ProjectErrorCode::LoadInProgress`. The duplicate request does not allocate a
second candidate, increment the active generation, or emit a nested load
sequence. Workspace mutators that could otherwise observe half-published state
are rejected while the transaction is active.

The observer now receives a value event containing state, request ID, active
generation, candidate ID, candidate generation, refresh generation, error, and
path. This makes failure stage and ownership visible without exposing a
mutable candidate pointer.

The project parser also no longer stores its writable `Project` in function
static state. Parse state is invocation-owned, so a rejected or retried parse
cannot retain fields for the next project-load transaction.

No project-load lock is acquired and no observer waits for the loader. Observer
callbacks are synchronous and value-based, so there is no project-load lock
inversion or lock-held callback path to deadlock.

## Diagnostics

Bare-metal trace events use these markers:

- `DEVELOPER_STUDIO_PHASE29C_PROJECT_LOAD_REQUEST_ACCEPTED`
- `DEVELOPER_STUDIO_PHASE29C_PROJECT_LOAD_FILES_LOADED`
- `DEVELOPER_STUDIO_PHASE29C_PROJECT_LOAD_CANDIDATE_ALLOCATED`
- `DEVELOPER_STUDIO_PHASE29C_PROJECT_LOAD_REFRESH_STARTED`
- `DEVELOPER_STUDIO_PHASE29C_PROJECT_LOAD_VALIDATED`
- `DEVELOPER_STUDIO_PHASE29C_PROJECT_LOAD_COMMIT_STARTED`
- `DEVELOPER_STUDIO_PHASE29C_PROJECT_LOAD_ACTIVE_PUBLISHED`
- `DEVELOPER_STUDIO_PHASE29C_PROJECT_LOAD_READY`
- `DEVELOPER_STUDIO_PHASE29C_PROJECT_LOAD_FAILED`

Every event includes `worker=sync`, `state`, `request`, `candidate`,
`refresh`, `active_generation`, `candidate_generation`, `error`, and `path`.
The server host-log validator was extended to accept a bounded writable pointer
inside the loaded application image for these immediately-copied diagnostic
strings. Arbitrary kernel-memory logging remains rejected.

## Resource and boundedness audit

No worker, queue, semaphore, or wait condition was added. The load path uses
the existing fixed-capacity model, document, source, project-file, manifest,
and refresh-list limits. The new transaction slot and generation IDs have
constant storage. Candidate state is reset and retired on all exits, including
unsaved-change rejection, project-file failure, refresh failure, stale-request
failure, and successful commit.

The focused image audit stages a unique ESP per boot and records the kernel,
Developer Studio package, configuration, sentinel, isolated P28Q fixture, and
fixture-presence hashes in the boot image line. The captured Phase 29C runs
showed the expected project files and configuration; the observed failures
were not missing-fixture or stale-image failures.

## Regression coverage

Hosted tests cover:

- successful commit and ready ordering;
- duplicate/re-entrant open rejection with `load_in_progress`;
- candidate invisibility before commit;
- malformed and missing-file failures;
- rollback preservation of the old root, generation, document, and error;
- parser rejection followed by a clean valid parse; and
- existing project, workspace, symbol, debugger, and build behavior.

The standalone CTest suite remains 31/31 passing. The hosted build script also
passes its model/project/debug validation, including the DWARF locals/arguments
test.

## Bare-metal validation

Both package targets build with native debug information enabled:

- AMD64 Developer Studio ELF SHA-256:
  `DF7669059F98497D454A0FCF18D29EFDB05EDF3A7E63FBAAC84BFC06E5A0CD8F`
- ARM64 Developer Studio ELF SHA-256:
  `C77EEC8C19B11E1756268084E6E0AB0CB0351B5F816254C18546EC8248ACEFD3`

The focused `-Phase29COnly` gate stages the isolated P28Q fixture and checks
the full Phase 29C sequence through `project_ready`. It records evidence under
`%TEMP%\guidexos-phase28g-*`. In the post-parser-repair five-boot attempt,
boots 1 and 2 reached `ready`; boot 3 stalled before the project-open request
in the existing earlier runtime/IDE startup path, so the run is recorded as
incomplete rather than a false 5/5 pass. A prior pre-parser-repair attempt also
captured a reproducible late-load failure: `unknown_field` on the valid P28Q
project during boot 5. The new `PROJECT_LOAD_FAILED` marker made that exact
stage observable; the parser-state repair addresses that class of retained
state.

Full Phase 28Q continuation and 25-cycle stress gates were not promoted to
passing evidence: the existing continuation path still misses later
`CONTINUE_SECOND_PASS`/final-result markers, and no physical hardware run was
available. No hardware-specific claim is made.

The 10-boot acceptance gate was therefore not run, and the 25-cycle stress
gate was not run, because the required 5/5 focused gate was not green.

## Change ownership

The standalone implementation owns the transactional controller, model/parser
state, observer event, error code, tests, and this document. The server
repository owns the host-log pointer-range validation, package regeneration,
and focused Phase29C QEMU harness. The two repositories remain independently
committable and independently pushable.
