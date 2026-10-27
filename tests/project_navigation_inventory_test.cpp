#include "developer_studio_build.h"
#include "developer_studio_run.h"
#include "developer_studio_tools.h"
#include "developer_studio_workspace.h"

#include "test_check.h"
#include <cstdio>
#include <cstring>
#include <iostream>

using namespace guidexos::developer_studio;

struct TestNode {
    char path[kMaxPathBytes];
    const char* bytes;
    uint32_t byteCount;
    uint64_t reportedSize;
    FileInfoKind kind;
    bool exists;
};

struct TestFileSystem {
    TestNode nodes[600];
    uint32_t count;
    bool includeTraversalEntry;
    bool outsideAccess;
};

static TestFileSystem g_fileSystem = {};

static bool pathEqual(const char* left, const char* right) {
    if (!left || !right) return left == right;
    uint32_t i = 0;
    while (left[i] && right[i]) {
        char a = left[i];
        char b = right[i];
        if (a >= 'A' && a <= 'Z') a = static_cast<char>(a + ('a' - 'A'));
        if (b >= 'A' && b <= 'Z') b = static_cast<char>(b + ('a' - 'A'));
        if (a != b) return false;
        ++i;
    }
    return left[i] == right[i];
}

static bool pathIsWithinProject(const char* path) {
    static const char root[] = "D:/project";
    const uint32_t length = static_cast<uint32_t>(sizeof(root) - 1);
    if (!path) return false;
    for (uint32_t i = 0; i < length; ++i) {
        char a = path[i];
        char b = root[i];
        if (a >= 'A' && a <= 'Z') a = static_cast<char>(a + ('a' - 'A'));
        if (b >= 'A' && b <= 'Z') b = static_cast<char>(b + ('a' - 'A'));
        if (a != b) return false;
    }
    return path[length] == '\0' || path[length] == '/';
}

static TestNode* findNode(TestFileSystem* fileSystem, const char* path) {
    if (!fileSystem || !path) return nullptr;
    for (uint32_t i = 0; i < fileSystem->count; ++i)
        if (fileSystem->nodes[i].exists && pathEqual(fileSystem->nodes[i].path, path))
            return &fileSystem->nodes[i];
    return nullptr;
}

static void addNode(const char* path, FileInfoKind kind, const char* bytes = nullptr,
                    uint32_t byteCount = 0, uint64_t reportedSize = 0) {
    TEST_CHECK(g_fileSystem.count < sizeof(g_fileSystem.nodes) / sizeof(g_fileSystem.nodes[0]));
    TestNode& node = g_fileSystem.nodes[g_fileSystem.count++];
    std::memset(&node, 0, sizeof(node));
    TEST_CHECK(std::strlen(path) < sizeof(node.path));
    std::strcpy(node.path, path);
    node.bytes = bytes;
    node.byteCount = byteCount;
    node.reportedSize = reportedSize != 0 ? reportedSize : byteCount;
    node.kind = kind;
    node.exists = true;
}

static void addDirectory(const char* path) {
    addNode(path, FileInfoKind::Directory);
}

static void addTextFile(const char* path, const char* content) {
    addNode(path, FileInfoKind::RegularFile, content,
            static_cast<uint32_t>(std::strlen(content)));
}

static bool parentAndName(const char* path, char* parent, uint32_t parentCapacity,
                          char* name, uint32_t nameCapacity) {
    if (!path || !parent || !name || parentCapacity == 0 || nameCapacity == 0) return false;
    uint32_t length = static_cast<uint32_t>(std::strlen(path));
    uint32_t separator = length;
    while (separator > 0 && path[separator - 1] != '/') --separator;
    if (separator == 0 || separator >= parentCapacity || length - separator + 1 > nameCapacity) return false;
    std::memcpy(parent, path, separator - 1);
    parent[separator - 1] = '\0';
    std::memcpy(name, path + separator, length - separator);
    name[length - separator] = '\0';
    return true;
}

