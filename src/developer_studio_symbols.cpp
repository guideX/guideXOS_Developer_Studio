#include "developer_studio_symbols.h"

namespace guidexos {
namespace developer_studio {
namespace {

static const uint32_t kMaxTokens = 131072u;
static const uint32_t kMaxScopeDepth = 96u;
static const uint32_t kMaxConditionalDepth = 64u;
static const uint32_t kMaxProjectDepth = 32u;

enum LexKind {
    LexIdentifier = 0,
    LexNumber,
    LexPunctuation
};

struct LexToken {
    uint32_t offset;
    uint32_t length;
    uint32_t line;
    uint32_t column;
    uint16_t kind;
    uint16_t braceDepth;
};

struct ScopeFrame {
    SymbolKind kind;
    char name[kSymbolMaxContainerBytes];
};

struct ParseScope {
    ScopeFrame frames[kMaxScopeDepth];
    uint32_t count;
};

static LexToken g_tokens[kMaxTokens];
static uint8_t g_braceKinds[kMaxTokens];
static char g_projectReadBuffer[kMaxEditorBytes + 1];
static const uint8_t kBraceNone = 0;
static const uint8_t kBraceNamespace = 1;
static const uint8_t kBraceClass = 2;
static const uint8_t kBraceStruct = 3;
static const uint8_t kBraceEnum = 4;
static const uint8_t kBraceUnion = 5;
static const uint8_t kBraceFunction = 6;

static uint32_t textLength(const char* text, uint32_t limit) {
    if (!text) return 0;
    uint32_t length = 0;
    while (length < limit && text[length] != '\0') ++length;
    return length;
}

static char lowerAscii(char value) {
    return value >= 'A' && value <= 'Z' ? static_cast<char>(value + ('a' - 'A')) : value;
}

static bool equalText(const char* left, const char* right, bool foldCase) {
    if (!left || !right) return left == right;
    uint32_t i = 0;
    while (left[i] != '\0' && right[i] != '\0') {
        char a = left[i];
        char b = right[i];
        if (foldCase) { a = lowerAscii(a); b = lowerAscii(b); }
        if (a != b) return false;
        ++i;
    }
    return left[i] == right[i];
}

static bool startsWith(const char* text, const char* prefix, bool foldCase) {
    if (!text || !prefix) return false;
    uint32_t i = 0;
    while (prefix[i] != '\0') {
        if (text[i] == '\0') return false;
        char a = text[i];
        char b = prefix[i];
        if (foldCase) { a = lowerAscii(a); b = lowerAscii(b); }
        if (a != b) return false;
        ++i;
    }
    return true;
}

static bool containsText(const char* text, const char* query, bool foldCase) {
    if (!text || !query) return false;
    const uint32_t textSize = textLength(text, kSymbolMaxContainerBytes + kSymbolMaxNameBytes);
    const uint32_t querySize = textLength(query, kSymbolMaxQueryBytes + 1);
    if (querySize == 0) return true;
    if (querySize > textSize) return false;
    for (uint32_t start = 0; start + querySize <= textSize; ++start) {
        bool matched = true;
        for (uint32_t i = 0; i < querySize; ++i) {
            char a = text[start + i];
            char b = query[i];
            if (foldCase) { a = lowerAscii(a); b = lowerAscii(b); }
            if (a != b) { matched = false; break; }
        }
        if (matched) return true;
    }
    return false;
}

static void clearBytes(char* output, uint32_t capacity) {
    if (!output) return;
    for (uint32_t i = 0; i < capacity; ++i) output[i] = '\0';
}

static void copyText(char* output, uint32_t capacity, const char* input) {
    if (!output || capacity == 0) return;
    uint32_t i = 0;
    if (input) while (i + 1 < capacity && input[i] != '\0') { output[i] = input[i]; ++i; }
    output[i] = '\0';
}

static void copyRange(char* output, uint32_t capacity, const char* input,
                      uint32_t offset, uint32_t length) {
    if (!output || capacity == 0) return;
    uint32_t i = 0;
    while (i + 1 < capacity && i < length && input && input[offset + i] != '\0') {
        output[i] = input[offset + i];
        ++i;
    }
    output[i] = '\0';
}

static bool isIdentifierStart(char value) {
    return value == '_' || (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z');
}

static bool isIdentifierPart(char value) {
    return isIdentifierStart(value) || (value >= '0' && value <= '9');
}

static bool tokenIs(const char* text, const LexToken& token, const char* value) {
    const uint32_t length = textLength(value, 64);
    if (token.length != length) return false;
    for (uint32_t i = 0; i < length; ++i) if (text[token.offset + i] != value[i]) return false;
    return true;
}

static bool tokenIsIdentifier(const LexToken& token) {
    return token.kind == LexIdentifier;
}

static bool tokenIsPunctuation(const char* text, const LexToken& token, const char* value) {
    return token.kind == LexPunctuation && tokenIs(text, token, value);
}

static bool appendChar(char* output, uint32_t capacity, uint32_t& length, char value) {
    if (!output || length + 1 >= capacity) return false;
    output[length++] = value;
    output[length] = '\0';
    return true;
}

static bool appendRange(char* output, uint32_t capacity, uint32_t& length,
                        const char* text, const LexToken& token) {
    for (uint32_t i = 0; i < token.length; ++i)
        if (!appendChar(output, capacity, length, text[token.offset + i])) return false;
    return true;
}

static bool appendTextRange(char* output, uint32_t capacity, uint32_t& length,
                            const char* text, uint32_t offset, uint32_t count) {
    for (uint32_t i = 0; i < count; ++i)
        if (!appendChar(output, capacity, length, text[offset + i])) return false;
    return true;
}

static bool isConditionalWord(const char* text, uint32_t offset, uint32_t end,
                              const char* word, uint32_t* after) {
    uint32_t cursor = offset;
    while (cursor < end && (text[cursor] == ' ' || text[cursor] == '\t')) ++cursor;
    const uint32_t length = textLength(word, 32);
    for (uint32_t i = 0; i < length; ++i)
        if (cursor + i >= end || text[cursor + i] != word[i]) return false;
    if (cursor + length < end && isIdentifierPart(text[cursor + length])) return false;
    if (after) *after = cursor + length;
    return true;
}

static bool directiveDisables(const char* text, uint32_t offset, uint32_t end) {
    uint32_t cursor = offset;
    while (cursor < end && (text[cursor] == ' ' || text[cursor] == '\t')) ++cursor;
    return cursor < end && text[cursor] == '0' &&
        (cursor + 1 == end || !isIdentifierPart(text[cursor + 1]));
}

static uint32_t tokenize(const char* text, uint32_t length, bool* truncated) {
    uint32_t tokenCount = 0;
    uint32_t line = 1;
    uint32_t position = 0;
    uint32_t braceDepth = 0;
    bool blockComment = false;
    bool conditional[kMaxConditionalDepth] = {};
    uint32_t conditionalDepth = 0;
    if (truncated) *truncated = false;
    while (position <= length) {
        uint32_t lineEnd = position;
        while (lineEnd < length && text[lineEnd] != '\n') ++lineEnd;
        uint32_t first = position;
        while (first < lineEnd && (text[first] == ' ' || text[first] == '\t' || text[first] == '\r')) ++first;
        bool directive = !blockComment && first < lineEnd && text[first] == '#';
        if (directive) {
            uint32_t word = first + 1;
            while (word < lineEnd && (text[word] == ' ' || text[word] == '\t')) ++word;
            uint32_t after = word;
            if (isConditionalWord(text, word, lineEnd, "if", &after)) {
                const bool parentDisabled = conditionalDepth > 0 && conditional[conditionalDepth - 1];
                const bool zero = directiveDisables(text, after, lineEnd);
                if (conditionalDepth < kMaxConditionalDepth) conditional[conditionalDepth++] = parentDisabled || zero;
            } else if (isConditionalWord(text, word, lineEnd, "ifdef", &after) ||
                       isConditionalWord(text, word, lineEnd, "ifndef", &after)) {
                const bool parentDisabled = conditionalDepth > 0 && conditional[conditionalDepth - 1];
                if (conditionalDepth < kMaxConditionalDepth) conditional[conditionalDepth++] = parentDisabled;
            } else if (isConditionalWord(text, word, lineEnd, "else", &after) ||
                       isConditionalWord(text, word, lineEnd, "elif", &after)) {
                if (conditionalDepth > 0) {
                    const bool parentDisabled = conditionalDepth > 1 && conditional[conditionalDepth - 2];
                    conditional[conditionalDepth - 1] = parentDisabled;
                }
            } else if (isConditionalWord(text, word, lineEnd, "endif", &after)) {
                if (conditionalDepth > 0) --conditionalDepth;
            }
        } else if (conditionalDepth == 0 || !conditional[conditionalDepth - 1]) {
            uint32_t cursor = position;
            while (cursor < lineEnd) {
                const char value = text[cursor];
                if (value == ' ' || value == '\t' || value == '\r') { ++cursor; continue; }
                if (blockComment) {
                    if (cursor + 1 < lineEnd && text[cursor] == '*' && text[cursor + 1] == '/') {
                        blockComment = false; cursor += 2;
                    } else ++cursor;
                    continue;
                }
                if (cursor + 1 < lineEnd && text[cursor] == '/' && text[cursor + 1] == '/') break;
                if (cursor + 1 < lineEnd && text[cursor] == '/' && text[cursor + 1] == '*') {
                    blockComment = true; cursor += 2; continue;
                }
                if (value == '"' || value == '\'') {
                    const char quote = value;
                    ++cursor;
                    while (cursor < lineEnd) {
                        if (text[cursor] == '\\' && cursor + 1 < lineEnd) { cursor += 2; continue; }
                        if (text[cursor] == quote) { ++cursor; break; }
                        ++cursor;
                    }
                    continue;
                }
                if (isIdentifierStart(value)) {
                    const uint32_t start = cursor++;
                    while (cursor < lineEnd && isIdentifierPart(text[cursor])) ++cursor;
                    if (tokenCount < kMaxTokens) {
                        g_tokens[tokenCount++] = { start, cursor - start, line, start - position, LexIdentifier,
                                                   static_cast<uint16_t>(braceDepth > 65535u ? 65535u : braceDepth) };
                    } else if (truncated) *truncated = true;
                    continue;
                }
                if (value >= '0' && value <= '9') {
                    const uint32_t start = cursor++;
                    while (cursor < lineEnd && (isIdentifierPart(text[cursor]) || text[cursor] == '.')) ++cursor;
                    if (tokenCount < kMaxTokens) {
                        g_tokens[tokenCount++] = { start, cursor - start, line, start - position, LexNumber,
                                                   static_cast<uint16_t>(braceDepth > 65535u ? 65535u : braceDepth) };
                    } else if (truncated) *truncated = true;
                    continue;
                }
                uint32_t punctuationLength = 1;
                if (value == ':' && cursor + 1 < lineEnd && text[cursor + 1] == ':') punctuationLength = 2;
                else if (value == '-' && cursor + 1 < lineEnd && text[cursor + 1] == '>') punctuationLength = 2;
                else if (value == '.' && cursor + 2 < lineEnd && text[cursor + 1] == '.' && text[cursor + 2] == '.') punctuationLength = 3;
                if (tokenCount < kMaxTokens) {
                    g_tokens[tokenCount++] = { cursor, punctuationLength, line, cursor - position, LexPunctuation,
                                               static_cast<uint16_t>(braceDepth > 65535u ? 65535u : braceDepth) };
                } else if (truncated) *truncated = true;
                if (value == '{') ++braceDepth;
                else if (value == '}' && braceDepth > 0) --braceDepth;
                cursor += punctuationLength;
            }
        }
        if (lineEnd >= length) break;
        position = lineEnd + 1;
        ++line;
    }
    return tokenCount;
}

static bool kindIsType(SymbolKind kind) {
    return kind == SymbolKind::Class || kind == SymbolKind::Struct ||
        kind == SymbolKind::Enum || kind == SymbolKind::Union;
}

static uint8_t braceCode(SymbolKind kind) {
    switch (kind) {
    case SymbolKind::Namespace: return kBraceNamespace;
    case SymbolKind::Class: return kBraceClass;
    case SymbolKind::Struct: return kBraceStruct;
    case SymbolKind::Enum: return kBraceEnum;
    case SymbolKind::Union: return kBraceUnion;
    default: return kBraceNone;
    }
}

static SymbolKind kindForBrace(uint8_t code) {
    switch (code) {
    case kBraceNamespace: return SymbolKind::Namespace;
    case kBraceClass: return SymbolKind::Class;
    case kBraceStruct: return SymbolKind::Struct;
    case kBraceEnum: return SymbolKind::Enum;
    case kBraceUnion: return SymbolKind::Union;
    default: return SymbolKind::Namespace;
    }
}

static void scopeContainer(const ParseScope& scope, char* output, uint32_t capacity, uint16_t* depth) {
    if (output && capacity > 0) output[0] = '\0';
    uint32_t length = 0;
    uint16_t named = 0;
    for (uint32_t i = 0; i < scope.count; ++i) {
        const ScopeFrame& frame = scope.frames[i];
        if (frame.kind == SymbolKind::Function) continue;
        if (frame.name[0] == '\0') continue;
        if (length > 0) appendTextRange(output, capacity, length, "::", 0, 2);
        appendTextRange(output, capacity, length, frame.name, 0, textLength(frame.name, sizeof(frame.name)));
        ++named;
    }
    if (depth) *depth = named;
}

static bool hasFunctionScope(const ParseScope& scope) {
    for (uint32_t i = 0; i < scope.count; ++i) if (scope.frames[i].kind == SymbolKind::Function) return true;
    return false;
}

static bool namespaceScopeOnly(const ParseScope& scope) {
    for (uint32_t i = 0; i < scope.count; ++i)
        if (scope.frames[i].kind != SymbolKind::Namespace) return false;
    return true;
}

static const char* nearestClassName(const ParseScope& scope) {
    for (uint32_t i = scope.count; i > 0; --i) {
        const ScopeFrame& frame = scope.frames[i - 1];
        if (kindIsType(frame.kind)) return frame.name;
    }
    return "";
}

static void pushScope(ParseScope& scope, SymbolKind kind, const char* name) {
    if (scope.count >= kMaxScopeDepth) return;
    ScopeFrame& frame = scope.frames[scope.count++];
    frame.kind = kind;
    copyText(frame.name, sizeof(frame.name), name);
}

static void popScope(ParseScope& scope) {
    if (scope.count > 0) --scope.count;
}

static uint32_t findTypeKeyword(const char* text, uint32_t braceIndex, SymbolKind kind) {
    uint32_t scanned = 0;
    for (uint32_t i = braceIndex; i > 0 && scanned < 256; ++scanned) {
        --i;
        if (tokenIsPunctuation(text, g_tokens[i], ";") || tokenIsPunctuation(text, g_tokens[i], "}")) break;
        if ((kind == SymbolKind::Namespace && tokenIs(text, g_tokens[i], "namespace")) ||
            (kind == SymbolKind::Class && tokenIs(text, g_tokens[i], "class")) ||
            (kind == SymbolKind::Struct && tokenIs(text, g_tokens[i], "struct")) ||
            (kind == SymbolKind::Enum && tokenIs(text, g_tokens[i], "enum")) ||
            (kind == SymbolKind::Union && tokenIs(text, g_tokens[i], "union"))) return i;
    }
    return braceIndex;
}

static void typeNameForBrace(const char* text, uint32_t braceIndex, SymbolKind kind,
                             char* output, uint32_t capacity) {
    if (!output || capacity == 0) return;
    output[0] = '\0';
    const uint32_t keyword = findTypeKeyword(text, braceIndex, kind);
    uint32_t i = keyword + 1;
    if (kind == SymbolKind::Enum && i < braceIndex &&
        (tokenIs(text, g_tokens[i], "class") || tokenIs(text, g_tokens[i], "struct"))) ++i;
    uint32_t length = 0;
    while (i < braceIndex) {
        if (tokenIsIdentifier(g_tokens[i])) {
            if (length > 0 && kind != SymbolKind::Namespace) break;
            if (length > 0) appendTextRange(output, capacity, length, "::", 0, 2);
            appendRange(output, capacity, length, text, g_tokens[i]);
        }
        ++i;
    }
}

static uint32_t findTypeNameStart(const char* text, uint32_t keyword, SymbolKind kind, uint32_t tokenCount,
                                  uint32_t* endIndex, uint32_t* braceIndex, char* joined, uint32_t joinedCapacity) {
    uint32_t i = keyword + 1;
    if (kind == SymbolKind::Enum && i < tokenCount &&
        (tokenIs(text, g_tokens[i], "class") || tokenIs(text, g_tokens[i], "struct"))) ++i;
    uint32_t firstName = tokenCount;
    uint32_t length = 0;
    if (joined && joinedCapacity > 0) joined[0] = '\0';
    while (i < tokenCount) {
        if (tokenIsPunctuation(text, g_tokens[i], "{")) {
            if (braceIndex) *braceIndex = i;
            if (endIndex) *endIndex = i;
            return firstName;
        }
        if (tokenIsPunctuation(text, g_tokens[i], ";")) {
            if (endIndex) *endIndex = i;
            if (braceIndex) *braceIndex = tokenCount;
            return firstName;
        }
        if (tokenIsIdentifier(g_tokens[i])) {
            if (firstName == tokenCount) firstName = i;
            if (joined) {
                if (length > 0) appendTextRange(joined, joinedCapacity, length, "::", 0, 2);
                appendRange(joined, joinedCapacity, length, text, g_tokens[i]);
            }
        }
        ++i;
    }
    if (endIndex) *endIndex = tokenCount;
    if (braceIndex) *braceIndex = tokenCount;
    return firstName;
}

static bool addSymbol(DocumentSymbol* output, uint32_t capacity, uint32_t& count,
                      SymbolKind kind, const char* text, const LexToken& nameToken,
                      const char* explicitName, const ParseScope& scope,
                      uint64_t documentId, uint32_t generation, uint16_t flags = 0) {
    if (!output || count >= capacity) return false;
    DocumentSymbol& symbol = output[count];
    symbol.kind = kind;
    symbol.ordinal = count;
    symbol.flags = flags;
    scopeContainer(scope, symbol.container, sizeof(symbol.container), &symbol.depth);
    if (explicitName) copyText(symbol.name, sizeof(symbol.name), explicitName);
    else copyRange(symbol.name, sizeof(symbol.name), text, nameToken.offset, nameToken.length);
    symbol.location.documentId = documentId;
    symbol.location.generation = generation;
    symbol.location.line = nameToken.line;
    symbol.location.column = nameToken.column + 1;
    symbol.location.identifierOffset = nameToken.offset;
    symbol.location.identifierLength = nameToken.length;
    if (explicitName) {
        const uint32_t explicitLength = textLength(explicitName, sizeof(symbol.name));
        symbol.location.identifierLength = explicitLength;
    }
    ++count;
    return true;
}

static bool isControlName(const char* text, const LexToken& token) {
    return tokenIs(text, token, "if") || tokenIs(text, token, "for") || tokenIs(text, token, "while") ||
        tokenIs(text, token, "switch") || tokenIs(text, token, "catch") || tokenIs(text, token, "sizeof") ||
        tokenIs(text, token, "return") || tokenIs(text, token, "decltype");
}

static bool functionCandidate(const char* text, uint32_t tokenCount, uint32_t openIndex,
                              uint32_t* nameIndex, uint32_t* bodyBrace, bool* destructor,
                              bool* prefixFound) {
    if (openIndex == 0 || !tokenIsPunctuation(text, g_tokens[openIndex], "(")) return false;
    uint32_t depth = 0;
    uint32_t closeIndex = tokenCount;
    for (uint32_t i = openIndex; i < tokenCount; ++i) {
        if (tokenIsPunctuation(text, g_tokens[i], "(")) ++depth;
        else if (tokenIsPunctuation(text, g_tokens[i], ")")) {
            if (depth == 0) return false;
            --depth;
            if (depth == 0) { closeIndex = i; break; }
        }
    }
    if (closeIndex == tokenCount) return false;
    uint32_t candidate = openIndex - 1;
    bool isDestructor = false;
    if (candidate > 0 && tokenIsPunctuation(text, g_tokens[candidate - 1], "~")) {
        if (!tokenIsIdentifier(g_tokens[candidate])) return false;
        isDestructor = true;
    }
    if (!tokenIsIdentifier(g_tokens[candidate]) || isControlName(text, g_tokens[candidate])) return false;
    if (candidate > 0 && (tokenIsPunctuation(text, g_tokens[candidate - 1], ".") ||
                          tokenIsPunctuation(text, g_tokens[candidate - 1], "->"))) return false;
    bool qualified = candidate > 0 && tokenIsPunctuation(text, g_tokens[candidate - 1], "::");
    bool prefixIdentifier = qualified;
    uint32_t boundary = candidate;
    for (uint32_t i = candidate; i > 0 && i + 96 > candidate; ) {
        --i;
        if (tokenIsPunctuation(text, g_tokens[i], ";") || tokenIsPunctuation(text, g_tokens[i], "{") ||
            tokenIsPunctuation(text, g_tokens[i], "}") || tokenIsPunctuation(text, g_tokens[i], "=") ||
            tokenIsPunctuation(text, g_tokens[i], ",") || tokenIsPunctuation(text, g_tokens[i], "(") ||
            tokenIsPunctuation(text, g_tokens[i], "[") || tokenIsPunctuation(text, g_tokens[i], "?") ||
            tokenIsPunctuation(text, g_tokens[i], ":") || tokenIs(text, g_tokens[i], "return") ||
            tokenIs(text, g_tokens[i], "throw") || tokenIs(text, g_tokens[i], "co_await") ||
            tokenIs(text, g_tokens[i], "co_return") || tokenIs(text, g_tokens[i], "new") ||
            tokenIs(text, g_tokens[i], "delete")) break;
        if (tokenIsIdentifier(g_tokens[i]) && !tokenIs(text, g_tokens[i], "template")) prefixIdentifier = true;
        boundary = i;
    }
    (void)boundary;
    if (prefixFound) *prefixFound = prefixIdentifier;
    uint32_t cursor = closeIndex + 1;
    uint32_t parenDepth = 0;
    uint32_t brace = tokenCount;
    for (; cursor < tokenCount; ++cursor) {
        if (tokenIsPunctuation(text, g_tokens[cursor], "(")) ++parenDepth;
        else if (tokenIsPunctuation(text, g_tokens[cursor], ")") && parenDepth > 0) --parenDepth;
        else if (parenDepth == 0 && tokenIsPunctuation(text, g_tokens[cursor], "{")) { brace = cursor; break; }
        else if (parenDepth == 0 && tokenIsPunctuation(text, g_tokens[cursor], ";")) break;
    }
    if (nameIndex) *nameIndex = candidate;
    if (bodyBrace) *bodyBrace = brace;
    if (destructor) *destructor = isDestructor;
    return brace != tokenCount || (cursor < tokenCount && tokenIsPunctuation(text, g_tokens[cursor], ";"));
}

static bool symbolBefore(const DocumentSymbol& left, const DocumentSymbol& right, const char* text) {
    if (left.location.identifierOffset != right.location.identifierOffset)
        return left.location.identifierOffset < right.location.identifierOffset;
    if (left.kind != right.kind) return static_cast<int>(left.kind) < static_cast<int>(right.kind);
    uint32_t i = 0;
    while (left.name[i] != '\0' && right.name[i] != '\0') {
        if (left.name[i] != right.name[i]) return left.name[i] < right.name[i];
        ++i;
    }
    (void)text;
    return left.name[i] < right.name[i];
}

static void sortSymbols(DocumentSymbol* symbols, uint32_t count, const char* text) {
    if (!symbols) return;
    for (uint32_t i = 1; i < count; ++i) {
        DocumentSymbol value = symbols[i];
        uint32_t j = i;
        while (j > 0 && symbolBefore(value, symbols[j - 1], text)) {
            symbols[j] = symbols[j - 1];
            --j;
        }
        symbols[j] = value;
    }
    for (uint32_t i = 0; i < count; ++i) symbols[i].ordinal = i;
}

static void updateScanResult(SymbolScanResult* result, bool success, bool truncated,
                             uint32_t symbols, uint32_t tokens) {
    if (!result) return;
    result->success = success;
    result->truncated = truncated;
    result->symbolCount = symbols;
    result->tokenCount = tokens;
}

static uint64_t hashPath(const char* path) {
    uint64_t hash = 1469598103934665603ull;
    if (!path) return hash;
    for (uint32_t i = 0; path[i] != '\0'; ++i) {
        char value = lowerAscii(path[i]);
        hash ^= static_cast<uint8_t>(value);
        hash *= 1099511628211ull;
    }
    return hash == 0 ? 1 : hash;
}

static bool pathEqual(const char* left, const char* right) {
    return PathsEqual(left, right);
}

static bool pathBefore(const char* left, const char* right) {
    uint32_t i = 0;
    while (left && right && left[i] != '\0' && right[i] != '\0') {
        char a = lowerAscii(left[i]);
        char b = lowerAscii(right[i]);
        if (a != b) return a < b;
        ++i;
    }
    return (left ? left[i] : '\0') < (right ? right[i] : '\0');
}

static void removeProjectSymbols(SymbolDatabase* database, uint32_t documentIndex) {
    if (!database || documentIndex >= database->documentCount) return;
    SymbolDocument& document = database->documents[documentIndex];
    const uint32_t start = document.symbolStart;
    const uint32_t count = document.symbolCount;
    if (count > 0 && start + count <= database->projectSymbolCount) {
        for (uint32_t i = start; i + count < database->projectSymbolCount; ++i)
            database->projectSymbols[i] = database->projectSymbols[i + count];
        database->projectSymbolCount -= count;
        for (uint32_t i = 0; i < database->documentCount; ++i) {
            if (i == documentIndex) continue;
            if (database->documents[i].symbolStart > start)
                database->documents[i].symbolStart -= count;
        }
        for (uint32_t i = 0; i < database->projectSymbolCount; ++i) {
            if (database->projectSymbols[i].documentIndex == documentIndex) {
                // This is only possible for malformed storage; the removed
                // block should have contained all entries for this document.
                database->projectSymbols[i].documentIndex = documentIndex;
            }
        }
    }
    document.symbolStart = 0;
    document.symbolCount = 0;
}

static int32_t findDocumentNormalized(const SymbolDatabase* database, const char* normalizedPath) {
    if (!database || !normalizedPath) return -1;
    for (uint32_t i = 0; i < database->documentCount; ++i)
        if (database->documents[i].used && pathEqual(database->documents[i].path, normalizedPath)) return static_cast<int32_t>(i);
    return -1;
}

static int32_t ensureDocument(SymbolDatabase* database, const char* normalizedPath,
                              uint64_t documentId, uint32_t generation, bool dirty) {
    int32_t existing = findDocumentNormalized(database, normalizedPath);
    if (existing >= 0) {
        SymbolDocument& document = database->documents[existing];
        document.documentId = documentId;
        document.generation = generation;
        document.dirty = dirty;
        return existing;
    }
    if (!database || database->documentCount >= database->documentCapacity) {
        if (database) { database->truncated = true; ++database->droppedDocuments; }
        return -1;
    }
    uint32_t insertion = database->documentCount;
    while (insertion > 0 && pathBefore(normalizedPath, database->documents[insertion - 1].path)) --insertion;
    for (uint32_t i = database->documentCount; i > insertion; --i) {
        database->documents[i] = database->documents[i - 1];
        for (uint32_t symbol = 0; symbol < database->projectSymbolCount; ++symbol)
            if (database->projectSymbols[symbol].documentIndex == i - 1)
                database->projectSymbols[symbol].documentIndex = i;
            else if (database->projectSymbols[symbol].documentIndex >= insertion &&
                     database->projectSymbols[symbol].documentIndex < i - 1)
                ++database->projectSymbols[symbol].documentIndex;
    }
    SymbolDocument& document = database->documents[insertion];
    clearBytes(reinterpret_cast<char*>(&document), sizeof(document));
    document.used = true;
    document.documentId = documentId;
    document.generation = generation;
    document.dirty = dirty;
    copyText(document.path, sizeof(document.path), normalizedPath);
    document.symbolStart = 0;
    document.symbolCount = 0;
    ++database->documentCount;
    return static_cast<int32_t>(insertion);
}

static bool replaceDocumentSymbols(SymbolDatabase* database, uint32_t documentIndex,
                                   const DocumentSymbol* symbols, uint32_t symbolCount,
                                   bool scanTruncated) {
    if (!database || documentIndex >= database->documentCount || !database->projectSymbols) return false;
    removeProjectSymbols(database, documentIndex);
    SymbolDocument& document = database->documents[documentIndex];
    uint32_t insertion = 0;
    for (uint32_t i = 0; i < documentIndex; ++i) insertion += database->documents[i].symbolCount;
    uint32_t available = database->projectCapacity - database->projectSymbolCount;
    uint32_t accepted = symbolCount < available ? symbolCount : available;
    if (accepted < symbolCount || scanTruncated) {
        database->truncated = true;
        database->droppedSymbols += symbolCount - accepted;
    }
    for (uint32_t i = database->projectSymbolCount; i > insertion; --i)
        database->projectSymbols[i + accepted - 1] = database->projectSymbols[i - 1];
    for (uint32_t i = 0; i < database->documentCount; ++i)
        if (i != documentIndex && database->documents[i].symbolStart >= insertion)
            database->documents[i].symbolStart += accepted;
    for (uint32_t i = 0; i < accepted; ++i) {
        database->projectSymbols[insertion + i].documentIndex = documentIndex;
        database->projectSymbols[insertion + i].symbol = symbols[i];
    }
    database->projectSymbolCount += accepted;
    document.symbolStart = insertion;
    document.symbolCount = accepted;
    database->lastIndexedDocumentId = document.documentId;
    database->lastIndexedSymbolCount = accepted;
    return true;
}

static bool isExcludedDirectory(const char* name) {
    return equalText(name, ".git", true) || equalText(name, ".vs", true) || equalText(name, "build", true) ||
        equalText(name, "build-native", true) || equalText(name, "tmp", true) || equalText(name, "out", true) ||
        equalText(name, "obj", true) || equalText(name, "debug", true) || equalText(name, "release", true) ||
        equalText(name, "node_modules", true);
}

static bool entryBefore(const FileListEntry& left, const FileListEntry& right) {
    if (left.kind != right.kind) return left.kind == FileInfoKind::Directory;
    return pathBefore(left.name, right.name);
}

static void sortEntries(FileListEntry* entries, uint32_t count) {
    for (uint32_t i = 1; i < count; ++i) {
        FileListEntry value = entries[i];
        uint32_t j = i;
        while (j > 0 && entryBefore(value, entries[j - 1])) { entries[j] = entries[j - 1]; --j; }
        entries[j] = value;
    }
}

static bool dirtyPath(const Document* documents, uint32_t count,
                      const char* path, const Document** outDocument) {
    if (outDocument) *outDocument = nullptr;
    for (uint32_t i = 0; i < count; ++i) {
        if (!documents[i].used || !documents[i].buffer.dirty) continue;
        if (pathEqual(documents[i].path, path)) {
            if (outDocument) *outDocument = &documents[i];
            return true;
        }
    }
    return false;
}

static bool indexDirectory(SymbolDatabase* database, const WorkspaceFileSystem& fileSystem,
                           const char* directory, uint32_t depth,
                           const Document* dirtyDocuments, uint32_t dirtyCount,
                           uint64_t projectGeneration) {
    if (depth > kMaxProjectDepth) { database->truncated = true; return false; }
    FileListEntry entries[kMaxWorkspaceEntries];
    uint32_t count = 0;
    bool truncated = false;
    if (!fileSystem.list || !fileSystem.list(fileSystem.userData, directory, entries,
                                             kMaxWorkspaceEntries, &count, &truncated)) {
        database->truncated = true;
        return false;
    }
    if (truncated) database->truncated = true;
    if (count > kMaxWorkspaceEntries) count = kMaxWorkspaceEntries;
    sortEntries(entries, count);
    for (uint32_t i = 0; i < count; ++i) {
        if (entries[i].name[0] == '\0' || PathContainsTraversal(entries[i].name)) continue;
        char path[kMaxPathBytes] = {};
        if (!JoinWorkspacePath(directory, entries[i].name, path, sizeof(path))) { database->truncated = true; continue; }
        if (entries[i].kind == FileInfoKind::Directory) {
            if (!isExcludedDirectory(entries[i].name)) indexDirectory(database, fileSystem, path, depth + 1,
                                                                       dirtyDocuments, dirtyCount, projectGeneration);
            continue;
        }
        if (entries[i].kind != FileInfoKind::RegularFile || !IsSymbolSourcePath(path)) continue;
        const Document* dirty = nullptr;
        if (dirtyPath(dirtyDocuments, dirtyCount, path, &dirty)) continue;
        if (entries[i].size > kMaxEditorBytes || !fileSystem.read) { database->truncated = true; continue; }
        uint32_t bytes = 0;
        if (!fileSystem.read(fileSystem.userData, path, g_projectReadBuffer, kMaxEditorBytes, &bytes) || bytes > kMaxEditorBytes) {
            database->truncated = true;
            continue;
        }
        SymbolDatabaseIndexDocument(database, path, hashPath(path), static_cast<uint32_t>(projectGeneration), false,
                                    g_projectReadBuffer, bytes);
    }
    return true;
}

static uint32_t symbolDisplayLength(const ProjectSymbol& symbol, char* output, uint32_t capacity) {
    if (!output || capacity == 0) return 0;
    uint32_t length = 0;
    if (symbol.symbol.container[0] != '\0') {
        appendTextRange(output, capacity, length, symbol.symbol.container, 0,
                        textLength(symbol.symbol.container, sizeof(symbol.symbol.container)));
        appendTextRange(output, capacity, length, "::", 0, 2);
    }
    appendTextRange(output, capacity, length, symbol.symbol.name, 0,
                    textLength(symbol.symbol.name, sizeof(symbol.symbol.name)));
    return length;
}

static bool queryValid(const char* query) {
    return query && textLength(query, kSymbolMaxQueryBytes + 1) <= kSymbolMaxQueryBytes;
}

} // namespace

const char* SymbolKindName(SymbolKind kind) {
    switch (kind) {
    case SymbolKind::Namespace: return "Namespace";
    case SymbolKind::Class: return "Class";
    case SymbolKind::Struct: return "Struct";
    case SymbolKind::Enum: return "Enum";
    case SymbolKind::Function: return "Function";
    case SymbolKind::Method: return "Method";
    case SymbolKind::Constructor: return "Constructor";
    case SymbolKind::Destructor: return "Destructor";
    case SymbolKind::GlobalVariable: return "Global Variable";
    case SymbolKind::StaticVariable: return "Static Variable";
    case SymbolKind::Typedef: return "Typedef";
    case SymbolKind::UsingAlias: return "Using Alias";
    case SymbolKind::Union: return "Union";
    case SymbolKind::Macro: return "Macro";
    default: return "Unknown";
    }
}

const char* SymbolKindPrefix(SymbolKind kind) {
    switch (kind) {
    case SymbolKind::Namespace: return "[n]";
    case SymbolKind::Class: return "[c]";
    case SymbolKind::Struct: return "[s]";
    case SymbolKind::Enum: return "[e]";
    case SymbolKind::Function: return "[f]";
    case SymbolKind::Method: return "[m]";
    case SymbolKind::Constructor: return "[C]";
    case SymbolKind::Destructor: return "[D]";
    case SymbolKind::GlobalVariable: return "[g]";
    case SymbolKind::StaticVariable: return "[S]";
    case SymbolKind::Typedef: return "[t]";
    case SymbolKind::UsingAlias: return "[u]";
    case SymbolKind::Union: return "[U]";
    case SymbolKind::Macro: return "[#]";
    default: return "[?]";
    }
}

bool IsSymbolSourcePath(const char* path) {
    if (!path) return false;
    const char* name = BaseName(path);
    uint32_t length = textLength(name, kMaxNameBytes);
    uint32_t dot = length;
    for (uint32_t i = 0; i < length; ++i) if (name[i] == '.') dot = i;
    if (dot == length) return false;
    const char* extension = name + dot;
    static const char* const extensions[] = { ".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx" };
    for (uint32_t i = 0; i < sizeof(extensions) / sizeof(extensions[0]); ++i)
        if (equalText(extension, extensions[i], true)) return true;
    return false;
}

bool ScanDocumentSymbols(const char* text, uint32_t length,
                         uint64_t documentId, uint32_t generation,
                         DocumentSymbol* output, uint32_t capacity,
                         SymbolScanResult* result) {
    if (result) updateScanResult(result, false, false, 0, 0);
    if (!text || !output || capacity == 0 || length > kMaxEditorBytes) return false;
    clearBytes(reinterpret_cast<char*>(g_braceKinds), sizeof(g_braceKinds));
    bool tokenTruncated = false;
    const uint32_t tokenCount = tokenize(text, length, &tokenTruncated);
    uint32_t symbolCount = 0;
    ParseScope scope = {};

    // Type declarations are recognized while walking braces so nested
    // namespace/class containers remain lexical and deterministic.
    for (uint32_t i = 0; i < tokenCount; ++i) {
        if (tokenIsPunctuation(text, g_tokens[i], "}")) { popScope(scope); continue; }
        if (tokenIsPunctuation(text, g_tokens[i], "{")) {
            const uint8_t code = g_braceKinds[i];
            if (code != kBraceNone) {
                char name[kSymbolMaxContainerBytes] = {};
                typeNameForBrace(text, i, kindForBrace(code), name, sizeof(name));
                pushScope(scope, kindForBrace(code), name);
            } else pushScope(scope, SymbolKind::Function, "");
            continue;
        }
        SymbolKind kind = SymbolKind::Namespace;
        bool isType = false;
        if (tokenIs(text, g_tokens[i], "namespace")) { kind = SymbolKind::Namespace; isType = true; }
        else if (tokenIs(text, g_tokens[i], "class")) {
            if (i > 0 && tokenIs(text, g_tokens[i - 1], "enum")) continue;
            kind = SymbolKind::Class; isType = true;
        }
        else if (tokenIs(text, g_tokens[i], "struct")) { kind = SymbolKind::Struct; isType = true; }
        else if (tokenIs(text, g_tokens[i], "enum")) { kind = SymbolKind::Enum; isType = true; }
        else if (tokenIs(text, g_tokens[i], "union")) { kind = SymbolKind::Union; isType = true; }
        if (!isType) continue;
        uint32_t end = tokenCount;
        uint32_t brace = tokenCount;
        char joined[kSymbolMaxContainerBytes] = {};
        const uint32_t firstName = findTypeNameStart(text, i, kind, tokenCount, &end, &brace, joined, sizeof(joined));
        if (firstName == tokenCount) {
            if (kind == SymbolKind::Namespace && brace != tokenCount) {
                LexToken anonymous = g_tokens[i];
                if (!addSymbol(output, capacity, symbolCount, kind, text, anonymous, "anonymous", scope,
                               documentId, generation, 1)) tokenTruncated = true;
            }
        } else {
            uint32_t nameIndex = firstName;
            while (nameIndex < end) {
                if (tokenIsIdentifier(g_tokens[nameIndex])) {
                    if (!addSymbol(output, capacity, symbolCount, kind, text, g_tokens[nameIndex], nullptr, scope,
                                   documentId, generation)) tokenTruncated = true;
                }
                ++nameIndex;
                if (kind != SymbolKind::Namespace) break;
                while (nameIndex < end && !tokenIsIdentifier(g_tokens[nameIndex])) ++nameIndex;
            }
        }
        if (brace != tokenCount) g_braceKinds[brace] = braceCode(kind);
    }

    // Functions are identified by a named parameter list followed by a body
    // or declaration terminator. Calls, control flow and lambdas are skipped.
    scope = {};
    for (uint32_t i = 0; i < tokenCount; ++i) {
        if (tokenIsPunctuation(text, g_tokens[i], "}")) { popScope(scope); continue; }
        if (tokenIsPunctuation(text, g_tokens[i], "{")) {
            if (g_braceKinds[i] != kBraceNone) {
                char name[kSymbolMaxContainerBytes] = {};
                typeNameForBrace(text, i, kindForBrace(g_braceKinds[i]), name, sizeof(name));
                pushScope(scope, kindForBrace(g_braceKinds[i]), name);
            } else pushScope(scope, SymbolKind::Function, "");
            continue;
        }
        if (!tokenIsPunctuation(text, g_tokens[i], "(")) continue;
        uint32_t nameIndex = tokenCount;
        uint32_t bodyBrace = tokenCount;
        bool destructor = false;
        bool prefixFound = false;
        if (!functionCandidate(text, tokenCount, i, &nameIndex, &bodyBrace, &destructor, &prefixFound)) continue;
        if (hasFunctionScope(scope)) continue;
        const char* className = nearestClassName(scope);
        char name[kSymbolMaxNameBytes] = {};
        if (destructor) {
            name[0] = '~';
            copyRange(name + 1, sizeof(name) - 1, text, g_tokens[nameIndex].offset, g_tokens[nameIndex].length);
        } else copyRange(name, sizeof(name), text, g_tokens[nameIndex].offset, g_tokens[nameIndex].length);
        if (!prefixFound && !destructor && (className[0] == '\0' || !equalText(className, name, false))) continue;
        SymbolKind kind = SymbolKind::Function;
        if (destructor) kind = SymbolKind::Destructor;
        else if (className[0] != '\0' && equalText(className, name, false)) kind = SymbolKind::Constructor;
        else if (className[0] != '\0' || (nameIndex > 0 && tokenIsPunctuation(text, g_tokens[nameIndex - 1], "::"))) kind = SymbolKind::Method;
        ParseScope qualifiedScope = scope;
        if (nameIndex > 0 && tokenIsPunctuation(text, g_tokens[nameIndex - 1], "::")) {
            char qualifier[kSymbolMaxContainerBytes] = {};
            uint32_t qLength = 0;
            uint32_t firstQualifier = nameIndex - 1;
            while (firstQualifier >= 2 && tokenIsPunctuation(text, g_tokens[firstQualifier - 1], "::") &&
                   tokenIsIdentifier(g_tokens[firstQualifier - 2])) firstQualifier -= 2;
            for (uint32_t q = firstQualifier; q + 1 < nameIndex; ++q) {
                if (!tokenIsIdentifier(g_tokens[q])) continue;
                if (qLength > 0) appendTextRange(qualifier, sizeof(qualifier), qLength, "::", 0, 2);
                appendRange(qualifier, sizeof(qualifier), qLength, text, g_tokens[q]);
            }
            if (qualifier[0] != '\0' && qualifiedScope.count < kMaxScopeDepth)
                pushScope(qualifiedScope, SymbolKind::Class, qualifier);
        }
        if (!addSymbol(output, capacity, symbolCount, kind, text, g_tokens[nameIndex], name, qualifiedScope,
                       documentId, generation, destructor ? 2 : 0)) tokenTruncated = true;
        else if (destructor && nameIndex > 0 && tokenIsPunctuation(text, g_tokens[nameIndex - 1], "~")) {
            DocumentSymbol& symbol = output[symbolCount - 1];
            symbol.location.identifierOffset = g_tokens[nameIndex - 1].offset;
            symbol.location.identifierLength = g_tokens[nameIndex].length + 1;
            symbol.location.column = g_tokens[nameIndex - 1].column + 1;
        }
        if (bodyBrace != tokenCount) g_braceKinds[bodyBrace] = kBraceFunction;
    }

    // Namespace-scope declarations provide the bounded global/static portion
    // of the index. Class fields and locals are intentionally omitted.
    scope = {};
    for (uint32_t i = 0; i < tokenCount; ++i) {
        if (tokenIsPunctuation(text, g_tokens[i], "}")) { popScope(scope); continue; }
        if (tokenIsPunctuation(text, g_tokens[i], "{")) {
            if (g_braceKinds[i] != kBraceNone) {
                char name[kSymbolMaxContainerBytes] = {};
                if (g_braceKinds[i] == kBraceFunction) pushScope(scope, SymbolKind::Function, "");
                else { typeNameForBrace(text, i, kindForBrace(g_braceKinds[i]), name, sizeof(name)); pushScope(scope, kindForBrace(g_braceKinds[i]), name); }
            } else pushScope(scope, SymbolKind::Function, "");
            continue;
        }
        if (!tokenIsPunctuation(text, g_tokens[i], ";") || hasFunctionScope(scope) || !namespaceScopeOnly(scope)) continue;
        bool excluded = false;
        bool isStatic = false;
        bool hasParen = false;
        uint32_t start = i;
        while (start > 0) {
            --start;
            if (tokenIsPunctuation(text, g_tokens[start], ";") || tokenIsPunctuation(text, g_tokens[start], "{") ||
                tokenIsPunctuation(text, g_tokens[start], "}")) { ++start; break; }
        }
        uint32_t lastIdentifier = tokenCount;
        for (uint32_t j = start; j < i; ++j) {
            if (tokenIsPunctuation(text, g_tokens[j], "(")) hasParen = true;
            if (tokenIs(text, g_tokens[j], "typedef") || tokenIs(text, g_tokens[j], "using") ||
                tokenIs(text, g_tokens[j], "class") || tokenIs(text, g_tokens[j], "struct") ||
                tokenIs(text, g_tokens[j], "enum") || tokenIs(text, g_tokens[j], "namespace") ||
                tokenIs(text, g_tokens[j], "union")) excluded = true;
            if (tokenIs(text, g_tokens[j], "static")) isStatic = true;
            if (tokenIsIdentifier(g_tokens[j])) lastIdentifier = j;
        }
        if (excluded || hasParen || lastIdentifier == tokenCount) continue;
        if (!addSymbol(output, capacity, symbolCount, isStatic ? SymbolKind::StaticVariable : SymbolKind::GlobalVariable,
                       text, g_tokens[lastIdentifier], nullptr, scope, documentId, generation)) tokenTruncated = true;
    }

    // Typedefs and using aliases are handled after declarations so their
    // final declarator name is unambiguous in the bounded token stream.
    scope = {};
    for (uint32_t i = 0; i < tokenCount; ++i) {
        if (tokenIsPunctuation(text, g_tokens[i], "}")) { popScope(scope); continue; }
        if (tokenIsPunctuation(text, g_tokens[i], "{")) {
            if (g_braceKinds[i] == kBraceFunction) pushScope(scope, SymbolKind::Function, "");
            else if (g_braceKinds[i] != kBraceNone) {
                char nameScope[kSymbolMaxContainerBytes] = {};
                const SymbolKind scopeKind = kindForBrace(g_braceKinds[i]);
                typeNameForBrace(text, i, scopeKind, nameScope, sizeof(nameScope));
                pushScope(scope, scopeKind, nameScope);
            } else pushScope(scope, SymbolKind::Function, "");
            continue;
        }
        if (tokenIs(text, g_tokens[i], "typedef")) {
            uint32_t last = tokenCount;
            for (uint32_t j = i + 1; j < tokenCount && !tokenIsPunctuation(text, g_tokens[j], ";"); ++j)
                if (tokenIsIdentifier(g_tokens[j])) last = j;
            if (last != tokenCount && !addSymbol(output, capacity, symbolCount, SymbolKind::Typedef, text,
                                                  g_tokens[last], nullptr, scope, documentId, generation)) tokenTruncated = true;
        } else if (tokenIs(text, g_tokens[i], "using")) {
            uint32_t alias = i + 1;
            if (alias < tokenCount && tokenIsIdentifier(g_tokens[alias]) && alias + 1 < tokenCount &&
                tokenIsPunctuation(text, g_tokens[alias + 1], "=")) {
                if (!addSymbol(output, capacity, symbolCount, SymbolKind::UsingAlias, text, g_tokens[alias], nullptr,
                               scope, documentId, generation)) tokenTruncated = true;
            }
        }
    }
    sortSymbols(output, symbolCount, text);
    updateScanResult(result, true, tokenTruncated, symbolCount, tokenCount);
    return true;
}

void SymbolDatabaseInit(SymbolDatabase* database,
                        ProjectSymbol* projectStorage, uint32_t projectCapacity,
                        SymbolDocument* documentStorage, uint32_t documentCapacity,
                        DocumentSymbol* scratchStorage, uint32_t scratchCapacity) {
    if (!database) return;
    database->projectSymbols = projectStorage;
    database->documents = documentStorage;
    database->scratchSymbols = scratchStorage;
    database->projectCapacity = projectCapacity;
    database->documentCapacity = documentCapacity;
    database->scratchCapacity = scratchCapacity;
    database->projectSymbolCount = 0;
    database->documentCount = 0;
    database->droppedSymbols = 0;
    database->droppedDocuments = 0;
    database->projectGeneration = 0;
    database->lastIndexedDocumentId = 0;
    database->lastIndexedSymbolCount = 0;
    database->fullIndexCount = 0;
    database->incrementalIndexCount = 0;
    database->projectIndexActive = false;
    database->truncated = false;
}

void SymbolDatabaseClear(SymbolDatabase* database) {
    if (!database) return;
    database->projectSymbolCount = 0;
    database->documentCount = 0;
    database->droppedSymbols = 0;
    database->droppedDocuments = 0;
    database->projectGeneration = 0;
    database->lastIndexedDocumentId = 0;
    database->lastIndexedSymbolCount = 0;
    database->projectIndexActive = false;
    database->truncated = false;
}

bool SymbolDatabaseIndexDocument(SymbolDatabase* database, const char* path,
                                 uint64_t documentId, uint32_t generation, bool dirty,
                                 const char* text, uint32_t length) {
    if (!database || !path || !text || !database->scratchSymbols || database->scratchCapacity == 0 || !IsSymbolSourcePath(path)) return false;
    char normalized[kMaxPathBytes] = {};
    if (!NormalizePath(path, normalized, sizeof(normalized))) return false;
    SymbolScanResult result = {};
    if (!ScanDocumentSymbols(text, length, documentId, generation, database->scratchSymbols,
                             database->scratchCapacity, &result)) return false;
    int32_t index = ensureDocument(database, normalized, documentId, generation, dirty);
    if (index < 0) return false;
    SymbolDocument& document = database->documents[index];
    document.documentId = documentId;
    document.generation = generation;
    document.dirty = dirty;
    if (!replaceDocumentSymbols(database, static_cast<uint32_t>(index), database->scratchSymbols,
                                result.symbolCount, result.truncated)) return false;
    if (!database->projectIndexActive) ++database->incrementalIndexCount;
    return true;
}

bool SymbolDatabaseIndexDiskDocument(SymbolDatabase* database, const WorkspaceFileSystem& fileSystem,
                                     const char* path, uint64_t projectGeneration) {
    if (!database || !path || !fileSystem.read || !IsSymbolSourcePath(path)) return false;
    FileInfo info = {};
    if (fileSystem.stat && fileSystem.stat(fileSystem.userData, path, &info) && info.size > kMaxEditorBytes) return false;
    uint32_t bytes = 0;
    if (!fileSystem.read(fileSystem.userData, path, g_projectReadBuffer, kMaxEditorBytes, &bytes)) return false;
    return SymbolDatabaseIndexDocument(database, path, hashPath(path), static_cast<uint32_t>(projectGeneration), false,
                                       g_projectReadBuffer, bytes);
}

bool SymbolDatabaseIndexProject(SymbolDatabase* database, const WorkspaceFileSystem& fileSystem,
                                const char* rootPath, const Document* dirtyDocuments,
                                uint32_t dirtyDocumentCount, uint64_t projectGeneration) {
    if (!database || !rootPath || !fileSystem.list || !fileSystem.read) return false;
    char root[kMaxPathBytes] = {};
    if (!NormalizePath(rootPath, root, sizeof(root))) return false;
    const uint32_t previousFullIndexCount = database->fullIndexCount;
    const uint32_t previousIncrementalIndexCount = database->incrementalIndexCount;
    SymbolDatabaseClear(database);
    database->fullIndexCount = previousFullIndexCount;
    database->incrementalIndexCount = previousIncrementalIndexCount;
    database->projectGeneration = projectGeneration;
    ++database->fullIndexCount;
    database->projectIndexActive = true;
    const bool traversed = indexDirectory(database, fileSystem, root, 0, dirtyDocuments, dirtyDocumentCount, projectGeneration);
    for (uint32_t i = 0; i < dirtyDocumentCount; ++i) {
        if (!dirtyDocuments || !dirtyDocuments[i].used || !dirtyDocuments[i].buffer.dirty ||
            !IsSymbolSourcePath(dirtyDocuments[i].path)) continue;
        SymbolDatabaseIndexDocument(database, dirtyDocuments[i].path, dirtyDocuments[i].documentId,
                                    dirtyDocuments[i].buffer.generation, true,
                                    dirtyDocuments[i].buffer.data, dirtyDocuments[i].buffer.length);
    }
    database->projectIndexActive = false;
    return traversed;
}

bool SymbolDatabaseRemoveDocument(SymbolDatabase* database, const char* path) {
    if (!database || !path) return false;
    char normalized[kMaxPathBytes] = {};
    if (!NormalizePath(path, normalized, sizeof(normalized))) return false;
    int32_t found = findDocumentNormalized(database, normalized);
    if (found < 0) return false;
    removeProjectSymbols(database, static_cast<uint32_t>(found));
    for (uint32_t i = static_cast<uint32_t>(found); i + 1 < database->documentCount; ++i) {
        database->documents[i] = database->documents[i + 1];
        for (uint32_t symbol = 0; symbol < database->projectSymbolCount; ++symbol)
            if (database->projectSymbols[symbol].documentIndex == i + 1) database->projectSymbols[symbol].documentIndex = i;
    }
    --database->documentCount;
    return true;
}

uint32_t SymbolDatabaseProjectSymbolCount(const SymbolDatabase* database) { return database ? database->projectSymbolCount : 0; }
const ProjectSymbol* SymbolDatabaseProjectSymbolAt(const SymbolDatabase* database, uint32_t index) {
    return database && index < database->projectSymbolCount ? &database->projectSymbols[index] : nullptr;
}
uint32_t SymbolDatabaseDocumentCount(const SymbolDatabase* database) { return database ? database->documentCount : 0; }
const SymbolDocument* SymbolDatabaseDocumentAt(const SymbolDatabase* database, uint32_t index) {
    return database && index < database->documentCount ? &database->documents[index] : nullptr;
}
const DocumentSymbol* SymbolDatabaseDocumentSymbolAt(const SymbolDatabase* database, uint32_t documentIndex, uint32_t symbolIndex) {
    const SymbolDocument* document = SymbolDatabaseDocumentAt(database, documentIndex);
    if (!document || symbolIndex >= document->symbolCount) return nullptr;
    return &database->projectSymbols[document->symbolStart + symbolIndex].symbol;
}
int32_t SymbolDatabaseFindDocumentByPath(const SymbolDatabase* database, const char* path) {
    if (!database || !path) return -1;
    char normalized[kMaxPathBytes] = {};
    return NormalizePath(path, normalized, sizeof(normalized)) ? findDocumentNormalized(database, normalized) : -1;
}
int32_t SymbolDatabaseFindDocumentById(const SymbolDatabase* database, uint64_t documentId) {
    if (!database || documentId == 0) return -1;
    for (uint32_t i = 0; i < database->documentCount; ++i) if (database->documents[i].documentId == documentId) return static_cast<int32_t>(i);
    return -1;
}
const char* SymbolDatabaseDocumentPath(const SymbolDatabase* database, uint32_t documentIndex) {
    const SymbolDocument* document = SymbolDatabaseDocumentAt(database, documentIndex);
    return document ? document->path : "";
}

uint32_t SymbolDatabaseLookupByFile(const SymbolDatabase* database, const char* path, uint32_t* indices, uint32_t capacity) {
    int32_t document = SymbolDatabaseFindDocumentByPath(database, path);
    if (document < 0) return 0;
    const SymbolDocument& source = database->documents[document];
    const uint32_t count = source.symbolCount < capacity ? source.symbolCount : capacity;
    for (uint32_t i = 0; i < count; ++i) indices[i] = source.symbolStart + i;
    return source.symbolCount;
}
uint32_t SymbolDatabaseLookupByKind(const SymbolDatabase* database, SymbolKind kind, uint32_t* indices, uint32_t capacity) {
    uint32_t found = 0;
    if (!database) return 0;
    for (uint32_t i = 0; i < database->projectSymbolCount; ++i) {
        if (database->projectSymbols[i].symbol.kind != kind) continue;
        if (found < capacity) indices[found] = i;
        ++found;
    }
    return found;
}
uint32_t SymbolDatabaseLookupByName(const SymbolDatabase* database, const char* name, bool caseSensitive, uint32_t* indices, uint32_t capacity) {
    uint32_t found = 0;
    if (!database || !name || textLength(name, kSymbolMaxNameBytes + 1) > kSymbolMaxNameBytes) return 0;
    for (uint32_t i = 0; i < database->projectSymbolCount; ++i) {
        if (!equalText(database->projectSymbols[i].symbol.name, name, !caseSensitive)) continue;
        if (found < capacity) indices[found] = i;
        ++found;
    }
    return found;
}
uint32_t SymbolDatabaseLookupByPrefix(const SymbolDatabase* database, const char* prefix, bool caseSensitive, uint32_t* indices, uint32_t capacity) {
    uint32_t found = 0;
    if (!database || !prefix || !queryValid(prefix)) return 0;
    for (uint32_t i = 0; i < database->projectSymbolCount; ++i) {
        if (!startsWith(database->projectSymbols[i].symbol.name, prefix, !caseSensitive)) continue;
        if (found < capacity) indices[found] = i;
        ++found;
    }
    return found;
}
uint32_t SymbolDatabaseFindSymbols(const SymbolDatabase* database, const char* query, bool caseSensitive, uint32_t* indices, uint32_t capacity) {
    if (!database || !queryValid(query)) return 0;
    uint32_t found = 0;
    char display[kSymbolMaxContainerBytes + kSymbolMaxNameBytes] = {};
    for (uint32_t i = 0; i < database->projectSymbolCount; ++i) {
        const ProjectSymbol& symbol = database->projectSymbols[i];
        display[0] = '\0';
        symbolDisplayLength(symbol, display, sizeof(display));
        const bool match = startsWith(symbol.symbol.name, query, !caseSensitive) ||
            containsText(symbol.symbol.name, query, !caseSensitive) ||
            containsText(display, query, !caseSensitive);
        if (!match) continue;
        if (found < capacity) indices[found] = i;
        ++found;
        if (found >= kSymbolMaxVisibleResults && capacity >= kSymbolMaxVisibleResults) break;
    }
    return found;
}

bool SymbolDatabaseIsTruncated(const SymbolDatabase* database) { return database && database->truncated; }

} // namespace developer_studio
} // namespace guidexos
