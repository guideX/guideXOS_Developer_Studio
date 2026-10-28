#include "developer_studio_project_symbols.h"
#include "developer_studio_build.h"
#include "developer_studio_run.h"
#include "developer_studio_workspace.h"
#include "test_check.h"

#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>

using namespace guidexos::developer_studio;

static ProjectSymbolResults g_results = {};
static ProjectSearchService g_searchService = {};
static WorkspaceController g_workspaceController = {};

struct SymbolFsNode {
    char path[kMaxPathBytes];
    FileInfoKind kind;
    char data[512];
    uint32_t length;
    bool exists;
};

struct SymbolTestFileSystem {
    SymbolFsNode nodes[32];
    uint32_t count;
    uint32_t writeCalls;
    char disappearedPath[kMaxPathBytes];
};

static SymbolTestFileSystem g_fileSystem = {};

static bool copyTestText(char* output, uint32_t capacity, const char* input) {
    if (!output || capacity == 0 || !input) return false;
    uint32_t i = 0;
    while (i + 1u < capacity && input[i]) { output[i] = input[i]; ++i; }
    if (input[i]) { output[0] = '\0'; return false; }
    output[i] = '\0';
    return true;
}

static bool sameTestPath(const char* left, const char* right) {
    return PathsEqual(left, right);
}

static SymbolFsNode* findNode(SymbolTestFileSystem* fileSystem, const char* path) {
    if (!fileSystem || !path) return nullptr;
    for (uint32_t i = 0; i < fileSystem->count; ++i)
        if (fileSystem->nodes[i].exists && sameTestPath(fileSystem->nodes[i].path, path))
            return &fileSystem->nodes[i];
    return nullptr;
}

static void addDirectory(SymbolTestFileSystem* fileSystem, const char* path) {
    SymbolFsNode& node = fileSystem->nodes[fileSystem->count++];
    node.kind = FileInfoKind::Directory;
    node.exists = true;
    TEST_CHECK(copyTestText(node.path, sizeof(node.path), path));
}

static void addFile(SymbolTestFileSystem* fileSystem, const char* path,
                    const char* data, uint32_t length) {
    SymbolFsNode& node = fileSystem->nodes[fileSystem->count++];
    node.kind = FileInfoKind::RegularFile;
    node.exists = true;
    node.length = length;
    TEST_CHECK(copyTestText(node.path, sizeof(node.path), path));
    if (length <= sizeof(node.data)) std::memcpy(node.data, data, length);
    else TEST_CHECK(false);
}

static void setFileData(SymbolTestFileSystem* fileSystem, const char* path,
                        const char* data, uint32_t length) {
    SymbolFsNode* node = findNode(fileSystem, path);
    TEST_CHECK(node && node->kind == FileInfoKind::RegularFile && length <= sizeof(node->data));
    if (!node || length > sizeof(node->data)) return;
    node->length = length;
    std::memcpy(node->data, data, length);
}

static bool symbolFsStat(void* userData, const char* path, FileInfo* info) {
    SymbolTestFileSystem* fileSystem = static_cast<SymbolTestFileSystem*>(userData);
    if (!fileSystem || !path || !info ||
        (fileSystem->disappearedPath[0] && sameTestPath(fileSystem->disappearedPath, path))) return false;
    SymbolFsNode* node = findNode(fileSystem, path);
    if (!node) return false;
    info->kind = node->kind;
    info->size = node->kind == FileInfoKind::RegularFile ? node->length : 0;
    return true;
}

