# guideXOS Developer Studio — Phase 29V DWARF Capacity and Truncation

**Outcome B.** The optional source-line address-cache truncation is repaired without increasing any capacity. Both real artifacts now parse as `Ready`, `error=none`, `truncated=0`; the Phase 15 line-37 source mapping returns five addresses and reverse-maps its primary address to `src/main.cpp:37`. The full hosted and QEMU acceptance gates remain unproven because a production package build against the specified Server SDK stops at an ABI-header mismatch. The earlier hosted diagnostic also stopped before debugger startup at “Manager snapshot unavailable.”

## Baseline and preservation

The stale-prompt gate found no authoritative `.phase` marker, no Phase 29V report, and no Phase 29V commit in the current Git history. The latest recorded phase is Phase 29U, which identifies the 413-DIE parser result as an independent boundary. The prompt was therefore current and work proceeded.

| Repository | Starting branch / HEAD | Starting worktree | Ahead / behind |
|---|---|---|---|
| Standalone | `main` / `a535704e96fd182e8f23ffc0a0dece5729a19218` | clean | 0 / 0 |
| Server | `v0.5_DEVELOPER_STUDIO` / `bf78d517b513b624f61bee4ae6d144377f6c8000` | existing user changes listed below | 0 / 0 |

The Server’s pre-existing `Apps/DeveloperStudio/app.json`, AMD64 ELF, and `ESP/Apps/DeveloperStudio/.phase28q-diagnostic` were not staged, reverted, overwritten, or committed. Their SHA-256 values were unchanged at closeout: `5793567C54ABF22423A8FCDE2F9B32E5DEEB73981E070707BA11E7D23F390401`, `106BD4D1C8827894AB6CE4850E5DC30E55E98A1BAA52BCA149027781B4081FE9`, and `967D29A9A500A3108D7331BA47A5E2053A7C2203B81A345BE990084F3F43698A` respectively. A diagnostic hosted launch added one window record to `desktop.json`; only that record was removed afterward, returning the file to its initially clean state. The Phase 15 smoke’s temporary fixture-config edits were also restored.

Phase 29O’s accepted stepping history remains protected: the recorded checkpoints are standalone `a4abb9c78584d2911ce87f5002080719cff1e14b` and Server `07a02f5c3f9247c313de1a97cb773e01087aa7f5`, with historical hosted 5/5, hosted stress 25/25, QEMU 10/10, and QEMU stress 25/25. No history was rewritten. Phase 29U’s boot-5 `invalid_project_root` trace remains recorded (SHA-256 `D5A707B786FB89FA2FFF483C7CC29F9ACB7E8E64F6A53FC481D12AB8A4F806BC`); its subsequent fresh QEMU repeat passed 5/5 (trace SHA-256 `77950A680635F663BFE774EECF250B843FD9E6B9D0C3F12B6C562843A3B0768F`). Neither Phase 29U result is erased or treated as proof that the intermittent failure cannot recur.

## Artifact audit

Both artifacts are ELF64, little-endian AMD64, DWARF 5, and contain two compile units. GNU `readelf` 2.46.0.20260210 completed `--debug-dump=info` on both files with exit code 0 and no malformed-input diagnostics. Its output included complete unit lengths, abbreviation references, DIEs, and attributes. The production parser also completed both line programs and indexed all DIEs. The `.comment` sections identify Clang 22.1.8 and LLD 22.1.8 (LLVM commit `ca7933e47d3a3451d81e72ac174dcb5aa28b59d1`) for both files.

| Artifact | ELF size / SHA-256 | `.debug_abbrev` offset / size | `.debug_info` offset / size | `.debug_line` offset / size | `.debug_str` offset / size | `.debug_line_str` size |
|---|---|---:|---:|---:|---:|---:|
| Canonical Phase 3B, 397 DIEs | 11,608 bytes / `37BABF03A9A1A8799EEE90E710FBFD4F72BA98F30C73B5C83393054CECBF7439` | `0x6d8` / 479 | `0x8b7` / 2,594 | `0x2260` / 1,022 | `0x1559` / 2,775 | 247 |
| Phase 15, 413 DIEs | 12,864 bytes / `1628251736D2BC9EC1FFE6BC4F0B4DB83C63E609778210918661B878E7AD7AE6` | `0x750` / 475 | `0x92b` / 2,702 | `0x2550` / 1,021 | `0x16a1` / 3,204 | 740 |

