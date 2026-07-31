#include "developer_studio_navigation.h"

namespace guidexos {
namespace developer_studio {
namespace {

static const uint32_t kContextMaxDepth = 96u;
static const uint32_t kContextScanBackBytes = 4096u;
static const uint32_t kCandidateNearbyBytes = 16u * 1024u;

struct ContextScope {
    char name[kDefinitionMaxScopeBytes / 8];
    bool named;
};

static uint32_t lengthOf(const char* text, uint32_t limit) {
    if (!text) return 0;
    uint32_t length = 0;
    while (length < limit && text[length] != '\0') ++length;
    return length;
}

static void clearBytes(char* output, uint32_t capacity) {
    if (!output) return;
    for (uint32_t i = 0; i < capacity; ++i) output[i] = '\0';
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
    while (input && i + 1 < capacity && i < length && input[start + i] != '\0') {
        output[i] = input[start + i];
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

static bool equalText(const char* left, const char* right, bool foldCase) {
    if (!left || !right) return left == right;
    uint32_t i = 0;
    while (left[i] != '\0' && right[i] != '\0') {
        char a = left[i];
        char b = right[i];
        if (foldCase) {
            if (a >= 'A' && a <= 'Z') a = static_cast<char>(a + ('a' - 'A'));
            if (b >= 'A' && b <= 'Z') b = static_cast<char>(b + ('a' - 'A'));
        }
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
        char a = text[i++];
        char b = prefix[i - 1];
        if (foldCase) {
            if (a >= 'A' && a <= 'Z') a = static_cast<char>(a + ('a' - 'A'));
            if (b >= 'A' && b <= 'Z') b = static_cast<char>(b + ('a' - 'A'));
        }
        if (a != b) return false;
    }
    return true;
}

static bool endsWithQualified(const char* text, const char* suffix) {
    if (!text || !suffix) return false;
    const uint32_t textLength = lengthOf(text, kSymbolMaxQualifiedNameBytes);
    const uint32_t suffixLength = lengthOf(suffix, kDefinitionMaxQualifierBytes + kDefinitionMaxIdentifierBytes + 4u);
    if (suffixLength == 0 || suffixLength > textLength) return false;
    const uint32_t start = textLength - suffixLength;
    if (start > 0 && !(text[start - 1] == ':' && text[start] == ':')) return false;
    for (uint32_t i = 0; i < suffixLength; ++i) if (text[start + i] != suffix[i]) return false;
    return true;
}

static bool appendChar(char* output, uint32_t capacity, uint32_t& length, char value) {
    if (!output || length + 1 >= capacity) return false;
    output[length++] = value;
    output[length] = '\0';
    return true;
}

static void appendText(char* output, uint32_t capacity, uint32_t& length, const char* text) {
    if (!text) return;
    for (uint32_t i = 0; text[i] != '\0'; ++i) {
        if (!appendChar(output, capacity, length, text[i])) break;
    }
}

static bool validIdentifierRange(const char* text, uint32_t length, uint32_t start,
                                 uint32_t end, bool allowDestructor) {
    if (!text || start >= end || end > length) return false;
    uint32_t first = start;
    if (allowDestructor && text[first] == '~') {
        ++first;
        if (first >= end) return false;
    }
    if (!isIdentifierStart(text[first])) return false;
    for (uint32_t i = first + 1; i < end; ++i)
        if (!isIdentifierPart(text[i])) return false;
    return true;
}

static bool identifierAt(const char* text, uint32_t length, uint32_t caret,
                         uint32_t* start, uint32_t* end) {
    if (!text || !start || !end) return false;
    if (caret > length) caret = length;
    uint32_t point = caret;
    if (point < length && !isIdentifierPart(text[point])) {
        if (point == 0 || !isIdentifierPart(text[point - 1])) return false;
        point -= 1;
    } else if (point == length) {
        if (point == 0 || !isIdentifierPart(text[point - 1])) return false;
        point -= 1;
    }
    uint32_t left = point;
    while (left > 0 && isIdentifierPart(text[left - 1])) --left;
    uint32_t right = point + 1;
    while (right < length && isIdentifierPart(text[right])) ++right;
    if (left > 0 && text[left - 1] == '~') --left;
    *start = left;
    *end = right;
    return true;
}

static bool readWordAt(const char* text, uint32_t length, uint32_t offset,
                       char* output, uint32_t capacity, uint32_t* next) {
    if (!text || offset >= length || !isIdentifierStart(text[offset])) return false;
    uint32_t cursor = offset + 1;
    while (cursor < length && isIdentifierPart(text[cursor])) ++cursor;
    copyRange(output, capacity, text, offset, cursor - offset);
    if (next) *next = cursor;
    return true;
}

static bool wordIs(const char* word, const char* expected) {
    return equalText(word, expected, false);
}

static bool isSpace(char value) {
    return value == ' ' || value == '\t' || value == '\r' || value == '\n';
}

static void resetPending(char* kind, char* name, uint32_t nameCapacity, bool* active) {
    if (kind) kind[0] = '\0';
    if (name && nameCapacity > 0) name[0] = '\0';
    if (active) *active = false;
}

static void captureScopeName(const char* text, uint32_t length, uint32_t from,
                             uint32_t brace, const char* kind, char* output,
                             uint32_t capacity) {
    if (!output || capacity == 0) return;
    output[0] = '\0';
    uint32_t cursor = from;
    uint32_t lengthOut = 0;
    bool first = true;
    bool enumTag = false;
    while (cursor < brace && isSpace(text[cursor])) ++cursor;
    if (wordIs(kind, "enum") && cursor + 5 < brace &&
        ((text[cursor] == 'c' && text[cursor + 1] == 'l' && text[cursor + 2] == 'a' &&
          text[cursor + 3] == 's' && text[cursor + 4] == 's') ||
         (text[cursor] == 's' && text[cursor + 1] == 't' && text[cursor + 2] == 'r' &&
          text[cursor + 3] == 'u' && text[cursor + 4] == 'c' && text[cursor + 5] == 't'))) {
        cursor += text[cursor] == 'c' ? 5u : 6u;
        enumTag = true;
    }
    (void)enumTag;
    while (cursor < brace) {
        if (isIdentifierStart(text[cursor])) {
            char word[kMaxNameBytes] = {};
            uint32_t next = cursor;
            readWordAt(text, length, cursor, word, sizeof(word), &next);
            const bool isBaseOrAttribute = wordIs(word, "public") || wordIs(word, "private") ||
                wordIs(word, "protected") || wordIs(word, "final") || wordIs(word, "alignas");
            if (!isBaseOrAttribute && (wordIs(kind, "namespace") || first)) {
                if (!first) appendText(output, capacity, lengthOut, "::");
                appendText(output, capacity, lengthOut, word);
                first = false;
                if (!wordIs(kind, "namespace")) break;
            }
            cursor = next;
            continue;
        }
        if (text[cursor] == ':' && cursor + 1 < brace && text[cursor + 1] == ':') {
            cursor += 2;
            continue;
        }
        if (text[cursor] == ':' || text[cursor] == ',' || text[cursor] == '=') break;
        ++cursor;
    }
}

static void captureScope(const char* text, uint32_t length, uint32_t caret,
                         char* output, uint32_t capacity) {
    if (!output || capacity == 0) return;
    output[0] = '\0';
    if (!text) return;
    if (caret > length) caret = length;
    ContextScope scopes[kContextMaxDepth] = {};
    uint32_t depth = 0;
    char pendingKind[24] = {};
    char pendingName[kDefinitionMaxScopeBytes / 8] = {};
    bool pending = false;
    uint32_t pendingFrom = 0;
    uint32_t cursor = 0;
    while (cursor < caret) {
        if (text[cursor] == '/' && cursor + 1 < caret && text[cursor + 1] == '/') {
            cursor += 2;
            while (cursor < caret && text[cursor] != '\n') ++cursor;
            continue;
        }
        if (text[cursor] == '/' && cursor + 1 < caret && text[cursor + 1] == '*') {
            cursor += 2;
            while (cursor + 1 < caret && !(text[cursor] == '*' && text[cursor + 1] == '/')) ++cursor;
            cursor += cursor + 1 < caret ? 2 : 0;
            continue;
        }
        if (text[cursor] == '"' || text[cursor] == '\'') {
            const char quote = text[cursor++];
            while (cursor < caret) {
                if (text[cursor] == '\\' && cursor + 1 < caret) { cursor += 2; continue; }
                if (text[cursor++] == quote) break;
            }
            continue;
        }
        if (isIdentifierStart(text[cursor])) {
            char word[kMaxNameBytes] = {};
            uint32_t next = cursor;
            readWordAt(text, length, cursor, word, sizeof(word), &next);
            if (wordIs(word, "namespace") || wordIs(word, "class") || wordIs(word, "struct") ||
                wordIs(word, "union") || wordIs(word, "enum")) {
                copyText(pendingKind, sizeof(pendingKind), word);
                pendingName[0] = '\0';
                pending = true;
                pendingFrom = next;
            } else if (pending && pendingName[0] == '\0' &&
                       !wordIs(word, "class") && !wordIs(word, "struct")) {
                copyText(pendingName, sizeof(pendingName), word);
            } else if (pending && wordIs(pendingKind, "namespace")) {
                uint32_t current = lengthOf(pendingName, sizeof(pendingName));
                if (current + 2 < sizeof(pendingName)) {
                    pendingName[current++] = ':';
                    pendingName[current++] = ':';
                    pendingName[current] = '\0';
                    appendText(pendingName, sizeof(pendingName), current, word);
                }
            }
            cursor = next;
            continue;
        }
        if (text[cursor] == ';' || text[cursor] == '=') resetPending(pendingKind, pendingName, sizeof(pendingName), &pending);
        if (text[cursor] == '{') {
            if (depth < kContextMaxDepth) {
                ContextScope& scope = scopes[depth++];
                scope.named = pending && pendingName[0] != '\0';
                scope.name[0] = '\0';
                if (scope.named) captureScopeName(text, length, pendingFrom, cursor,
                                                   pendingKind, scope.name, sizeof(scope.name));
                if (scope.named && scope.name[0] == '\0') scope.named = false;
            }
            resetPending(pendingKind, pendingName, sizeof(pendingName), &pending);
        } else if (text[cursor] == '}') {
            if (depth > 0) --depth;
            resetPending(pendingKind, pendingName, sizeof(pendingName), &pending);
        }
        ++cursor;
    }
    uint32_t outLength = 0;
    for (uint32_t i = 0; i < depth; ++i) {
        if (!scopes[i].named || scopes[i].name[0] == '\0') continue;
        if (outLength > 0) appendText(output, capacity, outLength, "::");
        appendText(output, capacity, outLength, scopes[i].name);
    }
}

static void captureQualifier(const char* text, uint32_t length, uint32_t start,
                             char* output, uint32_t capacity) {
    if (!output || capacity == 0) return;
    output[0] = '\0';
    if (!text || start < 2 || start > length || text[start - 2] != ':' || text[start - 1] != ':') return;
    char parts[32][kMaxNameBytes] = {};
    uint32_t partCount = 0;
    uint32_t cursor = start - 2;
    while (cursor > 0 && partCount < 32) {
        while (cursor > 0 && isSpace(text[cursor - 1])) --cursor;
        uint32_t end = cursor;
        while (cursor > 0 && isIdentifierPart(text[cursor - 1])) --cursor;
        if (end == cursor || !isIdentifierStart(text[cursor])) break;
        copyRange(parts[partCount++], sizeof(parts[0]), text, cursor, end - cursor);
        while (cursor > 0 && isSpace(text[cursor - 1])) --cursor;
        if (cursor < 2 || text[cursor - 2] != ':' || text[cursor - 1] != ':') break;
        cursor -= 2;
    }
    uint32_t outLength = 0;
    while (partCount > 0) {
        if (outLength > 0) appendText(output, capacity, outLength, "::");
        appendText(output, capacity, outLength, parts[--partCount]);
    }
}

static SymbolKind likelyKind(const char* text, uint32_t length, uint32_t start, uint32_t end) {
    uint32_t cursor = end;
    while (cursor < length && isSpace(text[cursor])) ++cursor;
    if (cursor < length && text[cursor] == '(') return SymbolKind::Function;
    const uint32_t begin = start > 96u ? start - 96u : 0u;
    for (uint32_t i = begin; i < start; ++i) {
        if (!isIdentifierStart(text[i])) continue;
        uint32_t next = i + 1;
        while (next < start && isIdentifierPart(text[next])) ++next;
        char word[32] = {};
        copyRange(word, sizeof(word), text, i, next - i);
        if (wordIs(word, "class")) return SymbolKind::Class;
        if (wordIs(word, "struct")) return SymbolKind::Struct;
        if (wordIs(word, "enum")) return SymbolKind::Enum;
        if (wordIs(word, "union")) return SymbolKind::Union;
        if (wordIs(word, "namespace")) return SymbolKind::Namespace;
        i = next;
    }
    return SymbolKind::Macro;
}

static bool prefixPath(const char* root, const char* path) {
    if (!root || !path) return false;
    uint32_t rootLength = lengthOf(root, kMaxPathBytes);
    uint32_t pathLength = lengthOf(path, kMaxPathBytes);
    if (pathLength <= rootLength) return false;
    for (uint32_t i = 0; i < rootLength; ++i) {
        char a = root[i];
        char b = path[i];
        if (a >= 'A' && a <= 'Z') a = static_cast<char>(a + ('a' - 'A'));
        if (b >= 'A' && b <= 'Z') b = static_cast<char>(b + ('a' - 'A'));
        if (a != b) return false;
    }
    return path[rootLength] == '/';
}

static bool relativePathFor(const char* root, const char* absolute, char* output, uint32_t capacity) {
    if (!output || capacity == 0 || !absolute) return false;
    output[0] = '\0';
    char normalizedRoot[kMaxPathBytes] = {};
    char normalizedPath[kMaxPathBytes] = {};
    if (root && root[0] && NormalizePath(root, normalizedRoot, sizeof(normalizedRoot)) &&
        NormalizePath(absolute, normalizedPath, sizeof(normalizedPath)) && prefixPath(normalizedRoot, normalizedPath)) {
        copyText(output, capacity, normalizedPath + lengthOf(normalizedRoot, sizeof(normalizedRoot)) + 1);
        return true;
    }
    copyText(output, capacity, absolute);
    return false;
}

static int pathCompare(const char* left, const char* right) {
    uint32_t i = 0;
    while (left && right && left[i] != '\0' && right[i] != '\0') {
        char a = left[i];
        char b = right[i];
        if (a >= 'A' && a <= 'Z') a = static_cast<char>(a + ('a' - 'A'));
        if (b >= 'A' && b <= 'Z') b = static_cast<char>(b + ('a' - 'A'));
        if (a != b) return a < b ? -1 : 1;
        ++i;
    }
    char a = left ? left[i] : '\0';
    char b = right ? right[i] : '\0';
    return a == b ? 0 : (a < b ? -1 : 1);
}

static int rolePriority(SymbolDeclarationRole role) {
    switch (role) {
    case SymbolDeclarationRole::Definition: return 5;
    case SymbolDeclarationRole::Alias: return 4;
    case SymbolDeclarationRole::Declaration: return 3;
    case SymbolDeclarationRole::ForwardDeclaration: return 2;
    default: return 1;
    }
}

static bool candidateBefore(const DefinitionCandidate& left, const DefinitionCandidate& right) {
    if (left.rankScore != right.rankScore) return left.rankScore > right.rankScore;
    const int leftRole = rolePriority(left.symbol.symbol.declarationRole);
    const int rightRole = rolePriority(right.symbol.symbol.declarationRole);
    if (leftRole != rightRole) return leftRole > rightRole;
    const int qualified = pathCompare(left.symbol.symbol.qualifiedName, right.symbol.symbol.qualifiedName);
    if (qualified != 0) return qualified < 0;
    const int path = pathCompare(left.relativePath, right.relativePath);
    if (path != 0) return path < 0;
    if (left.symbol.symbol.location.line != right.symbol.symbol.location.line)
        return left.symbol.symbol.location.line < right.symbol.symbol.location.line;
    if (left.symbol.symbol.location.column != right.symbol.symbol.location.column)
        return left.symbol.symbol.location.column < right.symbol.symbol.location.column;
    if (left.symbol.symbol.location.identifierOffset != right.symbol.symbol.location.identifierOffset)
        return left.symbol.symbol.location.identifierOffset < right.symbol.symbol.location.identifierOffset;
    return static_cast<int>(left.symbol.symbol.kind) < static_cast<int>(right.symbol.symbol.kind);
}

static uint64_t candidateHash(const DefinitionCandidate& candidate) {
    uint64_t hash = 1469598103934665603ull;
    for (uint32_t i = 0; candidate.relativePath[i] != '\0'; ++i) {
        hash ^= static_cast<uint8_t>(candidate.relativePath[i]);
        hash *= 1099511628211ull;
    }
    hash ^= candidate.symbol.symbol.location.identifierOffset;
    hash *= 1099511628211ull;
    hash ^= static_cast<uint32_t>(candidate.symbol.symbol.kind);
    return hash == 0 ? 1 : hash;
}

static bool samePrefixScope(const char* left, const char* right) {
    if (!left || !right || left[0] == '\0' || right[0] == '\0') return false;
    uint32_t leftEnd = lengthOf(left, kSymbolMaxQualifiedNameBytes);
    uint32_t rightEnd = lengthOf(right, kDefinitionMaxScopeBytes + 1u);
    uint32_t leftLast = 0;
    uint32_t rightLast = 0;
    for (uint32_t i = 0; i + 1 < leftEnd; ++i) if (left[i] == ':' && left[i + 1] == ':') leftLast = i + 2;
    for (uint32_t i = 0; i + 1 < rightEnd; ++i) if (right[i] == ':' && right[i + 1] == ':') rightLast = i + 2;
    if (leftLast == 0 || rightLast == 0) return leftLast == rightLast && equalText(left, right, false);
    for (uint32_t i = 0; i < leftLast && i < rightLast; ++i) if (left[i] != right[i]) return false;
    return leftLast == rightLast;
}

static int32_t scoreCandidate(const DefinitionQuery& query, const ProjectSymbol& symbol,
                              const char* relativePath, bool exactCase, bool stale,
                              uint32_t* reasonFlags) {
    int32_t score = 0;
    uint32_t reasons = 0;
    char expectedQualified[kDefinitionMaxQualifierBytes + kDefinitionMaxIdentifierBytes + 4] = {};
    uint32_t expectedLength = 0;
    if (query.lexicalQualifier[0] != '\0') {
        appendText(expectedQualified, sizeof(expectedQualified), expectedLength, query.lexicalQualifier);
        appendText(expectedQualified, sizeof(expectedQualified), expectedLength, "::");
        appendText(expectedQualified, sizeof(expectedQualified), expectedLength, query.identifier);
    } else if (query.containingScope[0] != '\0') {
        appendText(expectedQualified, sizeof(expectedQualified), expectedLength, query.containingScope);
        appendText(expectedQualified, sizeof(expectedQualified), expectedLength, "::");
        appendText(expectedQualified, sizeof(expectedQualified), expectedLength, query.identifier);
    } else {
        appendText(expectedQualified, sizeof(expectedQualified), expectedLength, query.identifier);
    }
    char scopedQualified[kDefinitionMaxQualifierBytes + kDefinitionMaxIdentifierBytes + 8] = {};
    uint32_t scopedLength = 0;
    if (query.containingScope[0] != '\0') {
        appendText(scopedQualified, sizeof(scopedQualified), scopedLength, query.containingScope);
        appendText(scopedQualified, sizeof(scopedQualified), scopedLength, "::");
    }
    if (query.lexicalQualifier[0] != '\0') {
        appendText(scopedQualified, sizeof(scopedQualified), scopedLength, query.lexicalQualifier);
        appendText(scopedQualified, sizeof(scopedQualified), scopedLength, "::");
    }
    appendText(scopedQualified, sizeof(scopedQualified), scopedLength, query.identifier);
    const bool qualifiedMatch = equalText(symbol.symbol.qualifiedName, expectedQualified, false) ||
        equalText(symbol.symbol.qualifiedName, scopedQualified, false) ||
        (query.lexicalQualifier[0] != '\0' && endsWithQualified(symbol.symbol.qualifiedName, expectedQualified));
    if (qualifiedMatch) { score += 1000; reasons |= DefinitionReasonExactQualifiedName; }
    if (equalText(symbol.symbol.container, query.containingScope, false) && query.containingScope[0] != '\0') { score += 700; reasons |= DefinitionReasonSameScope; }
    if (symbol.symbol.declarationRole == SymbolDeclarationRole::Definition) { score += 500; reasons |= DefinitionReasonDefinitionPreferred; }
    if (exactCase) { score += 300; reasons |= DefinitionReasonExactCase; }
    if (equalText(relativePath, query.relativePath, true) || symbol.symbol.location.documentId == query.documentId) { score += 200; reasons |= DefinitionReasonSameDocument; }
    if (samePrefixScope(symbol.symbol.container, query.containingScope)) { score += 150; reasons |= DefinitionReasonNearbyScope; }
    if (query.containingScope[0] != '\0' && symbol.symbol.container[0] != '\0' &&
        startsWith(symbol.symbol.container, query.containingScope, false)) { score += 125; reasons |= DefinitionReasonNearbyScope; }
    if (query.likelyKind != SymbolKind::Macro && query.likelyKind == symbol.symbol.kind) { score += 100; reasons |= DefinitionReasonMatchingKind; }
    if (relativePath[0] != '\0' && query.relativePath[0] != '\0' &&
        relativePath[0] == query.relativePath[0]) score += 75;
    if (symbol.symbol.declarationRole == SymbolDeclarationRole::Declaration) { score += 50; reasons |= DefinitionReasonDeclarationFallback; }
    if (symbol.symbol.declarationRole == SymbolDeclarationRole::ForwardDeclaration) score -= 100;
    score += 10;
    reasons |= DefinitionReasonProjectWideNameMatch;
    if (stale) score -= 200;
    if (reasonFlags) *reasonFlags = reasons;
    return score;
}

static void insertCandidate(DefinitionCandidate* candidates, uint32_t& count,
                            uint32_t capacity, const DefinitionCandidate& value,
                            bool* truncated) {
    if (!candidates || capacity == 0) return;
    if (count < capacity) {
        candidates[count++] = value;
    } else {
        if (truncated) *truncated = true;
        if (!candidateBefore(value, candidates[count - 1])) return;
        candidates[count - 1] = value;
    }
    uint32_t index = count - 1;
    while (index > 0 && candidateBefore(candidates[index], candidates[index - 1])) {
        DefinitionCandidate swap = candidates[index - 1];
        candidates[index - 1] = candidates[index];
        candidates[index] = swap;
        --index;
    }
}

static bool locationDuplicate(const DefinitionCandidate& left, const DefinitionCandidate& right) {
    return equalText(left.relativePath, right.relativePath, true) &&
        left.symbol.symbol.location.identifierOffset == right.symbol.symbol.location.identifierOffset &&
        left.symbol.symbol.declarationRole == right.symbol.symbol.declarationRole;
}

} // namespace

bool ExtractDefinitionIdentifier(const char* text, uint32_t length, uint32_t caret,
                                 bool selectionActive, uint32_t selectionAnchor,
                                 uint32_t selectionCaret, DefinitionIdentifier* output) {
    if (!output) return false;
    clearBytes(reinterpret_cast<char*>(output), sizeof(*output));
    if (!text) return false;
    if (caret > length) caret = length;
    if (selectionActive) {
        const uint32_t start = selectionAnchor < selectionCaret ? selectionAnchor : selectionCaret;
        const uint32_t end = selectionAnchor < selectionCaret ? selectionCaret : selectionAnchor;
        if (end > start && end <= length && validIdentifierRange(text, length, start, end, true)) {
            output->fromSelection = true;
            output->start = start;
            output->length = end - start;
            if (output->length > kDefinitionMaxIdentifierBytes) {
                output->tooLong = true;
                return false;
            }
            copyRange(output->text, sizeof(output->text), text, start, output->length);
            output->valid = true;
            return true;
        }
    }
    uint32_t start = 0;
    uint32_t end = 0;
    if (!identifierAt(text, length, caret, &start, &end)) return false;
    output->start = start;
    output->length = end - start;
    if (output->length > kDefinitionMaxIdentifierBytes) {
        output->tooLong = true;
        return false;
    }
    copyRange(output->text, sizeof(output->text), text, start, output->length);
    output->valid = validIdentifierRange(text, length, start, end, true);
    return output->valid;
}

bool BuildDefinitionQuery(const Document& document, const char* projectId,
                          uint64_t projectGeneration, const char* projectRoot,
                          const char* relativePath, uint64_t queryId,
                          DefinitionQuery* output) {
    if (!output) return false;
    clearBytes(reinterpret_cast<char*>(output), sizeof(*output));
    output->queryId = queryId;
    output->projectGeneration = projectGeneration;
    output->documentId = document.documentId;
    output->documentGeneration = document.buffer.generation;
    output->caretByteOffset = document.buffer.caret;
    output->likelyKind = SymbolKind::Macro;
    copyText(output->projectId, sizeof(output->projectId), projectId);
    copyText(output->projectRoot, sizeof(output->projectRoot), projectRoot);
    copyText(output->relativePath, sizeof(output->relativePath), relativePath);
    DefinitionIdentifier identifier = {};
    if (!ExtractDefinitionIdentifier(document.buffer.data, document.buffer.length, document.buffer.caret,
                                     document.buffer.selectionActive, document.buffer.selectionAnchor,
                                     document.buffer.caret, &identifier)) return false;
    copyText(output->identifier, sizeof(output->identifier), identifier.text);
    output->invokedFromSelection = identifier.fromSelection;
    captureQualifier(document.buffer.data, document.buffer.length, identifier.start,
                     output->lexicalQualifier, sizeof(output->lexicalQualifier));
    captureScope(document.buffer.data, document.buffer.length, identifier.start,
                 output->containingScope, sizeof(output->containingScope));
    output->likelyKind = likelyKind(document.buffer.data, document.buffer.length,
                                    identifier.start, identifier.start + identifier.length);
    if (output->likelyKind == SymbolKind::Macro) output->likelyKind = SymbolKind::Function;
    return true;
}

bool ResolveDefinition(const SymbolDatabase* database, const DefinitionQuery& query,
                       DefinitionCandidate* candidates, uint32_t candidateCapacity,
                       DefinitionResolution* output) {
    if (!output) return false;
    output->queryId = query.queryId;
    output->kind = DefinitionResolutionKind::Failed;
    output->candidates = candidates;
    output->candidateCount = 0;
    output->visibleCandidateCount = 0;
    output->truncated = false;
    output->declarationsOnly = false;
    output->statusCode[0] = '\0';
    if (!database || !candidates || candidateCapacity == 0) {
        copyText(output->statusCode, sizeof(output->statusCode), "GOTO_DEFINITION_INDEX_NOT_READY");
        return false;
    }
    if (query.identifier[0] == '\0') {
        output->kind = DefinitionResolutionKind::None;
        copyText(output->statusCode, sizeof(output->statusCode), "GOTO_DEFINITION_NO_IDENTIFIER");
        return true;
    }
    uint32_t exactNameCount = 0;
    for (uint32_t i = 0; i < database->projectSymbolCount; ++i)
        if (equalText(database->projectSymbols[i].symbol.name, query.identifier, false)) ++exactNameCount;
    const bool foldCase = exactNameCount == 0;
    uint32_t found = 0;
    for (uint32_t i = 0; i < database->projectSymbolCount; ++i) {
        const ProjectSymbol& symbol = database->projectSymbols[i];
        if (!equalText(symbol.symbol.name, query.identifier, foldCase)) continue;
        const char* absolute = SymbolDatabaseDocumentPath(database, symbol.documentIndex);
        DefinitionCandidate candidate = {};
        candidate.symbol = symbol;
        relativePathFor(query.projectRoot, absolute, candidate.relativePath, sizeof(candidate.relativePath));
        candidate.isDefinition = symbol.symbol.declarationRole == SymbolDeclarationRole::Definition;
        candidate.isDeclaration = symbol.symbol.declarationRole == SymbolDeclarationRole::Declaration;
        candidate.isForwardDeclaration = symbol.symbol.declarationRole == SymbolDeclarationRole::ForwardDeclaration;
        const SymbolDocument* document = SymbolDatabaseDocumentAt(database, symbol.documentIndex);
        candidate.stale = (database->projectGeneration != 0 && query.projectGeneration != 0 &&
                           database->projectGeneration != query.projectGeneration) ||
            (symbol.symbol.location.documentId == query.documentId &&
             symbol.symbol.location.generation != query.documentGeneration);
        candidate.rankScore = scoreCandidate(query, symbol, candidate.relativePath, !foldCase,
                                             candidate.stale, &candidate.reasonFlags);
        if (document && document->dirty && symbol.symbol.location.generation != document->generation)
            candidate.stale = true;
        candidate.candidateId = candidateHash(candidate);
        bool duplicate = false;
        for (uint32_t j = 0; j < found; ++j) if (locationDuplicate(candidate, candidates[j])) { duplicate = true; break; }
        if (!duplicate) insertCandidate(candidates, found, candidateCapacity, candidate, &output->truncated);
    }
    output->candidateCount = found;
    output->visibleCandidateCount = found < kDefinitionMaxVisibleCandidates ? found : kDefinitionMaxVisibleCandidates;
    if (found == 0) {
        output->kind = DefinitionResolutionKind::None;
        copyText(output->statusCode, sizeof(output->statusCode), "GOTO_DEFINITION_NO_CANDIDATES");
        return true;
    }
    bool hasDefinition = false;
    bool hasNonStale = false;
    for (uint32_t i = 0; i < found; ++i) {
        if (candidates[i].isDefinition) hasDefinition = true;
        if (!candidates[i].stale) hasNonStale = true;
    }
    if (!hasDefinition) output->declarationsOnly = true;
    for (uint32_t i = 0; i < found; ++i) {
        if (candidates[i].isForwardDeclaration && hasDefinition) candidates[i].rankScore -= 100;
    }
    for (uint32_t i = 1; i < found; ++i) {
        DefinitionCandidate value = candidates[i];
        uint32_t j = i;
        while (j > 0 && candidateBefore(value, candidates[j - 1])) { candidates[j] = candidates[j - 1]; --j; }
        candidates[j] = value;
    }
    if (!hasNonStale) {
        output->kind = DefinitionResolutionKind::Stale;
        copyText(output->statusCode, sizeof(output->statusCode), "GOTO_DEFINITION_PROJECT_STALE");
    } else if (output->truncated) {
        output->kind = DefinitionResolutionKind::Multiple;
        copyText(output->statusCode, sizeof(output->statusCode), "GOTO_DEFINITION_CANDIDATES_TRUNCATED");
    } else if (found == 1 || candidates[0].rankScore - candidates[1].rankScore >= 150) {
        output->kind = DefinitionResolutionKind::Direct;
        copyText(output->statusCode, sizeof(output->statusCode), output->declarationsOnly ?
                 "GOTO_DEFINITION_DECLARATIONS_ONLY" : "GOTO_DEFINITION_DIRECT");
    } else {
        output->kind = DefinitionResolutionKind::Multiple;
        copyText(output->statusCode, sizeof(output->statusCode), "GOTO_DEFINITION_MULTIPLE");
    }
    return true;
}

const char* DefinitionResolutionKindName(DefinitionResolutionKind kind) {
    switch (kind) {
    case DefinitionResolutionKind::Direct: return "DIRECT";
    case DefinitionResolutionKind::Multiple: return "MULTIPLE";
    case DefinitionResolutionKind::Stale: return "STALE";
    case DefinitionResolutionKind::Failed: return "FAILED";
    default: return "NONE";
    }
}

void NavigationHistoryInit(NavigationHistory* history) {
    if (!history) return;
    clearBytes(reinterpret_cast<char*>(history), sizeof(*history));
}

bool NavigationLocationEqual(const NavigationLocation& left, const NavigationLocation& right) {
    return equalText(left.projectId, right.projectId, false) && left.projectGeneration == right.projectGeneration &&
        equalText(left.relativePath, right.relativePath, true) && left.caretByteOffset == right.caretByteOffset &&
        left.selectionStart == right.selectionStart && left.selectionEnd == right.selectionEnd;
}

static void pushStack(NavigationLocation* stack, uint32_t& count, const NavigationLocation& location) {
    if (count > 0 && NavigationLocationEqual(stack[count - 1], location)) return;
    if (count == kNavigationHistoryCapacity) {
        for (uint32_t i = 1; i < count; ++i) stack[i - 1] = stack[i];
        --count;
    }
    stack[count++] = location;
}

void NavigationHistoryPush(NavigationHistory* history, const NavigationLocation& location) {
    if (!history) return;
    pushStack(history->back, history->backCount, location);
    history->forwardCount = 0;
}

bool NavigationHistoryBack(NavigationHistory* history, const NavigationLocation& current,
                           NavigationLocation* destination) {
    if (!history || history->backCount == 0 || !destination) return false;
    pushStack(history->forward, history->forwardCount, current);
    *destination = history->back[history->backCount - 1];
    --history->backCount;
    return true;
}

bool NavigationHistoryForward(NavigationHistory* history, const NavigationLocation& current,
                              NavigationLocation* destination) {
    if (!history || history->forwardCount == 0 || !destination) return false;
    pushStack(history->back, history->backCount, current);
    *destination = history->forward[history->forwardCount - 1];
    --history->forwardCount;
    return true;
}

void NavigationHistoryClear(NavigationHistory* history) {
    NavigationHistoryInit(history);
}

} // namespace developer_studio
} // namespace guidexos
