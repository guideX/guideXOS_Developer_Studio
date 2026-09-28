#include "developer_studio_projects.h"
#include "developer_studio_workspace.h"

#include "test_check.h"
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
    uint32_t readyCount = 0;
    uint32_t failedCount = 0;
    WorkspaceProjectOpenState lastProjectState = WorkspaceProjectOpenState::Idle;
    uint64_t lastProjectRequestId = 0;
    uint64_t lastProjectGeneration = 0;
    uint64_t lastCandidateId = 0;
    uint64_t lastRefreshGeneration = 0;
    ProjectErrorCode lastProjectError = ProjectErrorCode::None;
    WorkspaceController* controller = nullptr;
    enum class ReentryPoint { None, LoadStarted, ProjectMetadataRead, ApplicationManifestRead, RefreshList };
    ReentryPoint reentryPoint = ReentryPoint::None;
    bool reentryAttempted = false;
    bool reentrantOpenAccepted = false;
    ProjectErrorCode reentrantOpenError = ProjectErrorCode::None;
    std::string reentryProjectPath;
    bool candidateVisibleBeforeCommit = false;
    bool invalidateRequestGenerationOnLoaded = false;
    bool staleOwnerCheckObserved = false;
    bool staleFailureRequestGenerationMatched = true;
    bool staleFailureCandidateGenerationMatched = true;
    bool staleFailureOwnerMatches = true;
    WorkspaceProjectLoadOwnershipResult failedOwnershipResult = WorkspaceProjectLoadOwnershipResult::Current;
    WorkspaceProjectLoadOwnershipResult mismatchedCheckedRequestResult = WorkspaceProjectLoadOwnershipResult::Current;
    enum class OwnerMutation {
        None, RequestId, RequestGeneration, TransactionId, TransactionGeneration,
        CandidateId, CandidateGeneration, CandidateProjectGeneration, RefreshGeneration,
        ActiveProjectId, ActiveProjectGeneration, ControllerProgress
    };
    OwnerMutation ownerMutation = OwnerMutation::None;
    bool ownerMutationArmed = false;
    bool ownerMutationApplied = false;
    WorkspaceProjectLoadCheckpoint checkpoints[64] = {};
    WorkspaceProjectLoadOwnershipResult checkpointResults[64] = {};
    uint32_t checkpointCount = 0;
};

static void projectLoadTrace(void* userData, const WorkspaceProjectOpenEvent& event) {
    TestContext* context = static_cast<TestContext*>(userData);
    if (!context || context->checkpointCount >= 64) return;
    context->checkpoints[context->checkpointCount] = event.checkpoint;
    context->checkpointResults[context->checkpointCount] = event.ownershipResult;
    ++context->checkpointCount;
}

static void attemptNestedProjectOpen(TestContext* context, const char* caller) {
    if (!context || !context->controller || context->reentryAttempted) return;
    context->reentryAttempted = true;
    context->reentrantOpenAccepted = WorkspaceControllerOpenProjectFrom(
        context->controller, context->reentryProjectPath.c_str(), caller);
    context->reentrantOpenError = context->controller->lastProjectError;
}

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
    if (context && context->reentryPoint == TestContext::ReentryPoint::RefreshList)
        attemptNestedProjectOpen(context, "test_refresh_callback");
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
    if (event.state == WorkspaceProjectOpenState::Ready) ++context->readyCount;
    if (event.state == WorkspaceProjectOpenState::Failed) ++context->failedCount;
    context->lastProjectState = event.state;
    context->lastProjectRequestId = event.requestId;
    context->lastProjectGeneration = event.activeProjectGeneration;
    context->lastCandidateId = event.candidateId;
    context->lastRefreshGeneration = event.refreshGeneration;
    context->lastProjectError = event.error;
    if (event.state == WorkspaceProjectOpenState::CandidateAllocated && context->controller) {
        context->candidateVisibleBeforeCommit = context->controller->model.hasProject;
    }
    if (event.state == WorkspaceProjectOpenState::LoadStarted &&
        context->reentryPoint == TestContext::ReentryPoint::LoadStarted)
        attemptNestedProjectOpen(context, "test_load_started_observer");
    if (event.state == WorkspaceProjectOpenState::Loaded && context->invalidateRequestGenerationOnLoaded &&
        context->controller) {
        context->invalidateRequestGenerationOnLoaded = false;
        ++context->controller->projectOpenRequestGeneration;
    }
    if (event.state == WorkspaceProjectOpenState::Failed && context->staleOwnerCheckObserved) {
        context->staleFailureRequestGenerationMatched = event.requestGenerationMatches;
        context->staleFailureCandidateGenerationMatched = event.candidateGenerationMatches;
        context->staleFailureOwnerMatches = event.transactionOwnerMatches;
    }
    if (event.state == WorkspaceProjectOpenState::Failed)
        context->failedOwnershipResult = event.ownershipResult;
    if (event.state == WorkspaceProjectOpenState::Loaded && context->controller) {
        if (context->ownerMutationArmed && context->ownerMutation == TestContext::OwnerMutation::RequestId)
            ++context->controller->projectOpenRequestId;
        else if (context->ownerMutationArmed && context->ownerMutation == TestContext::OwnerMutation::RequestGeneration)
            ++context->controller->projectOpenRequestGeneration;
        else if (context->ownerMutationArmed && context->ownerMutation == TestContext::OwnerMutation::TransactionId)
            ++context->controller->projectOpenTransactionId;
        else if (context->ownerMutationArmed && context->ownerMutation == TestContext::OwnerMutation::TransactionGeneration)
            ++context->controller->projectOpenTransactionGeneration;
        else if (context->ownerMutationArmed && context->ownerMutation == TestContext::OwnerMutation::CandidateId)
            ++context->controller->projectOpenCandidateId;
        else if (context->ownerMutationArmed && context->ownerMutation == TestContext::OwnerMutation::CandidateGeneration)
            ++context->controller->projectOpenCandidateGeneration;
        else if (context->ownerMutationArmed && context->ownerMutation == TestContext::OwnerMutation::CandidateProjectGeneration)
            ++context->controller->projectOpenCandidateProjectGeneration;
        else if (context->ownerMutationArmed && context->ownerMutation == TestContext::OwnerMutation::RefreshGeneration)
            ++context->controller->projectOpenRefreshGeneration;
        else if (context->ownerMutationArmed && context->ownerMutation == TestContext::OwnerMutation::ActiveProjectId &&
                 context->controller->model.hasProject)
            std::strcpy(context->controller->model.project.projectId, "com.example.changed");
        else if (context->ownerMutationArmed && context->ownerMutation == TestContext::OwnerMutation::ActiveProjectGeneration)
            ++context->controller->model.projectGeneration;
        else if (context->ownerMutationArmed && context->ownerMutation == TestContext::OwnerMutation::ControllerProgress)
            context->controller->projectOpenInProgress = false;
        if (context->ownerMutationArmed && context->ownerMutation != TestContext::OwnerMutation::None)
            context->ownerMutationApplied = true;
        if (context->ownerMutation == TestContext::OwnerMutation::None)
            context->mismatchedCheckedRequestResult = WorkspaceControllerCheckProjectLoadOwnership(
                context->controller, event.checkedRequestId + 1);
    }
}

