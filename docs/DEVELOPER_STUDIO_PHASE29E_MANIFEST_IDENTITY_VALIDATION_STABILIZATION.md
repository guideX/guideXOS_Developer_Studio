# Developer Studio Phase 29E — Manifest Identity Validation Stabilization

## Result

**Outcome B.** The production manifest read/identity path is now bounded, invocation-owned, and deterministic in the hosted repetition test and five fresh QEMU boots. The historical Phase 29D `manifest_identity_mismatch` was not reproduced, so its exact mismatching field and root cause remain unknown. After the 5/5 manifest gate passed, the full acceptance attempt reached project `ready` and debugger-start processing, then stopped before `DEVELOPER_STUDIO_PHASE28Q_RUNNING_PASS` with `DEVELOPER_STUDIO_PHASE28Q_FAIL reason=debug_start` on boot 1. This is a later debugger-start boundary; Phase 29E did not change debugger lifecycle code.

## Repository baseline

| Repository | Branch | Starting HEAD |
| --- | --- | --- |
| Standalone Developer Studio | `main` | `486718bcc6f4c7809a8ad8c09d9edf0b1c80fe9d` |
| Server integration | `v0.5_DEVELOPER_STUDIO` | `0786df1159c8b2dda01762eeadad80e70ef36aca` |

Both worktrees were clean at those starting commits. They were at `ahead 0, behind 0` against their configured origins at the start of this phase. Phase 29D's recorded push failure was `Permission denied (publickey)`; no authentication, remotes, branches, or existing history were changed.

## Historical failure and reproduction status

The Phase 29D report says one of five focused QEMU boots reached Phase 29C `load_started` and then returned `manifest_identity_mismatch`. It did not capture a file hash, bytes-read count, identity fields, or the comparison result. A later one-boot diagnostic reached `ready`.

That exact generic mismatch did **not** recur during Phase 29E. Before the read/scratch repair, the available diagnostic QEMU manifest transactions all completed successfully with the same 570-byte fixture and FNV-1a-64 `0xF67BEB16CE32BC56`. The final clean five-boot run after the repair also passed all five boots. Thus there is no Phase 29E failing manifest hash or mismatching field to compare against a successful boot. The historical failure cannot be classified as same-hash/different-parse, different-hash, partial read, or stale expected identity from the saved Phase 29D evidence.

## Manifest identity contract

The production model is `ApplicationManifest`, parsed from `/P28Q/app/app.json`. The expected identity is built from `Project`, parsed from the same transaction's `guidexos.project` bytes. For a native ELF project, equality is field-by-field and case-sensitive:

| Authoritative field | Expected value / comparison |
| --- | --- |
| `schemaVersion` | Exactly integer `1` |
| `id` | Exact string equality with `Project.projectId` |
| `displayName` | Exact string equality with `Project.displayName` |
| `kind` | Exact string `NativeElf` |
| `entries` count | Exactly one for a single architecture; exactly two for `multi` |
| each entry `architecture` | Exact project architecture, or one each of `amd64` and `arm64` for `multi` |
| each entry `path` | Exact generated project-relative path, including `/` separators |
| each entry `entryPoint` | Exact `Project.entryPoint` |
| each entry `abi` | Exact `Project.abi` |
| each entry `runtime` | Exact string `native-elf` |

For a multi-architecture project, entry order does not define identity; duplicates and unsupported architectures are rejected, and both supported architectures must be present. There is no case folding, trimming, slash rewriting, or path canonicalization in identity comparison. The expected path is generated using the existing project path rules and compared exactly. Unknown and descriptive JSON properties such as `description` are ignored by identity equality. The existing flattened legacy manifest members are not used as a substitute for the entry list.

The parser clears the complete output model before parsing. Required identity strings and fields must be present; fixed buffers begin zeroed, and unused members/defaults remain zero/false. Authoritative string overflow reports `ManifestStringTruncated`, never a silently accepted prefix. The parser is length-bounded and does not require a trailing NUL. It accepts the existing simple JSON escapes (`\"`, `\\`, `\/`, `\b`, `\f`, `\n`, `\r`, `\t`); unsupported escapes, including `\u`, are malformed input. Equality does not compare structure padding, whole fixed buffers, pointers, or object addresses.

## Evidence from successful boots

