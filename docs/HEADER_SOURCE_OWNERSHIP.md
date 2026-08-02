# Header / Source Ownership

Header / Source Ownership is a project-local lexical relationship model. It
answers “which indexed code files are plausible counterparts for this file?”
It does not assert compiler-enforced translation-unit ownership, build-target
ownership, or semantic C++ ownership.

The implementation is in `src/developer_studio_ownership.h` and
`src/developer_studio_ownership.cpp`. It consumes the existing Symbol Index,
Include Graph, and Declaration–Definition Relationship Graph; it does not
reimplement either graph.

## Classification

Only these extensions are eligible:

| Kind | Extensions |
| --- | --- |
| C source | `.c` |
| C++ source | `.cc`, `.cpp`, `.cxx` |
| Header | `.h`, `.hh`, `.hpp`, `.hxx` |
| Inline header | `.inc`, `.inl` |

Extension matching is ASCII case-insensitive, while the original relative
path spelling is retained. Other text files, extensionless files, and unknown
extensions are not ownership candidates.

## Names and layout

The model keeps both an exact filename stem and a conservative normalized
stem. Normalization removes only the recognized suffixes `_win`, `_windows`,
`_linux`, `_posix`, `_unix`, `_guidexos`, `_hosted`, `_native`, `_amd64`,
`_x86`, `_arm`, `_arm64`, `_riscv`, `_impl`, `_internal`, and `_private`.
Ordinary underscore names are preserved. Suffix removal is weak evidence by
itself.

The recognized role roots are `include`, `includes`, `inc`, `header`,
`headers`, `src`, `source`, `sources`, `lib`, and `core`. After removing a
role root, the remaining module-relative path is compared. For example,
`include/build/project.hpp` and `src/build/project.cc` share the module path
`build/project`. A directory match is evidence, not proof.

## Candidate generation

Candidate pairs are generated only through bounded buckets:

1. exact stem;
2. normalized stem;
3. module-relative path;
4. resolved Include Graph edges and bounded transitive traversal;
5. Relationship Graph endpoint pairs; and
6. optional validated `OwnershipProjectMetadataPair` records.

Each pair is deduplicated through a bounded pair table. The implementation
never loops over every header against every source. A bucket can still be
ambiguous, and a per-bucket cap is reported as truncation rather than hidden.

## Evidence and ranking

Evidence is bounded to 32 records per candidate. Direct source-to-header
includes are strong positive evidence; transitive includes are moderate
evidence. A header including a source is retained as a conflict. Missing
include edges do not disqualify a pair.

Relationship Graph records are the strongest inferred lexical evidence. Exact,
Strong, and Possible relationship counts are kept separately, and duplicate
relationship identities are collapsed. Stale endpoints mark the candidate
stale instead of being treated as current proof.

Qualified symbol overlap can add evidence for shared qualified names,
namespaces, classes, method families, and global/static families. Common
unqualified names such as `Init`, `Create`, `Destroy`, `Result`, and `Config`
are not strong evidence by themselves. The shared-symbol examination is
bounded.

The ranking order is deterministic. It favors explicit metadata, relationship
confidence, exact/normalized names, module layout, direct/transitive includes,
and qualified overlap. Conflicting evidence, stale data, and variant mismatch
reduce the rank. Ties use confidence, exact-stem presence, relationship
counts, direct include presence, relative path, file kind, and file ID. Raw
scores are not displayed by the UI.

Confidence is rule-based rather than a single threshold:

- `Exact` requires a unique strong identity signal, normally an exact stem
  plus a direct include, exact relationship, or explicit metadata, without a
  meaningful conflict.
- `Strong` means several independent lexical signals agree.
- `Possible` means limited or weak evidence exists.
- `Ambiguous` means close candidates, intentional variants, or conflicts
  prevent a unique choice.
- `None` means no credible counterpart exists.

## Ownership forms

The graph preserves one-to-one, one-to-many, many-to-one, and many-to-many
groups. `Switch Header / Source` opens a unique Exact/Strong result directly;
several credible results use the bounded picker. A Possible-only result also
uses the picker.

A header is labelled header-only only when the lexical index contains a
definition in it and no credible external source counterpart exists. A
declarations-only header with no counterpart is reported as no counterpart. A
source with no credible header is source-only; private implementation files
and `main.cpp` are not errors.

Platform and generated variants remain visible. Active-target metadata, when
provided by a caller, is only a ranking hint. Generated candidates remain
labelled and are never silently hidden.

The current project model has no header/source pairing fields. The ownership
API therefore exposes an optional metadata-pair input and validates paths,
file kinds, and project containment through the inventory before accepting it.

## Lifecycle and generations

