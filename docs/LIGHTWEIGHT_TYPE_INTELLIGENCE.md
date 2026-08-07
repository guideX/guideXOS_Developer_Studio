# Lightweight Type Intelligence

Developer Studio now provides a bounded, generation-aware semantic type model. It is deliberately a lightweight type-information provider, not a complete C++ type system or compiler frontend. The primary UI is Quick Type Info (`Ctrl+Alt+T`) in the focused source editor.

## Supported declaration forms

The provider lexes the active project using the existing bounded document/filesystem and symbol-index inputs. It recognizes common primitive spellings, named `class`, `struct`, `enum`, and `union` declarations, ordinary variables, function parameters and return types, simple members, `typedef`, and `using` aliases. Declarator information retains pointer depth, lvalue/rvalue reference kind, `const`, `volatile`, and simple explicit array extents.

The model is tolerant of incomplete editor text. Comments, strings, characters, and preprocessor lines are skipped by its bounded lexical cursor, so type-looking text in those regions does not become a declaration.

## Lookup and evidence

Identifier lookup is bounded and uses nearest supported scope precedence:

1. local variable;
2. function parameter;
3. directly known class/struct member;
4. project/namespace declaration;
5. global declaration.

The index reuses project document inventory and the existing `SymbolDatabase` generation. It does not perform a whole-project scan for each inspection. Candidate buckets and nearby scope checks are capped; ties are reported instead of being resolved arbitrarily.

`Exact` means direct declaration evidence or an unambiguous bounded alias resolution. `Conservative` means a strong bounded inference, such as a literal or a unique known function return type. `Ambiguous` means multiple plausible declarations remain. `Unknown` means the provider cannot make a truthful determination. The UI omits misleading declaration fields for ambiguous and unknown results.

## Aliases and inference

Alias chains retain the displayed alias and the resolved underlying type where known. Alias traversal is capped at eight links and detects cycles. A cycle or unresolved target is not presented as an exact resolved type.

`auto` is intentionally narrow. Literal initializers such as `42`, `true`, `3.14f`, and character literals are recognized. A call initializer can use a unique known project function return type. Unknown initializers, ambiguous calls, conversions, overload resolution, and general C++ deduction remain unknown or ambiguous.

Function return types are exposed as records for inspection and for conservative call-result inference. This API is available for future completion and signature-help improvements without changing those features in this milestone.

## Boundedness and generations

The provider caps declarators at 512 bytes, type spellings at 192 bytes, canonical/qualified names at 192/256 bytes, initializers at 256 bytes, alias depth at 8, lookup candidates at 64, inspected scope nesting at 64, parser tokens at 32,768, documents at 2,048, records at 4,096, popup rows at 8, and popup text at 1,024 bytes. Truncation is retained in the model and is shown when it affects a result.

Every indexed document stores its document generation and project generation. The database also records the symbol-index generation. A result is current only when these generations still match. Project refresh, document close, and workspace close clear or invalidate the type database before UI storage can be reused. Indexing is synchronous because this first pass is bounded; no background thread or cancellation protocol is introduced.

## Quick Type Info

Press `Ctrl+Alt+T` with the caret on an identifier in the source editor. The shortcut was audited against the existing project/workspace, completion, signature-help, ownership, navigation, save, and run commands; it does not reuse `Ctrl+O`, `Ctrl+K`, `Alt+O`, or `Alt+Shift+O`. The popup is a bounded editor overlay and closes on `Escape`, outside mouse interaction, or the next editing command. Pressing `Ctrl+Alt+T` while it is open refreshes the result.

Exact and conservative results show the type, declaration path/line, declaration kind, evidence source, confidence, and alias or inference detail when available. Ambiguous results say that multiple declarations match. Unknown results say that type information is unavailable. This milestone proves keyboard Quick Type Info; it does not claim automatic mouse-hover timing.

## Deferred semantics

Templates, concepts, SFINAE, overload resolution, conversions, inheritance lookup, virtual dispatch, operator overloads, dependent names, complete `decltype`, complex function pointers, complete array decay/reference collapsing, macro-generated types, compiler ASTs, Clang/GCC/libclang, language servers, and C# semantics are intentionally deferred. Member completion is also deferred; the type API is only a future query surface for completion and signature help.
