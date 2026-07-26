#include "developer_studio_projects.h"
#include "developer_studio_workspace.h"

#include <cassert>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

using namespace guidexos::developer_studio;
namespace fs = std::filesystem;

struct TestContext {
    bool failMainWrite = false;
};

static bool statFile(void*, const char* path, FileInfo* outInfo) {
    std::error_code ec;
    fs::file_status status = fs::symlink_status(fs::path(path), ec);
    if (ec || !fs::exists(status)) return false;
    outInfo->kind = fs::is_directory(status) ? FileInfoKind::Directory : (fs::is_regular_file(status) ? FileInfoKind::RegularFile : FileInfoKind::Unknown);
    outInfo->size = outInfo->kind == FileInfoKind::RegularFile ? fs::file_size(path, ec) : 0;
    return !ec;
}

static bool listFiles(void*, const char* path, FileListEntry* entries, uint32_t capacity, uint32_t* outCount, bool* outTruncated) {
    *outCount = 0;
    *outTruncated = false;
    std::error_code ec;
    for (const fs::directory_entry& item : fs::directory_iterator(fs::path(path), ec)) {
        if (ec) return false;
        if (*outCount >= capacity) { *outTruncated = true; break; }
        std::string name = item.path().filename().string();
        std::strncpy(entries[*outCount].name, name.c_str(), sizeof(entries[*outCount].name) - 1);
        entries[*outCount].name[sizeof(entries[*outCount].name) - 1] = '\0';
        entries[*outCount].kind = item.is_directory(ec) ? FileInfoKind::Directory : FileInfoKind::RegularFile;
        entries[*outCount].size = entries[*outCount].kind == FileInfoKind::RegularFile ? item.file_size(ec) : 0;
        ++*outCount;
    }
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

static bool writeFile(void* userData, const char* path, const char* buffer, uint32_t bytes, uint32_t* outBytes) {
    TestContext* context = static_cast<TestContext*>(userData);
    if (context && context->failMainWrite && std::string(path).find("src/main.cpp") != std::string::npos) return false;
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) return false;
    output.write(buffer, bytes);
    output.flush();
    if (!output) return false;
    *outBytes = bytes;
    return true;
}

static bool createDirectory(void*, const char* path) {
    std::error_code ec;
    return fs::create_directory(fs::path(path), ec) && !ec;
}

static bool removePath(void*, const char* path) {
    std::error_code ec;
    return fs::remove(fs::path(path), ec) && !ec;
}

static std::string readAll(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
}

static Project makeProject() {
    Project project = {};
    project.formatVersion = 1;
    project.kind = ProjectKind::NativeGuiApplication;
    std::strcpy(project.projectId, "com.example.hello");
    std::strcpy(project.displayName, "Hello guideXOS");
    std::strcpy(project.sourceRoot, "src");
    std::strcpy(project.manifestPath, "app/app.json");
    std::strcpy(project.targetProfileId, "guidexos.amd64.hosted.native");
    std::strcpy(project.entryPoint, "gx_main");
    std::strcpy(project.abi, "guidexos-c-abi-v1");
    std::strcpy(project.architecture, "amd64");
    std::strcpy(project.outputName, "hello-guidexos");
    return project;
}