`OwnershipGraphService` permits one active build. Its states are collecting
files, generating candidates, collecting evidence, ranking, grouping,
completed, cancelling, cancelled, and failed. Polling consumes an explicit
work budget. Cancellation is safe and supersession cancels the old build before
starting the pending request. The last completed graph remains available while
a replacement is being built. A build is full-from-indexed-data: it does not
rescan source content when the Symbol Index and other supplied graphs are
current; differential updates are not claimed.

Graph identity records project, symbol-index, Include Graph, Relationship
Graph, and document generations. Activation checks project identity, graph
currentness, relative path validity, project containment, and target opening
before navigating. Open dirty documents are opened/reused through the existing
workspace controller, so their in-memory content remains authoritative.

## Command, picker, and panel

The command is `Switch Header / Source`. The shortcut audit found that `Ctrl+O`
already opens a workspace and `Ctrl+K` is not a safe key to reserve for a new
pending chord in this shell. The final binding is therefore the documented
fallback:

```text
Alt+O
```

No chord is installed, so there is no indefinite pending-chord state and the
existing `Ctrl+O` behavior is unchanged. `Alt+Shift+O` opens the `File
Ownership` inspection panel without navigating. The Project Explorer also
shows the Alt+O action in its contextual help.

The panel shows the current relative path, classification, build status,
confidence, candidate paths, and the first bounded evidence summary. For
multiple candidates it provides Up/Down, Page Up/Page Down, Home/End, Enter,
Escape, mouse wheel, and double-click activation. It does not display raw
rank scores.

When the caret is on an indexed symbol, switching first prefers an exact
Relationship Graph endpoint in the selected counterpart, then a strong
endpoint, then the beginning of the file. Overloads and unrelated same-name
symbols are not guessed. The existing shared navigation history is used:
the origin caret is pushed once, `Alt+Left` returns to it, and `Alt+Right`
continues to work. Ownership does not create a separate history.

## Interaction with existing commands

Ownership is file-level navigation. It leaves `F12` Go To Definition,
`Ctrl+F12` Go To Declaration, `Alt+F12` Switch Declaration / Definition,
`Shift+F12` Find All References, `F2` Rename, completion, signature help,
Include Graph (`Ctrl+Shift+I`), Outline, Go To Symbol, Build, Problems, and
Run semantics unchanged.

## Limits and status markers

The public contract bounds the project inventory at 100,000 code files,
stems at 100,000 buckets, a stem bucket at 1,000 files, initial candidates at
1,000 per file, retained candidates at 256 per file, total candidate pairs at
500,000, evidence at 32 records per pair, path text at 2,048 bytes, picker
results at 1,000 with 100 visible, shared symbols and relationship endpoints at
2,000 per pair, builds at five minutes, and a chord (if introduced by a
future UI) at two seconds. The embedded Native ELF configuration supplies
smaller caller-owned arrays and reports truncation explicitly.

Stable status names include `OWNERSHIP_NO_PROJECT`, `OWNERSHIP_GRAPH_STALE`,
`OWNERSHIP_BUILD_CANCELLED`, `OWNERSHIP_BUILD_TIMEOUT`,
`OWNERSHIP_CANDIDATE_LIMIT`, `OWNERSHIP_EVIDENCE_LIMIT`,
`OWNERSHIP_NO_COUNTERPART`, `OWNERSHIP_HEADER_ONLY`,
`OWNERSHIP_SOURCE_ONLY`, `OWNERSHIP_MULTIPLE_COUNTERPARTS`,
`OWNERSHIP_AMBIGUOUS`, `OWNERSHIP_RESULTS_TRUNCATED`,
`OWNERSHIP_TARGET_OUTSIDE_PROJECT`, `OWNERSHIP_ACTIVATION_STALE`, and
`OWNERSHIP_ACTIVATION_FAILED`.

Markers are emitted at build, resolution, activation, and cancellation
boundaries, including `ownership_build_begin`, `ownership_files`,
`ownership_candidates`, `ownership_groups`, `ownership_build`,
`ownership_activate`, and `ownership_build=CANCELLED`. There is no marker per
candidate pair.

## Known lexical limitations

This phase does not use Clang, ASTs, LSP, `compile_commands.json`, compiler
semantic ownership, build-target ownership, external SDK ownership,
cross-project ownership, module ownership, class-to-file refactoring,
automatic generation/moves/include edits, unused-file analysis, or system
header ownership. A relationship can remain Possible or Ambiguous when
macros, templates, overloads, conditional compilation, generated files,
platform variants, or unusual project layout defeat lexical evidence.

The dedicated model suite is `tests/ownership_test.cpp` and raises the CTest
count from the verified baseline of 17 to 18.
