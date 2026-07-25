#include "developer_studio_workspace.h"

#include <cassert>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>

#undef assert
#define assert(condition) do { if (!(condition)) { std::cerr << "workflow assertion failed: " << #condition << "\\n"; return 2; } } while (0)

using namespace guidexos::developer_studio;
namespace fs = std::filesystem;

static bool statFile(void*, const char* path, FileInfo* outInfo) {
    std::error_code ec;
    fs::file_status status = fs::symlink_status(fs::path(path), ec);
    if (ec || !fs::exists(status)) return false;
    outInfo->kind = fs::is_directory(status) ? FileInfoKind::Directory :
        (fs::is_regular_file(status) ? FileInfoKind::RegularFile : FileInfoKind::Unknown);
    outInfo->size = outInfo->kind == FileInfoKind::RegularFile ? fs::file_size(path, ec) : 0;
    return !ec;
}

static bool listFiles(void*, const char* path, FileListEntry* entries, uint32_t capacity, uint32_t* outCount, bool* outTruncated) {
    uint32_t count = 0;
    *outCount = 0;
    *outTruncated = false;
    std::error_code ec;
    for (const fs::directory_entry& item : fs::directory_iterator(fs::path(path), ec)) {
        if (ec) return false;
        if (count >= capacity) { *outTruncated = true; break; }
        const std::string name = item.path().filename().string();
        std::strncpy(entries[count].name, name.c_str(), sizeof(entries[count].name) - 1);
        entries[count].name[sizeof(entries[count].name) - 1] = '\0';
        entries[count].kind = item.is_directory(ec) ? FileInfoKind::Directory : FileInfoKind::RegularFile;
        entries[count].size = entries[count].kind == FileInfoKind::RegularFile ? item.file_size(ec) : 0;
        ++count;
    }
    *outCount = count;
    return true;
}

static bool readFile(void*, const char* path, char* buffer, uint32_t capacity, uint32_t* outBytes) {
    std::ifstream input(path, std::ios::binary);
    if (!input) return false;
    input.seekg(0, std::ios::end);
    std::streamoff size = input.tellg();
    if (size < 0 || static_cast<uint64_t>(size) > capacity) return false;
    input.seekg(0, std::ios::beg);
    input.read(buffer, size);
    if (!input && size > 0) return false;
    *outBytes = static_cast<uint32_t>(size);
    return true;
}

static bool writeFile(void*, const char* path, const char* buffer, uint32_t bytes, uint32_t* outBytes) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) return false;
    output.write(buffer, bytes);
    output.flush();
    if (!output) return false;
    *outBytes = bytes;
    return true;
}

static uint32_t usedDocuments(const WorkspaceModel& model) {
    uint32_t count = 0;
    for (uint32_t i = 0; i < kMaxOpenDocuments; ++i) if (model.documents[i].used) ++count;
    return count;
}

int main(int argc, char** argv) {
    std::cout << "workflow-start\n";
    assert(argc == 2);
    const fs::path root = fs::absolute(argv[1]);
    WorkspaceFileSystem fileSystem = { nullptr, statFile, listFiles, readFile, writeFile };
    static WorkspaceController controller;
    WorkspaceControllerInit(&controller, fileSystem);
    std::cout << "controller-ready\n";
    assert(WorkspaceControllerOpenWorkspace(&controller, root.string().c_str()));
    std::cout << "workspace-open\n";
    assert(controller.model.entryCount == 5);
    assert(controller.model.entries[0].kind == WorkspaceEntryKind::Directory);
    assert(WorkspaceControllerOpenDocument(&controller, "sample.cpp"));
    std::cout << "document-open\n";
    assert(usedDocuments(controller.model) == 1);
    assert(WorkspaceControllerOpenDocument(&controller, "sample.cpp"));
    assert(usedDocuments(controller.model) == 1);
    assert(controller.lastError == ModelErrorCode::DuplicateDocument);

    Document* document = WorkspaceControllerActiveDocument(&controller);
    assert(document);
    TextBufferEnd(&document->buffer);
    assert(TextBufferInsert(&document->buffer, "\n// saved", 9));
    assert(document->buffer.dirty);
    assert(WorkspaceControllerSaveActive(&controller));
    std::cout << "document-save\n";
    assert(!document->buffer.dirty);
    std::ifstream saved(root / "sample.cpp", std::ios::binary);
    std::string savedText((std::istreambuf_iterator<char>(saved)), std::istreambuf_iterator<char>());
    assert(savedText.find("// saved") != std::string::npos);

    assert(TextBufferInsert(&document->buffer, "cancel", 6));
    uint32_t documentIndex = controller.model.activeDocument;
    assert(!WorkspaceControllerCloseDocument(&controller, documentIndex, CloseDecision::Cancel));
    assert(controller.model.documents[documentIndex].used);
    assert(WorkspaceControllerCloseDocument(&controller, documentIndex, CloseDecision::Discard));
    assert(usedDocuments(controller.model) == 0);

    assert(WorkspaceControllerOpenDocument(&controller, "sample.cpp"));
    documentIndex = controller.model.activeDocument;
    document = WorkspaceControllerActiveDocument(&controller);
    assert(document);
    TextBufferEnd(&document->buffer);
    assert(TextBufferInsert(&document->buffer, "\n// close-save", 14));
    assert(WorkspaceControllerCloseDocument(&controller, documentIndex, CloseDecision::Save));
    assert(usedDocuments(controller.model) == 0);

    assert(!WorkspaceControllerOpenDocument(&controller, "binary.dat"));
    assert(controller.lastError == ModelErrorCode::UnsupportedFile);
    assert(!WorkspaceControllerOpenDocument(&controller, "oversized.txt"));
    assert(controller.lastError == ModelErrorCode::FileTooLarge);
    assert(WorkspaceControllerOpenDocument(&controller, "subdirectory/config.json"));
    assert(usedDocuments(controller.model) == 1);
    assert(WorkspaceControllerCloseWorkspace(&controller, CloseDecision::Discard));
    assert(!controller.model.open);
    std::cout << "Developer Studio workflow smoke PASS\n";
    return 0;
}