int main(int argc, char** argv) {
    assert(ValidateProjectDisplayName("Hello guideXOS"));
    assert(!ValidateProjectDisplayName("bad\nname"));
    assert(ValidateProjectId("com.example.hello"));
    assert(!ValidateProjectId("Com.example.hello"));
    assert(!ValidateProjectId("com.example..hello"));
    assert(!ValidateProjectId("com.guidexos.custom"));
    assert(!ValidateProjectFolderName("../outside"));
    char derived[128] = {};
    assert(DeriveProjectFolderName("Hello guideXOS", derived, sizeof(derived)));
    assert(std::strcmp(derived, "hello-guidexos") == 0);
    char outputName[128] = {};
    assert(DeriveProjectOutputName(derived, outputName, sizeof(outputName)));
    assert(std::strcmp(outputName, "hello-guidexos-guidexos") == 0);

    Project project = makeProject();
    char serialized[kMaxProjectFileBytes] = {};
    uint32_t serializedBytes = 0;
    ProjectErrorCode error = ProjectErrorCode::None;
    assert(SerializeProjectMetadata(project, serialized, sizeof(serialized), &serializedBytes, &error));
    const char expected[] = "{\n  \"formatVersion\": 1,\n  \"projectId\": \"com.example.hello\",\n  \"displayName\": \"Hello guideXOS\",\n  \"projectKind\": \"native-gui-application\",\n  \"sourceRoot\": \"src\",\n  \"applicationManifest\": \"app/app.json\",\n  \"defaultTargetProfile\": \"guidexos.amd64.hosted.native\",\n  \"entryPoint\": \"gx_main\",\n  \"abi\": \"guidexos-c-abi-v1\",\n  \"architecture\": \"amd64\",\n  \"outputName\": \"hello-guidexos\"\n}\n";
    assert(std::string(serialized, serializedBytes) == expected);
    Project parsed = {};
    assert(ParseProjectMetadata(serialized, serializedBytes, &parsed, &error));
    assert(std::strcmp(parsed.projectId, project.projectId) == 0);
    std::string unsupported(serialized, serializedBytes);
    const std::string versionToken = "\"formatVersion\": 1";
    const size_t versionPosition = unsupported.find(versionToken);
    assert(versionPosition != std::string::npos);
    unsupported[versionPosition + versionToken.size() - 1] = '2';
    assert(!ParseProjectMetadata(unsupported.data(), static_cast<uint32_t>(unsupported.size()), &parsed, &error) && error == ProjectErrorCode::UnsupportedFormatVersion);
    std::string unknownTarget(serialized, serializedBytes);
    const std::string targetValue = "guidexos.amd64.hosted.native";
    const size_t targetPosition = unknownTarget.find(targetValue);
    assert(targetPosition != std::string::npos);
    unknownTarget.replace(targetPosition, targetValue.size(), "guidexos.amd64.unknown");
    assert(!ParseProjectMetadata(unknownTarget.data(), static_cast<uint32_t>(unknownTarget.size()), &parsed, &error) && error == ProjectErrorCode::UnknownTargetProfile);
    const char duplicateJson[] = "{\"formatVersion\":1,\"formatVersion\":1}";
    assert(!ParseProjectMetadata(duplicateJson, sizeof(duplicateJson) - 1, &parsed, &error) && error == ProjectErrorCode::DuplicateField);
    assert(!ParseProjectMetadata("{\"formatVersion\":2}", 19, &parsed, &error) && error == ProjectErrorCode::MissingField);
    char oversized[kMaxProjectFileBytes + 1] = {};
    assert(!ParseProjectMetadata(oversized, sizeof(oversized), &parsed, &error) && error == ProjectErrorCode::ProjectFileTooLarge);

    const fs::path testRoot = fs::absolute(fs::path("tmp") / "developer-studio-project-test-8");
    std::error_code ec;
    const bool preserve = argc > 1 && std::strcmp(argv[1], "--preserve") == 0;
    if (argc > 1 && std::strcmp(argv[1], "--cleanup") == 0) {
        fs::remove_all(testRoot, ec);
        return 0;
    }
    if (fs::exists(testRoot)) { std::cerr << "test fixture already exists: " << testRoot << "\n"; return 2; }
    fs::create_directories(testRoot / "parent-one", ec);
    fs::create_directories(testRoot / "parent-two", ec);
    TestContext context;
    ProjectFileSystem fileSystem = { &context, statFile, listFiles, readFile, writeFile, createDirectory, removePath };
    ProjectCreateRequest request = {};
    std::strcpy(request.parentPath, (testRoot / "parent-one").string().c_str());
    std::strcpy(request.folderName, "hello");
    std::strcpy(request.projectId, "com.example.hello");
    std::strcpy(request.displayName, "Hello guideXOS");
    request.kind = ProjectKind::NativeGuiApplication;
    ProjectOperationResult created;
    if (!CreateNativeGuiProject(fileSystem, request, &created)) { std::cerr << "create error: " << ProjectErrorName(created.error) << "\n"; return 3; }
    assert(created.success && created.project.valid);
    const fs::path generatedRoot = testRoot / "parent-one" / "hello";
    assert(fs::exists(generatedRoot / "guidexos.project"));
    assert(fs::exists(generatedRoot / "app" / "app.json"));
    assert(fs::exists(generatedRoot / "src" / "main.cpp"));
    ProjectOperationResult loaded;
    assert(LoadProject(fileSystem, generatedRoot.string().c_str(), &loaded));
    assert(LoadProject(fileSystem, (generatedRoot / "guidexos.project").string().c_str(), &loaded));
    assert(std::strcmp(loaded.project.projectId, request.projectId) == 0);
    const std::string generatedMain = readAll(generatedRoot / "src" / "main.cpp");
    assert(generatedMain.find("gx_main") != std::string::npos);
    assert(generatedMain.find("request_window_ex") != std::string::npos);
    assert(generatedMain.find("Welcome to ") != std::string::npos);
    assert(generatedMain.find("D:\\dev\\") == std::string::npos);
    assert(readAll(generatedRoot / "build.ps1").find("D:\\dev\\guideXOSServer") == std::string::npos);

    static WorkspaceController controller;
    WorkspaceControllerInit(&controller, fileSystem);
    assert(WorkspaceControllerOpenProject(&controller, generatedRoot.string().c_str()));
    assert(controller.model.hasProject);
    assert(std::strcmp(controller.model.project.projectId, request.projectId) == 0);
    assert(WorkspaceControllerOpenDocument(&controller, "guidexos.project"));
    assert(WorkspaceControllerCloseWorkspace(&controller, CloseDecision::Discard));
    assert(!controller.model.hasProject);
    assert(WorkspaceControllerOpenWorkspace(&controller, generatedRoot.string().c_str()));
    assert(!controller.model.hasProject);

    ProjectCreateRequest repeat = request;
    std::strcpy(repeat.parentPath, (testRoot / "parent-two").string().c_str());
    ProjectOperationResult repeated;
    assert(CreateNativeGuiProject(fileSystem, repeat, &repeated));
    const fs::path repeatedRoot = testRoot / "parent-two" / "hello";
    const char* generatedFiles[] = { "guidexos.project", "CMakeLists.txt", "build.ps1", "README.md", "app/app.json", "src/main.cpp", "src/freestanding_memory.cpp" };
    for (const char* file : generatedFiles) assert(readAll(generatedRoot / file) == readAll(repeatedRoot / file));

    fs::create_directories(testRoot / "parent-one" / "occupied", ec);
    std::ofstream(testRoot / "parent-one" / "occupied" / "existing.txt") << "keep";
    ProjectCreateRequest occupied = request;
    std::strcpy(occupied.folderName, "occupied");
    assert(!CreateNativeGuiProject(fileSystem, occupied, &loaded) && loaded.error == ProjectErrorCode::DestinationExists);

    TestContext failingContext;
    failingContext.failMainWrite = true;
    ProjectFileSystem failingFileSystem = { &failingContext, statFile, listFiles, readFile, writeFile, createDirectory, removePath };
    ProjectCreateRequest failing = request;
    std::strcpy(failing.folderName, "rollback");
    std::strcpy(failing.parentPath, (testRoot / "parent-one").string().c_str());
    assert(!CreateNativeGuiProject(failingFileSystem, failing, &loaded));
    assert(loaded.rollbackAttempted && loaded.rollbackSucceeded);
    assert(!fs::exists(testRoot / "parent-one" / "rollback"));

    if (!preserve) fs::remove_all(testRoot, ec);
    std::cout << "Developer Studio project parser/generator PASS\n";
    return 0;
}
