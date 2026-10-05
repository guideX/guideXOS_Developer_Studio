# Developer Studio Phase 29Y — QEMU Pre-App and Stress Determinism

## Scope and phase gate

Phase 29Y followed Phase 29X. The exact expected starting commits were present before edits:

- Standalone `main`: `3e7f5dbb15df8ffb59fb200aa6dccfc06c9a9260`; clean worktree; upstream comparison `1 ahead / 0 behind`.
- Server `v0.5_DEVELOPER_STUDIO`: `3b47738fbd4616990c5c11d6e1f88fe08d58b4c`; only the three protected Phase 29X worktree items were present; upstream comparison `0 ahead / 0 behind`.
- No authoritative phase marker indicated that 29Y was stale, active elsewhere, or complete. Phase 29X documentation existed; no Phase 29Y closeout existed. The stale-prompt gate passed.

No branch switch, merge, rebase, reset, ref update, or authentication/remote change was made. No Server production source, SDK, ABI, or package file was changed. The Server protected files retained their required hashes:

| Protected path | SHA-256 |
|---|---|
| `Apps/DeveloperStudio/app.json` | `5793567C54ABF22423A8FCDE2F9B32E5DEEB73981E070707BA11E7D23F390401` |
| `Apps/DeveloperStudio/bin/amd64/developerstudio.elf` | `106BD4D1C8827894AB6CE4850E5DC30E55E98A1BAA52BCA149027781B4081FE9` |
| `ESP/Apps/DeveloperStudio/.phase28q-diagnostic` | `967D29A9A500A3108D7331BA47A5E2053A7C2203B81A345BE990084F3F43698A` |

## Protected Phase 29X evidence

Hosted Phase 29X remains the accepted baseline: focused conditional debugging `5/5`, conditional stress `25/25`, stepping, stack/locals, positive GXSM, and its focused regressions passed. The QEMU full-acceptance baseline was `10/10` fresh boots with no `invalid_project_root`. AMD64 and ARM64 package hashes were respectively `7605FE1BC4BDF82833032E9381D4678672641F906510290DB99B92D0E744A0DE` and `D4F5E831129A695A5189D33A7F37F2E53AEC49A82BC592E53F3311742B2FB85E`.

The unqualified hosted Manager-row and remap-retention routes remain separate coverage gaps. The path-sensitive long-root `malformed_dwarf` observation remains separate. Neither was pulled into this QEMU investigation.

## Recovered historical stress batches

All retained roots were found under `%LOCALAPPDATA%\Temp\guidexos-phase28g-*`. They contain per-boot serial, debugcon, QEMU debug, stdout/stderr, host-trace files, and unique `esp-bootN-<GUID>` directories. Batch IDs below use the retained evidence-root suffix.

| Batch | Last passing boot(s) | First failed boot | QEMU PID | Runner PID | First missing acceptance marker | Timeout/stop |
|---|---:|---:|---:|---|---|---|
| `03a6e634caef4e41b09dfb2d3c6ecd3d` (reported 8/25) | 1–8 | 9 | 47464 | Not recorded in retained host trace | `DEVELOPER_STUDIO_PHASE29L_OWNER checkpoint=before_commit ...`; boot 9 ended while writing the `commit_starting` tuple | Monitor stopped QEMU as `phase29l_ownership_failure`; this was the transient-token false positive, not a timeout |
| `b4233528824c4c57829a3f589666e436` (reported 22/25) | 1–22 | 23 | 46548 | Not recorded in retained host trace | `P28Z APP 00 gx_main_entry_raw` (Developer Studio entry) | `Invoke-QemuProofBoot` 120-second timeout; QEMU was alive and then killed by the harness |
| `dc37c2d324a44c05809c38455833705b` (reported 5/25) | 1–5 | 6 | 49968 | Not recorded in retained host trace | `P28Z APP 00 gx_main_entry_raw` (Developer Studio entry) | `Invoke-QemuProofBoot` 120-second timeout; QEMU was alive and then killed by the harness |

