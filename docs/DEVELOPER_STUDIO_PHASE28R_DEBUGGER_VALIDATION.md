# Developer Studio Phase 28R — Debugger Validation and Repository Hygiene

Phase 28S follow-up: [Native Launch and Debugger Proof](DEVELOPER_STUDIO_PHASE28S_NATIVE_LAUNCH_DEBUGGER_PROOF.md). Phase 28S is Outcome B: the native launch stall and hosted stack crashes were repaired/localized, but complete debugger acceptance and hosted terminal smoke remain open.

## Result

Phase 28R is **Outcome B — debugger proof exposes a real defect**. The repository hygiene debt was repaired, the missing Mbed TLS dependency was restored from the project’s pinned historical bootstrap, and the complete hosted Server and freestanding AMD64 kernel builds succeed. The bare-metal Phase 28Q proof was started through the real Developer Studio → debugger ABI → Server NativeElf path, but no complete QEMU proof is claimed.

The final fresh QEMU attempt reached the real P28Q build and artifact-open path, then stalled before `ELF Loader`/debug-start markers. Consequently, no bare-metal register, Call Stack, Variables, Watch, Data Tip, Pause/Continue-cycle, breakpoint-coexistence, or teardown pass is claimed.

## Starting state

- Standalone: `D:\dev\guideXOS_Developer_Studio`, branch `main`, starting HEAD `5032bfa5a0aeb0dcb2dd6af5931a16b2826ba7d2`.
- Server: `D:\dev\guideXOSServerV0.5_DEVELOPER_STUDIO`, branch `v0.5_DEVELOPER_STUDIO`, starting HEAD `02c299e4ab7b8b78b2c41afadcd47c532e475511`.
- Both checkouts were inspected before modification. Neither was detached, neither had stashes, and both had only the configured `origin` remote. The initial worktrees were clean and their configured upstreams were the expected branch names.
- The standalone Phase 28O/28P/28Q source, tests, and documentation were present after the recent `.git` repair; no historical Phase 28O/28P/28Q content was missing.

## Standalone repository hygiene

The following tracked generated top-level build trees were removed from Git tracking while their local files were preserved:

`build-native/`, `build-native-gcc/`, `build-native-elf-round1/`, `build-native-elf-round1-clang/`, `build-phase28p-host/`, `build-phase28q-host/`, `build-phase4-debug/`, `build-phase4-native/`, and `build-phase5-native/`.

`.gitignore` now contains `/build-*/`, scoped to top-level Phase/native build directories. It does not ignore source-controlled directories merely because they contain `build` in their name. A fresh build in `build-phase28r-host/` was configured and built successfully, and `ctest` completed without re-dirtying the repository with generated output.

## Mbed TLS dependency

The Server checkout had no `third_party/mbedtls` tree, no active `.gitmodules` entry, and the current Makefile/build flags expected the newer Mbed TLS 4.x plus TF-PSA layout. The old 2.28.9 archive was verified but rejected as the wrong generation and was not used as a workaround.

Authoritative project history at `8bc29f59` supplied the intended bootstrap, lock information, profile, configuration overlay, and patch series. That bootstrap was run in a short staging path with a dedicated Python environment after installing its declared driver requirements. It restored:

- Mbed TLS commit `0fe989b6b514192783c469039edd325fd0989806` (`mbedtls-4.1.0`)
- TF-PSA Crypto commit `29160dd877d29658279fd683b2ae57b320ddcf09` (`v1.1.0`)
- the three project patch applications

The bootstrap reported `MBEDTLS_PROFILE_VERIFY=PASS`, `MBEDTLS_BOOTSTRAP_INSTALL=PASS`, and the Makefile probes reported layout, compile, and link readiness. The dependency remains a local ignored third-party tree as intended by the Server repository; no TLS disablement, crypto stub, arbitrary binary, or fake implementation was introduced.

## Build evidence

Hosted Server build:

```text
cmd.exe /c build.bat
Build successful: guideXOSServer.exe
```

Freestanding kernel build:

```text
C:\mingw64\bin\mingw32-make.exe ARCH=amd64
Mbed TLS/TF-PSA freestanding compile probe complete
Linking amd64 kernel
Converting to ELF64
Built: build/amd64/bin/kernel.elf
```

The changed Server translation units were compiled by the real freestanding kernel build, and the full hosted build linked the changed debugger translation units. Existing warnings were retained in the evidence; no warning indicating a debugger-state, ABI, stack, or scheduler failure was dismissed as a successful proof.

Observed SHA-256 values from the completed build artifacts:

- Developer Studio AMD64 package: `A1270377FF92BA55661CB2D7C6668FC92F49CA35792A635E393A52995D41AC2A`
- Developer Studio ARM64 package: `E91B7170B59439C941DE2295DF2188886175EC41D65F28C48B3787D1C8CE57E3`
- Server executable: `57C195AA608258EBC40738D074784D47E956F04DC6BC38CAD0A78AACC6C56676`
- AMD64 kernel ELF from the final forced freestanding link: `A2FCC8C6E6084F1FD454FE59830EBD3E40703891E026B697E999E75AE9B7D469`
- Final P28Q compiler artifact: `fnv1a64:285E8C42C5212D02` (`10403` bytes)