The final focused command was:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/smoke-compiler-bootstrap.ps1 -Phase29EManifestOnly -BootCount 5 -TimeoutSeconds 180
```

All five fresh boots passed image/staging audit, Phase 29D startup ownership and fixture recognition, one request, Phase 29C acceptance, `load_started`, complete file read, identity validation, project commit, `ready`, and debugger-start request issuance. The generic mismatch marker was absent on all five.

Each boot logged the same content evidence:

| File | Expected size | Bytes read | FNV-1a-64 |
| --- | ---: | ---: | --- |
| `/P28Q/app/app.json` | 570 | 570 | `0xF67BEB16CE32BC56` |
| `/P28Q/guidexos.project` | 413 | 413 | `0x21088B42CD369EFB` |

The parsed and expected tuple matched on each boot:

```text
id/projectId=dev.guidexos.phase28q
displayName=Developer Studio Phase 28Q Pause Debugger
schemaVersion=1
kind=NativeElf
entries=1
architecture=amd64
path=bin/amd64/p28q.elf
entryPoint=gx_main
abi=guidexos-c-abi-v1
runtime=native-elf
mismatch=none; result=none
```

All logged generation values were current: request ID/generation `1/1`, candidate ID/generation `1/1`, pre-commit active project generation `0`, expected identity generation `1`, parsed identity generation `1`. This is class D's *successful* expected-identity evidence: the expected project came from the same loaded transaction and its generation matched the accepted request. It does not identify the lost Phase 29D failure's class.

The final run's serial evidence is preserved under:

`C:\Users\guideX\AppData\Local\Temp\guidexos-phase28g-a3ad201f3cea48a4984cc3dbf23fc1f0`

The command transcript is preserved beside the serial captures as `C:\Users\guideX\AppData\Local\Temp\guidexos-phase28g-a3ad201f3cea48a4984cc3dbf23fc1f0\phase29e-final-focused-5-transcript.log`. Earlier failed attempts were also preserved in their separate `guidexos-phase28g-*` temporary evidence directories; they were not counted as part of the final 5/5 run.

## Read, ownership, parser, and comparison audit

The filesystem API returns a byte count and its VFS/FAT path can report a read smaller than the requested size; a single read is not a full-read guarantee. `LoadProject` already obtains the stat size and bounds it by `kMaxProjectFileBytes`. It now requires `bytesRead == stat.size` for project metadata and the application manifest. A short metadata read returns `ProjectMetadataReadPartial`; a short application manifest read returns `ManifestReadPartial`. Oversized results and failed reads remain explicit errors. There are no repeated reads or timing retries. Parsing receives the exact explicit length, so neither an added NUL nor stale bytes after the file length participate.

The former load path retained function-static file/path/model scratch and did not consume the caller's controller-owned scratch. The repaired path uses the controller's `ProjectLoadScratch` for bare-metal transactions and a frame-local scratch for hosted direct calls. Manifest bytes and `ApplicationManifest` are owned by that scratch for the duration of the load. `CreateNativeGuiProject` also uses its supplied scratch for generated content and reuses it through the subsequent load. The controller passes its own scratch for project open and refresh; no global or function-static manifest identity buffer remains.

`ValidateApplicationManifestIdentity` uses explicit per-field string/integer comparisons. It reports a bounded mismatch field, expected/actual values, and a specific result code for schema, app ID, display name, kind, entry count, architecture, path, entry point, ABI, runtime, and stale expected/parsed/candidate generations. The higher-level `ManifestIdentityMismatch` code remains available for existing callers. A diagnostic reset inside validation had previously erased raw read evidence; `LoadProject` now restores the file path, sizes, byte counts, hashes, and generation tuple after validation so the emitted log retains the source evidence.

Generation validation checks the request ID/generation, candidate ID/generation, expected identity generation, and parsed identity generation. No Phase 29E QEMU transaction reported a stale generation. The historic failed boot had no generation values, so stale ownership cannot be established as its cause.

## Repairs and regression coverage

The repair adds exact stat-size/read-size checks for metadata and manifests; bounded per-transaction read/parser storage; explicit-length parsing; truncation errors; field-specific identity/generation failures; and compact diagnostics with content hashes and expected/parsed fields. It does not retry, special-case `/P28Q`, or relax identity validation.

The production parser/validator test now covers different app IDs and project IDs, target path mismatch, descriptive-field immunity, distinct input buffers, stale string tails, different padding bytes, stale expected/candidate/parsed generations, authoritative string truncation, partial manifest reads, and valid-A → different-B → valid-A parsing. It repeats production parsing and validation 1,000 times with identical authoritative fields. Two independent `ProjectLoadScratch` instances load the fixture consistently. The controller transaction test injects a partial read, confirms failure and preserves the previously active project and generation, then immediately retries successfully with a new request. These tests use production parsing, validation, and controller code.

Results:

- CTest: **32/32 passed**.
- Production parser/validator repetition: **1,000/1,000 identical results**, including the A→B→A transition.
- Partial-read transaction: rejected with `manifest_read_partial`; prior active project remained published; immediate fresh retry passed.
- Transaction visibility: candidate stayed private until commit; success published generation 1 and `ready`.

## Tier 1 validation

- Full CTest passed, 32/32.
- DWARF capacity passed: `dies=397`.
- DWARF locals/arguments validation passed on the AMD64 and ARM64 package builds.
- `tests/run-developer-studio-validation-fast.ps1 -ServerRoot D:\dev\guideXOSServerV0.5_DEVELOPER_STUDIO` passed in 380.2 seconds. It included the hosted project validation, package rebuilds, and a representative real hosted debugger lifecycle: source breakpoint pause, call stack/locals inspection, Continue/rebind, orderly teardown, and server exit 0.
- `tests/smoke-workspace.ps1 -ServerRoot D:\dev\guideXOSServerV0.5_DEVELOPER_STUDIO` passed.
- Final package-content audit passed for both architectures.

| Package | SHA-256 |
| --- | --- |
| AMD64 | `EBB3675EDA963CFF8B0D586C2FD7233AD36644135B52B32DEA7F1922840B244B` |
| ARM64 | `28C46E0E6A0A5DA18FF2C7F15E799BF88A69FF55DE7B6FE3B33394D6FAA89BF2` |

## Full acceptance boundary

After the focused 5/5 gate passed, the complete Phase 28Q/29D QEMU acceptance was started with `-Phase28QOnly -BootCount 10 -TimeoutSeconds 180`. It failed on the first fresh boot, so the accepted result is **0/10** and the remaining nine boots were not counted or run. The boot passed the project transaction through `ready` and emitted both `DEVELOPER_STUDIO_PHASE28Q_DEBUG_START_PASS` and the debugger-start event. It then emitted:

```text
DEVELOPER_STUDIO_PHASE28Q_FAIL_REASON_OTHER
DEVELOPER_STUDIO_PHASE28Q_FAIL reason=debug_start
DEVELOPER_STUDIO_PHASE28Q_FAILURE
```

The first required later marker that was absent was `DEVELOPER_STUDIO_PHASE28Q_RUNNING_PASS`; pause, STOPPED/Phase 29A source mapping, inspection, Continue/Step, target exit, and teardown markers consequently did not run. The failure is after the manifest transaction and is classified as debugger-start processing. It does not justify reopening manifest validation or changing Phase 29A/29B behavior within this phase.

Failure serial evidence is preserved under:

`C:\Users\guideX\AppData\Local\Temp\guidexos-phase28g-d60e38eeb3804301b2a1375f02e2892e`

The command transcript is preserved beside the serial captures as `C:\Users\guideX\AppData\Local\Temp\guidexos-phase28g-d60e38eeb3804301b2a1375f02e2892e\phase29e-full-10-acceptance-transcript.log`. Since 10/10 did not pass, the conditional 25-cycle stress run was **not run**. The Phase 28Q continuation remains incomplete at `DEVELOPER_STUDIO_PHASE28Q_RUNNING_PASS`. Physical hardware was not tested.

## Final classification

The original intermittent manifest mismatch remains unexplained because no failure field/hash/read trace exists and it did not reproduce. Phase 29E materially repairs two concrete hazards in the production loader—accepting a short read as though it were complete and shared static manifest/load scratch—and adds enough telemetry to identify a future mismatch precisely. The evidence now shows stable identity for 1,000 hosted production parser/validator repetitions and five fresh QEMU boots with identical bytes and generation values. The outstanding blocker is the later QEMU debugger-start failure after `ready`, not an observed manifest mismatch. Therefore this phase is **Outcome B**, not Outcome A or C.
