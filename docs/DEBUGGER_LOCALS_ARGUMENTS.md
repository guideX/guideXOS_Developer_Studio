# Debugger Locals and Arguments

Phase 9 adds a bounded, read-only inspection layer for simple variables in the exact stopped Native ELF artifact.

## Pipeline

`ELF sections -> .debug_abbrev/.debug_info reader -> bounded DIE index -> function lookup by PC -> type/variable index -> location evaluation -> target register/memory read -> typed display`

The index is tied to the artifact path, size, SHA-256, project generation, and mapper generation. It is rebuilt with the symbol mapper and is never reused for a different artifact.

## Phase 9 fixture evidence

The checked-in `tests/fixtures/debugger-phase9` artifact is ELF64 AMD64 ET_EXEC,
10,136 bytes, with two compilation units and four subprogram DIEs. Its relevant
DWARF section sizes are `.debug_abbrev` 0x1c9, `.debug_info` 0x877,
`.debug_str_offsets` 0x228, `.debug_str` 0x764, `.debug_addr` 0x78,
`.debug_line` 0x3de, and `.debug_frame` 0xe8. `readelf --debug-dump=loc`
reports no location-list entries for this fixture.

At `calculate`'s marked assignment, the external DWARF audit reports
`DW_AT_frame_base = DW_OP_reg6` and these exact locations:

```text
a        DW_OP_fbreg -4
b        DW_OP_fbreg -8
sum      DW_OP_fbreg -12
doubled  DW_OP_fbreg -16
positive DW_OP_fbreg -17
ptr      DW_OP_fbreg -32
```

The fixture therefore proves stack-backed arguments and locals, a boolean, and
a pointer without relying on a guessed calling convention. The evaluator also
has explicit AMD64 DWARF register mappings for register-backed expressions, but
this unoptimized Clang fixture spills these six live values to frame-relative
locations at the proof PC.

## Supported debug information

The current Clang 22 debug fixtures use DWARF v5 and are accepted for ELF64 AMD64 ET_EXEC artifacts. The Phase 9 subset recognizes:

- DIE tags: `DW_TAG_compile_unit`, `DW_TAG_subprogram`, `DW_TAG_formal_parameter`, `DW_TAG_variable`, `DW_TAG_lexical_block`, `DW_TAG_base_type`, `DW_TAG_pointer_type`, `DW_TAG_const_type`, `DW_TAG_volatile_type`, `DW_TAG_typedef`, `DW_TAG_structure_type`, `DW_TAG_class_type`, `DW_TAG_enumeration_type`, `DW_TAG_array_type`, and `DW_TAG_subrange_type`.
- Attributes: `DW_AT_name`, `DW_AT_type`, `DW_AT_location`, `DW_AT_frame_base`, `DW_AT_low_pc`, `DW_AT_high_pc`, `DW_AT_ranges` (recognized but not generalized), `DW_AT_byte_size`, `DW_AT_encoding`, `DW_AT_decl_file`, `DW_AT_decl_line`, `DW_AT_const_value`, `DW_AT_count`, `DW_AT_artificial`, `DW_AT_str_offsets_base`, and `DW_AT_addr_base`.
- Forms emitted by the current fixture: `DW_FORM_data1`, `DW_FORM_data2`, `DW_FORM_data4`, `DW_FORM_exprloc`, `DW_FORM_flag_present`, `DW_FORM_ref4`, `DW_FORM_sec_offset`, `DW_FORM_sdata`, `DW_FORM_udata`, `DW_FORM_strx1`, and `DW_FORM_addrx`.

Malformed lengths, references, nesting, forms, and section offsets fail closed. Limits cover units, DIEs, abbreviations, attributes, expression bytes/operations/stack, type depth, variable counts, display lengths, and scalar read size.

## Types and values

Signed and unsigned integral types, `bool`, pointers, qualifiers, typedef chains, opaque aggregates, and recognized arrays are displayed. Pointer values are printed as target addresses and are not dereferenced automatically. Aggregates display `<aggregate>` and arrays display `<array>`; member expansion is Phase 10 work.

Values are read from the stopped register context or the owner-bound target-memory callback. Source text, assignments, and fixture constants are never used to produce a value. Unsupported or failed evaluations display `<unavailable>` or a bounded status such as `unsupported location expression`.

## Location subset

The evaluator supports the operations proven in current debug fixtures: `DW_OP_reg0`–`DW_OP_reg31`, `DW_OP_regx`, `DW_OP_fbreg`, `DW_OP_breg0`–`DW_OP_breg31`, `DW_OP_bregx`, `DW_OP_addr`, `DW_OP_addrx` where resolved by the DIE reader, `DW_OP_plus_uconst`, constants, and `DW_OP_stack_value`. Dereference and generalized location-list expressions are rejected rather than guessed.

Current Clang emits `DW_AT_frame_base = DW_OP_reg6` (RBP) and simple `-O0 -fno-omit-frame-pointer` variables as `DW_OP_fbreg` expressions. The implementation checks that function’s actual frame-base expression before using a frame base; it does not globally assume RBP.

The native evaluator test resolves the real fixture bytes to `a=10`, `b=11`,
`sum=21`, `doubled=42`, `positive=true`, and a pointer equal to the target
address associated with `doubled`. The same test also resolves the supported
frame-relative argument in the Phase 8 caller frame and verifies wrong-process
memory reads become `ReadFailure`/`<unavailable>`.

## Frames and stale state

Frame #0 receives the real stopped general-purpose register context and frame base. Non-zero frames use the unwinder’s known frame base and saved-return-address-minus-one PC. Register-only caller variables are unavailable because the current unwinder does not reconstruct caller GPRs; frame-base-relative stack variables remain evaluable where target memory is readable. Selecting a Call Stack frame rebuilds both panels without moving the execution marker.

Locals and Arguments are rebuilt for every stopped generation. Continue, Step Into, Step Over, Step Out, exit, artifact replacement, and a running state invalidate the view. The UI never presents a prior stopped value as live. Structured children share the same selected-frame and stop-generation ownership.

## Not implemented

This is not a full DWARF evaluator. Watch expressions, arbitrary C++ expressions, generalized location lists, optimized-variable tracking, register editing, strings, STL/NatVis, and dynamic types remain intentionally deferred. Phase 10 adds bounded object member expansion and explicit pointer dereference in a separate structured-value model. The current behavior assumes deterministic unoptimized Clang AMD64 debug builds.

Phase 10 is complete; the next intentionally small milestone is **Debugger Phase 11 — Watches and Bounded Expression Evaluation Foundation**.

Next milestone: **Debugger Phase 10 — Structured Variables and Object Expansion**.
