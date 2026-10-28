#include "developer_studio_project_symbols.h"
#include "test_check.h"

#include <cstdio>
#include <cstring>

using namespace guidexos::developer_studio;

static ProjectSymbolResults g_results = {};
static ProjectSearchService g_search = {};
static char g_source[kProjectSearchMaxFileBytes + 1u] = {};
static char g_path[kMaxPathBytes] = {};

struct NativeLimitNode {
    char path[kMaxPathBytes];
    FileInfoKind kind;
    uint32_t length;
};

struct NativeLimitFileSystem {
    NativeLimitNode nodes[80];
    uint32_t count;
};

static NativeLimitFileSystem g_fileSystem = {};

static NativeLimitNode* findNode(const char* path) {
    if (!path) return nullptr;
    for (uint32_t i = 0; i < g_fileSystem.count; ++i)
        if (std::strcmp(g_fileSystem.nodes[i].path, path) == 0)
            return &g_fileSystem.nodes[i];
    return nullptr;
}

static bool statFile(void*, const char* path, FileInfo* info) {
    NativeLimitNode* node = findNode(path);
    if (!node || !info) return false;
    info->kind = node->kind;
    info->size = node->length;
    return true;
}

static bool listFiles(void*, const char* path, FileListEntry* entries,
                      uint32_t capacity, uint32_t* outCount, bool* outTruncated) {
    if (!path || !entries || !outCount || !outTruncated) return false;
    *outCount = 0;
    *outTruncated = false;
    for (uint32_t i = 0; i < g_fileSystem.count; ++i) {
        const NativeLimitNode& node = g_fileSystem.nodes[i];
        uint32_t length = 0;
        while (node.path[length]) ++length;
        uint32_t slash = length;
        while (slash > 0 && node.path[slash - 1u] != '/') --slash;
        if (slash == 0) continue;
        char parent[kMaxPathBytes] = {};
        for (uint32_t j = 0; j + 1u < slash; ++j) parent[j] = node.path[j];
        parent[slash - 1u] = '\0';
        if (std::strcmp(parent, path) != 0) continue;
        if (*outCount >= capacity) {
            *outTruncated = true;
            break;
        }
        FileListEntry& entry = entries[(*outCount)++];
        uint32_t nameIndex = 0;
        while (slash < length && nameIndex + 1u < sizeof(entry.name))
            entry.name[nameIndex++] = node.path[slash++];
        entry.name[nameIndex] = '\0';
        entry.kind = node.kind;
        entry.size = node.length;
    }
    return true;
}

static bool readFile(void*, const char* path, char* buffer,
                     uint32_t capacity, uint32_t* outBytes) {
    NativeLimitNode* node = findNode(path);
    if (!node || node->kind != FileInfoKind::RegularFile || !buffer || !outBytes ||
        node->length > capacity) return false;
    std::memcpy(buffer, g_source, node->length);
    *outBytes = node->length;
    return true;
}

static void addNode(const char* path, FileInfoKind kind, uint32_t length = 0) {
    TEST_CHECK(g_fileSystem.count < sizeof(g_fileSystem.nodes) / sizeof(g_fileSystem.nodes[0]));
    NativeLimitNode& node = g_fileSystem.nodes[g_fileSystem.count++];
    std::snprintf(node.path, sizeof(node.path), "%s", path);
    node.kind = kind;
    node.length = length;
}

static void prepareProject(uint32_t fileCount) {
    g_fileSystem = {};
    addNode("c:/project", FileInfoKind::Directory);
    addNode("c:/project/set0", FileInfoKind::Directory);
    addNode("c:/project/set1", FileInfoKind::Directory);
    addNode("c:/project/set2", FileInfoKind::Directory);
    for (uint32_t i = 0; i < fileCount; ++i) {
        const uint32_t directory = i / 22u;
        std::snprintf(g_path, sizeof(g_path), "c:/project/set%u/file%02u.cpp",
                      directory, i % 22u);
        addNode(g_path, FileInfoKind::RegularFile, kProjectSearchMaxFileBytes);
    }
}

static void scanPreparedProject(uint64_t* operationId) {
    ProjectSearchRequest request = {};
    std::snprintf(request.projectId, sizeof(request.projectId), "%s", "com.example.native-limits");
    std::snprintf(request.rootPath, sizeof(request.rootPath), "%s", "c:/project");
    request.projectGeneration = 1;
    ProjectSearchOptionsInit(&request.options);
    std::snprintf(request.options.query, sizeof(request.options.query), "%s", "native-limits");
    request.fileSystem.userData = &g_fileSystem;
    request.fileSystem.stat = statFile;
    request.fileSystem.list = listFiles;
    request.fileSystem.read = readFile;
    request.scanVisitor = ProjectSymbolsVisitFile;
    request.scanVisitorUserData = &g_results;
    ProjectSearchErrorCode error = ProjectSearchErrorCode::None;
    TEST_CHECK(ProjectSearchStart(&g_search, &request, 0, operationId, &error));
    for (uint32_t i = 0; i < 1000 && ProjectSearchIsActive(&g_search); ++i)
        TEST_CHECK(ProjectSearchPoll(&g_search, *operationId, 1, i));
}

