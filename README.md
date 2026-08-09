# guideXOS Developer Studio

guideXOS Developer Studio is the early native development-environment shell for guideXOS Server.

The current bounded phase proves the first useful source workflow:

- a manifest-driven Native ELF application with the canonical App Model ID `com.guidexos.developerstudio`;
- a native IDE shell with a bounded Explorer, document tabs, diagnostics, and status regions;
- runtime-neutral Workspace, Project, Target Profile, Capability, and maturity models;
- open workspace, browse, open, edit, save, switch, and safely close document behavior;
- version 1 `guidexos.project` metadata, Native GUI Application creation, and project loading;
- a truthful AMD64 hosted guideXOS target profile; and
- bounded project-local Header / Source Ownership with `Alt+O` switching and
  `Alt+Shift+O` File Ownership inspection; see [docs/HEADER_SOURCE_OWNERSHIP.md](docs/HEADER_SOURCE_OWNERSHIP.md).
- bounded, generation-aware Lightweight Type Intelligence with Quick Type Info via `Ctrl+Alt+T`; see [docs/LIGHTWEIGHT_TYPE_INTELLIGENCE.md](docs/LIGHTWEIGHT_TYPE_INTELLIGENCE.md).
- Type-Aware Member Completion uses that shared type layer for bounded direct members after `.` and `->`; see [docs/CODE_COMPLETION.md](docs/CODE_COMPLETION.md).
- deterministic model, filesystem-workflow, package, hosted App Model, bounded Build Project, temporary hosted Run Project, bounded C/C++ lexical highlighting, active-document Find/Replace, project-scoped Find in Files, lexical Document Outline/Project Symbol Index coverage, bounded lexical Go To Definition (`F12`/`Alt+Left`), project-local Declaration–Definition Relationships (`F12`/`Ctrl+F12`/`Alt+F12`), bounded project-local Find All References (`Shift+F12`), manually invoked lightweight lexical Code Completion (`Ctrl+Space`), bounded lexical Signature Help (`Ctrl+Shift+Space`), and a project-local bounded Include Graph (`Ctrl+Shift+I`).

The Debugger Foundation provides generation-safe hosted Native ELF session supervision, F9 source-breakpoint storage and markers, capability-gated Debug commands, and Breakpoints/Debug Session inspection; see [docs/DEBUGGER_FOUNDATION.md](docs/DEBUGGER_FOUNDATION.md). Instruction-level breakpoints, pause/continue, stepping, source-to-address mapping, and value inspection remain deferred. Additional project templates, semantic analysis, IntelliSense, Git integration, visual design tools, and multi-architecture orchestration remain deferred.

## Workspace versus project

A workspace is any directory that Developer Studio can browse and edit. It does not need metadata. A project is a validated guideXOS application workspace with a `guidexos.project` file, identity, target profile, source root, and App Model manifest. The shell labels these states as `Workspace:` and `Project:` respectively; opening a project never upgrades or modifies an arbitrary workspace.

## Project workflow

Use `File -> New Project` or `Ctrl+N` to create the one supported template, `Native GUI Application`. Enter a display name, absolute parent location, optional folder name, and application ID. `Tab` or `Enter` advances through the bounded fields; `Enter` on Application ID creates the project; `Escape` cancels without filesystem changes. The folder is derived as a lowercase, hyphenated name when left blank.

Use `File -> Open Project` or `Ctrl+Shift+O` with either the project root or its `guidexos.project` path. The metadata, target, required files, and manifest are validated before the project becomes active. The generated `src/main.cpp` opens automatically. `File -> Open Workspace` and `Ctrl+O` continue to support project-free directories.

### Version 1 project format

The authoritative metadata file is `guidexos.project`. It is bounded to 16 KiB, uses UTF-8-compatible JSON escapes supported by the parser, LF newlines, two-space indentation, a fixed field order, and a final newline. Version 1 has exactly these required fields; unknown fields and duplicate fields are rejected:

