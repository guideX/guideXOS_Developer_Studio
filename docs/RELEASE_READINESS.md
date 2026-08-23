# Phase 23 release readiness

Phase 23 is the release-hardening record for guideXOS Developer Studio. The
product remains an experimental, hosted-development Native ELF studio. The
supported shipped shape is one version-1 `Native GUI Application` project,
one truthful hosted AMD64 target, and one canonical packaged Developer Studio
application. This is not a 1.0 stability claim.

## Current supported surface

The native shell supports project creation and validation, project-free
workspaces, bounded Explorer navigation, up to eight editable text documents,
tabs, dirty tracking, Save/Save All, safe close prompts, lexical C/C++
highlighting, Outline and Symbol Index views, bounded navigation and search,
lexical references and rename, Include Graph, Header/Source Ownership,
Signature Help, Quick Type Info, and Type-Aware Member Completion. The package
and hosted Run Project paths validate project identity, manifest identity,
artifact containment, artifact hash, ELF shape, ABI, and entry point.

The hosted debugger supports F9 source breakpoints, real Continue, Step Into,
Step Over, Step Out, DWARF source mapping, a bounded AMD64 Call Stack,
read-only Locals and Arguments, structured values, read-only Watches, and
conditional breakpoints. Debug artifacts use the separate bounded `DebugSymbols`
configuration. Ordinary packaged output is an `ET_EXEC` image with a symbol
table and no `.debug_*` sections.

## Failure-path contract

Project metadata is bounded to 16 KiB and has an exact version-1 field set.
Malformed JSON, duplicate or unknown fields, missing fields, unsupported
values, absolute or traversal paths, overlong strings, invalid identifiers,
missing required files, missing source roots, and manifest identity mismatches
are rejected deterministically. Project loading validates the candidate before
installing it in the workspace controller. A failed candidate therefore does
not replace the previously active project, root, or document. Opening a
missing document returns a read error without adding a phantom editor buffer;
external deletion is detected on refresh and a later open recovers after the
file is restored.

Build and Run reject invalid project metadata, unsupported targets, failed
builds, invalid or stale artifacts, and invalid artifact identity. A package
build removes the previous production ELF before compilation, writes the new
ELF in a bounded staging directory, validates it, and only then moves it into
the package. A failed compile therefore cannot leave an older executable
looking like the current build result. Server-side run validation remains the
final boundary for manifest, containment, hash, ELF, ABI, and entry-point
checks.

## Package audit

The production package is intentionally exact:

```text
Apps/DeveloperStudio/app.json
Apps/DeveloperStudio/bin/amd64/developerstudio.elf
```

`tests/validate-developer-studio-package.ps1` rejects extra runtime files and
checks the canonical application ID `com.guidexos.developerstudio`, display
name, `NativeElf` kind, AMD64-only architecture, one `gx_main` entry, the
`guidexos-c-abi-v1` ABI, `native-elf` runtime, ELF64 little-endian AMD64
`ET_EXEC` headers, `.symtab`, and the absence of `.debug_*` sections. It prints
the absolute package path, byte size, SHA-256, entry point, and exact file
list. Use `-AllowDebugSections` only for a deliberate debug package audit.

## Reproducibility and boundaries

Generated projects are deterministic for identical inputs and do not embed
timestamps, usernames, machine paths, random IDs, or checkout paths. Package
identity and output paths are fixed by the project format and manifest. The
validation tiers are local/manual PowerShell entry points; this repository
does not claim GitHub Actions or remote CI execution.

Known limitations are intentional: only the fixed Native GUI Application kind
and hosted AMD64 profile are accepted; no Git integration, language server,
plugin/extension loading, package manager, terminal, remote deployment,
attach/remote debugging, multi-architecture orchestration, general C++
expression evaluator, instruction-breakpoint editing, memory/register editing,
CFI unwinding, optimized-code debugger parity, clipboard, redo, regex, or
Replace in Files is provided. The hosted path requires the paired Server
checkout, Windows process/runtime support, and the configured SDK/toolchain.

## Local validation entry points

Tier 1 is the bounded fast command:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\run-developer-studio-validation-fast.ps1 -ServerRoot D:\dev\guideXOSServerV0.5_DEVELOPER_STUDIO
```

The Phase 23 document/completion proof is:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\smoke-developer-studio-phase23-documents.ps1 -ServerRoot D:\dev\guideXOSServerV0.5_DEVELOPER_STUDIO
```

That hosted flow edits document A, switches to document B, saves A, restores
focus to A, invokes member completion, accepts the exact `origin` member, and
checks both Escape dismissal and document-change dismissal. All waits and
cleanup are bounded; failures retain a bounded trace artifact.
