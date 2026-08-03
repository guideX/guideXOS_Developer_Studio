#include "developer_studio_include_graph.h"

namespace guidexos {
namespace developer_studio {
namespace {

static const uint32_t kInvalidIndex = 0xFFFFFFFFu;

static void clearBytes(void* value, uint32_t size) {
    if (!value) return;
    unsigned char* bytes = static_cast<unsigned char*>(value);
    for (uint32_t i = 0; i < size; ++i) bytes[i] = 0;
}

static uint32_t textLength(const char* text, uint32_t limit) {
    if (!text) return 0;
    uint32_t length = 0;
    while (length < limit && text[length] != '\0') ++length;
    return length;
}

static bool copyText(char* output, uint32_t outputSize, const char* input) {
    if (!output || outputSize == 0 || !input) return false;
    uint32_t i = 0;
    while (i + 1 < outputSize && input[i] != '\0') {
        output[i] = input[i];
        ++i;
    }
    if (input[i] != '\0') {
        output[0] = '\0';
        return false;
    }
    output[i] = '\0';
    return true;
}

static bool copyBytes(char* output, uint32_t outputSize, const char* input, uint32_t length) {
    if (!output || outputSize == 0 || (!input && length != 0) || length + 1 > outputSize) return false;
    for (uint32_t i = 0; i < length; ++i) output[i] = input[i];
    output[length] = '\0';
    return true;
}

static char lowerAscii(char value) {
    return value >= 'A' && value <= 'Z' ? static_cast<char>(value + ('a' - 'A')) : value;
}

static bool equalExact(const char* left, const char* right) {
    if (!left || !right) return left == right;
    uint32_t i = 0;
    while (left[i] != '\0' && right[i] != '\0') {
        if (left[i] != right[i]) return false;
        ++i;
    }
    return left[i] == right[i];
}

static bool equalFold(const char* left, const char* right) {
    if (!left || !right) return left == right;
    uint32_t i = 0;
    while (left[i] != '\0' && right[i] != '\0') {
        if (lowerAscii(left[i]) != lowerAscii(right[i])) return false;
        ++i;
    }
    return left[i] == right[i];
}

static bool isSlash(char value) {
    return value == '/' || value == static_cast<char>(92);
}

static bool appendChar(char* output, uint32_t outputSize, uint32_t* length, char value) {
    if (!output || !length || *length + 1 >= outputSize) return false;
    output[(*length)++] = value;
    output[*length] = '\0';
    return true;
}

static bool isIdentifierStart(char value) {
    return (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z') || value == '_';
}

static bool isIdentifierPart(char value) {
    return isIdentifierStart(value) || (value >= '0' && value <= '9');
}

static bool isZeroToken(const char* text, uint32_t length) {
    return length == 1 && text[0] == '0';
}

static bool isOneToken(const char* text, uint32_t length) {
    return length == 1 && text[0] == '1';
}

static bool isExcludedDirectory(const char* name) {
    static const char* const names[] = {
        ".git", ".vs", ".idea", "build", "out", "bin", "obj", "dist", "node_modules"
    };
    for (uint32_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i)
        if (equalFold(name, names[i])) return true;
    return false;
}

static bool hasExtension(const char* path, const char* extension) {
    if (!path || !extension) return false;
    const uint32_t length = textLength(path, kMaxPathBytes);
    const uint32_t extensionLength = textLength(extension, 32);
    if (extensionLength > length) return false;
    return equalFold(path + length - extensionLength, extension);
}

static uint64_t hashText(const char* text) {
    uint64_t hash = 1469598103934665603ull;
    if (!text) return hash;
    for (uint32_t i = 0; text[i] != '\0'; ++i) {
        hash ^= static_cast<unsigned char>(text[i]);
        hash *= 1099511628211ull;
    }
    return hash == 0 ? 1 : hash;
}

static bool relativeDirectory(const char* path, char* output, uint32_t outputSize) {
    if (!path || !output || outputSize == 0) return false;
    const uint32_t length = textLength(path, kMaxPathBytes);
    uint32_t end = length;
    while (end > 0 && path[end - 1] != '/') --end;
    if (end == 0) return copyText(output, outputSize, "");
    return copyBytes(output, outputSize, path, end - 1);
}

static bool normalizeRelative(const char* input, char* output, uint32_t outputSize) {
    if (!input || !output || outputSize < 2) return false;
    output[0] = '\0';
    const uint32_t length = textLength(input, kIncludeGraphMaxRequestedPathBytes + 1);
    if (length == 0 || length > kIncludeGraphMaxRequestedPathBytes) return false;
    if (input[0] == '/' || input[0] == '\\' || (length >= 2 && input[1] == ':')) return false;
    char segments[64][kMaxNameBytes];
    uint32_t count = 0;
    uint32_t i = 0;
    while (i <= length) {
        while (i < length && isSlash(input[i])) ++i;
        const uint32_t start = i;
        while (i < length && !isSlash(input[i])) ++i;
        const uint32_t partLength = i - start;
        if (partLength == 0) break;
        if (partLength >= kMaxNameBytes) return false;
        if (partLength == 1 && input[start] == '.') continue;
        if (partLength == 2 && input[start] == '.' && input[start + 1] == '.') {
            if (count == 0) return false;
            --count;
            continue;
        }
        if (count >= sizeof(segments) / sizeof(segments[0])) return false;
        if (!copyBytes(segments[count], sizeof(segments[count]), input + start, partLength)) return false;
        ++count;
    }
    if (count == 0) return false;
    uint32_t out = 0;
    for (uint32_t part = 0; part < count; ++part) {
        if (part != 0 && !appendChar(output, outputSize, &out, '/')) return false;
        const uint32_t partLength = textLength(segments[part], sizeof(segments[part]));
        for (uint32_t j = 0; j < partLength; ++j)
            if (!appendChar(output, outputSize, &out, segments[part][j])) return false;
    }
    return true;
}

static bool joinRelative(const char* base, const char* path, char* output, uint32_t outputSize) {
    if (!path || !output) return false;
    char combined[kIncludeGraphMaxRequestedPathBytes + kMaxPathBytes + 4] = {};
    uint32_t length = 0;
    if (base && base[0] != '\0') {
        const uint32_t baseLength = textLength(base, kMaxPathBytes);
        if (baseLength + 1 >= sizeof(combined)) return false;
        for (uint32_t i = 0; i < baseLength; ++i) combined[length++] = base[i];
        if (length > 0 && combined[length - 1] != '/') combined[length++] = '/';
    }
    const uint32_t pathLength = textLength(path, kIncludeGraphMaxRequestedPathBytes + 1);
    if (length + pathLength + 1 >= sizeof(combined)) return false;
    for (uint32_t i = 0; i < pathLength; ++i) combined[length++] = path[i];
    combined[length] = '\0';
    return normalizeRelative(combined, output, outputSize);
}

static bool copyIncludeRoot(const IncludeGraphRequest& request, uint32_t index, char* output, uint32_t outputSize) {
    if (index >= request.includeRootCount || index >= kIncludeGraphMaxIncludeRoots) return false;
    if (request.includeRoots[index][0] == '\0') return copyText(output, outputSize, "");
    return normalizeRelative(request.includeRoots[index], output, outputSize);
}

static uint32_t findNodeExact(const IncludeGraph* graph, const char* path) {
    if (!graph || !path) return kInvalidIndex;
    for (uint32_t i = 0; i < graph->nodeCount; ++i)
        if (equalExact(graph->nodes[i].relativePath, path)) return i;
    return kInvalidIndex;
}

static bool pathLess(const char* left, const char* right) {
    uint32_t i = 0;
    while (left[i] != '\0' && right[i] != '\0') {
        const char a = lowerAscii(left[i]);
        const char b = lowerAscii(right[i]);
        if (a != b) return a < b;
        ++i;
    }
    return left[i] < right[i];
}

static uint32_t findNodeFold(const IncludeGraph* graph, const char* path, uint32_t* matches, uint32_t capacity) {
    uint32_t count = 0;
    if (!graph || !path) return 0;
    for (uint32_t i = 0; i < graph->nodeCount; ++i) {
        if (!equalFold(graph->nodes[i].relativePath, path)) continue;
        if (matches && count < capacity) matches[count] = i;
        ++count;
    }
    return count;
}

static bool isTraversalEdge(const IncludeEdge& edge, bool includeConditional) {
    if (edge.resolution.state != IncludeResolutionState::Resolved) return false;
    if (edge.directive.directiveState == IncludeDirectiveState::InactiveIfZero) return false;
    if (!includeConditional && edge.directive.directiveState == IncludeDirectiveState::ConditionalUnknown) return false;
    return true;
}

static uint32_t sourceNodeIndex(const IncludeGraph* graph, const IncludeEdge& edge) {
    return findNodeExact(graph, edge.sourceRelativePath);
}

static uint32_t targetNodeIndex(const IncludeGraph* graph, const IncludeEdge& edge) {
    return findNodeExact(graph, edge.targetRelativePath);
}

static void setStatus(char* output, uint32_t outputSize, const char* value) {
    copyText(output, outputSize, value ? value : "");
}

static void sortFileEntries(FileListEntry* entries, uint32_t count) {
    if (!entries) return;
    for (uint32_t i = 1; i < count; ++i) {
        FileListEntry value = entries[i];
        uint32_t position = i;
        while (position > 0) {
            const FileListEntry& previous = entries[position - 1];
            bool before = false;
            if (previous.kind == FileInfoKind::Directory && value.kind != FileInfoKind::Directory) before = true;
            else if (previous.kind != FileInfoKind::Directory && value.kind == FileInfoKind::Directory) before = false;
            else {
                uint32_t j = 0;
                while (previous.name[j] != '\0' && value.name[j] != '\0' &&
                       lowerAscii(previous.name[j]) == lowerAscii(value.name[j])) ++j;
                if (lowerAscii(previous.name[j]) == lowerAscii(value.name[j])) before = previous.name[j] <= value.name[j];
                else before = lowerAscii(previous.name[j]) < lowerAscii(value.name[j]);
            }
            if (before) break;
            entries[position] = previous;
            --position;
        }
        entries[position] = value;
    }
}

static bool pushDirectory(IncludeGraphBuildOperation* operation, const char* relativePath) {
    if (!operation || !relativePath || operation->pendingCount >= kIncludeGraphMaxPendingDirectories) {
        if (operation) operation->truncated = true;
        return false;
    }
    copyText(operation->pendingDirectories[operation->pendingTail], kMaxPathBytes, relativePath);
    operation->pendingTail = (operation->pendingTail + 1) % kIncludeGraphMaxPendingDirectories;
    ++operation->pendingCount;
    return true;
}

static bool popDirectory(IncludeGraphBuildOperation* operation, char* output, uint32_t outputSize, uint32_t* depth) {
    if (!operation || operation->pendingCount == 0 || !output) return false;
    copyText(output, outputSize, operation->pendingDirectories[operation->pendingHead]);
    operation->pendingHead = (operation->pendingHead + 1) % kIncludeGraphMaxPendingDirectories;
    --operation->pendingCount;
    uint32_t count = 0;
    for (uint32_t i = 0; output[i] != '\0'; ++i) if (output[i] == '/') ++count;
    if (depth) *depth = output[0] == '\0' ? 0 : count + 1;
    return true;
}

static bool appendRelativeChild(const char* directory, const char* name, char* output, uint32_t outputSize) {
    if (!directory || !name || !output) return false;
    uint32_t out = 0;
    const uint32_t dirLength = textLength(directory, kMaxPathBytes);
    const uint32_t nameLength = textLength(name, kMaxNameBytes);
    if (dirLength + nameLength + 2 > outputSize) return false;
    for (uint32_t i = 0; i < dirLength; ++i) output[out++] = directory[i];
    if (dirLength > 0) output[out++] = '/';
    for (uint32_t i = 0; i < nameLength; ++i) output[out++] = name[i];
    output[out] = '\0';
    return true;
}

static bool absoluteForRelative(const IncludeGraphRequest& request, const char* relative, char* output) {
    if (!relative || relative[0] == '\0') return NormalizePath(request.rootPath, output, kMaxPathBytes);
    return JoinWorkspacePath(request.rootPath, relative, output, kMaxPathBytes);
}

static bool dirtySnapshotFor(const IncludeGraphBuildOperation* operation, const char* relative,
                             const IncludeGraphDocumentSnapshot** output) {
    if (output) *output = nullptr;
    if (!operation || !relative) return false;
    for (uint32_t i = 0; i < operation->dirtyDocumentCount; ++i) {
        if (equalFold(operation->dirtyDocuments[i].relativePath, relative)) {
            if (output) *output = &operation->dirtyDocuments[i];
            return true;
        }
    }
    return false;
}

// The scanner keeps only a lexical conditional mode. It never expands macros
// or attempts to evaluate arbitrary expressions.
struct ConditionalFrame {
    uint8_t parentMode;
    uint8_t originalMode;
    bool inElse;
};

static uint8_t conditionForToken(const char* text, uint32_t length) {
    if (isZeroToken(text, length)) return 1; // inactive
    if (isOneToken(text, length)) return 0;  // active
    return 2; // unknown
}

static uint8_t childMode(uint8_t parent, uint8_t condition) {
    if (parent == 1) return 1;
    if (parent == 2) return 2;
    return condition;
}

static uint8_t conditionalStateForMode(uint8_t mode) {
    return mode == 1 ? static_cast<uint8_t>(IncludeDirectiveState::InactiveIfZero) :
        (mode == 2 ? static_cast<uint8_t>(IncludeDirectiveState::ConditionalUnknown) :
                     static_cast<uint8_t>(IncludeDirectiveState::Active));
}

struct LogicalDirective {
    char text[kIncludeGraphMaxLogicalDirectiveBytes + 1];
    uint32_t sourceOffset[kIncludeGraphMaxLogicalDirectiveBytes];
    uint32_t length;
    uint32_t nextOffset;
    bool tooLong;
    bool continuationLimit;
};

static bool readLogicalDirective(const char* data, uint32_t length, uint32_t start,
                                 LogicalDirective* output) {
    if (!data || !output || start >= length) return false;
    *output = LogicalDirective();
    uint32_t offset = start;
    uint32_t physicalLines = 1;
    while (offset < length) {
        bool continued = false;
        uint32_t lineEnd = offset;
        while (lineEnd < length && data[lineEnd] != '\n' && data[lineEnd] != '\r') ++lineEnd;
        uint32_t backslash = lineEnd;
        while (backslash > offset && (data[backslash - 1] == ' ' || data[backslash - 1] == '\t')) --backslash;
        if (backslash > offset && data[backslash - 1] == '\\') {
            continued = true;
            --backslash;
        }
        const uint32_t copyEnd = continued ? backslash : lineEnd;
        for (uint32_t i = offset; i < copyEnd; ++i) {
            if (output->length >= kIncludeGraphMaxLogicalDirectiveBytes) {
                output->tooLong = true;
                output->nextOffset = lineEnd;
                return true;
            }
            output->text[output->length] = data[i];
            output->sourceOffset[output->length] = i;
            ++output->length;
        }
        if (lineEnd >= length) {
            output->nextOffset = length;
            break;
        }
        uint32_t next = lineEnd;
        if (data[next] == '\r') {
            ++next;
            if (next < length && data[next] == '\n') ++next;
        } else {
            ++next;
        }
        output->nextOffset = next;
        if (!continued) break;
        if (++physicalLines > kIncludeGraphMaxContinuedLines) {
            output->continuationLimit = true;
            break;
        }
        offset = next;
    }
    output->text[output->length] = '\0';
    return true;
}

static bool directiveWord(const LogicalDirective& logical, uint32_t* cursor,
                          char* output, uint32_t outputSize) {
    uint32_t position = cursor ? *cursor : 0;
    while (position < logical.length && (logical.text[position] == ' ' || logical.text[position] == '\t')) ++position;
    const uint32_t start = position;
    while (position < logical.length && isIdentifierPart(logical.text[position])) ++position;
    if (position == start || position - start + 1 > outputSize) return false;
    if (!copyBytes(output, outputSize, logical.text + start, position - start)) return false;
    if (cursor) *cursor = position;
    return true;
}

static void skipSpaces(const LogicalDirective& logical, uint32_t* cursor) {
    if (!cursor) return;
    while (*cursor < logical.length && (logical.text[*cursor] == ' ' || logical.text[*cursor] == '\t')) ++*cursor;
}

static bool makeDirective(const LogicalDirective& logical, uint8_t mode, const char* sourcePath,
                          uint64_t documentId, uint64_t documentGeneration, uint64_t directiveOffset,
                          IncludeDirective* output) {
    if (!output || !sourcePath) return false;
    *output = IncludeDirective();
    output->sourceDocumentId = documentId;
    output->sourceDocumentGeneration = documentGeneration;
    if (!copyText(output->sourceRelativePath, sizeof(output->sourceRelativePath), sourcePath)) return false;
    output->directiveByteOffset = directiveOffset;
    output->line = 1;
    output->column = 1;
    output->directiveState = static_cast<IncludeDirectiveState>(conditionalStateForMode(mode));
    uint32_t cursor = 0;
    while (cursor < logical.length && (logical.text[cursor] == ' ' || logical.text[cursor] == '\t')) ++cursor;
    if (cursor >= logical.length || logical.text[cursor] != '#') return false;
    ++cursor;
    char keyword[32] = {};
    if (!directiveWord(logical, &cursor, keyword, sizeof(keyword))) return false;
    if (!equalExact(keyword, "include")) return false;
    skipSpaces(logical, &cursor);
    if (cursor >= logical.length) {
        output->directiveState = IncludeDirectiveState::Malformed;
        return true;
    }
    const char delimiter = logical.text[cursor];
    if (delimiter != '"' && delimiter != '<') {
        output->delimiterKind = IncludeDelimiterKind::Quoted;
        const uint32_t start = cursor;
        while (cursor < logical.length && logical.text[cursor] != ' ' && logical.text[cursor] != '\t') ++cursor;
        if (cursor == start || cursor - start > kIncludeGraphMaxRequestedPathBytes) {
            output->directiveState = IncludeDirectiveState::Malformed;
            return true;
        }
        output->includeTextByteOffset = logical.sourceOffset[start];
        output->includeTextLength = cursor - start;
        if (!copyBytes(output->requestedPath, sizeof(output->requestedPath), logical.text + start, cursor - start))
            output->directiveState = IncludeDirectiveState::Malformed;
        return true;
    }
    output->delimiterKind = delimiter == '<' ? IncludeDelimiterKind::Angled : IncludeDelimiterKind::Quoted;
    ++cursor;
    const uint32_t pathStart = cursor;
    while (cursor < logical.length && logical.text[cursor] != (delimiter == '<' ? '>' : '"')) ++cursor;
    if (cursor >= logical.length) {
        output->directiveState = IncludeDirectiveState::Malformed;
        return true;
    }
    if (cursor - pathStart == 0 || cursor - pathStart > kIncludeGraphMaxRequestedPathBytes) {
        output->directiveState = IncludeDirectiveState::Malformed;
        return true;
    }
    output->includeTextByteOffset = logical.sourceOffset[pathStart];
    output->includeTextLength = cursor - pathStart;
    output->literalPath = true;
    if (!copyBytes(output->requestedPath, sizeof(output->requestedPath), logical.text + pathStart, cursor - pathStart))
        output->directiveState = IncludeDirectiveState::Malformed;
    ++cursor;
    return true;
}

static bool conditionalDirective(const LogicalDirective& logical, char* keyword, uint32_t keywordSize,
                                 char* argument, uint32_t argumentSize) {
    uint32_t cursor = 0;
    while (cursor < logical.length && (logical.text[cursor] == ' ' || logical.text[cursor] == '\t')) ++cursor;
    if (cursor >= logical.length || logical.text[cursor] != '#') return false;
    ++cursor;
    if (!directiveWord(logical, &cursor, keyword, keywordSize)) return false;
    skipSpaces(logical, &cursor);
    const uint32_t start = cursor;
    while (cursor < logical.length && logical.text[cursor] != ' ' && logical.text[cursor] != '\t') ++cursor;
    if (argument && argumentSize > 0) {
        const uint32_t count = cursor - start < argumentSize - 1 ? cursor - start : argumentSize - 1;
        for (uint32_t i = 0; i < count; ++i) argument[i] = logical.text[start + i];
        argument[count] = '\0';
    }
    return true;
}

struct ScannerState {
    bool inBlockComment;
    bool inString;
    bool inCharacter;
    bool inRawString;
    char rawDelimiter[5];
    uint32_t rawDelimiterLength;
    bool atLineStart;
    uint8_t conditionalMode;
    ConditionalFrame conditionals[64];
    uint32_t conditionalDepth;
};

static void handleConditional(const LogicalDirective& logical, ScannerState* state) {
    if (!state) return;
    char keyword[32] = {};
    char argument[128] = {};
    if (!conditionalDirective(logical, keyword, sizeof(keyword), argument, sizeof(argument))) return;
    if (equalExact(keyword, "if") || equalExact(keyword, "ifdef") || equalExact(keyword, "ifndef")) {
        if (state->conditionalDepth >= sizeof(state->conditionals) / sizeof(state->conditionals[0])) {
            state->conditionalMode = 2;
            return;
        }
        ConditionalFrame& frame = state->conditionals[state->conditionalDepth++];
        frame.parentMode = state->conditionalMode;
        frame.originalMode = equalExact(keyword, "if") ? conditionForToken(argument, textLength(argument, sizeof(argument))) : 2;
        frame.inElse = false;
        state->conditionalMode = childMode(frame.parentMode, frame.originalMode);
    } else if (equalExact(keyword, "else")) {
        if (state->conditionalDepth == 0) return;
        ConditionalFrame& frame = state->conditionals[state->conditionalDepth - 1];
        frame.inElse = true;
        if (frame.parentMode == 1 || frame.parentMode == 2) state->conditionalMode = frame.parentMode;
        else if (frame.originalMode == 1) state->conditionalMode = 0;
        else if (frame.originalMode == 0) state->conditionalMode = 1;
        else state->conditionalMode = 2;
    } else if (equalExact(keyword, "elif")) {
        if (state->conditionalDepth == 0) return;
        ConditionalFrame& frame = state->conditionals[state->conditionalDepth - 1];
        if (frame.parentMode == 1 || frame.parentMode == 2) state->conditionalMode = frame.parentMode;
        else state->conditionalMode = frame.inElse ? 1 : childMode(frame.parentMode, conditionForToken(argument, textLength(argument, sizeof(argument))));
    } else if (equalExact(keyword, "endif")) {
        if (state->conditionalDepth == 0) return;
        state->conditionalMode = state->conditionals[--state->conditionalDepth].parentMode;
    }
}

static bool scanDirectiveAt(const char* data, uint32_t length, uint32_t offset,
                            const char* sourcePath, uint64_t documentId, uint64_t generation,
                            ScannerState* state, IncludeDirectiveVisitor visitor, void* userData,
                            uint32_t* count, bool* truncated) {
    LogicalDirective logical = {};
    if (!readLogicalDirective(data, length, offset, &logical)) return true;
    char keyword[32] = {};
    char argument[128] = {};
    const bool hasKeyword = conditionalDirective(logical, keyword, sizeof(keyword), argument, sizeof(argument));
    if (hasKeyword && (equalExact(keyword, "if") || equalExact(keyword, "ifdef") || equalExact(keyword, "ifndef") ||
                       equalExact(keyword, "else") || equalExact(keyword, "elif") || equalExact(keyword, "endif")))
        handleConditional(logical, state);
    if (hasKeyword && equalExact(keyword, "include")) {
        IncludeDirective directive = {};
        if (logical.tooLong || logical.continuationLimit ||
            !makeDirective(logical, state->conditionalMode, sourcePath, documentId, generation, offset, &directive)) {
            directive = IncludeDirective();
            copyText(directive.sourceRelativePath, sizeof(directive.sourceRelativePath), sourcePath);
            directive.directiveByteOffset = offset;
            directive.directiveState = IncludeDirectiveState::Malformed;
        }
        uint32_t line = 1;
        uint32_t column = 1;
        for (uint32_t i = 0; i < offset && i < length; ++i) {
            if (data[i] == '\n') { ++line; column = 1; }
            else ++column;
        }
        directive.line = line;
        directive.column = column;
        if (count) ++*count;
        if (count && *count > kIncludeGraphMaxDirectivesPerFile) {
            if (truncated) *truncated = true;
            return false;
        }
        if (visitor && !visitor(userData, &directive)) {
            if (truncated) *truncated = true;
            return false;
        }
    }
    state->atLineStart = true;
    return logical.nextOffset > offset;
}

static bool appendCandidate(IncludeGraph* graph, IncludeResolution* resolution, const char* path) {
    if (!graph || !resolution || !path) return false;
    if (resolution->ambiguousCandidateCount >= kIncludeGraphMaxAmbiguousCandidates) return true;
    if (graph->ambiguousCandidateCount >= graph->ambiguousCandidateCapacity) {
        graph->truncated = true;
        return false;
    }
    if (resolution->ambiguousCandidateCount == 0)
        resolution->ambiguousCandidateOffset = graph->ambiguousCandidateCount;
    copyText(graph->ambiguousCandidates[graph->ambiguousCandidateCount], kMaxPathBytes, path);
    ++graph->ambiguousCandidateCount;
    ++resolution->ambiguousCandidateCount;
    return true;
}

static bool resolveEdge(IncludeGraph* graph, IncludeEdge* edge, const IncludeGraphRequest& request) {
    if (!graph || !edge) return false;
    edge->resolution = IncludeResolution();
    const IncludeDirective& directive = edge->directive;
    if (directive.directiveState == IncludeDirectiveState::Malformed) {
        edge->resolution.state = IncludeResolutionState::InvalidPath;
        setStatus(edge->resolution.statusCode, sizeof(edge->resolution.statusCode), "INCLUDE_GRAPH_INVALID_DIRECTIVE");
        return true;
    }
    if (directive.requestedPath[0] == '\0') {
        edge->resolution.state = IncludeResolutionState::InvalidPath;
        setStatus(edge->resolution.statusCode, sizeof(edge->resolution.statusCode), "INCLUDE_GRAPH_PATH_INVALID");
        return true;
    }
    if (directive.includeTextLength > kIncludeGraphMaxRequestedPathBytes ||
        textLength(directive.requestedPath, sizeof(directive.requestedPath)) > kIncludeGraphMaxRequestedPathBytes) {
        edge->resolution.state = IncludeResolutionState::InvalidPath;
        setStatus(edge->resolution.statusCode, sizeof(edge->resolution.statusCode), "INCLUDE_GRAPH_PATH_TOO_LONG");
        return true;
    }
    if (!directive.literalPath) {
        edge->resolution.state = IncludeResolutionState::UnsupportedMacro;
        setStatus(edge->resolution.statusCode, sizeof(edge->resolution.statusCode), "INCLUDE_GRAPH_MACRO_UNSUPPORTED");
        return true;
    }
    // Normalize only after applying the candidate search base.  A quoted
    // include such as "../include/foo.h" is valid when the source lives in
    // src/, while the same path from the project root must be rejected.
    if (directive.requestedPath[0] == '/' || directive.requestedPath[0] == '\\' ||
        (directive.requestedPath[1] != '\0' && directive.requestedPath[1] == ':')) {
        edge->resolution.state = IncludeResolutionState::OutsideProject;
        setStatus(edge->resolution.statusCode, sizeof(edge->resolution.statusCode), "INCLUDE_GRAPH_PATH_OUTSIDE_PROJECT");
        return true;
    }
    char sourceDirectory[kMaxPathBytes] = {};
    relativeDirectory(directive.sourceRelativePath, sourceDirectory, sizeof(sourceDirectory));
    const uint32_t rootCount = request.includeRootCount > kIncludeGraphMaxIncludeRoots ?
        kIncludeGraphMaxIncludeRoots : request.includeRootCount;
    const uint32_t sourcePriorityCount = directive.delimiterKind == IncludeDelimiterKind::Quoted ? 1u : 0u;
    const uint32_t projectRootCount = request.allowProjectRoot ? 1u : 0u;
    bool hadCandidatePath = false;
    for (uint32_t priority = 0; priority < sourcePriorityCount + rootCount + projectRootCount; ++priority) {
        char base[kMaxPathBytes] = {};
        const bool sourcePriority = directive.delimiterKind == IncludeDelimiterKind::Quoted && priority == 0;
        if (sourcePriority) copyText(base, sizeof(base), sourceDirectory);
        else if ((priority >= sourcePriorityCount) && priority - sourcePriorityCount < rootCount) {
            const uint32_t rootIndex = priority - sourcePriorityCount;
            if (!copyIncludeRoot(request, rootIndex, base, sizeof(base))) continue;
        } else {
            if (!request.allowProjectRoot) continue;
            copyText(base, sizeof(base), "");
        }
        char candidate[kMaxPathBytes] = {};
        if (!joinRelative(base, directive.requestedPath, candidate, sizeof(candidate))) continue;
        hadCandidatePath = true;
        const uint32_t exactCount = findNodeExact(graph, candidate) == kInvalidIndex ? 0 : 1;
        if (exactCount > 0) {
            copyText(edge->targetRelativePath, sizeof(edge->targetRelativePath), candidate);
            copyText(edge->resolution.resolvedRelativePath, sizeof(edge->resolution.resolvedRelativePath), candidate);
            copyText(edge->resolution.resolutionRoot, sizeof(edge->resolution.resolutionRoot), base);
            edge->resolution.resolutionPriority = priority;
            edge->resolution.state = IncludeResolutionState::Resolved;
            setStatus(edge->resolution.statusCode, sizeof(edge->resolution.statusCode), "INCLUDE_GRAPH_RESOLVED");
            return true;
        }
        uint32_t matches[kIncludeGraphMaxAmbiguousCandidates];
        const uint32_t matchCount = findNodeFold(graph, candidate, matches, sizeof(matches) / sizeof(matches[0]));
        if (matchCount == 0) continue;
        edge->resolution.resolutionPriority = priority;
        copyText(edge->resolution.resolutionRoot, sizeof(edge->resolution.resolutionRoot), base);
        if (matchCount == 1) {
            copyText(edge->targetRelativePath, sizeof(edge->targetRelativePath), graph->nodes[matches[0]].relativePath);
            copyText(edge->resolution.resolvedRelativePath, sizeof(edge->resolution.resolvedRelativePath), graph->nodes[matches[0]].relativePath);
            edge->resolution.caseMismatched = !equalExact(candidate, graph->nodes[matches[0]].relativePath);
            edge->resolution.state = IncludeResolutionState::Resolved;
            setStatus(edge->resolution.statusCode, sizeof(edge->resolution.statusCode),
                      edge->resolution.caseMismatched ? "INCLUDE_GRAPH_CASE_MISMATCH" : "INCLUDE_GRAPH_RESOLVED");
            return true;
        }
        edge->resolution.state = IncludeResolutionState::Ambiguous;
        const uint32_t retainedMatches = matchCount < kIncludeGraphMaxAmbiguousCandidates ?
            matchCount : kIncludeGraphMaxAmbiguousCandidates;
        for (uint32_t i = 1; i < retainedMatches; ++i) {
            const uint32_t value = matches[i];
            uint32_t position = i;
            while (position > 0 && pathLess(graph->nodes[value].relativePath,
                                            graph->nodes[matches[position - 1]].relativePath)) {
                matches[position] = matches[position - 1];
                --position;
            }
            matches[position] = value;
        }
        for (uint32_t i = 0; i < retainedMatches; ++i)
            if (!appendCandidate(graph, &edge->resolution, graph->nodes[matches[i]].relativePath)) return false;
        setStatus(edge->resolution.statusCode, sizeof(edge->resolution.statusCode), "INCLUDE_GRAPH_INCLUDE_AMBIGUOUS");
        return true;
    }
    edge->resolution.state = !hadCandidatePath ? IncludeResolutionState::OutsideProject :
        (directive.delimiterKind == IncludeDelimiterKind::Angled ?
            IncludeResolutionState::ExternalUnresolved : IncludeResolutionState::Missing);
    setStatus(edge->resolution.statusCode, sizeof(edge->resolution.statusCode),
              !hadCandidatePath ? "INCLUDE_GRAPH_PATH_OUTSIDE_PROJECT" :
              (directive.delimiterKind == IncludeDelimiterKind::Angled ? "INCLUDE_GRAPH_EXTERNAL_UNRESOLVED" : "INCLUDE_GRAPH_INCLUDE_MISSING"));
    return true;
}

static bool addScannedDirective(void* userData, const IncludeDirective* directive) {
    struct ScanContext {
        IncludeGraph* graph;
        uint64_t* nextEdgeId;
        uint64_t* nextDirectiveId;
        bool fromDirtySnapshot;
    };
    ScanContext* context = static_cast<ScanContext*>(userData);
    if (!context || !context->graph || !directive || context->graph->edgeCount >= context->graph->edgeCapacity) {
        if (context && context->graph) context->graph->truncated = true;
        return false;
    }
    IncludeEdge edge = {};
    edge.edgeId = context->nextEdgeId ? (*context->nextEdgeId)++ : 0;
    edge.directive = *directive;
    edge.directive.directiveId = context->nextDirectiveId ? (*context->nextDirectiveId)++ : edge.edgeId;
    edge.fromDirtySnapshot = context->fromDirtySnapshot;
    edge.stale = false;
    copyText(edge.sourceRelativePath, sizeof(edge.sourceRelativePath), directive->sourceRelativePath);
    return IncludeGraphAddEdge(context->graph, edge);
}

static bool resolveAndAnalyze(IncludeGraph* graph, const IncludeGraphRequest& request,
                              uint64_t* resolved, uint64_t* unresolved) {
    if (!IncludeGraphResolveAll(graph, request, resolved, unresolved)) return false;
    if (!IncludeGraphBuildReverseEdges(graph)) return false;
    return IncludeGraphDetectCycles(graph, nullptr);
}

static bool sourceFileIsEligible(const char* path) {
    return IsIncludeGraphSourcePath(path) || IsIncludeGraphHeaderPath(path);
}

static void copyOperationRequest(IncludeGraphBuildOperation* operation, const IncludeGraphRequest& request) {
    copyText(operation->rootPath, sizeof(operation->rootPath), request.rootPath);
    operation->includeRootCount = request.includeRootCount > kIncludeGraphMaxIncludeRoots ?
        kIncludeGraphMaxIncludeRoots : request.includeRootCount;
    for (uint32_t i = 0; i < operation->includeRootCount; ++i)
        copyText(operation->includeRoots[i], sizeof(operation->includeRoots[i]), request.includeRoots[i]);
    operation->allowProjectRoot = request.allowProjectRoot;
    operation->fileSystem = request.fileSystem;
    operation->dirtyDocumentCount = request.dirtyDocumentCount > kMaxOpenDocuments ? kMaxOpenDocuments : request.dirtyDocumentCount;
    for (uint32_t i = 0; i < operation->dirtyDocumentCount; ++i) operation->dirtyDocuments[i] = request.dirtyDocuments[i];
}

static IncludeGraphRequest operationRequest(const IncludeGraphBuildOperation* operation) {
    IncludeGraphRequest request = {};
    copyText(request.projectId, sizeof(request.projectId), "");
    request.projectGeneration = operation->projectGeneration;
    copyText(request.rootPath, sizeof(request.rootPath), operation->rootPath);
    request.includeRootCount = operation->includeRootCount;
    for (uint32_t i = 0; i < request.includeRootCount; ++i) copyText(request.includeRoots[i], sizeof(request.includeRoots[i]), operation->includeRoots[i]);
    request.allowProjectRoot = operation->allowProjectRoot;
    request.fileSystem = operation->fileSystem;
    request.dirtyDocuments = operation->dirtyDocuments;
    request.dirtyDocumentCount = operation->dirtyDocumentCount;
    return request;
}

static bool addDirtyOnlyNodes(IncludeGraphBuildOperation* operation) {
    if (!operation || !operation->graph) return false;
    for (uint32_t i = 0; i < operation->dirtyDocumentCount; ++i) {
        if (!sourceFileIsEligible(operation->dirtyDocuments[i].relativePath)) continue;
        if (findNodeExact(operation->graph, operation->dirtyDocuments[i].relativePath) == kInvalidIndex) {
            if (!IncludeGraphAddNode(operation->graph, operation->dirtyDocuments[i].relativePath, true, nullptr)) {
                operation->truncated = true;
                return false;
            }
        }
    }
    return true;
}

static bool pollEnumeration(IncludeGraphBuildOperation* operation) {
    if (!operation || !operation->graph) return false;
    if (!operation->currentDirectoryActive) {
        if (!popDirectory(operation, operation->currentDirectory, sizeof(operation->currentDirectory), &operation->currentDirectoryDepth))
            return false;
        if (++operation->directoriesVisited > kIncludeGraphMaxDirectories) {
            operation->truncated = true;
            return false;
        }
        char absolute[kMaxPathBytes] = {};
        IncludeGraphRequest request = operationRequest(operation);
        if (!absoluteForRelative(request, operation->currentDirectory, absolute)) {
            operation->error = IncludeGraphErrorCode::PathOutsideProject;
            operation->state = IncludeGraphBuildState::Failed;
            return false;
        }
        operation->currentEntryCount = 0;
        operation->currentEntryIndex = 0;
        bool truncated = false;
        if (!operation->fileSystem.list || !operation->fileSystem.list(operation->fileSystem.userData, absolute,
                                                                      operation->currentEntries, kMaxWorkspaceEntries,
                                                                      &operation->currentEntryCount, &truncated)) {
            operation->error = IncludeGraphErrorCode::EnumerationFailed;
            operation->state = IncludeGraphBuildState::Failed;
            return false;
        }
        if (truncated) operation->truncated = true;
        sortFileEntries(operation->currentEntries, operation->currentEntryCount);
        operation->currentDirectoryActive = true;
    }
    if (operation->currentEntryIndex >= operation->currentEntryCount) {
        operation->currentDirectoryActive = false;
        if (operation->pendingCount == 0) {
            addDirtyOnlyNodes(operation);
            operation->state = IncludeGraphBuildState::Scanning;
        }
        return true;
    }
    const FileListEntry& entry = operation->currentEntries[operation->currentEntryIndex++];
    if (entry.name[0] == '\0' || equalExact(entry.name, ".") || equalExact(entry.name, "..")) return true;
    char relative[kMaxPathBytes] = {};
    if (!appendRelativeChild(operation->currentDirectory, entry.name, relative, sizeof(relative))) {
        operation->truncated = true;
        return true;
    }
    if (entry.kind == FileInfoKind::Directory) {
        if (isExcludedDirectory(entry.name)) return true;
        if (operation->currentDirectoryDepth >= kIncludeGraphMaxDepth) {
            operation->truncated = true;
            return true;
        }
        pushDirectory(operation, relative);
        return true;
    }
    if (entry.kind != FileInfoKind::RegularFile || !sourceFileIsEligible(relative)) return true;
    if (operation->filesEnumerated >= kIncludeGraphMaxFiles) {
        operation->truncated = true;
        operation->error = IncludeGraphErrorCode::FileLimit;
        return true;
    }
    ++operation->filesEnumerated;
    if (operation->graph->nodeCount >= operation->graph->nodeCapacity) {
        operation->truncated = true;
        operation->error = IncludeGraphErrorCode::NodeLimit;
        return true;
    }
    IncludeGraphAddNode(operation->graph, relative, false, nullptr);
    return true;
}

static bool pollScanning(IncludeGraphBuildOperation* operation) {
    if (!operation || !operation->graph) return false;
    if (operation->scanIndex >= operation->graph->nodeCount) {
        operation->state = IncludeGraphBuildState::Resolving;
        operation->resolveIndex = 0;
        return true;
    }
    const uint32_t nodeIndex = operation->scanIndex++;
    const IncludeNode& node = operation->graph->nodes[nodeIndex];
    const IncludeGraphDocumentSnapshot* dirty = nullptr;
    IncludeGraphRequest request = operationRequest(operation);
    const char* bytes = nullptr;
    uint32_t length = 0;
    bool fromDirty = false;
    if (dirtySnapshotFor(operation, node.relativePath, &dirty)) {
        bytes = dirty->data;
        length = dirty->length;
        fromDirty = true;
    } else {
        char absolute[kMaxPathBytes] = {};
        FileInfo info = {};
        if (!absoluteForRelative(request, node.relativePath, absolute) || !operation->fileSystem.stat ||
            !operation->fileSystem.stat(operation->fileSystem.userData, absolute, &info)) {
            operation->truncated = true;
            return true;
        }
        if (info.size > kIncludeGraphMaxScanFileBytes) {
            operation->truncated = true;
            operation->error = IncludeGraphErrorCode::FileTooLarge;
            return true;
        }
        uint32_t readBytes = 0;
        if (!operation->fileSystem.read || !operation->fileSystem.read(operation->fileSystem.userData, absolute,
                                                                        operation->scanBuffer, kIncludeGraphMaxScanFileBytes,
                                                                        &readBytes)) {
            operation->truncated = true;
            operation->error = IncludeGraphErrorCode::FileReadFailed;
            return true;
        }
        if (readBytes > kIncludeGraphMaxScanFileBytes || LooksBinary(operation->scanBuffer, readBytes)) {
            operation->truncated = true;
            operation->error = LooksBinary(operation->scanBuffer, readBytes) ? IncludeGraphErrorCode::BinaryFile : IncludeGraphErrorCode::FileTooLarge;
            return true;
        }
        bytes = operation->scanBuffer;
        length = readBytes;
    }
    if (operation->bytesScanned + length > kIncludeGraphMaxBytesScanned) {
        operation->truncated = true;
        operation->error = IncludeGraphErrorCode::ByteLimit;
        return true;
    }
    operation->bytesScanned += length;
    ++operation->filesScanned;
    struct ScanContext {
        IncludeGraph* graph;
        uint64_t* nextEdgeId;
        uint64_t* nextDirectiveId;
        bool fromDirtySnapshot;
    } context = { operation->graph, &operation->nextEdgeId, &operation->nextDirectiveId, fromDirty };
    uint32_t directives = 0;
    bool scanTruncated = false;
    IncludeGraphScanDocument(bytes, length, node.relativePath, dirty ? dirty->documentId : 0,
                             dirty ? dirty->documentGeneration : 0, addScannedDirective, &context,
                             &directives, &scanTruncated);
    operation->directivesFound += directives;
    if (scanTruncated) operation->truncated = true;
    return true;
}

static bool pollResolving(IncludeGraphBuildOperation* operation) {
    if (!operation || !operation->graph) return false;
    if (operation->resolveIndex >= operation->graph->edgeCount) {
        operation->state = IncludeGraphBuildState::BuildingReverseEdges;
        return true;
    }
    IncludeGraphRequest request = operationRequest(operation);
    IncludeEdge& edge = operation->graph->edges[operation->resolveIndex++];
    if (!resolveEdge(operation->graph, &edge, request)) operation->truncated = true;
    if (edge.resolution.state == IncludeResolutionState::Resolved) ++operation->edgesResolved;
    else ++operation->unresolvedEdges;
    return true;
}

} // namespace

const char* IncludeDelimiterName(IncludeDelimiterKind kind) {
    return kind == IncludeDelimiterKind::Angled ? "Angled" : "Quoted";
}

const char* IncludeDirectiveStateName(IncludeDirectiveState state) {
    switch (state) {
    case IncludeDirectiveState::Active: return "Active";
    case IncludeDirectiveState::InactiveIfZero: return "InactiveIfZero";
    case IncludeDirectiveState::ConditionalUnknown: return "ConditionalUnknown";
    case IncludeDirectiveState::Malformed: return "Malformed";
    default: return "Unknown";
    }
}

const char* IncludeResolutionStateName(IncludeResolutionState state) {
    switch (state) {
    case IncludeResolutionState::Resolved: return "Resolved";
    case IncludeResolutionState::Missing: return "Missing project file";
    case IncludeResolutionState::ExternalUnresolved: return "External or unresolved angled include";
    case IncludeResolutionState::Ambiguous: return "Ambiguous";
    case IncludeResolutionState::OutsideProject: return "Outside project";
    case IncludeResolutionState::UnsupportedMacro: return "Macro include unsupported";
    case IncludeResolutionState::InvalidPath: return "Invalid include path";
    case IncludeResolutionState::Unreadable: return "Unreadable target";
    default: return "Unknown";
    }
}

const char* IncludeGraphBuildStateName(IncludeGraphBuildState state) {
    switch (state) {
    case IncludeGraphBuildState::Idle: return "Idle";
    case IncludeGraphBuildState::Enumerating: return "Enumerating";
    case IncludeGraphBuildState::Scanning: return "Scanning";
    case IncludeGraphBuildState::Resolving: return "Resolving";
    case IncludeGraphBuildState::BuildingReverseEdges: return "BuildingReverseEdges";
    case IncludeGraphBuildState::DetectingCycles: return "DetectingCycles";
    case IncludeGraphBuildState::Completed: return "Completed";
    case IncludeGraphBuildState::Cancelling: return "Cancelling";
    case IncludeGraphBuildState::Cancelled: return "Cancelled";
    case IncludeGraphBuildState::Failed: return "Failed";
    default: return "Unknown";
    }
}

const char* IncludeGraphErrorName(IncludeGraphErrorCode error) {
    switch (error) {
    case IncludeGraphErrorCode::None: return "none";
    case IncludeGraphErrorCode::NoProject: return "INCLUDE_GRAPH_NO_PROJECT";
    case IncludeGraphErrorCode::ProjectStale: return "INCLUDE_GRAPH_PROJECT_STALE";
    case IncludeGraphErrorCode::BuildStale: return "INCLUDE_GRAPH_BUILD_STALE";
    case IncludeGraphErrorCode::Cancelled: return "INCLUDE_GRAPH_CANCELLED";
    case IncludeGraphErrorCode::Timeout: return "INCLUDE_GRAPH_TIMEOUT";
    case IncludeGraphErrorCode::FileLimit: return "INCLUDE_GRAPH_FILE_LIMIT";
    case IncludeGraphErrorCode::DirectoryLimit: return "INCLUDE_GRAPH_DIRECTORY_LIMIT";
    case IncludeGraphErrorCode::ByteLimit: return "INCLUDE_GRAPH_BYTE_LIMIT";
    case IncludeGraphErrorCode::DirectiveLimit: return "INCLUDE_GRAPH_DIRECTIVE_LIMIT";
    case IncludeGraphErrorCode::EdgeLimit: return "INCLUDE_GRAPH_EDGE_LIMIT";
    case IncludeGraphErrorCode::NodeLimit: return "INCLUDE_GRAPH_NODE_LIMIT";
    case IncludeGraphErrorCode::CycleLimit: return "INCLUDE_GRAPH_CYCLE_LIMIT";
    case IncludeGraphErrorCode::EnumerationFailed: return "INCLUDE_GRAPH_ENUMERATION_FAILED";
    case IncludeGraphErrorCode::FileReadFailed: return "INCLUDE_GRAPH_FILE_READ_FAILED";
    case IncludeGraphErrorCode::FileTooLarge: return "INCLUDE_GRAPH_FILE_TOO_LARGE";
    case IncludeGraphErrorCode::BinaryFile: return "INCLUDE_GRAPH_BINARY_FILE";
    case IncludeGraphErrorCode::InvalidDirective: return "INCLUDE_GRAPH_INVALID_DIRECTIVE";
    case IncludeGraphErrorCode::DirectiveTooLong: return "INCLUDE_GRAPH_DIRECTIVE_TOO_LONG";
    case IncludeGraphErrorCode::ContinuationLimit: return "INCLUDE_GRAPH_CONTINUATION_LIMIT";
    case IncludeGraphErrorCode::PathTooLong: return "INCLUDE_GRAPH_PATH_TOO_LONG";
    case IncludeGraphErrorCode::PathInvalid: return "INCLUDE_GRAPH_PATH_INVALID";
    case IncludeGraphErrorCode::PathOutsideProject: return "INCLUDE_GRAPH_PATH_OUTSIDE_PROJECT";
    case IncludeGraphErrorCode::TraversalLimit: return "INCLUDE_GRAPH_TRAVERSAL_LIMIT";
    case IncludeGraphErrorCode::Released: return "released";
    case IncludeGraphErrorCode::Internal: return "INCLUDE_GRAPH_INTERNAL";
    default: return "unknown";
    }
}

const char* IncludeGraphStatusText(IncludeGraphErrorCode error) {
    switch (error) {
    case IncludeGraphErrorCode::NoProject: return "Open a project before using Include Graph.";
    case IncludeGraphErrorCode::Cancelled: return "Include Graph build cancelled.";
    case IncludeGraphErrorCode::Timeout: return "Include Graph build timed out.";
    case IncludeGraphErrorCode::PathOutsideProject: return "Include path escaped the project root.";
    case IncludeGraphErrorCode::TraversalLimit: return "Dependency traversal truncated.";
    default: return IncludeGraphErrorName(error);
    }
}

bool IsIncludeGraphSourcePath(const char* path) {
    return hasExtension(path, ".c") || hasExtension(path, ".cc") || hasExtension(path, ".cpp") ||
           hasExtension(path, ".cxx");
}

bool IsIncludeGraphHeaderPath(const char* path) {
    return hasExtension(path, ".h") || hasExtension(path, ".hh") || hasExtension(path, ".hpp") ||
           hasExtension(path, ".hxx");
}

void IncludeGraphStorageInit(IncludeGraphStorage* storage) {
    if (!storage) return;
    clearBytes(storage, sizeof(*storage));
}

void IncludeGraphBuildOperationInit(IncludeGraphBuildOperation* operation) {
    if (!operation) return;
    clearBytes(operation, sizeof(*operation));
    operation->state = IncludeGraphBuildState::Idle;
    operation->error = IncludeGraphErrorCode::None;
}

void IncludeGraphInit(IncludeGraph* graph, IncludeGraphStorage* storage,
                      const char* projectId, uint64_t projectGeneration) {
    if (!graph) return;
    *graph = IncludeGraph();
    if (storage) {
        graph->nodes = storage->nodes;
        graph->edges = storage->edges;
        graph->outgoingEdgeIndices = storage->outgoingEdgeIndices;
        graph->incomingEdgeIndices = storage->incomingEdgeIndices;
        graph->cycles = storage->cycles;
        graph->cycleMembers = storage->cycleMembers;
        graph->cycleEdges = storage->cycleEdges;
        graph->ambiguousCandidates = storage->ambiguousCandidates;
        graph->nodeCapacity = kIncludeGraphMaxNodes;
        graph->edgeCapacity = kIncludeGraphMaxEdges;
        graph->cycleCapacity = kIncludeGraphMaxCycleGroups;
        graph->ambiguousCandidateCapacity = kIncludeGraphMaxStoredCandidates;
        graph->cycleMemberCapacity = kIncludeGraphMaxCycleMembers;
        graph->cycleEdgeCapacity = kIncludeGraphMaxCycleMembers;
    }
    graph->projectId = hashText(projectId);
    graph->projectGeneration = projectGeneration;
    graph->graphGeneration = 0;
}

bool IncludeGraphIsCurrent(const IncludeGraph* graph, const char* projectId, uint64_t projectGeneration) {
    return graph && graph->complete && graph->projectId == hashText(projectId) && graph->projectGeneration == projectGeneration;
}

const IncludeNode* IncludeGraphNodeAt(const IncludeGraph* graph, uint32_t index) {
    return graph && index < graph->nodeCount ? &graph->nodes[index] : nullptr;
}

const IncludeEdge* IncludeGraphEdgeAt(const IncludeGraph* graph, uint32_t index) {
    return graph && index < graph->edgeCount ? &graph->edges[index] : nullptr;
}

const IncludeCycleGroup* IncludeGraphCycleAt(const IncludeGraph* graph, uint32_t index) {
    return graph && index < graph->cycleCount ? &graph->cycles[index] : nullptr;
}

const char* IncludeGraphCandidateAt(const IncludeGraph* graph, const IncludeResolution& resolution, uint32_t index) {
    if (!graph || index >= resolution.ambiguousCandidateCount ||
        resolution.ambiguousCandidateOffset + index >= graph->ambiguousCandidateCount) return nullptr;
    return graph->ambiguousCandidates[resolution.ambiguousCandidateOffset + index];
}

bool IncludeGraphScanDocument(const char* data, uint32_t length,
                              const char* sourceRelativePath, uint64_t documentId,
                              uint64_t documentGeneration, IncludeDirectiveVisitor visitor,
                              void* userData, uint32_t* outDirectives, bool* outTruncated) {
    if (outDirectives) *outDirectives = 0;
    if (outTruncated) *outTruncated = false;
    if (!data || !sourceRelativePath || !visitor || length > kIncludeGraphMaxScanFileBytes) return false;
    ScannerState state = {};
    state.atLineStart = true;
    uint32_t offset = 0;
    while (offset < length) {
        const char value = data[offset];
        if (state.inRawString) {
            if (value == ')' && state.rawDelimiterLength <= 4) {
                uint32_t cursor = offset + 1;
                uint32_t i = 0;
                while (i < state.rawDelimiterLength && cursor < length && data[cursor] == state.rawDelimiter[i]) { ++i; ++cursor; }
                if (i == state.rawDelimiterLength && cursor < length && data[cursor] == '"') {
                    state.inRawString = false;
                    offset = cursor + 1;
                    continue;
                }
            }
            if (value == '\n') state.atLineStart = true;
            ++offset;
            continue;
        }
        if (state.inBlockComment) {
            if (value == '*' && offset + 1 < length && data[offset + 1] == '/') {
                state.inBlockComment = false;
                offset += 2;
                continue;
            }
            if (value == '\n') state.atLineStart = true;
            ++offset;
            continue;
        }
        if (state.inString || state.inCharacter) {
            const bool character = state.inCharacter;
            if (value == '\\' && offset + 1 < length) { offset += 2; continue; }
            if ((character && value == '\'') || (!character && value == '"')) {
                state.inString = false;
                state.inCharacter = false;
            } else if (value == '\n') {
                state.inString = false;
                state.inCharacter = false;
                state.atLineStart = true;
            }
            ++offset;
            continue;
        }
        if (state.atLineStart) {
            if (offset == 0 && static_cast<unsigned char>(data[offset]) == 0xEF && offset + 2 < length &&
                static_cast<unsigned char>(data[offset + 1]) == 0xBB && static_cast<unsigned char>(data[offset + 2]) == 0xBF) {
                offset += 3;
                continue;
            }
            if (value == ' ' || value == '\t' || value == '\r') { ++offset; continue; }
            if (value == '#') {
                const uint32_t oldOffset = offset;
                LogicalDirective logical = {};
                readLogicalDirective(data, length, offset, &logical);
                if (!scanDirectiveAt(data, length, offset, sourceRelativePath, documentId, documentGeneration,
                                     &state, visitor, userData, outDirectives, outTruncated)) return false;
                offset = logical.nextOffset > oldOffset ? logical.nextOffset : oldOffset + 1;
                continue;
            }
            state.atLineStart = false;
        }
        if (value == '/' && offset + 1 < length && data[offset + 1] == '/') {
            offset += 2;
            while (offset < length && data[offset] != '\n') ++offset;
            continue;
        }
        if (value == '/' && offset + 1 < length && data[offset + 1] == '*') {
            state.inBlockComment = true;
            offset += 2;
            continue;
        }
        if (value == '"') { state.inString = true; ++offset; continue; }
        if (value == '\'') { state.inCharacter = true; ++offset; continue; }
        if (value == 'R' && offset + 1 < length && data[offset + 1] == '"') {
            uint32_t cursor = offset + 2;
            uint32_t delimiterLength = 0;
            while (cursor < length && data[cursor] != '(' && data[cursor] != '\n' && delimiterLength <= 4) {
                state.rawDelimiter[delimiterLength++] = data[cursor++];
            }
            if (cursor < length && data[cursor] == '(' && delimiterLength <= 4) {
                state.rawDelimiterLength = delimiterLength;
                state.inRawString = true;
                offset = cursor + 1;
                continue;
            }
        }
        if (value == '\n') state.atLineStart = true;
        ++offset;
    }
    return true;
}

bool IncludeGraphAddNode(IncludeGraph* graph, const char* relativePath, bool dirty, uint64_t* outNodeId) {
    if (outNodeId) *outNodeId = 0;
    if (!graph || !relativePath || relativePath[0] == '\0') return false;
    char normalized[kMaxPathBytes] = {};
    if (!normalizeRelative(relativePath, normalized, sizeof(normalized))) return false;
    const uint32_t existing = findNodeExact(graph, normalized);
    if (existing != kInvalidIndex) {
        if (dirty) graph->nodes[existing].dirty = true;
        if (outNodeId) *outNodeId = graph->nodes[existing].nodeId;
        return true;
    }
    if (!graph->nodes || graph->nodeCount >= graph->nodeCapacity) {
        graph->truncated = true;
        return false;
    }
    IncludeNode& node = graph->nodes[graph->nodeCount];
    node = IncludeNode();
    node.nodeId = graph->nodeCount + 1;
    copyText(node.relativePath, sizeof(node.relativePath), normalized);
    node.sourceFile = IsIncludeGraphSourcePath(normalized);
    node.headerFile = IsIncludeGraphHeaderPath(normalized);
    node.dirty = dirty;
    if (outNodeId) *outNodeId = node.nodeId;
    ++graph->nodeCount;
    return true;
}

bool IncludeGraphAddEdge(IncludeGraph* graph, const IncludeEdge& edge) {
    if (!graph || !graph->edges || graph->edgeCount >= graph->edgeCapacity) {
        if (graph) graph->truncated = true;
        return false;
    }
    graph->edges[graph->edgeCount++] = edge;
    return true;
}

bool IncludeGraphResolveAll(IncludeGraph* graph, const IncludeGraphRequest& request,
                            uint64_t* outResolved, uint64_t* outUnresolved) {
    if (outResolved) *outResolved = 0;
    if (outUnresolved) *outUnresolved = 0;
    if (!graph) return false;
    // Rescans reuse the same graph storage.  Candidate slices are rebuilt
    // from scratch so old ambiguous resolutions cannot leak into new ones.
    graph->ambiguousCandidateCount = 0;
    for (uint32_t i = 0; i < graph->edgeCount; ++i) {
        if (!resolveEdge(graph, &graph->edges[i], request)) return false;
        if (graph->edges[i].resolution.state == IncludeResolutionState::Resolved) {
            if (outResolved) ++*outResolved;
        } else if (outUnresolved) ++*outUnresolved;
    }
    return true;
}

bool IncludeGraphBuildReverseEdges(IncludeGraph* graph) {
    if (!graph || !graph->nodes || !graph->edges) return false;
    for (uint32_t i = 0; i < graph->nodeCount; ++i) {
        graph->nodes[i].outgoingEdgeOffset = 0;
        graph->nodes[i].outgoingEdgeCount = 0;
        graph->nodes[i].incomingEdgeOffset = 0;
        graph->nodes[i].incomingEdgeCount = 0;
    }
    for (uint32_t i = 0; i < graph->edgeCount; ++i) {
        const uint32_t source = sourceNodeIndex(graph, graph->edges[i]);
        if (source != kInvalidIndex) ++graph->nodes[source].outgoingEdgeCount;
        if (graph->edges[i].resolution.state == IncludeResolutionState::Resolved) {
            const uint32_t target = targetNodeIndex(graph, graph->edges[i]);
            if (target != kInvalidIndex) ++graph->nodes[target].incomingEdgeCount;
        }
    }
    uint32_t outgoing = 0;
    uint32_t incoming = 0;
    for (uint32_t i = 0; i < graph->nodeCount; ++i) {
        graph->nodes[i].outgoingEdgeOffset = outgoing;
        graph->nodes[i].incomingEdgeOffset = incoming;
        outgoing += graph->nodes[i].outgoingEdgeCount;
        incoming += graph->nodes[i].incomingEdgeCount;
        graph->nodes[i].outgoingEdgeCount = 0;
        graph->nodes[i].incomingEdgeCount = 0;
    }
    uint32_t outgoingCursor[kIncludeGraphMaxNodes] = {};
    uint32_t incomingCursor[kIncludeGraphMaxNodes] = {};
    for (uint32_t i = 0; i < graph->nodeCount; ++i) {
        outgoingCursor[i] = graph->nodes[i].outgoingEdgeOffset;
        incomingCursor[i] = graph->nodes[i].incomingEdgeOffset;
    }
    for (uint32_t i = 0; i < graph->edgeCount; ++i) {
        const uint32_t source = sourceNodeIndex(graph, graph->edges[i]);
        if (source != kInvalidIndex) {
            graph->outgoingEdgeIndices[outgoingCursor[source]++] = i;
            ++graph->nodes[source].outgoingEdgeCount;
        }
        if (graph->edges[i].resolution.state == IncludeResolutionState::Resolved) {
            const uint32_t target = targetNodeIndex(graph, graph->edges[i]);
            if (target != kInvalidIndex) {
                graph->incomingEdgeIndices[incomingCursor[target]++] = i;
                ++graph->nodes[target].incomingEdgeCount;
            }
        }
    }
    // Edges are scanned in source-path order, but sort each segment explicitly
    // so a filesystem provider cannot affect panel or traversal order.
    for (uint32_t n = 0; n < graph->nodeCount; ++n) {
        const uint32_t start = graph->nodes[n].outgoingEdgeOffset;
        const uint32_t count = graph->nodes[n].outgoingEdgeCount;
        for (uint32_t i = 1; i < count; ++i) {
            const uint32_t value = graph->outgoingEdgeIndices[start + i];
            uint32_t p = i;
            while (p > 0) {
                const IncludeEdge& previous = graph->edges[graph->outgoingEdgeIndices[start + p - 1]];
                const IncludeEdge& current = graph->edges[value];
                bool before = previous.directive.line < current.directive.line ||
                    (previous.directive.line == current.directive.line && previous.directive.column <= current.directive.column);
                if (before) break;
                graph->outgoingEdgeIndices[start + p] = graph->outgoingEdgeIndices[start + p - 1];
                --p;
            }
            graph->outgoingEdgeIndices[start + p] = value;
        }
        const uint32_t reverseStart = graph->nodes[n].incomingEdgeOffset;
        const uint32_t reverseCount = graph->nodes[n].incomingEdgeCount;
        for (uint32_t i = 1; i < reverseCount; ++i) {
            const uint32_t value = graph->incomingEdgeIndices[reverseStart + i];
            uint32_t p = i;
            while (p > 0) {
                const IncludeEdge& previous = graph->edges[graph->incomingEdgeIndices[reverseStart + p - 1]];
                const IncludeEdge& current = graph->edges[value];
                const bool before = pathLess(previous.sourceRelativePath, current.sourceRelativePath) ||
                    (equalExact(previous.sourceRelativePath, current.sourceRelativePath) && previous.directive.line <= current.directive.line);
                if (before) break;
                graph->incomingEdgeIndices[reverseStart + p] = graph->incomingEdgeIndices[reverseStart + p - 1];
                --p;
            }
            graph->incomingEdgeIndices[reverseStart + p] = value;
        }
    }
    return true;
}

bool IncludeGraphDetectCycles(IncludeGraph* graph, uint32_t* outCycleCount) {
    if (outCycleCount) *outCycleCount = 0;
    if (!graph || !graph->nodes || !graph->edges) return false;
    graph->cycleCount = 0;
    graph->cycleMemberCount = 0;
    graph->cycleEdgeCount = 0;
    uint32_t order[kIncludeGraphMaxNodes] = {};
    uint32_t orderCount = 0;
    bool visited[kIncludeGraphMaxNodes] = {};
    uint32_t nodes[kIncludeGraphMaxNodes] = {};
    uint32_t cursors[kIncludeGraphMaxNodes] = {};
    for (uint32_t start = 0; start < graph->nodeCount; ++start) {
        if (visited[start]) continue;
        uint32_t depth = 0;
        nodes[depth] = start;
        cursors[depth] = 0;
        visited[start] = true;
        ++depth;
        while (depth > 0) {
            const uint32_t nodeIndex = nodes[depth - 1];
            const IncludeNode& node = graph->nodes[nodeIndex];
            bool descended = false;
            while (cursors[depth - 1] < node.outgoingEdgeCount) {
                const uint32_t edgeIndex = graph->outgoingEdgeIndices[node.outgoingEdgeOffset + cursors[depth - 1]++];
                const IncludeEdge& edge = graph->edges[edgeIndex];
                if (!isTraversalEdge(edge, true)) continue;
                const uint32_t target = targetNodeIndex(graph, edge);
                if (target == kInvalidIndex || visited[target]) continue;
                visited[target] = true;
                nodes[depth] = target;
                cursors[depth] = 0;
                ++depth;
                descended = true;
                break;
            }
            if (descended) continue;
            if (orderCount < kIncludeGraphMaxNodes) order[orderCount++] = nodeIndex;
            --depth;
        }
    }
    uint32_t component[kIncludeGraphMaxNodes];
    for (uint32_t i = 0; i < graph->nodeCount; ++i) component[i] = kInvalidIndex;
    uint32_t componentSizes[kIncludeGraphMaxNodes] = {};
    uint32_t componentCount = 0;
    uint32_t stack[kIncludeGraphMaxNodes] = {};
    for (uint32_t orderIndex = orderCount; orderIndex > 0; --orderIndex) {
        const uint32_t start = order[orderIndex - 1];
        if (component[start] != kInvalidIndex) continue;
        const uint32_t id = componentCount++;
        uint32_t stackCount = 0;
        stack[stackCount++] = start;
        component[start] = id;
        while (stackCount > 0) {
            const uint32_t nodeIndex = stack[--stackCount];
            ++componentSizes[id];
            const IncludeNode& node = graph->nodes[nodeIndex];
            for (uint32_t i = 0; i < node.incomingEdgeCount; ++i) {
                const IncludeEdge& edge = graph->edges[graph->incomingEdgeIndices[node.incomingEdgeOffset + i]];
                if (!isTraversalEdge(edge, true)) continue;
                const uint32_t source = sourceNodeIndex(graph, edge);
                if (source != kInvalidIndex && component[source] == kInvalidIndex) {
                    component[source] = id;
                    stack[stackCount++] = source;
                }
            }
        }
    }
    for (uint32_t id = 0; id < componentCount; ++id) {
        bool selfCycle = false;
        bool hasCycle = componentSizes[id] > 1;
        for (uint32_t i = 0; i < graph->edgeCount; ++i) {
            const IncludeEdge& edge = graph->edges[i];
            const uint32_t source = sourceNodeIndex(graph, edge);
            const uint32_t target = targetNodeIndex(graph, edge);
            if (source == kInvalidIndex || target == kInvalidIndex || component[source] != id || component[target] != id ||
                !isTraversalEdge(edge, true)) continue;
            if (source == target) selfCycle = true;
        }
        hasCycle = hasCycle || selfCycle;
        if (!hasCycle) continue;
        if (graph->cycleCount >= graph->cycleCapacity || graph->cycleMemberCount + componentSizes[id] > graph->cycleMemberCapacity) {
            graph->truncated = true;
            continue;
        }
        IncludeCycleGroup& cycle = graph->cycles[graph->cycleCount];
        cycle = IncludeCycleGroup();
        cycle.cycleId = graph->cycleCount + 1;
        cycle.memberOffset = graph->cycleMemberCount;
        cycle.edgeOffset = graph->cycleEdgeCount;
        cycle.memberCount = componentSizes[id];
        uint32_t memberCursor = graph->cycleMemberCount;
        for (uint32_t node = 0; node < graph->nodeCount; ++node) {
            if (component[node] != id) continue;
            graph->cycleMembers[memberCursor++] = node;
            if (cycle.representativePath[0] == '\0' || pathLess(graph->nodes[node].relativePath, cycle.representativePath))
                copyText(cycle.representativePath, sizeof(cycle.representativePath), graph->nodes[node].relativePath);
        }
        for (uint32_t i = 0; i < graph->edgeCount; ++i) {
            const IncludeEdge& edge = graph->edges[i];
            const uint32_t source = sourceNodeIndex(graph, edge);
            const uint32_t target = targetNodeIndex(graph, edge);
            if (source == kInvalidIndex || target == kInvalidIndex || component[source] != id || component[target] != id ||
                !isTraversalEdge(edge, true)) continue;
            if (graph->cycleEdgeCount >= graph->cycleEdgeCapacity) { graph->truncated = true; break; }
            graph->cycleEdges[graph->cycleEdgeCount++] = i;
            if (edge.directive.directiveState == IncludeDirectiveState::ConditionalUnknown) cycle.containsConditionalEdge = true;
        }
        cycle.edgeCount = graph->cycleEdgeCount - cycle.edgeOffset;
        cycle.selfCycle = selfCycle;
        graph->cycleMemberCount = memberCursor;
        ++graph->cycleCount;
    }
    if (outCycleCount) *outCycleCount = graph->cycleCount;
    return true;
}

bool IncludeGraphStart(IncludeGraphBuildOperation* operation, IncludeGraph* graph,
                       const IncludeGraphRequest& request, uint64_t nowMs,
                       uint64_t* outOperationId, IncludeGraphErrorCode* outError) {
    if (outOperationId) *outOperationId = 0;
    if (outError) *outError = IncludeGraphErrorCode::None;
    if (!operation || !graph || !request.rootPath[0] || !request.projectId[0]) {
        if (outError) *outError = IncludeGraphErrorCode::NoProject;
        return false;
    }
    static uint64_t nextOperationId = 1;
    if (IncludeGraphIsActive(operation)) {
        operation->cancellationRequested = true;
        operation->state = IncludeGraphBuildState::Cancelled;
        operation->terminalReported = true;
    }
    IncludeGraphBuildOperationInit(operation);
    operation->operationId = nextOperationId == 0 ? 1 : nextOperationId++;
    if (nextOperationId == 0) nextOperationId = 1;
    operation->projectId = hashText(request.projectId);
    operation->projectGeneration = request.projectGeneration;
    operation->state = IncludeGraphBuildState::Enumerating;
    operation->graph = graph;
    operation->startedAtMs = nowMs;
    operation->nextDirectiveId = 1;
    operation->nextEdgeId = 1;
    copyOperationRequest(operation, request);
    graph->complete = false;
    graph->truncated = false;
    graph->graphId = 0;
    graph->projectId = operation->projectId;
    graph->projectGeneration = request.projectGeneration;
    graph->graphGeneration = operation->operationId;
    graph->nodeCount = 0;
    graph->edgeCount = 0;
    graph->cycleCount = 0;
    graph->ambiguousCandidateCount = 0;
    graph->cycleMemberCount = 0;
    graph->cycleEdgeCount = 0;
    pushDirectory(operation, "");
    if (outOperationId) *outOperationId = operation->operationId;
    return true;
}

bool IncludeGraphPoll(IncludeGraphBuildOperation* operation, uint64_t operationId,
                      uint32_t workBudget, uint64_t nowMs) {
    if (!operation || operation->operationId != operationId) return false;
    if (operation->state == IncludeGraphBuildState::Completed || operation->state == IncludeGraphBuildState::Cancelled ||
        operation->state == IncludeGraphBuildState::Failed || operation->state == IncludeGraphBuildState::Idle) return true;
    if (operation->cancellationRequested) {
        operation->state = IncludeGraphBuildState::Cancelling;
        operation->error = IncludeGraphErrorCode::Cancelled;
        operation->state = IncludeGraphBuildState::Cancelled;
        operation->terminalReported = true;
        return true;
    }
    if (nowMs >= operation->startedAtMs && nowMs - operation->startedAtMs > kIncludeGraphMaxDurationMs) {
        operation->state = IncludeGraphBuildState::Failed;
        operation->error = IncludeGraphErrorCode::Timeout;
        operation->terminalReported = true;
        return true;
    }
    if (workBudget == 0) workBudget = 1;
    for (uint32_t work = 0; work < workBudget; ++work) {
        if (operation->cancellationRequested) break;
        bool progressed = false;
        switch (operation->state) {
        case IncludeGraphBuildState::Enumerating: progressed = pollEnumeration(operation); break;
        case IncludeGraphBuildState::Scanning: progressed = pollScanning(operation); break;
        case IncludeGraphBuildState::Resolving: progressed = pollResolving(operation); break;
        case IncludeGraphBuildState::BuildingReverseEdges:
            progressed = IncludeGraphBuildReverseEdges(operation->graph);
            operation->state = progressed ? IncludeGraphBuildState::DetectingCycles : IncludeGraphBuildState::Failed;
            if (!progressed) operation->error = IncludeGraphErrorCode::Internal;
            break;
        case IncludeGraphBuildState::DetectingCycles:
            progressed = IncludeGraphDetectCycles(operation->graph, nullptr);
            if (progressed) {
                operation->graph->complete = true;
                operation->graph->graphId = operation->operationId;
                operation->graph->truncated = operation->graph->truncated || operation->truncated;
                operation->truncated = operation->graph->truncated;
                operation->state = IncludeGraphBuildState::Completed;
                operation->terminalReported = true;
            } else {
                operation->error = IncludeGraphErrorCode::Internal;
                operation->state = IncludeGraphBuildState::Failed;
                operation->terminalReported = true;
            }
            break;
        default: progressed = false; break;
        }
        if (operation->state == IncludeGraphBuildState::Failed || operation->state == IncludeGraphBuildState::Completed) break;
        if (!progressed && operation->state == IncludeGraphBuildState::Enumerating && operation->pendingCount == 0 && !operation->currentDirectoryActive) {
            addDirtyOnlyNodes(operation);
            operation->state = IncludeGraphBuildState::Scanning;
            continue;
        }
        if (!progressed && operation->state == IncludeGraphBuildState::Scanning && operation->scanIndex >= operation->graph->nodeCount) {
            operation->state = IncludeGraphBuildState::Resolving;
            continue;
        }
    }
    if (operation->cancellationRequested && operation->state != IncludeGraphBuildState::Completed && operation->state != IncludeGraphBuildState::Failed) {
        operation->state = IncludeGraphBuildState::Cancelled;
        operation->error = IncludeGraphErrorCode::Cancelled;
        operation->terminalReported = true;
    }
    return true;
}

bool IncludeGraphCancel(IncludeGraphBuildOperation* operation, uint64_t operationId) {
    if (!operation || operation->operationId != operationId || !IncludeGraphIsActive(operation)) return false;
    operation->cancellationRequested = true;
    operation->state = IncludeGraphBuildState::Cancelling;
    return true;
}

bool IncludeGraphRelease(IncludeGraphBuildOperation* operation, uint64_t operationId) {
    if (!operation || operation->operationId != operationId) return false;
    if (IncludeGraphIsActive(operation)) return false;
    operation->state = IncludeGraphBuildState::Idle;
    operation->error = IncludeGraphErrorCode::Released;
    operation->graph = nullptr;
    operation->dirtyDocumentCount = 0;
    operation->terminalReported = false;
    return true;
}

bool IncludeGraphIsActive(const IncludeGraphBuildOperation* operation) {
    if (!operation) return false;
    return operation->state == IncludeGraphBuildState::Enumerating || operation->state == IncludeGraphBuildState::Scanning ||
           operation->state == IncludeGraphBuildState::Resolving || operation->state == IncludeGraphBuildState::BuildingReverseEdges ||
           operation->state == IncludeGraphBuildState::DetectingCycles || operation->state == IncludeGraphBuildState::Cancelling;
}

const IncludeGraphBuildOperation* IncludeGraphOperationInfo(const IncludeGraphBuildOperation* operation) {
    return operation;
}

bool IncludeGraphRescanDocument(IncludeGraph* graph, const IncludeGraphRequest& request,
                                const char* relativePath, uint64_t documentId,
                                uint64_t documentGeneration, const char* data,
                                uint32_t length, bool fromDirtySnapshot,
                                IncludeGraphErrorCode* outError) {
    if (outError) *outError = IncludeGraphErrorCode::None;
    if (!graph || !relativePath || !data || length > kIncludeGraphMaxScanFileBytes) {
        if (outError) *outError = IncludeGraphErrorCode::FileReadFailed;
        return false;
    }
    uint32_t write = 0;
    for (uint32_t i = 0; i < graph->edgeCount; ++i) {
        if (equalFold(graph->edges[i].sourceRelativePath, relativePath)) continue;
        graph->edges[write++] = graph->edges[i];
    }
    graph->edgeCount = write;
    const uint32_t sourceNode = findNodeExact(graph, relativePath);
    if (sourceNode == kInvalidIndex && !IncludeGraphAddNode(graph, relativePath, fromDirtySnapshot, nullptr)) {
        if (outError) *outError = IncludeGraphErrorCode::NodeLimit;
        return false;
    }
    if (sourceNode != kInvalidIndex && fromDirtySnapshot) graph->nodes[sourceNode].dirty = true;
    uint64_t nextEdge = 1;
    uint64_t nextDirective = 1;
    for (uint32_t i = 0; i < graph->edgeCount; ++i) {
        if (graph->edges[i].edgeId >= nextEdge) nextEdge = graph->edges[i].edgeId + 1;
        if (graph->edges[i].directive.directiveId >= nextDirective) nextDirective = graph->edges[i].directive.directiveId + 1;
    }
    struct ScanContext {
        IncludeGraph* graph;
        uint64_t* nextEdgeId;
        uint64_t* nextDirectiveId;
        bool fromDirtySnapshot;
    } context = { graph, &nextEdge, &nextDirective, fromDirtySnapshot };
    bool truncated = false;
    IncludeGraphScanDocument(data, length, relativePath, documentId, documentGeneration,
                             addScannedDirective, &context, nullptr, &truncated);
    graph->truncated = graph->truncated || truncated;
    uint64_t resolved = 0;
    uint64_t unresolved = 0;
    if (!resolveAndAnalyze(graph, request, &resolved, &unresolved)) {
        if (outError) *outError = IncludeGraphErrorCode::Internal;
        return false;
    }
    graph->graphGeneration += 1;
    graph->complete = true;
    return true;
}

bool IncludeGraphBuildTraversal(const IncludeGraph* graph, const char* startRelativePath,
                                IncludeGraphTraversalDirection direction, bool includeConditional,
                                IncludeGraphTraversalResult* result) {
    if (!graph || !startRelativePath || !result || !result->nodeIndices || !result->depths || !result->parentEdgeIndices) return false;
    result->nodeCount = 0;
    result->edgeCount = 0;
    result->maxDepth = 0;
    result->truncated = false;
    const uint32_t start = findNodeExact(graph, startRelativePath);
    if (start == kInvalidIndex) return false;
    uint32_t queue[kIncludeGraphMaxNodes] = {};
    uint32_t queueDepth[kIncludeGraphMaxNodes] = {};
    uint32_t queueParentEdges[kIncludeGraphMaxNodes] = {};
    bool visited[kIncludeGraphMaxNodes] = {};
    uint32_t head = 0;
    uint32_t tail = 0;
    queue[tail] = start;
    queueDepth[tail++] = 0;
    queueParentEdges[0] = kInvalidIndex;
    visited[start] = true;
    while (head < tail) {
        const uint32_t node = queue[head];
        const uint32_t depth = queueDepth[head++];
        if (node != start) {
            if (result->nodeCount >= result->nodeCapacity) { result->truncated = true; break; }
            result->nodeIndices[result->nodeCount] = node;
            result->depths[result->nodeCount] = depth;
            result->parentEdgeIndices[result->nodeCount] = queueParentEdges[head - 1];
            ++result->nodeCount;
            if (depth > result->maxDepth) result->maxDepth = depth;
        }
        if (depth >= kIncludeGraphMaxTraversalDepth) { result->truncated = true; continue; }
        const IncludeNode& current = graph->nodes[node];
        const uint32_t offset = direction == IncludeGraphTraversalDirection::Includes ? current.outgoingEdgeOffset : current.incomingEdgeOffset;
        const uint32_t count = direction == IncludeGraphTraversalDirection::Includes ? current.outgoingEdgeCount : current.incomingEdgeCount;
        const uint32_t* indices = direction == IncludeGraphTraversalDirection::Includes ? graph->outgoingEdgeIndices : graph->incomingEdgeIndices;
        for (uint32_t i = 0; i < count; ++i) {
            if (++result->edgeCount > kIncludeGraphMaxTraversalEdges) { result->truncated = true; return true; }
            const uint32_t edgeIndex = indices[offset + i];
            const IncludeEdge& edge = graph->edges[edgeIndex];
            if (!isTraversalEdge(edge, includeConditional)) continue;
            const uint32_t next = direction == IncludeGraphTraversalDirection::Includes ? targetNodeIndex(graph, edge) : sourceNodeIndex(graph, edge);
            if (next == kInvalidIndex || visited[next]) continue;
            visited[next] = true;
            if (tail >= kIncludeGraphMaxNodes) { result->truncated = true; return true; }
            queue[tail] = next;
            queueDepth[tail++] = depth + 1;
            queueParentEdges[tail - 1] = edgeIndex;
        }
    }
    return true;
}

const IncludeEdge* IncludeGraphFindDirectiveAt(const IncludeGraph* graph,
                                                const char* sourceRelativePath,
                                                uint64_t caretOffset) {
    if (!graph || !sourceRelativePath) return nullptr;
    for (uint32_t i = 0; i < graph->edgeCount; ++i) {
        const IncludeEdge& edge = graph->edges[i];
        if (!equalFold(edge.sourceRelativePath, sourceRelativePath)) continue;
        const uint64_t end = edge.directive.includeTextByteOffset + edge.directive.includeTextLength + 2;
        if (caretOffset >= edge.directive.directiveByteOffset && caretOffset <= end) return &edge;
    }
    return nullptr;
}

} // namespace developer_studio
} // namespace guidexos
