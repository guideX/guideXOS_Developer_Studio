# Phase 15 hosted debugger smoke

The normal Phase 15 command is now the deterministic focused suite:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\smoke-developer-studio-phase15.ps1
```

It runs separate real hosted sessions for condition-editor retention,
conditional false/false/true runtime behavior, `ConditionError`, Call Stack
and selected-frame inspection, structured Locals disclosure, and the
frame-sensitive Watch path. Each runtime wait is bounded and synchronized by
an observable debugger marker or stop condition.

The former all-in-one sequence remains available only with
`-LegacyAggregate` for historical diagnosis. It is retired from the normal
workflow because its fixed tree coordinates become stale after the preceding
frame-zero Watch refresh; the focused tree session retains the same disclosure
assertions and passes independently.
