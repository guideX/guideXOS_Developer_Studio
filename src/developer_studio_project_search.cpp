#include "developer_studio_project_search.h"

#include "developer_studio_find.h"

namespace guidexos {
namespace developer_studio {
namespace {

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

static char lowerAscii(char value) {
    return value >= 'A' && value <= 'Z' ? static_cast<char>(value + ('a' - 'A')) : value;
}

static bool equalAsciiInsensitive(const char* left, const char* right) {
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

static bool hasAbsoluteOrDrivePrefix(const char* path) {
    return path && (isSlash(path[0]) || isSlash(path[1]) || path[1] == ':');
}

static bool isWithinRoot(const char* root, const char* path) {
    if (!root || !path) return false;
    char normalizedRoot[kMaxPathBytes] = {};
    char normalizedPath[kMaxPathBytes] = {};
    if (!NormalizePath(root, normalizedRoot, sizeof(normalizedRoot)) ||
        !NormalizePath(path, normalizedPath, sizeof(normalizedPath))) return false;
    const uint32_t rootLength = textLength(normalizedRoot, sizeof(normalizedRoot));
    const uint32_t pathLength = textLength(normalizedPath, sizeof(normalizedPath));
    if (rootLength > pathLength) return false;
    for (uint32_t i = 0; i < rootLength; ++i)
        if (lowerAscii(normalizedRoot[i]) != lowerAscii(normalizedPath[i])) return false;
    return pathLength == rootLength || normalizedPath[rootLength] == '/';
}

static bool relativePathFor(const char* root, const char* absolute, char* output, uint32_t outputSize) {
    if (!root || !absolute || !output || !isWithinRoot(root, absolute)) return false;
    char normalizedRoot[kMaxPathBytes] = {};
    char normalizedAbsolute[kMaxPathBytes] = {};
    if (!NormalizePath(root, normalizedRoot, sizeof(normalizedRoot)) ||
        !NormalizePath(absolute, normalizedAbsolute, sizeof(normalizedAbsolute))) return false;
    const uint32_t rootLength = textLength(normalizedRoot, sizeof(normalizedRoot));
    const uint32_t absoluteLength = textLength(normalizedAbsolute, sizeof(normalizedAbsolute));
    if (absoluteLength == rootLength) return false;
    uint32_t start = rootLength;
    if (normalizedAbsolute[start] == '/') ++start;
    if (start >= absoluteLength) return false;
    return copyText(output, outputSize, normalizedAbsolute + start);
}

static uint64_t hashProjectId(const char* text) {
    uint64_t hash = 1469598103934665603ull;
    if (!text) return hash;
    for (uint32_t i = 0; text[i] != '\0'; ++i) {
        hash ^= static_cast<unsigned char>(text[i]);
        hash *= 1099511628211ull;
    }
    return hash == 0 ? 1 : hash;
}

static void setError(ProjectSearchService* service, ProjectSearchErrorCode error) {
    if (!service) return;
    service->operation.error = error;
}

static void setNotice(ProjectSearchService* service, const char* notice) {
    if (!service) return;
    copyText(service->operation.notice, sizeof(service->operation.notice), notice ? notice : "");
}

static void markTruncated(ProjectSearchService* service, ProjectSearchErrorCode reason) {
    if (!service) return;
    service->operation.truncated = true;
    if (service->operation.error == ProjectSearchErrorCode::None) service->operation.error = reason;
    setNotice(service, "Search results truncated.");
}

static bool isTerminal(ProjectSearchState state) {
    return state == ProjectSearchState::Completed || state == ProjectSearchState::Failed ||
        state == ProjectSearchState::Cancelled || state == ProjectSearchState::Idle;
}

static void clearResultStorage(ProjectSearchService* service) {
    if (!service) return;
    service->operation.resultFileCount = 0;
    service->operation.resultMatchCount = 0;
    for (uint32_t i = 0; i < kProjectSearchMaxResultFiles; ++i) {
        service->groups[i].relativePath[0] = '\0';
        service->groups[i].firstMatchIndex = 0;
        service->groups[i].matchCount = 0;
    }
}

static bool isDefaultExcludedDirectory(const char* name) {
    static const char* const names[] = {
        ".git", ".vs", ".idea", "out", "build", "bin", "obj", "dist", "node_modules",
        ".guidexos", ".developerstudio", ".gxdeploy", ".gxbuild", "developerstudio-tmp", "guidexos-tmp"
    };
    for (uint32_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i)
        if (equalAsciiInsensitive(name, names[i])) return true;
    return false;
}

static bool isSupportedCxxPath(const char* path) {
    const char* name = BaseName(path);
    const uint32_t length = textLength(name, kMaxNameBytes);
    uint32_t dot = length;
    for (uint32_t i = 0; i < length; ++i) if (name[i] == '.') dot = i;
    if (dot == length) return false;
    const char* extension = name + dot;
    static const char* const extensions[] = {
        ".c", ".h", ".cc", ".cpp", ".cxx", ".hh", ".hpp", ".hxx"
    };
    for (uint32_t i = 0; i < sizeof(extensions) / sizeof(extensions[0]); ++i)
        if (equalAsciiInsensitive(extension, extensions[i])) return true;
    return false;
}

static bool isSupportedProjectTextPath(const char* path) {
    const char* name = BaseName(path);
    if (equalAsciiInsensitive(name, "guidexos.project")) return true;
    const uint32_t length = textLength(name, kMaxNameBytes);
    uint32_t dot = length;
    for (uint32_t i = 0; i < length; ++i) if (name[i] == '.') dot = i;
    if (dot == length) return false;
    const char* extension = name + dot;
    static const char* const extensions[] = { ".gxproj", ".json", ".md", ".txt", ".cmake" };
    for (uint32_t i = 0; i < sizeof(extensions) / sizeof(extensions[0]); ++i)
        if (equalAsciiInsensitive(extension, extensions[i])) return true;
    return isSupportedCxxPath(path);
}

static bool hasPathTraversal(const char* path) {
    if (!path) return true;
    uint32_t start = 0;
    const uint32_t length = textLength(path, kProjectSearchMaxPatternBytesEach + 1u);
    for (uint32_t i = 0; i <= length; ++i) {
        if (i < length && !isSlash(path[i])) continue;
        const uint32_t segmentLength = i - start;
        if (segmentLength == 2 && path[start] == '.' && path[start + 1] == '.') return true;
        start = i + 1;
    }
    return false;
}

static bool validatePattern(const char* pattern, uint32_t length) {
    if (!pattern || length == 0 || length > kProjectSearchMaxPatternBytesEach ||
        hasAbsoluteOrDrivePrefix(pattern) || hasPathTraversal(pattern)) return false;
    for (uint32_t i = 0; i < length; ++i) {
        const unsigned char value = static_cast<unsigned char>(pattern[i]);
        if (value == 0 || value < 0x20 || value == 0x7f || value == '\\') return false;
    }
    return true;
}

static bool wildcardMatch(const char* pattern, const char* text) {
    const uint32_t patternLength = textLength(pattern, kProjectSearchMaxPatternBytesEach + 1u);
    const uint32_t textLengthValue = textLength(text, kProjectSearchMaxRelativePathBytes + 1u);
    uint32_t p = 0;
    uint32_t t = 0;
    uint32_t star = 0xFFFFFFFFu;
    uint32_t starText = 0;
    while (t < textLengthValue) {
        if (p < patternLength && pattern[p] != '*' &&
            (pattern[p] == '?' ? text[t] != '/' : lowerAscii(pattern[p]) == lowerAscii(text[t]))) {
            ++p;
            ++t;
        } else if (p < patternLength && pattern[p] == '*') {
            star = p++;
            starText = t;
        } else if (star != 0xFFFFFFFFu) {
            p = star + 1;
            t = ++starText;
        } else {
            return false;
        }
    }
    while (p < patternLength && pattern[p] == '*') ++p;
    return p == patternLength;
}

static bool patternSetMatches(char patterns[][kProjectSearchMaxPatternBytesEach + 1],
                              uint32_t count, const char* path) {
    for (uint32_t i = 0; i < count; ++i) if (wildcardMatch(patterns[i], path)) return true;
    return false;
}

static bool parsePatternSet(const char* input, char output[][kProjectSearchMaxPatternBytesEach + 1],
                            uint32_t* outCount, ProjectSearchErrorCode* error,
                            ProjectSearchErrorCode invalidError) {
    if (outCount) *outCount = 0;
    if (!input || input[0] == '\0') return true;
    const uint32_t totalLength = textLength(input, kProjectSearchMaxPatternBytes + 1u);
    if (totalLength > kProjectSearchMaxPatternBytes) {
        if (error) *error = ProjectSearchErrorCode::PatternTooLong;
        return false;
    }
    uint32_t start = 0;
    for (uint32_t i = 0; i <= totalLength; ++i) {
        if (i < totalLength && input[i] != ',' && input[i] != ';') continue;
        if (i == start || i - start > kProjectSearchMaxPatternBytesEach) {
            if (error) *error = invalidError;
            return false;
        }
        if (!outCount || *outCount >= kProjectSearchMaxPatterns) {
            if (error) *error = ProjectSearchErrorCode::PatternLimit;
            return false;
        }
        char pattern[kProjectSearchMaxPatternBytesEach + 1] = {};
        for (uint32_t j = start; j < i; ++j) pattern[j - start] = input[j] == '\\' ? '/' : input[j];
        pattern[i - start] = '\0';
        if (!validatePattern(pattern, i - start)) {
            if (error) *error = invalidError;
            return false;
        }
        copyText(output[*outCount], kProjectSearchMaxPatternBytesEach + 1, pattern);
        ++(*outCount);
        start = i + 1;
    }
    return true;
}

static bool copyRelativeFromEntry(const char* parent, const char* name, char* output, uint32_t outputSize) {
    if (!parent || !name || !output || name[0] == '\0' || PathContainsTraversal(name) ||
        hasAbsoluteOrDrivePrefix(name)) return false;
    for (uint32_t i = 0; name[i] != '\0'; ++i) if (isSlash(name[i])) return false;
    return JoinWorkspacePath(parent, name, output, outputSize);
}

static bool enqueueDirectory(ProjectSearchService* service, const char* path) {
    if (!service || !path || service->pendingCount >= kProjectSearchMaxPendingDirectories) {
        if (service) markTruncated(service, ProjectSearchErrorCode::DirectoryLimit);
        return false;
    }
    const uint32_t slot = service->pendingTail;
    if (!copyText(service->pendingDirectories[slot], kMaxPathBytes, path)) {
        markTruncated(service, ProjectSearchErrorCode::PathInvalid);
        return false;
    }
    service->pendingTail = (service->pendingTail + 1u) % kProjectSearchMaxPendingDirectories;
    ++service->pendingCount;
    return true;
}

static bool dequeueDirectory(ProjectSearchService* service) {
    if (!service || service->pendingCount == 0) return false;
    const uint32_t slot = service->pendingHead;
    copyText(service->currentDirectory, sizeof(service->currentDirectory), service->pendingDirectories[slot]);
    service->pendingHead = (service->pendingHead + 1u) % kProjectSearchMaxPendingDirectories;
    --service->pendingCount;
    ++service->directoriesVisited;
    if (service->directoriesVisited > kProjectSearchMaxDirectories) {
        markTruncated(service, ProjectSearchErrorCode::DirectoryLimit);
        return false;
    }
    uint32_t depth = 0;
    const char* root = service->rootPath;
    const uint32_t rootLength = textLength(root, kMaxPathBytes);
    const uint32_t pathLength = textLength(service->currentDirectory, kMaxPathBytes);
    for (uint32_t i = rootLength; i < pathLength; ++i) if (service->currentDirectory[i] == '/') ++depth;
    service->currentDirectoryDepth = depth;
    service->currentDirectoryActive = true;
    service->currentEntryCount = 0;
    service->currentEntryIndex = 0;
    if (!service->fileSystem.list || !service->fileSystem.list(service->fileSystem.userData,
        service->currentDirectory, service->currentEntries, kMaxWorkspaceEntries,
        &service->currentEntryCount, &service->operation.truncated)) {
        service->currentDirectoryActive = false;
        if (service->directoriesVisited == 1) {
            setError(service, ProjectSearchErrorCode::EnumerationFailed);
            service->operation.state = ProjectSearchState::Failed;
        }
        return false;
    }
    for (uint32_t i = 0; i < service->currentEntryCount; ++i) {
        for (uint32_t j = i + 1; j < service->currentEntryCount; ++j) {
            const char* left = service->currentEntries[j].name;
            const char* right = service->currentEntries[i].name;
            bool before = false;
            uint32_t k = 0;
            while (left[k] != '\0' && right[k] != '\0' && lowerAscii(left[k]) == lowerAscii(right[k])) ++k;
            if (lowerAscii(left[k]) != lowerAscii(right[k])) before = lowerAscii(left[k]) < lowerAscii(right[k]);
            else before = left[k] < right[k];
            if (before) {
                FileListEntry temp = service->currentEntries[i];
                service->currentEntries[i] = service->currentEntries[j];
                service->currentEntries[j] = temp;
            }
        }
    }
    if (service->operation.truncated) markTruncated(service, ProjectSearchErrorCode::FileLimit);
    return true;
}

static bool relativePathMatches(ProjectSearchService* service, const char* path) {
    if (!service || !path || path[0] == '\0' || PathContainsTraversal(path) ||
        hasAbsoluteOrDrivePrefix(path)) return false;
    if (service->includePatternCount == 0) {
        if (!isSupportedCxxPath(path)) return false;
    } else if (!isSupportedProjectTextPath(path)) {
        return false;
    }
    if (service->includePatternCount != 0 && !patternSetMatches(service->includePatterns, service->includePatternCount, path)) return false;
    if (service->excludePatternCount != 0 && patternSetMatches(service->excludePatterns, service->excludePatternCount, path)) return false;
    return true;
}

static const ProjectSearchDocumentSnapshot* dirtySnapshotFor(ProjectSearchService* service, const char* relativePath) {
    if (!service) return nullptr;
    for (uint32_t i = 0; i < service->dirtyDocumentCount; ++i)
        if (equalAsciiInsensitive(service->dirtyDocuments[i].relativePath, relativePath)) return &service->dirtyDocuments[i];
    return nullptr;
}

static void advancePosition(const char* data, uint32_t length, uint32_t* line, uint32_t* column) {
    if (!data || !line || !column) return;
    for (uint32_t i = 0; i < length; ++i) {
        if (data[i] == '\n') { ++(*line); *column = 1; }
        else ++(*column);
    }
}

static void makePreview(const char* data, uint32_t length, uint64_t offset, uint32_t matchLength,
                        ProjectSearchMatch* match) {
    if (!data || !match || offset > length) return;
    const uint32_t startOffset = static_cast<uint32_t>(offset);
    uint32_t lineStart = startOffset;
    while (lineStart > 0 && data[lineStart - 1] != '\n') --lineStart;
    uint32_t lineEnd = startOffset;
    while (lineEnd < length && data[lineEnd] != '\n') ++lineEnd;
    if (lineEnd > lineStart && data[lineEnd - 1] == '\r') --lineEnd;
    const uint32_t lineLength = lineEnd - lineStart;
    uint32_t localMatchEnd = startOffset + matchLength;
    if (localMatchEnd > lineEnd) localMatchEnd = lineEnd;
    const uint32_t localMatchStart = startOffset - lineStart;
    const uint32_t visibleMatchLength = localMatchEnd > startOffset ? localMatchEnd - startOffset : 0;
    uint32_t previewStart = 0;
    uint32_t previewLength = lineLength;
    if (previewLength > kProjectSearchMaxPreviewBytes) {
        previewStart = localMatchStart > 128u ? localMatchStart - 128u : 0;
        if (previewStart + kProjectSearchMaxPreviewBytes > lineLength)
            previewStart = lineLength - kProjectSearchMaxPreviewBytes;
        previewLength = kProjectSearchMaxPreviewBytes;
        match->previewLeftTruncated = previewStart != 0;
        match->previewRightTruncated = previewStart + previewLength < lineLength;
    } else {
        match->previewLeftTruncated = false;
        match->previewRightTruncated = false;
    }
    uint32_t output = 0;
    for (uint32_t i = 0; i < previewLength && output < kProjectSearchMaxPreviewBytes; ++i) {
        char value = data[lineStart + previewStart + i];
        if (value == '\t' || (static_cast<unsigned char>(value) < 0x20u && value != '\r')) value = ' ';
        match->preview[output++] = value;
    }
    match->preview[output] = '\0';
    match->previewMatchStart = localMatchStart >= previewStart ? localMatchStart - previewStart : 0;
    match->previewMatchLength = visibleMatchLength;
    if (match->previewMatchStart > output) match->previewMatchStart = output;
    if (match->previewMatchStart + match->previewMatchLength > output)
        match->previewMatchLength = output - match->previewMatchStart;
}

static int32_t findGroup(ProjectSearchService* service, const char* relativePath) {
    if (!service || !relativePath) return -1;
    for (uint32_t i = 0; i < service->operation.resultFileCount; ++i)
        if (equalAsciiInsensitive(service->groups[i].relativePath, relativePath)) return static_cast<int32_t>(i);
    return -1;
}

static bool appendMatch(ProjectSearchService* service, const char* relativePath,
                        ProjectSearchSourceKind sourceKind, uint64_t fileSize,
                        uint64_t documentId, uint32_t documentGeneration,
                        const ProjectSearchMatch& value) {
    if (!service) return false;
    int32_t groupIndex = findGroup(service, relativePath);
    if (groupIndex < 0) {
        if (service->operation.resultFileCount >= kProjectSearchMaxResultFiles) {
            markTruncated(service, ProjectSearchErrorCode::ResultLimit);
            return false;
        }
        groupIndex = static_cast<int32_t>(service->operation.resultFileCount++);
        ProjectSearchFileGroup& group = service->groups[groupIndex];
        copyText(group.relativePath, sizeof(group.relativePath), relativePath);
        group.firstMatchIndex = service->operation.resultMatchCount;
        group.matchCount = 0;
        group.sourceKind = sourceKind;
        group.fileSize = fileSize;
        group.documentId = documentId;
        group.documentGeneration = documentGeneration;
    }
    ProjectSearchFileGroup& group = service->groups[groupIndex];
    if (group.matchCount >= kProjectSearchMaxMatchesPerFile ||
        service->operation.resultMatchCount >= kProjectSearchMaxTotalMatches) {
        markTruncated(service, ProjectSearchErrorCode::MatchLimit);
        return false;
    }
    service->matches[service->operation.resultMatchCount] = value;
    ++service->operation.resultMatchCount;
    ++group.matchCount;
    ++service->operation.matchesFound;
    return true;
}

static void sortGroups(ProjectSearchService* service) {
    if (!service) return;
    for (uint32_t i = 0; i < service->operation.resultFileCount; ++i) {
        for (uint32_t j = i + 1; j < service->operation.resultFileCount; ++j) {
            const char* left = service->groups[j].relativePath;
            const char* right = service->groups[i].relativePath;
            bool before = false;
            uint32_t k = 0;
            while (left[k] != '\0' && right[k] != '\0' && lowerAscii(left[k]) == lowerAscii(right[k])) ++k;
            if (lowerAscii(left[k]) != lowerAscii(right[k])) before = lowerAscii(left[k]) < lowerAscii(right[k]);
            else before = left[k] < right[k];
            if (before) {
                ProjectSearchFileGroup temp = service->groups[i];
                service->groups[i] = service->groups[j];
                service->groups[j] = temp;
            }
        }
    }
}

static bool scanText(ProjectSearchService* service, const char* relativePath, const char* data,
                     uint32_t length, ProjectSearchSourceKind sourceKind, uint64_t fileSize,
                     uint64_t documentId, uint32_t documentGeneration) {
    if (!service || !data || length > kProjectSearchMaxFileBytes) return false;
    const uint32_t queryLength = textLength(service->parsedOptions.query, kFindMaxQueryBytes + 1u);
    if (queryLength == 0) return true;
    uint32_t line = 1;
    uint32_t column = 1;
    uint64_t offset = 0;
    while (offset < length) {
        if (FindLiteralMatchesAt(data, length, offset, service->parsedOptions.query, queryLength,
                                 service->parsedOptions.caseSensitive, service->parsedOptions.wholeWord)) {
            ProjectSearchMatch match = {};
            match.byteOffset = offset;
            match.line = line;
            match.column = column;
            match.matchLength = queryLength;
            makePreview(data, length, offset, queryLength, &match);
            if (!appendMatch(service, relativePath, sourceKind, fileSize, documentId, documentGeneration, match)) return false;
            advancePosition(data + offset, queryLength, &line, &column);
            offset += queryLength;
        } else {
            advancePosition(data + offset, 1, &line, &column);
            ++offset;
        }
        if (service->operation.bytesScanned >= kProjectSearchMaxBytesScanned) {
            markTruncated(service, ProjectSearchErrorCode::BytesLimit);
            return false;
        }
    }
    return true;
}

static bool scanCurrentFile(ProjectSearchService* service, const char* absolutePath,
                            const char* relativePath, const FileListEntry& entry) {
    if (!service || !absolutePath || !relativePath) return false;
    const ProjectSearchDocumentSnapshot* dirty = dirtySnapshotFor(service, relativePath);
    ProjectSearchSourceKind sourceKind = dirty ? ProjectSearchSourceKind::DirtyDocument : ProjectSearchSourceKind::Disk;
    uint64_t fileSize = entry.size;
    uint64_t documentId = 0;
    uint32_t documentGeneration = 0;
    uint32_t bytes = 0;
    const char* data = nullptr;
    if (dirty) {
        fileSize = dirty->length;
        documentId = dirty->documentId;
        documentGeneration = dirty->documentGeneration;
        data = dirty->data;
        bytes = dirty->length;
    } else {
        FileInfo info = {};
        if (!service->fileSystem.stat || !service->fileSystem.stat(service->fileSystem.userData, absolutePath, &info) ||
            info.kind != FileInfoKind::RegularFile) return false;
        fileSize = info.size;
        if (fileSize > kProjectSearchMaxFileBytes) {
            if (service->operation.error == ProjectSearchErrorCode::None) service->operation.error = ProjectSearchErrorCode::FileTooLarge;
            return false;
        }
        if (!service->fileSystem.read || !service->fileSystem.read(service->fileSystem.userData, absolutePath,
            service->scanBuffer, kProjectSearchMaxFileBytes, &bytes) || bytes > kProjectSearchMaxFileBytes) return false;
        data = service->scanBuffer;
    }
    service->operation.bytesScanned += bytes;
    if (service->operation.bytesScanned > kProjectSearchMaxBytesScanned) {
        markTruncated(service, ProjectSearchErrorCode::BytesLimit);
        return false;
    }
    if (LooksBinary(data, bytes)) return false;
    if (service->scanVisitor) {
        ProjectSearchScanFile file = {};
        file.relativePath = relativePath;
        file.data = data;
        file.length = bytes;
        file.sourceKind = sourceKind;
        file.fileSize = fileSize;
        file.documentId = documentId;
        file.documentGeneration = documentGeneration;
        if (!service->scanVisitor(service->scanVisitorUserData, &file)) {
            if (!service->operation.truncated)
                markTruncated(service, ProjectSearchErrorCode::ResultLimit);
            return false;
        }
        return true;
    }
    return scanText(service, relativePath, data, bytes, sourceKind, fileSize, documentId, documentGeneration);
}

static void finishIfReady(ProjectSearchService* service) {
    if (!service || service->operation.state == ProjectSearchState::Failed ||
        service->operation.state == ProjectSearchState::Cancelled) return;
    if (service->operation.truncated) {
        sortGroups(service);
        service->operation.state = ProjectSearchState::Completed;
        return;
    }
    if (!service->currentDirectoryActive && service->pendingCount == 0) {
        sortGroups(service);
        service->operation.state = ProjectSearchState::Completed;
    }
}

static bool validateRequest(ProjectSearchService* service, const ProjectSearchRequest* request,
                            ProjectSearchErrorCode* error) {
    if (!service || !request) { if (error) *error = ProjectSearchErrorCode::Internal; return false; }
    const uint32_t queryLength = textLength(request->options.query, kFindMaxQueryBytes + 2u);
    if (request->projectId[0] == '\0') { if (error) *error = ProjectSearchErrorCode::NoProject; return false; }
    if (queryLength == 0) { if (error) *error = ProjectSearchErrorCode::EmptyQuery; return false; }
    if (queryLength > kFindMaxQueryBytes) { if (error) *error = ProjectSearchErrorCode::QueryTooLong; return false; }
    if (request->projectGeneration == 0 || request->rootPath[0] == '\0' ||
        hasAbsoluteOrDrivePrefix(request->rootPath) == false || !request->fileSystem.stat ||
        !request->fileSystem.list || !request->fileSystem.read) {
        if (error) *error = ProjectSearchErrorCode::InvalidRoot;
        return false;
    }
    char normalizedRoot[kMaxPathBytes] = {};
    if (!NormalizePath(request->rootPath, normalizedRoot, sizeof(normalizedRoot)) ||
        (normalizedRoot[0] == '/' && normalizedRoot[1] == '/')) {
        if (error) *error = ProjectSearchErrorCode::InvalidRoot;
        return false;
    }
    FileInfo rootInfo = {};
    if (!request->fileSystem.stat(request->fileSystem.userData, normalizedRoot, &rootInfo) ||
        rootInfo.kind != FileInfoKind::Directory) {
        if (error) *error = ProjectSearchErrorCode::InvalidRoot;
        return false;
    }
    ProjectSearchErrorCode patternError = ProjectSearchErrorCode::None;
    if (!parsePatternSet(request->options.includePattern, service->includePatterns,
                         &service->includePatternCount, &patternError,
                         ProjectSearchErrorCode::InvalidIncludePattern)) {
        if (error) *error = patternError;
        return false;
    }
    if (!parsePatternSet(request->options.excludePattern, service->excludePatterns,
                         &service->excludePatternCount, &patternError,
                         ProjectSearchErrorCode::InvalidExcludePattern)) {
        if (error) *error = patternError;
        return false;
    }
    return true;
}

static void enterCancelled(ProjectSearchService* service) {
    if (!service || isTerminal(service->operation.state)) return;
    service->operation.state = ProjectSearchState::Cancelled;
    service->operation.error = ProjectSearchErrorCode::Cancelled;
    service->operation.cancellationRequested = true;
    service->currentDirectoryActive = false;
    service->pendingCount = 0;
}

} // namespace

const char* ProjectSearchStateName(ProjectSearchState state) {
    switch (state) {
    case ProjectSearchState::Idle: return "Idle";
    case ProjectSearchState::Enumerating: return "Enumerating";
    case ProjectSearchState::Searching: return "Searching";
    case ProjectSearchState::Cancelling: return "Cancelling";
    case ProjectSearchState::Completed: return "Completed";
    case ProjectSearchState::Failed: return "Failed";
    case ProjectSearchState::Cancelled: return "Cancelled";
    default: return "Unknown";
    }
}

const char* ProjectSearchErrorName(ProjectSearchErrorCode error) {
    switch (error) {
    case ProjectSearchErrorCode::None: return "PROJECT_SEARCH_NONE";
    case ProjectSearchErrorCode::NoProject: return "PROJECT_SEARCH_NO_PROJECT";
    case ProjectSearchErrorCode::EmptyQuery: return "PROJECT_SEARCH_EMPTY_QUERY";
    case ProjectSearchErrorCode::QueryTooLong: return "PROJECT_SEARCH_QUERY_TOO_LONG";
    case ProjectSearchErrorCode::InvalidRoot: return "PROJECT_SEARCH_PATH_INVALID";
    case ProjectSearchErrorCode::InvalidIncludePattern: return "PROJECT_SEARCH_INVALID_INCLUDE_PATTERN";
    case ProjectSearchErrorCode::InvalidExcludePattern: return "PROJECT_SEARCH_INVALID_EXCLUDE_PATTERN";
    case ProjectSearchErrorCode::PatternLimit: return "PROJECT_SEARCH_PATTERN_LIMIT";
    case ProjectSearchErrorCode::PatternTooLong: return "PROJECT_SEARCH_PATTERN_TOO_LONG";
    case ProjectSearchErrorCode::ProjectStale: return "PROJECT_SEARCH_PROJECT_STALE";
    case ProjectSearchErrorCode::ProjectMismatch: return "PROJECT_SEARCH_PROJECT_MISMATCH";
    case ProjectSearchErrorCode::PathInvalid: return "PROJECT_SEARCH_PATH_INVALID";
    case ProjectSearchErrorCode::PathOutsideProject: return "PROJECT_SEARCH_PATH_OUTSIDE_PROJECT";
    case ProjectSearchErrorCode::EnumerationFailed: return "PROJECT_SEARCH_ENUMERATION_FAILED";
    case ProjectSearchErrorCode::DirectoryLimit: return "PROJECT_SEARCH_DIRECTORY_LIMIT";
    case ProjectSearchErrorCode::FileLimit: return "PROJECT_SEARCH_FILE_LIMIT";
    case ProjectSearchErrorCode::FileTooLarge: return "PROJECT_SEARCH_FILE_TOO_LARGE";
    case ProjectSearchErrorCode::BinaryFile: return "PROJECT_SEARCH_BINARY_FILE";
    case ProjectSearchErrorCode::UnsupportedFile: return "PROJECT_SEARCH_UNSUPPORTED_FILE";
    case ProjectSearchErrorCode::FileReadFailed: return "PROJECT_SEARCH_FILE_READ_FAILED";
    case ProjectSearchErrorCode::BytesLimit: return "PROJECT_SEARCH_BYTES_LIMIT";
    case ProjectSearchErrorCode::MatchLimit: return "PROJECT_SEARCH_MATCH_LIMIT";
    case ProjectSearchErrorCode::ResultLimit: return "PROJECT_SEARCH_RESULT_LIMIT";
    case ProjectSearchErrorCode::Cancelled: return "PROJECT_SEARCH_CANCELLED";
    case ProjectSearchErrorCode::Timeout: return "PROJECT_SEARCH_TIMEOUT";
    case ProjectSearchErrorCode::OperationStale: return "PROJECT_SEARCH_OPERATION_STALE";
    case ProjectSearchErrorCode::Released: return "PROJECT_SEARCH_RELEASED";
    case ProjectSearchErrorCode::Internal: return "PROJECT_SEARCH_INTERNAL";
    default: return "PROJECT_SEARCH_UNKNOWN";
    }
}

void ProjectSearchOptionsInit(ProjectSearchOptions* options) {
    if (!options) return;
    options->caseSensitive = false;
    options->wholeWord = false;
    options->query[0] = '\0';
    options->includePattern[0] = '\0';
    options->excludePattern[0] = '\0';
}

void ProjectSearchServiceInit(ProjectSearchService* service) {
    if (!service) return;
    clearBytes(&service->operation, sizeof(service->operation));
    service->operation.state = ProjectSearchState::Idle;
    ProjectSearchOptionsInit(&service->operation.options);
    service->fileSystem = WorkspaceFileSystem();
    service->rootPath[0] = '\0';
    service->projectIdText[0] = '\0';
    service->pendingHead = service->pendingTail = service->pendingCount = 0;
    service->directoriesVisited = 0;
    service->currentEntryCount = service->currentEntryIndex = 0;
    service->currentDirectory[0] = '\0';
    service->currentDirectoryDepth = 0;
    service->currentDirectoryActive = false;
    ProjectSearchOptionsInit(&service->parsedOptions);
    service->includePatternCount = service->excludePatternCount = 0;
    service->dirtyDocumentCount = 0;
    service->scanVisitor = nullptr;
    service->scanVisitorUserData = nullptr;
    service->startedAtMs = 0;
    service->terminalReported = false;
    clearResultStorage(service);
}

bool ProjectSearchStart(ProjectSearchService* service, const ProjectSearchRequest* request,
                        uint64_t nowMs, uint64_t* outOperationId,
                        ProjectSearchErrorCode* outError) {
    if (outOperationId) *outOperationId = 0;
    if (outError) *outError = ProjectSearchErrorCode::None;
    if (!service || !request) { if (outError) *outError = ProjectSearchErrorCode::Internal; return false; }
    const uint64_t nextId = service->operation.operationId == 0 ? 1 : service->operation.operationId + 1;
    ProjectSearchServiceInit(service);
    service->operation.operationId = nextId == 0 ? 1 : nextId;
    service->operation.projectId = hashProjectId(request->projectId);
    service->operation.projectGeneration = request->projectGeneration;
    ProjectSearchErrorCode error = ProjectSearchErrorCode::None;
    if (!validateRequest(service, request, &error)) {
        service->operation.state = ProjectSearchState::Failed;
        setError(service, error);
        if (outError) *outError = error;
        if (outOperationId) *outOperationId = service->operation.operationId;
        return false;
    }
    service->fileSystem = request->fileSystem;
    copyText(service->rootPath, sizeof(service->rootPath), request->rootPath);
    copyText(service->projectIdText, sizeof(service->projectIdText), request->projectId);
    service->operation.options = request->options;
    service->parsedOptions = request->options;
    service->dirtyDocumentCount = request->dirtyDocumentCount > kMaxOpenDocuments ? kMaxOpenDocuments : request->dirtyDocumentCount;
    service->scanVisitor = request->scanVisitor;
    service->scanVisitorUserData = request->scanVisitorUserData;
    for (uint32_t i = 0; i < service->dirtyDocumentCount; ++i) {
        if (!request->dirtyDocuments) {
            service->dirtyDocumentCount = 0;
            break;
        }
        const ProjectSearchDocumentSnapshot& source = request->dirtyDocuments[i];
        ProjectSearchDocumentSnapshot& destination = service->dirtyDocuments[i];
        if (!copyText(destination.relativePath, sizeof(destination.relativePath), source.relativePath) ||
            source.length > kProjectSearchMaxFileBytes ||
            !relativePathFor(service->rootPath, JoinWorkspacePath(service->rootPath, source.relativePath,
                                                                  service->scanBuffer, sizeof(service->scanBuffer)) ? service->scanBuffer : "",
                             destination.relativePath, sizeof(destination.relativePath))) {
            service->dirtyDocumentCount = i;
            break;
        }
        destination.length = source.length;
        destination.documentId = source.documentId;
        destination.documentGeneration = source.documentGeneration;
        for (uint32_t j = 0; j < source.length; ++j) destination.data[j] = source.data[j];
        destination.data[source.length] = '\0';
    }
    service->startedAtMs = nowMs;
    service->operation.state = ProjectSearchState::Enumerating;
    service->operation.error = ProjectSearchErrorCode::None;
    setNotice(service, "");
    enqueueDirectory(service, service->rootPath);
    if (outOperationId) *outOperationId = service->operation.operationId;
    return true;
}

bool ProjectSearchPoll(ProjectSearchService* service, uint64_t operationId,
                       uint32_t workBudget, uint64_t nowMs) {
    if (!service || operationId == 0 || service->operation.operationId != operationId ||
        service->operation.state == ProjectSearchState::Idle) return false;
    if (isTerminal(service->operation.state)) return true;
    if (service->operation.cancellationRequested) {
        enterCancelled(service);
        return true;
    }
    if (nowMs >= service->startedAtMs && nowMs - service->startedAtMs > kProjectSearchMaxDurationMs) {
        setError(service, ProjectSearchErrorCode::Timeout);
        service->operation.state = ProjectSearchState::Failed;
        service->operation.truncated = true;
        setNotice(service, "Search timed out.");
        return true;
    }
    if (workBudget == 0) workBudget = kProjectSearchMaxEntriesPerPoll;
    if (workBudget > kProjectSearchMaxEntriesPerPoll) workBudget = kProjectSearchMaxEntriesPerPoll;
    uint32_t work = 0;
    while (work < workBudget && !service->operation.truncated) {
        if (service->operation.cancellationRequested) { enterCancelled(service); return true; }
        if (!service->currentDirectoryActive) {
            if (service->pendingCount == 0) break;
            if (!dequeueDirectory(service)) break;
            if (service->operation.state == ProjectSearchState::Failed) return true;
        }
        if (service->currentEntryIndex >= service->currentEntryCount) {
            service->currentDirectoryActive = false;
            continue;
        }
        const FileListEntry entry = service->currentEntries[service->currentEntryIndex++];
        if (entry.name[0] == '\0' || PathContainsTraversal(entry.name)) { ++work; continue; }
        char absolutePath[kMaxPathBytes] = {};
        char relativePath[kMaxPathBytes] = {};
        if (!copyRelativeFromEntry(service->currentDirectory, entry.name, absolutePath, sizeof(absolutePath)) ||
            !isWithinRoot(service->rootPath, absolutePath) ||
            !relativePathFor(service->rootPath, absolutePath, relativePath, sizeof(relativePath))) {
            setError(service, ProjectSearchErrorCode::PathOutsideProject);
            ++work;
            continue;
        }
        if (entry.kind == FileInfoKind::Directory) {
            if (!isDefaultExcludedDirectory(entry.name) && service->currentDirectoryDepth < kProjectSearchMaxDirectoryDepth)
                enqueueDirectory(service, absolutePath);
            ++work;
            continue;
        }
        if (entry.kind != FileInfoKind::RegularFile || !relativePathMatches(service, relativePath)) { ++work; continue; }
        if (service->operation.filesEnumerated >= kProjectSearchMaxFiles) {
            markTruncated(service, ProjectSearchErrorCode::FileLimit);
            break;
        }
        ++service->operation.filesEnumerated;
        service->operation.state = ProjectSearchState::Searching;
        ++service->operation.filesSearched;
        scanCurrentFile(service, absolutePath, relativePath, entry);
        ++work;
        // A synchronous hosted read is intentionally one file per poll.
        break;
    }
    finishIfReady(service);
    return true;
}

bool ProjectSearchCancel(ProjectSearchService* service, uint64_t operationId) {
    if (!service || operationId == 0 || service->operation.operationId != operationId ||
        isTerminal(service->operation.state)) return false;
    service->operation.cancellationRequested = true;
    service->operation.state = ProjectSearchState::Cancelling;
    return true;
}

bool ProjectSearchRelease(ProjectSearchService* service, uint64_t operationId) {
    if (!service || operationId == 0 || service->operation.operationId != operationId ||
        !isTerminal(service->operation.state)) return false;
    service->operation.error = ProjectSearchErrorCode::Released;
    service->operation.state = ProjectSearchState::Idle;
    clearResultStorage(service);
    service->pendingCount = 0;
    service->currentDirectoryActive = false;
    return true;
}

void ProjectSearchClearResults(ProjectSearchService* service) {
    if (!service) return;
    clearResultStorage(service);
}

bool ProjectSearchIsActive(const ProjectSearchService* service) {
    return service && !isTerminal(service->operation.state);
}

const ProjectSearchOperation* ProjectSearchOperationInfo(const ProjectSearchService* service) {
    return service ? &service->operation : nullptr;
}

uint32_t ProjectSearchQueryResultGroups(const ProjectSearchService* service) {
    return service ? service->operation.resultFileCount : 0;
}

const ProjectSearchFileGroup* ProjectSearchResultGroupAt(const ProjectSearchService* service, uint32_t index) {
    if (!service || index >= service->operation.resultFileCount) return nullptr;
    return &service->groups[index];
}

const ProjectSearchMatch* ProjectSearchResultMatchAt(const ProjectSearchService* service,
                                                     const ProjectSearchFileGroup* group,
                                                     uint32_t matchIndex) {
    if (!service || !group || matchIndex >= group->matchCount ||
        group->firstMatchIndex + matchIndex >= service->operation.resultMatchCount) return nullptr;
    return &service->matches[group->firstMatchIndex + matchIndex];
}

const char* ProjectSearchQuery(const ProjectSearchService* service) {
    return service ? service->operation.options.query : "";
}

bool ProjectSearchBuildPreview(const char* data, uint32_t length, uint64_t offset,
                               uint32_t matchLength, ProjectSearchPreview* output) {
    if (!output) return false;
    output->text[0] = '\0';
    output->matchStart = 0;
    output->matchLength = 0;
    output->leftTruncated = false;
    output->rightTruncated = false;
    if (!data || offset > length || matchLength > length - offset) return false;
    ProjectSearchMatch match = {};
    makePreview(data, length, offset, matchLength, &match);
    copyText(output->text, sizeof(output->text), match.preview);
    output->matchStart = match.previewMatchStart;
    output->matchLength = match.previewMatchLength;
    output->leftTruncated = match.previewLeftTruncated;
    output->rightTruncated = match.previewRightTruncated;
    return true;
}

} // namespace developer_studio
} // namespace guidexos
