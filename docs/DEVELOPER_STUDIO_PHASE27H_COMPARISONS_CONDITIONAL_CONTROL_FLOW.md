# Developer Studio Phase 27H — Comparisons and Conditional Control Flow

Phase 27H extends the guideXOS bootstrap compiler with bounded, target-neutral
comparisons and conditional control flow. It is intentionally limited to the
single `gx_main` function: loops, user-defined functions, `break`, `continue`,
logical `&&`/`||`, and short-circuit evaluation remain out of scope.

## Language surface

The lexer recognizes the six comparison operators:

```text
==  !=  <  <=  >  >=
```

It also recognizes the `if` and `else` keywords. Longest-match handling is
used for `==`, `!=`, `<=`, and `>=`; a lone `!` is rejected. Conditions accept
any integer expression. Zero is false and every nonzero signed 32-bit value is
true.

The parser accepts either a braced block or a single statement after `if` and
`else`, including nested conditionals and standalone blocks:

```c
if (value >= 0) {
    log(ctx, "nonnegative");
} else {
    log(ctx, "negative");
}
```

Expression precedence is:

```text
equality -> relational -> additive -> multiplicative -> unary -> primary
```

Parenthesized expressions parse the complete expression grammar. Comparison
expressions produce normalized integer values, `1` or `0`, so they can be
assigned, returned, or used by another condition.

## Signed semantics

Integer literals, unary negation, arithmetic, comparisons, and constant-return
evaluation use the bootstrap language's signed 32-bit model. The evaluator
retains 32-bit bit patterns and converts them to `int32_t` for signed ordering;
this makes cases such as `-1 < 1` deterministic on both the host tests and the
AMD64 NativeElf backend.

## Target-neutral IR

`FunctionIR` now contains bounded block records and linked statement records.
An `if` statement stores its condition expression plus then/else block roots;
ordinary statements and blocks use bounded linked indices. The IR contains no
machine labels, instruction offsets, or AMD64-specific branch encodings.

The parser records all returns, accepts unreachable statements for predictable
diagnostics/code generation, and performs a structural return-path analysis:

- a `return` guarantees a return;
- a block guarantees a return when its reachable statement chain does;
- an `if` guarantees a return only when both then and else paths guarantee it;
- a conditional without an `else` does not guarantee a return.

Consequently, a function that can reach its closing brace reports:

```text
gx_main may reach end without returning a value
```

The existing compatibility summary reports a constant return only when the
function has exactly one return and its expression is fully constant.

## AMD64 lowering

The backend evaluates comparison operands as 32-bit values and emits:

```text
cmp ecx,eax       ; left - right after operand evaluation
setcc al          ; condition-specific setcc
movzx eax,al      ; normalized 0/1 result
```

The `setcc` encodings are:

| Operator | Encoding |
| --- | --- |
| `==` | `0F 94 C0` |
| `!=` | `0F 95 C0` |
| `<` | `0F 9C C0` |
| `<=` | `0F 9E C0` |
| `>` | `0F 9F C0` |
| `>=` | `0F 9D C0` |

Truthiness uses `test eax,eax` followed by a near `jz` fixup. Conditional
control flow is emitted with bounded labels and fixups. Then and else paths
converge on a shared epilogue when required; returns in nested branches do not
duplicate teardown code. Unconditional joins use the AMD64 near-jump form
`E9 rel32`, and every relative displacement is range-checked before patching.

## Resource bounds

The bootstrap compiler remains fixed-capacity and allocation-free at runtime.
The Phase 27H limits are:

| Resource | Limit |
| --- | ---: |
| Source bytes | 65,536 |
| Tokens | 2,048 |
| Diagnostics | 16 |
| Identifier bytes | 63 |
| String literals | 16 |
| String data bytes | 2,048 |
| Locals | 32 |
| Statements | 256 |
| Expression nodes | 1,024 |
| Expression nesting | 16 |
| Blocks | 32 |
| Block nesting | 16 |
| Conditional nesting | 16 |
| Branch labels | 128 |
| Branch fixups | 128 |
| Generated code bytes | 8,192 |
| Generated data bytes | 2,048 |
| NativeElf output bytes | 12,288 |

Capacity exhaustion is reported through the existing bounded diagnostic path;
the backend refuses code, label, fixup, or relative-displacement overflow.

## Proof fixtures and integration

The server-side smoke fixture is staged at `/P27H`. `src/main.cpp` is reserved
for the Developer Studio workflow. Direct compiler cases are staged under
`tests/` so the FAT-backed directory remains small and deterministic. The
fixture includes valid project metadata, a NativeElf application manifest,
required project support files, and cases for:

- equality true and false;
- all six comparisons and precedence;
- signed ordering;
- simple `if`, branch suppression, `if`/`else`, else execution, and nesting;
- truthy and falsy integer conditions;
- assignment across a branch;
- missing-return and malformed-condition diagnostics.

The Developer Studio proof app is built by:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  D:\dev\guideXOS_Developer_Studio\scripts\build-phase27h.ps1 `
  -ServerRoot D:\dev\guideXOSServerV0.5_DEVELOPER_STUDIO
```

It opens `/P27H/src/main.cpp`, edits and runs the true branch returning `42`,
edits and runs the else branch returning `-1`, reruns deterministically,
rejects an invalid condition before launch, and recovers with a valid source.

## Required proof markers

The Phase 27H smoke requires these direct and integration markers:

```text
phase27h_equality=PASS
phase27h_comparisons=PASS
phase27h_if=PASS
phase27h_branch_suppression=PASS
phase27h_if_else=PASS
phase27h_artifact=PASS
phase27h_else_branch=PASS
phase27h_nested_if=PASS
phase27h_truthiness=PASS
phase27h_branch_assignment=PASS
phase27h_missing_return=PASS
phase27h_invalid_condition=PASS
phase27h_ide_program=PASS
phase27h_source_edit=PASS
phase27h_failure_recovery=PASS
phase27h_deterministic=PASS
phase27h_kernel_survival=PASS
phase27h=PASS
ELF Loader: Phase 27H bootstrap language smoke PASS
```

The same smoke run retains the Phase 27B–G regression chain. The QEMU harness
also exports the generated `h27ifelse.elf` over serial and audits it with
`readelf` and `objdump`; the guest image is memory-backed for the boot, so this
is independent artifact evidence rather than a claim of persistent guest
storage.

## Validation commands

Host compiler and runtime checks:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  D:\dev\guideXOSServerV0.5_DEVELOPER_STUDIO\scripts\run-compiler-bootstrap-host-test.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File `
  D:\dev\guideXOSServerV0.5_DEVELOPER_STUDIO\scripts\run-native-elf-runtime-host-test.ps1
```

Full QEMU proof:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  D:\dev\guideXOSServerV0.5_DEVELOPER_STUDIO\scripts\smoke-compiler-bootstrap.ps1 `
  -BootCount 3 -TimeoutSeconds 180 -Phase27H
```

Physical hardware testing is not part of this phase's evidence. The validation
target is the checked-in host tests plus the AMD64 QEMU boot proof and its
independent generated-artifact audit.
