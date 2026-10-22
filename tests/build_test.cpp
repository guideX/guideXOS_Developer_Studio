#include "developer_studio_build.h"

#include <cassert>
#include <cstring>
#include <iostream>

using namespace guidexos::developer_studio;

#if defined(GXOS_DEVELOPER_STUDIO_AARCH64)
static const char kTargetId[] = "guidexos.arm64.hosted.native";
static const char kArchitecture[] = "arm64";
static const char kArtifactPath[] = "build/bin/arm64/hello-guidexos.elf";
#else
static const char kTargetId[] = "guidexos.amd64.hosted.native";
static const char kArchitecture[] = "amd64";
static const char kArtifactPath[] = "build/bin/amd64/hello-guidexos.elf";
#endif

static Project validProject() {
    Project project = {};
    project.valid = true;
    project.formatVersion = 1;
    project.kind = ProjectKind::NativeGuiApplication;
    std::strcpy(project.projectId, "com.example.hello");
    std::strcpy(project.displayName, "Hello guideXOS");
    std::strcpy(project.rootPath, "D:/work/hello");
    std::strcpy(project.sourceRoot, "src");
    std::strcpy(project.manifestPath, "app/app.json");
    std::strcpy(project.targetProfileId, kTargetId);
    std::strcpy(project.entryPoint, "gx_main");
    std::strcpy(project.abi, "guidexos-c-abi-v1");
    std::strcpy(project.architecture, kArchitecture);
    std::strcpy(project.outputName, "hello-guidexos");
    return project;
}

struct FakeBuild {
    uint64_t handle = 0;
    uint32_t polls = 0;
    uint32_t startCalls = 0;
    bool released = false;
    bool fail = false;
    bool failRelease = false;
};

static bool startBuild(void* userData, const BuildRequest&, uint64_t* handle, BuildErrorCode*) {
    FakeBuild* fake = static_cast<FakeBuild*>(userData);
    ++fake->startCalls;
    fake->polls = 0;
    fake->released = false;
    fake->handle = 42;
    *handle = fake->handle;
    return true;
}

static bool pollBuild(void* userData, uint64_t, BuildResult* result, bool* completed, BuildErrorCode*) {
    FakeBuild* fake = static_cast<FakeBuild*>(userData);
    *result = BuildResult();
    const uint32_t pollIndex = fake->polls++;
    result->state = pollIndex == 0 ? BuildState::Running :
        (pollIndex == 1 && !fake->fail ? BuildState::ValidatingArtifact :
            (fake->fail ? BuildState::Failed : BuildState::Succeeded));
    result->error = fake->fail ? BuildErrorCode::LinkerFailed : BuildErrorCode::None;
    result->exitCode = fake->fail ? 1 : 0;
    result->artifactValid = result->state == BuildState::Succeeded;
    result->sourceFileCount = 3;
    result->compiledModuleCount = fake->fail ? 1 : 2;
    result->cachedModuleCount = fake->fail ? 1 : 1;
    result->linkedModuleCount = fake->fail ? 0 : 3;
    if (fake->fail) std::strcpy(result->errorMessage, "[linker] undefined external function 'missing'");
    std::strcpy(result->artifactPath, kArtifactPath);
    *completed = result->state == BuildState::Succeeded || result->state == BuildState::Failed ||
        result->state == BuildState::Cancelled;
    return true;
}

static bool releaseBuild(void* userData, uint64_t) {
    FakeBuild* fake = static_cast<FakeBuild*>(userData);
    fake->released = true;
    return !fake->failRelease;
}