The validation package build produced the AMD64 and ARM64 hashes above. The final tracked package binaries were restored after the harness mutated them, so their working-tree hashes are not substituted for the validation-package hashes. A complete three-boot determinism comparison was not possible because the proof did not complete. A prior successful kernel link produced a different hash from the final forced link; the full relink carries normal PE/link metadata differences, and no cross-boot package comparison was completed. ARM64 was packaged, but no ARM64 QEMU or hardware runtime debugger validation was performed.

## QEMU methodology and observed result

The intended harness was used with fresh isolated boots, including:

```text
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\smoke-compiler-bootstrap.ps1 -Phase28QOnly -BootCount 3 -TimeoutSeconds 300
```

Additional one-boot diagnostic runs were used after each narrowly scoped integration repair. At least three fresh boots were attempted overall; valid complete proof boots: `0`.

The path was not a hosted mock. The boots reached Developer Studio application construction, project open, the real compiler, P28Q ELF validation, `BareMetalBuild: PASS`, `DEVELOPER_STUDIO_PHASE28Q_DEBUG_START_PASS`, and the real `/P28Q/build/bin/amd64/p28q.elf` open operation.

Two genuine integration defects were identified and repaired during the attempts:

1. The one-shot NativeElf entry breakpoint remained patched after its first trap. The handler now restores the original instruction byte before yielding to the execution owner and clears the one-shot installation state.
2. The AMD64 scheduler-yield assembly restored the target’s saved RAX after the dispatch call, overwriting the dispatch boolean. The dispatch result is now preserved in an unused stack slot and restored as the function result.

After those repairs, the fixture used two existing GUI text calls as cooperative safe-yield boundaries, guarded by `native_elf_scheduler_in_target()`. The final boot opened the P28Q ELF and then stalled before `ELF Loader`/debug-start output. The preserved trace is:

`C:\Users\guideX\AppData\Local\Temp\guidexos-phase28g-5aa33b62eaee4d5892350642f7e8d403\boot1.serial.log`

An earlier trace showing the first entry-breakpoint ownership failure is preserved under:

`C:\Users\guideX\AppData\Local\Temp\guidexos-phase28g-1d42a0c...`

Because execution never reached an authentic paused target in the final run, the following required bare-metal claims remain unproven: `USER_PAUSE`, authenticated stop/session/runtime generations, genuine captured IP/register context, Call Stack, live Variables, Watch, Data Tips, unmapped-location truthfulness, Continue, a second cycle, duplicate-Pause bounds, Pause-while-paused rejection, persistent breakpoint coexistence, Step ownership, normal teardown, deterministic final output, and stale-state cleanup.

## Hosted smoke

The separate hosted smoke access violation still reproduces under the clean current build:

```text
serverExitCode=-1073741819
lastInput=desktop.launch com.guidexos.developerstudio
```

The child reached native app launch, NativeElf image validation/loading, and host call dispatch before exiting; it did not reach `initial_render`. Logs:

- `D:\dev\guideXOS_Developer_Studio\logs\developer-studio-debugger-required-condition-editor-20260920-132101.log`
- `D:\dev\guideXOS_Developer_Studio\logs\developer-studio-debugger-required-condition-editor-child.log`

The exact production/test-lifetime root cause was not established. It is recorded as a separate host-only stabilization defect and is not used as evidence against the bare-metal path.

## Regression status

The standalone native build completed and the currently relevant CTest suite passed `31/31` tests. The package/model validation also passed the accumulated controller/model coverage for async run, breakpoint pause/resume, source stepping, Step Over, Step Out, Call Stack, Variables, selected-frame inspection, watches, conditional and hit-count breakpoints, logpoints, workspace persistence, Data Tips, and manual Pause/Continue at the source/model level. This does not substitute for the missing bare-metal proof.

## Limitations and next phase

Phase 28R does not claim validation debt closure. The remaining blocker is a reproducible bare-metal NativeElf launch/safe-yield integration failure after artifact open. Phase 28S+ should diagnose that handoff without changing the Phase 28Q pause semantics, ABI append-only rules, execution ownership, generation authentication, breakpoint manager ownership, or scheduler safe-yield boundary. No new user-facing debugger capability was added in Phase 28R.

Exact commands and artifacts above are retained so the next run can start from the repaired dependency/build baseline rather than recreating the Phase 28Q infrastructure investigation.

## Closing Git state

- Standalone ending phase commit before this documentation-only correction: `70d21cda22ad7093507cd4e1845313b065631cfa`.
- Server ending phase commit: `8d438eabda4d5767aab340d945530b911b8dad72`.
- Both worktrees were clean after their commits. Each local branch was one commit ahead of its configured upstream because both normal SSH pushes were rejected with `git@github.com: Permission denied (publickey)`; no force-push or history rewrite was attempted.
