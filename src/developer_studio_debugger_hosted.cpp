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

static bool textEquals(const char* left, const char* right) {
    if (!left || !right) return left == right;
    uint32_t index = 0;
    while (left[index] != '\0' && right[index] != '\0') {
        if (left[index] != right[index]) return false;
        ++index;
    }
    return left[index] == right[index];
}

static RunRequest makeRunRequest(const DebugTarget& target) {
    RunRequest request = {};
    copyText(request.projectRoot, sizeof(request.projectRoot), target.projectRoot);
    copyText(request.projectId, sizeof(request.projectId), target.projectId);
    copyText(request.projectKind, sizeof(request.projectKind), "native-gui-application");
    copyText(request.targetProfile, sizeof(request.targetProfile), target.targetProfile);
    copyText(request.manifestPath, sizeof(request.manifestPath), target.manifestPath);
    copyText(request.artifactPath, sizeof(request.artifactPath), target.executablePath);
    request.artifactSize = target.artifactSize;
    copyText(request.artifactSha256, sizeof(request.artifactSha256), target.artifactSha256);
    copyText(request.artifactArchitecture, sizeof(request.artifactArchitecture), target.architecture);
    copyText(request.artifactAbi, sizeof(request.artifactAbi), target.abi);
    request.debugControlled = true;
    return request;
}

