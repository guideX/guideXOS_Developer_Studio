# Developer Studio Phase 27K — `break` / `continue` and bounded loop targets

Phase 27K adds unlabeled `break` and `continue` for `while` loops only.

## Grammar

The statement grammar adds:

```text
statement :=
      declaration
    | assignment
    | log_statement
    | return_statement
    | if_statement
    | while_statement
    | break_statement
    | continue_statement
    | block

break_statement := "break" ";"
continue_statement := "continue" ";"
```

`break` and `continue` are lexer keywords only when identifier boundaries
end the word. `breaker`, `breakfast`, `continued`, and `continueValue` remain
ordinary identifiers.

## Target-neutral representation

The parser emits `StatementKind::Break` and `StatementKind::Continue`. These
statements carry source location and semantic intent; they do not contain
AMD64 opcodes, label numbers, or machine-code offsets.

The parser tracks loop depth for two source checks: rejecting either statement
outside a loop and rejecting nesting beyond the existing bound. It preserves
the existing conservative return analysis: control transfer is not a return,
so a loop containing only `break` or only `continue` does not satisfy
`gx_main`'s required return path.

## Bounded loop-target stack

The AMD64 emitter owns a per-emission stack of:

```text
LoopTarget {
    continueLabel;
    breakLabel;
}
```

`COMPILER_MAX_LOOP_TARGET_DEPTH` is 8, matching
`COMPILER_MAX_LOOP_NESTING`. On entry to a `while`, the emitter creates and
defines the condition label, creates the exit label, and pushes those two
labels before emitting the body. It pops the target after the body, including
the failure path. `break` reads the top `breakLabel`; `continue` reads the top
`continueLabel`. An empty-stack lookup, push overflow, pop underflow, invalid
label, or non-zero depth at successful function completion rejects emission.

Each compiler invocation creates fresh parser and emitter state. The parser's
IR is cleared by `parse_function`, and the emitter's labels, fixups, and loop
stack are initialized per `emit_function`, so failed or nested compilations do
not retain stale targets.

## Semantics and lowering

`break` emits an unconditional AMD64 `jmp` to the active loop exit. No later
statement in that loop body executes on that path. `continue` emits an
unconditional `jmp` to the active loop condition label, so the condition is
reevaluated before another iteration is admitted. It never targets the first
body instruction directly.

Nested loops push a new pair while their bodies are emitted. Therefore the
top entry always denotes the innermost active loop. Popping restores the
outer-loop pair after the inner loop is complete. No AST scan, global singleton
label, stack unwinding, cleanup, or scope-destructor behavior is involved.

The existing branch/fixup system resolves the labels after emission. A
representative `break` disassembly contains a forward `e9 <positive rel32>`
to the loop exit; a representative `continue` contains a negative
`e9 <negative rel32>` to the condition/retest code. The Phase 27K QEMU smoke
exports both generated ELFs for host-side `readelf`/`objdump` inspection.

## Developer Studio proof

The Phase 27K application is built by:

```text
Developer Studio edit
→ Run
→ Save All
→ BuildController
→ bare-metal bootstrap compiler
→ NativeElf
→ RunController
```

The primary source increments `i`, continues for `i < 3`, breaks for `i > 8`,
and returns `3 + 4 + 5 + 6 + 7 + 8 + 9`, or 42. Editing only `i > 8` to
`i > 7` changes the returned result to 34 and changes both source and ELF
hashes. Restoring the source returns 42. The failure proof edits in an
out-of-loop `break`, confirms build failure and run blocking, then restores a
valid source and runs successfully.

Runtime fixtures also prove basic control transfer, nested innermost-loop
targeting, skipped tails, bounded host-call counts, missing-semicolon and
outside-loop diagnostics, deterministic output, capacity rejection, and
compiler reset after nested/failed/simple/nested compilations.

## Resource bounds and limitations

The relevant bounds remain source 64 KiB, tokens 2048, locals 32, statements
256, expressions 1024, blocks 32, block depth 16, conditional depth 16,
loop depth 8, loop-target depth 8, labels 128, fixups 128, code 8192 bytes,
data 2048 bytes, and ELF output 12288 bytes.

Phase 27K supports one source file, one `gx_main`, the signed 32-bit integer
subset, `while`, unlabeled `break`, and unlabeled `continue`. It does not add
labeled control flow, `for`, `do/while`, `switch`, `goto`, increment/decrement,
user-defined functions, arrays, general pointers, or full C/C++. The backend
is AMD64-only, uses no general linker, trusts the kernel-owned runtime, has no
debugger attachment, and has no watchdog/preemption for infinite loops.

The known Developer Studio close/freeze issue remains a separate reliability
follow-up and was not investigated as part of Phase 27K.
