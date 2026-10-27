#include "developer_studio_project_search.h"
#include "developer_studio_workspace.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

using namespace guidexos::developer_studio;
namespace fs = std::filesystem;

struct SearchFileSystemBehavior {
    std::string unreadablePath;
    std::string disappearedPath;
};

static bool pathEndsWith(const char* path, const std::string& suffix) {
    if (!path || suffix.empty()) return false;
    const uint32_t length = static_cast<uint32_t>(std::strlen(path));
    if (length < suffix.size()) return false;
    const char* name = path + length;
    while (name > path && name[-1] != '/' && name[-1] != '\\') --name;
    return std::strcmp(name, suffix.c_str()) == 0;
}

static bool statFile(void* userData, const char* path, FileInfo* info) {
    SearchFileSystemBehavior* behavior = static_cast<SearchFileSystemBehavior*>(userData);
    if (behavior && pathEndsWith(path, behavior->disappearedPath)) return false;
    std::error_code ec;
    const fs::file_status status = fs::symlink_status(fs::path(path), ec);
    if (ec || !fs::exists(status)) return false;
    info->kind = fs::is_directory(status) ? FileInfoKind::Directory :
        (fs::is_regular_file(status) ? FileInfoKind::RegularFile : FileInfoKind::Unknown);
    info->size = info->kind == FileInfoKind::RegularFile ? fs::file_size(path, ec) : 0;
    return !ec;
}

static bool listFiles(void*, const char* path, FileListEntry* entries, uint32_t capacity,
                      uint32_t* outCount, bool* outTruncated) {
    *outCount = 0;
    *outTruncated = false;
    std::error_code ec;
    for (const fs::directory_entry& item : fs::directory_iterator(fs::path(path), ec)) {
        if (ec) return false;
        if (*outCount >= capacity) { *outTruncated = true; break; }
        const fs::file_status status = item.symlink_status(ec);
        if (ec) return false;
        const std::string name = item.path().filename().string();
        std::strncpy(entries[*outCount].name, name.c_str(), sizeof(entries[*outCount].name) - 1);
        entries[*outCount].name[sizeof(entries[*outCount].name) - 1] = '\0';
        entries[*outCount].kind = fs::is_directory(status) ? FileInfoKind::Directory :
            (fs::is_regular_file(status) ? FileInfoKind::RegularFile : FileInfoKind::Unknown);
        entries[*outCount].size = entries[*outCount].kind == FileInfoKind::RegularFile ? item.file_size(ec) : 0;
        ++(*outCount);
    }
    return true;
}

static bool readFile(void* userData, const char* path, char* buffer, uint32_t capacity, uint32_t* outBytes) {
    SearchFileSystemBehavior* behavior = static_cast<SearchFileSystemBehavior*>(userData);
    if (behavior && pathEndsWith(path, behavior->unreadablePath)) return false;
    std::ifstream input(path, std::ios::binary);
    if (!input) return false;
    input.seekg(0, std::ios::end);
    const std::streamoff size = input.tellg();
    if (size < 0 || static_cast<uint64_t>(size) > capacity) return false;
    input.seekg(0, std::ios::beg);
    input.read(buffer, size);
    if (!input && size > 0) return false;
    *outBytes = static_cast<uint32_t>(size);
    return true;
}

static void writeFile(const fs::path& path, const char* contents) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output << contents;
}

static void setText(char* output, uint32_t capacity, const char* value) {
    std::strncpy(output, value, capacity - 1);
    output[capacity - 1] = '\0';
}

static ProjectSearchRequest requestFor(const fs::path& root, const char* query) {
    ProjectSearchRequest request = {};
    setText(request.projectId, sizeof(request.projectId), "com.example.search");
    request.projectGeneration = 7;
    setText(request.rootPath, sizeof(request.rootPath), root.string().c_str());
    ProjectSearchOptionsInit(&request.options);
    setText(request.options.query, sizeof(request.options.query), query);
    request.fileSystem = { nullptr, statFile, listFiles, readFile, nullptr, nullptr, nullptr };
    return request;
}