static void snapshotFromRun(const HostedDebugBackend& backend, uint64_t generation,
                            DebugBackendSnapshot* snapshot) {
    if (!snapshot) return;
    *snapshot = DebugBackendSnapshot();
    snapshot->sessionGeneration = generation;
    switch (backend.runController.state) {
    case RunState::Exited:
    case RunState::Completed:
    case RunState::Cancelled:
        snapshot->state = DebugSessionState::Exited;
        break;
    case RunState::Failed:
        snapshot->state = DebugSessionState::Failed;
        break;
    case RunState::Running:
        snapshot->state = DebugSessionState::Running;
        break;
    case RunState::Closing:
    case RunState::CleaningUp:
        snapshot->state = DebugSessionState::Stopping;
        break;
    case RunState::Paused:
    case RunState::Stepping:
    default:
        // The run service owns paused/stepping details. The debugger poll
        // supplies the authenticated stop context after launch while the
        // controller remains active during ABI setup.
        snapshot->state = DebugSessionState::Launching;
        break;
    }
    if (backend.runController.closeRequested && snapshot->state != DebugSessionState::Exited &&
        snapshot->state != DebugSessionState::Failed) snapshot->state = DebugSessionState::Stopping;
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

static void applyRegisterSnapshot(const HostedDebugResult& result,
                                  DebugBackendSnapshot* snapshot) {
    if (!snapshot) return;
    snapshot->registerContext.valid = result.registerContext.valid;
    snapshot->registerContext.architecture = static_cast<DebugArchitecture>(result.registerContext.architecture);
    snapshot->registerContext.processId = result.registerContext.processId;
    snapshot->registerContext.nativeRuntimeId = result.registerContext.nativeRuntimeId;
    snapshot->registerContext.threadId = result.registerContext.threadId;
    snapshot->registerContext.sessionGeneration = result.registerContext.sessionGeneration;
    snapshot->registerContext.stopGeneration = result.registerContext.stopGeneration;
    snapshot->registerContext.rip = result.registerContext.rip;
    snapshot->registerContext.rflags = result.registerContext.rflags;
    snapshot->registerContext.rsp = result.registerContext.rsp;
    snapshot->registerContext.rbp = result.registerContext.rbp;
    snapshot->registerContext.rax = result.registerContext.rax;
    snapshot->registerContext.rbx = result.registerContext.rbx;
    snapshot->registerContext.rcx = result.registerContext.rcx;
    snapshot->registerContext.rdx = result.registerContext.rdx;
    snapshot->registerContext.rsi = result.registerContext.rsi;
    snapshot->registerContext.rdi = result.registerContext.rdi;
    snapshot->registerContext.r8 = result.registerContext.r8;
    snapshot->registerContext.r9 = result.registerContext.r9;
    snapshot->registerContext.r10 = result.registerContext.r10;
    snapshot->registerContext.r11 = result.registerContext.r11;
    snapshot->registerContext.r12 = result.registerContext.r12;
    snapshot->registerContext.r13 = result.registerContext.r13;
    snapshot->registerContext.r14 = result.registerContext.r14;
    snapshot->registerContext.r15 = result.registerContext.r15;
    snapshot->registerContext.stackLow = result.stackLow;
    snapshot->registerContext.stackHigh = result.stackHigh;
}

static bool launch(void* userData, const DebugTarget& target, uint64_t generation, DebugBackendSnapshot* outSnapshot) {
    HostedDebugBackend* backend = static_cast<HostedDebugBackend*>(userData);
    if (!backend || !outSnapshot) return false;
    RunControllerInit(&backend->runController);
    backend->userPauseStopPending = false;
    backend->userStepStopPending = false;
    backend->internalTrapStopPending = false;
    backend->lastSnapshot = DebugBackendSnapshot();
    backend->lastSnapshotValid = false;
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
    if (!backend || !outSnapshot) return false;
    if (!RunControllerIsActive(&backend->runController)) {
        // The hosted Server can publish target cleanup and unregister its
        // debugger runtime one poll before the deployment publishes its final
        // terminal state. Preserve that durable terminal snapshot so the
        // controller can complete its shutdown stages instead of reporting a
        // transient "target runtime is not registered" backend failure.
        snapshotFromRun(*backend, generation, outSnapshot);
        return outSnapshot->state == DebugSessionState::Exited ||
            outSnapshot->state == DebugSessionState::Failed;
    }
    const uint64_t handle = backend->runController.result.handle;
    if (!RunControllerPoll(&backend->runController, backend->runService)) return false;
    snapshotFromRun(*backend, generation, outSnapshot);
    // The run deployment owns publication of the authenticated process/runtime
    // identity. During the short Launching boundary the target may already be
    // registered but the snapshot can still carry zero identity fields. Do
    // not send a debugger command with an incomplete identity and turn that
    // expected publication delay into a failed session; the next owner poll
    // will retry after the run service publishes both IDs.
    // Hosted deployments publish both process and NativeElf runtime identity
    // through the deployment snapshot before debugger commands are safe.  A
    // bare-metal NativeElf deployment deliberately leaves processId empty and
    // publishes its generation through the debugger snapshot instead; gating
    // that backend on hosted identity prevents the first entry-breakpoint poll
    // from ever reaching the debugger.
    const bool runtimeIdentityReady = outSnapshot->processId != 0 && outSnapshot->nativeRuntimeId != 0;
    const bool bareMetalIdentityIsDebuggerOwned = backend->runService.backend == RunBackendKind::BareMetal;
    if (backend->runService.debugCommand && handle != 0 &&
        (runtimeIdentityReady || bareMetalIdentityIsDebuggerOwned) &&
        (outSnapshot->state == DebugSessionState::Running || outSnapshot->state == DebugSessionState::Launching)) {
        HostedDebugResult debugResult = {};
        if (!backend->runService.debugCommand(backend->runService.userData, HostedDebugCommand::Poll, handle,
                                              generation, outSnapshot->processId, outSnapshot->nativeRuntimeId,
                                              0, 0, backend->runController.request.artifactSha256, 0, 0, false, 0, 0, &debugResult)) {
            if (backend->runController.closeRequested) {
                // Close/cancel owns the remainder of this lifecycle. The
                // deployment poll remains authoritative while the native
                // runtime is being torn down, so do not convert a transient
                // debugger-unregistration race into a terminal controller
                // failure.
                return true;
            }
            copyText(outSnapshot->errorMessage, sizeof(outSnapshot->errorMessage), debugResult.errorMessage[0] ? debugResult.errorMessage : "Hosted debugger trap poll failed");
            return false;
        }
        if (textEquals(debugResult.errorMessage, "NativeElf target exited")) {
            // Debug polling has observed the target return.  The second run
            // poll is lifecycle acknowledgement only: it consumes the durable
            // EXITED metadata and performs the owner-side release.  It does
            // not dispatch NativeElf execution again.
            if (!RunControllerPoll(&backend->runController, backend->runService)) return false;
            snapshotFromRun(*backend, generation, outSnapshot);
            return outSnapshot->state == DebugSessionState::Exited ||
                outSnapshot->state == DebugSessionState::Failed;
        }
        // Bare-metal run snapshots deliberately omit process/runtime identity;
        // the authenticated debugger snapshot is the authoritative source for
        // the target identity used by the controller and breakpoint ownership.
        if (debugResult.processId != 0 || debugResult.nativeRuntimeId != 0) {
            outSnapshot->processId = debugResult.processId;
            outSnapshot->nativeRuntimeId = debugResult.nativeRuntimeId;
        }
        if (debugResult.threadId != 0) outSnapshot->threadId = debugResult.threadId;
        outSnapshot->debugHandle = handle;
        outSnapshot->stackLow = debugResult.stackLow;
        outSnapshot->stackHigh = debugResult.stackHigh;
        if (debugResult.status == 3 &&
            debugResult.trapKind == 0 && debugResult.pauseReason == 8) {
            outSnapshot->state = DebugSessionState::Paused;
            outSnapshot->stopReason = DebugStopReason::UserPause;
            outSnapshot->breakpointTrap = false;
            outSnapshot->singleStepTrap = false;
            outSnapshot->threadId = debugResult.threadId;
            outSnapshot->instructionPointer = debugResult.instructionPointer;
            outSnapshot->targetAddress.valid = debugResult.targetAddress != 0;
            outSnapshot->targetAddress.value = debugResult.targetAddress;
            outSnapshot->stopGeneration = debugResult.stopGeneration;
            outSnapshot->executionState = DebugBackendExecutionState::PausedAtUserPause;
            applyRegisterSnapshot(debugResult, outSnapshot);
            copyText(outSnapshot->errorMessage, sizeof(outSnapshot->errorMessage),
                     debugResult.errorMessage[0] ? debugResult.errorMessage :
                     "User Pause captured at the cooperative scheduler boundary");
            backend->userPauseStopPending = true;
        } else if (debugResult.status == 3 && debugResult.trapKind == 1) {
            outSnapshot->state = DebugSessionState::Paused;
            outSnapshot->stopReason = debugResult.internalBreakpointTrap ? DebugStopReason::Step : DebugStopReason::Breakpoint;
            outSnapshot->breakpointTrap = true;
            outSnapshot->threadId = debugResult.threadId;
            outSnapshot->instructionPointer = debugResult.instructionPointer;
            outSnapshot->targetAddress.valid = true;
            outSnapshot->targetAddress.value = debugResult.targetAddress;
            outSnapshot->breakpointBindingId = debugResult.bindingId;
            outSnapshot->bindingOwnerCount = debugResult.bindingCount;
            outSnapshot->bindingInstalled = debugResult.bindingInstalled;
            outSnapshot->originalByte = debugResult.originalByte;
            outSnapshot->installedByte = debugResult.installedByte;
            outSnapshot->originalByteValid = debugResult.originalByteValid;
            outSnapshot->stopGeneration = debugResult.stopGeneration;
            outSnapshot->executionState = debugResult.internalBreakpointTrap ?
                DebugBackendExecutionState::PausedAtStepOver : DebugBackendExecutionState::PausedAtBreakpoint;
            outSnapshot->internalBreakpointTrap = debugResult.internalBreakpointTrap;
            outSnapshot->internalBreakpointId = debugResult.internalBreakpointId;
            outSnapshot->internalBreakpointPurpose = debugResult.internalBreakpointPurpose;
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
            outSnapshot->registerContext.stackLow = debugResult.stackLow;
            outSnapshot->registerContext.stackHigh = debugResult.stackHigh;
            copyText(outSnapshot->errorMessage, sizeof(outSnapshot->errorMessage),
                      debugResult.internalBreakpointPurpose == static_cast<uint32_t>(HostedDebugInternalBreakpointPurpose::StepOut) ?
                          "Step out return breakpoint observed" :
                          (debugResult.internalBreakpointTrap ? "Step over return breakpoint observed" : "Breakpoint trap observed"));
            backend->internalTrapStopPending = debugResult.internalBreakpointTrap;
        } else if (debugResult.status == 3 && debugResult.trapKind == 2 &&
                   debugResult.singleStepKind == 2) {
            outSnapshot->state = DebugSessionState::Stepping;
            outSnapshot->stopReason = DebugStopReason::None;
            outSnapshot->singleStepTrap = true;
            outSnapshot->singleStepKind = 2;
            outSnapshot->threadId = debugResult.threadId;
            outSnapshot->instructionPointer = debugResult.instructionPointer;
            outSnapshot->targetAddress.valid = debugResult.targetAddress != 0;
            outSnapshot->targetAddress.value = debugResult.targetAddress;
            outSnapshot->breakpointBindingId = debugResult.bindingId;
            outSnapshot->executionState = DebugBackendExecutionState::UserSourceStepPending;
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
            outSnapshot->registerContext.stackLow = debugResult.stackLow;
            outSnapshot->registerContext.stackHigh = debugResult.stackHigh;
            copyText(outSnapshot->errorMessage, sizeof(outSnapshot->errorMessage), "User source-step EXCEPTION_SINGLE_STEP observed");
            backend->userStepStopPending = true;
            backend->userStepStopSnapshot = *outSnapshot;
        } else if (debugResult.status == 3 && debugResult.trapKind == 2) {
            outSnapshot->state = DebugSessionState::Running;
            outSnapshot->stopReason = DebugStopReason::None;
            outSnapshot->executionState = DebugBackendExecutionState::Running;
            copyText(outSnapshot->errorMessage, sizeof(outSnapshot->errorMessage), "Internal single-step complete; breakpoint rebound");
        } else if (debugResult.status == 6 && debugResult.singleStepKind == 2 && backend->userStepStopPending) {
            *outSnapshot = backend->userStepStopSnapshot;
            outSnapshot->sessionGeneration = generation;
            outSnapshot->state = DebugSessionState::Paused;
            outSnapshot->stopReason = DebugStopReason::Step;
            outSnapshot->executionState = DebugBackendExecutionState::PausedAtSourceStep;
            outSnapshot->singleStepTrap = false;
            copyText(outSnapshot->errorMessage, sizeof(outSnapshot->errorMessage), "User source-step stop is paused");
        } else if (debugResult.status == 6 && debugResult.singleStepKind == 2) {
            outSnapshot->state = DebugSessionState::Stepping;
            outSnapshot->stopReason = DebugStopReason::None;
            outSnapshot->executionState = DebugBackendExecutionState::UserSourceStepPending;
            outSnapshot->singleStepKind = 2;
            outSnapshot->threadId = debugResult.threadId;
            outSnapshot->targetAddress.valid = debugResult.targetAddress != 0;
            outSnapshot->targetAddress.value = debugResult.targetAddress;
            outSnapshot->breakpointBindingId = debugResult.bindingId;
            copyText(outSnapshot->errorMessage, sizeof(outSnapshot->errorMessage), "User source-step instruction pending");
        } else if (debugResult.status == 6) {
            outSnapshot->state = DebugSessionState::Paused;
            outSnapshot->stopReason = DebugStopReason::Breakpoint;
            outSnapshot->executionState = DebugBackendExecutionState::SingleStepPending;
            copyText(outSnapshot->errorMessage, sizeof(outSnapshot->errorMessage), "Breakpoint continuation pending internal single-step");
        }
    }
    backend->lastSnapshot = *outSnapshot;
    backend->lastSnapshotValid = true;
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
                                          backend->runController.request.artifactSha256, 0, 0, false, 0, 0, &result)) {
        copyText(outBinding->message, sizeof(outBinding->message), result.errorMessage[0] ? result.errorMessage : "software breakpoint bind failed");
        return true;
    }
    outBinding->accepted = result.status == 2 && result.bindingInstalled;
    outBinding->bindingId = result.bindingId;
    outBinding->ownerCount = result.bindingCount;
    outBinding->originalByte = result.originalByte;
    outBinding->installedByte = result.installedByte;
    outBinding->originalByteValid = result.originalByteValid;
    copyText(outBinding->message, sizeof(outBinding->message), outBinding->accepted ? "Bound / Verified" : (result.errorMessage[0] ? result.errorMessage : "software breakpoint rejected"));
    return true;
}

