# Developer Studio Phase 27O: Globals and External Data

Phase 27O extends the bounded in-guideXOS compiler/linker pipeline with
mutable signed 32-bit file-scope integers and external data symbols. Each
source file remains an independent translation unit; the guest linker joins
function and data symbols into one NativeElf image.

This phase adds only signed 32-bit mutable globals. It does not add arrays,
structs, source pointers, address-of/dereference, `static`, TLS, constructors,
dynamic linking, or general object files.

## Grammar and initialization

The supported file-scope forms are:

```text
translation_unit := external_declaration+
external_declaration := function_definition
                      | function_declaration
                      | global_definition
                      | global_declaration
global_definition := "int" identifier ";"
                   | "int" identifier "=" integer_literal ";"
                   | "int" identifier "=" "-" integer_literal ";"
global_declaration := "extern" "int" identifier ";"
```

`int answer;` allocates four explicit zero bytes in mutable data. Literal
initializers are emitted as little-endian signed 32-bit values, including
two's-complement negative values. Expressions, calls, and global-to-global
initializers are rejected with `global initializer must be a constant integer`.
Repeated `extern` declarations are compatible, and an `extern` followed by a
definition is valid. An unused `extern` does not create a link dependency.

Inside a function, parameters and locals shadow globals. A global reference is
valid only when the global is defined in that translation unit or declared
there with `extern`; the parser never scans another translation unit to make a
name visible.

## IR, modules, and symbols

The target-neutral IR contains `GlobalSymbolIR` definitions/declarations,
`ExpressionKind::LoadGlobal`, and `StatementKind::StoreGlobal`. Global IR
stores the source name/index, definition and initializer state, initial signed
value, source location, and module data offset; it never stores a final virtual
address.

Compiled modules carry bounded function and data exports/imports. Data symbols
are `SymbolKind::Data` with four-byte size and alignment. The project linker
uses one ordinary namespace for functions and globals, so duplicate definitions
and function/data collisions fail with source-located diagnostics. Exactly one
`gx_main` function is still required.

The linked symbol record contains `name`, `kind`, sorted module index, module
code/data offset, final code/data offset, parameter count for functions, size,
alignment, and entry state. The current limits are 32 globals per module, 128
project exports/imports, 16 translation units, 64 relocations per module, and
256 relocations per project.

## Relocations and data layout

The internal relocation kinds are:

* `CallRel32`: a four-byte AMD64 `E8` displacement to a function.
* `DataAddress64`: an eight-byte absolute address for module read-only data,
  such as a logged string.
* `GlobalDataAddress64`: an eight-byte absolute address of one final mutable
  global allocation.

Modules are sorted by normalized source path. Code is laid out in that order
with 16-byte module starts; read-only strings follow in module order; mutable
globals follow in module order and global declaration order, each aligned to
four bytes. The linked mutable-data limit is 8 KiB. Definitions allocate one
four-byte slot; imports allocate no storage and are patched to the definition's
single final slot. Zero-initialized data is represented as explicit zero bytes,
so the ELF bytes and artifact hash are deterministic.

## Backend and NativeElf permissions

Global reads and writes remain runtime memory operations. A read is lowered as:

```asm
mov rdx, <patched-global-address>
mov eax, [rdx]
```

and a store evaluates the right-hand side, loads the patched address, then
stores `eax` through it. Module compilation emits zero address placeholders;
only the linker knows the final mutable-data address.

The ELF writer emits separate page-aligned `PT_LOAD` segments when present:

```text
code    PF_R | PF_X
rodata  PF_R
rwdata  PF_R | PF_W
```

No writable load is executable. The validator rejects W+X loads, overlapping
or out-of-range regions, an entry point outside executable code, and mutable
data that does not match the linked bytes. The NativeElf loader's existing
image reset path restores the initial RW data payload on every independent
run, including after runtime failure; code, rodata, stack, and recursion guard
behavior remain unchanged.

## Developer Studio proof project

The Server fixture `scripts/fixtures/phase27o` contains `state.cpp`,
`math.cpp`, and `main.cpp`:

```cpp
// state.cpp
int answer = 40;

// math.cpp
extern int answer;
int add_two() { answer = answer + 2; return answer; }

// main.cpp
extern int answer;
int add_two();
int gx_main(gx_app_context* ctx) {
    add_two();
    log(ctx, "Linked global state updated.");
    return answer;
}
```

`build-phase27o.ps1` builds the small Developer Studio proof app, while the
bare-metal smoke harness stages the real three-file project. The app opens all
three documents, performs Save All builds, observes the shared value 42,
edits the initializer to prove a changed artifact and result, exercises
undefined/duplicate data diagnostics and compile/link recovery, checks
zero/constant initialization, global loop/condition/recursion behavior, and
repeats runs to prove global reinitialization. The harness exports a
guest-generated ELF and audits it with `readelf`/`objdump`; those tools do not
participate in guest compilation or linking.

Run the focused proof with:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File D:\dev\guideXOSServerV0.5_DEVELOPER_STUDIO\scripts\smoke-compiler-bootstrap.ps1 `
  -BootCount 3 -TimeoutSeconds 600 -Phase27OOnly
```

Use `-Phase27O` for the chained Phase 27B–27O regression. The separate known
Developer Studio close/freeze issue is outside this phase and is not changed.

## Bounds and limitations

The compiler remains AMD64-only, integer-only, direct-call-only, and bounded
to four integer parameters, 16 source files, 16 functions per module, 24 KiB
module code, 64 KiB linked code, 8 KiB read-only data, 8 KiB mutable data,
96 KiB bootstrap ELF output, and the existing 1 MiB NativeElf mapped-image
contract. There are no arrays, structs, pointers, address-taking, arbitrary
constant expressions, libraries/archives, shared or dynamic linking, general
ELF `.o` compatibility, debugger symbols, or general process isolation.
