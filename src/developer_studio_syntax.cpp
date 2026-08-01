#include "developer_studio_syntax.h"

namespace guidexos {
namespace developer_studio {
namespace {

static uint32_t boundedLength(const char* text, uint32_t limit) {
    if (!text) return 0;
    uint32_t length = 0;
    while (length < limit && text[length] != '\0') ++length;
    return length;
}

static bool asciiEqual(const char* left, const char* right) {
    if (!left || !right) return left == right;
    uint32_t i = 0;
    while (left[i] != '\0' && right[i] != '\0') {
        if (left[i] != right[i]) return false;
        ++i;
    }
    return left[i] == right[i];
}

static bool asciiEqualRange(const char* text, uint32_t start, uint32_t length, const char* word) {
    if (!text || !word) return false;
    uint32_t i = 0;
    while (word[i] != '\0') {
        if (i >= length || text[start + i] != word[i]) return false;
        ++i;
    }
    return i == length;
}

static bool isAsciiSpace(char value) {
    return value == ' ' || value == '\t' || value == '\r' || value == '\f' || value == '\v';
}

static bool isAsciiLetter(char value) {
    return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z');
}

static bool isIdentifierStart(char value) {
    return isAsciiLetter(value) || value == '_';
}

static bool isIdentifierContinue(char value) {
    return isIdentifierStart(value) || (value >= '0' && value <= '9');
}

static bool isDecimalDigit(char value) {
    return value >= '0' && value <= '9';
}

static bool isHexDigit(char value) {
    return isDecimalDigit(value) || (value >= 'a' && value <= 'f') || (value >= 'A' && value <= 'F');
}

static bool isControlByte(char value) {
    unsigned char byte = static_cast<unsigned char>(value);
    return byte < 32u && value != '\t' && value != '\r';
}

static void setNormal(SyntaxLineState* state) {
    if (!state) return;
    state->kind = SyntaxLineStateKind::Normal;
    state->auxiliary = 0;
}

static uint32_t rawDelimiterLength(uint32_t auxiliary) {
    return (auxiliary >> 24) & 0xFFu;
}

static uint32_t packRawDelimiter(const char* text, uint32_t start, uint32_t length) {
    uint32_t auxiliary = (length & 0xFFu) << 24;
    for (uint32_t i = 0; i < length && i < kSyntaxMaxRawStringDelimiterBytes; ++i) {
        auxiliary |= static_cast<uint32_t>(static_cast<unsigned char>(text[start + i])) << (i * 8u);
    }
    return auxiliary;
}

static char unpackRawDelimiter(uint32_t auxiliary, uint32_t index) {
    return static_cast<char>((auxiliary >> (index * 8u)) & 0xFFu);
}

static bool isKeyword(SyntaxLanguage language, const char* text, uint32_t start, uint32_t length) {
    static const char* const cKeywords[] = {
        "auto", "break", "case", "const", "continue", "default", "do", "else", "enum", "extern",
        "for", "goto", "if", "inline", "register", "restrict", "return", "sizeof", "static",
        "struct", "switch", "typedef", "union", "volatile", "while", "_Alignas", "_Alignof",
        "_Atomic", "_Bool", "_Complex", "_Generic", "_Imaginary", "_Noreturn", "_Static_assert",
        "_Thread_local"
    };
    static const char* const cppKeywords[] = {
        "alignas", "alignof", "and", "and_eq", "asm", "auto", "bitand", "bitor", "break", "case",
        "catch", "class", "compl", "concept", "const", "consteval", "constexpr", "constinit",
        "const_cast", "continue", "co_await", "co_return", "co_yield", "decltype", "default", "delete",
        "do", "dynamic_cast", "else", "explicit", "export", "extern", "for", "friend", "goto", "if",
        "inline", "mutable", "namespace", "new", "noexcept", "not", "not_eq", "operator", "or", "or_eq",
        "private", "protected", "public", "register", "reinterpret_cast", "requires", "return", "signed",
        "sizeof", "static", "static_assert", "static_cast", "struct", "switch", "template", "this",
        "thread_local", "throw", "try", "typedef", "typeid", "typename", "union", "unsigned", "using",
        "virtual", "volatile", "while", "xor", "xor_eq", "override", "final"
    };
    const char* const* words = language == SyntaxLanguage::Cpp ? cppKeywords : cKeywords;
    const uint32_t count = language == SyntaxLanguage::Cpp
        ? static_cast<uint32_t>(sizeof(cppKeywords) / sizeof(cppKeywords[0]))
        : static_cast<uint32_t>(sizeof(cKeywords) / sizeof(cKeywords[0]));
    for (uint32_t i = 0; i < count; ++i) if (asciiEqualRange(text, start, length, words[i])) return true;
    return false;
}

static bool isTypeKeyword(SyntaxLanguage language, const char* text, uint32_t start, uint32_t length) {
    static const char* const cTypes[] = {
        "void", "char", "short", "int", "long", "float", "double", "signed", "unsigned", "_Bool",
        "size_t"
    };
    static const char* const cppTypes[] = {
        "void", "bool", "char", "short", "int", "long", "float", "double", "signed", "unsigned",
        "wchar_t", "char8_t", "char16_t", "char32_t", "size_t", "nullptr", "true", "false"
    };
    const char* const* words = language == SyntaxLanguage::Cpp ? cppTypes : cTypes;
    const uint32_t count = language == SyntaxLanguage::Cpp
        ? static_cast<uint32_t>(sizeof(cppTypes) / sizeof(cppTypes[0]))
        : static_cast<uint32_t>(sizeof(cTypes) / sizeof(cTypes[0]));
    for (uint32_t i = 0; i < count; ++i) if (asciiEqualRange(text, start, length, words[i])) return true;
    return false;
}

static bool emitSpan(SyntaxTokenSpan* spans, uint32_t capacity, uint32_t* count,
                     uint32_t start, uint32_t length, SyntaxTokenKind kind) {
    if (!count || length == 0) return true;
    if (!spans || *count >= capacity) return false;
    SyntaxTokenSpan& span = spans[*count];
    span.start = start;
    span.length = length;
    span.kind = kind;
    ++(*count);
    return true;
}

static bool matchAt(const char* line, uint32_t length, uint32_t start, const char* text, uint32_t textLength) {
    if (!line || start > length || textLength > length - start) return false;
    for (uint32_t i = 0; i < textLength; ++i) if (line[start + i] != text[i]) return false;
    return true;
}

static uint32_t scanNumber(const char* line, uint32_t start, uint32_t length) {
    uint32_t cursor = start;
    if (cursor < length && line[cursor] == '.') {
        ++cursor;
    } else if (cursor + 1 < length && line[cursor] == '0' && (line[cursor + 1] == 'x' || line[cursor + 1] == 'X')) {
        cursor += 2;
        while (cursor < length && (isHexDigit(line[cursor]) || line[cursor] == '\'' || line[cursor] == '_')) ++cursor;
        if (cursor < length && line[cursor] == '.') {
            ++cursor;
            while (cursor < length && (isHexDigit(line[cursor]) || line[cursor] == '\'' || line[cursor] == '_')) ++cursor;
        }
        if (cursor < length && (line[cursor] == 'p' || line[cursor] == 'P')) {
            ++cursor;
            if (cursor < length && (line[cursor] == '+' || line[cursor] == '-')) ++cursor;
            while (cursor < length && (isDecimalDigit(line[cursor]) || line[cursor] == '\'' || line[cursor] == '_')) ++cursor;
        }
    } else if (cursor + 1 < length && line[cursor] == '0' && (line[cursor + 1] == 'b' || line[cursor + 1] == 'B')) {
        cursor += 2;
        while (cursor < length && (line[cursor] == '0' || line[cursor] == '1' || line[cursor] == '\'' || line[cursor] == '_')) ++cursor;
    } else {
        while (cursor < length && (isDecimalDigit(line[cursor]) || line[cursor] == '\'' || line[cursor] == '_')) ++cursor;
        if (cursor < length && line[cursor] == '.') {
            ++cursor;
            while (cursor < length && (isDecimalDigit(line[cursor]) || line[cursor] == '\'' || line[cursor] == '_')) ++cursor;
        }
        if (cursor < length && (line[cursor] == 'e' || line[cursor] == 'E')) {
            ++cursor;
            if (cursor < length && (line[cursor] == '+' || line[cursor] == '-')) ++cursor;
            while (cursor < length && (isDecimalDigit(line[cursor]) || line[cursor] == '\'' || line[cursor] == '_')) ++cursor;
        }
    }
    while (cursor < length && (isAsciiLetter(line[cursor]) || isDecimalDigit(line[cursor]) || line[cursor] == '_')) ++cursor;
    return cursor > start ? cursor : start + 1;
}

static bool rawPrefix(const char* line, uint32_t start, uint32_t length,
                      uint32_t* quote, uint32_t* delimiterStart, uint32_t* delimiterLength) {
    uint32_t quoteIndex = start;
    if (matchAt(line, length, start, "R\"", 2)) quoteIndex = start + 1;
    else if (matchAt(line, length, start, "u8R\"", 4)) quoteIndex = start + 3;
    else if (matchAt(line, length, start, "uR\"", 3) || matchAt(line, length, start, "UR\"", 3) ||
             matchAt(line, length, start, "LR\"", 3)) quoteIndex = start + 2;
    else return false;
    uint32_t cursor = quoteIndex + 1;
    while (cursor < length && line[cursor] != '(') {
        if (cursor - (quoteIndex + 1) >= kSyntaxMaxRawStringDelimiterBytes || line[cursor] == ' ' ||
            line[cursor] == '\t' || line[cursor] == ')' || line[cursor] == '\\' || line[cursor] == '"') return false;
        ++cursor;
    }
    if (cursor >= length) return false;
    if (quote) *quote = quoteIndex;
    if (delimiterStart) *delimiterStart = quoteIndex + 1;
    if (delimiterLength) *delimiterLength = cursor - (quoteIndex + 1);
    return true;
}

static uint32_t rawClose(const char* line, uint32_t start, uint32_t length, uint32_t auxiliary) {
    const uint32_t delimiterLength = rawDelimiterLength(auxiliary);
    if (delimiterLength > kSyntaxMaxRawStringDelimiterBytes) return length;
    for (uint32_t cursor = start; cursor < length; ++cursor) {
        if (line[cursor] != ')' || cursor + delimiterLength + 1 >= length) continue;
        bool match = true;
        for (uint32_t i = 0; i < delimiterLength; ++i) {
            if (line[cursor + 1 + i] != unpackRawDelimiter(auxiliary, i)) { match = false; break; }
        }
        if (match && line[cursor + 1 + delimiterLength] == '"') return cursor + delimiterLength + 2;
    }
    return length;
}

static uint32_t scanQuoted(const char* line, uint32_t quote, uint32_t length, char delimiter) {
    uint32_t cursor = quote + 1;
    while (cursor < length) {
        if (line[cursor] == '\\') {
            if (cursor + 1 < length) cursor += 2;
            else ++cursor;
            continue;
        }
        if (line[cursor] == delimiter) return cursor + 1;
        ++cursor;
    }
    return length;
}

static bool ordinaryStringPrefix(const char* line, uint32_t start, uint32_t length, uint32_t* quote) {
    if (start < length && line[start] == '"') { if (quote) *quote = start; return true; }
    if (matchAt(line, length, start, "L\"", 2) || matchAt(line, length, start, "u\"", 2) ||
        matchAt(line, length, start, "U\"", 2)) { if (quote) *quote = start + 1; return true; }
    if (matchAt(line, length, start, "u8\"", 3)) { if (quote) *quote = start + 2; return true; }
    return false;
}

static bool ordinaryCharacterPrefix(const char* line, uint32_t start, uint32_t length, uint32_t* quote) {
    if (start < length && line[start] == '\'') { if (quote) *quote = start; return true; }
    if (matchAt(line, length, start, "L'", 2) || matchAt(line, length, start, "u'", 2) ||
        matchAt(line, length, start, "U'", 2)) { if (quote) *quote = start + 1; return true; }
    return false;
}

static bool operatorAt(const char* line, uint32_t length, uint32_t start, uint32_t* operatorLength) {
    static const char* const operators[] = {
        "<<=", ">>=", "->*", "...", "==", "!=", "<=", ">=", "&&", "||", "<<", ">>", "++", "--",
        "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", "->", "::", ".*"
    };
    for (uint32_t i = 0; i < sizeof(operators) / sizeof(operators[0]); ++i) {
        const uint32_t candidateLength = i < 4 ? 3u : 2u;
        if (matchAt(line, length, start, operators[i], candidateLength)) { if (operatorLength) *operatorLength = candidateLength; return true; }
    }
    static const char singleOperators[] = "+-*/%=<>!&|^~.? :";
    for (uint32_t i = 0; singleOperators[i] != '\0'; ++i) {
        if (line[start] == singleOperators[i] && line[start] != ' ') { if (operatorLength) *operatorLength = 1; return true; }
    }
    return false;
}

static bool punctuation(char value) {
    return value == '(' || value == ')' || value == '[' || value == ']' || value == '{' || value == '}' ||
           value == ',' || value == ';';
}

static uint32_t lineCountForText(const char* text, uint32_t length) {
    uint32_t count = 1;
    if (!text) return 0;
    for (uint32_t i = 0; i < length; ++i) if (text[i] == '\n') ++count;
    return count;
}

static uint32_t lineStartFor(const char* text, uint32_t length, uint32_t line) {
    if (!text) return 0;
    uint32_t current = 0;
    uint32_t index = 0;
    while (index < length && current < line) if (text[index++] == '\n') ++current;
    return index;
}

static uint32_t lineEndFor(const char* text, uint32_t length, uint32_t start) {
    uint32_t end = start;
    while (end < length && text[end] != '\n') ++end;
    if (end > start && text[end - 1] == '\r') --end;
    return end;
}

static void lineCacheSet(SyntaxLineCache* line, const SyntaxLineResult& result, uint32_t spanStart) {
    line->spanStart = spanStart;
    line->spanCount = static_cast<uint16_t>(result.spanCount);
    line->inputKind = static_cast<uint8_t>(result.inputState.kind);
    line->outputKind = static_cast<uint8_t>(result.outputState.kind);
    line->inputAuxiliary = result.inputState.auxiliary;
    line->outputAuxiliary = result.outputState.auxiliary;
}

static SyntaxLineState lineInput(const SyntaxLineCache& line) {
    SyntaxLineState state;
    state.kind = static_cast<SyntaxLineStateKind>(line.inputKind);
    state.auxiliary = line.inputAuxiliary;
    return state;
}

static SyntaxLineState lineOutput(const SyntaxLineCache& line) {
    SyntaxLineState state;
    state.kind = static_cast<SyntaxLineStateKind>(line.outputKind);
    state.auxiliary = line.outputAuxiliary;
    return state;
}

static void setFallback(SyntaxCache* cache, SyntaxLanguage language, SyntaxErrorCode error,
                        uint32_t generation, uint32_t lineCount) {
    if (!cache) return;
    cache->language = language;
    cache->valid = true;
    cache->fallback = true;
    cache->lastUpdateWasIncremental = false;
    cache->lastUpdateConverged = false;
    cache->fallbackCode = error;
    cache->lineCount = lineCount;
    cache->totalSpanCount = 0;
    cache->generation = generation;
    cache->dirtyLineStart = 0;
    cache->lastInvalidatedLineCount = lineCount;
    cache->lastRetokenizedLineCount = 0;
    cache->lastConvergenceLine = 0;
}

static bool replaceLineSpans(SyntaxCache* cache, uint32_t line, const SyntaxTokenSpan* spans, uint32_t count) {
    if (!cache || line >= cache->lineCount || count > kSyntaxMaxTokensPerLine) return false;
    SyntaxLineCache& cachedLine = cache->lines[line];
    const uint32_t oldStart = cachedLine.spanStart;
    const uint32_t oldCount = cachedLine.spanCount;
    if (oldStart > cache->totalSpanCount || oldCount > cache->totalSpanCount - oldStart) return false;
    const int32_t delta = static_cast<int32_t>(count) - static_cast<int32_t>(oldCount);
    if (delta > 0 && cache->totalSpanCount + static_cast<uint32_t>(delta) > kSyntaxMaxCachedTokenSpans) return false;
    const uint32_t oldEnd = oldStart + oldCount;
    if (delta > 0) {
        for (uint32_t index = cache->totalSpanCount; index > oldEnd; --index)
            cache->spans[index + static_cast<uint32_t>(delta) - 1] = cache->spans[index - 1];
    } else if (delta < 0) {
        for (uint32_t index = oldEnd; index < cache->totalSpanCount; ++index)
            cache->spans[index - static_cast<uint32_t>(-delta)] = cache->spans[index];
    }
    cache->totalSpanCount = static_cast<uint32_t>(static_cast<int32_t>(cache->totalSpanCount) + delta);
    for (uint32_t index = line + 1; index < cache->lineCount; ++index) {
        cache->lines[index].spanStart = static_cast<uint32_t>(static_cast<int32_t>(cache->lines[index].spanStart) + delta);
    }
    for (uint32_t index = 0; index < count; ++index) cache->spans[oldStart + index] = spans[index];
    cachedLine.spanStart = oldStart;
    cachedLine.spanCount = static_cast<uint16_t>(count);
    return true;
}

} // namespace

SyntaxLanguage DetectSyntaxLanguage(const char* path) {
    if (!path) return SyntaxLanguage::None;
    uint32_t length = boundedLength(path, 768);
    uint32_t start = length;
    while (start > 0 && path[start - 1] != '/' && path[start - 1] != static_cast<char>(92)) --start;
    uint32_t dot = length;
    for (uint32_t i = start; i < length; ++i) if (path[i] == '.') dot = i;
    if (dot == length || dot + 1 >= length) return SyntaxLanguage::None;
    const uint32_t extensionLength = length - dot;
    char extension[8] = {};
    if (extensionLength >= sizeof(extension)) return SyntaxLanguage::None;
    for (uint32_t i = 0; i < extensionLength; ++i) {
        char value = path[dot + i];
        if (value >= 'A' && value <= 'Z') value = static_cast<char>(value + ('a' - 'A'));
        extension[i] = value;
    }
    if (asciiEqual(extension, ".c")) return SyntaxLanguage::C;
    if (asciiEqual(extension, ".cc") || asciiEqual(extension, ".cpp") || asciiEqual(extension, ".cxx") ||
        asciiEqual(extension, ".hpp") || asciiEqual(extension, ".hh") || asciiEqual(extension, ".hxx") ||
        asciiEqual(extension, ".h")) return SyntaxLanguage::Cpp;
    return SyntaxLanguage::None;
}

const char* SyntaxLanguageName(SyntaxLanguage language) {
    switch (language) {
    case SyntaxLanguage::C: return "C";
    case SyntaxLanguage::Cpp: return "C++";
    default: return "None";
    }
}

const char* SyntaxTokenKindName(SyntaxTokenKind kind) {
    switch (kind) {
    case SyntaxTokenKind::PlainText: return "plain_text";
    case SyntaxTokenKind::Keyword: return "keyword";
    case SyntaxTokenKind::TypeKeyword: return "type_keyword";
    case SyntaxTokenKind::Identifier: return "identifier";
    case SyntaxTokenKind::Number: return "number";
    case SyntaxTokenKind::StringLiteral: return "string";
    case SyntaxTokenKind::CharacterLiteral: return "character";
    case SyntaxTokenKind::Comment: return "comment";
    case SyntaxTokenKind::Preprocessor: return "preprocessor";
    case SyntaxTokenKind::Operator: return "operator";
    case SyntaxTokenKind::Punctuation: return "punctuation";
    case SyntaxTokenKind::Invalid: return "invalid";
    default: return "unknown";
    }
}

const char* SyntaxErrorName(SyntaxErrorCode code) {
    switch (code) {
    case SyntaxErrorCode::None: return "SYNTAX_NONE";
    case SyntaxErrorCode::UnsupportedLanguage: return "SYNTAX_UNSUPPORTED_LANGUAGE";
    case SyntaxErrorCode::DocumentTooLarge: return "SYNTAX_DOCUMENT_TOO_LARGE";
    case SyntaxErrorCode::TooManyLines: return "SYNTAX_TOO_MANY_LINES";
    case SyntaxErrorCode::LineTooLong: return "SYNTAX_LINE_TOO_LONG";
    case SyntaxErrorCode::TokenLimit: return "SYNTAX_TOKEN_LIMIT";
    case SyntaxErrorCode::CacheInvalid: return "SYNTAX_CACHE_INVALID";
    case SyntaxErrorCode::StateInvalid: return "SYNTAX_STATE_INVALID";
    case SyntaxErrorCode::IncrementalFallback: return "SYNTAX_INCREMENTAL_FALLBACK";
    case SyntaxErrorCode::RenderSpanInvalid: return "SYNTAX_RENDER_SPAN_INVALID";
    default: return "SYNTAX_UNKNOWN";
    }
}

bool SyntaxIsKeyword(const char* text) {
    if (!text || text[0] == '\0') return false;
    uint32_t length = 0;
    while (length <= 1024u && text[length] != '\0') ++length;
    if (length > 1024u) return false;
    return isKeyword(SyntaxLanguage::C, text, 0, length) ||
        isKeyword(SyntaxLanguage::Cpp, text, 0, length) ||
        isTypeKeyword(SyntaxLanguage::C, text, 0, length) ||
        isTypeKeyword(SyntaxLanguage::Cpp, text, 0, length);
}

bool SyntaxLineStateEqual(const SyntaxLineState& left, const SyntaxLineState& right) {
    return left.kind == right.kind && left.auxiliary == right.auxiliary;
}

bool SyntaxLineStateValid(const SyntaxLineState& state) {
    if (state.kind == SyntaxLineStateKind::Normal || state.kind == SyntaxLineStateKind::InBlockComment ||
        state.kind == SyntaxLineStateKind::InStringContinuation || state.kind == SyntaxLineStateKind::InCharacterContinuation)
        return state.auxiliary == 0;
    if (state.kind == SyntaxLineStateKind::InRawString) return rawDelimiterLength(state.auxiliary) <= kSyntaxMaxRawStringDelimiterBytes;
    return false;
}

bool SyntaxValidateSpans(const SyntaxTokenSpan* spans, uint32_t count, uint32_t lineLength) {
    if (count > kSyntaxMaxTokensPerLine || (count != 0 && !spans)) return false;
    uint32_t previousEnd = 0;
    for (uint32_t i = 0; i < count; ++i) {
        const SyntaxTokenSpan& span = spans[i];
        if (span.kind == SyntaxTokenKind::PlainText || span.kind >= SyntaxTokenKind::Count ||
            span.start > lineLength || span.length > lineLength - span.start ||
            (i > 0 && span.start < previousEnd)) return false;
        previousEnd = span.start + span.length;
    }
    return true;
}

bool SyntaxTokenizeLine(SyntaxLanguage language, const char* line, uint32_t lineLength,
                        const SyntaxLineState& inputState, SyntaxLineResult* result,
                        SyntaxTokenSpan* spans, uint32_t spanCapacity) {
    if (!result) return false;
    result->inputState = inputState;
    result->outputState = inputState;
    result->spanCount = 0;
    result->spanValidationPassed = false;
    result->error = SyntaxErrorCode::None;
    if (!SyntaxLineStateValid(inputState)) { result->error = SyntaxErrorCode::StateInvalid; return false; }
    if (!line && lineLength != 0) { result->error = SyntaxErrorCode::CacheInvalid; return false; }
    if (lineLength > kSyntaxMaxTokenizableLineBytes) { result->error = SyntaxErrorCode::LineTooLong; return false; }
    if (language == SyntaxLanguage::None) {
        setNormal(&result->outputState);
        result->spanValidationPassed = true;
        return true;
    }

    uint32_t firstNonWhitespace = 0;
    while (firstNonWhitespace < lineLength && isAsciiSpace(line[firstNonWhitespace])) ++firstNonWhitespace;
    const bool directive = firstNonWhitespace < lineLength && line[firstNonWhitespace] == '#' &&
        inputState.kind == SyntaxLineStateKind::Normal;
    SyntaxLineState state = inputState;
    uint32_t cursor = 0;
    uint32_t iterations = 0;
    while (cursor < lineLength) {
        if (++iterations > lineLength * 8u + 32u) {
            result->error = SyntaxErrorCode::CacheInvalid;
            return false;
        }
        if (state.kind == SyntaxLineStateKind::InBlockComment) {
            uint32_t end = cursor;
            while (end + 1 < lineLength && !(line[end] == '*' && line[end + 1] == '/')) ++end;
            if (end + 1 < lineLength) {
                end += 2;
                if (!emitSpan(spans, spanCapacity, &result->spanCount, cursor, end - cursor, SyntaxTokenKind::Comment)) {
                    result->error = SyntaxErrorCode::TokenLimit; return false;
                }
                setNormal(&state);
                cursor = end;
            } else {
                if (!emitSpan(spans, spanCapacity, &result->spanCount, cursor, lineLength - cursor, SyntaxTokenKind::Comment)) {
                    result->error = SyntaxErrorCode::TokenLimit; return false;
                }
                result->outputState = state;
                result->spanValidationPassed = SyntaxValidateSpans(spans, result->spanCount, lineLength);
                return result->spanValidationPassed;
            }
            continue;
        }
        if (state.kind == SyntaxLineStateKind::InRawString) {
            uint32_t end = rawClose(line, cursor, lineLength, state.auxiliary);
            if (end < lineLength) {
                if (!emitSpan(spans, spanCapacity, &result->spanCount, cursor, end - cursor, SyntaxTokenKind::StringLiteral)) {
                    result->error = SyntaxErrorCode::TokenLimit; return false;
                }
                setNormal(&state);
                cursor = end;
            } else {
                if (!emitSpan(spans, spanCapacity, &result->spanCount, cursor, lineLength - cursor, SyntaxTokenKind::StringLiteral)) {
                    result->error = SyntaxErrorCode::TokenLimit; return false;
                }
                result->outputState = state;
                result->spanValidationPassed = SyntaxValidateSpans(spans, result->spanCount, lineLength);
                return result->spanValidationPassed;
            }
            continue;
        }
        if (isAsciiSpace(line[cursor])) { ++cursor; continue; }
        if (directive && cursor == firstNonWhitespace && line[cursor] == '#') {
            uint32_t end = cursor + 1;
            while (end < lineLength && isAsciiSpace(line[end])) ++end;
            while (end < lineLength && isIdentifierContinue(line[end])) ++end;
            if (!emitSpan(spans, spanCapacity, &result->spanCount, cursor, end - cursor, SyntaxTokenKind::Preprocessor)) {
                result->error = SyntaxErrorCode::TokenLimit; return false;
            }
            cursor = end;
            continue;
        }
        if (cursor + 1 < lineLength && line[cursor] == '/' && line[cursor + 1] == '/') {
            if (!emitSpan(spans, spanCapacity, &result->spanCount, cursor, lineLength - cursor, SyntaxTokenKind::Comment)) {
                result->error = SyntaxErrorCode::TokenLimit; return false;
            }
            cursor = lineLength;
            continue;
        }
        if (cursor + 1 < lineLength && line[cursor] == '/' && line[cursor + 1] == '*') {
            uint32_t end = cursor + 2;
            while (end + 1 < lineLength && !(line[end] == '*' && line[end + 1] == '/')) ++end;
            if (end + 1 < lineLength) {
                end += 2;
                if (!emitSpan(spans, spanCapacity, &result->spanCount, cursor, end - cursor, SyntaxTokenKind::Comment)) {
                    result->error = SyntaxErrorCode::TokenLimit; return false;
                }
                cursor = end;
            } else {
                if (!emitSpan(spans, spanCapacity, &result->spanCount, cursor, lineLength - cursor, SyntaxTokenKind::Comment)) {
                    result->error = SyntaxErrorCode::TokenLimit; return false;
                }
                state.kind = SyntaxLineStateKind::InBlockComment;
                state.auxiliary = 0;
                cursor = lineLength;
            }
            continue;
        }
        uint32_t quote = cursor;
        uint32_t delimiterStart = 0;
        uint32_t delimiterLength = 0;
        if (language == SyntaxLanguage::Cpp && rawPrefix(line, cursor, lineLength, &quote, &delimiterStart, &delimiterLength)) {
            const uint32_t auxiliary = packRawDelimiter(line, delimiterStart, delimiterLength);
            uint32_t end = rawClose(line, delimiterStart + delimiterLength + 1, lineLength, auxiliary);
            if (end < lineLength) {
                if (!emitSpan(spans, spanCapacity, &result->spanCount, cursor, end - cursor, SyntaxTokenKind::StringLiteral)) {
                    result->error = SyntaxErrorCode::TokenLimit; return false;
                }
                cursor = end;
            } else {
                if (!emitSpan(spans, spanCapacity, &result->spanCount, cursor, lineLength - cursor, SyntaxTokenKind::StringLiteral)) {
                    result->error = SyntaxErrorCode::TokenLimit; return false;
                }
                state.kind = SyntaxLineStateKind::InRawString;
                state.auxiliary = auxiliary;
                cursor = lineLength;
            }
            continue;
        }
        if (ordinaryStringPrefix(line, cursor, lineLength, &quote)) {
            uint32_t end = scanQuoted(line, quote, lineLength, '"');
            if (!emitSpan(spans, spanCapacity, &result->spanCount, cursor, end - cursor, SyntaxTokenKind::StringLiteral)) {
                result->error = SyntaxErrorCode::TokenLimit; return false;
            }
            cursor = end;
            continue;
        }
        if (ordinaryCharacterPrefix(line, cursor, lineLength, &quote)) {
            uint32_t end = scanQuoted(line, quote, lineLength, '\'');
            if (!emitSpan(spans, spanCapacity, &result->spanCount, cursor, end - cursor, SyntaxTokenKind::CharacterLiteral)) {
                result->error = SyntaxErrorCode::TokenLimit; return false;
            }
            cursor = end;
            continue;
        }
        if (isDecimalDigit(line[cursor]) || (line[cursor] == '.' && cursor + 1 < lineLength && isDecimalDigit(line[cursor + 1]))) {
            uint32_t end = scanNumber(line, cursor, lineLength);
            if (!emitSpan(spans, spanCapacity, &result->spanCount, cursor, end - cursor, SyntaxTokenKind::Number)) {
                result->error = SyntaxErrorCode::TokenLimit; return false;
            }
            cursor = end;
            continue;
        }
        if (isIdentifierStart(line[cursor])) {
            uint32_t end = cursor + 1;
            while (end < lineLength && isIdentifierContinue(line[end])) ++end;
            SyntaxTokenKind kind = SyntaxTokenKind::Identifier;
            if (isTypeKeyword(language, line, cursor, end - cursor)) kind = SyntaxTokenKind::TypeKeyword;
            else if (isKeyword(language, line, cursor, end - cursor)) kind = SyntaxTokenKind::Keyword;
            if (!emitSpan(spans, spanCapacity, &result->spanCount, cursor, end - cursor, kind)) {
                result->error = SyntaxErrorCode::TokenLimit; return false;
            }
            cursor = end;
            continue;
        }
        uint32_t operatorLength = 0;
        if (operatorAt(line, lineLength, cursor, &operatorLength)) {
            if (!emitSpan(spans, spanCapacity, &result->spanCount, cursor, operatorLength, SyntaxTokenKind::Operator)) {
                result->error = SyntaxErrorCode::TokenLimit; return false;
            }
            cursor += operatorLength;
            continue;
        }
        if (punctuation(line[cursor])) {
            if (!emitSpan(spans, spanCapacity, &result->spanCount, cursor, 1, SyntaxTokenKind::Punctuation)) {
                result->error = SyntaxErrorCode::TokenLimit; return false;
            }
            ++cursor;
            continue;
        }
        const SyntaxTokenKind kind = isControlByte(line[cursor]) ? SyntaxTokenKind::Invalid : SyntaxTokenKind::PlainText;
        if (kind != SyntaxTokenKind::PlainText && !emitSpan(spans, spanCapacity, &result->spanCount, cursor, 1, kind)) {
            result->error = SyntaxErrorCode::TokenLimit; return false;
        }
        ++cursor;
    }
    result->outputState = state;
    result->spanValidationPassed = SyntaxValidateSpans(spans, result->spanCount, lineLength);
    if (!result->spanValidationPassed) result->error = SyntaxErrorCode::RenderSpanInvalid;
    return result->spanValidationPassed;
}

void SyntaxCacheInit(SyntaxCache* cache) {
    if (!cache) return;
    cache->language = SyntaxLanguage::None;
    cache->valid = false;
    cache->fallback = false;
    cache->lastUpdateWasIncremental = false;
    cache->lastUpdateConverged = false;
    cache->fallbackCode = SyntaxErrorCode::None;
    cache->lineCount = 0;
    cache->totalSpanCount = 0;
    cache->generation = 0;
    cache->dirtyLineStart = 0;
    cache->lastInvalidatedLineCount = 0;
    cache->lastRetokenizedLineCount = 0;
    cache->lastConvergenceLine = 0;
    cache->linesInvalidated = 0;
    cache->linesRetokenized = 0;
    cache->fullRebuildCount = 0;
    cache->incrementalRebuildCount = 0;
}

void SyntaxCacheClear(SyntaxCache* cache) {
    SyntaxCacheInit(cache);
}

bool SyntaxCacheBuild(SyntaxCache* cache, SyntaxLanguage language, const char* text, uint32_t length,
                      uint32_t generation) {
    if (!cache || (!text && length != 0)) return false;
    ++cache->fullRebuildCount;
    cache->language = language;
    cache->lastUpdateWasIncremental = false;
    cache->lastUpdateConverged = false;
    cache->fallbackCode = SyntaxErrorCode::None;
    const uint32_t lineCount = text ? lineCountForText(text, length) : 1;
    if (length > kSyntaxMaxHighlightedDocumentBytes) {
        setFallback(cache, language, SyntaxErrorCode::DocumentTooLarge, generation, 0);
        return true;
    }
    if (lineCount > kSyntaxMaxHighlightedLineCount) {
        setFallback(cache, language, SyntaxErrorCode::TooManyLines, generation, 0);
        return true;
    }
    cache->valid = true;
    cache->fallback = false;
    cache->lineCount = lineCount;
    cache->totalSpanCount = 0;
    cache->generation = generation;
    cache->dirtyLineStart = 0;
    cache->lastInvalidatedLineCount = lineCount;
    cache->lastRetokenizedLineCount = 0;
    SyntaxLineState state;
    setNormal(&state);
    uint32_t start = 0;
    for (uint32_t lineIndex = 0; lineIndex < lineCount; ++lineIndex) {
        const uint32_t end = lineEndFor(text, length, start);
        const uint32_t lineLength = end >= start ? end - start : 0;
        if (lineLength > kSyntaxMaxTokenizableLineBytes) {
            setFallback(cache, language, SyntaxErrorCode::LineTooLong, generation, lineCount);
            return true;
        }
        SyntaxLineResult result;
        const uint32_t remainingCapacity = kSyntaxMaxCachedTokenSpans - cache->totalSpanCount;
        const uint32_t capacity = remainingCapacity < kSyntaxMaxTokensPerLine ? remainingCapacity : kSyntaxMaxTokensPerLine;
        if (!SyntaxTokenizeLine(language, text + start, lineLength, state, &result,
                                 cache->spans + cache->totalSpanCount, capacity)) {
            setFallback(cache, language, result.error == SyntaxErrorCode::None ? SyntaxErrorCode::CacheInvalid : result.error,
                        generation, lineCount);
            return true;
        }
        if (result.spanCount > kSyntaxMaxTokensPerLine || result.spanCount > capacity) {
            setFallback(cache, language, SyntaxErrorCode::TokenLimit, generation, lineCount);
            return true;
        }
        lineCacheSet(&cache->lines[lineIndex], result, cache->totalSpanCount);
        cache->totalSpanCount += result.spanCount;
        ++cache->lastRetokenizedLineCount;
        state = result.outputState;
        start = end < length && text[end] == '\n' ? end + 1 : length;
    }
    cache->linesInvalidated += lineCount;
    cache->linesRetokenized += cache->lastRetokenizedLineCount;
    return true;
}

static bool spansEqual(const SyntaxTokenSpan* left, uint32_t leftCount,
                       const SyntaxTokenSpan* right, uint32_t rightCount) {
    if (leftCount != rightCount) return false;
    for (uint32_t i = 0; i < leftCount; ++i) {
        if (left[i].start != right[i].start || left[i].length != right[i].length || left[i].kind != right[i].kind) return false;
    }
    return true;
}

static bool SyntaxCacheUpdateLineStructure(SyntaxCache* cache, SyntaxLanguage language, const char* text,
                                           uint32_t length, uint32_t generation, const SyntaxEditInfo& edit,
                                           uint32_t newLineCount) {
    const uint32_t oldLineCount = cache->lineCount;
    const uint32_t oldTotalSpanCount = cache->totalSpanCount;
    uint32_t first = edit.firstAffectedLine < oldLineCount ? edit.firstAffectedLine : oldLineCount - 1;
    int32_t newResumeSigned = static_cast<int32_t>(first) + 1 + (edit.lineDelta > 0 ? edit.lineDelta : 0);
    int32_t oldResumeSigned = newResumeSigned - edit.lineDelta;
    if (newResumeSigned < 0) newResumeSigned = 0;
    if (oldResumeSigned < 0) oldResumeSigned = 0;
    if (newResumeSigned > static_cast<int32_t>(newLineCount)) newResumeSigned = static_cast<int32_t>(newLineCount);
    if (oldResumeSigned > static_cast<int32_t>(oldLineCount)) oldResumeSigned = static_cast<int32_t>(oldLineCount);
    const uint32_t newResume = static_cast<uint32_t>(newResumeSigned);
    const uint32_t oldResume = static_cast<uint32_t>(oldResumeSigned);

    if (edit.lineDelta > 0) {
        const uint32_t delta = static_cast<uint32_t>(edit.lineDelta);
        for (uint32_t index = oldLineCount; index > oldResume; --index) cache->lines[index + delta - 1] = cache->lines[index - 1];
    } else if (edit.lineDelta < 0) {
        const uint32_t delta = static_cast<uint32_t>(-edit.lineDelta);
        for (uint32_t index = oldResume; index < oldLineCount; ++index) cache->lines[index - delta] = cache->lines[index];
    }
    cache->lineCount = newLineCount;
    const uint32_t prefixSpanCount = first < oldLineCount ? cache->lines[first].spanStart : oldTotalSpanCount;
    if (prefixSpanCount > oldTotalSpanCount) return SyntaxCacheBuild(cache, language, text, length, generation);
    for (uint32_t i = 0; i < prefixSpanCount; ++i) cache->rebuildSpans[i] = cache->spans[i];

    cache->lastUpdateWasIncremental = true;
    cache->lastUpdateConverged = false;
    cache->lastInvalidatedLineCount = 0;
    cache->lastRetokenizedLineCount = 0;
    cache->lastConvergenceLine = 0;
    cache->dirtyLineStart = first;
    ++cache->incrementalRebuildCount;

    SyntaxLineState input = first == 0 ? SyntaxLineState{ SyntaxLineStateKind::Normal, 0 } : lineOutput(cache->lines[first - 1]);
    uint32_t line = first;
    uint32_t start = lineStartFor(text, length, first);
    uint32_t rebuiltSpanCount = prefixSpanCount;
    while (line < newLineCount) {
        const uint32_t end = lineEndFor(text, length, start);
        const uint32_t lineLength = end >= start ? end - start : 0;
        if (lineLength > kSyntaxMaxTokenizableLineBytes) return SyntaxCacheBuild(cache, language, text, length, generation);
        SyntaxLineResult result;
        if (!SyntaxTokenizeLine(language, text + start, lineLength, input, &result,
                                cache->scratch, kSyntaxMaxTokensPerLine))
            return SyntaxCacheBuild(cache, language, text, length, generation);
        if (result.spanCount > kSyntaxMaxCachedTokenSpans - rebuiltSpanCount)
            return SyntaxCacheBuild(cache, language, text, length, generation);
        const bool suffixCandidate = line >= newResume && line < newLineCount && line < oldLineCount + static_cast<uint32_t>(edit.lineDelta > 0 ? edit.lineDelta : 0);
        SyntaxLineCache oldLine = {};
        if (suffixCandidate) oldLine = cache->lines[line];
        const bool suffixUnchanged = suffixCandidate &&
            SyntaxLineStateEqual(result.inputState, lineInput(oldLine)) &&
            SyntaxLineStateEqual(result.outputState, lineOutput(oldLine)) &&
            spansEqual(cache->scratch, result.spanCount, cache->spans + oldLine.spanStart, oldLine.spanCount);
        lineCacheSet(&cache->lines[line], result, rebuiltSpanCount);
        for (uint32_t i = 0; i < result.spanCount; ++i) cache->rebuildSpans[rebuiltSpanCount + i] = cache->scratch[i];
        rebuiltSpanCount += result.spanCount;
        ++cache->lastInvalidatedLineCount;
        ++cache->lastRetokenizedLineCount;
        ++cache->linesInvalidated;
        ++cache->linesRetokenized;
        cache->lastConvergenceLine = line;
        input = result.outputState;
        if (suffixUnchanged) {
            cache->lastUpdateConverged = true;
            uint32_t suffix = line + 1;
            while (suffix < newLineCount) {
                const SyntaxLineCache oldSuffix = cache->lines[suffix];
                if (oldSuffix.spanStart > oldTotalSpanCount || oldSuffix.spanCount > oldTotalSpanCount - oldSuffix.spanStart ||
                    rebuiltSpanCount + oldSuffix.spanCount > kSyntaxMaxCachedTokenSpans) return SyntaxCacheBuild(cache, language, text, length, generation);
                cache->lines[suffix].spanStart = rebuiltSpanCount;
                for (uint32_t i = 0; i < oldSuffix.spanCount; ++i)
                    cache->rebuildSpans[rebuiltSpanCount + i] = cache->spans[oldSuffix.spanStart + i];
                rebuiltSpanCount += oldSuffix.spanCount;
                ++suffix;
            }
            break;
        }
        if (end >= length) break;
        start = end + 1;
        ++line;
    }
    for (uint32_t i = 0; i < rebuiltSpanCount; ++i) cache->spans[i] = cache->rebuildSpans[i];
    cache->totalSpanCount = rebuiltSpanCount;
    cache->generation = generation;
    cache->valid = true;
    cache->fallback = false;
    cache->fallbackCode = SyntaxErrorCode::None;
    return true;
}

bool SyntaxCacheUpdate(SyntaxCache* cache, SyntaxLanguage language, const char* text, uint32_t length,
                       uint32_t generation, const SyntaxEditInfo& edit) {
    if (!cache || (!text && length != 0)) return false;
    const uint32_t newLineCount = lineCountForText(text, length);
    const bool expectedLineDelta = edit.lineDelta != 0 &&
        static_cast<int64_t>(cache->lineCount) + static_cast<int64_t>(edit.lineDelta) == static_cast<int64_t>(newLineCount);
    if (!cache->valid || cache->language != language || edit.fullReplacement || cache->fallback ||
        (edit.lineDelta == 0 && cache->lineCount != newLineCount) ||
        (edit.lineDelta != 0 && !expectedLineDelta) ||
        cache->totalSpanCount > kSyntaxMaxCachedTokenSpans) {
        const bool rebuilt = SyntaxCacheBuild(cache, language, text, length, generation);
        if (rebuilt) cache->fallbackCode = cache->fallback ? cache->fallbackCode : SyntaxErrorCode::None;
        return rebuilt;
    }
    if (edit.lineDelta != 0) return SyntaxCacheUpdateLineStructure(cache, language, text, length, generation, edit, newLineCount);
    if (edit.firstAffectedLine >= cache->lineCount) return SyntaxCacheBuild(cache, language, text, length, generation);
    cache->lastUpdateWasIncremental = true;
    cache->lastUpdateConverged = false;
    cache->lastInvalidatedLineCount = 0;
    cache->lastRetokenizedLineCount = 0;
    cache->dirtyLineStart = edit.firstAffectedLine;
    ++cache->incrementalRebuildCount;
    uint32_t line = edit.firstAffectedLine;
    uint32_t start = lineStartFor(text, length, line);
    SyntaxLineState input = line == 0 ? SyntaxLineState{ SyntaxLineStateKind::Normal, 0 } : lineOutput(cache->lines[line - 1]);
    while (line < cache->lineCount) {
        const uint32_t end = lineEndFor(text, length, start);
        const uint32_t lineLength = end >= start ? end - start : 0;
        if (lineLength > kSyntaxMaxTokenizableLineBytes) return SyntaxCacheBuild(cache, language, text, length, generation);
        const SyntaxLineCache oldLine = cache->lines[line];
        SyntaxLineResult result;
        if (!SyntaxTokenizeLine(language, text + start, lineLength, input, &result,
                                cache->scratch, kSyntaxMaxTokensPerLine)) {
            return SyntaxCacheBuild(cache, language, text, length, generation);
        }
        if (!replaceLineSpans(cache, line, cache->scratch, result.spanCount)) {
            const bool rebuilt = SyntaxCacheBuild(cache, language, text, length, generation);
            if (rebuilt && !cache->fallback) cache->fallbackCode = SyntaxErrorCode::IncrementalFallback;
            return rebuilt;
        }
        lineCacheSet(&cache->lines[line], result, cache->lines[line].spanStart);
        ++cache->lastInvalidatedLineCount;
        ++cache->lastRetokenizedLineCount;
        ++cache->linesInvalidated;
        ++cache->linesRetokenized;
        cache->lastConvergenceLine = line;
        const bool converged = SyntaxLineStateEqual(result.inputState, lineInput(oldLine)) &&
                               SyntaxLineStateEqual(result.outputState, lineOutput(oldLine));
        input = result.outputState;
        if (converged) {
            cache->lastUpdateConverged = true;
            break;
        }
        if (end >= length) break;
        start = end + 1;
        ++line;
    }
    cache->generation = generation;
    cache->valid = true;
    cache->fallback = false;
    cache->fallbackCode = SyntaxErrorCode::None;
    return true;
}

bool SyntaxCacheValidate(const SyntaxCache* cache, const char* text, uint32_t length) {
    if (!cache || !cache->valid || (!text && length != 0)) return false;
    if (cache->fallback) return true;
    if (cache->lineCount != lineCountForText(text, length) || cache->lineCount > kSyntaxMaxHighlightedLineCount ||
        cache->totalSpanCount > kSyntaxMaxCachedTokenSpans) return false;
    for (uint32_t line = 0; line < cache->lineCount; ++line) {
        const uint32_t start = lineStartFor(text, length, line);
        const uint32_t end = lineEndFor(text, length, start);
        const uint32_t lineLength = end >= start ? end - start : 0;
        const SyntaxLineCache& cached = cache->lines[line];
        if (cached.spanStart > cache->totalSpanCount || cached.spanCount > cache->totalSpanCount - cached.spanStart ||
            !SyntaxLineStateValid(lineInput(cached)) || !SyntaxLineStateValid(lineOutput(cached)) ||
            !SyntaxValidateSpans(cache->spans + cached.spanStart, cached.spanCount, lineLength)) return false;
    }
    return true;
}

const SyntaxLineCache* SyntaxCacheLine(const SyntaxCache* cache, uint32_t line) {
    if (!cache || !cache->valid || cache->fallback || line >= cache->lineCount) return nullptr;
    return &cache->lines[line];
}

const SyntaxTokenSpan* SyntaxCacheLineSpans(const SyntaxCache* cache, uint32_t line, uint32_t* outCount) {
    if (outCount) *outCount = 0;
    const SyntaxLineCache* cached = SyntaxCacheLine(cache, line);
    if (!cached || cached->spanStart > cache->totalSpanCount || cached->spanCount > cache->totalSpanCount - cached->spanStart) return nullptr;
    if (outCount) *outCount = cached->spanCount;
    return cache->spans + cached->spanStart;
}

static bool appendRenderRange(SyntaxRenderRun* output, uint32_t capacity, uint32_t* count,
                              uint32_t start, uint32_t length, SyntaxTokenKind kind,
                              const SyntaxSelection& selection) {
    if (!count || length == 0) return true;
    uint32_t cursor = start;
    const uint32_t end = start + length;
    const bool selectionValid = selection.active && selection.end > selection.start;
    while (cursor < end) {
        bool selected = selectionValid && cursor >= selection.start && cursor < selection.end;
        uint32_t next = end;
        if (selectionValid && selected && selection.end < next) next = selection.end;
        if (selectionValid && !selected && selection.start > cursor && selection.start < next) next = selection.start;
        if (*count >= capacity) return false;
        output[*count].start = cursor;
        output[*count].length = next - cursor;
        output[*count].kind = kind;
        output[*count].selected = selected;
        ++(*count);
        cursor = next;
    }
    return true;
}

uint32_t SyntaxBuildRenderRuns(const SyntaxTokenSpan* spans, uint32_t spanCount,
                               uint32_t lineLength, const SyntaxSelection& selection,
                               SyntaxRenderRun* output, uint32_t outputCapacity) {
    if (!output || outputCapacity == 0 || !SyntaxValidateSpans(spans, spanCount, lineLength)) return 0;
    uint32_t count = 0;
    uint32_t cursor = 0;
    for (uint32_t i = 0; i < spanCount; ++i) {
        if (spans[i].start > cursor && !appendRenderRange(output, outputCapacity, &count, cursor, spans[i].start - cursor,
                                                            SyntaxTokenKind::PlainText, selection)) return 0;
        if (!appendRenderRange(output, outputCapacity, &count, spans[i].start, spans[i].length, spans[i].kind, selection)) return 0;
        cursor = spans[i].start + spans[i].length;
    }
    if (cursor < lineLength && !appendRenderRange(output, outputCapacity, &count, cursor, lineLength - cursor,
                                                    SyntaxTokenKind::PlainText, selection)) return 0;
    if (lineLength == 0 && count == 0) return 0;
    return count;
}

const SyntaxPalette& DefaultSyntaxPalette() {
    static const SyntaxPalette palette = {
        0x111722u, 0x394F79u, 0x3C654Du, 0x111722u, 0x5C4A36u, 0x684638u,
        0x684638u, 0x394047u, 0x5B426Bu, 0x464A63u, 0x34445Bu, 0x713F46u, 0x2F5D8Au
    };
    return palette;
}

uint32_t SyntaxPaletteColor(const SyntaxPalette& palette, SyntaxTokenKind kind, bool selected) {
    if (selected) return palette.selection;
    switch (kind) {
    case SyntaxTokenKind::Keyword: return palette.keyword;
    case SyntaxTokenKind::TypeKeyword: return palette.typeKeyword;
    case SyntaxTokenKind::Identifier: return palette.identifier;
    case SyntaxTokenKind::Number: return palette.number;
    case SyntaxTokenKind::StringLiteral: return palette.stringLiteral;
    case SyntaxTokenKind::CharacterLiteral: return palette.characterLiteral;
    case SyntaxTokenKind::Comment: return palette.comment;
    case SyntaxTokenKind::Preprocessor: return palette.preprocessor;
    case SyntaxTokenKind::Operator: return palette.op;
    case SyntaxTokenKind::Punctuation: return palette.punctuation;
    case SyntaxTokenKind::Invalid: return palette.invalid;
    default: return palette.plainText;
    }
}

} // namespace developer_studio
} // namespace guidexos