The build recipes use the same compiler/linker versions and freestanding C++11 target. They are not byte-for-byte identical: Phase 15 adds `-fno-omit-frame-pointer`; its recipe uses `-O0 -g`, while the canonical recipe’s debug-symbol configuration also applies a compilation-directory and prefix-map pair. The Phase 15 source intentionally adds `Point`, `Rectangle`, and `Node`, `debugLoop` / `debugCaller`, aggregate locals and a repeated breakpoint. This is legitimate fixture content, not a compiler-version change.

| DWARF tag | 397 | 413 | Delta |
|---|---:|---:|---:|
| `DW_TAG_array_type` | 1 | 3 | +2 |
| `DW_TAG_base_type` | 9 | 9 | 0 |
| `DW_TAG_compile_unit` | 2 | 2 | 0 |
| `DW_TAG_const_type` | 9 | 8 | -1 |
| `DW_TAG_enumeration_type` | 4 | 4 | 0 |
| `DW_TAG_enumerator` | 32 | 38 | +6 |
| `DW_TAG_formal_parameter` | 121 | 117 | -4 |
| `DW_TAG_lexical_block` | 6 | 4 | -2 |
| `DW_TAG_member` | 68 | 75 | +7 |
| `DW_TAG_namespace` | 1 | 1 | 0 |
| `DW_TAG_pointer_type` | 55 | 55 | 0 |
| `DW_TAG_structure_type` | 14 | 17 | +3 |
| `DW_TAG_subprogram` | 4 | 4 | 0 |
| `DW_TAG_subrange_type` | 1 | 3 | +2 |
| `DW_TAG_subroutine_type` | 30 | 30 | 0 |
| `DW_TAG_typedef` | 23 | 22 | -1 |
| `DW_TAG_variable` | 16 | 20 | +4 |
| `DW_TAG_volatile_type` | 1 | 1 | 0 |
| **Total** | **397** | **413** | **+16** |

Both artifacts have four indexed functions. The mapper reports 23 / 21 modeled function variables (locals and parameters), 4 / 5 source files, 191 / 180 line rows, and 6 / 6 external source records for the 397 / 413 inputs. The direct Phase 15 source table is `src/main.cpp`, `main.cpp`, `src/stdint.h`, `src/freestanding_memory.cpp`, and `freestanding_memory.cpp`; the canonical source table is `src/main.cpp`, `stdint.h`, `src/freestanding_memory.cpp`, and `__stddef_size_t.h`.

## First truncation and resource provenance

The historical first `truncated=true` in `src/developer_studio_debug_symbols.cpp:addLineAddress` was the per-line address cache. Its `DebugDwarfLineKey` held a `uint32_t addressCount` and `uint64_t addresses[8]`. At count 8 it attempted to store index 8, the ninth distinct address. Phase 15 first reached this at the tenth line-program row, `src/main.cpp:52`, address `0x20001237`; the final line contained 12 distinct addresses. The canonical fixture first hit it at row index 9, `src/main.cpp:9`, address `0x200011f7`; its most populated line contains 14 addresses. The parser continued after the cache filled.

This was a redundant, optional collection: every line-program address was already retained in the bounded line-row table and could be deduplicated from those rows when a source-line query ran. The 413 DIE count was only correlated with a richer fixture; it was not the resource that filled. The original direct parse of each exact artifact returned `Ready`, `error=none`, `truncated=1`. Therefore the reported hosted `malformed_dwarf` was not reproduced by this direct code path. No direct code path converted this optional cache overflow into `MalformedDwarf`; the exact source of the historical higher-level label remains unproven. The prior Phase 29U report’s `malformed_dwarf` / `truncated=1` observation is retained as a report, not silently replaced.

The separate bare-metal DIE ceiling is 512. At 397 DIEs it had 115 unused entries, which proves that “PASS, 397 DIEs” described the canonical artifact size, not an observed maximum. At 413, 99 entries remained. The first actual truncation was the address cache, already full at its eight-address bound.

`DebugDwarfMapper` owns fixed arrays embedded in the application’s static `g_debugMapper` in `src/main.cpp`; these records are static application data, not heap or application-stack allocations. The new `DebugDwarfLineKey` is 24 bytes (source index, line, primary address, and flags). Its fixed 1,024-entry table consumes 24,576 bytes. The line-row table remains 4,096 × 48 = 196,608 bytes; the DIE table remains 512 × 856 = 438,272 bytes. The whole bare-metal mapper is 833,040 bytes.