int main() {
    TEST_CHECK(kProjectSymbolsMaxFiles == 64u);
    TEST_CHECK(kProjectSymbolsMaxCandidates == 32u);
    TEST_CHECK(kProjectSymbolsMaxDisplayedResults == 24u);
    TEST_CHECK(kProjectSearchMaxFileBytes == 16u * 1024u);
    TEST_CHECK(kProjectSymbolsMaxFiles * kProjectSearchMaxFileBytes <
               kProjectSymbolsMaxBytes);

    std::memset(g_source, ' ', kProjectSearchMaxFileBytes);
    g_source[kProjectSearchMaxFileBytes] = '\0';

    // Exercise the complete Native-sized scanner path with maximum-size files.
    // The 2 MiB byte ceiling is not reachable while the current file and file
    // count bounds permit at most 64 * 16 KiB = 1 MiB.
    uint64_t operationId = 0;
    prepareProject(kProjectSymbolsMaxFiles);
    ProjectSymbolResultsInit(&g_results);
    ProjectSearchServiceInit(&g_search);
    scanPreparedProject(&operationId);
    const ProjectSearchOperation* operation = ProjectSearchOperationInfo(&g_search);
    TEST_CHECK(operation && operation->state == ProjectSearchState::Completed);
    TEST_CHECK(operation->filesSearched == kProjectSymbolsMaxFiles);
    TEST_CHECK(operation->bytesScanned ==
               static_cast<uint64_t>(kProjectSymbolsMaxFiles) * kProjectSearchMaxFileBytes);
    TEST_CHECK(g_results.filesProcessed == kProjectSymbolsMaxFiles);
    TEST_CHECK(g_results.bytesProcessed == operation->bytesScanned);
    TEST_CHECK(!operation->truncated && !g_results.truncated && !g_results.incomplete);
    TEST_CHECK(ProjectSearchRelease(&g_search, operationId));

    prepareProject(kProjectSymbolsMaxFiles + 1u);
    ProjectSymbolResultsInit(&g_results);
    ProjectSearchServiceInit(&g_search);
    scanPreparedProject(&operationId);
    operation = ProjectSearchOperationInfo(&g_search);
    TEST_CHECK(operation && operation->state == ProjectSearchState::Completed);
    TEST_CHECK(operation->truncated && operation->error == ProjectSearchErrorCode::ResultLimit);
    TEST_CHECK(g_results.filesProcessed == kProjectSymbolsMaxFiles);
    TEST_CHECK(g_results.truncated && g_results.incomplete);
    TEST_CHECK(ProjectSearchRelease(&g_search, operationId));

    ProjectSymbolResultsInit(&g_results);
    for (uint32_t i = 0; i < kProjectSymbolsMaxFiles; ++i) {
        std::snprintf(g_path, sizeof(g_path), "src/limit%02u.cpp", i);
        ProjectSearchScanFile file = {};
        file.relativePath = g_path;
        file.data = g_source;
        file.length = kProjectSearchMaxFileBytes;
        TEST_CHECK(ProjectSymbolsVisitFile(&g_results, &file));
    }
    TEST_CHECK(g_results.filesProcessed == kProjectSymbolsMaxFiles);
    TEST_CHECK(g_results.bytesProcessed ==
               static_cast<uint64_t>(kProjectSymbolsMaxFiles) * kProjectSearchMaxFileBytes);
    TEST_CHECK(!g_results.truncated && !g_results.incomplete);

    ProjectSearchScanFile overLimitFile = {};
    overLimitFile.relativePath = "src/limit-overflow.cpp";
    overLimitFile.data = g_source;
    overLimitFile.length = 1;
    TEST_CHECK(!ProjectSymbolsVisitFile(&g_results, &overLimitFile));
    TEST_CHECK(g_results.filesProcessed == kProjectSymbolsMaxFiles);
    TEST_CHECK(g_results.truncated && g_results.incomplete);

    ProjectSymbolResultsInit(&g_results);
    char declarations[1024] = {};
    uint32_t declarationBytes = 0;
    for (uint32_t i = 0; i < kProjectSymbolsMaxCandidates + 8u; ++i) {
        const int written = std::snprintf(declarations + declarationBytes,
            sizeof(declarations) - declarationBytes,
            "int candidate_%02u() {}\n", i);
        TEST_CHECK(written > 0 && static_cast<uint32_t>(written) <
                   sizeof(declarations) - declarationBytes);
        declarationBytes += static_cast<uint32_t>(written);
    }
    ProjectSearchScanFile candidateFile = {};
    candidateFile.relativePath = "src/candidates.cpp";
    candidateFile.data = declarations;
    candidateFile.length = declarationBytes;
    TEST_CHECK(!ProjectSymbolsVisitFile(&g_results, &candidateFile));
    TEST_CHECK(g_results.count == kProjectSymbolsMaxCandidates);
    TEST_CHECK(g_results.candidatesDropped == 1u);
    TEST_CHECK(g_results.truncated && g_results.incomplete);
    ProjectSymbolResultsSort(&g_results);
    TEST_CHECK(ProjectSymbolFilteredCount(&g_results, "", "") ==
               kProjectSymbolsMaxCandidates);
    TEST_CHECK(ProjectSymbolFilteredAt(&g_results, "", "",
               kProjectSymbolsMaxDisplayedResults - 1u) != nullptr);
    TEST_CHECK(ProjectSymbolFilteredAt(&g_results, "", "",
               kProjectSymbolsMaxDisplayedResults) == nullptr);
    return 0;
}
