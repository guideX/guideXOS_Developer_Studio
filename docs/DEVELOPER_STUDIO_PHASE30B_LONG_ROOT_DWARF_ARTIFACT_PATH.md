# Developer Studio Phase 30B — Long-Root DWARF Parsing and Artifact Path Determinism

**Outcome B.** The historical long-root issue is localized to a bounded DWARF name/path-capacity defect and explicit artifact-path-capacity classification was added. Parser, path, package, debugger, GXSM, and one-boot QEMU checks passed where run. The required hosted acceptance gate stopped on its first short-root cycle: symbols reached Ready, then a stale fixture breakpoint at line 7 failed remapping. The long-root 5/5 gate and 25-cycle stress were not started. Phase 30B is not accepted and Phase 30C must not start.

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
