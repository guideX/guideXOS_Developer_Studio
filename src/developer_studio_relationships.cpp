#include "developer_studio_relationships.h"

namespace guidexos {
namespace developer_studio {
namespace {

static const uint32_t kMaxSignatureScan = kRelationshipMaxNormalizedSignatureBytes;
static const uint32_t kMaxParameterScratch = 512u;
static const uint32_t kInvalidIndex = 0xffffffffu;

static uint32_t lengthOf(const char* text, uint32_t limit) {
    if (!text) return 0;
    uint32_t length = 0;
    while (length < limit && text[length] != '\0') ++length;
    return length;
}

static void copyText(char* output, uint32_t capacity, const char* input) {
    if (!output || capacity == 0) return;
    uint32_t i = 0;
    while (input && i + 1 < capacity && input[i] != '\0') { output[i] = input[i]; ++i; }
    output[i] = '\0';
}

static bool equalText(const char* left, const char* right) {
    if (!left || !right) return left == right;
    uint32_t i = 0;
    while (left[i] != '\0' && right[i] != '\0') { if (left[i] != right[i]) return false; ++i; }
    return left[i] == right[i];
}

static bool equalFolded(const char* left, const char* right) {
    if (!left || !right) return left == right;
    uint32_t i = 0;
    while (left[i] != '\0' && right[i] != '\0') {
        char a = left[i], b = right[i];
        if (a >= 'A' && a <= 'Z') a = static_cast<char>(a + ('a' - 'A'));
        if (b >= 'A' && b <= 'Z') b = static_cast<char>(b + ('a' - 'A'));
        if (a != b) return false;
        ++i;
    }
    return left[i] == right[i];
}

static bool relativePathForRoot(const char* root, const char* path, char* output, uint32_t capacity) {
    if (!output || capacity == 0 || !path) return false;
    output[0] = '\0';
    if (!root || root[0] == '\0') { copyText(output, capacity, path); return true; }
    char normalizedRoot[kMaxPathBytes] = {};
    char normalizedPath[kMaxPathBytes] = {};
    if (!NormalizePath(root, normalizedRoot, sizeof(normalizedRoot)) ||
        !NormalizePath(path, normalizedPath, sizeof(normalizedPath))) return false;
    const uint32_t rootLength = lengthOf(normalizedRoot, sizeof(normalizedRoot));
    const uint32_t pathLength = lengthOf(normalizedPath, sizeof(normalizedPath));
    if (pathLength < rootLength || !equalFolded(normalizedRoot, normalizedPath)) {
        if (pathLength < rootLength) return false;
        for (uint32_t i = 0; i < rootLength; ++i) {
            char a = normalizedRoot[i], b = normalizedPath[i];
            if (a >= 'A' && a <= 'Z') a = static_cast<char>(a + ('a' - 'A'));
            if (b >= 'A' && b <= 'Z') b = static_cast<char>(b + ('a' - 'A'));
            if (a != b) return false;
        }
    }
    if (pathLength == rootLength) return false;
    if (normalizedPath[rootLength] != '/') return false;
    copyText(output, capacity, normalizedPath + rootLength + 1);
    return output[0] != '\0';
}

static bool isIdentifierStart(char value) {
    return value == '_' || (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z');
}

static bool isIdentifierPart(char value) {
    return isIdentifierStart(value) || (value >= '0' && value <= '9');
}

static bool isSpace(char value) {
    return value == ' ' || value == '\t' || value == '\r' || value == '\n';
}

static uint32_t skipSpaceBack(const char* text, uint32_t start, uint32_t end) {
    while (end > start && isSpace(text[end - 1])) --end;
    return end;
}

static uint32_t previousIdentifierStart(const char* text, uint32_t start, uint32_t end,
                                        char* word, uint32_t wordCapacity) {
    end = skipSpaceBack(text, start, end);
    if (end == start || !isIdentifierPart(text[end - 1])) return start;
    uint32_t wordStart = end - 1;
    while (wordStart > start && isIdentifierPart(text[wordStart - 1])) --wordStart;
    if (word) {
        uint32_t length = end - wordStart;
        if (length + 1 > wordCapacity) length = wordCapacity - 1;
        for (uint32_t i = 0; i < length; ++i) word[i] = text[wordStart + i];
        word[length] = '\0';
    }
    return wordStart;
}

static bool appendChar(char* output, uint32_t capacity, uint32_t& length, char value) {
    if (!output || length + 1 >= capacity) return false;
    output[length++] = value;
    output[length] = '\0';
    return true;
}

static bool punctuationNeedsNoSpace(char value) {
    return value == '(' || value == ')' || value == '[' || value == ']' || value == ',' ||
        value == '<' || value == '>' || value == '*' || value == '&' || value == ':';
}

static bool appendNormalizedText(char* output, uint32_t capacity, uint32_t& length,
                                 const char* text, uint32_t start, uint32_t end) {
    bool pendingSpace = false;
    for (uint32_t i = start; i < end; ++i) {
        const char value = text[i];
        if (isSpace(value)) { pendingSpace = true; continue; }
        const bool noSpace = punctuationNeedsNoSpace(value) ||
            (i + 1 < end && (text[i + 1] == ':' || text[i + 1] == ',' || text[i + 1] == ')' ||
                              text[i + 1] == ']' || text[i + 1] == '>'));
        if (pendingSpace && length > 0 && !noSpace && output[length - 1] != '(' &&
            output[length - 1] != '[' && output[length - 1] != '<' && output[length - 1] != ':' &&
            output[length - 1] != '*' && output[length - 1] != '&' && output[length - 1] != ',') {
            if (!appendChar(output, capacity, length, ' ')) return false;
        }
        pendingSpace = false;
        if (!appendChar(output, capacity, length, value)) return false;
    }
    return true;
}

static uint32_t matchingClose(const char* text, uint32_t start, uint32_t end,
                              char open, char close, bool* approximate) {
    uint32_t depth = 0;
    bool quoted = false;
    char quote = '\0';
    for (uint32_t i = start; i < end; ++i) {
        const char value = text[i];
        if (quoted) {
            if (value == '\\' && i + 1 < end) { ++i; continue; }
            if (value == quote) quoted = false;
            continue;
        }
        if (value == '\'' || value == '"') { quoted = true; quote = value; continue; }
        if (value == open) ++depth;
        else if (value == close && depth > 0) {
            if (--depth == 0) return i;
        }
    }
    if (approximate) *approximate = true;
    return end;
}

static bool hasTopLevelDefault(const char* text, uint32_t start, uint32_t end,
                               uint32_t* equalOffset, bool* approximate) {
    uint32_t parens = 0, brackets = 0, braces = 0, angles = 0;
    bool quoted = false;
    char quote = '\0';
    for (uint32_t i = start; i < end; ++i) {
        const char value = text[i];
        if (quoted) {
            if (value == '\\' && i + 1 < end) { ++i; continue; }
            if (value == quote) quoted = false;
            continue;
        }
        if (value == '\'' || value == '"') { quoted = true; quote = value; continue; }
        if (value == '(') ++parens;
        else if (value == ')' && parens > 0) --parens;
        else if (value == '[') ++brackets;
        else if (value == ']' && brackets > 0) --brackets;
        else if (value == '{') ++braces;
        else if (value == '}' && braces > 0) --braces;
        else if (value == '<') ++angles;
        else if (value == '>' && angles > 0) --angles;
        else if (value == '=' && parens == 0 && brackets == 0 && braces == 0 && angles == 0) {
            if (equalOffset) *equalOffset = i;
            return true;
        }
    }
    if (quoted || parens != 0 || brackets != 0 || braces != 0) {
        if (approximate) *approximate = true;
    }
    return false;
}

static bool removeSafeName(const char* input, uint32_t length, char* output,
                           uint32_t capacity, bool* approximate) {
    if (!input || !output || capacity == 0) return false;
    // First remove a function-pointer name such as (*callback).  The name is
    // safe to remove only when the surrounding declarator visibly contains a
    // pointer/reference operator.
    char working[kMaxParameterScratch] = {};
    const uint32_t bounded = length < sizeof(working) - 1 ? length : sizeof(working) - 1;
    for (uint32_t i = 0; i < bounded; ++i) working[i] = input[i];
    working[bounded] = '\0';
    if (length >= sizeof(working) && approximate) *approximate = true;
    uint32_t cursor = 0;
    while (cursor < bounded) {
        if (!isIdentifierStart(working[cursor])) { ++cursor; continue; }
        uint32_t wordEnd = cursor + 1;
        while (wordEnd < bounded && isIdentifierPart(working[wordEnd])) ++wordEnd;
        uint32_t next = wordEnd;
        while (next < bounded && isSpace(working[next])) ++next;
        if (next < bounded && working[next] == ')') {
            uint32_t prior = cursor;
            while (prior > 0 && isSpace(working[prior - 1])) --prior;
            bool pointerContext = false;
            for (uint32_t p = prior; p > 0 && prior - p < 32u; ) {
                --p;
                if (working[p] == '(') break;
                if (working[p] == '*' || working[p] == '&') { pointerContext = true; break; }
            }
            char previousWord[64] = {};
            const uint32_t previousStart = previousIdentifierStart(working, 0, cursor, previousWord, sizeof(previousWord));
            const bool hasPrevious = previousStart < cursor;
            const bool qualifierOnly = hasPrevious && (equalText(previousWord, "const") || equalText(previousWord, "volatile"));
            if (pointerContext || (hasPrevious && !qualifierOnly)) {
                for (uint32_t p = cursor; p < next; ++p) working[p] = ' ';
            }
        }
        if (next < bounded && working[next] == '[') {
            uint32_t bracket = next + 1;
            uint32_t depth = 1;
            while (bracket < bounded && depth > 0) {
                if (working[bracket] == '[') ++depth;
                else if (working[bracket] == ']') --depth;
                ++bracket;
            }
            if (depth != 0 && approximate) *approximate = true;
            char previousWord[64] = {};
            const uint32_t previousStart = previousIdentifierStart(working, 0, cursor, previousWord, sizeof(previousWord));
            if (previousStart < cursor && !(previousStart == 0 && equalText(previousWord, "const")))
                for (uint32_t p = cursor; p < wordEnd; ++p) working[p] = ' ';
        }
        cursor = wordEnd;
    }
    // Remove an ordinary final parameter name only when the lexical shape
    // gives us evidence that a type precedes it.  A single `T` remains
    // untouched because it may be an unnamed parameter type.
    uint32_t end = skipSpaceBack(working, 0, bounded);
    uint32_t nameStart = previousIdentifierStart(working, 0, end, nullptr, 0);
    if (nameStart < end) {
        char finalWord[64] = {};
        previousIdentifierStart(working, 0, end, finalWord, sizeof(finalWord));
        uint32_t previousEnd = nameStart;
        while (previousEnd > 0 && isSpace(working[previousEnd - 1])) --previousEnd;
        char previousWord[64] = {};
        const uint32_t previousStart = previousIdentifierStart(working, 0, previousEnd,
                                                               previousWord, sizeof(previousWord));
        const bool hasPrevious = previousStart < previousEnd;
        bool previousIsScope = previousStart > 0 && working[previousStart - 1] == ':';
        bool definitelyName = hasPrevious && !previousIsScope;
        if (definitelyName && hasPrevious && equalText(previousWord, "const") && previousStart == 0) {
            // `const T` is a type without a name unless there is an additional
            // declarator token; do not corrupt it.
            definitelyName = false;
        }
        if (definitelyName && hasPrevious && equalText(previousWord, "volatile") && previousStart == 0)
            definitelyName = false;
        if (definitelyName) {
            for (uint32_t p = nameStart; p < end; ++p) working[p] = ' ';
        }
    }
    output[0] = '\0';
    uint32_t outLength = 0;
    return appendNormalizedText(output, capacity, outLength, working, 0, bounded);
}

static bool normalizeParameters(const char* text, uint32_t start, uint32_t end,
                                char* output, uint32_t capacity, uint32_t* count,
                                bool* approximate) {
    if (!output || capacity == 0) return false;
    output[0] = '\0';
    uint32_t outputLength = 0;
    uint32_t partStart = start;
    uint32_t parens = 0, brackets = 0, braces = 0, angles = 0;
    bool quoted = false;
    char quote = '\0';
    uint32_t parameterCount = 0;
    for (uint32_t i = start; i <= end; ++i) {
        const char value = i < end ? text[i] : ',';
        if (quoted) {
            if (value == '\\' && i + 1 < end) { ++i; continue; }
            if (value == quote) quoted = false;
            continue;
        }
        if (value == '\'' || value == '"') { quoted = true; quote = value; continue; }
        if (value == '(') ++parens;
        else if (value == ')' && parens > 0) --parens;
        else if (value == '[') ++brackets;
        else if (value == ']' && brackets > 0) --brackets;
        else if (value == '{') ++braces;
        else if (value == '}' && braces > 0) --braces;
        else if (value == '<') ++angles;
        else if (value == '>' && angles > 0) --angles;
        if (value == ',' && parens == 0 && brackets == 0 && braces == 0 && angles == 0) {
            uint32_t pieceEnd = i;
            while (pieceEnd > partStart && isSpace(text[pieceEnd - 1])) --pieceEnd;
            uint32_t pieceStart = partStart;
            while (pieceStart < pieceEnd && isSpace(text[pieceStart])) ++pieceStart;
            if (pieceStart < pieceEnd) {
                uint32_t defaultOffset = pieceEnd;
                bool hasDefault = hasTopLevelDefault(text, pieceStart, pieceEnd, &defaultOffset, approximate);
                if (hasDefault) pieceEnd = defaultOffset;
                while (pieceEnd > pieceStart && isSpace(text[pieceEnd - 1])) --pieceEnd;
                char normalized[kMaxParameterScratch] = {};
                bool localApproximate = false;
                if (!removeSafeName(text + pieceStart, pieceEnd - pieceStart, normalized,
                                    sizeof(normalized), &localApproximate)) return false;
                if (localApproximate && approximate) *approximate = true;
                if (parameterCount > 0 && !appendChar(output, capacity, outputLength, ',')) return false;
                for (uint32_t p = 0; normalized[p] != '\0'; ++p)
                    if (!appendChar(output, capacity, outputLength, normalized[p])) return false;
                ++parameterCount;
            }
            partStart = i + 1;
        }
    }
    if (quoted || parens != 0 || brackets != 0 || braces != 0 || angles != 0)
        if (approximate) *approximate = true;
    if (count) *count = parameterCount;
    return true;
}

static bool startsWith(const char* text, const char* prefix) {
    if (!text || !prefix) return false;
    uint32_t i = 0;
    while (prefix[i] != '\0') { if (text[i] != prefix[i]) return false; ++i; }
    return true;
}

static bool classLike(SymbolKind kind) {
    return kind == SymbolKind::Class || kind == SymbolKind::Struct;
}

static bool compatibleKind(SymbolKind left, SymbolKind right, bool* mismatch) {
    if (mismatch) *mismatch = false;
    if (left == right) return true;
    if (classLike(left) && classLike(right)) { if (mismatch) *mismatch = true; return true; }
    return false;
}

static SymbolKind canonicalKind(SymbolKind kind) {
    return kind == SymbolKind::Struct ? SymbolKind::Class : kind;
}

static bool relationshipEligible(SymbolKind kind, SymbolDeclarationRole role) {
    if (role == SymbolDeclarationRole::Alias || role == SymbolDeclarationRole::Unknown) return false;
    return kind != SymbolKind::Namespace && kind != SymbolKind::Macro && kind != SymbolKind::Typedef &&
        kind != SymbolKind::UsingAlias;
}

static bool isStaticSymbol(const ProjectSymbol& symbol) {
    return symbol.symbol.kind == SymbolKind::StaticVariable || (symbol.symbol.flags & 4u) != 0;
}

static uint64_t hashText(uint64_t hash, const char* text) {
    if (!text) return hash;
    for (uint32_t i = 0; text[i] != '\0'; ++i) {
        hash ^= static_cast<uint8_t>(text[i]);
        hash *= 1099511628211ull;
    }
    return hash;
}

static bool keyEqual(const SymbolRelationshipKey& left, const SymbolRelationshipKey& right) {
    return left.kind == right.kind && equalText(left.name, right.name) &&
        equalText(left.qualifiedName, right.qualifiedName) &&
        equalText(left.normalizedSignature, right.normalizedSignature) &&
        equalText(left.containingScope, right.containingScope) && left.isStatic == right.isStatic &&
        left.isConstQualified == right.isConstQualified && left.isVolatileQualified == right.isVolatileQualified &&
        left.isLValueRefQualified == right.isLValueRefQualified && left.isRValueRefQualified == right.isRValueRefQualified &&
        left.isNoexcept == right.isNoexcept;
}

static void copyEndpoint(const SymbolRelationshipGraph* graph, uint32_t symbolIndex,
                         SymbolRelationshipEndpoint* output) {
    if (!output) return;
    *output = SymbolRelationshipEndpoint();
    if (!graph || !graph->relationships || !graph->database || symbolIndex >= SymbolDatabaseProjectSymbolCount(graph->database)) return;
    const ProjectSymbol* projectSymbol = SymbolDatabaseProjectSymbolAt(graph->database, symbolIndex);
    if (!projectSymbol) return;
    const ProjectSymbol& symbol = *projectSymbol;
    const char* path = SymbolDatabaseDocumentPath(graph->database, symbol.documentIndex);
    output->symbolId = SymbolRelationshipSymbolId(symbol, path);
    output->documentId = symbol.symbol.location.documentId;
    output->documentGeneration = symbol.symbol.location.generation;
    if (!relativePathForRoot(graph->projectRoot, path, output->relativePath, sizeof(output->relativePath)))
        copyText(output->relativePath, sizeof(output->relativePath), path);
    output->byteOffset = symbol.symbol.location.identifierOffset;
    output->line = symbol.symbol.location.line;
    output->column = symbol.symbol.location.column;
    output->identifierLength = symbol.symbol.location.identifierLength;
    output->symbolKind = symbol.symbol.kind;
    output->declarationRole = symbol.symbol.declarationRole;
    copyText(output->name, sizeof(output->name), symbol.symbol.name);
    copyText(output->qualifiedName, sizeof(output->qualifiedName), symbol.symbol.qualifiedName);
    SymbolRelationshipKey key = {};
    if (BuildSymbolRelationshipKey(symbol, path, &key))
        copyText(output->normalizedSignature, sizeof(output->normalizedSignature), key.normalizedSignature);
}

static bool directIncludeEvidence(const IncludeGraph* graph, const char* declarationPath, const char* definitionPath) {
    if (!graph || !declarationPath || !definitionPath) return false;
    for (uint32_t i = 0; i < graph->edgeCount; ++i) {
        const IncludeEdge* edge = IncludeGraphEdgeAt(graph, i);
        if (!edge || edge->resolution.state != IncludeResolutionState::Resolved) continue;
        if (equalText(edge->sourceRelativePath, definitionPath) &&
            equalText(edge->targetRelativePath, declarationPath)) return true;
    }
    return false;
}

static bool sameStem(const char* left, const char* right) {
    if (!left || !right) return false;
    const char* leftBase = BaseName(left);
    const char* rightBase = BaseName(right);
    char a[128] = {}, b[128] = {};
    copyText(a, sizeof(a), leftBase);
    copyText(b, sizeof(b), rightBase);
    const char* leftDot = nullptr;
    const char* rightDot = nullptr;
    for (uint32_t i = 0; a[i] != '\0'; ++i) if (a[i] == '.') leftDot = a + i;
    for (uint32_t i = 0; b[i] != '\0'; ++i) if (b[i] == '.') rightDot = b + i;
    if (!leftDot || !rightDot) return false;
    *const_cast<char*>(leftDot) = '\0';
    *const_cast<char*>(rightDot) = '\0';
    return equalText(a, b);
}

static bool endpointIsDefinition(const SymbolRelationshipEndpoint& endpoint) {
    return endpoint.declarationRole == SymbolDeclarationRole::Definition;
}

static int rolePriority(SymbolRelationshipConfidence confidence) {
    switch (confidence) {
    case SymbolRelationshipConfidence::Exact: return 4;
    case SymbolRelationshipConfidence::Strong: return 3;
    case SymbolRelationshipConfidence::Possible: return 2;
    default: return 1;
    }
}

static bool edgeBefore(const SymbolRelationship& left, const SymbolRelationship& right) {
    if (left.rankScore != right.rankScore) return left.rankScore > right.rankScore;
    if (rolePriority(left.confidence) != rolePriority(right.confidence))
        return rolePriority(left.confidence) > rolePriority(right.confidence);
    if (left.target.relativePath[0] != right.target.relativePath[0]) return left.target.relativePath[0] < right.target.relativePath[0];
    uint32_t i = 0;
    while (left.target.relativePath[i] != '\0' && right.target.relativePath[i] != '\0' &&
           left.target.relativePath[i] == right.target.relativePath[i]) ++i;
    if (left.target.relativePath[i] != right.target.relativePath[i]) return left.target.relativePath[i] < right.target.relativePath[i];
    if (left.target.line != right.target.line) return left.target.line < right.target.line;
    return left.target.column < right.target.column;
}

static void sortEdges(SymbolRelationship* edges, uint32_t count) {
    for (uint32_t i = 1; i < count; ++i) {
        SymbolRelationship value = edges[i];
        uint32_t j = i;
        while (j > 0 && edgeBefore(value, edges[j - 1])) { edges[j] = edges[j - 1]; --j; }
        edges[j] = value;
    }
}

static uint32_t findGroup(const SymbolRelationshipGraph* graph, const SymbolRelationshipKey& key) {
    if (!graph) return kInvalidIndex;
    for (uint32_t i = 0; i < graph->groupCount; ++i) if (keyEqual(graph->groups[i].key, key)) return i;
    return kInvalidIndex;
}

static bool addCandidateToGroup(SymbolRelationshipGraph* graph, SymbolRelationshipGroup* group,
                                uint32_t symbolIndex, SymbolDeclarationRole role) {
    if (!graph || !group) return false;
    // During collection counts are temporary.  The final flat arrays are
    // packed after all keys have been discovered.
    uint32_t* count = role == SymbolDeclarationRole::Definition ? &group->definitionCount :
        (role == SymbolDeclarationRole::ForwardDeclaration ? &group->forwardCount : &group->declarationCount);
    if (*count >= kRelationshipMaxEndpointsPerGroup) { group->truncated = true; graph->truncated = true; return true; }
    ++*count;
    (void)symbolIndex;
    return true;
}

static bool makeEdge(SymbolRelationshipGraph* graph, const IncludeGraph* includeGraph,
                     uint32_t declarationIndex, uint32_t definitionIndex,
                     SymbolRelationshipKind kind, const SymbolRelationshipKey& key,
                     bool groupAmbiguous) {
    if (!graph || graph->relationshipCount >= graph->relationshipCapacity ||
        graph->relationshipCount >= kRelationshipMaxEdges) { graph->truncated = true; return false; }
    const ProjectSymbol* declaration = SymbolDatabaseProjectSymbolAt(graph->database, declarationIndex);
    const ProjectSymbol* definition = SymbolDatabaseProjectSymbolAt(graph->database, definitionIndex);
    if (!declaration || !definition) return false;
    const char* declarationPath = SymbolDatabaseDocumentPath(graph->database, declaration->documentIndex);
    const char* definitionPath = SymbolDatabaseDocumentPath(graph->database, definition->documentIndex);
    SymbolRelationship& edge = graph->relationships[graph->relationshipCount];
    edge = SymbolRelationship();
    edge.relationshipId = static_cast<uint64_t>(graph->relationshipCount + 1u);
    edge.kind = kind;
    edge.source = SymbolRelationshipEndpoint();
    edge.target = SymbolRelationshipEndpoint();
    copyEndpoint(graph, kind == SymbolRelationshipKind::DefinitionToDeclaration || kind == SymbolRelationshipKind::DefinitionToForward ? definitionIndex : declarationIndex, &edge.source);
    copyEndpoint(graph, kind == SymbolRelationshipKind::DefinitionToDeclaration || kind == SymbolRelationshipKind::DefinitionToForward ? declarationIndex : definitionIndex, &edge.target);
    bool classMismatch = false;
    const bool kindCompatible = compatibleKind(declaration->symbol.kind, definition->symbol.kind, &classMismatch);
    const bool exactName = equalText(declaration->symbol.qualifiedName, definition->symbol.qualifiedName);
    const bool exactSignature = equalText(key.normalizedSignature, key.normalizedSignature) && key.signatureComplete && !key.lexicallyApproximate;
    edge.confidence = groupAmbiguous ? SymbolRelationshipConfidence::Ambiguous :
        (kindCompatible && exactName && exactSignature && !classMismatch ? SymbolRelationshipConfidence::Exact :
         (kindCompatible && exactName ? SymbolRelationshipConfidence::Strong : SymbolRelationshipConfidence::Possible));
    edge.reasonFlags = RelationshipReasonExactQualifiedName | RelationshipReasonCompatibleRole;
    if (kindCompatible && !classMismatch) edge.reasonFlags |= RelationshipReasonExactKind;
    if (classMismatch) edge.reasonFlags |= RelationshipReasonClassKeyMismatch;
    if (exactSignature) edge.reasonFlags |= RelationshipReasonExactSignature;
    else edge.reasonFlags |= RelationshipReasonApproximateSignature;
    if (equalText(declaration->symbol.container, definition->symbol.container)) edge.reasonFlags |= RelationshipReasonSameScope;
    if (directIncludeEvidence(includeGraph, declarationPath, definitionPath)) edge.reasonFlags |= RelationshipReasonDirectInclude;
    if (sameStem(declarationPath, definitionPath)) edge.reasonFlags |= RelationshipReasonHeaderSourceStem;
    if (groupAmbiguous) edge.reasonFlags |= RelationshipReasonMultipleCandidates;
    edge.rankScore = 0;
    if (exactName) edge.rankScore += 3000;
    if (exactSignature) edge.rankScore += 2500;
    if (kindCompatible) edge.rankScore += 900;
    if (equalText(declaration->symbol.container, definition->symbol.container)) edge.rankScore += 1200;
    if (edge.reasonFlags & RelationshipReasonDirectInclude) edge.rankScore += 500;
    if (edge.reasonFlags & RelationshipReasonHeaderSourceStem) edge.rankScore += 350;
    if (edge.confidence == SymbolRelationshipConfidence::Possible) edge.rankScore -= 700;
    if (groupAmbiguous) edge.rankScore -= 1000;
    edge.stale = declaration->symbol.location.generation == 0 || definition->symbol.location.generation == 0;
    if (edge.stale) { edge.reasonFlags |= RelationshipReasonStaleEndpoint; edge.rankScore -= 1500; }
    ++graph->relationshipCount;
    return true;
}

static bool buildKeyFromSignature(const ProjectSymbol& symbol, const char* path,
                                  SymbolRelationshipKey* output) {
    if (!output) return false;
    *output = SymbolRelationshipKey();
    output->kind = canonicalKind(symbol.symbol.kind);
    copyText(output->name, sizeof(output->name), symbol.symbol.name);
    copyText(output->qualifiedName, sizeof(output->qualifiedName), symbol.symbol.qualifiedName);
    copyText(output->containingScope, sizeof(output->containingScope), symbol.symbol.container);
    output->isStatic = isStaticSymbol(symbol);
    uint32_t parameterCount = 0;
    bool complete = true;
    bool approximate = false;
    const bool callable = symbol.symbol.kind == SymbolKind::Function || symbol.symbol.kind == SymbolKind::Method ||
        symbol.symbol.kind == SymbolKind::Constructor || symbol.symbol.kind == SymbolKind::Destructor;
    if (callable) {
        if (!NormalizeRelationshipSignature(symbol.symbol.signature, output->normalizedSignature,
                                            sizeof(output->normalizedSignature), &parameterCount,
                                            &complete, &approximate)) return false;
        const char* signature = symbol.symbol.signature;
        const uint32_t length = lengthOf(signature, kSymbolMaxSignatureBytes);
        for (uint32_t i = 0; i < length; ++i) {
            if (signature[i] == ')' && i + 1 < length) {
                uint32_t cursor = i + 1;
                while (cursor < length && isSpace(signature[cursor])) ++cursor;
                char suffix[128] = {};
                uint32_t suffixLength = 0;
                while (cursor < length && suffixLength + 1 < sizeof(suffix)) suffix[suffixLength++] = signature[cursor++];
                suffix[suffixLength] = '\0';
                for (uint32_t q = 0; suffix[q] != '\0'; ++q) {
                    if (startsWith(suffix + q, "const")) output->isConstQualified = true;
                    if (startsWith(suffix + q, "volatile")) output->isVolatileQualified = true;
                    if (startsWith(suffix + q, "noexcept")) output->isNoexcept = true;
                    if (suffix[q] == '&') {
                        if (q + 1 < suffixLength && suffix[q + 1] == '&') output->isRValueRefQualified = true;
                        else output->isLValueRefQualified = true;
                    }
                }
                break;
            }
        }
    } else {
        output->normalizedSignature[0] = '\0';
        complete = true;
    }
    output->parameterCount = parameterCount;
    output->signatureComplete = complete;
    output->lexicallyApproximate = approximate;
    (void)path;
    return true;
}

static uint32_t groupEndpointAt(const SymbolRelationshipGraph* graph, const SymbolRelationshipGroup& group,
                                SymbolDeclarationRole role, uint32_t index) {
    const uint32_t* values = role == SymbolDeclarationRole::Definition ? graph->definitions :
        (role == SymbolDeclarationRole::ForwardDeclaration ? graph->forwardDeclarations : graph->declarations);
    const uint32_t offset = role == SymbolDeclarationRole::Definition ? group.definitionOffset :
        (role == SymbolDeclarationRole::ForwardDeclaration ? group.forwardOffset : group.declarationOffset);
    const uint32_t count = role == SymbolDeclarationRole::Definition ? group.definitionCount :
        (role == SymbolDeclarationRole::ForwardDeclaration ? group.forwardCount : group.declarationCount);
    return values && index < count ? values[offset + index] : kInvalidIndex;
}

static bool graphBuildCollect(SymbolRelationshipGraphBuildOperation* operation, uint32_t budget) {
    if (!operation || !operation->database || !operation->buildingGraph) return false;
    const uint32_t count = SymbolDatabaseProjectSymbolCount(operation->database);
    while (budget-- > 0 && operation->collectIndex < count) {
        const uint32_t symbolIndex = operation->collectIndex++;
        if (symbolIndex >= operation->symbolGroupCapacity) { operation->buildingGraph->truncated = true; continue; }
        const ProjectSymbol* symbol = SymbolDatabaseProjectSymbolAt(operation->database, symbolIndex);
        const char* path = symbol ? SymbolDatabaseDocumentPath(operation->database, symbol->documentIndex) : nullptr;
        operation->symbolGroupIndices[symbolIndex] = kInvalidIndex;
        if (!symbol || !relationshipEligible(symbol->symbol.kind, symbol->symbol.declarationRole)) continue;
        SymbolRelationshipKey key = {};
        if (!BuildSymbolRelationshipKey(*symbol, path, &key)) continue;
        uint32_t groupIndex = findGroup(operation->buildingGraph, key);
        if (groupIndex == kInvalidIndex) {
            if (operation->buildingGraph->groupCount >= operation->buildingGraph->groupCapacity ||
                operation->buildingGraph->groupCount >= kRelationshipMaxGroups) {
                operation->buildingGraph->truncated = true;
                continue;
            }
            groupIndex = operation->buildingGraph->groupCount++;
            SymbolRelationshipGroup& group = operation->buildingGraph->groups[groupIndex];
            group = SymbolRelationshipGroup();
            group.key = key;
        }
        operation->symbolGroupIndices[symbolIndex] = groupIndex;
        SymbolRelationshipGroup& group = operation->buildingGraph->groups[groupIndex];
        if (!addCandidateToGroup(operation->buildingGraph, &group, symbolIndex, symbol->symbol.declarationRole)) return false;
    }
    if (operation->collectIndex >= count) operation->state = RelationshipGraphState::GroupingCandidates;
    return true;
}

static bool graphBuildPack(SymbolRelationshipGraphBuildOperation* operation, uint32_t budget) {
    if (!operation || !operation->database || !operation->buildingGraph) return false;
    SymbolRelationshipGraph* graph = operation->buildingGraph;
    const uint32_t symbolCount = SymbolDatabaseProjectSymbolCount(operation->database);
    if (operation->packIndex == 0) {
        graph->declarationCount = graph->definitionCount = graph->forwardCount = 0;
        uint32_t declarationCursor = 0, definitionCursor = 0, forwardCursor = 0;
        for (uint32_t i = 0; i < graph->groupCount; ++i) {
            SymbolRelationshipGroup& group = graph->groups[i];
            group.declarationOffset = declarationCursor;
            group.definitionOffset = definitionCursor;
            group.forwardOffset = forwardCursor;
            declarationCursor += group.declarationCount;
            definitionCursor += group.definitionCount;
            forwardCursor += group.forwardCount;
            group.declarationCount = 0;
            group.definitionCount = 0;
            group.forwardCount = 0;
        }
        if (declarationCursor > graph->declarationCapacity || definitionCursor > graph->definitionCapacity ||
            forwardCursor > graph->forwardCapacity) graph->truncated = true;
        graph->declarationCount = declarationCursor < graph->declarationCapacity ? declarationCursor : graph->declarationCapacity;
        graph->definitionCount = definitionCursor < graph->definitionCapacity ? definitionCursor : graph->definitionCapacity;
        graph->forwardCount = forwardCursor < graph->forwardCapacity ? forwardCursor : graph->forwardCapacity;
    }
    while (budget-- > 0 && operation->packIndex < symbolCount) {
        const uint32_t symbolIndex = operation->packIndex++;
        const uint32_t groupIndex = symbolIndex < operation->symbolGroupCapacity ? operation->symbolGroupIndices[symbolIndex] : kInvalidIndex;
        if (groupIndex == kInvalidIndex || groupIndex >= graph->groupCount) continue;
        const ProjectSymbol* symbol = SymbolDatabaseProjectSymbolAt(operation->database, symbolIndex);
        if (!symbol) continue;
        SymbolRelationshipGroup& group = graph->groups[groupIndex];
        if (symbol->symbol.declarationRole == SymbolDeclarationRole::Definition) {
            if (group.definitionCount < kRelationshipMaxEndpointsPerGroup && group.definitionOffset + group.definitionCount < graph->definitionCapacity)
                graph->definitions[group.definitionOffset + group.definitionCount++] = symbolIndex;
            else { group.truncated = true; graph->truncated = true; }
        } else if (symbol->symbol.declarationRole == SymbolDeclarationRole::ForwardDeclaration) {
            if (group.forwardCount < kRelationshipMaxEndpointsPerGroup && group.forwardOffset + group.forwardCount < graph->forwardCapacity)
                graph->forwardDeclarations[group.forwardOffset + group.forwardCount++] = symbolIndex;
            else { group.truncated = true; graph->truncated = true; }
        } else {
            if (group.declarationCount < kRelationshipMaxEndpointsPerGroup && group.declarationOffset + group.declarationCount < graph->declarationCapacity)
                graph->declarations[group.declarationOffset + group.declarationCount++] = symbolIndex;
            else { group.truncated = true; graph->truncated = true; }
        }
    }
    if (operation->packIndex >= symbolCount) operation->state = RelationshipGraphState::MatchingRelationships;
    return true;
}

static bool graphBuildMatch(SymbolRelationshipGraphBuildOperation* operation, uint32_t budget) {
    if (!operation || !operation->buildingGraph) return false;
    SymbolRelationshipGraph* graph = operation->buildingGraph;
    while (budget-- > 0 && operation->matchGroup < graph->groupCount) {
        SymbolRelationshipGroup& group = graph->groups[operation->matchGroup];
        if (group.definitionCount > 1) group.ambiguous = true;
        if (group.declarationCount + group.forwardCount > 1) group.ambiguous = group.ambiguous || false;
        while (operation->matchDeclaration < group.declarationCount + group.forwardCount && budget > 0) {
            const bool forward = operation->matchDeclaration >= group.declarationCount;
            const uint32_t local = forward ? operation->matchDeclaration - group.declarationCount : operation->matchDeclaration;
            const uint32_t declarationIndex = groupEndpointAt(graph, group,
                forward ? SymbolDeclarationRole::ForwardDeclaration : SymbolDeclarationRole::Declaration, local);
            while (operation->matchDefinition < group.definitionCount && budget > 0) {
                const uint32_t definitionIndex = groupEndpointAt(graph, group, SymbolDeclarationRole::Definition, operation->matchDefinition++);
                const SymbolRelationshipKind kind = forward ? SymbolRelationshipKind::ForwardToDefinition : SymbolRelationshipKind::DeclarationToDefinition;
                makeEdge(graph, operation->includeGraph, declarationIndex, definitionIndex, kind, group.key, group.ambiguous);
                --budget;
            }
            if (operation->matchDefinition >= group.definitionCount) { operation->matchDefinition = 0; ++operation->matchDeclaration; }
        }
        if (operation->matchDeclaration >= group.declarationCount + group.forwardCount) {
            ++operation->matchGroup;
            operation->matchDeclaration = 0;
            operation->matchDefinition = 0;
        }
    }
    if (operation->matchGroup >= graph->groupCount) {
        sortEdges(graph->relationships, graph->relationshipCount);
        graph->complete = true;
        operation->state = RelationshipGraphState::Completed;
    }
    return true;
}

static bool buildStep(SymbolRelationshipGraphBuildOperation* operation, uint32_t budget) {
    if (!operation) return false;
    if (operation->cancellationRequested) {
        operation->state = RelationshipGraphState::Cancelled;
        operation->error = RelationshipErrorCode::BuildCancelled;
        operation->terminalReported = true;
        return true;
    }
    switch (operation->state) {
    case RelationshipGraphState::CollectingSymbols: return graphBuildCollect(operation, budget);
    case RelationshipGraphState::GroupingCandidates: return graphBuildPack(operation, budget);
    case RelationshipGraphState::MatchingRelationships: return graphBuildMatch(operation, budget);
    default: return true;
    }
}

static uint32_t findSymbolEdges(const SymbolRelationshipGraph* graph, uint64_t symbolId,
                                bool definitions, bool forwards, SymbolRelationship* output,
                                uint32_t capacity) {
    if (!graph || !graph->complete) return 0;
    uint32_t count = 0;
    for (uint32_t i = 0; i < graph->relationshipCount; ++i) {
        const SymbolRelationship& edge = graph->relationships[i];
        if (edge.source.symbolId != symbolId && edge.target.symbolId != symbolId) continue;
        const bool sourceIsDefinition = endpointIsDefinition(edge.source);
        const bool targetIsDefinition = endpointIsDefinition(edge.target);
        const bool sourceIsForward = edge.source.declarationRole == SymbolDeclarationRole::ForwardDeclaration;
        const bool targetIsForward = edge.target.declarationRole == SymbolDeclarationRole::ForwardDeclaration;
        bool matches = false;
        if (definitions) matches = (edge.source.symbolId == symbolId && !sourceIsDefinition && targetIsDefinition) ||
            (edge.target.symbolId == symbolId && !targetIsDefinition && sourceIsDefinition);
        else if (forwards) matches = (edge.source.symbolId == symbolId && sourceIsForward) ||
            (edge.target.symbolId == symbolId && targetIsForward);
        else matches = (edge.source.symbolId == symbolId && edge.target.declarationRole == SymbolDeclarationRole::Declaration) ||
            (edge.target.symbolId == symbolId && edge.source.declarationRole == SymbolDeclarationRole::Declaration);
        if (!matches) continue;
        if (output && count < capacity) output[count] = edge;
        ++count;
    }
    return count;
}

} // namespace

const char* SymbolRelationshipKindName(SymbolRelationshipKind kind) {
    switch (kind) {
    case SymbolRelationshipKind::DeclarationToDefinition: return "DeclarationToDefinition";
    case SymbolRelationshipKind::DefinitionToDeclaration: return "DefinitionToDeclaration";
    case SymbolRelationshipKind::ForwardToDefinition: return "ForwardToDefinition";
    case SymbolRelationshipKind::DefinitionToForward: return "DefinitionToForward";
    case SymbolRelationshipKind::DeclarationGroup: return "DeclarationGroup";
    case SymbolRelationshipKind::DefinitionGroup: return "DefinitionGroup";
    default: return "PossibleRelationship";
    }
}

const char* SymbolRelationshipConfidenceName(SymbolRelationshipConfidence confidence) {
    switch (confidence) {
    case SymbolRelationshipConfidence::Exact: return "Exact";
    case SymbolRelationshipConfidence::Strong: return "Strong";
    case SymbolRelationshipConfidence::Possible: return "Possible";
    default: return "Ambiguous";
    }
}

const char* RelationshipGraphStateName(RelationshipGraphState state) {
    switch (state) {
    case RelationshipGraphState::Idle: return "Idle";
    case RelationshipGraphState::CollectingSymbols: return "CollectingSymbols";
    case RelationshipGraphState::GroupingCandidates: return "GroupingCandidates";
    case RelationshipGraphState::MatchingRelationships: return "MatchingRelationships";
    case RelationshipGraphState::Completed: return "Completed";
    case RelationshipGraphState::Cancelling: return "Cancelling";
    case RelationshipGraphState::Cancelled: return "Cancelled";
    default: return "Failed";
    }
}

const char* RelationshipErrorName(RelationshipErrorCode error) {
    switch (error) {
    case RelationshipErrorCode::None: return "RELATIONSHIP_NONE";
    case RelationshipErrorCode::NoProject: return "RELATIONSHIP_NO_PROJECT";
    case RelationshipErrorCode::IndexNotReady: return "RELATIONSHIP_INDEX_NOT_READY";
    case RelationshipErrorCode::ProjectStale: return "RELATIONSHIP_PROJECT_STALE";
    case RelationshipErrorCode::SymbolStale: return "RELATIONSHIP_SYMBOL_STALE";
    case RelationshipErrorCode::GraphStale: return "RELATIONSHIP_GRAPH_STALE";
    case RelationshipErrorCode::BuildCancelled: return "RELATIONSHIP_BUILD_CANCELLED";
    case RelationshipErrorCode::BuildTimeout: return "RELATIONSHIP_BUILD_TIMEOUT";
    case RelationshipErrorCode::KeyLimit: return "RELATIONSHIP_KEY_LIMIT";
    case RelationshipErrorCode::EndpointLimit: return "RELATIONSHIP_ENDPOINT_LIMIT";
    case RelationshipErrorCode::EdgeLimit: return "RELATIONSHIP_EDGE_LIMIT";
    case RelationshipErrorCode::SignatureTooLong: return "RELATIONSHIP_SIGNATURE_TOO_LONG";
    case RelationshipErrorCode::NormalizationApproximate: return "RELATIONSHIP_NORMALIZATION_APPROXIMATE";
    case RelationshipErrorCode::NoDefinition: return "RELATIONSHIP_NO_DEFINITION";
    case RelationshipErrorCode::NoDeclaration: return "RELATIONSHIP_NO_DECLARATION";
    case RelationshipErrorCode::MultipleDefinitions: return "RELATIONSHIP_MULTIPLE_DEFINITIONS";
    case RelationshipErrorCode::MultipleDeclarations: return "RELATIONSHIP_MULTIPLE_DECLARATIONS";
    case RelationshipErrorCode::Ambiguous: return "RELATIONSHIP_AMBIGUOUS";
    case RelationshipErrorCode::AlreadyAtDefinition: return "RELATIONSHIP_ALREADY_AT_DEFINITION";
    case RelationshipErrorCode::AlreadyAtDeclaration: return "RELATIONSHIP_ALREADY_AT_DECLARATION";
    case RelationshipErrorCode::PathInvalid: return "RELATIONSHIP_PATH_INVALID";
    case RelationshipErrorCode::PathOutsideProject: return "RELATIONSHIP_PATH_OUTSIDE_PROJECT";
    case RelationshipErrorCode::FileMissing: return "RELATIONSHIP_FILE_MISSING";
    case RelationshipErrorCode::LocationStale: return "RELATIONSHIP_LOCATION_STALE";
    case RelationshipErrorCode::ActivationFailed: return "RELATIONSHIP_ACTIVATION_FAILED";
    default: return "RELATIONSHIP_INTERNAL";
    }
}

uint64_t SymbolRelationshipSymbolId(const ProjectSymbol& symbol, const char* relativePath) {
    uint64_t hash = 1469598103934665603ull;
    hash = hashText(hash, relativePath);
    hash = hashText(hash, symbol.symbol.qualifiedName);
    hash = hashText(hash, symbol.symbol.signature);
    hash ^= symbol.symbol.location.documentId; hash *= 1099511628211ull;
    hash ^= symbol.symbol.location.identifierOffset; hash *= 1099511628211ull;
    hash ^= static_cast<uint32_t>(symbol.symbol.kind); hash *= 1099511628211ull;
    return hash == 0 ? 1 : hash;
}

bool NormalizeRelationshipSignature(const char* signature, char* output, uint32_t outputCapacity,
                                    uint32_t* parameterCount, bool* signatureComplete,
                                    bool* lexicallyApproximate) {
    if (parameterCount) *parameterCount = 0;
    if (signatureComplete) *signatureComplete = false;
    if (lexicallyApproximate) *lexicallyApproximate = false;
    if (!signature || !output || outputCapacity == 0) return false;
    output[0] = '\0';
    const uint32_t length = lengthOf(signature, kSymbolMaxSignatureBytes);
    uint32_t open = 0;
    while (open < length && signature[open] != '(') ++open;
    if (open >= length) {
        if (lexicallyApproximate) *lexicallyApproximate = true;
        copyText(output, outputCapacity, signature);
        return false;
    }
    bool approximate = false;
    const uint32_t close = matchingClose(signature, open, length, '(', ')', &approximate);
    if (close >= length) approximate = true;
    char name[128] = {};
    uint32_t nameLength = open;
    while (nameLength > 0 && isSpace(signature[nameLength - 1])) --nameLength;
    if (nameLength >= sizeof(name)) { nameLength = sizeof(name) - 1; approximate = true; }
    for (uint32_t i = 0; i < nameLength; ++i) name[i] = signature[i];
    name[nameLength] = '\0';
    uint32_t outLength = 0;
    if (!appendNormalizedText(output, outputCapacity, outLength, name, 0, nameLength)) return false;
    if (!appendChar(output, outputCapacity, outLength, '(')) return false;
    uint32_t count = 0;
    if (!normalizeParameters(signature, open + 1, close < length ? close : length, output + outLength,
                             outputCapacity - outLength, &count, &approximate)) return false;
    outLength += lengthOf(output + outLength, outputCapacity - outLength);
    if (!appendChar(output, outputCapacity, outLength, ')')) return false;
    if (close < length) {
        uint32_t suffixStart = close + 1;
        while (suffixStart < length && isSpace(signature[suffixStart])) ++suffixStart;
        if (suffixStart < length && !appendNormalizedText(output, outputCapacity, outLength,
                                                           signature, suffixStart, length)) return false;
    }
    if (parameterCount) *parameterCount = count;
    if (signatureComplete) *signatureComplete = !approximate && length < kSymbolMaxSignatureBytes - 1u;
    if (lexicallyApproximate) *lexicallyApproximate = approximate;
    return true;
}

bool SymbolRelationshipGraphHasRelationship(const SymbolRelationshipGraph* graph,
                                            uint64_t firstSymbolId, uint64_t secondSymbolId) {
    if (!graph || firstSymbolId == 0 || secondSymbolId == 0 || firstSymbolId == secondSymbolId) return false;
    for (uint32_t i = 0; i < graph->relationshipCount; ++i) {
        const SymbolRelationship& relationship = graph->relationships[i];
        const bool forward = relationship.source.symbolId == firstSymbolId &&
            relationship.target.symbolId == secondSymbolId;
        const bool reverse = relationship.source.symbolId == secondSymbolId &&
            relationship.target.symbolId == firstSymbolId;
        if (forward || reverse) return true;
    }
    return false;
}

bool BuildSymbolRelationshipKey(const ProjectSymbol& symbol, const char* relativePath,
                                SymbolRelationshipKey* output) {
    return buildKeyFromSignature(symbol, relativePath, output);
}

void SymbolRelationshipGraphStorageInit(SymbolRelationshipGraphStorage* storage,
                                        SymbolRelationshipGroup* groups, uint32_t groupCapacity,
                                        SymbolRelationship* relationships, uint32_t relationshipCapacity,
                                        uint32_t* declarations, uint32_t declarationCapacity,
                                        uint32_t* definitions, uint32_t definitionCapacity,
                                        uint32_t* forwardDeclarations, uint32_t forwardCapacity,
                                        uint32_t* symbolGroupIndices, uint32_t symbolGroupCapacity) {
    if (!storage) return;
    storage->groups = groups; storage->groupCapacity = groupCapacity;
    storage->relationships = relationships; storage->relationshipCapacity = relationshipCapacity;
    storage->declarations = declarations; storage->declarationCapacity = declarationCapacity;
    storage->definitions = definitions; storage->definitionCapacity = definitionCapacity;
    storage->forwardDeclarations = forwardDeclarations; storage->forwardCapacity = forwardCapacity;
    storage->symbolGroupIndices = symbolGroupIndices; storage->symbolGroupCapacity = symbolGroupCapacity;
}

void SymbolRelationshipGraphInit(SymbolRelationshipGraph* graph,
                                 const SymbolRelationshipGraphStorage* storage,
                                 const char* projectId, uint64_t projectGeneration,
                                 uint64_t symbolDatabaseGeneration) {
    if (!graph || !storage) return;
    *graph = SymbolRelationshipGraph();
    graph->graphId = projectGeneration == 0 ? 1 : projectGeneration;
    graph->projectGeneration = projectGeneration;
    graph->symbolDatabaseGeneration = symbolDatabaseGeneration;
    copyText(graph->projectIdText, sizeof(graph->projectIdText), projectId);
    graph->projectId = 1469598103934665603ull;
    graph->projectId = hashText(graph->projectId, projectId);
    graph->groups = storage->groups; graph->groupCapacity = storage->groupCapacity;
    graph->relationships = storage->relationships; graph->relationshipCapacity = storage->relationshipCapacity;
    graph->declarations = storage->declarations; graph->declarationCapacity = storage->declarationCapacity;
    graph->definitions = storage->definitions; graph->definitionCapacity = storage->definitionCapacity;
    graph->forwardDeclarations = storage->forwardDeclarations; graph->forwardCapacity = storage->forwardCapacity;
    graph->symbolGroupIndices = storage->symbolGroupIndices;
    graph->symbolGroupCapacity = storage->symbolGroupCapacity;
}

void SymbolRelationshipGraphClear(SymbolRelationshipGraph* graph) {
    if (!graph) return;
    graph->groupCount = graph->relationshipCount = 0;
    graph->declarationCount = graph->definitionCount = graph->forwardCount = 0;
    graph->complete = false; graph->truncated = false;
}

bool SymbolRelationshipGraphIsCurrent(const SymbolRelationshipGraph* graph,
                                      const char* projectId, uint64_t projectGeneration,
                                      uint64_t symbolDatabaseGeneration) {
    if (!graph || !graph->complete) return false;
    uint64_t id = 1469598103934665603ull;
    id = hashText(id, projectId);
    return graph->projectId == id && graph->projectGeneration == projectGeneration &&
        graph->symbolDatabaseGeneration == symbolDatabaseGeneration;
}

void SymbolRelationshipGraphServiceInit(SymbolRelationshipGraphService* service,
                                        SymbolRelationshipGraph* completedGraph,
                                        SymbolRelationshipGraph* buildingGraph) {
    if (!service) return;
    *service = SymbolRelationshipGraphService();
    service->completedGraph = completedGraph;
    service->buildingGraph = buildingGraph;
    service->nextOperationId = 1;
    if (completedGraph) completedGraph->complete = false;
    if (buildingGraph) buildingGraph->complete = false;
    service->operation.state = RelationshipGraphState::Idle;
}

bool SymbolRelationshipGraphBuildStart(SymbolRelationshipGraphService* service,
                                       const SymbolDatabase* database,
                                       const IncludeGraph* includeGraph,
                                       const char* projectId, uint64_t projectGeneration,
                                       const char* projectRoot,
                                       uint64_t nowMs, uint64_t* operationId) {
    (void)nowMs;
    if (!service || !database || !service->buildingGraph || !service->completedGraph) return false;
    if (SymbolRelationshipGraphBuildIsActive(service)) {
        service->operation.cancellationRequested = true;
        service->operation.state = RelationshipGraphState::Cancelled;
    }
    SymbolRelationshipGraphClear(service->buildingGraph);
    service->buildingGraph->projectGeneration = projectGeneration;
    service->buildingGraph->symbolDatabaseGeneration = database->symbolDatabaseGeneration;
    copyText(service->buildingGraph->projectIdText, sizeof(service->buildingGraph->projectIdText), projectId);
    service->buildingGraph->projectId = hashText(1469598103934665603ull, projectId);
    service->buildingGraph->graphId = service->buildingGraph->graphId == UINT64_MAX ? 1 : service->buildingGraph->graphId + 1;
    service->operation = SymbolRelationshipGraphBuildOperation();
    service->operation.operationId = service->nextOperationId++;
    service->operation.state = RelationshipGraphState::CollectingSymbols;
    service->operation.database = database;
    service->operation.includeGraph = includeGraph;
    service->operation.buildingGraph = service->buildingGraph;
    service->buildingGraph->database = database;
    copyText(service->buildingGraph->projectRoot, sizeof(service->buildingGraph->projectRoot), projectRoot);
    service->operation.projectGeneration = projectGeneration;
    service->operation.symbolDatabaseGeneration = database->symbolDatabaseGeneration;
    copyText(service->operation.projectId, sizeof(service->operation.projectId), projectId);
    service->operation.symbolGroupCapacity = service->buildingGraph->symbolGroupCapacity;
    service->operation.symbolGroupIndices = service->buildingGraph->symbolGroupIndices;
    if (operationId) *operationId = service->operation.operationId;
    return service->operation.symbolGroupIndices != nullptr &&
        service->operation.symbolGroupCapacity >= database->projectSymbolCount;
}

bool SymbolRelationshipGraphBuildPoll(SymbolRelationshipGraphService* service,
                                      uint64_t operationId, uint32_t workBudget,
                                      uint64_t nowMs) {
    (void)nowMs;
    if (!service || service->operation.operationId != operationId || !SymbolRelationshipGraphBuildIsActive(service)) return false;
    if (!service->operation.symbolGroupIndices) return false;
    if (workBudget == 0) workBudget = 1;
    if (!buildStep(&service->operation, workBudget)) {
        service->operation.state = RelationshipGraphState::Failed;
        service->operation.error = RelationshipErrorCode::Internal;
        return false;
    }
    if (service->operation.state == RelationshipGraphState::Completed) {
        SymbolRelationshipGraph* oldCompleted = service->completedGraph;
        service->completedGraph = service->buildingGraph;
        service->buildingGraph = oldCompleted;
        service->completedGraph->complete = true;
        service->operation.state = RelationshipGraphState::Completed;
        service->operation.terminalReported = true;
    }
    return true;
}

bool SymbolRelationshipGraphBuildCancel(SymbolRelationshipGraphService* service,
                                        uint64_t operationId) {
    if (!service || service->operation.operationId != operationId || !SymbolRelationshipGraphBuildIsActive(service)) return false;
    service->operation.cancellationRequested = true;
    service->operation.state = RelationshipGraphState::Cancelling;
    return true;
}

bool SymbolRelationshipGraphBuildIsActive(const SymbolRelationshipGraphService* service) {
    if (!service) return false;
    return service->operation.state == RelationshipGraphState::CollectingSymbols ||
        service->operation.state == RelationshipGraphState::GroupingCandidates ||
        service->operation.state == RelationshipGraphState::MatchingRelationships ||
        service->operation.state == RelationshipGraphState::Cancelling;
}

const SymbolRelationshipGraphBuildOperation* SymbolRelationshipGraphBuildInfo(
    const SymbolRelationshipGraphService* service) {
    return service ? &service->operation : nullptr;
}

uint32_t SymbolRelationshipGraphFindDefinitions(const SymbolRelationshipGraph* graph,
                                                uint64_t symbolId, SymbolRelationship* output,
                                                uint32_t capacity) {
    return findSymbolEdges(graph, symbolId, true, false, output, capacity);
}

uint32_t SymbolRelationshipGraphFindDeclarations(const SymbolRelationshipGraph* graph,
                                                 uint64_t symbolId, SymbolRelationship* output,
                                                 uint32_t capacity) {
    return findSymbolEdges(graph, symbolId, false, false, output, capacity);
}

uint32_t SymbolRelationshipGraphFindForwards(const SymbolRelationshipGraph* graph,
                                             uint64_t symbolId, SymbolRelationship* output,
                                             uint32_t capacity) {
    return findSymbolEdges(graph, symbolId, false, true, output, capacity);
}

const SymbolRelationshipGroup* SymbolRelationshipGraphGroupAt(const SymbolRelationshipGraph* graph,
                                                               uint32_t index) {
    return graph && index < graph->groupCount ? &graph->groups[index] : nullptr;
}

const SymbolRelationship* SymbolRelationshipGraphRelationshipAt(const SymbolRelationshipGraph* graph,
                                                                 uint32_t index) {
    return graph && index < graph->relationshipCount ? &graph->relationships[index] : nullptr;
}

} // namespace developer_studio
} // namespace guidexos
