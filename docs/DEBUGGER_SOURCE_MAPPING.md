# Debugger Phase 2 — DWARF / Source-Line Mapping

## Phase 7 call-stack enrichment

Callers discovered from the bounded AMD64 frame-pointer chain keep their raw
saved return address, but source and function lookup use `returnAddress - 1`.
This maps the call site in the caller rather than the first instruction after
the call. The stack contract and partial-result policy are documented in
[DEBUGGER_CALL_STACK.md](DEBUGGER_CALL_STACK.md).

## Phase 6 Step Over lookup

Step Over uses the same sequence-aware reverse mapper as Step Into. The
controller records the exact return address of a decoded call, validates that
address against the executable image, and resolves the real internal return
trap back to a source location. A source line is never inferred from the
called function's line number or by adding one to the current line. Same-line
return stops are suppressed through bounded real execution; a user breakpoint
at the return address takes precedence.

## Phase 5 source-step lookup

Source-level Step Into compares canonical project-relative source paths and
line numbers. Columns remain metadata and do not create separate visible stops.
Address lookup honors DWARF line-program sequence boundaries, so a row range is
never carried across `DW_LNE_end_sequence`. Unmapped instructions are stepped
through under the bounded source-step policy and do not receive fabricated
source locations. See [DEBUGGER_STEPPING.md](DEBUGGER_STEPPING.md).

## Phase 3 integration

Phase 2 source-to-address mappings now feed the hosted software-breakpoint binder. A
mapped row retains every bounded executable address, while the hosted backend patches
only the deterministic primary address. A real owned trap supplies the normalized
target address to the Phase 2 reverse mapper; only then does the debugger show the
current source location and transition to Paused. Mapping alone remains Mapped, never
Verified or Hit.

The Phase 3B fixture proof exercises this contract through the Developer Studio UI:
`tests/fixtures/debugger-phase3b/src/main.cpp:20` resolves to the primary linked
address `0x20001218` for the fixed-base image at `0x20000000`. The stop address is
validated against the owned binding before reverse mapping and source navigation;
the UI does not infer a stop from a source-line marker.

Phase 2 adds truthful source-line evidence to the Phase 1 debugger foundation.
It maps a project-relative source file and line to one or more linked
instruction addresses, and maps an address back to a source location. It does
not add process suspension, trap delivery, software breakpoints, register
access, or breakpoint-hit events.

## Artifact contract

The existing hosted Native ELF build remains the source of truth for the
target identity and artifact hash. A debug session requests the existing build
service with the `DebugSymbols` configuration. That configuration adds:

- `-g` and `-O0`;
- `-fdebug-compilation-dir=<repository root>`; and
- `-fdebug-prefix-map=<repository root>=.`.

The target remains freestanding C++11, `x86_64-unknown-elf`, statically linked
by `ld.lld` as ELF64 little-endian AMD64 `ET_EXEC`, with entry point `gx_main`.
The current runtime has no relocation or PIE/load-bias support, so Phase 2
records a zero load bias and uses the linked addresses directly. Ordinary Run
and Build continue to request the existing `Debug` configuration.

The build service already captures the expected relative artifact path, exact
file size, SHA-256, architecture, and validation state. The mapper reads that
same artifact through the existing workspace filesystem API; it does not
invoke `readelf`, `objdump`, GDB, LLDB, or another external debugger at
runtime.

## Parser and mapping model

`src/developer_studio_debug_symbols.*` contains a direct bounded parser for
the artifact shape emitted by the current toolchain. It supports DWARF line
tables in versions 4 and 5, including the DWARF 5 directory/file tables and
the forms used by Clang (`line_strp`, `strp`, `udata`, fixed-width data,
`sec_offset`, flags, and `data16`). It handles the standard and extended line
program operations needed by the current artifact, including address changes,
line/file/column changes, `set_address`, `end_sequence`, and statement flags.

The parser validates ELF headers, section-table bounds, file-backed section
ranges, line-unit lengths, ULEB/SLEB values, opcode operands, and string
references before consuming them. It retains no pointer into the input
buffer. Unsupported versions, forms, architectures, and opcodes produce an
explicit error rather than an approximation.