```json
{
  "formatVersion": 1,
  "projectId": "com.example.hello",
  "displayName": "Hello guideXOS",
  "projectKind": "native-gui-application",
  "sourceRoot": "src",
  "applicationManifest": "app/app.json",
  "defaultTargetProfile": "guidexos.amd64.hosted.native",
  "entryPoint": "gx_main",
  "abi": "guidexos-c-abi-v1",
  "architecture": "amd64",
  "outputName": "hello-guidexos"
}
```

String limits are 96 bytes for project IDs and display names, 160 bytes for project-relative paths, 96 bytes for output names, and 128 bytes for model names. Display names are non-empty, printable, and cannot contain `/`, `\\`, control bytes, or leading/trailing spaces. IDs are lowercase reverse-domain identities with at least two non-empty segments; each segment starts with `a-z` and continues with `a-z`, digits, or `-`, with no repeated dots or trailing hyphen. User-created projects cannot claim the reserved `com.guidexos` namespace. Relative paths use `/`, reject absolute forms, empty segments, `.`, and `..`. Entry points are bounded C identifiers. The only accepted kind in this phase is `native-gui-application`; the only accepted target is `guidexos.amd64.hosted.native`, with `amd64` and `guidexos-c-abi-v1`.

## Generated Native GUI Application

The generated layout is:

```text
<ProjectRoot>/
    guidexos.project
    CMakeLists.txt
    build.ps1
    README.md
    app/
        app.json
    src/
        main.cpp
        freestanding_memory.cpp
```

`main.cpp` exports `extern "C" gx_result GX_CALL gx_main(...)`, creates a centered resizable Native ELF window titled with the project display name, renders `Welcome to <Project Display Name>`, handles close/Escape, and emits deterministic `GUIDEXOS_NATIVE_TEMPLATE_MARKER` messages. It does not auto-launch. `app/app.json` uses `NativeElf`, the validated application ID, `bin/amd64/<outputName>.elf`, `gx_main`, `guidexos-c-abi-v1`, and manual-launch permissions.

The generated `build.ps1` is the fixed Build Project recipe. It requires explicit `-SdkInclude` and `-ToolchainRoot` values and contains no Developer Studio or Server checkout absolute path. The generated CMake file remains an optional external build description. A typical external build is:

```powershell
.\build.ps1 -SdkInclude D:\path\to\guideXOSServer\sdk\include -ToolchainRoot "C:\Program Files\LLVM\bin"
```

Generation is byte-for-byte deterministic for identical inputs: no timestamps, usernames, machine paths, random IDs, or session state are embedded. It creates only a new destination or an empty pre-existing destination, tracks every file and directory it creates, verifies the required layout, parses the generated metadata and manifest, and removes only tracked files and empty directories after a partial failure. Non-empty destinations are rejected before changes.

## Workspace workflow

Use `File -> Open Workspace` or `Ctrl+O`, then enter an absolute hosted path in the temporary path-entry dialog. The Explorer shows a bounded directory listing; activate a supported text file to open it in a document tab. `Ctrl+S` saves the active document and `Ctrl+Shift+S` saves all named documents. `Ctrl+W` closes the active tab.

Supported text extensions are `.c`, `.cc`, `.cpp`, `.cxx`, `.h`, `.hh`, `.hpp`, `.hxx`, `.txt`, `.md`, `.json`, `.xml`, `.css`, `.html`, `.htm`, `.js`, `.ts`, `.ps1`, `.cmd`, `.bat`, `.cmake`, `.ini`, `.cfg`, `.log`, `.mk`, `.yml`, and `.yaml`. Unknown, binary-looking, and oversized files are visible when practical but are not opened as editable text.

Limits are intentionally explicit: 128 visible entries per directory, 8 open documents, 256 KiB per editable file, 8 navigated workspace path segments, 768-byte model paths (the hosted ABI additionally bounds an entered host path to 240 bytes), and bounded output history. Directory enumeration is immediate-directory only and never follows symlinks or recursively scans without a user navigation action.

