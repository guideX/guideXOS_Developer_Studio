#include "developer_studio_workspace.h"
#include "developer_studio_build.h"

#include <cassert>
#include <cstring>
#include <iostream>
#include <map>
#include <string>

using namespace guidexos::developer_studio;

struct MemoryFs {
    struct Node { bool directory; std::string bytes; };
    std::map<std::string, Node> nodes;
    std::string partialWritePath;
};

static bool memoryStat(void* userData, const char* path, FileInfo* outInfo) {
    MemoryFs& fs = *static_cast<MemoryFs*>(userData);
    const auto found = fs.nodes.find(path ? path : "");
    if (found == fs.nodes.end() || !outInfo) return false;
    outInfo->kind = found->second.directory ? FileInfoKind::Directory : FileInfoKind::RegularFile;
    outInfo->size = found->second.bytes.size();
    return true;
}

static bool memoryList(void* userData, const char* path, FileListEntry* entries,
                       uint32_t capacity, uint32_t* outCount, bool* outTruncated) {
    if (!path || !entries || !outCount || !outTruncated) return false;
    MemoryFs& fs = *static_cast<MemoryFs*>(userData);
    const std::string prefix = std::string(path) + "/";
    *outCount = 0;
    *outTruncated = false;
    for (const auto& item : fs.nodes) {
        if (item.first.compare(0, prefix.size(), prefix) != 0) continue;
        const std::string rest = item.first.substr(prefix.size());
        if (rest.empty() || rest.find('/') != std::string::npos) continue;
        if (*outCount == capacity) { *outTruncated = true; break; }
        FileListEntry& entry = entries[(*outCount)++];
        entry = {};
        if (rest.size() >= sizeof(entry.name)) return false;
        std::memcpy(entry.name, rest.c_str(), rest.size() + 1);
        entry.kind = item.second.directory ? FileInfoKind::Directory : FileInfoKind::RegularFile;
        entry.size = item.second.bytes.size();
    }
    return true;
}

static bool memoryRead(void* userData, const char* path, char* buffer,
                       uint32_t capacity, uint32_t* outBytes) {
    MemoryFs& fs = *static_cast<MemoryFs*>(userData);
    const auto found = fs.nodes.find(path ? path : "");
    if (found == fs.nodes.end() || found->second.directory || !buffer || !outBytes ||
        found->second.bytes.size() > capacity) return false;
    std::memcpy(buffer, found->second.bytes.data(), found->second.bytes.size());
    *outBytes = static_cast<uint32_t>(found->second.bytes.size());
    return true;
}

static bool memoryWrite(void* userData, const char* path, const char* buffer,
                        uint32_t bytes, uint32_t* outBytes) {
    MemoryFs& fs = *static_cast<MemoryFs*>(userData);
    if (!path || (!buffer && bytes) || !outBytes) return false;
    MemoryFs::Node& node = fs.nodes[path];
    node.directory = false;
    node.bytes.assign(buffer ? buffer : "", bytes);
    *outBytes = bytes;
    if (path == fs.partialWritePath) {
        if (!node.bytes.empty()) node.bytes.resize(node.bytes.size() / 2);
        *outBytes = static_cast<uint32_t>(node.bytes.size());
        return false;
    }
    return true;
}

static bool memoryRemove(void* userData, const char* path) {
    MemoryFs& fs = *static_cast<MemoryFs*>(userData);
    return fs.nodes.erase(path ? path : "") != 0;
}

static void addDirectory(MemoryFs& fs, const char* path) {
    fs.nodes[path] = {true, std::string()};
}

static Project makeProject() {
    Project project = {};
    project.valid = true;
    project.formatVersion = 1;
    project.kind = ProjectKind::NativeGuiApplication;
    std::strcpy(project.projectId, "com.example.workflow");
    std::strcpy(project.displayName, "Workflow Test");
    std::strcpy(project.rootPath, "d:/work/app");
    std::strcpy(project.sourceRoot, "src");
    std::strcpy(project.manifestPath, "app/app.json");
    std::strcpy(project.targetProfileId, InitialTargetProfile().id);
    std::strcpy(project.entryPoint, "gx_main");
    std::strcpy(project.abi, "guidexos-c-abi-v1");
    std::strcpy(project.architecture, "amd64");
    std::strcpy(project.outputName, "workflow-test");
    std::strcpy(project.sourceEntry, "main.cpp");
    return project;
}

