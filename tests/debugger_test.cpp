#include "developer_studio_debugger.h"

#include <cassert>
#include <cstring>
#include <iostream>

using namespace guidexos::developer_studio;

static Project validProject() {
    Project project = {};
    project.valid = true;
    project.formatVersion = 1;
    project.kind = ProjectKind::NativeGuiApplication;
    std::strcpy(project.projectId, "com.example.debugger");
    std::strcpy(project.displayName, "Debugger Fixture");
    std::strcpy(project.rootPath, "D:/work/debugger");
    std::strcpy(project.manifestPath, "app/app.json");
    std::strcpy(project.targetProfileId, "guidexos.amd64.hosted.native");
    std::strcpy(project.sourceRoot, "src");
    std::strcpy(project.entryPoint, "gx_main");
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
    uint32_t bindCalls = 0;
    uint32_t physicalBinds = 0;
    uint32_t debugCommands = 0;
    uint32_t restoreCommands = 0;
    bool launchFails = false;
    bool pauseCalled = false;
    bool trapOnSecondPoll = false;
    bool delayRuntimePublication = false;
    uint32_t failBindAt = 0;
    DebugSessionState nextState = DebugSessionState::Running;
    uint64_t sharedBindingId = 9001;
    uint64_t lastBoundAddress = 0;
    uint8_t originalByte = 0x55;
    uint8_t installedByte = 0xCC;
    uint32_t continueCalls = 0;
    bool continuePending = false;
    bool terminalOnResume = false;
    bool terminalOnContinue = false;
    DebugRegisterContext lastContinueContext = {};
    uint64_t lastContinueBreakpointId = 0;
    uint64_t lastContinueBindingId = 0;
    uint64_t lastContinueAddress = 0;
    bool lastContinueReinstall = false;
};

static bool launch(void* userData, const DebugTarget&, uint64_t generation, DebugBackendSnapshot* snapshot) {
    FakeBackend* fake = static_cast<FakeBackend*>(userData);
    ++fake->launches;
    *snapshot = DebugBackendSnapshot();
    snapshot->sessionGeneration = generation;
    snapshot->state = fake->launchFails ? DebugSessionState::Failed : DebugSessionState::Launching;
    snapshot->processId = 12;
    snapshot->nativeRuntimeId = fake->delayRuntimePublication ? 0 : 77;
    std::strcpy(snapshot->backendName, "Test Backend");
    if (fake->launchFails) std::strcpy(snapshot->errorMessage, "test launch failed");
    return !fake->launchFails;
}

