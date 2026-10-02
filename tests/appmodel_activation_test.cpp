#include "developer_studio_workspace.h"
#include "test_check.h"
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

using namespace guidexos::developer_studio;
namespace fs = std::filesystem;
struct FsContext { bool failWrite = false; };
static WorkspaceController firstController, secondController;
static uint32_t checks = 0;
#define CHECK_A(x) do { ++checks; TEST_CHECK(x); } while (false)

static bool statPath(void*, const char* path, FileInfo* out) {
    if (!path || !out) return false;
    std::error_code ec; const fs::file_status status = fs::status(path, ec);
    if (ec || !fs::exists(status)) return false;
    if (fs::is_directory(status)) { out->kind = FileInfoKind::Directory; out->size = 0; return true; }
    if (!fs::is_regular_file(status)) return false;
    out->kind = FileInfoKind::RegularFile; out->size = fs::file_size(path, ec); return !ec;
}
static bool listPath(void*, const char* path, FileListEntry* entries, uint32_t cap, uint32_t* count, bool* truncated) {
    if (!path || !entries || !count || !truncated) return false;
    *count = 0; *truncated = false; std::error_code ec;
    fs::directory_iterator it(path, ec); if (ec) return false;
    for (const auto& item : it) {
        if (*count == cap) { *truncated = true; break; }
        FileListEntry& e = entries[*count]; e = {};
        const std::string name = item.path().filename().string();
        if (name.empty() || name.size() >= sizeof(e.name)) continue;
        std::memcpy(e.name, name.data(), name.size());
        e.kind = item.is_directory(ec) ? FileInfoKind::Directory : FileInfoKind::RegularFile;
        if (ec) { ec.clear(); continue; }
        e.size = e.kind == FileInfoKind::RegularFile ? item.file_size(ec) : 0;
        if (ec) { ec.clear(); e.size = 0; }
        ++*count;
    }
    return true;
}
static bool readPath(void*, const char* path, char* buffer, uint32_t cap, uint32_t* bytes) {
    if (!path || !buffer || !bytes) return false;
    *bytes = 0; std::error_code ec; const uintmax_t size = fs::file_size(path, ec);
    if (ec || size > cap) return false;
    std::ifstream in(path, std::ios::binary); if (!in) return false;
    if (size) { in.read(buffer, static_cast<std::streamsize>(size)); if (!in || static_cast<uintmax_t>(in.gcount()) != size) return false; }
    *bytes = static_cast<uint32_t>(size); return true;
}
static bool writePath(void* user, const char* path, const char* data, uint32_t size, uint32_t* written) {
    FsContext* ctx = static_cast<FsContext*>(user); if (written) *written = 0;
    if (!ctx || ctx->failWrite || !path || (!data && size) || !written) return false;
    std::ofstream out(path, std::ios::binary | std::ios::trunc); if (!out) return false;
    if (size) out.write(data, size);
    out.flush();
    if (!out) return false;
    *written = size;
    return true;
}
static void put(const fs::path& path, const std::string& data) {
    fs::create_directories(path.parent_path()); std::ofstream out(path, std::ios::binary | std::ios::trunc);
    TEST_CHECK(static_cast<bool>(out)); if (!data.empty()) out.write(data.data(), data.size()); out.flush(); TEST_CHECK(static_cast<bool>(out));
}
static std::string get(const fs::path& path) {
    std::ifstream in(path, std::ios::binary); return std::string(std::istreambuf_iterator<char>(in), {});
}