static bool symbolFsList(void* userData, const char* path, FileListEntry* entries,
                         uint32_t capacity, uint32_t* outCount, bool* outTruncated) {
    SymbolTestFileSystem* fileSystem = static_cast<SymbolTestFileSystem*>(userData);
    if (!fileSystem || !path || !entries || !outCount || !outTruncated) return false;
    *outCount = 0;
    *outTruncated = false;
    for (uint32_t i = 0; i < fileSystem->count; ++i) {
        const SymbolFsNode& node = fileSystem->nodes[i];
        if (!node.exists) continue;
        uint32_t length = 0;
        while (node.path[length]) ++length;
        uint32_t slash = length;
        while (slash > 0 && node.path[slash - 1u] != '/' && node.path[slash - 1u] != '\\') --slash;
        if (slash == 0 || !sameTestPath(path, node.path)) {
            char parent[kMaxPathBytes] = {};
            if (slash == 0) continue;
            for (uint32_t j = 0; j + 1u < slash; ++j) parent[j] = node.path[j];
            parent[slash - 1u] = '\0';
            if (!sameTestPath(path, parent)) continue;
        } else {
            // The listed path is itself a file/directory, not a child entry.
            continue;
        }
        if (*outCount >= capacity) { *outTruncated = true; break; }
        const char* name = node.path + slash;
        FileListEntry& entry = entries[(*outCount)++];
        if (!copyTestText(entry.name, sizeof(entry.name), name)) return false;
        entry.kind = node.kind;
        entry.size = node.kind == FileInfoKind::RegularFile ? node.length : 0;
    }
    return true;
}

static bool symbolFsRead(void* userData, const char* path, char* buffer,
                         uint32_t capacity, uint32_t* outBytes) {
    SymbolTestFileSystem* fileSystem = static_cast<SymbolTestFileSystem*>(userData);
    SymbolFsNode* node = findNode(fileSystem, path);
    if (!node || node->kind != FileInfoKind::RegularFile || !buffer || !outBytes ||
        node->length > capacity) return false;
    std::memcpy(buffer, node->data, node->length);
    *outBytes = node->length;
    return true;
}

static bool symbolFsWrite(void* userData, const char*, const char*, uint32_t bytes, uint32_t* outBytes) {
    SymbolTestFileSystem* fileSystem = static_cast<SymbolTestFileSystem*>(userData);
    if (!fileSystem || !outBytes) return false;
    ++fileSystem->writeCalls;
    *outBytes = bytes;
    return true;
}

static WorkspaceFileSystem makeSymbolFileSystem(SymbolTestFileSystem* fileSystem) {
    WorkspaceFileSystem value = {};
    value.userData = fileSystem;
    value.stat = symbolFsStat;
    value.list = symbolFsList;
    value.read = symbolFsRead;
    value.write = symbolFsWrite;
    return value;
}

static void pollSymbolSearch(uint64_t operationId, uint64_t startTime = 0) {
    for (uint32_t i = 0; i < 1000 && ProjectSearchIsActive(&g_searchService); ++i)
        TEST_CHECK(ProjectSearchPoll(&g_searchService, operationId, 1, startTime + i));
}

static bool visit(const char* path, const char* source) {
    ProjectSearchScanFile file = {};
    file.relativePath = path;
    file.data = source;
    file.length = static_cast<uint32_t>(std::strlen(source));
    return ProjectSymbolsVisitFile(&g_results, &file);
}

static uint32_t find(const char* name, const char* path, uint32_t occurrence = 0) {
    for (uint32_t i = 0; i < g_results.count; ++i) {
        if (std::strcmp(g_results.candidates[i].name, name) != 0 ||
            std::strcmp(g_results.candidates[i].relativePath, path) != 0) continue;
        if (occurrence-- == 0) return i;
    }
    return 0xFFFFFFFFu;
}

