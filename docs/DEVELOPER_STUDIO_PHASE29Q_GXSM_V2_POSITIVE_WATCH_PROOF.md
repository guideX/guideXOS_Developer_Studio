# Developer Studio Phase 29Q — GXSM v2 Positive Watch Proof

## Result

**Outcome A.** A real artifact from the guideXOS bootstrap compiler carried GXSM v2 variable records into the hosted debugger. `counter` resolved in the current `debugProbe` frame, its value was read from target memory, and the production watch evaluator returned `1` for `counter == 2`. Step Over read the changed value `3` and returned `0`; frame change and Step Out reported the local unavailable. Continue invalidated stopped state, and relaunch parsed and read the artifact again with a new process, session, and address.

The positive focused hosted gate passed **5/5**, the subsequent hosted stress gate passed **25/25**, and the metadata-free Phase 15 regression passed **5/5**. The bounded QEMU regression passed **5/5** fresh boots.

## Baseline and worktree

The Phase 29P paste lists standalone starting HEAD `b236243e0af5d22831af5edffa9fa45c4a40bb0a` and Server starting HEAD `28be097bb22ac31c13021c311a2e7fa11c46b135`. At this work's start, the observed standalone `main` HEAD was `18c093dcb9c4fc0daf5a019d82283aba0ac319a4`; that already-present commit added Developer Studio document activation. It was preserved. The Server branch started at the pasted HEAD. At inspection time both local branches matched their current remote-tracking refs; this differed from the paste's reported ahead counts. No fetch, branch switch, history rewrite, remote change, or authentication change was made.

The Phase 15 metadata-free ELF was produced by the existing Clang/LLD `-O0 -g` fixture build. That route emits DWARF but does not run the bootstrap compiler's GXSM trailer writer, so Phase 15 remains intentionally GXSM-free. Its earlier `metadata unavailable` result was category **K — fixture-only metadata omission**. Phase 29P separately repaired signed integer equality in the shared watch evaluator; that repair was left intact.

The positive fixture is in `tests/fixtures/debugger-phase29q-positive`. Its build recipe calls the Server's production bootstrap compiler path. The generator runs `compile_module_from_source`, compiler-object serialization and deserialization, `link_modules`, the production ELF writer and GXSM v2 writer, then validates the resulting ELF and resolves its emitted source records. It does not append or construct GXSM bytes. The fixture source and smoke contain no hard-coded runtime address, frame offset, type, or live range.

The compiler/writer overload accepts an explicit image base so this sectionless ELF can be loaded at `0x20000000` by the hosted test process. The host build service now validates the bootstrap ELF entry through the production ELF validator when a section table is absent. The native Win64 trampoline saves and restores R14/R15 around the foreign entry call.

## Build A artifact and GXSM identity

Build A is:

`D:\dev\guideXOS_Developer_Studio\tests\fixtures\debugger-phase29q-positive\build\bin\amd64\debugger-phase29q-positive.elf`

| Property | Value |
|---|---|
| Size | 5,021 bytes |
| SHA-256 | `80054B83736EE446F416530B95B0611B34C116D6E4B26B122B00190BD3641C46` |
| GXSM marker/header | Found and accepted |
| GXSM version | 2 |
| Trailer offset / size | 4,273 / 748 bytes |
| Trailer FNV-1a64 | `0x553853e1b71a2ea4` |
| Source files / functions | 1 / 2 |
| Source mappings / variables | 4 / 2 |
| Parser capacity | Accepted; limits are 16 files, 256 functions, 256 mappings, 576 variables |

The executable SHA-256 is the artifact and metadata identity. The trailer FNV-1a64 is its internal checksum, not a substitute for executable identity. `NativeAppDebugger` logged `present=1 valid=1 version=2 ... result=accepted`, then accepted and resolved both variable records. The compiler generator reported `counter` at record index **1**, derived from compiler debug information.

| `counter` record field | Compiler-emitted value |
|---|---|
| Name / index | `counter` / 1 |
| Type / kind | signed i32 / local |
| Owner / declaration | `debugProbe`, `src/main.cpp:3` |
| Live PC range | 25–64 |
| Location | RBP-relative, signed offset `-8` |

