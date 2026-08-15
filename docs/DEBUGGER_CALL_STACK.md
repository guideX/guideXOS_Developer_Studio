# Debugger Phase 7 — Bounded AMD64 Call Stack

Status: implemented for the hosted AMD64 Native ELF debug path. This phase
adds a real, bounded call stack for a stopped thread. It is intentionally a
frame-pointer unwinder, not a general-purpose unwind engine.

Selecting a Call Stack frame also rebuilds the Phase 9 Locals and Arguments view. Frame #0 has real GPR context; caller frames conservatively expose only locations supported by known frame data.
Phase 10 structured roots follow the selected frame and are rebuilt on frame change; nested children are lazy and use only memory-backed locations supported by that frame.

## Scope and evidence boundary

The supported target remains the Windows-hosted, little-endian ELF64 AMD64,
fixed-address `ET_EXEC` Native ELF runtime with load bias zero. The current
unwinder accepts only a valid stopped AMD64 register context and a ready
artifact mapper. It follows saved `RBP` links and reads target stack memory
through the debug ABI. It never dereferences a target address as a Developer
Studio host pointer.

The supported evidence boundary is:

- stopped current frame from the copied `RIP`, `RSP`, and `RBP`;
- caller frames from `[RBP] = previous RBP` and `[RBP+8] = saved return address`;
- ELF `STT_FUNC` names and DWARF line/source mapping from the exact hashed artifact;
- bounded stack ranges, frame count, frame delta, reads, cycle detection, and
  stale identity checks; and
- source navigation only for a mapped frame.

Step Out consumes the current frame's validated raw caller return address; there
is still no DWARF CFI execution, `eh_frame`/`eh_frame_hdr` unwinding,
inline-frame expansion, attach, or
optimized-code parity in this phase.

## Model

`DebugCallStack` records session, process, Native runtime, thread, stop,
mapper, artifact, selected-frame, validity, and stale identity. Each fixed
`DebugStackFrame` records the instruction address, raw return address, lookup
address, `RSP`, `RBP`, function range/name, source path/line/column, mapping
state, and confidence. The current frame is `ExactCurrent`; saved-return
frames are `FramePointer`; invalid or unmapped frames are shown explicitly.

The result is bounded to 64 frames. A single frame-pointer hop is bounded to
1 MiB. The unwind stops at an end sentinel, invalid/non-monotonic pointer,
cycle, target-read failure, outside-stack pointer, outside-target return
address, stale context, or frame limit. Partial results remain visible and
carry a termination reason; failure is never represented as a fabricated
complete stack.

## Target memory contract

The Server appends `stackLow` and `stackHigh` to the existing debug snapshot.
The Native runtime obtains these bounds from the active target thread's stack
marker. A stack read must provide the exact session/process/runtime/thread/stop
identity, request at most 16 bytes, and remain wholly inside the owned stopped
stack range. The Server accepts the read only while the matching trap or user
step is still stopped. Instruction reads remain a separate executable-image
operation and cannot be used as a stack-read substitute.

The Developer Studio callback receives bytes through the hosted command and
requires an exact 16-byte result for each frame link. The callback boundary is
owner-bound and generation-aware; a resumed target clears the call stack.

## AMD64 frame-pointer algorithm

1. Validate the stopped context, session generation, canonical `RIP`/`RSP`,
   AMD64 architecture, stack bounds, and mapper identity.
2. Emit frame #0 from the stopped context and map its `RIP` exactly.
3. Read 16 bytes at the current `RBP` through the target-memory callback.
4. Decode little-endian previous `RBP` and saved return address.
5. Require a canonical, executable saved return address. Enrich the caller
   using `savedReturnAddress - 1` as the lookup address.
6. Require a canonical, aligned, in-range, strictly increasing previous `RBP`
   with a bounded delta. Continue until a zero sentinel or a safety reason.

The `-1` rule is intentional: a saved return address points to the first
instruction after the call, while source rows and function ranges normally
describe the call instruction in the caller. The raw return address is kept
separately for diagnostics and identity.

## Symbol policy

The bounded ELF parser reads `.symtab` and its linked `.strtab`, retaining only
`STT_FUNC` symbols with nonzero addresses whose starts lie in executable
`PT_LOAD` segments. Names and function counts are capped; symbols are sorted
deterministically by address, size, and name. A nonzero symbol size uses a
half-open range. A zero-sized symbol is an exact-start match only, avoiding
guesses across adjacent functions. Unnamed, external, malformed, and
non-executable symbols are not presented as target function names.

Source enrichment reuses the existing sequence-aware DWARF line mapper and
project-relative path policy. If source mapping is absent, the frame remains
visible with its address/symbol state and an explicit unmapped status.

## UI and stale-state behavior

The Debug panel has a Call Stack tab. It displays the selected frame, bounded
rows, function or `<unknown>`, source path/line when mapped, and partial-stack
termination. Up/Down selects a frame and Enter or double-click navigates to a
mapped source frame through the existing workspace location API. Navigation
does not alter execution markers or ordinary editor history.

The call stack is built only after a real Paused transition. Continue, Step
Into, Step Over, Stop Debugging, process exit, failed commands, session restart,
or a changed stop identity clears or invalidates it. Selection requires the
current paused session and stop generation, so a stale row cannot issue a
target command or navigate as if it were current.

## Fixture and tests

`tests/fixtures/debugger-phase7` is built with `-g -O0 -fno-omit-frame-pointer`
and contains `gx_phase7_level1 -> gx_phase7_level2 -> gx_phase7_level3`. The
produced artifact is verified as ELF64 AMD64 `ET_EXEC` with executable load
segments, `.symtab`, `.debug_line`, `.debug_frame`, DWARF CFI v4, and explicit
RBP prologues. The model test covers nested frames, symbol/source enrichment,
the `returnAddress - 1` lookup, no-frame-pointer fallback, read failure, cycle,
misaligned/outside-stack pointers, invalid target returns, selection, and
stale-state rejection.

The existing Windows native debugger harness remains the runtime authority for
real VEH/trap behavior. Phase 8 Step Out consumes frame #0's raw caller return
address and rebuilds this stack after the real return trap, resetting selection
to frame #0. Selecting another row is inspection only and never changes the
execution frame. A full hosted Call Stack UI proof still requires the Windows
runtime/compositor lane; the model, ELF, ABI, and package boundaries do not
claim that visual proof by themselves.
Selecting a Call Stack frame also rebuilds the Phase 9 Locals and Arguments view. Frame #0 has real GPR context; caller frames conservatively expose only locations supported by known frame data.
