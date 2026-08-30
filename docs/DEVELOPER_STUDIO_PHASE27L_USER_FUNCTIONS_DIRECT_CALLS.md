# Developer Studio Phase 27L: User Functions and Direct Calls

Phase 27L supports direct, nonrecursive integer functions in one source file.
It extends the guideXOS bootstrap language from one source-defined function to a
bounded translation unit containing several functions. Parsing, semantic
analysis, code generation, NativeElf creation, loading, and execution remain on
the guideXOS bare-metal path.

## Implemented grammar

The implemented translation-unit grammar is:

```text
translation_unit      := function_definition+
function_definition   := "int" identifier "(" parameter_list? ")" block
parameter_list        := parameter ("," parameter)*
parameter             := "int" identifier
                       | "gx_app_context" "*" identifier
block                 := "{" statement* "}"
```

All functions return `int` and have bodies. Ordinary user functions may use
zero to four `int` parameters. Exactly one `gx_main(gx_app_context* ctx)` is
required and is the only source function that may use the context parameter.
The compatibility parser spelling `void* ctx` remains accepted for the older
bootstrap fixtures; the Phase 27L contract and new fixtures use
`gx_app_context* ctx`.

The existing bounded statements and expressions remain available: locals,
assignment, arithmetic, comparisons, `&&`, `||`, `if`/`else`, `while`,
`break`, `continue`, `return`, and the distinguished `log(ctx, "...")` host
operation in `gx_main`.

## Function symbols and IR

The parser builds a `TranslationUnitIR` with a fixed array of 16 `FunctionIR`
records. Each function symbol records its name, function index, parameter
count, and source location. Function names share one translation-unit namespace;
duplicate definitions are rejected deterministically.

Each `FunctionIR` owns its parameter symbols, local symbols, statements, blocks,
expressions, string literals, call sites, and argument-expression index list.
The call expression is target-neutral: it stores a resolved callee function
index and a bounded `CallSite` range. It does not store machine addresses or
relocations.

Calls are resolved after all function bodies have been parsed, so both forward
and backward calls work. Unknown callees, `gx_main` calls, unsupported argument
counts, and malformed call argument lists fail before an artifact is written.

## Parameters, evaluation, and frames

Parameters are signed 32-bit values passed by value. The maximum arity is four,
matching the first four integer argument registers in the Microsoft x64 ABI.
Parameters and locals occupy one function-local namespace; declaring a local
with a parameter's name is rejected.

Call arguments are evaluated left-to-right. Every completed argument is spilled
to a compiler-managed temporary slot before the next argument is evaluated.
After all arguments are ready, the slots are loaded into `ECX`, `EDX`, `R8D`,
and `R9D`. Temporary usage is bounded and nested call depth is capped.

The framed backend uses this logical layout:

```text
[rbp-4]                         parameter/local slot 0
[rbp-8]                         parameter/local slot 1
...
[rbp-4*(parameterCount+1)]       first ordinary local
...
[rbp-(variableBytes+8)]         gx_main context pointer, when present
[rbp-(variableBytes+12)]        first temporary argument slot
...
```

The exact frame allocation is a fixed, 16-byte-aligned reservation computed as
`align16(40 + parameterBytes + localBytes + temporaryBytes)`. The extra ABI
space includes the Microsoft x64 32-byte caller shadow space and alignment
slack. Each function owns its frame and epilogue, so a callee return cannot
land in `gx_main` accidentally.

## Calling convention and fixups

The direct source-function ABI is:

| Value | Register |
|---|---|
| integer argument 0 | `RCX` / `ECX` |
| integer argument 1 | `RDX` / `EDX` |
| integer argument 2 | `R8` / `R8D` |
| integer argument 3 | `R9` / `R9D` |
| integer return | `EAX` |

Each call reserves 32 bytes of shadow space and preserves 16-byte stack
alignment. Calls use a genuine AMD64 direct-call instruction:

```text
E8 <signed rel32>
```

The backend lays out functions in deterministic source order, creates labels
for every function, and patches the displacement as:

```text
callee_address - address_after_call
```

The signed 32-bit range is checked. The same bounded label/fixup mechanism
handles conditional branches, loop branches, and calls.

Representative generated shape:

```asm
push rbp
mov  rbp, rsp
sub  rsp, frame_size
mov  [rbp-4], ecx       ; stable parameter copy
...
mov  eax, result
jmp  function_epilogue
function_epilogue:
mov  rsp, rbp
pop  rbp
ret
```

## Entry point and recursion

The parser records the unique `gx_main` index. The backend records its code
offset while emitting the source-order function stream, and the NativeElf
writer sets `e_entry` to the image base plus that offset. `gx_main` therefore
remains the entry even when it is in the middle or at the end of the file.

A bounded 16-by-16 call-graph matrix is built while resolving calls. A DFS
rejects direct and mutual cycles with:

```text
recursive function calls are not supported
```