## Stop, target read, and watch result

At the authoritative line-4 breakpoint, the accepted Build A trace records project generation 1, build operation 3, session generation 2, target generation 1, process 13, native runtime 3, thread 41420, stop generation 1, frame 0, module and symbol generation 2, PC `0x20001019`, function `debugProbe`, and source `src/main.cpp:4`.

The production `InspectVariables` path reported RBP `0x4adf5fe770`. Applying the signed metadata offset gives:

`0x4adf5fe770 + (-8) = 0x4adf5fe768`

The backend read four target bytes at `0x4adf5fe768`: `02 00 00 00`. The signed decoded value was **2**. The variable result carried the same name and type (`counter`, `signed_i32`), current session/stop/thread/frame, function, PC, and live range. The Locals inspection published one local at this stop; its raw value agreed with the independent Watch evaluation below.

The hosted Watch request evaluated `counter == 2` against that same paused frame and stop. It returned status 1, accepted 1, error category 0, and integer value **1**. This is the real watch result; it was not inferred from Locals text.

Step Over produced a fresh stop (stop generation 7, PC `0x2000102e`, `debugProbe`). The target read returned `03 00 00 00`, signed value **3**, and the watch returned **0**. The test checks the raw target read as well as the expression result.

Selecting caller frame 1 made the watch unavailable with “identifier is not visible in selected frame.” Step Out returned to `gx_main` at `src/main.cpp:10`, stop generation 8, where the same explicit unavailable result was reported. No previous value was retained.

## Freshness, Build A → B, and lifecycle

Continue marks the stopped context, stack, Locals, and Watches stale before target exit. Model tests reject watch evaluation against a stale stop generation; the stopped-stack API also rejects a request with a mismatched stop generation. The debugger checks the expected executable SHA-256 on variable and expression requests. Artifact mismatch tests cover SHA-256, size, project generation, build operation, architecture, and path; a wrong artifact identity is rejected.

The real Build A → B sequence used the same positive project recipe in isolated fixture roots. Build B changed the unused parameter spelling from `seed` to `inputSeed`, leaving the target's `counter` behavior and successful return unchanged. Build B was produced and consumed by the same bootstrap compiler route:

`C:\Users\guideX\AppData\Local\Temp\phase29q-build-ab-b2-20261002\debugger-phase29q-positive\build\bin\amd64\debugger-phase29q-positive.elf`

| Property | Build A | Build B |
|---|---|---|
| Size | 5,021 bytes | 5,021 bytes |
| SHA-256 | `80054B83736EE446F416530B95B0611B34C116D6E4B26B122B00190BD3641C46` | `BFBCFD643C79C0DF050146FE05F31827660D79280DC6EB1DDB28E099A1F3DC4E` |
| GXSM checksum | `0x553853e1b71a2ea4` | `0xf095d6213537f91c` |
| Trailer / counts | v2, 4,273 / 748, 4 mappings / 2 variables | v2, 4,273 / 748, 4 mappings / 2 variables |

Build B's new hash and checksum were accepted by `NativeAppDebugger`; its `counter` record was resolved at `RBP-8`, returned the actual value 2, and its Watch returned 1. Step Over returned a target value of 3 and Watch result 0. Its relaunch also parsed Build B again and read the same logical variable at a distinct address under a new process and session. The model identity checks cover the old-build-versus-new-build rejection, while the hosted run proves the new artifact is selected and evaluated. This was not a hot rebuild while one process remained paused.

The hosted lifecycle gate covers breakpoint, scope, frame selection, Step Over, Step Out, Continue, exit, cleanup, and relaunch. Both positive target sessions exited with code 0 and released their runtime resources. Relaunch performed a new GXSM parse and target read; the runtime address changed.

## Metadata-free regression

The original Phase 15 ELF remained unchanged and built without GXSM. Five fresh hosted runs retained the line-42 breakpoint and Step Into, Step Over, and Step Out behavior. On initial and later stops, `counter == 2` returned an explicit “authoritative GXSM metadata is unavailable” result (present 0, valid 0, error category 13). It did not return false or reuse a value from a positive session.

