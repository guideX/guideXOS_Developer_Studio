#include "developer_studio_build.h"

#include <cassert>
#include <cstring>
#include <iostream>

using namespace guidexos::developer_studio;

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
    std::strcpy(project.targetProfileId, "guidexos.amd64.hosted.native");
    std::strcpy(project.entryPoint, "gx_main");
    std::strcpy(project.abi, "guidexos-c-abi-v1");
    std::strcpy(project.architecture, "amd64");
    std::strcpy(project.outputName, "hello-guidexos");
    return project;
}

struct FakeBuild {
    uint64_t handle = 0;
    uint32_t polls = 0;
    bool released = false;
};

static bool startBuild(void* userData, const BuildRequest&, uint64_t* handle, BuildErrorCode*) {
    FakeBuild* fake = static_cast<FakeBuild*>(userData);
    fake->handle = 42;
    *handle = fake->handle;
    return true;
}

static bool pollBuild(void* userData, uint64_t, BuildResult* result, bool* completed, BuildErrorCode*) {
    FakeBuild* fake = static_cast<FakeBuild*>(userData);
    *result = BuildResult();
    result->state = fake->polls++ == 0 ? BuildState::Running : BuildState::Succeeded;
    result->exitCode = 0;
    result->artifactValid = result->state == BuildState::Succeeded;
    std::strcpy(result->artifactPath, "build/bin/amd64/hello-guidexos.elf");
    *completed = result->state == BuildState::Succeeded;
    return true;
}

static bool releaseBuild(void* userData, uint64_t) {
    static_cast<FakeBuild*>(userData)->released = true;
    return true;
}

int main() {
    Project project = validProject();
    BuildRequest request = {};
    BuildErrorCode error = BuildErrorCode::None;
    assert(BuildRequestFromProject(project, &request, &error));
    assert(std::strcmp(request.buildSystem, "guidexos-native-build-script-v1") == 0);
    assert(std::strcmp(request.buildScript, "build.ps1") == 0);
    assert(std::strcmp(request.expectedArtifact, "build/bin/amd64/hello-guidexos.elf") == 0);
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
    assert(BuildControllerStart(&controller, &workspace, service, BuildDirtyDecision::SaveAll, &error));
    assert(controller.state == BuildState::Running);
    assert(!BuildControllerStart(&controller, &workspace, service, BuildDirtyDecision::SaveAll, &error) && error == BuildErrorCode::AlreadyRunning);
    assert(BuildControllerPoll(&controller, service));
    assert(BuildControllerIsActive(&controller));
    assert(BuildControllerPoll(&controller, service));
    assert(!BuildControllerIsActive(&controller));
    assert(controller.state == BuildState::Succeeded);
    assert(fake.released);

    workspace.model.documents[0].used = true;
    std::strcpy(workspace.model.documents[0].path, "D:/work/hello/src/main.cpp");
    workspace.model.documents[0].buffer.dirty = true;
    assert(!BuildControllerStart(&controller, &workspace, service, BuildDirtyDecision::Cancel, &error));
    assert(error == BuildErrorCode::UserCancelled && controller.state == BuildState::Cancelled);
    std::cout << "Developer Studio build controller PASS\n";
    return 0;
}