static void testMultiFileCategoriesFiltersAndOrdering() {
    ProjectSymbolResultsInit(&g_results);
    const char* zSource =
        "int duplicate() { return 1; }\n"
        "namespace tools { class Worker { public: void run() {} }; }\n"
        "int duplicate() { return 2; }\n";
    const char* aSource =
        "struct Point { int x; };\n"
        "enum Color { Red, Blue };\n"
        "int duplicate();\n";
    TEST_CHECK(visit("src/z.cpp", zSource));
    TEST_CHECK(visit("include/a.hpp", aSource));
    TEST_CHECK(g_results.filesProcessed == 2);
    TEST_CHECK(g_results.totalRecognized == 8);
    TEST_CHECK(g_results.count == 8);
    ProjectSymbolResultsSort(&g_results);
    TEST_CHECK(std::strcmp(g_results.candidates[0].relativePath, "include/a.hpp") == 0);
    TEST_CHECK(find("Point", "include/a.hpp") != 0xFFFFFFFFu);
    TEST_CHECK(find("Color", "include/a.hpp") != 0xFFFFFFFFu);
    TEST_CHECK(find("Worker", "src/z.cpp") != 0xFFFFFFFFu);
    TEST_CHECK(find("run", "src/z.cpp") != 0xFFFFFFFFu);
    TEST_CHECK(find("duplicate", "src/z.cpp", 0) != 0xFFFFFFFFu);
    TEST_CHECK(find("duplicate", "src/z.cpp", 1) != 0xFFFFFFFFu);

    TEST_CHECK(ProjectSymbolFilterValid("dup", "src/z"));
    TEST_CHECK(ProjectSymbolFilteredCount(&g_results, "dup", "src/z.cpp") == 2);
    TEST_CHECK(ProjectSymbolFilteredCount(&g_results, "point", "src/z.cpp") == 0);
    TEST_CHECK(ProjectSymbolFilteredCount(&g_results, "point", "a.hpp") == 1);
    const ProjectSymbolCandidate* filtered =
        ProjectSymbolFilteredAt(&g_results, "DUP", "SRC/Z", 1);
    TEST_CHECK(filtered && std::strcmp(filtered->name, "duplicate") == 0 && filtered->line == 3);
    TEST_CHECK(ProjectSymbolFilteredAt(&g_results, "dup", "src/z.cpp",
        kProjectSymbolsMaxDisplayedResults) == nullptr);

    char tooLongName[kProjectSymbolsMaxNameFilterBytes + 2u] = {};
    std::memset(tooLongName, 'n', sizeof(tooLongName) - 1u);
    TEST_CHECK(!ProjectSymbolFilterValid(tooLongName, ""));
}

static void testCurrentContentsAndDuplicateIdentity() {
    ProjectSymbolResultsInit(&g_results);
    const char* saved = "int repeated() { return 1; }\nint repeated() { return 2; }\n";
    TEST_CHECK(visit("src/repeat.cpp", saved));
    ProjectSymbolResultsSort(&g_results);
    const uint32_t targetIndex = find("repeated", "src/repeat.cpp", 1);
    TEST_CHECK(targetIndex != 0xFFFFFFFFu);
    const ProjectSymbolCandidate selected = g_results.candidates[targetIndex];

    const char* shifted = "\n\nint repeated() { return 1; }\nint repeated() { return 2; }\n";
    uint32_t line = 0;
    uint32_t offset = 0;
    TEST_CHECK(ProjectSymbolValidateCurrent(&g_results, &selected, "src/repeat.cpp",
        shifted, static_cast<uint32_t>(std::strlen(shifted)), &line, &offset));
    TEST_CHECK(line == 4);
    TEST_CHECK(offset == static_cast<uint32_t>(
        std::strstr(shifted, "int repeated() { return 2;") - shifted + 4));

    const char* changed = "int changed() { return 1; }\n";
    TEST_CHECK(!ProjectSymbolValidateCurrent(&g_results, &selected, "src/repeat.cpp",
        changed, static_cast<uint32_t>(std::strlen(changed)), &line, &offset));
    TEST_CHECK(!ProjectSymbolValidateCurrent(&g_results, &selected, "src/other.cpp",
        saved, static_cast<uint32_t>(std::strlen(saved)), &line, &offset));
}

