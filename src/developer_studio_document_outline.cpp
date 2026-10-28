#include "developer_studio_document_outline.h"

#include "developer_studio_cpp_lexer.h"

namespace guidexos {
namespace developer_studio {
namespace {

static const uint32_t kNoSymbol = 0xFFFFFFFFu;

enum ScopeKind : uint8_t {
    ScopeOther = 0,
    ScopeNamespace,
    ScopeClass
};

struct OutlineScope {
    ScopeKind kind;
    uint32_t symbolIndex;
};

struct ScopeDeclaration {
    bool found;
    bool hasBody;
    bool limitReached;
    DocumentOutlineCategory category;
    ScopeKind scopeKind;
    CppLexToken name;
    uint32_t endOffset;
};

struct FunctionDeclaration {
    bool found;
    bool hasBody;
    bool limitReached;
    uint32_t endOffset;
};

static bool punctuation(const char* data, const CppLexToken& token, char value) {
    return token.kind == CppLexTokenKind::Punctuation && token.length == 1u &&
        data[token.start] == value;
}

static bool nextCodeToken(CppLexer* lexer, CppLexToken* token) {
    if (!lexer || !token) return false;
    while (CppLexerNext(lexer, token)) {
        if (token->kind != CppLexTokenKind::Comment &&
            token->kind != CppLexTokenKind::Preprocessor) return true;
        if (token->kind == CppLexTokenKind::End) return true;
    }
    return false;
}

static bool copyIdentifier(char* output, uint32_t outputSize, const char* data,
                            const CppLexToken& token) {
    if (!output || outputSize == 0 || !data || token.kind != CppLexTokenKind::Identifier ||
        token.length == 0 || token.length >= outputSize) return false;
    for (uint32_t i = 0; i < token.length; ++i) output[i] = data[token.start + i];
    output[token.length] = '\0';
    return true;
}

static bool isTypeWord(const char* data, const CppLexToken& token) {
    static const char* const words[] = {
        "void", "bool", "char", "wchar_t", "char8_t", "char16_t", "char32_t",
        "short", "int", "long", "float", "double", "signed", "unsigned",
        "auto", "decltype", "const", "volatile", "static", "inline", "constexpr",
        "consteval", "constinit", "virtual", "explicit", "friend", "extern"
    };
    for (uint32_t i = 0; i < sizeof(words) / sizeof(words[0]); ++i)
        if (CppLexTokenEquals(data, token, words[i])) return true;
    return false;
}

static bool isKeyword(const char* data, const CppLexToken& token, const char* word) {
    return token.kind == CppLexTokenKind::Identifier && CppLexTokenEquals(data, token, word);
}

static bool copySignature(DocumentOutlineSymbol* symbol, const char* data, uint32_t length,
                          uint32_t start, uint32_t end, uint32_t symbolOffset,
                          bool* clipped) {
    if (!symbol || !data || start > length || end > length || start > end || !clipped) return false;
    if (symbolOffset < start || symbolOffset > end) symbolOffset = start;
    // A pathological declaration can stretch across the whole bounded file.
    // Keep excerpt work small and move the window close to the symbol name.
    const uint32_t kExcerptSourceLimit = 512u;
    uint32_t excerptStart = start;
    if (end - excerptStart > kExcerptSourceLimit && symbolOffset > excerptStart + kExcerptSourceLimit)
        excerptStart = symbolOffset - 96u;
    uint32_t excerptEnd = end;
    if (excerptEnd - excerptStart > kExcerptSourceLimit) excerptEnd = excerptStart + kExcerptSourceLimit;
    *clipped = excerptEnd < end || excerptStart > start;

    uint32_t output = 0;
    bool pendingSpace = false;
    for (uint32_t i = excerptStart; i < excerptEnd && output < kDocumentOutlineMaxSignatureBytes; ++i) {
        const unsigned char value = static_cast<unsigned char>(data[i]);
        if (value == ' ' || value == '\t' || value == '\r' || value == '\n' || value == '\f') {
            pendingSpace = output != 0;
            continue;
        }
        if (pendingSpace && output < kDocumentOutlineMaxSignatureBytes) symbol->signature[output++] = ' ';
        pendingSpace = false;
        symbol->signature[output++] = value >= 0x20u && value <= 0x7Eu ? static_cast<char>(value) : '?';
    }
    if (output == kDocumentOutlineMaxSignatureBytes) *clipped = true;
    if (*clipped && output >= 3u) {
        output -= 3u;
        symbol->signature[output++] = '.';
        symbol->signature[output++] = '.';
        symbol->signature[output++] = '.';
    }
    symbol->signature[output] = '\0';
    return true;
}

static bool nameIsKnownType(const DocumentOutline* outline, const char* data,
                            const CppLexToken& token) {
    if (!outline || token.kind != CppLexTokenKind::Identifier) return false;
    for (uint32_t i = 0; i < outline->count; ++i) {
        const DocumentOutlineSymbol& symbol = outline->symbols[i];
        if (symbol.category != DocumentOutlineCategory::Class &&
            symbol.category != DocumentOutlineCategory::Struct &&
            symbol.category != DocumentOutlineCategory::Enum) continue;
        uint32_t j = 0;
        while (j < token.length && symbol.name[j] && symbol.name[j] == data[token.start + j]) ++j;
        if (j == token.length && symbol.name[j] == '\0') return true;
    }
    return false;
}

static uint32_t addSymbol(DocumentOutline* outline, const char* data, uint32_t length,
                          const CppLexToken& name, DocumentOutlineCategory category,
                          uint32_t signatureStart, uint32_t signatureEnd) {
    if (!outline || !data) return kNoSymbol;
    if (outline->totalCount != 0xFFFFFFFFu) ++outline->totalCount;
    if (name.length == 0 || name.length > kDocumentOutlineMaxIdentifierBytes) {
        outline->truncated = true;
        return kNoSymbol;
    }
    if (outline->count >= kDocumentOutlineMaxSymbols) {
        outline->truncated = true;
        return kNoSymbol;
    }
    DocumentOutlineSymbol& symbol = outline->symbols[outline->count];
    if (!copyIdentifier(symbol.name, sizeof(symbol.name), data, name)) {
        outline->truncated = true;
        return kNoSymbol;
    }
    symbol.category = category;
    symbol.line = name.line;
    symbol.offset = name.start;
    bool clipped = false;
    if (!copySignature(&symbol, data, length, signatureStart, signatureEnd,
                       name.start, &clipped)) {
        symbol.signature[0] = '\0';
        outline->truncated = true;
    } else if (clipped) {
        outline->truncated = true;
    }
    return outline->count++;
}

static ScopeDeclaration probeScopeDeclaration(const char* data, uint32_t length,
                                               const CppLexer& afterKeyword,
                                               const CppLexToken& keyword,
                                               const CppLexToken& previous) {
    ScopeDeclaration result = {};
    CppLexer look = afterKeyword;
    CppLexToken token = {};
    if (isKeyword(data, keyword, "namespace")) {
        result.category = DocumentOutlineCategory::Namespace;
        result.scopeKind = ScopeNamespace;
        if (!nextCodeToken(&look, &token)) return result;
        if (token.kind == CppLexTokenKind::Identifier) {
            result.name = token;
            CppLexToken next = {};
            if (!nextCodeToken(&look, &next)) return result;
            if (punctuation(data, next, ':')) {
                if (next.start != token.start + token.length) return result;
                CppLexer secondColonLook = look;
                CppLexToken secondColon = {};
                if (!nextCodeToken(&secondColonLook, &secondColon) ||
                    !punctuation(data, secondColon, ':')) return result;
                if (secondColon.start != next.start + next.length) return result;
                CppLexer nameLook = secondColonLook;
                CppLexToken secondName = {};
                if (!nextCodeToken(&nameLook, &secondName) ||
                    secondName.kind != CppLexTokenKind::Identifier ||
                    secondName.start != secondColon.start + secondColon.length ||
                    secondName.length + token.length + 2u > kDocumentOutlineMaxIdentifierBytes) return result;
                result.name.length += 2u + secondName.length;
                // Qualified namespaces are emitted as one bounded name below.
                look = nameLook;
                next = {};
                if (!nextCodeToken(&look, &next)) return result;
            }
            while (!punctuation(data, next, '{') && !punctuation(data, next, ';')) {
                if (look.offset - keyword.start > kDocumentOutlineMaxLookaheadBytes) {
                    result.limitReached = true;
                    return result;
                }
                if (!nextCodeToken(&look, &next)) return result;
            }
            result.found = punctuation(data, next, '{');
            result.hasBody = result.found;
            result.endOffset = next.start;
            return result;
        }
        if (punctuation(data, token, '{')) {
            result.found = true;
            result.hasBody = true;
            result.endOffset = token.start;
        }
        return result;
    }

    if (!isKeyword(data, keyword, "class") && !isKeyword(data, keyword, "struct") &&
        !isKeyword(data, keyword, "enum")) return result;
    if (isKeyword(data, keyword, "class") && isKeyword(data, previous, "enum")) return result;
    result.category = isKeyword(data, keyword, "class") ? DocumentOutlineCategory::Class :
        (isKeyword(data, keyword, "struct") ? DocumentOutlineCategory::Struct : DocumentOutlineCategory::Enum);
    result.scopeKind = result.category == DocumentOutlineCategory::Class ||
        result.category == DocumentOutlineCategory::Struct ? ScopeClass : ScopeOther;

    if (isKeyword(data, keyword, "enum")) {
        if (!nextCodeToken(&look, &token)) return result;
        if (isKeyword(data, token, "class") || isKeyword(data, token, "struct")) {
            if (!nextCodeToken(&look, &token)) return result;
        }
    } else if (!nextCodeToken(&look, &token)) return result;

    if (token.kind != CppLexTokenKind::Identifier) {
        // Anonymous class/enum bodies still affect brace nesting, but do not
        // produce a displayable symbol.
        CppLexer scan = look;
        uint32_t tokens = 0;
        while (tokens++ < kDocumentOutlineMaxLookaheadTokens &&
               scan.offset - keyword.start <= kDocumentOutlineMaxLookaheadBytes) {
            if (!nextCodeToken(&scan, &token)) break;
            if (punctuation(data, token, '{')) {
                result.found = true;
                result.hasBody = true;
                result.endOffset = token.start;
                return result;
            }
            if (punctuation(data, token, ';')) break;
        }
        if (tokens >= kDocumentOutlineMaxLookaheadTokens ||
            scan.offset - keyword.start > kDocumentOutlineMaxLookaheadBytes)
            result.limitReached = true;
        return result;
    }
    result.name = token;
    CppLexer scan = look;
    uint32_t tokens = 0;
    while (tokens++ < kDocumentOutlineMaxLookaheadTokens &&
           scan.offset - keyword.start <= kDocumentOutlineMaxLookaheadBytes) {
        if (!nextCodeToken(&scan, &token)) break;
        if (punctuation(data, token, '{')) {
            result.found = true;
            result.hasBody = true;
            result.endOffset = token.start;
            return result;
        }
        if (punctuation(data, token, ';')) {
            result.found = true;
            result.hasBody = false;
            result.endOffset = token.start;
            return result;
        }
        if ((result.category == DocumentOutlineCategory::Class ||
             result.category == DocumentOutlineCategory::Struct) &&
            (punctuation(data, token, '*') || punctuation(data, token, '&') ||
             punctuation(data, token, '(') || punctuation(data, token, '[') ||
             punctuation(data, token, '='))) return result;
    }
    if (tokens >= kDocumentOutlineMaxLookaheadTokens ||
        scan.offset - keyword.start > kDocumentOutlineMaxLookaheadBytes)
        result.limitReached = true;
    (void)length;
    return result;
}

static FunctionDeclaration probeFunction(const char* data, uint32_t length,
                                         const CppLexer& afterName,
                                         uint32_t nameOffset) {
    FunctionDeclaration result = {};
    CppLexer look = afterName;
    CppLexToken token = {};
    uint32_t tokenCount = 0;
    uint32_t nesting = 0;
    bool opened = false;
    while (tokenCount < kDocumentOutlineMaxLookaheadTokens &&
           look.offset - nameOffset <= kDocumentOutlineMaxLookaheadBytes) {
        if (!nextCodeToken(&look, &token)) return result;
        ++tokenCount;
        if (!opened) {
            if (!punctuation(data, token, '(')) return result;
            opened = true;
            nesting = 1;
            continue;
        }
        if (punctuation(data, token, '(')) ++nesting;
        else if (punctuation(data, token, ')')) {
            if (nesting == 0) return result;
            --nesting;
            if (nesting == 0) break;
        }
        if (nesting > kDocumentOutlineMaxNestingDepth) {
            result.limitReached = true;
            return result;
        }
        if (token.kind == CppLexTokenKind::End) return result;
    }
    if (nesting != 0 || tokenCount >= kDocumentOutlineMaxLookaheadTokens ||
        look.offset - nameOffset > kDocumentOutlineMaxLookaheadBytes) {
        result.limitReached = true;
        return result;
    }

    uint32_t parens = 0;
    uint32_t brackets = 0;
    while (tokenCount < kDocumentOutlineMaxLookaheadTokens &&
           look.offset - nameOffset <= kDocumentOutlineMaxLookaheadBytes) {
        if (!nextCodeToken(&look, &token)) return result;
        ++tokenCount;
        if (token.kind == CppLexTokenKind::End) return result;
        if (punctuation(data, token, '(')) ++parens;
        else if (punctuation(data, token, ')')) {
            if (parens == 0) return result;
            --parens;
        } else if (punctuation(data, token, '[')) ++brackets;
        else if (punctuation(data, token, ']')) {
            if (brackets == 0) return result;
            --brackets;
        } else if (parens == 0 && brackets == 0 && punctuation(data, token, ';')) {
            result.found = true;
            result.hasBody = false;
            result.endOffset = token.start;
            return result;
        } else if (parens == 0 && brackets == 0 && punctuation(data, token, '{')) {
            result.found = true;
            result.hasBody = true;
            result.endOffset = token.start;
            return result;
        } else if (parens == 0 && brackets == 0 && punctuation(data, token, '}')) {
            return result;
        }
        if (parens > kDocumentOutlineMaxNestingDepth ||
            brackets > kDocumentOutlineMaxNestingDepth) {
            result.limitReached = true;
            return result;
        }
    }
    if (tokenCount >= kDocumentOutlineMaxLookaheadTokens ||
        look.offset - nameOffset > kDocumentOutlineMaxLookaheadBytes)
        result.limitReached = true;
    (void)length;
    return result;
}

static bool scopeAllowsDeclarations(const OutlineScope* scopes, uint32_t depth) {
    if (!scopes) return depth == 0;
    for (uint32_t i = 0; i < depth; ++i)
        if (scopes[i].kind != ScopeClass && scopes[i].kind != ScopeNamespace) return false;
    return depth == 0 || scopes[depth - 1u].kind == ScopeClass ||
        scopes[depth - 1u].kind == ScopeNamespace;
}

static bool hasCurrentClassScope(const OutlineScope* scopes, uint32_t depth) {
    return scopes && depth != 0 && scopes[depth - 1u].kind == ScopeClass &&
        scopeAllowsDeclarations(scopes, depth);
}

static bool isLikelyFunctionPrefix(const DocumentOutline* outline, const char* data,
                                   const CppLexToken* previous, uint32_t previousCount,
                                   bool constructor) {
    if (constructor) return true;
    if (previousCount == 0) return false;
    const CppLexToken& last = previous[previousCount - 1u];
    if (last.kind == CppLexTokenKind::Punctuation && last.length == 1u &&
        (data[last.start] == '*' || data[last.start] == '&' || data[last.start] == '>')) return true;
    if (last.kind == CppLexTokenKind::Identifier &&
        (isTypeWord(data, last) || nameIsKnownType(outline, data, last))) return true;
    if (last.kind == CppLexTokenKind::Punctuation && data[last.start] == ':' && previousCount >= 2u &&
        previous[previousCount - 2u].kind == CppLexTokenKind::Punctuation &&
        data[previous[previousCount - 2u].start] == ':' && previousCount >= 4u) {
        const CppLexToken& typeBeforeQualifier = previous[previousCount - 4u];
        if (typeBeforeQualifier.kind == CppLexTokenKind::Punctuation &&
            (data[typeBeforeQualifier.start] == '*' || data[typeBeforeQualifier.start] == '&' ||
             data[typeBeforeQualifier.start] == '>')) return true;
        if (typeBeforeQualifier.kind == CppLexTokenKind::Identifier &&
            (isTypeWord(data, typeBeforeQualifier) ||
             nameIsKnownType(outline, data, typeBeforeQualifier))) return true;
    }
    return false;
}

static uint32_t currentClassSymbol(const OutlineScope* scopes, uint32_t depth) {
    return hasCurrentClassScope(scopes, depth) ? scopes[depth - 1u].symbolIndex : kNoSymbol;
}

static bool isConstructorName(const DocumentOutline* outline, const char* data,
                              const CppLexToken& name, uint32_t classSymbol) {
    if (!outline || classSymbol >= outline->count || name.kind != CppLexTokenKind::Identifier)
        return false;
    const char* className = outline->symbols[classSymbol].name;
    uint32_t i = 0;
    while (i < name.length && className[i] && className[i] == data[name.start + i]) ++i;
    return i == name.length && className[i] == '\0';
}

} // namespace

void DocumentOutlineInit(DocumentOutline* outline) {
    if (!outline) return;
    outline->count = 0;
    outline->totalCount = 0;
    outline->truncated = false;
    outline->error = DocumentOutlineError::None;
    outline->documentId = 0;
    outline->projectGeneration = 0;
    outline->documentGeneration = 0;
    outline->documentPath[0] = '\0';
    for (uint32_t i = 0; i < kDocumentOutlineMaxSymbols; ++i) {
        outline->symbols[i].name[0] = '\0';
        outline->symbols[i].signature[0] = '\0';
        outline->symbols[i].category = DocumentOutlineCategory::Function;
        outline->symbols[i].line = 0;
        outline->symbols[i].offset = 0;
    }
}

bool DocumentOutlineScan(const char* path, const char* data, uint32_t length,
                         DocumentOutline* outline) {
    if (!outline) return false;
    DocumentOutlineInit(outline);
    if (!path || (!data && length != 0)) {
        outline->error = DocumentOutlineError::InvalidInput;
        return false;
    }
    if (!CppIsSourcePath(path)) {
        outline->error = DocumentOutlineError::UnsupportedLanguage;
        return false;
    }
    if (length > kDocumentOutlineMaxInputBytes) {
        outline->error = DocumentOutlineError::DocumentTooLarge;
        outline->truncated = true;
        return false;
    }

    CppLexer lexer = {};
    CppLexerInit(&lexer, data, length);
    OutlineScope scopes[kDocumentOutlineMaxNestingDepth] = {};
    uint32_t depth = 0;
    uint32_t overflowDepth = 0;
    uint32_t statementStart = 0;
    ScopeKind pendingScope = ScopeOther;
    uint32_t pendingClassSymbol = kNoSymbol;
    bool pendingScopeValid = false;
    bool templatePrefix = false;
    CppLexToken previous[4] = {};
    uint32_t previousCount = 0;
    CppLexToken token = {};
    while (CppLexerNext(&lexer, &token)) {
        if (token.kind == CppLexTokenKind::End) break;
        if (token.kind == CppLexTokenKind::Comment) continue;
        if (token.kind == CppLexTokenKind::Preprocessor) {
            previousCount = 0;
            statementStart = lexer.offset;
            continue;
        }
        if (token.kind == CppLexTokenKind::Identifier &&
            token.length > kDocumentOutlineMaxIdentifierBytes) outline->truncated = true;

        if (overflowDepth != 0) {
            if (punctuation(data, token, '{')) ++overflowDepth;
            else if (punctuation(data, token, '}')) --overflowDepth;
            continue;
        }

        const bool declarationScope = scopeAllowsDeclarations(scopes, depth);
        if (token.kind == CppLexTokenKind::Identifier && isKeyword(data, token, "template"))
            templatePrefix = true;

        if (token.kind == CppLexTokenKind::Identifier && !templatePrefix) {
            ScopeDeclaration scope = probeScopeDeclaration(data, length, lexer, token,
                previousCount ? previous[previousCount - 1u] : CppLexToken());
            if (scope.limitReached) outline->truncated = true;
            if (scope.found) {
                uint32_t symbolIndex = kNoSymbol;
                if (scope.name.kind == CppLexTokenKind::Identifier) {
                    symbolIndex = addSymbol(outline, data, length, scope.name, scope.category,
                                            token.start, scope.endOffset);
                }
                if (scope.hasBody) {
                    pendingScope = scope.scopeKind;
                    pendingClassSymbol = symbolIndex;
                    pendingScopeValid = true;
                }
            }
        }

        if (token.kind == CppLexTokenKind::Identifier && declarationScope && !templatePrefix &&
            !isKeyword(data, token, "operator")) {
            CppLexer look = lexer;
            CppLexToken next = {};
            if (nextCodeToken(&look, &next) && punctuation(data, next, '(')) {
                const uint32_t classSymbol = currentClassSymbol(scopes, depth);
                const bool constructor = isConstructorName(outline, data, token, classSymbol);
                if (isLikelyFunctionPrefix(outline, data, previous, previousCount, constructor)) {
                    const FunctionDeclaration function = probeFunction(data, length, lexer, token.start);
                    if (function.limitReached) outline->truncated = true;
                    if (function.found) {
                        const bool qualifiedMethod = previousCount >= 4u &&
                            previous[previousCount - 1u].kind == CppLexTokenKind::Punctuation &&
                            previous[previousCount - 2u].kind == CppLexTokenKind::Punctuation &&
                            data[previous[previousCount - 1u].start] == ':' &&
                            data[previous[previousCount - 2u].start] == ':';
                        const bool qualifiedClassMethod = qualifiedMethod &&
                            previous[previousCount - 3u].kind == CppLexTokenKind::Identifier &&
                            nameIsKnownType(outline, data, previous[previousCount - 3u]);
                        const DocumentOutlineCategory category = hasCurrentClassScope(scopes, depth) ||
                            qualifiedClassMethod ? DocumentOutlineCategory::Method : DocumentOutlineCategory::Function;
                        if (function.hasBody) {
                            pendingScope = ScopeOther;
                            pendingClassSymbol = kNoSymbol;
                            pendingScopeValid = true;
                        }
                        addSymbol(outline, data, length, token, category,
                                  statementStart <= token.start ? statementStart : token.start,
                                  function.endOffset);
                    }
                }
            }
        }

        if (punctuation(data, token, '{')) {
            ScopeKind kind = pendingScopeValid ? pendingScope : ScopeOther;
            const uint32_t classIndex = pendingScopeValid && kind == ScopeClass ?
                pendingClassSymbol : kNoSymbol;
            if (depth < kDocumentOutlineMaxNestingDepth) {
                scopes[depth].kind = kind;
                scopes[depth].symbolIndex = classIndex;
                ++depth;
            } else {
                outline->truncated = true;
                overflowDepth = 1u;
            }
            pendingScopeValid = false;
            pendingScope = ScopeOther;
            pendingClassSymbol = kNoSymbol;
            templatePrefix = false;
            statementStart = token.start + token.length;
        } else if (punctuation(data, token, '}')) {
            if (depth > 0) --depth;
            pendingScopeValid = false;
            pendingScope = ScopeOther;
            pendingClassSymbol = kNoSymbol;
            templatePrefix = false;
            statementStart = token.start + token.length;
        } else if (punctuation(data, token, ';')) {
            pendingScopeValid = false;
            pendingScope = ScopeOther;
            pendingClassSymbol = kNoSymbol;
            templatePrefix = false;
            statementStart = token.start + token.length;
        }

        if (token.kind != CppLexTokenKind::Comment && token.kind != CppLexTokenKind::Preprocessor) {
            if (previousCount < 4u) previous[previousCount++] = token;
            else {
                previous[0] = previous[1];
                previous[1] = previous[2];
                previous[2] = previous[3];
                previous[3] = token;
            }
        }
    }
    return true;
}

void DocumentOutlineSetIdentity(DocumentOutline* outline, uint64_t documentId,
                                uint64_t projectGeneration, uint32_t documentGeneration,
                                const char* path) {
    if (!outline) return;
    outline->documentId = documentId;
    outline->projectGeneration = projectGeneration;
    outline->documentGeneration = documentGeneration;
    uint32_t i = 0;
    if (path) {
        while (i + 1u < sizeof(outline->documentPath) && path[i]) {
            outline->documentPath[i] = path[i];
            ++i;
        }
    }
    outline->documentPath[i] = '\0';
    if (path && path[i] != '\0') outline->truncated = true;
}

bool DocumentOutlineIdentityMatches(const DocumentOutline* outline,
                                    uint64_t documentId, uint64_t projectGeneration,
                                    uint32_t documentGeneration, const char* path) {
    if (!outline || !path || documentId == 0 || outline->documentId != documentId ||
        outline->projectGeneration != projectGeneration ||
        outline->documentGeneration != documentGeneration) return false;
    uint32_t i = 0;
    while (outline->documentPath[i] && path[i] && outline->documentPath[i] == path[i]) ++i;
    return outline->documentPath[i] == '\0' && path[i] == '\0';
}

bool DocumentOutlineCanNavigate(const DocumentOutline* outline, uint32_t symbolIndex,
                                uint64_t documentId, uint64_t projectGeneration,
                                uint32_t documentGeneration, const char* path,
                                const char* currentData, uint32_t currentLength,
                                const DocumentOutlineSymbol** outSymbol) {
    if (outSymbol) *outSymbol = nullptr;
    if (!DocumentOutlineIdentityMatches(outline, documentId, projectGeneration,
                                        documentGeneration, path) ||
        symbolIndex >= outline->count || (!currentData && currentLength != 0)) return false;
    const DocumentOutlineSymbol& symbol = outline->symbols[symbolIndex];
    uint32_t nameLength = 0;
    while (nameLength < sizeof(symbol.name) && symbol.name[nameLength]) ++nameLength;
    if (nameLength == 0 || symbol.offset > currentLength ||
        nameLength > currentLength - symbol.offset) return false;
    for (uint32_t i = 0; i < nameLength; ++i)
        if (currentData[symbol.offset + i] != symbol.name[i]) return false;
    if (outSymbol) *outSymbol = &symbol;
    return true;
}

const char* DocumentOutlineCategoryName(DocumentOutlineCategory category) {
    switch (category) {
    case DocumentOutlineCategory::Function: return "Function";
    case DocumentOutlineCategory::Method: return "Method";
    case DocumentOutlineCategory::Class: return "Class";
    case DocumentOutlineCategory::Struct: return "Struct";
    case DocumentOutlineCategory::Enum: return "Enum";
    case DocumentOutlineCategory::Namespace: return "Namespace";
    }
    return "Symbol";
}

const char* DocumentOutlineErrorName(DocumentOutlineError error) {
    switch (error) {
    case DocumentOutlineError::None: return "none";
    case DocumentOutlineError::InvalidInput: return "invalid_input";
    case DocumentOutlineError::UnsupportedLanguage: return "unsupported_language";
    case DocumentOutlineError::DocumentTooLarge: return "document_too_large";
    }
    return "unknown";
}

} // namespace developer_studio
} // namespace guidexos
