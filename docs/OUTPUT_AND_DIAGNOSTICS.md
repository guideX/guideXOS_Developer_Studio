# Output and Diagnostics

Developer Studio uses one bounded, runtime-neutral `OutputService` for Build,
Run, application-facing, and Developer Studio messages. The service owns only
value records; it stores no UI pointers, process handles, host paths, or
secrets. The native UI queries the service for rows and Problems projections.

## Record model

`OutputRecord` contains monotonically increasing `sequence` and explicit
`operationId` values; source, severity, category, and stdout/stderr origin;
bounded text, project ID, project-relative path, and diagnostic code; optional
1-based line/column and end-location fields; and `hasLocation`, `isTruncated`,
and `isTerminal` flags.

Sources are DeveloperStudio, Build, Compiler, Linker, ArtifactValidator, Run,
Runtime, Application, AppModel, and System. Categories are General,
BuildLifecycle, BuildDiagnostic, Artifact, RunLifecycle, RuntimeDiagnostic,
ApplicationOutput, and Internal.

## Capacity and lifecycle

The fixed limits are 512 retained records, 128 records per operation, 20
retained operation slots, 512 bytes of record text, 160 bytes of project
relative path, 96 bytes of project ID, and 32 bytes of diagnostic code. Problems
are derived from retained records and are not a second source of truth.

Each Build and Run receives a new operation ID. A Build clears prior
`BuildDiagnostic` records for the current project at start, while ordinary
history remains until bounded eviction. Run retains its associated Build as a
separate operation and does not copy Build Problems into Run. A completed
operation rejects later appends as stale. When an operation or the total store
reaches a limit, the oldest eligible record is evicted and one warning record
is emitted: `Output truncated: record limit reached.` Repeated truncation
notices are suppressed. A terminal record is published exactly once.

Completed operations are evicted oldest-first when the operation-slot limit is
reached. Active operations are not evicted; the current workflow permits one
Build and one Run operation at a time.

## Channels and Problems

Channels are filters over the same records:

- Build: Build, Compiler, Linker, and Artifact Validator;
- Run: Run, AppModel, Runtime, and Application;
- Application: Application only;
- Developer Studio: DeveloperStudio and System;
- All: every source.

Problems include only Warning, Error, and Fatal records in BuildDiagnostic or
RuntimeDiagnostic categories. The default ordering is retained sequence
(compiler emission order), which is stable and tested. Ordinary Run lifecycle
messages and unparsed build lines are never Problems.

## Build integration and parser scope

BuildController starts an operation, clears current-project Build Problems,
publishes Build Started/project/target records, and forwards each new host
snapshot line once. The Server snapshot retains stream origin; stdout is
`StandardOutput` and stderr is `StandardError`. Recognized diagnostics become
Compiler or Linker records. Unrecognized lines remain ordinary Build records.
The terminal record includes elapsed time, warning/error counts, artifact
path/hash when available, and the bounded failure code.

The first parser supports only the formats used by the generated Native GUI
recipe:

```text
src\\main.cpp(12,5): error C1234: message
src/main.cpp:12:5: error: message
src/main.cpp:12: warning: message
LINK : fatal error LNK1104: message
```

Backslashes are normalized. Relative paths reject absolute prefixes, `.`, and
`..` segments. Absolute paths are accepted only beneath the project root and
are stored as project-relative paths. Outside-root, malformed, or overflowed
locations remain visible as diagnostics without a navigable location. Source
echoes, caret lines, and ordinary sentences that merely contain “error” are
not classified. The parser does not claim complete GCC/Clang/MSVC
compatibility, semantic analysis, IntelliSense, or perfect Unicode mapping.

## Run integration

F5 starts a Run operation and selects the Run channel. It emits Run requested,
Build required, Build completed or deployment skipped, artifact revalidation,
temporary deployment, launch, application state, cleanup, and one Run terminal
record. The Build-before-Run operation remains separately identifiable and its
compiler Problems remain Build Problems. Native application stdout capture is
deferred because the current Native App ABI has no safe development logging
surface.

## Problems navigation

Problems activation verifies the active project and project ID, accepts only a
validated project-relative path, opens or selects the existing document, and
sets the caret. Line and column are interpreted as 1-based byte positions from
the compiler output; tabs count as one stored byte and Unicode display width is
not inferred. Missing files and rejected paths report bounded error codes
without crashing. Out-of-range lines and columns clamp to the document end
while the original Problem remains unchanged. Dirty documents retain their
dirty state. Reusable editor operations are
`WorkspaceControllerOpenDocumentAtLocation` and
`WorkspaceControllerSetCaretPosition`; viewport and focus updates remain UI
responsibilities.

## UI behavior

The lower panel has Output and Problems tabs, channel cycling, bounded visible
rows, severity/source labels, a Clear action, error/warning counts, and the
`No problems found.` empty state. Mouse wheel and keyboard navigation adjust a
bounded scroll position. New output follows the tail only when the user is at
the bottom; scrolling upward preserves the manual position. Problems can be
activated with Enter or double-click. There is no ANSI terminal emulation,
command input, shell prompt, clickable URL, or terminal widget.

## Markers and error codes

Existing Build and Run markers remain emitted to the host log. Structured
operation-begin and operation-complete markers were added; ordinary records do
not generate one marker per line. Stable service/navigation codes include
`OUTPUT_OPERATION_NOT_FOUND`, `OUTPUT_OPERATION_STALE`, `OUTPUT_RECORD_LIMIT`,
`OUTPUT_TEXT_TRUNCATED`, `DIAGNOSTIC_INVALID_PATH`,
`DIAGNOSTIC_PATH_OUTSIDE_PROJECT`, `DIAGNOSTIC_FILE_NOT_FOUND`,
`DIAGNOSTIC_PROJECT_MISMATCH`, `NAVIGATION_NO_PROJECT`,
`NAVIGATION_OPEN_FAILED`, and `NAVIGATION_LOCATION_CLAMPED`.

## Validation

Automated coverage includes `developer_studio_output_test`, the existing
model/project/Build/Run tests, the CMake build, and the Native ELF build. The
real hosted UI sequence remains manual validation only; no live certification
is claimed unless the sequence is observed.
