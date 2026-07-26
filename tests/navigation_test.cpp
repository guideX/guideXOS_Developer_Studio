#include "developer_studio_workspace.h"

#include <cassert>
#include <cstring>
#include <iostream>

using namespace guidexos::developer_studio;

static bool statFile(void*, const char* path, FileInfo* info) {
    if (!path || !info || std::strcmp(path, "d:/project/src/main.cpp") != 0) return false;
    info->kind = FileInfoKind::RegularFile;
    info->size = 14;
    return true;
}

static bool readFile(void*, const char* path, char* buffer, uint32_t capacity, uint32_t* bytes) {
    const char content[] = "one\r\ntwo\nthree";
    if (!path || !buffer || !bytes || std::strcmp(path, "d:/project/src/main.cpp") != 0 || capacity < sizeof(content) - 1) return false;
    for (uint32_t i = 0; i < sizeof(content) - 1; ++i) buffer[i] = content[i];
    *bytes = sizeof(content) - 1;
    return true;
}

int main() {
    WorkspaceFileSystem fileSystem = { nullptr, statFile, nullptr, readFile, nullptr, nullptr, nullptr };
    static WorkspaceController controller;
    WorkspaceControllerInit(&controller, fileSystem);
    controller.model.open = true;
    controller.model.hasProject = true;
    std::strcpy(controller.model.rootPath, "D:/project");
    std::strcpy(controller.model.project.projectId, "com.example.navigation");
    controller.model.project.valid = true;

    uint32_t index = kMaxOpenDocuments;
    OutputErrorCode error = OutputErrorCode::None;
    assert(WorkspaceControllerOpenDocumentAtLocation(&controller, "com.example.navigation", "src/main.cpp", 2, 2, &index, &error));
    assert(index == controller.model.activeDocument);
    Document* document = WorkspaceControllerActiveDocument(&controller);
    assert(document && document->buffer.caret == 6 && !document->buffer.dirty);

    assert(WorkspaceControllerOpenDocumentAtLocation(&controller, "com.example.navigation", "src/main.cpp", 99, 99, &index, &error));
    assert(error == OutputErrorCode::NavigationLocationClamped);
    assert(document->buffer.dirty == false);
    assert(!WorkspaceControllerOpenDocumentAtLocation(&controller, "com.other.project", "src/main.cpp", 1, 1, &index, &error));
    assert(error == OutputErrorCode::DiagnosticProjectMismatch);
    assert(!WorkspaceControllerOpenDocumentAtLocation(&controller, "com.example.navigation", "../outside.cpp", 1, 1, &index, &error));
    assert(error == OutputErrorCode::DiagnosticPathOutsideProject);
    assert(!WorkspaceControllerOpenDocumentAtLocation(&controller, "com.example.navigation", "missing.cpp", 1, 1, &index, &error));
    assert(error == OutputErrorCode::DiagnosticFileNotFound);

    document->buffer.dirty = true;
    assert(WorkspaceControllerOpenDocumentAtLocation(&controller, "com.example.navigation", "src/main.cpp", 1, 1, &index, &error));
    assert(document->buffer.dirty);
    std::cout << "Developer Studio diagnostic navigation PASS\n";
    return 0;
}