static bool debugCommand(void* userData, HostedDebugCommand command, uint64_t handle,
                         uint64_t sessionGeneration, uint64_t processId, uint64_t nativeRuntimeId,
                         uint64_t breakpointId, uint64_t targetAddress, const char* artifactSha256,
                         uint64_t threadId, uint64_t stopGeneration, bool reinstallBreakpoint,
                         uint64_t auxiliaryAddress, uint32_t readByteCount,
                         HostedDebugResult* outResult) {
    HostedDebugBackend* backend = static_cast<HostedDebugBackend*>(userData);
    return backend && backend->runService.debugCommand && backend->runService.debugCommand(
        backend->runService.userData, command, handle, sessionGeneration, processId, nativeRuntimeId,
        breakpointId, targetAddress, artifactSha256, threadId, stopGeneration, reinstallBreakpoint,
        auxiliaryAddress, readByteCount, outResult);
}

static bool pause(void* userData, uint64_t sessionGeneration) {
    HostedDebugBackend* backend = static_cast<HostedDebugBackend*>(userData);
    if (!backend || !backend->runService.debugCommand || backend->runController.result.handle == 0)
        return false;
    HostedDebugResult result = {};
    if (!backend->runService.debugCommand(backend->runService.userData, HostedDebugCommand::Pause,
                                          backend->runController.result.handle, sessionGeneration,
                                          0, backend->runController.result.nativeRuntimeId,
                                          0, 0, backend->runController.request.artifactSha256,
                                          0, 0, false, 0, 0, &result)) return false;
    return result.status == 7;
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
                                          context.stopGeneration, reinstallBreakpoint, 0, 0, &result)) return false;
    return result.status == 6;
}

