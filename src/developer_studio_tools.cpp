#include "developer_studio_tools.h"

namespace guidexos {
namespace developer_studio {
namespace {

static uint32_t textLength(const char* text, uint32_t capacity) {
    if (!text) return 0;
    uint32_t length = 0;
    while (length < capacity && text[length] != '\0') ++length;
    return length;
}

static char lowerAscii(char value) {
    return value >= 'A' && value <= 'Z' ? static_cast<char>(value + ('a' - 'A')) : value;
}

static bool isPathSeparator(char value) {
    return value == '/' || value == static_cast<char>(92);
}

static char normalizedPathChar(char value) {
    return isPathSeparator(value) ? '/' : lowerAscii(value);
}

static bool containsInsensitive(const char* text, const char* query) {
    if (!text || !query) return false;
    const uint32_t queryLength = textLength(query, kDeveloperToolMaxFileQueryBytes + 1u);
    if (queryLength == 0) return true;
    const uint32_t textSize = textLength(text, kMaxPathBytes);
    if (queryLength > textSize) return false;
    for (uint32_t start = 0; start + queryLength <= textSize; ++start) {
        uint32_t i = 0;
        while (i < queryLength && lowerAscii(text[start + i]) == lowerAscii(query[i])) ++i;
        if (i == queryLength) return true;
    }
    return false;
}

static bool safeRelativePath(const char* path) {
    if (!path || path[0] == '\0' || isPathSeparator(path[0]) ||
        (path[0] != '\0' && path[1] == ':')) return false;
    uint32_t componentLength = 0;
    uint32_t componentStart = 0;
    for (uint32_t i = 0; path[i] != '\0'; ++i) {
        const unsigned char value = static_cast<unsigned char>(path[i]);
        if (value < 0x20u || path[i] == ':') return false;
        if (isPathSeparator(path[i])) {
            componentLength = i - componentStart;
            if (componentLength == 0 ||
                (componentLength == 1 && path[componentStart] == '.') ||
                (componentLength == 2 && path[componentStart] == '.' && path[componentStart + 1] == '.')) return false;
            componentStart = i + 1;
        }
    }
    const uint32_t length = textLength(path, kMaxPathBytes);
    if (length == 0 || length >= kMaxPathBytes || componentStart >= length) return false;
    componentLength = length - componentStart;
    return !(componentLength == 1 && path[componentStart] == '.') &&
        !(componentLength == 2 && path[componentStart] == '.' && path[componentStart + 1] == '.');
}

static bool relativePathFor(const char* root, const char* absolutePath,
                            char* output, uint32_t outputSize) {
    if (!root || !absolutePath || !output || outputSize == 0) return false;
    uint32_t rootLength = textLength(root, kMaxPathBytes);
    const uint32_t pathLength = textLength(absolutePath, kMaxPathBytes);
    if (rootLength == 0 || rootLength >= kMaxPathBytes || pathLength == 0 ||
        pathLength >= kMaxPathBytes) return false;
    while (rootLength > 1 && isPathSeparator(root[rootLength - 1])) --rootLength;
    if (rootLength > pathLength) return false;
    for (uint32_t i = 0; i < rootLength; ++i)
        if (normalizedPathChar(root[i]) != normalizedPathChar(absolutePath[i])) return false;

    uint32_t relativeStart = rootLength;
    if (rootLength == 1 && isPathSeparator(root[0])) {
        relativeStart = 1;
    } else {
        if (relativeStart >= pathLength || !isPathSeparator(absolutePath[relativeStart])) return false;
        ++relativeStart;
    }
    if (relativeStart >= pathLength || pathLength - relativeStart >= outputSize) return false;
    for (uint32_t i = relativeStart; i < pathLength; ++i)
        output[i - relativeStart] = isPathSeparator(absolutePath[i]) ? '/' : absolutePath[i];
    output[pathLength - relativeStart] = '\0';
    return safeRelativePath(output);
}

static int comparePath(const char* left, const char* right) {
    if (!left || !right) return left == right ? 0 : (left ? 1 : -1);
    uint32_t i = 0;
    while (left[i] != '\0' && right[i] != '\0') {
        const char leftFolded = lowerAscii(left[i]);
        const char rightFolded = lowerAscii(right[i]);
        if (leftFolded < rightFolded) return -1;
        if (leftFolded > rightFolded) return 1;
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

static bool fileResultBefore(bool fileNameMatch, const char* relativePath,
                             const DeveloperToolFileSearchResult& existing) {
    if (fileNameMatch != existing.fileNameMatch) return fileNameMatch;
    return comparePath(relativePath, existing.relativePath) < 0;
}

static uint32_t findIndex(const DeveloperToolRegistry* registry, DeveloperToolId id) {
    if (!registry || id == DeveloperToolId::None) return kDeveloperToolMaxCommands;
    for (uint32_t i = 0; i < registry->count; ++i)
        if (registry->entries[i].command.id == id) return i;
    return kDeveloperToolMaxCommands;
}

} // namespace

bool DeveloperToolBuildFileSearchResults(const char* projectRoot,
                                         const char* const* indexedAbsolutePaths,
                                         uint32_t indexedPathCount,
                                         const char* query,
                                         DeveloperToolFileSearchResults* output) {
    if (!output) return false;
    *output = DeveloperToolFileSearchResults();
    if (!projectRoot || !indexedAbsolutePaths || !query ||
        indexedPathCount > kDeveloperToolMaxIndexedFiles ||
        textLength(query, kDeveloperToolMaxFileQueryBytes + 1u) > kDeveloperToolMaxFileQueryBytes) return false;

    for (uint32_t i = 0; i < indexedPathCount; ++i) {
        char relativePath[kMaxPathBytes] = {};
        if (!relativePathFor(projectRoot, indexedAbsolutePaths[i], relativePath, sizeof(relativePath))) continue;
        const char* fileName = relativePath;
        for (uint32_t p = 0; relativePath[p] != '\0'; ++p)
            if (relativePath[p] == '/') fileName = relativePath + p + 1;
        const bool fileNameMatch = query[0] != '\0' && containsInsensitive(fileName, query);
        if (query[0] != '\0' && !fileNameMatch && !containsInsensitive(relativePath, query)) continue;

        if (output->totalMatches != 0xFFFFFFFFu) ++output->totalMatches;
        if (output->totalMatches > kDeveloperToolMaxFileResults) output->truncated = true;
        uint32_t insertAt = 0;
        while (insertAt < output->count &&
               !fileResultBefore(fileNameMatch, relativePath, output->results[insertAt])) ++insertAt;
        if (output->count < kDeveloperToolMaxFileResults) {
            for (uint32_t move = output->count; move > insertAt; --move)
                output->results[move] = output->results[move - 1];
            ++output->count;
        } else {
            if (insertAt >= kDeveloperToolMaxFileResults) continue;
            for (uint32_t move = kDeveloperToolMaxFileResults - 1; move > insertAt; --move)
                output->results[move] = output->results[move - 1];
        }
        uint32_t pathIndex = 0;
        while (pathIndex + 1 < sizeof(output->results[insertAt].relativePath) &&
               relativePath[pathIndex] != '\0') {
            output->results[insertAt].relativePath[pathIndex] = relativePath[pathIndex];
            ++pathIndex;
        }
        output->results[insertAt].relativePath[pathIndex] = '\0';
        output->results[insertAt].fileNameMatch = fileNameMatch;
    }
    return true;
}

bool DeveloperToolProjectIdentityMatches(const char* expectedProjectId,
                                         uint64_t expectedGeneration,
                                         const char* actualProjectId,
                                         uint64_t actualGeneration) {
    if (!expectedProjectId || !actualProjectId || expectedProjectId[0] == '\0' ||
        actualProjectId[0] == '\0' || expectedGeneration == 0 ||
        expectedGeneration != actualGeneration) return false;
    const uint32_t expectedLength = textLength(expectedProjectId, kMaxProjectIdBytes);
    const uint32_t actualLength = textLength(actualProjectId, kMaxProjectIdBytes);
    if (expectedLength == 0 || expectedLength >= kMaxProjectIdBytes ||
        actualLength == 0 || actualLength >= kMaxProjectIdBytes) return false;
    uint32_t i = 0;
    while (expectedProjectId[i] != '\0' && actualProjectId[i] != '\0') {
        if (lowerAscii(expectedProjectId[i]) != lowerAscii(actualProjectId[i])) return false;
        ++i;
    }
    return expectedProjectId[i] == actualProjectId[i];
}

DeveloperToolId DeveloperToolIdFromShortcut(uint32_t keyCode, bool control,
                                            bool shift, bool alt) {
    if (!control || alt) return DeveloperToolId::None;
    if (!shift && (keyCode == 80u || keyCode == 112u)) return DeveloperToolId::GoToFile;
    if (!shift && (keyCode == 71u || keyCode == 103u)) return DeveloperToolId::GoToLine;
    if (shift && (keyCode == 70u || keyCode == 102u)) return DeveloperToolId::FindInFiles;
    return DeveloperToolId::None;
}

bool DeveloperToolParsePositiveLineNumber(const char* input, uint32_t* outLine) {
    if (outLine) *outLine = 0;
    if (!input || !outLine) return false;
    const uint32_t length = textLength(input, kDeveloperToolMaxLineInputBytes + 1u);
    if (length == 0 || length > kDeveloperToolMaxLineInputBytes) return false;
    uint32_t value = 0;
    for (uint32_t i = 0; i < length; ++i) {
        const char character = input[i];
        if (character < '0' || character > '9') return false;
        const uint32_t digit = static_cast<uint32_t>(character - '0');
        if (value > 0xFFFFFFFFu / 10u ||
            (value == 0xFFFFFFFFu / 10u && digit > 0xFFFFFFFFu % 10u)) return false;
        value = value * 10u + digit;
    }
    if (value == 0) return false;
    *outLine = value;
    return true;
}

bool DeveloperToolPositionAtLine(const char* text, uint32_t length, uint32_t line,
                                uint32_t* caret, uint32_t* selectionAnchor,
                                bool* selectionActive) {
    if ((length != 0 && !text) || line == 0 || !caret || !selectionAnchor || !selectionActive) return false;
    uint32_t currentLine = 1;
    uint32_t offset = 0;
    if (line > 1) {
        bool found = false;
        for (uint32_t i = 0; i < length; ++i) {
            if (text[i] != '\n') continue;
            ++currentLine;
            if (currentLine == line) {
                offset = i + 1;
                found = true;
                break;
            }
        }
        if (!found) return false;
    }
    *caret = offset;
    *selectionAnchor = offset;
    *selectionActive = false;
    return true;
}

void DeveloperToolRegistryInit(DeveloperToolRegistry* registry) {
    if (!registry) return;
    registry->count = 0;
    registry->activeId = DeveloperToolId::None;
    for (uint32_t i = 0; i < kDeveloperToolMaxCommands; ++i) {
        registry->entries[i].command.id = DeveloperToolId::None;
        registry->entries[i].command.label = nullptr;
        registry->entries[i].command.shortcut = nullptr;
        registry->entries[i].command.activate = nullptr;
        registry->entries[i].command.close = nullptr;
        registry->entries[i].active = false;
    }
}

bool DeveloperToolRegistryRegister(DeveloperToolRegistry* registry,
                                   const DeveloperToolCommand* command) {
    if (!registry || !command || command->id == DeveloperToolId::None ||
        !command->label || command->label[0] == '\0' || !command->activate || !command->close ||
        registry->count >= kDeveloperToolMaxCommands ||
        findIndex(registry, command->id) != kDeveloperToolMaxCommands) return false;
    registry->entries[registry->count].command = *command;
    registry->entries[registry->count].active = false;
    ++registry->count;
    return true;
}

const DeveloperToolCommand* DeveloperToolRegistryFind(const DeveloperToolRegistry* registry,
                                                       DeveloperToolId id) {
    const uint32_t index = findIndex(registry, id);
    return index < kDeveloperToolMaxCommands ? &registry->entries[index].command : nullptr;
}

const DeveloperToolCommand* DeveloperToolRegistryAt(const DeveloperToolRegistry* registry,
                                                     uint32_t index) {
    return registry && index < registry->count ? &registry->entries[index].command : nullptr;
}

bool DeveloperToolRegistryIsActive(const DeveloperToolRegistry* registry, DeveloperToolId id) {
    const uint32_t index = findIndex(registry, id);
    return index < kDeveloperToolMaxCommands && registry->entries[index].active;
}

bool DeveloperToolRegistryClose(DeveloperToolRegistry* registry, DeveloperToolId id,
                                void* userData) {
    const uint32_t index = findIndex(registry, id);
    if (index >= kDeveloperToolMaxCommands || registry->activeId != id ||
        !registry->entries[index].active) return true;
    DeveloperToolRegistryEntry& entry = registry->entries[index];
    if (!entry.command.close(userData)) return false;
    entry.active = false;
    registry->activeId = DeveloperToolId::None;
    return true;
}

bool DeveloperToolRegistryCloseActive(DeveloperToolRegistry* registry, void* userData) {
    return !registry || registry->activeId == DeveloperToolId::None ? true :
        DeveloperToolRegistryClose(registry, registry->activeId, userData);
}

bool DeveloperToolRegistryActivate(DeveloperToolRegistry* registry, DeveloperToolId id,
                                   void* userData) {
    const uint32_t index = findIndex(registry, id);
    if (index >= kDeveloperToolMaxCommands) return false;
    if (registry->activeId != DeveloperToolId::None && registry->activeId != id &&
        !DeveloperToolRegistryCloseActive(registry, userData)) return false;
    DeveloperToolRegistryEntry& entry = registry->entries[index];
    if (!entry.command.activate(userData)) return false;
    entry.active = true;
    registry->activeId = id;
    return true;
}

} // namespace developer_studio
} // namespace guidexos
