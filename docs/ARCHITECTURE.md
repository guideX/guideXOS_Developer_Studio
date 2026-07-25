# guideXOS Developer Studio Bounded Editing Phase Architecture

Status: minimal workspace and source editor

## Integration

Developer Studio is an independently maintained Native ELF application consumed by guideXOS Server's existing App Model. The Developer Studio repository owns the manifest, native source, runtime-neutral models, controller, tests, build script, and documentation. The build stages the ordinary package at `Apps/DeveloperStudio/app.json` plus `bin/amd64/developerstudio.elf`.

The canonical route remains:

```text
com.guidexos.developerstudio
  -> AppRegistry
  -> DesktopService::LaunchApp
  -> NativeElf launch resolver
  -> guidexos-c-abi-v1
  -> gx_main
```

No plugin, extension, package, or loader architecture is introduced. Developer Studio is not a built-in application and is not auto-launched at Server startup.

The active target profile is the one proven route:

```text
id:           guidexos.amd64.hosted.native
architecture: amd64
ABI:          guidexos-c-abi-v1
machine:      Windows hosted guideXOS Server
SDK:          guideXOS Native SDK v1
toolchain:    LLVM/LLD Native ELF
runner:       guideXOS Hosted Native ELF Runtime
maturity:     experimental
```

## Layering

```text
UI shell
  |
Workspace/document controller
  |
Workspace/document/text-buffer models
  |
Filesystem abstraction
  |
guideXOS Native ABI
```

`main.cpp` owns painting and input routing only. `developer_studio_workspace.*` owns workspace/document commands and the filesystem adapter. `developer_studio_models.*` owns normalized paths, bounds, document state, text-buffer mutation, ordering, and error codes. File I/O is supplied through a small callback interface backed by the Native ABI.

## Native ABI audit and decisions

The existing ABI provided window creation, retained rectangle/text drawing, key/mouse events, package-relative reads, and monotonic ticks. It did not provide a Native ABI folder picker, directory enumeration, arbitrary hosted workspace reads/writes, multiline text input, caret/scroll controls, clipboard, or atomic replacement.

The Server checkout now appends four hosted workspace calls to the ABI table:

- `file_stat`;
- `file_read_workspace`;
- `file_list`; and
- `file_write_all`.

They use existing `filesystem.read` and `filesystem.write` manifest permissions and accept an explicit user-entered hosted path. Developer Studio still normalizes the selected root, rejects traversal, and enforces root containment for every document operation. Package-relative `file_read_all` remains unchanged.

Because there is no reusable Native ABI picker or multiline editor, Developer Studio owns the temporary path-entry dialog, fixed-size text buffer, caret/key/mouse editing, vertical scroll state, tabs, and Save/Discard/Cancel modal. This is intentionally a phase-local editing surface, not a general widget framework.

## Workspace and document limits

- one open workspace root at a time;
- 128 entries in the visible directory listing;
- immediate directory enumeration with deterministic directory-first, case-insensitive ordering;
- 8 maximum open documents;
- 256 KiB maximum editable file size;
- 8 navigated workspace path segments and 768-byte model paths (the hosted ABI accepts up to 240 bytes for an entered host path);
- slash-normalized, case-insensitive path comparison;
- `..` and equivalent traversal outside the selected root are rejected;
- embedded-NUL content is rejected as binary-looking;
- output history is bounded to four concise lines; and
- navigation is one directory at a time, with no unbounded recursive scan.

Supported text extensions are `.c`, `.cc`, `.cpp`, `.h`, `.hh`, `.hpp`, `.txt`, `.md`, `.json`, `.xml`, `.css`, `.html`, `.htm`, `.js`, `.ts`, `.ps1`, `.cmd`, `.bat`, `.cmake`, `.ini`, `.cfg`, `.log`, `.mk`, `.yml`, and `.yaml`.

The path-entry dialog is temporary. Save uses direct truncate/write/flush because the ABI has no atomic replacement operation. The write result and byte count are verified; dirty state is retained on failure. Atomicity is not claimed.

## Diagnostics and tests

Stable app markers include:

```text
workspace_open=PASS|FAIL reason=<code>
document_open=PASS|FAIL reason=<code>
document_dirty=TRUE
document_save=PASS|FAIL reason=<code>
document_close=PASS
unsaved_close=SAVE|DISCARD|CANCEL
application_close=PASS
```

Document contents and full paths are not emitted by Developer Studio markers. The model test covers normalization, traversal, ordering, duplicate documents, active switching, dirty transitions, open/document/file-size limits, binary detection, and save success/failure transitions. `tests/smoke-workspace.ps1` creates only its own temporary fixture and proves the real host-filesystem controller workflow.

The hosted validation split is deliberate: model and filesystem-contract tests are automated; the real App Model launch is marker-asserted through the Server shell; keyboard and mouse editing in the compositor are additionally manual validation until a bounded input-driving harness exists. The current hosted proof covers package discovery, AMD64 ELF validation, ABI negotiation, `gx_main`, construction, target profile, window creation, initial render, and clean teardown.

The environment created three automatic Developer Studio checkpoint commits during this phase: `d4787670522e4c242f9f012664dcf126944c46dc` (initial repository), `f1708ac0926f0be1541d776e3e85eb1e8f35ffea`, and `6b26336bfb964d5d38b88e47b7a259e9a57166f0`. They were inspected without rewriting history. The Developer Studio `.gitignore` is at the repository root and ignores build output and ELF binaries; none are tracked in this repository.

## Deferred work

There is no compiler integration, project wizard, project-file parsing, build command, Run/Debug, language server, syntax highlighting, completion, navigation, refactoring, Git integration, terminal, visual designer, website preview, game editor, container tooling, remote deployment, extension loading, theme selection, session restore, crash recovery, binary/hex editing, selection, clipboard, or undo/redo.

## Hosted Server dependency and path policy

The hosted Server checkout requires `third_party/stb/stb_image.h` for `png_loader.cpp`. The selected header is the identical historically used guideXOS vendor file from `D:\dev\guideXOSServerV0.2\third_party\stb\stb_image.h`: stb_image v2.30, upstream revision `1692fe6e21ce7b7abbc6fcb6d1d3ff6ebe0b8537`, SHA-256 `1F8C1B6B408F26E3B20CBFBBD4758AFB3DC9B837FF1E17C258928F406148A87C`, public-domain license. It is restored at `Server\third_party\stb\stb_image.h`; Server `.gitignore` ignores other vendor content but permits this exact required header. No arbitrary latest header or package manager is used.

The four appended filesystem calls are hosted-development extensions. Their paths are explicit absolute UTF-8 host paths and are permitted only when the manifest grants `filesystem.read` or `filesystem.write`. Server validation rejects empty, relative, traversal, device/UNC, control-byte, invalid-UTF-8, overlong, and symlink-component paths; the Developer Studio model additionally enforces containment under the selected workspace root. The hosted Server does not currently know that selected root, so these calls are not a production sandbox or a bare-metal VFS contract. Limits are 240 path bytes, 256 KiB per workspace read/write, 128 visible Developer Studio entries, and 8 open documents. Reads are full-file and require a sufficiently sized buffer; list order is directory-first and deterministic; writes truncate and flush directly, with no atomic-replacement guarantee.
