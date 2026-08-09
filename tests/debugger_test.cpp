#include "developer_studio_debugger.h"

#include <cassert>
#include <cstring>
#include <iostream>

using namespace guidexos::developer_studio;

static Project validProject() {
    Project project = {};
    project.valid = true;
    project.kind = ProjectKind::NativeGuiApplication;
    std::strcpy(project.projectId, "com.example.debugger");
    std::strcpy(project.displayName, "Debugger Fixture");
    std::strcpy(project.rootPath, "D:/work/debugger");
    std::strcpy(project.manifestPath, "app/app.json");
    std::strcpy(project.targetProfileId, "guidexos.amd64.hosted.native");
    std::strcpy(project.architecture, "amd64");
    std::strcpy(project.abi, "guidexos-c-abi-v1");
    std::strcpy(project.outputName, "debugger-fixture");
    return project;
}

static BuildResult validBuild() {
    BuildResult build = {};
    build.state = BuildState::Succeeded;
    build.artifactValid = true;
    build.artifactEntryPoint = true;
    std::strcpy(build.artifactPath, "build/bin/amd64/debugger-fixture.elf");
    std::strcpy(build.artifactSha256, "0123456789012345678901234567890123456789012345678901234567890123");
    return build;
}

struct FakeBackend {
    uint32_t launches = 0;
    uint32_t polls = 0;
    uint32_t stops = 0;
    bool launchFails = false;
    bool pauseCalled = false;
    DebugSessionState nextState = DebugSessionState::Running;
};

static bool launch(void* userData, const DebugTarget&, uint64_t generation, DebugBackendSnapshot* snapshot) {
    FakeBackend* fake = static_cast<FakeBackend*>(userData);
    ++fake->launches;
    *snapshot = DebugBackendSnapshot();
    snapshot->sessionGeneration = generation;
    snapshot->state = fake->launchFails ? DebugSessionState::Failed : DebugSessionState::Launching;
    snapshot->processId = 12;
    snapshot->nativeRuntimeId = 77;
    std::strcpy(snapshot->backendName, "Test Backend");
    if (fake->launchFails) std::strcpy(snapshot->errorMessage, "test launch failed");
    return !fake->launchFails;
}

static bool poll(void* userData, uint64_t generation, DebugBackendSnapshot* snapshot) {
    FakeBackend* fake = static_cast<FakeBackend*>(userData);
    *snapshot = DebugBackendSnapshot();
    snapshot->sessionGeneration = generation;
    snapshot->processId = 12;
    snapshot->nativeRuntimeId = 77;
    snapshot->state = fake->polls++ == 0 ? fake->nextState : DebugSessionState::Exited;
    snapshot->stopReason = snapshot->state == DebugSessionState::Exited ? DebugStopReason::Exited : DebugStopReason::None;
    snapshot->exitCode = 0;
    snapshot->cleanupComplete = snapshot->state == DebugSessionState::Exited;
    return true;
}

static bool stop(void* userData, uint64_t) {
    ++static_cast<FakeBackend*>(userData)->stops;
    return true;
}

static bool pause(void* userData, uint64_t) {
    static_cast<FakeBackend*>(userData)->pauseCalled = true;
    return true;
}

static DebugBackend makeBackend(FakeBackend* fake) {
    DebugBackend backend = {};
    backend.userData = fake;
    backend.capabilities.canLaunch = true;
    backend.capabilities.canStop = true;
    backend.capabilities.canPause = false;
    backend.capabilities.canContinue = false;
    std::strcpy(backend.name, "Test Backend");
    backend.launch = launch;
    backend.poll = poll;
    backend.stop = stop;
    backend.pause = pause;
    return backend;
}

