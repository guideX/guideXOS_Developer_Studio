# Developer Studio Phase 29Z — Native Loader to Application Entry Determinism

## Phase gate and starting state

The stale-prompt gate passed. No authoritative phase marker was present. At the
start of work, the Phase 29Y closeout existed and no Phase 29Z closeout or
Phase 29Z commit existed.

| Worktree | Branch | Starting HEAD | Expected divergence | Observed starting divergence | Status |
|---|---|---|---|---|---|
| Standalone `D:\dev\guideXOS_Developer_Studio` | `main` | `27df1a62f6737a1b38813b4b98089e35fcda3aca` | 1 ahead / 0 behind | 1 ahead / 0 behind | clean |
| Server `D:\dev\guideXOSServerV0.5_DEVELOPER_STUDIO` | `v0.5_DEVELOPER_STUDIO` | `b32c472a21680ab8cb2abd01549e759dc396c207` | 1 ahead / 0 behind | 0 ahead / 0 behind | clean |

The Server divergence discrepancy was reported by comparing live Git state;
the requested starting HEAD matched. No branch or history operation was made.

## Phase 29Y baseline and qualified payload

Phase 29Y recorded **10/10 full acceptance boots passing** with the pinned
Developer Studio AMD64 payload, then stopped ownership stress on boot 4 of 25.
Each full-acceptance boot proved all eight Phase 29I sentinel markers,
`/P28Q` project ready, Phase 29L ownership, build-directory preparation,
`COMPILE_ENTRY`, and `DEVELOPER_STUDIO_PHASE28Q_PASS`; none emitted
`invalid_project_root` or `DEVELOPER_STUDIO_PHASE28Q_FAILURE`. No historical
25-boot count is resumed here. The retained boot-4 token is
`dd4e14074ebe467485e9f22332a4fed0`, with QEMU PID `26212`. Its evidence is
preserved at
`C:\Users\guideX\AppData\Local\Temp\guidexos-phase28g-f0ccd701a9644cbcb0d345d0dc95e3ac`.

The qualified AMD64 payload is **1,117,692 bytes**, SHA-256
`7605FE1BC4BDF82833032E9381D4678672641F906510290DB99B92D0E744A0DE`.
Boot 4's host trace records that hash, zero host mutations after audit, and a
QEMU instance alive at the existing 120-second timeout before the runner killed
and reaped it. The package files staged on passing boot 3 and failed boot 4
have the same hashes: manifest `5793567C54ABF22423A8FCDE2F9B32E5DEEB73981E070707BA11E7D23F390401`,
AMD64 ELF `7605FE1BC4BDF82833032E9381D4678672641F906510290DB99B92D0E744A0DE`,
ARM64 ELF `73C449E224CFFCBDA094B69C46375CA467C4B6D449A107C6AAE87BE816D1E44B`,
and sentinel
`967D29A9A500A3108D7331BA47A5E2053A7C2203B81A345BE990084F3F43698A`.

The prompt's 22/25 and 5/25 historical stress batches also lacked Developer
Studio APP 00 and timed out in the kernel pump. They establish a recurring
pre-entry symptom, but do not identify a loader call for the Developer Studio
image.

## Retained boot-4 evidence: first divergence

The most important correction from the source and serial audit is that the
retained boot-4 `P28Z BOOT 05` and `P28Z BOOT 08` lines are generic NativeElf
smoke-call markers. The occurrences are associated with paths such as
`/r42.elf`, `/r41.elf`, and `/d27a.elf`; neither marker includes a target path
in the old build. They do not establish that Developer Studio reached the
loader, and the missing APP 00 cannot be interpreted as a failed transfer to
that application.

The boot instead reached `DEVELOPER_STUDIO_PHASE28M_APP_DISCOVERY_PASS=FAIL`.
It emitted no `ELF Loader: file=/Apps/DeveloperStudio/...` line, no APP 00 or
APP 01 for Developer Studio, and no Developer Studio return value. The
Phase28M package-discovery check short-circuited before `run_file`; thus no
Developer Studio function pointer, target stack, entry register state, or
application fault was observed on the failed boot. The correct first-divergence
classification is **package discovery / guest VFS identity check**, separate
from a loader-to-entry failure and separate from `invalid_project_root`.