static bool stepInstruction(void* userData, uint64_t sessionGeneration,
                            const DebugRegisterContext& context, uint64_t breakpointId,
                            uint64_t bindingId, uint64_t targetAddress, bool reinstallBreakpoint) {
    (void)bindingId;
    HostedDebugBackend* backend = static_cast<HostedDebugBackend*>(userData);
    if (!backend || !backend->runService.debugCommand || !context.valid) return false;
    HostedDebugResult result = {};
    const HostedDebugCommand command = backend->internalTrapStopPending ?
        HostedDebugCommand::StepInternalTrap : HostedDebugCommand::StepInstruction;
    if (!backend->runService.debugCommand(backend->runService.userData, command,
                                          backend->runController.result.handle, sessionGeneration,
                                          context.processId, context.nativeRuntimeId,
                                          backend->internalTrapStopPending ? 0 : breakpointId,
                                          backend->internalTrapStopPending ? 0 : targetAddress,
                                          backend->runController.request.artifactSha256, context.threadId,
                                          context.stopGeneration, backend->internalTrapStopPending ? false : reinstallBreakpoint,
                                          0, 0, &result)) return false;
    if (result.status != 6 || result.singleStepKind != 2) return false;
    backend->userStepStopPending = false;
    backend->internalTrapStopPending = false;
    return true;
}