struct FakeBuild {
    uint32_t starts;
    uint32_t polls;
    bool released;
};

static bool startBuild(void* userData, const BuildRequest&, uint64_t* handle, BuildErrorCode*) {
    FakeBuild& fake = *static_cast<FakeBuild*>(userData);
    ++fake.starts;
    fake.polls = 0;
    fake.released = false;
    *handle = 1;
    return true;
}

static bool pollBuild(void* userData, uint64_t, BuildResult* result, bool* completed, BuildErrorCode*) {
    FakeBuild& fake = *static_cast<FakeBuild*>(userData);
    *result = BuildResult();
    result->state = BuildState::Succeeded;
    result->artifactValid = true;
    result->artifactEntryPoint = true;
    std::strcpy(result->artifactPath, "build/bin/amd64/workflow-test.elf");
    std::strcpy(result->artifactArchitecture, "amd64");
    std::strcpy(result->artifactSha256, "0123456789012345678901234567890123456789012345678901234567890123");
    *completed = ++fake.polls == 1;
    return true;
}

static bool releaseBuild(void* userData, uint64_t) {
    static_cast<FakeBuild*>(userData)->released = true;
    return true;
}

int main() {
    MemoryFs memory;
    addDirectory(memory, "d:/work/app");
    addDirectory(memory, "d:/work/app/src");
    addDirectory(memory, "d:/work/app/app");
    memory.nodes["d:/work/app/src/main.cpp"] = {false, "int gx_main() { return 0; }\n"};
    Project project = makeProject();
    char metadata[kMaxProjectFileBytes] = {};
    uint32_t metadataBytes = 0;
    ProjectErrorCode projectError = ProjectErrorCode::None;
    assert(SerializeProjectMetadata(project, metadata, sizeof(metadata), &metadataBytes, &projectError));
    memory.nodes["d:/work/app/guidexos.project"] = {false, std::string(metadata, metadataBytes)};

    WorkspaceFileSystem fileSystem = {};
    fileSystem.userData = &memory;
    fileSystem.stat = memoryStat;
    fileSystem.list = memoryList;
    fileSystem.read = memoryRead;
    fileSystem.write = memoryWrite;
    fileSystem.removePath = memoryRemove;
    static WorkspaceController controller = {};
    WorkspaceControllerInit(&controller, fileSystem);
    controller.model.open = true;
    controller.model.hasProject = true;
    controller.model.project = project;
    std::strcpy(controller.model.rootPath, project.rootPath);
    assert(WorkspaceModelSetBrowsePath(&controller.model, project.sourceRoot));

    ProjectSourceFile sources[kMaxProjectSourceFiles] = {};
    uint32_t sourceCount = 0;
    assert(WorkspaceControllerEnumerateProjectSources(&controller, sources, kMaxProjectSourceFiles, &sourceCount));
    assert(sourceCount == 1 && std::strcmp(sources[0].relativePath, "src/main.cpp") == 0);

    uint32_t helperIndex = kMaxOpenDocuments;
    assert(WorkspaceControllerCreateProjectFile(&controller, "helper.cpp", &helperIndex));
    assert(helperIndex < kMaxOpenDocuments && controller.model.activeDocument == helperIndex);
    assert(std::strcmp(controller.model.documents[helperIndex].path, "d:/work/app/src/helper.cpp") == 0);
    assert(controller.model.documents[helperIndex].documentId != 0);
    uint32_t diagnosticIndex = kMaxOpenDocuments;
    OutputErrorCode navigationError = OutputErrorCode::None;
    assert(WorkspaceControllerOpenDocumentAtLocation(&controller, project.projectId,
                                                       "src/helper.cpp", 1, 0,
                                                       &diagnosticIndex, &navigationError));
    assert(navigationError == OutputErrorCode::None && diagnosticIndex == helperIndex);
    assert(controller.model.activeDocument == helperIndex);
    assert(controller.model.entryCount >= 2);
    bool helperVisible = false;
    for (uint32_t i = 0; i < controller.model.entryCount; ++i)
        if (std::strcmp(controller.model.entries[i].name, "helper.cpp") == 0) helperVisible = true;
    assert(helperVisible);
    assert(controller.model.project.hasSourceFiles && controller.model.project.sourceFileCount == 2);
    assert(std::strcmp(controller.model.project.sourceFiles[0], "main.cpp") == 0);
    assert(std::strcmp(controller.model.project.sourceFiles[1], "helper.cpp") == 0);
    assert(!WorkspaceControllerCreateProjectFile(&controller, "helper.cpp", nullptr));
    assert(!WorkspaceControllerCreateProjectFile(&controller, "../escape.cpp", nullptr));
    assert(!WorkspaceControllerCreateProjectFile(&controller, "unsupported.hpp", nullptr));

    uint32_t headerIndex = kMaxOpenDocuments;
    assert(WorkspaceControllerCreateProjectFile(&controller, "helper.h", &headerIndex));
    assert(controller.model.project.sourceFileCount == 2);
    const char initialHeaderText[] = "int calculate_value();\n// first header line\n// second header line\n// header diagnostic line\n";
    TextBuffer& initialHeader = controller.model.documents[headerIndex].buffer;
    std::memcpy(initialHeader.data, initialHeaderText, sizeof(initialHeaderText));
    initialHeader.length = static_cast<uint32_t>(sizeof(initialHeaderText) - 1);
    initialHeader.dirty = true;
    diagnosticIndex = kMaxOpenDocuments;
    assert(WorkspaceControllerOpenDocumentAtLocation(&controller, project.projectId,
                                                       "src/helper.h", 4, 7,
                                                       &diagnosticIndex, &navigationError));
    assert(navigationError == OutputErrorCode::None && diagnosticIndex == headerIndex);
    assert(controller.model.activeDocument == headerIndex);
    uint32_t textIndex = kMaxOpenDocuments;
    assert(WorkspaceControllerCreateProjectFile(&controller, "notes.txt", &textIndex));
    assert(controller.model.project.sourceFileCount == 2);

    TextBuffer& helperBuffer = controller.model.documents[helperIndex].buffer;
    const char helperText[] = "int calculate_value() { return 42; }\n";
    std::memcpy(helperBuffer.data, helperText, sizeof(helperText));
    helperBuffer.length = static_cast<uint32_t>(sizeof(helperText) - 1);
    helperBuffer.dirty = true;
    const uint64_t helperDocumentId = controller.model.documents[helperIndex].documentId;
    assert(WorkspaceControllerSaveAs(&controller, helperIndex, "helper_copy.cpp"));
    assert(std::strcmp(controller.model.documents[helperIndex].path,
                       "d:/work/app/src/helper_copy.cpp") == 0);
    assert(!helperBuffer.dirty);
    assert(memory.nodes["d:/work/app/src/helper.cpp"].bytes.empty());
    assert(memory.nodes["d:/work/app/src/helper_copy.cpp"].bytes == helperText);
    assert(controller.model.project.sourceFileCount == 2);

    helperBuffer.dirty = true;
    assert(!WorkspaceControllerSaveAs(&controller, helperIndex, "main.cpp"));
    assert(controller.lastError == ModelErrorCode::DestinationExists);
    assert(helperBuffer.dirty);
    assert(std::strcmp(controller.model.documents[helperIndex].path,
                       "d:/work/app/src/helper_copy.cpp") == 0);

    memory.partialWritePath = "d:/work/app/src/partial.cpp";
    assert(!WorkspaceControllerSaveAs(&controller, helperIndex, "partial.cpp"));
    assert(controller.lastError == ModelErrorCode::WriteFailed);
    assert(helperBuffer.dirty);
    assert(std::strcmp(controller.model.documents[helperIndex].path,
                       "d:/work/app/src/helper_copy.cpp") == 0);
    assert(memory.nodes.find(memory.partialWritePath) == memory.nodes.end());
    memory.partialWritePath.clear();

    assert(WorkspaceControllerRegisterProjectSource(&controller, helperIndex));
    assert(controller.model.project.sourceFileCount == 3);
    assert(std::strcmp(controller.model.project.sourceFiles[2], "helper_copy.cpp") == 0);
    assert(!WorkspaceControllerRegisterProjectSource(&controller, helperIndex));
    assert(controller.lastError == ModelErrorCode::DestinationExists);
    assert(WorkspaceControllerSaveDocument(&controller, helperIndex));
    assert(!helperBuffer.dirty);

    const uint64_t oldDocumentId = controller.model.documents[helperIndex].documentId;
    assert(WorkspaceControllerCloseDocument(&controller, helperIndex, CloseDecision::Discard));
    assert(WorkspaceControllerOpenDocument(&controller, "src/helper_copy.cpp"));
    int reopened = FindOpenDocument(&controller.model, "d:/work/app/src/helper_copy.cpp");
    assert(reopened >= 0);
    assert(controller.model.documents[reopened].documentId != oldDocumentId);
    assert(std::string(controller.model.documents[reopened].buffer.data,
                       controller.model.documents[reopened].buffer.length) == helperText);
    assert(helperDocumentId != 0);

    const char helperUpdated[] = "int calculate_value() { return 43; }\n";
    TextBuffer& helperDirty = controller.model.documents[reopened].buffer;
    std::memcpy(helperDirty.data, helperUpdated, sizeof(helperUpdated));
    helperDirty.length = static_cast<uint32_t>(sizeof(helperUpdated) - 1);
    helperDirty.dirty = true;
    const char headerUpdated[] = "int calculate_value();\n";
    TextBuffer& headerDirty = controller.model.documents[headerIndex].buffer;
    std::memcpy(headerDirty.data, headerUpdated, sizeof(headerUpdated));
    headerDirty.length = static_cast<uint32_t>(sizeof(headerUpdated) - 1);
    headerDirty.dirty = true;
    assert(WorkspaceControllerOpenDocument(&controller, "src/main.cpp"));
    const int mainIndex = FindOpenDocument(&controller.model, "d:/work/app/src/main.cpp");
    assert(mainIndex >= 0);
    const char mainUpdated[] = "int gx_main() { return calculate_value(); }\n";
    TextBuffer& mainDirty = controller.model.documents[mainIndex].buffer;
    std::memcpy(mainDirty.data, mainUpdated, sizeof(mainUpdated));
    mainDirty.length = static_cast<uint32_t>(sizeof(mainUpdated) - 1);
    mainDirty.dirty = true;

    FakeBuild fake = {};
    HostedBuildService buildService = { &fake, startBuild, pollBuild, releaseBuild };
    BuildController buildController = {};
    assert(BuildControllerInit(&buildController));
    static OutputService buildOutput;
    OutputServiceInit(&buildOutput);
    BuildErrorCode buildError = BuildErrorCode::None;
    assert(!BuildControllerStart(&buildController, &controller, buildService,
                                 BuildDirtyDecision::Cancel, &buildError, &buildOutput));
    assert(buildError == BuildErrorCode::UserCancelled && fake.starts == 0);
    assert(helperDirty.dirty && headerDirty.dirty && mainDirty.dirty);

    memory.partialWritePath = "d:/work/app/src/helper.h";
    assert(!BuildControllerStart(&buildController, &controller, buildService,
                                 BuildDirtyDecision::SaveAll, &buildError, &buildOutput));
    assert(buildError == BuildErrorCode::SaveFailed && fake.starts == 0);
    assert(!helperDirty.dirty && headerDirty.dirty && mainDirty.dirty);
    assert(memory.nodes["d:/work/app/src/helper_copy.cpp"].bytes == helperUpdated);

    memory.partialWritePath.clear();
    assert(BuildControllerStart(&buildController, &controller, buildService,
                               BuildDirtyDecision::SaveAll, &buildError, &buildOutput));
    assert(fake.starts == 1 && !headerDirty.dirty && !mainDirty.dirty);
    assert(memory.nodes["d:/work/app/src/helper.h"].bytes == headerUpdated);
    assert(memory.nodes["d:/work/app/src/main.cpp"].bytes == mainUpdated);
    assert(BuildControllerPoll(&buildController, buildService));
    assert(buildController.state == BuildState::Succeeded && fake.released);

    std::cout << "Developer Studio file workflow PASS\n";
    return 0;
}