Passing stress boots 1–3 each loaded the exact Developer Studio path at base
`0x50000000`, entry `0x500BF640`, and emitted APP 00 and APP 01. Failed boot 4
has no Developer Studio loader record or entry address. Passing boot 3 reached
`DEVELOPER_STUDIO_PHASE28M_APP_DISCOVERY_PASS=PASS` for
`/Apps/DeveloperStudio/bin/amd64/developerstudio.elf`, image base
`0x50000000`, entry `0x500BF640`, mapped size `16,093,184` bytes, and emitted
APP 00 and APP 01. The passing and failing host-staged package hashes match,
so the historical difference is in guest package stat/read/token discovery,
not the host payload identity.

The retained failure has no serial exception marker. Its QEMU debug log was
configured for guest errors and CPU resets; the absence of a recorded exception
therefore does not prove that no CPU fault occurred. In this boot, however, the
Developer Studio entry was not attempted, so an app-entry CPU fault is not a
plausible explanation for this particular failure.

## Marker producers and exact call semantics

| Marker | Producer and meaning |
|---|---|
| `P28Z BOOT 05 gx_main_invoke` | `kernel/core/native_elf/native_elf_loader.cpp`, `run_file_internal`. It is printed before `invoke_native_entry_on_stack`; historically it did not name the path. It marks dispatch preparation, not the indirect call instruction itself. |
| `P28Z BOOT 08 gx_main_returned` | Same function, after `invoke_native_entry_on_stack` returned true, the trampoline result was captured, runtime state/report fields were updated, and the return value was printed. The assembly trampoline returns true only on the path after `call rax` returns. Without a path field, the old marker proves only that some NativeElf entry returned. |
| `P28Z APP 00 gx_main_entry_raw` | Standalone `src/main.cpp`, body of `extern "C" gx_result GX_CALL gx_main(gx_app_context* ctx)`. It is emitted through `logMarker` after the initial `ctx` / `ctx->host` validity check. The compiler prologue executes before the source body; there is no earlier app wrapper or static constructor in the retained ELF entry path. |
| `P28Z APP 01 gx_main_entered` | Same `gx_main` startup path, after APP 00, as the app proceeds into startup. It is a later lifecycle marker and cannot substitute for APP 00. |

`P28Z APP 00` uses the application's host logger, not a raw serial or debugcon
write. It can therefore be absent if the function body is reached but its
context/host/logger preconditions fail. The app source and marker were not
changed because boot 4 never invoked this payload.

`run_file_internal` in `native_elf_loader.cpp` emits BOOT 05 after validation
and immediately before calling the stack-switching trampoline. BOOT 05 is not
immediately adjacent to the indirect target call: the trampoline validates its
arguments, saves the Microsoft nonvolatile GPR/XMM state, sets the app stack,
loads the context into RCX, and only then executes `call rax`. The focused
build's `P29Z CALL_BEGIN` is emitted just before entering that trampoline;
APP 00/APP 01 prove target-side execution, and `P29Z CALL_RETURN` proves the
trampoline returned. The qualified path is also explicit on BOOT 05/08 in the
new build.

The exact trampoline source is `kernel/arch/amd64/native_elf_trampoline.asm`.
In the linked kernel image used by the formal runs (SHA-256
`D513CED93B2DA0F0EDC1EBDB6828D2F499066DD520A31D651497CE0F749D4CCF`),
`invoke_native_entry_on_stack` executes the indirect call at `0x26FFF3`; the
CPU pushes the return address `0x26FFF5` at the app stack's entry RSP. That
return site stores EAX into the trampoline result, stores runtime status,
restores the original kernel RSP and nonvolatile registers, and returns true.
The C++ caller then emits `P29Z CALL_RETURN`, captures the result, and later
emits BOOT 08. On the focused run, the pushed return address occupied stack
slot `0x6FFFFFD8`.

## Qualified ELF and ABI audit

The qualified file is an AMD64 fixed-address `ET_EXEC` with no section table,
five program headers, and `e_entry = 0x500BF640`. The standalone CMake link
options use `-e,gx_main`; the stripped artifact has no symbol table, so the ELF
does not independently retain a named `gx_main` symbol. The link setting,
entry disassembly, and passing boot's APP markers establish the intended
mapping from `e_entry` to `gx_main`.

| `PT_LOAD` | File offset | Virtual address | File size | Memory size | Flags |
|---|---:|---:|---:|---:|---|
| 0 | `0x0` | `0x50000000` | `0x26D90` | `0x26D90` | R |
| 1 | `0x26D90` | `0x50027D90` | `0xE9F6B` | `0xE9F6B` | R-X |
| 2 | `0x110D00` | `0x50112D00` | `0xFC` | `0xE45D00` | RW |

