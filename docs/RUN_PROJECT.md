# Run Project vertical slice

Run Project supports the hosted development target and the bare-metal
bootstrap target for the supported `native-gui-application` project shape.
The Server capability table selects the backend; the Studio controller and
build-before-run contract are shared by both paths.

```text
F5 / Run Project
  -> active-project and dirty-document gate
  -> Build Project (selected capability backend)
  -> artifact and SHA-256 validation
  -> development_run_prepare
  -> hosted: temporary in-memory AppRegistry entry
  -> hosted: DesktopService + Native ELF launch pipeline
  -> bare: NativeElfRunService + nested NativeElf launch
  -> bounded application-output bridge
  -> process/runtime polling
  -> close request or application exit
  -> owned-window/runtime cleanup, unregister or release
```

Build-before-run is unconditional. The Studio controller never trusts a
previous build result, project metadata, manifest, or artifact path without
the current build and Server revalidation.

## Ownership and safety

The append-only ABI uses a versioned request/snapshot pair. A deployment handle
contains a slot and generation. Every operation checks the Developer Studio
owner runtime and generation, so stale handles and another native runtime
cannot poll, close, or release a deployment.

Preparation requires the exact generated project shape: bounded
`guidexos.project`, `app/app.json`, NativeElf/amd64/gx_main/
`guidexos-c-abi-v1`/`native-elf`, the `log`/`window`/`draw` permissions, no
file associations, and no desktop registry hints. The artifact must be inside
the project root, non-symlink, below the size cap, hash-equal to the build
snapshot, and valid ELF64 AMD64 `ET_EXEC` containing `gx_main`.

The temporary App Model record is held only in the Server process. It is not
written to package storage, recent programs, pinned items, or desktop state.
Installed IDs are rejected and privileged manifest claims are rejected.
Hosted launching uses the normal AppRegistry/AppLaunchResolver/DesktopService
path and the existing NativeElfImageLoader/NativeAppRuntime/NativeElfExecutor
path. Bare-metal launching targets the exact capability
`guidexos.amd64.baremetal.bootstrap.native`, validates the freshly built
artifact identity again at Start, and executes it through the NativeElf nested
runtime path with a separate fixed application stack.

## States and close behavior

The Studio controller exposes `Idle`, `Validating`, `Prepared`, `Launching`,
`Running`, `Exited`, `CleaningUp`, `Completed`, and `Failed`. A terminal state
is released only after the Server reports `cleanupComplete`.

`Request Project Close` and the close modal publish close events only for
windows owned by the active temporary deployment. They do not terminate an
arbitrary process. When the application exits, the Server unregisters the
temporary App Model entry, and the Studio releases the generation-bound handle.

## Diagnostics and tests

Run lifecycle messages use the same Output Service as Build, but retain a
separate Run operation and channel. Build-before-Run remains a separate Build
operation; Build compiler Problems are not copied into Run and ordinary Run
lifecycle messages are not Problems. Run publishes one terminal structured
record and keeps bounded prior history. Bare-metal NativeElf `log` output is
bridged as bounded Application/Standard Output lines (16 lines, 256 bytes per
line), with an explicit truncation flag; hosted application output remains on
the hosted runtime path.

Bare-metal Run is intentionally synchronous in this phase: one active runtime
operation is allowed, and close/cancel after launch reports the explicit
unsupported error. A nonzero application return value is still a successful
Run when the lifecycle reaches `Completed`; the exit code is retained in the
terminal result.

Stable markers are:

```text
run_request=PASS
run_precondition=PASS|FAIL
run_build_required=TRUE
run_artifact_validation=PASS|FAIL
run_deployment_prepare=PASS|FAIL
run_launch=PASS|FAIL
run_application_state=RUNNING|EXITED
run_close=REQUESTED|FAIL|CANCEL
run_cleanup=START|PASS
run_complete=SUCCEEDED|FAILED reason=<bounded-code>
phase27f=PASS
```

The Studio controller test is `developer_studio_run_test`; the Server ABI test
is `tests/native_abi_layout_test.cpp`. Build with:

```powershell
powershell -ExecutionPolicy Bypass -File .\build.ps1 -ServerRoot <server>
cmd /c .\build-native-experimental.bat
```

The existing hosted smoke proves the real App Model launch and clean Studio
teardown. Interactive F5/close-driving requires a compositor input harness;
report that separately from automated controller, ABI, package, and bare-metal
QEMU evidence. Phase 27F details and exact commands are in
`DEVELOPER_STUDIO_PHASE27F_BARE_METAL_RUN_INTEGRATION.md`.