The old runner traces did not preserve the parent PowerShell PID or host process snapshots. The QEMU PID, timestamps, command line, process state, and stop owner are available for each failed boot. This limitation is kept explicit rather than reconstructing an unobserved runner PID.

Each saved launch command has the same shape (paths vary by batch and boot):

```text
"C:\Program Files\qemu\qemu-system-x86_64.exe" -machine pc,usb=off -drive if=pflash,format=raw,readonly=on,file=<phase29x-stage>\OVMF.fd -drive file=fat:rw:<evidence-root>\esp-bootN-<stage-guid>,format=raw,if=ide,index=0 -m 4096M -vga std -serial file:<evidence-root>\bootN.serial.log -debugcon file:<evidence-root>\bootN.debugcon.log -global isa-debugcon.iobase=0xe9 -d cpu_reset -D <evidence-root>\bootN.qemu-debug.log -display none -no-reboot -no-shutdown
```

The outer runner returned failure for each incomplete batch. The old per-boot files do not identify its PID; the exact aggregate process exit code is not preserved in these evidence roots.

### Boot stages and last authoritative evidence

The recorded stage sequence is: runner/stage creation; complete staging and ESP audit; QEMU spawn; firmware/UEFI loader; kernel image load and handoff; kernel entry/UART; NativeElf dispatch; kernel main loop/desktop pump; Developer Studio package entry; Developer Studio startup; Phase 29C project-load request and transaction; project ready.

**Batch 1, boot 9:** staging completed and its pre-spawn tree hash equaled the audited tree. QEMU PID 47464 launched from the recorded command, emitted serial and debugcon, and reached UEFI loader entry, kernel handoff, UART, NativeElf dispatch, Developer Studio `P28Z APP 00`/`APP 01`, and Phase 29D startup. The app accepted the `/P28Q` request and project loading reached `validated` and `commit_starting`. The saved serial tail ends in the next transaction tuple, before the required `before_commit` owner checkpoint. The harness's old matcher stopped the process mid-commit. The saved, completed owner records report `CURRENT`; the snapshot containing the transient partial poll buffer was not retained.

**Batch 2, boot 23:** QEMU PID 46548 remained alive through the 120-second deadline. P29J guest records show UEFI loader entry, kernel image load, `ExitBootServices`, kernel handoff, UART initialization, and NativeElf dispatch. Serial then shows kernel initialization, input setup, entry to `[KERNEL] Entering main loop (waiting for input)...`, and repeated desktop pump output. It contains no Developer Studio `P28Z APP 00`, no Phase 29D startup marker, no Phase 29C request-accepted marker, and no `invalid_project_root`. No Developer Studio launch request was observed.

**Batch 3, boot 6:** QEMU PID 49968 remained alive through the same 120-second deadline. It reached the same UEFI/kernel/NativeElf and kernel-main-loop stages as batch 2, then continued to emit desktop pump/render output. It contains no Developer Studio `P28Z APP 00`, Phase 29D startup, Phase 29C request-accepted marker, or `invalid_project_root`.

Both timeout boots therefore stopped at the pre-Developer-Studio application-dispatch boundary while the kernel desktop main loop was active. They were not firmware or bootloader failures, process deaths, loader-to-entry transfer failures after Developer Studio entry, breakpoint failures, DWARF failures, ABI failures, or project-root failures. The kernel pump continued, so these are stalled transitions at the application-dispatch stage, not evidence that the whole guest stopped executing. The available traces do not identify whether an app launch request was never issued or was lost before a Developer Studio entry marker; no request/admission event was logged for these two boots.

The existing `NATIVE_LOADER_REACHED` aggregate classification is present in each P29J host trace, but its marker denotes the guest's shared NativeElf dispatch path. It does not prove that the Developer Studio payload reached its entry point. The Developer Studio-specific entry marker is absent on boots 23 and 6.

