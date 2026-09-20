# Developer Studio Debugger Workspace Persistence Design

**Date:** 2026-09-18  
**Phase:** 28O  
**Prerequisite:** Phase 28N Source Editor Debugger Integration  
**Standalone branch:** `phase28o-debugger-workspace-persistence`

## Goal

Persist project-scoped source-level breakpoint policies and Watch expression
strings across Developer Studio close/relaunch and project reopen, while
materializing fresh manager-owned runtime breakpoint identities for every new
Debug generation.

## Existing architecture findings

Developer Studio already has a project-root `guidexos.project` file, a bounded
JSON cursor/serializer in `developer_studio_projects.*`, and a
`WorkspaceFileSystem` callback boundary backed by the Native ABI. The workspace
model derives canonical project-relative paths from the active project root.

There is no existing project/session/settings persistence layer and the file
ABI exposes bounded read/write/create/remove operations but no rename or atomic
replace operation. Build source enumeration is rooted in the project source
directory, so metadata beside `guidexos.project` is not compiler input.

The packaged debugger has two relevant layers: the Developer Studio UI keeps
configured presentation data and calls the existing server source-breakpoint
manager, while the manager owns live IDs, mapping, patch installation, raw hit
counts, and generation identity. Phase 28O will preserve that authority.

## Storage decision

Store the configuration at:

```text
<project-root>/guidexos.debugger.json
```

Use a recovery companion at:

```text
<project-root>/guidexos.debugger.json.bak
```

This follows the current project-root metadata convention because no separate
workspace-local storage API exists. It is project-scoped, intentionally user
debugger state, and currently lives in the project tree; source-control policy
will be documented rather than invented by the implementation. Both files are
outside `sourceRoot` and therefore excluded from the compiler's source input.

The serializer builds the complete bounded document in memory, writes the
backup first, verifies the callback's byte count, then writes the primary file
and verifies it. A load accepts a valid primary file first and falls back to a
valid backup only when the primary is missing or invalid. A failed write is
reported through bounded UI status and never invalidates the in-memory model.
There is no atomic replacement claim because the current ABI cannot provide
one.

## Version-1 schema

The exact persisted shape is deterministic JSON:

```json
{
  "version": 1,
  "breakpoints": [
    {
      "sourcePath": "src/main.cpp",
      "line": 13,
      "column": 1,
      "enabled": true,
      "action": "BREAK",
      "condition": "input == 10",
      "hitPolicy": "EQUAL",
      "hitThreshold": 3,
      "logTemplate": ""
    }
  ],
  "watches": ["input + delta", "adjusted * 2"]
}
```

The parser rejects malformed JSON, duplicate or unknown fields, missing
required fields, unsupported or malformed versions, invalid enum names, zero
or otherwise invalid thresholds, overlong strings, over-capacity collections,
duplicate canonical source/line specifications, and invalid project-relative
paths. Parsing is all-or-nothing; a rejected file produces an empty safe
configuration plus a bounded load error, without issuing runtime manager
commands.

The persisted breakpoint capacity is 8, matching the real packaged manager.
The persisted Watch capacity is 8, matching the Developer Studio UI capacity.
Condition, log-template, and Watch strings use the existing 256-byte expression
and log-template limits. Source paths use `kMaxProjectPathBytes` and are
canonicalized with the same slash-normalized, traversal-rejecting path rules
used by Phase 28N source mapping.

## Persistent and runtime models

Add a UI-independent bounded `DebuggerWorkspace` model containing only:

- ordered `DebuggerWorkspaceBreakpoint` records with source path, line/column,
  enabled state, action, condition, hit policy/threshold, and log template;
- ordered Watch expression strings; and
- bounded load/save error status.

Do not add runtime ID, backend binding, target address, original byte,
installed byte, generation, session handle, raw hit count, condition result,
Watch result/error, selected frame, register context, execution/inspection
marker, Locals/Arguments, Output, or temporary stepping state to this model.

The editor projection will accept configured rows with `id == 0` before Debug,
live rows with a current-generation manager ID during Debug, and an explicit
unresolved visual/status state when source mapping fails. Persistent identity
is the ordered configuration record matched by canonical project-relative path
and source line/column; it is never treated as a backend breakpoint ID.

## Lifecycle and data flow

### Project open

