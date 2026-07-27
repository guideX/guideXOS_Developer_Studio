# Bounded C/C++ syntax highlighting

This phase adds a lexical editor service. It does not parse C or C++, invoke a
compiler, run a language server, or infer semantic types.

## Pipeline and ownership

The pipeline is:

```text
Document::buffer.data (UTF-8-compatible bytes)
    -> SyntaxTokenizeLine()
    -> Document::syntax line cache and token spans
    -> visible editor rows -> contiguous render runs
```

`developer_studio_syntax.*` has no UI, document, filesystem, compiler, or host
runtime dependency. `Document` owns one independent `SyntaxCache`; closing a
document clears it. The cache is embedded in the bounded document slot, so it
cannot be accidentally shared when a document slot is reused.

The editor stores source as a NUL-terminated byte buffer. Token offsets and
diagnostic columns remain UTF-8 byte offsets. The scanner recognizes ASCII C/C++
lexical forms only; it does not claim Unicode-aware identifier semantics.

## Language detection

Detection is extension-only and case-insensitive:

| Extension | Language |
| --- | --- |
| `.c` | C |
| `.h` | C++ default |
| `.cc`, `.cpp`, `.cxx` | C++ |
| `.hh`, `.hpp`, `.hxx` | C++ |
| everything else | None |

No content-based inference is performed. JSON, CMake, PowerShell, Markdown,
project metadata, and other supported text files remain plain text.

## Token model

The language-neutral token kinds are `PlainText`, `Keyword`, `TypeKeyword`,
`Identifier`, `Number`, `StringLiteral`, `CharacterLiteral`, `Comment`,
`Preprocessor`, `Operator`, `Punctuation`, and `Invalid`. Plain gaps are not
stored as spans; the render-run builder reconstructs those gaps. Stored spans
are sorted, non-overlapping, and bounded to their line.

`true`, `false`, and `nullptr` are grouped with `TypeKeyword` to keep the
bounded palette small. `size_t` is also highlighted as a built-in type spelling,
although it is technically a library typedef rather than a language keyword.
User-defined types are never inferred from spelling or capitalization.

The C keyword set is:

```text
auto break case const continue default do else enum extern for goto if inline
register restrict return sizeof static struct switch typedef union volatile while
_Alignas _Alignof _Atomic _Bool _Complex _Generic _Imaginary _Noreturn
_Static_assert _Thread_local
```

The C++ keyword set is:

```text
alignas alignof and and_eq asm auto bitand bitor break case catch class compl
concept const consteval constexpr constinit const_cast continue co_await
co_return co_yield decltype default delete do dynamic_cast else explicit export
extern for friend goto if inline mutable namespace new noexcept not not_eq operator
or or_eq private protected public register reinterpret_cast requires return signed
sizeof static static_assert static_cast struct switch template this thread_local
throw try typedef typeid typename union unsigned using virtual volatile while xor
xor_eq override final
```

Built-in/type spellings are:

```text
C:   void char short int long float double signed unsigned _Bool size_t
C++: void bool char short int long float double signed unsigned wchar_t char8_t
     char16_t char32_t size_t nullptr true false
```

## Lexical precedence and state

Each line receives an explicit input and output `SyntaxLineState`. The scanner
uses this precedence:

```text
existing block-comment/raw-string state
    -> comments
    -> strings and character literals
    -> first-token preprocessor directive
    -> numbers
    -> identifiers/keywords
    -> longest operators
    -> punctuation
    -> plain/invalid bytes
```

`//` consumes the rest of the line. `/* ... */` carries
`InBlockComment` through following lines. Quotes inside comments and comment
markers inside ordinary or raw strings are therefore inert.

Ordinary strings and character literals support `\\` escapes and terminate
safely at the line boundary when unterminated; escaped physical-newline
continuation is deliberately not carried as a string state in this phase.

C++ raw strings support `R"..."`, `u8R"..."`, `uR"..."`, `UR"..."`, and
`LR"..."`. Raw delimiters are limited to four ASCII bytes. `InRawString`
stores that bounded delimiter and persists across lines until `)delimiter"` is
found. A delimiter beyond the bound is treated conservatively as ordinary
lexical text rather than allocating based on source input.

Preprocessor highlighting starts only when `#` is the first non-whitespace byte
of a normal line. The `#` and directive word are `Preprocessor`; strings and
comments later on the line retain their own kinds. Backslash macro-line
continuation is not evaluated across lines.