The editor supports caret movement, bounded selection, insertion, Enter, Backspace, Delete, arrows, Home, End, mouse placement, vertical wheel scrolling, bounded automatic horizontal scrolling, visible line separation, dirty tracking, tabs, save failure preservation, lexical C/C++ syntax highlighting, active-document literal Find/Replace, project-scoped Find in Files, a document Outline, project-wide Ctrl+T symbol lookup, lexical F12 Go To Definition with Ctrl+F12 declaration navigation, Alt+F12 switching, and Alt+Left/Alt+Right history, lexical Shift+F12 Find All References, conservative preview-first F2 Rename Symbol with one-operation Ctrl+Z undo, manually invoked lexical Code Completion with Ctrl+Space, manually invoked lexical Signature Help with Ctrl+Shift+Space, Quick Type Info with Ctrl+Alt+T, and project-local Include Graph views with F12 include navigation. It has no clipboard, redo, folding, regex, Replace in Files, or full semantic reference binding. See [docs/FIND_AND_REPLACE.md](docs/FIND_AND_REPLACE.md), [docs/FIND_IN_FILES.md](docs/FIND_IN_FILES.md), [docs/SYNTAX_HIGHLIGHTING.md](docs/SYNTAX_HIGHLIGHTING.md), [docs/SYMBOL_INDEX.md](docs/SYMBOL_INDEX.md), [docs/CODE_COMPLETION.md](docs/CODE_COMPLETION.md), [docs/SIGNATURE_HELP.md](docs/SIGNATURE_HELP.md), [docs/LIGHTWEIGHT_TYPE_INTELLIGENCE.md](docs/LIGHTWEIGHT_TYPE_INTELLIGENCE.md), [docs/GO_TO_DEFINITION.md](docs/GO_TO_DEFINITION.md), [docs/DECLARATION_DEFINITION_RELATIONSHIPS.md](docs/DECLARATION_DEFINITION_RELATIONSHIPS.md), [docs/FIND_ALL_REFERENCES.md](docs/FIND_ALL_REFERENCES.md), [docs/RENAME_SYMBOL.md](docs/RENAME_SYMBOL.md), and [docs/INCLUDE_GRAPH.md](docs/INCLUDE_GRAPH.md).

Type-Aware Member Completion is automatic after a typed `.` or completed `->`,
and remains available through the existing `Ctrl+Space` completion command.
It is intentionally limited to direct members of a truthfully resolved type;
inheritance, access filtering, templates, static `Type::` lookup, and arbitrary
chained expressions remain deferred.

## Build

Build Developer Studio itself with an explicit Server checkout:

```powershell
powershell -ExecutionPolicy Bypass -File .\build.ps1 -ServerRoot D:\path\to\guideXOSServer
```

For a valid Native GUI Application project, use `Build -> Build Project` or `Ctrl+Shift+B`. Developer Studio derives the fixed build system, `build.ps1`, `Debug` configuration, and `build/bin/amd64/<outputName>.elf` artifact from the version 1 project metadata. Dirty project documents prompt for Save All or Cancel; an active build keeps the shell responsive and blocks close. The hosted Server validates the project, resolves the SDK/toolchain, runs the recipe asynchronously, captures separately bounded stdout/stderr output, and validates the resulting ELF64 AMD64 `ET_EXEC` image and `gx_main` entry point.

The service is hosted-development only. It does not accept arbitrary commands or metadata-defined executables, supports one build at a time, caps output at 64 KiB and 32 retained lines of 255 bytes, and stops a build after five minutes. SDK/toolchain resolution uses the hosted Server `sdk/include` and fixed LLVM locations, with `GUIDEXOS_SDK_ROOT` and `GUIDEXOS_TOOLCHAIN_ROOT` overrides.

## Run Project

For the supported Native GUI Application, use the Build menu's `Run Project (F5)` item or press `F5`. Every run rebuilds the active project first; dirty documents go through the existing Save All/Cancel gate so a stale artifact is not launched.

