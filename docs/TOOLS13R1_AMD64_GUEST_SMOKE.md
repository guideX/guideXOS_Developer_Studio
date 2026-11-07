# TOOLS13R1 — AMD64 Native Guest Smoke

**Current disposition: Outcome D — TOOLS13R1_ENVIRONMENT_BLOCKED.** The refreshed preflight on 2026-11-07 verified the intended TOOLS12 AMD64 ELF and the live QAC coordinator. No guest image was prepared or launched: the Server checkout reports version 0.1.0 while the application manifest requires 0.5.0, and the .NET Support acceptance handoff is not evidenced. This is an environment and compatibility block, not a Developer Studio guest failure or guest acceptance.

## Repository baseline

| Checkout | Branch | Requested / expected HEAD | Observed at start of this refresh | Worktree |
| --- | --- | --- | --- | --- |
| Developer Studio Tools source | DEVELOPER_STUDIO_TOOLS | 3d3feba2a460e52a765d713c876eda455e0bcc47 | 8d761a67ab78daa9ac4cf95fae8e582f9c678d61 | clean |
| Server integration | DEVELOPER_STUDIO_TOOLS | a13768dea9e5808baadb3c2908a8fca463505f16 | a13768dea9e5808baadb3c2908a8fca463505f16 | clean |

The source branch is correct, but its starting HEAD was not the requested HEAD. The requested commit exists and is an ancestor of the observed HEAD. The only change between them is the already committed addition of this qualification report; no application source changed. No branch switch or source-code edit was made.

## Existing TOOLS12 hosted qualification

The existing result is documented in docs/TOOLS12_NAVIGATION_HISTORY.md as TOOLS12_HOSTED_VERIFIED. It records real hosted UI navigation-history acceptance, Debug and Release suites, and isolated AMD64/ARM64 package audits. That hosted result is not native guest acceptance and was not rerun during this preflight.

## AMD64 package identity

The staged package was enumerated under the source checkout's out directory. Two copies matched the requested TOOLS12 AMD64 identity:

- D:/dev/guideXOS_DeveloperStudio_Tools_Source/out/TOOLS12/amd64-acceptance-stage/Apps/DeveloperStudio/bin/amd64/developerstudio.elf
- D:/dev/guideXOS_DeveloperStudio_Tools_Source/out/TOOLS12/dual-architecture-package/Apps/DeveloperStudio/bin/amd64/developerstudio.elf

Both files are 1,262,228 bytes with SHA256 9CBC32163417A553B38A1F01EF69CDBAF49B99C58B9D63D0F06E2E9A31C1AC16. This matches the accepted TOOLS12 value. No rebuild or unrelated binary substitution was made.

The matching manifest identifies com.guidexos.developerstudio version 0.1.0, kind NativeElf, AMD64 entry bin/amd64/developerstudio.elf, entry point gx_main, ABI guidexos-c-abi-v1, runtime native-elf, and minimum guideXOS version 0.5.0. The TOOLS12 report records implementation commit de921d3d2ba49f630295443d2236dfa7da34fec0. No separate build-provenance manifest ties the staged ELF bytes to an exact compiler invocation.

The production Server package was not overwritten. Its tracked AMD64 ELF remains 1,128,572 bytes with SHA256 1387CB09F2026252E4A277E96256C2BB59D06AE70ACFAA56A84EFEFF2F9CF845.

## Server compatibility and image preparation

The specified Server checkout identifies itself as guideXOS 0.1.0 in kernel/core/include/kernel/version.h. The application manifest requires guideXOS 0.5.0. In the manifest loader and application registry code inspected, minGuideXOSVersion is parsed and stored, but no comparison was found that establishes this package as compatible with this Server version. Compatibility therefore remains unproven and the declared minimum is not met.

The Server checkout has build and QEMU scripts, ESP/ramdisk.img (67,108,864 bytes), and disposable test disks. Those files do not establish a compatible, isolated guest image. No image was built or modified; there is no guest image path or SHA256 and no Server build output was changed. The Server build identification is checkout HEAD a13768dea9e5808baadb3c2908a8fca463505f16, kernel version 0.1.0.

The existing disposable fixture is at:

D:/dev/guideXOS_DeveloperStudio_Tools_Source/out/TOOLS13R1/fixture/DeveloperStudioNavigationSmoke

It contains main.cpp, helpers.cpp, helpers.h, and README.md. The sources include a structure, a helper function, TODO and FIXME markers, TOOLS13R1_KNOWN_SEARCH_ID, and enough source lines for Go to Line. It remains outside the Server checkout and was not copied into an image. Because no compatible guest image or guest workspace path was established, its guest path is unverified.

## QAC authorization and admission

QAC's documented procedure is in D:/dev/guideXOS_QAC/README.md. The supported CLI apphost is:

D:/dev/guideXOS_QAC/src/GuideXOS.QAC.Cli/bin/Debug/net10.0-windows/GuideXOS.QAC.Cli.exe

Read-only status, list, and queue commands were run through that CLI on 2026-11-07. The coordinator was online as QAC 2.0.0, PID 5860. Status reported zero QEMU processes (zero foreign), zero managed sessions, zero queued requests, and zero blocked requests. List reported zero reservations and zero managed sessions; queue reported zero queued reservations.

The latest local coordination closeout, docs/QAC_SHARED_QEMU_COORDINATION_PHASE4R2.md, says .NET Support retains first QEMU priority and requires a separately authorized handoff before guest acceptance. No explicit .NET Support release or independent authorization was present in the reviewed local coordination records. The empty live QAC census does not establish that the owner baton was released. No TOOLS13R1 reservation was requested, no QEMU command was run, and there is no reservation ID or guest process ownership to report.

QAC gate: TOOLS13R1_READY_GUEST_PENDING_BATON. The overall phase remains Outcome D because the incompatible Server version also blocks image preparation. This QAC lane status does not imply that the guest image is ready or that any guest acceptance occurred.

## Guest results and evidence

No guest was launched. Therefore:

- Kernel, desktop/compositor, input services, package recognition, Developer Studio startup/window, and native stability were not observed.
- Tests A–E were not performed: Tools menu; Go to File; Go to Line; Task List scan and navigation; Navigate Back/Forward.
- Optional utility checks and read-only replacement safety were not performed.
- No guest screenshot, serial output, application diagnostic, or shutdown result was generated.
- No guest failure or application exception was observed; this is not evidence of a pass.
- Shutdown and QAC cleanup were not applicable because no reservation or guest was started.
- No Compilation, Debugging, Server, or QAC implementation was changed. No production package was overwritten and no push was made.

## Recommended TOOLS13R2 scope

Resolve the package minimum-version contract by selecting or preparing a production-compatible Server target whose declared version satisfies 0.5.0; do not lower the application minimum or bypass a compatibility gate. Obtain explicit .NET Support baton release or independent lane authorization, then acquire one QAC-managed AMD64 reservation. Use the verified TOOLS12 ELF and a disposable image, establish the guest workspace path for the prepared fixture, and perform the bounded Tests A–E with retained visual and serial evidence. Do not expand into a feature phase or begin that work automatically.
