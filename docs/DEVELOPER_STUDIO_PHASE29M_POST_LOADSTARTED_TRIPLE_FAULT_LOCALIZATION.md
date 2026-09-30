# Phase 29M — Post-`LoadStarted` Triple-Fault Localization and Recovery

## Result

The first recovered architectural fault is a supervisor page fault in the NativeElf Developer Studio image:

```text
#PF vector 14, error=0, RIP=0x500d1e26, CR2=0x7000a5f7
```

The faulting operation is `movsbl (%rax,%rcx), %eax` in `lengthOf()` (`src/main.cpp:1395`). An enabled timer IRQ interrupted AMD64 application code compiled with the SysV red zone enabled. The interrupt frame and saved registers used the same CPL0 stack and overwrote the function's red-zone locals: the saved text pointer became the interrupted RBP (`0x6fffa370`), and the saved length became the interrupted RFLAGS (`0x10287`). The resulting read crossed the NativeElf stack's upper boundary. This is stack-local corruption by IRQ entry, not stack exhaustion, a bad project pointer, or an ownership mismatch.

The old exception path had no usable exception gates for vectors 0–31. The original #PF therefore raised #GP during delivery, escalated to #DF, then raised another #GP while trying to deliver #DF; QEMU reported a triple fault. The repair disables the AMD64 red zone for NativeElf builds, installs and validates exception gates, and gives #DF a dedicated TSS/IST stack. Exception diagnostics write a bounded record to debugcon and halt; they do not call serial or UI/filesystem code.

## Baseline and evidence

Phase 29L's ownership model remains unchanged. The investigation began from standalone `9a2f19753606078715642367dfb1693874a87b14` and Server `c9e60bf8f346074f53ffd671e9b9b41114bf875f`. The protected sentinel at `ESP/Apps/DeveloperStudio/.phase28q-diagnostic` retained SHA-256 `967D29A9A500A3108D7331BA47A5E2053A7C2203B81A345BE990084F3F43698A`.

The recorded Phase 29L baseline was Debug CTest 33/33, Release CTest 33/33, DWARF capacity PASS with 397 DIEs, and package audit PASS. Its AMD64 and ARM64 package hashes were `BE256467CD05E197579C30BF9DD91E912A16918D0A582B4B353C99D069105117` and `882D5A438DAC513C9A0850BBFA2A639ECCA5BCEF6703D7E9880AF1CD5676C950`, respectively. Phase 29L reported its standalone branch two commits ahead and Server one commit ahead, with both branches zero behind.

The original Phase 29L boot-2 capture had reset-only QEMU logging. It showed the guest reaching `LoadStarted`, then stopping before metadata `STAT_BEGIN`, but it did not retain an architectural exception record. A diagnostic rerun with QEMU `-d int,cpu_reset`, using the exact old AMD64 application image (SHA-256 `BE256467CD05E197579C30BF9DD91E912A16918D0A582B4B353C99D069105117`), recovered the same #PF at the same RIP. That capture occurred in an analogous later observer/transaction-settlement callback, after the project had reached `ready`; it proves the same fault and escalation mechanism, but it cannot prove the exact internal callback stage of the original boot 2.

Preserved evidence directories under `%TEMP%`:

- Original Phase 29L boot-2 capture: `guidexos-phase28g-d1f3b20e69fe4ce886378e5b02f4b375`.
- First architectural exception and escalation: `guidexos-phase28g-8db15693844f46a5a6b6ccc51348bf71`.
- First run with the new exception handler but the old packaged application: `guidexos-phase28g-d91a51d171bf4ecda53974f9e7ad16af`.
- Final focused 5/5 run: `guidexos-phase28g-b2080b92ab05437c8591f2834c7b27ed`.
- Extended 5/5 debugger run: `guidexos-phase28g-e01525354ed54de68f4a6772508b62fa`.
- Full 10/10 acceptance run: `guidexos-phase28g-5b79d78f784d406d9199c9a43d206a68`.
- Lifecycle stress run: `guidexos-phase28g-dcac5266c97a44c0a02b1ed01cad5f2f`.

The QEMU trace option is diagnostic-only: `-QemuExceptionTrace` is accepted only with the Phase 29L full-acceptance or ownership-only run and adds `int` to the QEMU `-d` log selector. Normal acceptance logging remains `cpu_reset`; the existing `-no-reboot -no-shutdown` arguments were already part of the runner.

## First fault and symbolization

The faulting QEMU state from `boot1.qemu-debug.log`:

| Register/state | Value |
|---|---:|
| Vector / error | #PF (14) / `0x0000` (supervisor read, not-present page) |
| RIP / CS / CPL | `0x500d1e26` / `0x0008` / 0 |
| RSP / RBP | `0x6fffa370` / `0x6fffa370` |
| RFLAGS | `0x00010287` |
| CR2 | `0x7000a5f7` |
| RAX / RCX | `0x6fffa370` / `0x00010287` |
| RSI (length limit) | `0x0140` |
| RDI | `0x50e27d50` |