static bool poll(void* userData, uint64_t generation, DebugBackendSnapshot* snapshot) {
    FakeBackend* fake = static_cast<FakeBackend*>(userData);
    *snapshot = DebugBackendSnapshot();
    snapshot->sessionGeneration = generation;
    snapshot->processId = 12;
    const uint32_t pollIndex = fake->polls++;
    snapshot->nativeRuntimeId = fake->delayRuntimePublication && pollIndex == 0 ? 0 : 77;
    if (fake->trapOnSecondPoll && pollIndex == 1) {
        snapshot->state = DebugSessionState::Paused;
        snapshot->stopReason = DebugStopReason::Breakpoint;
        snapshot->breakpointTrap = true;
        snapshot->threadId = 44;
        snapshot->instructionPointer = 0x401011;
        snapshot->targetAddress.valid = true;
        snapshot->targetAddress.value = 0x401010;
        snapshot->breakpointBindingId = fake->sharedBindingId;
        snapshot->stopGeneration = 1;
        snapshot->registerContext.valid = true;
        snapshot->registerContext.architecture = DebugArchitecture::Amd64;
        snapshot->registerContext.processId = 12;
        snapshot->registerContext.nativeRuntimeId = 77;
        snapshot->registerContext.threadId = 44;
        snapshot->registerContext.sessionGeneration = generation;
        snapshot->registerContext.stopGeneration = 1;
        snapshot->registerContext.rip = snapshot->instructionPointer;
        snapshot->registerContext.rflags = 0x202;
        snapshot->registerContext.rsp = 0x700000;
        snapshot->registerContext.rbp = 0x700100;
        return true;
    }
    if (fake->continuePending) {
        fake->continuePending = false;
        snapshot->state = DebugSessionState::Running;
        snapshot->stopReason = DebugStopReason::None;
        snapshot->executionState = DebugBackendExecutionState::Running;
        return true;
    }
    snapshot->state = fake->delayRuntimePublication ?
        (pollIndex == 0 ? DebugSessionState::Launching :
         (pollIndex == 1 ? fake->nextState : DebugSessionState::Exited)) :
        (pollIndex == 0 ? fake->nextState : DebugSessionState::Exited);
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

static bool continueExecution(void* userData, uint64_t generation, const DebugRegisterContext& context,
                              uint64_t breakpointId, uint64_t bindingId, uint64_t targetAddress,
                              bool reinstallBreakpoint) {
    FakeBackend* fake = static_cast<FakeBackend*>(userData);
    if (!fake || !context.valid || context.sessionGeneration != generation) return false;
    ++fake->continueCalls;
    fake->lastContinueContext = context;
    fake->lastContinueBreakpointId = breakpointId;
    fake->lastContinueBindingId = bindingId;
    fake->lastContinueAddress = targetAddress;
    fake->lastContinueReinstall = reinstallBreakpoint;
    fake->continuePending = !fake->terminalOnContinue;
    return true;
}

static bool resumeExecution(void* userData, uint64_t generation,
                            const DebugRegisterContext& context) {
    FakeBackend* fake = static_cast<FakeBackend*>(userData);
    if (!fake || !context.valid || context.sessionGeneration != generation) return false;
    ++fake->continueCalls;
    fake->lastContinueContext = context;
    fake->continuePending = !fake->terminalOnResume;
    return true;
}

static bool consumeResumeTerminal(void* userData, uint64_t generation,
                                  DebugBackendSnapshot* snapshot) {
    FakeBackend* fake = static_cast<FakeBackend*>(userData);
    if (!fake || !snapshot || (!fake->terminalOnResume && !fake->terminalOnContinue)) return false;
    fake->terminalOnResume = false;
    fake->terminalOnContinue = false;
    *snapshot = DebugBackendSnapshot();
    snapshot->sessionGeneration = generation;
    snapshot->state = DebugSessionState::Exited;
    snapshot->stopReason = DebugStopReason::Exited;
    snapshot->nativeRuntimeId = 77;
    snapshot->exitCode = 0;
    snapshot->cleanupComplete = true;
    return true;
}

static bool bindSoftwareBreakpoint(void* userData, const DebugTarget&, uint64_t,
                                   uint64_t, uint64_t, const DebugBreakpoint& breakpoint,
                                   DebugBackendBinding* binding) {
    FakeBackend* fake = static_cast<FakeBackend*>(userData);
    if (!fake || !binding || !breakpoint.location.instructionAddress.valid) return false;
    ++fake->bindCalls;
    if (fake->failBindAt != 0 && fake->bindCalls == fake->failBindAt) {
        std::strcpy(binding->message, "Rejected: test bind failure");
        return true;
    }
    if (fake->lastBoundAddress != breakpoint.location.instructionAddress.value) {
        fake->lastBoundAddress = breakpoint.location.instructionAddress.value;
        ++fake->physicalBinds;
    }
    *binding = DebugBackendBinding();
    binding->accepted = true;
    binding->bindingId = fake->sharedBindingId;
    binding->originalByte = fake->originalByte;
    binding->installedByte = fake->installedByte;
    binding->originalByteValid = true;
    std::strcpy(binding->message, "Bound / Verified");
    return true;
}

static bool debugCommand(void* userData, HostedDebugCommand command, uint64_t,
                         uint64_t, uint64_t, uint64_t, uint64_t, uint64_t,
                         const char*, uint64_t, uint64_t, bool, uint64_t, uint32_t,
                         HostedDebugResult* result) {
    FakeBackend* fake = static_cast<FakeBackend*>(userData);
    if (!fake || !result) return false;
    ++fake->debugCommands;
    if (command == HostedDebugCommand::RestoreAll) ++fake->restoreCommands;
    *result = HostedDebugResult();
    result->status = command == HostedDebugCommand::ReleaseExecution ? 1 :
        (command == HostedDebugCommand::RestoreAll ? 4 : 2);
    result->bindingId = fake->sharedBindingId;
    result->originalByte = fake->originalByte;
    result->installedByte = fake->installedByte;
    result->originalByteValid = true;
    result->bindingInstalled = command == HostedDebugCommand::BindSoftwareBreakpoint;
    return true;
}

static DebugBackend makeBackend(FakeBackend* fake) {
    DebugBackend backend = {};
    backend.userData = fake;
    backend.capabilities.canLaunch = true;
    backend.capabilities.canStop = true;
    backend.capabilities.canPause = false;
    backend.capabilities.canContinue = true;
    std::strcpy(backend.name, "Test Backend");
    backend.launch = launch;
    backend.poll = poll;
    backend.stop = stop;
    backend.pause = pause;
    return backend;
}

static DebugBackend makeBreakpointBackend(FakeBackend* fake) {
    DebugBackend backend = makeBackend(fake);
    backend.capabilities.canSetInstructionBreakpoint = true;
    backend.capabilities.canSetSourceBreakpoint = true;
    backend.capabilities.canBindSoftwareBreakpoint = true;
    backend.capabilities.canObserveBreakpointTrap = true;
    backend.capabilities.canRestoreBreakpoint = true;
    backend.capabilities.canReadInstructionPointer = true;
    backend.bindSoftwareBreakpoint = bindSoftwareBreakpoint;
    backend.debugCommand = debugCommand;
    backend.continueExecution = continueExecution;
    backend.consumeResumeTerminal = consumeResumeTerminal;
    return backend;
}

static DebugBackend makePauseBackend(FakeBackend* fake) {
    DebugBackend backend = makeBackend(fake);
    backend.capabilities.canPause = true;
    backend.resumeExecution = resumeExecution;
    backend.consumeResumeTerminal = consumeResumeTerminal;
    return backend;
}

static void prepareMappedBreakpoint(DebugController* controller, uint64_t breakpointId) {
    assert(controller && controller->breakpointCount == 1);
    DebugBreakpoint& breakpoint = controller->breakpoints[0];
    assert(breakpoint.id == breakpointId);
    breakpoint.state = DebugBreakpointState::Mapped;
    breakpoint.location.mapping = DebugMappingState::Mapped;
    breakpoint.location.instructionAddress.valid = true;
    breakpoint.location.instructionAddress.value = 0x401010;
    breakpoint.mappedAddressCount = 1;
    breakpoint.mappedAddresses[0] = breakpoint.location.instructionAddress;
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

    FakeBackend pauseFake;
    DebugBackend pauseBackend = makePauseBackend(&pauseFake);
    static DebugController pauseController = {};
    assert(DebugControllerInit(&pauseController));
    assert(DebugControllerSetProjectContext(&pauseController, project.projectId, project.rootPath, 9));
    assert(DebugControllerStart(&pauseController, pauseBackend, target, &error));
    assert(DebugControllerPoll(&pauseController, pauseBackend));
    assert(pauseController.state == DebugSessionState::Running);
    assert(DebugControllerCanPause(&pauseController));
    assert(DebugControllerPause(&pauseController, pauseBackend, &error));
    assert(pauseController.pauseRequestPending && pauseFake.pauseCalled);
    assert(!DebugControllerPause(&pauseController, pauseBackend, &error));
    assert(error == DebugErrorCode::PauseAlreadyRequested);
    DebugBackendSnapshot userPause = {};
    userPause.sessionGeneration = pauseController.sessionGeneration;
    userPause.state = DebugSessionState::Paused;
    userPause.stopReason = DebugStopReason::UserPause;
    userPause.processId = 0;
    userPause.nativeRuntimeId = 77;
    userPause.threadId = 1;
    userPause.instructionPointer = 0x401234;
    userPause.targetAddress.valid = true;
    userPause.targetAddress.value = 0x401234;
    userPause.stopGeneration = 4;
    userPause.executionState = DebugBackendExecutionState::PausedAtUserPause;
    userPause.registerContext.valid = true;
    userPause.registerContext.architecture = DebugArchitecture::Amd64;
    userPause.registerContext.processId = 0;
    userPause.registerContext.nativeRuntimeId = 77;
    userPause.registerContext.threadId = 1;
    userPause.registerContext.sessionGeneration = pauseController.sessionGeneration;
    userPause.registerContext.stopGeneration = 4;
    userPause.registerContext.rip = 0x401234;
    userPause.registerContext.rflags = 0x202;
    userPause.registerContext.rsp = 0x700008;
    userPause.registerContext.rbp = 0x700100;
    assert(DebugControllerApplySnapshot(&pauseController, pauseController.sessionGeneration, userPause));
    assert(pauseController.state == DebugSessionState::Paused);
    assert(pauseController.stopReason == DebugStopReason::UserPause);
    assert(pauseController.backendExecutionState == DebugBackendExecutionState::PausedAtUserPause);
    assert(pauseController.currentInstructionAddress.valid &&
           pauseController.currentInstructionAddress.value == 0x401234);
    assert(!pauseController.pauseRequestPending && DebugControllerCanContinue(&pauseController));
    assert(!DebugControllerPause(&pauseController, pauseBackend, &error));
    assert(error == DebugErrorCode::AlreadyPaused);
    assert(DebugControllerContinue(&pauseController, pauseBackend, &error));
    assert(pauseController.state == DebugSessionState::Running &&
           pauseController.stopReason == DebugStopReason::None);
    assert(pauseFake.continueCalls == 1);
    assert(DebugControllerPoll(&pauseController, pauseBackend));
    assert(pauseController.state == DebugSessionState::Running);
    assert(DebugControllerPause(&pauseController, pauseBackend, &error));
    assert(pauseController.pauseRequestPending);
    assert(DebugControllerRequestStop(&pauseController, pauseBackend, &error));
    assert(!pauseController.pauseRequestPending && pauseController.state == DebugSessionState::Stopping);
    assert(DebugControllerPoll(&pauseController, pauseBackend));
    assert(pauseController.state == DebugSessionState::Exited && !pauseController.active);

    FakeBackend terminalResumeFake;
    DebugBackend terminalResumeBackend = makePauseBackend(&terminalResumeFake);
    static DebugController terminalResumeController = {};
    assert(DebugControllerInit(&terminalResumeController));
    assert(DebugControllerSetProjectContext(&terminalResumeController, project.projectId,
                                            project.rootPath, 9));
    assert(DebugControllerStart(&terminalResumeController, terminalResumeBackend, target, &error));
    assert(DebugControllerPoll(&terminalResumeController, terminalResumeBackend));
    assert(DebugControllerPause(&terminalResumeController, terminalResumeBackend, &error));
    userPause.sessionGeneration = terminalResumeController.sessionGeneration;
    userPause.registerContext.sessionGeneration = terminalResumeController.sessionGeneration;
    assert(DebugControllerApplySnapshot(&terminalResumeController,
                                        terminalResumeController.sessionGeneration, userPause));
    terminalResumeFake.terminalOnResume = true;
    assert(DebugControllerContinue(&terminalResumeController, terminalResumeBackend, &error));
    assert(terminalResumeController.state == DebugSessionState::Exited &&
           !terminalResumeController.active && !terminalResumeController.pauseRequestPending);

    FakeBackend completionFake;
    DebugBackend completionBackend = makePauseBackend(&completionFake);
    static DebugController completion = {};
    assert(DebugControllerInit(&completion));
    assert(DebugControllerSetProjectContext(&completion, project.projectId, project.rootPath, 9));
    assert(DebugControllerStart(&completion, completionBackend, target, &error));
    assert(DebugControllerPoll(&completion, completionBackend));
    assert(DebugControllerPause(&completion, completionBackend, &error));
    DebugBackendSnapshot completionSnapshot = {};
    completionSnapshot.sessionGeneration = completion.sessionGeneration;
    completionSnapshot.state = DebugSessionState::Exited;
    completionSnapshot.stopReason = DebugStopReason::Exited;
    completionSnapshot.cleanupComplete = true;
    assert(DebugControllerApplySnapshot(&completion, completion.sessionGeneration, completionSnapshot));
    assert(completion.state == DebugSessionState::Exited && !completion.active &&
           !completion.pauseRequestPending);
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

    FakeBackend breakpointFake;
    DebugBackend breakpointBackend = makeBreakpointBackend(&breakpointFake);
    DebugController bound = {};
    assert(DebugControllerInit(&bound));
    assert(DebugControllerSetProjectContext(&bound, project.projectId, project.rootPath, 9));
    uint64_t boundBreakpointId = 0;
    assert(DebugControllerToggleBreakpoint(&bound, project.projectId, project.rootPath, 9,
                                           "src/main.cpp", 50, 0, 3, &boundBreakpointId, &error));
    prepareMappedBreakpoint(&bound, boundBreakpointId);
    assert(DebugControllerStart(&bound, breakpointBackend, target, &error));
    assert(DebugControllerPoll(&bound, breakpointBackend));
    assert(bound.state == DebugSessionState::Running);
    assert(bound.targetExecutionReleased);
    assert(bound.breakpoints[0].state == DebugBreakpointState::Verified);
    assert(bound.breakpoints[0].backendBindingId == breakpointFake.sharedBindingId);
    assert(breakpointFake.bindCalls == 1 && breakpointFake.physicalBinds == 1);
    assert(breakpointFake.originalByte == 0x55 && breakpointFake.installedByte == 0xCC);
    assert(breakpointFake.debugCommands == 1);
    assert(bound.state != DebugSessionState::Paused);

    FakeBackend duplicateFake;
    DebugBackend duplicateBackend = makeBreakpointBackend(&duplicateFake);
    DebugController duplicates = {};
    assert(DebugControllerInit(&duplicates));
    assert(DebugControllerSetProjectContext(&duplicates, project.projectId, project.rootPath, 9));
    uint64_t firstDuplicateId = 0;
    uint64_t secondDuplicateId = 0;
    assert(DebugControllerToggleBreakpoint(&duplicates, project.projectId, project.rootPath, 9,
                                           "src/main.cpp", 60, 0, 3, &firstDuplicateId, &error));
    assert(DebugControllerToggleBreakpoint(&duplicates, project.projectId, project.rootPath, 9,
                                           "src/main.cpp", 61, 0, 3, &secondDuplicateId, &error));
    for (uint32_t i = 0; i < duplicates.breakpointCount; ++i) {
        duplicates.breakpoints[i].state = DebugBreakpointState::Mapped;
        duplicates.breakpoints[i].location.mapping = DebugMappingState::Mapped;
        duplicates.breakpoints[i].location.instructionAddress.valid = true;
        duplicates.breakpoints[i].location.instructionAddress.value = 0x401010;
        duplicates.breakpoints[i].mappedAddressCount = 1;
        duplicates.breakpoints[i].mappedAddresses[0] = duplicates.breakpoints[i].location.instructionAddress;
    }
    assert(DebugControllerStart(&duplicates, duplicateBackend, target, &error));
    assert(DebugControllerPoll(&duplicates, duplicateBackend));
    assert(duplicates.breakpoints[0].state == DebugBreakpointState::Verified);
    assert(duplicates.breakpoints[1].state == DebugBreakpointState::Verified);
    assert(duplicates.breakpoints[0].backendBindingId == duplicates.breakpoints[1].backendBindingId);
    assert(duplicateFake.bindCalls == 2 && duplicateFake.physicalBinds == 1);

    FakeBackend hitFake;
    hitFake.trapOnSecondPoll = true;
    DebugBackend hitBackend = makeBreakpointBackend(&hitFake);
    DebugController hit = {};
    assert(DebugControllerInit(&hit));
    assert(DebugControllerSetProjectContext(&hit, project.projectId, project.rootPath, 9));
    uint64_t hitBreakpointId = 0;
    assert(DebugControllerToggleBreakpoint(&hit, project.projectId, project.rootPath, 9,
                                           "src/main.cpp", 70, 0, 3, &hitBreakpointId, &error));
    prepareMappedBreakpoint(&hit, hitBreakpointId);
    assert(DebugControllerStart(&hit, hitBackend, target, &error));
    assert(DebugControllerPoll(&hit, hitBackend));
    assert(hit.state == DebugSessionState::Running);
    assert(DebugControllerPoll(&hit, hitBackend));
    assert(hit.state == DebugSessionState::Paused);
    assert(hit.stopReason == DebugStopReason::Breakpoint);
    assert(hit.currentInstructionAddress.valid && hit.currentInstructionAddress.value == 0x401010);
    assert(hit.reportedInstructionPointer == 0x401011);
    assert(hit.currentThreadId == 44 && hit.lastBreakpointId == hitBreakpointId);
    assert(hit.breakpoints[0].lastHit && hit.breakpoints[0].state == DebugBreakpointState::Verified);
    assert(DebugRegisterContextIsValid(hit.stoppedContext));
    assert(DebugControllerCanContinue(&hit));
    assert(DebugControllerContinue(&hit, hitBackend, &error));
    assert(hit.state == DebugSessionState::Paused);
    assert(hit.backendExecutionState == DebugBackendExecutionState::SingleStepPending);
    assert(!DebugControllerCanContinue(&hit));
    assert(!DebugControllerContinue(&hit, hitBackend, &error));
    assert(error == DebugErrorCode::CapabilityUnavailable);
    assert(DebugControllerPoll(&hit, hitBackend));
    assert(hit.state == DebugSessionState::Running);
    assert(hit.backendExecutionState == DebugBackendExecutionState::Running);
    assert(!hit.stoppedContext.valid && !hit.currentInstructionAddress.valid && hit.currentThreadId == 0);
    assert(hit.stopReason == DebugStopReason::Breakpoint);
    assert(hitFake.continueCalls == 1 && hitFake.lastContinueBreakpointId == hitBreakpointId &&
           hitFake.lastContinueBindingId == hitFake.sharedBindingId &&
           hitFake.lastContinueAddress == 0x401010 && hitFake.lastContinueReinstall);
    assert(hitFake.lastContinueContext.sessionGeneration == hit.sessionGeneration &&
           hitFake.lastContinueContext.stopGeneration == 1 &&
           hitFake.lastContinueContext.threadId == 44);

    // Phase 28W: Continue can be accepted at a valid breakpoint stop while
    // the target returns before the normal acknowledgement path.  The
    // terminal hook must complete the same command exactly once, and a late
    // duplicate must not re-enter the backend.
    FakeBackend breakpointExitFake;
    breakpointExitFake.trapOnSecondPoll = true;
    DebugBackend breakpointExitBackend = makeBreakpointBackend(&breakpointExitFake);
    static DebugController breakpointExit = {};
    assert(DebugControllerInit(&breakpointExit));
    assert(DebugControllerSetProjectContext(&breakpointExit, project.projectId, project.rootPath, 9));
    uint64_t breakpointExitId = 0;
    assert(DebugControllerToggleBreakpoint(&breakpointExit, project.projectId, project.rootPath, 9,
                                           "src/main.cpp", 71, 0, 3, &breakpointExitId, &error));
    prepareMappedBreakpoint(&breakpointExit, breakpointExitId);
    assert(DebugControllerStart(&breakpointExit, breakpointExitBackend, target, &error));
    assert(DebugControllerPoll(&breakpointExit, breakpointExitBackend));
    assert(DebugControllerPoll(&breakpointExit, breakpointExitBackend));
    assert(breakpointExit.state == DebugSessionState::Paused);
    const uint32_t continueCallsBeforeExit = breakpointExitFake.continueCalls;
    breakpointExitFake.terminalOnContinue = true;
    assert(DebugControllerContinue(&breakpointExit, breakpointExitBackend, &error));
    assert(breakpointExit.state == DebugSessionState::Exited && !breakpointExit.active);
    assert(breakpointExitFake.continueCalls == continueCallsBeforeExit + 1);
    assert(!DebugControllerContinue(&breakpointExit, breakpointExitBackend, &error));
    assert(breakpointExitFake.continueCalls == continueCallsBeforeExit + 1);

    // A fresh launch generation cannot consume the previous generation's
    // terminal completion.  The controller must establish a new stop before
    // accepting another Continue.
    FakeBackend relaunchFake;
    DebugBackend relaunchBackend = makePauseBackend(&relaunchFake);
    static DebugController relaunch = {};
    assert(DebugControllerInit(&relaunch));
    assert(DebugControllerSetProjectContext(&relaunch, project.projectId, project.rootPath, 9));
    assert(DebugControllerStart(&relaunch, relaunchBackend, target, &error));
    assert(DebugControllerPoll(&relaunch, relaunchBackend));
    assert(DebugControllerPause(&relaunch, relaunchBackend, &error));
    userPause.sessionGeneration = relaunch.sessionGeneration;
    userPause.registerContext.sessionGeneration = relaunch.sessionGeneration;
    assert(DebugControllerApplySnapshot(&relaunch, relaunch.sessionGeneration, userPause));
    relaunchFake.terminalOnResume = true;
    const uint64_t relaunchGeneration = relaunch.sessionGeneration;
    assert(DebugControllerContinue(&relaunch, relaunchBackend, &error));
    assert(relaunch.state == DebugSessionState::Exited);
    relaunchFake.polls = 0;
    assert(DebugControllerStart(&relaunch, relaunchBackend, target, &error));
    assert(relaunch.sessionGeneration != relaunchGeneration);
    assert(relaunch.state == DebugSessionState::Launching);
    assert(!DebugControllerContinue(&relaunch, relaunchBackend, &error));

    FakeBackend partialFake;
    partialFake.failBindAt = 2;
    DebugBackend partialBackend = makeBreakpointBackend(&partialFake);
    DebugController partial = {};
    assert(DebugControllerInit(&partial));
    assert(DebugControllerSetProjectContext(&partial, project.projectId, project.rootPath, 9));
    uint64_t partialFirstId = 0;
    uint64_t partialSecondId = 0;
    assert(DebugControllerToggleBreakpoint(&partial, project.projectId, project.rootPath, 9,
                                           "src/main.cpp", 80, 0, 3, &partialFirstId, &error));
    assert(DebugControllerToggleBreakpoint(&partial, project.projectId, project.rootPath, 9,
                                           "src/main.cpp", 81, 0, 3, &partialSecondId, &error));
    for (uint32_t i = 0; i < partial.breakpointCount; ++i) {
        partial.breakpoints[i].state = DebugBreakpointState::Mapped;
        partial.breakpoints[i].location.mapping = DebugMappingState::Mapped;
        partial.breakpoints[i].location.instructionAddress.valid = true;
        partial.breakpoints[i].location.instructionAddress.value = 0x401010 + i * 4;
        partial.breakpoints[i].mappedAddressCount = 1;
        partial.breakpoints[i].mappedAddresses[0] = partial.breakpoints[i].location.instructionAddress;
    }
    assert(DebugControllerStart(&partial, partialBackend, target, &error));
    assert(!DebugControllerPoll(&partial, partialBackend));
    assert(partial.state == DebugSessionState::Failed && !partial.active);
    assert(partialFake.restoreCommands == 1 && partialFake.debugCommands == 1);
    assert(partial.breakpoints[0].backendBindingId == 0 && partial.breakpoints[0].state == DebugBreakpointState::Mapped);

    FakeBackend delayedFake;
    delayedFake.delayRuntimePublication = true;
    DebugBackend delayedBackend = makeBreakpointBackend(&delayedFake);
    DebugController delayed = {};
    assert(DebugControllerInit(&delayed));
    assert(DebugControllerSetProjectContext(&delayed, project.projectId, project.rootPath, 9));
    uint64_t delayedBreakpointId = 0;
    assert(DebugControllerToggleBreakpoint(&delayed, project.projectId, project.rootPath, 9,
                                           "src/main.cpp", 85, 0, 3, &delayedBreakpointId, &error));
    prepareMappedBreakpoint(&delayed, delayedBreakpointId);
    assert(DebugControllerStart(&delayed, delayedBackend, target, &error));
    assert(DebugControllerPoll(&delayed, delayedBackend));
    assert(delayed.state == DebugSessionState::Launching && !delayed.targetExecutionReleased);
    assert(delayedFake.bindCalls == 0 && delayedFake.debugCommands == 0);
    assert(DebugControllerPoll(&delayed, delayedBackend));
    assert(delayed.state == DebugSessionState::Running && delayed.targetExecutionReleased);
    assert(delayedFake.bindCalls == 1 && delayedFake.debugCommands == 1);
    bool sawTargetCreated = false;
    uint64_t previousEventSequence = 0;
    for (uint32_t i = 0; i < delayed.eventCount; ++i) {
        const DebugEvent& event = delayed.events[i];
        assert(event.sequence > previousEventSequence);
        previousEventSequence = event.sequence;
        if (event.kind == DebugEventKind::TargetCreated) {
            sawTargetCreated = true;
            assert(event.processId == 12 && event.nativeRuntimeId == 77);
        }
    }
    assert(sawTargetCreated);
    assert(DebugControllerPoll(&delayed, delayedBackend));
    assert(delayed.state == DebugSessionState::Exited && !delayed.active);

    FakeBackend externalReleaseFake;
    DebugBackend externalReleaseBackend = makeBreakpointBackend(&externalReleaseFake);
    static DebugController externalRelease = {};
    assert(DebugControllerInit(&externalRelease));
    assert(DebugControllerSetProjectContext(&externalRelease, project.projectId, project.rootPath, 9));
    DebugControllerSetExecutionReleaseDeferred(&externalRelease, true);
    assert(DebugControllerStart(&externalRelease, externalReleaseBackend, target, &error));
    assert(externalRelease.state == DebugSessionState::Launching);
    const uint64_t externalReleaseGeneration = externalRelease.sessionGeneration;
    assert(DebugControllerAcceptExternalExecutionRelease(&externalRelease,
                                                         externalReleaseGeneration, &error));
    assert(externalRelease.state == DebugSessionState::Running);
    assert(externalRelease.targetExecutionReleased);
    assert(DebugControllerAcceptExternalExecutionRelease(&externalRelease,
                                                         externalReleaseGeneration, &error));
    assert(error == DebugErrorCode::None && externalRelease.state == DebugSessionState::Running);
    assert(!DebugControllerAcceptExternalExecutionRelease(&externalRelease,
                                                          externalReleaseGeneration + 1, &error));
    assert(error == DebugErrorCode::InvalidTransition);

    FakeBackend firstRejectFake;
    firstRejectFake.failBindAt = 1;
    DebugBackend firstRejectBackend = makeBreakpointBackend(&firstRejectFake);
    DebugController firstReject = {};
    assert(DebugControllerInit(&firstReject));
    assert(DebugControllerSetProjectContext(&firstReject, project.projectId, project.rootPath, 9));
    uint64_t firstRejectId = 0;
    assert(DebugControllerToggleBreakpoint(&firstReject, project.projectId, project.rootPath, 9,
                                           "src/main.cpp", 86, 0, 3, &firstRejectId, &error));
    prepareMappedBreakpoint(&firstReject, firstRejectId);
    assert(DebugControllerStart(&firstReject, firstRejectBackend, target, &error));
    assert(!DebugControllerPoll(&firstReject, firstRejectBackend));
    assert(firstReject.state == DebugSessionState::Failed && !firstReject.active);
    assert(!firstReject.targetExecutionReleased && firstRejectFake.restoreCommands == 1);
    assert(firstReject.breakpoints[0].state == DebugBreakpointState::Rejected);

    FakeBackend unexpectedFake;
    DebugBackend unexpectedBackend = makeBreakpointBackend(&unexpectedFake);
    DebugController unexpected = {};
    assert(DebugControllerInit(&unexpected));
    assert(DebugControllerSetProjectContext(&unexpected, project.projectId, project.rootPath, 9));
    uint64_t unexpectedBreakpointId = 0;
    assert(DebugControllerToggleBreakpoint(&unexpected, project.projectId, project.rootPath, 9,
                                           "src/main.cpp", 90, 0, 3, &unexpectedBreakpointId, &error));
    prepareMappedBreakpoint(&unexpected, unexpectedBreakpointId);
    assert(DebugControllerStart(&unexpected, unexpectedBackend, target, &error));
    assert(DebugControllerPoll(&unexpected, unexpectedBackend));
    DebugBackendSnapshot wrongTrap = {};
    wrongTrap.sessionGeneration = unexpected.sessionGeneration;
    wrongTrap.state = DebugSessionState::Paused;
    wrongTrap.processId = 999;
    wrongTrap.nativeRuntimeId = 77;
    wrongTrap.breakpointTrap = true;
    wrongTrap.targetAddress.valid = true;
    wrongTrap.targetAddress.value = 0x401010;
    wrongTrap.breakpointBindingId = unexpectedFake.sharedBindingId;
    assert(!DebugControllerApplySnapshot(&unexpected, unexpected.sessionGeneration, wrongTrap));
    assert(unexpected.error == DebugErrorCode::BackendError && unexpected.state == DebugSessionState::Running);
    wrongTrap.processId = 12;
    wrongTrap.targetAddress.value = 0x401011;
    assert(!DebugControllerApplySnapshot(&unexpected, unexpected.sessionGeneration, wrongTrap));
    assert(unexpected.error == DebugErrorCode::BackendError && unexpected.state == DebugSessionState::Running);
    DebugBackendSnapshot missingTrap = {};
    missingTrap.sessionGeneration = unexpected.sessionGeneration;
    missingTrap.state = DebugSessionState::Paused;
    missingTrap.stopReason = DebugStopReason::Breakpoint;
    missingTrap.processId = 12;
    missingTrap.nativeRuntimeId = 77;
    assert(!DebugControllerApplySnapshot(&unexpected, unexpected.sessionGeneration, missingTrap));
    assert(unexpected.error == DebugErrorCode::BackendError && unexpected.state == DebugSessionState::Running);
    assert(DebugControllerPoll(&unexpected, unexpectedBackend));
    assert(unexpected.state == DebugSessionState::Exited);

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
