#include "developer_studio_project_symbols.h"

namespace guidexos {
namespace developer_studio {
namespace {

static uint32_t boundedLength(const char* value, uint32_t capacity) {
    if (!value) return capacity;
    uint32_t length = 0;
    while (length < capacity && value[length] != '\0') ++length;
    return length;
}

static char lowerAscii(char value) {
    return value >= 'A' && value <= 'Z' ? static_cast<char>(value + ('a' - 'A')) : value;
}

static bool equalText(const char* left, const char* right) {
    if (!left || !right) return left == right;
    uint32_t i = 0;
    while (left[i] && right[i] && left[i] == right[i]) ++i;
    return left[i] == right[i];
}

static bool equalTextInsensitive(const char* left, const char* right) {
    if (!left || !right) return left == right;
    uint32_t i = 0;
    while (left[i] && right[i] && lowerAscii(left[i]) == lowerAscii(right[i])) ++i;
    return left[i] == right[i];
}

static bool sameKey(const DocumentOutlineSymbol& left, const DocumentOutlineSymbol& right) {
    return left.category == right.category && equalText(left.name, right.name) &&
        equalText(left.signature, right.signature);
}

static bool containsInsensitive(const char* text, const char* query) {
    if (!text || !query) return false;
    const uint32_t queryLength = boundedLength(query, kProjectSymbolsMaxFileFilterBytes + 1u);
    if (queryLength == 0) return true;
    const uint32_t textLength = boundedLength(text, kMaxPathBytes + 1u);
    if (queryLength > textLength) return false;
    for (uint32_t start = 0; start + queryLength <= textLength; ++start) {
        uint32_t i = 0;
        while (i < queryLength && lowerAscii(text[start + i]) == lowerAscii(query[i])) ++i;
        if (i == queryLength) return true;
    }
    return false;
}

static bool candidateMatches(const ProjectSymbolCandidate& candidate,
                             const char* nameFilter, const char* fileFilter) {
    return containsInsensitive(candidate.name, nameFilter) &&
        containsInsensitive(candidate.relativePath, fileFilter);
}

static int compareText(const char* left, const char* right) {
    uint32_t i = 0;
    while (left[i] && right[i] && lowerAscii(left[i]) == lowerAscii(right[i])) ++i;
    const char a = lowerAscii(left[i]);
    const char b = lowerAscii(right[i]);
    if (a < b) return -1;
    if (a > b) return 1;
    if (left[i] < right[i]) return -1;
    if (left[i] > right[i]) return 1;
    return 0;
}

static bool candidateBefore(const ProjectSymbolCandidate& left,
                            const ProjectSymbolCandidate& right) {
    int order = compareText(left.relativePath, right.relativePath);
    if (order != 0) return order < 0;
    if (left.line != right.line) return left.line < right.line;
    if (left.offset != right.offset) return left.offset < right.offset;
    order = compareText(left.name, right.name);
    if (order != 0) return order < 0;
    if (left.category != right.category)
        return static_cast<uint8_t>(left.category) < static_cast<uint8_t>(right.category);
    return left.duplicateOrdinal < right.duplicateOrdinal;
}

static bool copyText(char* output, uint32_t outputSize, const char* input) {
    if (!output || outputSize == 0 || !input) return false;
    uint32_t i = 0;
    while (i + 1u < outputSize && input[i]) {
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

static bool candidateKeyMatches(const ProjectSymbolCandidate& candidate,
                                const DocumentOutlineSymbol& symbol) {
    return candidate.category == symbol.category &&
        equalText(candidate.name, symbol.name) &&
        equalText(candidate.signature, symbol.signature);
}

static bool isSeparator(char value) {
    return value == '/' || value == static_cast<char>(92);
}

} // namespace

void ProjectSymbolResultsInit(ProjectSymbolResults* results) {
    if (!results) return;
    results->count = 0;
    results->totalRecognized = 0;
    results->filesProcessed = 0;
    results->filesWithSymbols = 0;
    results->filesTruncated = 0;
    results->candidatesDropped = 0;
    results->bytesProcessed = 0;
    results->truncated = false;
    results->incomplete = false;
    DocumentOutlineInit(&results->outlineScratch);
}

bool ProjectSymbolsVisitFile(void* userData, const ProjectSearchScanFile* file) {
    ProjectSymbolResults* results = static_cast<ProjectSymbolResults*>(userData);
    if (!results || !file || !file->relativePath || !file->data ||
        file->length > kProjectSearchMaxFileBytes) return false;
    if (results->filesProcessed >= kProjectSymbolsMaxFiles ||
        file->length > kProjectSymbolsMaxBytes - results->bytesProcessed) {
        results->truncated = true;
        results->incomplete = true;
        return false;
    }
    ++results->filesProcessed;
    results->bytesProcessed += file->length;

    if (!DocumentOutlineScan(file->relativePath, file->data, file->length,
                             &results->outlineScratch)) {
        results->incomplete = true;
        return true;
    }
    const DocumentOutline& outline = results->outlineScratch;
    if (outline.totalCount != 0) ++results->filesWithSymbols;
    if (outline.totalCount > UINT64_MAX - results->totalRecognized)
        results->totalRecognized = UINT64_MAX;
    else
        results->totalRecognized += outline.totalCount;
    if (outline.truncated) {
        results->truncated = true;
        results->incomplete = true;
        if (results->filesTruncated != UINT32_MAX) ++results->filesTruncated;
    }

    for (uint32_t i = 0; i < outline.count; ++i) {
        if (results->count >= kProjectSymbolsMaxCandidates) {
            results->truncated = true;
            results->incomplete = true;
            if (results->candidatesDropped != UINT32_MAX) ++results->candidatesDropped;
            return false;
        }
        const DocumentOutlineSymbol& symbol = outline.symbols[i];
        ProjectSymbolCandidate& candidate = results->candidates[results->count];
        if (!copyText(candidate.name, sizeof(candidate.name), symbol.name) ||
            !copyText(candidate.signature, sizeof(candidate.signature), symbol.signature) ||
            !copyText(candidate.relativePath, sizeof(candidate.relativePath), file->relativePath)) {
            results->truncated = true;
            results->incomplete = true;
            if (results->candidatesDropped != UINT32_MAX) ++results->candidatesDropped;
            continue;
        }
        candidate.category = symbol.category;
        candidate.line = symbol.line;
        candidate.offset = symbol.offset;
        candidate.duplicateOrdinal = 0;
        for (uint32_t prior = 0; prior < i; ++prior)
            if (sameKey(outline.symbols[prior], symbol)) ++candidate.duplicateOrdinal;
        ++results->count;
    }
    return true;
}

void ProjectSymbolResultsSort(ProjectSymbolResults* results) {
    if (!results) return;
    for (uint32_t i = 0; i < results->count; ++i) {
        for (uint32_t j = i + 1u; j < results->count; ++j) {
            if (candidateBefore(results->candidates[j], results->candidates[i])) {
                const ProjectSymbolCandidate temporary = results->candidates[i];
                results->candidates[i] = results->candidates[j];
                results->candidates[j] = temporary;
            }
        }
    }
}

bool ProjectSymbolFilterValid(const char* nameFilter, const char* fileFilter) {
    return nameFilter && fileFilter &&
        boundedLength(nameFilter, kProjectSymbolsMaxNameFilterBytes + 1u) <=
            kProjectSymbolsMaxNameFilterBytes &&
        boundedLength(fileFilter, kProjectSymbolsMaxFileFilterBytes + 1u) <=
            kProjectSymbolsMaxFileFilterBytes;
}

bool ProjectSymbolsProjectIdentityMatches(const char* expectedProjectId,
                                         uint64_t expectedGeneration,
                                         const char* expectedRoot,
                                         const char* actualProjectId,
                                         uint64_t actualGeneration,
                                         const char* actualRoot) {
    return expectedProjectId && expectedProjectId[0] && expectedGeneration != 0 &&
        expectedRoot && expectedRoot[0] && actualProjectId && actualProjectId[0] &&
        actualGeneration == expectedGeneration && actualRoot && actualRoot[0] &&
        equalTextInsensitive(expectedProjectId, actualProjectId) &&
        PathsEqual(expectedRoot, actualRoot);
}

bool ProjectSymbolsResolvePath(const char* projectRoot, const char* relativePath,
                               char* absolutePath, uint32_t outputSize) {
    if (!projectRoot || !relativePath || !absolutePath || outputSize == 0 ||
        projectRoot[0] == '\0' || relativePath[0] == '\0' ||
        isSeparator(relativePath[0]) || relativePath[1] == ':' ||
        PathContainsTraversal(relativePath)) return false;
    // JoinWorkspacePath normalizes the result and checks a separator-bounded
    // root prefix, so this one shared operation is the containment proof.
    return JoinWorkspacePath(projectRoot, relativePath, absolutePath, outputSize);
}

uint32_t ProjectSymbolFilteredCount(const ProjectSymbolResults* results,
                                    const char* nameFilter, const char* fileFilter) {
    if (!results || !ProjectSymbolFilterValid(nameFilter, fileFilter)) return 0;
    uint32_t count = 0;
    for (uint32_t i = 0; i < results->count; ++i)
        if (candidateMatches(results->candidates[i], nameFilter, fileFilter)) ++count;
    return count;
}

const ProjectSymbolCandidate* ProjectSymbolFilteredAt(const ProjectSymbolResults* results,
                                                       const char* nameFilter,
                                                       const char* fileFilter,
                                                       uint32_t index) {
    if (!results || index >= kProjectSymbolsMaxDisplayedResults ||
        !ProjectSymbolFilterValid(nameFilter, fileFilter)) return nullptr;
    uint32_t match = 0;
    for (uint32_t i = 0; i < results->count; ++i) {
        if (!candidateMatches(results->candidates[i], nameFilter, fileFilter)) continue;
        if (match++ == index) return &results->candidates[i];
    }
    return nullptr;
}

bool ProjectSymbolValidateCurrent(ProjectSymbolResults* results,
                                  const ProjectSymbolCandidate* candidate,
                                  const char* path, const char* data, uint32_t length,
                                  uint32_t* outLine, uint32_t* outOffset) {
    if (!results || !candidate || !path || !data || !outLine || !outOffset ||
        !equalText(candidate->relativePath, path)) return false;
    if (!DocumentOutlineScan(path, data, length, &results->outlineScratch)) return false;
    uint32_t ordinal = 0;
    const DocumentOutline& outline = results->outlineScratch;
    for (uint32_t i = 0; i < outline.count; ++i) {
        const DocumentOutlineSymbol& symbol = outline.symbols[i];
        if (!candidateKeyMatches(*candidate, symbol)) continue;
        if (ordinal++ == candidate->duplicateOrdinal) {
            *outLine = symbol.line;
            *outOffset = symbol.offset;
            return true;
        }
    }
    return false;
}

} // namespace developer_studio
} // namespace guidexos
