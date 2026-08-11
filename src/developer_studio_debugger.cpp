#include "developer_studio_debugger.h"
#include "developer_studio_debug_symbols.h"

namespace guidexos {
namespace developer_studio {
namespace {

static uint32_t textLength(const char* value, uint32_t capacity) {
    if (!value) return 0;
    uint32_t length = 0;
    while (length < capacity && value[length] != '\0') ++length;
    return length;
}

static bool copyText(char* output, uint32_t outputSize, const char* input) {
    if (!output || outputSize == 0) return false;
    if (!input) { output[0] = '\0'; return false; }
    const uint32_t length = textLength(input, outputSize);
    const uint32_t copyLength = length < outputSize ? length : outputSize - 1;
    for (uint32_t i = 0; i < copyLength; ++i) output[i] = input[i];
    output[copyLength] = '\0';
    return length < outputSize;
}

static bool equalText(const char* left, const char* right, bool caseInsensitive) {
    if (!left || !right) return false;
    uint32_t index = 0;
    while (left[index] != '\0' || right[index] != '\0') {
        char a = left[index];
        char b = right[index];
        if (caseInsensitive) {
            if (a >= 'A' && a <= 'Z') a = static_cast<char>(a + ('a' - 'A'));
            if (b >= 'A' && b <= 'Z') b = static_cast<char>(b + ('a' - 'A'));
        }
        if (a != b) return false;
        ++index;
    }
    return true;
}

static bool isSlash(char value) { return value == '/' || value == static_cast<char>(92); }

static void clearSnapshot(DebugBackendSnapshot* snapshot) {
    if (!snapshot) return;
    *snapshot = DebugBackendSnapshot();
    snapshot->state = DebugSessionState::Idle;
    snapshot->stopReason = DebugStopReason::None;
    snapshot->executionState = DebugBackendExecutionState::None;
}

static bool validTransition(DebugSessionState from, DebugSessionState to) {
    if (from == to) return true;
    switch (from) {
    case DebugSessionState::Idle: return to == DebugSessionState::Launching;
    case DebugSessionState::Launching: return to == DebugSessionState::Running || to == DebugSessionState::Failed || to == DebugSessionState::Exited;
    case DebugSessionState::Running: return to == DebugSessionState::Paused || to == DebugSessionState::Stepping || to == DebugSessionState::Stopping || to == DebugSessionState::Exited || to == DebugSessionState::Failed;
    case DebugSessionState::Paused: return to == DebugSessionState::Running || to == DebugSessionState::Stepping || to == DebugSessionState::Stopping || to == DebugSessionState::Exited || to == DebugSessionState::Failed;
    case DebugSessionState::Stepping: return to == DebugSessionState::Paused || to == DebugSessionState::Running || to == DebugSessionState::Stopping || to == DebugSessionState::Exited || to == DebugSessionState::Failed;
    case DebugSessionState::Stopping: return to == DebugSessionState::Exited || to == DebugSessionState::Failed;
    case DebugSessionState::Exited: return to == DebugSessionState::Launching;
    case DebugSessionState::Failed: return to == DebugSessionState::Launching;
    }
    return false;
}

static void appendEvent(DebugController* controller, DebugEventKind kind, DebugSessionState state,
                        DebugStopReason reason, const char* message) {
    if (!controller) return;
    if (controller->eventCount == kDebugMaxEvents) {
        for (uint32_t i = 1; i < kDebugMaxEvents; ++i) controller->events[i - 1] = controller->events[i];
        controller->eventCount = kDebugMaxEvents - 1;
    }
    DebugEvent& event = controller->events[controller->eventCount++];
    event = DebugEvent();
    event.sequence = controller->nextEventSequence == 0 ? 1 : controller->nextEventSequence;
    controller->nextEventSequence = event.sequence == UINT64_MAX ? 1 : event.sequence + 1;
    event.sessionGeneration = controller->sessionGeneration;
    event.kind = kind;
    event.state = state;
    event.stopReason = reason;
    event.processId = controller->processId;
    event.nativeRuntimeId = controller->nativeRuntimeId;
    event.exitCode = controller->exitCode;
    event.breakpointId = controller->lastBreakpointId;
    event.targetAddress = controller->currentInstructionAddress.valid ? controller->currentInstructionAddress.value : 0;
    event.threadId = controller->currentThreadId;
    event.projectGeneration = controller->target.projectGeneration;
    event.location = controller->currentLocation;
    copyText(event.message, sizeof(event.message), message ? message : "");
}

static void setMessage(DebugController* controller, const char* message) {
    if (!controller) return;
    copyText(controller->lastMessage, sizeof(controller->lastMessage), message ? message : "");
}

static bool sameSourceStepLocation(const DebugSourceLocation& left, const DebugSourceLocation& right) {
    return left.relativePath[0] != '\0' && right.relativePath[0] != '\0' &&
        PathsEqual(left.relativePath, right.relativePath) && left.line == right.line;
}

static void clearSourceStep(DebugController* controller, DebugSourceStepStatus status,
                            const char* reason) {
    if (!controller) return;
    controller->sourceStep.active = false;
    controller->sourceStep.status = status;
    if (reason) copyText(controller->sourceStep.reason, sizeof(controller->sourceStep.reason), reason);
}

static void clearStoppedContext(DebugController* controller) {
    if (!controller) return;
    controller->currentInstructionAddress = DebugAddress();
    controller->currentLocation = DebugSourceLocation();
    controller->currentThreadId = 0;
    controller->reportedInstructionPointer = 0;
    controller->stopGeneration = 0;
    controller->stoppedContext = DebugRegisterContext();
}

static int findBreakpoint(const DebugController* controller, uint64_t breakpointId) {
    if (!controller || breakpointId == 0) return -1;
    for (uint32_t i = 0; i < controller->breakpointCount; ++i)
        if (controller->breakpoints[i].id == breakpointId) return static_cast<int>(i);
    return -1;
}

static bool validRelativeSourcePath(const char* projectRoot, const char* sourcePath,
                                    char* normalized, uint32_t normalizedSize) {
    if (!projectRoot || !sourcePath || !normalized || sourcePath[0] == '\0' ||
        isSlash(sourcePath[0]) || sourcePath[1] == ':' || PathContainsTraversal(sourcePath)) return false;
    if (!NormalizePath(sourcePath, normalized, normalizedSize) || normalized[0] == '\0' ||
        equalText(normalized, ".", false) || isSlash(normalized[0]) || normalized[1] == ':') return false;
    char absolute[kMaxPathBytes] = {};
    return JoinWorkspacePath(projectRoot, normalized, absolute, sizeof(absolute));
}

static void setBreakpointMessage(DebugBreakpoint& breakpoint, const char* message) {
    copyText(breakpoint.message, sizeof(breakpoint.message), message ? message : "");
}

static void appendText(char* output, uint32_t outputSize, const char* input) {
    if (!output || outputSize == 0 || !input) return;
    uint32_t offset = textLength(output, outputSize);
    for (uint32_t inputOffset = 0; offset + 1 < outputSize && input[inputOffset] != '\0'; ++inputOffset)
        output[offset++] = input[inputOffset];
    output[offset] = '\0';
}

static void appendHexAddress(char* output, uint32_t outputSize, uint64_t value) {
    static const char digits[] = "0123456789ABCDEF";
    if (!output || outputSize == 0) return;
    uint32_t offset = textLength(output, outputSize);
    const char prefix[] = "0x";
    for (uint32_t i = 0; i < 2 && offset + 1 < outputSize; ++i) output[offset++] = prefix[i];
    for (int32_t shift = 60; shift >= 0 && offset + 1 < outputSize; shift -= 4)
        output[offset++] = digits[(value >> shift) & 0xfu];
    output[offset] = '\0';
}

static void clearBreakpointMapping(DebugBreakpoint& breakpoint) {
    breakpoint.location.instructionAddress = DebugAddress();
    breakpoint.mappedAddressCount = 0;
    for (uint32_t i = 0; i < kDebugMaxMappedAddresses; ++i) breakpoint.mappedAddresses[i] = DebugAddress();
}

static DebugErrorCode mapDwarfError(DebugDwarfError error) {
    switch (error) {
    case DebugDwarfError::None: return DebugErrorCode::None;
    case DebugDwarfError::NoDebugInfo: return DebugErrorCode::NoDebugInfo;
    case DebugDwarfError::MissingLineSection: return DebugErrorCode::MissingLineSection;
    case DebugDwarfError::MalformedElf: return DebugErrorCode::MalformedElf;
    case DebugDwarfError::MalformedDwarf: return DebugErrorCode::MalformedDwarf;
    case DebugDwarfError::UnsupportedDwarfVersion: return DebugErrorCode::UnsupportedDwarfVersion;
    case DebugDwarfError::UnsupportedForm: return DebugErrorCode::UnsupportedForm;
    case DebugDwarfError::UnsupportedArchitecture: return DebugErrorCode::UnsupportedArchitecture;
    case DebugDwarfError::ArtifactChanged: return DebugErrorCode::ArtifactChanged;
    case DebugDwarfError::SourceNotFound: return DebugErrorCode::SourceNotFound;
    case DebugDwarfError::LineNotMapped: return DebugErrorCode::LineNotMapped;
    case DebugDwarfError::Truncated: return DebugErrorCode::Truncated;
    case DebugDwarfError::LimitExceeded: return DebugErrorCode::MappingLimitExceeded;
    case DebugDwarfError::UnsupportedOpcode: return DebugErrorCode::UnsupportedOpcode;
    }
    return DebugErrorCode::BackendError;
}

static void setMappingMessage(DebugBreakpoint& breakpoint, uint32_t addressCount, uint64_t primary, bool truncated) {
    char message[kDebugMaxMessageBytes] = {};
    copyText(message, sizeof(message), "Mapped -> ");
    appendHexAddress(message, sizeof(message), primary);
    if (addressCount > 1) {
        char count[24] = {};
        uint32_t value = addressCount;
        uint32_t countLength = 0;
        do { count[countLength++] = static_cast<char>('0' + (value % 10)); value /= 10; } while (value != 0 && countLength < sizeof(count));
        appendText(message, sizeof(message), " | ");
        uint32_t offset = textLength(message, sizeof(message));
        while (countLength > 0 && offset + 1 < sizeof(message)) message[offset++] = count[--countLength];
        message[offset] = '\0';
        appendText(message, sizeof(message), " addresses");
    }
    if (truncated) appendText(message, sizeof(message), " | truncated");
    setBreakpointMessage(breakpoint, message);
}

static void initializeBreakpoint(DebugBreakpoint* breakpoint, uint64_t id, const char* projectId,
                                 const char* relativePath, uint64_t projectGeneration,
                                 uint32_t line, uint32_t column, uint32_t sourceGeneration) {
    *breakpoint = DebugBreakpoint();
    breakpoint->id = id;
    copyText(breakpoint->projectId, sizeof(breakpoint->projectId), projectId);
    copyText(breakpoint->location.relativePath, sizeof(breakpoint->location.relativePath), relativePath);
    breakpoint->location.line = line;
    breakpoint->location.column = column;
    breakpoint->location.projectGeneration = projectGeneration;
    breakpoint->location.sourceGeneration = sourceGeneration;
    breakpoint->location.mapping = DebugMappingState::Unavailable;
    breakpoint->mappingError = DebugErrorCode::SourceMappingUnavailable;
    clearBreakpointMapping(*breakpoint);
    breakpoint->enabled = true;
    breakpoint->state = DebugBreakpointState::Pending;
    breakpoint->sessionGeneration = 0;
    setBreakpointMessage(*breakpoint, "Pending: source mapping unavailable");
}

static void applySnapshotUnchecked(DebugController* controller, const DebugBackendSnapshot& snapshot) {
    const DebugSessionState previous = controller->state;
    const bool continuingFromBreakpoint = previous == DebugSessionState::Paused &&
        snapshot.state == DebugSessionState::Running &&
        controller->stopReason == DebugStopReason::Breakpoint;
    const bool continuingFromSourceStep = previous == DebugSessionState::Paused &&
        snapshot.state == DebugSessionState::Running &&
        controller->stopReason == DebugStopReason::Step;
    controller->processId = snapshot.processId;
    controller->nativeRuntimeId = snapshot.nativeRuntimeId;
    controller->debugHandle = snapshot.debugHandle;
    controller->exitCode = snapshot.exitCode;
    if (!continuingFromBreakpoint) controller->stopReason = snapshot.stopReason;
    if (snapshot.executionState != DebugBackendExecutionState::None)
        controller->backendExecutionState = snapshot.executionState;
    else if (snapshot.state == DebugSessionState::Running)
        controller->backendExecutionState = DebugBackendExecutionState::Running;
    else if (snapshot.state == DebugSessionState::Paused && snapshot.stopReason == DebugStopReason::Breakpoint)
        controller->backendExecutionState = DebugBackendExecutionState::PausedAtBreakpoint;
    else if (snapshot.state == DebugSessionState::Paused && snapshot.stopReason == DebugStopReason::Step)
        controller->backendExecutionState = DebugBackendExecutionState::PausedAtSourceStep;
    if (snapshot.backendName[0]) copyText(controller->backendName, sizeof(controller->backendName), snapshot.backendName);
    if (snapshot.errorMessage[0]) setMessage(controller, snapshot.errorMessage);
    if (controller->processId != 0 && controller->nativeRuntimeId != 0) {
        bool targetPublished = false;
        for (uint32_t i = 0; i < controller->eventCount; ++i) {
            const DebugEvent& event = controller->events[i];
            if (event.sessionGeneration == controller->sessionGeneration && event.kind == DebugEventKind::TargetCreated &&
                event.processId == controller->processId && event.nativeRuntimeId == controller->nativeRuntimeId) {
                targetPublished = true;
                break;
            }
        }
        if (!targetPublished)
            appendEvent(controller, DebugEventKind::TargetCreated, snapshot.state, DebugStopReason::None,
                        "Target created; execution gate remains closed until bindings are verified");
    }
    if (snapshot.state != previous && !validTransition(previous, snapshot.state)) {
        controller->error = DebugErrorCode::InvalidTransition;
        setMessage(controller, "Invalid debugger session transition");
        return;
    }
    controller->state = snapshot.state;
    controller->active = snapshot.state == DebugSessionState::Launching ||
        snapshot.state == DebugSessionState::Running || snapshot.state == DebugSessionState::Paused ||
        snapshot.state == DebugSessionState::Stopping || snapshot.state == DebugSessionState::Stepping;
    if (snapshot.state == DebugSessionState::Running && previous != DebugSessionState::Running)
        appendEvent(controller, DebugEventKind::Running, snapshot.state, DebugStopReason::None, "Hosted Native ELF target running");
    else if (snapshot.state == DebugSessionState::Paused && previous != DebugSessionState::Paused)
        appendEvent(controller, DebugEventKind::Paused, snapshot.state, snapshot.stopReason,
                    snapshot.errorMessage[0] ? snapshot.errorMessage : controller->lastMessage);
    else if (snapshot.state == DebugSessionState::Stopping && previous != DebugSessionState::Stopping)
        appendEvent(controller, DebugEventKind::Stopped, snapshot.state, DebugStopReason::UserRequested, "Debug stop requested");
    else if (snapshot.state == DebugSessionState::Exited && previous != DebugSessionState::Exited) {
        appendEvent(controller, DebugEventKind::Exited, snapshot.state, DebugStopReason::Exited, "Hosted Native ELF target exited");
        controller->active = false;
        clearStoppedContext(controller);
        controller->lastBreakpointId = 0;
        controller->backendExecutionState = DebugBackendExecutionState::None;
        clearSourceStep(controller, DebugSourceStepStatus::Cancelled, "Source step ended because the target exited");
    } else if (snapshot.state == DebugSessionState::Failed && previous != DebugSessionState::Failed) {
        appendEvent(controller, DebugEventKind::Failed, snapshot.state, snapshot.stopReason, controller->lastMessage);
        controller->active = false;
        clearStoppedContext(controller);
        controller->lastBreakpointId = 0;
        controller->backendExecutionState = DebugBackendExecutionState::None;
        clearSourceStep(controller, DebugSourceStepStatus::Failed, "Source step ended because the session failed");
    }
    if (snapshot.state == DebugSessionState::Paused && snapshot.stopReason == DebugStopReason::Breakpoint &&
        snapshot.executionState != DebugBackendExecutionState::SingleStepPending) {
        controller->stopGeneration = snapshot.stopGeneration == 0 ? controller->stopGeneration + 1 : snapshot.stopGeneration;
        if (snapshot.registerContext.valid) controller->stoppedContext = snapshot.registerContext;
    } else if (continuingFromBreakpoint || continuingFromSourceStep) {
        // The stopped context is valid only for the exact paused stop. Do not
        // let it survive the real internal single-step completion.
        clearStoppedContext(controller);
    }
}

static bool applyOwnedBreakpointTrap(DebugController* controller, const DebugBackendSnapshot& snapshot) {
    if (!controller || !snapshot.breakpointTrap || !snapshot.targetAddress.valid ||
        snapshot.processId != controller->processId || snapshot.nativeRuntimeId != controller->nativeRuntimeId ||
        snapshot.breakpointBindingId == 0) return false;
    if (controller->sourceStep.active && snapshot.threadId != controller->sourceStep.threadId) return false;
    int ownerIndex = -1;
    for (uint32_t i = 0; i < controller->breakpointCount; ++i) {
        const DebugBreakpoint& breakpoint = controller->breakpoints[i];
        bool ownsAddress = breakpoint.location.instructionAddress.valid &&
            breakpoint.location.instructionAddress.value == snapshot.targetAddress.value;
        for (uint32_t addressIndex = 0; !ownsAddress && addressIndex < breakpoint.mappedAddressCount; ++addressIndex) {
            ownsAddress = breakpoint.mappedAddresses[addressIndex].valid &&
                breakpoint.mappedAddresses[addressIndex].value == snapshot.targetAddress.value;
        }
        if (breakpoint.enabled && breakpoint.backendBindingId == snapshot.breakpointBindingId && ownsAddress) {
            ownerIndex = static_cast<int>(i);
            break;
        }
    }
    if (ownerIndex < 0) return false;
    controller->currentInstructionAddress = snapshot.targetAddress;
    controller->currentThreadId = snapshot.threadId;
    controller->reportedInstructionPointer = snapshot.instructionPointer;
    controller->stopGeneration = snapshot.stopGeneration == 0 ? controller->stopGeneration + 1 : snapshot.stopGeneration;
    if (snapshot.registerContext.valid) controller->stoppedContext = snapshot.registerContext;
    controller->lastBreakpointId = controller->breakpoints[ownerIndex].id;
    controller->stopReason = DebugStopReason::Breakpoint;
    if (controller->sourceStep.active)
        clearSourceStep(controller, DebugSourceStepStatus::Cancelled, "Source step interrupted by a breakpoint");
    copyText(controller->lastMessage, sizeof(controller->lastMessage), "Breakpoint trap observed");
    appendEvent(controller, DebugEventKind::BreakpointTrap, controller->state, DebugStopReason::Breakpoint, controller->lastMessage);
    for (uint32_t i = 0; i < controller->breakpointCount; ++i) {
        DebugBreakpoint& breakpoint = controller->breakpoints[i];
        bool ownsAddress = breakpoint.location.instructionAddress.valid &&
            breakpoint.location.instructionAddress.value == snapshot.targetAddress.value;
        for (uint32_t addressIndex = 0; !ownsAddress && addressIndex < breakpoint.mappedAddressCount; ++addressIndex) {
            ownsAddress = breakpoint.mappedAddresses[addressIndex].valid &&
                breakpoint.mappedAddresses[addressIndex].value == snapshot.targetAddress.value;
        }
        if (breakpoint.enabled && breakpoint.backendBindingId == snapshot.breakpointBindingId && ownsAddress) {
            breakpoint.lastHit = true;
            breakpoint.lastHitSessionGeneration = controller->sessionGeneration;
        }
    }
    appendEvent(controller, DebugEventKind::BreakpointHit, controller->state, DebugStopReason::Breakpoint, "Owned breakpoint hit");
    return true;
}

static bool bindAndReleaseIfReady(DebugController* controller, const DebugBackend& backend,
                                  DebugBackendSnapshot* snapshot) {
    if (!controller || !snapshot || controller->targetExecutionReleased || snapshot->nativeRuntimeId == 0 ||
        !backend.capabilities.canBindSoftwareBreakpoint || !backend.debugCommand) return true;
    uint32_t acceptedBindings = 0;
    for (uint32_t i = 0; i < controller->breakpointCount; ++i) {
        DebugBreakpoint& breakpoint = controller->breakpoints[i];
        if (!breakpoint.enabled || breakpoint.state != DebugBreakpointState::Mapped ||
            breakpoint.backendBindingId != 0 || !breakpoint.location.instructionAddress.valid) continue;
        DebugBackendBinding binding = {};
        const bool called = backend.bindSoftwareBreakpoint(backend.userData, controller->target,
            controller->sessionGeneration, snapshot->processId, snapshot->nativeRuntimeId, breakpoint, &binding);
        if (!called || !binding.accepted) {
            breakpoint.state = DebugBreakpointState::Rejected;
            breakpoint.mappingError = DebugErrorCode::BreakpointRejected;
            setBreakpointMessage(breakpoint, binding.message[0] ? binding.message : "Rejected: software breakpoint bind failed");
            appendEvent(controller, DebugEventKind::BreakpointRejected, controller->state, DebugStopReason::Unknown, breakpoint.message);
            HostedDebugResult restored = {};
            const bool restoredSuccessfully = backend.debugCommand(backend.userData, HostedDebugCommand::RestoreAll,
                controller->debugHandle, controller->sessionGeneration, snapshot->processId, snapshot->nativeRuntimeId,
                0, 0, controller->target.artifactSha256, &restored);
            for (uint32_t rollbackIndex = 0; rollbackIndex < controller->breakpointCount; ++rollbackIndex) {
                DebugBreakpoint& rollback = controller->breakpoints[rollbackIndex];
                if (rollback.backendBindingId == 0) continue;
                rollback.backendBindingId = 0;
                rollback.state = DebugBreakpointState::Mapped;
                rollback.location.mapping = DebugMappingState::Mapped;
                rollback.lastHit = false;
                rollback.lastHitSessionGeneration = 0;
                setBreakpointMessage(rollback, "Mapped: bind rolled back");
            }
            controller->error = restoredSuccessfully ? DebugErrorCode::BreakpointRejected : DebugErrorCode::BackendError;
            setMessage(controller, restoredSuccessfully ?
                (acceptedBindings == 0 ? "Hosted breakpoint bind rejected; execution gate remains closed" :
                    "Hosted breakpoint bind rolled back after partial failure") :
                (restored.errorMessage[0] ? restored.errorMessage : "Hosted breakpoint restore failed; execution gate remains closed"));
            return false;
        }
        DebugAddress address = breakpoint.location.instructionAddress;
        DebugErrorCode error = DebugErrorCode::None;
        if (!DebugControllerApplyBreakpointBinding(controller, controller->sessionGeneration, breakpoint.id, true,
                                                   binding.bindingId, address, binding.message[0] ? binding.message : "Bound / Verified", &error)) return false;
        ++acceptedBindings;
    }
    HostedDebugResult released = {};
    if (!backend.debugCommand(backend.userData, HostedDebugCommand::ReleaseExecution, controller->debugHandle,
                              controller->sessionGeneration, snapshot->processId, snapshot->nativeRuntimeId,
                              0, 0, controller->target.artifactSha256, &released)) {
        controller->error = DebugErrorCode::BackendError;
        setMessage(controller, released.errorMessage[0] ? released.errorMessage : "Hosted debugger could not release the launch gate");
        return false;
    }
    controller->targetExecutionReleased = true;
    snapshot->state = DebugSessionState::Running;
    snapshot->stopReason = DebugStopReason::None;
    return true;
}

static bool mapSourceStepLocation(const DebugController* controller, const DebugDwarfMapper* mapper,
                                  uint64_t address, DebugSourceLocation* location) {
    if (!controller || !mapper || !location || !DebugDwarfMapperIsReady(mapper)) return false;
    *location = DebugSourceLocation();
    location->instructionAddress.valid = true;
    location->instructionAddress.value = address;
    location->projectGeneration = controller->target.projectGeneration;
    location->mapping = DebugMappingState::Mapped;
    DebugDwarfError mappingError = DebugDwarfError::None;
    return DebugDwarfMapperMapAddressToSource(mapper, address, location->relativePath,
                                              sizeof(location->relativePath), &location->line,
                                              &location->column, &mappingError);
}

static void publishSourceStepStop(DebugController* controller, const DebugBackendSnapshot& snapshot,
                                  const DebugSourceLocation& location, DebugSourceStepStatus status,
                                  const char* message) {
    controller->state = DebugSessionState::Paused;
    controller->active = true;
    controller->stopReason = DebugStopReason::Step;
    controller->backendExecutionState = DebugBackendExecutionState::PausedAtSourceStep;
    controller->currentInstructionAddress.valid = true;
    controller->currentInstructionAddress.value = snapshot.registerContext.rip;
    controller->reportedInstructionPointer = snapshot.instructionPointer == 0 ?
        snapshot.registerContext.rip : snapshot.instructionPointer;
    controller->currentThreadId = snapshot.threadId;
    controller->stopGeneration = controller->stopGeneration == UINT64_MAX ? 1 : controller->stopGeneration + 1;
    controller->stoppedContext = snapshot.registerContext;
    controller->stoppedContext.stopGeneration = controller->stopGeneration;
    controller->currentLocation = location;
    controller->currentLocation.instructionAddress = controller->currentInstructionAddress;
    controller->sourceStep.lastAddress = snapshot.registerContext.rip;
    controller->sourceStep.lastSourceLocation = location;
    clearSourceStep(controller, status, message);
    setMessage(controller, message);
    appendEvent(controller, status == DebugSourceStepStatus::LimitExceeded ?
        DebugEventKind::SourceStepLimitExceeded : DebugEventKind::StepComplete,
        controller->state, DebugStopReason::Step, message);
}

static bool processSourceStepTrap(DebugController* controller, const DebugBackend& backend,
                                  const DebugDwarfMapper* mapper, const DebugBackendSnapshot& snapshot) {
    if (!controller || !controller->sourceStep.active || !snapshot.singleStepTrap ||
        snapshot.singleStepKind != static_cast<uint32_t>(HostedDebugSingleStepKind::UserSource)) return false;
    DebugSourceStepOperation& operation = controller->sourceStep;
    if ((snapshot.sessionGeneration != 0 && snapshot.sessionGeneration != operation.sessionGeneration) ||
        snapshot.processId != operation.processId || snapshot.nativeRuntimeId != controller->nativeRuntimeId ||
        snapshot.threadId != operation.threadId || !DebugRegisterContextIsValid(snapshot.registerContext) ||
        snapshot.registerContext.sessionGeneration != operation.sessionGeneration ||
        snapshot.registerContext.threadId != operation.threadId) {
        controller->error = snapshot.threadId != operation.threadId ? DebugErrorCode::WrongStepThread : DebugErrorCode::StaleSourceStep;
        setMessage(controller, "Rejected stale or wrong-thread source-step event");
        clearSourceStep(controller, DebugSourceStepStatus::Failed, controller->lastMessage);
        controller->state = DebugSessionState::Failed;
        controller->active = false;
        appendEvent(controller, DebugEventKind::Failed, controller->state, DebugStopReason::Exception, controller->lastMessage);
        return true;
    }
    ++operation.stepCount;
    operation.lastAddress = snapshot.registerContext.rip;
    operation.lastSourceLocation = DebugSourceLocation();
    DebugSourceLocation location = {};
    const bool mapped = mapSourceStepLocation(controller, mapper, snapshot.registerContext.rip, &location);
    if (mapped) operation.lastSourceLocation = location;
    if (mapped && !sameSourceStepLocation(operation.startingSourceLocation, location)) {
        publishSourceStepStop(controller, snapshot, location, DebugSourceStepStatus::Completed,
                               "Source step completed at a new source location");
        return true;
    }
    if (operation.stepCount >= operation.maxStepCount) {
        publishSourceStepStop(controller, snapshot, mapped ? location : DebugSourceLocation(),
                               DebugSourceStepStatus::LimitExceeded,
                               "SourceStepLimitExceeded: bounded instruction-step limit reached");
        controller->error = DebugErrorCode::SourceStepLimitExceeded;
        return true;
    }
    controller->stoppedContext = snapshot.registerContext;
    const bool accepted = backend.stepInstruction && backend.stepInstruction(
        backend.userData, controller->sessionGeneration, controller->stoppedContext,
        0, 0, 0, false);
    controller->stoppedContext = DebugRegisterContext();
    if (!accepted) {
        controller->error = DebugErrorCode::SourceStepFailed;
        setMessage(controller, "Source step instruction request was rejected");
        clearSourceStep(controller, DebugSourceStepStatus::Failed, controller->lastMessage);
        controller->state = DebugSessionState::Failed;
        controller->active = false;
        appendEvent(controller, DebugEventKind::Failed, controller->state, DebugStopReason::Exception, controller->lastMessage);
        return true;
    }
    controller->state = DebugSessionState::Stepping;
    controller->stopReason = DebugStopReason::None;
    controller->backendExecutionState = DebugBackendExecutionState::UserSourceStepPending;
    setMessage(controller, "Source step instruction pending");
    return true;
}

} // namespace

const char* DebugSessionStateName(DebugSessionState state) {
    switch (state) {
    case DebugSessionState::Idle: return "Idle";
    case DebugSessionState::Launching: return "Launching";
    case DebugSessionState::Running: return "Running";
    case DebugSessionState::Paused: return "Paused";
    case DebugSessionState::Stopping: return "Stopping";
    case DebugSessionState::Exited: return "Exited";
    case DebugSessionState::Failed: return "Failed";
    case DebugSessionState::Stepping: return "Stepping";
    }
    return "Unknown";
}

const char* DebugErrorName(DebugErrorCode error) {
    switch (error) {
    case DebugErrorCode::None: return "none";
    case DebugErrorCode::InvalidRequest: return "invalid_request";
    case DebugErrorCode::InvalidTransition: return "invalid_transition";
    case DebugErrorCode::NoProject: return "no_project";
    case DebugErrorCode::NoRunnableTarget: return "no_runnable_target";
    case DebugErrorCode::BackendUnavailable: return "backend_unavailable";
    case DebugErrorCode::CapabilityUnavailable: return "capability_unavailable";
    case DebugErrorCode::LaunchFailed: return "launch_failed";
    case DebugErrorCode::StopFailed: return "stop_failed";
    case DebugErrorCode::PollFailed: return "poll_failed";
    case DebugErrorCode::StaleSession: return "stale_session";
    case DebugErrorCode::OutsideProject: return "outside_project";
    case DebugErrorCode::InvalidSourcePath: return "invalid_source_path";
    case DebugErrorCode::InvalidLine: return "invalid_line";
    case DebugErrorCode::DuplicateBreakpoint: return "duplicate_breakpoint";
    case DebugErrorCode::BreakpointNotFound: return "breakpoint_not_found";
    case DebugErrorCode::BreakpointRejected: return "breakpoint_rejected";
    case DebugErrorCode::SourceMappingUnavailable: return "source_mapping_unavailable";
    case DebugErrorCode::ProjectGenerationMismatch: return "project_generation_mismatch";
    case DebugErrorCode::BackendError: return "backend_error";
    case DebugErrorCode::NoDebugInfo: return "no_debug_info";
    case DebugErrorCode::MissingLineSection: return "missing_line_section";
    case DebugErrorCode::MalformedElf: return "malformed_elf";
    case DebugErrorCode::MalformedDwarf: return "malformed_dwarf";
    case DebugErrorCode::UnsupportedDwarfVersion: return "unsupported_dwarf_version";
    case DebugErrorCode::UnsupportedForm: return "unsupported_form";
    case DebugErrorCode::UnsupportedArchitecture: return "unsupported_architecture";
    case DebugErrorCode::ArtifactChanged: return "artifact_changed";
    case DebugErrorCode::SourceNotFound: return "source_not_found";
    case DebugErrorCode::LineNotMapped: return "line_not_mapped";
    case DebugErrorCode::Truncated: return "truncated";
    case DebugErrorCode::MappingLimitExceeded: return "mapping_limit_exceeded";
    case DebugErrorCode::UnsupportedOpcode: return "unsupported_opcode";
    case DebugErrorCode::StaleStopContext: return "stale_stop_context";
    case DebugErrorCode::OriginalByteRestoreFailed: return "original_byte_restore_failed";
    case DebugErrorCode::ContextReadFailed: return "context_read_failed";
    case DebugErrorCode::ContextWriteFailed: return "context_write_failed";
    case DebugErrorCode::RipCorrectionFailed: return "rip_correction_failed";
    case DebugErrorCode::TrapFlagSetFailed: return "trap_flag_set_failed";
    case DebugErrorCode::TargetResumeFailed: return "target_resume_failed";
    case DebugErrorCode::SingleStepNotObserved: return "single_step_not_observed";
    case DebugErrorCode::BreakpointReinstallFailed: return "breakpoint_reinstall_failed";
    case DebugErrorCode::TrapFlagClearFailed: return "trap_flag_clear_failed";
    case DebugErrorCode::SourceStepUnavailable: return "source_step_unavailable";
    case DebugErrorCode::SourceStepLimitExceeded: return "source_step_limit_exceeded";
    case DebugErrorCode::SourceStepFailed: return "source_step_failed";
    case DebugErrorCode::StaleSourceStep: return "stale_source_step";
    case DebugErrorCode::WrongStepThread: return "wrong_step_thread";
    case DebugErrorCode::StepNotObserved: return "step_not_observed";
    }
    return "unknown";
}

const char* DebugBreakpointStateName(DebugBreakpointState state) {
    switch (state) {
    case DebugBreakpointState::Pending: return "Pending";
    case DebugBreakpointState::Mapped: return "Mapped";
    case DebugBreakpointState::Verified: return "Verified";
    case DebugBreakpointState::Rejected: return "Rejected";
    case DebugBreakpointState::Disabled: return "Disabled";
    case DebugBreakpointState::Stale: return "Stale";
    }
    return "Unknown";
}

const char* DebugStopReasonName(DebugStopReason reason) {
    switch (reason) {
    case DebugStopReason::None: return "None";
    case DebugStopReason::Breakpoint: return "Breakpoint";
    case DebugStopReason::UserRequested: return "User requested";
    case DebugStopReason::Exited: return "Exited";
    case DebugStopReason::Exception: return "Exception";
    case DebugStopReason::Signal: return "Signal";
    case DebugStopReason::EntryPoint: return "Entry point";
    case DebugStopReason::Unknown: return "Unknown";
    case DebugStopReason::Step: return "Step";
    }
    return "Unknown";
}

const char* DebugEventKindName(DebugEventKind kind) {
    switch (kind) {
    case DebugEventKind::None: return "None";
    case DebugEventKind::Launched: return "Launched";
    case DebugEventKind::Running: return "Running";
    case DebugEventKind::Paused: return "Paused";
    case DebugEventKind::BreakpointBound: return "Breakpoint bound";
    case DebugEventKind::BreakpointRejected: return "Breakpoint rejected";
    case DebugEventKind::Stopped: return "Stopped";
    case DebugEventKind::Exited: return "Exited";
    case DebugEventKind::Failed: return "Failed";
    case DebugEventKind::TargetCreated: return "Target created";
    case DebugEventKind::BreakpointUnbound: return "Breakpoint unbound";
    case DebugEventKind::BreakpointTrap: return "Breakpoint trap";
    case DebugEventKind::BreakpointHit: return "Breakpoint hit";
    case DebugEventKind::BreakpointRestoreFailed: return "Breakpoint restore failed";
    case DebugEventKind::UnexpectedTrap: return "Unexpected trap";
    case DebugEventKind::ContinueRequested: return "Continue requested";
    case DebugEventKind::SingleStepComplete: return "Internal single-step complete";
    case DebugEventKind::BreakpointRebound: return "Breakpoint rebound";
    case DebugEventKind::StepRequested: return "Step requested";
    case DebugEventKind::StepComplete: return "Step complete";
    case DebugEventKind::SourceStepLimitExceeded: return "Source-step limit exceeded";
    }
    return "Unknown";
}

bool DebugCapabilitiesEqual(const DebugCapabilities& left, const DebugCapabilities& right) {
    return left.canLaunch == right.canLaunch && left.canStop == right.canStop &&
        left.canPause == right.canPause && left.canContinue == right.canContinue &&
        left.canSetInstructionBreakpoint == right.canSetInstructionBreakpoint &&
        left.canSetSourceBreakpoint == right.canSetSourceBreakpoint && left.canStepInto == right.canStepInto &&
        left.canStepOver == right.canStepOver && left.canStepOut == right.canStepOut &&
        left.canReadRegisters == right.canReadRegisters && left.canReadMemory == right.canReadMemory &&
        left.canWriteMemory == right.canWriteMemory && left.canEnumerateThreads == right.canEnumerateThreads &&
        left.canReadCallStack == right.canReadCallStack && left.canResolveSourceLocations == right.canResolveSourceLocations &&
        left.canEvaluateExpressions == right.canEvaluateExpressions &&
        left.canBindSoftwareBreakpoint == right.canBindSoftwareBreakpoint &&
        left.canObserveBreakpointTrap == right.canObserveBreakpointTrap &&
        left.canRestoreBreakpoint == right.canRestoreBreakpoint &&
        left.canReadInstructionPointer == right.canReadInstructionPointer;
}

bool DebugCapabilitiesHasPause(const DebugCapabilities& capabilities) { return capabilities.canPause; }
bool DebugCapabilitiesHasContinue(const DebugCapabilities& capabilities) { return capabilities.canContinue; }
bool DebugRegisterContextIsValid(const DebugRegisterContext& context) {
    return context.valid && context.architecture != DebugArchitecture::Unknown &&
        context.processId != 0 && context.nativeRuntimeId != 0 && context.threadId != 0 &&
        context.sessionGeneration != 0 && context.stopGeneration != 0;
}

bool DebugTargetFromBuild(const Project& project, const BuildResult& build,
                          uint64_t projectGeneration, DebugTarget* target, DebugErrorCode* error) {
    if (error) *error = DebugErrorCode::None;
    if (!target) { if (error) *error = DebugErrorCode::InvalidRequest; return false; }
    *target = DebugTarget();
    RunRequest request = {};
    RunErrorCode runError = RunErrorCode::None;
    if (!RunRequestFromBuild(project, build, &request, &runError)) {
        if (error) *error = runError == RunErrorCode::BuildRequired ? DebugErrorCode::NoRunnableTarget : DebugErrorCode::InvalidRequest;
        return false;
    }
    if (!copyText(target->projectId, sizeof(target->projectId), project.projectId) ||
        !copyText(target->projectRoot, sizeof(target->projectRoot), project.rootPath) ||
        !copyText(target->targetProfile, sizeof(target->targetProfile), project.targetProfileId) ||
        !copyText(target->architecture, sizeof(target->architecture), project.architecture[0] ? project.architecture : "amd64") ||
        !copyText(target->abi, sizeof(target->abi), project.abi[0] ? project.abi : "guidexos-c-abi-v1") ||
        !copyText(target->applicationId, sizeof(target->applicationId), project.projectId) ||
        !copyText(target->manifestPath, sizeof(target->manifestPath), request.manifestPath) ||
        !copyText(target->executablePath, sizeof(target->executablePath), request.artifactPath) ||
        !copyText(target->artifactSha256, sizeof(target->artifactSha256), request.artifactSha256) ||
        !copyText(target->workingDirectory, sizeof(target->workingDirectory), project.rootPath)) {
        if (error) *error = DebugErrorCode::InvalidRequest;
        return false;
    }
    target->artifactSize = build.artifactSize;
    target->projectGeneration = projectGeneration;
    return true;
}

bool DebugRelativeSourcePath(const char* projectRoot, const char* absolutePath,
                             char* relativePath, uint32_t relativePathSize) {
    if (!projectRoot || !absolutePath || !relativePath || relativePathSize == 0) return false;
    char root[kMaxPathBytes] = {};
    char absolute[kMaxPathBytes] = {};
    if (!NormalizePath(projectRoot, root, sizeof(root)) || !NormalizePath(absolutePath, absolute, sizeof(absolute))) return false;
    const uint32_t rootLength = textLength(root, sizeof(root));
    const uint32_t absoluteLength = textLength(absolute, sizeof(absolute));
    if (absoluteLength <= rootLength || absolute[rootLength] != '/') return false;
    // The prefix check is case-insensitive and path-boundary aware. This
    // keeps D:/workspace/app2 outside D:/workspace/app.
    for (uint32_t i = 0; i < rootLength; ++i) {
        char left = root[i];
        char right = absolute[i];
        if (left >= 'A' && left <= 'Z') left = static_cast<char>(left + ('a' - 'A'));
        if (right >= 'A' && right <= 'Z') right = static_cast<char>(right + ('a' - 'A'));
        if (left != right) return false;
    }
    const char* value = absolute + rootLength + 1;
    return copyText(relativePath, relativePathSize, value) && !PathContainsTraversal(relativePath);
}

bool DebugControllerInit(DebugController* controller) {
    if (!controller) return false;
    // Do not value-initialize this large bounded controller through a stack
    // temporary: Native ELF application stacks are intentionally small.
    unsigned char* bytes = reinterpret_cast<unsigned char*>(controller);
    for (uint32_t i = 0; i < sizeof(DebugController); ++i) bytes[i] = 0;
    controller->state = DebugSessionState::Idle;
    controller->error = DebugErrorCode::None;
    controller->nextBreakpointId = 1;
    controller->nextEventSequence = 1;
    return true;
}

bool DebugControllerSetProjectContext(DebugController* controller, const char* projectId,
                                      const char* projectRoot, uint64_t projectGeneration) {
    if (!controller || !projectId || !projectRoot || projectId[0] == '\0' || projectRoot[0] == '\0') return false;
    if (!copyText(controller->projectId, sizeof(controller->projectId), projectId) ||
        !copyText(controller->projectRoot, sizeof(controller->projectRoot), projectRoot)) return false;
    controller->projectGeneration = projectGeneration;
    return true;
}

void DebugControllerClearBreakpoints(DebugController* controller) {
    if (!controller) return;
    controller->breakpointCount = 0;
}

bool DebugControllerStart(DebugController* controller, const DebugBackend& backend,
                          const DebugTarget& target, DebugErrorCode* error) {
    if (error) *error = DebugErrorCode::None;
    if (!controller || !backend.launch || !backend.capabilities.canLaunch || target.projectId[0] == '\0' ||
        target.executablePath[0] == '\0') {
        if (error) *error = !backend.launch || !backend.capabilities.canLaunch ? DebugErrorCode::CapabilityUnavailable : DebugErrorCode::InvalidRequest;
        return false;
    }
    if (controller->active || (controller->state != DebugSessionState::Idle &&
        controller->state != DebugSessionState::Exited && controller->state != DebugSessionState::Failed)) {
        if (error) *error = DebugErrorCode::InvalidTransition;
        return false;
    }
    if (controller->sessionGeneration == UINT64_MAX) controller->sessionGeneration = 1;
    else ++controller->sessionGeneration;
    if (controller->sessionGeneration == 0) controller->sessionGeneration = 1;
    controller->target = target;
    controller->projectGeneration = target.projectGeneration;
    copyText(controller->projectId, sizeof(controller->projectId), target.projectId);
    copyText(controller->projectRoot, sizeof(controller->projectRoot), target.projectRoot);
    controller->capabilities = backend.capabilities;
    copyText(controller->backendName, sizeof(controller->backendName), backend.name);
    controller->state = DebugSessionState::Launching;
    controller->active = true;
    controller->error = DebugErrorCode::None;
    controller->processId = 0;
    controller->nativeRuntimeId = 0;
    controller->debugHandle = 0;
    controller->exitCode = 0;
    controller->stopReason = DebugStopReason::None;
    controller->targetExecutionReleased = false;
    controller->backendExecutionState = DebugBackendExecutionState::None;
    controller->stopGeneration = 0;
    controller->stoppedContext = DebugRegisterContext();
    controller->currentInstructionAddress = DebugAddress();
    controller->currentLocation = DebugSourceLocation();
    controller->currentThreadId = 0;
    controller->reportedInstructionPointer = 0;
    controller->lastBreakpointId = 0;
    controller->sourceStep = DebugSourceStepOperation();
    setMessage(controller, "Launching debug target");
    appendEvent(controller, DebugEventKind::Launched, DebugSessionState::Launching, DebugStopReason::None, "Debug target launch requested");
    for (uint32_t i = 0; i < controller->breakpointCount; ++i) {
        DebugBreakpoint& breakpoint = controller->breakpoints[i];
        breakpoint.sessionGeneration = controller->sessionGeneration;
        if (!breakpoint.enabled) breakpoint.state = DebugBreakpointState::Disabled;
        else if (breakpoint.location.projectGeneration != target.projectGeneration) {
            breakpoint.state = DebugBreakpointState::Stale;
            setBreakpointMessage(breakpoint, "Stale: built project generation differs");
        } else if (breakpoint.state == DebugBreakpointState::Mapped && breakpoint.mappedAddressCount > 0) {
            breakpoint.location.mapping = DebugMappingState::Mapped;
        } else if (!backend.capabilities.canSetSourceBreakpoint) {
            breakpoint.state = DebugBreakpointState::Pending;
            setBreakpointMessage(breakpoint, "Pending: source mapping unavailable");
        }
    }
    DebugBackendSnapshot snapshot;
    clearSnapshot(&snapshot);
    snapshot.sessionGeneration = controller->sessionGeneration;
    if (!backend.launch(backend.userData, target, controller->sessionGeneration, &snapshot)) {
        controller->state = DebugSessionState::Failed;
        controller->active = false;
        controller->error = DebugErrorCode::LaunchFailed;
        setMessage(controller, snapshot.errorMessage[0] ? snapshot.errorMessage : "Debugger backend launch failed");
        appendEvent(controller, DebugEventKind::Failed, controller->state, DebugStopReason::Unknown, controller->lastMessage);
        if (error) *error = controller->error;
        return false;
    }
    if (snapshot.sessionGeneration == 0) snapshot.sessionGeneration = controller->sessionGeneration;
    if (!DebugControllerApplySnapshot(controller, controller->sessionGeneration, snapshot)) {
        if (error) *error = controller->error;
        return false;
    }
    return true;
}

bool DebugControllerApplySnapshot(DebugController* controller, uint64_t sessionGeneration,
                                  const DebugBackendSnapshot& snapshot) {
    if (!controller || sessionGeneration == 0 || sessionGeneration != controller->sessionGeneration ||
        (snapshot.sessionGeneration != 0 && snapshot.sessionGeneration != sessionGeneration)) {
        if (controller) controller->error = DebugErrorCode::StaleSession;
        return false;
    }
    const bool requiresOwnedBreakpoint = snapshot.state == DebugSessionState::Paused &&
        snapshot.stopReason == DebugStopReason::Breakpoint &&
        snapshot.executionState != DebugBackendExecutionState::SingleStepPending;
    if ((requiresOwnedBreakpoint || snapshot.breakpointTrap) &&
        (!snapshot.breakpointTrap || !applyOwnedBreakpointTrap(controller, snapshot))) {
        controller->error = DebugErrorCode::BackendError;
        setMessage(controller, "Rejected unowned hosted breakpoint trap");
        appendEvent(controller, DebugEventKind::UnexpectedTrap, controller->state, DebugStopReason::Unknown, controller->lastMessage);
        return false;
    }
    if (snapshot.executionState == DebugBackendExecutionState::SingleStepPending &&
        (!DebugRegisterContextIsValid(controller->stoppedContext) ||
         controller->state != DebugSessionState::Paused ||
         controller->stopReason != DebugStopReason::Breakpoint)) {
        controller->error = DebugErrorCode::StaleStopContext;
        setMessage(controller, "Rejected stale internal single-step state");
        return false;
    }
    if (snapshot.executionState == DebugBackendExecutionState::UserSourceStepPending &&
        (!controller->sourceStep.active || snapshot.processId != controller->sourceStep.processId ||
         snapshot.nativeRuntimeId != controller->nativeRuntimeId || snapshot.threadId != controller->sourceStep.threadId)) {
        controller->error = DebugErrorCode::StaleSourceStep;
        setMessage(controller, "Rejected stale user source-step state");
        return false;
    }
    controller->error = DebugErrorCode::None;
    applySnapshotUnchecked(controller, snapshot);
    return controller->error != DebugErrorCode::InvalidTransition;
}

bool DebugControllerPoll(DebugController* controller, const DebugBackend& backend,
                         const DebugDwarfMapper* mapper) {
    if (!controller || !controller->active || !backend.poll) return false;
    DebugBackendSnapshot snapshot;
    clearSnapshot(&snapshot);
    snapshot.sessionGeneration = controller->sessionGeneration;
    if (!backend.poll(backend.userData, controller->sessionGeneration, &snapshot)) {
        controller->error = DebugErrorCode::PollFailed;
        controller->state = DebugSessionState::Failed;
        controller->active = false;
        setMessage(controller, snapshot.errorMessage[0] ? snapshot.errorMessage : "Debugger backend poll failed");
        appendEvent(controller, DebugEventKind::Failed, controller->state, DebugStopReason::Unknown, controller->lastMessage);
        return false;
    }
    if (!bindAndReleaseIfReady(controller, backend, &snapshot)) {
        controller->state = DebugSessionState::Failed;
        controller->active = false;
        appendEvent(controller, DebugEventKind::Failed, controller->state, DebugStopReason::Unknown, controller->lastMessage);
        return false;
    }
    if (snapshot.singleStepTrap &&
        snapshot.singleStepKind == static_cast<uint32_t>(HostedDebugSingleStepKind::UserSource)) {
        if (!processSourceStepTrap(controller, backend, mapper, snapshot)) {
            controller->error = DebugErrorCode::StaleSourceStep;
            setMessage(controller, "Unexpected user source-step event");
            return false;
        }
        return true;
    }
    if (controller->sourceStep.active && snapshot.singleStepTrap &&
        snapshot.singleStepKind != static_cast<uint32_t>(HostedDebugSingleStepKind::UserSource)) {
        controller->error = DebugErrorCode::StaleSourceStep;
        setMessage(controller, "Internal breakpoint-recovery step crossed the user source-step boundary");
        clearSourceStep(controller, DebugSourceStepStatus::Failed, controller->lastMessage);
        return false;
    }
    return DebugControllerApplySnapshot(controller, controller->sessionGeneration, snapshot);
}

bool DebugControllerRequestStop(DebugController* controller, const DebugBackend& backend,
                                DebugErrorCode* error) {
    if (error) *error = DebugErrorCode::None;
    if (!controller || !controller->active || !backend.stop || !backend.capabilities.canStop ||
        (controller->state != DebugSessionState::Running && controller->state != DebugSessionState::Paused &&
         controller->state != DebugSessionState::Stepping &&
         controller->state != DebugSessionState::Launching)) {
        if (error) *error = !backend.stop || !backend.capabilities.canStop ? DebugErrorCode::CapabilityUnavailable : DebugErrorCode::InvalidTransition;
        return false;
    }
    if (!backend.stop(backend.userData, controller->sessionGeneration)) {
        controller->error = DebugErrorCode::StopFailed;
        if (error) *error = controller->error;
        setMessage(controller, "Debugger backend stop request failed");
        return false;
    }
    controller->state = DebugSessionState::Stopping;
    controller->stopReason = DebugStopReason::UserRequested;
    controller->backendExecutionState = DebugBackendExecutionState::None;
    controller->stopGeneration = 0;
    controller->stoppedContext = DebugRegisterContext();
    clearSourceStep(controller, DebugSourceStepStatus::Cancelled, "Source step cancelled by stop request");
    setMessage(controller, "Stop requested");
    appendEvent(controller, DebugEventKind::Stopped, controller->state, DebugStopReason::UserRequested, controller->lastMessage);
    return true;
}

bool DebugControllerPause(DebugController* controller, const DebugBackend& backend,
                          DebugErrorCode* error) {
    if (error) *error = DebugErrorCode::None;
    if (!controller || !controller->active || controller->state != DebugSessionState::Running ||
        !backend.pause || !backend.capabilities.canPause) {
        if (error) *error = DebugErrorCode::CapabilityUnavailable;
        return false;
    }
    if (!backend.pause(backend.userData, controller->sessionGeneration)) {
        if (error) *error = DebugErrorCode::BackendError;
        return false;
    }
    return true;
}

bool DebugControllerContinue(DebugController* controller, const DebugBackend& backend,
                             DebugErrorCode* error) {
    if (error) *error = DebugErrorCode::None;
    if (!controller || !controller->active || controller->state != DebugSessionState::Paused ||
        !DebugRegisterContextIsValid(controller->stoppedContext) ||
        !backend.capabilities.canContinue) {
        if (error) *error = DebugErrorCode::CapabilityUnavailable;
        return false;
    }
    if (controller->stopReason == DebugStopReason::Step) {
        if (controller->backendExecutionState != DebugBackendExecutionState::PausedAtSourceStep ||
            !backend.resumeExecution) {
            if (error) *error = DebugErrorCode::CapabilityUnavailable;
            return false;
        }
        if (!backend.resumeExecution(backend.userData, controller->sessionGeneration, controller->stoppedContext)) {
            controller->error = DebugErrorCode::BackendError;
            if (error) *error = controller->error;
            setMessage(controller, "Source-step resume was rejected; target remains paused");
            return false;
        }
        controller->state = DebugSessionState::Running;
        controller->stopReason = DebugStopReason::None;
        controller->backendExecutionState = DebugBackendExecutionState::Running;
        clearStoppedContext(controller);
        clearSourceStep(controller, DebugSourceStepStatus::Cancelled, "Source-step context resumed");
        setMessage(controller, "Source-step stop resumed");
        return true;
    }
    if (controller->stopReason != DebugStopReason::Breakpoint ||
        controller->backendExecutionState != DebugBackendExecutionState::PausedAtBreakpoint ||
        !backend.continueExecution) {
        if (error) *error = DebugErrorCode::CapabilityUnavailable;
        return false;
    }
    const int breakpointIndex = findBreakpoint(controller, controller->lastBreakpointId);
    if (breakpointIndex < 0) {
        controller->error = DebugErrorCode::StaleStopContext;
        if (error) *error = controller->error;
        return false;
    }
    const DebugBreakpoint& current = controller->breakpoints[breakpointIndex];
    if (current.sessionGeneration != controller->sessionGeneration || current.backendBindingId == 0 ||
        !current.location.instructionAddress.valid ||
        current.location.instructionAddress.value != controller->currentInstructionAddress.value ||
        controller->stoppedContext.processId != controller->processId ||
        controller->stoppedContext.nativeRuntimeId != controller->nativeRuntimeId ||
        controller->stoppedContext.threadId != controller->currentThreadId ||
        controller->stoppedContext.stopGeneration != controller->stopGeneration) {
        controller->error = DebugErrorCode::StaleStopContext;
        setMessage(controller, "Stopped debugger context is stale");
        if (error) *error = controller->error;
        return false;
    }
    bool reinstallBreakpoint = false;
    for (uint32_t i = 0; i < controller->breakpointCount; ++i) {
        const DebugBreakpoint& breakpoint = controller->breakpoints[i];
        if (!breakpoint.enabled || breakpoint.backendBindingId != current.backendBindingId ||
            !breakpoint.location.instructionAddress.valid ||
            breakpoint.location.instructionAddress.value != current.location.instructionAddress.value) continue;
        reinstallBreakpoint = true;
        break;
    }
    controller->backendExecutionState = DebugBackendExecutionState::PreparingBreakpointResume;
    appendEvent(controller, DebugEventKind::ContinueRequested, controller->state,
                DebugStopReason::Breakpoint, "Breakpoint continuation requested");
    if (!backend.continueExecution(backend.userData, controller->sessionGeneration, controller->stoppedContext,
                                   current.id, current.backendBindingId, current.location.instructionAddress.value,
                                   reinstallBreakpoint)) {
        controller->backendExecutionState = DebugBackendExecutionState::PausedAtBreakpoint;
        controller->error = DebugErrorCode::BackendError;
        setMessage(controller, "Breakpoint continuation was rejected; target remains paused");
        if (error) *error = controller->error;
        return false;
    }
    controller->backendExecutionState = DebugBackendExecutionState::SingleStepPending;
    setMessage(controller, "Original breakpoint instruction restored; internal single-step pending");
    return true;
}

bool DebugControllerIsActive(const DebugController* controller) { return controller && controller->active; }
bool DebugControllerCanStart(const DebugController* controller) { return controller && !controller->active; }
bool DebugControllerCanStop(const DebugController* controller) { return controller && controller->active && controller->capabilities.canStop; }
bool DebugControllerCanPause(const DebugController* controller) { return controller && controller->active && controller->state == DebugSessionState::Running && controller->capabilities.canPause; }
bool DebugControllerCanContinue(const DebugController* controller) {
    if (!controller || !controller->active || controller->state != DebugSessionState::Paused ||
        !DebugRegisterContextIsValid(controller->stoppedContext) || !controller->capabilities.canContinue) return false;
    if (controller->stopReason == DebugStopReason::Step)
        return controller->backendExecutionState == DebugBackendExecutionState::PausedAtSourceStep &&
            controller->capabilities.canContinue;
    return controller->stopReason == DebugStopReason::Breakpoint &&
        controller->backendExecutionState == DebugBackendExecutionState::PausedAtBreakpoint;
}

bool DebugControllerCanStepInto(const DebugController* controller) {
    if (!controller || !controller->active || controller->state != DebugSessionState::Paused ||
        !controller->capabilities.canStepInto || !DebugRegisterContextIsValid(controller->stoppedContext) ||
        !controller->currentInstructionAddress.valid || !controller->currentLocation.relativePath[0] ||
        controller->currentLocation.line == 0 || controller->sourceStep.active) return false;
    return (controller->stopReason == DebugStopReason::Breakpoint &&
            controller->backendExecutionState == DebugBackendExecutionState::PausedAtBreakpoint) ||
        (controller->stopReason == DebugStopReason::Step &&
         controller->backendExecutionState == DebugBackendExecutionState::PausedAtSourceStep);
}

bool DebugControllerStepInto(DebugController* controller, const DebugBackend& backend,
                             const DebugDwarfMapper* mapper, DebugErrorCode* error) {
    if (error) *error = DebugErrorCode::None;
    if (!controller || !mapper || !DebugControllerCanStepInto(controller) ||
        !DebugDwarfMapperIsReady(mapper) || !backend.stepInstruction) {
        if (error) *error = !backend.stepInstruction || !controller || !controller->capabilities.canStepInto ?
            DebugErrorCode::CapabilityUnavailable : DebugErrorCode::SourceStepUnavailable;
        return false;
    }
    if (controller->stoppedContext.sessionGeneration != controller->sessionGeneration ||
        controller->stoppedContext.processId != controller->processId ||
        controller->stoppedContext.nativeRuntimeId != controller->nativeRuntimeId ||
        controller->stoppedContext.threadId != controller->currentThreadId ||
        controller->stoppedContext.stopGeneration != controller->stopGeneration) {
        if (error) *error = DebugErrorCode::StaleStopContext;
        controller->error = DebugErrorCode::StaleStopContext;
        return false;
    }
    const DebugRegisterContext startContext = controller->stoppedContext;
    DebugSourceStepOperation operation = DebugSourceStepOperation();
    operation.active = true;
    operation.status = DebugSourceStepStatus::Active;
    operation.sessionGeneration = controller->sessionGeneration;
    operation.stopGeneration = controller->stopGeneration;
    operation.processId = controller->processId;
    operation.threadId = controller->currentThreadId;
    operation.startingAddress = controller->currentInstructionAddress.value;
    operation.startingSourceLocation = controller->currentLocation;
    operation.startingSequence = 0;
    operation.stepCount = 0;
    operation.maxStepCount = kDebugMaxSourceStepInstructions;
    operation.lastAddress = operation.startingAddress;
    operation.lastSourceLocation = operation.startingSourceLocation;
    const bool fromBreakpoint = controller->stopReason == DebugStopReason::Breakpoint;
    if (fromBreakpoint) {
        const int breakpointIndex = findBreakpoint(controller, controller->lastBreakpointId);
        if (breakpointIndex < 0) {
            if (error) *error = DebugErrorCode::StaleStopContext;
            return false;
        }
        const DebugBreakpoint& breakpoint = controller->breakpoints[breakpointIndex];
        operation.breakpointId = breakpoint.id;
        operation.bindingId = breakpoint.backendBindingId;
        operation.breakpointAddress = breakpoint.location.instructionAddress.value;
        operation.reinstallBreakpoint = breakpoint.enabled;
    }
    controller->sourceStep = operation;
    appendEvent(controller, DebugEventKind::StepRequested, DebugSessionState::Stepping,
                DebugStopReason::Step, "User source-step requested");
    if (!backend.stepInstruction(backend.userData, controller->sessionGeneration, startContext,
                                 operation.breakpointId, operation.bindingId,
                                 fromBreakpoint ? operation.breakpointAddress : 0,
                                 fromBreakpoint && operation.reinstallBreakpoint)) {
        clearSourceStep(controller, DebugSourceStepStatus::Failed, "Source-step instruction request was rejected");
        controller->error = DebugErrorCode::SourceStepFailed;
        if (error) *error = controller->error;
        setMessage(controller, controller->sourceStep.reason);
        return false;
    }
    controller->state = DebugSessionState::Stepping;
    controller->stopReason = DebugStopReason::None;
    controller->backendExecutionState = DebugBackendExecutionState::UserSourceStepPending;
    clearStoppedContext(controller);
    setMessage(controller, "Source step instruction pending");
    return true;
}

bool DebugControllerAddBreakpoint(DebugController* controller, const char* projectId,
                                   const char* projectRoot, uint64_t projectGeneration,
                                   const char* sourcePath, uint32_t line, uint32_t column,
                                   uint32_t sourceGeneration, uint64_t* outBreakpointId,
                                   DebugErrorCode* error) {
    if (error) *error = DebugErrorCode::None;
    if (outBreakpointId) *outBreakpointId = 0;
    if (!controller || !projectId || !projectRoot || projectId[0] == '\0' ||
        controller->breakpointCount >= kDebugMaxBreakpoints) {
        if (error) *error = DebugErrorCode::InvalidRequest;
        return false;
    }
    if (!DebugControllerSetProjectContext(controller, projectId, projectRoot, projectGeneration)) {
        if (error) *error = DebugErrorCode::InvalidRequest;
        return false;
    }
    if (line == 0) { if (error) *error = DebugErrorCode::InvalidLine; return false; }
    char relative[kMaxProjectPathBytes] = {};
    if (!validRelativeSourcePath(projectRoot, sourcePath, relative, sizeof(relative))) {
        if (error) *error = DebugErrorCode::OutsideProject;
        return false;
    }
    for (uint32_t i = 0; i < controller->breakpointCount; ++i) {
        const DebugBreakpoint& existing = controller->breakpoints[i];
        if (equalText(existing.projectId, projectId, true) && existing.location.line == line &&
            PathsEqual(existing.location.relativePath, relative)) {
            if (error) *error = DebugErrorCode::DuplicateBreakpoint;
            if (outBreakpointId) *outBreakpointId = existing.id;
            return false;
        }
    }
    const uint64_t id = controller->nextBreakpointId == 0 ? 1 : controller->nextBreakpointId++;
    initializeBreakpoint(&controller->breakpoints[controller->breakpointCount++], id, projectId, relative,
                         projectGeneration, line, column, sourceGeneration);
    if (outBreakpointId) *outBreakpointId = id;
    return true;
}

bool DebugControllerToggleBreakpoint(DebugController* controller, const char* projectId,
                                     const char* projectRoot, uint64_t projectGeneration,
                                     const char* sourcePath, uint32_t line, uint32_t column,
                                     uint32_t sourceGeneration, uint64_t* outBreakpointId,
                                     DebugErrorCode* error) {
    if (error) *error = DebugErrorCode::None;
    if (outBreakpointId) *outBreakpointId = 0;
    char relative[kMaxProjectPathBytes] = {};
    if (!validRelativeSourcePath(projectRoot, sourcePath, relative, sizeof(relative))) {
        if (error) *error = DebugErrorCode::OutsideProject;
        return false;
    }
    if (!controller || !projectId || line == 0) { if (error) *error = DebugErrorCode::InvalidRequest; return false; }
    if (!DebugControllerSetProjectContext(controller, projectId, projectRoot, projectGeneration)) {
        if (error) *error = DebugErrorCode::InvalidRequest;
        return false;
    }
    for (uint32_t i = 0; i < controller->breakpointCount; ++i) {
        DebugBreakpoint& breakpoint = controller->breakpoints[i];
        if (!equalText(breakpoint.projectId, projectId, true) || breakpoint.location.line != line ||
            !PathsEqual(breakpoint.location.relativePath, relative)) continue;
        breakpoint.enabled = !breakpoint.enabled;
        breakpoint.state = breakpoint.enabled ? DebugBreakpointState::Pending : DebugBreakpointState::Disabled;
        breakpoint.location.mapping = breakpoint.enabled ? DebugMappingState::Pending : DebugMappingState::Unavailable;
        breakpoint.mappingError = breakpoint.enabled ? DebugErrorCode::SourceMappingUnavailable : DebugErrorCode::None;
        clearBreakpointMapping(breakpoint);
        breakpoint.location.projectGeneration = projectGeneration;
        breakpoint.location.sourceGeneration = sourceGeneration;
        setBreakpointMessage(breakpoint, breakpoint.enabled ? "Pending: source mapping unavailable" : "Disabled by user");
        if (outBreakpointId) *outBreakpointId = breakpoint.id;
        return true;
    }
    return DebugControllerAddBreakpoint(controller, projectId, projectRoot, projectGeneration, relative,
                                        line, column, sourceGeneration, outBreakpointId, error);
}

bool DebugControllerDeleteBreakpoint(DebugController* controller, uint64_t breakpointId,
                                     DebugErrorCode* error) {
    if (error) *error = DebugErrorCode::None;
    const int index = findBreakpoint(controller, breakpointId);
    if (index < 0) { if (error) *error = DebugErrorCode::BreakpointNotFound; return false; }
    for (uint32_t i = static_cast<uint32_t>(index) + 1; i < controller->breakpointCount; ++i)
        controller->breakpoints[i - 1] = controller->breakpoints[i];
    --controller->breakpointCount;
    return true;
}

bool DebugControllerSetBreakpointEnabled(DebugController* controller, uint64_t breakpointId,
                                         bool enabled, DebugErrorCode* error) {
    if (error) *error = DebugErrorCode::None;
    const int index = findBreakpoint(controller, breakpointId);
    if (index < 0) { if (error) *error = DebugErrorCode::BreakpointNotFound; return false; }
    DebugBreakpoint& breakpoint = controller->breakpoints[index];
    breakpoint.enabled = enabled;
    breakpoint.state = enabled ? DebugBreakpointState::Pending : DebugBreakpointState::Disabled;
    breakpoint.location.mapping = enabled ? DebugMappingState::Pending : DebugMappingState::Unavailable;
    breakpoint.mappingError = enabled ? DebugErrorCode::SourceMappingUnavailable : DebugErrorCode::None;
    clearBreakpointMapping(breakpoint);
    setBreakpointMessage(breakpoint, enabled ? "Pending: source mapping unavailable" : "Disabled by user");
    return true;
}

bool DebugControllerApplyBreakpointBinding(DebugController* controller, uint64_t sessionGeneration,
                                            uint64_t breakpointId, bool accepted,
                                            uint64_t backendBindingId, DebugAddress address,
                                            const char* message, DebugErrorCode* error) {
    if (error) *error = DebugErrorCode::None;
    if (!controller || sessionGeneration != controller->sessionGeneration) {
        if (error) *error = DebugErrorCode::StaleSession;
        return false;
    }
    const int index = findBreakpoint(controller, breakpointId);
    if (index < 0) { if (error) *error = DebugErrorCode::BreakpointNotFound; return false; }
    DebugBreakpoint& breakpoint = controller->breakpoints[index];
    if (!breakpoint.enabled) { breakpoint.state = DebugBreakpointState::Disabled; return true; }
    breakpoint.backendBindingId = accepted ? backendBindingId : 0;
    breakpoint.location.instructionAddress = address;
    breakpoint.location.mapping = accepted ? DebugMappingState::Mapped : DebugMappingState::Rejected;
    breakpoint.state = accepted ? DebugBreakpointState::Verified : DebugBreakpointState::Rejected;
    breakpoint.mappingError = accepted ? DebugErrorCode::None : DebugErrorCode::BreakpointRejected;
    if (accepted && address.valid) {
        if (breakpoint.mappedAddressCount == 0) breakpoint.mappedAddressCount = 1;
        breakpoint.mappedAddresses[0] = address;
    }
    breakpoint.lastHit = false;
    breakpoint.lastHitSessionGeneration = 0;
    setBreakpointMessage(breakpoint, message ? message : (accepted ? "Verified" : "Rejected by backend"));
    appendEvent(controller, accepted ? DebugEventKind::BreakpointBound : DebugEventKind::BreakpointRejected,
                controller->state, accepted ? DebugStopReason::None : DebugStopReason::Unknown,
                breakpoint.message);
    if (!accepted && error) *error = DebugErrorCode::BreakpointRejected;
    return accepted;
}

void DebugControllerMarkProjectGeneration(DebugController* controller, uint64_t projectGeneration) {
    if (!controller) return;
    controller->projectGeneration = projectGeneration;
    for (uint32_t i = 0; i < controller->breakpointCount; ++i) {
        DebugBreakpoint& breakpoint = controller->breakpoints[i];
        if (breakpoint.location.projectGeneration == projectGeneration || breakpoint.state == DebugBreakpointState::Disabled) continue;
        breakpoint.state = DebugBreakpointState::Stale;
        setBreakpointMessage(breakpoint, "Stale: built project generation differs");
    }
}

void DebugControllerMarkArtifactStale(DebugController* controller, const char* message) {
    if (!controller) return;
    for (uint32_t i = 0; i < controller->breakpointCount; ++i) {
        DebugBreakpoint& breakpoint = controller->breakpoints[i];
        if (!breakpoint.enabled) continue;
        breakpoint.state = DebugBreakpointState::Stale;
        breakpoint.location.mapping = DebugMappingState::Unavailable;
        breakpoint.mappingError = DebugErrorCode::ArtifactChanged;
        clearBreakpointMapping(breakpoint);
        setBreakpointMessage(breakpoint, message ? message : "Stale: executable changed");
    }
}

bool DebugControllerMapBreakpoints(DebugController* controller, const DebugDwarfMapper* mapper,
                                   DebugErrorCode* error) {
    if (error) *error = DebugErrorCode::None;
    if (!controller || !mapper) {
        if (error) *error = DebugErrorCode::InvalidRequest;
        return false;
    }
    if (!DebugDwarfMapperIsReady(mapper)) {
        DebugDwarfError mapperError = mapper->error == DebugDwarfError::None ? DebugDwarfError::NoDebugInfo : mapper->error;
        const DebugErrorCode mappedError = mapDwarfError(mapperError);
        controller->error = mappedError;
        for (uint32_t i = 0; i < controller->breakpointCount; ++i) {
            DebugBreakpoint& breakpoint = controller->breakpoints[i];
            if (!breakpoint.enabled) continue;
            breakpoint.state = DebugBreakpointState::Pending;
            breakpoint.location.mapping = DebugMappingState::Unavailable;
            breakpoint.mappingError = mappedError;
            clearBreakpointMapping(breakpoint);
            char message[kDebugMaxMessageBytes] = {};
            copyText(message, sizeof(message), "Pending: ");
            appendText(message, sizeof(message), DebugDwarfErrorName(mapperError));
            setBreakpointMessage(breakpoint, message);
        }
        if (error) *error = mappedError;
        return false;
    }
    if (!DebugDwarfMapperMatchesArtifact(mapper, controller->target.projectRoot,
                                         controller->target.projectId, controller->target.targetProfile,
                                         controller->target.architecture, controller->target.executablePath,
                                         controller->target.artifactSize, controller->target.artifactSha256,
                                         controller->target.projectGeneration)) {
        DebugControllerMarkArtifactStale(controller, "Stale: executable changed");
        controller->error = DebugErrorCode::ArtifactChanged;
        if (error) *error = controller->error;
        return false;
    }
    bool allMapped = true;
    for (uint32_t i = 0; i < controller->breakpointCount; ++i) {
        DebugBreakpoint& breakpoint = controller->breakpoints[i];
        if (!breakpoint.enabled) continue;
        if (breakpoint.location.projectGeneration != controller->target.projectGeneration) {
            breakpoint.state = DebugBreakpointState::Stale;
            breakpoint.mappingError = DebugErrorCode::ProjectGenerationMismatch;
            setBreakpointMessage(breakpoint, "Stale: built project generation differs");
            allMapped = false;
            continue;
        }
        uint64_t addresses[kDebugMaxMappedAddresses] = {};
        uint32_t addressCount = 0;
        uint64_t primary = 0;
        DebugDwarfError mapperError = DebugDwarfError::None;
        const bool mapped = DebugDwarfMapperMapSourceToAddresses(mapper, breakpoint.location.relativePath,
            breakpoint.location.line, addresses, kDebugMaxMappedAddresses, &addressCount, &primary, &mapperError);
        if (!mapped || addressCount == 0) {
            breakpoint.state = DebugBreakpointState::Pending;
            breakpoint.location.mapping = DebugMappingState::Pending;
            breakpoint.mappingError = mapDwarfError(mapperError == DebugDwarfError::None ? DebugDwarfError::LineNotMapped : mapperError);
            clearBreakpointMapping(breakpoint);
            if (mapperError == DebugDwarfError::SourceNotFound)
                setBreakpointMessage(breakpoint, "Pending: source file not in debug line table");
            else
                setBreakpointMessage(breakpoint, "Pending: no executable mapping for this line");
            allMapped = false;
            continue;
        }
        breakpoint.state = DebugBreakpointState::Mapped;
        breakpoint.location.mapping = DebugMappingState::Mapped;
        breakpoint.mappingError = DebugErrorCode::None;
        breakpoint.mappedAddressCount = addressCount;
        for (uint32_t address = 0; address < addressCount; ++address) {
            breakpoint.mappedAddresses[address].valid = true;
            breakpoint.mappedAddresses[address].value = addresses[address];
        }
        breakpoint.location.instructionAddress.valid = true;
        breakpoint.location.instructionAddress.value = primary;
        setMappingMessage(breakpoint, addressCount, primary, mapper->truncated);
    }
    if (error) *error = allMapped ? DebugErrorCode::None : DebugErrorCode::LineNotMapped;
    return allMapped;
}

void DebugControllerMarkSourceGeneration(DebugController* controller, const char* projectId,
                                         const char* sourcePath, uint32_t sourceGeneration) {
    if (!controller || !projectId || !sourcePath) return;
    char relative[kMaxProjectPathBytes] = {};
    const bool absolute = sourcePath[0] != '\0' &&
        (isSlash(sourcePath[0]) || sourcePath[1] == ':');
    if (absolute &&
        !DebugRelativeSourcePath(controller->projectRoot, sourcePath, relative, sizeof(relative))) return;
    if (relative[0] == '\0') copyText(relative, sizeof(relative), sourcePath);
    for (uint32_t i = 0; i < controller->breakpointCount; ++i) {
        DebugBreakpoint& breakpoint = controller->breakpoints[i];
        if (!equalText(breakpoint.projectId, projectId, true) || !PathsEqual(breakpoint.location.relativePath, relative) ||
            breakpoint.location.sourceGeneration == sourceGeneration || breakpoint.state == DebugBreakpointState::Disabled) continue;
        breakpoint.state = DebugBreakpointState::Stale;
        breakpoint.location.mapping = DebugMappingState::Unavailable;
        breakpoint.mappingError = DebugErrorCode::ProjectGenerationMismatch;
        setBreakpointMessage(breakpoint, "Stale: source generation differs from built artifact");
    }
}

bool DebugControllerResolveCurrentStop(DebugController* controller, const DebugDwarfMapper* mapper,
                                       DebugErrorCode* error) {
    if (error) *error = DebugErrorCode::None;
    if (!controller || !mapper || controller->state != DebugSessionState::Paused ||
        !controller->currentInstructionAddress.valid || !DebugDwarfMapperIsReady(mapper)) {
        if (error) *error = DebugErrorCode::SourceMappingUnavailable;
        return false;
    }
    DebugSourceLocation location = {};
    location.instructionAddress = controller->currentInstructionAddress;
    location.projectGeneration = controller->target.projectGeneration;
    location.mapping = DebugMappingState::Mapped;
    DebugDwarfError mappingError = DebugDwarfError::None;
    if (!DebugDwarfMapperMapAddressToSource(mapper, controller->currentInstructionAddress.value,
                                            location.relativePath, sizeof(location.relativePath),
                                            &location.line, &location.column, &mappingError)) {
        if (error) *error = mapDwarfError(mappingError);
        return false;
    }
    if (controller->stopReason == DebugStopReason::Breakpoint) {
        const int breakpointIndex = findBreakpoint(controller, controller->lastBreakpointId);
        if (breakpointIndex < 0 || !PathsEqual(location.relativePath,
                                               controller->breakpoints[breakpointIndex].location.relativePath) ||
            location.line != controller->breakpoints[breakpointIndex].location.line) {
            controller->error = DebugErrorCode::BackendError;
            setMessage(controller, "Breakpoint address resolved to a different source location");
            if (error) *error = controller->error;
            return false;
        }
    }
    controller->currentLocation = location;
    return true;
}

const DebugBreakpoint* DebugControllerBreakpointAt(const DebugController* controller, uint32_t index) {
    return controller && index < controller->breakpointCount ? &controller->breakpoints[index] : nullptr;
}

const DebugEvent* DebugControllerEventAt(const DebugController* controller, uint32_t index) {
    return controller && index < controller->eventCount ? &controller->events[index] : nullptr;
}

} // namespace developer_studio
} // namespace guidexos
