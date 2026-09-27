# Developer Studio Phase 29I — Guest Diagnostic Sentinel Detection Stabilization

## Result

Phase 29I established one canonical sentinel path and a production detection state machine that separates “not ready” from “ready and absent.” A fresh QEMU boot using the rebuilt AMD64 application proved that the host-staged file was in the exact directory-backed FAT filesystem passed to QEMU and that the guest could stat, read, accept, and latch that file before diagnostic-mode selection. That boot also created one Phase 29D `/P28Q` request and reached Phase 29C `load_started` and `ready`.

The required 5-boot gate did not pass: boot 1 passed; boot 2 timed out before the first kernel/loader marker and produced no serial file; boots 3–5 were not run. The historical Phase 29H `fixture=absent` result did not preserve a lower-level VFS result, so its exact original cause cannot be identified. Phase 29I did not reproduce a guest lookup failure on a boot that reached Developer Studio.

**Classification: Outcome B.** The sentinel path is positively proven end to end on one rebuilt fresh boot, but the 5/5 gate and later acceptance tiers remain incomplete.

## Repository baseline and preserved files

| Repository | Branch | Starting HEAD |
|---|---|---|
| Standalone Developer Studio | `main` | `9f1865f11675686f71dc51ff2c32182cedb448b4` |
| Server integration | `v0.5_DEVELOPER_STUDIO` | `80d82969c191316453ee9d2f2b198eea36a6393c` |

Both repositories began with no tracked changes. The existing standalone untracked Phase 3B JSON pair, Phase 15 JSON pair, and `tests/fixtures/debugger-phase15-h29-0927/` remain untouched and are not part of a commit. The QEMU runner rebuilds `ESP/Apps/DeveloperStudio` in place; its first Phase 29I run removed the existing untracked server sentinel while replacing that directory. A retained pre-rebuild staging backup contained the original 17-byte canonical content `guideXOS-phase28q` (SHA-256 `967D29A9A500A3108D7331BA47A5E2053A7C2203B81A345BE990084F3F43698A`), and those exact bytes were restored to `ESP/Apps/DeveloperStudio/.phase28q-diagnostic`. The sentinel is still untracked and excluded from commits. The harness now snapshots/restores `Apps/DeveloperStudio` for the Phase 29Q-only run mode as well. Hosted validation used a separate copy of the H29 fixture; cleanup of that newly created copy was rejected by the command policy, so it remains an additional untracked directory at `tests/fixtures/debugger-phase15-h29-0927-phase29i-hosted-run-1/`. The original H29 fixture and its JSON files were not used as the mutable test copy.

## Canonical sentinel contract

The single definition is in `src/developer_studio_diagnostic_sentinel.h`:

```text
guest path: /Apps/DeveloperStudio/.phase28q-diagnostic
content:    guideXOS-phase28q
```

The path is absolute, case-preserving, and rooted at `/`. It is on the QEMU ESP filesystem mounted by the kernel as FAT32 at `/`; it is not on a ramdisk or a separate workspace volume. The boot harness reads the path and content definitions from that header and derives the host-relative path `Apps/DeveloperStudio/.phase28q-diagnostic`. The Phase 28Q staging function creates the sentinel as part of the copied Developer Studio package. The package manifest is already present in that package before the sentinel write.

The guest normalizer converts backslashes to `/`, collapses repeated separators, removes a trailing separator, requires an absolute path, and rejects `.` and `..` components. It does not case-fold the canonical path or try alternate roots.

## Host staging and QEMU attachment audit

The runner's actual order is: stage the P28Q project and its app configuration in the working ESP; copy the packaged Developer Studio (including its `app.json`) into `ESP/Apps/DeveloperStudio`; write the canonical sentinel; finish copying kernel, bootloader, and other configured inputs; clone the complete ESP directory to a fresh per-boot path; then run the final audit. The audit reopens the sentinel, checks exact content and size, hashes every file into a sorted tree manifest, and records the kernel hash. `File.WriteAllText` closes its handle on return, and the copy operations complete before audit. The script now snapshots/restores `ESP/Apps/DeveloperStudio` in Phase 29I mode so this in-place staging does not remove the pre-existing source sentinel after the run. There is no separate disk-image library handle to detach: QEMU uses its directory-backed `fat:rw:` vvfat backend directly. The same `$activeEspDirectory` is hashed, printed in `P29I QEMU_ATTACH`, and passed in QEMU's `-drive` argument.

