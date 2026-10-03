# guideXOS Developer Studio — Phase 29W
## Server SDK Contract Alignment and Hosted Debugger Requalification

**Outcome B.** The production build mismatch was reconciled by appending an optional callback to the specified Server SDK, preserving all existing slots and using the existing table-size check for older providers. New AMD64 and ARM64 Developer Studio packages build and pass package audits. The rebuilt package completed one Phase 15 stepping lifecycle and one positive GXSM lifecycle. The focused hosted gate stopped at its first line-37 conditional run when the condition editor did not open and the UI reported “Manager snapshot unavailable.” The required QEMU full gate stopped at boot 1 on the pre-existing invalid_project_root path.

## Phase gate and worktree preservation

No authoritative .phase marker or Phase 29W report existed in either repository before changes. The history and live branch checks showed Phase 29V was next, so this prompt was current.

| Repository | Starting branch / HEAD | Starting ahead / behind | Starting state |
|---|---|---:|---|
| Standalone Developer Studio | main / dbb6eec81e3180f75996072da232605a61f476fc | 1 / 0 | Phase 29V commit; otherwise clean |
| Server integration | v0.5_DEVELOPER_STUDIO / bf78d517b513b624f61bee4ae6d144377f6c8000 | 0 / 0 | Three protected pre-existing changes |

The protected Server paths and their original SHA-256 values were:

| Protected path | Starting SHA-256 |
|---|---|
| Apps/DeveloperStudio/app.json | 5793567C54ABF22423A8FCDE2F9B32E5DEEB73981E070707BA11E7D23F390401 |
| Apps/DeveloperStudio/bin/amd64/developerstudio.elf | 106BD4D1C8827894AB6CE4850E5DC30E55E98A1BAA52BCA149027781B4081FE9 |
| ESP/Apps/DeveloperStudio/.phase28q-diagnostic | 967D29A9A500A3108D7331BA47A5E2053A7C2203B81A345BE990084F3F43698A |

All QEMU modifications and hosted package staging were performed in a detached temporary worktree at C:\Users\guideX\AppData\Local\Temp\guidexos-phase29w-server-stage-40eb15cbcbc84c4f8c9bdcd31c96fb49. The protected Server paths were not used as build outputs.

## Reproduced production build boundary

Before edits, the supported build was run for AMD64 DebugSymbols against the specified SDK include root. `-ServerRoot` pointed to a fresh temporary package root to protect the original Server package; its literal path was not retained in the captured command record:

    powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Architecture amd64 -Configuration DebugSymbols -SdkInclude D:\dev\guideXOSServerV0.5_DEVELOPER_STUDIO\sdk\include -ServerRoot <fresh temporary package root; literal path not retained>

Compiler: C:\Program Files\LLVM\bin\clang++.exe, Clang 22.1.8, target x86_64-unknown-elf. The exact header was D:\dev\guideXOSServerV0.5_DEVELOPER_STUDIO\sdk\include\guidexos\abi.h. The first failing translation unit was src/main.cpp, where openAppModelDocumentActivation referenced the absent member at lines 5359–5363. The diagnostic was:

    error: no member named 'get_document_activation_path' in 'gx_host_calls'

The output root was redirected to a fresh temp directory to avoid writing to the protected Server package.

## Contract ownership, definitions, and history

The public handwritten ABI definition for the specified integration is the Server SDK header at sdk/include/guidexos/abi.h. It was not generated from a schema. The standalone does not define a private gx_host_calls structure; it includes the selected SDK header.

The v0.5 Server also has a handwritten NativeHostCallTable in native_app_runtime.h. It mirrors the SDK prefix and pins each field offset. Its hosted runtime table ends at the Phase 28I expression callback and advertises size 448. It has no document activation field or activation provider. This 448-byte table remains a valid older provider: the standalone checks size before it reads the new optional tail.

The bare-metal NativeElf runtime stores the public gx_host_calls in NativeElfRuntime. The header pins its field offsets and size. Before Phase 29W, this was also 448 bytes and ended at offset 440. There was no document activation producer in the specified v0.5 branch.

The consumer was introduced in standalone commit 18c093dcb9c4fc0daf5a019d82283aba0ac319a4, “Add Developer Studio document activation,” on 2026-10-02. It asks at application startup for an App Model document path, validates the supported extension, then opens it through the existing workspace controller.