After a successful build, Developer Studio passes the project root, identity, fixed target, manifest path, artifact path, and build SHA-256 through the append-only hosted Run ABI. The Server revalidates the project metadata, exact generated manifest shape, artifact containment/non-symlink status, hash, ELF64 AMD64 `ET_EXEC` image, ABI, and `gx_main` entry point. It then registers the result as an in-memory temporary App Model application and launches it through the existing AppRegistry, DesktopService, Native ELF loader, and hosted runtime.

The temporary application is owner- and generation-bound to the Developer Studio runtime. Its ID cannot collide with an installed application, it is not persisted or added to recent/pinned state, and only the generated Native GUI permission set is accepted. `Request Project Close` sends a close event only to windows owned by that deployment; on exit the temporary registration is removed and the handle is released. Closing Developer Studio while a run is active first presents a close-request modal.

Run state is visible in the bottom status bar. Deterministic markers include `run_request`, `run_build_required`, `run_artifact_validation`, `run_deployment_prepare`, `run_launch`, `run_application_state`, `run_close`, `run_cleanup`, and `run_complete`. See [docs/RUN_PROJECT.md](docs/RUN_PROJECT.md) for the state machine and validation boundaries.

## Smoke test

```powershell
powershell -ExecutionPolicy Bypass -File .\tests\smoke-developer-studio.ps1
```

The smoke test checks the manifest, model invariants, registry identity, target resolution, no-auto-launch startup behavior, native dispatch, window ownership/title, render markers, and clean close.

The bounded workflow smoke test is:

```powershell
powershell -ExecutionPolicy Bypass -File .\tests\smoke-workspace.ps1
```

It creates and removes only its own temporary fixture and verifies enumeration, editing, saving, duplicate-tab prevention, safe binary/size rejection, and Save/Discard/Cancel close behavior.

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for the integration audit, boundaries, and intentionally deferred work.
See [docs/BUILD_PROJECT.md](docs/BUILD_PROJECT.md) for the Build Project contract and diagnostics.
See [docs/OUTPUT_AND_DIAGNOSTICS.md](docs/OUTPUT_AND_DIAGNOSTICS.md) for the unified Output and Problems architecture, bounded limits, supported diagnostic formats, and navigation behavior.
See [docs/SYNTAX_HIGHLIGHTING.md](docs/SYNTAX_HIGHLIGHTING.md) for lexical tokenization, multiline state, incremental cache behavior, limits, palette, and known limitations.
See [docs/FIND_AND_REPLACE.md](docs/FIND_AND_REPLACE.md) for active-document literal search, navigation, replacement safety, limits, overlays, lifecycle, and keyboard shortcuts.
See [docs/FIND_IN_FILES.md](docs/FIND_IN_FILES.md) for project scope, bounded asynchronous search, patterns, dirty-document policy, lifecycle, results, and navigation.
See [docs/SYMBOL_INDEX.md](docs/SYMBOL_INDEX.md) for lexical symbol scanning, incremental indexing, storage bounds, Outline behavior, and Ctrl+T navigation.
See [docs/SIGNATURE_HELP.md](docs/SIGNATURE_HELP.md) for bounded active-call discovery, lexical candidate lookup, parameter parsing, popup behavior, staleness, limits, and known limitations.
See [docs/FIND_ALL_REFERENCES.md](docs/FIND_ALL_REFERENCES.md) for lexical target resolution, token filtering, bounded asynchronous scanning, confidence labels, dirty snapshots, cancellation, and stale-safe activation.
See [docs/INCLUDE_GRAPH.md](docs/INCLUDE_GRAPH.md) for project-local include scanning, deterministic resolution, reverse edges, transitive traversal, cycle diagnostics, dirty-document updates, lifecycle, limits, and F12 navigation.
See [docs/HEADER_SOURCE_OWNERSHIP.md](docs/HEADER_SOURCE_OWNERSHIP.md) for lexical ownership classification, bounded candidate generation, evidence, ambiguity, lifecycle, and navigation behavior.
See [docs/LIGHTWEIGHT_TYPE_INTELLIGENCE.md](docs/LIGHTWEIGHT_TYPE_INTELLIGENCE.md) for supported declarations, confidence, bounded alias/inference behavior, generation handling, Quick Type Info, and deferred semantics.
