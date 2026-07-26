#include "developer_studio_run.h"

#include <cassert>
#include <cstring>
#include <iostream>

using namespace guidexos::developer_studio;

static Project validProject() {
    Project project = {};
    project.valid = true;
    project.kind = ProjectKind::NativeGuiApplication;
    std::strcpy(project.projectId, "com.example.run");
    std::strcpy(project.rootPath, "D:/work/run");
    std::strcpy(project.manifestPath, "app/app.json");
    std::strcpy(project.targetProfileId, "guidexos.amd64.hosted.native");
    return project;
}

struct FakeRun {
    uint32_t polls = 0;
    bool closeRequested = false;
    bool released = false;
    bool startFails = false;
};

static bool prepareRun(void* userData, const RunRequest&, uint64_t* handle, RunResult* result) {
    FakeRun* fake = static_cast<FakeRun*>(userData);
    *handle = 77;
    *result = RunResult();
    result->state = RunState::Registered;
    std::strcpy(result->applicationId, "com.example.run");
    return fake != nullptr;
}

static bool startRun(void* userData, uint64_t, RunResult* result) {
    if (static_cast<FakeRun*>(userData)->startFails) {
        result->error = RunErrorCode::LaunchFailed;
        result->state = RunState::Failed;
        return false;
    }
    result->state = RunState::Launching;
    return true;
}

static bool pollRun(void* userData, uint64_t, RunResult* result) {
    FakeRun* fake = static_cast<FakeRun*>(userData);
    *result = RunResult();
    if (fake->polls++ == 0) result->state = RunState::Running;
    else { result->state = RunState::Completed; result->cleanupComplete = true; result->exitCode = 0; }
    return true;
}

static bool closeRun(void* userData, uint64_t) {
    static_cast<FakeRun*>(userData)->closeRequested = true;
    return true;
}

static bool releaseRun(void* userData, uint64_t) {
    static_cast<FakeRun*>(userData)->released = true;
    return true;
}

int main() {
    Project project = validProject();
    BuildResult build = {};
    build.state = BuildState::Succeeded;
    build.artifactValid = true;
    build.artifactEntryPoint = true;
    std::strcpy(build.artifactPath, "build/bin/amd64/run.elf");
    std::strcpy(build.artifactSha256, "0123456789012345678901234567890123456789012345678901234567890123");

    RunRequest request = {};
    RunErrorCode error = RunErrorCode::None;
    assert(RunRequestFromBuild(project, build, &request, &error));
    assert(std::strcmp(request.manifestPath, "app/app.json") == 0);
    assert(std::strcmp(request.artifactPath, "build/bin/amd64/run.elf") == 0);

    FakeRun fake;
    HostedDevelopmentRunService service = { &fake, prepareRun, startRun, pollRun, closeRun, releaseRun };
    RunController controller = {};
    assert(RunControllerInit(&controller));
    static OutputService output;
    OutputServiceInit(&output);
    const uint64_t operationId = OutputServiceBeginOperation(&output, OutputOperationType::Run, project.projectId);
    RunControllerAttachOutput(&controller, &output, operationId);
    assert(RunControllerPrepare(&controller, service, request, &error));
    assert(controller.state == RunState::Prepared && RunControllerIsActive(&controller));
    assert(RunControllerStart(&controller, service, &error));
    assert(RunControllerRequestClose(&controller, service));
    assert(fake.closeRequested);
    assert(RunControllerPoll(&controller, service));
    assert(controller.state == RunState::Running && RunControllerIsActive(&controller));
    assert(RunControllerPoll(&controller, service));
    assert(controller.state == RunState::Completed && !RunControllerIsActive(&controller));
    assert(fake.released);
    assert(OutputServiceProblemCount(&output, project.projectId) == 0);
    uint32_t terminalCount = 0;
    for (uint32_t i = 0; i < OutputServiceRecordCount(&output); ++i) if (OutputServiceRecordAt(&output, i)->isTerminal) ++terminalCount;
    assert(terminalCount == 1);

    FakeRun startFailure;
    startFailure.startFails = true;
    service.userData = &startFailure;
    assert(RunControllerInit(&controller));
    assert(RunControllerPrepare(&controller, service, request, &error));
    assert(!RunControllerStart(&controller, service, &error));
    assert(error == RunErrorCode::LaunchFailed);
    assert(startFailure.released && controller.handle == 0 && !RunControllerIsActive(&controller));

    std::cout << "Developer Studio run controller PASS\n";
    return 0;
}