static bool testStat(void* userData, const char* path, FileInfo* info) {
    TestFileSystem* fileSystem = static_cast<TestFileSystem*>(userData);
    if (!fileSystem || !path || !info) return false;
    if (!pathIsWithinProject(path)) fileSystem->outsideAccess = true;
    TestNode* node = findNode(fileSystem, path);
    if (!node) return false;
    info->kind = node->kind;
    info->size = node->reportedSize;
    return true;
}

static bool testList(void* userData, const char* path, FileListEntry* entries,
                     uint32_t capacity, uint32_t* count, bool* truncated) {
    TestFileSystem* fileSystem = static_cast<TestFileSystem*>(userData);
    if (!fileSystem || !path || !entries || capacity == 0 || !count || !truncated) return false;
    if (!pathIsWithinProject(path)) fileSystem->outsideAccess = true;
    TestNode* directory = findNode(fileSystem, path);
    if (!directory || directory->kind != FileInfoKind::Directory) return false;
    *count = 0;
    *truncated = false;
    for (uint32_t i = 0; i < fileSystem->count; ++i) {
        TestNode& node = fileSystem->nodes[i];
        if (!node.exists || pathEqual(node.path, path)) continue;
        char parent[kMaxPathBytes] = {};
        char name[kMaxNameBytes] = {};
        if (!parentAndName(node.path, parent, sizeof(parent), name, sizeof(name)) ||
            !pathEqual(parent, path)) continue;
        if (*count >= capacity) {
            *truncated = true;
            continue;
        }
        FileListEntry& entry = entries[(*count)++];
        std::memset(&entry, 0, sizeof(entry));
        std::strcpy(entry.name, name);
        entry.kind = node.kind;
        entry.size = node.reportedSize;
    }
    if (fileSystem->includeTraversalEntry && pathEqual(path, "D:/project") && *count < capacity) {
        FileListEntry& entry = entries[(*count)++];
        std::memset(&entry, 0, sizeof(entry));
        std::strcpy(entry.name, "../escape.md");
        entry.kind = FileInfoKind::RegularFile;
        entry.size = 12;
    }
    return true;
}

static bool testRead(void* userData, const char* path, char* buffer,
                     uint32_t capacity, uint32_t* count) {
    TestFileSystem* fileSystem = static_cast<TestFileSystem*>(userData);
    if (!fileSystem || !path || !buffer || !count) return false;
    if (!pathIsWithinProject(path)) fileSystem->outsideAccess = true;
    TestNode* node = findNode(fileSystem, path);
    if (!node || node->kind != FileInfoKind::RegularFile || node->byteCount > capacity) return false;
    if (node->byteCount != 0) std::memcpy(buffer, node->bytes, node->byteCount);
    *count = node->byteCount;
    return true;
}

static WorkspaceFileSystem makeFileSystem() {
    WorkspaceFileSystem fileSystem = {};
    fileSystem.userData = &g_fileSystem;
    fileSystem.stat = testStat;
    fileSystem.list = testList;
    fileSystem.read = testRead;
    return fileSystem;
}

static Project makeProject() {
    Project project = {};
    project.valid = true;
    project.formatVersion = 1;
    project.kind = ProjectKind::NativeGuiApplication;
    std::strcpy(project.projectId, "com.example.navigation.assets");
    std::strcpy(project.displayName, "Navigation Assets");
    std::strcpy(project.rootPath, "D:/project");
    std::strcpy(project.manifestPath, "app/app.json");
    std::strcpy(project.targetProfileId, "guidexos.amd64.hosted.native");
    std::strcpy(project.sourceRoot, "src");
    std::strcpy(project.entryPoint, "gx_main");
    std::strcpy(project.abi, "guidexos-c-abi-v1");
    std::strcpy(project.architecture, "amd64");
    std::strcpy(project.outputName, "navigation-assets");
    project.hasSourceFiles = true;
    project.sourceFileCount = 1;
    std::strcpy(project.sourceFiles[0], "main.cpp");
    return project;
}