A companion Server commit exists in the separate D:\dev\guideXOSServer checkout: 81565ee227ed784862ecc04fc692cbd1904deef4, “Add Developer Studio document activation,” also on 2026-10-02. That branch adds a real hosted provider backed by an owned App Model activation context, but its gx_host_calls slot is at offset 248 in a 256-byte table. That layout predates the much longer v0.5 table and cannot be copied into the specified integration. The specified v0.5 branch did not receive that paired SDK/runtime change.

The primary fault is the missing runtime/SDK portion of the paired Server change in the specified v0.5 integration (failure category D, with stale declaration drift as the direct build symptom). The v0.5 public SDK remains authoritative for this integration. Its size field already provides append-only optional-tail discovery, so the safe resolution is a new tail slot at 448, not the separate branch’s offset 248.

## ABI and callback semantics

Before repair, sizeof(gx_host_calls) was 448 bytes. After repair it is 456 bytes, with get_document_activation_path at byte offset 448. All prior fields retain their original offsets. Both AMD64 and ARM64 are 64-bit targets with the same 8-byte function-pointer alignment; cross-target Clang layout probes reported offset 448 and size 456 for x86_64-unknown-elf and aarch64-none-elf.

The table has a size field at offset 0, a version field at offset 4, and get_api_version at offset 16. GX_API_VERSION and the current host API version remain 0. There is no capability bitmap. Table size is the compatibility mechanism: older 448-byte providers do not expose this field, and callers must test the size before reading the slot. No existing callback offset was moved.

The callback signature is gx_result (GX_CALL *)(gx_app_context*, char*, uint32_t, uint32_t*). It is synchronous in a live application context. It copies UTF-8 path bytes into a caller-owned bounded buffer; the callback does not return or retain a borrowed pointer. requiredBytes includes the trailing NUL. GX_OK with requiredBytes equal to zero means an ordinary launch has no document. A provider without activation may omit the tail slot or return GX_ERROR_UNSUPPORTED. If the buffer is too small, the provider reports the required length and returns no truncated path. Developer Studio’s hosted destination is 768 bytes; the bare-metal model destination is 256 bytes. The new bare-metal host implementation returns GX_ERROR_UNSUPPORTED because the v0.5 bare-metal App Model has no document activation context.

The companion provider’s original implementation keeps activation context owned for the launched process and makes a bounded caller-buffer copy. The v0.5 hosted NativeHostCallTable remains size 448 and therefore exercises the explicit missing-slot fallback. The v0.5 bare-metal table advertises the appended slot and returns unsupported. App Model document activation is meaningful for a hosted launch with an OS document target; it is unavailable in the current bare-metal path.

## Repair

The Server SDK now declares the callback at the end of gx_host_calls. The bare-metal NativeElf runtime installs a bounded unsupported provider, initializes the required-length output and clears the first caller byte when possible, then returns GX_ERROR_UNSUPPORTED. The runtime layout assertions and existing native ABI test were extended for the new offset, size, and callback type.

The standalone now calls CopyAppModelActivationPath, which verifies host table size before reading the optional slot. It handles a missing or null callback and provider unsupported status as an explicit unavailable state. Ordinary no-document launch is distinct from unavailable. The helper copies successful path data into caller-owned storage and rejects short, unterminated, embedded-NUL, and oversized results. The UI publishes appmodel_document_activation=UNAVAILABLE when the current host has no provider.

## Regression and production package results

The focused activation helper regression reported developerStudioDocumentActivationAbiChecks=9/9. It covers a missing slot, a shorter legacy table, null callback, exact valid UTF-8 path copy and termination, ordinary no-document launch, unsupported provider, short destination, missing terminator, embedded NUL, and independence from provider storage.

The Server native ABI layout test passed with the new expected size and callback type. Cross-target Clang probes passed for AMD64 and ARM64.

Both production packages were rebuilt from standalone source against the specified v0.5 SDK. Their package root was C:\Users\guideX\AppData\Local\Temp\guidexos-phase29w-packages-90e5807259e5490894f91cc97f53b63c:

