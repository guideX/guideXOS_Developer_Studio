# guideXOS Developer Studio

guideXOS Developer Studio is the early native development-environment shell for guideXOS Server.

The current bounded phase proves the first useful source workflow:

- a manifest-driven Native ELF application with the canonical App Model ID `com.guidexos.developerstudio`;
- a native IDE shell with a bounded Explorer, document tabs, diagnostics, and status regions;
- runtime-neutral Workspace, Project, Target Profile, Capability, and maturity models;
- open workspace, browse, open, edit, save, switch, and safely close document behavior;
- version 1 `guidexos.project` metadata, Native GUI Application creation, and project loading;
- a truthful AMD64 hosted guideXOS target profile; and
- deterministic model, filesystem-workflow, package, and hosted App Model smoke coverage.

Compiler integration inside Developer Studio, additional project templates, syntax highlighting, IntelliSense, debugging, Git integration, visual design tools, and multi-architecture orchestration remain deferred.

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

The generated `build.ps1` is an external build command, not Developer Studio build integration. It requires an explicit `-ServerRoot` and accepts `-SdkInclude`, `-PackageRoot`, and `-SkipReadElf`; it contains no Developer Studio or Server checkout absolute path. The generated CMake file likewise requires a configurable `GUIDEXOS_SERVER_ROOT`. A typical external build is:

```powershell
.\\build.ps1 -ServerRoot D:\\path\\to\\guideXOSServer
```

Generation is byte-for-byte deterministic for identical inputs: no timestamps, usernames, machine paths, random IDs, or session state are embedded. It creates only a new destination or an empty pre-existing destination, tracks every file and directory it creates, verifies the required layout, parses the generated metadata and manifest, and removes only tracked files and empty directories after a partial failure. Non-empty destinations are rejected before changes.

## Workspace workflow

Use `File -> Open Workspace` or `Ctrl+O`, then enter an absolute hosted path in the temporary path-entry dialog. The Explorer shows a bounded directory listing; activate a supported text file to open it in a document tab. `Ctrl+S` saves the active document and `Ctrl+Shift+S` saves all named documents. `Ctrl+W` closes the active tab.

Supported text extensions are `.c`, `.cc`, `.cpp`, `.h`, `.hh`, `.hpp`, `.txt`, `.md`, `.json`, `.xml`, `.css`, `.html`, `.htm`, `.js`, `.ts`, `.ps1`, `.cmd`, `.bat`, `.cmake`, `.ini`, `.cfg`, `.log`, `.mk`, `.yml`, and `.yaml`. Unknown, binary-looking, and oversized files are visible when practical but are not opened as editable text.

Limits are intentionally explicit: 128 visible entries per directory, 8 open documents, 256 KiB per editable file, 8 navigated workspace path segments, 768-byte model paths (the hosted ABI additionally bounds an entered host path to 240 bytes), and bounded output history. Directory enumeration is immediate-directory only and never follows symlinks or recursively scans without a user navigation action.

The editor supports caret movement, insertion, Enter, Backspace, Delete, arrows, Home, End, mouse placement, vertical wheel scrolling, visible line separation, dirty tracking, tabs, and save failure preservation. It has no syntax highlighting, selection, clipboard, undo/redo, code completion, folding, or semantic services.

## Build

From this repository, with the guideXOS Server checkout at its documented sibling path:

```powershell
powershell -ExecutionPolicy Bypass -File .\build.ps1
```

The script uses the Server checkout's existing Native SDK headers and stages the application package under `Server\Apps\DeveloperStudio`. It writes no generated binaries into this repository.

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
