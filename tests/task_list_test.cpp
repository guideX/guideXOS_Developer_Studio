#include "developer_studio_task_list.h"
#include "developer_studio_workspace.h"
#include "developer_studio_tools.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

using namespace guidexos::developer_studio;
namespace fs = std::filesystem;

struct TestFileSystem {
    uint32_t writeCalls;
};

static bool statFile(void*, const char* path, FileInfo* info) {
    std::error_code error;
    const fs::file_status status = fs::symlink_status(fs::path(path), error);
    if (error || !fs::exists(status)) return false;
    info->kind = fs::is_directory(status) ? FileInfoKind::Directory :
        (fs::is_regular_file(status) ? FileInfoKind::RegularFile : FileInfoKind::Unknown);
    info->size = info->kind == FileInfoKind::RegularFile ? fs::file_size(path, error) : 0;
    return !error;
}

static bool listFiles(void*, const char* path, FileListEntry* entries, uint32_t capacity,
                      uint32_t* outCount, bool* outTruncated) {
    *outCount = 0;
    *outTruncated = false;
    std::error_code error;
    for (const fs::directory_entry& item : fs::directory_iterator(fs::path(path), error)) {
        if (error) return false;
        if (*outCount >= capacity) { *outTruncated = true; break; }
        const std::string name = item.path().filename().string();
        std::strncpy(entries[*outCount].name, name.c_str(), sizeof(entries[*outCount].name) - 1u);
        entries[*outCount].name[sizeof(entries[*outCount].name) - 1u] = '\0';
        const fs::file_status status = item.symlink_status(error);
        if (error) return false;
        entries[*outCount].kind = fs::is_directory(status) ? FileInfoKind::Directory :
            (fs::is_regular_file(status) ? FileInfoKind::RegularFile : FileInfoKind::Unknown);
        entries[*outCount].size = entries[*outCount].kind == FileInfoKind::RegularFile ? item.file_size(error) : 0;
        if (error) return false;
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

static bool writeFile(void* userData, const char*, const char*, uint32_t, uint32_t*) {
    TestFileSystem* fileSystem = static_cast<TestFileSystem*>(userData);
    ++fileSystem->writeCalls;
    return false;
}

static void writeBytes(const fs::path& path, const std::string& content) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(content.data(), static_cast<std::streamsize>(content.size()));
    assert(output.good());
}

static void setText(char* output, uint32_t capacity, const char* value) {
    std::strncpy(output, value, capacity - 1u);
    output[capacity - 1u] = '\0';
}

static bool scanOne(ProjectTaskList* tasks, const char* path, const std::string& content) {
    ProjectSearchScanFile file = {};
    file.relativePath = path;
    file.data = content.data();
    file.length = static_cast<uint32_t>(content.size());
    return ProjectTaskListVisitFile(tasks, &file);
}

static WorkspaceFileSystem makeFileSystem(TestFileSystem* behavior) {
    WorkspaceFileSystem fileSystem = {};
    fileSystem.userData = behavior;
    fileSystem.stat = statFile;
    fileSystem.list = listFiles;
    fileSystem.read = readFile;
    fileSystem.write = writeFile;
    return fileSystem;
}

static ProjectSearchRequest makeRequest(const fs::path& root, TestFileSystem* behavior) {
    ProjectSearchRequest request = {};
    setText(request.projectId, sizeof(request.projectId), "com.example.task-list");
    request.projectGeneration = 7;
    setText(request.rootPath, sizeof(request.rootPath), root.string().c_str());
    ProjectSearchOptionsInit(&request.options);
    setText(request.options.query, sizeof(request.options.query), "x");
    setText(request.options.includePattern, sizeof(request.options.includePattern),
            "*.c;*.h;*.cc;*.cpp;*.cxx;*.hh;*.hpp;*.hxx");
    request.fileSystem = makeFileSystem(behavior);
    return request;
}

static const ProjectSearchOperation* runScan(ProjectSearchService* service,
                                               ProjectTaskList* tasks,
                                               ProjectSearchRequest request,
                                               uint64_t* outOperationId) {
    request.scanVisitor = ProjectTaskListVisitFile;
    request.scanVisitorUserData = tasks;
    ProjectSearchErrorCode error = ProjectSearchErrorCode::None;
    assert(ProjectSearchStart(service, &request, 0, outOperationId, &error));
    for (uint32_t poll = 0; poll < 20000 && ProjectSearchIsActive(service); ++poll)
        assert(ProjectSearchPoll(service, *outOperationId, 16, poll));
    assert(!ProjectSearchIsActive(service));
    ProjectTaskListSort(tasks);
    return ProjectSearchOperationInfo(service);
}

int main() {
    static ProjectTaskList tasks;
    static ProjectSearchService service;
    ProjectTaskListInit(&tasks);

    const std::string lexical =
        "const char* line = \"// TODO string\";\n"
        "const char* block = \"/* FIXME string */\";\n"
        "char c = 'x'; // TODO: actual line comment\n"
        "// TODOABLE NOTODO _TODO TODO1 FIXME_extra; todo: lowercase\n"
        "/* FIXME: first block line\n"
        " * TODO: second block line */\n"
        "auto raw = R\"tag(// TODO raw string\n/* FIXME raw string */)tag\";\n";
    assert(scanOne(&tasks, "src/lexical.cpp", lexical));
    assert(tasks.count == 4);
    assert(tasks.todoCount == 3 && tasks.fixmeCount == 1);
    assert(tasks.tasks[0].line == 3);
    assert(tasks.tasks[1].line == 4);
    assert(tasks.tasks[2].line == 5);
    assert(tasks.tasks[3].line == 6);
    assert(std::strcmp(tasks.tasks[1].description, "todo: lowercase") == 0);
    assert(std::strcmp(ProjectTaskAnnotationName(ProjectTaskAnnotation::Todo), "TODO") == 0);
    assert(std::strcmp(ProjectTaskAnnotationName(ProjectTaskAnnotation::Fixme), "FIXME") == 0);

    ProjectTaskListInit(&tasks);
    const std::string endings = "// TODO first\r\n/* FIXME second\nTODO third */\r\n";
    assert(scanOne(&tasks, "src/endings.c", endings));
    assert(tasks.count == 3);
    assert(tasks.tasks[0].line == 1 && tasks.tasks[1].line == 2 && tasks.tasks[2].line == 3);

    ProjectTaskListInit(&tasks);
    std::string longDescription = "// TODO ";
    longDescription.append(400, 'L');
    longDescription += "\n";
    assert(scanOne(&tasks, "src/long.cpp", longDescription));
    assert(tasks.count == 1);
    assert(std::strlen(tasks.tasks[0].description) <= kProjectTaskListMaxDescriptionBytes);
    assert(tasks.tasks[0].description[std::strlen(tasks.tasks[0].description) - 1u] == '.');

    ProjectTaskListInit(&tasks);
    assert(scanOne(&tasks, "src/no-tasks.cpp", "// ordinary comment\nint value = 1;\n"));
    assert(tasks.count == 0 && tasks.todoCount == 0 && tasks.fixmeCount == 0);
    assert(scanOne(&tasks, "src/ignored.txt", "// TODO in unsupported source type\n"));
    assert(tasks.count == 0);
    assert(!scanOne(&tasks, "../outside.cpp", "// TODO outside\n"));
    assert(tasks.truncated);

    ProjectTaskListInit(&tasks);
    assert(scanOne(&tasks, "z.cpp", "// TODO z\n"));
    assert(scanOne(&tasks, "a.cpp", "// FIXME a\n// TODO a\n"));
    ProjectTaskListSort(&tasks);
    assert(std::strcmp(tasks.tasks[0].relativePath, "a.cpp") == 0);
    assert(tasks.tasks[0].line == 1 && tasks.tasks[0].annotation == ProjectTaskAnnotation::Fixme);
    assert(tasks.tasks[1].line == 2 && tasks.tasks[1].annotation == ProjectTaskAnnotation::Todo);

    ProjectTaskListInit(&tasks);
    std::string tooMany;
    for (uint32_t i = 0; i <= kProjectTaskListMaxTasks; ++i) tooMany += "// TODO item\n";
    assert(!scanOne(&tasks, "src/many.cpp", tooMany));
    assert(tasks.count == kProjectTaskListMaxTasks && tasks.truncated);

    const fs::path root = fs::temp_directory_path() / "guidexos-project-task-list-test";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root / "src", ec);
    fs::create_directories(root / "build", ec);
    writeBytes(root / "src" / "main.cpp", "int main() {\n  // TODO: open here\n}\n");
    writeBytes(root / "src" / "empty.cpp", "// no work here\n");
    writeBytes(root / "notes.txt", "// TODO in a non-source text file\n");
    writeBytes(root / "build" / "generated.cpp", "// FIXME excluded generated file\n");
    writeBytes(root / "binary.cpp", std::string("// TODO hidden\0binary", 21));
    TestFileSystem behavior = {};
    WorkspaceFileSystem fileSystem = makeFileSystem(&behavior);
    ProjectSearchRequest request = makeRequest(root, &behavior);
    static ProjectTaskList scanned;
    ProjectTaskListInit(&scanned);
    uint64_t operationId = 0;
    const ProjectSearchOperation* operation = runScan(&service, &scanned, request, &operationId);
    assert(operation && operation->state == ProjectSearchState::Completed && !operation->truncated);
    assert(scanned.count == 1 && scanned.todoCount == 1 && scanned.fixmeCount == 0);
    assert(scanned.filesScanned == 2);
    assert(operation->diagnostics.binaryFiles == 1);
    assert(behavior.writeCalls == 0);

    // Hitting the task capacity stops the shared traversal and marks its partial result.
    const fs::path overflowRoot = fs::temp_directory_path() / "guidexos-project-task-list-overflow";
    fs::remove_all(overflowRoot, ec);
    fs::create_directories(overflowRoot / "src", ec);
    std::string overflowContent;
    for (uint32_t i = 0; i <= kProjectTaskListMaxTasks; ++i) overflowContent += "// TODO overflow\n";
    writeBytes(overflowRoot / "src" / "overflow.cpp", overflowContent);
    request = makeRequest(overflowRoot, &behavior);
    ProjectTaskListInit(&scanned);
    operation = runScan(&service, &scanned, request, &operationId);
    assert(operation && operation->state == ProjectSearchState::Completed && operation->truncated);
    assert(scanned.count == kProjectTaskListMaxTasks && scanned.truncated);
    request = makeRequest(root, &behavior);

    // Repeating a scan replaces the model with current project contents.
    ProjectTaskListInit(&scanned);
    writeBytes(root / "src" / "empty.cpp", "// FIXME: added after the first scan\n");
    operation = runScan(&service, &scanned, request, &operationId);
    assert(operation && operation->state == ProjectSearchState::Completed);
    assert(scanned.count == 2 && scanned.todoCount == 1 && scanned.fixmeCount == 1);

    // Empty and annotation-free projects complete with zero results.
    const fs::path emptyRoot = fs::temp_directory_path() / "guidexos-project-task-list-empty";
    fs::remove_all(emptyRoot, ec);
    fs::create_directories(emptyRoot, ec);
    request = makeRequest(emptyRoot, &behavior);
    ProjectTaskListInit(&scanned);
    operation = runScan(&service, &scanned, request, &operationId);
    assert(operation && operation->state == ProjectSearchState::Completed && scanned.count == 0);

    fs::create_directories(emptyRoot / "src", ec);
    writeBytes(emptyRoot / "src" / "ordinary.cpp", "int answer = 42;\n// not a task\n");
    ProjectTaskListInit(&scanned);
    request = makeRequest(emptyRoot, &behavior);
    operation = runScan(&service, &scanned, request, &operationId);
    assert(operation && operation->state == ProjectSearchState::Completed && scanned.count == 0);

    // Cancellation produces a distinct partial result state and a later scan can complete.
    const fs::path cancelRoot = fs::temp_directory_path() / "guidexos-project-task-list-cancel";
    fs::remove_all(cancelRoot, ec);
    fs::create_directories(cancelRoot, ec);
    for (uint32_t i = 0; i < 12; ++i) {
        char name[32] = {};
        std::snprintf(name, sizeof(name), "file%02u.cpp", i);
        writeBytes(cancelRoot / name, "// TODO repeated\n");
    }
    request = makeRequest(cancelRoot, &behavior);
    ProjectTaskListInit(&scanned);
    request.scanVisitor = ProjectTaskListVisitFile;
    request.scanVisitorUserData = &scanned;
    ProjectSearchErrorCode searchError = ProjectSearchErrorCode::None;
    assert(ProjectSearchStart(&service, &request, 0, &operationId, &searchError));
    assert(ProjectSearchPoll(&service, operationId, 1, 1));
    assert(ProjectSearchCancel(&service, operationId));
    assert(ProjectSearchPoll(&service, operationId, 1, 2));
    assert(ProjectSearchOperationInfo(&service)->state == ProjectSearchState::Cancelled);
    const uint64_t cancelledOperation = operationId;
    ProjectTaskListInit(&scanned);
    operation = runScan(&service, &scanned, request, &operationId);
    assert(operationId != cancelledOperation);
    assert(operation && operation->state == ProjectSearchState::Completed && scanned.count == 12);

    // Selection revalidates the current project and file before editor navigation.
    static WorkspaceController controller;
    WorkspaceControllerInit(&controller, fileSystem);
    controller.model.open = true;
    controller.model.hasProject = true;
    controller.model.project.valid = true;
    controller.model.projectGeneration = 7;
    setText(controller.model.rootPath, sizeof(controller.model.rootPath), root.generic_string().c_str());
    setText(controller.model.project.projectId, sizeof(controller.model.project.projectId), "com.example.task-list");
    assert(DeveloperToolProjectIdentityMatches("com.example.task-list", 7,
        controller.model.project.projectId, controller.model.projectGeneration));
    assert(!DeveloperToolProjectIdentityMatches("com.example.task-list", 7,
        "com.example.other", controller.model.projectGeneration));
    uint32_t documentIndex = kMaxOpenDocuments;
    OutputErrorCode navigationError = OutputErrorCode::None;
    assert(WorkspaceControllerOpenDocumentAtLocation(&controller, controller.model.project.projectId,
        "src/main.cpp", 2, 1, &documentIndex, &navigationError));
    const Document* opened = WorkspaceControllerActiveDocumentConst(&controller);
    assert(opened && opened->buffer.caret == std::strlen("int main() {\n"));
    fs::remove(root / "src" / "main.cpp", ec);
    assert(!WorkspaceControllerOpenDocumentAtLocation(&controller, controller.model.project.projectId,
        "src/main.cpp", 2, 1, &documentIndex, &navigationError));
    assert(behavior.writeCalls == 0);

    fs::remove_all(root, ec);
    fs::remove_all(overflowRoot, ec);
    fs::remove_all(emptyRoot, ec);
    fs::remove_all(cancelRoot, ec);
    std::cout << "Developer Studio bounded project TODO/FIXME scanner PASS\n";
    return 0;
}
