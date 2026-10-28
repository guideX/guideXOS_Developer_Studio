# TOOLS9: C/C++ Document Outline

## Result

TOOLS9 adds a read-only **Tools → Document Outline** panel for the active C or C++ editor buffer. It rescans on activation and when the active document, project generation, path, or editor-buffer generation changes. The panel lists each retained symbol's name, category, 1-based source line, and bounded signature excerpt, along with the total recognized count and truncation state.

The implementation uses the editor's current buffer, including unsaved changes. Choosing a row positions the editor caret at the symbol identifier and scrolls it into view. Before moving, the app checks document ID, project generation, edit generation, path, index, offset, and the current identifier bytes. A stale or invalid target is rejected and the active buffer is rescanned. Scanning and navigation do not write source files or change buffer contents.

## Recognized syntax

- C and C++ source/header extensions: `.c`, `.h`, `.cc`, `.cpp`, `.cxx`, `.hh`, `.hpp`, and `.hxx` (case-insensitive extension check).
- Functions with a recognizable return prefix, including built-in types, `auto`, previously discovered class/struct/enum names, and simple pointer/reference or qualified forms. Declarations ending in `;` and definitions with a body are listed.
- Methods declared inside a class or struct, compactly qualified out-of-class methods when the class name is recognized earlier in the document, and constructors whose names match the enclosing class.
- Named class, struct, enum, and namespace declarations with bodies or forward declarations where applicable. `enum class` and compact C++17 nested namespace names such as `namespace outer::inner` are recognized.
- Multiline parameter lists and common trailing qualifiers are scanned with bounded lookahead.

The shared lexical iterator skips line and block comments, escaped line continuations, ordinary and prefixed character/string literals, raw string literals, and preprocessor directive lines. Braces inside those regions therefore do not affect declaration or scope recognition. Function bodies and other non-namespace/non-class scopes are excluded from declaration discovery.

## Limitations

This is a conservative lexical scanner, not a complete C/C++ parser or language server. It does not expand macros or report macro-generated declarations. Template declarations and their contents, overloaded operators, aliases whose underlying return type is unknown, complex declarators, and syntax that exceeds a scan bound are omitted. Qualified nested namespace names require the compact spelling `a::b`; whitespace around the scope operators is not interpreted. Function return types are intentionally inferred from a small set of built-in words, visible class/struct/enum declarations, and simple pointer/reference/qualification patterns; unfamiliar aliases may be missed. Conditional-compilation branches are scanned lexically without evaluating which branch is active.

## Fixed resource limits

| Resource | Hosted build | Bare-metal/native ELF build |
| --- | ---: | ---: |
| Input buffer | 256 KiB | 16 KiB |
| Retained symbols | 128 | 48 |
| Identifier | 64 bytes | 64 bytes |
| Signature excerpt | 128 bytes | 128 bytes |
| Source bytes copied while forming one excerpt | 512 bytes | 512 bytes |
| Lookahead for one declaration | 96 tokens and 1,024 source bytes | 96 tokens and 1,024 source bytes |
| Scope/parenthesis/bracket nesting | 32 | 32 |
| Retained document path | `kMaxPathBytes` (768 bytes) | `kMaxPathBytes` (256 bytes) |
| Fixed `DocumentOutline` result storage | 26,920 bytes (GCC 15.2 hosted build) | 10,088 bytes (GCC 15.2 bare-metal build) |
| Scanner call-chain stack | about 1.2 KiB in the measured GCC 15.2 x86-64 build; compiler dependent | 1,104 bytes AMD64; 1,280 bytes ARM64 (Clang target `-fstack-usage`, unoptimized build, deepest fixed call chain) |

Exceeding a retention or declaration bound marks results truncated. An input larger than the configured editor limit returns `DocumentTooLarge` without scanning. The scanner uses fixed arrays and bounded lookahead; it adds no threads, STL containers, exceptions, RTTI, or runtime dependencies to the freestanding ELF.

The result-storage measurements are `sizeof(DocumentOutline)` and include its retained symbols and document identity. Stack figures sum compiler-reported static frames along the deepest nonrecursive scanner/helper call chain; they exclude the caller's baseline frame and may differ with compiler or ABI. The app keeps one result object in static storage and scans the existing editor buffer in place, with no input copy or heap allocation.

## Validation

The model tests cover simple and multiple functions, prototypes, multiline declarations, classes, structs, enums, namespaces, methods, constructors, nested bodies, comments, ordinary and raw literals, preprocessor lines, templates, operators, empty and unsupported files, capacity/size limits, identity changes, and stale navigation. Task List now uses the same lexer for comments/literals; its test assertions use `TEST_CHECK` so setup expressions remain active in Release.

The hosted integration script launches the existing experimental Server from an isolated temporary runtime, copies only the staged app into that runtime, and uses actual Server UI input. It verifies Tools-menu activation, the no-document state, Escape closure, opening files through Explorer, an unsaved-only declaration, caret jumps, rescan on reactivation, switching to another document, and unchanged fixture file hashes. It discards the deliberately unsaved test edit through the app close prompt.

Hosted UI acceptance: **PASS** against the actual experimental Server and staged AMD64 ELF. The run exercised Tools-menu activation, the no-document state, Escape closure, Explorer file opening and switching, unsaved-buffer discovery and navigation, rescan on activation, the second document's symbols, unchanged fixture-file hashes, and clean discard/Server shutdown.

Hosted UI evidence: `D:\dev\TOOLS9\evidence-amd64-final\document-outline-hosted-ui.log`.

### Build and test record

| Configuration | Build | CTest |
| --- | --- | --- |
| Debug | Clean CMake/Ninja build in `D:\dev\TOOLS9\cmake-debug` | 39/39 passing (serial run) |
| Release | Clean CMake/Ninja build in `D:\dev\TOOLS9\cmake-release` | 39/39 passing |

### Staged package audit

Architecture builds are staged under `D:\dev\TOOLS9\stage-amd64` and `D:\dev\TOOLS9\stage-arm64`, separate from the Server installation. The complete isolated package at `D:\dev\TOOLS9\stage-multiarch\Apps\DeveloperStudio` contains both staged ELFs and the app manifest (`com.guidexos.developerstudio`, `NativeElf`, entry point `gx_main`, ABI `guidexos-c-abi-v1`). Both manifest entries were checked to resolve to an ELF with the matching machine type.

| Target | ELF header machine | Size | SHA256 |
| --- | --- | ---: | --- |
| AMD64 | `Advanced Micro Devices X86-64` (ELF64 executable) | 1,236,884 bytes | `62FFE5D74F31435B3B0E7673DDE516F5DE01F4FF1AD8A8CD28CEBEB1F6718E94` |
| ARM64 | `AArch64` (ELF64 executable) | 1,420,444 bytes | `456DC862A44C5DBC2210321CDC17A9D644C48FD170864F39844BD74AC64E7621` |

## Qualification boundary and follow-up

Guest/QEMU qualification remains pending and is not part of TOOLS9 hosted acceptance. .NET Support retains priority for guest qualification. No Server filesystem or ABI changes are included, and guarded Replace Apply remains disabled. A later tools increment can decide whether richer semantic parsing (templates, aliases, and macro-aware indexing) is warranted; no TOOLS10 work is started here.