| Architecture | Package path | Size | SHA-256 | Audit |
|---|---|---:|---|---|
| AMD64 | C:\Users\guideX\AppData\Local\Temp\guidexos-phase29w-packages-90e5807259e5490894f91cc97f53b63c\Apps\DeveloperStudio\bin\amd64\developerstudio.elf | 1,107,932 bytes | 322823CC6A7F291A46103F8329ED8F59AB27A0739C9909E608F97F40B016AA57 | PASS |
| ARM64 | C:\Users\guideX\AppData\Local\Temp\guidexos-phase29w-packages-90e5807259e5490894f91cc97f53b63c\Apps\DeveloperStudio\bin\arm64\developerstudio.elf | 1,267,724 bytes | 8784169DB2B082AA50F2D949F63392E4B8F9E85CF66B630EAFB6BA89D748DD63 | PASS |

The standard package audit verified ELF64, little endian, ET_EXEC, the expected AMD64/AArch64 machine, zero section headers, and the exact runtime set of app.json plus the two architecture-specific ELF files. QEMU staging later recorded the same AMD64 package hash in its boot image.

## Phase 29V parser and debugger evidence

The direct production parser runner was rerun on the checked-in Phase 15 fixture. It reported Ready, error=none, truncated=0, and 413 DIEs. The source breakpoint query for src/main.cpp:37 returned five addresses, primary 0x200016ac, and reverse-mapped to line 37. The 511- and 512-DIE fixtures passed, 513 returned limit_exceeded, and malformed synthetic DWARF was rejected as malformed_dwarf.

The rebuilt hosted package completed one fresh Phase 15 SteppingLifecycle session using the documented line-42 breakpoint entry. The breakpoint hit; Step Into, Step Over, and Step Out each completed at a fresh stop with current source mapping. Stack and locals refreshed from current stop generations, arguments were present, the temporary Step Out return binding was cleaned, Continue exited the target, and debugger teardown passed. The no-GXSM watch explicitly reported “authoritative GXSM metadata is unavailable.” The trace is under phase29w-traces/phase15-initial-step-lifecycle.log in the isolated Server stage.

That hosted product run proves parser readiness and the general lifecycle, but it used line 42. The exact five-address line-37 result above is from the direct parser runner, not from the hosted product. The separate line-37 runtime-only session stopped before a breakpoint or debugger-start marker: it timed out waiting for debug_condition_editor=OPEN, and its retained UI trace displayed “Manager snapshot unavailable.” Do not infer a hosted line-37 breakpoint hit from the direct parser result.

The positive GXSM hosted lifecycle passed after staging all required fixture files into a fresh temp project. GXSM v2 was accepted; the debugger read live target memory for counter=2; counter == 2 evaluated true; Step Over observed the changed value; Continue invalidated the stopped value; the relaunch read a fresh target; both target sessions exited and tore down cleanly. The passing trace is phase29w-traces/phase29q-positive-gxsm-corrected-child.log in the isolated Server stage. An earlier attempt stopped at project_open=FAIL because the fresh fixture copy omitted CMakeLists.txt and README.md; no watch or debug lifecycle began in that attempt.

The “Manager snapshot unavailable” message did not appear in the complete line-42 stepping trace. It did appear in the later line-37 conditional attempt, so it is a reproduced hosted runtime boundary. Its UI producer is the integrated debugger panel in `src/main.cpp`:14810, which renders the text whenever `g_debugUiBreakpointValid` is false. The snapshot in question is the source-breakpoint-manager `gx_development_debug_snapshot`; the bare-metal path obtains it through `GX_DEVELOPMENT_DEBUG_LIST_SOURCE_BREAKPOINTS` and the `bare_metal_development_debug` host call. The hosted path deliberately does not request that persistent manager snapshot: `debugUiRefreshBreakpoints` refreshes the hosted editor-owned rows instead. In the retained runtime-only trace, the project has `main.cpp` line 37 open and the integrated panel is visible, but the manager-valid flag is false. Consequently there is no manager row for `debugUiBreakpointAt` to return to the selected-breakpoint condition-editor handler. The first missing prerequisite was the marker `debug_condition_editor=OPEN`; it did not appear within 240 seconds. This identifies the observed UI state and gate dependency, not a failed hosted manager-provider call, because that hosted path does not call the persistent manager API.

## Acceptance gates