int main() {
    Project project = validProject();
    BuildResult build = validBuild();
    DebugTarget target = {};
    DebugErrorCode error = DebugErrorCode::None;
    assert(DebugTargetFromBuild(project, build, 9, &target, &error));
    assert(std::strcmp(target.executablePath, build.artifactPath) == 0);
    assert(std::strcmp(target.architecture, "amd64") == 0);

    char relative[kMaxProjectPathBytes] = {};
    assert(DebugRelativeSourcePath(project.rootPath, "d:\\work\\debugger\\src\\main.cpp", relative, sizeof(relative)));
    assert(std::strcmp(relative, "src/main.cpp") == 0);
    assert(!DebugRelativeSourcePath(project.rootPath, "D:/work/debugger/../other.cpp", relative, sizeof(relative)));

    DebugController controller = {};
    assert(DebugControllerInit(&controller));
    assert(DebugControllerSetProjectContext(&controller, project.projectId, project.rootPath, 9));
    uint64_t breakpointId = 0;
    assert(DebugControllerToggleBreakpoint(&controller, project.projectId, project.rootPath, 9,
                                           "src\\main.cpp", 42, 0, 3, &breakpointId, &error));
    assert(breakpointId != 0 && controller.breakpointCount == 1);
    assert(controller.breakpoints[0].state == DebugBreakpointState::Pending);
    uint64_t duplicateId = 0;
    assert(DebugControllerToggleBreakpoint(&controller, project.projectId, project.rootPath, 9,
                                           "src/main.cpp", 42, 0, 3, &duplicateId, &error));
    assert(duplicateId == breakpointId && controller.breakpoints[0].state == DebugBreakpointState::Disabled);
    assert(DebugControllerToggleBreakpoint(&controller, project.projectId, project.rootPath, 9,
                                           "src/main.cpp", 42, 0, 3, &duplicateId, &error));
    assert(controller.breakpoints[0].state == DebugBreakpointState::Pending);
    assert(!DebugControllerToggleBreakpoint(&controller, project.projectId, project.rootPath, 9,
                                            "../outside.cpp", 1, 0, 1, &duplicateId, &error));
    assert(error == DebugErrorCode::OutsideProject);

    FakeBackend fake;
    DebugBackend backend = makeBackend(&fake);
    assert(DebugControllerStart(&controller, backend, target, &error));
    assert(controller.state == DebugSessionState::Launching && controller.active);
    const uint64_t firstGeneration = controller.sessionGeneration;
    assert(DebugControllerPoll(&controller, backend));
    assert(controller.state == DebugSessionState::Running);
    assert(controller.processId == 12 && controller.nativeRuntimeId == 77);
    assert(!DebugControllerCanPause(&controller));
    assert(!DebugControllerPause(&controller, backend, &error));
    assert(error == DebugErrorCode::CapabilityUnavailable && !fake.pauseCalled);
    assert(DebugControllerRequestStop(&controller, backend, &error));
    assert(controller.state == DebugSessionState::Stopping && fake.stops == 1);
    assert(DebugControllerPoll(&controller, backend));
    assert(controller.state == DebugSessionState::Exited && !controller.active);
    assert(controller.eventCount <= kDebugMaxEvents);

    fake.polls = 0;
    assert(DebugControllerStart(&controller, backend, target, &error));
    const uint64_t secondGeneration = controller.sessionGeneration;
    assert(secondGeneration != firstGeneration);
    DebugBackendSnapshot stale = {};
    stale.sessionGeneration = firstGeneration;
    stale.state = DebugSessionState::Failed;
    assert(!DebugControllerApplySnapshot(&controller, firstGeneration, stale));
    assert(controller.state == DebugSessionState::Launching);
    assert(DebugControllerPoll(&controller, backend));
    assert(DebugControllerPoll(&controller, backend));
    assert(controller.state == DebugSessionState::Exited);

    DebugControllerMarkSourceGeneration(&controller, project.projectId, "src/main.cpp", 4);
    assert(controller.breakpoints[0].state == DebugBreakpointState::Stale);
    DebugControllerMarkProjectGeneration(&controller, 10);
    assert(controller.breakpoints[0].state == DebugBreakpointState::Stale);

    FakeBackend failing;
    failing.launchFails = true;
    DebugBackend failingBackend = makeBackend(&failing);
    DebugController failed = {};
    assert(DebugControllerInit(&failed));
    assert(!DebugControllerStart(&failed, failingBackend, target, &error));
    assert(failed.state == DebugSessionState::Failed && !failed.active);
    assert(error == DebugErrorCode::LaunchFailed);

    for (uint32_t i = 0; i < kDebugMaxEvents + 8; ++i) {
        DebugBackendSnapshot snapshot = {};
        snapshot.sessionGeneration = controller.sessionGeneration;
        snapshot.state = DebugSessionState::Exited;
        DebugControllerApplySnapshot(&controller, controller.sessionGeneration, snapshot);
    }
    assert(controller.eventCount <= kDebugMaxEvents);
    char oversized[kDebugMaxMessageBytes * 2] = {};
    for (uint32_t i = 0; i + 1 < sizeof(oversized); ++i) oversized[i] = 'x';
    assert(!DebugControllerApplyBreakpointBinding(&controller, controller.sessionGeneration, breakpointId, false,
                                                  0, DebugAddress(), oversized, &error));
    assert(std::strlen(controller.breakpoints[0].message) < sizeof(controller.breakpoints[0].message));

    assert(DebugControllerDeleteBreakpoint(&controller, breakpointId, &error));
    assert(controller.breakpointCount == 0);
    std::cout << "Developer Studio debugger model PASS\n";
    return 0;
}
