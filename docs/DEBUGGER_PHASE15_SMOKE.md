# Phase 15 hosted debugger smoke

Phase 21 validation tiers, the authoritative required command, the bounded
hosted soak, and CI limitations are documented in
[VALIDATION_TIERS.md](VALIDATION_TIERS.md).

The normal Phase 15 command is now the deterministic focused suite:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\smoke-developer-studio-phase15.ps1
```

It runs separate real hosted sessions for the Phase 20 condition-editor and
ConditionError/recovery proofs, then preserves the Phase 15 focused runtime,
Call Stack/selected-frame, structured Locals, and frame-sensitive Watch
coverage. The legacy editor and ConditionError cases are no longer the
authoritative path.

## Modern condition-editor harness

The focused runner is:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\smoke-developer-studio-phase20.ps1 -Case ConditionEditor
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\smoke-developer-studio-phase20.ps1 -Case ConditionErrorRecovery
```

It opens the real Breakpoints panel and condition editor, records the stable
breakpoint ID and bounded editor text, and targets every key through
`gui.keyto <windowId>`. Text input is paced at a bounded 120 ms per character
so the existing priority lane remains below its cap while compositor redraws
complete. The harness waits on semantic markers for editor open/close, ordered
input, valid/invalid parse state, clear, cancel, enable retention, debugger
stop, and targeted shutdown; it does not assign the controller model.

The setup records the panel breakpoint count and disables only a stale leading
row left by an interrupted legacy run. The intended fixture breakpoint stays
enabled and is selected with the panel's Up/Down semantics. After debugger
start, panel readiness is explicitly reset and the panel is reopened through
the supported menu path before condition interaction.

The `ConditionErrorRecovery` case enters `unknown_value == 2` through the real
editor, proves the real hosted condition error and stopped state, changes it
to `counter == 2`, proves conditional recovery, clears the condition, and
uses `gui.close <windowId>` while the normal hosted UI is active. The same
targeted close is used while the condition modal is active in the editor case.

On failure the runner retains a bounded 64 KiB trace under `logs/`, with the
case, window ID, expected marker, last input, shutdown stage, server exit
code, the last 96 lifecycle/UI markers, and the last 80 output lines. Passing
runs print only case-level results such as
`condition_editor_valid=PASS` and `condition_error_recovery=PASS`.

The five-cycle launch/debug/interaction/close regression is:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\smoke-developer-studio-phase20-cycles.ps1
```

It runs Continue, Step Out, Step Out followed by Continue, Step Out followed
by Step Into, and Watch inspection. Each child uses the established
Phase 19 `gui.close <windowId>` shutdown and lifecycle assertions, including
target/session teardown, window ownership release, bounded native-app state,
and Server exit code zero. The child trace index identifies the cycle on
failure; successful output is one `debug_cycle_N=PASS` line per cycle.

The former all-in-one sequence remains available only with
`-LegacyAggregate` for historical diagnosis. It is retired from the normal
workflow because its fixed tree coordinates become stale after the preceding
frame-zero Watch refresh; the focused tree session retains the same disclosure
assertions and passes independently. The old Phase 15 editor and
`-ExpectConditionError` invocations are likewise retired from normal
validation and delegated to the Phase 20 focused runner.