## Transient-token matcher race and repair

The original live matcher used a negative lookahead that rejected prefixes `CURRENT` and `TRANSACTION_NOT_ACTIVE`, then accepted any other non-space text. A poll during serial output could see a partial token such as `result=CUR`; because it was not yet the complete valid token, the regex treated it as a failure and terminated QEMU. The historical Phase 29X note says the isolated staging-copy workaround required whitespace after a complete token. That isolated one-line staging diff was not retained as a separate file; the staged source recovered from disk still contained the old live matcher. No more specific historical partial byte buffer was saved.

The authoritative Server harness now calls `Find-Phase29LOwnerMismatch` from `scripts/Phase29L.OwnershipEvidence.psm1`. It requires a full newline-terminated owner record before recognizing a mismatch. It continues to recognize a complete unexpected result and ignores complete valid `CURRENT` and `TRANSACTION_NOT_ACTIVE` results. Per-boot serial files and stage IDs prevent previous-iteration output from entering the current matcher input. This is harness-only and preserves acceptance semantics; it removes false failure detection from incomplete reads.

The focused regression `scripts/test-phase29l-owner-matcher.ps1` passed all six cases: partial `CUR`; complete-looking token without newline; valid `CURRENT`; valid transaction release; complete current record followed by partial next record; and a complete unexpected owner result.

## Staging, image, process, and package audit

The three failed boots each used an independent directory-backed writable FAT image (`-drive file=fat:rw:<unique ESP directory>,format=raw,if=ide,index=0`) copied from the same read-only staging source. This is the intended fresh-boot contract; guest writes to one ESP do not become input state for the next. Each boot had a unique stage GUID, ESP tree, serial file, debugcon file, QEMU debug file, stdout/stderr files, and `bootN` index. The host trace records the pre-spawn tree hash and confirms no host mutations after audit. Staging was complete before spawn; the pre-spawn tree hashes matched their audits. There is no evidence of a package-copy race or incomplete write.

For all three failed boots, the staged AMD64 Developer Studio ELF hash was the intended Phase 29X hash `7605FE1BC4BDF82833032E9381D4678672641F906510290DB99B92D0E744A0DE`; the staged ARM64 package was also audited at `D4F5E831129A695A5189D33A7F37F2E53AEC49A82BC592E53F3311742B2FB85E`. Each sentinel hash remained `967D29A9A500A3108D7331BA47A5E2053A7C2203B81A345BE990084F3F43698A`. Kernel and loader hashes were stable within each recorded source stage. The three failed runs used distinct loader hashes; this is recorded as image identity evidence and is not attributed to Developer Studio because all three packages and kernel hashes match their intended identities.

QEMU was run sequentially by the harness. The code waits for process exit, drains redirected stdout/stderr tasks, reads final logs, disposes the `Process` object, and only then advances to the next boot. The old host traces do not include per-iteration handle counts or runner PIDs, so a numeric historical handle trend cannot be reconstructed. The evidence roots contain only one QEMU instance per boot and one trace set per boot; there is no retained evidence of overlapping Phase 29X emulators. A separate unrelated Phase 35Q QEMU process was observed during Phase 29Y preparation and was left untouched; new Phase 29Y QEMU gates are being held until it exits to avoid cross-run resource contention.

The timeout owner is the Server harness function `Invoke-QemuProofBoot` in `scripts/smoke-compiler-bootstrap.ps1`. It polls until `$TimeoutSeconds` (120 in the two historical failures), then kills the still-running QEMU, waits for exit, drains output, captures evidence, and returns failure because required markers are absent. The host trace distinguishes `alive_at_timeout=1` for both stalls. Neither timeout is a marker-observation loss: the final serial files lack the application-entry and startup markers themselves. No timeout inflation, retry, replacement boot, or between-boot delay was introduced for the matcher repair.