### Pre-rebuild characterization boot (not counted in the 5-boot gate)

Command: `scripts/smoke-compiler-bootstrap.ps1 -Phase29GBeginDebugReturnOnly -BootCount 1 -TimeoutSeconds 120`.

| Field | Value |
|---|---|
| Stage ID | `4d64c8293ad44b78a60856d136c401fd` |
| Filesystem backend | QEMU vvfat directory-backed FAT |
| Staged ESP path | `C:\Users\guideX\AppData\Local\Temp\guidexos-phase28g-5cb66f727f0a45d781320261f409be98\esp-boot1-4d64c8293ad44b78a60856d136c401fd` |
| Host sentinel path | `<ESP>\Apps\DeveloperStudio\.phase28q-diagnostic` |
| Guest-relative path | `Apps/DeveloperStudio/.phase28q-diagnostic` |
| Sentinel | 17 bytes, `guideXOS-phase28q`, SHA-256 `967D29A9A500A3108D7331BA47A5E2053A7C2203B81A345BE990084F3F43698A` |
| ESP identity / tree SHA-256 | `E499E6FEC3EBF87127E1CF26C3112E8245C06A2B668CC6964C677EE43FB3BBC0` / `4DAE6897AA26B9084830F41E46D0C5646B57634C3BBF68AF7CC4068923BA6281` |
| Kernel SHA-256 | `DED960566738DE613797C76B4C2A1AC1B631928CC25FBA8417CD0C3CBFB2D801` |
| AMD64 Studio payload | Pre-rebuild package, SHA-256 `8E5F00473E489319BE43523600097BFA1F2F7B440E606D4762998A099F921A2E` |
| QEMU attachment | `C:\Program Files\qemu\qemu-system-x86_64.exe`, `-drive file=fat:rw:<the exact ESP path>,format=raw,if=ide,index=0` |

The guest mounted FAT32 at `/`, identified the containing device as `ata0m`, opened the app ELF and the sentinel, selected diagnostic mode, handed off the P28Q request, and reached `DEVELOPER_STUDIO_PHASE29A_STOP_MAPPING_PASS`. This characterization predates the rebuilt Phase 29I app and is not counted as formal acceptance.

### Rebuilt Phase 29I focused run

Command: `scripts/smoke-compiler-bootstrap.ps1 -Phase29ISentinelOnly -BootCount 5 -TimeoutSeconds 120`.

| Boot | Stage ID and exact staged ESP path | Sentinel audit | QEMU attachment / guest result |
|---|---|---|---|
| 1 | `d47bb6c58bdf4ca6b59cbff2b5ca5baa`; `C:\Users\guideX\AppData\Local\Temp\guidexos-phase28g-b4fcd2df5b9e457c99da7f716f24741f\esp-boot1-d47bb6c58bdf4ca6b59cbff2b5ca5baa` | Passed. 17 bytes, exact content, SHA-256 `967D29A9A500A3108D7331BA47A5E2053A7C2203B81A345BE990084F3F43698A`; tree SHA-256 `DE4B99DBEDA5A32C27640F420CAA2CE264ABCE4751E2E8CD20E0CBA4A2C6D21B`; ESP identity `32B6A606180D33AC94568EC3902BFFFCF4743A7CA230FEBE16321AF4CAB58DF3` | QEMU attached the same path using `fat:rw:<path>,format=raw,if=ide,index=0`. Guest sentinel detection passed through Phase 29C `load_started` and `ready`. |
| 2 | `79023af2867a4d69b0008cbd835ae351`; `C:\Users\guideX\AppData\Local\Temp\guidexos-phase28g-b4fcd2df5b9e457c99da7f716f24741f\esp-boot2-79023af2867a4d69b0008cbd835ae351` | Passed. Same sentinel bytes/hash and tree SHA-256 as boot 1; distinct per-boot stage ID and ESP identity `9915597C04B1DFDDD28EDBEB5FFB382D7F8D0C38ED2CC5B023E4348992DF60BF` | Runner invoked QEMU with this same path. No `boot2.serial.log` was created; no guest milestone was observed before timeout. Boot 2 failed and the runner stopped. |

Both staged runs audited kernel SHA-256 `3715A42C6E72F311034191671B56659372186F7489A2C332DB3D68A06423DB2C` and AMD64 Developer Studio SHA-256 `C59CBE0E4823C668399284E5867BFB8C35F5440B105910010ADB300783B3E896`. The tree digest proves the host-inspected sentinel is included in the exact directory supplied as QEMU’s IDE drive. Boot 2’s attachment is proven as the invoked QEMU argument; without a serial file, no guest-side attachment/mount confirmation is claimed for that boot.

