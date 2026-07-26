# Build Project contract

This phase implements one bounded vertical slice: Build Project for a validated `native-gui-application` targeting `guidexos.amd64.hosted.native` on the hosted Windows AMD64 runtime.

## Request and resolution

The UI derives a runtime-neutral request from the active version 1 project. The request is fixed to:

- build system: `guidexos-native-build-script-v1`;
- script: `build.ps1` at the project root;
- configuration: `Debug`; and
- artifact: `build/bin/amd64/<outputName>.elf`.

No project field supplies an executable, argument list, working directory, or arbitrary command. The Server accepts only the fixed project kind, target, build system, script, configuration, and artifact shape. It also requires the generated recipe marker, regular non-symlink metadata/script files, root containment, and no symlink components in the project path or artifact path.

The SDK resolves from `GUIDEXOS_SDK_ROOT\include` when configured, otherwise the hosted Server checkout's `sdk\include`. The toolchain resolves from `GUIDEXOS_TOOLCHAIN_ROOT`, then the fixed local LLVM locations. The service requires `clang++.exe` and `ld.lld.exe`; PowerShell is resolved from the Windows system directory.

## State and dirty behavior

The controller states are `Idle`, `Validating`, `Preparing`, `Running`, `ValidatingArtifact`, `Succeeded`, `Failed`, and `Cancelled`. The UI starts a build only for an active project. Dirty documents under the project root produce a Save All / Cancel decision; documents outside that root do not block the build. Saving is completed before the host request is sent.

The request, polling, and release calls are appended to the Native Host Call Table. The Server runs the process on a worker thread, captures stdout and stderr independently without blocking the compositor, and returns bounded snapshots. Only the owning runtime can release a completed handle. A second build is rejected while one is active. Closing the project, workspace, or application is blocked while a build is active; there is no implicit cancellation command in this slice.

## Artifact and diagnostics contract

The Server removes only the exact expected artifact before starting, so a stale artifact cannot satisfy a failed build. On success it checks:

- regular non-symlink file, nonzero and at most 64 MiB;
- ELF64, little-endian, static `ET_EXEC`, AMD64 machine type; and
- a `gx_main` symbol in the ELF symbol tables.

The snapshot includes exit code, elapsed time, warning/error counts, bounded output, artifact size/path/SHA-256/architecture, and a failure code. Output is capped at 64 KiB per stream and 32 lines of 255 bytes per retained snapshot line. The process tree is placed in a kill-on-close Windows Job Object and has a five-minute timeout.

Developer Studio forwards each snapshot line once into the shared bounded
Output Service. Stdout and stderr retain their origin where the hosted Server
provides it. Strictly recognized compiler/linker lines become structured
Problems; all other lines remain Build output. A new Build removes only prior
BuildDiagnostic records for the current project, and one terminal structured
record reports success or failure with duration, counts, artifact metadata, and
the bounded failure code. See [OUTPUT_AND_DIAGNOSTICS.md](OUTPUT_AND_DIAGNOSTICS.md).

Stable UI markers include `build_request`, `build_precondition`, `build_start`, `build_process_exit`, `build_warning_count`, `build_error_count`, `build_artifact_validation`, `build_complete`, `build_timeout`, and `build_close`. Paths and source contents are not emitted in markers.

## Supported outcome matrix

Successful builds publish `Succeeded`, the process exit code, diagnostics, and validated artifact metadata. A compiler/linker failure publishes `Failed` with captured output and a nonzero process exit. Missing SDK/toolchain/script, invalid project metadata, timeout, missing/invalid/wrong-architecture artifact, missing `gx_main`, output truncation, save failure, and user cancellation have distinct bounded error paths. Repeated successful builds replace only the exact expected artifact and are revalidated; a failed build cannot reuse a prior artifact.

Run, Debug, attach, target switching, multiple configurations, cancellation UI, remote builds, non-native project kinds, and production/bare-metal execution remain outside this slice.
