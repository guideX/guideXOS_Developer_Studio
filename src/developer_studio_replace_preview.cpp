#include "developer_studio_replace_preview.h"

#include "developer_studio_find.h"

namespace guidexos {
namespace developer_studio {
namespace {

static uint32_t textLength(const char* text, uint32_t capacity) {
    if (!text) return 0;
    uint32_t length = 0;
    while (length < capacity && text[length] != '\0') ++length;
    return length;
}

static bool copyText(char* destination, uint32_t capacity, const char* source) {
    if (!destination || capacity == 0 || !source) return false;
    const uint32_t length = textLength(source, capacity);
    if (length >= capacity) return false;
    for (uint32_t i = 0; i <= length; ++i) destination[i] = source[i];
    return true;
}

static char lowerAscii(char value) {
    return value >= 'A' && value <= 'Z' ? static_cast<char>(value + ('a' - 'A')) : value;
}

static int comparePath(const char* left, const char* right) {
    uint32_t i = 0;
    while (left[i] != '\0' && right[i] != '\0') {
        const char a = lowerAscii(left[i]);
        const char b = lowerAscii(right[i]);
        if (a < b) return -1;
        if (a > b) return 1;
        ++i;
    }
    if (left[i] != right[i]) return left[i] == '\0' ? -1 : 1;
    i = 0;
    while (left[i] != '\0' && right[i] != '\0') {
        if (left[i] < right[i]) return -1;
        if (left[i] > right[i]) return 1;
        ++i;
    }
    return 0;
}

static int32_t fileIndexFor(const ReplacePreviewPlan* plan, const char* path) {
    if (!plan || !path) return -1;
    for (uint32_t i = 0; i < plan->filesAffected; ++i) {
        const char* left = plan->files[i].relativePath;
        uint32_t j = 0;
        while (left[j] != '\0' && path[j] != '\0' && left[j] == path[j]) ++j;
        if (left[j] == path[j]) return static_cast<int32_t>(i);
    }
    return -1;
}

static uint32_t ensureFile(ReplacePreviewPlan* plan, const ProjectSearchScanFile* scanFile) {
    const int32_t found = fileIndexFor(plan, scanFile->relativePath);
    if (found >= 0) return static_cast<uint32_t>(found);
    if (plan->filesAffected >= kReplacePreviewMaxAffectedFiles) {
        plan->capacityExceeded = true;
        return kReplacePreviewMaxAffectedFiles;
    }
    uint32_t insertAt = 0;
    while (insertAt < plan->filesAffected &&
           comparePath(plan->files[insertAt].relativePath, scanFile->relativePath) < 0) ++insertAt;
    for (uint32_t i = plan->filesAffected; i > insertAt; --i)
        plan->files[i] = plan->files[i - 1];
    for (uint32_t i = 0; i < plan->matchCount; ++i)
        if (plan->matches[i].fileIndex >= insertAt) ++plan->matches[i].fileIndex;
    ReplacePreviewFile& file = plan->files[insertAt];
    file = ReplacePreviewFile();
    if (!copyText(file.relativePath, sizeof(file.relativePath), scanFile->relativePath)) {
        plan->capacityExceeded = true;
        return kReplacePreviewMaxAffectedFiles;
    }
    file.sourceSize = scanFile->length;
    file.sourceFingerprint = ReplacePreviewFingerprint(scanFile->data, scanFile->length);
    file.documentId = scanFile->documentId;
    file.documentGeneration = scanFile->documentGeneration;
    file.sourceKind = scanFile->sourceKind;
    ++plan->filesAffected;
    return insertAt;
}

static uint32_t lineStartFor(const char* data, uint32_t offset) {
    while (offset > 0 && data[offset - 1] != '\n') --offset;
    return offset;
}

static uint32_t contextEndFor(const char* data, uint32_t length, uint32_t offset,
                              uint32_t matchLength) {
    uint32_t end = offset + matchLength;
    if (end > length) end = length;
    while (end < length && data[end] != '\n') ++end;
    if (end > 0 && data[end - 1] == '\r') --end;
    return end;
}

static uint32_t displayByteLength(unsigned char value) {
    return value == '\t' || value == '\r' || value == '\n' || value < 0x20u ? 2u : 1u;
}

static bool appendVisibleByte(char* output, uint32_t capacity, uint32_t* length,
                              unsigned char value) {
    if (!output || !length || capacity == 0) return false;
    if (value == '\t' || value == '\r' || value == '\n' || value < 0x20u) {
        if (*length + 2u >= capacity) return false;
        output[(*length)++] = '\\';
        output[(*length)++] = value == '\t' ? 't' :
            (value == '\r' ? 'r' : (value == '\n' ? 'n' : '?'));
        output[*length] = '\0';
        return true;
    }
    if (*length + 1u >= capacity) return false;
    output[(*length)++] = static_cast<char>(value);
    output[*length] = '\0';
    return true;
}

static void renderBefore(const char* data, uint32_t rangeStart, uint32_t rangeEnd,
                         uint32_t matchStart, uint32_t matchLength,
                         ReplacePreviewMatch* match) {
    const uint32_t rangeLength = rangeEnd - rangeStart;
    const uint32_t localMatchStart = matchStart - rangeStart;
    uint32_t windowStart = 0;
    uint32_t windowLength = rangeLength;
    if (windowLength > kReplacePreviewMaxPreviewBytes) {
        windowStart = localMatchStart > 32u ? localMatchStart - 32u : 0;
        if (windowStart + kReplacePreviewMaxPreviewBytes > rangeLength)
            windowStart = rangeLength - kReplacePreviewMaxPreviewBytes;
        windowLength = kReplacePreviewMaxPreviewBytes;
    }
    match->beforeLeftTruncated = windowStart != 0;
    match->beforeRightTruncated = windowStart + windowLength < rangeLength;
    uint32_t outputLength = 0;
    for (uint32_t i = 0; i < windowLength; ++i) {
        if (!appendVisibleByte(match->before, sizeof(match->before), &outputLength,
                               static_cast<unsigned char>(data[rangeStart + windowStart + i]))) {
            match->beforeRightTruncated = true;
            break;
        }
    }
    match->before[outputLength] = '\0';
}

static void renderAfter(const char* data, uint32_t rangeStart, uint32_t rangeEnd,
                        uint32_t matchStart, uint32_t matchLength,
                        const char* replacement, uint32_t replacementLength,
                        ReplacePreviewMatch* match) {
    const uint32_t sourceLength = rangeEnd - rangeStart;
    const uint32_t localMatchStart = matchStart - rangeStart;
    const uint32_t projectedLength = sourceLength - matchLength + replacementLength;
    uint32_t windowStart = localMatchStart > 32u ? localMatchStart - 32u : 0;
    if (replacementLength >= kReplacePreviewMaxPreviewBytes) windowStart = localMatchStart;
    else if (projectedLength > kReplacePreviewMaxPreviewBytes &&
             windowStart + kReplacePreviewMaxPreviewBytes < localMatchStart + replacementLength)
        windowStart = localMatchStart + replacementLength - kReplacePreviewMaxPreviewBytes;
    if (projectedLength > kReplacePreviewMaxPreviewBytes &&
        windowStart + kReplacePreviewMaxPreviewBytes > projectedLength &&
        replacementLength < kReplacePreviewMaxPreviewBytes)
        windowStart = projectedLength - kReplacePreviewMaxPreviewBytes;
    uint32_t windowLength = projectedLength - windowStart;
    if (windowLength > kReplacePreviewMaxPreviewBytes) windowLength = kReplacePreviewMaxPreviewBytes;
    match->afterLeftTruncated = windowStart != 0;
    match->afterRightTruncated = windowStart + windowLength < projectedLength;

    uint32_t outputLength = 0;
    uint32_t projectedOffset = 0;
    for (uint32_t i = 0; i < localMatchStart && projectedOffset < windowStart; ++i, ++projectedOffset) {}
    for (; projectedOffset < localMatchStart && outputLength < kReplacePreviewMaxPreviewBytes; ++projectedOffset) {
        if (projectedOffset >= windowStart &&
            !appendVisibleByte(match->after, sizeof(match->after), &outputLength,
                              static_cast<unsigned char>(data[rangeStart + projectedOffset]))) {
            match->afterRightTruncated = true;
            break;
        }
    }
    if (projectedOffset == localMatchStart) {
        for (uint32_t i = 0; i < replacementLength && outputLength < kReplacePreviewMaxPreviewBytes; ++i, ++projectedOffset) {
            if (projectedOffset >= windowStart &&
                !appendVisibleByte(match->after, sizeof(match->after), &outputLength,
                                   static_cast<unsigned char>(replacement[i]))) {
                match->afterRightTruncated = true;
                break;
            }
        }
    }
    for (uint32_t i = localMatchStart + matchLength; i < sourceLength && outputLength < kReplacePreviewMaxPreviewBytes; ++i, ++projectedOffset) {
        if (projectedOffset < windowStart) continue;
        if (!appendVisibleByte(match->after, sizeof(match->after), &outputLength,
                               static_cast<unsigned char>(data[rangeStart + i]))) {
            match->afterRightTruncated = true;
            break;
        }
    }
    if (outputLength < kReplacePreviewMaxPreviewBytes &&
        projectedOffset < windowStart + windowLength) match->afterRightTruncated = true;
    match->after[outputLength] = '\0';
}

static void advancePosition(const char* data, uint32_t length,
                            uint32_t* line, uint32_t* column) {
    for (uint32_t i = 0; i < length; ++i) {
        if (data[i] == '\n') { ++(*line); *column = 1; }
        else ++(*column);
    }
}

static void sortMatches(ReplacePreviewPlan* plan) {
    if (!plan) return;
    for (uint32_t i = 0; i < plan->matchCount; ++i) {
        for (uint32_t j = i + 1; j < plan->matchCount; ++j) {
            const ReplacePreviewMatch& left = plan->matches[j];
            const ReplacePreviewMatch& right = plan->matches[i];
            if (left.fileIndex < right.fileIndex ||
                (left.fileIndex == right.fileIndex && left.byteOffset < right.byteOffset)) {
                const ReplacePreviewMatch temp = plan->matches[i];
                plan->matches[i] = plan->matches[j];
                plan->matches[j] = temp;
            }
        }
    }
}

} // namespace

void ReplacePreviewPlanInit(ReplacePreviewPlan* plan) {
    if (plan) *plan = ReplacePreviewPlan();
}

ReplacePreviewError ReplacePreviewPlanBegin(ReplacePreviewPlan* plan,
                                            const char* projectId,
                                            uint64_t projectGeneration,
                                            const char* findText,
                                            const char* replacementText,
                                            bool caseSensitive) {
    if (!plan) return ReplacePreviewError::NoProject;
    ReplacePreviewPlanInit(plan);
    if (!projectId || projectId[0] == '\0' || projectGeneration == 0)
        return ReplacePreviewError::NoProject;
    const uint32_t findLength = textLength(findText, kReplacePreviewMaxFindBytes + 1u);
    if (findLength == 0) return ReplacePreviewError::EmptyFind;
    if (findLength > kReplacePreviewMaxFindBytes) return ReplacePreviewError::FindTooLong;
    const uint32_t replacementLength = textLength(replacementText,
                                                   kReplacePreviewMaxReplacementBytes + 1u);
    if (replacementLength > kReplacePreviewMaxReplacementBytes)
        return ReplacePreviewError::ReplacementTooLong;
    if (!copyText(plan->projectId, sizeof(plan->projectId), projectId) ||
        !copyText(plan->findText, sizeof(plan->findText), findText) ||
        !copyText(plan->replacementText, sizeof(plan->replacementText), replacementText))
        return ReplacePreviewError::NoProject;
    plan->projectGeneration = projectGeneration;
    plan->caseSensitive = caseSensitive;
    plan->configured = true;
    return ReplacePreviewError::None;
}

void ReplacePreviewPlanClear(ReplacePreviewPlan* plan) {
    if (plan) ReplacePreviewPlanInit(plan);
}

void ReplacePreviewPlanFinish(ReplacePreviewPlan* plan, bool scanComplete) {
    if (!plan) return;
    sortMatches(plan);
    plan->complete = scanComplete && !plan->capacityExceeded && !plan->stale;
}

void ReplacePreviewPlanMarkStale(ReplacePreviewPlan* plan) {
    if (!plan) return;
    plan->stale = true;
    plan->complete = false;
}

bool ReplacePreviewPlanVisitFile(void* userData, const ProjectSearchScanFile* file) {
    ReplacePreviewPlan* plan = static_cast<ReplacePreviewPlan*>(userData);
    if (!plan || !plan->configured || !file || !file->relativePath ||
        (!file->data && file->length != 0) || plan->stale) return false;
    const uint32_t findLength = textLength(plan->findText, sizeof(plan->findText));
    const uint32_t replacementLength = textLength(plan->replacementText,
                                                  sizeof(plan->replacementText));
    if (findLength == 0 || findLength > file->length) return true;

    uint32_t line = 1;
    uint32_t column = 1;
    uint32_t offset = 0;
    while (offset + findLength <= file->length) {
        if (!FindLiteralMatchesAt(file->data, file->length, offset, plan->findText,
                                  findLength, plan->caseSensitive, false)) {
            advancePosition(file->data + offset, 1, &line, &column);
            ++offset;
            continue;
        }
        ++plan->matchesObserved;
        if (plan->matchCount >= kReplacePreviewMaxMatches) {
            plan->capacityExceeded = true;
            return false;
        }
        const uint32_t fileIndex = ensureFile(plan, file);
        if (fileIndex >= kReplacePreviewMaxAffectedFiles) return false;

        ReplacePreviewMatch& match = plan->matches[plan->matchCount];
        match = ReplacePreviewMatch();
        match.fileIndex = fileIndex;
        match.byteOffset = offset;
        match.line = line;
        match.column = column;
        match.matchLength = findLength;
        uint32_t rangeStart = lineStartFor(file->data, offset);
        uint32_t rangeEnd = contextEndFor(file->data, file->length, offset, findLength);
        const uint32_t matchEnd = offset + findLength;
        while (rangeEnd < matchEnd) {
            rangeEnd = contextEndFor(file->data, file->length, rangeEnd, matchEnd - rangeEnd);
            if (rangeEnd == file->length) break;
        }
        // Include interior line endings in the context when a literal query
        // itself spans lines, while omitting the trailing line ending.
        if (matchEnd > rangeEnd) rangeEnd = matchEnd;
        renderBefore(file->data, rangeStart, rangeEnd, offset, findLength, &match);
        renderAfter(file->data, rangeStart, rangeEnd, offset, findLength,
                    plan->replacementText, replacementLength, &match);
        ++plan->matchCount;
        ++plan->files[fileIndex].matchCount;
        advancePosition(file->data + offset, findLength, &line, &column);
        offset += findLength; // leftmost, non-overlapping matches; adjacent hits are retained.
    }
    return true;
}

const ReplacePreviewFile* ReplacePreviewPlanFileAt(const ReplacePreviewPlan* plan,
                                                   uint32_t index) {
    return plan && index < plan->filesAffected ? &plan->files[index] : nullptr;
}

const ReplacePreviewMatch* ReplacePreviewPlanMatchAt(const ReplacePreviewPlan* plan,
                                                     uint32_t index) {
    return plan && index < plan->matchCount ? &plan->matches[index] : nullptr;
}

uint64_t ReplacePreviewFingerprint(const char* data, uint32_t length) {
    uint64_t hash = 14695981039346656037ull;
    for (uint32_t i = 0; data && i < length; ++i) {
        hash ^= static_cast<unsigned char>(data[i]);
        hash *= 1099511628211ull;
    }
    return hash;
}

bool ReplacePreviewFileFingerprintMatches(const ReplacePreviewFile* file,
                                          const char* data, uint32_t length) {
    return file && (length == 0 || data) && file->sourceSize == length &&
        file->sourceFingerprint == ReplacePreviewFingerprint(data, length);
}

} // namespace developer_studio
} // namespace guidexos
