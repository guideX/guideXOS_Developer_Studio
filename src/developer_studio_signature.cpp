#include "developer_studio_signature.h"
#include "developer_studio_relationships.h"

namespace guidexos {
namespace developer_studio {
namespace {

static const uint32_t kInvalidIndex = 0xFFFFFFFFu;

static uint32_t textLength(const char* text, uint32_t limit) {
    if (!text) return 0;
    uint32_t length = 0;
    while (length < limit && text[length] != '\0') ++length;
    return length;
}

static void copyText(char* output, uint32_t capacity, const char* input) {
    if (!output || capacity == 0) return;
    uint32_t i = 0;
    if (input) while (i + 1 < capacity && input[i] != '\0') { output[i] = input[i]; ++i; }
    output[i] = '\0';
}

static void copyRange(char* output, uint32_t capacity, const char* input,
                      uint32_t start, uint32_t length) {
    if (!output || capacity == 0) return;
    uint32_t i = 0;
    while (input && i + 1 < capacity && i < length) output[i] = input[start + i], ++i;
    output[i] = '\0';
}

static bool appendChar(char* output, uint32_t capacity, uint32_t& length, char value) {
    if (!output || length + 1 >= capacity) return false;
    output[length++] = value;
    output[length] = '\0';
    return true;
}

static bool appendRange(char* output, uint32_t capacity, uint32_t& length,
                        const char* input, uint32_t start, uint32_t count) {
    for (uint32_t i = 0; i < count; ++i)
        if (!appendChar(output, capacity, length, input[start + i])) return false;
    return true;
}

static bool isIdentifierStart(char value) {
    return value == '_' || (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z');
}

static bool isIdentifierPart(char value) {
    return isIdentifierStart(value) || (value >= '0' && value <= '9');
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

static void trimRange(const char* text, uint32_t* start, uint32_t* end) {
    if (!text || !start || !end) return;
    while (*start < *end && (text[*start] == ' ' || text[*start] == '\t' ||
                             text[*start] == '\r' || text[*start] == '\n')) ++*start;
    while (*end > *start && (text[*end - 1] == ' ' || text[*end - 1] == '\t' ||
                             text[*end - 1] == '\r' || text[*end - 1] == '\n')) --*end;
}

static bool isIgnoredKind(SyntaxTokenKind kind) {
    return kind == SyntaxTokenKind::Comment || kind == SyntaxTokenKind::StringLiteral ||
        kind == SyntaxTokenKind::CharacterLiteral || kind == SyntaxTokenKind::Preprocessor;
}

static bool syntaxKindAt(const Document& document, uint32_t offset, SyntaxTokenKind* output) {
    if (output) *output = SyntaxTokenKind::PlainText;
    if (offset >= document.buffer.length) return true;
    if (!document.syntax.valid || document.syntax.generation != document.buffer.generation) return false;
    uint32_t line = 0;
    uint32_t column = 0;
    if (!OffsetToLineColumn(&document.buffer, offset, &line, &column)) return false;
    uint32_t count = 0;
    const SyntaxTokenSpan* spans = SyntaxCacheLineSpans(&document.syntax, line, &count);
    const uint32_t lineStart = TextBufferLineStart(&document.buffer, line);
    const uint32_t relative = offset - lineStart;
    for (uint32_t i = 0; i < count; ++i) {
        if (relative >= spans[i].start && relative < spans[i].start + spans[i].length) {
            if (output) *output = spans[i].kind;
            return true;
        }
    }
    return true;
}

static bool isIgnoredAt(const Document& document, uint32_t offset, bool* ready) {
    SyntaxTokenKind kind = SyntaxTokenKind::PlainText;
    const bool valid = syntaxKindAt(document, offset, &kind);
    if (ready && !valid) *ready = false;
    return valid && isIgnoredKind(kind);
}

static bool rawStringNear(const Document& document, uint32_t offset) {
    if (offset > document.buffer.length) return false;
    const uint32_t start = offset > 1024u ? offset - 1024u : 0;
    for (uint32_t cursor = offset; cursor > start; --cursor) {
        const uint32_t position = cursor - 1;
        if (document.buffer.data[position] != 'R' || position + 1 >= document.buffer.length || document.buffer.data[position + 1] != '"') continue;
        return true;
    }
    return false;
}

static bool skipBackwardTrivia(const Document& document, uint32_t lower, uint32_t* cursor) {
    if (!cursor) return false;
    while (*cursor > lower) {
        const uint32_t offset = *cursor - 1;
        bool ready = true;
        if (isIgnoredAt(document, offset, &ready)) {
            if (!ready) return false;
            --*cursor;
            continue;
        }
        const char value = document.buffer.data[offset];
        if (value == ' ' || value == '\t' || value == '\r' || value == '\n') --*cursor;
        else break;
    }
    return true;
}

static bool inactiveIfZero(const Document& document, uint32_t target) {
    if (target > kSignatureMaxContextScanBytes) return false;
    bool disabled[64] = {};
    uint32_t depth = 0;
    uint32_t lineStart = 0;
    while (lineStart <= target && lineStart < document.buffer.length) {
        uint32_t lineEnd = lineStart;
        while (lineEnd < document.buffer.length && document.buffer.data[lineEnd] != '\n') ++lineEnd;
        uint32_t first = lineStart;
        while (first < lineEnd && (document.buffer.data[first] == ' ' || document.buffer.data[first] == '\t')) ++first;
        if (first < lineEnd && document.buffer.data[first] == '#') {
            uint32_t cursor = first + 1;
            while (cursor < lineEnd && (document.buffer.data[cursor] == ' ' || document.buffer.data[cursor] == '\t')) ++cursor;
            uint32_t wordStart = cursor;
            while (cursor < lineEnd && isIdentifierPart(document.buffer.data[cursor])) ++cursor;
            const uint32_t wordLength = cursor - wordStart;
            while (cursor < lineEnd && (document.buffer.data[cursor] == ' ' || document.buffer.data[cursor] == '\t')) ++cursor;
            const bool parentDisabled = depth > 0 && disabled[depth - 1];
            if (wordLength == 2 && document.buffer.data[wordStart] == 'i' && document.buffer.data[wordStart + 1] == 'f') {
                const bool zero = cursor < lineEnd && document.buffer.data[cursor] == '0' &&
                    (cursor + 1 == lineEnd || !isIdentifierPart(document.buffer.data[cursor + 1]));
                if (depth < 64) disabled[depth++] = parentDisabled || zero;
            } else if ((wordLength == 4 && document.buffer.data[wordStart] == 'e' &&
                        document.buffer.data[wordStart + 1] == 'l' && document.buffer.data[wordStart + 2] == 's' &&
                        document.buffer.data[wordStart + 3] == 'e') ||
                       (wordLength == 4 && document.buffer.data[wordStart] == 'e' &&
                        document.buffer.data[wordStart + 1] == 'l' && document.buffer.data[wordStart + 2] == 'i' &&
                        document.buffer.data[wordStart + 3] == 'f')) {
                if (depth > 0) disabled[depth - 1] = depth > 1 && disabled[depth - 2];
            } else if (wordLength == 5 && document.buffer.data[wordStart] == 'e' &&
                       document.buffer.data[wordStart + 1] == 'n' && document.buffer.data[wordStart + 2] == 'd' &&
                       document.buffer.data[wordStart + 3] == 'i' && document.buffer.data[wordStart + 4] == 'f') {
                if (depth > 0) --depth;
            }
        }
        if (target <= lineEnd) break;
        lineStart = lineEnd + 1;
    }
    return depth > 0 && disabled[depth - 1];
}

static bool keywordCannotBeCallable(const char* name) {
    static const char* const words[] = {
        "if", "for", "while", "switch", "catch", "sizeof", "decltype", "alignof",
        "static_cast", "dynamic_cast", "const_cast", "reinterpret_cast", "typeid", "noexcept"
    };
    for (uint32_t i = 0; i < sizeof(words) / sizeof(words[0]); ++i)
        if (equalText(name, words[i], false)) return true;
    return false;
}

static bool isCodeChar(const Document& document, uint32_t offset) {
    bool ready = true;
    return offset < document.buffer.length && !isIgnoredAt(document, offset, &ready) && ready;
}

static bool findInvocationOpen(const Document& document, uint32_t caret, uint32_t* output,
                               uint32_t* nestingDepth, bool* contextTooLarge, bool* nestingLimit) {
    if (output) *output = 0;
    if (nestingDepth) *nestingDepth = 0;
    if (contextTooLarge) *contextTooLarge = false;
    if (nestingLimit) *nestingLimit = false;
    const uint32_t start = caret > kSignatureMaxContextScanBytes ?
        caret - kSignatureMaxContextScanBytes : 0;
    char expected[kSignatureMaxNestingDepth] = {};
    uint32_t depth = 0;
    for (uint32_t cursor = caret; cursor > start; ) {
        const uint32_t offset = --cursor;
        if (!isCodeChar(document, offset)) continue;
        const char value = document.buffer.data[offset];
        if (value == ')' || value == ']' || value == '}') {
            if (depth >= kSignatureMaxNestingDepth) {
                if (nestingLimit) *nestingLimit = true;
                return false;
            }
            expected[depth++] = value == ')' ? '(' : value == ']' ? '[' : '{';
        } else if (value == '(' || value == '[' || value == '{') {
            if (depth > 0 && expected[depth - 1] == value) --depth;
            else if (value == '(') {
                if (output) *output = offset;
                if (nestingDepth) *nestingDepth = depth;
                return true;
            }
        }
    }
    if (depth > 0 && start > 0 && contextTooLarge) *contextTooLarge = true;
    return false;
}

static bool parseQualifierBackward(const Document& document, uint32_t lower, uint32_t end,
                                   uint32_t* outputStart) {
    uint32_t cursor = end;
    uint32_t start = end;
    bool found = false;
    while (cursor > lower) {
        if (!skipBackwardTrivia(document, lower, &cursor)) return false;
        uint32_t identifierEnd = cursor;
        while (cursor > lower && isIdentifierPart(document.buffer.data[cursor - 1])) --cursor;
        if (cursor == identifierEnd) break;
        start = cursor;
        found = true;
        if (!skipBackwardTrivia(document, lower, &cursor)) return false;
        if (cursor < lower + 2 || document.buffer.data[cursor - 2] != ':' || document.buffer.data[cursor - 1] != ':') break;
        cursor -= 2;
    }
    if (outputStart) *outputStart = found ? start : end;
    return found;
}

static bool extractCallable(const Document& document, uint32_t openParen,
                            SignatureInvocationContext* context, SignatureErrorCode* error) {
    uint32_t cursor = openParen;
    if (!skipBackwardTrivia(document, 0, &cursor)) { if (error) *error = SignatureErrorCode::IndexNotReady; return false; }
    uint32_t nameEnd = cursor;
    while (cursor > 0 && isIdentifierPart(document.buffer.data[cursor - 1])) --cursor;
    if (cursor == nameEnd) {
        if (nameEnd > 0 && document.buffer.data[nameEnd - 1] == ')') {
            context->kind = SignatureContextKind::FunctionPointerCall;
        }
        if (error) *error = SignatureErrorCode::UnsupportedContext;
        return false;
    }
    const uint32_t nameStart = cursor;
    if (nameEnd - nameStart > kSignatureMaxCallableBytes) {
        if (error) *error = SignatureErrorCode::CallableTooLong;
        return false;
    }
    copyRange(context->callableName, sizeof(context->callableName), document.buffer.data,
              nameStart, nameEnd - nameStart);
    if (keywordCannotBeCallable(context->callableName)) {
        if (error) *error = SignatureErrorCode::UnsupportedContext;
        return false;
    }

    cursor = nameStart;
    if (!skipBackwardTrivia(document, 0, &cursor)) { if (error) *error = SignatureErrorCode::IndexNotReady; return false; }
    uint32_t qualifierStart = cursor;
    if (cursor >= 2 && document.buffer.data[cursor - 2] == ':' && document.buffer.data[cursor - 1] == ':') {
        qualifierStart = cursor - 2;
        if (!parseQualifierBackward(document, 0, qualifierStart, &qualifierStart)) {
            if (error) *error = SignatureErrorCode::IndexNotReady;
            return false;
        }
        if (nameStart - qualifierStart > kSignatureMaxQualifierBytes) {
            if (error) *error = SignatureErrorCode::QualifierTooLong;
            return false;
        }
        copyRange(context->explicitQualifier, sizeof(context->explicitQualifier), document.buffer.data,
                  qualifierStart, nameStart - 2 - qualifierStart);
        context->hasExplicitQualifier = true;
        context->kind = SignatureContextKind::QualifiedFunctionCall;
    } else {
        bool member = false;
        uint32_t receiverEnd = cursor;
        uint32_t receiverStart = receiverEnd;
        if (cursor > 0 && document.buffer.data[cursor - 1] == '.') {
            member = true;
            receiverEnd = cursor - 1;
        } else if (cursor >= 2 && document.buffer.data[cursor - 2] == '-' && document.buffer.data[cursor - 1] == '>') {
            member = true;
            receiverEnd = cursor - 2;
        }
        if (member) {
            receiverStart = receiverEnd;
            while (receiverStart > 0 && isIdentifierPart(document.buffer.data[receiverStart - 1])) --receiverStart;
            if (receiverEnd - receiverStart > kSignatureMaxReceiverHintBytes) {
                if (error) *error = SignatureErrorCode::QualifierTooLong;
                return false;
            }
            copyRange(context->receiverExpressionHint, sizeof(context->receiverExpressionHint), document.buffer.data,
                      receiverStart, receiverEnd - receiverStart);
            context->kind = SignatureContextKind::MethodCallLexical;
        } else context->kind = SignatureContextKind::FunctionCall;
    }
    context->receiverTypeResolved = false;
    return true;
}

static bool plausibleAngleOpen(const char* text, uint32_t start, uint32_t position, uint32_t end) {
    uint32_t cursor = position;
    while (cursor > start && (text[cursor - 1] == ' ' || text[cursor - 1] == '\t')) --cursor;
    if (cursor == start || !(isIdentifierPart(text[cursor - 1]) || text[cursor - 1] == '>')) return false;
    uint32_t scanned = 0;
    for (cursor = position + 1; cursor < end && scanned < 512; ++cursor, ++scanned) {
        const char value = text[cursor];
        if (value == ';' || value == '\n' || value == '{' || value == '}') return false;
        if (value == '>' && (cursor == position + 1 || text[cursor - 1] != '-')) return true;
        if (value == '+' || value == '-' || value == '/' || value == '*' || value == '=' || value == '&' || value == '|') return false;
    }
    return false;
}

static bool computeArgumentIndex(const Document& document, uint32_t openParen, uint32_t caret,
                                 uint32_t* argument, uint32_t* nestingDepth, bool* approximate,
                                 SignatureErrorCode* error) {
    if (argument) *argument = 0;
    if (nestingDepth) *nestingDepth = 0;
    if (approximate) *approximate = false;
    uint32_t parens = 0;
    uint32_t brackets = 0;
    uint32_t braces = 0;
    uint32_t angles = 0;
    uint32_t commas = 0;
    for (uint32_t cursor = openParen + 1; cursor < caret; ++cursor) {
        if (!isCodeChar(document, cursor)) continue;
        const char value = document.buffer.data[cursor];
        if (value == '(') { if (++parens > kSignatureMaxNestingDepth) { if (error) *error = SignatureErrorCode::NestingLimit; return false; } }
        else if (value == ')' && parens > 0) --parens;
        else if (value == '[') { if (++brackets > kSignatureMaxNestingDepth) { if (error) *error = SignatureErrorCode::NestingLimit; return false; } }
        else if (value == ']' && brackets > 0) --brackets;
        else if (value == '{') { if (++braces > kSignatureMaxNestingDepth) { if (error) *error = SignatureErrorCode::NestingLimit; return false; } }
        else if (value == '}' && braces > 0) --braces;
        else if (value == '<' && plausibleAngleOpen(document.buffer.data, openParen + 1, cursor, caret)) {
            if (++angles > kSignatureMaxNestingDepth) { if (error) *error = SignatureErrorCode::NestingLimit; return false; }
            if (approximate) *approximate = true;
        } else if (value == '>' && angles > 0) --angles;
        else if (value == ',' && parens == 0 && brackets == 0 && braces == 0 && angles == 0) ++commas;
    }
    if (argument) *argument = commas;
    if (nestingDepth) *nestingDepth = parens + brackets + braces + angles;
    if (angles != 0 && approximate) *approximate = true;
    return true;
}

static bool inferContainingScope(const SignatureInvocationContext& context, const SymbolDatabase* database,
                                 char* output, uint32_t capacity) {
    if (!output || capacity == 0) return false;
    output[0] = '\0';
    if (!database) return false;
    const ProjectSymbol* best = nullptr;
    for (uint32_t i = 0; i < SymbolDatabaseProjectSymbolCount(database); ++i) {
        const ProjectSymbol* symbol = SymbolDatabaseProjectSymbolAt(database, i);
        if (!symbol || symbol->symbol.location.documentId != context.documentId ||
            symbol->symbol.location.identifierOffset >= context.openParenByteOffset) continue;
        if (symbol->symbol.kind != SymbolKind::Namespace && symbol->symbol.kind != SymbolKind::Class &&
            symbol->symbol.kind != SymbolKind::Struct && symbol->symbol.kind != SymbolKind::Union) continue;
        if (!best || symbol->symbol.location.identifierOffset > best->symbol.location.identifierOffset) best = symbol;
    }
    if (!best) return false;
    copyText(output, capacity, best->symbol.qualifiedName);
    return output[0] != '\0';
}

static bool supportedKind(SymbolKind kind) {
    return kind == SymbolKind::Function || kind == SymbolKind::Method || kind == SymbolKind::Constructor;
}

static bool isTypeWord(const char* word) {
    static const char* const words[] = {
        "void", "bool", "char", "signed", "unsigned", "short", "int", "long", "float", "double",
        "auto", "const", "volatile", "wchar_t", "char8_t", "char16_t", "char32_t", "size_t"
    };
    for (uint32_t i = 0; i < sizeof(words) / sizeof(words[0]); ++i) if (equalText(word, words[i], false)) return true;
    return false;
}

static bool findTopLevelEquals(const char* text, uint32_t start, uint32_t end, uint32_t* equals) {
    uint32_t parens = 0, brackets = 0, braces = 0, angles = 0;
    for (uint32_t i = start; i < end; ++i) {
        const char value = text[i];
        if (value == '(') ++parens;
        else if (value == ')' && parens > 0) --parens;
        else if (value == '[') ++brackets;
        else if (value == ']' && brackets > 0) --brackets;
        else if (value == '{') ++braces;
        else if (value == '}' && braces > 0) --braces;
        else if (value == '<' && plausibleAngleOpen(text, start, i, end)) ++angles;
        else if (value == '>' && angles > 0) --angles;
        else if (value == '=' && parens == 0 && brackets == 0 && braces == 0 && angles == 0) {
            if (equals) *equals = i;
            return true;
        }
    }
    return false;
}

static void parseParameterNameAndType(SignatureParameter* parameter, const char* text,
                                      uint32_t start, uint32_t end) {
    uint32_t coreEnd = end;
    uint32_t equals = 0;
    if (findTopLevelEquals(text, start, end, &equals)) {
        parameter->hasDefaultValue = true;
        coreEnd = equals;
    }
    trimRange(text, &start, &coreEnd);
    if (coreEnd <= start) return;
    uint32_t nameEnd = coreEnd;
    while (nameEnd > start && !isIdentifierPart(text[nameEnd - 1])) --nameEnd;
    uint32_t nameStart = nameEnd;
    while (nameStart > start && isIdentifierPart(text[nameStart - 1])) --nameStart;
    char lastWord[kSignatureMaxParameterNameBytes + 1] = {};
    if (nameEnd > nameStart) copyRange(lastWord, sizeof(lastWord), text, nameStart, nameEnd - nameStart);
    const bool onlyWord = nameStart == start && isTypeWord(lastWord);
    if (nameEnd == nameStart || onlyWord) {
        copyRange(parameter->typeText, sizeof(parameter->typeText), text, start, coreEnd - start);
        return;
    }
    copyRange(parameter->name, sizeof(parameter->name), text, nameStart, nameEnd - nameStart);
    uint32_t typeEnd = nameStart;
    trimRange(text, &start, &typeEnd);
    copyRange(parameter->typeText, sizeof(parameter->typeText), text, start, typeEnd - start);
}

static bool parseCandidateParameters(SignatureHelpSession* session, SignatureCandidate* candidate) {
    const uint32_t length = textLength(candidate->displaySignature, sizeof(candidate->displaySignature));
    uint32_t open = length;
    for (uint32_t i = 0; i < length; ++i) if (candidate->displaySignature[i] == '(') { open = i; break; }
    if (open == length) { candidate->parameterParseFailed = true; return false; }
    uint32_t close = length;
    uint32_t parens = 0, brackets = 0, braces = 0, angles = 0;
    for (uint32_t i = open; i < length; ++i) {
        const char value = candidate->displaySignature[i];
        if (value == '(') ++parens;
        else if (value == ')' && parens > 0) {
            --parens;
            if (parens == 0) { close = i; break; }
        } else if (value == '[') ++brackets;
        else if (value == ']' && brackets > 0) --brackets;
        else if (value == '{') ++braces;
        else if (value == '}' && braces > 0) --braces;
        else if (value == '<' && plausibleAngleOpen(candidate->displaySignature, open + 1, i, length)) ++angles;
        else if (value == '>' && angles > 0) --angles;
    }
    if (close == length || parens != 0 || brackets != 0 || braces != 0) {
        candidate->parameterParseFailed = true;
        return false;
    }
    uint32_t first = open + 1;
    uint32_t parameterCount = 0;
    for (uint32_t cursor = open + 1; cursor <= close; ++cursor) {
        bool split = cursor == close;
        if (!split) {
            const char value = candidate->displaySignature[cursor];
            if (value == '(') ++parens;
            else if (value == ')' && parens > 0) --parens;
            else if (value == '[') ++brackets;
            else if (value == ']' && brackets > 0) --brackets;
            else if (value == '{') ++braces;
            else if (value == '}' && braces > 0) --braces;
            else if (value == '<' && plausibleAngleOpen(candidate->displaySignature, first, cursor, close)) ++angles;
            else if (value == '>' && angles > 0) --angles;
            else if (value == ',' && parens == 0 && brackets == 0 && braces == 0 && angles == 0) split = true;
        }
        if (!split) continue;
        uint32_t start = first;
        uint32_t end = cursor;
        trimRange(candidate->displaySignature, &start, &end);
        if (start == end) { first = cursor + 1; continue; }
        if (end - start == 4 && candidate->displaySignature[start] == 'v' && candidate->displaySignature[start + 1] == 'o' &&
            candidate->displaySignature[start + 2] == 'i' && candidate->displaySignature[start + 3] == 'd') {
            first = cursor + 1;
            continue;
        }
        if (parameterCount >= kSignatureMaxParameters || parameterCount >= session->parameterCapacity - session->parameterCount) {
            candidate->truncatedParameters = true;
            candidate->parameterParseFailed = true;
            return false;
        }
        SignatureParameter& parameter = session->parameters[session->parameterCount++];
        parameter = {};
        parameter.displayStart = start;
        parameter.displayLength = end - start;
        parameter.displayTruncated = parameter.displayLength > kSignatureMaxParameterDisplayBytes;
        copyRange(parameter.displayText, sizeof(parameter.displayText), candidate->displaySignature, start,
                   parameter.displayLength);
        parseParameterNameAndType(&parameter, candidate->displaySignature, start, end);
        const uint32_t textSize = textLength(parameter.displayText, sizeof(parameter.displayText));
        parameter.variadic = textSize >= 3 && parameter.displayText[textSize - 3] == '.' &&
            parameter.displayText[textSize - 2] == '.' && parameter.displayText[textSize - 1] == '.';
        if (!parameter.variadic) {
            for (uint32_t i = 0; i + 2 < textSize; ++i)
                if (parameter.displayText[i] == '.' && parameter.displayText[i + 1] == '.' && parameter.displayText[i + 2] == '.') parameter.variadic = true;
        }
        first = cursor + 1;
        ++parameterCount;
    }
    candidate->parameterStart = session->parameterCount - parameterCount;
    candidate->parameterCount = parameterCount;
    if (candidate->parameterCount == 0 && close > open + 1) candidate->parameterParseFailed = true;
    return !candidate->parameterParseFailed;
}

static bool normalizedSignatureEqual(const SignatureCandidate& left, const SignatureCandidate& right) {
    if (!equalText(left.qualifiedName, right.qualifiedName, false) || left.symbolKind != right.symbolKind) return false;
    char leftNormalized[kSignatureMaxDisplayBytes + 1] = {};
    char rightNormalized[kSignatureMaxDisplayBytes + 1] = {};
    uint32_t leftParameters = 0;
    uint32_t rightParameters = 0;
    bool leftComplete = false;
    bool rightComplete = false;
    bool leftApproximate = false;
    bool rightApproximate = false;
    if (NormalizeRelationshipSignature(left.displaySignature, leftNormalized, sizeof(leftNormalized),
                                       &leftParameters, &leftComplete, &leftApproximate) &&
        NormalizeRelationshipSignature(right.displaySignature, rightNormalized, sizeof(rightNormalized),
                                       &rightParameters, &rightComplete, &rightApproximate) &&
        leftParameters == rightParameters && equalText(leftNormalized, rightNormalized, false)) return true;
    const uint32_t leftLength = textLength(left.displaySignature, sizeof(left.displaySignature));
    const uint32_t rightLength = textLength(right.displaySignature, sizeof(right.displaySignature));
    uint32_t l = 0, r = 0;
    while (l < leftLength && r < rightLength) {
        if (left.displaySignature[l] == ' ' || left.displaySignature[l] == '\t' || left.displaySignature[l] == '\r' || left.displaySignature[l] == '\n') { ++l; continue; }
        if (right.displaySignature[r] == ' ' || right.displaySignature[r] == '\t' || right.displaySignature[r] == '\r' || right.displaySignature[r] == '\n') { ++r; continue; }
        if (left.displaySignature[l++] != right.displaySignature[r++]) return false;
    }
    while (l < leftLength && (left.displaySignature[l] == ' ' || left.displaySignature[l] == '\t' || left.displaySignature[l] == '\r' || left.displaySignature[l] == '\n')) ++l;
    while (r < rightLength && (right.displaySignature[r] == ' ' || right.displaySignature[r] == '\t' || right.displaySignature[r] == '\r' || right.displaySignature[r] == '\n')) ++r;
    return l == leftLength && r == rightLength;
}

static int32_t roleScore(SymbolDeclarationRole role) {
    return role == SymbolDeclarationRole::Definition ? 300 :
        role == SymbolDeclarationRole::Declaration ? 200 : role == SymbolDeclarationRole::ForwardDeclaration ? 100 : 0;
}

static uint64_t hashText(uint64_t hash, const char* text) {
    if (!text) return hash;
    for (uint32_t i = 0; text[i] != '\0'; ++i) hash = (hash ^ static_cast<uint8_t>(text[i])) * 1099511628211ULL;
    return hash;
}

static uint64_t candidateId(const SignatureCandidate& candidate) {
    uint64_t hash = 1469598103934665603ULL;
    hash = hashText(hash, candidate.qualifiedName);
    hash = hashText(hash, candidate.displaySignature);
    hash ^= static_cast<uint64_t>(candidate.symbolKind) + 1u;
    return hash;
}

static bool candidateLess(const SignatureCandidate& left, const SignatureCandidate& right) {
    if (left.rankScore != right.rankScore) return left.rankScore > right.rankScore;
    int compare = 0;
    const char* a = left.qualifiedName;
    const char* b = right.qualifiedName;
    while (*a && *b && *a == *b) ++a, ++b;
    compare = static_cast<unsigned char>(*a) - static_cast<unsigned char>(*b);
    if (compare != 0) return compare < 0;
    a = left.displaySignature; b = right.displaySignature;
    while (*a && *b && *a == *b) ++a, ++b;
    compare = static_cast<unsigned char>(*a) - static_cast<unsigned char>(*b);
    if (compare != 0) return compare < 0;
    if (left.declarationRole != right.declarationRole) return left.declarationRole < right.declarationRole;
    a = left.relativePath; b = right.relativePath;
    while (*a && *b && *a == *b) ++a, ++b;
    compare = static_cast<unsigned char>(*a) - static_cast<unsigned char>(*b);
    if (compare != 0) return compare < 0;
    if (left.line != right.line) return left.line < right.line;
    if (left.column != right.column) return left.column < right.column;
    return left.byteOffset < right.byteOffset;
}

static void insertionSort(SignatureCandidate* candidates, uint32_t count) {
    for (uint32_t i = 1; i < count; ++i) {
        SignatureCandidate value = candidates[i];
        uint32_t j = i;
        while (j > 0 && candidateLess(value, candidates[j - 1])) candidates[j] = candidates[j - 1], --j;
        candidates[j] = value;
    }
}

static void buildDisplaySignature(const DocumentSymbol& symbol, SignatureCandidate* candidate) {
    const char* signature = symbol.signature;
    uint32_t length = textLength(signature, kSymbolMaxSignatureBytes);
    uint32_t open = length;
    for (uint32_t i = 0; i < length; ++i) if (signature[i] == '(') { open = i; break; }
    uint32_t output = 0;
    if (symbol.qualifiedName[0] != '\0') appendRange(candidate->displaySignature, sizeof(candidate->displaySignature), output,
                                                       symbol.qualifiedName, 0, textLength(symbol.qualifiedName, sizeof(symbol.qualifiedName)));
    else appendRange(candidate->displaySignature, sizeof(candidate->displaySignature), output,
                     symbol.name, 0, textLength(symbol.name, sizeof(symbol.name)));
    if (open < length) appendRange(candidate->displaySignature, sizeof(candidate->displaySignature), output, signature, open, length - open);
    candidate->displaySignature[output] = '\0';
}

static void buildDetail(const SymbolDatabase* database, uint32_t symbolIndex, SignatureCandidate* candidate) {
    uint32_t output = 0;
    const char* role = SymbolDeclarationRoleName(candidate->declarationRole);
    appendRange(candidate->detailText, sizeof(candidate->detailText), output, role, 0, textLength(role, 64));
    const char* path = SymbolDatabaseDocumentPath(database, symbolIndex < SymbolDatabaseProjectSymbolCount(database) ?
                                                  SymbolDatabaseProjectSymbolAt(database, symbolIndex)->documentIndex : 0);
    if (path && path[0] != '\0') {
        appendChar(candidate->detailText, sizeof(candidate->detailText), output, ' ');
        appendRange(candidate->detailText, sizeof(candidate->detailText), output, path, 0, textLength(path, kMaxPathBytes));
    }
    candidate->detailText[output] = '\0';
}

static void updateActiveParameter(SignatureHelpSession* session) {
    session->activeParameterIndex = kInvalidIndex;
    const SignatureCandidate* candidate = SignatureHelpSessionSelected(session);
    if (!candidate || candidate->parameterParseFailed || session->context.parameterPositionApproximate) return;
    if (session->context.activeArgumentIndex < candidate->parameterCount) {
        session->activeParameterIndex = session->context.activeArgumentIndex;
        return;
    }
    if (candidate->parameterCount > 0 && session->parameters[candidate->parameterStart + candidate->parameterCount - 1].variadic)
        session->activeParameterIndex = candidate->parameterCount - 1;
}

static void setError(SignatureErrorCode* error, SignatureErrorCode value) { if (error) *error = value; }

} // namespace

const char* SignatureContextKindName(SignatureContextKind kind) {
    switch (kind) {
    case SignatureContextKind::FunctionCall: return "FunctionCall";
    case SignatureContextKind::QualifiedFunctionCall: return "QualifiedFunctionCall";
    case SignatureContextKind::MethodCallLexical: return "MethodCallLexical";
    case SignatureContextKind::ConstructorCall: return "ConstructorCall";
    case SignatureContextKind::FunctionPointerCall: return "FunctionPointerCall";
    default: return "Unsupported";
    }
}

const char* SignatureErrorName(SignatureErrorCode code) {
    switch (code) {
    case SignatureErrorCode::None: return "SIGNATURE_NONE";
    case SignatureErrorCode::NoProject: return "SIGNATURE_NO_PROJECT";
    case SignatureErrorCode::NoDocument: return "SIGNATURE_NO_DOCUMENT";
    case SignatureErrorCode::NoActiveCall: return "SIGNATURE_NO_ACTIVE_CALL";
    case SignatureErrorCode::UnsupportedContext: return "SIGNATURE_UNSUPPORTED_CONTEXT";
    case SignatureErrorCode::InComment: return "SIGNATURE_IN_COMMENT";
    case SignatureErrorCode::InString: return "SIGNATURE_IN_STRING";
    case SignatureErrorCode::InCharacter: return "SIGNATURE_IN_CHARACTER";
    case SignatureErrorCode::InRawString: return "SIGNATURE_IN_RAW_STRING";
    case SignatureErrorCode::InPreprocessor: return "SIGNATURE_IN_PREPROCESSOR";
    case SignatureErrorCode::ContextTooLarge: return "SIGNATURE_CONTEXT_TOO_LARGE";
    case SignatureErrorCode::NestingLimit: return "SIGNATURE_NESTING_LIMIT";
    case SignatureErrorCode::CallableTooLong: return "SIGNATURE_CALLABLE_TOO_LONG";
    case SignatureErrorCode::QualifierTooLong: return "SIGNATURE_QUALIFIER_TOO_LONG";
    case SignatureErrorCode::IndexNotReady: return "SIGNATURE_INDEX_NOT_READY";
    case SignatureErrorCode::ProjectStale: return "SIGNATURE_PROJECT_STALE";
    case SignatureErrorCode::DocumentStale: return "SIGNATURE_DOCUMENT_STALE";
    case SignatureErrorCode::SessionStale: return "SIGNATURE_SESSION_STALE";
    case SignatureErrorCode::NoCandidates: return "SIGNATURE_NO_CANDIDATES";
    case SignatureErrorCode::ResultsTruncated: return "SIGNATURE_RESULTS_TRUNCATED";
    case SignatureErrorCode::CandidateLimit: return "SIGNATURE_CANDIDATE_LIMIT";
    case SignatureErrorCode::ParseApproximate: return "SIGNATURE_PARSE_APPROXIMATE";
    case SignatureErrorCode::ParameterParseFailed: return "SIGNATURE_PARAMETER_PARSE_FAILED";
    case SignatureErrorCode::ReceiverUnresolved: return "SIGNATURE_RECEIVER_UNRESOLVED";
    default: return "SIGNATURE_INTERNAL";
    }
}

const char* SignatureStatusText(SignatureErrorCode code) {
    switch (code) {
    case SignatureErrorCode::NoProject: return "Open a project before using Signature Help.";
    case SignatureErrorCode::NoDocument: return "Focus the source editor before using Signature Help.";
    case SignatureErrorCode::NoActiveCall: return "No active function call at the caret.";
    case SignatureErrorCode::InComment: return "Signature Help is unavailable in a comment.";
    case SignatureErrorCode::InString: return "Signature Help is unavailable in a string literal.";
    case SignatureErrorCode::InCharacter: return "Signature Help is unavailable in a character literal.";
    case SignatureErrorCode::InRawString: return "Signature Help is unavailable in a raw string literal.";
    case SignatureErrorCode::InPreprocessor: return "Signature Help is unavailable in a preprocessor region.";
    case SignatureErrorCode::ContextTooLarge: return "Signature context is too distant.";
    case SignatureErrorCode::NestingLimit: return "Signature context nesting limit reached.";
    case SignatureErrorCode::IndexNotReady: return "Signature model is unavailable until syntax indexing is ready.";
    case SignatureErrorCode::NoCandidates: return "No stored signatures match this call.";
    case SignatureErrorCode::ResultsTruncated: return "Signature results truncated.";
    case SignatureErrorCode::ReceiverUnresolved: return "Receiver type unresolved; showing lexical method signatures.";
    case SignatureErrorCode::ParseApproximate: return "Parameter position is approximate.";
    case SignatureErrorCode::ParameterParseFailed: return "Parameter highlighting unavailable for this signature.";
    default: return "";
    }
}

uint64_t SignatureProjectId(const char* projectId) {
    uint64_t hash = 1469598103934665603ULL;
    if (!projectId) return hash;
    for (uint32_t i = 0; projectId[i] != '\0'; ++i) hash = (hash ^ static_cast<uint8_t>(projectId[i])) * 1099511628211ULL;
    return hash;
}

void SignatureHelpSessionInit(SignatureHelpSession* session,
                              SignatureCandidate* candidateStorage,
                              uint32_t candidateCapacity,
                              SignatureParameter* parameterStorage,
                              uint32_t parameterCapacity) {
    if (!session) return;
    *session = {};
    session->candidates = candidateStorage;
    session->parameters = parameterStorage;
    session->candidateCapacity = candidateCapacity;
    session->parameterCapacity = parameterCapacity;
    session->activeParameterIndex = kInvalidIndex;
}

bool SignatureExtractInvocationContext(const Document& document, uint64_t projectId,
                                       uint64_t projectGeneration, uint64_t sessionId,
                                       SignatureInvocationContext* output,
                                       SignatureErrorCode* error) {
    setError(error, SignatureErrorCode::None);
    if (!output) { setError(error, SignatureErrorCode::Internal); return false; }
    *output = {};
    output->sessionId = sessionId;
    output->projectId = projectId;
    output->projectGeneration = projectGeneration;
    output->documentId = document.documentId;
    output->documentGeneration = document.buffer.generation;
    output->caretByteOffset = document.buffer.caret;
    output->activeArgumentIndex = 0;
    output->receiverTypeResolved = false;
    if (!document.syntax.valid || document.syntax.generation != document.buffer.generation) {
        setError(error, SignatureErrorCode::IndexNotReady);
        return false;
    }
    if (document.buffer.caret > document.buffer.length) { setError(error, SignatureErrorCode::DocumentStale); return false; }
    if (inactiveIfZero(document, document.buffer.caret)) { setError(error, SignatureErrorCode::InPreprocessor); return false; }
    if (document.buffer.caret < document.buffer.length) {
        SyntaxTokenKind kind = SyntaxTokenKind::PlainText;
        syntaxKindAt(document, document.buffer.caret, &kind);
        if (kind == SyntaxTokenKind::Comment) { setError(error, SignatureErrorCode::InComment); return false; }
        if (kind == SyntaxTokenKind::StringLiteral) { setError(error, rawStringNear(document, document.buffer.caret) ? SignatureErrorCode::InRawString : SignatureErrorCode::InString); return false; }
        if (kind == SyntaxTokenKind::CharacterLiteral) { setError(error, SignatureErrorCode::InCharacter); return false; }
        if (kind == SyntaxTokenKind::Preprocessor) { setError(error, SignatureErrorCode::InPreprocessor); return false; }
    }
    if (document.buffer.caret > 0 && (document.buffer.caret == document.buffer.length || document.buffer.data[document.buffer.caret] == '\n')) {
        SyntaxTokenKind previous = SyntaxTokenKind::PlainText;
        syntaxKindAt(document, document.buffer.caret - 1, &previous);
        if (previous == SyntaxTokenKind::Comment) { setError(error, SignatureErrorCode::InComment); return false; }
    }
    uint32_t openParen = 0;
    uint32_t nesting = 0;
    bool tooLarge = false;
    bool nestingLimit = false;
    if (!findInvocationOpen(document, document.buffer.caret, &openParen, &nesting, &tooLarge, &nestingLimit)) {
        setError(error, nestingLimit ? SignatureErrorCode::NestingLimit : tooLarge ? SignatureErrorCode::ContextTooLarge : SignatureErrorCode::NoActiveCall);
        return false;
    }
    output->openParenByteOffset = openParen;
    output->nestingDepth = nesting;
    SignatureErrorCode extractionError = SignatureErrorCode::None;
    if (!extractCallable(document, openParen, output, &extractionError)) { setError(error, extractionError); return false; }
    if (!computeArgumentIndex(document, openParen, document.buffer.caret, &output->activeArgumentIndex,
                              &output->nestingDepth, &output->parameterPositionApproximate, error)) return false;
    return true;
}

bool SignatureHelpBuildSession(SignatureHelpSession* session, const Document& document,
                               uint64_t projectId, uint64_t projectGeneration,
                               const SymbolDatabase* database, SignatureErrorCode* error) {
    setError(error, SignatureErrorCode::None);
    if (!session || !session->candidates || !session->parameters || !database || session->candidateCapacity == 0) {
        setError(error, SignatureErrorCode::Internal); return false;
    }
    session->candidateCount = 0;
    session->parameterCount = 0;
    session->collectedCount = 0;
    session->active = false;
    session->truncated = false;
    session->contextStale = false;
    SignatureInvocationContext context = {};
    if (!SignatureExtractInvocationContext(document, projectId, projectGeneration, session->sessionId,
                                           &context, error)) return false;
    inferContainingScope(context, database, context.containingScope, sizeof(context.containingScope));
    const uint32_t symbolCount = SymbolDatabaseProjectSymbolCount(database);
    bool hasExactCase = false;
    bool hasConstructor = false;
    char exactQualifiedName[kSignatureMaxQualifierBytes + kSignatureMaxCallableBytes + 4] = {};
    if (context.hasExplicitQualifier) {
        uint32_t exactLength = 0;
        appendRange(exactQualifiedName, sizeof(exactQualifiedName), exactLength, context.explicitQualifier, 0,
                    textLength(context.explicitQualifier, sizeof(context.explicitQualifier)));
        appendRange(exactQualifiedName, sizeof(exactQualifiedName), exactLength, "::", 0, 2);
        appendRange(exactQualifiedName, sizeof(exactQualifiedName), exactLength, context.callableName, 0,
                    textLength(context.callableName, sizeof(context.callableName)));
    }
    bool exactQualifiedAvailable = false;
    for (uint32_t i = 0; i < symbolCount; ++i) {
        const ProjectSymbol* symbol = SymbolDatabaseProjectSymbolAt(database, i);
        if (!symbol || !supportedKind(symbol->symbol.kind)) continue;
        if (equalText(symbol->symbol.name, context.callableName, false)) hasExactCase = true;
        if (symbol->symbol.kind == SymbolKind::Constructor && equalText(symbol->symbol.name, context.callableName, false)) hasConstructor = true;
        if (context.hasExplicitQualifier && equalText(symbol->symbol.qualifiedName, exactQualifiedName, false) &&
            equalText(symbol->symbol.name, context.callableName, false)) exactQualifiedAvailable = true;
    }
    if (hasConstructor && context.kind == SignatureContextKind::FunctionCall) context.kind = SignatureContextKind::ConstructorCall;

    for (uint32_t index = 0; index < symbolCount; ++index) {
        const ProjectSymbol* projectSymbol = SymbolDatabaseProjectSymbolAt(database, index);
        if (!projectSymbol || !supportedKind(projectSymbol->symbol.kind)) continue;
        const DocumentSymbol& symbol = projectSymbol->symbol;
        if (!equalText(symbol.name, context.callableName, !hasExactCase)) continue;
        if (symbol.signature[0] == '\0') continue;
        ++session->collectedCount;
        if (session->collectedCount > kSignatureMaxCandidateCollection) { session->truncated = true; continue; }
        bool exactQualified = false;
        if (context.hasExplicitQualifier) {
            char fullName[kSignatureMaxQualifierBytes + kSignatureMaxCallableBytes + 4] = {};
            uint32_t fullLength = 0;
            appendRange(fullName, sizeof(fullName), fullLength, context.explicitQualifier, 0, textLength(context.explicitQualifier, sizeof(context.explicitQualifier)));
            appendRange(fullName, sizeof(fullName), fullLength, "::", 0, 2);
            appendRange(fullName, sizeof(fullName), fullLength, context.callableName, 0, textLength(context.callableName, sizeof(context.callableName)));
            exactQualified = equalText(symbol.qualifiedName, fullName, false);
        }
        if (context.hasExplicitQualifier && exactQualified == false && symbol.kind == SymbolKind::Function && symbol.container[0] == '\0') {
            // Keep the unqualified fallback, but it remains below the exact
            // qualified result through deterministic ranking.
        }
        if (context.kind == SignatureContextKind::MethodCallLexical && symbol.kind != SymbolKind::Method) continue;
        if (context.kind == SignatureContextKind::QualifiedFunctionCall && !exactQualified &&
            !equalText(symbol.container, context.explicitQualifier, false)) {
            // Qualified calls may fall back to same-name project symbols only
            // when the index has no exact qualified binding.
            if (exactQualifiedAvailable) continue;
        }
        if (session->candidateCount >= session->candidateCapacity || session->candidateCount >= kSignatureMaxRetainedCandidates) {
            session->truncated = true;
            continue;
        }
        SignatureCandidate candidate = {};
        copyText(candidate.callableName, sizeof(candidate.callableName), symbol.name);
        copyText(candidate.qualifiedName, sizeof(candidate.qualifiedName), symbol.qualifiedName);
        copyText(candidate.relativePath, sizeof(candidate.relativePath), SymbolDatabaseDocumentPath(database, projectSymbol->documentIndex));
        candidate.symbolKind = symbol.kind;
        candidate.declarationRole = symbol.declarationRole;
        candidate.line = symbol.location.line;
        candidate.column = symbol.location.column;
        candidate.byteOffset = symbol.location.identifierOffset;
        candidate.exactQualifiedMatch = exactQualified;
        candidate.sameDocument = symbol.location.documentId == context.documentId;
        candidate.sameScope = context.containingScope[0] != '\0' && equalText(symbol.container, context.containingScope, false);
        candidate.lexicallyAmbiguous = context.kind == SignatureContextKind::MethodCallLexical;
        candidate.rankScore = 0;
        if (candidate.exactQualifiedMatch) candidate.rankScore += 2000;
        if (hasExactCase) candidate.rankScore += 1500;
        if (candidate.sameScope) candidate.rankScore += 1000;
        if (context.hasExplicitQualifier && equalText(symbol.container, context.explicitQualifier, false)) candidate.rankScore += 800;
        if (candidate.sameDocument) candidate.rankScore += 600;
        candidate.rankScore += roleScore(candidate.declarationRole);
        if ((context.kind == SignatureContextKind::MethodCallLexical && symbol.kind == SymbolKind::Method) ||
            (context.kind == SignatureContextKind::ConstructorCall && symbol.kind == SymbolKind::Constructor) ||
            (context.kind == SignatureContextKind::FunctionCall && symbol.kind == SymbolKind::Function)) candidate.rankScore += 200;
        if (!hasExactCase) candidate.rankScore -= 100;
        if (candidate.lexicallyAmbiguous) candidate.rankScore -= 300;
        buildDisplaySignature(symbol, &candidate);
        candidate.candidateId = candidateId(candidate);
        parseCandidateParameters(session, &candidate);
        if (context.activeArgumentIndex < candidate.parameterCount ||
            (candidate.parameterCount > 0 && session->parameters[candidate.parameterStart + candidate.parameterCount - 1].variadic))
            candidate.rankScore += 150;
        else candidate.rankScore -= 50;
        buildDetail(database, index, &candidate);
        if (candidate.lexicallyAmbiguous) {
            uint32_t detailLength = textLength(candidate.detailText, sizeof(candidate.detailText));
            appendRange(candidate.detailText, sizeof(candidate.detailText), detailLength, " | receiver type unresolved", 0, 25);
        }
        bool merged = false;
        for (uint32_t existingIndex = 0; existingIndex < session->candidateCount; ++existingIndex) {
            SignatureCandidate& existing = session->candidates[existingIndex];
            if (existing.candidateId != candidate.candidateId || !normalizedSignatureEqual(existing, candidate)) continue;
            const bool sameLocation = existing.byteOffset == candidate.byteOffset &&
                equalText(existing.relativePath, candidate.relativePath, false);
            const bool existingDefinition = existing.declarationRole == SymbolDeclarationRole::Definition;
            const bool candidateDefinition = candidate.declarationRole == SymbolDeclarationRole::Definition;
            if (sameLocation || existingDefinition == candidateDefinition) {
                if (sameLocation && candidateDefinition && !existingDefinition) existing = candidate;
                merged = true;
                break;
            }
            if (candidateDefinition && !existingDefinition) existing = candidate;
            merged = true;
            break;
        }
        if (!merged) session->candidates[session->candidateCount++] = candidate;
    }
    if (session->candidateCount == 0) {
        session->context = context;
        setError(error, session->truncated ? SignatureErrorCode::ResultsTruncated : SignatureErrorCode::NoCandidates);
        return false;
    }
    insertionSort(session->candidates, session->candidateCount);
    session->context = context;
    session->selectedSignatureIndex = 0;
    session->active = true;
    updateActiveParameter(session);
    if (session->truncated) setError(error, SignatureErrorCode::ResultsTruncated);
    else if (context.parameterPositionApproximate) setError(error, SignatureErrorCode::ParseApproximate);
    else if (session->candidates[0].parameterParseFailed) setError(error, SignatureErrorCode::ParameterParseFailed);
    return true;
}

bool SignatureHelpSessionRefresh(SignatureHelpSession* session, const Document& document,
                                 uint64_t projectId, uint64_t projectGeneration,
                                 const SymbolDatabase* database, SignatureErrorCode* error) {
    if (!session) { setError(error, SignatureErrorCode::Internal); return false; }
    const uint64_t oldId = session->selectedSignatureIndex < session->candidateCount ?
        session->candidates[session->selectedSignatureIndex].candidateId : 0;
    if (!SignatureHelpBuildSession(session, document, projectId, projectGeneration, database, error)) {
        session->contextStale = true;
        return false;
    }
    for (uint32_t i = 0; i < session->candidateCount; ++i) if (session->candidates[i].candidateId == oldId) {
        session->selectedSignatureIndex = i;
        break;
    }
    updateActiveParameter(session);
    return true;
}

const SignatureCandidate* SignatureHelpSessionSelected(const SignatureHelpSession* session) {
    if (!session || !session->active || session->selectedSignatureIndex >= session->candidateCount) return nullptr;
    return &session->candidates[session->selectedSignatureIndex];
}

const SignatureParameter* SignatureHelpSessionActiveParameter(const SignatureHelpSession* session) {
    const SignatureCandidate* candidate = SignatureHelpSessionSelected(session);
    if (!session || !candidate || session->activeParameterIndex == kInvalidIndex ||
        session->activeParameterIndex >= candidate->parameterCount) return nullptr;
    return &session->parameters[candidate->parameterStart + session->activeParameterIndex];
}

bool SignatureHelpSessionMove(SignatureHelpSession* session, int32_t delta) {
    if (!session || !session->active || session->candidateCount == 0 || delta == 0) return false;
    int64_t selected = static_cast<int64_t>(session->selectedSignatureIndex) + delta;
    if (selected < 0) selected = 0;
    if (selected >= static_cast<int64_t>(session->candidateCount)) selected = session->candidateCount - 1;
    session->selectedSignatureIndex = static_cast<uint32_t>(selected);
    updateActiveParameter(session);
    return true;
}

bool SignatureHelpSessionPage(SignatureHelpSession* session, int32_t direction) {
    return SignatureHelpSessionMove(session, direction < 0 ? -8 : 8);
}

bool SignatureHelpSessionHome(SignatureHelpSession* session) {
    if (!session || !session->active) return false;
    session->selectedSignatureIndex = 0;
    updateActiveParameter(session);
    return true;
}

bool SignatureHelpSessionEnd(SignatureHelpSession* session) {
    if (!session || !session->active) return false;
    session->selectedSignatureIndex = session->candidateCount - 1;
    updateActiveParameter(session);
    return true;
}

void SignatureHelpSessionDismiss(SignatureHelpSession* session) {
    if (!session) return;
    session->active = false;
    session->contextStale = false;
    session->activeParameterIndex = kInvalidIndex;
}

bool SignatureHelpSessionIsCurrent(const SignatureHelpSession* session, const Document& document,
                                   uint64_t projectId, uint64_t projectGeneration,
                                   SignatureErrorCode* error) {
    setError(error, SignatureErrorCode::None);
    if (!session || !session->active) { setError(error, SignatureErrorCode::SessionStale); return false; }
    if (session->context.projectId != projectId || session->context.projectGeneration != projectGeneration) {
        setError(error, SignatureErrorCode::ProjectStale); return false;
    }
    if (session->context.documentId != document.documentId) { setError(error, SignatureErrorCode::DocumentStale); return false; }
    if (session->context.documentGeneration != document.buffer.generation) { setError(error, SignatureErrorCode::DocumentStale); return false; }
    return true;
}

} // namespace developer_studio
} // namespace guidexos