The entry lies in executable segment 1, whose bounds are
`[0x50027D90, 0x50111CFB)`. Its segment-relative offset is `0x97850`, giving
file offset `0xBE640`. The 16 expected entry bytes are:

```text
55 48 89 E5 41 57 41 56 41 55 41 54 56 57 53 48
```

The disassembly begins `push rbp; mov rbp,rsp`, followed by saving Microsoft
x64 nonvolatile registers and allocating a `0x4978`-byte frame. The ELF is
fixed-address `ET_EXEC`, has no dynamic/relocation headers, and requires no
runtime relocation for this entry path. The loader uses validated `e_entry`
with load bias zero, not a runtime symbol-table lookup.

The loader's actual path passes the validated entry as a `uint64_t` to the
assembly trampoline, whose contract is Microsoft x64 ABI and whose `call rax`
passes the single context pointer in RCX and captures the 32-bit EAX result.
`native_elf_executor.cpp` also contains the typed helper alias
`int32_t (__attribute__((ms_abi)) *)(void*)` on GCC/Clang (MSVC x64 uses its
default Microsoft ABI); `run_file_internal` uses the stack-switching assembly
trampoline instead of that C++ helper. The application declaration is
`extern "C" gx_result GX_CALL gx_main(gx_app_context* ctx)`; the SDK's
`GX_CALL` is Microsoft x64 ABI on AMD64. `gx_result` is 32-bit and the single
pointer argument is 64-bit, so return/argument widths and calling convention
match. There is no app-specific wrapper or C++ constructor sequence on this
loader path; the trampoline calls the validated entry directly.

The dedicated app stack range is `[0x6FF80000, 0x70000000)` (512 KiB). The
trampoline sets call-site RSP to `top - 0x20` and target-entry RSP to
`top - 0x28`; call-site RSP is 16-byte aligned and entry RSP has the required
modulo-16 value 8. The app has 524,248 bytes of headroom at entry. The context
is a 24-byte `gx_app_context` with API version 0, host-table pointer, and
runtime user data. The host table is 456 bytes, version 0; `log`,
`get_api_version`, and the required bare-metal build start/poll/release tail
callbacks are populated. The retained failed boot never passed these values to
Developer Studio.

The image is copied into executable memory immediately before invocation. The
AMD64 loader does not issue an instruction-cache flush; the x86 path is
coherent. The focused diagnostic build records exact PT_LOAD copy and zero-fill
counts, runtime entry bytes, PTE permissions, context/table fields, CS/CPL,
stack state, and call/return timing for the qualified path.

## Phase 29Z bounded diagnostics

Server `kernel/core/native_elf/native_elf_smoke.cpp` now reports the Phase28M
manifest/artifact stat results, file types/sizes, and each identity-token
stat/read/byte-count/found result. The probes preserve the original
short-circuit order and do not add VFS reads after a failed check. The original
`DEVELOPER_STUDIO_PHASE28M_APP_DISCOVERY_PASS` contract is unchanged.

Server `kernel/core/native_elf/native_elf_loader.cpp` adds target-path-only
records for resolved `e_entry`, each segment's copy/zero-fill bounds, expected
and mapped entry bytes, entry PTE, app context/host table, CS/CPL, stack and
ABI, a path-qualified `P28Z BOOT 05`, `P29Z CALL_BEGIN`, APP 00/APP 01 host-log
observations, `P29Z CALL_RETURN`, and a path-qualified `P28Z BOOT 08` with
return value. These records are bounded and only emitted for the exact
qualified Developer Studio path, except the legacy smoke markers which remain
generic and now include their path.

## Validation and gate results

The Phase29Z diagnostic AMD64 kernel build passed with the Phase28M smoke gate
enabled, after the diagnostic helper's Phase28M compile guard was corrected.
The Server AMD64 Release bootloader rebuild passed. No new product test was
added because no causal loader/app defect reproduced; the diagnostics were
validated through the requested focused and formal QEMU gates. `git diff --check`
passed with only the repository's existing LF-to-CRLF normalization notices.
No application source, package, SDK, or ABI
was changed; no AMD64/ARM64 payload rebuild or hosted/SDK ABI regression is
required for the diagnostic-only Server changes.

