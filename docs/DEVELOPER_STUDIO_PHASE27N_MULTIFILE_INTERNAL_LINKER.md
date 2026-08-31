# Developer Studio Phase 27N: Multi-File Compilation and Internal Linking

Phase 27N adds bounded multi-file Build and Run support for bootstrap NativeElf
projects. A project with an empty `sourceEntry` enumerates regular `.c` and
`.cpp` files directly below `sourceRoot`, normalizes the project-relative paths,
sorts them ascending, and rejects an empty or over-capacity set. An explicit
`sourceEntry` remains a deterministic one-file selection. Files are compiled
in that order, while the linker sorts module paths again so its output does not
depend on filesystem enumeration order.

## Translation units

Each source file gets an independent lexer invocation, parser invocation,
bounded token arena, IR arena, call arena, diagnostics, and AMD64 module
emission. Source text is never concatenated. A failed translation unit aborts
the project before linking or replacing the prior artifact; later builds start
with fresh module state.

The declaration grammar is:

```text
function_declaration := "int" identifier "(" parameter_type_list? ")" ";"
parameter_type_list  := "int" { "," "int" }
```

Definitions retain the Phase 27L function grammar. Parameter names are
optional in declarations and do not participate in signature identity. A
compatible repeated declaration is allowed. A conflicting declaration or a
definition with a different arity reports `conflicting declaration for
function 'name'`. Calls require a visible declaration in the calling
translation unit; an undeclared call remains `unknown function 'name'`.

## In-memory module format

Intermediate modules are not written as `.o` files. They are bounded in-memory
records containing:

* module source path, source size, token count, and source hash;
* module-relative machine code and read-only data;
* function exports with name, module code offset, arity, and entry flag;
* imports created only for declared external functions that are actually
  called; and
* relocation records with patch offset, width, target symbol, and source
  location.

The supported internal relocation kinds are:

* `CallRel32`: an AMD64 `E8` call with a zero placeholder displacement;
* `DataAddress64`: an absolute read-only-data address used by generated host
  logging code.

Local calls may use the same relocation representation as external calls. No
general ELF relocatable-object format is produced.

## Linker pipeline

```text
modules
  -> deterministic module layout (16-byte code starts)
  -> bounded global export table
  -> duplicate-definition and signature checks
  -> import resolution
  -> CallRel32/DataAddress64 patching
  -> project call-graph/SCC reconstruction
  -> linked code/data image
  -> NativeElf ET_EXEC writer
  -> BuildResult artifact and Run
```

The global symbol table records name, sorted module index, module-relative
offset, final code offset, parameter count, and entry status. There must be
exactly one exported `gx_main`; it may be in any module. Missing entry,
duplicate definition, undefined used import, signature mismatch, relocation
bounds failure, and rel32 overflow all fail closed before an ELF is written.
Unused declarations do not require definitions.

The linker combines module data in sorted module order and applies data
relocations after final offsets are known. The NativeElf writer places code in
an R/X segment and data in a page-aligned R-only segment; data-free images omit
the data segment. The final entry is the linked `gx_main` address, not an
assumed first-module address.

## Recursion and diagnostics

The linker reconstructs cross-module call edges from module relocations and
module-local call-graph metadata. Mutual recursion therefore remains visible
to the project SCC analysis, while the existing runtime call-depth guard still
protects every generated source-defined call. Excessive cross-module recursion
terminates with the existing `CallDepthExceeded` result and releases the
NativeElf runtime state.

Compiler diagnostics retain the source path, line, column, and diagnostic
code. Developer Studio Build output announces each `Compiling src/...` step,
then `Linking N modules`, and preserves the source identity for translation
unit and linker failures. A failed build never starts Run and never replaces a
valid prior artifact.

## Bounds

The current Phase 27N limits are:

| Resource | Limit |
| --- | ---: |
| source translation units | 16 |
| module functions/exports | 16 per module |
| project exports | 128 |
| project imports | 128 |
| module relocations | 64 |
| project relocations | 256 |
| module code | 24 KiB per module |
| linked code | 64 KiB |
| linked read-only data | 8 KiB |
| bootstrap ELF output | 96 KiB |
| NativeElf mapped image | 1 MiB loader contract |

Existing language bounds remain in force: integer return values and up to four
integer parameters, bounded expressions/statements/locals, AMD64 only, direct
calls only, no function pointers, arrays, global variables, external data
symbols, classes, templates, or C++ name mangling.

## Proof coverage

The Phase 27N fixture contains `main.cpp`, `math.cpp`, and `helpers.cpp`. The
Developer Studio proof performs source enumeration, opens all three files,
builds and runs a three-file call chain with loop control flow, edits a helper
and observes a changed artifact/result, checks compiler and linker diagnostics,
tests missing and duplicate entry points, exercises cross-file mutual
recursion and safe depth failure, recovers, and repeats the build for
deterministic output. Host tests also verify placeholder relocations before
linking, patched rel32 targets afterward, deterministic layout under reordered
module input, module isolation, data relocations, signature/undefined-symbol
failures, and cross-module recursive SCC accounting.

The external smoke harness supports `-Phase27N` and focused `-Phase27NOnly`
modes. It stages a fresh project image per QEMU boot and validates the prior
Phase 27B-27M markers as applicable. The separate Developer Studio close/freeze
issue remains outside this phase.

Phase 27N implements an internal linker for the bootstrap NativeElf compiler.
It is not a general-purpose ELF system linker.
