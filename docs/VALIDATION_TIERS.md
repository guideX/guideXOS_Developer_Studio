# Developer Studio validation tiers

Phase 21 promotes the deterministic hosted debugger checks into explicit
repository-native validation tiers. The scripts are PowerShell entry points so
the same commands can run on a Windows developer machine or a compatible
self-hosted Windows CI agent. This repository currently has no checked-in
GitHub Actions workflow and no declared hosted-debugger runner label; remote CI
execution is therefore not claimed by these scripts.

## Existing architecture

The native model and debugger tests are registered with CMake/CTest. The
checked-in `build.ps1` remains the direct clang/LLD Native ELF package recipe;
it stages `app/app.json` and `Apps/DeveloperStudio/bin/amd64/developerstudio.elf`
in the paired Server checkout. Hosted UI validation launches
`guideXOSServer.experimental.exe` from that checkout and sends the bounded
command stream through the Server stdin protocol.

The Developer Studio and Server checkouts must be paired explicitly with
`-ServerRoot`. The focused hosted sessions do not require QEMU; they require a
Windows host, the experimental Server executable, the paired SDK/toolchain,
and the hosted fixture. QEMU/target-image validation is outside these tiers.

## Tier 1: fast validation

Run ordinary native/PR validation with:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\run-developer-studio-validation-fast.ps1 -ServerRoot D:\dev\guideXOSServerV0.5_DEVELOPER_STUDIO
```

This configures and builds the CMake tree, runs CTest, checks the canonical
manifest resolver fields, builds the Native ELF package, and runs one
representative hosted Continue/targeted-close lifecycle. The command prints
each stage and elapsed time. `-SkipPackage` and `-SkipHosted` are available for
diagnosing a toolchain or hosted-environment limitation; either prints `NOT
RUN` and is not a passing substitute for that coverage.

## Tier 2: hosted debugger required/extended

The authoritative focused command is:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\run-developer-studio-debugger-required.ps1 -ServerRoot D:\dev\guideXOSServerV0.5_DEVELOPER_STUDIO
```

It runs these independent complete hosted sessions:

1. condition editor: valid, edit, invalid, clear, cancel, enable retention,
   modal-active targeted close;
2. hosted ConditionError and valid-condition recovery;
3. conditional false -> false -> true runtime;
4. selected frame with Locals/Arguments and Watch refresh;
5. structured Locals tree with array, pointer, and cycle disclosure;
6. frame-sensitive Watch;
7. Phase 16 FIFO/targeted input delivery;
8. representative Step Out lifecycle and targeted shutdown.

Every authoritative child must exit zero. The aggregate stops on the first
failure by default; `-ContinueAfterFailure -MaxFailures N` collects a bounded
number of failures and still returns nonzero. No timeout or skipped child is
reported as a pass.

Run the required tier three consecutive times for repeatability:

```powershell
1..3 | ForEach-Object {
  powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\run-developer-studio-debugger-required.ps1 -ServerRoot D:\dev\guideXOSServerV0.5_DEVELOPER_STUDIO
  if ($LASTEXITCODE -ne 0) { throw "required run $_ failed" }
}
```

## Tier 3: hosted soak

The bounded default soak is 20 complete iterations:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\smoke-developer-studio-debugger-soak.ps1 -ServerRoot D:\dev\guideXOSServerV0.5_DEVELOPER_STUDIO
```

The deterministic repeating pattern covers ordinary Continue, Step Into, Step
Over, Step Out followed by Continue, overlapping Step Out/re-hit, conditional
false -> false -> true, ConditionError/recovery, selected frame, structured
tree, and Watch. Every child performs launch, real breakpoint interaction,
inspection or stepping, `gui.close <windowId>`, authoritative shutdown, and
process/ownership cleanup before the next iteration. The runner reports total,
average, and slowest iteration time plus per-variant counts.

For a scheduled or manual extended run, the same runner scales without code
changes:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\smoke-developer-studio-debugger-soak.ps1 -ServerRoot D:\dev\guideXOSServerV0.5_DEVELOPER_STUDIO -Iterations 100
```

Use `-StartIteration N -Iterations 1` to reproduce the deterministic variant
at a failed iteration. The default failure policy stops at the first failure;
`-ContinueAfterFailure -MaxFailures N` is bounded collection mode.

## Failure artifacts and bounds

Passes retain only concise suite/iteration output. A failure writes a named
artifact under `logs/` such as
`developer-studio-debugger-soak-iteration-07.log` or
`developer-studio-debugger-required-structured-tree-<run>.log`. Each artifact
contains the failed suite/iteration/variant, child exit code, the last 96
lifecycle/state lines, and the last 80 runner output lines. The final file is
bounded to 64 KiB. Existing child traces also cap the Server debug-log query
at 64 entries. Soak artifacts are rotated to the newest 32 matching files.

The trace includes the authoritative session, target, breakpoint/Step Out
ownership, selected-frame/Watch markers, input/modal/owner evidence when the
Server emits it, and shutdown stage. The child harnesses own the assertions for
session, target, step operation, breakpoint ownership, inspection generation,
input capture, window ownership, and monotonic shutdown completion; the soak
runner treats any child assertion failure as an iteration failure and cleans up
through the child `finally` path.

The required and soak entry points use a 240-second semantic-marker wait on
this hosted compositor path and pass a 360-second bounded cleanup/process wait
to each child. The complete wall-clock time can exceed 360 seconds because the
children also perform deliberate UI pacing and multiple semantic waits before
that final cleanup guard. These are bounded waits, not unbounded process
polls. Timeout diagnostics include the expected marker, observed count, last
input, and recent lifecycle state.

The required Step Out suite explicitly passes breakpoint line 9 because the
checked-in Phase 8 fixture maps its Step Out proof there; the other required
hosted fixtures use line 37.

The maximum priority input queue depth is intentionally reported as
`NOT OBSERVABLE`: Phase 21 does not add intrusive production telemetry. FIFO
delivery and the bounded priority lane remain covered by the Phase 16 focused
regression and the Watch/condition interactions.

## Legacy smoke disposition

`tests/smoke-developer-studio.ps1` and its `-LegacyAggregate` paths remain
diagnostic/historical. The coordinate-heavy aggregate is excluded from the
required tier. The focused Phase 15/20 sessions are authoritative and preserve
the same debugger feature assertions with semantic markers and targeted
ownership/shutdown.

## Production behavior and CI use

All lifecycle, input, soak, and trace output is emitted by test scripts or by
the existing diagnostic-mode hosted paths. Normal Developer Studio operation
does not enable soak metadata or per-frame lifecycle logging. A compatible
Windows CI agent can call Tier 1 on PR/push, Tier 2 manually, and Tier 3 on a
nightly schedule using the commands above. Until such a runner is declared in
repository infrastructure, the workflow is CI-ready but remains local/manual;
local success must not be described as remote CI execution.