Before repair, the key record was 96 bytes and the same table used 1,024 × 96 = 98,304 bytes. The implied old mapper total was 906,768 bytes. The repair saves 73,728 bytes; it does not increase the line-key capacity, DIE capacity, or another parser array. Parsing now keeps each key’s statement/primary address and derives unique query addresses from retained rows. A too-small caller query buffer still receives `DebugDwarfError::Truncated`; exhausting a required bounded parser table reports `LimitExceeded` with a stable failure stage/reason.

## Repair and evidence

`DebugDwarfMapperMapSourceToAddresses` now derives a unique address list from line rows. `DebugDwarfLineKey` no longer has a per-line address array. Required abbreviation, CU, DIE, nesting-workspace, function, and variable capacity failures set `truncated=true`, `DebugDwarfError::LimitExceeded`, and a capacity-specific stage/reason. `DebugDwarfParseVariables` preserves an explicit capacity error and assigns `MalformedDwarf` only to an otherwise unclassified parse failure. Malformed encodings remain rejected.

The capacity runner compiles the bare-metal parser and passes both real artifacts. It reports the parser state, counts, bounded-memory sizes, source-table contents, and address mapping. For Phase 15, line 37 maps from normalized `src/main.cpp` to five addresses, primary address `0x200016ac`, and reverse-maps to `src/main.cpp:37`; the standalone mapper load uses project generation 1 / mapper generation 1. This proves source association and the production mapper’s breakpoint-address calculation; it is not a hosted breakpoint hit.

The runner synthesizes valid DWARF at 511 DIEs and at the exact 512-DIE limit; both load without truncation. At 513 DIEs it retains 512 and returns `limit_exceeded`, stage `dwarf_die_capacity`, reason `die_table_capacity_exceeded`. A synthetic unknown abbreviation is rejected as `malformed_dwarf`. Existing regressions continue to reject a physically short ELF, an invalid section length, a malformed line-program ULEB sequence, invalid ELF data, and unsupported DWARF/opcodes. These checks distinguish malformed input from a genuine fixed-store limit.

`DebugDwarfMapperLoad` recognizes the entire 413-DIE DWARF5 artifact; there is no unsupported tag/form result. The production model retains every DIE, even though only functions, scoped variables, source rows, and related types are needed by the debugger’s current source mapping, stepping, stack, and locals features. This change avoids a streaming redesign because the existing row table already contains the needed addresses.

| Check | Result |
|---|---|
| Canonical 397-DIE direct parse | `Ready`, `error=none`, `truncated=0`; 397 DIEs; max 14 line addresses; PASS |
| Phase 15 413-DIE direct parse | `Ready`, `error=none`, `truncated=0`; 413 DIEs; max 12 line addresses; PASS |
| Phase 15 line-37 mapper query | 5 addresses; primary `0x200016ac`; reverse source `src/main.cpp:37`; PASS |
| DIE boundary | 511 PASS; 512 PASS; 513 `limit_exceeded`; PASS |
| Malformed synthetic abbreviation | rejected as `malformed_dwarf`; PASS |
| Debug CTest | 33/33 PASS, serial |
| Release CTest | 33/33 PASS, serial |
| Existing Server package audit | PASS, read-only baseline audit; see hashes below |

The Phase 15 fixture has no authoritative GXSM metadata. Its explicit no-GXSM hosted watch-unavailable result was not reached in Phase 29V. The Phase 29Q positive GXSM end-to-end fixture was not rerun after the parser change; the CTest watch and variable models passed, but they are not a substitute for that fixture. Phase 29U’s historical 5/5 and 25/25 GXSM results remain valid evidence for that earlier code state only.

## Hosted, QEMU, and package gates

One diagnostic hosted Phase 15 run before the repair stopped while waiting for `debug_condition_editor=OPEN` and showed “Manager snapshot unavailable.” No parser-ready, breakpoint-hit, source-step, or Step Into marker appeared. Its shutdown trace is `logs/developer-studio-debugger-shutdown-trace.log` (SHA-256 `E3169C6CDB0933940A02B586054E3805EDEF7BD897F57C1D38A4FE12860DE40F`); captured host output is `C:\Users\guideX\AppData\Local\Temp\guidexos-phase15-16588.out` (SHA-256 `5E0CA9953614EA38DE1A0DC305FE86DB12511FE43AE75FC637726F2034A203C1`). It did not reproduce a malformed parse and was not counted as a post-repair acceptance iteration.