## Phase 29Y harness changes and regression status

Only the Server QEMU harness and its small ownership-evidence helper/test were edited. The protected Server package/config/sentinel remain unchanged. The focused matcher regression passed `6/6`; PowerShell parser validation passed; `git diff --check` passed. No Developer Studio or Server production binary was rebuilt.

The run gates and closeout state are filled in below after execution:

| Gate | Result |
|---|---|
| Fresh QEMU full acceptance | Pending: required `10/10` |
| Fresh QEMU ownership stress | Pending: required `25/25`, no retries |
| Hosted Phase 29X regressions | Not rerun; no product code changed |
| `invalid_project_root` in Phase 29Y | None in the three historical failed boots; final gate pending |
| Physical hardware | Not used |
| Final outcome | Pending |

## Phase 29Y continuation — bare-metal directory-preparation diagnostics

The continuation added bounded `DEVELOPER_STUDIO_PHASE29Y_DIRECTORY` records in
`kernel/core/compiler/compiler_build_service.cpp`, inside
`BareMetalBuildService::run_build_core` and its directory-preparation helpers.
Records identify semantic steps (build root, output, intermediate, object, and
source object-cache directories), build handle, project ID, owned root and
length, bounded derived path and capacity/result, VFS `stat` and `mkdir` names
and codes, and the observed classification. The build request carries no debug
request ID or project generation at this layer; records mark both unavailable.
Production does not perform a post-`mkdir` lookup, so no extra verification was
introduced. The external `GX_BUILD_ERROR_INVALID_PROJECT_ROOT` mapping remains
unchanged.

The Phase 29L owner matcher regression passed **7/7**. PowerShell parsing and
`git diff --check` passed. The AMD64 kernel and bootloader/package build
succeeded. The focused QEMU script's audited diagnostic kernel was
`5AB7CD509437AD3BC50E573AAABB6AC11099FD71BBFA0D61A01CE38638D7F31E`; the
rebuilt `ESP/kernel.elf` is
`B5FC438425A49E931294C440FF38C8ED0E67FDD2B85210D99C922472526FBD30`. The
audited current ESP AMD64 Developer Studio payload was
`B59653AE06A6953FFDE1151CB92E065076D7372FD6591AAE17CE474D5E679D04`, its
configuration was
`5793567C54ABF22423A8FCDE2F9B32E5DEEB73981E070707BA11E7D23F390401`, and the
sentinel was
`967D29A9A500A3108D7331BA47A5E2053A7C2203B81A345BE990084F3F43698A`. The
tree audit passed with no host mutations after audit. The `Apps/DeveloperStudio`
payload/config and ESP sentinel remained at their pre-run hashes.

The first bounded QEMU attempt exposed a staging mismatch: the stock Phase 28Q
path copied the protected `Apps/DeveloperStudio` payload over the active ESP
payload. Those three boots passed, but they are **not counted** as the requested
current-package reproduction. The harness now has the narrow opt-in
`-Phase29YUseStagedDeveloperStudioPackage`, which preserves the current ESP AMD64
payload for this investigation.

With that option, boot 1 used the audited current ESP payload and emitted the
new directory records. It reached `METADATA_READY`, `COMPILE_ENTRY`, and
`DEVELOPER_STUDIO_PHASE28Q_PASS`; it did **not** emit `invalid_project_root`.
For `/P28Q` (5 bytes, absolute), the root lookup returned `VFS_OK` and
`exists-directory`. The standard directory operations and
`/P28Q/build/obj/amd64/src` returned `VFS_ERR_NOT_FOUND` then `mkdir=VFS_OK`
(`created`); the source-cache follow-up lookup returned `VFS_OK` and
`exists-directory`. No directory path was truncated. Build handle was 1 and
project ID was `dev.guidexos.phase28q`. The build request itself provides no
debug request ID or project generation; the surrounding application trace
showed project-load request 1 and generation 1. No post-create validation is
performed by production code. The retained serial, debugcon, QEMU debug, stage,
and host traces are under
`C:\Users\guideX\AppData\Local\Temp\guidexos-phase28g-9e3444a179f14673bf4ca29ace632bf6`.