The first invocation of this gate stopped during kernel compilation before QEMU launch because a mount diagnostic used a nonexistent `serial::put_hex` formatter. It was corrected to the existing `put_hex8`/`put_hex32` APIs. That build failure was not a QEMU boot and is not counted among the fresh boot attempts.

## Guest readiness, mount, and lookup

The guest readiness boundary is the Native ELF loader opening the Developer Studio ELF from the root filesystem, after kernel VFS initialization has completed. On boot 1, the serial log shows:

```text
[VFS] Mounted FAT32 at '/' device_index=00 volume_id=00000000 device=ata0m
[KERNEL] Successfully mounted persistent storage from ata0m
ELF Loader: file=/Apps/DeveloperStudio/bin/amd64/developerstudio.elf
[VFS] Opened: /Apps/DeveloperStudio/bin/amd64/developerstudio.elf
```

Here `volume_id=00000000` is the VFS FAT-volume slot index, not a globally unique hardware volume ID. The bounded log provides filesystem type, root mount, device index, FAT-volume slot, and device name. No volume label is exposed by this mount log. The root mount was ready before `gx_main`; mount discovery was complete, not an asynchronous post-launch operation.

The exact production lookup in `gx_main`:

1. Begins a detection record owned by the current Phase 29D startup generation.
2. Treats the opened Native ELF’s containing root filesystem as the authoritative ready mount.
3. Normalizes the one canonical absolute path.
4. Calls the production VFS-backed stat adapter, then reads the file and compares the exact expected byte count and content.
5. Reports a result-specific marker and resolves Phase 29D’s sentinel decision once.
6. Keeps a positive result latched for that generation; it does not poll every frame.

Boot 1 logged `FS_READY readiness=ready boundary=loaded_application_image`, `MOUNT_READY mount=/ identity=containing_loaded_application_volume status=authoritative`, normalized `/Apps/DeveloperStudio/.phase28q-diagnostic`, stat `found gx_result=0 size=17 type=regular`, read `found gx_result=0 bytes=17`, exact file acceptance, and `DIAGNOSTIC_MODE enabled=1 result=accepted`. The old `fixture=absent` result had no raw stat/read error attached, so it cannot be reconstructed as `NOT_FOUND`, `WRONG_MOUNT`, `MOUNT_NOT_READY`, `INVALID_PATH`, or `IO_ERROR` from the Phase 29H trace.

The new production reducer has `WaitingForFilesystem`, `Present`, `Absent`, and `Error` states, plus bounded reason codes. An unavailable filesystem or mount remains pending without issuing an I/O lookup. A definitive `NotFound` becomes final `Absent` only after authoritative mount readiness. Wrong mount, invalid path, and I/O errors remain distinct errors. Positive detection is terminal for that startup generation. An application relaunch begins with a fresh detection record. Hosted startup uses an explicit hosted bypass and continues to activate its workspace fixture through its existing hosted flow; it does not pretend to share the QEMU VFS sentinel.

## Startup ownership and transaction handoff

On QEMU boot 1, the same app/startup generation owned the entire path:

```text
app=1 generation=1
sentinel=present
diagnostic=phase28q enabled
request=1 path=/P28Q
Phase 29C state=load_started
Phase 29E manifest identity=valid
Phase 29C state=ready active_generation=1
```

The Phase 29D request-created, request-submitted, request-accepted, and handoff-complete records occurred once. Handoff completed when Phase 29C accepted the exact `/P28Q` `load_started` request. The boot continued synchronously through Phase 29E manifest validation and Phase 29C `ready` before the focused process was stopped.

## Regression and build validation

The new test target exercises the production detection reducer through 12 focused cases: pending without lookup before readiness; present; positive latch; ready-and-absent; wrong mount; canonical path normalization; invalid path; stale generation rejection; fresh relaunch; mount pending; explicit I/O and stat/read/content errors; and single Phase 29D project-request ownership with duplicate request rejection.

