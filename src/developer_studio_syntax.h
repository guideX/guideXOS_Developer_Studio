#pragma once

#include <stdint.h>

namespace guidexos {
namespace developer_studio {

// Syntax offsets are UTF-8 byte offsets.  The editor intentionally does not
// claim Unicode-aware identifier semantics in this phase; non-ASCII bytes are
// retained as plain/invalid text and never make the scanner walk past bounds.
enum class SyntaxLanguage {
    None = 0,
    C,
    Cpp
};

enum class SyntaxTokenKind {
    PlainText = 0,
    Keyword,
    TypeKeyword,
    Identifier,
    Number,
    StringLiteral,
    CharacterLiteral,
    Comment,
    Preprocessor,
    Operator,
    Punctuation,
    Invalid,
    Count
};

enum class SyntaxLineStateKind {
    Normal = 0,
    InBlockComment,
    InStringContinuation,
    InCharacterContinuation,
    InRawString
};

enum class SyntaxErrorCode {
    None = 0,
    UnsupportedLanguage,
    DocumentTooLarge,
    TooManyLines,
    LineTooLong,
    TokenLimit,
    CacheInvalid,
    StateInvalid,
    IncrementalFallback,
    RenderSpanInvalid
};

#if defined(GXOS_DEVELOPER_STUDIO_BARE_METAL)
static const uint32_t kSyntaxMaxHighlightedDocumentBytes = 16u * 1024u;
static const uint32_t kSyntaxMaxHighlightedLineCount = 2048u;
static const uint32_t kSyntaxMaxTokenizableLineBytes = 2048u;
static const uint32_t kSyntaxMaxTokensPerLine = 256u;
#else
static const uint32_t kSyntaxMaxHighlightedDocumentBytes = 2u * 1024u * 1024u;
static const uint32_t kSyntaxMaxHighlightedLineCount = 100000u;
static const uint32_t kSyntaxMaxTokenizableLineBytes = 32u * 1024u;
static const uint32_t kSyntaxMaxTokensPerLine = 4096u;
#endif

// The current native editor has eight embedded document slots.  A 65,536-span
// per-document cap keeps the embedded cache bounded while leaving room for
// ordinary generated and hand-written source.  A document that exceeds this
// cap remains editable and is rendered as plain text.
#if defined(GXOS_DEVELOPER_STUDIO_BARE_METAL)
static const uint32_t kSyntaxMaxCachedTokenSpans = 4096u;
#else
static const uint32_t kSyntaxMaxCachedTokenSpans = 65536u;
#endif
static const uint32_t kSyntaxMaxRawStringDelimiterBytes = 4u;
static const uint32_t kSyntaxTabWidth = 4u;

struct SyntaxTokenSpan {
    uint32_t start;
    uint32_t length;
    SyntaxTokenKind kind;
};

// auxiliary stores a bounded raw-string delimiter: the high byte is the
// delimiter length and the low four bytes contain ASCII delimiter characters.
struct SyntaxLineState {
    SyntaxLineStateKind kind;
    uint32_t auxiliary;
};

struct SyntaxLineResult {
    SyntaxLineState inputState;
    SyntaxLineState outputState;
    uint32_t spanCount;
    bool spanValidationPassed;
    SyntaxErrorCode error;
};

struct SyntaxLineCache {
    uint32_t spanStart;
    uint16_t spanCount;
    uint8_t inputKind;
    uint8_t outputKind;
    uint32_t inputAuxiliary;
    uint32_t outputAuxiliary;
};

struct SyntaxCache {
    SyntaxLanguage language;
    bool valid;
    bool fallback;
    bool lastUpdateWasIncremental;
    bool lastUpdateConverged;
    SyntaxErrorCode fallbackCode;
    uint32_t lineCount;
    uint32_t totalSpanCount;
    uint32_t generation;
    uint32_t dirtyLineStart;
    uint32_t lastInvalidatedLineCount;
    uint32_t lastRetokenizedLineCount;
    uint32_t lastConvergenceLine;
    uint32_t linesInvalidated;
    uint32_t linesRetokenized;
    uint32_t fullRebuildCount;
    uint32_t incrementalRebuildCount;
    SyntaxLineCache lines[kSyntaxMaxHighlightedLineCount];
    SyntaxTokenSpan spans[kSyntaxMaxCachedTokenSpans];
    SyntaxTokenSpan rebuildSpans[kSyntaxMaxCachedTokenSpans];
    SyntaxTokenSpan scratch[kSyntaxMaxTokensPerLine];
};

struct SyntaxEditInfo {
    bool fullReplacement;
    uint32_t firstAffectedLine;
    int32_t lineDelta;
};

struct SyntaxSelection {
    bool active;
    uint32_t start;
    uint32_t end;
};

struct SyntaxRenderRun {
    uint32_t start;
    uint32_t length;
    SyntaxTokenKind kind;
    bool selected;
};

struct SyntaxPalette {
    uint32_t plainText;
    uint32_t keyword;
    uint32_t typeKeyword;
    uint32_t identifier;
    uint32_t number;
    uint32_t stringLiteral;
    uint32_t characterLiteral;
    uint32_t comment;
    uint32_t preprocessor;
    uint32_t op;
    uint32_t punctuation;
    uint32_t invalid;
    uint32_t selection;
};

SyntaxLanguage DetectSyntaxLanguage(const char* path);
const char* SyntaxLanguageName(SyntaxLanguage language);
const char* SyntaxTokenKindName(SyntaxTokenKind kind);
const char* SyntaxErrorName(SyntaxErrorCode code);
bool SyntaxIsKeyword(const char* text);
uint32_t SyntaxKeywordCount(SyntaxLanguage language);
const char* SyntaxKeywordAt(SyntaxLanguage language, uint32_t index);

bool SyntaxLineStateEqual(const SyntaxLineState& left, const SyntaxLineState& right);
bool SyntaxLineStateValid(const SyntaxLineState& state);
bool SyntaxValidateSpans(const SyntaxTokenSpan* spans, uint32_t count, uint32_t lineLength);

bool SyntaxTokenizeLine(SyntaxLanguage language, const char* line, uint32_t lineLength,
                        const SyntaxLineState& inputState, SyntaxLineResult* result,
                        SyntaxTokenSpan* spans, uint32_t spanCapacity);

void SyntaxCacheInit(SyntaxCache* cache);
bool SyntaxCacheBuild(SyntaxCache* cache, SyntaxLanguage language, const char* text, uint32_t length,
                      uint32_t generation);
bool SyntaxCacheUpdate(SyntaxCache* cache, SyntaxLanguage language, const char* text, uint32_t length,
                       uint32_t generation, const SyntaxEditInfo& edit);
void SyntaxCacheClear(SyntaxCache* cache);
bool SyntaxCacheValidate(const SyntaxCache* cache, const char* text, uint32_t length);
const SyntaxLineCache* SyntaxCacheLine(const SyntaxCache* cache, uint32_t line);
const SyntaxTokenSpan* SyntaxCacheLineSpans(const SyntaxCache* cache, uint32_t line, uint32_t* outCount);

// Produces contiguous plain/token runs, including uncovered plain gaps.  The
// model is used by the renderer and by tests; selection background has
// precedence over syntax background.
uint32_t SyntaxBuildRenderRuns(const SyntaxTokenSpan* spans, uint32_t spanCount,
                               uint32_t lineLength, const SyntaxSelection& selection,
                               SyntaxRenderRun* output, uint32_t outputCapacity);

const SyntaxPalette& DefaultSyntaxPalette();
uint32_t SyntaxPaletteColor(const SyntaxPalette& palette, SyntaxTokenKind kind, bool selected);

} // namespace developer_studio
} // namespace guidexos