Recursion is deferred because the NativeElf app stack is trusted, fixed at
64 KiB, and has no stack guard or recovery mechanism in this phase.

## Resource bounds

The final Phase 27L bounds are fixed and deterministic:

| Resource | Limit |
|---|---:|
| source bytes | 65,536 |
| tokens | 2,048 |
| functions per translation unit | 16 |
| integer parameters per function | 4 |
| call expressions per function | 32 |
| call argument-expression nodes per function | 128 |
| call-graph edges | 128 |
| temporary argument slots | 64 |
| nested call depth | 8 |
| locals per function | 32 |
| statements / expressions | 256 / 1,024 |
| generated code bytes | 24,576 |
| NativeElf bytes | 32,768 |

Capacity exhaustion is rejected without dynamic emitter growth. Compiler state,
symbols, call sites, fixups, and frame calculations are reset for each build.

## Proof coverage

Focused host coverage is run with:

```powershell
./scripts/run-compiler-bootstrap-host-test.ps1
./scripts/run-native-elf-runtime-host-test.ps1
./scripts/run-compiler-functions-host-test.ps1
```

The Phase 27L host test executes generated AMD64 code in memory and covers
zero-, one-, two-, and four-argument calls; nested calls; arithmetic and
condition calls; loop/`if`/`break`/`continue` inside a user function; forward
and backward calls; direct `E8` fixups; local and parameter isolation; entry
selection; determinism; function/parameter capacity; missing return; duplicate
function/parameter; argument count; unknown function; `gx_main` call; direct
and mutual recursion rejection; and output capacity.

The primary source is:

```cpp
int sum_to(int n)
{
    int total = 0;
    int i = 1;
    while (i <= n)
    {
        total = total + i;
        i = i + 1;
    }
    return total;
}

int double_value(int value)
{
    return value * 2;
}

int gx_main(gx_app_context* ctx)
{
    int result = double_value(sum_to(6));
    log(ctx, "Functions executed.");
    return result;
}
```

The guideXOS smoke harness runs it and observes `Functions executed.` and exit
code `42`. It also proves the source edit `value * 2` to `value + 1` changes
the result to `22`, changes source and artifact hashes, blocks a bad build from
running a stale artifact, and then recovers to the valid 42 result.

Run the complete bare-metal regression with:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File ..\guideXOSServerV0.5_DEVELOPER_STUDIO\scripts\smoke-compiler-bootstrap.ps1 `
  -BootCount 3 -TimeoutSeconds 300 -Phase27L
```

The required Phase 27L markers cover all arities, nested and expression calls,
conditions, existing control flow, call opcode/direction, isolation,
diagnostics, entry selection, host integration, Developer Studio integration,
source edit, recovery, determinism, and kernel survival. All earlier Phase
27B through 27K markers remain mandatory.

## Disassembly evidence

The guest-generated primary ELF from the smoke run had entry address
`0x100010aa` and code bodies in source order:

```text
sum_to       0x10001000
double_value 0x10001082
gx_main      0x100010aa
```

The external audit showed:

```asm
0x100010ce: e8 2d ff ff ff    call 0x10001000
0x100010e7: e8 96 ff ff ff    call 0x10001082
```

It also showed each helper's `push rbp`/`mov rbp,rsp` prologue, an epilogue
ending in `ret`, and a backward call from `gx_main` to each earlier body.
The audit is validation only; no host compiler, linker, or runtime participates
in the guest Build-and-Run operation.

## Developer Studio proof path

The real Phase 27L project exercises:

```text
edit document
→ Save All through BuildDirtyDecision::SaveAll
→ BuildController
→ bare-metal compiler
→ multi-function NativeElf
→ RunController identity validation
→ NativeElf execution
→ output and exit observation
→ cleanup and VFS survival
```

The proof project and its compiler fixtures live under the Server smoke fixture
because the compiler and bare-metal harness are Server-owned. The Developer
Studio repository supplies the Phase 27L proof app/package and this document.

## Known limitations

Phase 27L deliberately does not provide:

* more than one source file or a general linker;
* recursion or mutual recursion at runtime;
* function pointers, indirect calls, prototypes, or separate declarations;
* non-`int` return types, non-integer parameters, structs, arrays, or general pointers;
* varargs, default parameters, overloading, methods, classes, templates, or full C/C++;
* `for` or `do/while` loops;
* architectures other than AMD64;
* debugger attachment or general symbol/section emission;
* stack guards for the trusted 64 KiB app stack.

The known Developer Studio close/freeze issue remains a separate reliability
item and was intentionally not changed for Phase 27L.

## Outcome and next bounded phase

Outcome A — user-defined functions and direct calls compile and execute through
Developer Studio.

The next architecturally justified bounded phase is **recursion-safe call-stack
hardening and bounded recursion**. It should add stack-depth accounting and
failure recovery before enabling recursive source functions.