static void testEmptyUnsupportedAndCapacityBounds() {
    ProjectSymbolResultsInit(&g_results);
    TEST_CHECK(visit("src/empty.cpp", ""));
    TEST_CHECK(g_results.count == 0 && g_results.totalRecognized == 0);
    TEST_CHECK(visit("notes.txt", "int hidden() {}\n"));
    TEST_CHECK(g_results.incomplete);

    ProjectSymbolResultsInit(&g_results);
    std::string many;
    const uint32_t requested = kProjectSymbolsMaxCandidates + 8u;
    for (uint32_t i = 0; i < requested; ++i) {
        char declaration[64] = {};
        std::snprintf(declaration, sizeof(declaration), "int candidate_%03u() {}\n", i);
        many += declaration;
    }
    bool accepted = true;
    uint32_t fileIndex = 0;
    const uint32_t maxSymbolsInOneFile = kDocumentOutlineMaxSymbols;
    for (uint32_t start = 0; start < requested && accepted; start += maxSymbolsInOneFile) {
        std::string fileSource;
        const uint32_t end = start + maxSymbolsInOneFile < requested ?
            start + maxSymbolsInOneFile : requested;
        for (uint32_t i = start; i < end; ++i) {
            char declaration[64] = {};
            std::snprintf(declaration, sizeof(declaration), "int candidate_%03u() {}\n", i);
            fileSource += declaration;
        }
        char path[64] = {};
        std::snprintf(path, sizeof(path), "src/capacity%u.cpp", fileIndex++);
        accepted = visit(path, fileSource.c_str());
    }
    TEST_CHECK(!accepted);
    TEST_CHECK(g_results.count == kProjectSymbolsMaxCandidates);
    TEST_CHECK(g_results.truncated && g_results.incomplete && g_results.candidatesDropped != 0);
}

static void testPathContainmentAndNoInputMutation() {
    const char source[] = "int readOnly() {}\n";
    char before[sizeof(source)] = {};
    std::memcpy(before, source, sizeof(source));
    ProjectSymbolResultsInit(&g_results);
    TEST_CHECK(visit("src/readonly.cpp", source));
    TEST_CHECK(std::memcmp(source, before, sizeof(source)) == 0);

    char resolved[kMaxPathBytes] = {};
    TEST_CHECK(ProjectSymbolsResolvePath("C:/project", "src/main.cpp", resolved, sizeof(resolved)));
    TEST_CHECK(std::strcmp(resolved, "c:/project/src/main.cpp") == 0);
    TEST_CHECK(!ProjectSymbolsResolvePath("C:/project", "../outside.cpp", resolved, sizeof(resolved)));
    TEST_CHECK(!ProjectSymbolsResolvePath("C:/project", "C:/outside.cpp", resolved, sizeof(resolved)));
    TEST_CHECK(!ProjectSymbolsResolvePath("C:/project", "/outside.cpp", resolved, sizeof(resolved)));
    TEST_CHECK(!ProjectSymbolsResolvePath("C:/project", "../project-sibling/x.cpp", resolved, sizeof(resolved)));
}

static Project validProjectForIsolation() {
    Project project = {};
    project.valid = true;
    project.formatVersion = 1;
    project.kind = ProjectKind::NativeGuiApplication;
    copyTestText(project.projectId, sizeof(project.projectId), "com.example.symbols");
    copyTestText(project.displayName, sizeof(project.displayName), "Symbol Fixture");
    copyTestText(project.rootPath, sizeof(project.rootPath), "C:/project");
    copyTestText(project.manifestPath, sizeof(project.manifestPath), "app/app.json");
    copyTestText(project.targetProfileId, sizeof(project.targetProfileId), "guidexos.amd64.hosted.native");
    copyTestText(project.sourceRoot, sizeof(project.sourceRoot), "src");
    copyTestText(project.entryPoint, sizeof(project.entryPoint), "gx_main");
    copyTestText(project.abi, sizeof(project.abi), "guidexos-c-abi-v1");
    copyTestText(project.architecture, sizeof(project.architecture), "amd64");
    copyTestText(project.outputName, sizeof(project.outputName), "symbol-fixture");
    project.hasSourceFiles = true;
    project.sourceFileCount = 1;
    copyTestText(project.sourceFiles[0], sizeof(project.sourceFiles[0]), "main.cpp");
    return project;
}