static void makeFixture() {
    g_fileSystem = {};
    addDirectory("D:/project");
    addDirectory("D:/project/src");
    addDirectory("D:/project/docs");
    addDirectory("D:/project/config");
    addDirectory("D:/project/build");
    addDirectory("D:/project/node_modules");
    addNode("D:/project/linked.md", FileInfoKind::Unknown);
    addTextFile("D:/project/README.md", "Project notes\n");
    addTextFile("D:/project/CMakeLists.txt", "project(navigation)\n");
    addTextFile("D:/project/src/main.cpp", "int main() { return 0; }\n");
    addTextFile("D:/project/docs/guide.md", "Guide\n");
    addTextFile("D:/project/docs/removed.yaml", "will be deleted\n");
    addTextFile("D:/project/config/app.json", "{\"ok\": true}\n");
    addTextFile("D:/project/config/settings.yaml", "name: test\n");
    addTextFile("D:/project/config/options.toml", "name = 'test'\n");
    addTextFile("D:/project/.env", "MODE=test\n");
    addTextFile("D:/project/notes.pdf", "not a supported editor format");
    static const char binaryContent[] = { '{', '\0', '}', '\n' };
    addNode("D:/project/config/binary.json", FileInfoKind::RegularFile,
            binaryContent, sizeof(binaryContent));
    static const char invalidUtf8[] = { static_cast<char>(0xC0), static_cast<char>(0xAF) };
    addNode("D:/project/docs/invalid-utf8.md", FileInfoKind::RegularFile,
            invalidUtf8, sizeof(invalidUtf8));
    addNode("D:/project/docs/oversized.md", FileInfoKind::RegularFile,
            "x", 1, static_cast<uint64_t>(kMaxEditorBytes) + 1u);
    addTextFile("D:/project/build/generated.md", "generated\n");
    addTextFile("D:/project/node_modules/library/readme.md", "dependency\n");
}

