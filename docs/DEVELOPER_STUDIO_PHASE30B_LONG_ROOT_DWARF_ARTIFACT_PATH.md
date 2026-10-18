# Developer Studio Phase 30B — Long-Root DWARF Parsing and Artifact Path Determinism

**Current status as of 2026-10-17: Outcome B.** The captured Server access violation was localized to a native-app worker polling the process-global IPC map during static destruction, and the shutdown repair now waits for worker termination. The repaired host passed GXSM shutdown 5/5 and mixed debugger shutdown stress 25/25 (13 ordinary debugger sessions and 12 GXSM sessions; all Server exits were 0). Short-root and long-root hosted debugger gates remain 5/5 each, hosted path/artifact stress remains 25/25, and Debug and Release CTest remain 33/33 each. Attempt 08 passed the PowerShell/package preflight and waited for the isolated QEMU lane, but its first launched guest boot was stopped when the in-gate process snapshot caught a foreign QEMU `-version` process from the C176 support runner. The guard stopped only the Phase 30B QEMU and child. Guest acceptance is 0/5; the failed launched boot is preserved and no replacement boot was started. Phase 30B remains open; do not begin Phase 30C. The historical `0x40010004` termination owner remains unproven.

## Prompt and baseline gate

- No authoritative `.phase` marker was present in either repository.
- The accepted Phase 30A closeout, `docs/DEVELOPER_STUDIO_PHASE30A_BREAKPOINT_MANAGER_REMAP_RETENTION.md`, was treated as authoritative. It records Outcome C: Manager 5/5, rebuild/remap 5/5, condition-editor marker freshness 10/10, condition input 20/20, hosted Manager/remap stress 25/25, all five remap checkpoints passed, and no established production defect. Phase 30A remains frozen; no Phase 30A production ownership or condition code was changed.
- Standalone repository started on `main` at `f1894058aa06704e638e57f503cbd9fa31e36a35`, clean.
- Server repository started on `v0.5_DEVELOPER_STUDIO` at `84cc3c781d59bf837a2d0fd92ef3b5615de6a648`, clean.
- No branch, worktree, merge, rebase, reset, stash, or remote/authentication change was made.
- Phase 29V report, `docs/DEVELOPER_STUDIO_PHASE29V_DWARF_CAPACITY_TRUNCATION.md`, remains the historical 413-DIE acceptance record. It reports Ready, no error, no truncation, line 37 mapping to five addresses with primary `0x200016AC`, reverse mapping, 511/512 accepted, 513 limited, and malformed input rejected. The report records artifact SHA-256 `1628251736D2BC9EC1FFE6BC4F0B4DB83C63E609778210918661B878E7AD7AE6`, size 12,864 bytes. That exact artifact was not present in the recovered evidence, so its 413-DIE result is cited as a historical report and was not represented as a fresh reproduction.

## Historical evidence recovered

The short-root binary was recovered at `C:\Users\guideX\AppData\Local\Temp\p29xe\build\bin\amd64\debugger-phase15.elf`. It is 12,544 bytes, SHA-256 `B2AB1D0DAABF50A975A2033DB56EF0ECD665DB92058607D174C1062B05B99374`, with project root length 40 characters and artifact path length 77.

The long-root binary was recovered at `C:\Users\guideX\AppData\Local\Temp\guidexos-phase29x-final-ui-regressions-20261004\condition-error-recovery\build\bin\amd64\debugger-phase15.elf`. It is 12,944 bytes, SHA-256 `F9D4FF9C38D3B8E057E933123B2DEE6310C095EE4007DBADFCF277616C9ED73D`, with project root length 107 characters and artifact path length 144. The historical UI trace records `malformed_dwarf`, 421 DIEs and 180 line rows, but zero functions and variables. The exact original command line and reliably attributable parser offset/error stage were not retained. The recovered file, trace, report, and extracted section hashes are the available evidence; missing invocation details were not reconstructed from guesses.

Both recovered binaries are ELF64, little-endian, ET_EXEC, AMD64. They are **not byte-identical**. They share `.debug_abbrev`, `.debug_info`, `.debug_addr`, and `.debug_frame`; their path-bearing string and line sections differ:

| Section | Short-root size / SHA-256 | Long-root size / SHA-256 |
|---|---|---|
| `.debug_abbrev` | 475 / `324270C152E835BE74496ACEA068D1490F3520433FA8FA5DE2F71C8BBF616A9F` | 475 / same |
| `.debug_info` | 2,749 / `6931A666FE1FE84DA3F5D5F3FE828D953075E46D87D82B837CD3E7BF5C71178D` | 2,749 / same |
| `.debug_line` | 1,021 / `9972E52C03B989F524549B03C0E79763C411428212CF00DE18E1C836C42E7CB6` | 1,021 / `5A65FED3DA8C3A294500B5DC574A3F8D7531E546BA82C486FCE16158DF483DA4` |
| `.debug_str_offsets` | 748 / `1AA78C6C54EC1DCD6C85BE4F0011594DAA65EE9290052A6E0826783FA5E994C3` | 748 / `90B90810F6921981C2456E2C4746976B7D5564D85B37C6198A4FA10311710AB6` |
| `.debug_str` | 3,161 / `53B9833449844F5979782A9039E02B5F9E8EB9FEE01A654E1976B1B16E3BB65C` | 3,362 / `5A9E0E114E6EEFFBC7E7AF85AE7E11B12ADDDF89D8CA9714A576A2BCC55EDDC9` |
| `.debug_addr` | 120 / `006C0C4349551301CD5D8FCE9FEBE5BE6E148ED30CD42FC7E64BA70465C8B` | 120 / same |
| `.debug_frame` | 232 / `D4386BEFFBF0A37D50C7E89130CD50F04D059A4ADFF5B6EB6977B8DB597CBF32` | 232 / same |
| `.debug_line_str` | 414 / `5E56A894C06D11BB4E816F642AFF6F6C7D9E3FEA27F33EEF00F743AB0F2124C5` | 615 / `255D4C63652DE0F8E8E83F33E93DC4BA2F8925E0F163A3FFB14123535CB306D2` |

The `main.cpp` source hash is the same in both historical trees: `9636FCE10823AD2FF7CD61DB41E7FEB8BE93A79934BC8722695CDD0D3CE9E175`. The short compilation directory is 40 characters; the long one is 107. The long artifact contains an absolute second-CU source name of 135 characters (`...\src\freestanding_memory.cpp`). Thus the historic comparison changed both artifact bytes and embedded DWARF paths; it could not by itself prove that merely relocating identical bytes caused the old `malformed_dwarf`.

## Controlled comparison and earliest divergences