static void testProjectWalkerSnapshotsBoundsAndBuildRunIsolation() {
    g_fileSystem = {};
    addDirectory(&g_fileSystem, "c:/project");
    addDirectory(&g_fileSystem, "c:/project/.git");
    addDirectory(&g_fileSystem, "c:/project/build");
    addDirectory(&g_fileSystem, "c:/project/node_modules");
    addDirectory(&g_fileSystem, "c:/project/src");
    const char initialMain[] = "int oldSaved() {}\n";
    const char diskDirty[] = "int diskOnly() {}\n";
    const char typeHeader[] =
        "namespace demo { class Worker { public: void run() {} }; enum Color { Red }; }\n";
    const char binary[] = { 'i', 'n', 't', ' ', '\0', 'b', 'i', 'n', 'a', 'r', 'y' };
    addFile(&g_fileSystem, "c:/project/src/main.cpp", initialMain, sizeof(initialMain) - 1u);
    addFile(&g_fileSystem, "c:/project/src/dirty.cpp", diskDirty, sizeof(diskDirty) - 1u);
    addFile(&g_fileSystem, "c:/project/src/types.hpp", typeHeader, sizeof(typeHeader) - 1u);
    addFile(&g_fileSystem, "c:/project/src/binary.cpp", binary, sizeof(binary));
    addFile(&g_fileSystem, "c:/project/src/notes.txt", "int unsupported() {}", 20u);
    addFile(&g_fileSystem, "c:/project/src/removed.cpp", "int removed() {}", 16u);
    addFile(&g_fileSystem, "c:/project/build/generated.cpp", "int generated() {}", 19u);
    addFile(&g_fileSystem, "c:/project/.git/hidden.cpp", "int hidden() {}", 15u);
    addFile(&g_fileSystem, "c:/project/node_modules/vendor.cpp", "int vendor() {}", 15u);
    copyTestText(g_fileSystem.disappearedPath, sizeof(g_fileSystem.disappearedPath),
                 "c:/project/src/removed.cpp");

    Project project = validProjectForIsolation();
    g_workspaceController = {};
    WorkspaceControllerInit(&g_workspaceController, makeSymbolFileSystem(&g_fileSystem));
    g_workspaceController.model.open = true;
    g_workspaceController.model.hasProject = true;
    g_workspaceController.model.project = project;
    copyTestText(g_workspaceController.model.rootPath,
        sizeof(g_workspaceController.model.rootPath), project.rootPath);
    TEST_CHECK(WorkspaceControllerOpenDocument(&g_workspaceController,
        "c:/project/src/main.cpp"));
    Document* openMain = WorkspaceControllerActiveDocument(&g_workspaceController);
    TEST_CHECK(openMain != nullptr);
    if (openMain) {
        openMain->buffer.caret = 5;
        openMain->buffer.selectionAnchor = 2;
        openMain->buffer.selectionActive = true;
    }
    const uint64_t originalDocumentId = openMain ? openMain->documentId : 0;
    uint32_t openedIndex = kMaxOpenDocuments;
    OutputErrorCode navigationError = OutputErrorCode::None;
    TEST_CHECK(!WorkspaceControllerOpenDocumentAtLocation(&g_workspaceController,
        project.projectId, "src/missing.cpp", 1, 1, &openedIndex, &navigationError));
    Document* stillActive = WorkspaceControllerActiveDocument(&g_workspaceController);
    TEST_CHECK(stillActive && stillActive->documentId == originalDocumentId);
    TEST_CHECK(stillActive && stillActive->buffer.caret == 5 &&
        stillActive->buffer.selectionAnchor == 2 && stillActive->buffer.selectionActive);
    ProjectSourceFile sources[kMaxProjectSourceFiles] = {};
    uint32_t sourceCount = 0;
    TEST_CHECK(WorkspaceControllerEnumerateProjectSources(&g_workspaceController, sources,
        kMaxProjectSourceFiles, &sourceCount));
    TEST_CHECK(sourceCount == 1 && std::strcmp(sources[0].relativePath, "src/main.cpp") == 0);

    BuildRequest beforeBuild = {};
    BuildErrorCode buildError = BuildErrorCode::None;
    TEST_CHECK(BuildRequestFromProject(project, &beforeBuild, &buildError));
    BuildResult built = {};
    built.state = BuildState::Succeeded;
    built.artifactValid = true;
    built.artifactEntryPoint = true;
    built.sourceMembershipHash = BuildRequestSourceMembershipHash(beforeBuild);
    copyTestText(built.projectId, sizeof(built.projectId), project.projectId);
    copyTestText(built.projectRoot, sizeof(built.projectRoot), project.rootPath);
    copyTestText(built.targetProfile, sizeof(built.targetProfile), project.targetProfileId);
    copyTestText(built.configuration, sizeof(built.configuration), beforeBuild.configuration);
    copyTestText(built.artifactPath, sizeof(built.artifactPath), beforeBuild.expectedArtifact);
    copyTestText(built.artifactArchitecture, sizeof(built.artifactArchitecture), "amd64");
    copyTestText(built.artifactSha256, sizeof(built.artifactSha256),
        "0123456789012345678901234567890123456789012345678901234567890123");
    RunRequest runBefore = {};
    RunErrorCode runError = RunErrorCode::None;
    TEST_CHECK(RunRequestFromBuild(project, built, &runBefore, &runError));
    TEST_CHECK(std::strcmp(runBefore.artifactPath, beforeBuild.expectedArtifact) == 0);

    ProjectSymbolResultsInit(&g_results);
    ProjectSearchServiceInit(&g_searchService);
    ProjectSearchRequest request = {};
    copyTestText(request.projectId, sizeof(request.projectId), project.projectId);
    copyTestText(request.rootPath, sizeof(request.rootPath), project.rootPath);
    request.projectGeneration = 7;
    ProjectSearchOptionsInit(&request.options);
    copyTestText(request.options.query, sizeof(request.options.query), "lexical-symbol-scan");
    request.fileSystem = makeSymbolFileSystem(&g_fileSystem);
    ProjectSearchDocumentSnapshot dirty = {};
    copyTestText(dirty.relativePath, sizeof(dirty.relativePath), "src/dirty.cpp");
    copyTestText(dirty.data, sizeof(dirty.data), "int memoryOnly() {}\n");
    dirty.length = static_cast<uint32_t>(std::strlen(dirty.data));
    dirty.documentId = 42;
    dirty.documentGeneration = 9;
    request.dirtyDocuments = &dirty;
    request.dirtyDocumentCount = 1;
    request.scanVisitor = ProjectSymbolsVisitFile;
    request.scanVisitorUserData = &g_results;
    uint64_t operationId = 0;
    ProjectSearchErrorCode searchError = ProjectSearchErrorCode::None;
    TEST_CHECK(ProjectSearchStart(&g_searchService, &request, 0, &operationId, &searchError));
    TEST_CHECK(ProjectSearchPoll(&g_searchService, operationId, 1, 1));
    TEST_CHECK(g_searchService.operation.filesSearched == 0);
    const char modifiedMain[] =
        "// int commentFake() {}\nconst char* text = \"int stringFake() {}\";\nint latestSaved() {}\n";
    setFileData(&g_fileSystem, "c:/project/src/main.cpp", modifiedMain, sizeof(modifiedMain) - 1u);
    copyTestText(dirty.data, sizeof(dirty.data), "int changedAfterStart() {}\n");
    pollSymbolSearch(operationId, 2);
    const ProjectSearchOperation* operation = ProjectSearchOperationInfo(&g_searchService);
    TEST_CHECK(operation && operation->state == ProjectSearchState::Completed);
    TEST_CHECK(g_results.filesProcessed == 3);
    TEST_CHECK(g_results.totalRecognized == 6);
    ProjectSymbolResultsSort(&g_results);
    TEST_CHECK(find("latestSaved", "src/main.cpp") != 0xFFFFFFFFu);
    TEST_CHECK(find("oldSaved", "src/main.cpp") == 0xFFFFFFFFu);
    TEST_CHECK(find("commentFake", "src/main.cpp") == 0xFFFFFFFFu);
    TEST_CHECK(find("stringFake", "src/main.cpp") == 0xFFFFFFFFu);
    TEST_CHECK(find("memoryOnly", "src/dirty.cpp") != 0xFFFFFFFFu);
    TEST_CHECK(find("diskOnly", "src/dirty.cpp") == 0xFFFFFFFFu);
    TEST_CHECK(find("Worker", "src/types.hpp") != 0xFFFFFFFFu);
    TEST_CHECK(find("run", "src/types.hpp") != 0xFFFFFFFFu);
    TEST_CHECK(find("Color", "src/types.hpp") != 0xFFFFFFFFu);
    TEST_CHECK(find("generated", "build/generated.cpp") == 0xFFFFFFFFu);
    TEST_CHECK(find("hidden", ".git/hidden.cpp") == 0xFFFFFFFFu);
    TEST_CHECK(find("vendor", "node_modules/vendor.cpp") == 0xFFFFFFFFu);
    TEST_CHECK(find("unsupported", "src/notes.txt") == 0xFFFFFFFFu);
    TEST_CHECK(operation->diagnostics.binaryFiles == 1);
    TEST_CHECK(operation->diagnostics.changedOrDisappearedFiles == 1);
    TEST_CHECK(operation->diagnostics.filesSkipped == 2);
    TEST_CHECK(g_fileSystem.writeCalls == 0);

    ProjectSourceFile sourcesAfter[kMaxProjectSourceFiles] = {};
    uint32_t sourceCountAfter = 0;
    TEST_CHECK(WorkspaceControllerEnumerateProjectSources(&g_workspaceController, sourcesAfter,
        kMaxProjectSourceFiles, &sourceCountAfter));
    TEST_CHECK(sourceCountAfter == sourceCount &&
        std::strcmp(sourcesAfter[0].relativePath, sources[0].relativePath) == 0);
    BuildRequest afterBuild = {};
    TEST_CHECK(BuildRequestFromProject(project, &afterBuild, &buildError));
    TEST_CHECK(BuildRequestSourceMembershipHash(afterBuild) ==
        BuildRequestSourceMembershipHash(beforeBuild));
    RunRequest runAfter = {};
    TEST_CHECK(RunRequestFromBuild(project, built, &runAfter, &runError));
    TEST_CHECK(std::strcmp(runAfter.artifactPath, runBefore.artifactPath) == 0);
    TEST_CHECK(g_fileSystem.writeCalls == 0);
    TEST_CHECK(ProjectSearchRelease(&g_searchService, operationId));
}