The exact packaged ELF was stripped, so it has no DWARF/symbol table. Its disassembly was inspected directly and matched the `lengthOf()` implementation at `src/main.cpp:1395`:

```asm
500d1e17: cmp    -0x14(%rbp),%ecx
500d1e1d: jae    0x500d1e33
500d1e1f: mov    -0x10(%rbp),%rax
500d1e23: mov    -0x18(%rbp),%ecx
500d1e26: movsbl (%rax,%rcx),%eax  ; #PF
```

At the fault, the loaded pointer in RAX equals both RBP and RSP. RCX equals RFLAGS exactly. In this frameless-local layout, the pointer and length locals at `-0x10(%rbp)` and `-0x18(%rbp)` sit in the AMD64 128-byte red zone. The timer interrupt saves the interrupted register/CPU frame below RSP on the same stack; its saved RBP and RFLAGS overwrite those red-zone slots. `RAX + RCX = 0x7000a5f7`, exactly CR2.

The preceding bounds branch compared the local length with the caller's `0x140` limit. The corrupted length `0x10287` made the `jae` branch fall through to the byte read. This explains the invalid index without requiring an invalid caller buffer or expired project state.

## Stack, pointer, path, and observer audit

The NativeElf stack is 512 KiB, from `0x6ff80000` to `0x70000000`. At the exception RSP was `0x2a370` (172,912 bytes) above the lower bound, so the stack had substantial remaining downward-growth margin. CR2 was above the mapped stack top. The evidence is consistent with the corrupted pointer/length pair and inconsistent with stack exhaustion.

The Phase 29M stage line uses one bounded 320-byte static trace buffer; it adds no large stack staging array. `ProjectLoadScratch` is owned by the workspace controller, not allocated in the call frame. It holds normalized/root/metadata paths and read buffers. The production metadata path is constructed into its `metadataPath[kMaxPathBytes]` storage; the bare-metal capacity is 256 bytes. `/P28Q/guidexos.project` is 22 bytes plus the NUL. `joinProjectPath` validates the bounded destination, normalization occurs before joining, and the observer receives the pointer synchronously while the controller-owned scratch remains alive. The production regression test captures that callback path synchronously and verifies the exact normalized value and stage order.

The faulting read was in the bounded trace/string-length helper while running at CPL0 in NativeElf. The preserved trace does not include a scheduler task/thread identifier; the request and transaction IDs were both 1. The Phase 29L tuple immediately before the analogous captured observer fault reported `CURRENT`: request ID/generation `1/1`, transaction ID/generation `1/1`, active transaction and controller load-in-progress true, candidate generation valid, active/candidate project generations matched. In that later settlement callback the project was committed and `ready`; release count was still zero. No ownership mismatch preceded the CPU exception.

The observer was active in this analogous capture. The callback entered and returned with `CURRENT`; the fault was in tracing's `lengthOf()` work, not in dereferencing the controller, candidate, request path, metadata path, or VFS argument. In the original Phase 29L boot-2 capture, the precise failing observer substage is not recoverable because QEMU did not record the exception or instruction stream.

## Exception-delivery failure and repair

Before Phase 29M, QEMU showed a GDT limit of `0x17` (three entries), `TR=0`, and an IDT with no exception gates for vectors 0–31. There was no valid #PF handler, TSS-backed double-fault stack, or #DF gate. The trace's escalation was:

```text
#PF (14), error 0, RIP 0x500d1e26
  -> #GP while delivering #PF (missing vector-14 IDT gate; selector error 0x72 inferred)
  -> #DF (8)
  -> #GP while delivering #DF (missing vector-8 gate; selector error 0x42 inferred)
  -> QEMU Triple fault
```

The QEMU exception log explicitly records `check_exception old 0xe new 0xd`, then `old 0x8 new 0xd`, then `Triple fault`. The `0x72` and `0x42` values are the selector-error values calculated from the absent IDT entries; QEMU's old log does not print them as error-code fields. No exception-handler prologue was reached in that old configuration.

The Server kernel now installs stubs and interrupt gates for exceptions 0–31, including the architecture-defined error-code vectors. Every checked gate uses the kernel code selector `0x08`, interrupt-gate attributes `0x8E`, and a nonzero handler offset; the #DF vector's IST field is 1. A five-entry GDT contains an available 64-bit TSS descriptor at selector `0x18`; `ltr` loads it. The TSS supplies a ring-0 stack and a dedicated 16 KiB kernel double-fault stack in IST1. The assembly entry preserves the interrupted state, aligns the stack for the C ABI, and dispatches to a bounded fatal handler. Runtime setup validation emits `P29M EXCEPTION_SETUP valid=1 idt_limit=0x0FFF pf=1 gp=1 df=1 df_ist=1 tr=0x0018 df_stack=16384` before enabling interrupts.

The fatal handler writes a fixed-width vector/error/RIP/RFLAGS/native-ELF-state record and CR2 for #PF through the existing debugcon port-`0xe9` path, then halts. It does not wait on COM1 transmit readiness and does not call UI or filesystem code. A pre-rebuild diagnostic run confirmed that the new handler catches #PF without escalating to #DF/#TF; that run intentionally used the old app image, so the fault remained until the AMD64 package was rebuilt with the actual red-zone fix.

