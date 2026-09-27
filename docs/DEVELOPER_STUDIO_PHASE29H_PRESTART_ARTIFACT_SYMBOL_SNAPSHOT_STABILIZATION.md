# Phase 29H — Pre-Start Artifact & Symbol Snapshot Stabilization

Date: 2026-09-27

## Outcome

**Outcome B.** The artifact identity mismatch contract, bounded read/ELF validation, parser diagnostics, and production mapper regression coverage were materially improved. The focused QEMU gate stopped on its first attempted fresh boot because the guest reported the required Phase 28Q diagnostic fixture absent. Hosted validation reached project open but stopped before source-breakpoint readiness. The prescribed full 10-boot and 25-cycle tiers were not run because the focused 5/5 gate did not pass.

The central invariant is now represented by an explicit logical identity carried from the completed build target into symbol loading and breakpoint mapping: project ID, project generation, build operation ID, target profile/name, architecture, normalized project-relative artifact path, completed artifact size, and SHA-256. The mapper has its own snapshot generation. Diagnostic reads revalidate exact bytes at the pre-start boundaries. This does not make the filesystem immutable: the server compiler writes the final ELF directly to its final path, closes and reopens it, and validates a full readback before declaring success. Later in-place mutation is detectable by the added boundary checks, but no filesystem lock or atomic final-path publication was found.

## Starting state and preserved files

| Repository | Branch | Starting HEAD |
| --- | --- | --- |
| Standalone | `main` | `62a74c89d7e761cf9338ac35afc73b5b2c281eb1` |
| Server integration | `v0.5_DEVELOPER_STUDIO` | `e0cfc71bfe7b231f85d80273f6a72223750147ef` |

The original standalone untracked fixture pairs were preserved with their initial contents and hashes:

- `tests/fixtures/debugger-phase3b/guidexos.debugger.json` and `.bak`: SHA-256 `6A53C3A2E9B5F5CEA837F278FC7CBE0D4BD58542058AB7AAFAB7DA5BBEC972A5` each.
- `tests/fixtures/debugger-phase15/guidexos.debugger.json` and `.bak`: SHA-256 `9DEE80AC5715601268BAB67FC9595EAC12AC1B285EF336B064C3D917A75C0E82` each.

The server sentinel `ESP/Apps/DeveloperStudio/.phase28q-diagnostic` was preserved: 17 bytes, SHA-256 `967D29A9A500A3108D7331BA47A5E2053A7C2203B81A345BE990084F3F43698A`, contents `guideXOS-phase28q`.

A temporary in-repository fixture copy used for hosted validation remains at `tests/fixtures/debugger-phase15-h29-0927/`. The execution layer rejected removal of that exact workspace path, including individual-file cleanup after the resolved path had been checked. It is not part of the Phase 29H commit. The original fixture pairs and server sentinel remain unchanged.

## Phase 29G baseline and reported failures

Phase 29G repaired the post-`RUNNING` client return stall in bounded `NativeElf::host_log`/COM1 echo. This phase did not change that logging or return behavior.

The prior `artifact_changed` run was reported at breakpoint mapping after two successful boots, but the saved evidence available for this work did not contain the validator's expected/observed field values. Static audit found a concrete identity inconsistency: the mapper stored an unnormalized executable path while later checks compared the current target after path normalization. Thus equivalent spellings could compare unequal. Phase 29H fixes that contract by applying the same normalization at mapper load and comparison and returns a field-specific mismatch. **The exact historical field/value was not available, and `artifact_changed` was not reproduced in the focused acceptance attempt.** It would be inaccurate to report a specific old runtime value as reproduced.

The previous `malformed_dwarf` report likewise lacked the artifact hash, exact read count, parser offset, or failure stage. It was not reproduced in this work. The focused attempt failed before the guest entered the diagnostic path and therefore before symbol parsing. Parser/ELF failure stage, offset, and reason are now retained by the production mapper for a future failure. No exact historical malformed-DWARF stage/reason can be recovered from the supplied evidence.

## Artifact and snapshot contract

`BuildTargetFromBuild` captures the completed build result identity together with the project model generation and `OutputService` build operation ID. `DebugDwarfArtifactIdentity` now carries the build operation ID as well as project ID/generation, target profile, architecture, normalized artifact path, size, and SHA-256. The target name/profile remains part of target identity. Build operation ID, project model generation, and mapper generation are separate namespaces; equality of their numeric values is not required. A distinct artifact publication/build generation is not provided by the existing build result and was not invented.

The mapper generation is an independent monotonic snapshot counter. `DebugControllerMapBreakpoints` requires a ready mapper, a matching artifact identity, and current project/source mapping context before it maps. A stale identity marks the breakpoint stale and returns `ArtifactChanged`; it does not proceed with addresses from a different snapshot. Diagnostics additionally reopen, stat, exact-read, hash, and restat the captured path at symbol-load, mapping, and debug-start handoff boundaries. The bounded read buffer is 512 KiB. A size mismatch, partial read, or hash mismatch fails explicitly; the stat size is not substituted for the completed build size.