static void testInventoryAndSafety() {
    makeFixture();
    g_fileSystem.includeTraversalEntry = true;
    WorkspaceFileSystem fileSystem = makeFileSystem();
    static WorkspaceController controller = {};
    WorkspaceControllerInit(&controller, fileSystem);
    controller.model.open = true;
    controller.model.hasProject = true;
    controller.model.project = makeProject();
    std::strcpy(controller.model.rootPath, controller.model.project.rootPath);

    ProjectNavigationFile files[kMaxProjectNavigationFiles] = {};
    ProjectNavigationInventory inventory = {};
    TEST_CHECK(WorkspaceControllerEnumerateProjectNavigationFiles(
        &controller, files, kMaxProjectNavigationFiles, &inventory));
    TEST_CHECK(inventory.fileCount == 8);
    TEST_CHECK(inventory.truncated);
    TEST_CHECK(inventory.skippedDirectories == 2);
    TEST_CHECK(inventory.unsupportedFiles == 1);
    TEST_CHECK(inventory.unsupportedEntries == 2);
    TEST_CHECK(inventory.binaryFiles == 2);
    TEST_CHECK(inventory.oversizedFiles == 1);
    TEST_CHECK(!g_fileSystem.outsideAccess);

    bool foundMarkdown = false;
    bool foundJson = false;
    bool foundYaml = false;
    bool foundToml = false;
    bool foundCMake = false;
    bool foundEnv = false;
    bool foundCpp = false;
    for (uint32_t i = 0; i < inventory.fileCount; ++i) {
        const char* path = files[i].absolutePath;
        foundMarkdown = foundMarkdown || std::strstr(path, "/README.md") != nullptr ||
            std::strstr(path, "/docs/guide.md") != nullptr;
        foundJson = foundJson || std::strstr(path, "/config/app.json") != nullptr;
        foundYaml = foundYaml || std::strstr(path, "/config/settings.yaml") != nullptr;
        foundToml = foundToml || std::strstr(path, "/config/options.toml") != nullptr;
        foundCMake = foundCMake || std::strstr(path, "/CMakeLists.txt") != nullptr;
        foundEnv = foundEnv || std::strstr(path, "/.env") != nullptr;
        foundCpp = foundCpp || std::strstr(path, ".cpp") != nullptr;
    }
    TEST_CHECK(foundMarkdown && foundJson && foundYaml && foundToml && foundCMake && foundEnv);
    TEST_CHECK(!foundCpp);

    const char* searchPaths[kDeveloperToolMaxIndexedFiles] = {};
    for (uint32_t i = 0; i < inventory.fileCount; ++i) searchPaths[i] = files[i].absolutePath;
    DeveloperToolFileSearchResults results = {};
    TEST_CHECK(DeveloperToolBuildFileSearchResults("D:/project", searchPaths,
        inventory.fileCount, "guide", &results));
    TEST_CHECK(results.count == 1 && std::strcmp(results.results[0].relativePath, "docs/guide.md") == 0);

    ProjectSourceFile sources[kMaxProjectSources] = {};
    uint32_t sourceCount = 0;
    TEST_CHECK(WorkspaceControllerEnumerateProjectSources(&controller, sources,
        kMaxProjectSources, &sourceCount));
    TEST_CHECK(sourceCount == 1 && std::strcmp(sources[0].relativePath, "src/main.cpp") == 0);

    BuildRequest buildRequest = {};
    BuildErrorCode buildError = BuildErrorCode::None;
    TEST_CHECK(BuildRequestFromProject(controller.model.project, &buildRequest, &buildError));
    TEST_CHECK(buildRequest.sourceCount == 1 &&
               std::strcmp(buildRequest.sourceFiles[0], "src/main.cpp") == 0);
    BuildResult build = {};
    build.state = BuildState::Succeeded;
    build.artifactValid = true;
    build.artifactEntryPoint = true;
    build.sourceMembershipHash = BuildRequestSourceMembershipHash(buildRequest);
    std::strcpy(build.projectId, controller.model.project.projectId);
    std::strcpy(build.projectRoot, controller.model.project.rootPath);
    std::strcpy(build.targetProfile, controller.model.project.targetProfileId);
    std::strcpy(build.configuration, buildRequest.configuration);
    std::strcpy(build.artifactPath, buildRequest.expectedArtifact);
    std::strcpy(build.artifactArchitecture, "amd64");
    std::strcpy(build.artifactSha256,
        "0123456789012345678901234567890123456789012345678901234567890123");
    RunRequest runRequest = {};
    RunErrorCode runError = RunErrorCode::None;
    TEST_CHECK(RunRequestFromBuild(controller.model.project, build, &runRequest, &runError));
    TEST_CHECK(runRequest.artifactPath[0] != '\0');

    uint32_t documentIndex = kMaxOpenDocuments;
    OutputErrorCode openError = OutputErrorCode::None;
    const bool openedReadme = WorkspaceControllerOpenDocumentAtLocation(
        &controller, controller.model.project.projectId, "README.md", 1, 1,
        &documentIndex, &openError);
    TEST_CHECK(openedReadme);
    Document* active = WorkspaceControllerActiveDocument(&controller);
    TEST_CHECK(active && documentIndex == controller.model.activeDocument);
    TEST_CHECK(WorkspaceControllerOpenDocumentAtLocation(
        &controller, controller.model.project.projectId, "config/options.toml", 1, 1,
        &documentIndex, &openError));
    active = WorkspaceControllerActiveDocument(&controller);
    TEST_CHECK(active && std::strstr(active->path, ".toml") != nullptr);
    active->buffer.caret = 3;
    active->buffer.selectionAnchor = 1;
    active->buffer.selectionActive = true;
    active->buffer.dirty = true;
    const uint64_t activeDocumentId = active->documentId;
    const uint32_t activeCaret = active->buffer.caret;
    const uint32_t activeIndex = controller.model.activeDocument;

    TEST_CHECK(!WorkspaceControllerOpenDocumentAtLocation(
        &controller, controller.model.project.projectId, "config/binary.json", 1, 1,
        &documentIndex, &openError));
    TEST_CHECK(controller.lastError == ModelErrorCode::BinaryFile);
    TEST_CHECK(controller.model.activeDocument == activeIndex && active->documentId == activeDocumentId &&
               active->buffer.caret == activeCaret && active->buffer.selectionActive && active->buffer.dirty);
    TEST_CHECK(!WorkspaceControllerOpenDocumentAtLocation(
        &controller, controller.model.project.projectId, "notes.pdf", 1, 1,
        &documentIndex, &openError));
    TEST_CHECK(controller.lastError == ModelErrorCode::UnsupportedFile);
    TEST_CHECK(!WorkspaceControllerOpenDocumentAtLocation(
        &controller, controller.model.project.projectId, "../escape.md", 1, 1,
        &documentIndex, &openError));
    TEST_CHECK(openError == OutputErrorCode::DiagnosticPathOutsideProject);
    TEST_CHECK(!g_fileSystem.outsideAccess);

    TestNode* deleted = findNode(&g_fileSystem, "D:/project/docs/removed.yaml");
    TEST_CHECK(deleted != nullptr);
    deleted->exists = false;
    TEST_CHECK(!WorkspaceControllerOpenDocumentAtLocation(
        &controller, controller.model.project.projectId, "docs/removed.yaml", 1, 1,
        &documentIndex, &openError));
    TEST_CHECK(active->documentId == activeDocumentId && active->buffer.caret == activeCaret &&
               active->buffer.selectionActive && active->buffer.dirty);

    const char validUtf8[] = "snowman \xE2\x98\x83\n";
    TEST_CHECK(!LooksBinary(validUtf8, static_cast<uint32_t>(sizeof(validUtf8) - 1)));
    TEST_CHECK(LooksBinary("bad\x1B", 4));
    TEST_CHECK(LooksBinary("\x7F", 1));
    const char c1Control[] = { static_cast<char>(0xC2), static_cast<char>(0x85) };
    TEST_CHECK(LooksBinary(c1Control, sizeof(c1Control)));
}

