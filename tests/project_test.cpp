#include "developer_studio_projects.h"
#include "developer_studio_workspace.h"

#include <cassert>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

using namespace guidexos::developer_studio;
namespace fs = std::filesystem;

struct TestContext {
    bool failMainWrite = false;
    bool failList = false;
    bool partialManifestRead = false;
    uint32_t observerCount = 0;
    WorkspaceProjectOpenState lastProjectState = WorkspaceProjectOpenState::Idle;
    uint64_t lastProjectRequestId = 0;
    uint64_t lastProjectGeneration = 0;
    uint64_t lastCandidateId = 0;
    uint64_t lastRefreshGeneration = 0;
    ProjectErrorCode lastProjectError = ProjectErrorCode::None;
    WorkspaceController* controller = nullptr;
    bool attemptReentrantOpen = false;
    bool reentrantOpenAccepted = false;
    ProjectErrorCode reentrantOpenError = ProjectErrorCode::None;
    bool candidateVisibleBeforeCommit = false;
};

static bool statFile(void*, const char* path, FileInfo* outInfo) {
    std::error_code ec;
    fs::file_status status = fs::symlink_status(fs::path(path), ec);
    if (ec || !fs::exists(status)) return false;
    outInfo->kind = fs::is_directory(status) ? FileInfoKind::Directory : (fs::is_regular_file(status) ? FileInfoKind::RegularFile : FileInfoKind::Unknown);
    outInfo->size = outInfo->kind == FileInfoKind::RegularFile ? fs::file_size(path, ec) : 0;
    return !ec;
}

