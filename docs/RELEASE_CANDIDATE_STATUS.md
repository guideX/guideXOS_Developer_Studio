# Phase 25 release-candidate status

Date: 2026-08-23
Product: guideXOS Developer Studio 0.1.0
Classification: **RELEASE CANDIDATE SIGNED OFF WITH MINOR LIMITATIONS**

This record covers the Phase 25 manual distribution rehearsal. The product
remains an experimental, hosted-development Native ELF studio; this is not a
1.0 stability claim.

## Source baseline

- Developer Studio release source baseline: `main`, commit
  `715a29df535103d1d281c7181219b5f986b41f7c`, tracking `origin/main`.
- Phase 25 rehearsal checkout: `main`, commit
  `bb66e11c38dffae21514384f73b1920b53ac64fe` (`Sign off Developer Studio
  release candidate`), tracking `origin/main`, initially clean and `0/0`
  ahead/behind. This commit is documentation/evidence-only and has the Phase
  24 source baseline as its parent.
- Paired Server: `v0.5_DEVELOPER_STUDIO`, commit
  `fb71e36c77866cdabf20873e6ad3de0ddc3bbe02`, tracking
  `origin/v0.5_DEVELOPER_STUDIO`, initially clean and `0/0` ahead/behind.
- The Phase 24 source and Server commits were treated as feature-frozen; no
  product feature or history rewrite was made during the rehearsal.

## Runtime package

The authoritative package is `Apps/DeveloperStudio` in the paired Server
checkout. Its exact runtime payload is:

```text
app.json
bin/amd64/developerstudio.elf
```

Recorded release values:

| Field | Result |
| --- | --- |
| App Model ID | `com.guidexos.developerstudio` |
| Display name | `guideXOS Developer Studio` |
| Version | `0.1.0` |
| Kind / architecture | `NativeElf` / `amd64` |
| Entry | `bin/amd64/developerstudio.elf`, `gx_main` |
| ABI / runtime | `guidexos-c-abi-v1` / `native-elf` |
| ELF | ELF64, little-endian, AMD64, `ET_EXEC` |
| ELF entry point | `0x2998f0` |
| ELF sections | `.symtab` present; no `.debug_*` sections |
| ELF size | `1,015,064` bytes |
| ELF SHA-256 | `5343F5DF0EDFE423542348C0AF8C9F8690CE339D197DEB4737DD05A27348CAC7` |
| Manifest SHA-256 | `D59C12267D360C4E31B70DEC7F9C1B0689FB5816011D681B38EE9CDEF0D2F53` |
| Package total | `1,016,176` bytes across 2 files |

The external staging root was `D:\dev\release-rehearsal\phase25`, outside both
repositories. Staging pass 1 and the recreated pass 2 matched the authoritative
manifest and ELF hashes. An exact path/fixture/log filename audit of the staged
package found no checkout paths, test paths, log files, or extra runtime files.

## Rehearsal results

- The real Server App Model registry discovered the package by canonical ID and
  `source=Package`; the first staged launch rendered Developer Studio visibly,
  opened the checked-in fixture project, and closed cleanly.
- The hosted document workflow passed edit/save, A/B document switching with
  dirty-state preservation, exact `Point::origin` completion acceptance,
  completion dismissal, successful Build Project output, and clean close.
- A direct hosted project session sent the real Alt+O shortcut and passed the
  ownership markers (`ownership_build=PASS groups=2 pairs=0`) before clean close.
- The post-debug hosted follow-up workflow then passed edit/save, A/B switching,
  completion dismissal/acceptance, Build Project success, and clean close.
- The intentional compile failure was reported in Output, then a recovery build
  succeeded and the hosted Server exited cleanly.
- The staged debugger workflow passed F9, Ctrl+F5, a real source breakpoint,
  source navigation/execution marker, Call Stack, Locals/Arguments, Watches,
  Continue/rebind, target identity, and targeted shutdown. The required tier
  additionally covered conditional breakpoints, selected-frame inspection,
  structured/cyclic values, input delivery, and Step Out.
- Removing the installed package and reinstalling the recreated pass-2 staging
  copy produced the same allowlist, size, and hashes. A second App Model launch
  passed.
- A temporary package with a missing ELF was rejected by the package audit and
  by the real App Model launch with no stale Developer Studio process. A
  one-byte mutation in a temporary ELF changed its hash from the release value
  to `BC70AA869172E338431F69AA74DD4EBF0ED353FD6BB5FE5B81A10404FA79E8E7`.
- Two unchanged `build.ps1 -Configuration Debug` rebuilds both produced
  `1,015,064` bytes and the release ELF SHA-256 above.

## Validation evidence

- Final fast tier: CMake/native build passed, CTest **27/27**, package build and
  audit passed, and hosted debugger lifecycle passed; elapsed `328.1` seconds.
- Required debugger tier: **8/8 suites** passed on the frozen release candidate;
  final rerun total `2116.2` seconds, with all eight suite results recorded in
  `final-tier2.log`.
- Final supported in-repository CTest run: **27/27** passed; the explicit
  `developer_studio_debugger_watches_test` rerun also passed.
- Explicit `developer_studio_debugger_watches_test`: passed; its Windows
  regression thread is created with a `64u * 1024u` stack.
- Paired experimental Server build: passed; existing compiler warnings were
  non-blocking and no release source was changed for them.
- Evidence directory: `D:\dev\release-rehearsal\phase25` (not shipped and not
  part of either repository). Key logs include `final-fast-tier.log`,
  `final-tier2.log`, `documents-workflow.log`, `build-recovery-workflow.log`,
  `debugger-continue-workflow.log`, `alt-o-workflow.log`,
  `post-debug-build-retry.log`, `corrupt-launch.log`, and the rebuild logs.

## Dependency and notice boundary

Launching the packaged app requires the paired guideXOS Server App Model,
Native ELF runtime, and the package itself; it does not require a Developer
Studio or Server checkout path embedded in the package. The Build Project
workflow additionally requires the hosted Server SDK headers and LLVM
`clang++`/`ld.lld`; CMake, CTest, and the native test toolchain are validation
dependencies. The repository has no standalone `LICENSE*` or `NOTICE*` file in
the Developer Studio or paired Server root, so this rehearsal adds no new
licensing assertion or notice file.

## Minor limitations and handoff

The known non-blocking limitations remain: no external-edit conflict-merge
subsystem, no remote CI claim, and the fixed hosted AMD64/Native GUI
Application scope. No high-severity or release-blocking issue was found.
The next action is manual synchronization/push by the repository owner when
desired; no push was performed during this task.
