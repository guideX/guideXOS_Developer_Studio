#include "developer_studio_include_graph.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

using namespace guidexos::developer_studio;

namespace {

struct CapturedDirectives {
    IncludeDirective items[64];
    uint32_t count;
};

static bool captureDirective(void* userData, const IncludeDirective* directive) {
    CapturedDirectives* captured = static_cast<CapturedDirectives*>(userData);
    if (!captured || !directive || captured->count >= 64) return false;
    captured->items[captured->count++] = *directive;
    return true;
}

struct EdgeContext {
    IncludeGraph* graph;
    uint64_t edgeId;
    uint64_t directiveId;
};

static bool addCapturedEdge(void* userData, const IncludeDirective* directive) {
    EdgeContext* context = static_cast<EdgeContext*>(userData);
    IncludeEdge edge = {};
    edge.edgeId = context->edgeId++;
    edge.directive = *directive;
    edge.directive.directiveId = context->directiveId++;
    strcpy(edge.sourceRelativePath, directive->sourceRelativePath);
    return IncludeGraphAddEdge(context->graph, edge);
}

struct MemFile {
    const char* relativePath;
    const char* contents;
    bool directory;
};

struct MemFs {
    const MemFile* files;
    uint32_t count;
};

static bool equals(const char* left, const char* right) {
    return left && right && strcmp(left, right) == 0;
}

static bool memStat(void* userData, const char* path, FileInfo* outInfo) {
    MemFs* fs = static_cast<MemFs*>(userData);
    for (uint32_t i = 0; i < fs->count; ++i) {
        char absolute[kMaxPathBytes] = {};
        strcpy(absolute, "c:/include-graph/");
        if (fs->files[i].relativePath[0]) strcat(absolute, fs->files[i].relativePath);
        if (equals(absolute, path)) {
            outInfo->kind = fs->files[i].directory ? FileInfoKind::Directory : FileInfoKind::RegularFile;
            outInfo->size = fs->files[i].contents ? strlen(fs->files[i].contents) : 0;
            return true;
        }
    }
    return false;
}

static bool memList(void* userData, const char* path, FileListEntry* entries, uint32_t capacity,
                    uint32_t* outCount, bool* outTruncated) {
    MemFs* fs = static_cast<MemFs*>(userData);
    *outCount = 0;
    *outTruncated = false;
    char prefix[kMaxPathBytes] = {};
    strcpy(prefix, path);
    if (prefix[0] != '\0' && prefix[strlen(prefix) - 1] != '/') strcat(prefix, "/");
    const size_t prefixLength = strlen(prefix);
    for (uint32_t i = 0; i < fs->count; ++i) {
        char absolute[kMaxPathBytes] = {};
        strcpy(absolute, "c:/include-graph/");
        strcat(absolute, fs->files[i].relativePath);
        if (strncmp(absolute, prefix, prefixLength) != 0) continue;
        const char* remaining = absolute + prefixLength;
        if (*remaining == '/') ++remaining;
        if (*remaining == '\0' || strchr(remaining, '/') != nullptr) continue;
        if (*outCount >= capacity) { *outTruncated = true; break; }
        strcpy(entries[*outCount].name, remaining);
        entries[*outCount].kind = fs->files[i].directory ? FileInfoKind::Directory : FileInfoKind::RegularFile;
        entries[*outCount].size = fs->files[i].contents ? strlen(fs->files[i].contents) : 0;
        ++*outCount;
    }
    return true;
}

static bool memRead(void* userData, const char* path, char* buffer, uint32_t capacity, uint32_t* outBytes) {
    MemFs* fs = static_cast<MemFs*>(userData);
    for (uint32_t i = 0; i < fs->count; ++i) {
        char absolute[kMaxPathBytes] = {};
        strcpy(absolute, "c:/include-graph/");
        strcat(absolute, fs->files[i].relativePath);
        if (equals(absolute, path) && !fs->files[i].directory) {
            const uint32_t length = static_cast<uint32_t>(strlen(fs->files[i].contents));
            if (length > capacity) return false;
            memcpy(buffer, fs->files[i].contents, length);
            *outBytes = length;
            return true;
        }
    }
    return false;
}

static bool unusedWrite(void*, const char*, const char*, uint32_t, uint32_t*) { return false; }
static bool unusedDirectory(void*, const char*) { return false; }
static bool unusedRemove(void*, const char*) { return false; }

static WorkspaceFileSystem makeFileSystem(MemFs* fs) {
    WorkspaceFileSystem result = {};
    result.userData = fs;
    result.stat = memStat;
    result.list = memList;
    result.read = memRead;
    result.write = unusedWrite;
    result.createDirectory = unusedDirectory;
    result.removePath = unusedRemove;
    return result;
}

static void testScanner() {
    const char source[] =
        "// #include \"comment.h\"\n"
        "const char* text = \"#include \\\"string.h\\\"\";\n"
        "const char* raw = R\"tag(#include \"raw.h\")tag\";\n"
        "#include \"renderer.h\"\n"
        "# include <project/api.h>\n"
        "#include CONFIG_HEADER\n"
        "#if 0\n#include \"inactive.h\"\n#else\n#include \\\n"
        "  \"continued.h\"\n#endif\n"
        "#if FEATURE\n#include \"conditional.h\"\n#endif\n";
    CapturedDirectives captured = {};
    uint32_t count = 0;
    bool truncated = false;
    assert(IncludeGraphScanDocument(source, static_cast<uint32_t>(strlen(source)), "src/main.cpp", 11, 4,
                                    captureDirective, &captured, &count, &truncated));
    assert(!truncated);
    assert(count == 6);
    assert(strcmp(captured.items[0].requestedPath, "renderer.h") == 0);
    assert(captured.items[0].delimiterKind == IncludeDelimiterKind::Quoted);
    assert(captured.items[1].delimiterKind == IncludeDelimiterKind::Angled);
    assert(strcmp(captured.items[2].requestedPath, "CONFIG_HEADER") == 0);
    assert(captured.items[3].directiveState == IncludeDirectiveState::InactiveIfZero);
    assert(captured.items[0].literalPath);
    assert(!captured.items[2].literalPath);
    assert(strcmp(captured.items[4].requestedPath, "continued.h") == 0);
    assert(captured.items[4].directiveState == IncludeDirectiveState::Active);
    assert(captured.items[5].directiveState == IncludeDirectiveState::ConditionalUnknown);
}

static void testGraphModel() {
    static IncludeGraphStorage storage;
    IncludeGraphStorageInit(&storage);
    IncludeGraph graph = {};
    IncludeGraphInit(&graph, &storage, "demo", 7);
    assert(IncludeGraphAddNode(&graph, "src/main.cpp", false, nullptr));
    assert(IncludeGraphAddNode(&graph, "src/renderer.h", false, nullptr));
    assert(IncludeGraphAddNode(&graph, "include/api.h", false, nullptr));
    const char source[] = "#include \"renderer.h\"\n#include <api.h>\n#include \"missing.h\"\n";
    EdgeContext context = { &graph, 1, 1 };
    uint32_t directives = 0;
    bool truncated = false;
    assert(IncludeGraphScanDocument(source, static_cast<uint32_t>(strlen(source)), "src/main.cpp", 1, 2,
                                    addCapturedEdge, &context, &directives, &truncated));
    assert(directives == 3 && graph.edgeCount == 3);
    IncludeGraphRequest request = {};
    strcpy(request.projectId, "demo");
    request.projectGeneration = 7;
    strcpy(request.rootPath, "C:/demo");
    strcpy(request.includeRoots[0], "include");
    request.includeRootCount = 1;
    request.allowProjectRoot = true;
    uint64_t resolved = 0;
    uint64_t unresolved = 0;
    assert(IncludeGraphResolveAll(&graph, request, &resolved, &unresolved));
    assert(resolved == 2 && unresolved == 1);
    assert(graph.edges[0].resolution.state == IncludeResolutionState::Resolved);
    assert(strcmp(graph.edges[0].targetRelativePath, "src/renderer.h") == 0);
    assert(graph.edges[1].resolution.state == IncludeResolutionState::Resolved);
    assert(strcmp(graph.edges[1].targetRelativePath, "include/api.h") == 0);
    assert(graph.edges[2].resolution.state == IncludeResolutionState::Missing);
    assert(IncludeGraphBuildReverseEdges(&graph));
    assert(graph.nodes[0].outgoingEdgeCount == 3);
    assert(graph.nodes[1].incomingEdgeCount == 1);
    uint32_t nodes[8] = {}, depths[8] = {}, parents[8] = {};
    IncludeGraphTraversalResult traversal = { nodes, depths, parents, 0, 8, 0, 0, false };
    assert(IncludeGraphBuildTraversal(&graph, "src/main.cpp", IncludeGraphTraversalDirection::Includes, false, &traversal));
    assert(traversal.nodeCount == 2);
    assert(traversal.parentEdgeIndices[0] != 0xFFFFFFFFu);
}

static void testResolutionSafety() {
    static IncludeGraphStorage storage;
    IncludeGraphStorageInit(&storage);
    IncludeGraph graph = {};
    IncludeGraphInit(&graph, &storage, "demo", 3);
    assert(IncludeGraphAddNode(&graph, "src/main.cpp", false, nullptr));
    assert(IncludeGraphAddNode(&graph, "include/api.h", false, nullptr));
    assert(IncludeGraphAddNode(&graph, "Header.h", false, nullptr));
    assert(IncludeGraphAddNode(&graph, "header.h", false, nullptr));
    const char source[] =
        "#include \"../include/api.h\"\n"
        "#include <HEADER.h>\n"
        "#include HEADER_NAME\n"
        "#include \"/absolute.h\"\n"
        "#include \"../../outside.h\"\n";
    EdgeContext context = { &graph, 1, 1 };
    uint32_t directives = 0;
    bool truncated = false;
    assert(IncludeGraphScanDocument(source, static_cast<uint32_t>(strlen(source)), "src/main.cpp", 8, 2,
                                    addCapturedEdge, &context, &directives, &truncated));
    assert(directives == 5 && graph.edgeCount == 5);
    IncludeGraphRequest request = {};
    strcpy(request.projectId, "demo");
    request.projectGeneration = 3;
    strcpy(request.rootPath, "C:/demo");
    request.allowProjectRoot = true;
    uint64_t resolved = 0;
    uint64_t unresolved = 0;
    assert(IncludeGraphResolveAll(&graph, request, &resolved, &unresolved));
    assert(graph.edges[0].resolution.state == IncludeResolutionState::Resolved);
    assert(strcmp(graph.edges[0].targetRelativePath, "include/api.h") == 0);
    assert(graph.edges[1].resolution.state == IncludeResolutionState::Ambiguous);
    assert(graph.edges[1].resolution.ambiguousCandidateCount == 2);
    assert(graph.edges[2].resolution.state == IncludeResolutionState::UnsupportedMacro);
    assert(graph.edges[3].resolution.state == IncludeResolutionState::OutsideProject);
    assert(graph.edges[4].resolution.state == IncludeResolutionState::OutsideProject);
    assert(resolved == 1 && unresolved == 4);
}

static void testOperationAndCycle() {
    static const MemFile files[] = {
        { "src", nullptr, true },
        { "include", nullptr, true },
        { "src/main.cpp", "#include \"a.h\"\n", false },
        { "src/a.h", "#include \"b.h\"\n", false },
        { "src/b.h", "#include \"a.h\"\n", false }
    };
    MemFs mem = { files, sizeof(files) / sizeof(files[0]) };
    static IncludeGraphStorage storage;
    IncludeGraphStorageInit(&storage);
    IncludeGraph graph = {};
    IncludeGraphInit(&graph, &storage, "demo", 9);
    IncludeGraphRequest request = {};
    strcpy(request.projectId, "demo");
    request.projectGeneration = 9;
    strcpy(request.rootPath, "c:/include-graph");
    request.fileSystem = makeFileSystem(&mem);
    request.allowProjectRoot = true;
    static IncludeGraphBuildOperation operation;
    operation = IncludeGraphBuildOperation();
    uint64_t operationId = 0;
    IncludeGraphErrorCode error = IncludeGraphErrorCode::None;
    assert(IncludeGraphStart(&operation, &graph, request, 100, &operationId, &error));
    for (uint32_t i = 0; i < 500 && IncludeGraphIsActive(&operation); ++i) assert(IncludeGraphPoll(&operation, operationId, 4, 100 + i));
    assert(operation.state == IncludeGraphBuildState::Completed);
    assert(graph.complete);
    assert(graph.nodeCount == 3);
    assert(graph.edgeCount == 3);
    assert(graph.cycleCount == 1);
    assert(graph.cycles[0].memberCount == 2);
    assert(IncludeGraphIsCurrent(&graph, "demo", 9));

    const char replacement[] = "#include \"missing.h\"\n";
    assert(IncludeGraphRescanDocument(&graph, request, "src/b.h", 21, 6, replacement,
                                      static_cast<uint32_t>(strlen(replacement)), true, &error));
    assert(graph.complete);
    assert(graph.edgeCount == 3);
    assert(graph.cycleCount == 0);

    static IncludeGraphStorage cancelledStorage;
    IncludeGraphStorageInit(&cancelledStorage);
    IncludeGraph cancelledGraph = {};
    IncludeGraphInit(&cancelledGraph, &cancelledStorage, "demo", 9);
    static IncludeGraphBuildOperation cancelled = {};
    uint64_t cancelledId = 0;
    assert(IncludeGraphStart(&cancelled, &cancelledGraph, request, 500, &cancelledId, &error));
    assert(IncludeGraphPoll(&cancelled, cancelledId, 1, 500));
    assert(IncludeGraphCancel(&cancelled, cancelledId));
    assert(IncludeGraphPoll(&cancelled, cancelledId, 1, 501));
    assert(cancelled.state == IncludeGraphBuildState::Cancelled);
}

} // namespace

int main() {
    testScanner();
    testGraphModel();
    testResolutionSafety();
    testOperationAndCycle();
    printf("Developer Studio include graph model PASS\n");
    return 0;
}