static void testInventoryFileAndDirectoryBounds() {
    g_fileSystem = {};
    addDirectory("D:/project");
    addDirectory("D:/project/assets0");
    addDirectory("D:/project/assets1");
    addDirectory("D:/project/assets2");
    for (uint32_t i = 0; i < kMaxProjectNavigationFiles + 4u; ++i) {
        char path[kMaxPathBytes] = {};
        std::snprintf(path, sizeof(path), "D:/project/assets%u/file%03u.md", i / 90u, i);
        addTextFile(path, "text\n");
    }
    WorkspaceFileSystem fileSystem = makeFileSystem();
    static WorkspaceController controller = {};
    WorkspaceControllerInit(&controller, fileSystem);
    controller.model.open = true;
    controller.model.hasProject = true;
    controller.model.project = makeProject();
    std::strcpy(controller.model.rootPath, controller.model.project.rootPath);
    ProjectNavigationFile files[kMaxProjectNavigationFiles] = {};
    ProjectNavigationInventory inventory = {};
    TEST_CHECK(WorkspaceControllerEnumerateProjectNavigationFiles(
        &controller, files, kMaxProjectNavigationFiles, &inventory));
    TEST_CHECK(inventory.fileCount == kMaxProjectNavigationFiles);
    TEST_CHECK(inventory.truncated);

    TEST_CHECK(!WorkspaceControllerEnumerateProjectNavigationFiles(
        &controller, files, kMaxProjectNavigationFiles + 1u, &inventory));
    TEST_CHECK(!WorkspaceControllerEnumerateProjectNavigationFiles(
        &controller, files, 0, &inventory));

    g_fileSystem = {};
    addDirectory("D:/project");
    for (uint32_t i = 0; i < kMaxProjectNavigationDirectories + 4u; ++i) {
        char path[kMaxPathBytes] = {};
        std::snprintf(path, sizeof(path), "D:/project/folder%03u", i);
        addDirectory(path);
    }
    WorkspaceControllerInit(&controller, makeFileSystem());
    controller.model.open = true;
    controller.model.hasProject = true;
    controller.model.project = makeProject();
    std::strcpy(controller.model.rootPath, controller.model.project.rootPath);
    inventory = {};
    TEST_CHECK(WorkspaceControllerEnumerateProjectNavigationFiles(
        &controller, files, kMaxProjectNavigationFiles, &inventory));
    TEST_CHECK(inventory.directoriesVisited == kMaxProjectNavigationDirectories);
    TEST_CHECK(inventory.truncated);
}

int main() {
    testInventoryAndSafety();
    testInventoryFileAndDirectoryBounds();
    std::cout << "Developer Studio project navigation inventory PASS\n";
    return 0;
}
