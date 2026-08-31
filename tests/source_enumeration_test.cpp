#include "developer_studio_workspace.h"

#include <cassert>
#include <cstring>
#include <iostream>

using namespace guidexos::developer_studio;

static bool fakeStat(void*, const char* path, FileInfo* outInfo) {
    if (!path || !outInfo) return false;
    if (std::strcmp(path, "d:/work/hello/src") == 0) {
        outInfo->kind = FileInfoKind::Directory;
        outInfo->size = 0;
        return true;
    }
    if (std::strcmp(path, "d:/work/hello/src/main.cpp") == 0) {
        outInfo->kind = FileInfoKind::RegularFile;
        outInfo->size = 11;
        return true;
    }
    return false;
}

static bool fakeList(void*, const char* path, FileListEntry* entries, uint32_t capacity,
                     uint32_t* outCount, bool* outTruncated) {
    if (!path || !entries || !outCount || !outTruncated || capacity < 4 ||
        std::strcmp(path, "d:/work/hello/src") != 0) return false;
    const char* names[] = {"math.cpp", "README.md", "helpers.cpp", "main.cpp"};
    const uint64_t sizes[] = {8, 3, 12, 11};
    for (uint32_t i = 0; i < 4; ++i) {
        std::memset(&entries[i], 0, sizeof(entries[i]));
        std::strcpy(entries[i].name, names[i]);
        entries[i].kind = FileInfoKind::RegularFile;
        entries[i].size = sizes[i];
    }
    *outCount = 4;
    *outTruncated = false;
    return true;
}

static Project project() {
    Project result = {};
    result.valid = true;
    result.formatVersion = 1;
    result.kind = ProjectKind::NativeGuiApplication;
    std::strcpy(result.rootPath, "D:/work/hello");
    std::strcpy(result.sourceRoot, "src");
    std::strcpy(result.entryPoint, "gx_main");
    return result;
}

int main() {
    WorkspaceFileSystem fileSystem = {};
    fileSystem.stat = fakeStat;
    fileSystem.list = fakeList;

    static WorkspaceController controller = {};
    WorkspaceControllerInit(&controller, fileSystem);
    controller.model.open = true;
    controller.model.hasProject = true;
    controller.model.project = project();

    ProjectSourceFile files[3] = {};
    uint32_t count = 0;
    assert(WorkspaceControllerEnumerateProjectSources(&controller, files, 3, &count));
    assert(count == 3);
    assert(std::strcmp(files[0].relativePath, "src/helpers.cpp") == 0);
    assert(std::strcmp(files[1].relativePath, "src/main.cpp") == 0);
    assert(std::strcmp(files[2].relativePath, "src/math.cpp") == 0);
    assert(files[0].size == 12 && files[1].size == 11 && files[2].size == 8);

    std::strcpy(controller.model.project.sourceEntry, "main.cpp");
    ProjectSourceFile explicitFile = {};
    count = 0;
    assert(WorkspaceControllerEnumerateProjectSources(&controller, &explicitFile, 1, &count));
    assert(count == 1);
    assert(std::strcmp(explicitFile.relativePath, "src/main.cpp") == 0);
    assert(explicitFile.size == 11);

    std::cout << "Developer Studio source enumeration PASS\n";
    return 0;
}