The positive and Phase 15 smoke paths now save and restore their debugger JSON and `.bak` bytes. The Build B verification run confirmed those files' hashes were unchanged after the lifecycle smoke.

## ABI, validation, and QEMU

**ABI unchanged.** No native app ABI request, result, register, debug variable, or call-stack structure changed; no GXSM version was changed. The new ELF writer overload and Server parser state are internal implementation details. Existing ABI layout assertions remain unchanged.

| Gate | Result |
|---|---:|
| Debug CTest | 33/33 passed |
| Release CTest | 33/33 passed |
| DWARF capacity | Passed, 397 DIEs |
| Positive GXSM focused hosted gate | 5/5 passed (runs 30–34) |
| Positive GXSM hosted stress | 25/25 passed (runs 35–59) |
| Phase 15 no-GXSM regression | 5/5 passed (runs 11–15) |
| QEMU regression | 5/5 fresh Phase 28Q boots passed |
| AMD64 and ARM64 package audit | Passed |

The QEMU regression used `scripts/smoke-compiler-bootstrap.ps1 -Phase28QOnly -BootCount 5 -TimeoutSeconds 180`; each fresh image passed the guest startup, packaged Developer Studio, diagnostic sentinel, and image integrity checks. Phase 29Q did not change the shared guest watch evaluator/parser/runtime sources, so the bounded regression applied; full 10-boot and 25-boot watch stress were not triggered. Phase 29P's earlier 25-boot attempts that stopped before UEFI loader startup on boot 7 and at `invalid_project_root` on boot 13 remain separate startup/infrastructure failures; neither reached watch evaluation.

The package audit was read-only; the generated production package was not rebuilt.

| Package | Size | SHA-256 |
|---|---:|---|
| AMD64 `Apps/DeveloperStudio/bin/amd64/developerstudio.elf` | 1,101,516 bytes | `106BD4D1C8827894AB6CE4850E5DC30E55E98A1BAA52BCA149027781B4081FE9` |
| ARM64 `Apps/DeveloperStudio/bin/arm64/developerstudio.elf` | 1,260,292 bytes | `7F3C61A05A335B47C32B76C5F0769A6C6251F601171D4D01DA0737B926A40335` |

No physical hardware was used.

## Repairs and preserved diagnostics

Phase 29Q added the production compiler fixture generator and recipe; explicit image-base support for bootstrap linking/writing; sectionless bootstrap ELF entry validation in the build service; bounded GXSM v2 metadata diagnostics and accepted-record/raw-read traces; and safe inspection of the completed internal step trap after the step owner is removed. The watch evaluator continues to use the Phase 29P equality fix. No-metadata behavior remains explicit.

The smoke test's additional Build B experiments are retained outside the repository under `C:\Users\guideX\AppData\Local\Temp\phase29q-build-ab-20261002\logs` and `...\phase29q-build-ab-b2-20261002\logs`. Two invocations stopped at preflight because of the generic line-20 default and the temporary fixture directory name. An exploratory Build B that changed the target's return from 0 to 1 reached the lifecycle run but failed waiting for the expected clean `debug_state=EXITED` marker. That diagnostic trace was retained; it was excluded from the 5/5 and 25/25 gates. The successful Build B changed only the unused parameter name and completed the entire lifecycle. No acceptance-gate failure was discarded or retried.

The protected `.phase28q-diagnostic` sentinel remained at SHA-256 `967D29A9A500A3108D7331BA47A5E2053A7C2203B81A345BE990084F3F43698A`. Existing unrelated standalone changes, untracked validation artifacts, and generated Server package files were preserved and excluded from Phase 29Q staging.

## Fault classification

The old Phase 15 metadata absence remains category **K**, an intentional fixture-path difference. Phase 29P's `==` defect was category **H** and remains repaired. Phase 29Q proved a supported compiler path for positive GXSM v2, so there is no remaining GXSM emission, parser, identity, scope, location, target-read, watch, stop-ownership, lifecycle, or no-metadata blocker.
