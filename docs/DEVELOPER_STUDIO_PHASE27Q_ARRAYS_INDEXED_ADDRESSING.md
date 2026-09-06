# Developer Studio Phase 27Q — Arrays and Bounded Indexed Addressing

Phase 27Q adds fixed-size signed 32-bit integer arrays to the bootstrap
compiler. It establishes bounded indexed memory access without introducing
general pointers.

## Implemented language surface

The parser accepts the following forms:

```c
int local;
int local = expression;
int local_array[positive_integer_literal];

int global;
int global = signed_integer_literal;
int global_array[positive_integer_literal];
int initialized_array[positive_integer_literal] = { signed_integer_literal, ... };
extern int global;
extern int global_array[positive_integer_literal];

array[index_expression]
array[index_expression] = expression;
```

Array lengths are literal positive integers. Local arrays are limited to 64
elements and 256 aggregate bytes per function. Global arrays are limited to
256 elements; each element is four bytes and global data remains bounded by
the existing 8192-byte RW-data limit. Initializer lists contain integer
literals only, may be shorter than the declared array, and zero-fill the
remainder.

Array parameters, variable-length arrays, multidimensional arrays, bare array
expressions, whole-array assignment, and array returns are rejected. Arrays do
not decay to pointers.

## IR and storage

`LocalSymbol` and `GlobalSymbolIR` carry `ScalarInt`/`ArrayInt`, element count,
element size, and byte size. Indexed loads use `ExpressionKind::LoadIndexed`;
indexed stores use `StatementKind::StoreIndexed`. The IR contains the base
kind, symbol index, index expression, element count, and element size. It does
not contain final machine addresses.

Local stack slots descend from `rbp`. To make logical array element zero the
lowest address, the array symbol's base slot is the highest logical element.
For `int values[4]` with no integer parameters, the array uses 16 bytes and
its base is `-16(%rbp)`; element `i` is addressed as `-16(%rbp,%rax,4)`.
The context slot in a representative 20-byte local-storage frame is
`-28(%rbp)`.

## Bounds and effective addresses

Constant literal indices are rejected during parsing when negative or greater
than or equal to the declared element count. Dynamic indices are checked as
signed values before address generation:

```text
evaluate index
cmp index, 0       ; jl -> bounds failure
cmp index, count   ; jge -> bounds failure
movsxd rax, eax
```

AMD64 lowering then uses genuine scaled addressing. Local arrays use:

```asm
lea rdx, [rbp + rax*4 + base_displacement]
mov eax, [rdx]             ; indexed load
mov [rdx], eax             ; indexed store
```

Global arrays first receive a `GlobalDataAddress64` relocation for the linked
array base, then use `lea rdx, [rdx + rax*4]`. The generated code is not an
unrolled equality cascade.

Bounds failure is sticky through generated calls and is reported through the
NativeElf runtime channel as `NativeRuntimeStatus::ArrayBoundsExceeded`.
RunController maps it to `array_bounds_exceeded`; the loader diagnostic is
`ELF Loader: Application terminated: array index out of bounds.` The trampoline
stores the low status byte and the observed activation depth separately.

## Recursion and resource policy

Phase 27Q increases the maximum generated frame from 448 to 576 bytes and the
maximum activation accounting from 632 to 760 bytes. With the existing 8192-
byte reserve and 64 KiB application stack, the conservative runtime call-depth
limit changes from 90 to 75. This is a deliberate safety migration: legal
local arrays cannot silently retain the old unsafe depth policy.

Other relevant bounds remain fixed: 16 functions per translation unit, 32
globals per translation unit, 256 linked global elements per array, 24576
generated code bytes per module, 65536 linked code bytes, 8192 RW-data bytes,
128 KiB serialized `.gxo` objects, and the existing NativeElf image limits.

## Cross-file globals and objects

Array definitions export `DataArray` with element count, element size, byte
size, and alignment. Sized `extern` declarations import the same signature.
The linker rejects scalar/array conflicts, differing element counts, differing
sizes, duplicate definitions, and undefined data symbols. Imports reference
one final linked RW-data allocation; they never create copies.

The object ABI is version 2. Array metadata, initializer/storage information,
and per-function local-storage bytes are serialized and validated. Old Phase
27P objects are therefore rejected and rebuilt. The final ELF remains split
into R-X code, R-- read-only data, and RW- mutable data; no RWX segment is
accepted. Every NativeElf launch reinitializes mutable data from the linked
image, including after a bounds failure.

## Diagnostics and proof coverage

The implementation reports bounded array length errors, constant out-of-range
indices, bare-array use, array parameters, and whole-array assignment. Host
coverage exercises local dynamic stores/loads, signed lower and upper bounds,
frame layout, global zero/initializer data, global relocations, cross-file
signatures, scalar/array conflicts, object serialization, and ABI migration.

The Phase 27Q Developer Studio project is a three-file project (`state.cpp`,
`math.cpp`, `main.cpp`). Its cold build compiles all three units and returns
42; a warm build links cached `.gxo` objects; editing only the array-producing
translation unit returns 41; a dynamic out-of-range store maps to the bounds
failure; restoring the source returns 42 without rebooting. The kernel smoke
also emits a NativeElf artifact for external segment/disassembly inspection.

Known limitation: this phase supports only fixed-size signed 32-bit integer
arrays with bounded source/object/resource counts on the AMD64 bootstrap
backend. It does not add pointers, address-of, dereference, pointer arithmetic,
array parameters/returns, structs, heap allocation, or general memory safety.
The separate Developer Studio close/freeze issue remains untouched.
