#include "developer_studio_project_search.h"

#include <cassert>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>

using namespace guidexos::developer_studio;
namespace fs = std::filesystem;

static bool statFile(void*, const char* path, FileInfo* info) {
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

static bool readFile(void*, const char* path, char* buffer, uint32_t capacity, uint32_t* outBytes) {
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

    ProjectSearchOptionsInit(&request.options);
    setText(request.options.query, sizeof(request.options.query), "CreateWindow");
    setText(request.options.includePattern, sizeof(request.options.includePattern), "bad/../pattern");
    assert(!ProjectSearchStart(&service, &request, 0, &operationId, &error));
    assert(error == ProjectSearchErrorCode::InvalidIncludePattern);

    fs::remove_all(root, ec);
    std::cout << "Developer Studio project search PASS\n";
    return 0;
}
