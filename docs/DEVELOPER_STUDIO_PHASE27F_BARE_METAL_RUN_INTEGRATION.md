# Developer Studio Phase 27F — Bare-Metal Run Integration

Phase 27F connects Developer Studio Run Project to the real bare-metal
NativeElf runtime while preserving the existing hosted Run path.

## Scope

The supported flow is:

```text
F5 / Run Project
  -> BuildController start/poll
  -> build failure blocks Run
  -> RunRequestFromBuild (artifact path, size, SHA-256, architecture, ABI)
  -> capability-selected development_run service
  -> NativeElfRunService validation
  -> NativeElf nested launch
  -> bounded log bridge and exit-code snapshot
  -> teardown and release
```

The backend is selected by capability, not by a Studio-only target string.
The five appended bare-metal callbacks are used only when the Server exposes
the complete set; otherwise the hosted callback set remains the fallback.

## Bare-metal contract

The exact target is `guidexos.amd64.baremetal.bootstrap.native`. Preparation
requires a workspace-only Phase 27F project, matching `guidexos.project` and
`app/app.json`, amd64 ELF64 `ET_EXEC`, `gx_main`, `guidexos-c-abi-v1`, and the
fresh BuildResult artifact identity. Start revalidates the identity immediately
before launch so a replacement artifact cannot run under an old build result.

The public ABI is append-only. The new request fields begin at offsets 72,
80, and 88, with request size 96. The new snapshot fields begin at offset
448, with snapshot size 4552. Older callers may omit appended request fields;
the hosted wrapper gates those fields by `request->size` and copies only the
snapshot prefix the caller supplied.

NativeElf captures at most 16 application log lines of at most 256 bytes each.
Output is copied into the RunResult and published as Application Output on the
Run operation, separate from Build Problems and Run lifecycle records. The
runtime exposes artifact-size-changed, runtime-busy, and cancel-unsupported
diagnostics as distinct errors.

The current runtime is synchronous and allows one active bare-metal operation.
The application return value is preserved as `exitCode`; any completed
lifecycle, including exit code 42 in the proof fixture, is a successful Run.
Close after launch is intentionally reported as unsupported for this phase.

## Nested runtime safety

When Developer Studio itself is the NativeElf parent, the child run uses a
separate fixed stack below the parent stack:

```text
parent application stack   0x101F0000 - 0x10200000
child application stack    0x101E0000 - 0x101F0000
nested service stack       0x101D0000 - 0x101E0000
```

The parent image bytes, page-table permissions, and execution context are
saved before the child launch and restored after child teardown. This keeps
the Studio runtime executable and permits repeated runs without accumulating
mapped pages or stale output.

## Proof fixture and smoke coverage

The focused fixture is
`D:/dev/guideXOSServerV0.5_DEVELOPER_STUDIO/scripts/fixtures/phase27f`.
The app starts with `return 42`, is edited to a second source returning 41,
then receives an invalid `return banana` build, is restored, and is run three
additional times. The smoke asserts:

```text
phase27f_run_backend=PASS
phase27f_ide_run=PASS
phase27f_exit_code=PASS
phase27f_source_edit_run=PASS
phase27f_output_isolation=PASS
phase27f_artifact_identity=PASS
phase27f_build_failure_blocks_run=PASS
phase27f_recovery=PASS
phase27f_run_recovery=PASS
phase27f_repeat=PASS
phase27f_repeat_run=PASS
phase27f_kernel_survival=PASS
phase27f=PASS
phase27f_app_launch=PASS
ELF Loader: Phase 27F smoke PASS
```

## Verification

The focused checks completed during implementation were:

```powershell
Set-Location D:/dev/guideXOSServerV0.5_DEVELOPER_STUDIO
powershell -ExecutionPolicy Bypass -File scripts/run-native-abi-layout-test.ps1
powershell -ExecutionPolicy Bypass -File scripts/run-native-elf-runtime-host-test.ps1
powershell -ExecutionPolicy Bypass -File scripts/run-native-elf-host-test.ps1
powershell -ExecutionPolicy Bypass -File scripts/run-compiler-bootstrap-host-test.ps1

Set-Location D:/dev/guideXOS_Developer_Studio
powershell -ExecutionPolicy Bypass -File tests/run-developer-studio-validation-fast.ps1

Set-Location D:/dev/guideXOSServerV0.5_DEVELOPER_STUDIO
powershell -ExecutionPolicy Bypass -File scripts/smoke-compiler-bootstrap.ps1 -BootCount 3 -TimeoutSeconds 45 -Phase27F
```

The result was a passing ABI layout check, server runtime/validator/compiler
host checks, 27/27 Developer Studio validation tests, and three passing Phase
27F QEMU boots with all markers above. No remote push or third-repository change is
part of this phase.