Identity mismatch reporting names the differing field (mapper/context availability, project ID, target profile, architecture, normalized path, size, SHA-256, project generation, or build operation). SHA-256 comparison is case-insensitive. Path normalization converts separators, resolves dot segments, makes project-relative paths comparable, and rejects paths escaping the project root.

## Build publication and open-file audit

In the server compiler driver, the final project ELF is written synchronously to its final VFS path. The writer requires the exact output size, closes the write, reopens read-only, reads back the complete file, validates the ELF, and checks the compiler driver's FNV digest before its success summary is published. The higher-level compiler build service then stats and exact-reads the final artifact, validates it with `NativeElf`, computes SHA-256, and only then publishes `SUCCEEDED`, size, path, architecture, and hash to `BuildResult`. The successful call returns after this validation. No temporary-file/atomic-rename step exists for the final project ELF. Compiler object publication uses staging/rename when creating a missing object; it is distinct from final ELF publication.

The native run service validates exact request identity before start: path, size, SHA-256, and ELF identity are checked against the current file, and the temporary registration is re-resolved and compared with the request. It starts only after those checks. The audit found no open write handle retained after successful final-artifact publication. There is no independent post-build strip or mutation step in this path. Package ELFs are a separate deliverable and are not the debug artifact consumed by the P28Q scenario.

## Debug artifact vs runtime/package artifacts

The standalone production mapper has both DWARF-section parsing and the project bootstrap `GXSM` source-map trailer path. The exercised Phase 28Q `build/bin/amd64/p28q.elf` is one ET_EXEC artifact used for the QEMU scenario and parsed by Developer Studio's GXSM trailer path; it is not a separate stripped-runtime/debug-symbol pair. Earlier exploratory diagnostic runs observed the trailer source association `src/helper.cpp` under source root `src`.

The separately packaged Developer Studio AMD64 and ARM64 ELFs are intentionally sectionless ET_EXEC package/runtime binaries. They are audited as packages, not presented as the Phase 28Q debug artifact, and do not provide DWARF debug sections. The production mapper now validates ELF64, little-endian, version 1, ET_EXEC, AMD64, bounded program/section tables, PT_LOAD ranges, and available section ranges before entering DWARF parsing; valid sectionless ELF is accepted for the GXSM flow.

## Exact reads, ELF validation, and parser determinism

Symbol loading uses the completed artifact size as the requested length and checks the supplied byte count against it before parsing. Injected partial input fails with `ArtifactChanged` and stage `artifact_size`. A truncated ELF fails before DWARF with stage `elf_header`; a wrong machine fails with `UnsupportedArchitecture` and stage `elf_machine`. Parser diagnostics retain a bounded failure stage, byte offset, and reason for ELF, GXSM, line, abbreviation, DIE, reference, and index failures. The mapper reset clears all added ELF and failure-diagnostic state.

Production-path regression tests exercise the real mapper with deterministic fixtures, including a valid A → B → A load sequence. The second A matches the first A's identity, rows, sources, DIE count, sequences, source association, and mapped line addresses. Stale artifact mapping is rejected with exact `ArtifactChanged` and clears prior mapped addresses. Other tests cover normalized path equivalence, field-specific mismatch reporting, injected partial read, truncated ELF, wrong architecture, and malformed LEB stage/offset. The existing DWARF capacity test remains bounded and unchanged at **PASS, dies=397**.

These are unit/regression repetitions over the production mapper code. A repeated QEMU production parse did not run because the focused guest boot did not reach symbol loading.

## Generation and source association

Phase 29G's client model generation (`0` or `1`) and the server target generation (`1`) are not one shared counter. The client value denotes project model state; the build operation ID identifies an output operation; the mapper generation identifies a symbol snapshot; the server registration/run generation belongs to its own runtime lifecycle. These domains are traced distinctly and must not be forced to equal numbers.

An earlier exploratory QEMU diagnostic run recorded client project generation 1, build operation 2, mapper generation 1, and a stable project/target/path/size/hash tuple across the diagnostic boundaries. It associated `src/helper.cpp` under source root `src`, mapped breakpoints, reached server request/RUNNING, and emitted the Phase 29G return marker. This was a single exploratory run, not an acceptance tier. In the focused acceptance attempt the guest never entered the diagnostic mode, so source association, breakpoint mapping, server start, return, and Phase 29A stop mapping were not evaluated.

## Tests and validation