static bool resumeExecution(void* userData, uint64_t sessionGeneration,
                            const DebugRegisterContext& context) {
    HostedDebugBackend* backend = static_cast<HostedDebugBackend*>(userData);
    if (!backend || !backend->runService.debugCommand || !context.valid) return false;
    HostedDebugResult result = {};
    const HostedDebugCommand command = backend->userPauseStopPending ?
        HostedDebugCommand::Resume : (backend->internalTrapStopPending ?
        HostedDebugCommand::ResumeInternalTrap : HostedDebugCommand::ResumeStep);
    const uint64_t targetAddress = backend->userPauseStopPending ? 0 : context.rip;
    if (!backend->runService.debugCommand(backend->runService.userData, command,
                                          backend->runController.result.handle, sessionGeneration,
                                          context.processId, context.nativeRuntimeId, 0, 0,
                                          backend->runController.request.artifactSha256, context.threadId,
                                          context.stopGeneration, false, targetAddress, 0, &result)) return false;
    if (result.status != 1 && result.status != 3 && result.status != 6) return false;
    backend->userPauseStopPending = false;
    backend->userStepStopPending = false;
    backend->internalTrapStopPending = false;
    return true;
}

static bool readMemory(void* userData, uint64_t sessionGeneration, uint64_t processId,
                       uint64_t nativeRuntimeId, uint64_t address, uint8_t* bytes,
                       uint32_t requested, uint32_t* returned) {
    HostedDebugBackend* backend = static_cast<HostedDebugBackend*>(userData);
    if (returned) *returned = 0;
    if (!backend || !backend->runService.debugCommand || !bytes || requested == 0 || requested > kDebugMaxInstructionBytes)
        return false;
    HostedDebugResult result = {};
    if (!backend->runService.debugCommand(backend->runService.userData, HostedDebugCommand::ReadMemory,
                                          backend->runController.result.handle, sessionGeneration, processId,
                                          nativeRuntimeId, 0, address, backend->runController.request.artifactSha256,
                                          0, 0, false, 0, requested, &result) || result.status != 1 ||
        result.byteCount == 0 || result.byteCount > requested) return false;
    for (uint32_t i = 0; i < result.byteCount; ++i) bytes[i] = result.bytes[i];
    if (returned) *returned = result.byteCount;
    return true;
}