int main() {
    const fs::path root = fs::temp_directory_path() / "guidexos-project-search-test";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root / "src", ec);
    fs::create_directories(root / "build", ec);
    fs::create_directories(root / "third_party", ec);
    fs::create_directories(root / "rebuild_source", ec);
    writeFile(root / "z.cpp", "Foo foo FOO\n");
    writeFile(root / "a.cpp", "prefix CreateWindow suffix\n");
    writeFile(root / "src" / "main.cpp", "CreateWindow on disk\n");
    writeFile(root / "src" / "notes.txt", "CreateWindow text\n");
    writeFile(root / "build" / "output.cpp", "CreateWindow should be excluded\n");
    writeFile(root / "third_party" / "vendor.cpp", "CreateWindow vendor\n");
    writeFile(root / "rebuild_source" / "rebuilt.cpp", "CreateWindow from a legitimate source directory\n");
    { std::ofstream binary(root / "binary.cpp", std::ios::binary); binary.write("Create\0Window", 13); }

    static ProjectSearchService service;
    ProjectSearchServiceInit(&service);
    ProjectSearchRequest request = requestFor(root, "createwindow");
    setText(request.options.excludePattern, sizeof(request.options.excludePattern), "third_party/*");
    uint64_t operationId = 0;
    ProjectSearchErrorCode error = ProjectSearchErrorCode::None;
    assert(ProjectSearchStart(&service, &request, 0, &operationId, &error));
    for (uint32_t i = 0; i < 1000 && ProjectSearchIsActive(&service); ++i)
        assert(ProjectSearchPoll(&service, operationId, 16, i));
    const ProjectSearchOperation* operation = ProjectSearchOperationInfo(&service);
    assert(operation && operation->state == ProjectSearchState::Completed);
    assert(operation->resultFileCount == 3);
    assert(operation->resultMatchCount == 3);
    assert(std::strcmp(ProjectSearchResultGroupAt(&service, 0)->relativePath, "a.cpp") == 0);
    assert(std::strcmp(ProjectSearchResultGroupAt(&service, 1)->relativePath, "rebuild_source/rebuilt.cpp") == 0);
    assert(std::strcmp(ProjectSearchResultGroupAt(&service, 2)->relativePath, "src/main.cpp") == 0);
    const ProjectSearchMatch* first = ProjectSearchResultMatchAt(&service, ProjectSearchResultGroupAt(&service, 0), 0);
    assert(first && first->line == 1 && first->column == 8 && first->byteOffset == 7);
    assert(first->previewMatchLength == 12);

    WorkspaceFileSystem navigationFileSystem = { nullptr, statFile, nullptr, readFile,
        nullptr, nullptr, nullptr };
    static WorkspaceController navigationController;
    WorkspaceControllerInit(&navigationController, navigationFileSystem);
    navigationController.model.open = true;
    navigationController.model.hasProject = true;
    setText(navigationController.model.rootPath, sizeof(navigationController.model.rootPath), root.generic_string().c_str());
    setText(navigationController.model.project.projectId,
            sizeof(navigationController.model.project.projectId), "com.example.search");
    navigationController.model.project.valid = true;
    uint32_t navigationIndex = kMaxOpenDocuments;
    OutputErrorCode navigationError = OutputErrorCode::None;
    const ProjectSearchFileGroup* navigationGroup = ProjectSearchResultGroupAt(&service, 2);
    const ProjectSearchMatch* navigationMatch = ProjectSearchResultMatchAt(&service, navigationGroup, 0);
    assert(navigationGroup && navigationMatch);
    const bool navigatedToSearchResult = WorkspaceControllerOpenDocumentAtLocation(&navigationController,
        navigationController.model.project.projectId, navigationGroup->relativePath,
        navigationMatch->line, navigationMatch->column, &navigationIndex, &navigationError);
    if (!navigatedToSearchResult)
        std::cerr << "Search result navigation failed: error=" << static_cast<int>(navigationError)
                  << " modelError=" << static_cast<int>(navigationController.lastError)
                  << " root=" << navigationController.model.rootPath
                  << " result=" << navigationGroup->relativePath << '\n';
    assert(navigatedToSearchResult);
    const Document* navigated = WorkspaceControllerActiveDocumentConst(&navigationController);
    assert(navigated && std::strstr(navigated->path, "/src/main.cpp") != nullptr);
    assert(navigated->buffer.caret == navigationMatch->byteOffset);

    ProjectSearchOptionsInit(&request.options);
    setText(request.options.query, sizeof(request.options.query), "CreateWindow");
    setText(request.options.includePattern, sizeof(request.options.includePattern), "src/*.cpp;*.txt");
    setText(request.options.excludePattern, sizeof(request.options.excludePattern), "src/main.cpp");
    assert(ProjectSearchStart(&service, &request, 0, &operationId, &error));
    for (uint32_t i = 0; i < 1000 && ProjectSearchIsActive(&service); ++i) ProjectSearchPoll(&service, operationId, 16, i);
    operation = ProjectSearchOperationInfo(&service);
    assert(operation->state == ProjectSearchState::Completed);
    assert(operation->resultFileCount == 1);
    assert(std::strcmp(ProjectSearchResultGroupAt(&service, 0)->relativePath, "src/notes.txt") == 0);

    ProjectSearchOptionsInit(&request.options);
    setText(request.options.query, sizeof(request.options.query), "UnsavedSymbol");
    ProjectSearchDocumentSnapshot snapshot = {};
    setText(snapshot.relativePath, sizeof(snapshot.relativePath), "src/main.cpp");
    setText(snapshot.data, sizeof(snapshot.data), "UnsavedSymbol only in memory\n");
    snapshot.length = static_cast<uint32_t>(std::strlen(snapshot.data));
    snapshot.documentId = 42;
    snapshot.documentGeneration = 9;
    request.dirtyDocuments = &snapshot;
    request.dirtyDocumentCount = 1;
    assert(ProjectSearchStart(&service, &request, 0, &operationId, &error));
    for (uint32_t i = 0; i < 1000 && ProjectSearchIsActive(&service); ++i) ProjectSearchPoll(&service, operationId, 16, i);
    operation = ProjectSearchOperationInfo(&service);
    assert(operation->resultFileCount == 1);
    const ProjectSearchFileGroup* dirtyGroup = ProjectSearchResultGroupAt(&service, 0);
    assert(dirtyGroup && dirtyGroup->sourceKind == ProjectSearchSourceKind::DirtyDocument);
    assert(dirtyGroup->documentId == 42 && dirtyGroup->documentGeneration == 9);

    assert(ProjectSearchCancel(&service, operationId) == false); // completed cancel is harmless and rejected
    assert(ProjectSearchRelease(&service, operationId));
    assert(!ProjectSearchPoll(&service, operationId, 16, 1));

    request.dirtyDocuments = nullptr;
    request.dirtyDocumentCount = 0;
    setText(request.options.query, sizeof(request.options.query), "CreateWindow");
    assert(ProjectSearchStart(&service, &request, 0, &operationId, &error));
    const uint64_t oldOperation = operationId;
    assert(ProjectSearchStart(&service, &request, 0, &operationId, &error));
    assert(operationId != oldOperation);
    assert(!ProjectSearchPoll(&service, oldOperation, 16, 1));
    assert(ProjectSearchCancel(&service, operationId));
    assert(ProjectSearchPoll(&service, operationId, 16, 1));
    assert(ProjectSearchOperationInfo(&service)->state == ProjectSearchState::Cancelled);
    assert(ProjectSearchOperationInfo(&service)->error == ProjectSearchErrorCode::Cancelled);
    assert(ProjectSearchRelease(&service, operationId));
    assert(ProjectSearchQueryResultGroups(&service) == 0);
    assert(ProjectSearchOperationInfo(&service)->state == ProjectSearchState::Idle);

    ProjectSearchOptionsInit(&request.options);
    setText(request.options.query, sizeof(request.options.query), "CreateWindow");
    setText(request.options.includePattern, sizeof(request.options.includePattern), "bad/../pattern");
    assert(!ProjectSearchStart(&service, &request, 0, &operationId, &error));
    assert(error == ProjectSearchErrorCode::InvalidIncludePattern);
    assert(ProjectSearchRelease(&service, operationId));

    const fs::path oversizedPath = root / "oversized.cpp";
    const fs::path unreadablePath = root / "unreadable.cpp";
    const fs::path disappearedPath = root / "disappeared.cpp";
    std::string oversizedContents(kProjectSearchMaxFileBytes + 1u, 'x');
    oversizedContents.replace(0, 12, "CreateWindow");
    { std::ofstream output(oversizedPath, std::ios::binary); output.write(oversizedContents.data(), oversizedContents.size()); }
    writeFile(unreadablePath, "CreateWindow hidden by the test filesystem\n");
    writeFile(disappearedPath, "CreateWindow disappears after enumeration\n");
    SearchFileSystemBehavior behavior = { "unreadable.cpp", "disappeared.cpp" };
    ProjectSearchOptionsInit(&request.options);
    setText(request.options.query, sizeof(request.options.query), "CreateWindow");
    setText(request.options.excludePattern, sizeof(request.options.excludePattern), "third_party/*");
    request.fileSystem.userData = &behavior;
    assert(ProjectSearchStart(&service, &request, 0, &operationId, &error));
    for (uint32_t i = 0; i < 1000 && ProjectSearchIsActive(&service); ++i)
        assert(ProjectSearchPoll(&service, operationId, 16, i));
    operation = ProjectSearchOperationInfo(&service);
    assert(operation && operation->state == ProjectSearchState::Completed && !operation->truncated);
    assert(operation->diagnostics.filesSkipped == 4);
    assert(operation->diagnostics.unreadableFiles == 1);
    assert(operation->diagnostics.binaryFiles == 1);
    assert(operation->diagnostics.oversizedFiles == 1);
    assert(operation->diagnostics.changedOrDisappearedFiles == 1);

    const fs::path capacityRoot = fs::temp_directory_path() / "guidexos-project-search-capacity-test";
    fs::remove_all(capacityRoot, ec);
    fs::create_directories(capacityRoot, ec);
    const uint32_t filesPerDirectory = 80;
    for (uint32_t directoryIndex = 0; directoryIndex <= kProjectSearchMaxResultFiles / filesPerDirectory; ++directoryIndex) {
        char directoryName[24] = {};
        std::snprintf(directoryName, sizeof(directoryName), "set-%02u", directoryIndex);
        fs::create_directories(capacityRoot / directoryName, ec);
    }
    for (uint32_t i = 0; i <= kProjectSearchMaxResultFiles; ++i) {
        char fileName[32] = {};
        char directoryName[24] = {};
        std::snprintf(fileName, sizeof(fileName), "match-%03u.cpp", i);
        std::snprintf(directoryName, sizeof(directoryName), "set-%02u", i / filesPerDirectory);
        writeFile(capacityRoot / directoryName / fileName, "Needle\n");
    }
    request = requestFor(capacityRoot, "Needle");
    assert(ProjectSearchStart(&service, &request, 0, &operationId, &error));
    for (uint32_t i = 0; i < 1000 && ProjectSearchIsActive(&service); ++i)
        assert(ProjectSearchPoll(&service, operationId, 16, i));
    operation = ProjectSearchOperationInfo(&service);
    assert(operation && operation->state == ProjectSearchState::Completed && operation->truncated);
    assert(operation->error == ProjectSearchErrorCode::ResultLimit);
    assert(operation->resultFileCount == kProjectSearchMaxResultFiles);
    assert(operation->resultMatchCount == kProjectSearchMaxResultFiles);
    assert(ProjectSearchRelease(&service, operationId));
    fs::remove_all(capacityRoot, ec);

    fs::remove_all(root, ec);
    std::cout << "Developer Studio project search PASS\n";
    return 0;
}