- Native CMake build completed; **CTest 32/32 passed** after implementation changes.
- DWARF capacity validation: **PASS, dies=397**.
- Freestanding AMD64 and ARM64 Developer Studio builds completed, including the build script's locals/arguments proof (`Developer Studio DWARF locals/arguments test PASS`). The host build/model validation also completed.
- Server PowerShell harness AST parsing passed after the marker checks and early stop predicate were updated.
- `git diff --check` passed for both repositories before documentation was added.
- The two architecture package ELFs were copied to an isolated audit root and passed ELF64, little-endian, ET_EXEC, sectionless package checks. The audit root contained only the manifest and the two ELFs.

Package identity:

| Architecture | Size | SHA-256 |
| --- | ---: | --- |
| AMD64 | 1,048,220 bytes | `8E5F00473E489319BE43523600097BFA1F2F7B440E606D4762998A099F921A2E` |
| ARM64 | 1,212,596 bytes | `97930F633AAE79D66A6A4EA32428B10E85512442C765B9F36FB702424EE53679` |

Hosted validation first rejected a fixture copied under `%TEMP%` because the project path was outside the host's allowed project root; that attempt did not test the code path. The corrected in-repository Phase 15 fixture reached `project_open=PASS`, breakpoint toggle/key checks, and then timed out waiting for `GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_breakpoint=PENDING`. No debug start was reached. This is the next hosted readiness boundary, independent of the QEMU sentinel startup failure.

## Focused QEMU gate and later tiers

The focused command requested five fresh boots with `-Phase29GBeginDebugReturnOnly -BootCount 5 -TimeoutSeconds 120`. Staging audit passed and confirmed the active staged ESP sentinel had the expected content. On the first attempted boot, the guest emitted `SENTINEL_DECISION ... fixture=absent diagnostic=off` while opening `/Apps/DeveloperStudio/.phase28q-diagnostic`. It did not enter Phase 28Q diagnostic mode and produced no Phase 29H snapshot markers. The harness stopped at boot 1. Result: **0/5 passed; one boot attempted; no retry**. The staged file and preserved server-root sentinel were independently checked; the guest's absent classification remains unexplained.

An earlier exploratory single boot, after compacting the diagnostic marker format to respect the guest's 256-byte text buffer, did reach matching identity/hash checks, GXSM parsing/source association, breakpoint mapping, server request/RUNNING, `BEGIN_DEBUG_SESSION_RETURN`, and `PHASE29A_STOP_MAPPING_PASS`. It is not part of the fresh 5/5 acceptance count. During an even earlier instrument-format attempt, a marker was truncated; the trace format was split into shorter markers before the final focused attempt.

Since focused 5/5 did not pass:

- Full fresh QEMU acceptance: **not run (0/10)**.
- 25-cycle stress: **not run (0/25)**.
- Phase 29A after the failed focused boot: **not evaluated**; the earlier exploratory single boot passed.
- First later Phase 28Q marker: none; guest startup diagnostic precondition was the first missing boundary, before Phase 29D startup/manifest/project/build markers.
- Phase 28Q Pause/STOPPED, stack, locals, watches, stepping, exit, and teardown acceptance were not reached in this tier.

The failed focused serial log is `C:\Users\guideX\AppData\Local\Temp\guidexos-phase28g-f61ab511c930442894db3c80e9e165c2\boot1.serial.log`. The corrected hosted trace is `C:\Users\guideX\AppData\Local\Temp\guidexos-phase29h-hosted-trace-valid-ed45f60237694d02844b85a37fed7965\phase29h-hosted-condition-editor-inrepo.log`.

## Root causes, repairs, and remaining boundary

1. **`artifact_changed`:** exact historical field/value was unavailable, but a real path canonicalization mismatch was found between mapper capture and later target comparison. Both now use the same project-contained normalized relative path. Identity comparisons report the first specific differing field. In regression tests, an intentional hash mismatch is reported as `Sha256`, and size, project generation, build operation, architecture, and path mismatches each have exact results. No focused runtime reproduction occurred.
2. **`malformed_dwarf`:** not reproduced, and the saved historical failure lacks parser offset/reason and artifact identity. The production path now validates exact size/read and ELF bounds before parsing and retains detailed failure stage/offset/reason. A bounded A → B → A test passes. This is a diagnostic and regression improvement, not evidence that the historical bad artifact or parser condition was identified.
3. **Current QEMU boundary:** guest classifies the staged diagnostic sentinel as absent, so it never starts the scenario that exercises artifact/symbol ownership. This is the first QEMU blocker to resolve before acceptance can continue.
4. **Current hosted boundary:** condition-editor validation does not establish a pending source breakpoint after project open and toggle/key passes. Hosted debug launch therefore remains unverified.

## Final state

Physical hardware was not used. Package builds and local regression checks passed. No push authentication was changed. The focused acceptance failure prohibits the later gated tiers under the supplied instructions. The generated hosted fixture copy remains untracked because workspace cleanup was rejected by the execution layer; it was excluded from commits.