int main() {
    CHECK_A(IsAppModelDocumentPathSupported("/work/MAIN.CPP"));
    CHECK_A(IsAppModelDocumentPathSupported("C:\\work\\HEADER.HPP"));
    CHECK_A(IsAppModelDocumentPathSupported("/work/notes.TXT"));
    CHECK_A(!IsAppModelDocumentPathSupported("/work/a.cs"));
    CHECK_A(!IsAppModelDocumentPathSupported("/work/a.s"));
    CHECK_A(!IsAppModelDocumentPathSupported("/work/a.ini"));
    CHECK_A(!IsAppModelDocumentPathSupported("/work/README"));
    CHECK_A(!IsAppModelDocumentPathSupported("/work/a."));
    CHECK_A(!IsAppModelDocumentPathSupported("/work/../a.txt"));
    CHECK_A(!WorkspaceControllerOpenStandaloneDocument(nullptr, "/work/a.cpp"));

    const fs::path root = fs::temp_directory_path() / ("gxos-ds-appmodel-" + std::to_string(reinterpret_cast<uintptr_t>(&firstController)));
    std::error_code ec; fs::remove_all(root, ec);
    const fs::path nested = root / "nested" / "source files";
    const fs::path crlfPath = nested / "Mixed Case.TXT", secondPath = nested / "second instance.TXT";
    const fs::path cppPath = nested / "main.cpp", emptyPath = nested / "empty.txt";
    const fs::path nearPath = nested / "near.cpp", overPath = nested / "over.cpp";
    const std::string crlf = "one\r\ntwo\r\n", lf = "one\ntwo\n", near(kMaxEditorBytes, 'n');
    put(crlfPath, crlf); put(secondPath, crlf); put(cppPath, "int main() {}\n"); put(emptyPath, "");
    put(nested / "lf.txt", lf); put(nearPath, near); put(overPath, std::string(kMaxEditorBytes + 1u, 'x'));
    FsContext context; const WorkspaceFileSystem fsys = { &context, statPath, listPath, readPath, writePath, nullptr, nullptr };
    WorkspaceControllerInit(&firstController, fsys); WorkspaceControllerInit(&secondController, fsys);

    CHECK_A(!WorkspaceControllerOpenStandaloneDocument(&firstController, (nested / "gone.cpp").string().c_str()));
    CHECK_A(firstController.model.activeDocument == kMaxOpenDocuments && !WorkspaceControllerActiveDocument(&firstController));
    CHECK_A(!WorkspaceControllerOpenStandaloneDocument(&firstController, (nested / "bad.ini").string().c_str()));
    CHECK_A(WorkspaceControllerOpenStandaloneDocument(&firstController, crlfPath.string().c_str()));
    Document* doc = WorkspaceControllerActiveDocument(&firstController);
    CHECK_A(doc && doc->used && !firstController.model.hasProject && doc->buffer.caret == 0 && !doc->buffer.dirty);
    CHECK_A(doc && PathsEqual(doc->path, crlfPath.string().c_str()));
    CHECK_A(doc && doc->buffer.length == crlf.size() && std::memcmp(doc->buffer.data, crlf.data(), crlf.size()) == 0);
    if (doc) doc->buffer.caret = doc->buffer.length;
    CHECK_A(doc && TextBufferInsert(&doc->buffer, "x", 1) && doc->buffer.dirty);
    context.failWrite = true;
    CHECK_A(!WorkspaceControllerSaveDocument(&firstController, firstController.model.activeDocument) && doc->buffer.dirty);
    context.failWrite = false;
    CHECK_A(WorkspaceControllerSaveDocument(&firstController, firstController.model.activeDocument));
    CHECK_A(doc && !doc->buffer.dirty && PathsEqual(doc->path, crlfPath.string().c_str()) && get(crlfPath) == crlf + "x");

    CHECK_A(WorkspaceControllerOpenStandaloneDocument(&firstController, emptyPath.string().c_str()));
    doc = WorkspaceControllerActiveDocument(&firstController);
    CHECK_A(doc && doc->used && doc->buffer.length == 0 && PathsEqual(doc->path, emptyPath.string().c_str()));
    CHECK_A(WorkspaceControllerOpenStandaloneDocument(&firstController, (nested / "lf.txt").string().c_str()));
    doc = WorkspaceControllerActiveDocument(&firstController);
    CHECK_A(doc && doc->buffer.length == lf.size() && std::memcmp(doc->buffer.data, lf.data(), lf.size()) == 0);
    CHECK_A(WorkspaceControllerOpenStandaloneDocument(&firstController, nearPath.string().c_str()));
    doc = WorkspaceControllerActiveDocument(&firstController);
    CHECK_A(doc && doc->buffer.length == kMaxEditorBytes && std::memcmp(doc->buffer.data, near.data(), near.size()) == 0);
    CHECK_A(WorkspaceControllerCloseDocument(&firstController, firstController.model.activeDocument, CloseDecision::Discard));
    CHECK_A(!WorkspaceControllerOpenStandaloneDocument(&firstController, overPath.string().c_str()));
    CHECK_A(firstController.model.activeDocument == kMaxOpenDocuments);

    std::string longPath = "/"; for (int i = 0; i < 7; ++i) { longPath.append(127, 'a' + i); longPath += "/"; } longPath += "x.txt";
    CHECK_A(longPath.size() >= kMaxPathBytes && !WorkspaceControllerOpenStandaloneDocument(&firstController, longPath.c_str()));
    CHECK_A(WorkspaceControllerOpenStandaloneDocument(&firstController, cppPath.string().c_str()));
    CHECK_A(WorkspaceControllerCloseDocument(&firstController, firstController.model.activeDocument, CloseDecision::Discard));
    uint32_t cycles = 0;
    for (; cycles < 100; ++cycles) {
        const bool opened = WorkspaceControllerOpenStandaloneDocument(&firstController, cppPath.string().c_str());
        const bool closed = opened && WorkspaceControllerCloseDocument(&firstController, firstController.model.activeDocument, CloseDecision::Discard);
        CHECK_A(opened && closed && firstController.model.activeDocument == kMaxOpenDocuments);
    }
    CHECK_A(WorkspaceControllerOpenStandaloneDocument(&firstController, cppPath.string().c_str()));
    doc = WorkspaceControllerActiveDocument(&firstController); if (doc) doc->buffer.caret = doc->buffer.length;
    CHECK_A(doc && TextBufferInsert(&doc->buffer, "!", 1));
    CHECK_A(!WorkspaceControllerCloseDocument(&firstController, firstController.model.activeDocument, CloseDecision::Cancel) && doc->buffer.dirty);
    CHECK_A(WorkspaceControllerCloseDocument(&firstController, firstController.model.activeDocument, CloseDecision::Save));
    CHECK_A(WorkspaceControllerOpenStandaloneDocument(&firstController, cppPath.string().c_str()));
    CHECK_A(WorkspaceControllerOpenStandaloneDocument(&secondController, secondPath.string().c_str()));
    Document* one = WorkspaceControllerActiveDocument(&firstController); Document* two = WorkspaceControllerActiveDocument(&secondController);
    CHECK_A(one && two && !PathsEqual(one->path, two->path));
    if (one) one->buffer.caret = one->buffer.length;
    CHECK_A(one && TextBufferInsert(&one->buffer, "独", 3));
    CHECK_A(one && one->buffer.dirty && two && !two->buffer.dirty && two->buffer.caret == 0);
    CHECK_A(two && two->buffer.length == crlf.size() && std::memcmp(two->buffer.data, crlf.data(), crlf.size()) == 0);
    CHECK_A(WorkspaceControllerCloseDocument(&firstController, firstController.model.activeDocument, CloseDecision::Discard));
    CHECK_A(WorkspaceControllerCloseDocument(&secondController, secondController.model.activeDocument, CloseDecision::Discard));
    fs::remove_all(root, ec);
    std::cout << "developerStudioAppModelActivationChecks=" << checks << "/" << checks << "\n";
    std::cout << "developerStudioAppModelActivationLifecycleCycles=" << cycles << "/100\n";
    return 0;
}
