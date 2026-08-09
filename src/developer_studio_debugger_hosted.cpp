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
    snapshot->exitCode = backend.runController.result.exitCode;
    snapshot->cleanupComplete = backend.runController.result.cleanupComplete;
    snapshot->stopReason = snapshot->state == DebugSessionState::Exited ? DebugStopReason::Exited :
        (backend.runController.closeRequested ? DebugStopReason::UserRequested : DebugStopReason::None);
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
    if (!RunControllerPoll(&backend->runController, backend->runService)) return false;
    snapshotFromRun(*backend, generation, outSnapshot);
    return true;
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
    result.capabilities.canContinue = false;
    result.capabilities.canSetInstructionBreakpoint = false;
    result.capabilities.canSetSourceBreakpoint = false;
    result.capabilities.canStepInto = false;
    result.capabilities.canStepOver = false;
    result.capabilities.canStepOut = false;
    result.capabilities.canReadRegisters = false;
    result.capabilities.canReadMemory = false;
    result.capabilities.canWriteMemory = false;
    result.capabilities.canEnumerateThreads = false;
    result.capabilities.canReadCallStack = false;
    result.capabilities.canResolveSourceLocations = false;
    result.capabilities.canEvaluateExpressions = false;
    copyText(result.name, sizeof(result.name), "Hosted Native ELF");
    result.launch = launch;
    result.poll = poll;
    result.stop = stop;
    result.pause = nullptr;
    result.continueExecution = nullptr;
    return result;
}

} // namespace developer_studio
} // namespace guidexos
