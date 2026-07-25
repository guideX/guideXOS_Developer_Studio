# guideXOS Developer Studio

guideXOS Developer Studio is the early native development-environment shell for guideXOS Server.

The current bounded phase proves the first useful source workflow:

- a manifest-driven Native ELF application with the canonical App Model ID `com.guidexos.developerstudio`;
- a native IDE shell with a bounded Explorer, document tabs, diagnostics, and status regions;
- runtime-neutral Workspace, Project, Target Profile, Capability, and maturity models;
- open workspace, browse, open, edit, save, switch, and safely close document behavior;
- a truthful AMD64 hosted guideXOS target profile; and
- deterministic model, filesystem-workflow, package, and hosted App Model smoke coverage.

Compiler integration, project templates, syntax highlighting, IntelliSense, debugging, Git integration, visual design tools, and multi-architecture orchestration remain deferred.

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