static bool readTargetMemory(void* userData, uint64_t sessionGeneration, uint64_t processId,
                             uint64_t nativeRuntimeId, uint64_t threadId, uint64_t stopGeneration,
                             uint64_t address, uint8_t* bytes, uint32_t requested,
                             uint32_t* returned) {
    HostedDebugBackend* backend = static_cast<HostedDebugBackend*>(userData);
    if (returned) *returned = 0;
    if (!backend || !backend->runService.debugCommand || !bytes || requested == 0 ||
        requested > kDebugMaxInstructionBytes || threadId == 0 || stopGeneration == 0) return false;
    HostedDebugResult result = {};
    if (!backend->runService.debugCommand(backend->runService.userData, HostedDebugCommand::ReadMemory,
                                          backend->runController.result.handle, sessionGeneration,
                                          processId, nativeRuntimeId, 0, address,
                                          backend->runController.request.artifactSha256, threadId,
                                          stopGeneration, false, 0, requested, &result) ||
        result.status != 1 || result.byteCount != requested) return false;
    for (uint32_t i = 0; i < result.byteCount; ++i) bytes[i] = result.bytes[i];
    if (returned) *returned = result.byteCount;
    return true;
}

static bool stepOverCall(void* userData, uint64_t sessionGeneration,
                         const DebugRegisterContext& context, uint64_t callAddress,
                         uint64_t returnAddress, uint64_t temporaryBreakpointId) {
    HostedDebugBackend* backend = static_cast<HostedDebugBackend*>(userData);
    if (!backend || !backend->runService.debugCommand || !context.valid) return false;
    HostedDebugResult result = {};
    if (!backend->runService.debugCommand(backend->runService.userData, HostedDebugCommand::StepOverCall,
                                          backend->runController.result.handle, sessionGeneration,
                                          context.processId, context.nativeRuntimeId, temporaryBreakpointId,
                                          callAddress, backend->runController.request.artifactSha256,
                                          context.threadId, context.stopGeneration, false, returnAddress, 0, &result)) return false;
    const bool accepted = result.status == 1 ||
        (result.status == 3 && result.trapKind == 1 && result.internalBreakpointTrap);
    if (!accepted) return false;
    backend->userStepStopPending = false;
    backend->internalTrapStopPending = false;
    return true;
}