static bool readFile(void* userData, const char* path, char* buffer, uint32_t capacity, uint32_t* outBytes) {
    TestContext* context = static_cast<TestContext*>(userData);
    const std::string pathText(path ? path : "");
    if (context && context->reentryPoint == TestContext::ReentryPoint::ProjectMetadataRead &&
        pathText.find("guidexos.project") != std::string::npos)
        attemptNestedProjectOpen(context, "test_project_metadata_read");
    if (context && context->reentryPoint == TestContext::ReentryPoint::ApplicationManifestRead &&
        pathText.find("app/app.json") != std::string::npos)
        attemptNestedProjectOpen(context, "test_application_manifest_read");
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

static bool traceHasCheckpoint(const TestContext& context, WorkspaceProjectLoadCheckpoint checkpoint,
                               WorkspaceProjectLoadOwnershipResult result) {
    for (uint32_t i = 0; i < context.checkpointCount; ++i) {
        if (context.checkpoints[i] != checkpoint) continue;
        if (context.checkpointResults[i] == result) return true;
    }
    return false;
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

static void runProjectReentryCase(const fs::path& projectRoot,
                                  TestContext::ReentryPoint reentryPoint,
                                  const char* expectedReentryCaller) {
    static WorkspaceController controller;
    static WorkspaceController relaunchedController;
    TestContext context;
    context.reentryPoint = reentryPoint;
    context.reentryProjectPath = projectRoot.string();
    ProjectFileSystem fileSystem = {
        &context, statFile, listFiles, readFile, writeFile, createDirectory, removePath
    };
    WorkspaceControllerInit(&controller, fileSystem);
    context.controller = &controller;
    WorkspaceControllerSetProjectOpenObserver(&controller, projectOpenObserver, &context);
    WorkspaceControllerSetProjectLoadTrace(&controller, projectLoadTrace, &context);

    TEST_CHECK(WorkspaceControllerOpenProjectFrom(&controller, projectRoot.string().c_str(),
                                                  "test_outer_transaction"));
    TEST_CHECK(context.reentryAttempted);
    TEST_CHECK(!context.reentrantOpenAccepted);
    TEST_CHECK(context.reentrantOpenError == ProjectErrorCode::LoadInProgress);
    TEST_CHECK(controller.projectOpenReentryCount == 1);
    TEST_CHECK(controller.projectOpenMaximumEntryDepth == 2);
    TEST_CHECK(std::strcmp(controller.projectOpenLastReentryCaller, expectedReentryCaller) == 0);
    TEST_CHECK(controller.projectOpenState == WorkspaceProjectOpenState::Ready);
    TEST_CHECK(controller.projectOpenRequestId == 1);
    TEST_CHECK(controller.projectOpenManifestValidationCount == 1);
    TEST_CHECK(controller.projectOpenRefreshCount == 1);
    TEST_CHECK(controller.projectOpenCommitCount == 1);
    TEST_CHECK(controller.projectOpenReleaseCount == 1);
    TEST_CHECK(!controller.projectOpenInProgress);
    TEST_CHECK(controller.lastProjectError == ProjectErrorCode::None);
    TEST_CHECK(context.readyCount == 1 && context.failedCount == 0);
    TEST_CHECK(context.mismatchedCheckedRequestResult == WorkspaceProjectLoadOwnershipResult::RequestIdMismatch);
    TEST_CHECK(traceHasCheckpoint(context, WorkspaceProjectLoadCheckpoint::RequestAccepted,
                                  WorkspaceProjectLoadOwnershipResult::Current));
    TEST_CHECK(traceHasCheckpoint(context, WorkspaceProjectLoadCheckpoint::TransactionCreated,
                                  WorkspaceProjectLoadOwnershipResult::Current));
    TEST_CHECK(traceHasCheckpoint(context, WorkspaceProjectLoadCheckpoint::ProjectMetadataValidated,
                                  WorkspaceProjectLoadOwnershipResult::Current));
    TEST_CHECK(traceHasCheckpoint(context, WorkspaceProjectLoadCheckpoint::ApplicationManifestValidated,
                                  WorkspaceProjectLoadOwnershipResult::Current));
    TEST_CHECK(traceHasCheckpoint(context, WorkspaceProjectLoadCheckpoint::Loaded,
                                  WorkspaceProjectLoadOwnershipResult::Current));
    TEST_CHECK(traceHasCheckpoint(context, WorkspaceProjectLoadCheckpoint::BeforeRefresh,
                                  WorkspaceProjectLoadOwnershipResult::Current));
    TEST_CHECK(traceHasCheckpoint(context, WorkspaceProjectLoadCheckpoint::AfterRefresh,
                                  WorkspaceProjectLoadOwnershipResult::Current));
    TEST_CHECK(traceHasCheckpoint(context, WorkspaceProjectLoadCheckpoint::BeforeCommit,
                                  WorkspaceProjectLoadOwnershipResult::Current));
    TEST_CHECK(traceHasCheckpoint(context, WorkspaceProjectLoadCheckpoint::AfterCommit,
                                  WorkspaceProjectLoadOwnershipResult::Current));
    TEST_CHECK(traceHasCheckpoint(context, WorkspaceProjectLoadCheckpoint::Ready,
                                  WorkspaceProjectLoadOwnershipResult::Current));
    TEST_CHECK(traceHasCheckpoint(context, WorkspaceProjectLoadCheckpoint::ObserverReturn,
                                  WorkspaceProjectLoadOwnershipResult::Current));
    TEST_CHECK(traceHasCheckpoint(context, WorkspaceProjectLoadCheckpoint::TransactionRelease,
                                  WorkspaceProjectLoadOwnershipResult::Current));
    TEST_CHECK(traceHasCheckpoint(context, WorkspaceProjectLoadCheckpoint::TransactionReleased,
                                  WorkspaceProjectLoadOwnershipResult::TransactionNotActive));
    TEST_CHECK(controller.model.hasProject);
    TEST_CHECK(controller.model.projectGeneration == controller.projectOpenCandidateProjectGeneration);
    TEST_CHECK(controller.projectOpenTransactionId != 0);
    const uint64_t firstTransactionId = controller.projectOpenTransactionId;
    const uint64_t firstTransactionGeneration = controller.projectOpenTransactionGeneration;

    TEST_CHECK(WorkspaceControllerOpenProjectFrom(&controller, projectRoot.string().c_str(),
                                                  "test_immediate_new_request"));
    TEST_CHECK(controller.projectOpenRequestId == 2);
    TEST_CHECK(controller.projectOpenTransactionId > firstTransactionId);
    TEST_CHECK(controller.projectOpenTransactionGeneration > firstTransactionGeneration);
    TEST_CHECK(controller.projectOpenState == WorkspaceProjectOpenState::Ready);
    TEST_CHECK(!controller.projectOpenInProgress);
    TEST_CHECK(controller.model.projectGeneration == controller.projectOpenCandidateProjectGeneration);
    TEST_CHECK(context.readyCount == 2 && context.failedCount == 0);

    WorkspaceControllerInit(&relaunchedController, fileSystem);
    context.controller = &relaunchedController;
    WorkspaceControllerSetProjectOpenObserver(&relaunchedController, projectOpenObserver, &context);
    WorkspaceControllerSetProjectLoadTrace(&relaunchedController, projectLoadTrace, &context);
    TEST_CHECK(WorkspaceControllerOpenProjectFrom(&relaunchedController,
                                                  projectRoot.string().c_str(), "test_app_relaunch"));
    TEST_CHECK(relaunchedController.projectOpenRequestId == 1);
    TEST_CHECK(relaunchedController.projectOpenTransactionId > controller.projectOpenTransactionId);
    TEST_CHECK(relaunchedController.projectOpenTransactionGeneration >
               controller.projectOpenTransactionGeneration);
    TEST_CHECK(relaunchedController.projectOpenState == WorkspaceProjectOpenState::Ready);
    TEST_CHECK(!relaunchedController.projectOpenInProgress);
    TEST_CHECK(relaunchedController.model.projectGeneration ==
               relaunchedController.projectOpenCandidateProjectGeneration);
    TEST_CHECK(relaunchedController.projectOpenReleaseCount == 1);
    TEST_CHECK(context.readyCount == 3 && context.failedCount == 0);
}

static void runProjectStaleGenerationCase(const fs::path& projectRoot) {
    static WorkspaceController controller;
    TestContext context;
    ProjectFileSystem fileSystem = {
        &context, statFile, listFiles, readFile, writeFile, createDirectory, removePath
    };
    WorkspaceControllerInit(&controller, fileSystem);
    context.controller = &controller;
    WorkspaceControllerSetProjectOpenObserver(&controller, projectOpenObserver, &context);
    WorkspaceControllerSetProjectLoadTrace(&controller, projectLoadTrace, &context);

    TEST_CHECK(WorkspaceControllerOpenProjectFrom(&controller, projectRoot.string().c_str(),
                                                  "test_stale_generation_baseline"));
    TEST_CHECK(controller.projectOpenRequestId == 1);
    const uint64_t previousProjectGeneration = controller.model.projectGeneration;
    const uint64_t previousTransactionId = controller.projectOpenTransactionId;
    context.invalidateRequestGenerationOnLoaded = true;
    context.staleOwnerCheckObserved = true;

    TEST_CHECK(!WorkspaceControllerOpenProjectFrom(&controller, projectRoot.string().c_str(),
                                                   "test_stale_generation"));
    TEST_CHECK(context.lastProjectState == WorkspaceProjectOpenState::Failed);
    TEST_CHECK(context.lastProjectError == ProjectErrorCode::LoadInProgress);
    TEST_CHECK(controller.projectOpenRequestId == 2);
    TEST_CHECK(controller.projectOpenTransactionId > previousTransactionId);
    TEST_CHECK(context.staleFailureRequestGenerationMatched == false);
    TEST_CHECK(context.staleFailureCandidateGenerationMatched);
    TEST_CHECK(!context.staleFailureOwnerMatches);
    TEST_CHECK(context.failedOwnershipResult == WorkspaceProjectLoadOwnershipResult::RequestGenerationMismatch);
    TEST_CHECK(controller.projectOpenManifestValidationCount == 1);
    TEST_CHECK(controller.projectOpenRefreshCount == 0);
    TEST_CHECK(controller.projectOpenCommitCount == 0);
    TEST_CHECK(controller.projectOpenReleaseCount == 1);
    TEST_CHECK(!controller.projectOpenInProgress);
    TEST_CHECK(controller.model.hasProject);
    TEST_CHECK(controller.model.projectGeneration == previousProjectGeneration);
    TEST_CHECK(context.readyCount == 1 && context.failedCount == 1);

    context.staleOwnerCheckObserved = false;
    TEST_CHECK(WorkspaceControllerOpenProjectFrom(&controller, projectRoot.string().c_str(),
                                                  "test_after_stale_failure"));
    TEST_CHECK(controller.projectOpenRequestId == 3);
    TEST_CHECK(controller.projectOpenState == WorkspaceProjectOpenState::Ready);
    TEST_CHECK(!controller.projectOpenInProgress);
    TEST_CHECK(context.readyCount == 2 && context.failedCount == 1);
}

static void runProjectOwnerReasonCase(const fs::path& projectRoot,
                                      TestContext::OwnerMutation mutation,
                                      WorkspaceProjectLoadOwnershipResult expectedReason,
                                      bool establishActiveProject) {
    static WorkspaceController controller;
    TestContext context;
    context.ownerMutation = mutation;
    ProjectFileSystem fileSystem = {
        &context, statFile, listFiles, readFile, writeFile, createDirectory, removePath
    };
    WorkspaceControllerInit(&controller, fileSystem);
    context.controller = &controller;
    WorkspaceControllerSetProjectOpenObserver(&controller, projectOpenObserver, &context);
    WorkspaceControllerSetProjectLoadTrace(&controller, projectLoadTrace, &context);
    if (establishActiveProject)
        TEST_CHECK(WorkspaceControllerOpenProjectFrom(&controller, projectRoot.string().c_str(),
                                                      "test_owner_reason_baseline"));
    context.ownerMutationArmed = true;
    const uint64_t activeGeneration = controller.model.projectGeneration;
    char activeProjectId[kMaxProjectIdBytes] = {};
    if (controller.model.hasProject)
        std::strcpy(activeProjectId, controller.model.project.projectId);
    TEST_CHECK(!WorkspaceControllerOpenProjectFrom(&controller, projectRoot.string().c_str(),
                                                   "test_owner_reason_mismatch"));
    TEST_CHECK(context.ownerMutationApplied);
    TEST_CHECK(context.failedOwnershipResult == expectedReason);
    TEST_CHECK(controller.projectOpenReleaseCount == 1);
    TEST_CHECK(controller.projectOpenCommitCount == 0);
    TEST_CHECK(!controller.projectOpenInProgress);
    if (mutation == TestContext::OwnerMutation::ActiveProjectId)
        std::strcpy(controller.model.project.projectId, activeProjectId);
    if (mutation == TestContext::OwnerMutation::ActiveProjectGeneration)
        controller.model.projectGeneration = activeGeneration;
    TEST_CHECK(WorkspaceControllerCheckProjectLoadOwnership(&controller,
        controller.projectOpenRequestId) == WorkspaceProjectLoadOwnershipResult::TransactionNotActive);
}

int main(int argc, char** argv) {
    TEST_CHECK(ValidateProjectDisplayName("Hello guideXOS"));
    TEST_CHECK(!ValidateProjectDisplayName("bad\nname"));
    TEST_CHECK(ValidateProjectId("com.example.hello"));
    TEST_CHECK(!ValidateProjectId("Com.example.hello"));
    TEST_CHECK(!ValidateProjectId("com.example..hello"));
    TEST_CHECK(!ValidateProjectId("com.guidexos.custom"));
    TEST_CHECK(!ValidateProjectFolderName("../outside"));
    char derived[128] = {};
    TEST_CHECK(DeriveProjectFolderName("Hello guideXOS", derived, sizeof(derived)));
    TEST_CHECK(std::strcmp(derived, "hello-guidexos") == 0);
    char outputName[128] = {};
    TEST_CHECK(DeriveProjectOutputName(derived, outputName, sizeof(outputName)));
    TEST_CHECK(std::strcmp(outputName, "hello-guidexos-guidexos") == 0);

    Project project = makeProject();
    char serialized[kMaxProjectFileBytes] = {};
    uint32_t serializedBytes = 0;
    ProjectErrorCode error = ProjectErrorCode::None;
    TEST_CHECK(SerializeProjectMetadata(project, serialized, sizeof(serialized), &serializedBytes, &error));
    const char expected[] = "{\n  \"formatVersion\": 1,\n  \"projectId\": \"com.example.hello\",\n  \"displayName\": \"Hello guideXOS\",\n  \"projectKind\": \"native-gui-application\",\n  \"sourceRoot\": \"src\",\n  \"applicationManifest\": \"app/app.json\",\n  \"defaultTargetProfile\": \"guidexos.amd64.hosted.native\",\n  \"entryPoint\": \"gx_main\",\n  \"abi\": \"guidexos-c-abi-v1\",\n  \"architecture\": \"amd64\",\n  \"outputName\": \"hello-guidexos\"\n}\n";
    TEST_CHECK(std::string(serialized, serializedBytes) == expected);
    Project parsed = {};
    TEST_CHECK(ParseProjectMetadata(serialized, serializedBytes, &parsed, &error));
    TEST_CHECK(std::strcmp(parsed.projectId, project.projectId) == 0);
    std::string unsupported(serialized, serializedBytes);
    const std::string versionToken = "\"formatVersion\": 1";
    const size_t versionPosition = unsupported.find(versionToken);
    TEST_CHECK(versionPosition != std::string::npos);
    unsupported[versionPosition + versionToken.size() - 1] = '2';
    TEST_CHECK(!ParseProjectMetadata(unsupported.data(), static_cast<uint32_t>(unsupported.size()), &parsed, &error) && error == ProjectErrorCode::UnsupportedFormatVersion);
    std::string unknownTarget(serialized, serializedBytes);
    const std::string targetValue = "guidexos.amd64.hosted.native";
    const size_t targetPosition = unknownTarget.find(targetValue);
    TEST_CHECK(targetPosition != std::string::npos);
    unknownTarget.replace(targetPosition, targetValue.size(), "guidexos.amd64.unknown");
    TEST_CHECK(!ParseProjectMetadata(unknownTarget.data(), static_cast<uint32_t>(unknownTarget.size()), &parsed, &error) && error == ProjectErrorCode::UnknownTargetProfile);
    std::string unknownField(serialized, serializedBytes);
    const size_t objectEnd = unknownField.rfind("\n}");
    TEST_CHECK(objectEnd != std::string::npos);
    unknownField.insert(objectEnd, ",\n  \"futureMetadata\": true");
    TEST_CHECK(!ParseProjectMetadata(unknownField.data(), static_cast<uint32_t>(unknownField.size()), &parsed, &error) && error == ProjectErrorCode::UnknownField);
    // A rejected parse must not poison the next transaction with shared
    // writable parser state.
    TEST_CHECK(ParseProjectMetadata(serialized, serializedBytes, &parsed, &error));
    TEST_CHECK(std::strcmp(parsed.projectId, project.projectId) == 0);
    const char duplicateJson[] = "{\"formatVersion\":1,\"formatVersion\":1}";
    TEST_CHECK(!ParseProjectMetadata(duplicateJson, sizeof(duplicateJson) - 1, &parsed, &error) && error == ProjectErrorCode::DuplicateField);
    TEST_CHECK(!ParseProjectMetadata("{\"formatVersion\":2}", 19, &parsed, &error) && error == ProjectErrorCode::MissingField);
    TEST_CHECK(!ParseProjectMetadata("", 0, &parsed, &error) && error == ProjectErrorCode::MalformedJson);
    const char malformedJson[] = "{\"formatVersion\":1";
    TEST_CHECK(!ParseProjectMetadata(malformedJson, sizeof(malformedJson) - 1, &parsed, &error) && error == ProjectErrorCode::MalformedJson);
    std::string missingRequired(serialized, serializedBytes);
    const std::string outputField = ",\n  \"outputName\": \"hello-guidexos\"";
    const size_t outputFieldPosition = missingRequired.find(outputField);
    TEST_CHECK(outputFieldPosition != std::string::npos);
    missingRequired.erase(outputFieldPosition, outputField.size());
    TEST_CHECK(!ParseProjectMetadata(missingRequired.data(), static_cast<uint32_t>(missingRequired.size()), &parsed, &error) && error == ProjectErrorCode::MissingField);
    std::string invalidType(serialized, serializedBytes);
    const size_t versionTypePosition = invalidType.find("\"formatVersion\": 1");
    TEST_CHECK(versionTypePosition != std::string::npos);
    invalidType.replace(versionTypePosition, std::strlen("\"formatVersion\": 1"), "\"formatVersion\": \"1\"");
    TEST_CHECK(!ParseProjectMetadata(invalidType.data(), static_cast<uint32_t>(invalidType.size()), &parsed, &error) && error == ProjectErrorCode::MalformedJson);
    std::string invalidArchitecture(serialized, serializedBytes);
    const size_t architecturePosition = invalidArchitecture.find("\"architecture\": \"amd64\"");
    TEST_CHECK(architecturePosition != std::string::npos);
    invalidArchitecture.replace(architecturePosition, std::strlen("\"architecture\": \"amd64\""), "\"architecture\": \"i386\"");
    TEST_CHECK(!ParseProjectMetadata(invalidArchitecture.data(), static_cast<uint32_t>(invalidArchitecture.size()), &parsed, &error) && error == ProjectErrorCode::InvalidArchitecture);
    std::string invalidPath(serialized, serializedBytes);
    const size_t sourcePosition = invalidPath.find("\"sourceRoot\": \"src\"");
    TEST_CHECK(sourcePosition != std::string::npos);
    invalidPath.replace(sourcePosition, std::strlen("\"sourceRoot\": \"src\""), "\"sourceRoot\": \"../src\"");
    TEST_CHECK(!ParseProjectMetadata(invalidPath.data(), static_cast<uint32_t>(invalidPath.size()), &parsed, &error) && error == ProjectErrorCode::InvalidRelativePath);
    std::string boundary(kMaxProjectFileBytes, ' ');
    TEST_CHECK(!ParseProjectMetadata(boundary.data(), static_cast<uint32_t>(boundary.size()), &parsed, &error) && error != ProjectErrorCode::ProjectFileTooLarge);
    char oversized[kMaxProjectFileBytes + 1] = {};
    TEST_CHECK(!ParseProjectMetadata(oversized, sizeof(oversized), &parsed, &error) && error == ProjectErrorCode::ProjectFileTooLarge);
    std::string longDisplay(kMaxProjectDisplayNameBytes - 1, 'a');
    TEST_CHECK(ValidateProjectDisplayName(longDisplay.c_str()));
    longDisplay.push_back('a');
    TEST_CHECK(!ValidateProjectDisplayName(longDisplay.c_str()));

    Project bareProject = makeProject();
    std::strcpy(bareProject.targetProfileId, BareMetalTargetProfile().id);
    std::strcpy(bareProject.sourceEntry, "main.cpp");
    TEST_CHECK(SerializeProjectMetadata(bareProject, serialized, sizeof(serialized), &serializedBytes, &error));
    TEST_CHECK(std::string(serialized, serializedBytes).find("\"sourceEntry\": \"main.cpp\"") != std::string::npos);
    TEST_CHECK(ParseProjectMetadata(serialized, serializedBytes, &parsed, &error));
    TEST_CHECK(std::strcmp(parsed.targetProfileId, BareMetalTargetProfile().id) == 0);
    TEST_CHECK(std::strcmp(parsed.sourceEntry, "main.cpp") == 0);

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
    TEST_CHECK(created.success && created.project.valid);
    const fs::path generatedRoot = testRoot / "parent-one" / "hello";
    TEST_CHECK(fs::exists(generatedRoot / "guidexos.project"));
    TEST_CHECK(fs::exists(generatedRoot / "app" / "app.json"));
    TEST_CHECK(fs::exists(generatedRoot / "src" / "main.cpp"));
    ProjectOperationResult loaded;
    TEST_CHECK(LoadProject(fileSystem, generatedRoot.string().c_str(), &loaded));
    TEST_CHECK(loaded.manifestDiagnostic.validationCount == 1);
    TEST_CHECK(std::strcmp(loaded.manifestDiagnostic.validationRole, "application") == 0);
    TEST_CHECK(std::strstr(loaded.manifestDiagnostic.projectMetadataPath, "guidexos.project") != nullptr);
    TEST_CHECK(std::strstr(loaded.manifestDiagnostic.manifestPath, "app/app.json") != nullptr);
    TEST_CHECK(LoadProject(fileSystem, (generatedRoot / "guidexos.project").string().c_str(), &loaded));
    TEST_CHECK(loaded.manifestDiagnostic.validationCount == 1);
    TEST_CHECK(std::strcmp(loaded.project.projectId, request.projectId) == 0);
    const std::string validManifestBytes = readAll(generatedRoot / "app" / "app.json");
    ApplicationManifest parsedManifestA = {};
    TEST_CHECK(ParseApplicationManifest(validManifestBytes.data(), static_cast<uint32_t>(validManifestBytes.size()), &parsedManifestA, &error));
    ManifestValidationDiagnostic manifestDiagnostic = {};
    TEST_CHECK(ValidateApplicationManifestIdentity(parsedManifestA, loaded.project, nullptr, &manifestDiagnostic));
    TEST_CHECK(manifestDiagnostic.resultCode == ProjectErrorCode::None);
    const uint64_t stableManifestHash = ComputeManifestContentHashFnv1a64(validManifestBytes.data(), static_cast<uint32_t>(validManifestBytes.size()));
    TEST_CHECK(stableManifestHash == ComputeManifestContentHashFnv1a64(validManifestBytes.data(), static_cast<uint32_t>(validManifestBytes.size())));

    // Production parser/validator repetition, including valid A -> different B -> valid A.
    std::string differentManifestBytes = validManifestBytes;
    const std::string validId = "com.example.hello";
    size_t idAt = differentManifestBytes.find(validId);
    TEST_CHECK(idAt != std::string::npos);
    differentManifestBytes.replace(idAt, validId.size(), "com.example.other");
    ApplicationManifest parsedManifestB = {};
    TEST_CHECK(ParseApplicationManifest(differentManifestBytes.data(), static_cast<uint32_t>(differentManifestBytes.size()), &parsedManifestB, &error));
    TEST_CHECK(!ValidateApplicationManifestIdentity(parsedManifestB, loaded.project, nullptr, &manifestDiagnostic));
    TEST_CHECK(manifestDiagnostic.mismatchField == ManifestIdentityMismatchField::AppId);
    TEST_CHECK(manifestDiagnostic.resultCode == ProjectErrorCode::ManifestIdentityAppIdMismatch);
    TEST_CHECK(std::strcmp(manifestDiagnostic.mismatchExpected, "com.example.hello") == 0);
    TEST_CHECK(std::strcmp(manifestDiagnostic.mismatchActual, "com.example.other") == 0);

    std::string descriptiveManifestBytes = validManifestBytes;
    const std::string oldDescription = "Minimal guideXOS Native GUI application.";
    size_t descriptionAt = descriptiveManifestBytes.find(oldDescription);
    TEST_CHECK(descriptionAt != std::string::npos);
    descriptiveManifestBytes.replace(descriptionAt, oldDescription.size(), "A different descriptive sentence.");
    ApplicationManifest parsedDescriptiveManifest = {};
    TEST_CHECK(ParseApplicationManifest(descriptiveManifestBytes.data(), static_cast<uint32_t>(descriptiveManifestBytes.size()), &parsedDescriptiveManifest, &error));
    TEST_CHECK(ValidateApplicationManifestIdentity(parsedDescriptiveManifest, loaded.project, nullptr, &manifestDiagnostic));

    std::string independentManifestBuffer = validManifestBytes;
    ApplicationManifest parsedIndependentManifest = {};
    TEST_CHECK(ParseApplicationManifest(independentManifestBuffer.data(), static_cast<uint32_t>(independentManifestBuffer.size()), &parsedIndependentManifest, &error));
    TEST_CHECK(ValidateApplicationManifestIdentity(parsedIndependentManifest, loaded.project, nullptr, &manifestDiagnostic));
    ApplicationManifest staleTailManifest = parsedManifestA;
    staleTailManifest.id[std::strlen(staleTailManifest.id) + 1] = 'x';
    staleTailManifest.entries[0].path[std::strlen(staleTailManifest.entries[0].path) + 1] = 'x';
    TEST_CHECK(ValidateApplicationManifestIdentity(staleTailManifest, loaded.project, nullptr, &manifestDiagnostic));
    ApplicationManifest paddingVariantManifest = parsedManifestA;
    const size_t manifestLogicalEnd = offsetof(ApplicationManifest, hasEntry) + sizeof(paddingVariantManifest.hasEntry);
    TEST_CHECK(sizeof(paddingVariantManifest) > manifestLogicalEnd);
    std::memset(reinterpret_cast<unsigned char*>(&paddingVariantManifest) + manifestLogicalEnd,
                0xA5, sizeof(paddingVariantManifest) - manifestLogicalEnd);
    TEST_CHECK(ValidateApplicationManifestIdentity(paddingVariantManifest, loaded.project, nullptr, &manifestDiagnostic));

    Project differentExpectedProject = loaded.project;
    std::strcpy(differentExpectedProject.projectId, "com.example.other");
    TEST_CHECK(!ValidateApplicationManifestIdentity(parsedManifestA, differentExpectedProject, nullptr, &manifestDiagnostic));
    TEST_CHECK(manifestDiagnostic.mismatchField == ManifestIdentityMismatchField::AppId);
    TEST_CHECK(std::strcmp(manifestDiagnostic.mismatchExpected, "com.example.other") == 0);
    TEST_CHECK(std::strcmp(manifestDiagnostic.mismatchActual, "com.example.hello") == 0);

    std::string differentTargetBytes = validManifestBytes;
    const std::string validTargetPath = "bin/amd64/hello-guidexos.elf";
    size_t targetPathAt = differentTargetBytes.find(validTargetPath);
    TEST_CHECK(targetPathAt != std::string::npos);
    differentTargetBytes.replace(targetPathAt, validTargetPath.size(), "bin/amd64/other.elf");
    ApplicationManifest differentTargetManifest = {};
    TEST_CHECK(ParseApplicationManifest(differentTargetBytes.data(), static_cast<uint32_t>(differentTargetBytes.size()), &differentTargetManifest, &error));
    TEST_CHECK(!ValidateApplicationManifestIdentity(differentTargetManifest, loaded.project, nullptr, &manifestDiagnostic));
    TEST_CHECK(manifestDiagnostic.mismatchField == ManifestIdentityMismatchField::Path);
    TEST_CHECK(manifestDiagnostic.resultCode == ProjectErrorCode::ManifestIdentityPathMismatch);

    ManifestValidationGeneration validationGeneration = {};
    validationGeneration.requestId = 7;
    validationGeneration.requestGeneration = 7;
    validationGeneration.candidateId = 11;
    validationGeneration.candidateGeneration = 7;
    validationGeneration.expectedIdentityGeneration = 6;
    validationGeneration.parsedIdentityGeneration = 7;
    TEST_CHECK(!ValidateApplicationManifestIdentity(parsedManifestA, loaded.project, &validationGeneration, &manifestDiagnostic));
    TEST_CHECK(manifestDiagnostic.resultCode == ProjectErrorCode::ManifestExpectedGenerationStale);
    validationGeneration.expectedIdentityGeneration = 7;
    validationGeneration.candidateGeneration = 0;
    TEST_CHECK(!ValidateApplicationManifestIdentity(parsedManifestA, loaded.project, &validationGeneration, &manifestDiagnostic));
    TEST_CHECK(manifestDiagnostic.resultCode == ProjectErrorCode::ManifestCandidateGenerationStale);
    validationGeneration.candidateGeneration = 7;
    validationGeneration.parsedIdentityGeneration = 6;
    TEST_CHECK(!ValidateApplicationManifestIdentity(parsedManifestA, loaded.project, &validationGeneration, &manifestDiagnostic));
    TEST_CHECK(manifestDiagnostic.resultCode == ProjectErrorCode::ManifestParsedGenerationStale);
    validationGeneration.requestGeneration = 19;
    validationGeneration.candidateGeneration = 8;
    validationGeneration.expectedIdentityGeneration = 8;
    validationGeneration.parsedIdentityGeneration = 8;
    TEST_CHECK(ValidateApplicationManifestIdentity(parsedManifestA, loaded.project,
                                                    &validationGeneration, &manifestDiagnostic));

    std::string truncatedIdentityManifest = validManifestBytes;
    idAt = truncatedIdentityManifest.find(validId);
    TEST_CHECK(idAt != std::string::npos);
    truncatedIdentityManifest.replace(idAt, validId.size(), std::string(kMaxProjectIdBytes, 'a'));
    ApplicationManifest truncatedIdentity = {};
    TEST_CHECK(!ParseApplicationManifest(truncatedIdentityManifest.data(), static_cast<uint32_t>(truncatedIdentityManifest.size()), &truncatedIdentity, &error));
    TEST_CHECK(error == ProjectErrorCode::ManifestStringTruncated);

    for (uint32_t iteration = 0; iteration < 1000; ++iteration) {
        ApplicationManifest repeatedManifest = {};
        TEST_CHECK(ParseApplicationManifest(validManifestBytes.data(), static_cast<uint32_t>(validManifestBytes.size()), &repeatedManifest, &error));
        TEST_CHECK(ValidateApplicationManifestIdentity(repeatedManifest, loaded.project, nullptr, &manifestDiagnostic));
        TEST_CHECK(std::strcmp(repeatedManifest.id, parsedManifestA.id) == 0);
        TEST_CHECK(std::strcmp(repeatedManifest.displayName, parsedManifestA.displayName) == 0);
        TEST_CHECK(std::strcmp(repeatedManifest.entries[0].architecture, parsedManifestA.entries[0].architecture) == 0);
        TEST_CHECK(std::strcmp(repeatedManifest.entries[0].path, parsedManifestA.entries[0].path) == 0);
        if (iteration == 499) {
            TEST_CHECK(ParseApplicationManifest(differentManifestBytes.data(), static_cast<uint32_t>(differentManifestBytes.size()), &parsedManifestB, &error));
            TEST_CHECK(ParseApplicationManifest(validManifestBytes.data(), static_cast<uint32_t>(validManifestBytes.size()), &repeatedManifest, &error));
            TEST_CHECK(ValidateApplicationManifestIdentity(repeatedManifest, loaded.project, nullptr, &manifestDiagnostic));
        }
    }
    TEST_CHECK(stableManifestHash == ComputeManifestContentHashFnv1a64(validManifestBytes.data(), static_cast<uint32_t>(validManifestBytes.size())));

    ProjectLoadScratch firstLoadScratch = {};
    ProjectLoadScratch secondLoadScratch = {};
    ProjectOperationResult firstScratchLoad;
    ProjectOperationResult secondScratchLoad;
    TEST_CHECK(LoadProject(fileSystem, generatedRoot.string().c_str(), &firstScratchLoad, &firstLoadScratch));
    TEST_CHECK(LoadProject(fileSystem, generatedRoot.string().c_str(), &secondScratchLoad, &secondLoadScratch));
    TEST_CHECK(firstLoadScratch.manifest.id[0] != '\0' && secondLoadScratch.manifest.id[0] != '\0');
    TEST_CHECK(std::strcmp(firstLoadScratch.manifest.id, secondLoadScratch.manifest.id) == 0);
    TEST_CHECK(std::strcmp(firstLoadScratch.manifestPath, secondLoadScratch.manifestPath) == 0);
    context.partialManifestRead = true;
    ProjectOperationResult partialManifestLoad;
    TEST_CHECK(!LoadProject(fileSystem, generatedRoot.string().c_str(), &partialManifestLoad, &firstLoadScratch));
    context.partialManifestRead = false;
    TEST_CHECK(partialManifestLoad.error == ProjectErrorCode::ManifestReadPartial);
    TEST_CHECK(partialManifestLoad.manifestDiagnostic.manifestExpectedSize == partialManifestLoad.manifestDiagnostic.manifestBytesRead + 1);
    TEST_CHECK(partialManifestLoad.manifestDiagnostic.resultCode == ProjectErrorCode::ManifestReadPartial);

    ProjectOperationResult missingProject;
    TEST_CHECK(!LoadProject(fileSystem, (testRoot / "does-not-exist" / "guidexos.project").string().c_str(), &missingProject));
    TEST_CHECK(missingProject.error == ProjectErrorCode::ParentNotFound);
    const std::string generatedMain = readAll(generatedRoot / "src" / "main.cpp");
    TEST_CHECK(generatedMain.find("gx_main") != std::string::npos);
    TEST_CHECK(generatedMain.find("request_window_ex") != std::string::npos);
    TEST_CHECK(generatedMain.find("Welcome to ") != std::string::npos);
    TEST_CHECK(generatedMain.find("D:\\dev\\") == std::string::npos);
    const std::string generatedBuild = readAll(generatedRoot / "build.ps1");
    TEST_CHECK(generatedBuild.find("GUIDEXOS_NATIVE_BUILD_RECIPE_V1") != std::string::npos);
    TEST_CHECK(generatedBuild.find("ServerRoot") == std::string::npos);
    TEST_CHECK(generatedBuild.find("PackageRoot") == std::string::npos);
    TEST_CHECK(generatedBuild.find("Join-Path $BuildRoot (\"bin\\\" + $TargetArchitecture)") != std::string::npos);
    TEST_CHECK(generatedBuild.find("ValidateSet(\"amd64\",\"arm64\")") != std::string::npos);
    TEST_CHECK(generatedBuild.find("aarch64-none-elf") != std::string::npos);
    TEST_CHECK(generatedBuild.find("aarch64elf") != std::string::npos);
    TEST_CHECK(generatedBuild.find("D:\\dev\\guideXOSServer") == std::string::npos);

    static WorkspaceController controller;
    WorkspaceControllerInit(&controller, fileSystem);
    context.controller = &controller;
    context.reentryPoint = TestContext::ReentryPoint::LoadStarted;
    context.reentryProjectPath = generatedRoot.string();
    WorkspaceControllerSetProjectOpenObserver(&controller, projectOpenObserver, &context);
    TEST_CHECK(WorkspaceControllerOpenProject(&controller, generatedRoot.string().c_str()));
    TEST_CHECK(!context.reentrantOpenAccepted);
    TEST_CHECK(context.reentrantOpenError == ProjectErrorCode::LoadInProgress);
    TEST_CHECK(controller.model.hasProject);
    TEST_CHECK(std::strcmp(controller.model.project.projectId, request.projectId) == 0);
    TEST_CHECK(context.lastProjectState == WorkspaceProjectOpenState::Ready);
    TEST_CHECK(context.lastProjectRequestId == 1);
    TEST_CHECK(context.lastProjectGeneration == controller.model.projectGeneration);
    TEST_CHECK(context.lastCandidateId != 0);
    TEST_CHECK(context.lastRefreshGeneration != 0);
    TEST_CHECK(!context.candidateVisibleBeforeCommit);
    TEST_CHECK(controller.projectOpenReentryCount == 1);
    TEST_CHECK(controller.projectOpenMaximumEntryDepth == 2);
    TEST_CHECK(std::strcmp(controller.projectOpenLastReentryCaller,
                            "test_load_started_observer") == 0);
    TEST_CHECK(controller.projectOpenManifestValidationCount == 1);
    TEST_CHECK(controller.lastManifestDiagnostic.validationCount == 1);
    TEST_CHECK(std::strcmp(controller.lastManifestDiagnostic.validationRole, "application") == 0);
    TEST_CHECK(std::strstr(controller.lastManifestDiagnostic.projectMetadataPath,
                           "guidexos.project") != nullptr);
    TEST_CHECK(std::strstr(controller.lastManifestDiagnostic.manifestPath,
                           "app/app.json") != nullptr);
    TEST_CHECK(std::strcmp(controller.lastManifestDiagnostic.projectMetadataPath,
                            controller.lastManifestDiagnostic.manifestPath) != 0);
    TEST_CHECK(controller.projectOpenRefreshCount == 1 && controller.projectOpenCommitCount == 1);
    TEST_CHECK(!controller.projectOpenInProgress);

    const uint64_t activeProjectGenerationBeforePartialOpen = controller.model.projectGeneration;
    char activeProjectIdBeforePartialOpen[kMaxProjectIdBytes] = {};
    std::strncpy(activeProjectIdBeforePartialOpen, controller.model.project.projectId,
                 sizeof(activeProjectIdBeforePartialOpen) - 1);
    context.partialManifestRead = true;
    const bool partialOpenAccepted = WorkspaceControllerOpenProject(&controller, generatedRoot.string().c_str());
    context.partialManifestRead = false;
    TEST_CHECK(!partialOpenAccepted);
    TEST_CHECK(controller.lastProjectError == ProjectErrorCode::ManifestReadPartial);
    TEST_CHECK(context.lastProjectState == WorkspaceProjectOpenState::Failed);
    TEST_CHECK(context.lastProjectRequestId == 2);
    TEST_CHECK(controller.model.hasProject);
    TEST_CHECK(controller.model.projectGeneration == activeProjectGenerationBeforePartialOpen);
    TEST_CHECK(std::strcmp(controller.model.project.projectId, activeProjectIdBeforePartialOpen) == 0);
    TEST_CHECK(controller.lastManifestDiagnostic.resultCode == ProjectErrorCode::ManifestReadPartial);
    TEST_CHECK(!controller.projectOpenInProgress);
    TEST_CHECK(context.failedCount == 1);
    TEST_CHECK(WorkspaceControllerOpenProject(&controller, generatedRoot.string().c_str()));
    TEST_CHECK(context.lastProjectRequestId == 3);
    TEST_CHECK(controller.model.hasProject);
    TEST_CHECK(std::strcmp(controller.model.project.projectId, request.projectId) == 0);
    TEST_CHECK(WorkspaceControllerOpenDocument(&controller, "src/main.cpp"));
    char oldRoot[kMaxPathBytes] = {};
    std::strcpy(oldRoot, controller.model.rootPath);
    const uint64_t oldDocumentId = WorkspaceControllerActiveDocument(&controller)->documentId;
    const uint64_t oldProjectGeneration = controller.model.projectGeneration;
    context.failList = true;
    TEST_CHECK(!WorkspaceControllerOpenProject(&controller, generatedRoot.string().c_str()));
    context.failList = false;
    TEST_CHECK(controller.lastProjectError == ProjectErrorCode::RequiredFileMissing);
    TEST_CHECK(context.lastProjectState == WorkspaceProjectOpenState::Failed);
    TEST_CHECK(context.lastProjectRequestId == 4);
    TEST_CHECK(!controller.projectOpenInProgress);
    TEST_CHECK(controller.model.hasProject && std::strcmp(controller.model.rootPath, oldRoot) == 0);
    TEST_CHECK(controller.model.projectGeneration == oldProjectGeneration);
    TEST_CHECK(WorkspaceControllerActiveDocument(&controller) != nullptr &&
           WorkspaceControllerActiveDocument(&controller)->documentId == oldDocumentId);
    fs::create_directories(testRoot / "invalid-project", ec);
    std::ofstream(testRoot / "invalid-project" / "guidexos.project") << "";
    TEST_CHECK(!WorkspaceControllerOpenProject(&controller, (testRoot / "invalid-project").string().c_str()));
    TEST_CHECK(controller.lastProjectError == ProjectErrorCode::MalformedJson);
    TEST_CHECK(controller.model.hasProject && std::strcmp(controller.model.rootPath, oldRoot) == 0);
    TEST_CHECK(WorkspaceControllerActiveDocument(&controller) != nullptr &&
           WorkspaceControllerActiveDocument(&controller)->documentId == oldDocumentId);
    TEST_CHECK(WorkspaceControllerOpenDocument(&controller, "guidexos.project"));

    const fs::path extraSource = generatedRoot / "src" / "extra.cpp";
    std::ofstream(extraSource) << "int extra_value = 1;\n";
    TEST_CHECK(WorkspaceControllerOpenDocument(&controller, "src/extra.cpp"));
    fs::remove(extraSource, ec);
    TEST_CHECK(WorkspaceControllerRefresh(&controller));
    TEST_CHECK(!WorkspaceControllerOpenDocument(&controller, "src/extra.cpp"));
    TEST_CHECK(controller.lastError == ModelErrorCode::ReadFailed);
    std::ofstream(extraSource) << "int extra_value = 2;\n";
    TEST_CHECK(WorkspaceControllerCloseDocument(&controller, controller.model.activeDocument, CloseDecision::Discard));
    TEST_CHECK(WorkspaceControllerOpenDocument(&controller, "src/extra.cpp"));

    const std::string mainBeforeRemoval = generatedMain;
    fs::remove(generatedRoot / "src" / "main.cpp", ec);
    TEST_CHECK(WorkspaceControllerRefresh(&controller));
    TEST_CHECK(!WorkspaceControllerOpenDocument(&controller, "src/main.cpp"));
    TEST_CHECK(controller.lastError == ModelErrorCode::ReadFailed);
    std::ofstream restoredMain(generatedRoot / "src" / "main.cpp", std::ios::binary);
    restoredMain.write(mainBeforeRemoval.data(), static_cast<std::streamsize>(mainBeforeRemoval.size()));
    restoredMain.close();
    TEST_CHECK(WorkspaceControllerOpenDocument(&controller, "src/main.cpp"));
    TEST_CHECK(WorkspaceControllerCloseWorkspace(&controller, CloseDecision::Discard));
    TEST_CHECK(!controller.model.hasProject);
    TEST_CHECK(WorkspaceControllerOpenWorkspace(&controller, generatedRoot.string().c_str()));
    TEST_CHECK(!controller.model.hasProject);

    ProjectCreateRequest repeat = request;
    std::strcpy(repeat.parentPath, (testRoot / "parent-two").string().c_str());
    ProjectOperationResult repeated;
    TEST_CHECK(CreateNativeGuiProject(fileSystem, repeat, &repeated));
    const fs::path repeatedRoot = testRoot / "parent-two" / "hello";
    const char* generatedFiles[] = { "guidexos.project", "CMakeLists.txt", "build.ps1", "README.md", "app/app.json", "src/main.cpp", "src/freestanding_memory.cpp" };
    for (const char* file : generatedFiles) TEST_CHECK(readAll(generatedRoot / file) == readAll(repeatedRoot / file));

    fs::create_directories(testRoot / "parent-one" / "occupied", ec);
    std::ofstream(testRoot / "parent-one" / "occupied" / "existing.txt") << "keep";
    ProjectCreateRequest occupied = request;
    std::strcpy(occupied.folderName, "occupied");
    TEST_CHECK(!CreateNativeGuiProject(fileSystem, occupied, &loaded) && loaded.error == ProjectErrorCode::DestinationExists);

    TestContext failingContext;
    failingContext.failMainWrite = true;
    ProjectFileSystem failingFileSystem = { &failingContext, statFile, listFiles, readFile, writeFile, createDirectory, removePath };
    ProjectCreateRequest failing = request;
    std::strcpy(failing.folderName, "rollback");
    std::strcpy(failing.parentPath, (testRoot / "parent-one").string().c_str());
    TEST_CHECK(!CreateNativeGuiProject(failingFileSystem, failing, &loaded));
    TEST_CHECK(loaded.rollbackAttempted && loaded.rollbackSucceeded);
    TEST_CHECK(!fs::exists(testRoot / "parent-one" / "rollback"));

    runProjectReentryCase(generatedRoot, TestContext::ReentryPoint::ProjectMetadataRead,
                          "test_project_metadata_read");
    runProjectReentryCase(generatedRoot, TestContext::ReentryPoint::ApplicationManifestRead,
                          "test_application_manifest_read");
    runProjectReentryCase(generatedRoot, TestContext::ReentryPoint::RefreshList,
                          "test_refresh_callback");
    runProjectReentryCase(generatedRoot, TestContext::ReentryPoint::LoadStarted,
                          "test_load_started_observer");
    runProjectStaleGenerationCase(generatedRoot);
    runProjectOwnerReasonCase(generatedRoot, TestContext::OwnerMutation::RequestId,
        WorkspaceProjectLoadOwnershipResult::RequestIdMismatch, false);
    runProjectOwnerReasonCase(generatedRoot, TestContext::OwnerMutation::RequestGeneration,
        WorkspaceProjectLoadOwnershipResult::RequestGenerationMismatch, false);
    runProjectOwnerReasonCase(generatedRoot, TestContext::OwnerMutation::TransactionId,
        WorkspaceProjectLoadOwnershipResult::TransactionIdMismatch, false);
    runProjectOwnerReasonCase(generatedRoot, TestContext::OwnerMutation::TransactionGeneration,
        WorkspaceProjectLoadOwnershipResult::TransactionGenerationMismatch, false);
    runProjectOwnerReasonCase(generatedRoot, TestContext::OwnerMutation::CandidateId,
        WorkspaceProjectLoadOwnershipResult::CandidateIdMismatch, false);
    runProjectOwnerReasonCase(generatedRoot, TestContext::OwnerMutation::CandidateGeneration,
        WorkspaceProjectLoadOwnershipResult::CandidateGenerationMismatch, false);
    runProjectOwnerReasonCase(generatedRoot, TestContext::OwnerMutation::CandidateProjectGeneration,
        WorkspaceProjectLoadOwnershipResult::CandidateProjectGenerationMismatch, false);
    runProjectOwnerReasonCase(generatedRoot, TestContext::OwnerMutation::RefreshGeneration,
        WorkspaceProjectLoadOwnershipResult::RefreshGenerationMismatch, false);
    runProjectOwnerReasonCase(generatedRoot, TestContext::OwnerMutation::ActiveProjectId,
        WorkspaceProjectLoadOwnershipResult::ActiveProjectIdMismatch, true);
    runProjectOwnerReasonCase(generatedRoot, TestContext::OwnerMutation::ActiveProjectGeneration,
        WorkspaceProjectLoadOwnershipResult::ActiveProjectGenerationMismatch, true);
    runProjectOwnerReasonCase(generatedRoot, TestContext::OwnerMutation::ControllerProgress,
        WorkspaceProjectLoadOwnershipResult::ControllerNotInProgress, false);

    if (!preserve) fs::remove_all(testRoot, ec);
    std::cout << "Developer Studio project parser/generator PASS\n";
    return 0;
}