static void testEmptyProjectCancellationRescanAndIdentitySwitch() {
    g_fileSystem = {};
    addDirectory(&g_fileSystem, "c:/project");
    addDirectory(&g_fileSystem, "c:/project/src");
    addFile(&g_fileSystem, "c:/project/src/only.cpp", "int only() {}\n", 14u);
    ProjectSearchRequest request = {};
    copyTestText(request.projectId, sizeof(request.projectId), "com.example.symbols");
    copyTestText(request.rootPath, sizeof(request.rootPath), "C:/project");
    request.projectGeneration = 3;
    ProjectSearchOptionsInit(&request.options);
    copyTestText(request.options.query, sizeof(request.options.query), "symbols");
    request.fileSystem = makeSymbolFileSystem(&g_fileSystem);
    request.scanVisitor = ProjectSymbolsVisitFile;
    request.scanVisitorUserData = &g_results;
    ProjectSymbolResultsInit(&g_results);
    ProjectSearchServiceInit(&g_searchService);
    uint64_t operationId = 0;
    ProjectSearchErrorCode error = ProjectSearchErrorCode::None;
    TEST_CHECK(ProjectSearchStart(&g_searchService, &request, 0, &operationId, &error));
    TEST_CHECK(ProjectSearchPoll(&g_searchService, operationId, 1, 1));
    TEST_CHECK(ProjectSearchCancel(&g_searchService, operationId));
    TEST_CHECK(ProjectSearchPoll(&g_searchService, operationId, 1, 2));
    TEST_CHECK(ProjectSearchOperationInfo(&g_searchService)->state == ProjectSearchState::Cancelled);
    TEST_CHECK(ProjectSearchRelease(&g_searchService, operationId));

    ProjectSymbolResultsInit(&g_results);
    TEST_CHECK(ProjectSearchStart(&g_searchService, &request, 10, &operationId, &error));
    pollSymbolSearch(operationId, 10);
    TEST_CHECK(ProjectSearchOperationInfo(&g_searchService)->state == ProjectSearchState::Completed);
    TEST_CHECK(find("only", "src/only.cpp") != 0xFFFFFFFFu);
    TEST_CHECK(ProjectSearchRelease(&g_searchService, operationId));

    g_fileSystem = {};
    addDirectory(&g_fileSystem, "c:/empty");
    copyTestText(request.projectId, sizeof(request.projectId), "com.example.empty");
    copyTestText(request.rootPath, sizeof(request.rootPath), "C:/empty");
    request.projectGeneration = 4;
    request.fileSystem = makeSymbolFileSystem(&g_fileSystem);
    ProjectSymbolResultsInit(&g_results);
    TEST_CHECK(ProjectSearchStart(&g_searchService, &request, 20, &operationId, &error));
    pollSymbolSearch(operationId, 20);
    TEST_CHECK(ProjectSearchOperationInfo(&g_searchService)->state == ProjectSearchState::Completed);
    TEST_CHECK(g_results.count == 0 && g_results.totalRecognized == 0);
    TEST_CHECK(ProjectSearchRelease(&g_searchService, operationId));

    TEST_CHECK(ProjectSymbolsProjectIdentityMatches("com.example.symbols", 3,
        "C:/project", "COM.EXAMPLE.SYMBOLS", 3, "c:/project"));
    TEST_CHECK(!ProjectSymbolsProjectIdentityMatches("com.example.symbols", 3,
        "C:/project", "com.example.other", 3, "C:/project"));
    TEST_CHECK(!ProjectSymbolsProjectIdentityMatches("com.example.symbols", 3,
        "C:/project", "com.example.symbols", 4, "C:/project"));
    TEST_CHECK(!ProjectSymbolsProjectIdentityMatches("com.example.symbols", 3,
        "C:/project", "com.example.symbols", 3, "C:/outside"));
}

int main() {
    testMultiFileCategoriesFiltersAndOrdering();
    testCurrentContentsAndDuplicateIdentity();
    testEmptyUnsupportedAndCapacityBounds();
    testPathContainmentAndNoInputMutation();
    testProjectWalkerSnapshotsBoundsAndBuildRunIsolation();
    testEmptyProjectCancellationRescanAndIdentitySwitch();
    std::cout << "Developer Studio project symbols model PASS\n";
    return 0;
}