| Gate | Result |
|---|---|
| Focused fresh QEMU diagnostic | **1/1 PASS**; QEMU PID `14664` was reaped after the required marker |
| Full acceptance | **10/10 PASS**, fresh QEMU boots, zero retries |
| Ownership stress | **25/25 PASS**, fresh QEMU boots, zero retries |
| `invalid_project_root` | No occurrence in retained Phase29Y boot 4, focused boot, 10/10 full acceptance, or 25/25 stress |
| AMD64 payload | Retained unchanged at 1,117,692 bytes, SHA-256 `7605FE1BC4BDF82833032E9381D4678672641F906510290DB99B92D0E744A0DE` |
| ARM64 payload | Retained unchanged at 1,195,916 bytes, SHA-256 `73C449E224CFFCBDA094B69C46375CA467C4B6D449A107C6AAE87BE816D1E44B` |
| Hosted Phase29X regressions | Not rerun; standalone app and shared hosted debugger code were not changed |
| SDK ABI regressions | Not rerun; public ABI and SDK declarations were not changed |
| Physical hardware | Not used |
| Outcome | **Outcome C** — the retained failure was before Developer Studio loader dispatch; no current loader/app defect reproduced |

An unrelated Phase35Q emulator (PID `11496`) was active during preparation. Its
command line points to `D:/dev/guideXOSUEFI_Phase35R12_SANDBOX`; it was left
untouched. The Phase29Z runner's guard stopped one attempted launch before it
created a VM. After PID `11496` exited, the one-boot diagnostic was run without
overlap.

### Focused current-payload diagnostic boot

Command:
`scripts/smoke-compiler-bootstrap.ps1 -Phase29LFullAcceptance -BootCount 1 -TimeoutSeconds 120 -Phase29YUseStagedDeveloperStudioPackage`.
The run exited 0 after reaching its required full-acceptance marker. Evidence is
under
`C:\Users\guideX\AppData\Local\Temp\guidexos-phase28g-422a02fc792b472d9bd95aa3b610857a`,
boot 1, stage `0aa008f6b6774e029612b1f7c539c4a2`. The host audit and pre-spawn
tree hashes match, host mutations after audit are zero, the staged Developer
Studio hash is the qualified `7605FE...A0DE`, and QEMU PID `14664` was reaped
at the required-marker stop (`alive_at_timeout=0`, `process_reaped=1`). The
boot's kernel hash was
`D513CED93B2DA0F0EDC1EBDB6828D2F499066DD520A31D651497CE0F749D4CCF`; the
UEFI loader hash was
`05D206BAE80A7C990576A2094F32BACC81DC69EE290240B54E35DFE97F68F1CD`.

The new package record shows manifest stat `VFS_OK`, regular file, 1,364 bytes;
artifact stat `VFS_OK`, regular file, 1,117,692 bytes; each of the five
identity-token reads returned 1,364 bytes and found its token. The app discovery
gate passed. This confirms that boot 4's earlier package-discovery failure was
guest-side/intermittent relative to identical host-staged bytes, but the old
boot contains no per-token record to identify which lookup first diverged.

For this boot, load bias was zero; `e_entry`, function pointer, and intended
`gx_main` address were all `0x500BF640`. Runtime segment copies were complete:

| Segment | File offset | Destination | Filesz = copied | Memsz | Zero fill | Bounds | Flags |
|---|---:|---:|---:|---:|---:|---|---:|
| 0 | `0x0` | `0x50000000` | 159,120 | 159,120 | 0 | `[0x50000000, 0x50026D90)` | R |
| 1 | `0x26D90` | `0x50027D90` | 958,315 | 958,315 | 0 | `[0x50027D90, 0x50111CFB)` | R-X |
| 2 | `0x110D00` | `0x50112D00` | 252 | 14,966,016 | 14,965,764 | `[0x50112D00, 0x50F58A00)` | RW |

At entry file offset `0xBE640`, the expected and runtime signatures both were
`55 48 89 E5 41 57 41 56 41 55 41 54 56 57 53 48` (`match=1`). The entry PTE
was `0x500BF061` for page/physical page `0x500BF000`: present, read-only, and
executable (`NX=0`). Code coherency was the x86 coherent path; no explicit
instruction-cache flush was issued.

