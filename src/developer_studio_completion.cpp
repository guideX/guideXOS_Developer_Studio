#include "developer_studio_completion.h"

#include "developer_studio_syntax.h"

namespace guidexos {
namespace developer_studio {
namespace {

static bool isIdentifierStart(char value) {
    return value == '_' || (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z');
}

static bool isIdentifierPart(char value) {
    return isIdentifierStart(value) || (value >= '0' && value <= '9');
}

static char lowerAscii(char value) {
    return value >= 'A' && value <= 'Z' ? static_cast<char>(value + ('a' - 'A')) : value;
}

static uint32_t textLength(const char* text, uint32_t limit) {
    if (!text) return 0;
    uint32_t length = 0;
    while (length < limit && text[length] != '\0') ++length;
    return length;
}

static void clearText(char* output, uint32_t capacity) {
    if (output && capacity > 0) output[0] = '\0';
}

static void copyText(char* output, uint32_t capacity, const char* input) {
    if (!output || capacity == 0) return;
    uint32_t i = 0;
    if (input) while (i + 1 < capacity && input[i] != '\0') {
        output[i] = input[i];
        ++i;
    }
    output[i] = '\0';
}

static void copyRange(char* output, uint32_t capacity, const char* input,
                      uint32_t start, uint32_t length) {
    if (!output || capacity == 0) return;
    uint32_t i = 0;
    while (input && i + 1 < capacity && i < length) {
        output[i] = input[start + i];
        ++i;
    }
    output[i] = '\0';
}

static void appendText(char* output, uint32_t capacity, const char* input) {
    if (!output || capacity == 0 || !input) return;
    uint32_t offset = textLength(output, capacity);
    uint32_t i = 0;
    while (offset + 1 < capacity && input[i] != '\0') output[offset++] = input[i++];
    output[offset] = '\0';
}

static void appendUnsigned(char* output, uint32_t capacity, uint32_t value) {
    char digits[16] = {};
    uint32_t count = 0;
    do {
        digits[count++] = static_cast<char>('0' + (value % 10u));
        value /= 10u;
    } while (value != 0 && count < sizeof(digits));
    while (count > 0) {
        char valueText[2] = { digits[--count], '\0' };
        appendText(output, capacity, valueText);
    }
}

static bool equalText(const char* left, const char* right, bool foldCase) {
    if (!left || !right) return left == right;
    uint32_t i = 0;
    while (left[i] != '\0' && right[i] != '\0') {
        const char a = foldCase ? lowerAscii(left[i]) : left[i];
        const char b = foldCase ? lowerAscii(right[i]) : right[i];
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
        const char a = foldCase ? lowerAscii(text[i]) : text[i];
        const char b = foldCase ? lowerAscii(prefix[i]) : prefix[i];
        if (a != b) return false;
        ++i;
    }
    return true;
}

static bool containsText(const char* text, const char* query, bool foldCase) {
    const uint32_t textSize = textLength(text, kCompletionMaxQualifiedDisplayBytes + 1);
    const uint32_t querySize = textLength(query, kCompletionMaxPrefixBytes + 1);
    if (querySize == 0) return true;
    if (querySize > textSize) return false;
    for (uint32_t start = 0; start + querySize <= textSize; ++start) {
        bool matched = true;
        for (uint32_t i = 0; i < querySize; ++i) {
            const char a = foldCase ? lowerAscii(text[start + i]) : text[start + i];
            const char b = foldCase ? lowerAscii(query[i]) : query[i];
            if (a != b) { matched = false; break; }
        }
        if (matched) return true;
    }
    return false;
}

static int32_t asciiCompare(const char* left, const char* right) {
    if (!left || !right) return left == right ? 0 : (left ? 1 : -1);
    uint32_t i = 0;
    while (left[i] != '\0' && right[i] != '\0') {
        if (left[i] < right[i]) return -1;
        if (left[i] > right[i]) return 1;
        ++i;
    }
    if (left[i] == right[i]) return 0;
    return left[i] == '\0' ? -1 : 1;
}

static bool isWhitespace(char value) {
    return value == ' ' || value == '\t' || value == '\r' || value == '\n';
}

static uint64_t hashText(const char* text) {
    uint64_t hash = 1469598103934665603ull;
    if (!text) return hash;
    for (uint32_t i = 0; text[i] != '\0'; ++i) {
        hash ^= static_cast<uint64_t>(static_cast<unsigned char>(text[i]));
        hash *= 1099511628211ull;
    }
    return hash;
}

static uint32_t lineForOffset(const Document& document, uint32_t offset) {
    const uint32_t count = TextBufferLineCount(&document.buffer);
    for (uint32_t line = 0; line < count; ++line) {
        if (offset <= TextBufferLineEnd(&document.buffer, line)) return line;
    }
    return count == 0 ? 0 : count - 1;
}

static SyntaxTokenKind syntaxKindAt(const Document& document, uint32_t offset) {
    if (!document.syntax.valid || offset > document.buffer.length) return SyntaxTokenKind::PlainText;
    const uint32_t line = lineForOffset(document, offset);
    const uint32_t lineStart = TextBufferLineStart(&document.buffer, line);
    const uint32_t lineEnd = TextBufferLineEnd(&document.buffer, line);
    uint32_t spanCount = 0;
    const SyntaxTokenSpan* spans = SyntaxCacheLineSpans(&document.syntax, line, &spanCount);
    for (uint32_t i = 0; i < spanCount; ++i) {
        const SyntaxTokenSpan& span = spans[i];
        const uint32_t start = lineStart + span.start;
        const uint32_t end = start + span.length;
        if (offset >= start && offset < end) return span.kind;
        if (offset == end && end == lineEnd && span.kind == SyntaxTokenKind::Comment)
            return span.kind;
        if (offset == end && end == lineEnd && span.kind == SyntaxTokenKind::Preprocessor)
            return span.kind;
        if (offset == end && end == lineEnd &&
            (span.kind == SyntaxTokenKind::StringLiteral || span.kind == SyntaxTokenKind::CharacterLiteral) &&
            (end == start || (document.buffer.data[end - 1] != '"' && document.buffer.data[end - 1] != '\'')))
            return span.kind;
    }
    return SyntaxTokenKind::PlainText;
}

static bool lineDirective(const char* text, uint32_t start, uint32_t end,
                          char* word, uint32_t wordCapacity, bool* zeroIf) {
    if (!text || !word || wordCapacity == 0) return false;
    word[0] = '\0';
    uint32_t cursor = start;
    while (cursor < end && (text[cursor] == ' ' || text[cursor] == '\t' || text[cursor] == '\r')) ++cursor;
    if (cursor >= end || text[cursor] != '#') return false;
    ++cursor;
    while (cursor < end && (text[cursor] == ' ' || text[cursor] == '\t')) ++cursor;
    uint32_t wordLength = 0;
    while (cursor < end && isIdentifierPart(text[cursor])) {
        if (wordLength + 1 < wordCapacity) word[wordLength++] = text[cursor];
        ++cursor;
    }
    word[wordLength] = '\0';
    while (cursor < end && isWhitespace(text[cursor])) ++cursor;
    if (zeroIf) *zeroIf = cursor < end && text[cursor] == '0' &&
        (cursor + 1 == end || !isIdentifierPart(text[cursor + 1]));
    return wordLength != 0;
}

static bool lineInactiveAt(const Document& document, uint32_t targetLine) {
    bool inactive[64] = {};
    bool parentInactive[64] = {};
    uint32_t depth = 0;
    const uint32_t lineCount = TextBufferLineCount(&document.buffer);
    if (targetLine >= lineCount) targetLine = lineCount == 0 ? 0 : lineCount - 1;
    for (uint32_t line = 0; line <= targetLine && line < lineCount; ++line) {
        const uint32_t start = TextBufferLineStart(&document.buffer, line);
        const uint32_t end = TextBufferLineEnd(&document.buffer, line);
        char word[24] = {};
        bool zeroIf = false;
        if (!lineDirective(document.buffer.data, start, end, word, sizeof(word), &zeroIf)) continue;
        if (equalText(word, "if", false) || equalText(word, "ifdef", false) || equalText(word, "ifndef", false)) {
            if (depth < 64) {
                parentInactive[depth] = depth > 0 && inactive[depth - 1];
                inactive[depth] = parentInactive[depth] || (equalText(word, "if", false) && zeroIf);
                ++depth;
            }
        } else if (equalText(word, "else", false) || equalText(word, "elif", false)) {
            if (depth > 0) inactive[depth - 1] = parentInactive[depth - 1] ||
                (equalText(word, "elif", false) ? zeroIf : !inactive[depth - 1]);
        } else if (equalText(word, "endif", false) && depth > 0) {
            --depth;
        }
    }
    return depth > 0 && inactive[depth - 1];
}

static bool isValidIdentifierRange(const char* text, uint32_t start, uint32_t end) {
    if (!text || end <= start || !isIdentifierStart(text[start])) return false;
    for (uint32_t i = start + 1; i < end; ++i) if (!isIdentifierPart(text[i])) return false;
    return true;
}

static bool appendScopeName(char* output, uint32_t capacity, const char* name) {
    if (!output || !name || name[0] == '\0') return true;
    if (output[0] != '\0') appendText(output, capacity, "::");
    const uint32_t oldLength = textLength(output, capacity);
    if (oldLength >= capacity) return false;
    appendText(output, capacity, name);
    return textLength(output, capacity) > oldLength;
}

static void extractLexicalScope(const Document& document, uint32_t caret, char* output, uint32_t capacity) {
    clearText(output, capacity);
    if (!output || capacity == 0) return;
    const uint32_t start = caret > kCompletionMaxContextScanBytes
        ? caret - kCompletionMaxContextScanBytes : 0;
    char names[32][kMaxNameBytes + 1] = {};
    uint32_t depth = 0;
    for (uint32_t offset = start; offset < caret; ++offset) {
        const char value = document.buffer.data[offset];
        const SyntaxTokenKind tokenKind = syntaxKindAt(document, offset);
        if (tokenKind == SyntaxTokenKind::Comment || tokenKind == SyntaxTokenKind::StringLiteral ||
            tokenKind == SyntaxTokenKind::CharacterLiteral || tokenKind == SyntaxTokenKind::Preprocessor) continue;
        if (value == '}') {
            if (depth > 0) --depth;
            continue;
        }
        if (value != '{' || depth >= 32) continue;
        uint32_t cursor = offset;
        while (cursor > start && isWhitespace(document.buffer.data[cursor - 1])) --cursor;
        const uint32_t nameEnd = cursor;
        while (cursor > start && isIdentifierPart(document.buffer.data[cursor - 1])) --cursor;
        char name[kMaxNameBytes + 1] = {};
        if (nameEnd > cursor && isIdentifierStart(document.buffer.data[cursor]))
            copyRange(name, sizeof(name), document.buffer.data, cursor, nameEnd - cursor);
        names[depth][0] = '\0';
        if (name[0] != '\0') {
            uint32_t keywordEnd = cursor;
            while (keywordEnd > start && isWhitespace(document.buffer.data[keywordEnd - 1])) --keywordEnd;
            uint32_t keywordStart = keywordEnd;
            while (keywordStart > start && isIdentifierPart(document.buffer.data[keywordStart - 1])) --keywordStart;
            char keyword[24] = {};
            if (keywordEnd > keywordStart) copyRange(keyword, sizeof(keyword), document.buffer.data,
                                                       keywordStart, keywordEnd - keywordStart);
            if (equalText(keyword, "namespace", false) || equalText(keyword, "class", false) ||
                equalText(keyword, "struct", false) || equalText(keyword, "union", false) ||
                equalText(keyword, "enum", false)) copyText(names[depth], sizeof(names[depth]), name);
        }
        ++depth;
    }
    for (uint32_t i = 0; i < depth; ++i) appendScopeName(output, capacity, names[i]);
}

static void inferScopeFromSymbols(const SymbolDatabase* database, const Document& document,
                                  CompletionContext* context) {
    if (!database || !context) return;
    uint32_t bestDepth = 0;
    uint32_t bestOffset = 0;
    const uint32_t count = SymbolDatabaseProjectSymbolCount(database);
    for (uint32_t i = 0; i < count; ++i) {
        const ProjectSymbol* projectSymbol = SymbolDatabaseProjectSymbolAt(database, i);
        if (!projectSymbol || projectSymbol->symbol.location.documentId != document.documentId) continue;
        const SymbolKind kind = projectSymbol->symbol.kind;
        if (kind != SymbolKind::Namespace && kind != SymbolKind::Class && kind != SymbolKind::Struct &&
            kind != SymbolKind::Union && kind != SymbolKind::Enum) continue;
        if (projectSymbol->symbol.location.identifierOffset > context->caretByteOffset) continue;
        const uint32_t depth = projectSymbol->symbol.depth;
        if (depth < bestDepth || (depth == bestDepth && projectSymbol->symbol.location.identifierOffset < bestOffset)) continue;
        if (projectSymbol->symbol.qualifiedName[0] == '\0') continue;
        copyText(context->containingScope, sizeof(context->containingScope), projectSymbol->symbol.qualifiedName);
        bestDepth = depth;
        bestOffset = projectSymbol->symbol.location.identifierOffset;
    }
}

static bool precedingMemberAccess(const Document& document, uint32_t offset) {
    if (offset > 0 && document.buffer.data[offset - 1] == '.') return true;
    return offset > 1 && document.buffer.data[offset - 1] == '>' && document.buffer.data[offset - 2] == '-';
}

static bool explicitQualifierAt(const Document& document, uint32_t replacementStart,
                                 char* output, uint32_t capacity, CompletionErrorCode* error) {
    clearText(output, capacity);
    if (replacementStart < 2 || document.buffer.data[replacementStart - 2] != ':' ||
        document.buffer.data[replacementStart - 1] != ':') return false;
    uint32_t start = replacementStart - 2;
    while (start > 0 && (isIdentifierPart(document.buffer.data[start - 1]) || document.buffer.data[start - 1] == ':')) --start;
    const uint32_t length = replacementStart - 2 - start;
    if (length == 0) return false;
    if (length >= capacity) {
        if (error) *error = CompletionErrorCode::QualifierTooLong;
        return false;
    }
    for (uint32_t i = 0; i < length; ++i) {
        const char value = document.buffer.data[start + i];
        if (!(isIdentifierPart(value) || value == ':')) return false;
        output[i] = value;
    }
    output[length] = '\0';
    return true;
}

static uint32_t matchingTier(const char* name, const char* prefix,
                             bool* exactCase, bool* insensitivePrefix, bool* substring) {
    if (exactCase) *exactCase = false;
    if (insensitivePrefix) *insensitivePrefix = false;
    if (substring) *substring = false;
    if (!name || !prefix) return 0;
    if (prefix[0] == '\0') return 1;
    if (equalText(name, prefix, false)) {
        if (exactCase) *exactCase = true;
        return 5;
    }
    if (startsWith(name, prefix, false)) {
        if (exactCase) *exactCase = true;
        return 4;
    }
    if (startsWith(name, prefix, true)) {
        if (insensitivePrefix) *insensitivePrefix = true;
        return 3;
    }
    if (containsText(name, prefix, false)) {
        if (substring) *substring = true;
        return 2;
    }
    if (containsText(name, prefix, true)) {
        if (substring) *substring = true;
        return 1;
    }
    return 0;
}

static int32_t matchScore(uint32_t tier) {
    switch (tier) {
    case 5: return 2000;
    case 4: return 1500;
    case 3: return 1100;
    case 2: return 700;
    case 1: return 500;
    default: return 0;
    }
}

static int32_t sourceScore(CompletionCandidateSource source) {
    switch (source) {
    case CompletionCandidateSource::CurrentScope: return 900;
    case CompletionCandidateSource::CurrentDocument: return 600;
    case CompletionCandidateSource::ProjectIndex: return 400;
    case CompletionCandidateSource::DocumentWordSet: return 250;
    case CompletionCandidateSource::KeywordSet: return 150;
    default: return 0;
    }
}

static int32_t sourcePriority(CompletionCandidateSource source) {
    switch (source) {
    case CompletionCandidateSource::CurrentScope: return 5;
    case CompletionCandidateSource::CurrentDocument: return 4;
    case CompletionCandidateSource::ProjectIndex: return 3;
    case CompletionCandidateSource::KeywordSet: return 2;
    case CompletionCandidateSource::DocumentWordSet: return 1;
    default: return 0;
    }
}

static int32_t kindPriority(CompletionCandidateKind kind) {
    switch (kind) {
    case CompletionCandidateKind::Function:
    case CompletionCandidateKind::Method: return 9;
    case CompletionCandidateKind::Class: return 8;
    case CompletionCandidateKind::Struct:
    case CompletionCandidateKind::Union:
    case CompletionCandidateKind::Enum: return 7;
    case CompletionCandidateKind::Namespace: return 6;
    case CompletionCandidateKind::Typedef:
    case CompletionCandidateKind::UsingAlias: return 5;
    case CompletionCandidateKind::Variable:
    case CompletionCandidateKind::StaticVariable:
    case CompletionCandidateKind::Member: return 4;
    case CompletionCandidateKind::Constructor: return 3;
    case CompletionCandidateKind::DocumentWord: return 2;
    case CompletionCandidateKind::Keyword: return 1;
    default: return 0;
    }
}

static CompletionCandidateKind completionKind(SymbolKind kind) {
    switch (kind) {
    case SymbolKind::Namespace: return CompletionCandidateKind::Namespace;
    case SymbolKind::Class: return CompletionCandidateKind::Class;
    case SymbolKind::Struct: return CompletionCandidateKind::Struct;
    case SymbolKind::Union: return CompletionCandidateKind::Union;
    case SymbolKind::Enum: return CompletionCandidateKind::Enum;
    case SymbolKind::Function: return CompletionCandidateKind::Function;
    case SymbolKind::Method: return CompletionCandidateKind::Method;
    case SymbolKind::Constructor: return CompletionCandidateKind::Constructor;
    case SymbolKind::Destructor: return CompletionCandidateKind::Destructor;
    case SymbolKind::GlobalVariable: return CompletionCandidateKind::Variable;
    case SymbolKind::StaticVariable: return CompletionCandidateKind::StaticVariable;
    case SymbolKind::Typedef: return CompletionCandidateKind::Typedef;
    case SymbolKind::UsingAlias: return CompletionCandidateKind::UsingAlias;
    default: return CompletionCandidateKind::Unknown;
    }
}

static bool scopeMatches(const char* scope, const char* container) {
    if (!scope || !container) return false;
    if (container[0] == '\0') return true;
    if (equalText(scope, container, false)) return true;
    const uint32_t length = textLength(container, kCompletionMaxScopeBytes);
    if (length + 1 >= kCompletionMaxScopeBytes + 1) return false;
    return startsWith(scope, container, false) && scope[length] == ':' && scope[length + 1] == ':';
}

static bool qualifierMatches(const CompletionContext& context, const DocumentSymbol& symbol) {
    if (!context.hasExplicitQualifier) return true;
    if (equalText(context.explicitQualifier, symbol.container, false)) return true;
    char parent[kCompletionMaxQualifiedDisplayBytes + 1] = {};
    copyText(parent, sizeof(parent), context.explicitQualifier);
    appendText(parent, sizeof(parent), "::");
    return startsWith(symbol.qualifiedName, parent, false);
}

static void makeDetail(CompletionCandidate* candidate) {
    if (!candidate) return;
    clearText(candidate->detailText, sizeof(candidate->detailText));
    appendText(candidate->detailText, sizeof(candidate->detailText), CompletionCandidateKindPrefix(candidate->kind));
    if (candidate->lexicallyAmbiguous) appendText(candidate->detailText, sizeof(candidate->detailText), "  Lexical");
    else if (candidate->qualifiedName[0] != '\0') {
        appendText(candidate->detailText, sizeof(candidate->detailText), "  ");
        appendText(candidate->detailText, sizeof(candidate->detailText), candidate->qualifiedName);
    }
}

static void makeDisplay(CompletionCandidate* candidate) {
    if (!candidate) return;
    clearText(candidate->displayText, sizeof(candidate->displayText));
    appendText(candidate->displayText, sizeof(candidate->displayText), candidate->insertionText);
    if (candidate->signature[0] != '\0') {
        appendText(candidate->displayText, sizeof(candidate->displayText), "  ");
        appendText(candidate->displayText, sizeof(candidate->displayText), candidate->signature);
    }
    if (candidate->overloadCount > 1) {
        appendText(candidate->displayText, sizeof(candidate->displayText), "  (");
        appendUnsigned(candidate->displayText, sizeof(candidate->displayText), candidate->overloadCount);
        appendText(candidate->displayText, sizeof(candidate->displayText), " overloads)");
    }
}

static bool betterCandidate(const CompletionCandidate& left, const CompletionCandidate& right) {
    if (left.rankScore != right.rankScore) return left.rankScore > right.rankScore;
    const int32_t leftSource = sourcePriority(left.source);
    const int32_t rightSource = sourcePriority(right.source);
    if (leftSource != rightSource) return leftSource > rightSource;
    const int32_t leftKind = kindPriority(left.kind);
    const int32_t rightKind = kindPriority(right.kind);
    if (leftKind != rightKind) return leftKind > rightKind;
    int32_t compare = asciiCompare(left.insertionText, right.insertionText);
    if (compare != 0) return compare < 0;
    compare = asciiCompare(left.qualifiedName, right.qualifiedName);
    if (compare != 0) return compare < 0;
    compare = asciiCompare(left.signature, right.signature);
    if (compare != 0) return compare < 0;
    compare = asciiCompare(left.relativePath, right.relativePath);
    if (compare != 0) return compare < 0;
    if (left.line != right.line) return left.line < right.line;
    if (left.column != right.column) return left.column < right.column;
    return left.candidateId < right.candidateId;
}

static void sortCandidates(CompletionSession* session) {
    if (!session) return;
    for (uint32_t i = 1; i < session->candidateCount; ++i) {
        CompletionCandidate value = session->candidates[i];
        uint32_t cursor = i;
        while (cursor > 0 && betterCandidate(value, session->candidates[cursor - 1])) {
            session->candidates[cursor] = session->candidates[cursor - 1];
            --cursor;
        }
        session->candidates[cursor] = value;
    }
}

static void addCandidate(CompletionSession* session, const CompletionCandidate& candidate,
                         uint32_t collectionLimit) {
    if (!session || !session->candidates || session->candidateCapacity == 0) return;
    if (session->collectedCount >= collectionLimit) {
        session->truncated = true;
        return;
    }
    ++session->collectedCount;
    for (uint32_t i = 0; i < session->candidateCount; ++i) {
        CompletionCandidate& existing = session->candidates[i];
        if (!equalText(existing.insertionText, candidate.insertionText, false)) continue;
        const bool callable = kindPriority(existing.kind) >= 9 || kindPriority(candidate.kind) >= 9 ||
            existing.kind == CompletionCandidateKind::Constructor;
        const uint32_t overloadCount = existing.overloadCount + (callable ? 1u : 0u);
        const bool fromCurrentScope = existing.fromCurrentScope || candidate.fromCurrentScope;
        const bool fromCurrentDocument = existing.fromCurrentDocument || candidate.fromCurrentDocument;
        const bool lexicallyAmbiguous = existing.lexicallyAmbiguous || candidate.lexicallyAmbiguous;
        if (sourcePriority(candidate.source) > sourcePriority(existing.source)) existing.source = candidate.source;
        if (betterCandidate(candidate, existing)) existing = candidate;
        existing.fromCurrentScope = fromCurrentScope;
        existing.fromCurrentDocument = fromCurrentDocument;
        existing.lexicallyAmbiguous = lexicallyAmbiguous;
        if (callable) existing.overloadCount = overloadCount;
        makeDetail(&existing);
        makeDisplay(&existing);
        return;
    }
    CompletionCandidate retained = candidate;
    if (retained.overloadCount == 0) retained.overloadCount = 1;
    makeDetail(&retained);
    makeDisplay(&retained);
    if (session->candidateCount < session->candidateCapacity &&
        session->candidateCount < kCompletionMaxRetainedCandidates) {
        session->candidates[session->candidateCount++] = retained;
        return;
    }
    uint32_t worst = 0;
    for (uint32_t i = 1; i < session->candidateCount; ++i)
        if (betterCandidate(session->candidates[worst], session->candidates[i])) worst = i;
    if (betterCandidate(retained, session->candidates[worst])) session->candidates[worst] = retained;
    else session->truncated = true;
}

static void candidateFromWord(CompletionCandidate* output, const DocumentWordEntry& entry,
                              const CompletionContext& context) {
    if (!output) return;
    *output = CompletionCandidate();
    copyText(output->insertionText, sizeof(output->insertionText), entry.word);
    copyText(output->qualifiedName, sizeof(output->qualifiedName), entry.word);
    output->candidateId = hashText(entry.word);
    output->kind = CompletionCandidateKind::DocumentWord;
    output->source = CompletionCandidateSource::DocumentWordSet;
    output->fromCurrentDocument = true;
    output->rankScore = matchScore(matchingTier(entry.word, context.prefix, &output->exactCasePrefix,
                                                &output->caseInsensitivePrefix, &output->substringMatch));
    output->rankScore += sourceScore(output->source);
    output->rankScore += entry.occurrenceCount > 75 ? 75 : static_cast<int32_t>(entry.occurrenceCount);
    output->line = 0;
    output->column = 0;
    makeDetail(output);
    makeDisplay(output);
}

static bool symbolCandidate(const CompletionContext& context, const Document& document,
                            const SymbolDatabase* database, const ProjectSymbol& projectSymbol,
                            CompletionCandidate* output) {
    if (!output) return false;
    const DocumentSymbol& symbol = projectSymbol.symbol;
    if (symbol.name[0] == '\0' || symbol.kind == SymbolKind::Destructor || !qualifierMatches(context, symbol)) return false;
    if (context.kind == CompletionContextKind::MemberAccessLexical &&
        symbol.kind != SymbolKind::Method && symbol.kind != SymbolKind::Function &&
        symbol.kind != SymbolKind::Constructor && symbol.kind != SymbolKind::Class &&
        symbol.kind != SymbolKind::Struct && symbol.kind != SymbolKind::Union &&
        symbol.kind != SymbolKind::Enum && symbol.kind != SymbolKind::Typedef &&
        symbol.kind != SymbolKind::UsingAlias && symbol.kind != SymbolKind::GlobalVariable &&
        symbol.kind != SymbolKind::StaticVariable) return false;
    *output = CompletionCandidate();
    copyText(output->insertionText, sizeof(output->insertionText), symbol.name);
    copyText(output->qualifiedName, sizeof(output->qualifiedName), symbol.qualifiedName);
    copyText(output->signature, sizeof(output->signature), symbol.signature);
    copyText(output->relativePath, sizeof(output->relativePath), SymbolDatabaseDocumentPath(database, projectSymbol.documentIndex));
    output->candidateId = hashText(symbol.qualifiedName) ^ hashText(symbol.signature) ^ symbol.location.identifierOffset;
    output->kind = completionKind(symbol.kind);
    output->fromCurrentDocument = symbol.location.documentId == document.documentId;
    output->fromCurrentScope = scopeMatches(context.containingScope, symbol.container);
    output->source = output->fromCurrentScope ? CompletionCandidateSource::CurrentScope :
        (output->fromCurrentDocument ? CompletionCandidateSource::CurrentDocument : CompletionCandidateSource::ProjectIndex);
    output->lexicallyAmbiguous = context.kind == CompletionContextKind::MemberAccessLexical;
    output->rankScore = matchScore(matchingTier(symbol.name, context.prefix, &output->exactCasePrefix,
                                                &output->caseInsensitivePrefix, &output->substringMatch));
    output->rankScore += sourceScore(output->source);
    if (context.hasExplicitQualifier && equalText(context.explicitQualifier, symbol.container, false)) output->rankScore += 700;
    if (output->fromCurrentDocument) output->rankScore += 0;
    if (symbol.declarationRole == SymbolDeclarationRole::Definition) output->rankScore += 150;
    if (output->exactCasePrefix) output->rankScore += 200;
    if (output->lexicallyAmbiguous) output->rankScore -= 200;
    output->line = symbol.location.line;
    output->column = symbol.location.column;
    return output->rankScore > 0 || context.prefix[0] == '\0';
}

static void addKeywords(CompletionSession* session, const Document& document,
                        uint32_t collectionLimit) {
    const SyntaxLanguage language = document.syntax.language;
    const uint32_t count = SyntaxKeywordCount(language);
    for (uint32_t i = 0; i < count; ++i) {
        const char* keyword = SyntaxKeywordAt(language, i);
        if (!keyword || keyword[0] == '\0') continue;
        CompletionCandidate candidate = {};
        copyText(candidate.insertionText, sizeof(candidate.insertionText), keyword);
        copyText(candidate.qualifiedName, sizeof(candidate.qualifiedName), keyword);
        candidate.candidateId = hashText(keyword);
        candidate.kind = CompletionCandidateKind::Keyword;
        candidate.source = CompletionCandidateSource::KeywordSet;
        candidate.rankScore = matchScore(matchingTier(keyword, session->context.prefix,
                                                      &candidate.exactCasePrefix,
                                                      &candidate.caseInsensitivePrefix,
                                                      &candidate.substringMatch)) + sourceScore(candidate.source);
        if (candidate.rankScore > 0 || session->context.prefix[0] == '\0') addCandidate(session, candidate, collectionLimit);
    }
}

static void addSymbols(CompletionSession* session, const Document& document,
                       const SymbolDatabase* database, uint32_t collectionLimit, bool highPriority) {
    if (!session || !database) return;
    const uint32_t count = SymbolDatabaseProjectSymbolCount(database);
    for (uint32_t i = 0; i < count; ++i) {
        const ProjectSymbol* symbol = SymbolDatabaseProjectSymbolAt(database, i);
        if (!symbol) continue;
        const bool currentDocument = symbol->symbol.location.documentId == document.documentId;
        const bool currentScope = scopeMatches(session->context.containingScope, symbol->symbol.container);
        const bool explicitMatch = session->context.hasExplicitQualifier && qualifierMatches(session->context, symbol->symbol);
        if (highPriority && !currentDocument && !currentScope && !explicitMatch) continue;
        if (!highPriority && (currentDocument || currentScope || explicitMatch)) continue;
        CompletionCandidate candidate = {};
        if (symbolCandidate(session->context, document, database, *symbol, &candidate))
            addCandidate(session, candidate, collectionLimit);
    }
}

static void addWords(CompletionSession* session, const DocumentWordCache* cache, uint32_t collectionLimit) {
    if (!session || !cache || !cache->valid) return;
    for (uint32_t i = 0; i < cache->count; ++i) {
        const DocumentWordEntry* entry = DocumentWordCacheAt(cache, i);
        if (!entry) continue;
        CompletionCandidate candidate = {};
        candidateFromWord(&candidate, *entry, session->context);
        if (candidate.rankScore >= sourceScore(candidate.source) || session->context.prefix[0] == '\0')
            addCandidate(session, candidate, collectionLimit);
    }
}

static void rebuildDisplay(CompletionSession* session) {
    if (!session) return;
    for (uint32_t i = 0; i < session->candidateCount; ++i) {
        makeDetail(&session->candidates[i]);
        makeDisplay(&session->candidates[i]);
    }
}

} // namespace

const char* CompletionCandidateKindName(CompletionCandidateKind kind) {
    switch (kind) {
    case CompletionCandidateKind::Keyword: return "Keyword";
    case CompletionCandidateKind::Namespace: return "Namespace";
    case CompletionCandidateKind::Class: return "Class";
    case CompletionCandidateKind::Struct: return "Struct";
    case CompletionCandidateKind::Union: return "Union";
    case CompletionCandidateKind::Enum: return "Enum";
    case CompletionCandidateKind::Function: return "Function";
    case CompletionCandidateKind::Method: return "Method";
    case CompletionCandidateKind::Constructor: return "Constructor";
    case CompletionCandidateKind::Destructor: return "Destructor";
    case CompletionCandidateKind::Variable: return "Variable";
    case CompletionCandidateKind::StaticVariable: return "Static variable";
    case CompletionCandidateKind::Member: return "Member";
    case CompletionCandidateKind::Typedef: return "Typedef";
    case CompletionCandidateKind::UsingAlias: return "Using alias";
    case CompletionCandidateKind::DocumentWord: return "Document word";
    default: return "Unknown";
    }
}

const char* CompletionCandidateKindPrefix(CompletionCandidateKind kind) {
    switch (kind) {
    case CompletionCandidateKind::Keyword: return "[k]";
    case CompletionCandidateKind::Namespace: return "[n]";
    case CompletionCandidateKind::Class: return "[c]";
    case CompletionCandidateKind::Struct: return "[s]";
    case CompletionCandidateKind::Union: return "[u]";
    case CompletionCandidateKind::Enum: return "[e]";
    case CompletionCandidateKind::Function: return "[f]";
    case CompletionCandidateKind::Method: return "[m]";
    case CompletionCandidateKind::Constructor: return "[c]";
    case CompletionCandidateKind::Variable: return "[v]";
    case CompletionCandidateKind::StaticVariable: return "[v]";
    case CompletionCandidateKind::Member: return "[m]";
    case CompletionCandidateKind::Typedef:
    case CompletionCandidateKind::UsingAlias: return "[a]";
    case CompletionCandidateKind::DocumentWord: return "[w]";
    default: return "[?]";
    }
}

const char* CompletionCandidateSourceName(CompletionCandidateSource source) {
    switch (source) {
    case CompletionCandidateSource::CurrentScope: return "Current scope";
    case CompletionCandidateSource::CurrentDocument: return "Current document";
    case CompletionCandidateSource::ProjectIndex: return "Project index";
    case CompletionCandidateSource::KeywordSet: return "Keyword";
    case CompletionCandidateSource::DocumentWordSet: return "Document word";
    default: return "Unknown";
    }
}

const char* CompletionContextKindName(CompletionContextKind kind) {
    switch (kind) {
    case CompletionContextKind::Identifier: return "IDENTIFIER";
    case CompletionContextKind::QualifiedName: return "QUALIFIED";
    case CompletionContextKind::NamespaceQualifier: return "NAMESPACE_QUALIFIER";
    case CompletionContextKind::TypeQualifier: return "TYPE_QUALIFIER";
    case CompletionContextKind::MemberAccessLexical: return "MEMBER_LEXICAL";
    case CompletionContextKind::Preprocessor: return "PREPROCESSOR";
    case CompletionContextKind::CommentOrString: return "COMMENT_OR_STRING";
    default: return "UNSUPPORTED";
    }
}

const char* CompletionErrorName(CompletionErrorCode code) {
    switch (code) {
    case CompletionErrorCode::None: return "COMPLETION_NONE";
    case CompletionErrorCode::NoProject: return "COMPLETION_NO_PROJECT";
    case CompletionErrorCode::NoDocument: return "COMPLETION_NO_DOCUMENT";
    case CompletionErrorCode::UnsupportedContext: return "COMPLETION_UNSUPPORTED_CONTEXT";
    case CompletionErrorCode::InComment: return "COMPLETION_IN_COMMENT";
    case CompletionErrorCode::InString: return "COMPLETION_IN_STRING";
    case CompletionErrorCode::InCharacter: return "COMPLETION_IN_CHARACTER";
    case CompletionErrorCode::InRawString: return "COMPLETION_IN_RAW_STRING";
    case CompletionErrorCode::InPreprocessor: return "COMPLETION_IN_PREPROCESSOR";
    case CompletionErrorCode::PrefixTooLong: return "COMPLETION_PREFIX_TOO_LONG";
    case CompletionErrorCode::QualifierTooLong: return "COMPLETION_QUALIFIER_TOO_LONG";
    case CompletionErrorCode::IndexNotReady: return "COMPLETION_INDEX_NOT_READY";
    case CompletionErrorCode::ProjectStale: return "COMPLETION_PROJECT_STALE";
    case CompletionErrorCode::DocumentStale: return "COMPLETION_DOCUMENT_STALE";
    case CompletionErrorCode::SessionStale: return "COMPLETION_SESSION_STALE";
    case CompletionErrorCode::NoResults: return "COMPLETION_NO_RESULTS";
    case CompletionErrorCode::ResultsTruncated: return "COMPLETION_RESULTS_TRUNCATED";
    case CompletionErrorCode::CandidateLimit: return "COMPLETION_CANDIDATE_LIMIT";
    case CompletionErrorCode::WordLimit: return "COMPLETION_WORD_LIMIT";
    case CompletionErrorCode::InvalidReplacementRange: return "COMPLETION_INVALID_REPLACEMENT_RANGE";
    case CompletionErrorCode::ExpectedTextMismatch: return "COMPLETION_EXPECTED_TEXT_MISMATCH";
    case CompletionErrorCode::InsertionTooLong: return "COMPLETION_INSERTION_TOO_LONG";
    case CompletionErrorCode::InsertionFailed: return "COMPLETION_INSERTION_FAILED";
    default: return "COMPLETION_INTERNAL";
    }
}

const char* CompletionStatusText(CompletionErrorCode code) {
    switch (code) {
    case CompletionErrorCode::NoResults: return "No completion suggestions.";
    case CompletionErrorCode::ResultsTruncated: return "Completion results truncated.";
    case CompletionErrorCode::SessionStale: return "Completion session expired.";
    case CompletionErrorCode::InComment:
    case CompletionErrorCode::InString:
    case CompletionErrorCode::InCharacter:
    case CompletionErrorCode::InRawString:
    case CompletionErrorCode::InPreprocessor:
    case CompletionErrorCode::UnsupportedContext: return "Code completion is unavailable in this context.";
    default: return "";
    }
}

uint64_t CompletionProjectId(const char* projectId) { return hashText(projectId); }

void DocumentWordCacheInit(DocumentWordCache* cache, DocumentWordEntry* entries, uint32_t capacity) {
    if (!cache) return;
    cache->entries = entries;
    cache->capacity = capacity > kCompletionMaxDocumentWords ? kCompletionMaxDocumentWords : capacity;
    cache->documentId = 0;
    cache->documentGeneration = 0;
    cache->count = 0;
    cache->valid = false;
    cache->truncated = false;
}

bool DocumentWordCacheRefresh(DocumentWordCache* cache, const Document& document,
                              CompletionErrorCode* error) {
    if (error) *error = CompletionErrorCode::None;
    if (!cache || !cache->entries || cache->capacity == 0) {
        if (error) *error = CompletionErrorCode::Internal;
        return false;
    }
    cache->count = 0;
    cache->truncated = false;
    cache->valid = false;
    cache->documentId = document.documentId;
    cache->documentGeneration = document.buffer.generation;
    if (!document.syntax.valid || document.syntax.fallback || document.buffer.length > kCompletionMaxDocumentWordScanBytes) {
        if (error) *error = CompletionErrorCode::IndexNotReady;
        return false;
    }
    const uint32_t lineCount = TextBufferLineCount(&document.buffer);
    bool inactive[64] = {};
    bool parentInactive[64] = {};
    uint32_t conditionalDepth = 0;
    for (uint32_t line = 0; line < lineCount; ++line) {
        const uint32_t lineStart = TextBufferLineStart(&document.buffer, line);
        const uint32_t lineEnd = TextBufferLineEnd(&document.buffer, line);
        const bool lineIsInactive = conditionalDepth > 0 && inactive[conditionalDepth - 1];
        if (!lineIsInactive) {
            uint32_t spanCount = 0;
            const SyntaxTokenSpan* spans = SyntaxCacheLineSpans(&document.syntax, line, &spanCount);
            for (uint32_t i = 0; i < spanCount; ++i) {
                if (spans[i].kind != SyntaxTokenKind::Identifier) continue;
                const uint32_t start = lineStart + spans[i].start;
                const uint32_t length = spans[i].length;
                if (start + length > lineEnd || length == 0 || length > kCompletionMaxInsertionBytes) continue;
                char word[kCompletionMaxInsertionBytes + 1] = {};
                copyRange(word, sizeof(word), document.buffer.data, start, length);
                if (word[0] == '\0' || SyntaxIsKeyword(word)) continue;
                uint32_t existing = cache->count;
                for (uint32_t j = 0; j < cache->count; ++j) {
                    if (equalText(cache->entries[j].word, word, false)) { existing = j; break; }
                }
                if (existing < cache->count) {
                    ++cache->entries[existing].occurrenceCount;
                    continue;
                }
                if (cache->count >= cache->capacity) {
                    cache->truncated = true;
                    continue;
                }
                DocumentWordEntry& entry = cache->entries[cache->count++];
                clearText(entry.word, sizeof(entry.word));
                copyText(entry.word, sizeof(entry.word), word);
                entry.occurrenceCount = 1;
                entry.firstOffset = start;
            }
        }
        char directive[24] = {};
        bool zeroIf = false;
        if (lineDirective(document.buffer.data, lineStart, lineEnd, directive, sizeof(directive), &zeroIf)) {
            if (equalText(directive, "if", false) || equalText(directive, "ifdef", false) || equalText(directive, "ifndef", false)) {
                if (conditionalDepth < 64) {
                    parentInactive[conditionalDepth] = conditionalDepth > 0 && inactive[conditionalDepth - 1];
                    inactive[conditionalDepth] = parentInactive[conditionalDepth] ||
                        (equalText(directive, "if", false) && zeroIf);
                    ++conditionalDepth;
                }
            } else if (equalText(directive, "else", false) || equalText(directive, "elif", false)) {
                if (conditionalDepth > 0) {
                    const uint32_t level = conditionalDepth - 1;
                    inactive[level] = parentInactive[level] || !inactive[level];
                }
            } else if (equalText(directive, "endif", false) && conditionalDepth > 0) {
                --conditionalDepth;
            }
        }
    }
    cache->valid = true;
    if (cache->truncated && error) *error = CompletionErrorCode::WordLimit;
    return true;
}

const DocumentWordEntry* DocumentWordCacheAt(const DocumentWordCache* cache, uint32_t index) {
    return cache && cache->entries && index < cache->count ? &cache->entries[index] : nullptr;
}

bool CompletionExtractContext(const Document& document, uint64_t projectId,
                              uint64_t projectGeneration, uint64_t sessionId,
                              bool manuallyInvoked, CompletionContext* output,
                              CompletionErrorCode* error) {
    if (error) *error = CompletionErrorCode::None;
    if (!output) {
        if (error) *error = CompletionErrorCode::Internal;
        return false;
    }
    *output = CompletionContext();
    output->sessionId = sessionId;
    output->projectId = projectId;
    output->projectGeneration = projectGeneration;
    output->documentId = document.documentId;
    output->documentGeneration = document.buffer.generation;
    output->manuallyInvoked = manuallyInvoked;
    output->caretByteOffset = document.buffer.caret <= document.buffer.length ? document.buffer.caret : document.buffer.length;
    if (!document.used) {
        if (error) *error = CompletionErrorCode::NoDocument;
        return false;
    }
    if (document.syntax.language != SyntaxLanguage::C && document.syntax.language != SyntaxLanguage::Cpp) {
        if (error) *error = CompletionErrorCode::UnsupportedContext;
        return false;
    }
    const uint32_t caret = static_cast<uint32_t>(output->caretByteOffset);
    const uint32_t line = lineForOffset(document, caret);
    if (lineInactiveAt(document, line)) {
        output->kind = CompletionContextKind::Unsupported;
        if (error) *error = CompletionErrorCode::UnsupportedContext;
        return false;
    }
    const SyntaxTokenKind tokenKind = syntaxKindAt(document, caret);
    if (tokenKind == SyntaxTokenKind::Comment) { if (error) *error = CompletionErrorCode::InComment; return false; }
    if (tokenKind == SyntaxTokenKind::CharacterLiteral) { if (error) *error = CompletionErrorCode::InCharacter; return false; }
    if (tokenKind == SyntaxTokenKind::StringLiteral) {
        if (error) *error = CompletionErrorCode::InString;
        return false;
    }
    if (tokenKind == SyntaxTokenKind::Preprocessor) { if (error) *error = CompletionErrorCode::InPreprocessor; return false; }

    uint32_t start = caret;
    uint32_t end = caret;
    bool selected = false;
    bool invalidSelection = false;
    if (document.buffer.selectionActive) {
        const uint32_t anchor = document.buffer.selectionAnchor < document.buffer.caret ? document.buffer.selectionAnchor : document.buffer.caret;
        const uint32_t selectedEnd = document.buffer.selectionAnchor < document.buffer.caret ? document.buffer.caret : document.buffer.selectionAnchor;
        if (isValidIdentifierRange(document.buffer.data, anchor, selectedEnd)) {
            start = anchor;
            end = selectedEnd;
            selected = true;
        } else invalidSelection = true;
    }
    if (!selected && !invalidSelection) {
        while (start > 0 && start > caret - (caret > kCompletionMaxContextScanBytes ? kCompletionMaxContextScanBytes : caret) &&
               isIdentifierPart(document.buffer.data[start - 1])) --start;
        while (end < document.buffer.length && end - caret < kCompletionMaxContextScanBytes &&
               isIdentifierPart(document.buffer.data[end])) ++end;
        if (start < end && !isIdentifierStart(document.buffer.data[start])) start = caret;
    }
    if (end - start > kCompletionMaxPrefixBytes) {
        if (error) *error = CompletionErrorCode::PrefixTooLong;
        return false;
    }
    output->replacementStart = start;
    output->replacementEnd = end;
    output->fromSelection = selected;
    copyRange(output->prefix, sizeof(output->prefix), document.buffer.data, start, end - start);
    extractLexicalScope(document, caret, output->containingScope, sizeof(output->containingScope));
    CompletionErrorCode qualifierError = CompletionErrorCode::None;
    if (explicitQualifierAt(document, start, output->explicitQualifier,
                             sizeof(output->explicitQualifier), &qualifierError)) {
        output->hasExplicitQualifier = true;
        output->kind = CompletionContextKind::QualifiedName;
    } else if (qualifierError != CompletionErrorCode::None) {
        if (error) *error = qualifierError;
        return false;
    } else if (precedingMemberAccess(document, start)) {
        output->kind = CompletionContextKind::MemberAccessLexical;
    } else {
        output->kind = CompletionContextKind::Identifier;
    }
    return true;
}

void CompletionSessionInit(CompletionSession* session, CompletionCandidate* storage, uint32_t capacity) {
    if (!session) return;
    *session = CompletionSession();
    session->candidates = storage;
    session->candidateCapacity = capacity > kCompletionMaxRetainedCandidates ? kCompletionMaxRetainedCandidates : capacity;
    session->sessionId = 1;
}

bool CompletionBuildSession(CompletionSession* session, const Document& document,
                            uint64_t projectId, uint64_t projectGeneration,
                            const SymbolDatabase* database, DocumentWordCache* wordCache,
                            bool manuallyInvoked, CompletionErrorCode* error) {
    if (error) *error = CompletionErrorCode::None;
    if (!session || !session->candidates || session->candidateCapacity == 0) {
        if (error) *error = CompletionErrorCode::Internal;
        return false;
    }
    CompletionContext context = {};
    CompletionErrorCode contextError = CompletionErrorCode::None;
    if (!CompletionExtractContext(document, projectId, projectGeneration, session->sessionId,
                                  manuallyInvoked, &context, &contextError)) {
        if (error) *error = contextError;
        session->active = false;
        return false;
    }
    session->context = context;
    if (database) inferScopeFromSymbols(database, document, &session->context);
    session->candidateCount = 0;
    session->collectedCount = 0;
    session->selectedIndex = 0;
    session->visibleStart = 0;
    session->truncated = false;
    if (!wordCache || !wordCache->valid || wordCache->documentId != document.documentId ||
        wordCache->documentGeneration != document.buffer.generation) {
        CompletionErrorCode wordError = CompletionErrorCode::None;
        if (wordCache && !DocumentWordCacheRefresh(wordCache, document, &wordError) && wordError != CompletionErrorCode::IndexNotReady) {
            if (error) *error = wordError;
        }
    }
    const uint32_t collectionLimit = session->context.prefix[0] == '\0'
        ? kCompletionMaxContextualCollection : kCompletionMaxCandidateCollection;
    addKeywords(session, document, collectionLimit);
    addSymbols(session, document, database, collectionLimit, true);
    addWords(session, wordCache, collectionLimit);
    addSymbols(session, document, database, collectionLimit, false);
    sortCandidates(session);
    rebuildDisplay(session);
    session->active = session->candidateCount > 0;
    if (!session->active) {
        if (error) *error = CompletionErrorCode::NoResults;
        return true;
    }
    if (session->truncated && error) *error = CompletionErrorCode::ResultsTruncated;
    return true;
}

bool CompletionSessionRefresh(CompletionSession* session, const Document& document,
                              uint64_t projectId, uint64_t projectGeneration,
                              const SymbolDatabase* database, DocumentWordCache* wordCache,
                              CompletionErrorCode* error) {
    if (error) *error = CompletionErrorCode::None;
    if (!session) {
        if (error) *error = CompletionErrorCode::Internal;
        return false;
    }
    char selectedText[kCompletionMaxInsertionBytes + 1] = {};
    const CompletionCandidate* selected = CompletionSessionSelected(session);
    if (selected) copyText(selectedText, sizeof(selectedText), selected->insertionText);
    const uint64_t sessionId = session->sessionId;
    if (!CompletionBuildSession(session, document, projectId, projectGeneration, database,
                                wordCache, true, error)) return false;
    session->sessionId = sessionId;
    session->context.sessionId = sessionId;
    for (uint32_t i = 0; i < session->candidateCount; ++i) {
        if (equalText(session->candidates[i].insertionText, selectedText, false)) {
            session->selectedIndex = i;
            break;
        }
    }
    if (session->selectedIndex >= session->candidateCount) session->selectedIndex = 0;
    return true;
}

const CompletionCandidate* CompletionSessionSelected(const CompletionSession* session) {
    return session && session->active && session->selectedIndex < session->candidateCount
        ? &session->candidates[session->selectedIndex] : nullptr;
}

bool CompletionSessionMove(CompletionSession* session, int32_t delta) {
    if (!session || !session->active || session->candidateCount == 0) return false;
    int32_t next = static_cast<int32_t>(session->selectedIndex) + delta;
    if (next < 0) next = 0;
    if (next >= static_cast<int32_t>(session->candidateCount)) next = static_cast<int32_t>(session->candidateCount) - 1;
    session->selectedIndex = static_cast<uint32_t>(next);
    if (session->selectedIndex < session->visibleStart) session->visibleStart = session->selectedIndex;
    if (session->selectedIndex >= session->visibleStart + kCompletionMaxVisibleCandidates)
        session->visibleStart = session->selectedIndex - kCompletionMaxVisibleCandidates + 1;
    return true;
}

bool CompletionSessionPage(CompletionSession* session, int32_t direction) {
    return CompletionSessionMove(session, direction * static_cast<int32_t>(kCompletionMaxVisibleCandidates));
}

bool CompletionSessionHome(CompletionSession* session) {
    if (!session || !session->active) return false;
    session->selectedIndex = 0;
    session->visibleStart = 0;
    return true;
}

bool CompletionSessionEnd(CompletionSession* session) {
    if (!session || !session->active) return false;
    session->selectedIndex = session->candidateCount - 1;
    if (session->candidateCount > kCompletionMaxVisibleCandidates)
        session->visibleStart = session->candidateCount - kCompletionMaxVisibleCandidates;
    return true;
}

void CompletionSessionDismiss(CompletionSession* session) {
    if (!session) return;
    session->active = false;
    session->candidateCount = 0;
    session->selectedIndex = 0;
    session->visibleStart = 0;
}

bool CompletionSessionIsCurrent(const CompletionSession* session, const Document& document,
                                uint64_t projectId, uint64_t projectGeneration,
                                CompletionErrorCode* error) {
    if (error) *error = CompletionErrorCode::None;
    if (!session || !session->active) { if (error) *error = CompletionErrorCode::SessionStale; return false; }
    if (session->context.projectId != projectId || session->context.projectGeneration != projectGeneration) {
        if (error) *error = CompletionErrorCode::ProjectStale;
        return false;
    }
    if (document.documentId != session->context.documentId) {
        if (error) *error = CompletionErrorCode::DocumentStale;
        return false;
    }
    if (document.buffer.generation != session->context.documentGeneration) {
        if (error) *error = CompletionErrorCode::DocumentStale;
        return false;
    }
    if (session->context.replacementEnd > document.buffer.length ||
        session->context.replacementStart > session->context.replacementEnd) {
        if (error) *error = CompletionErrorCode::InvalidReplacementRange;
        return false;
    }
    if (document.buffer.caret != session->context.caretByteOffset) {
        if (error) *error = CompletionErrorCode::SessionStale;
        return false;
    }
    if (session->context.fromSelection) {
        const uint32_t start = document.buffer.selectionAnchor < document.buffer.caret ? document.buffer.selectionAnchor : document.buffer.caret;
        const uint32_t end = document.buffer.selectionAnchor < document.buffer.caret ? document.buffer.caret : document.buffer.selectionAnchor;
        if (!document.buffer.selectionActive || start != session->context.replacementStart || end != session->context.replacementEnd) {
            if (error) *error = CompletionErrorCode::SessionStale;
            return false;
        }
    } else if (document.buffer.selectionActive) {
        if (error) *error = CompletionErrorCode::SessionStale;
        return false;
    }
    return true;
}

bool CompletionSessionTextMatches(const CompletionSession* session, const Document& document,
                                  CompletionErrorCode* error) {
    if (error) *error = CompletionErrorCode::None;
    if (!session || session->context.replacementStart > session->context.replacementEnd ||
        session->context.replacementEnd > document.buffer.length) {
        if (error) *error = CompletionErrorCode::InvalidReplacementRange;
        return false;
    }
    const uint32_t length = session->context.replacementEnd - session->context.replacementStart;
    if (length != textLength(session->context.prefix, sizeof(session->context.prefix) - 1) ||
        !equalText(document.buffer.data + session->context.replacementStart, session->context.prefix, false)) {
        if (error) *error = CompletionErrorCode::ExpectedTextMismatch;
        return false;
    }
    return true;
}

} // namespace developer_studio
} // namespace guidexos