A production-package build was attempted with all outputs directed to `C:\Users\guideX\AppData\Local\Temp\guidexos-phase29v-package-audit`. The build reached `src/main.cpp` and failed because the specified Server SDK at `D:\dev\guideXOSServerV0.5_DEVELOPER_STUDIO\sdk\include` does not declare `gx_host_calls::get_document_activation_path`, referenced by the standalone source at `src/main.cpp:5359–5363`. No package was emitted. No Server source or protected package file was changed to mask this ABI mismatch.

The read-only package audit of the existing protected Server package passed. These hashes describe the pre-existing package and do not prove a Phase 29V rebuild:

| Architecture | Size | SHA-256 | Audit |
|---|---:|---|---|
| AMD64 | 1,101,516 bytes | `106BD4D1C8827894AB6CE4850E5DC30E55E98A1BAA52BCA149027781B4081FE9` | PASS, existing package |
| ARM64 | 1,260,292 bytes | `7F3C61A05A335B47C32B76C5F0769A6C6251F601171D4D01DA0737B926A40335` | PASS, existing package |

Because the rebuilt product package was unavailable, the required post-repair hosted 5/5 and 25/25 gates were not started, and the shared production DWARF change did not proceed to the required QEMU 10/10 full acceptance or 25/25 stress gates. Phase 29V has no new QEMU observation: `invalid_project_root` was neither reproduced nor cleared by a new run. Physical hardware was not used. Push and ahead/behind status are recorded in the final response; no Server commit was created.

The exact next boundary is a compatible production build input for the specified Server branch: its SDK header must match the optional Native ABI callback referenced by the standalone source, or the two repositories must otherwise establish their intended ABI version together. After producing and auditing that exact package, run one fresh hosted Phase 15 session first. If it again stops at “Manager snapshot unavailable,” the next fault domain is project-manager/session readiness before debugger startup; if it reaches the debugger, proceed through the required step/inspection sequence before starting the 5/5 gate.

## Final report checklist