The loader passed `RCX=0x7297560` (`gx_app_context`, size 24, API version 0),
host table `0x7297578` (size 456, version 0), and user data `0x7297520`. The
host logger, API-version query, and all three bare-metal build start/poll/release
callbacks were present. CS was `0x8`, CPL 0. At `P29Z CALL_BEGIN`, the kernel
RSP was `0x7847850`; the app stack was `[0x6FF80000,0x70000000)`, with call RSP
`0x6FFFFFE0` (mod 16 = 0), entry RSP `0x6FFFFFD8` (mod 16 = 8), and 524,248
bytes remaining to the lower bound. The trampoline restored the same kernel
RSP after return.

APP 00 was observed at tick `0x1B7`, the same tick as CALL_BEGIN, and APP 01 at
tick `0x1B9` (two ticks later). `gx_main` returned 0; `P29Z CALL_RETURN`
reported runtime status 0 and elapsed `0x300` PIT ticks (768 ticks, about
7.68 seconds at the observed 10 ms PIT tick). The path-qualified
BOOT 08 then reported the same Developer Studio path and return value 0.
Phase28M app cleanup passed; `/P28Q` reached ready and
`DEVELOPER_STUDIO_PHASE28Q_PASS` appeared. No `invalid_project_root`,
Phase28Q failure marker, or serial exception marker appeared. This focused boot
proves the intended entry, loaded bytes, ABI, stack, and marker route on a
passing boot; it does not by itself disprove an intermittent package-discovery
failure.

Across the ten full-acceptance boots, APP 00 arrived 0–1 PIT ticks after
CALL_BEGIN and APP 01 arrived 1–2 ticks after CALL_BEGIN. `P29Z CALL_RETURN`
reported 665–686 ticks (about 6.65–6.86 seconds at the observed 10 ms PIT
tick). The elapsed timer starts immediately after the path-qualified BOOT 05
record and immediately before CALL_BEGIN; the counter is PIT-tick granular.
The retained failed boot 4 has no Developer Studio BOOT 05 or BOOT 08 interval
because package discovery stopped before loader dispatch.

Every instrumented focused/full/stress run resolved the same image base
`0x50000000`, `e_entry` and call address `0x500BF640`, with load bias zero.

### Formal full acceptance

Command:
`scripts/smoke-compiler-bootstrap.ps1 -Phase29LFullAcceptance -BootCount 10 -TimeoutSeconds 120 -Phase29YUseStagedDeveloperStudioPackage`.
Result: **10/10 PASS**, exit 0, no retry or replacement boot. Evidence is under
`C:\Users\guideX\AppData\Local\Temp\guidexos-phase28g-fc8d97b358dc400f860ff89d43bfb301`.
Each boot used a unique directory-backed ESP whose tree hash matched its
pre-spawn audit; each host trace reports zero post-audit mutations and the
qualified AMD64 payload hash. All ten reached the Phase28M app gate and the
Phase28Q terminal acceptance marker; host traces classified all ten as
`NATIVE_LOADER_REACHED`; Developer Studio APP 00 and APP 01 were
present on each; all ten had matching expected/runtime entry bytes and the
present, read-only, executable entry PTE. Their package records show all five
identity tokens found in 1,364-byte manifest reads. Every QEMU was reaped at
the required-marker stop. No
`invalid_project_root` or Phase28Q failure marker occurred. The runner reports
“Phase 29L ownership and full Phase 28Q lifecycle validation completed across
10 fresh boot(s).” This gate followed the focused 1/1 pass and is not a retry
of the historical boot-4 failure.

### Formal ownership stress

Command:
`scripts/smoke-compiler-bootstrap.ps1 -Phase29LOwnershipOnly -BootCount 25 -TimeoutSeconds 120 -Phase29YUseStagedDeveloperStudioPackage`.
Result: **25/25 PASS**, exit 0, no retry or replacement boot. Evidence is under
`C:\Users\guideX\AppData\Local\Temp\guidexos-phase28g-a764593374db43b0a59d0fe338540ddc`.
All 25 fresh stages used the exact qualified AMD64 hash, passed host tree audit
with zero post-audit mutations, were classified `NATIVE_LOADER_REACHED`, emitted
APP 00 and APP 01, produced the qualified loader entry and CALL_BEGIN evidence,
and had matching entry bytes, executable entry PTEs, and package records with
all five identity tokens found in 1,364-byte manifest reads. They reached
Phase29C project-load ready and ended at the ownership transaction release result
`TRANSACTION_NOT_ACTIVE` with one commit and one release. The runner's strict
per-boot verifier accepted all 17 required main ownership checkpoints exactly
once and in order, all eight observer callback/return pairs, and the associated
request/transaction/candidate records. All QEMU processes were reaped after
their required terminal markers. There were no entry timeouts, ownership
mismatches, `invalid_project_root` results, or retries. Historical failure
ordinals 4, 6, and 23 also passed. The ownership-only gate stops after its
required lifecycle evidence while Developer Studio remains active; it does not
wait for `gx_main` to return. Accordingly, none of the 25 stress serials has a
`P29Z CALL_RETURN` or target-path BOOT 08 record.