int main() {
    assert(ClassifyBuildFailureDetail("[compiler] unexpected token") == BuildErrorCode::CompilerFailed);
    assert(ClassifyBuildFailureDetail("[project] invalid source selection") == BuildErrorCode::ProjectValidationFailed);
    assert(ClassifyBuildFailureDetail("[source] source could not be read") == BuildErrorCode::SourceReadFailed);
    assert(ClassifyBuildFailureDetail("[object] object serialization failed") == BuildErrorCode::ObjectGenerationFailed);
    assert(ClassifyBuildFailureDetail("[linker] undefined external function 'missing'") == BuildErrorCode::LinkerFailed);
    assert(ClassifyBuildFailureDetail("[ELF writer] ELF writer failed") == BuildErrorCode::ElfWriterFailed);
    assert(ClassifyBuildFailureDetail("[artifact validation] AMD64 image is invalid") == BuildErrorCode::ArtifactValidationFailed);
    assert(ClassifyBuildFailureDetail("[artifact publication] output readback failed") == BuildErrorCode::ArtifactPublicationFailed);
    assert(ClassifyBuildDiagnosticLine("src/main.cpp:4:8: error: unknown name") == BuildErrorCode::CompilerFailed);
    assert(ClassifyBuildDiagnosticLine("lld-link: error: unresolved external symbol 'missing'") == BuildErrorCode::LinkerFailed);
    assert(ClassifyBuildDiagnosticLine("PowerShell completed the build script") == BuildErrorCode::None);
    Project project = validProject();
    BuildRequest request = {};
    BuildErrorCode error = BuildErrorCode::None;
    assert(BuildRequestFromProject(project, &request, &error));
    assert(std::strcmp(request.projectKind, "native-gui-application") == 0);
    assert(std::strcmp(request.buildSystem, "guidexos-native-build-script-v1") == 0);
    assert(std::strcmp(request.buildScript, "build.ps1") == 0);
    assert(std::strcmp(request.expectedArtifact, kArtifactPath) == 0);
    assert(std::strcmp(request.configuration, "Debug") == 0);
    assert(BuildRequestEnableDebugInfo(&request));
    assert(std::strcmp(request.configuration, "DebugSymbols") == 0);
    Project bareProject = validProject();
    std::strcpy(bareProject.targetProfileId, BareMetalTargetProfile().id);
    std::strcpy(bareProject.sourceEntry, "main.cpp");
    assert(BuildRequestFromProject(bareProject, &request, &error, BuildBackendKind::BareMetal));
    assert(std::strcmp(request.buildSystem, "guidexos-native-baremetal-bootstrap-v1") == 0);
    assert(std::strcmp(request.buildScript, "") == 0);
    assert(std::strcmp(request.expectedArtifact, kArtifactPath) == 0);
    bareProject.hasSourceFiles = true;
    bareProject.sourceFileCount = 3;
    std::strcpy(bareProject.sourceFiles[0], "main.cpp");
    std::strcpy(bareProject.sourceFiles[1], "helper.cpp");
    std::strcpy(bareProject.sourceFiles[2], "utility.c");
    assert(BuildRequestFromProject(bareProject, &request, &error, BuildBackendKind::BareMetal));
    assert(request.sourceCount == 3);
    assert(std::strcmp(request.sourceFiles[0], "src/main.cpp") == 0);
    assert(std::strcmp(request.sourceFiles[1], "src/helper.cpp") == 0);
    assert(std::strcmp(request.sourceFiles[2], "src/utility.c") == 0);
    assert(!BuildRequestFromProject(bareProject, &request, &error, BuildBackendKind::Hosted) &&
           error == BuildErrorCode::UnsupportedTarget);
    project.kind = ProjectKind::ConsoleApplication;
    assert(!BuildRequestFromProject(project, &request, &error) && error == BuildErrorCode::InvalidRequest);

    static WorkspaceController workspace = {};
    WorkspaceControllerInit(&workspace, WorkspaceFileSystem());
    workspace.model.open = true;
    workspace.model.hasProject = true;
    workspace.model.project = validProject();
    std::strcpy(workspace.model.rootPath, "D:/work/hello");
    BuildController controller = {};
    assert(BuildControllerInit(&controller));
    FakeBuild fake;
    HostedBuildService service = { &fake, startBuild, pollBuild, releaseBuild };
    static OutputService output;
    OutputServiceInit(&output);
    assert(BuildControllerStart(&controller, &workspace, service, BuildDirtyDecision::SaveAll, &error, &output));
    assert(controller.state == BuildState::Running);
    assert(controller.operationId != 0);
    assert(OutputServiceFilteredCount(&output, OutputChannel::Build) >= 3);
    assert(!BuildControllerStart(&controller, &workspace, service, BuildDirtyDecision::SaveAll, &error) && error == BuildErrorCode::AlreadyRunning);
    assert(BuildControllerPoll(&controller, service));
    assert(BuildControllerIsActive(&controller));
    assert(BuildControllerPoll(&controller, service));
    assert(BuildControllerIsActive(&controller) && controller.state == BuildState::ValidatingArtifact);
    assert(BuildControllerPoll(&controller, service));
    assert(!BuildControllerIsActive(&controller));
    assert(controller.state == BuildState::Succeeded);
    assert(fake.released);
    assert(std::strcmp(controller.result.projectId, workspace.model.project.projectId) == 0);
    assert(std::strcmp(controller.result.projectRoot, workspace.model.project.rootPath) == 0);
    assert(controller.result.sourceMembershipHash == BuildRequestSourceMembershipHash(controller.request));
    assert(std::strcmp(controller.result.configuration, controller.request.configuration) == 0);
    assert(controller.result.sourceFileCount == 3 && controller.result.compiledModuleCount == 2 &&
           controller.result.cachedModuleCount == 1 && controller.result.linkedModuleCount == 3);
    uint32_t terminalCount = 0;
    for (uint32_t i = 0; i < OutputServiceRecordCount(&output); ++i) if (OutputServiceRecordAt(&output, i)->isTerminal) ++terminalCount;
    assert(terminalCount == 1);

    fake.fail = true;
    assert(BuildControllerStart(&controller, &workspace, service, BuildDirtyDecision::SaveAll, &error, &output));
    assert(BuildControllerPoll(&controller, service) && BuildControllerIsActive(&controller));
    assert(BuildControllerPoll(&controller, service) && !BuildControllerIsActive(&controller));
    assert(controller.state == BuildState::Failed && controller.result.error == BuildErrorCode::LinkerFailed);
    assert(std::strstr(controller.result.errorMessage, "undefined external") != nullptr);
    fake.fail = false;
    assert(BuildControllerStart(&controller, &workspace, service, BuildDirtyDecision::SaveAll, &error, &output));
    assert(BuildControllerPoll(&controller, service));
    assert(BuildControllerPoll(&controller, service));
    assert(BuildControllerIsActive(&controller));
    assert(BuildControllerPoll(&controller, service));
    assert(controller.state == BuildState::Succeeded && fake.released);
    fake.failRelease = true;
    assert(BuildControllerStart(&controller, &workspace, service, BuildDirtyDecision::SaveAll, &error, &output));
    assert(BuildControllerPoll(&controller, service));
    assert(BuildControllerPoll(&controller, service));
    assert(BuildControllerIsActive(&controller));
    assert(BuildControllerPoll(&controller, service));
    assert(controller.state == BuildState::Failed && controller.result.serviceReleaseFailed &&
           !controller.result.artifactValid && controller.result.artifactPath[0] == '\0');
    fake.failRelease = false;

    workspace.model.documents[0].used = true;
    std::strcpy(workspace.model.documents[0].path, "D:/work/hello/src/main.cpp");
    workspace.model.documents[0].buffer.dirty = true;
    const uint32_t startsBeforeCancel = fake.startCalls;
    assert(!BuildControllerStart(&controller, &workspace, service, BuildDirtyDecision::Cancel, &error));
    assert(error == BuildErrorCode::UserCancelled && controller.state == BuildState::Cancelled);
    assert(fake.startCalls == startsBeforeCancel);
    std::cout << "Developer Studio build controller PASS\n";
    return 0;
}
