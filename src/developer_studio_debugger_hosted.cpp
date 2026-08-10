#include "developer_studio_debugger_hosted.h"

namespace guidexos {
namespace developer_studio {
namespace {

static uint32_t textLength(const char* value, uint32_t capacity) {
    if (!value) return 0;
    uint32_t length = 0;
    while (length < capacity && value[length] != '\0') ++length;
    return length;
}

static void copyText(char* output, uint32_t outputSize, const char* input) {
    if (!output || outputSize == 0) return;
    uint32_t length = textLength(input, outputSize);
    if (length >= outputSize) length = outputSize - 1;
    for (uint32_t i = 0; i < length; ++i) output[i] = input[i];
    output[length] = '\0';
}

static RunRequest makeRunRequest(const DebugTarget& target) {
    RunRequest request = {};
    copyText(request.projectRoot, sizeof(request.projectRoot), target.projectRoot);
    copyText(request.projectId, sizeof(request.projectId), target.projectId);
    copyText(request.projectKind, sizeof(request.projectKind), "native-gui-application");
    copyText(request.targetProfile, sizeof(request.targetProfile), target.targetProfile);
    copyText(request.manifestPath, sizeof(request.manifestPath), target.manifestPath);
    copyText(request.artifactPath, sizeof(request.artifactPath), target.executablePath);
    copyText(request.artifactSha256, sizeof(request.artifactSha256), target.artifactSha256);
    request.debugControlled = true;
    return request;
}

static void snapshotFromRun(const HostedDebugBackend& backend, uint64_t generation,
                            DebugBackendSnapshot* snapshot) {
    if (!snapshot) return;
    *snapshot = DebugBackendSnapshot();
    snapshot->sessionGeneration = generation;
    snapshot->state = backend.runController.state == RunState::Completed ? DebugSessionState::Exited :
        (backend.runController.state == RunState::Failed ? DebugSessionState::Failed :
         (backend.runController.state == RunState::Running ? DebugSessionState::Running :
          (backend.runController.state == RunState::CleaningUp || backend.runController.closeRequested ? DebugSessionState::Stopping : DebugSessionState::Launching)));
    snapshot->processId = backend.runController.result.processId;
    snapshot->nativeRuntimeId = backend.runController.result.nativeRuntimeId;
    snapshot->debugHandle = backend.runController.result.handle;
    snapshot->exitCode = backend.runController.result.exitCode;
    snapshot->cleanupComplete = backend.runController.result.cleanupComplete;
    snapshot->stopReason = snapshot->state == DebugSessionState::Exited ? DebugStopReason::Exited :
        (backend.runController.closeRequested ? DebugStopReason::UserRequested : DebugStopReason::None);
    snapshot->executionState = snapshot->state == DebugSessionState::Running ?
        DebugBackendExecutionState::Running : DebugBackendExecutionState::None;
    copyText(snapshot->backendName, sizeof(snapshot->backendName), "Hosted Native ELF");
    if (backend.runController.result.errorMessage[0]) copyText(snapshot->errorMessage, sizeof(snapshot->errorMessage), backend.runController.result.errorMessage);
    else if (backend.runController.result.error != RunErrorCode::None) copyText(snapshot->errorMessage, sizeof(snapshot->errorMessage), RunErrorName(backend.runController.result.error));
}

static bool launch(void* userData, const DebugTarget& target, uint64_t generation, DebugBackendSnapshot* outSnapshot) {
    HostedDebugBackend* backend = static_cast<HostedDebugBackend*>(userData);
    if (!backend || !outSnapshot) return false;
    RunControllerInit(&backend->runController);
    const RunRequest request = makeRunRequest(target);
    RunErrorCode error = RunErrorCode::None;
    if (!RunControllerPrepare(&backend->runController, backend->runService, request, &error) ||
        !RunControllerStart(&backend->runController, backend->runService, &error)) {
        snapshotFromRun(*backend, generation, outSnapshot);
        if (outSnapshot->errorMessage[0] == '\0') copyText(outSnapshot->errorMessage, sizeof(outSnapshot->errorMessage), RunErrorName(error));
        return false;
    }
    snapshotFromRun(*backend, generation, outSnapshot);
    return true;
}

static bool poll(void* userData, uint64_t generation, DebugBackendSnapshot* outSnapshot) {
    HostedDebugBackend* backend = static_cast<HostedDebugBackend*>(userData);
    if (!backend || !outSnapshot || !RunControllerIsActive(&backend->runController)) return false;
    const uint64_t handle = backend->runController.result.handle;
    if (!RunControllerPoll(&backend->runController, backend->runService)) return false;
    snapshotFromRun(*backend, generation, outSnapshot);
    if (backend->runService.debugCommand && handle != 0 && outSnapshot->processId != 0 && outSnapshot->nativeRuntimeId != 0 &&
        (outSnapshot->state == DebugSessionState::Running || outSnapshot->state == DebugSessionState::Launching)) {
        HostedDebugResult debugResult = {};
        if (!backend->runService.debugCommand(backend->runService.userData, HostedDebugCommand::Poll, handle,
                                              generation, outSnapshot->processId, outSnapshot->nativeRuntimeId,
                                              0, 0, backend->runController.request.artifactSha256, 0, 0, false, &debugResult)) {
            copyText(outSnapshot->errorMessage, sizeof(outSnapshot->errorMessage), debugResult.errorMessage[0] ? debugResult.errorMessage : "Hosted debugger trap poll failed");
            return false;
        }
        if (debugResult.status == 3 && debugResult.trapKind == 1) {
            outSnapshot->state = DebugSessionState::Paused;
            outSnapshot->stopReason = DebugStopReason::Breakpoint;
            outSnapshot->breakpointTrap = true;
            outSnapshot->threadId = debugResult.threadId;
            outSnapshot->instructionPointer = debugResult.instructionPointer;
            outSnapshot->targetAddress.valid = true;
            outSnapshot->targetAddress.value = debugResult.targetAddress;
            outSnapshot->breakpointBindingId = debugResult.bindingId;
            outSnapshot->originalByte = debugResult.originalByte;
            outSnapshot->installedByte = debugResult.installedByte;
            outSnapshot->originalByteValid = debugResult.originalByteValid;
            outSnapshot->stopGeneration = debugResult.stopGeneration;
            outSnapshot->executionState = DebugBackendExecutionState::PausedAtBreakpoint;
            outSnapshot->registerContext.valid = debugResult.registerContext.valid;
            outSnapshot->registerContext.architecture = static_cast<DebugArchitecture>(debugResult.registerContext.architecture);
            outSnapshot->registerContext.processId = debugResult.registerContext.processId;
            outSnapshot->registerContext.nativeRuntimeId = debugResult.registerContext.nativeRuntimeId;
            outSnapshot->registerContext.threadId = debugResult.registerContext.threadId;
            outSnapshot->registerContext.sessionGeneration = debugResult.registerContext.sessionGeneration;
            outSnapshot->registerContext.stopGeneration = debugResult.registerContext.stopGeneration;
            outSnapshot->registerContext.rip = debugResult.registerContext.rip;
            outSnapshot->registerContext.rflags = debugResult.registerContext.rflags;
            outSnapshot->registerContext.rsp = debugResult.registerContext.rsp;
            outSnapshot->registerContext.rbp = debugResult.registerContext.rbp;
            outSnapshot->registerContext.rax = debugResult.registerContext.rax;
            outSnapshot->registerContext.rbx = debugResult.registerContext.rbx;
            outSnapshot->registerContext.rcx = debugResult.registerContext.rcx;
            outSnapshot->registerContext.rdx = debugResult.registerContext.rdx;
            outSnapshot->registerContext.rsi = debugResult.registerContext.rsi;
            outSnapshot->registerContext.rdi = debugResult.registerContext.rdi;
            outSnapshot->registerContext.r8 = debugResult.registerContext.r8;
            outSnapshot->registerContext.r9 = debugResult.registerContext.r9;
            outSnapshot->registerContext.r10 = debugResult.registerContext.r10;
            outSnapshot->registerContext.r11 = debugResult.registerContext.r11;
            outSnapshot->registerContext.r12 = debugResult.registerContext.r12;
            outSnapshot->registerContext.r13 = debugResult.registerContext.r13;
            outSnapshot->registerContext.r14 = debugResult.registerContext.r14;
            outSnapshot->registerContext.r15 = debugResult.registerContext.r15;
            copyText(outSnapshot->errorMessage, sizeof(outSnapshot->errorMessage), "Breakpoint trap observed");
        } else if (debugResult.status == 3 && debugResult.trapKind == 2) {
            outSnapshot->state = DebugSessionState::Running;
            outSnapshot->stopReason = DebugStopReason::None;
            outSnapshot->executionState = DebugBackendExecutionState::Running;
            copyText(outSnapshot->errorMessage, sizeof(outSnapshot->errorMessage), "Internal single-step complete; breakpoint rebound");
        } else if (debugResult.status == 6) {
            outSnapshot->state = DebugSessionState::Paused;
            outSnapshot->stopReason = DebugStopReason::Breakpoint;
            outSnapshot->executionState = DebugBackendExecutionState::SingleStepPending;
            copyText(outSnapshot->errorMessage, sizeof(outSnapshot->errorMessage), "Breakpoint continuation pending internal single-step");
        }
    }
    return true;
}

static bool bindSoftwareBreakpoint(void* userData, const DebugTarget&, uint64_t sessionGeneration,
                                   uint64_t processId, uint64_t nativeRuntimeId, const DebugBreakpoint& breakpoint,
                                   DebugBackendBinding* outBinding) {
    HostedDebugBackend* backend = static_cast<HostedDebugBackend*>(userData);
    if (!backend || !outBinding || !backend->runService.debugCommand || !breakpoint.location.instructionAddress.valid) return false;
    *outBinding = DebugBackendBinding();
    HostedDebugResult result = {};
    if (!backend->runService.debugCommand(backend->runService.userData, HostedDebugCommand::BindSoftwareBreakpoint,
                                          backend->runController.result.handle, sessionGeneration, processId,
                                          nativeRuntimeId, breakpoint.id, breakpoint.location.instructionAddress.value,
                                          backend->runController.request.artifactSha256, 0, 0, false, &result)) {
        copyText(outBinding->message, sizeof(outBinding->message), result.errorMessage[0] ? result.errorMessage : "software breakpoint bind failed");
        return true;
    }
    outBinding->accepted = result.status == 2 && result.bindingInstalled;
    outBinding->bindingId = result.bindingId;
    outBinding->originalByte = result.originalByte;
    outBinding->installedByte = result.installedByte;
    outBinding->originalByteValid = result.originalByteValid;
    copyText(outBinding->message, sizeof(outBinding->message), outBinding->accepted ? "Bound / Verified" : (result.errorMessage[0] ? result.errorMessage : "software breakpoint rejected"));
    return true;
}

static bool debugCommandWithOptions(void* userData, HostedDebugCommand command, uint64_t handle,
                         uint64_t sessionGeneration, uint64_t processId, uint64_t nativeRuntimeId,
                         uint64_t breakpointId, uint64_t targetAddress, const char* artifactSha256,
                         uint64_t threadId, uint64_t stopGeneration, bool reinstallBreakpoint,
                         HostedDebugResult* outResult) {
    HostedDebugBackend* backend = static_cast<HostedDebugBackend*>(userData);
    return backend && backend->runService.debugCommand && backend->runService.debugCommand(
        backend->runService.userData, command, handle, sessionGeneration, processId, nativeRuntimeId,
        breakpointId, targetAddress, artifactSha256, threadId, stopGeneration, reinstallBreakpoint, outResult);
}

static bool debugCommand(void* userData, HostedDebugCommand command, uint64_t handle,
                         uint64_t sessionGeneration, uint64_t processId, uint64_t nativeRuntimeId,
                         uint64_t breakpointId, uint64_t targetAddress, const char* artifactSha256,
                         HostedDebugResult* outResult) {
    HostedDebugBackend* backend = static_cast<HostedDebugBackend*>(userData);
    return backend && backend->runService.debugCommand && backend->runService.debugCommand(
        backend->runService.userData, command, handle, sessionGeneration, processId, nativeRuntimeId,
        breakpointId, targetAddress, artifactSha256, 0, 0, false, outResult);
}

static bool continueExecution(void* userData, uint64_t sessionGeneration,
                              const DebugRegisterContext& context, uint64_t breakpointId,
                              uint64_t bindingId, uint64_t targetAddress, bool reinstallBreakpoint) {
    (void)bindingId;
    HostedDebugBackend* backend = static_cast<HostedDebugBackend*>(userData);
    if (!backend || !backend->runService.debugCommand || !context.valid) return false;
    HostedDebugResult result = {};
    if (!backend->runService.debugCommand(backend->runService.userData, HostedDebugCommand::ContinueBreakpoint,
                                          backend->runController.result.handle, sessionGeneration,
                                          context.processId, context.nativeRuntimeId, breakpointId, targetAddress,
                                          backend->runController.request.artifactSha256, context.threadId,
                                          context.stopGeneration, reinstallBreakpoint, &result)) return false;
    return result.status == 6;
}

static bool stop(void* userData, uint64_t generation) {
    (void)generation;
    HostedDebugBackend* backend = static_cast<HostedDebugBackend*>(userData);
    return backend && RunControllerRequestClose(&backend->runController, backend->runService);
}

} // namespace

void HostedDebugBackendInit(HostedDebugBackend* backend,
                            const HostedDevelopmentRunService& runService) {
    if (!backend) return;
    *backend = HostedDebugBackend();
    backend->runService = runService;
    RunControllerInit(&backend->runController);
}

DebugBackend HostedDebugBackendCreate(HostedDebugBackend* backend) {
    DebugBackend result = {};
    result.userData = backend;
    result.capabilities.canLaunch = true;
    result.capabilities.canStop = true;
    result.capabilities.canPause = false;
    result.capabilities.canContinue = true;
    result.capabilities.canSetInstructionBreakpoint = true;
    result.capabilities.canSetSourceBreakpoint = true;
    result.capabilities.canStepInto = false;
    result.capabilities.canStepOver = false;
    result.capabilities.canStepOut = false;
    result.capabilities.canReadRegisters = true;
    result.capabilities.canReadMemory = false;
    result.capabilities.canWriteMemory = false;
    result.capabilities.canEnumerateThreads = false;
    result.capabilities.canReadCallStack = false;
    result.capabilities.canResolveSourceLocations = false;
    result.capabilities.canEvaluateExpressions = false;
    result.capabilities.canBindSoftwareBreakpoint = true;
    result.capabilities.canObserveBreakpointTrap = true;
    result.capabilities.canRestoreBreakpoint = true;
    result.capabilities.canReadInstructionPointer = true;
    copyText(result.name, sizeof(result.name), "Hosted Native ELF");
    result.launch = launch;
    result.poll = poll;
    result.stop = stop;
    result.pause = nullptr;
    result.continueExecution = continueExecution;
    result.bindSoftwareBreakpoint = bindSoftwareBreakpoint;
    result.debugCommand = debugCommand;
    return result;
}

} // namespace developer_studio
} // namespace guidexos