An independent post-run audit checked every formal boot's serial and host trace:
10/10 full-acceptance rows and 25/25 stress rows contain the expected payload
hash, clean staging audit, reaped-QEMU record, app discovery pass, entry bytes
match, executable entry PTE, path-qualified CALL_BEGIN, APP 00/APP 01,
project-load ready, and no `invalid_project_root` or Phase28Q failure. The
10/10 full-acceptance rows also contain `P29Z CALL_RETURN` with return 0 and
target-path BOOT 08 with return 0. The 25/25 ownership rows contain the
exactly-once transaction release result and correctly have no target return
record because the harness stops while the application remains active.

The full-acceptance series retained the Phase29Y directory diagnostics: 140
`DEVELOPER_STUDIO_PHASE29Y_DIRECTORY` records across ten boots, with no path
truncation or unexpected VFS results. All ten boots reached `COMPILE_ENTRY` and
`DEVELOPER_STUDIO_PHASE28Q_PASS`; no
`DEVELOPER_STUDIO_PHASE28Q_FAILURE` or `invalid_project_root` appeared. The
ownership-only series does not run the build-directory operation; those
diagnostics remain in place and were exercised on every full-acceptance boot.

## Passing versus failed retained boot

| Field | Passing retained boot 3 | Failed retained boot 4 |
|---|---|---|
| Qualified payload | 1,117,692 bytes, SHA-256 `7605FE...A0DE` | Same |
| Package files staged on host | Manifest, AMD64 ELF, ARM64 ELF, and sentinel hashes match boot 4 | Same |
| Phase28M package discovery | PASS | FAIL |
| Developer Studio loader path | `/Apps/DeveloperStudio/bin/amd64/developerstudio.elf` | No `run_file` call for this path |
| Image base / entry | `0x50000000` / `0x500BF640` | Not loaded for Developer Studio |
| Entry bytes / PTE / stack / host table | Focused 29Z boot: bytes match, PTE R-X, ABI-aligned stack and populated table | Not applicable: no Developer Studio invocation |
| Actual Developer Studio call | Focused 29Z boot reached CALL_BEGIN and returned through the exact target path; APP 00/01 appeared | Not attempted |
| Developer Studio return value | Focused 29Z boot returned `0` | None |
| Exception evidence | No serial exception marker; call returned normally | No entry-specific exception record; entry not attempted |
| First divergence | Package discovery passed | Package discovery failed before loader dispatch |

The detailed VFS/token cause from boot 4 remains unavailable because it
predates the new bounded record. The serial only proves the historical
Phase28M discovery check failed before `run_file`; it does not retain which
stat/read/token subcheck failed. The focused boot passed package and entry
checks, full acceptance passed 10/10, and ownership stress passed 25/25. The
historical package-discovery miss did not reproduce. No loader/app defect or
production repair is justified by this evidence. The Server changes are bounded
diagnostics: Phase28M package-probe observations and exact qualified-path
loader observations. The old generic BOOT05/BOOT08 markers now name their path.

**Final classification: Outcome C.** The qualified call path is verified,
entry bytes match, page permissions and ABI are correct, and focused/full
acceptance calls returned normally with result 0. All current gates pass. The
ownership stress gate observed its required transaction lifecycle while the
app remained active and stopped before return. The original boot-4 event was a
guest package-discovery failure before the application loader call, not a
demonstrated loader-to-entry failure. Since its exact VFS subcheck is absent
from retained evidence and it did not recur in the bounded formal gates, no
speculative loader repair was made.

## Phase 29Z continuation closeout

Complete: focused diagnostic **1/1**, full acceptance **10/10**, and ownership
stress **25/25**, all with zero retries. Commit IDs, push results, ending HEADs,
and ahead/behind counts are reported in the Phase 29Z closeout response.
Remaining follow-up coverage stays separate: persistent hosted
Breakpoint Manager-row qualification, hosted condition remap/rebuild
retention, and long-root `malformed_dwarf`.