static bool stepOutReturn(void* userData, uint64_t sessionGeneration,
                          const DebugRegisterContext& context, uint64_t breakpointId,
                          uint64_t bindingId, uint64_t targetAddress, bool reinstallBreakpoint,
                          uint64_t returnAddress, uint64_t temporaryBreakpointId) {
    (void)breakpointId;
    (void)bindingId;
    HostedDebugBackend* backend = static_cast<HostedDebugBackend*>(userData);
    if (!backend || !backend->runService.debugCommand || !context.valid ||
        targetAddress == 0 || returnAddress == 0 || temporaryBreakpointId == 0) return false;
    HostedDebugResult result = {};
    if (!backend->runService.debugCommand(backend->runService.userData, HostedDebugCommand::StepOutReturn,
                                          backend->runController.result.handle, sessionGeneration,
                                          context.processId, context.nativeRuntimeId, temporaryBreakpointId,
                                          targetAddress, backend->runController.request.artifactSha256,
                                          context.threadId, context.stopGeneration, reinstallBreakpoint,
                                          returnAddress, 0, &result)) return false;
    const bool accepted = result.status == 1 ||
        (result.status == 3 && result.trapKind == 1 && result.internalBreakpointTrap);
    if (!accepted) return false;
    backend->userStepStopPending = false;
    backend->internalTrapStopPending = false;
    return true;
}