static bool listFiles(void* userData, const char* path, FileListEntry* entries, uint32_t capacity, uint32_t* outCount, bool* outTruncated) {
    TestContext* context = static_cast<TestContext*>(userData);
    if (context && context->failList) return false;
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

static void projectOpenObserver(void* userData, const WorkspaceProjectOpenEvent& event) {
    TestContext* context = static_cast<TestContext*>(userData);
    if (!context) return;
    ++context->observerCount;
    context->lastProjectState = event.state;
    context->lastProjectRequestId = event.requestId;
    context->lastProjectGeneration = event.activeProjectGeneration;
    context->lastCandidateId = event.candidateId;
    context->lastRefreshGeneration = event.refreshGeneration;
    context->lastProjectError = event.error;
    if (event.state == WorkspaceProjectOpenState::CandidateAllocated && context->controller) {
        context->candidateVisibleBeforeCommit = context->controller->model.hasProject;
    }
    if (event.state == WorkspaceProjectOpenState::LoadStarted && context->attemptReentrantOpen &&
        context->controller) {
        context->attemptReentrantOpen = false;
        context->reentrantOpenAccepted = WorkspaceControllerOpenProject(context->controller, event.path);
        context->reentrantOpenError = context->controller->lastProjectError;
    }
}

static bool readFile(void* userData, const char* path, char* buffer, uint32_t capacity, uint32_t* outBytes) {
    TestContext* context = static_cast<TestContext*>(userData);
    std::ifstream input(path, std::ios::binary);
    if (!input) return false;
    input.seekg(0, std::ios::end);
    std::streamoff size = input.tellg();
    if (size < 0 || static_cast<uint64_t>(size) > capacity) return false;
    input.seekg(0, std::ios::beg);
    if (context && context->partialManifestRead && std::string(path).find("app/app.json") != std::string::npos && size > 0) --size;
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
    std::string unknownField(serialized, serializedBytes);
    const size_t objectEnd = unknownField.rfind("\n}");
    assert(objectEnd != std::string::npos);
    unknownField.insert(objectEnd, ",\n  \"futureMetadata\": true");
    assert(!ParseProjectMetadata(unknownField.data(), static_cast<uint32_t>(unknownField.size()), &parsed, &error) && error == ProjectErrorCode::UnknownField);
    // A rejected parse must not poison the next transaction with shared
    // writable parser state.
    assert(ParseProjectMetadata(serialized, serializedBytes, &parsed, &error));
    assert(std::strcmp(parsed.projectId, project.projectId) == 0);
    const char duplicateJson[] = "{\"formatVersion\":1,\"formatVersion\":1}";
    assert(!ParseProjectMetadata(duplicateJson, sizeof(duplicateJson) - 1, &parsed, &error) && error == ProjectErrorCode::DuplicateField);
    assert(!ParseProjectMetadata("{\"formatVersion\":2}", 19, &parsed, &error) && error == ProjectErrorCode::MissingField);
    assert(!ParseProjectMetadata("", 0, &parsed, &error) && error == ProjectErrorCode::MalformedJson);
    const char malformedJson[] = "{\"formatVersion\":1";
    assert(!ParseProjectMetadata(malformedJson, sizeof(malformedJson) - 1, &parsed, &error) && error == ProjectErrorCode::MalformedJson);
    std::string missingRequired(serialized, serializedBytes);
    const std::string outputField = ",\n  \"outputName\": \"hello-guidexos\"";
    const size_t outputFieldPosition = missingRequired.find(outputField);
    assert(outputFieldPosition != std::string::npos);
    missingRequired.erase(outputFieldPosition, outputField.size());
    assert(!ParseProjectMetadata(missingRequired.data(), static_cast<uint32_t>(missingRequired.size()), &parsed, &error) && error == ProjectErrorCode::MissingField);
    std::string invalidType(serialized, serializedBytes);
    const size_t versionTypePosition = invalidType.find("\"formatVersion\": 1");
    assert(versionTypePosition != std::string::npos);
    invalidType.replace(versionTypePosition, std::strlen("\"formatVersion\": 1"), "\"formatVersion\": \"1\"");
    assert(!ParseProjectMetadata(invalidType.data(), static_cast<uint32_t>(invalidType.size()), &parsed, &error) && error == ProjectErrorCode::MalformedJson);
    std::string invalidArchitecture(serialized, serializedBytes);
    const size_t architecturePosition = invalidArchitecture.find("\"architecture\": \"amd64\"");
    assert(architecturePosition != std::string::npos);
    invalidArchitecture.replace(architecturePosition, std::strlen("\"architecture\": \"amd64\""), "\"architecture\": \"i386\"");
    assert(!ParseProjectMetadata(invalidArchitecture.data(), static_cast<uint32_t>(invalidArchitecture.size()), &parsed, &error) && error == ProjectErrorCode::InvalidArchitecture);
    std::string invalidPath(serialized, serializedBytes);
    const size_t sourcePosition = invalidPath.find("\"sourceRoot\": \"src\"");
    assert(sourcePosition != std::string::npos);
    invalidPath.replace(sourcePosition, std::strlen("\"sourceRoot\": \"src\""), "\"sourceRoot\": \"../src\"");
    assert(!ParseProjectMetadata(invalidPath.data(), static_cast<uint32_t>(invalidPath.size()), &parsed, &error) && error == ProjectErrorCode::InvalidRelativePath);
    std::string boundary(kMaxProjectFileBytes, ' ');
    assert(!ParseProjectMetadata(boundary.data(), static_cast<uint32_t>(boundary.size()), &parsed, &error) && error != ProjectErrorCode::ProjectFileTooLarge);
    char oversized[kMaxProjectFileBytes + 1] = {};
    assert(!ParseProjectMetadata(oversized, sizeof(oversized), &parsed, &error) && error == ProjectErrorCode::ProjectFileTooLarge);
    std::string longDisplay(kMaxProjectDisplayNameBytes - 1, 'a');
    assert(ValidateProjectDisplayName(longDisplay.c_str()));
    longDisplay.push_back('a');
    assert(!ValidateProjectDisplayName(longDisplay.c_str()));

    Project bareProject = makeProject();
    std::strcpy(bareProject.targetProfileId, BareMetalTargetProfile().id);
    std::strcpy(bareProject.sourceEntry, "main.cpp");
    assert(SerializeProjectMetadata(bareProject, serialized, sizeof(serialized), &serializedBytes, &error));
    assert(std::string(serialized, serializedBytes).find("\"sourceEntry\": \"main.cpp\"") != std::string::npos);
    assert(ParseProjectMetadata(serialized, serializedBytes, &parsed, &error));
    assert(std::strcmp(parsed.targetProfileId, BareMetalTargetProfile().id) == 0);
    assert(std::strcmp(parsed.sourceEntry, "main.cpp") == 0);

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
    const std::string validManifestBytes = readAll(generatedRoot / "app" / "app.json");
    ApplicationManifest parsedManifestA = {};
    assert(ParseApplicationManifest(validManifestBytes.data(), static_cast<uint32_t>(validManifestBytes.size()), &parsedManifestA, &error));
    ManifestValidationDiagnostic manifestDiagnostic = {};
    assert(ValidateApplicationManifestIdentity(parsedManifestA, loaded.project, nullptr, &manifestDiagnostic));
    assert(manifestDiagnostic.resultCode == ProjectErrorCode::None);
    const uint64_t stableManifestHash = ComputeManifestContentHashFnv1a64(validManifestBytes.data(), static_cast<uint32_t>(validManifestBytes.size()));
    assert(stableManifestHash == ComputeManifestContentHashFnv1a64(validManifestBytes.data(), static_cast<uint32_t>(validManifestBytes.size())));

    // Production parser/validator repetition, including valid A -> different B -> valid A.
    std::string differentManifestBytes = validManifestBytes;
    const std::string validId = "com.example.hello";
    size_t idAt = differentManifestBytes.find(validId);
    assert(idAt != std::string::npos);
    differentManifestBytes.replace(idAt, validId.size(), "com.example.other");
    ApplicationManifest parsedManifestB = {};
    assert(ParseApplicationManifest(differentManifestBytes.data(), static_cast<uint32_t>(differentManifestBytes.size()), &parsedManifestB, &error));
    assert(!ValidateApplicationManifestIdentity(parsedManifestB, loaded.project, nullptr, &manifestDiagnostic));
    assert(manifestDiagnostic.mismatchField == ManifestIdentityMismatchField::AppId);
    assert(manifestDiagnostic.resultCode == ProjectErrorCode::ManifestIdentityAppIdMismatch);
    assert(std::strcmp(manifestDiagnostic.mismatchExpected, "com.example.hello") == 0);
    assert(std::strcmp(manifestDiagnostic.mismatchActual, "com.example.other") == 0);

    std::string descriptiveManifestBytes = validManifestBytes;
    const std::string oldDescription = "Minimal guideXOS Native GUI application.";
    size_t descriptionAt = descriptiveManifestBytes.find(oldDescription);
    assert(descriptionAt != std::string::npos);
    descriptiveManifestBytes.replace(descriptionAt, oldDescription.size(), "A different descriptive sentence.");
    ApplicationManifest parsedDescriptiveManifest = {};
    assert(ParseApplicationManifest(descriptiveManifestBytes.data(), static_cast<uint32_t>(descriptiveManifestBytes.size()), &parsedDescriptiveManifest, &error));
    assert(ValidateApplicationManifestIdentity(parsedDescriptiveManifest, loaded.project, nullptr, &manifestDiagnostic));

    std::string independentManifestBuffer = validManifestBytes;
    ApplicationManifest parsedIndependentManifest = {};
    assert(ParseApplicationManifest(independentManifestBuffer.data(), static_cast<uint32_t>(independentManifestBuffer.size()), &parsedIndependentManifest, &error));
    assert(ValidateApplicationManifestIdentity(parsedIndependentManifest, loaded.project, nullptr, &manifestDiagnostic));
    ApplicationManifest staleTailManifest = parsedManifestA;
    staleTailManifest.id[std::strlen(staleTailManifest.id) + 1] = 'x';
    staleTailManifest.entries[0].path[std::strlen(staleTailManifest.entries[0].path) + 1] = 'x';
    assert(ValidateApplicationManifestIdentity(staleTailManifest, loaded.project, nullptr, &manifestDiagnostic));
    ApplicationManifest paddingVariantManifest = parsedManifestA;
    const size_t manifestLogicalEnd = offsetof(ApplicationManifest, hasEntry) + sizeof(paddingVariantManifest.hasEntry);
    assert(sizeof(paddingVariantManifest) > manifestLogicalEnd);
    std::memset(reinterpret_cast<unsigned char*>(&paddingVariantManifest) + manifestLogicalEnd,
                0xA5, sizeof(paddingVariantManifest) - manifestLogicalEnd);
    assert(ValidateApplicationManifestIdentity(paddingVariantManifest, loaded.project, nullptr, &manifestDiagnostic));

    Project differentExpectedProject = loaded.project;
    std::strcpy(differentExpectedProject.projectId, "com.example.other");
    assert(!ValidateApplicationManifestIdentity(parsedManifestA, differentExpectedProject, nullptr, &manifestDiagnostic));
    assert(manifestDiagnostic.mismatchField == ManifestIdentityMismatchField::AppId);
    assert(std::strcmp(manifestDiagnostic.mismatchExpected, "com.example.other") == 0);
    assert(std::strcmp(manifestDiagnostic.mismatchActual, "com.example.hello") == 0);

    std::string differentTargetBytes = validManifestBytes;
    const std::string validTargetPath = "bin/amd64/hello-guidexos.elf";
    size_t targetPathAt = differentTargetBytes.find(validTargetPath);
    assert(targetPathAt != std::string::npos);
    differentTargetBytes.replace(targetPathAt, validTargetPath.size(), "bin/amd64/other.elf");
    ApplicationManifest differentTargetManifest = {};
    assert(ParseApplicationManifest(differentTargetBytes.data(), static_cast<uint32_t>(differentTargetBytes.size()), &differentTargetManifest, &error));
    assert(!ValidateApplicationManifestIdentity(differentTargetManifest, loaded.project, nullptr, &manifestDiagnostic));
    assert(manifestDiagnostic.mismatchField == ManifestIdentityMismatchField::Path);
    assert(manifestDiagnostic.resultCode == ProjectErrorCode::ManifestIdentityPathMismatch);

    ManifestValidationGeneration validationGeneration = {};
    validationGeneration.requestId = 7;
    validationGeneration.requestGeneration = 7;
    validationGeneration.candidateId = 11;
    validationGeneration.candidateGeneration = 7;
    validationGeneration.expectedIdentityGeneration = 6;
    validationGeneration.parsedIdentityGeneration = 7;
    assert(!ValidateApplicationManifestIdentity(parsedManifestA, loaded.project, &validationGeneration, &manifestDiagnostic));
    assert(manifestDiagnostic.resultCode == ProjectErrorCode::ManifestExpectedGenerationStale);
    validationGeneration.expectedIdentityGeneration = 7;
    validationGeneration.candidateGeneration = 8;
    assert(!ValidateApplicationManifestIdentity(parsedManifestA, loaded.project, &validationGeneration, &manifestDiagnostic));
    assert(manifestDiagnostic.resultCode == ProjectErrorCode::ManifestCandidateGenerationStale);
    validationGeneration.candidateGeneration = 7;
    validationGeneration.parsedIdentityGeneration = 6;
    assert(!ValidateApplicationManifestIdentity(parsedManifestA, loaded.project, &validationGeneration, &manifestDiagnostic));
    assert(manifestDiagnostic.resultCode == ProjectErrorCode::ManifestParsedGenerationStale);

    std::string truncatedIdentityManifest = validManifestBytes;
    idAt = truncatedIdentityManifest.find(validId);
    assert(idAt != std::string::npos);
    truncatedIdentityManifest.replace(idAt, validId.size(), std::string(kMaxProjectIdBytes, 'a'));
    ApplicationManifest truncatedIdentity = {};
    assert(!ParseApplicationManifest(truncatedIdentityManifest.data(), static_cast<uint32_t>(truncatedIdentityManifest.size()), &truncatedIdentity, &error));
    assert(error == ProjectErrorCode::ManifestStringTruncated);

    for (uint32_t iteration = 0; iteration < 1000; ++iteration) {
        ApplicationManifest repeatedManifest = {};
        assert(ParseApplicationManifest(validManifestBytes.data(), static_cast<uint32_t>(validManifestBytes.size()), &repeatedManifest, &error));
        assert(ValidateApplicationManifestIdentity(repeatedManifest, loaded.project, nullptr, &manifestDiagnostic));
        assert(std::strcmp(repeatedManifest.id, parsedManifestA.id) == 0);
        assert(std::strcmp(repeatedManifest.displayName, parsedManifestA.displayName) == 0);
        assert(std::strcmp(repeatedManifest.entries[0].architecture, parsedManifestA.entries[0].architecture) == 0);
        assert(std::strcmp(repeatedManifest.entries[0].path, parsedManifestA.entries[0].path) == 0);
        if (iteration == 499) {
            assert(ParseApplicationManifest(differentManifestBytes.data(), static_cast<uint32_t>(differentManifestBytes.size()), &parsedManifestB, &error));
            assert(ParseApplicationManifest(validManifestBytes.data(), static_cast<uint32_t>(validManifestBytes.size()), &repeatedManifest, &error));
            assert(ValidateApplicationManifestIdentity(repeatedManifest, loaded.project, nullptr, &manifestDiagnostic));
        }
    }
    assert(stableManifestHash == ComputeManifestContentHashFnv1a64(validManifestBytes.data(), static_cast<uint32_t>(validManifestBytes.size())));

    ProjectLoadScratch firstLoadScratch = {};
    ProjectLoadScratch secondLoadScratch = {};
    ProjectOperationResult firstScratchLoad;
    ProjectOperationResult secondScratchLoad;
    assert(LoadProject(fileSystem, generatedRoot.string().c_str(), &firstScratchLoad, &firstLoadScratch));
    assert(LoadProject(fileSystem, generatedRoot.string().c_str(), &secondScratchLoad, &secondLoadScratch));
    assert(firstLoadScratch.manifest.id[0] != '\0' && secondLoadScratch.manifest.id[0] != '\0');
    assert(std::strcmp(firstLoadScratch.manifest.id, secondLoadScratch.manifest.id) == 0);
    assert(std::strcmp(firstLoadScratch.manifestPath, secondLoadScratch.manifestPath) == 0);
    context.partialManifestRead = true;
    ProjectOperationResult partialManifestLoad;
    assert(!LoadProject(fileSystem, generatedRoot.string().c_str(), &partialManifestLoad, &firstLoadScratch));
    context.partialManifestRead = false;
    assert(partialManifestLoad.error == ProjectErrorCode::ManifestReadPartial);
    assert(partialManifestLoad.manifestDiagnostic.manifestExpectedSize == partialManifestLoad.manifestDiagnostic.manifestBytesRead + 1);
    assert(partialManifestLoad.manifestDiagnostic.resultCode == ProjectErrorCode::ManifestReadPartial);

    ProjectOperationResult missingProject;
    assert(!LoadProject(fileSystem, (testRoot / "does-not-exist" / "guidexos.project").string().c_str(), &missingProject));
    assert(missingProject.error == ProjectErrorCode::ParentNotFound);
    const std::string generatedMain = readAll(generatedRoot / "src" / "main.cpp");
    assert(generatedMain.find("gx_main") != std::string::npos);
    assert(generatedMain.find("request_window_ex") != std::string::npos);
    assert(generatedMain.find("Welcome to ") != std::string::npos);
    assert(generatedMain.find("D:\\dev\\") == std::string::npos);
    const std::string generatedBuild = readAll(generatedRoot / "build.ps1");
    assert(generatedBuild.find("GUIDEXOS_NATIVE_BUILD_RECIPE_V1") != std::string::npos);
    assert(generatedBuild.find("ServerRoot") == std::string::npos);
    assert(generatedBuild.find("PackageRoot") == std::string::npos);
    assert(generatedBuild.find("Join-Path $BuildRoot (\"bin\\\" + $TargetArchitecture)") != std::string::npos);
    assert(generatedBuild.find("ValidateSet(\"amd64\",\"arm64\")") != std::string::npos);
    assert(generatedBuild.find("aarch64-none-elf") != std::string::npos);
    assert(generatedBuild.find("aarch64elf") != std::string::npos);
    assert(generatedBuild.find("D:\\dev\\guideXOSServer") == std::string::npos);

    static WorkspaceController controller;
    WorkspaceControllerInit(&controller, fileSystem);
    context.controller = &controller;
    context.attemptReentrantOpen = true;
    WorkspaceControllerSetProjectOpenObserver(&controller, projectOpenObserver, &context);
    assert(WorkspaceControllerOpenProject(&controller, generatedRoot.string().c_str()));
    assert(!context.reentrantOpenAccepted);
    assert(context.reentrantOpenError == ProjectErrorCode::LoadInProgress);
    assert(controller.model.hasProject);
    assert(std::strcmp(controller.model.project.projectId, request.projectId) == 0);
    assert(context.lastProjectState == WorkspaceProjectOpenState::Ready);
    assert(context.lastProjectRequestId == 1);
    assert(context.lastProjectGeneration == controller.model.projectGeneration);
    assert(context.lastCandidateId != 0);
    assert(context.lastRefreshGeneration != 0);
    assert(!context.candidateVisibleBeforeCommit);

    const uint64_t activeProjectGenerationBeforePartialOpen = controller.model.projectGeneration;
    char activeProjectIdBeforePartialOpen[kMaxProjectIdBytes] = {};
    std::strncpy(activeProjectIdBeforePartialOpen, controller.model.project.projectId,
                 sizeof(activeProjectIdBeforePartialOpen) - 1);
    context.partialManifestRead = true;
    const bool partialOpenAccepted = WorkspaceControllerOpenProject(&controller, generatedRoot.string().c_str());
    context.partialManifestRead = false;
    assert(!partialOpenAccepted);
    assert(controller.lastProjectError == ProjectErrorCode::ManifestReadPartial);
    assert(context.lastProjectState == WorkspaceProjectOpenState::Failed);
    assert(context.lastProjectRequestId == 2);
    assert(controller.model.hasProject);
    assert(controller.model.projectGeneration == activeProjectGenerationBeforePartialOpen);
    assert(std::strcmp(controller.model.project.projectId, activeProjectIdBeforePartialOpen) == 0);
    assert(controller.lastManifestDiagnostic.resultCode == ProjectErrorCode::ManifestReadPartial);
    assert(WorkspaceControllerOpenProject(&controller, generatedRoot.string().c_str()));
    assert(context.lastProjectRequestId == 3);
    assert(controller.model.hasProject);
    assert(std::strcmp(controller.model.project.projectId, request.projectId) == 0);
    assert(WorkspaceControllerOpenDocument(&controller, "src/main.cpp"));
    char oldRoot[kMaxPathBytes] = {};
    std::strcpy(oldRoot, controller.model.rootPath);
    const uint64_t oldDocumentId = WorkspaceControllerActiveDocument(&controller)->documentId;
    const uint64_t oldProjectGeneration = controller.model.projectGeneration;
    context.failList = true;
    assert(!WorkspaceControllerOpenProject(&controller, generatedRoot.string().c_str()));
    context.failList = false;
    assert(controller.lastProjectError == ProjectErrorCode::RequiredFileMissing);
    assert(context.lastProjectState == WorkspaceProjectOpenState::Failed);
    assert(context.lastProjectRequestId == 4);
    assert(controller.model.hasProject && std::strcmp(controller.model.rootPath, oldRoot) == 0);
    assert(controller.model.projectGeneration == oldProjectGeneration);
    assert(WorkspaceControllerActiveDocument(&controller) != nullptr &&
           WorkspaceControllerActiveDocument(&controller)->documentId == oldDocumentId);
    fs::create_directories(testRoot / "invalid-project", ec);
    std::ofstream(testRoot / "invalid-project" / "guidexos.project") << "";
    assert(!WorkspaceControllerOpenProject(&controller, (testRoot / "invalid-project").string().c_str()));
    assert(controller.lastProjectError == ProjectErrorCode::MalformedJson);
    assert(controller.model.hasProject && std::strcmp(controller.model.rootPath, oldRoot) == 0);
    assert(WorkspaceControllerActiveDocument(&controller) != nullptr &&
           WorkspaceControllerActiveDocument(&controller)->documentId == oldDocumentId);
    assert(WorkspaceControllerOpenDocument(&controller, "guidexos.project"));

    const fs::path extraSource = generatedRoot / "src" / "extra.cpp";
    std::ofstream(extraSource) << "int extra_value = 1;\n";
    assert(WorkspaceControllerOpenDocument(&controller, "src/extra.cpp"));
    fs::remove(extraSource, ec);
    assert(WorkspaceControllerRefresh(&controller));
    assert(!WorkspaceControllerOpenDocument(&controller, "src/extra.cpp"));
    assert(controller.lastError == ModelErrorCode::ReadFailed);
    std::ofstream(extraSource) << "int extra_value = 2;\n";
    assert(WorkspaceControllerCloseDocument(&controller, controller.model.activeDocument, CloseDecision::Discard));
    assert(WorkspaceControllerOpenDocument(&controller, "src/extra.cpp"));

    const std::string mainBeforeRemoval = generatedMain;
    fs::remove(generatedRoot / "src" / "main.cpp", ec);
    assert(WorkspaceControllerRefresh(&controller));
    assert(!WorkspaceControllerOpenDocument(&controller, "src/main.cpp"));
    assert(controller.lastError == ModelErrorCode::ReadFailed);
    std::ofstream restoredMain(generatedRoot / "src" / "main.cpp", std::ios::binary);
    restoredMain.write(mainBeforeRemoval.data(), static_cast<std::streamsize>(mainBeforeRemoval.size()));
    restoredMain.close();
    assert(WorkspaceControllerOpenDocument(&controller, "src/main.cpp"));
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