Source paths are normalized to `/`, have `.` and `..` components resolved, and
must remain under the active project root. The debug-build prefix map makes
the project source paths stable (`./src/...` becomes `src/...`). System and
external headers are counted for diagnostics but are not accepted as active
project source mappings.

Forward lookup aggregates rows by normalized source file and line. A line may
have up to eight distinct addresses; the primary address is the first
statement address when one exists, otherwise the lowest address. Reverse
lookup uses exact or half-open row ranges and deterministic row ordering.

## Identity and state

Every loaded mapper records project ID/root and generation, target profile,
architecture, executable path, file size, SHA-256, load bias, and mapper
generation. A lookup is rejected as stale when any artifact identity or
project generation component no longer matches.

Mapper states are:

- `Empty`: no artifact is loaded;
- `Ready`: line information was parsed and indexed;
- `Failed`: the artifact is absent, malformed, unsupported, truncated, or has
  no usable line rows; and
- `Stale`: reserved for an invalidated loaded identity.

Breakpoints use the following truthful distinction:

- `Pending`: no usable mapping is available, or the requested source line is
  absent;
- `Mapped`: the source line resolved to linked addresses, but the hosted
  backend has not bound a runtime breakpoint;
- `Verified`: reserved for a future backend binding result;
- `Rejected`, `Disabled`, and `Stale`: explicit lifecycle/error states.

`Mapped` is therefore an inspection result, not evidence that execution will
stop. The hosted backend still advertises only launch and owner-bound stop.

## Lifecycle integration

Start Debugging builds with debug symbols, reads the exact successful artifact,
loads the mapper, records the target identity, maps existing breakpoints, and
then starts the existing hosted Development Run backend. F9 and the
Breakpoints panel re-run mapping after an enabled breakpoint changes. A failed
rebuild or identity mismatch clears mapped addresses and marks enabled
breakpoints stale. Closing the workspace resets the mapper and breakpoint
artifact bindings.

The Debug Session panel exposes mapper state, source-file count, line-row
count, and artifact path. Mapping diagnostics use the same bounded output and
error reporting conventions as the rest of Developer Studio.

## Bounds and tests

The implementation bounds the input ELF and each file-backed section at 16 MiB;
the section table at 256 entries; directories at 128; files at 1,024; project
source files at 512; line rows at 131,072; line keys at 32,768; sequences at
2,048; and addresses per source line at eight. `.bss` virtual size is checked
for ELF range validity but is not treated as file-backed debug input.

`tests/debug_symbols_test.cpp` covers a synthetic DWARF 5 ELF fixture,
forward and reverse mapping, multiple addresses on one line, path containment,
artifact identity mismatch, malformed ELF, unsupported DWARF version,
truncated line data, missing `.debug_line`, breakpoint `Mapped`/`Stale`
transitions, and bounded error propagation.

The current packaged Developer Studio ELF was also parsed directly: DWARF 5,
43 project-visible source files, 78,892 line rows, 23,022 line keys, and 24
sequences. Five initial line/address pairs matched `readelf` decoded-line
output, and reverse lookup returned the original normalized source location.

## Phase 4 integration evidence

The real hosted path captures and validates the exact artifact SHA-256 before symbol
loading and before every debugger command. It preserves project-relative source
identity, project/source generations, target address validity, and the loaded artifact
identity across the launch. The proof reached `debug_state=PAUSED_BREAKPOINT`,
`debug_source_navigation=PASS`, and `debug_execution_marker=PASS` after the native
trap was observed. A Phase 4 Continue preserves the exact normalized address and
source identity while the hosted backend restores the original byte, rewinds RIP,
sets AMD64 TF, resumes the trapped thread, consumes only the matching real
`EXCEPTION_SINGLE_STEP`, and rebinds `0xCC`. The current source marker is cleared
only after the backend publishes Running.

## Deferred work

Call-aware Step Over is documented in [DEBUGGER_STEP_OVER.md](DEBUGGER_STEP_OVER.md)
and reuses this mapper for the post-return source stop. The bounded Phase 7
Call Stack is documented in [DEBUGGER_CALL_STACK.md](DEBUGGER_CALL_STACK.md).
User-visible Step Out, arbitrary memory, expressions, locals, watches,
conditional/data breakpoints, and attach/remote debugging remain future
milestones. Phase 4 uses the mapping only to identify the exact physical address;
it does not infer source-level stepping from a single machine instruction.