All recovered and newly created roots were on volume `C:\`; their path text was ASCII, so character and UTF-8 byte lengths match. Direct runner working directory recorded for the controlled cases was `D:\dev\guideXOS_Developer_Studio`. The original historical command and working directory remain unavailable.

| Case | Root and artifact path lengths | Artifact identity | Result |
|---|---:|---|---|
| Historical short baseline | root 40, artifact 77 | B2AB, 12,544 bytes | Historical direct parse passed. |
| Same B2AB bytes, controlled short copy | root 68, artifact 105 | B2AB, 12,544 bytes | Post-repair direct parse passed. |
| Same B2AB bytes, controlled long relocation | root 110, artifact 147 | B2AB, 12,544 bytes | Post-repair direct parse passed, including line 37 and reverse mapping. The relocated fixture had no physical `src/main.cpp`; this result proves parser/source-map normalization, not disk source-file availability. |
| Same B2AB bytes, over-capacity relocation | root 145, artifact 182 | B2AB, 12,544 bytes | Complete file read, then pre-classification code returned `artifact_changed` before parser entry. This failed instance was preserved and not rerun. The new unit boundary tests separately establish the correct explicit capacity error. |
| Historical long-root artifact | root 107, artifact 144 | F9D, 12,944 bytes | Historical UI reports `malformed_dwarf`. The old direct capacity runner only retained a `no_debug_info` lookup failure, not the parser's exact internal error or offset. |
| New hosted short-root build | root 42, artifact 79 | 12,560 bytes, SHA-256 `4E1F7B5A2AF8D4D257175FDE58F6FB936C41AB2BC272BF92A90BA7DEAAE543CD` | Parser Ready, then the formal first cycle stopped at stale line-7 breakpoint remap (`line_not_mapped`). |

The short and long historical ELF files differ, and the path-bearing sections and CU source name differ. The earliest well-supported historical producer is a **G — parser capacity/error-classification defect**: the former 128-byte DIE name buffer could not hold the recovered 135-character absolute CU name. The old path from string resolution/reference handling then surfaced a parser failure as malformed rather than reporting a bounded capacity error. The exact historical failing offset was not captured, so this localization is supported by recovered strings, the old code path, and the reproduced capacity boundary, not by a preserved old-parser stack/offset.

The controlled same-byte tests also exposed a separate **B — path-capacity classification defect**. An artifact path of 182 characters exceeded the 160-byte path representation, and old code hid the capacity failure as `artifact_changed` before parse. This instance was preserved. No silent truncation was found. Fresh tests now require an explicit `limit_exceeded` result at that capacity.

The first controlled cross-root attempt before the source-path fix failed before producing line rows because DWARF 5 directory indexes were treated as one-based. This was **F — DWARF source-path handling**, not structural corruption. The fix follows DWARF 5's zero-based directory table, where index 0 names the compilation directory. The relevant standard is [DWARF Version 5](https://dwarfstd.org/doc/DWARF5.pdf).

## Repair and bounded diagnostics

Production changes are limited to the standalone Developer Studio parser/source-path handling:

- DWARF 5 directory indexes use zero-based indexing; index 0 resolves to the compilation directory.
- Absolute source names under the artifact's original compilation directory are reduced to their project-relative name and re-rooted beneath the current project root. SDK/external source paths remain external.
- The DWARF DIE name capacity is now 160 bytes, matching the mapper path representation; local/member name limits remain unchanged.
- String and CU-name capacity failures propagate as `limit_exceeded` with bounded stage/reason detail instead of being overwritten by a generic form/reference error or reported as malformed.
- Absolute artifact path overflow is explicitly `limit_exceeded`, stage `artifact_identity_path_capacity`, reason `project_root_exceeded` or `executable_path_exceeded`.
- The capacity runner verifies the actual file read count equals the expected file size and emits bounded state/error/stage/reason/count diagnostics on early failure. No per-DIE logging or global logging framework was added.

The 160-byte capacity includes the terminating NUL, so at most 159 text bytes are supported. Production tests exercise:

- Source-path lengths 159: accepted; 160 and 161: `limit_exceeded`.
- Absolute artifact-path lengths 159: accepted; 160 and 161: `limit_exceeded`.
- CU DW_AT_name lengths 159: accepted; 160 and 161: `limit_exceeded`, stage `dwarf_source_path_capacity`, reason `compilation_unit_name_exceeded`.
- The separate 145-character-root / 182-character-artifact instance was not repeated after repair.

## Parser and source-map results

The recovered short B2AB artifact was read completely: 12,544 of 12,544 bytes. Post-repair direct parse is Ready, `error=none`, `truncated=0`, offset 12,544; DWARF 5, 2 CUs, 421 DIEs, 4 functions, 21 variables, 2 sources, 7 external sources, 180 rows, 54 line keys, 2 sequences, `.debug_info` 2,749 bytes, `.debug_abbrev` 475 bytes, and `.debug_line` 1,021 bytes. `src/main.cpp:37` maps to five addresses, primary `0x200016ac`, and reverse mapping returns line 37.

The same artifact hash at the 110-character controlled long root produced the same structural counts and line-37 mapping. The parser normalized its source table to `src/main.cpp` and `src/freestanding_memory.cpp`. Physical source-file availability at that exact relocated root was absent and is not claimed.

The original 413-DIE Phase 29V artifact was unavailable for a fresh run. The recovered B2AB artifact is a different 421-DIE build and is not substituted for the 413-DIE baseline.

Boundary tests pass: 511 DIEs accepted, 512 accepted, and 513 returns `limit_exceeded`. Intentionally malformed DWARF remains rejected as `malformed_dwarf`. The parser's malformed-input behavior was not weakened.

## Hosted acceptance gate and harness evidence

The first hosted smoke attempt reached the editor but stopped because its assertion still expected `manager_snapshot=absent`; accepted Phase 30A now emits `manager_snapshot=workspace`. The test-only expectation was updated to match that frozen Phase 30A projection. No Phase 30A production code changed.

The required fresh short-root gate then started one cycle at `C:\Users\guideX\AppData\Local\Temp\p30bs02` (root length 42). Its newly built artifact was selected as the execution and debug ELF, size 12,560, SHA-256 `4E1F7B5A2AF8D4D257175FDE58F6FB936C41AB2BC272BF92A90BA7DEAAE543CD`. The project manifest hash was `A45F6C4A3E7B4B141E8FCC6195BE8F4DECEC5756CAD4055EDDA36F3951FA4205`. Logs show project generation 1, build operation 2, artifact hash validation, symbol generation 1, DWARF 5, 421 DIEs, 2 CUs, 4 functions, 21 variables, 2 files and 180 line rows. The parser reached Ready with no truncation.

The first breakpoint-remap request came from a copied stale `.debugger.json`: `src/main.cpp:7` was enabled, while intended line 37 was disabled. The current artifact's line 7 had no row, so remap returned `line_not_mapped` and debugger startup was skipped. This is **J — harness fixture state**, after successful symbol loading; it is not a `malformed_dwarf` parser result, stale generation, or product breakpoint ownership defect. The trace and console log are preserved at `C:\Users\guideX\AppData\Local\Temp\phase30b-repair-20261012-6f19c2\hosted\short-formal-01.log` and `C:\Users\guideX\AppData\Local\Temp\phase30b-repair-20261012-6f19c2\hosted-short-formal-01-console.log`.

The short-root focused gate is therefore **0/5; stopped at cycle 1**. No retry of that failed case was made. Under the ordered gate, the long-root 5/5 gate did not start, and the combined 25-cycle stress did not start (**0/25 run**). The formal hosted gate does not satisfy Phase 30B.

The hosted application log confirms a current build artifact hash match and `DWARF_READY`; the hosted marker does not expose an exact byte-count field. Direct parser runner reads for the B2AB cases report exact complete byte counts. The generation chain in the formal hosted run is current: project generation 1, build operation 2, symbol generation 1. No stale debugger session was admitted.

## Other required regressions

- Debug CTest: 32/32 passed after excluding the single unrelated `developer_studio_symbol_test` case that had failed once when Debug and Release CTest ran concurrently against the same fixed temporary directory. Focused `developer_studio_debug_symbols_test` passed in Debug after final production changes.
- Release CTest: 32/32 passed after excluding that same unrelated shared-temp case; focused `developer_studio_debug_symbols_test` passed in Release. A full Release 33/33 run also passed before the final artifact-path error-classification change.
- Hosted debugger stepping regression passed on a clean fixture: source breakpoint, Step Into, Step Over, Step Out, stack/locals refresh, Watch invalidation, Continue, target exit, and teardown. Its clean root length was 70. Evidence: `C:\Users\guideX\AppData\Local\Temp\phase30b-repair-20261012-6f19c2\hosted\debugger-stepping.log`.
- Positive GXSM regression passed: GXSM v2, live `counter`, `counter == 2`, refresh/invalidation, relaunch with fresh generations, both target exits and teardown. It used a separate fixture and preserved Phase 15 behavior without GXSM. Evidence: `C:\Users\guideX\AppData\Local\Temp\phase30b-repair-20261012-6f19c2\hosted\gxsm-positive.log`.
- Phase 30A Manager/remap stress was not repeated because its accepted shared model/condition paths were unchanged.

## QEMU and package qualification

QEMU was required because the changed parser is compiled into the bare-metal Developer Studio ELF as well as host tests. The Server checkout itself was not modified. A disposable Server tree and isolated sibling source header were staged under `C:\Users\guideX\AppData\Local\Temp\phase30b-repair-20261012-6f19c2\qemu-isolated`; QEMU used the already audited AMD64 Developer Studio ELF, SHA-256 `C5433205239735622C027A6EEE880E3B2F81340DFAC9304EE0A0A205D0ED78F4`, size 1,128,092 bytes.

The first setup attempt stopped in the copied kernel build before QEMU launch because the isolated copy omitted top-level Server headers. Those root-level build inputs were copied into the isolated tree and the bounded run then passed: one fresh QEMU Phase 28Q boot, packaged Developer Studio discovery and launch, debugger project open, symbol mapper invocation, Ready line table (DWARF 0, 2 files, 22 rows for this guest fixture), breakpoint stop/source navigation at `src/main.cpp:13`, continue, cleanup, and `DEVELOPER_STUDIO_PHASE28Q_PASS`. This is a guest integration regression, not long-root acceptance.

Both standalone packages were rebuilt in a temporary stage with the specified v0.5 Server SDK and audited:

| Architecture | Size | SHA-256 | Audit |
|---|---:|---|---|
| AMD64 | 1,128,092 bytes | `C5433205239735622C027A6EEE880E3B2F81340DFAC9304EE0A0A205D0ED78F4` | ELF64 little-endian ET_EXEC; no section headers/debug sections |
| ARM64 | 1,288,628 bytes | `EAA477DE53171F49638EB9A6012E88326CA006B3F196E70A4F0D3B87A5711048` | ELF64 little-endian ET_EXEC; no section headers/debug sections |

The staged Server runtime binary hash matched its source. No protected package, SDK, or source in the live Server repository was overwritten; its final HEAD and worktree remained unchanged.

## Closeout

- **Historical root cause:** best-supported classification G, a DWARF name/path-capacity and error-classification defect exposed by distinct path-bearing artifact bytes. The separate over-capacity artifact identity path also had a hidden capacity classification (B), now covered by explicit capacity tests.
- **Historical artifact equivalence:** no; B2AB short and F9D long are distinct builds. The exact Phase 29V 413-DIE file was not recovered.
- **Production repair:** yes, limited to DWARF source-path indexing/normalization and bounded capacity/error reporting.
- **Harness repair:** yes, updated the stale Manager projection assertion; the later formal cycle exposed an independent stale breakpoint fixture.
- **Remaining acceptance boundary:** the required short-root gate failed at its first cycle because the fixture enabled line 7 instead of line 37. Do not count parser Ready as a passing debugger cycle. The long-root 5/5 and fresh 25-cycle hosted stress remain outstanding. Phase 30B remains Outcome B; do not advance to Phase 30C.

## Evidence and final repository state

- Direct parser inputs opened and read successfully for the B2AB runs; the runner recorded 12,544 bytes read for 12,544 expected. Hosted symbol-load logs recorded the built artifact hash validation and Ready state but did not expose a separate byte-count field.
- Evidence was kept in unique directories under `C:\Users\guideX\AppData\Local\Temp\phase30b-repair-20261012-6f19c2`, plus the QEMU run's unique `guidexos-phase28g-ea0dfee468db415c97fbd8a3f23cdf6e` directory. Historical evidence directories were not moved or overwritten. Each failed attempt has its own preserved log; the over-capacity relocation was not rerun.
- No physical hardware was used or qualified. The guest regression ran under QEMU only.
- Phase 30A regression gate required: no. Its accepted Manager/remap/condition model paths were not modified; the hosted smoke assertion was updated only to reflect the accepted Manager snapshot marker.
- The standalone changes are committed locally on `main`. An ordinary `git push origin main` was attempted and rejected with `Permission denied (publickey)`. No authentication troubleshooting was performed.
- The standalone branch was 1 ahead and 0 behind origin/main at closeout; its worktree was clean after the commit.
- Server ending HEAD remains `84cc3c781d59bf837a2d0fd92ef3b5615de6a648`, 0 ahead and 0 behind its configured branch. The Server worktree is clean; no Server commit was created.
- Remaining blocker/fault domain: replace the stale line-7 fixture state with an independently prepared valid breakpoint fixture in a new run, then complete the required fresh short-root and long-root 5/5 gates before starting the 25-cycle hosted stress. Do not retry the failed p30bs02 case or advance to Phase 30C.

## Phase 30B continuation acceptance — 2026-10-13

This addendum supersedes the earlier **remaining hosted gate** status above for the continuation runs only. It preserves the earlier failed `p30bs02` result and all historical evidence. Phase 30B remains Outcome B because the positive GXSM smoke reached its final host-shutdown check but the Server process exited abnormally. Phase 30C was not started.

### Continuation gate and repository state

The mandatory stale-prompt check passed: no `.phase` marker blocked continuation, the authoritative report marked Phase 30B incomplete/Outcome B, and Phase 30C was not active. Starting repositories were clean:

| Repository | Branch / starting HEAD | Starting upstream status |
|---|---|---|
| Standalone Developer Studio | `main` / `5f0a09e1023d1c327e56e8689be47c5b33b96820` | 1 ahead, 0 behind |
| Server integration | `v0.5_DEVELOPER_STUDIO` / `84cc3c781d59bf837a2d0fd92ef3b5615de6a648` | 0 ahead, 0 behind |

No Server source, SDK, or package in the Server worktree was modified. The continuation changes are limited to the standalone debugger fixture, smoke harness, negative source-map assertion, and this report.

### Stale breakpoint fixture and correction

The enabled line-7 entry came from the tracked `tests/fixtures/debugger-phase15/guidexos.debugger.json` and its `.bak`, which the failing hosted fixture copied. It was first in the enabled-breakpoint list; line 37 was also enabled in that stale copy. It was not generated by workspace initialization during the failed run. In the current `src/main.cpp`, line 7 is a closing `};` and has no executable DWARF row. The parser correctly returns `line_not_mapped` for that source location.

The fixture JSON and backup now contain one enabled breakpoint, `src/main.cpp:37`. A focused negative regression still asks for line 7 and requires `DebugDwarfError::LineNotMapped`. The hosted lifecycle uses the legitimate compiler-produced line 37 with condition `counter == 2`. Direct maps show five addresses and reverse mapping; the hosted debugger binds and stops at the line. For these fresh compiler outputs, stepping was checked at lines 38, 39, and 48 in `debugCaller`.

The successful independent preflight used a fresh short root and a fresh 110-character long root with source trees in place. It recorded compiler `clang 22.1.8`, the staged v0.5 SDK, `DebugSymbols` builds, full ELF hashes, and a direct mapper result of Ready/no truncation. The independent long-root preflight artifact is 12,984 bytes, SHA-256 `631ECB1789657182B3766BA9FD731BDF17EF108B62FB070E4794FFE235D575C4`. The separate qualified same-byte artifact used in the cross-root control is 12,720 bytes, SHA-256 `D5B4453DB64DE646EB76A721CFC05DA856D824DC3615FB31AC05283D03492C87`. The log under `phase30b-continuation-20261013-01\long-preflight-source-map.log` records DWARF 5, 2 CUs, 421 DIEs, 4 functions, 21 variables, and 180 line rows. It resolves `src/main.cpp` and `src/freestanding_memory.cpp` to paths of 123 and 138 characters, with line 37 mapped to five addresses and reverse lookup passing.

### Fresh hosted 5/5 gates

Each formal fixture has a unique project ID, project root, artifact, console, and trace. Physical source files and project metadata were present in all ten roots. Each smoke ran the hosted debugger through build, symbol selection, breakpoint binding and pause, stack/locals, Step Into/Over/Out, Continue, target exit, and teardown. All used the same staged AMD64 package SHA-256 `C5433205239735622C027A6EEE880E3B2F81340DFAC9304EE0A0A205D0ED78F4`.

| Gate run | Project root / artifact path lengths | Artifact SHA-256 |
|---|---:|---|
| short-02 | 75 / 112 | `43CBA659B9B6F7B4AFAA3DFE6DB720EB0B6D059C66B280D3A5A0DDDB187B3847` |
| short-03 | 75 / 112 | `7394C320D5CE35127C4FC95AB58CAEB2A608B350FB508BB4F23DCD488C61BF97` |
| short-04 | 75 / 112 | `ECB120E5F7D2CDFF0AE1FD25D5D6FCEE2F91841C96EEECEB3BAAFE57C56976BB` |
| short-05 | 75 / 112 | `4ABC80B7B82A0742334AC88D0330E0C5537B936BCFD829A438547275522D480E` |
| short-06 | 75 / 112 | `3AEB61DFF2BDB25ADD9E5E116842AEB39DB10B51670756E13098A57C83FEEC8C` |
| long-01 | 110 / 147 | `CA48BEF7F9F0F549593455181767476C303C950F2E923D7E887B50933DF8E977` |
| long-02 | 110 / 147 | `1565E599AF0D82832E75C651E40CDCA08F6F2A300ED528CE849F5A43D71624C8` |
| long-03 | 110 / 147 | `85065E0094CC916EE66AA9E0E460E39E7779AD4739EC8A0FD1B1F6965F2EC0A2` |
| long-04 | 110 / 147 | `98C0CE7962A8C6602712C726178F435A76FE1FA153A02F7B588AF2ECFC950D28` |
| long-05 | 110 / 147 | `2F9CDF6716BFCD08F67A8BB47571FBF87E41ECC78A7DCC00FE0F0D648D0FD725` |

**Short-root gate: 5/5 PASS. Long-root gate: 5/5 PASS.** Short run 02's native smoke passed every lifecycle check; a wrapper post-check looked for spaced `Step Into` while the emitted marker is `StepInto`, so its manifest retained a wrapper false-negative. The raw console and trace were independently adjudicated in `formal-short-gate-sessions\short-02\native-smoke-assessment.json`. A separate short-01 runner setup error occurred before any Server process/session and is not one of the five hosted sessions. The historical `p30bs02` cycle was not retried or counted.

The direct read-only DWARF audit for all ten formal artifacts confirms Ready, no truncation, DWARF 5, 2 CUs, 421 DIEs, 4 functions, 21 variables, 180 rows, five line-37 addresses, reverse lookup PASS, and line 7 rejected as `line_not_mapped`. The artifact-specific source paths are 88/103 characters for short-root main/memory files and 123/138 for long-root main/memory files. Full section hashes and source-query logs are recorded in `formal-gate-direct-audit.json`.

Across these ten independently built ELF files, `.debug_info` is 2,749 bytes with SHA-256 `48C8E4A3953E3EF09337A4DA93A7FC5A07F863FCB100CDB23198408B0651BE1F`, and `.debug_abbrev` is 475 bytes with SHA-256 `324270C152E835BE74496ACEA068D1490F3520433FA8FA5DE2F71C8BBF616A9F`. Their path-bearing `.debug_line`, `.debug_str`, and `.debug_line_str` sections have distinct hashes across the ten roots; every per-artifact size and hash is in the direct-audit JSON. The common `.debug_info`/`.debug_abbrev` identities and changing path-section identities are consistent with the different compilation directories.

### Same-byte cross-root control and path boundaries

A 12,720-byte artifact was copied without modification from the 69-character preflight root into a supported 110-character root (artifact path 147). Both roots had physical source trees. SHA-256 matched at `D5B4453DB64DE646EB76A721CFC05DA856D824DC3615FB31AC05283D03492C87`. Read-only section extraction confirms identical hashes for `.debug_info`, `.debug_abbrev`, `.debug_line`, `.debug_str`, and `.debug_line_str`; both direct parses decode 30 source rows identically, map line 37 to the same five addresses, and reverse-map successfully. The relocated direct parse is Ready with 421 DIEs and 180 rows. The hosted stress also exercised six same-byte copy cycles across both supported root classes. Details are in `same-byte-cross-root-comparison.json` and the six cycle build records.

The supported 110-character roots keep every path component below 64 characters and their artifact paths at 147 characters. A fresh negative boundary probe used a 145-character root and 182-character artifact path, read all 12,760 input bytes, and explicitly returned `limit_exceeded`, stage `artifact_identity_path_capacity`, reason `executable_path_exceeded`, offset 160. This is an expected capacity rejection, not malformed DWARF. The 159/160/161 source-path and CU-name boundaries and 511/512/513 DIE boundary remain covered by the capacity regression. Malformed DWARF remains `malformed_dwarf`.

The recovered historical B2AB file was also freshly read at `C:\Users\guideX\AppData\Local\Temp\p29xe\build\bin\amd64\debugger-phase15.elf`: 12,544 bytes, SHA-256 `B2AB1D0DAABF50A975A2033DB56EF0ECD665DB92058607D174C1062B05B99374`. Its source tree exists. The parser is Ready, no error/truncation, DWARF 5, 2 CUs, 421 DIEs, 4 functions, 21 variables, and 180 rows. `src/main.cpp:37` maps to the historical five addresses with primary `0x200016AC` and reverse mapping PASS; line 7 remains unmapped. The original 413-DIE Phase 29V ELF was not found in the available continuation evidence and remains a historical-only limitation.

### Fresh hosted stress

The 25-cycle schedule was written before cycle 1 and was not changed; SHA-256 `1E5F326C139286495108147B149B6875BCA52580D4C3D239E57A386AD234FB8F`. **25/25 PASS**, with 25 unique project IDs and evidence directories: 7 short-root builds, 6 long-root builds, 6 same-byte copies, and 6 independent rebuilds. Every artifact hash was rechecked against its cycle manifest; each console and trace is present; each cycle records the real breakpoint, steps, target exit, and teardown. All six same-byte outputs match the qualified `D5B445…` source and have external per-cycle build records. Independent builds have distinct hashes from that copied baseline. No stress cycle failed; no cycle was retried. The schedule and integrity index are `hosted-stress-schedule.json` and `stress-evidence-integrity.json`.

### Regression closeout and remaining failure

- Fresh Debug CTest: **33/33 PASS**, including `developer_studio_symbol_test`.
- Fresh Release CTest: **33/33 PASS**, including `developer_studio_symbol_test`.
- The debug-symbol test and fresh capacity probe preserve 421-DIE parsing, reverse mapping, line-7 rejection, 511/512/513 DIE limits, 159/160/161 source-name boundaries, and malformed-input classification.
- The positive GXSM run used genuine v2 metadata (`valid=1`, version 2), read live `counter=2`, evaluated `counter == 2`, invalidated old inspection state, relaunched with session generation 2, and emitted target exit and debugger teardown markers. The bounded smoke nevertheless **failed** its final clean-host-shutdown invariant: the hosted Server process returned `-1073741819` (`0xC0000005`) after the debugger lifecycle. This check was not retried. Its console and trace are retained under `gxsm-positive-run`.
- No new QEMU boot was run. This continuation changed only hosted fixtures/tests/harness/docs; the guest package remains the previously qualified AMD64 binary at SHA-256 `C5433205239735622C027A6EEE880E3B2F81340DFAC9304EE0A0A205D0ED78F4`. Under the continuation's QEMU exception, another boot or package rebuild was unnecessary. The earlier isolated QEMU pass remains the prior Phase 30B evidence, not a new continuation run.
- The staged package hashes are unchanged: AMD64 `C5433205239735622C027A6EEE880E3B2F81340DFAC9304EE0A0A205D0ED78F4` (1,128,092 bytes) and ARM64 `EAA477DE53171F49638EB9A6012E88326CA006B3F196E70A4F0D3B87A5711048` (1,288,628 bytes). No Server package was rebuilt or replaced.
- Phase 30A Manager/remap suites were not rerun because no shared Manager, condition, or breakpoint production code changed. No physical hardware was used.

After the hosted gates completed, a section-extraction command accidentally invoked GNU `objcopy --dump-section` without a separate output ELF, which rewrote some formal artifact files. This was an evidence-tooling error after successful sessions, not a hosted/product failure. The altered short-run artifacts were rebuilt with the recorded Clang 22.1.8 recipe, staged v0.5 SDK, and exact compilation-directory casing until their SHA-256 values matched the original hosted manifests; the restored files and replacement hashes were reverified. The short-02 and long-01 restores are in `post-audit-artifact-restoration.json`; short-03 through short-06 restores are in `short03-06-artifact-restoration.json`. No hosted session was repeated, no trace/console was replaced, and section hashes above were then computed by a read-only ELF reader.

**Final continuation outcome: B.** The stale fixture was repaired, both hosted 5/5 gates and the 25/25 hosted stress passed, and Debug/Release CTest passed 33/33. The positive GXSM lifecycle's final hosted Server shutdown check failed with `0xC0000005`, so Phase 30B remains open and Phase 30C must not begin. The original 413-DIE artifact remains unavailable; the prior one-boot QEMU evidence was reused under the explicit unchanged-package exception.

## Phase 30B shutdown lifetime continuation — 2026-10-16

This continuation supersedes the previous shutdown blocker description above while preserving every earlier parser, fixture, hosted gate, stress, CTest, package, and QEMU record.

### Stale prompt gate and starting repositories

No authoritative .phase marker exists in either repository. The latest accepted state in this report was Phase 30B Outcome B, with the failed GXSM host-shutdown check as its blocker. Phase 30C was not underway.

| Repository | Branch / starting HEAD | Starting worktree |
|---|---|---|
| Standalone Developer Studio | main / 34f1e7422712fa35328bcf9da9cbf5ad54237843 | clean |
| Server integration | v0.5_DEVELOPER_STUDIO / 84cc3c781d59bf837a2d0fd92ef3b5615de6a648 | clean |

No branch switching, creation, reset, merge, rebase, cherry-pick, stash, cleanup, remote, or credential changes were made. The only standalone edits for this continuation are the smoke/test helpers and this report. The Server edits are the hosted shutdown lifetime fix and its focused test harnesses.

### Original failed GXSM smoke identity and boundary

The original retained failure is under C:\Users\guideX\AppData\Local\Temp\phase30b-continuation-20261013-01\gxsm-positive-run.

- Fixture root: C:\Users\guideX\AppData\Local\Temp\phase30b-debugger-phase29q-positive-run-20261013. It came from the checked-in debugger-phase29q-positive fixture; the generated fixture identity and files are in console.log and manifest.json.
- Server executable: C:\Users\guideX\AppData\Local\Temp\phase30b-repair-20261012-6f19c2\server-stage\guideXOSServer.experimental.exe, SHA-256 862A1005BDD847D144B007B5A94485CD8183AD94522A7F0737627F6AB6225C90.
- Server PID 26616; wrapper PID 792. The smoke recorded SERVER_PROCESS_CLEANUP with wrapper exit -1073741819, no forced termination, and the Server process reaped. The original run did not retain an independent Server process handle or native exception record.
- The original runner did not retain its complete top-level argv. The exact one-iteration positive-exit gate invocation is preserved by session-command.json for the separate native-capture reproduction. It was: powershell.exe -NoProfile -ExecutionPolicy Bypass -File D:\dev\guideXOS_Developer_Studio\tests\smoke-developer-studio-debugger-positive-exit-gate.ps1 -ServerRoot C:\Users\guideX\AppData\Local\Temp\phase30b-repair-20261012-6f19c2\server-stage -FixtureRoot D:\dev\guideXOS_Developer_Studio\tests\fixtures\debugger-phase29q-positive -Iterations 1 -StartIteration 1 -ExpectedTargetExitCode 0 -MaxRuntimeSeconds 240 -DebugWaitSeconds 120 -TraceDirectory C:\Users\guideX\AppData\Local\Temp\phase30b-gxsm-shutdown-5of5-20261013-01\session-001\trace. This is the captured reproduction command, not a claim that the first run's argv was separately preserved.
- In the original console, genuine GXSM v2 metadata was accepted; live counter=2 and expression counter == 2 evaluated true; watch invalidation and relaunch were observed. Target process 12/runtime 2/session generation 1 and process 13/runtime 3/session generation 2 each emitted TARGET_EXIT_NORMAL code=0 and debugger_teardown=PASS. The final debugger stop was session generation 2, process 13/runtime 3, thread 31428.
- The last normal host marker was “guideXOSServer server exiting.” The first missing completion proof was a clean Server process exit with code 0. The console records -1073741819 (0xC0000005), without forced termination. The retained hosted shutdown trace says lastShutdownStage=none and serverExitCode=unknown because those direct host fields were not available to that original trace writer.
- The original console is the retained stdout/test transcript; no separate original Server stderr file was retained. The original failure directory has no dump or first-chance exception record.

The target process exits are distinct from host Server exit. The original wrapper-owned PID 26616 is the process that returned the AV status; the later native-debug capture independently confirms that the faulting process was the Server executable, not Developer Studio, the target ELF, or the gate runner.

### Native exception capture and faulting resource

The bounded, separately identified reproduction is GXSMSHUTDOWNGATE-20261013-01, under C:\Users\guideX\AppData\Local\Temp\phase30b-gxsm-shutdown-5of5-20261013-01\session-001. It used the same pre-fix Server SHA-256 862A1005BDD847D144B007B5A94485CD8183AD94522A7F0737627F6AB6225C90. The native monitor opened the Server process directly as PID 4892; its parent wrapper was PID 32400. This reproduction captured server-access-violation.dmp (635,535,639 bytes), plus native-debug-monitor.log and gate.stdout.log. The original GXSM failure remains recorded as failed; this reproduction is separate evidence and does not reclassify it.

- Fault thread: Windows thread ID 22440, a detached ProcessTable worker executing the still-running Developer Studio native-app runtime (runtimeId 1, internal process 11). It was not either already-exited debugger target worker.
- Exception: 0xC0000005, first chance then second chance, read access (ExceptionInformation[0]=0), exception record memory address 0xFFFFFFFFFFFFFFFF (ExceptionInformation[1]), instruction pointer 0x7FF752C40BAE, Server module RVA 0x390BAE. Server image base in that run was 0x7FF7528B0000. The faulting module was guideXOSServer.experimental.exe.
- The captured x64 context has RIP 0x7FF752C40BAE, RSP 0x3D64DF91D0, RBP 0x3D64DF98C0, and RBX 0xFEEEFEEEFEEEFEEE. Disassembly at the instruction pointer is a compare loading [RBX+0x10] inside libstdc++ unordered-map find specialized for string keys and shared_ptr<gxos::ipc::Channel>. The exception record reports 0xFFFFFFFFFFFFFFFF while the saved RBX poison pattern gives a different effective address for that load; both values are recorded rather than silently reconciled.
- Raw captured native stack for TID 22440 begins at Server RVA 0x390BAE in std::_Hashtable<...shared_ptr<gxos::ipc::Channel>...>::find. Reliable symbol-address frames include Server RVA 0x136E61 in Bus::ensure and RVA 0x137034 in Bus::get, with libwinpthread frames. Unwind entries at RVA 0x4C2460 land in .bss, not executable code, so part of the native unwind is corrupt and is not presented as a callable stack frame. Source inspection shows the hostPollEvent caller polls gui.output through Bus::pop -> Bus::get -> Bus::ensure; that caller chain is established from source/function identity rather than a complete raw unwind. Other captured Server threads were in ntdll wait states. The unmodified complete raw stack is retained in native-debug-monitor.log.
- The relevant object is the static Bus::g unordered_map in ipc_bus.cpp, whose values are shared_ptr<Channel>. The remaining worker was executing the app event loop and called hostPollEvent -> Bus::pop -> Bus::get -> Bus::ensure, which queries that map. Windows heap fill RBX=FEEEFEEE is consistent with a freed node/reference. The event record, instruction, map source, worker identity, and static teardown timing together establish a worker-after-owner lifetime fault. The register/effective-address mismatch above limits claiming the exact effective operand address from the context alone; the exception record's reported invalid address remains 0xFFFFFFFFFFFFFFFF.
- No evidence links this fault to GXSM metadata, debugger frames, watch evaluation, the target session, the vectored exception handler, or a callback firing after debugger teardown. The watch and target teardown had completed before host shutdown.

### Phase 29R/29S comparison

Phase 29R recorded the same visible ordering and status: code-0 target exits, debugger teardown, normal Server shutdown output, then host status 0xC0000005. It had no first-chance record, fault IP, access type/address, stack, or dump. Phase 29S reported non-reproduction in later clean runs and explicitly left detached ProcessTable workers and the once-registered vectored handler as unproven source-level lifetime candidates; its GDB attempts that intercepted normal target SIGTRAP were excluded.

This Phase 30B capture establishes the cause for the captured Phase 30B Server failure. It supports the detached-worker concern raised in 29S, but it does not retroactively prove that the separate Phase 29R event had this exact thread, object, or instruction. The process-wide exception handler was not changed.

### Actual pre-fix shutdown sequence

The checked-out Server code at starting HEAD 84cc3c781d59bf837a2d0fd92ef3b5615de6a648 performed this order:

1. The debugger target runtimes exited normally and their debugger sessions tore down. The Developer Studio UI native-app runtime remained open and continued its event-poll worker.
2. Server command processing called DevelopmentRunService::Shutdown, then Lifecycle::shutdown.
3. Lifecycle set ShuttingDown, sent a polite stop request to the compositor, waited up to 2 seconds, then called ProcessTable::terminate on its entry. The console path only logged that stop was shadowed and cleared its PID; it did not stop the console worker.
4. Scheduler::shutdown stopped its owned scheduler threads. Lifecycle set allocatorReady false and phase Stopped.
5. main logged “guideXOSServer server exiting.” and returned. A ShutdownGuard also invoked the shutdown functions during function exit.
6. ProcessTable::spawn had detached the per-process std::thread. No lifecycle step requested close to every running native app or waited for the Developer Studio worker thread.
7. Process-wide static/global objects, including Bus::g, were then destroyed while that app worker could still enter Bus::get/pop from hostPollEvent. The native capture faulted in the map lookup during this boundary.

The debugger target's successful exit and teardown did not mean that the hosting Developer Studio native-app process had exited. That distinction explains why the GUI/IDE event worker, rather than the debugger target, was still active.

### Repair and focused regression

The Server repair is restricted to hosted process/lifecycle ownership:

- ProcessTable now exposes waitForThreadExit alongside process-entry completion. The Windows/MinGW path stores a duplicated native Windows thread handle obtained through pthread_gethandle; ProcessTable::waitForThreadExit waits on that handle. The detached worker's “finished” flag alone is not treated as proof that its native thread has terminated.
- Lifecycle::shutdown first enumerates Running/Closing native apps, queries their process-table ownership, sends MT_Close to each owned window, waits for the process entry to finish, then waits for its native worker thread to exit. It only then erases/reaps the process-table entry.
- It next sends the console exit message and waits/reaps the console, stops and waits for the compositor, and shuts down the scheduler last. Bus/static destruction follows main return only after these workers complete.
- A 10-second wait is logged as the bounded shutdown budget. If it expires, the code retains shared resources and waits for completion before destruction; it returns failure status after safe completion rather than bypassing cleanup.
- server.cpp returns 0 only when Lifecycle::shutdown reports success. No sleep, AV suppression, exception-handler removal, early process termination, or weakened teardown assertion was introduced.

The focused host-thread regression uses the same MinGW pthread native-handle conversion as production. It holds a detached worker behind an event, asserts the duplicated Windows thread handle is not signaled before release, then releases it and requires the handle to signal. Result: PROCESS_THREAD_HANDLE_TEST=PASS backend=mingw_pthread native_handle=windows_handle detached_worker_exit=observed. Server native debugger runtime host test also passed. The exact test executable and build outputs are retained under phase30b-shutdown-repair-20261013-01\server-tests and native-thread-handle-regression.

The ordinary metadata-free Phase 15 regression passed after the repair: Server PID 30652, direct handle exit 0, no AV, no remaining Server; real breakpoint, variables, Step Into, Step Over, Step Out, Continue, target exit, debugger teardown, and close all passed. Evidence: C:\Users\guideX\AppData\Local\Temp\phase30b-shutdown-repair-20261013-01\ordinary-phase15-regression-02.

A separate dedicated invocation of run-native-app-shutdown-lifecycle-regression.ps1 used token NATIVEAPPSHUTDOWN-20261016-01 and evidence root C:\Users\guideX\AppData\Local\Temp\phase30b-nativeapp-shutdown-regression-20261016-01. It started repaired Server PID 9140 and the monitor recorded direct exit 0, no AV, monitor PASS, and no Server remaining. The smoke itself failed before project open because the Desktop launch success marker/window ownership was not observed; it emitted no target-exit or debugger-teardown evidence. An unrelated QEMU acceptance process (PID 10488, in D:\dev\guideXOSServer_DiskManagerImprovements) was active during this hosted attempt. This run therefore provides only clean host-exit evidence; whether the concurrent GUI load contributed to the missing launch marker is unknown. The attempt is retained as failed and was not retried. The separate successful ordinary regression and the five-session GXSM gate below remain distinct evidence.

### Fresh GXSM gate and mixed shutdown stress

The repaired Server image is C:\Users\guideX\AppData\Local\Temp\phase30b-shutdown-repair-20261013-01\server-stage\guideXOSServer.experimental.exe, SHA-256 DAC0D5A2E3752F7E691A4864D26F3A3003D878C1735F20EFE55CCB962F14E7E6.

The fresh positive GXSM gate token GXSMSHUTDOWNGATE-20261013-REPAIR02 passed 5/5 under C:\Users\guideX\AppData\Local\Temp\phase30b-gxsm-shutdown-5of5-repair-20261013-02. The five direct Server PIDs were 27876, 34584, 18848, 30164, and 35360. Each Server handle reported exit_code=0x0, the monitor reported PASS with zero access-violation events, each matching Server process was reaped, and each gate child exited 0. Every session used a fresh fixture and independently proved valid GXSM v2 metadata, live counter=2, true counter == 2 evaluation, Step Over/Out, watch invalidation, two clean target exits, and debugger teardown. The original failed smoke is not counted or relabeled.

Because this repair changes shared host process/worker shutdown, the predetermined mixed hosted stress schedule alternated ordinary Phase 15 and positive GXSM sessions. Its run token is HOSTSHUTDOWNSTRESS-20261013-03; evidence root is C:\Users\guideX\AppData\Local\Temp\phase30b-host-shutdown-stress-25-20261013-03. Cycles 1–18 were adjudicated PASS: 9 ordinary and 9 GXSM, with unique Server PIDs, fresh per-cycle evidence, direct Server exit 0, zero AVs, monitor completion, and no matching Server left running. Cycle 1's original summary had false-negative checks; its retained independent reanalysis verifies smoke terminal PASS, direct handle exit 0, completed monitor/no AV, and no process remaining. Cycles 2–18 each have their own passing cycle result.

Cycle 19 was the first current failure. It was an ordinary Phase 15 session, token HOSTSHUTDOWNSTRESS-20261013-03-019, Server PID 26260, wrapper PID 11176. Its smoke output stopped after the Step Into marker and contains no terminal PASS or complete Server cleanup assertion. The native monitor recorded handle exit_code=0x40010004, event_code=<no-exit-event>, access_violation_captured=false, and monitor return code 0; the monitor did not capture an AV or an EXIT_PROCESS debug event. The stress runner stopped before writing a cycle-result entry; stress-progress.json therefore remains at completed=18/status=IN_PROGRESS. The existing logs are preserved. The termination source is not established, so this cycle is a gate failure, not evidence that the historical 0xC0000005 recurred. No retry was made and cycles 20–25 were not run. The required mixed stress result is 18/25 PASS, gate incomplete/failed at cycle 19.

### Preserved Phase 30B results, QEMU, and packages

- The earlier short-root hosted gate remains 5/5 PASS.
- The earlier long-root hosted gate remains 5/5 PASS.
- The earlier hosted path/artifact stress remains 25/25 PASS. These gates were not rerun because the Server lifecycle repair did not change parser, path, fixture, or source-map code.
- Debug CTest remains 33/33 PASS and Release CTest remains 33/33 PASS, including developer_studio_symbol_test. No standalone production parser source changed in this continuation; no parser capacity or malformed-input result was reopened.
- Protected DWARF results remain unchanged: 421-DIE recovered B2AB Ready; line 37 has five mapped addresses; 511/512 accepted and 513 limit_exceeded; 159-character path accepted and 160/161 limit_exceeded; malformed DWARF remains malformed_dwarf; line 7 remains line_not_mapped. The historical 413-DIE artifact remains unavailable for fresh recheck.
- The earlier QEMU debugger evidence was one fresh guest boot, not five. A new required five-boot guest debugger regression was not started because QEMU PID 10488 was actively running an unrelated acceptance task in D:\dev\guideXOSServer_DiskManagerImprovements. No QEMU instance was stopped or overlapped. This guest gate remains outstanding and must be reconciled by a later safe continuation.
- No Developer Studio production parser/package code changed. The retained package hashes are AMD64 C5433205239735622C027A6EEE880E3B2F81340DFAC9304EE0A0A205D0ED78F4 and ARM64 EAA477DE53171F49638EB9A6012E88326CA006B3F196E70A4F0D3B87A5711048. No Server package artifact was changed.

### Continuation disposition

The Phase 30B access-violation root cause for the captured Server failure is classification A + F: the detached native-app worker outlived the Server's shared IPC map, and static/global destruction occurred while it was still polling the map. The Server production fix establishes close, completion, and worker-thread-exit ordering before host-resource teardown. The current mixed stress did not complete its required 25/25 gate, and the dedicated guest regression remains pending. The current outcome is therefore **B**; Phase 30B remains open and Phase 30C must not start.

The original failure mechanism is established for the native-captured Phase 30B reproduction. Phase 29R's historical event has the same external symptom but no retained native record proving that it had this exact cause. Remaining limitations are the uncompleted cycle 19 stress record/termination attribution, the pending fresh QEMU 5/5 guest gate, and the separate UI-start failure in the one-off focused lifecycle wrapper.
## Phase 30B continuation — 2026-10-17

This continuation began from standalone HEAD `2c62ce7934524aac6f5c255f17054f5187bf9e69` on `main` and Server HEAD `88f0a276923c97a0efd8b52e9b956f4f65ea2dc8` on `v0.5_DEVELOPER_STUDIO`; both worktrees were clean. No authoritative `.phase` marker was present. The preceding Outcome B remains open. The October 13 native-captured failure, repaired Server lifecycle, and original acceptance history above remain intact.

### Recovered failure and host shutdown result

The original GXSM smoke and native exception evidence are recorded in the preceding sections. The final successful debugger transition was target exit with code 0 and debugger teardown PASS; the target had already exited before the original host Server faulted with `-1073741819` (`0xC0000005`). The native capture attributes that fault to the Server PID 4892 in the separately identified reproduction, thread 22440, read AV at the unordered-map lookup in `Bus::ensure`/`Bus::get`, caused by the detached native-app event worker polling `Bus::g` across static destruction (classification A + F). Phase 29R/29S has the same reported external symptom but no retained native record establishing the same mechanism. The production repair and focused thread-handle regression are documented above; no additional production source was changed in this continuation.

The fresh mixed shutdown stress token `HOSTSHUTDOWNSTRESS-20261017-04` completed 25/25 cycles in 2,862.8 seconds: 13 ordinary Phase 15 and 12 positive GXSM sessions, alternating under the predetermined schedule. All 25 smoke children reported success, each Server process handle reported exit code `0x0`, every cycle had a distinct Server PID and evidence directory, and no matching Server remained. The Server image SHA-256 was `DAC0D5A2E3752F7E691A4864D26F3A3003D878C1735F20EFE55CCB962F14E7E6`. The cycle records are under `C:\Users\guideX\AppData\Local\Temp\phase30b-continuation-20261017-01\shutdown-stress-25-attempt-04`; summary: `stress-summary.json`. The direct-handle monitor fallback was used because debug attach returned Win32 error 87 or failed to complete in earlier instrumentation attempts. It recorded exact process-handle exit status and was validated against both a clean exit and a synthetic `0xC0000005`; direct mode does not provide first-chance exception events, so the stress evidence is a clean process-exit gate, not a per-cycle native AV trace. The earlier 18/25 attempt and its cycle-19 failure remain preserved as a separate failed run; this fresh schedule was not a retry of a failed cycle and stopped on the first failure rule.

### Fresh guest regression status

The outstanding guest requirement is still five fresh isolated Phase 28Q debugger boots using the unchanged AMD64 package SHA-256 `C5433205239735622C027A6EEE880E3B2F81340DFAC9304EE0A0A205D0ED78F4`. Attempt `QEMUGUEST30B-20261017-01` did not boot because its isolated MbedTLS subtree was incomplete. Attempt `QEMUGUEST30B-20261017-02` recorded two fresh passing guest boots, then aborted before crediting a gate when an unrelated QEMU process appeared; the Phase 30B QEMU process was stopped by verified PID and unique ESP path, and no retry was made within that attempt. Its result is retained at `C:\Users\guideX\AppData\Local\Temp\phase30b-continuation-20261017-01\qemu-guest-5of5-attempt-02\attempt-result.json`.

Attempt `QEMUGUEST30B-20261017-03` uses a guarded runner and a corrected isolated MbedTLS copy. At this checkpoint, the runner manifest records `started=false`; no Phase 30B boot has been launched. A waiter is holding for all unrelated QEMU and known acceptance runners to end, then requires 30 seconds of exclusive lane time before starting the five-boot gate. At the last process check, three unrelated QEMU processes and their acceptance runners remained active. No unrelated QEMU instance was stopped or overlapped. Attempt 03 is not a pass; the guest 5/5 requirement remains outstanding.

### Current disposition

The repaired host shutdown has now passed the fresh mixed 25/25 process-exit stress, superseding the earlier incomplete 18/25 stress result as the current stress-gate evidence while preserving that historical failure. Prior ordinary debugger shutdown, repaired GXSM 5/5, short-root 5/5, long-root 5/5, hosted artifact/path stress 25/25, Debug/Release CTest 33/33, and parser/DWARF results remain as recorded above; no unrelated accepted subsystem was rerun. Package hashes remain AMD64 `C5433205239735622C027A6EEE880E3B2F81340DFAC9304EE0A0A205D0ED78F4` and ARM64 `EAA477DE53171F49638EB9A6012E88326CA006B3F196E70A4F0D3B87A5711048`.

Phase 30B remains **Outcome B** until the outstanding fresh QEMU guest regression is completed or otherwise explicitly reconciled against the acceptance contract. Do not begin Phase 30C. No commit or push was made as of this checkpoint.

### Attempt 03 setup failure and gate accounting correction

The guarded waiter reached an exclusive lane after 965.7 seconds, but the guarded QEMU script failed before boot 1 when `Register-WmiEvent` for `Win32_ProcessStartTrace` returned `Access denied`. `attempt-result.json` records FAIL, zero boots, and no formal credit; no QEMU guest was launched by Phase 30B attempt 03. The waiter captured process exit code 0 despite its stdout printing `QEMU_GUEST_GATE=FAIL` and the failure artifact; its manifest had therefore incorrectly derived `status=PASS`. The retained manifest is corrected to `FAIL_SETUP`, preserves the observed `gate_exit_code=0`, and records the result-artifact contradiction explicitly. The gate is not passed. The cause of the wrapper/PowerShell exit-code discrepancy is not established; the next uniquely identified attempt will require a PASS `gate-result.json` with five accepted boots before the waiter may write PASS.

### Attempt 04 prepared after the runner setup failure

Attempt `QEMUGUEST30B-20261017-04` is a new uniquely identified run, not a continuation or credit claim for attempt 03. It retains the same isolated server-root and AMD64 package hash. Its guard uses a 100 ms `Win32_Process` snapshot during each one-boot bootstrap and stops only the Phase 30B child and its own QEMU process if an unrelated QEMU appears. The guarded script and waiter both passed PowerShell syntax parsing, and the process snapshot query succeeded. Unlike attempt 03, the waiter will report PASS only if the guarded runner exits 0 and writes a valid `gate-result.json` containing all five accepted boots. The run manifest initially records `started=false`; it waits for all active QEMU, C175, and TaskManager R13 runs to clear and for a further 30-second quiet interval. No guest result is claimed yet.
At 2026-10-17 17:31 Pacific time, attempt 04 still had `started=false`; one unrelated QEMU process and the C175 runner were active. The waiter remained alive within its 12-hour bound.

### Attempt 04 setup failure and attempt 05

Attempt `QEMUGUEST30B-20261017-04` reached its quiet lane after 622.1 seconds but failed before boot 1: the guarded Windows PowerShell invocation reported `Get-FileHash` unavailable. A separate same-executable probe reports Windows PowerShell 5.1 and resolves `Get-FileHash`; the difference in command availability inside the `-File` runner is unexplained. Its retained attempt artifact is FAIL with zero boots/no formal credit; the waiter correctly used the result artifact and recorded FAIL despite the captured child exit code 0. No guest was launched by attempt 04.

Attempt `QEMUGUEST30B-20261017-05` replaces only the guarded runner's hash preflight with .NET `SHA256`, retaining the package identity, boot checks, 100 ms process snapshot, and artifact-validated waiter. The native Windows PowerShell 5.1 helper preflight passed with the expected AMD64 package hash and a successful process snapshot query. Both scripts parse. Its manifest records `started=false` and the waiter is active; no boot or gate credit is recorded as of this update.

### Attempt 05 bootstrap failure and attempt 06

Attempt `QEMUGUEST30B-20261017-05` reached an exclusive lane after 276.3 seconds. The guarded runner validated the unchanged package and started one bootstrap child; that bootstrap compiled and staged the Phase 28Q fixture, then failed at its own `Get-FileHash` calls. No serial or host-trace log was produced and no QEMU process was launched, so there is no guest boot result. `attempt-result.json` records FAIL with zero completed boots and no formal credit; the artifact-validating waiter likewise recorded FAIL. A separate native Windows PowerShell 5.1 probe successfully resolves `Get-FileHash` after importing both existing project helper modules, so the cmdlet-visibility failure inside the full bootstrap remains unexplained. The staged directory and all logs are retained.

Attempt `QEMUGUEST30B-20261017-06` uses a new wrapper that explicitly imports `Microsoft.PowerShell.Utility` and the two existing helper modules before invoking the unchanged bootstrap; a `-PreflightOnly` invocation under the same Windows PowerShell 5.1 executable passed and verified the expected AMD64 package hash. The runner and waiter parse, and the process-snapshot guard is retained. At the latest check, the attempt 06 waiter was active with `started=false`; one unrelated QEMU process remained, so no Phase 30B guest had launched and no credit was claimed.

## Phase 30B guest-gate continuation — 2026-10-17

This status update preserves the historical GXSM shutdown fault capture and repair described above. No Phase 30C work has begun. No authoritative `.phase` marker exists in either repository. At the start of this continuation the standalone `main` HEAD was `2503490b555dd28580331916a840bc3111386d07` with the phase report already modified by the prior continuation; the Server `v0.5_DEVELOPER_STUDIO` HEAD was `88f0a276923c97a0efd8b52e9b956f4f65ea2dc8` and clean. No production code changed in this continuation.

### Corrections to attempts 06 and 07

The earlier attempt-06 paragraph records its pending state before the waiter ran. Attempt `QEMUGUEST30B-20261017-06` subsequently reached the lane, compiled/staged the Phase 28Q fixture, and then failed inside the unchanged bootstrap because `Get-FileHash` was reported as an unrecognized command even though the wrapper explicitly imported `Microsoft.PowerShell.Utility`. The guarded-runner result and waiter both record FAIL, zero accepted boots, and no formal credit; the captured outer gate exit was 0 while the result artifact says FAIL. No serial or host-trace evidence proving a guest boot was produced. The reason that this bootstrap runspace could not resolve the cmdlet remains unexplained. Its manifest and logs are preserved under `C:\Users\guideX\AppData\Local\Temp\phase30b-continuation-20261017-01\qemu-guest-5of5-attempt-06`.

Attempt `QEMUGUEST30B-20261017-07` used a run-specific bootstrap copy with the four SHA-256 calls routed through an equivalent .NET SHA-256 helper. The package preflight remained the original AMD64 package hash, and the patched bootstrap and runners passed Windows PowerShell 5.1 parsing/preflight. After an 89.9-second lane wait plus 30 quiet seconds, an unrelated QEMU appeared while boot 1 was starting. The guarded run stopped only its own child and QEMU, then recorded FAIL, zero accepted boots, and no formal credit. The first runner did not record the foreign process identity, so the unrelated QEMU's owner is not established from that attempt. A subsequent process-table check found no Phase 30B or isolated-server QEMU process and no attempt-owned child still running. Attempt 07 is preserved at `C:\Users\guideX\AppData\Local\Temp\phase30b-continuation-20261017-01\qemu-guest-5of5-attempt-07`.

### Attempt 08 waiting for an exclusive QEMU lane

Attempt `QEMUGUEST30B-20261017-08` is separately identified. Its preflight passed; the staged AMD64 guest package remains SHA-256 `C5433205239735622C027A6EEE880E3B2F81340DFAC9304EE0A0A205D0ED78F4`. The bootstrap SHA-256 is `43DB0577A72B6A40BD74A844C7705A54F3CB54F7BFEA8DD29A92BFCB66A750E0`; it differs from the original bootstrap only by replacing the four hash-command calls with the equivalent .NET helper. The process-snapshot guard now records foreign and owned QEMU identities if overlap occurs, and the waiter requires 120 quiet seconds. Its runner filter also recognizes the active `C175ManagedNotesResize`/`C176ManagedNotesNavigation` spelling.

At 2026-10-17 18:05 Pacific time, attempt 08 remained `READY`, `started=false`, with no guest boot and no credit. The captured lane snapshot is `lane-snapshot-20261018T010529Z.json`, SHA-256 `1E24D6D786A7460BD015F0269AB2F6F895DCDD6CB12DA05CA9BD893F7F4214D9`. It showed unrelated visualizer and S19 QEMU instances plus an active C176 acceptance runner. No unrelated process was stopped. The waiter continues to hold the gate until an exclusive lane is available; this status is not a QEMU gate result.

The required guest debugger regression remains outstanding. The already-passed repaired-host GXSM 5/5 gate, mixed hosted shutdown 25/25, prior short-root and long-root hosted 5/5, hosted artifact/path stress 25/25, Debug/Release CTest 33/33, parser/DWARF qualification, and package hashes remain as recorded above. Phase 30B remains **Outcome B** until the fresh guest 5/5 gate is accepted; do not begin Phase 30C.

## Attempt 08 resolution and current Phase 30B disposition — 2026-10-17

This section supersedes the time-specific statement above that attempt 08 was still waiting. The earlier pending status and all prior failed/aborted attempt records remain part of the history.

### Stale-prompt gate and repository baseline

- Current phase at inspection: Phase 30B, still open. No Phase 30C work began.
- No authoritative `.phase` marker was present in either repository.
- Standalone Developer Studio: branch `main`, starting and ending HEAD `4e62295cd4e54cb5cc48d005118a5bc3e38993ec`; it was clean before this documentation update. It remains one commit ahead and zero behind `origin/main`.
- Server integration: branch `v0.5_DEVELOPER_STUDIO`, starting and ending HEAD `88f0a276923c97a0efd8b52e9b956f4f65ea2dc8`; it remains clean and matches its tracking branch.
- No branch, worktree, merge, rebase, reset, clean, stash, history rewrite, remote, or authentication change was made. No production source or guest package was changed.

### Attempt 08 waiter and QEMU-lane result

On the first live check, attempt `QEMUGUEST30B-20261017-08` was `READY`, `started=false`, with no boot credit. The existing waiter was PID 7280, created at 18:02:51 Pacific, and its command line matched this attempt's `wait-and-run-qemu-gate.ps1`. Its manifest specified a 12-hour maximum wait and 120 continuously quiet seconds. The earlier waiter PID 33892 was already recorded as superseded; no new waiter or runner was started during this continuation.

The same waiter observed the lane quiet for 120 seconds and launched the guarded gate after a total lane wait of 3,385.9 seconds. The gate began at 18:59:18 Pacific with runner PID 24696 and bootstrap child PID 35552. QEMU PID 19744 was launched for boot 1 at 18:59:35 Pacific. At 18:59:48, the in-gate process snapshot recorded foreign QEMU PID 7924, parent PID 31324, command line `qemu-system-x86_64.exe -version`. The parent process command line identified the C175/C176 support runner, with `C176ManagedNotesNavigation`; the same parent later launched QEMU PID 34724 from that C176 evidence tree. This identifies the observed overlap as foreign acceptance activity that began after attempt 08's quiet-lane admission. The guard recorded the foreign and owned QEMU identities, left the foreign process untouched, and stopped only owned QEMU PID 19744 and bootstrap child PID 35552. No boot 2 was launched.

At the final process check, attempt 08's waiter, gate runner, bootstrap child, and QEMU PID 19744 were absent. The C176-owned QEMU PID 34724 was still active and was left alone. There was no residual Phase 30B process or Phase 30B QEMU. The foreign QEMU `-version` process was transient and absent; its C176 parent and subsequent QEMU process provided the ownership evidence above.

### Boot 1 evidence and failure classification

The source and staged Developer Studio package both had size 1,128,092 bytes and SHA-256 `C5433205239735622C027A6EEE880E3B2F81340DFAC9304EE0A0A205D0ED78F4`. The staged application was at `C:\Users\guideX\AppData\Local\Temp\guidexos-phase28g-bbbf1fa99c12473c8125df654ea77a89\esp-boot1-8760e76edf8941229ef3ca15efef95f9\Apps\DeveloperStudio\bin\amd64\developerstudio.elf`. Its stage ID was `8760e76edf8941229ef3ca15efef95f9`, staged image-tree SHA-256 was `92151942E1F7FB038A3A0340A1E2D99EF8DECD940709D54CA9BD893F7F4214D9`, kernel SHA-256 was `D513CED93B2DA0F0EDC1EBDB6828D2F499066DD520A31D651497CE0F749D4CCF`, and loader SHA-256 was `1A7A30345C692FCB1BAB8ED138B7E1F4E8CF24FA93304BD1A411899A7E00BBC6`. The staged project and config hashes were `26478FC36C20D87F8D057D69ECB38A44A34303240259DA4BED0DBD557FE7E331` and `5E66BF31978C1EAA78985B50D3EB71966D3AE7EA56BFD553340CE5798D5BEFF5`. The pre-spawn image audit and package hash matched; the exact AMD64 package was not rebuilt or replaced.

| Boot | Package SHA verified | DWARF Ready | Breakpoint binding/stop | Continue and cleanup | QEMU exit/reap | Result |
|---|---|---|---|---|---|---|
| 1 | PASS | Not reached | Not reached | Not reached | Guard stopped PID 19744; no clean exit code captured; PID later absent | FAIL — foreign QEMU overlap |
| 2 | — | — | — | — | — | Not run after boot 1 failure |
| 3 | — | — | — | — | — | Not run after boot 1 failure |
| 4 | — | — | — | — | — | Not run after boot 1 failure |
| 5 | — | — | — | — | — | Not run after boot 1 failure |

The host trace reached `P29J HOST 08 guest_loader_marker boot=1 marker=uefi_loader_entry`. Serial output reached the UEFI loader and kernel-loading messages but did not reach the first required guest marker, `P28Z BOOT 01 native_loader_entered`. The first failure domain was QEMU isolation; Developer Studio startup, DWARF parsing, source mapping, breakpoint handling, Continue, and application cleanup were not reached. The serial log is `C:\Users\guideX\AppData\Local\Temp\guidexos-phase28g-bbbf1fa99c12473c8125df654ea77a89\boot1.serial.log`; the host trace is `C:\Users\guideX\AppData\Local\Temp\guidexos-phase28g-bbbf1fa99c12473c8125df654ea77a89\boot1.host-trace.log`. Both, bootstrap stdout/stderr, guarded-gate stdout/stderr, overlap events, manifest, failed attempt result, and waiter log remain in their original evidence roots. This QEMU command did not configure debugcon or a QEMU debug-output file, so neither artifact exists for this boot.

The runner's `attempt-result.json` has an empty `boots` array and the manifest's `boot_count=0` describes zero completed boot records; raw `qemu_spawned` evidence proves one formal guest boot was launched. To preserve this distinction without rewriting the original runner artifacts, `attempt-08-boot-01-reconciliation.json` was added under the attempt 08 evidence root. It records the launched failed boot, process IDs, package and staged-image identities, foreign overlap, first missing marker, unavailable QEMU exit code, and hashes of the retained evidence files.

The Windows PowerShell 5.1.26100.9711 preflight passed in the native runner environment using the .NET `System.Security.Cryptography.SHA256` helper. The actual staged package was independently rehashed with the same expected value. The in-gate 100 ms process-snapshot guard recorded the foreign process identities and did not kill the foreign process. The wrapper/gate process reported exit code 0 even though it printed FAIL; the waiter correctly required the explicit result artifact, recorded `gate_artifact_status=FAIL`, left the final manifest at FAIL with `formal_credit=none`, and did not create a PASS `gate-result.json`. Thus attempt 08 is **0/5 accepted**, with **one launched failed guest attempt**. No retry or replacement boot was made after that failure.

### Full guest attempt history and preserved acceptance results

| Attempt | Observed disposition |
|---|---|
| 01 | Preboot setup stopped on an incomplete isolated MbedTLS subtree; no guest boot. |
| 02 | Two fresh guest boots passed their per-boot checks; a later unrelated QEMU overlap aborted the run before a five-boot gate result. No 5/5 credit. |
| 03 | `Register-WmiEvent` was denied before guest launch; zero guest boots. The contradictory wrapper status was corrected to FAIL_SETUP. |
| 04 | `Get-FileHash` was unavailable in the launched PowerShell bootstrap before guest launch; zero guest boots. |
| 05 | The inner bootstrap again failed on `Get-FileHash` after staging and before QEMU launch; zero guest boots. |
| 06 | `Get-FileHash` remained unavailable despite explicit module imports; no serial/host trace proving launch and zero accepted boots. |
| 07 | An unrelated QEMU appeared during boot-1 setup; the guard stopped the attempt. Its records contain no foreign identity and no serial/host trace proving a guest launch. No accepted boots. |
| 08 | The waiter completed its quiet-lane admission, then a guest QEMU launched. The guard detected C176's foreign QEMU `-version` process, stopped only Phase 30B-owned processes, and failed boot 1. One launched attempt; zero accepted boots; boots 2–5 were not run. |

The earlier passing evidence remains valid and was not rerun: short-root hosted 5/5; long-root hosted 5/5; hosted path/artifact stress 25/25; positive GXSM shutdown 5/5; ordinary debugger shutdown PASS; mixed shutdown stress 25/25 (13 ordinary debugger and 12 GXSM sessions); Debug CTest 33/33; Release CTest 33/33. The mixed stress used Server image SHA-256 `DAC0D5A2E3752F7E691A4864D26F3A3003D878C1735F20EFE55CCB962F14E7E6`. Package identities remain AMD64 `C5433205239735622C027A6EEE880E3B2F81340DFAC9304EE0A0A205D0ED78F4` and ARM64 `EAA477DE53171F49638EB9A6012E88326CA006B3F196E70A4F0D3B87A5711048`.

The older 18/25 mixed-stress failure and cycle-19 `0x40010004` event remain in the history. The fresh 25/25 stress supersedes that run for the current mixed-shutdown acceptance result, but neither it nor this guest attempt establishes who caused the historical `0x40010004` termination. The original `0xC0000005` worker-lifetime repair remains qualified; no contradictory new Server evidence appeared.

### Final disposition

Phase 30B is **Outcome B — remains open** because the required fresh QEMU guest debugger 5/5 gate did not pass. The only launched boot in attempt 08 failed the isolation invariant before Developer Studio startup; this result does not establish a new Developer Studio or Server production defect. The guest acceptance answer is **no, not yet**: the exact package was verified, but five consecutive isolated debugger boots were not completed. The mandatory stop-on-first-failure rule was followed, with no replacement boot. Hosted accepted gates remain retained. Phase 30C was not started.

At the end of the attempt-08 continuation, the standalone worktree contained only its requested documentation update; its branch/HEAD were unchanged and it was one commit ahead of `origin/main`. The Server worktree was clean and even with `origin/v0.5_DEVELOPER_STUDIO`. No commit or push had been made at that point; the coordination follow-up below records the subsequent evidence-only update.

## Cross-project QEMU coordination investigation — 2026-10-17

This follow-up completed the stale-prompt gate and read-only launcher investigation. Phase 30B remains Outcome B, no Phase 30C work or authoritative `.phase` marker exists, and attempt 08 is terminal (`FAIL`, `formal_credit=none`). No Phase 30B waiter or runner was active in the process census. The only matching process in one census was the inspection shell itself. The attempt-08 reconciliation remains unchanged: one launched boot, zero accepted boots, and no captured QEMU exit code or clean reap record.

### Live competing work and process ownership

At the 19:58:01 Pacific process snapshot, two independently owned guest QEMU processes were active:

| QEMU | Verified parent and workload | Evidence |
|---|---|---|
| PID 30464 | Parent PID 24956, PowerShell child of C176 orchestrator PID 26780. Its arguments point to `D:\dev\guideXOSServerV1.1_DOTNET_SUPPORT\out\dotnet\c176-managed-notes-navigation\c167\settings-ui-proof-runs\20261018-025442-0fedcc9f\boot2\serial.log`; this was a live guest boot in the C176 acceptance run. | Process creation time, parent chain, command line, and C176 evidence path were captured together. The same C176 process tree earlier had boot-1 QEMU PID 35272 active, then advanced to PID 30464 for boot 2. |
| PID 32012 | Parent PID 16660, Python process running `C:\gxR2\out\taskmanager-memorydetails-qualification-r2\run_matrix.py` for Task Manager qualification scenario F. QEMU arguments reference that run's scenario-F evidence directory and guest media. | Process creation time, parent command line, QEMU arguments, and evidence path were captured together. |

An earlier census also saw Task Manager scenario E QEMU PID 22772 and C176 boot-1 QEMU PID 35272; those processes had ended by the 19:58:01 snapshot. These changing live workloads show that a quiet census is temporary. No unrelated process was stopped, suspended, or queried through QMP.

### Launcher semantics and coordination

The C176 runner is `scripts/dotnet/run-c166-file-associations.ps1` in the Server V1.1 support checkout. Its `& $qemu -version` call populates a version string during capability preflight. That invocation has no guest machine, firmware, or drive configuration and exits after the version probe; it does not enter guest execution. The same runner later calls `Invoke-ProofQemuBoot` to launch actual guest boots. Thus attempt 08's foreign PID 7924 was a version probe associated with the C176 runner, not a C176 guest boot. It still remains a strict isolation conflict because the Phase 30B contract counts every foreign QEMU process.

The inspected launchers do not share a QEMU ownership coordinator, global lock, or scheduler/queue:

- C176 uses a process census scoped to QEMU command lines containing its own checkout path. It checks that project-scoped view before each guest boot and records/cleans up its own processes. This is process detection, not a global reservation, and it does not hold lane ownership across the full acceptance run.
- The Task Manager `run_matrix.py` launches QEMU with `subprocess.Popen` for its scenario and uses its own QMP/guest lifecycle. The inspected runner contains no shared-lane lock or cross-project reservation.
- Phase 30B attempt 08 waited until it saw no QEMU/C175/C176/Task Manager runners and required 120 quiet seconds, then used a 100 ms all-QEMU process snapshot while running. This detected overlap and stopped only its own guest, but it created no reservation honored by other launchers. The 120-second wait could not prevent a new launcher from starting immediately afterward.
- Other repository launch scripts use direct QEMU launches or project-local process checks; no common queue or cooperative QEMU lane lock was found in the inspected Developer Studio, Server V0.5, Server V1.1, or Task Manager launch paths.

A census establishes observed process state at one instant. It cannot establish ownership of the next instant. The existing project-scoped check and the Phase 30B private polling guard therefore do not satisfy the requirement for atomic, identifiable ownership held across all five boots and released at completion.

### External coordination blocker and disposition

The live C176 and Task Manager guest runs could launch QEMU independently, and neither participates in a shared atomic lane reservation. Exclusive ownership for the complete five-boot Phase 30B gate could not be established. The required result is **`EXTERNAL_QEMU_COORDINATION_BLOCKER`**. No new Phase 30B attempt, run token, boot, or package preflight was created; no acceptance requirement was weakened. Attempt 08 remains preserved as one launched failed boot and 0/5 accepted. Its version-probe overlap remains a guard failure under the unchanged strict policy, while the probe itself is not evidence of guest execution or a Developer Studio defect.

The next formal run requires a cooperative coordinator honored by every relevant QEMU launcher, with atomic acquisition, visible ownership, reservation retained through all five boots, safe stale-owner recovery, launch census and overlap monitoring, and recorded release. Until that shared contract exists, Phase 30B remains open. Hosted accepted baselines, including mixed Server shutdown 25/25, GXSM shutdown 5/5, short-root and long-root debugger 5/5, hosted artifact stress 25/25, and Debug/Release CTest 33/33, remain retained. Phase 30C was not started.

## Final continuation recheck — 2026-10-18

The mandatory stale-prompt gate was repeated before any test or guest launch. No authoritative `.phase` marker exists in either repository; the latest report still records Phase 30B Outcome B and Phase 30C not started. Attempt 08 remains terminal with one launched failed boot and 0/5 accepted. Its waiter PID 7280, superseded waiter PID 33892, gate PID 24696, bootstrap PID 35552, owned QEMU PID 19744, foreign probe PID 7924, parent PID 31324, and later C176 QEMU PID 34724 were all absent from the 2026-10-18 14:36:58 Pacific process census. This confirms current absence only; it does not add the missing historical QEMU exit-code/reap evidence or change attempt 08's classification.

The live repository state had advanced beyond the continuation prompt's expected Studio starting HEAD: standalone Studio was clean on `main` at `1555d8c9d382f64f67dd35039ac86be03d5ae96e`, synchronized with `origin/main` (0 ahead/0 behind), rather than the prompt's `4e62295cd4e54cb5cc48d005118a5bc3e38993ec`. Server remained clean on `v0.5_DEVELOPER_STUDIO` at the expected `88f0a276923c97a0efd8b52e9b956f4f65ea2dc8`, synchronized with origin (0/0). No branch or worktree changes were made.

The launcher source was rechecked read-only. In Server V1.1, `scripts/dotnet/run-c166-file-associations.ps1` calls `& $qemu -version` to populate the QEMU capability/version field; that invocation has no guest arguments and is not a guest launch. The same runner separately uses `Invoke-ProofQemuBoot` for guest boots. Task Manager's `run_matrix.py` starts QEMU directly with `subprocess.Popen`. Neither participates in a shared atomic lane reservation. At the census, no QEMU, Phase 30B runner/waiter, C176 runner, or Task Manager acceptance runner was present. An active `ctest --test-dir build --output-on-failure` process had only `guideXOS_SqlDatabase/build/database_sql7_test.exe` as its child and no QEMU child.

An empty census is still only a point-in-time observation. Because the independent launchers can start QEMU without acquiring a common reservation, no reliable exclusive window for the whole five-boot gate was established. The strict all-foreign-QEMU guard remains unchanged, including for version probes. This continuation created no new run token or evidence root, performed no package preflight, launched no guest boot, modified no other project, and earned no guest credit. The result remains **`EXTERNAL_QEMU_COORDINATION_BLOCKER` / Phase 30B Outcome B, 0/5 guest boots accepted**. All attempt-08 evidence and hosted accepted baselines above remain retained; Phase 30C was not started.

## Live lane recheck — 2026-10-18 15:41 Pacific

Before making this documentation update, the stale-prompt gate was repeated. No authoritative `.phase` file or Phase 30C artifact was present in either the standalone Developer Studio or Server V0.5 repository. The report and attempt-08 artifacts still show Phase 30B Outcome B and attempt 08 terminal with zero accepted boots. The attempt-08 reconciliation remains at `C:\Users\guideX\AppData\Local\Temp\phase30b-continuation-20261017-01\qemu-guest-5of5-attempt-08\attempt-08-boot-01-reconciliation.json`; it records one launched boot, `FAIL_QEMU_ISOLATION`, no QEMU exit code, and no clean reap record. The corresponding attempt result is FAIL with `formal_credit=none`; the manifest is FAIL and no PASS gate-result artifact exists. The historical attempt's waiter, gate, bootstrap, owned QEMU, foreign probe, and probe-parent PIDs were absent from the 15:41:41 Pacific census. This confirms current absence only and does not add historical exit/reap evidence.

The live standalone checkout was clean on `main` at `d87773018439220462a64442ab65086ed4e49383`, one commit ahead of `origin/main`; this is newer than the prompt's expected `4e62295cd4e54cb5cc48d005118a5bc3e38993ec` and the prior recheck's recorded `1555d8c9d382f64f67dd35039ac86be03d5ae96e`. No branch or worktree operation was made. Server V0.5 was clean on `v0.5_DEVELOPER_STUDIO` at the expected `88f0a276923c97a0efd8b52e9b956f4f65ea2dc8`, even with `origin/v0.5_DEVELOPER_STUDIO`. No Phase 30B waiter or runner was active.

Three foreign QEMU processes were active in the same census:

| PID | Verified ownership evidence | Workload |
|---|---|---|
| 29712 | Created 15:36:48; parent PID 14036 is `powershell.exe` running Server V1.1 `scripts/dotnet/run-c166-file-associations.ps1`; its parent PID 11680 is the invoking PowerShell process. The command includes `-C176ManagedNotesNavigation`, C177, and C178 options. Its QEMU arguments reference the C178 proof tree `D:\dev\guideXOSServerV1.1_DOTNET_SUPPORT\out\dotnet\c178-managed-calculator-resize\settings-ui-proof-runs\20261018-223231-5cb117de\boot2`. | Live guest boot in the combined C176–C178 acceptance run. |
| 20384 | Created 15:36:38; parent PID 33640 is Python running `C:\gxR3work\out\taskmanager-memorydetails-qualification-r3\run_matrix.py` with `--scenario E`. | Task Manager production-postfix scenario E, under `production-postfix-20261018\E`. |
| 1872 | Created 15:10:51; its QEMU command line names the Task Manager evidence directory `C:\gxR3work\out\taskmanager-memorydetails-qualification-r3\allocator-locator-interlifetime-gap-20261018`. Parent PID 27080 was not present in the census, so no live parent attribution is claimed. | Task Manager allocator/locator inter-lifetime-gap guest run. |

The C176 runner source confirms that `& $qemu -version` is used only to populate the QEMU capability/version field; it has no guest machine, firmware, or drive arguments and does not enter guest execution. The same script separately launches real boots through `Invoke-ProofQemuBoot`. Its QEMU census is scoped to command lines containing that checkout's path, so it is detection and cleanup for its own project rather than global reservation. The active Task Manager runner starts QEMU with `subprocess.Popen`; the inspected path contains no shared reservation. Together with the prior cross-project source review, no common atomic lock, owner registry, or queue honored by these runners exists. The Phase 30B process census and in-gate poll remain detection only.

The three live foreign processes demonstrate that a reliable exclusive five-boot window is not established. No new Phase 30B attempt, token, evidence root, package preflight, staging audit, or guest boot was created. The prior attempt-08 package preflight remains its historical PASS using Windows PowerShell 5.1 and the .NET SHA-256 helper; the preflight was not repeated for a new run because the shared-lane ownership prerequisite failed first. Consequently there are no new guest boot 1–5 results, no new DWARF/source-map/breakpoint/Continue results, and no new QEMU exit/reap entries. Attempt 08 remains one launched failed boot and 0/5 accepted. The strict all-foreign-QEMU rule is unchanged. No other project's QEMU or runner was touched. Hosted accepted baselines remain retained without rerun. Final disposition remains **`EXTERNAL_QEMU_COORDINATION_BLOCKER` / Phase 30B Outcome B**; Phase 30C was not started.