| Gate | Result |
|---|---|
| Hosted focused 5/5 | Stopped before qualification: the line-42 full lifecycle passed but is not a line-37 iteration; the following line-37 conditional run failed before debugger start. 0/5 eligible complete line-37 runs. |
| Hosted stress 25/25 | Not started because the focused gate failed. |
| Positive GXSM | 1/1 smoke command passed; its two target sessions both passed the GXSM lifecycle. |
| Debug CTest | 33/33 PASS, serial, as recorded after the changes. |
| Release CTest | 33/33 PASS, serial, as recorded after the changes. |
| DWARF boundaries | 511 PASS; 512 PASS; 513 limit_exceeded; malformed DWARF rejected. |
| QEMU full acceptance 10/10 | Stopped on boot 1, which reproduced invalid_project_root before the debugger service accepted a start. No second boot was attempted. |
| QEMU ABI ownership stress 25/25 | PASS: all 25 fresh boots reached `NATIVE_LOADER_REACHED`; each boot audited the same AMD64 package hash and retained the original sentinel hash. |
| Physical hardware | Not used. |

The failed full QEMU boot evidence is retained under C:\Users\guideX\AppData\Local\Temp\guidexos-phase28g-f6b2370949aa4138873e63df2a6d16c5. The project root was /P28Q and the request path remained /P28Q through normalization. Project request 1, manifest transaction 1, and project generation 1 reached project state ready. The boot image recorded Developer Studio package SHA-256 322823CC6A7F291A46103F8329ED8F59AB27A0739C9909E608F97F40B016AA57. The first required Phase 29F success marker absent was event=request_ownership result=DEBUG_START_PROJECT_AND_BUILD_CURRENT. The build reported build_artifact_validation=SKIPPED reason=invalid_project_root, then DEBUG_START_BUILD_FAILED; Server debug-start admission remained at zero. The package loaded and the project opened, but no debug callback was naturally invoked and no invalid function pointer was observed.

The successful ABI ownership stress logs and per-boot staged images are retained under C:\Users\guideX\AppData\Local\Temp\guidexos-phase28g-76a790464a704d85ab93e00cf9cf159e. The harness completed 25/25 fresh boots. Each boot logged the rebuilt AMD64 package hash above, verified the unchanged sentinel SHA-256 967D29A9A500A3108D7331BA47A5E2053A7C2203B81A345BE990084F3F43698A, loaded Developer Studio, and emitted `appmodel_document_activation=UNAVAILABLE` without an invalid function-pointer call. This was QEMU validation; physical hardware was not used.

## Git and closeout

No branch was switched, merged, rebased, reset, or rewritten. The Server SDK/runtime/layout/test changes were committed as `3b47738fbd4616990c05c11d6e1f88fe08d58b4c` (`Phase 29W align document activation SDK contract`). The three original package/app changes remain in the Server worktree and their original hashes are unchanged. The ordinary Server push was attempted and failed with `git@github.com: Permission denied (publickey)`; no remote or authentication settings were changed.

The standalone implementation and report commit is `32a24dbc4cb880c0cb52f51ef062d2094184de56` (`Phase 29W align SDK and requalify hosted debugger`). Its ordinary push failed with `git@github.com: Permission denied (publickey)`; no remote or authentication settings were changed. The later closeout and manager-snapshot documentation commits are `19c29e7` and `a4561b7`. This final report-state correction is a separate local documentation commit above those, leaving standalone `main` at 5 ahead / 0 behind after it is committed, with a clean worktree.

Final Server state is branch `v0.5_DEVELOPER_STUDIO`, HEAD `3b47738fbd4616990c05c11d6e1f88fe08d58b4c`, 1 ahead / 0 behind. The only Server worktree changes are the same three protected pre-existing paths listed above. Their final hashes match their starting values exactly: `app.json` 5793567C54ABF22423A8FCDE2F9B32E5DEEB73981E070707BA11E7D23F390401; AMD64 package 106BD4D1C8827894AB6CE4850E5DC30E55E98A1BAA52BCA149027781B4081FE9; diagnostic sentinel 967D29A9A500A3108D7331BA47A5E2053A7C2203B81A345BE990084F3F43698A. The Server commit's ordinary push also failed with `git@github.com: Permission denied (publickey)`. No `.phase` marker is present; this report is the Phase 29W record.