That corrected series stopped on boot 1 because the Phase 29L ownership harness
required Phase 29I sentinel marker lines that the current ESP AMD64 payload
does not emit. The serial nevertheless records the existing Phase 29D sentinel
decision, project request, debug-start request, successful directory
preparation, compile entry, and Phase 28Q pass. No retry or later boot was run.
The target directory-preparation failure was not reproduced, but the planned
three-boot series did not complete under its strict ownership-marker gate.
Classification remains **unresolved intermittent directory-preparation
failure**; there is no proven production defect and no production repair.

No filesystem-injection seam exists for the static bare-metal helper, so a
focused hosted test would require the fake-filesystem framework the prompt
discourages. Debug/Release CTest, Phase 29V parser, and SDK ABI regressions were
not rerun because this change is isolated to the bare-metal build/VFS path and
does not touch shared project/debugger behavior or public structures. The
formal 10/10 gate was not started. Given the corrected run's harness-marker
stop, formal acceptance is **not yet justified**; resolve the current-payload
Phase 29I marker qualification before starting it.

### Continuation checklist (items 1–25)

1. Bounded directory diagnostics were added.
2. Source/function: Server `kernel/core/compiler/compiler_build_service.cpp`,
   `BareMetalBuildService::run_build_core` and preparation helpers.
3. Matcher: **7/7 PASS**.
4. Diagnostic build/package: AMD64 kernel build passed; QEMU kernel hash is
   `5AB7CD...D7F31E`; ESP kernel hash is `B5FC43...F2F17C`; package tree audit
   passed.
5. Focused QEMU boots: three with the wrong staged app payload (excluded),
   then one with the current ESP app payload (series stopped on harness gate).
6. `invalid_project_root` reproduced: **no**.
7. Reproduction boot: none.
8. Owned root: `/P28Q`.
9. Root length: 5 bytes.
10. Failed operation: none; each observed preparation operation succeeded.
11. Derived paths: `/P28Q/build`, `/P28Q/build/bin`,
    `/P28Q/build/bin/amd64`, `/P28Q/build/obj`,
    `/P28Q/build/obj/amd64`, `/P28Q/build/obj/amd64/src`.
12. Derived path lengths: 11, 15, 21, 15, 21, and 25 bytes, respectively.
13. Pre-stat: missing for first five paths; source-cache path was missing then
    found as a directory after creation.
14. `mkdir`: `VFS_OK` for every attempted creation.
15. Post-create validation: no separate production lookup; the source-cache
    helper's next lookup returned `VFS_OK`.
16. Project ID: `dev.guidexos.phase28q`.
17. Project generation: unavailable to build service; surrounding app trace
    reports generation 1.
18. Request identity: project-load request 1; debug request ID unavailable to
    build service.
19. Build operation: build handle 1.
20. Exact VFS failure result: none observed; lookups were `VFS_OK` or
    `VFS_ERR_NOT_FOUND`, and creation was `VFS_OK`.
21. Classification: unresolved intermittent directory-preparation event;
    the corrected diagnostic series stopped on missing Phase 29I harness
    markers before its planned three boots completed.
22. Production repair: none.
23. Focused directory-helper unit test: not practical without an injectable
    VFS seam; matcher regression passed 7/7.
24. Worktrees: Server has the Phase 29Y kernel diagnostic and matcher/harness
    edits plus the pre-existing package/config/sentinel changes; standalone
    `main` retains this untracked report. No reset, rebase, clean, stash, or
    commit was performed.
25. Formal 10/10: **not yet justified or started**; current-payload Phase 29I
    marker qualification is the next boundary.