After `WorkspaceControllerOpenProject` succeeds, reset all debugger runtime
state, initialize the empty configured model, and load the active project's
primary/backup debugger file. A successful load populates the configured gutter,
Breakpoints pane, and Watch pane before any Debug session exists. A failed load
leaves the project usable with a safe empty configuration and bounded status.

Opening another project first clears the old configured model and runtime
projection, then loads only the new project's file. No project ID or basename
fallback is used for matching.

### Configuration mutation

F9, Breakpoints-pane remove, enable/disable, action toggle, condition edit,
hit-policy edit, threshold edit, log-template edit, and Watch add/edit/remove
mutate the configured model in the user's current order. If a live manager
entry exists, the existing authoritative manager command is performed first;
the configured model is updated only after acceptance. Every accepted mutation
immediately saves the project configuration and reports a bounded save failure
without discarding the accepted in-memory state.

When no Debug generation exists, F9 creates a configured breakpoint directly
with no fabricated runtime ID. Disabled configured entries remain visible and
cannot install an active breakpoint.

### Debug generation start

After the existing build, target, source-map, and session setup succeeds:

1. Clear any previous live ID/address/binding association and runtime results.
2. Iterate configured breakpoints in persisted order.
3. For each enabled entry, canonicalize its source path and call the existing
   source-breakpoint manager add command.
4. Apply the persisted condition and BREAK/LOG/hit-policy/threshold/template
   through the existing manager policy command.
5. Read the authoritative manager snapshot and associate the returned ID with
   the current configuration record only for this generation.
6. Leave disabled entries configuration-only and mark unresolved entries with
   bounded status when the source map cannot resolve them.
7. Refresh the source-editor gutter and Breakpoints pane from the configured
   rows plus current live status.

No persistence code patches target memory or bypasses manager APIs. A new
generation therefore starts raw hit counts at zero and receives fresh IDs and
addresses even when its source-level configuration is unchanged.

### Debug teardown and application close

On stop, session teardown, project close, or application close, clear live
manager mirrors, Watch results, selected-frame state, output snapshots, and
execution/inspection markers. Retain and save only the configured source-level
model. Normal application close emits the existing close markers plus bounded
debugger save status.

## Unresolved and stale behavior

If a configured path/line no longer maps to executable code, keep the record in
the persisted file, do not delete it, and do not fabricate an ID or patch.
Render it as unresolved with a bounded message. A later rebuild or source
correction may materialize it again. Source edits preserve dirty buffers and do
not save/reload source text as a side effect of debugger persistence.

## ABI and repository boundaries

The implementation uses the existing filesystem and debugger calls. No Native
debugger command, debugger C ABI version, compiler object ABI, or App Model ABI
change is planned. The server repository receives only the normal packaged
Developer Studio artifacts and the narrowly scoped Phase 28O fixture/validation
evidence needed for the real guest proof, unless implementation evidence shows
an existing interface is insufficient.

The standalone `main` branch remains untouched. Work starts from
`77d861f0e00b9d0db1943cdeebded474fac90727`. The live server baseline is
`b1ea3d152fe4ca993b83f7ae9d50c8d84d844763`, which is one existing packaging
commit after the prompt's stale `88a1e81f...` reference.

## Verification strategy

Use test-first cycles for:

- serializer/parser round trips, deterministic ordering, exact schema version,
  canonical source identity, all policy fields, capacity, bounded strings,
  malformed/future/truncated/duplicate/invalid inputs, and project isolation;
- two-slot save/fallback behavior and failed writes;
- configured-before-Debug projection, live-ID absence, materialization, fresh
  generation IDs/addresses, unresolved retention, enable/remove persistence,
  Watch ordering and result reset, and runtime marker/frame reset; and
- regression coverage for existing Phase 28N source-editor behavior.

Then run the complete standalone CTest/host suites, PowerShell parsing and
`git diff --check`, normal AMD64/ARM64 package builds and audits, relevant
server debugger/backend regressions, and three fresh isolated packaged QEMU
boots. At least one boot must prove restored LOG output, hit-count reset, and
disabled-breakpoint behavior. The final Phase 28O document will record actual
package sizes/hashes, boot markers, ABI values, storage lifetime, limitations,
physical mouse status, clean worktrees, and nothing-pushed status.

## Out of scope

Cloud sync, cross-user sharing, runtime hit-count/ID/address persistence,
selected execution state, Watch folders, global evaluation, relocation of moved
statements, breakpoint groups, import/export, data/exception/remote/attach
breakpoints, and unrelated UI or ABI redesign remain out of scope.