Numbers cover decimal, hexadecimal, binary, octal-like, floating-point,
exponent, hexadecimal-floating-point, suffix, and practical digit-separator
forms. Malformed numeric spellings are scanned conservatively and never cause
an out-of-bounds read. Operators use longest-match ordering for `<<=`, `>>=`,
`->*`, `...`, comparisons, logical operators, shifts, increments, assignments,
`->`, `::`, and `.*`, followed by common single-character operators and
punctuation.

## Incremental cache

`TextBuffer` records the generation, earliest affected line, inserted/deleted
byte counts, and newline delta for each successful mutation. `DocumentUpdateSyntax`
consumes that edit record and updates only the affected line and following
lines. If the output state and input state match the old cached result, ordinary
single-line edits stop immediately. A state-changing edit propagates to the
end of the affected lexical region.

Newline insertion and deletion use a bounded line-structure update: unchanged
prefix line results are retained, the split/join region is retokenized, and the
unchanged suffix is copied only after state and token-span convergence. This
keeps line-index changes safe without rescanning the whole source on ordinary
newline edits. Full rebuilds remain the safe path for a cache generation
mismatch, unsupported/changed language, full document replacement, malformed
cache metadata, or a limit transition.

Counters on each cache provide deterministic proof:

```text
linesInvalidated
linesRetokenized
fullRebuildCount
incrementalRebuildCount
lastInvalidatedLineCount
lastRetokenizedLineCount
lastUpdateConverged
lastConvergenceLine
```

## Limits and fallback

The syntax service limits are:

| Limit | Value |
| --- | ---: |
| highlighted document size | 2 MiB |
| highlighted line count | 100,000 |
| tokenizable line length | 32 KiB |
| tokens per line | 4,096 |
| cached spans per document | 65,536 |
| raw delimiter length | 4 ASCII bytes |
| open document slots | 8 (existing editor limit) |

The existing editable text buffer remains capped at 256 KiB, so the 2 MiB
syntax limit is deliberately future-proofed while the current editor rejects
larger files at its existing model boundary. A source file that exceeds a
syntax limit is still editable whenever the existing text-buffer limit permits;
the syntax cache is marked plain-text fallback with a stable code such as
`SYNTAX_DOCUMENT_TOO_LARGE`, `SYNTAX_TOO_MANY_LINES`, `SYNTAX_LINE_TOO_LONG`,
or `SYNTAX_TOKEN_LIMIT`. Invalid cache state is discarded and rebuilt. A
failed rebuild leaves a valid plain-text fallback rather than crashing or
refusing an otherwise editable buffer.

Normal editing does not append per-keystroke records to Output. The UI header
shows the language and bounded fallback code. One-time host markers include
`syntax_language`, `syntax_cache_initialize`, `syntax_full_tokenize`,
`syntax_incremental_tokenize`, `syntax_state_converged`,
`syntax_span_validation`, `syntax_render_visible`, and `syntax_fallback`.

## Rendering, tabs, scrolling, and selection precedence

The editor already paints only its 26 visible rows. Each visible row obtains
cached spans, reconstructs plain gaps, clips to the automatic horizontal byte
view, expands tabs to four visual columns, and draws contiguous runs. It does
not construct a temporary string per source character. The caret x-coordinate
and mouse hit-testing use visual columns derived from stored byte offsets, so
token boundaries do not affect caret behavior.

The ABI exposes rectangle and uncolored text drawing, but no colored-text call.
The default palette is therefore represented by subtle token background colors
behind the existing readable text call. The centralized palette contains
plain, keyword, type keyword, identifier, number, string, character, comment,
preprocessor, operator, punctuation, invalid, and selection colors.

The render-run model defines selection precedence. The editor now stores a
bounded anchor/caret selection for Find initialization and current-match
selection; selection background overrides syntax background and selected text
remains readable. Find overlays are drawn independently after syntax runs and
never alter the cached token spans. See [FIND_AND_REPLACE.md](FIND_AND_REPLACE.md)
for the search and replacement contract.

## Tests and known limitations

`tests/syntax_test.cpp` covers extension detection, all major token classes,
longest operators, comments/strings/raw strings, multiline state, preprocessor
precedence, invalid input safety, render gaps and selection splitting,
incremental convergence, newline insertion/deletion, tab visual columns, and
size/token fallbacks. Existing model, project, navigation, Build, Run, Output,
and workflow tests continue to exercise save, diagnostics, Problems navigation,
and close behavior.

This is not a full C/C++ grammar. It does not evaluate inactive preprocessor
regions, expand macros, identify user-defined types, parse templates, validate
numeric literals perfectly, support compiler-equivalent Unicode identifiers,
perform semantic highlighting, provide completion/IntelliSense, fold code, or
offer a theme editor. Raw-string delimiter handling is intentionally bounded.