static bool stop(void* userData, uint64_t generation) {
    HostedDebugBackend* backend = static_cast<HostedDebugBackend*>(userData);
    if (!backend) return false;
    backend->lastStopRoute = 1; // paused cancellation eligible only with a current poll snapshot
    backend->lastStopStatus = 0;
    backend->lastStopFallbackFailed = false;
    if (!backend->lastSnapshotValid) backend->lastStopRoute = 2;
    else if (backend->lastSnapshot.sessionGeneration != generation) backend->lastStopRoute = 3;
    else if (backend->lastSnapshot.state != DebugSessionState::Paused &&
             backend->lastSnapshot.state != DebugSessionState::Stepping) backend->lastStopRoute = 4;
    if (backend->lastSnapshotValid && backend->lastSnapshot.sessionGeneration == generation &&
        (backend->lastSnapshot.state == DebugSessionState::Paused ||
         backend->lastSnapshot.state == DebugSessionState::Stepping)) {
        if (!backend->runService.debugCommand) { backend->lastStopRoute = 5; return false; }
        if (backend->runController.result.handle == 0) { backend->lastStopRoute = 6; return false; }
        const DebugBackendSnapshot& stopped = backend->lastSnapshot;
        const DebugRegisterContext& context = stopped.registerContext;
        const uint64_t processId = context.processId != 0 ? context.processId :
            (stopped.processId != 0 ? stopped.processId :
             (backend->runController.result.processId != 0 ? backend->runController.result.processId :
              backend->stopProcessId));
        const uint64_t runtimeId = context.nativeRuntimeId != 0 ? context.nativeRuntimeId :
            (stopped.nativeRuntimeId != 0 ? stopped.nativeRuntimeId :
             (backend->runController.result.nativeRuntimeId != 0 ? backend->runController.result.nativeRuntimeId :
              backend->stopRuntimeId));
        const uint64_t threadId = context.threadId != 0 ? context.threadId : stopped.threadId;
        const uint64_t stopGeneration = context.stopGeneration != 0 ? context.stopGeneration : stopped.stopGeneration;
        // Bare-metal NativeElf uses processId == 0 by ABI; its runtime
        // registration generation is the authenticated identity instead.
        if (runtimeId == 0) { backend->lastStopRoute = 7; return false; }
        HostedDebugResult result = {};
        const bool cancelled = backend->runService.debugCommand(
                backend->runService.userData, HostedDebugCommand::CancelExecution,
                backend->runController.result.handle, generation, processId, runtimeId,
                0, 0, backend->runController.request.artifactSha256, threadId,
                stopGeneration, false, 0, 0, &result);
        backend->lastStopStatus = result.status;
        if (!cancelled) { backend->lastStopRoute = 8; return false; }
        if (result.status != 1u && result.status != 4u) { backend->lastStopRoute = 9; return false; }
        // CancelExecution releases a paused NativeElf trap, but it does not
        // itself publish the deployment-owned close request.  Keep the two
        // ownership transitions ordered: first release the execution gate,
        // then ask the run service to enqueue the terminal close event.  A
        // local flag alone lets the target resume without ever receiving the
        // close event, leaving the controller in Stopping forever.
        if (!RunControllerRequestClose(&backend->runController, backend->runService)) {
            backend->lastStopRoute = 12;
            backend->lastStopFallbackFailed = true;
            return false;
        }
        backend->lastSnapshot.state = DebugSessionState::Stopping;
        backend->lastSnapshot.stopReason = DebugStopReason::UserRequested;
        backend->lastStopRoute = 10;
        return true;
    }
    const bool closeRequested = RunControllerRequestClose(&backend->runController, backend->runService);
    backend->lastStopRoute = closeRequested ? 11 : backend->lastStopRoute;
    backend->lastStopFallbackFailed = !closeRequested;
    return closeRequested;
}

} // namespace

void HostedDebugBackendInit(HostedDebugBackend* backend,
                            const HostedDevelopmentRunService& runService) {
    if (!backend) return;
    *backend = HostedDebugBackend();
    backend->runService = runService;
    RunControllerInit(&backend->runController);
}

void HostedDebugBackendSetStopIdentity(HostedDebugBackend* backend,
                                       uint64_t processId, uint64_t nativeRuntimeId) {
    if (!backend) return;
    backend->stopProcessId = processId;
    backend->stopRuntimeId = nativeRuntimeId;
}

DebugBackend HostedDebugBackendCreate(HostedDebugBackend* backend) {
    DebugBackend result = {};
    result.userData = backend;
    result.capabilities.canLaunch = true;
    result.capabilities.canStop = true;
    result.capabilities.canPause = true;
    result.capabilities.canContinue = true;
    result.capabilities.canSetInstructionBreakpoint = true;
    result.capabilities.canSetSourceBreakpoint = true;
    result.capabilities.canStepInto = true;
    result.capabilities.canStepOver = true;
    result.capabilities.canStepOut = true;
    result.capabilities.canReadRegisters = true;
    result.capabilities.canReadMemory = true;
    result.capabilities.canWriteMemory = false;
    result.capabilities.canEnumerateThreads = false;
    result.capabilities.canReadCallStack = true;
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
    result.pause = pause;
    result.continueExecution = continueExecution;
    result.stepInstruction = stepInstruction;
    result.resumeExecution = resumeExecution;
    result.bindSoftwareBreakpoint = bindSoftwareBreakpoint;
    result.debugCommand = debugCommand;
    result.readMemory = readMemory;
    result.readTargetMemory = readTargetMemory;
    result.stepOverCall = stepOverCall;
    result.stepOutReturn = stepOutReturn;
    return result;
}

} // namespace developer_studio
} // namespace guidexos