| # | Prompt item | Phase 29V result |
|---:|---|---|
| 1 | Outcome | B |
| 2 | Stale/duplicate prompt | Current; no stale marker or completed Phase 29V work found |
| 3 | Phase marker | No authoritative `.phase` marker in either repository |
| 4 | Standalone starting HEAD | `a535704e96fd182e8f23ffc0a0dece5729a19218` on `main` |
| 5 | Standalone ending HEAD | One Phase 29V local commit; exact hash is in the final response |
| 6 | Server starting HEAD | `bf78d517b513b624f61bee4ae6d144377f6c8000` |
| 7 | Server ending HEAD | Unchanged; `bf78d517b513b624f61bee4ae6d144377f6c8000` |
| 8 | Commits | One standalone Phase 29V commit; no Server commit |
| 9 | Worktree preservation | Protected Server app manifest, AMD64 ELF, and diagnostic sentinel hashes unchanged |
| 10 | 413-DIE failure reproduced | `truncated=1` reproduced before repair; direct `malformed_dwarf` result not reproduced |
| 11 | 397 artifact | `tests/fixtures/debugger-phase3b/build/bin/amd64/debugger-phase3b.elf` |
| 12 | 397 semantic meaning | Canonical artifact DIE count, not a capacity |
| 13 | 413 artifact | `tests/fixtures/debugger-phase15/build/bin/amd64/debugger-phase15.elf` |
| 14 | 413 size / hash | 12,864 bytes / `1628251736D2BC9EC1FFE6BC4F0B4DB83C63E609778210918661B878E7AD7AE6` |
| 15 | Compiler | Clang 22.1.8, LLVM commit `ca7933e47d3a3451d81e72ac174dcb5aa28b59d1` |
| 16 | Linker | LLD 22.1.8, same toolchain build |
| 17 | DWARF version | DWARF 5 |
| 18 | `.debug_info` size | 2,594 bytes canonical; 2,702 bytes Phase 15 |
| 19 | `.debug_abbrev` size | 479 bytes canonical; 475 bytes Phase 15 |
| 20 | `.debug_line` size | 1,022 bytes canonical; 1,021 bytes Phase 15 |
| 21 | `.debug_str` size | 2,775 bytes canonical; 3,204 bytes Phase 15 |
| 22 | CU count | 2 each |
| 23 | DIE count | 397 and 413 |
| 24 | DIE-tag distribution | Full per-tag table above; total delta +16 |
| 25 | 397→413 delta | +16, explained by source fixture additions and tag changes above |
| 26 | First truncation site | Historical `addLineAddress`, Phase 15 row 9 / `src/main.cpp:52` |
| 27 | Owning bounded structure | Optional source-line address cache in `DebugDwarfLineKey` |
| 28 | Old capacity | 8 distinct addresses per source line |
| 29 | Old record size | 96 bytes |
| 30 | Old footprint | 1,024 × 96 = 98,304 bytes |
| 31 | Replacement | Store primary/statement address; derive unique query addresses from line rows |
| 32 | New footprint | 1,024 × 24 = 24,576 bytes; total mapper 833,040 bytes |
| 33 | Malformed vs capacity | Hard capacity is `LimitExceeded`; corrupt DWARF remains `MalformedDwarf` |
| 34 | Root cause | Redundant eight-address cache overflow, unrelated to 413 DIEs |
| 35 | Exact repair | Removed per-line address list; map addresses from retained line rows |
| 36 | Capacity tests | 511/512 pass; 513 returns `limit_exceeded` |
| 37 | Malformed regression | Synthetic bad abbreviation and existing invalid ELF/section/ULEB regressions pass |
| 38 | Unsupported constructs | None encountered; parser and GNU readelf accepted the artifact |
| 39 | Phase 15 parser | `Ready`, error none |
| 40 | `truncated` | 0 after repair |
| 41 | Source association | Five normalized source paths; ready |
| 42 | Breakpoint mapping / hit | Line 37 maps to five addresses, primary `0x200016ac`, reverse `src/main.cpp:37`; hosted hit not run |
| 43 | Step Into | Not run; hosted startup boundary prevented it |
| 44 | Step Over | Not run in Phase 15 hosted product route |
| 45 | Step Out | Not run in Phase 15 hosted product route |
| 46 | Stack | Not verified in Phase 15 hosted session |
| 47 | Locals | Parser modeled 21 variables; current-stop hosted refresh not verified |
| 48 | No-GXSM watch | Hosted unavailable result not reached |
| 49 | Positive GXSM regression | Not rerun; model CTests pass, but no end-to-end claim |
| 50 | Debug CTest | 33/33 PASS, serial |
| 51 | Release CTest | 33/33 PASS, serial |
| 52 | Canonical DWARF audit | 397 DIE parse PASS, `truncated=0` |
| 53 | Phase 15 DWARF audit | 413 DIE parse PASS, `truncated=0` |
| 54 | Package audit | Existing protected package audit PASS; rebuilt 29V package blocked |
| 55 | Hosted focused /5 | 0/5; not started after package-build gate |
| 56 | Hosted stress /25 | 0/25; prerequisite focused gate not reached |
| 57 | QEMU full /10 | Not run; no Phase 29V production package |
| 58 | QEMU stress /25 | Not run; prerequisite QEMU/package gates not reached |
| 59 | `invalid_project_root` | Historical Phase 29U boot-5 failure retained; fresh Phase 29U 5/5 repeat passed; no new Phase 29V QEMU run |
| 60 | AMD64 package hash | Existing package SHA-256 `106BD4D1C8827894AB6CE4850E5DC30E55E98A1BAA52BCA149027781B4081FE9` |
| 61 | ARM64 package hash | Existing package SHA-256 `7F3C61A05A335B47C32B76C5F0769A6C6251F601171D4D01DA0737B926A40335` |
| 62 | Physical hardware | Not used |
| 63 | Push | Normal push attempted after commit; result in final response |
| 64 | Ahead / behind | Standalone 1/0 if push is rejected; Server remains 0/0 |
| 65 | Final worktree | Standalone clean after commit; Server retains only its three pre-existing changes |
| 66 | Remaining boundary | Specified Server SDK lacks the ABI member needed to build the standalone package; hosted manager/session boundary remains untested after rebuild |
