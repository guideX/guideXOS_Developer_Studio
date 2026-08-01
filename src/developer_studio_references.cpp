#include "developer_studio_references.h"

#include "developer_studio_syntax.h"

namespace guidexos {
namespace developer_studio {
namespace {

static uint32_t lengthOf(const char* text, uint32_t limit) {
    if (!text) return 0;
    uint32_t length = 0;
    while (length < limit && text[length] != '\0') ++length;
    return length;
}

static void clearBytes(void* value, uint32_t size) {
    if (!value) return;
    char* bytes = static_cast<char*>(value);
    for (uint32_t i = 0; i < size; ++i) bytes[i] = 0;
}

static void copyText(char* output, uint32_t outputSize, const char* input) {
    if (!output || outputSize == 0) return;
    uint32_t i = 0;
    if (input) while (i + 1 < outputSize && input[i] != '\0') { output[i] = input[i]; ++i; }
    output[i] = '\0';
}

static char lowerAscii(char value) {
    return value >= 'A' && value <= 'Z' ? static_cast<char>(value + ('a' - 'A')) : value;
}

static bool equalText(const char* left, const char* right, bool caseSensitive) {
    if (!left || !right) return left == right;
    uint32_t i = 0;
    while (left[i] != '\0' && right[i] != '\0') {
        const char a = caseSensitive ? left[i] : lowerAscii(left[i]);
        const char b = caseSensitive ? right[i] : lowerAscii(right[i]);
        if (a != b) return false;
        ++i;
    }
    return left[i] == right[i];
}

static uint64_t hashBytes(uint64_t hash, const char* text) {
    if (!text) return hash;
    for (uint32_t i = 0; text[i] != '\0'; ++i) {
        hash ^= static_cast<unsigned char>(text[i]);
        hash *= 1099511628211ull;
    }
    return hash;
}

static uint64_t hashProjectId(const char* text) {
    const uint64_t hash = hashBytes(1469598103934665603ull, text);
    return hash == 0 ? 1 : hash;
}

static void hashUnsigned(uint64_t* hash, uint64_t value) {
    if (!hash) return;
    for (uint32_t i = 0; i < 8; ++i) {
        *hash ^= static_cast<unsigned char>(value & 0xffu);
        *hash *= 1099511628211ull;
        value >>= 8;
    }
}

static uint64_t targetIdentity(const ReferenceTarget& target) {
    uint64_t hash = 1469598103934665603ull;
    hash = hashBytes(hash, target.projectIdText);
    hash = hashBytes(hash, target.qualifiedName[0] ? target.qualifiedName : target.identifier);
    hash = hashBytes(hash, target.signature);
    hash = hashBytes(hash, target.containingScope);
    hashUnsigned(&hash, static_cast<uint64_t>(target.kind));
    if (!target.hasQualifiedIdentity) {
        hash = hashBytes(hash, target.declarationPath);
        hashUnsigned(&hash, target.declarationByteOffset);
    }
    return hash == 0 ? 1 : hash;
}

static bool isWithinRoot(const char* root, const char* path) {
    char normalizedRoot[kMaxPathBytes] = {};
    char normalizedPath[kMaxPathBytes] = {};
    if (!root || !path || !NormalizePath(root, normalizedRoot, sizeof(normalizedRoot)) ||
        !NormalizePath(path, normalizedPath, sizeof(normalizedPath))) return false;
    const uint32_t rootLength = lengthOf(normalizedRoot, sizeof(normalizedRoot));
    const uint32_t pathLength = lengthOf(normalizedPath, sizeof(normalizedPath));
    if (rootLength > pathLength) return false;
    for (uint32_t i = 0; i < rootLength; ++i)
        if (lowerAscii(normalizedRoot[i]) != lowerAscii(normalizedPath[i])) return false;
    return rootLength == pathLength || normalizedPath[rootLength] == '/';
}

static bool relativePathFor(const char* root, const char* path, char* output, uint32_t outputSize) {
    if (!root || !path || !output || !isWithinRoot(root, path)) return false;
    char normalizedRoot[kMaxPathBytes] = {};
    char normalizedPath[kMaxPathBytes] = {};
    if (!NormalizePath(root, normalizedRoot, sizeof(normalizedRoot)) ||
        !NormalizePath(path, normalizedPath, sizeof(normalizedPath))) return false;
    const uint32_t rootLength = lengthOf(normalizedRoot, sizeof(normalizedRoot));
    const uint32_t pathLength = lengthOf(normalizedPath, sizeof(normalizedPath));
    if (pathLength <= rootLength) return false;
    uint32_t start = rootLength;
    if (normalizedPath[start] == '/') ++start;
    if (start >= pathLength || lengthOf(normalizedPath + start, outputSize) >= outputSize) return false;
    copyText(output, outputSize, normalizedPath + start);
    return true;
}

static bool isIdentifierStart(char value) {
    return (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z') || value == '_';
}

static bool isIdentifierContinue(char value) {
    return isIdentifierStart(value) || (value >= '0' && value <= '9');
}

static bool isSpace(char value) {
    return value == ' ' || value == '\t' || value == '\r' || value == '\f' || value == '\v';
}

static uint32_t previousNonSpace(const char* data, uint32_t start) {
    uint32_t cursor = start;
    while (cursor > 0 && isSpace(data[cursor - 1])) --cursor;
    return cursor == 0 ? 0 : cursor - 1;
}

static uint32_t nextNonSpace(const char* data, uint32_t length, uint32_t start) {
    uint32_t cursor = start;
    while (cursor < length && isSpace(data[cursor])) ++cursor;
    return cursor;
}

static bool tokenEquals(const char* data, uint32_t offset, uint32_t length, const char* value) {
    if (!data || !value) return false;
    const uint32_t valueLength = lengthOf(value, kDefinitionMaxIdentifierBytes + 1u);
    if (valueLength != length) return false;
    for (uint32_t i = 0; i < length; ++i) if (data[offset + i] != value[i]) return false;
    return true;
}

static bool isTypeLike(SymbolKind kind) {
    return kind == SymbolKind::Class || kind == SymbolKind::Struct || kind == SymbolKind::Enum ||
        kind == SymbolKind::Union || kind == SymbolKind::Typedef || kind == SymbolKind::UsingAlias;
}

static uint32_t lastQualifierSeparator(const char* qualifiedName) {
    uint32_t result = 0;
    if (!qualifiedName) return 0;
    for (uint32_t i = 0; qualifiedName[i] != '\0' && qualifiedName[i + 1] != '\0'; ++i)
        if (qualifiedName[i] == ':' && qualifiedName[i + 1] == ':') result = i;
    return result;
}

static void captureQualifier(const char* data, uint32_t length, uint32_t identifierStart,
                             char* output, uint32_t outputSize) {
    if (!output || outputSize == 0) return;
    output[0] = '\0';
    if (!data || identifierStart > length) return;
    uint32_t cursor = identifierStart;
    uint32_t qualifierStart = identifierStart;
    bool found = false;
    while (cursor >= 2) {
        uint32_t probe = cursor;
        while (probe > 0 && isSpace(data[probe - 1])) --probe;
        if (probe < 2 || data[probe - 1] != ':' || data[probe - 2] != ':') break;
        probe -= 2;
        while (probe > 0 && isSpace(data[probe - 1])) --probe;
        const uint32_t identifierEnd = probe;
        while (probe > 0 && isIdentifierContinue(data[probe - 1])) --probe;
        if (probe == identifierEnd) break;
        qualifierStart = probe;
        cursor = probe;
        found = true;
    }
    if (!found) return;
    uint32_t outputLength = 0;
    for (uint32_t i = qualifierStart; i < identifierStart && outputLength + 1 < outputSize; ++i) {
        if (isSpace(data[i])) continue;
        if (isIdentifierContinue(data[i]) || data[i] == ':') output[outputLength++] = data[i];
    }
    output[outputLength] = '\0';
}

static bool qualifierMatchesTarget(const ReferenceTarget& target, const char* qualifier) {
    if (!qualifier || qualifier[0] == '\0' || target.qualifiedName[0] == '\0') return false;
    const uint32_t separator = lastQualifierSeparator(target.qualifiedName);
    if (separator == 0) return false;
    char parent[kReferenceMaxQualifiedNameBytes + 1] = {};
    for (uint32_t i = 0; i < separator && i + 1 < sizeof(parent); ++i) parent[i] = target.qualifiedName[i];
    return equalText(parent, qualifier, true);
}

static bool sameScope(const ReferenceTarget& target, const char* scope) {
    return target.containingScope[0] != '\0' && scope && scope[0] != '\0' &&
        equalText(target.containingScope, scope, true);
}

static bool directiveInfo(const char* line, uint32_t length, char* name, uint32_t nameSize,
                          bool* ifZero) {
    if (name && nameSize) name[0] = '\0';
    if (ifZero) *ifZero = false;
    uint32_t first = 0;
    while (first < length && isSpace(line[first])) ++first;
    if (first >= length || line[first] != '#') return false;
    uint32_t cursor = first + 1;
    while (cursor < length && isSpace(line[cursor])) ++cursor;
    const uint32_t wordStart = cursor;
    while (cursor < length && isIdentifierContinue(line[cursor])) ++cursor;
    if (name && nameSize) {
        uint32_t out = 0;
        for (uint32_t i = wordStart; i < cursor && out + 1 < nameSize; ++i) name[out++] = line[i];
        name[out] = '\0';
    }
    if (ifZero && equalText(name, "if", true)) {
        while (cursor < length && isSpace(line[cursor])) ++cursor;
        *ifZero = cursor < length && line[cursor] == '0' &&
            (cursor + 1 == length || !isIdentifierContinue(line[cursor + 1]));
    }
    return true;
}

static ReferenceSearchErrorCode mapProjectError(ProjectSearchErrorCode error) {
    switch (error) {
    case ProjectSearchErrorCode::NoProject: return ReferenceSearchErrorCode::NoProject;
    case ProjectSearchErrorCode::EmptyQuery: return ReferenceSearchErrorCode::NoIdentifier;
    case ProjectSearchErrorCode::QueryTooLong: return ReferenceSearchErrorCode::IdentifierTooLong;
    case ProjectSearchErrorCode::InvalidRoot: return ReferenceSearchErrorCode::PathInvalid;
    case ProjectSearchErrorCode::EnumerationFailed: return ReferenceSearchErrorCode::EnumerationFailed;
    case ProjectSearchErrorCode::PathOutsideProject: return ReferenceSearchErrorCode::PathOutsideProject;
    case ProjectSearchErrorCode::FileTooLarge: return ReferenceSearchErrorCode::FileTooLarge;
    case ProjectSearchErrorCode::FileReadFailed: return ReferenceSearchErrorCode::FileReadFailed;
    case ProjectSearchErrorCode::BytesLimit: return ReferenceSearchErrorCode::ByteLimit;
    case ProjectSearchErrorCode::FileLimit: return ReferenceSearchErrorCode::FileLimit;
    case ProjectSearchErrorCode::Timeout: return ReferenceSearchErrorCode::Timeout;
    case ProjectSearchErrorCode::Cancelled: return ReferenceSearchErrorCode::Cancelled;
    case ProjectSearchErrorCode::ResultLimit:
    case ProjectSearchErrorCode::MatchLimit: return ReferenceSearchErrorCode::ResultLimit;
    default: return ReferenceSearchErrorCode::Internal;
    }
}

static void setNotice(ReferenceSearchService* service, const char* value) {
    if (service) copyText(service->operation.notice, sizeof(service->operation.notice), value);
}

static void markReferenceTruncated(ReferenceSearchService* service, ReferenceSearchErrorCode error) {
    if (!service) return;
    service->operation.truncated = true;
    if (service->operation.error == ReferenceSearchErrorCode::None ||
        service->operation.error == ReferenceSearchErrorCode::LexicalFallback)
        service->operation.error = error;
    setNotice(service, "Reference results truncated.");
}

static int pathCompare(const char* left, const char* right) {
    uint32_t i = 0;
    while (left && right && left[i] != '\0' && right[i] != '\0') {
        const char a = lowerAscii(left[i]);
        const char b = lowerAscii(right[i]);
        if (a != b) return a < b ? -1 : 1;
        ++i;
    }
    const char a = left ? left[i] : '\0';
    const char b = right ? right[i] : '\0';
    return a == b ? 0 : (a < b ? -1 : 1);
}

static void clearResults(ReferenceSearchService* service) {
    if (!service) return;
    for (uint32_t i = 0; i < kReferenceMaxResultFiles; ++i) {
        service->groups[i].relativePath[0] = '\0';
        service->groups[i].firstMatchIndex = 0;
        service->groups[i].matchCount = 0;
    }
    service->operation.referencesFound = 0;
}

static int32_t findGroupByCount(const ReferenceSearchService* service, const char* path) {
    if (!service || !path) return -1;
    uint32_t groupCount = 0;
    for (uint32_t i = 0; i < kReferenceMaxResultFiles; ++i)
        if (service->groups[i].relativePath[0] != '\0') ++groupCount;
    for (uint32_t i = 0; i < groupCount; ++i)
        if (equalText(service->groups[i].relativePath, path, false)) return static_cast<int32_t>(i);
    return -1;
}

static int32_t findHint(const ReferenceSearchService* service, const char* path, uint32_t offset) {
    if (!service || !path) return -1;
    for (uint32_t i = 0; i < service->declarationHintCount; ++i)
        if (equalText(service->declarationHints[i].relativePath, path, false) &&
            service->declarationHints[i].identifierOffset == offset) return static_cast<int32_t>(i);
    return -1;
}

struct ReferenceScopeStack {
    char names[32][kReferenceMaxContainingScopeBytes + 1];
    uint32_t count;
};

static void scopeText(const ReferenceScopeStack& stack, char* output, uint32_t outputSize) {
    if (!output || outputSize == 0) return;
    output[0] = '\0';
    uint32_t out = 0;
    for (uint32_t i = 0; i < stack.count; ++i) {
        if (stack.names[i][0] == '\0') continue;
        if (out != 0 && out + 2 < outputSize) { output[out++] = ':'; output[out++] = ':'; }
        for (uint32_t j = 0; stack.names[i][j] != '\0' && out + 1 < outputSize; ++j)
            output[out++] = stack.names[i][j];
    }
    output[out] = '\0';
}

static void scopePushForBrace(ReferenceScopeStack* stack, const char* data, uint32_t braceOffset) {
    if (!stack || stack->count >= 32 || !data || braceOffset == 0) return;
    uint32_t end = braceOffset;
    while (end > 0 && isSpace(data[end - 1])) --end;
    uint32_t nameEnd = end;
    while (end > 0 && isIdentifierContinue(data[end - 1])) --end;
    const uint32_t nameStart = end;
    char name[kReferenceMaxContainingScopeBytes + 1] = {};
    if (nameStart != nameEnd) {
        uint32_t keywordEnd = nameStart;
        while (keywordEnd > 0 && isSpace(data[keywordEnd - 1])) --keywordEnd;
        uint32_t keywordStart = keywordEnd;
        while (keywordStart > 0 && isIdentifierContinue(data[keywordStart - 1])) --keywordStart;
        char keyword[16] = {};
        uint32_t keywordLength = keywordEnd - keywordStart;
        if (keywordLength >= sizeof(keyword)) keywordLength = sizeof(keyword) - 1;
        for (uint32_t i = 0; i < keywordLength; ++i) keyword[i] = data[keywordStart + i];
        if (equalText(keyword, "namespace", true) || equalText(keyword, "class", true) ||
            equalText(keyword, "struct", true) || equalText(keyword, "union", true) ||
            equalText(keyword, "enum", true)) {
            uint32_t length = nameEnd - nameStart;
            if (length >= sizeof(name)) length = sizeof(name) - 1;
            for (uint32_t i = 0; i < length; ++i) name[i] = data[nameStart + i];
        }
    }
    copyText(stack->names[stack->count++], sizeof(stack->names[0]), name);
}

static void scopePop(ReferenceScopeStack* stack) {
    if (stack && stack->count > 0) --stack->count;
}

static bool hintMatchesTarget(const ReferenceTarget& target, const ReferenceSymbolHint& hint) {
    if (!equalText(target.identifier, hint.identifier, true)) return false;
    if (target.hasQualifiedIdentity && !equalText(target.qualifiedName, hint.qualifiedName, true)) return false;
    if (target.hasSignature && target.signature[0] && hint.signature[0] &&
        !equalText(target.signature, hint.signature, true)) return false;
    return target.kind == hint.kind || target.kind == SymbolKind::Function || target.kind == SymbolKind::Method;
}

static ReferenceKind declarationKind(SymbolDeclarationRole role, SymbolKind kind) {
    if (role == SymbolDeclarationRole::Alias || kind == SymbolKind::Typedef || kind == SymbolKind::UsingAlias)
        return ReferenceKind::AliasDeclaration;
    if (role == SymbolDeclarationRole::Definition) return ReferenceKind::Definition;
    if (role == SymbolDeclarationRole::ForwardDeclaration) return ReferenceKind::ForwardDeclaration;
    if (role == SymbolDeclarationRole::Declaration) return ReferenceKind::Declaration;
    return ReferenceKind::Unknown;
}

static ReferenceKind classifyKind(const ReferenceTarget& target, const char* data, uint32_t length,
                                  uint32_t start, uint32_t tokenLength,
                                  const ReferenceSymbolHint* hint) {
    if (hint) return declarationKind(hint->role, hint->kind);
    const uint32_t before = previousNonSpace(data, start);
    const uint32_t after = nextNonSpace(data, length, start + tokenLength);
    const bool call = after < length && data[after] == '(';
    const bool member = before > 0 && ((data[before] == '.' && (before == 0 || data[before - 1] != '.')) ||
        (data[before] == '>' && before > 0 && data[before - 1] == '-') ||
        (data[before] == ':' && before > 0 && data[before - 1] == ':'));
    const bool address = before > 0 && data[before] == '&' && (before == 1 || data[before - 1] != '&');
    const bool write = after < length && (data[after] == '=' ||
        (data[after] == '+' && after + 1 < length && data[after + 1] == '=') ||
        (data[after] == '-' && after + 1 < length && data[after + 1] == '=') ||
        (data[after] == '+' && after + 1 < length && data[after + 1] == '+') ||
        (data[after] == '-' && after + 1 < length && data[after + 1] == '-'));
    if (target.kind == SymbolKind::Namespace) return ReferenceKind::NamespaceUse;
    if (isTypeLike(target.kind)) return ReferenceKind::TypeUse;
    if (target.kind == SymbolKind::Method || target.kind == SymbolKind::Constructor ||
        target.kind == SymbolKind::Destructor) {
        if (member && call) return ReferenceKind::MethodCall;
        if (member) return ReferenceKind::MemberAccess;
        if (address) return ReferenceKind::AddressUse;
        if (call) return ReferenceKind::FunctionCall;
        return ReferenceKind::Unknown;
    }
    if (target.kind == SymbolKind::Function) {
        if (address) return ReferenceKind::AddressUse;
        if (call) return ReferenceKind::FunctionCall;
        return ReferenceKind::Unknown;
    }
    if (member) return ReferenceKind::MemberAccess;
    if (write) return ReferenceKind::PossibleWrite;
    return ReferenceKind::VariableUse;
}

static ReferenceConfidence classifyConfidence(const ReferenceTarget& target, const char* qualifier,
                                              const char* scope, bool member, int32_t hintIndex) {
    if (target.lexicallyAmbiguous) return ReferenceConfidence::LexicalOnly;
    if (hintIndex >= 0) return ReferenceConfidence::Exact;
    if (qualifier && qualifier[0]) return qualifierMatchesTarget(target, qualifier) ?
        ReferenceConfidence::Exact : ReferenceConfidence::Ambiguous;
    if (member && (target.kind == SymbolKind::Method || target.kind == SymbolKind::Constructor ||
                   target.kind == SymbolKind::Destructor)) return ReferenceConfidence::Ambiguous;
    if (target.hasSignature && hintIndex < 0 &&
        (target.kind == SymbolKind::Function || target.kind == SymbolKind::Method ||
         target.kind == SymbolKind::Constructor || target.kind == SymbolKind::Destructor))
        return ReferenceConfidence::Ambiguous;
    if (sameScope(target, scope)) return ReferenceConfidence::Exact;
    if (target.containingScope[0] != '\0' && scope && scope[0] != '\0') return ReferenceConfidence::Ambiguous;
    return ReferenceConfidence::Likely;
}

static uint64_t matchIdentity(const ReferenceTarget& target, const char* path, uint64_t offset) {
    uint64_t hash = target.targetId == 0 ? 1469598103934665603ull : target.targetId;
    hash = hashBytes(hash, path);
    hashUnsigned(&hash, offset);
    return hash == 0 ? 1 : hash;
}

static bool appendMatch(ReferenceSearchService* service, const ProjectSearchScanFile& file,
                        uint64_t offset, uint32_t line, uint32_t column, ReferenceKind kind,
                        ReferenceConfidence confidence) {
    if (!service) return false;
    const int32_t existingGroup = findGroupByCount(service, file.relativePath);
    int32_t groupIndex = existingGroup;
    uint32_t groupCount = 0;
    for (uint32_t i = 0; i < kReferenceMaxResultFiles; ++i)
        if (service->groups[i].relativePath[0] != '\0') ++groupCount;
    if (groupIndex < 0) {
        if (groupCount >= kReferenceMaxResultFiles) {
            markReferenceTruncated(service, ReferenceSearchErrorCode::ResultLimit);
            return false;
        }
        groupIndex = static_cast<int32_t>(groupCount++);
        ReferenceFileGroup& group = service->groups[groupIndex];
        copyText(group.relativePath, sizeof(group.relativePath), file.relativePath);
        group.firstMatchIndex = static_cast<uint32_t>(service->operation.referencesFound);
        group.matchCount = 0;
        group.sourceKind = file.sourceKind;
        group.fileSize = file.fileSize;
        group.documentId = file.documentId;
        group.documentGeneration = file.documentGeneration;
    }
    ReferenceFileGroup& group = service->groups[groupIndex];
    if (group.matchCount >= kReferenceMaxMatchesPerFile ||
        service->operation.referencesFound >= kReferenceMaxTotalMatches) {
        markReferenceTruncated(service, ReferenceSearchErrorCode::ResultLimit);
        return false;
    }
    for (uint32_t i = 0; i < group.matchCount; ++i) {
        const ReferenceMatch& prior = service->matches[group.firstMatchIndex + i];
        if (prior.byteOffset == offset) return true;
    }
    ReferenceMatch& match = service->matches[service->operation.referencesFound];
    clearBytes(&match, sizeof(match));
    match.matchId = matchIdentity(service->operation.target, file.relativePath, offset);
    copyText(match.relativePath, sizeof(match.relativePath), file.relativePath);
    match.byteOffset = offset;
    match.line = line;
    match.column = column;
    match.identifierLength = lengthOf(service->operation.target.identifier, sizeof(service->operation.target.identifier));
    match.kind = kind;
    match.confidence = confidence;
    match.sourceDocumentId = file.documentId;
    match.sourceDocumentGeneration = file.documentGeneration;
    match.fromDirtySnapshot = file.sourceKind == ProjectSearchSourceKind::DirtyDocument;
    ProjectSearchPreview preview = {};
    if (ProjectSearchBuildPreview(file.data, file.length, offset, match.identifierLength, &preview)) {
        copyText(match.previewText, sizeof(match.previewText), preview.text);
        match.previewMatchStart = preview.matchStart;
        match.previewMatchLength = preview.matchLength;
        match.previewLeftTruncated = preview.leftTruncated;
        match.previewRightTruncated = preview.rightTruncated;
    }
    ++group.matchCount;
    ++service->operation.referencesFound;
    return true;
}

static bool scanReferenceFile(ReferenceSearchService* service, const ProjectSearchScanFile& file) {
    if (!service || !file.data || file.length > kProjectSearchMaxFileBytes) return false;
    const SyntaxLanguage language = DetectSyntaxLanguage(file.relativePath);
    if (language == SyntaxLanguage::None) return true;
            SyntaxLineState syntaxState = {};
    syntaxState.kind = SyntaxLineStateKind::Normal;
    uint32_t inactiveDepth = 0;
    ReferenceScopeStack scopes = {};
    uint32_t lineStart = 0;
    uint32_t line = 1;
    while (lineStart <= file.length) {
        uint32_t lineEnd = lineStart;
        while (lineEnd < file.length && file.data[lineEnd] != '\n') ++lineEnd;
        uint32_t logicalEnd = lineEnd;
        if (logicalEnd > lineStart && file.data[logicalEnd - 1] == '\r') --logicalEnd;
        const uint32_t lineLength = logicalEnd - lineStart;
        char directive[24] = {};
        bool ifZero = false;
        const bool isDirective = directiveInfo(file.data + lineStart, lineLength, directive,
                                               sizeof(directive), &ifZero);
        if (isDirective) {
            if (equalText(directive, "if", true) && ifZero) {
                ++inactiveDepth;
                syntaxState.kind = SyntaxLineStateKind::Normal;
            } else if (inactiveDepth > 0 && equalText(directive, "if", true)) {
                ++inactiveDepth;
            } else if (inactiveDepth > 0 && equalText(directive, "endif", true)) {
                --inactiveDepth;
                syntaxState.kind = SyntaxLineStateKind::Normal;
            } else if (inactiveDepth > 0 && equalText(directive, "else", true) && inactiveDepth == 1) {
                inactiveDepth = 0;
                syntaxState.kind = SyntaxLineStateKind::Normal;
            }
        } else if (inactiveDepth == 0) {
            SyntaxLineResult tokenResult = {};
            if (!SyntaxTokenizeLine(language, file.data + lineStart, lineLength, syntaxState,
                                    &tokenResult, service->tokenScratch, kSyntaxMaxTokensPerLine)) {
                service->operation.error = ReferenceSearchErrorCode::TokenLimit;
                markReferenceTruncated(service, ReferenceSearchErrorCode::TokenLimit);
                return false;
            }
            syntaxState = tokenResult.outputState;
            char scope[kReferenceMaxContainingScopeBytes + 1] = {};
            scopeText(scopes, scope, sizeof(scope));
            for (uint32_t i = 0; i < tokenResult.spanCount; ++i) {
                const SyntaxTokenSpan& span = service->tokenScratch[i];
                const uint32_t absoluteOffset = lineStart + span.start;
                if (span.kind == SyntaxTokenKind::Identifier) {
                    ++service->operation.tokensExamined;
                    if (service->operation.tokensExamined > kReferenceMaxTokensPerOperation) {
                        markReferenceTruncated(service, ReferenceSearchErrorCode::TokenLimit);
                        return false;
                    }
                    uint32_t matchOffset = absoluteOffset;
                    uint32_t matchLength = span.length;
                    bool targetMatch = tokenEquals(file.data, absoluteOffset, span.length,
                                                   service->operation.target.identifier);
                    const uint32_t targetLength = lengthOf(service->operation.target.identifier,
                                                           sizeof(service->operation.target.identifier));
                    if (!targetMatch && targetLength > 1 && service->operation.target.identifier[0] == '~' &&
                        span.length == targetLength - 1 && absoluteOffset > lineStart &&
                        file.data[absoluteOffset - 1] == '~' &&
                        tokenEquals(file.data, absoluteOffset, span.length,
                                    service->operation.target.identifier + 1)) {
                        targetMatch = true;
                        matchOffset = absoluteOffset - 1;
                        matchLength = targetLength;
                    }
                    if (targetMatch) {
                        const int32_t hintIndex = findHint(service, file.relativePath, matchOffset);
                        const ReferenceSymbolHint* hint = hintIndex >= 0 ?
                            &service->declarationHints[hintIndex] : nullptr;
                        if (service->operation.target.lexicallyAmbiguous) hint = nullptr;
                        if (hint && !hintMatchesTarget(service->operation.target, *hint)) hint = nullptr;
                        if (hint && !service->includeDeclarations && hint->role != SymbolDeclarationRole::Definition)
                            continue;
                        const uint32_t before = previousNonSpace(file.data, matchOffset);
                        const bool member = before > 0 &&
                            ((file.data[before] == '.' && (before == 0 || file.data[before - 1] != '.')) ||
                             (file.data[before] == '>' && before > 0 && file.data[before - 1] == '-') ||
                             (file.data[before] == ':' && before > 0 && file.data[before - 1] == ':'));
                        char qualifier[kReferenceMaxQualifiedNameBytes + 1] = {};
                        captureQualifier(file.data, file.length, matchOffset, qualifier, sizeof(qualifier));
                        const ReferenceKind kind = classifyKind(service->operation.target, file.data, file.length,
                                                                 matchOffset, matchLength, hint);
                        ReferenceConfidence confidence = classifyConfidence(service->operation.target, qualifier,
                                                                            scope, member, hint ? hintIndex : -1);
                        if (confidence == ReferenceConfidence::Ambiguous && !service->operation.target.lexicallyAmbiguous &&
                            !service->operation.target.hasQualifiedIdentity && service->operation.target.containingScope[0] == '\0')
                            confidence = ReferenceConfidence::Likely;
                        if (service->operation.target.lexicallyAmbiguous) confidence = ReferenceConfidence::LexicalOnly;
                        if (confidence == ReferenceConfidence::Ambiguous && !service->includeAmbiguous) continue;
                        if (service->operation.target.lexicallyAmbiguous) {
                            if (!appendMatch(service, file, matchOffset, line, matchOffset - lineStart + 1,
                                              ReferenceKind::LexicalMatch, ReferenceConfidence::LexicalOnly)) return false;
                        } else {
                            if (!appendMatch(service, file, matchOffset, line, matchOffset - lineStart + 1,
                                              kind, confidence)) return false;
                        }
                    }
                }
                if (span.kind == SyntaxTokenKind::Punctuation && span.length == 1 &&
                    file.data[absoluteOffset] == '}') scopePop(&scopes);
                if (span.kind == SyntaxTokenKind::Punctuation && span.length == 1 &&
                    file.data[absoluteOffset] == '{') scopePushForBrace(&scopes, file.data, absoluteOffset);
                scopeText(scopes, scope, sizeof(scope));
            }
        }
        if (lineEnd >= file.length) break;
        lineStart = lineEnd + 1;
        ++line;
    }
    return true;
}

static bool referenceVisitor(void* userData, const ProjectSearchScanFile* file) {
    return file && scanReferenceFile(static_cast<ReferenceSearchService*>(userData), *file);
}

static bool candidatePathFromDatabase(const SymbolDatabase* database, const char* root,
                                      const ProjectSymbol& symbol, char* output, uint32_t outputSize) {
    if (!database || !root || !output) return false;
    const char* absolute = SymbolDatabaseDocumentPath(database, symbol.documentIndex);
    return absolute && relativePathFor(root, absolute, output, outputSize);
}

static bool terminal(ReferenceSearchState state) {
    return state == ReferenceSearchState::Idle || state == ReferenceSearchState::Completed ||
        state == ReferenceSearchState::Cancelled || state == ReferenceSearchState::Failed;
}

static void sortGroups(ReferenceSearchService* service) {
    if (!service) return;
    uint32_t groupCount = 0;
    for (uint32_t i = 0; i < kReferenceMaxResultFiles; ++i)
        if (service->groups[i].relativePath[0] != '\0') ++groupCount;
    for (uint32_t i = 0; i < groupCount; ++i) for (uint32_t j = i + 1; j < groupCount; ++j) {
        if (pathCompare(service->groups[j].relativePath, service->groups[i].relativePath) < 0) {
            ReferenceFileGroup temp = service->groups[i];
            service->groups[i] = service->groups[j];
            service->groups[j] = temp;
        }
    }
}

static void copyHints(ReferenceSearchService* service, const ReferenceSearchRequest& request) {
    if (!service || !request.symbolDatabase) return;
    const SymbolDatabase& database = *request.symbolDatabase;
    bool overflow = false;
    for (uint32_t i = 0; i < database.projectSymbolCount; ++i) {
        const ProjectSymbol& symbol = database.projectSymbols[i];
        if (!equalText(symbol.symbol.name, request.target.identifier, true)) continue;
        if (service->declarationHintCount >= kReferenceMaxDeclarationHints) { overflow = true; break; }
        ReferenceSymbolHint hint = {};
        if (!candidatePathFromDatabase(&database, request.rootPath, symbol, hint.relativePath, sizeof(hint.relativePath))) continue;
        copyText(hint.identifier, sizeof(hint.identifier), symbol.symbol.name);
        copyText(hint.qualifiedName, sizeof(hint.qualifiedName), symbol.symbol.qualifiedName);
        copyText(hint.container, sizeof(hint.container), symbol.symbol.container);
        copyText(hint.signature, sizeof(hint.signature), symbol.symbol.signature);
        hint.documentId = symbol.symbol.location.documentId;
        hint.generation = symbol.symbol.location.generation;
        hint.identifierOffset = symbol.symbol.location.identifierOffset;
        hint.line = symbol.symbol.location.line;
        hint.column = symbol.symbol.location.column;
        hint.kind = symbol.symbol.kind;
        hint.role = symbol.symbol.declarationRole;
        if (request.target.hasQualifiedIdentity &&
            !equalText(request.target.qualifiedName, hint.qualifiedName, true)) continue;
        if (request.target.hasSignature && request.target.signature[0] && hint.signature[0] &&
            !equalText(request.target.signature, hint.signature, true)) continue;
        service->declarationHints[service->declarationHintCount++] = hint;
    }
    if (overflow) markReferenceTruncated(service, ReferenceSearchErrorCode::ResultLimit);
}

} // namespace

const char* ReferenceSearchStateName(ReferenceSearchState state) {
    switch (state) {
    case ReferenceSearchState::Idle: return "Idle";
    case ReferenceSearchState::ResolvingTarget: return "ResolvingTarget";
    case ReferenceSearchState::Enumerating: return "Enumerating";
    case ReferenceSearchState::Searching: return "Searching";
    case ReferenceSearchState::Cancelling: return "Cancelling";
    case ReferenceSearchState::Completed: return "Completed";
    case ReferenceSearchState::Cancelled: return "Cancelled";
    case ReferenceSearchState::Failed: return "Failed";
    default: return "Unknown";
    }
}

const char* ReferenceKindName(ReferenceKind kind) {
    switch (kind) {
    case ReferenceKind::Definition: return "Definition";
    case ReferenceKind::Declaration: return "Declaration";
    case ReferenceKind::ForwardDeclaration: return "ForwardDeclaration";
    case ReferenceKind::AliasDeclaration: return "AliasDeclaration";
    case ReferenceKind::FunctionCall: return "FunctionCall";
    case ReferenceKind::MethodCall: return "MethodCall";
    case ReferenceKind::TypeUse: return "TypeUse";
    case ReferenceKind::NamespaceUse: return "NamespaceUse";
    case ReferenceKind::MemberAccess: return "MemberAccess";
    case ReferenceKind::VariableUse: return "VariableUse";
    case ReferenceKind::PossibleWrite: return "PossibleWrite";
    case ReferenceKind::PossibleRead: return "PossibleRead";
    case ReferenceKind::AddressUse: return "AddressUse";
    case ReferenceKind::LexicalMatch: return "LexicalMatch";
    default: return "Unknown";
    }
}

const char* ReferenceConfidenceName(ReferenceConfidence confidence) {
    switch (confidence) {
    case ReferenceConfidence::Exact: return "Exact";
    case ReferenceConfidence::Likely: return "Likely";
    case ReferenceConfidence::Ambiguous: return "Ambiguous";
    case ReferenceConfidence::LexicalOnly: return "LexicalOnly";
    default: return "Unknown";
    }
}

const char* ReferenceSearchErrorName(ReferenceSearchErrorCode error) {
    switch (error) {
    case ReferenceSearchErrorCode::None: return "REFERENCES_NONE";
    case ReferenceSearchErrorCode::NoProject: return "REFERENCES_NO_PROJECT";
    case ReferenceSearchErrorCode::NoDocument: return "REFERENCES_NO_DOCUMENT";
    case ReferenceSearchErrorCode::NoIdentifier: return "REFERENCES_NO_IDENTIFIER";
    case ReferenceSearchErrorCode::IdentifierTooLong: return "REFERENCES_IDENTIFIER_TOO_LONG";
    case ReferenceSearchErrorCode::TargetNotFound: return "REFERENCES_TARGET_NOT_FOUND";
    case ReferenceSearchErrorCode::TargetAmbiguous: return "REFERENCES_TARGET_AMBIGUOUS";
    case ReferenceSearchErrorCode::LexicalFallback: return "REFERENCES_LEXICAL_FALLBACK";
    case ReferenceSearchErrorCode::IndexNotReady: return "REFERENCES_INDEX_NOT_READY";
    case ReferenceSearchErrorCode::ProjectStale: return "REFERENCES_PROJECT_STALE";
    case ReferenceSearchErrorCode::DocumentStale: return "REFERENCES_DOCUMENT_STALE";
    case ReferenceSearchErrorCode::EnumerationFailed: return "REFERENCES_ENUMERATION_FAILED";
    case ReferenceSearchErrorCode::PathInvalid: return "REFERENCES_PATH_INVALID";
    case ReferenceSearchErrorCode::PathOutsideProject: return "REFERENCES_PATH_OUTSIDE_PROJECT";
    case ReferenceSearchErrorCode::FileReadFailed: return "REFERENCES_FILE_READ_FAILED";
    case ReferenceSearchErrorCode::FileTooLarge: return "REFERENCES_FILE_TOO_LARGE";
    case ReferenceSearchErrorCode::BinaryFile: return "REFERENCES_BINARY_FILE";
    case ReferenceSearchErrorCode::ByteLimit: return "REFERENCES_BYTE_LIMIT";
    case ReferenceSearchErrorCode::TokenLimit: return "REFERENCES_TOKEN_LIMIT";
    case ReferenceSearchErrorCode::FileLimit: return "REFERENCES_FILE_LIMIT";
    case ReferenceSearchErrorCode::ResultLimit: return "REFERENCES_RESULT_LIMIT";
    case ReferenceSearchErrorCode::Timeout: return "REFERENCES_TIMEOUT";
    case ReferenceSearchErrorCode::Cancelled: return "REFERENCES_CANCELLED";
    case ReferenceSearchErrorCode::OperationStale: return "REFERENCES_OPERATION_STALE";
    case ReferenceSearchErrorCode::NoResults: return "REFERENCES_NO_RESULTS";
    case ReferenceSearchErrorCode::ActivationStale: return "REFERENCES_ACTIVATION_STALE";
    case ReferenceSearchErrorCode::ActivationFailed: return "REFERENCES_ACTIVATION_FAILED";
    default: return "REFERENCES_INTERNAL";
    }
}

const char* ReferenceTargetResolutionKindName(ReferenceTargetResolutionKind kind) {
    switch (kind) {
    case ReferenceTargetResolutionKind::Direct: return "DIRECT";
    case ReferenceTargetResolutionKind::Multiple: return "MULTIPLE";
    case ReferenceTargetResolutionKind::Stale: return "STALE";
    case ReferenceTargetResolutionKind::LexicalFallback: return "LEXICAL_FALLBACK";
    case ReferenceTargetResolutionKind::Failed: return "FAILED";
    default: return "NONE";
    }
}

void ReferenceTargetInit(ReferenceTarget* target) {
    if (!target) return;
    clearBytes(target, sizeof(*target));
    target->kind = SymbolKind::Function;
    target->role = SymbolDeclarationRole::Unknown;
}

bool ReferenceTargetFromDefinitionCandidate(const DefinitionCandidate& candidate,
                                            const char* projectId,
                                            uint64_t projectGeneration,
                                            ReferenceTarget* output) {
    if (!output || !projectId) return false;
    ReferenceTargetInit(output);
    output->projectId = hashProjectId(projectId);
    output->projectGeneration = projectGeneration;
    copyText(output->projectIdText, sizeof(output->projectIdText), projectId);
    copyText(output->identifier, sizeof(output->identifier), candidate.symbol.symbol.name);
    copyText(output->qualifiedName, sizeof(output->qualifiedName), candidate.symbol.symbol.qualifiedName);
    copyText(output->containingScope, sizeof(output->containingScope), candidate.symbol.symbol.container);
    copyText(output->signature, sizeof(output->signature), candidate.symbol.symbol.signature);
    output->kind = candidate.symbol.symbol.kind;
    output->role = candidate.symbol.symbol.declarationRole;
    copyText(output->declarationPath, sizeof(output->declarationPath), candidate.relativePath);
    output->declarationByteOffset = candidate.symbol.symbol.location.identifierOffset;
    output->declarationLine = candidate.symbol.symbol.location.line;
    output->declarationColumn = candidate.symbol.symbol.location.column;
    output->sourceDocumentId = candidate.symbol.symbol.location.documentId;
    output->sourceDocumentGeneration = candidate.symbol.symbol.location.generation;
    output->hasQualifiedIdentity = output->qualifiedName[0] != '\0';
    output->hasSignature = output->signature[0] != '\0';
    output->lexicallyAmbiguous = false;
    output->targetId = targetIdentity(*output);
    return output->identifier[0] != '\0' && output->targetId != 0;
}

bool ReferenceTargetFromDefinitionQuery(const DefinitionQuery& query,
                                        const char* projectId,
                                        uint64_t projectGeneration,
                                        ReferenceTarget* output) {
    if (!output || !projectId || query.identifier[0] == '\0') return false;
    ReferenceTargetInit(output);
    output->projectId = hashProjectId(projectId);
    output->projectGeneration = projectGeneration;
    output->sourceDocumentId = query.documentId;
    output->sourceDocumentGeneration = query.documentGeneration;
    copyText(output->projectIdText, sizeof(output->projectIdText), projectId);
    copyText(output->identifier, sizeof(output->identifier), query.identifier);
    copyText(output->declarationPath, sizeof(output->declarationPath), query.relativePath);
    output->declarationByteOffset = query.caretByteOffset;
    output->kind = query.likelyKind == SymbolKind::Macro ? SymbolKind::Function : query.likelyKind;
    output->role = SymbolDeclarationRole::Unknown;
    if (query.lexicalQualifier[0]) {
        copyText(output->qualifiedName, sizeof(output->qualifiedName), query.lexicalQualifier);
        uint32_t length = lengthOf(output->qualifiedName, sizeof(output->qualifiedName));
        if (length + 2 < sizeof(output->qualifiedName)) {
            output->qualifiedName[length++] = ':';
            output->qualifiedName[length++] = ':';
            copyText(output->qualifiedName + length, sizeof(output->qualifiedName) - length, query.identifier);
        }
    } else if (query.containingScope[0]) {
        copyText(output->containingScope, sizeof(output->containingScope), query.containingScope);
        copyText(output->qualifiedName, sizeof(output->qualifiedName), query.containingScope);
        uint32_t length = lengthOf(output->qualifiedName, sizeof(output->qualifiedName));
        if (length + 2 < sizeof(output->qualifiedName)) {
            output->qualifiedName[length++] = ':';
            output->qualifiedName[length++] = ':';
            copyText(output->qualifiedName + length, sizeof(output->qualifiedName) - length, query.identifier);
        }
    } else {
        copyText(output->qualifiedName, sizeof(output->qualifiedName), query.identifier);
    }
    output->hasQualifiedIdentity = query.lexicalQualifier[0] != '\0' || query.containingScope[0] != '\0';
    output->lexicallyAmbiguous = true;
    output->targetId = targetIdentity(*output);
    return output->targetId != 0;
}

bool ReferenceTargetIsValid(const ReferenceTarget* target) {
    return target && target->targetId != 0 && target->identifier[0] != '\0' &&
        lengthOf(target->identifier, sizeof(target->identifier)) <= kDefinitionMaxIdentifierBytes;
}

bool ResolveReferenceTarget(const SymbolDatabase* database, const DefinitionQuery& query,
                            DefinitionCandidate* candidates, uint32_t candidateCapacity,
                            bool allowLexicalFallback,
                            ReferenceTargetResolution* output) {
    if (!output) return false;
    clearBytes(output, sizeof(*output));
    output->queryId = query.queryId;
    DefinitionResolution resolution = {};
    if (!ResolveDefinition(database, query, candidates, candidateCapacity, &resolution)) {
        output->kind = ReferenceTargetResolutionKind::Failed;
        copyText(output->statusCode, sizeof(output->statusCode), "REFERENCES_INDEX_NOT_READY");
        return false;
    }
    output->candidateCount = resolution.candidateCount;
    output->visibleCandidateCount = resolution.visibleCandidateCount;
    output->truncated = resolution.truncated;
    if (resolution.kind == DefinitionResolutionKind::Direct) output->kind = ReferenceTargetResolutionKind::Direct;
    else if (resolution.kind == DefinitionResolutionKind::Multiple) output->kind = ReferenceTargetResolutionKind::Multiple;
    else if (resolution.kind == DefinitionResolutionKind::Stale) output->kind = ReferenceTargetResolutionKind::Stale;
    else if (resolution.kind == DefinitionResolutionKind::None && allowLexicalFallback && query.identifier[0]) {
        output->kind = ReferenceTargetResolutionKind::LexicalFallback;
        output->lexicalFallback = true;
        copyText(output->statusCode, sizeof(output->statusCode), "REFERENCES_LEXICAL_FALLBACK");
        return true;
    } else {
        output->kind = ReferenceTargetResolutionKind::None;
    }
    copyText(output->statusCode, sizeof(output->statusCode),
             output->kind == ReferenceTargetResolutionKind::Direct ? "REFERENCES_TARGET_DIRECT" :
             output->kind == ReferenceTargetResolutionKind::Multiple ? "REFERENCES_TARGET_AMBIGUOUS" :
             output->kind == ReferenceTargetResolutionKind::Stale ? "REFERENCES_PROJECT_STALE" :
             "REFERENCES_TARGET_NOT_FOUND");
    return true;
}

void ReferenceSearchServiceInit(ReferenceSearchService* service) {
    if (!service) return;
    ProjectSearchServiceInit(&service->scanner);
    clearBytes(&service->operation, sizeof(service->operation));
    service->operation.state = ReferenceSearchState::Idle;
    service->declarationHintCount = 0;
    service->includeDeclarations = true;
    service->includeAmbiguous = true;
    service->startedAtMs = 0;
    service->terminalReported = false;
    clearResults(service);
}

bool ReferenceSearchStart(ReferenceSearchService* service, const ReferenceSearchRequest* request,
                          uint64_t nowMs, uint64_t* outOperationId,
                          ReferenceSearchErrorCode* outError) {
    if (outOperationId) *outOperationId = 0;
    if (outError) *outError = ReferenceSearchErrorCode::None;
    if (!service || !request) { if (outError) *outError = ReferenceSearchErrorCode::Internal; return false; }
    const uint64_t nextId = service->operation.operationId == 0 ? 1 : service->operation.operationId + 1;
    ReferenceSearchServiceInit(service);
    service->operation.operationId = nextId == 0 ? 1 : nextId;
    service->operation.target = request->target;
    service->operation.lexicalFallback = request->lexicalFallback || request->target.lexicallyAmbiguous;
    service->operation.state = ReferenceSearchState::ResolvingTarget;
    service->startedAtMs = nowMs;
    service->includeDeclarations = request->includeDeclarations;
    service->includeAmbiguous = request->includeAmbiguous;
    copyHints(service, *request);
    ProjectSearchRequest scanRequest = {};
    copyText(scanRequest.projectId, sizeof(scanRequest.projectId), request->projectId);
    if (scanRequest.projectId[0] == '\0') copyText(scanRequest.projectId, sizeof(scanRequest.projectId), request->target.projectIdText);
    scanRequest.projectGeneration = request->projectGeneration;
    copyText(scanRequest.rootPath, sizeof(scanRequest.rootPath), request->rootPath);
    scanRequest.options = request->scanOptions;
    copyText(scanRequest.options.query, sizeof(scanRequest.options.query), request->target.identifier);
    scanRequest.fileSystem = request->fileSystem;
    scanRequest.dirtyDocuments = request->dirtyDocuments;
    scanRequest.dirtyDocumentCount = request->dirtyDocumentCount;
    scanRequest.scanVisitor = referenceVisitor;
    scanRequest.scanVisitorUserData = service;
    ProjectSearchErrorCode scanError = ProjectSearchErrorCode::None;
    uint64_t scannerOperationId = 0;
    if (!ReferenceTargetIsValid(&service->operation.target) || scanRequest.projectId[0] == '\0' ||
        request->projectGeneration == 0) {
        service->operation.state = ReferenceSearchState::Failed;
        service->operation.error = scanRequest.projectId[0] == '\0' ? ReferenceSearchErrorCode::NoProject :
            (service->operation.target.identifier[0] == '\0' ? ReferenceSearchErrorCode::NoIdentifier :
             ReferenceSearchErrorCode::Internal);
        if (outError) *outError = service->operation.error;
        if (outOperationId) *outOperationId = service->operation.operationId;
        return false;
    }
    if (!ProjectSearchStart(&service->scanner, &scanRequest, nowMs, &scannerOperationId, &scanError)) {
        service->operation.state = ReferenceSearchState::Failed;
        service->operation.error = mapProjectError(scanError);
        if (outError) *outError = service->operation.error;
        if (outOperationId) *outOperationId = service->operation.operationId;
        return false;
    }
    service->operation.state = ReferenceSearchState::Enumerating;
    if (service->operation.lexicalFallback) {
        service->operation.error = ReferenceSearchErrorCode::LexicalFallback;
        setNotice(service, "No indexed symbol was resolved. Showing lexical identifier matches.");
    } else setNotice(service, "");
    service->terminalReported = false;
    service->startedAtMs = nowMs;
    if (outOperationId) *outOperationId = service->operation.operationId;
    (void)scannerOperationId;
    return true;
}

bool ReferenceSearchPoll(ReferenceSearchService* service, uint64_t operationId,
                         uint32_t workBudget, uint64_t nowMs) {
    if (!service || operationId == 0 || service->operation.operationId != operationId ||
        service->operation.state == ReferenceSearchState::Idle) return false;
    if (terminal(service->operation.state)) return true;
    if (service->operation.cancellationRequested)
        ProjectSearchCancel(&service->scanner, service->scanner.operation.operationId);
    ProjectSearchPoll(&service->scanner, service->scanner.operation.operationId, workBudget, nowMs);
    const ProjectSearchOperation* scan = ProjectSearchOperationInfo(&service->scanner);
    if (!scan) return false;
    service->operation.filesEnumerated = scan->filesEnumerated;
    service->operation.filesSearched = scan->filesSearched;
    service->operation.bytesScanned = scan->bytesScanned;
    if (service->operation.state == ReferenceSearchState::Cancelling || scan->state == ProjectSearchState::Cancelling)
        service->operation.state = ReferenceSearchState::Cancelling;
    else if (scan->state == ProjectSearchState::Searching || scan->state == ProjectSearchState::Enumerating)
        service->operation.state = scan->state == ProjectSearchState::Searching ?
            ReferenceSearchState::Searching : ReferenceSearchState::Enumerating;
    if (ProjectSearchIsActive(&service->scanner)) return true;
    if (scan->truncated) service->operation.truncated = true;
    if (scan->state == ProjectSearchState::Cancelled || service->operation.cancellationRequested) {
        service->operation.state = ReferenceSearchState::Cancelled;
        service->operation.error = ReferenceSearchErrorCode::Cancelled;
    } else if (scan->state == ProjectSearchState::Failed) {
        service->operation.state = ReferenceSearchState::Failed;
        service->operation.error = service->operation.error == ReferenceSearchErrorCode::None ?
            mapProjectError(scan->error) : service->operation.error;
    } else {
        sortGroups(service);
        service->operation.state = ReferenceSearchState::Completed;
        if (service->operation.error == ReferenceSearchErrorCode::None && service->operation.referencesFound == 0)
            service->operation.error = ReferenceSearchErrorCode::NoResults;
    }
    return true;
}

bool ReferenceSearchCancel(ReferenceSearchService* service, uint64_t operationId) {
    if (!service || operationId == 0 || service->operation.operationId != operationId ||
        terminal(service->operation.state)) return false;
    service->operation.cancellationRequested = true;
    service->operation.state = ReferenceSearchState::Cancelling;
    ProjectSearchCancel(&service->scanner, service->scanner.operation.operationId);
    return true;
}

bool ReferenceSearchRelease(ReferenceSearchService* service, uint64_t operationId) {
    if (!service || operationId == 0 || service->operation.operationId != operationId ||
        !terminal(service->operation.state)) return false;
    ProjectSearchRelease(&service->scanner, service->scanner.operation.operationId);
    service->operation.state = ReferenceSearchState::Idle;
    service->operation.error = ReferenceSearchErrorCode::None;
    clearResults(service);
    service->declarationHintCount = 0;
    return true;
}

bool ReferenceSearchIsActive(const ReferenceSearchService* service) {
    return service && !terminal(service->operation.state);
}

const ReferenceSearchOperation* ReferenceSearchOperationInfo(const ReferenceSearchService* service) {
    return service ? &service->operation : nullptr;
}

uint32_t ReferenceSearchResultGroups(const ReferenceSearchService* service) {
    if (!service) return 0;
    uint32_t count = 0;
    for (uint32_t i = 0; i < kReferenceMaxResultFiles; ++i)
        if (service->groups[i].relativePath[0] != '\0') ++count;
    return count;
}

const ReferenceFileGroup* ReferenceSearchResultGroupAt(const ReferenceSearchService* service, uint32_t index) {
    if (!service || index >= ReferenceSearchResultGroups(service)) return nullptr;
    return &service->groups[index];
}

const ReferenceMatch* ReferenceSearchResultMatchAt(const ReferenceSearchService* service,
                                                   const ReferenceFileGroup* group,
                                                   uint32_t matchIndex) {
    if (!service || !group || matchIndex >= group->matchCount ||
        group->firstMatchIndex + matchIndex >= kReferenceMaxTotalMatches) return nullptr;
    return &service->matches[group->firstMatchIndex + matchIndex];
}

} // namespace developer_studio
} // namespace guidexos
