# TOOLS12 — Cross-Tool Navigation History

## Outcome

**TOOLS12_HOSTED_VERIFIED.** Back and Forward history is implemented across Developer Studio navigation tools, the real hosted Server UI acceptance passed, both configured test suites passed, and isolated AMD64 and ARM64 packages were validated. Guest qualification remains pending for the .NET Support schedule.

## Architecture

`NavigationHistory` is owned by Developer Studio's main UI state and stores two fixed arrays of `NavigationLocation` records. A location contains the project ID and generation, document ID and buffer generation, validated project-relative path, caret byte offset, selection bounds, one-based line and column, and editor viewport line. It contains no pointers or source text.

The shared capture and restore helpers sit at the Tools/UI boundary. Each navigation tool captures its origin, performs its existing open or location operation, and records the origin only after a successful move to a different location. Ordinary typing and caret motion do not create entries. Consecutive duplicates are suppressed, and a new navigation clears Forward history.

Back and Forward first peek at a destination. The UI restores it through `WorkspaceControllerOpenDocumentAtLocation`, then commits the stack movement only after restoration succeeds. Failed restores leave the entry available for retry. Entries that are permanently stale, deleted, outside the active project, or from an earlier project generation are discarded conservatively. Project changes and project closure clear the history.

Go to File uses the hosted symbol index when it is available. The Native ELF profile does not have that database, so its bounded project-file inventory can opt into C/C++ source and header paths as a fallback. This keeps source files discoverable in the real hosted Native ELF UI without changing project metadata or build inputs.

## Navigation integration

The shared history records successful navigation from:

- Go to File
- Go to Line
- Find in Files
- Task List
- Document Outline
- Project Symbols

Existing definition, reference, and include-target navigation also records through the same model. Back/Forward restoration does not recursively add history.

The Tools menu shows **Navigate Back (Alt+Left)** and **Navigate Forward (Alt+Right)**. The existing Alt+Left and Alt+Right keyboard paths and the menu items call the same Back and Forward functions.

## Lifecycle and dirty-buffer safeguards

Every restore checks the active project identity and generation, validates a non-traversing relative path, and opens only through the existing workspace controller. A missing or out-of-project file cannot escape the current root.

If the original document ID and buffer generation are still open, Back/Forward restores the caret, selection, and viewport directly in that in-memory buffer, including unsaved edits. If a document has been replaced or reloaded, restoration uses the saved line and column with the workspace API's bounds handling. History does not save, discard, or duplicate source contents. The hosted acceptance verified that the disposable project remained byte-for-byte unchanged after navigation and after a deliberate unsaved edit was restored.

## Capacity and resource impact

| Target | Locations per direction | `NavigationLocation` | Static `NavigationHistory` |
| --- | ---: | ---: | ---: |
| Hosted | 64 | 928 bytes | 118,792 bytes (about 116 KiB) |
| Native ELF | 16 | 416 bytes | 13,320 bytes (about 13 KiB) |

The history is a static global in BSS; it allocates no heap memory and retains no editor buffers. The largest new transient payload is two locations on the call stack: 1,856 bytes hosted or 832 bytes Native ELF. No large stack arrays were added. The existing Native ELF stack contract remains 512 KiB. Clearing a history resets the fixed storage and counts; there is no dynamic cleanup.

## Hosted UI evidence

The extended `smoke-developer-studio-document-outline.ps1` harness launches the real experimental Server with the isolated AMD64 Native ELF package. It exercised the Tools menu and keyboard shortcuts through the actual UI, navigated across Go to File, Project Symbols, and Find in Files, verified repeated Back and Forward, kept an unsaved edit in the buffer, and checked project-switch invalidation. The existing Document Outline and Project Symbols stale-result, cancellation, and source-preservation flows also passed.

- Direct acceptance transcript: `out/TOOLS12/hosted-nav-acceptance3/navigation-history-hosted-ui.log`
- Debug CTest transcript: `build-tools12-debug-retry/tools12-hosted-ui-evidence/navigation-history-hosted-ui.log`
- Release CTest transcript: `build-tools12-release-retry/tools12-hosted-ui-evidence/navigation-history-hosted-ui.log`

The configured hosted test emitted successful navigation, clean-close, and byte-for-byte fixture-integrity checks. This is real hosted UI acceptance, not a model-only result.

## Build and test qualification

- Clean Debug build: `cmake --build build-tools12-debug-retry --clean-first --parallel 8` — 124/124 build steps passed.
- Clean Release build: `cmake --build build-tools12-release-retry --clean-first --parallel 8` — 124/124 build steps passed.
- Debug CTest: 43/43 passed, including both hosted UI tests.
- Release CTest: 43/43 passed, including both hosted UI tests.
- The two Native ELF builder runs skipped the model and project fixture steps because those checks are covered by both CTest suites and share fixture paths. The builder's remaining architecture-specific checks and ELF builds passed.

## Isolated package audit

Packages were built under `out/TOOLS12/amd64-acceptance-stage` and `out/TOOLS12/arm64-acceptance-stage`, then assembled under `out/TOOLS12/dual-architecture-package`. The Server checkout was not modified and its production ELF files were not overwritten.

| Architecture | ELF | Entry point | Size | SHA256 |
| --- | --- | ---: | ---: | --- |
| AMD64 | ELF64 little-endian ET_EXEC; no section metadata | `0x500d36f0` | 1,262,228 bytes | `9CBC32163417A553B38A1F01EF69CDBAF49B99C58B9D63D0F06E2E9A31C1AC16` |
| ARM64 | ELF64 little-endian ET_EXEC; no section metadata | `0x500f60d0` | 1,448,964 bytes | `9CEB966F4AD74EE70E138F7A2BA1797FCE9B50A007F3E57E74BD3C5EAD365AB7` |

The package validator confirmed the manifest and exactly two architecture ELFs; the content audit passed.

## Qualification status and next step

No QEMU run was started. Guest acceptance remains pending until .NET Support schedules it. Recommended next step: coordinate the guest qualification window with .NET Support, then run the package through that guest acceptance path. TOOLS13 was not started.

## Git record

- Source checkout: `D:\dev\guideXOS_DeveloperStudio_Tools_Source`
- Branch: `DEVELOPER_STUDIO_TOOLS`
- Starting HEAD: `c4ad0fd078f6f8a5fb78c11546b2822b58e8d228`
- Server checkout HEAD: `a13768dea9e5808baadb3c2908a8fca463505f16` (unchanged)
- Local commit: recorded after final audit.
- Push: none.