## Production path and bounded trace

The Phase 29M trace is a separate `DEVELOPER_STUDIO_PHASE29M` record format, leaving Phase 29L ownership records and their parser unchanged. It records the actual synchronous production path:

```text
observer_enter owner=CURRENT request=1 tx=1 path=/P28Q
observer_return owner=CURRENT request=1 tx=1 path=/P28Q
load_started_return owner=CURRENT request=1 tx=1 path=/P28Q
owner_check_current owner=CURRENT request=1 tx=1 path=/P28Q
metadata_path_begin owner=CURRENT request=1 tx=1 path=/P28Q
metadata_path_ready owner=CURRENT request=1 tx=1 path=/P28Q/guidexos.project
metadata_stat_call owner=CURRENT request=1 tx=1 path=/P28Q/guidexos.project
P28Z FS STAT_BEGIN path=/P28Q/guidexos.project
```

Metadata and manifest validation, candidate creation, refresh, commit, release, and project `ready` follow. The call order is `WorkspaceControllerOpenProjectFrom` → `notifyProjectOpen(LoadStarted)` → synchronous observer → `LoadProject` → metadata path join → VFS `stat`. The `LoadStarted` observer returns before the read-only current-owner check and metadata path construction. Last entered/returned functions are bounded by the diagnostic: in the captured analogous fault, the last entered function was `lengthOf()` from Phase 29L transaction/observer trace formatting and no failing-function return was observed; in the original boot-2 serial evidence, the last logged production marker was `LoadStarted` and no metadata stat began. In successful Phase 29M traces, the observer and `lengthOf()` return normally before the metadata VFS call.

The regression test exercises the real `WorkspaceControllerOpenProjectFrom` and `LoadProject` path, records callback paths, and asserts observer return, owner-current, metadata-path begin/ready/stat ordering and `/P28Q/guidexos.project`. It does not substitute a synthetic project-state model.

## Changes

Standalone Developer Studio:

- Add AMD64-only `-mno-red-zone` to the native build script and NativeElf CMake target. ARM64 flags are unchanged.
- Add the minimal Phase 29M path/observer checkpoints and bounded stage trace, passing the actual metadata path through the synchronous callback.
- Add the production-path regression test for checkpoint ordering and path lifetime.
- Leave Phase 29L ownership semantics unchanged.

Server integration:

- Install and validate the complete x86-64 exception gate set, TSS, and #DF IST1 stack.
- Use bounded debugcon output for fatal exceptions.
- Add the opt-in QEMU exception trace flag and tests for normal versus diagnostic QEMU arguments.
- Rebuild and package the AMD64 and ARM64 Developer Studio artifacts.

## Validation

| Check | Result |
|---|---|
| Debug CTest | 33/33 PASS |
| Release CTest | 33/33 PASS |
| Bare DWARF capacity | PASS, 397 DIEs |
| AMD64/ARM64 package audit | PASS |
| Phase 29J QEMU argument regression | PASS |
| Focused exception-traced project path | 5/5 fresh boots PASS |
| Extended 29H/29F/29B/29G/29A debugger path | 5/5 fresh boots PASS |
| Full Phase 28Q acceptance | 10/10 fresh boots PASS |
| Lifecycle stress | 25/25 fresh boots PASS |

Every focused, extended, full-acceptance, and stress boot reached exception setup, one sentinel-recognized `/P28Q` request, Phase 29C `ready`, and the required later markers for its gate. All 25 stress host traces retained the sentinel SHA-256 and reported `host_mutations_after_audit=0`. The focused and extended diagnostic runs recorded no #PF/#DF/#TF or exception-delivery failure. Full acceptance and stress reached project loading, symbol handling, debug start, RUNNING, return, Pause/STOPPED, mapping, stack/locals/watches, Continue/Step, exit, and teardown. Each fresh QEMU trace contained two initial CPU-reset records from VM/firmware startup and no post-handoff reset/re-entry. `-no-reboot` preserved a single failure episode in diagnostic runs; the original triple-fault trace contains one escalation chain, not multiple independent guest failures.

Packaged image SHA-256:

| Architecture | SHA-256 |
|---|---|
| AMD64 | `36DD75F65461A20E62887797A87D52A874977BE06551AC73EF8C03D20939F53A` |
| ARM64 | `9BF7637D8BC54363187E0F8E3FA27412B431015C89E546A28216A9426062C64C` |

Physical hardware was not used; all boot gates ran under QEMU.

## Remaining boundary

The hosted breakpoint/source-association issue remains an independent open item. Phase 29M did not modify later debugger admission, RUNNING/Pause, return, stop mapping, or hosted breakpoint behavior. The Phase 29Q guest logs contain neither a hosted `No symbols` result nor a hosted pending/installed/rejected source-breakpoint disposition. They do show a Phase 27Z guest machine breakpoint installation, which is a different path and does not resolve hosted source association. The first-fault repair and requested QEMU lifecycle gates are complete; the lifecycle stress gate result and final Git/push state are recorded in the commit and task report.
