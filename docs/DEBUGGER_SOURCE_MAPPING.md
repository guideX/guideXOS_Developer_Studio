# Debugger Phase 2 — DWARF / Source-Line Mapping

## Phase 3 integration

Phase 2 source-to-address mappings now feed the hosted software-breakpoint binder. A
mapped row retains every bounded executable address, while the hosted backend patches
only the deterministic primary address. A real owned trap supplies the normalized
target address to the Phase 2 reverse mapper; only then does the debugger show the
current source location and transition to Paused. Mapping alone remains Mapped, never
Verified or Hit.

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

## Deferred work

Runtime software breakpoints, trap/exception delivery, pause/continue after a
real stop, instruction-pointer correction, stepping, call stacks, registers,
memory, expressions, locals, watches, conditional/data breakpoints, and
attach/remote debugging remain future milestones. They require a Server
debugger ABI and runtime evidence that Phase 1/2 do not provide.