| Check | Result |
|---|---|
| Debug CTest | **33/33 passed** |
| Release CTest build | 26/33 passed; seven model suites failed under the Release MinGW configuration (`model`, `project`, `navigation`, `symbol`, `relationship`, `structured variables`, and `conditional breakpoints`). The targeted sentinel test passed. |
| Standalone AMD64 native package build | PASS |
| Standalone ARM64 native package build | PASS |
| AMD64 / ARM64 package content audit | **PASS**; package contains only `app.json` and the two sectionless ELF files |
| Bare DWARF capacity | **PASS, dies=397** |
| Updated QEMU PowerShell parser | PASS |

| Architecture | Size | SHA-256 |
|---|---:|---|
| AMD64 | 1,054,092 bytes | `C59CBE0E4823C668399284E5867BFB8C35F5440B105910010ADB300783B3E896` |
| ARM64 | 1,219,404 bytes | `1DBAB082DAFFBA9C7081F58E4524BF7466AD688538AED854EB80BAD4ADF17240` |

## Hosted breakpoint boundary

Hosted validation was rerun with `smoke-developer-studio-phase20.ps1 -Case ConditionEditor` against the disposable copy of the H29 fixture. Hosted activation logged `SENTINEL_HOSTED_BYPASS ... result=not_queried`, preserving the hosted/QEMU distinction.

Hosted evidence:

- Project open passed for the copied Phase 15 fixture.
- The editor opened `src/main.cpp`, navigated to line 37, and logged `debug_breakpoint_key=PASS` and `debug_breakpoint_toggle=PASS`.
- The copied debugger configuration contained an enabled `BREAK` record for `sourcePath=src/main.cpp`, line 37, column 1. The breakpoint toggle was saved.
- The UI repeatedly rendered `No symbols`. The host toggle trace still reported `debug_active=0 generation=0 process=0 runtime=0 breakpoints=0`.
- No hosted symbol association or debug-start mapping was produced. No explicit pending/installed/rejected mapping result was observed. The script timed out waiting for `GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_breakpoint=PENDING`.

This remains an independent hosted pre-mapping boundary. It is outside the Phase 29I guest sentinel repair and prevents hosted validation from passing.

## Focused and later acceptance gates

| Gate | Result |
|---|---|
| Phase 29I focused sentinel QEMU | **Failed at boot 2: boot 1 passed; boot 2 timed out before the first kernel/loader milestone with no serial file; boots 3–5 not run.** One complete guest sentinel proof is available; required 5/5 is not satisfied. |
| Phase 29D → Phase 29C request handoff | Boot 1 passed: one `/P28Q` request, `load_started` accepted. |
| Phase 29C transaction | Boot 1 passed through manifest validation and `ready`. |
| Extended focused gate | Not run after 5/5 failure. Pre-rebuild exploratory boot reached Phase 29A stop mapping, but is not a Phase 29I extended-gate result. |
| Phase 29H artifact/symbol snapshot | Not reached in the rebuilt Phase 29I gate. |
| Phase 29F debug-start / Phase 29B RUNNING | Not reached in the rebuilt Phase 29I gate. |
| Phase 29G return | Not reached in the rebuilt Phase 29I gate. |
| Hosted breakpoint readiness | Failed at missing `debug_breakpoint=PENDING`; mapping was not attempted. |
| Full QEMU acceptance | **0/10; not started** because the required 5/5 sentinel gate failed. |
| Lifecycle stress | **0/25; not started** because 10/10 did not pass. |
| Physical hardware | Not run; all hardware-path evidence is QEMU emulation. |

## Root cause and remaining blocker

The Phase 29H serial evidence preserved only `fixture=absent diagnostic=off`. The previous bool helper did not distinguish mount-not-ready, wrong mount, invalid path, not found, or I/O failure. Therefore the exact historical guest failure cause is **not recoverable from retained evidence**. Phase 29I now makes these states observable and proves that a correctly staged sentinel is visible through the expected mounted root on one rebuilt boot. It does not claim that the old boot’s exact false-negative mechanism was reproduced.

The current acceptance blocker is the fresh-boot reliability boundary at boot 2: QEMU did not produce serial output or a first loader/kernel milestone within the gate timeout. The sentinel lookup was not reached on that boot. Separately, hosted breakpoint readiness remains unresolved after a persisted `src/main.cpp:37` request. No artifact, DWARF, debugger, or breakpoint behavior was changed in Phase 29I.

## Git and publication

Phase 29I changes are committed locally in separate standalone and server commits. The preserved untracked fixture pairs and original sentinel are excluded. Normal pushes were attempted; `Permission denied (publickey)` was returned. Authentication and remotes were not changed.
