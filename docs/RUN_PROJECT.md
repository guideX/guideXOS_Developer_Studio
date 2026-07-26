# Run Project vertical slice

Run Project is a hosted-development-only path for the supported
`native-gui-application` targeting `guidexos.amd64.hosted.native`.

```text
F5 / Run Project
  -> active-project and dirty-document gate
  -> Build Project
  -> artifact and SHA-256 validation
  -> development_run_prepare
  -> temporary in-memory AppRegistry entry
  -> DesktopService + Native ELF launch pipeline
  -> process/window polling
  -> close request or application exit
  -> owned-window cleanup, unregister, release
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
Launching uses the normal AppRegistry/AppLaunchResolver/DesktopService path and
the existing NativeElfImageLoader/NativeAppRuntime/NativeElfExecutor path.

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
record and keeps bounded prior history. Native application output capture is
deferred because the current ABI has no safe development logging function.

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
```

The Studio controller test is `developer_studio_run_test`; the Server ABI test
is `tests/native_abi_layout_test.cpp`. Build with:

```powershell
powershell -ExecutionPolicy Bypass -File .\build.ps1 -ServerRoot <server>
cmd /c .\build-native-experimental.bat
```

The existing hosted smoke proves the real App Model launch and clean Studio
teardown. Interactive F5/close-driving requires a compositor input harness;
report that separately from automated controller, ABI, and package evidence.
