#include "developer_studio_debugger.h"
#include "developer_studio_debug_symbols.h"

namespace guidexos {
namespace developer_studio {
namespace {

struct DebugBreakpointConditionStorage {
    const DebugController* owner;
    uint64_t breakpointId;
    char expression[kDebugWatchMaxExpressionBytes + 1];
    char parseDiagnostic[kDebugWatchMaxDiagnosticBytes];
};

static DebugBreakpointConditionStorage g_conditionStorages[kDebugMaxConditionStorages] = {};

static void releaseConditionStorages(const DebugController* owner) {
    if (!owner) return;
    for (uint32_t i = 0; i < kDebugMaxConditionStorages; ++i) {
        if (g_conditionStorages[i].owner != owner) continue;
        g_conditionStorages[i] = DebugBreakpointConditionStorage();
    }
}

static DebugBreakpointConditionStorage* conditionStorage(const DebugController* owner,
                                                          const DebugBreakpoint& breakpoint) {
    if (!owner || !breakpoint.condition) return nullptr;
    for (uint32_t i = 0; i < kDebugMaxConditionStorages; ++i) {
        DebugBreakpointConditionStorage& storage = g_conditionStorages[i];
        if (storage.owner == owner && storage.breakpointId == breakpoint.id &&
            storage.expression == breakpoint.condition) return &storage;
    }
    return nullptr;
}

static DebugBreakpointConditionStorage* reserveConditionStorage(const DebugController* owner,
                                                                uint64_t breakpointId) {
    if (!owner || breakpointId == 0) return nullptr;
    for (uint32_t i = 0; i < kDebugMaxConditionStorages; ++i) {
        DebugBreakpointConditionStorage& storage = g_conditionStorages[i];
        if (storage.owner) continue;
        storage = DebugBreakpointConditionStorage();
        storage.owner = owner;
        storage.breakpointId = breakpointId;
        return &storage;
    }
    return nullptr;
}

static void releaseConditionStorage(const DebugController* owner, const DebugBreakpoint& breakpoint) {
    DebugBreakpointConditionStorage* storage = conditionStorage(owner, breakpoint);
    if (storage) *storage = DebugBreakpointConditionStorage();
}

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

static void clearStepOver(DebugController* controller, DebugStepOverStatus status,
                          const char* reason) {
    if (!controller) return;
    controller->stepOver.active = false;
    controller->stepOver.status = status;
    controller->stepOver.mode = DebugStepOverMode::None;
    controller->stepOver.temporaryInstalled = false;
    if (reason) copyText(controller->stepOver.reason, sizeof(controller->stepOver.reason), reason);
}

static void clearStepOut(DebugController* controller, DebugStepOutStatus status,
                         const char* reason) {
    if (!controller) return;
    controller->stepOut.active = false;
    controller->stepOut.status = status;
    controller->stepOut.temporaryInstalled = false;
    if (reason) copyText(controller->stepOut.reason, sizeof(controller->stepOut.reason), reason);
}

static bool canonicalAmd64Address(uint64_t address) {
    const uint64_t upper = address >> 48;
    return address != 0 && (upper == 0 || upper == 0xffffu);
}

static bool currentCallStackIsFresh(const DebugController* controller) {
    return controller && controller->callStack.valid && !controller->callStack.stale &&
        controller->callStack.sessionGeneration == controller->sessionGeneration &&
        controller->callStack.processId == controller->processId &&
        controller->callStack.nativeRuntimeId == controller->nativeRuntimeId &&
        controller->callStack.threadId == controller->currentThreadId &&
        controller->callStack.stopGeneration == controller->stopGeneration &&
        equalText(controller->callStack.artifactSha256, controller->target.artifactSha256, false);
}

static void clearCallStack(DebugController* controller) {
    if (!controller) return;
    unsigned char* bytes = reinterpret_cast<unsigned char*>(&controller->callStack);
    for (uint32_t i = 0; i < sizeof(DebugCallStack); ++i) bytes[i] = 0;
}

static void clearVariableView(DebugDwarfVariableView* view) {
    if (!view) return;
    unsigned char* bytes = reinterpret_cast<unsigned char*>(view);
    for (uint32_t i = 0; i < sizeof(DebugDwarfVariableView); ++i) bytes[i] = 0;
}

static void clearStoppedContext(DebugController* controller) {
    if (!controller) return;
    controller->currentInstructionAddress = DebugAddress();
    controller->currentLocation = DebugSourceLocation();
    controller->currentThreadId = 0;
    controller->reportedInstructionPointer = 0;
    controller->stopGeneration = 0;
    controller->stoppedContext = DebugRegisterContext();
    controller->conditionResumePending = false;
    controller->conditionEvaluationPending = false;
    clearVariableView(&controller->variables);
    if (controller->watches) DebugWatchCollectionMarkStale(controller->watches);
    clearCallStack(controller);
}

static int findBreakpoint(const DebugController* controller, uint64_t breakpointId) {
    if (!controller || breakpointId == 0) return -1;
    for (uint32_t i = 0; i < controller->breakpointCount; ++i)
        if (controller->breakpoints[i].id == breakpointId) return static_cast<int>(i);
    return -1;
}

static int findBreakpointAtAddress(const DebugController* controller, uint64_t address,
                                   uint64_t bindingId) {
    if (!controller || address == 0) return -1;
    for (uint32_t i = 0; i < controller->breakpointCount; ++i) {
        const DebugBreakpoint& breakpoint = controller->breakpoints[i];
        if (!breakpoint.enabled || (bindingId != 0 && breakpoint.backendBindingId != bindingId)) continue;
        bool ownsAddress = breakpoint.location.instructionAddress.valid &&
            breakpoint.location.instructionAddress.value == address;
        for (uint32_t j = 0; !ownsAddress && j < breakpoint.mappedAddressCount; ++j)
            ownsAddress = breakpoint.mappedAddresses[j].valid && breakpoint.mappedAddresses[j].value == address;
        if (ownsAddress) return static_cast<int>(i);
    }
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

static void publishBreakpointHit(DebugController* controller) {
    if (!controller || controller->lastBreakpointId == 0) return;
    appendEvent(controller, DebugEventKind::BreakpointHit, controller->state,
                DebugStopReason::Breakpoint, "Owned breakpoint hit");
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
        controller->backendExecutionState = snapshot.executionState == DebugBackendExecutionState::PausedAtStepOver ?
            DebugBackendExecutionState::PausedAtStepOver : DebugBackendExecutionState::PausedAtSourceStep;
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
        clearStepOver(controller, DebugStepOverStatus::Cancelled, "Step over ended because the target exited");
        clearStepOut(controller, DebugStepOutStatus::Cancelled, "Step out ended because the target exited");
    } else if (snapshot.state == DebugSessionState::Failed && previous != DebugSessionState::Failed) {
        appendEvent(controller, DebugEventKind::Failed, snapshot.state, snapshot.stopReason, controller->lastMessage);
        controller->active = false;
        clearStoppedContext(controller);
        controller->lastBreakpointId = 0;
        controller->backendExecutionState = DebugBackendExecutionState::None;
        clearSourceStep(controller, DebugSourceStepStatus::Failed, "Source step ended because the session failed");
        clearStepOver(controller, DebugStepOverStatus::Failed, "Step over ended because the session failed");
        clearStepOut(controller, DebugStepOutStatus::Failed, "Step out ended because the session failed");
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
    controller->conditionEvaluationPending = true;
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
                0, 0, controller->target.artifactSha256, 0, 0, false, 0, 0, &restored);
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
                               0, 0, controller->target.artifactSha256, 0, 0, false, 0, 0, &released)) {
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

static bool readStepOverInstruction(const DebugController* controller, const DebugBackend& backend,
                                    uint64_t address, uint8_t* bytes, uint32_t* byteCount,
                                    DebugAmd64Instruction* instruction, DebugErrorCode* error) {
    if (error) *error = DebugErrorCode::None;
    if (!controller || !backend.readMemory || !bytes || !byteCount || !instruction || address == 0) {
        if (error) *error = DebugErrorCode::InstructionReadFailed;
        return false;
    }
    uint32_t returned = 0;
    if (!backend.readMemory(backend.userData, controller->sessionGeneration, controller->processId,
                            controller->nativeRuntimeId, address, bytes, kDebugMaxInstructionBytes, &returned) ||
        returned == 0) {
        if (error) *error = DebugErrorCode::InstructionReadFailed;
        return false;
    }
    if (byteCount) *byteCount = returned;
    if (!DebugDecodeAmd64Instruction(bytes, returned, address, 0, instruction)) {
        if (error) *error = DebugErrorCode::UnsupportedCallEncoding;
        return false;
    }
    return true;
}

static bool removeStepOverBreakpoint(DebugController* controller, const DebugBackend& backend,
                                     const DebugStepOverOperation& operation) {
    if (!controller || !backend.debugCommand || operation.temporaryBreakpointId == 0) return false;
    HostedDebugResult result = {};
    return backend.debugCommand(backend.userData, HostedDebugCommand::RemoveSoftwareBreakpointOwner,
        controller->debugHandle, controller->sessionGeneration, controller->processId,
        controller->nativeRuntimeId, operation.temporaryBreakpointId, operation.returnAddress,
        controller->target.artifactSha256, 0, 0, false, 0, 0, &result);
}

static bool startStepOverCall(DebugController* controller, const DebugBackend& backend,
                              DebugStepOverOperation* operation, const DebugRegisterContext& context,
                              uint64_t callAddress, uint64_t returnAddress, DebugErrorCode* error) {
    if (error) *error = DebugErrorCode::None;
    if (!controller || !operation || !backend.bindSoftwareBreakpoint || !backend.stepOverCall ||
        !backend.debugCommand || operation->callCount >= operation->maxCallCount) {
        if (error) *error = DebugErrorCode::StepOverLimitExceeded;
        return false;
    }
    uint64_t temporaryId = controller->nextTemporaryBreakpointId++;
    if (temporaryId == 0 || temporaryId < 0x8000000000000001ull)
        temporaryId = controller->nextTemporaryBreakpointId++;
    DebugBreakpoint temporary = {};
    temporary.id = temporaryId;
    temporary.enabled = true;
    temporary.state = DebugBreakpointState::Mapped;
    temporary.sessionGeneration = controller->sessionGeneration;
    temporary.location.instructionAddress.valid = true;
    temporary.location.instructionAddress.value = returnAddress;
    DebugBackendBinding binding = {};
    if (!backend.bindSoftwareBreakpoint(backend.userData, controller->target, controller->sessionGeneration,
                                        controller->processId, controller->nativeRuntimeId, temporary, &binding) ||
        !binding.accepted) {
        if (error) *error = DebugErrorCode::UnsupportedCallEncoding;
        setMessage(controller, binding.message[0] ? binding.message : "Step over temporary breakpoint bind failed");
        return false;
    }
    operation->mode = DebugStepOverMode::TemporaryReturnBreakpoint;
    operation->callAddress = callAddress;
    operation->returnAddress = returnAddress;
    operation->temporaryBreakpointId = temporaryId;
    operation->temporaryBindingId = binding.bindingId;
    operation->originalByte = binding.originalByte;
    operation->installedByte = binding.installedByte;
    operation->originalByteValid = binding.originalByteValid;
    operation->temporaryInstalled = true;
    ++operation->callCount;
    appendEvent(controller, DebugEventKind::StepOverCallDetected, DebugSessionState::Stepping,
                DebugStopReason::Step, "Step over call detected");
    appendEvent(controller, DebugEventKind::StepOverTemporaryBreakpointBound, DebugSessionState::Stepping,
                DebugStopReason::Step, "Step over temporary return breakpoint bound");
    if (!backend.stepOverCall(backend.userData, controller->sessionGeneration, context,
                              callAddress, returnAddress, temporaryId)) {
        removeStepOverBreakpoint(controller, backend, *operation);
        operation->temporaryInstalled = false;
        if (error) *error = DebugErrorCode::StepOverFailed;
        setMessage(controller, "Step over call resume was rejected");
        return false;
    }
    controller->state = DebugSessionState::Stepping;
    controller->stopReason = DebugStopReason::None;
    controller->backendExecutionState = DebugBackendExecutionState::StepOverPending;
    clearStoppedContext(controller);
    setMessage(controller, "Step over call running to its return address");
    return true;
}

static bool startStepOverSourceInstruction(DebugController* controller, const DebugBackend& backend,
                                           DebugStepOverOperation* operation,
                                           const DebugRegisterContext& context, DebugErrorCode* error) {
    if (error) *error = DebugErrorCode::None;
    if (!controller || !operation || !backend.stepInstruction) {
        if (error) *error = DebugErrorCode::StepOverFailed;
        return false;
    }
    const bool fromBreakpoint = controller->stopReason == DebugStopReason::Breakpoint &&
        controller->backendExecutionState == DebugBackendExecutionState::PausedAtBreakpoint;
    uint64_t breakpointId = 0;
    uint64_t bindingId = 0;
    uint64_t address = 0;
    bool reinstall = false;
    if (fromBreakpoint) {
        const int breakpointIndex = findBreakpoint(controller, controller->lastBreakpointId);
        if (breakpointIndex < 0) {
            if (error) *error = DebugErrorCode::StaleStopContext;
            return false;
        }
        const DebugBreakpoint& breakpoint = controller->breakpoints[breakpointIndex];
        breakpointId = breakpoint.id;
        bindingId = breakpoint.backendBindingId;
        address = breakpoint.location.instructionAddress.value;
        reinstall = breakpoint.enabled;
        operation->breakpointId = breakpointId;
        operation->bindingId = bindingId;
        operation->breakpointAddress = address;
        operation->reinstallBreakpoint = reinstall;
    }
    if (!backend.stepInstruction(backend.userData, controller->sessionGeneration, context,
                                breakpointId, bindingId, address, fromBreakpoint && reinstall)) {
        if (error) *error = DebugErrorCode::StepOverFailed;
        setMessage(controller, "Step over source instruction request was rejected");
        return false;
    }
    operation->mode = DebugStepOverMode::SourceSingleStep;
    controller->state = DebugSessionState::Stepping;
    controller->stopReason = DebugStopReason::None;
    controller->backendExecutionState = DebugBackendExecutionState::StepOverPending;
    clearStoppedContext(controller);
    setMessage(controller, "Step over source instruction pending");
    return true;
}

static void publishStepOverStop(DebugController* controller, const DebugBackendSnapshot& snapshot,
                                const DebugSourceLocation& location, DebugStepOverStatus status,
                                const char* message) {
    if (!controller) return;
    controller->state = DebugSessionState::Paused;
    controller->active = true;
    controller->stopReason = DebugStopReason::Step;
    controller->backendExecutionState = DebugBackendExecutionState::PausedAtStepOver;
    controller->currentInstructionAddress = snapshot.targetAddress;
    if (!controller->currentInstructionAddress.valid) {
        controller->currentInstructionAddress.valid = true;
        controller->currentInstructionAddress.value = snapshot.registerContext.rip;
    }
    controller->reportedInstructionPointer = snapshot.instructionPointer == 0 ?
        snapshot.registerContext.rip : snapshot.instructionPointer;
    controller->currentThreadId = snapshot.threadId;
    controller->stopGeneration = snapshot.stopGeneration != 0 ? snapshot.stopGeneration :
        (controller->stopGeneration == UINT64_MAX ? 1 : controller->stopGeneration + 1);
    controller->stoppedContext = snapshot.registerContext;
    controller->stoppedContext.stopGeneration = controller->stopGeneration;
    controller->currentLocation = location;
    controller->currentLocation.instructionAddress = controller->currentInstructionAddress;
    controller->stepOver.lastAddress = controller->currentInstructionAddress.value;
    controller->stepOver.lastSourceLocation = location;
    clearStepOver(controller, status, message);
    setMessage(controller, message);
    appendEvent(controller, status == DebugStepOverStatus::LimitExceeded ?
        DebugEventKind::StepOverLimitExceeded : DebugEventKind::StepOverCompleted,
        controller->state, DebugStopReason::Step, message);
}

static bool processStepOverSourceTrap(DebugController* controller, const DebugBackend& backend,
                                      const DebugDwarfMapper* mapper, const DebugBackendSnapshot& snapshot) {
    if (!controller || !controller->stepOver.active || !snapshot.singleStepTrap ||
        snapshot.singleStepKind != static_cast<uint32_t>(HostedDebugSingleStepKind::UserSource)) return false;
    DebugStepOverOperation& operation = controller->stepOver;
    if (snapshot.sessionGeneration != 0 && snapshot.sessionGeneration != operation.sessionGeneration) return false;
    if (snapshot.processId != operation.processId || snapshot.nativeRuntimeId != operation.nativeRuntimeId ||
        snapshot.threadId != operation.threadId || !DebugRegisterContextIsValid(snapshot.registerContext)) return false;
    ++operation.instructionCount;
    operation.lastAddress = snapshot.registerContext.rip;
    DebugSourceLocation location = {};
    const bool mapped = mapSourceStepLocation(controller, mapper, snapshot.registerContext.rip, &location);
    operation.lastSourceLocation = location;
    if (mapped && !sameSourceStepLocation(operation.startingSourceLocation, location)) {
        publishStepOverStop(controller, snapshot, location, DebugStepOverStatus::Completed,
                             "Step over completed at a new source location");
        return true;
    }
    if (operation.instructionCount >= operation.maxInstructionCount) {
        controller->error = DebugErrorCode::StepOverLimitExceeded;
        publishStepOverStop(controller, snapshot, mapped ? location : DebugSourceLocation(),
                            DebugStepOverStatus::LimitExceeded,
                            "StepOverLimitExceeded: bounded instruction-step limit reached");
        return true;
    }
    uint8_t bytes[kDebugMaxInstructionBytes] = {};
    uint32_t byteCount = 0;
    DebugAmd64Instruction instruction = {};
    DebugErrorCode readError = DebugErrorCode::None;
    if (!readStepOverInstruction(controller, backend, snapshot.registerContext.rip, bytes, &byteCount,
                                 &instruction, &readError)) {
        controller->error = readError;
        clearStepOver(controller, DebugStepOverStatus::Failed, "Step over instruction inspection failed");
        setMessage(controller, controller->stepOver.reason);
        return false;
    }
    (void)byteCount;
    if (instruction.kind == DebugAmd64InstructionKind::Call) {
        return startStepOverCall(controller, backend, &operation, snapshot.registerContext,
                                 snapshot.registerContext.rip, instruction.returnAddress, &controller->error);
    }
    return startStepOverSourceInstruction(controller, backend, &operation, snapshot.registerContext,
                                          &controller->error);
}

static bool processStepOverInternalTrap(DebugController* controller, const DebugBackend& backend,
                                        const DebugDwarfMapper* mapper, const DebugBackendSnapshot& snapshot) {
    if (!controller || !controller->stepOver.active || !snapshot.breakpointTrap ||
        !snapshot.internalBreakpointTrap || !snapshot.targetAddress.valid) return false;
    DebugStepOverOperation& operation = controller->stepOver;
    if ((snapshot.sessionGeneration != 0 && snapshot.sessionGeneration != operation.sessionGeneration) ||
        snapshot.processId != operation.processId || snapshot.nativeRuntimeId != operation.nativeRuntimeId ||
        snapshot.threadId != operation.threadId || snapshot.targetAddress.value != operation.returnAddress ||
        snapshot.breakpointBindingId != operation.temporaryBindingId) {
        controller->error = DebugErrorCode::StaleStepOver;
        clearStepOver(controller, DebugStepOverStatus::Failed, "Rejected stale Step Over return trap");
        return false;
    }
    appendEvent(controller, DebugEventKind::StepOverReturnReached, DebugSessionState::Stepping,
                DebugStopReason::Step, "Step over return breakpoint reached");
    if (!removeStepOverBreakpoint(controller, backend, operation)) {
        controller->error = DebugErrorCode::StepOverFailed;
        clearStepOver(controller, DebugStepOverStatus::Failed, "Step over temporary breakpoint cleanup failed");
        return false;
    }
    operation.temporaryInstalled = false;
    const int userBreakpointIndex = findBreakpointAtAddress(controller, snapshot.targetAddress.value,
                                                             snapshot.breakpointBindingId);
    if (userBreakpointIndex >= 0) {
        DebugBackendSnapshot userSnapshot = snapshot;
        userSnapshot.internalBreakpointTrap = false;
        userSnapshot.stopReason = DebugStopReason::Breakpoint;
        userSnapshot.executionState = DebugBackendExecutionState::PausedAtBreakpoint;
        controller->state = DebugSessionState::Paused;
        if (!applyOwnedBreakpointTrap(controller, userSnapshot)) return false;
        clearStepOver(controller, DebugStepOverStatus::Cancelled,
                      "Step over interrupted by a user breakpoint at the return address");
        return true;
    }
    DebugSourceLocation location = {};
    const bool mapped = mapSourceStepLocation(controller, mapper, snapshot.targetAddress.value, &location);
    operation.lastAddress = snapshot.targetAddress.value;
    operation.lastSourceLocation = location;
    if (mapped && !sameSourceStepLocation(operation.startingSourceLocation, location)) {
        publishStepOverStop(controller, snapshot, location, DebugStepOverStatus::Completed,
                            "Step over completed after the call returned");
        return true;
    }
    if (operation.instructionCount >= operation.maxInstructionCount) {
        controller->error = DebugErrorCode::StepOverLimitExceeded;
        publishStepOverStop(controller, snapshot, mapped ? location : DebugSourceLocation(),
                            DebugStepOverStatus::LimitExceeded,
                            "StepOverLimitExceeded: bounded instruction-step limit reached");
        return true;
    }
    uint8_t bytes[kDebugMaxInstructionBytes] = {};
    uint32_t byteCount = 0;
    DebugAmd64Instruction instruction = {};
    if (!readStepOverInstruction(controller, backend, snapshot.targetAddress.value, bytes, &byteCount,
                                 &instruction, &controller->error)) {
        clearStepOver(controller, DebugStepOverStatus::Failed, "Step over return instruction inspection failed");
        return false;
    }
    (void)byteCount;
    if (instruction.kind == DebugAmd64InstructionKind::Call)
        return startStepOverCall(controller, backend, &operation, snapshot.registerContext,
                                 snapshot.targetAddress.value, instruction.returnAddress, &controller->error);
    return startStepOverSourceInstruction(controller, backend, &operation, snapshot.registerContext,
                                          &controller->error);
}

static bool removeStepOutBreakpoint(DebugController* controller, const DebugBackend& backend,
                                    const DebugStepOutOperation& operation) {
    if (!controller || !backend.debugCommand || operation.temporaryBreakpointId == 0) return false;
    HostedDebugResult result = {};
    return backend.debugCommand(backend.userData, HostedDebugCommand::RemoveSoftwareBreakpointOwner,
        controller->debugHandle, controller->sessionGeneration, controller->processId,
        controller->nativeRuntimeId, operation.temporaryBreakpointId, operation.rawReturnAddress,
        controller->target.artifactSha256, 0, 0, false, 0, 0, &result);
}

static void publishStepOutStop(DebugController* controller, const DebugBackendSnapshot& snapshot,
                               const DebugSourceLocation& location, DebugStepOutStatus status,
                               const char* message) {
    if (!controller) return;
    controller->state = DebugSessionState::Paused;
    controller->active = true;
    controller->stopReason = DebugStopReason::Step;
    controller->backendExecutionState = DebugBackendExecutionState::PausedAtStepOut;
    controller->currentInstructionAddress = snapshot.targetAddress;
    if (!controller->currentInstructionAddress.valid) {
        controller->currentInstructionAddress.valid = true;
        controller->currentInstructionAddress.value = snapshot.registerContext.rip;
    }
    controller->reportedInstructionPointer = snapshot.instructionPointer == 0 ?
        snapshot.registerContext.rip : snapshot.instructionPointer;
    controller->currentThreadId = snapshot.threadId;
    controller->stopGeneration = snapshot.stopGeneration != 0 ? snapshot.stopGeneration :
        (controller->stopGeneration == UINT64_MAX ? 1 : controller->stopGeneration + 1);
    controller->stoppedContext = snapshot.registerContext;
    controller->stoppedContext.stopGeneration = controller->stopGeneration;
    controller->currentLocation = location;
    controller->currentLocation.instructionAddress = controller->currentInstructionAddress;
    clearStepOut(controller, status, message);
    setMessage(controller, message);
    appendEvent(controller, DebugEventKind::StepOutCompleted, controller->state,
                DebugStopReason::Step, message);
}

static bool processStepOutInternalTrap(DebugController* controller, const DebugBackend& backend,
                                       const DebugDwarfMapper* mapper,
                                       const DebugBackendSnapshot& snapshot) {
    if (!controller || !controller->stepOut.active || !snapshot.breakpointTrap ||
        !snapshot.internalBreakpointTrap || !snapshot.targetAddress.valid) return false;
    DebugStepOutOperation& operation = controller->stepOut;
    if ((snapshot.sessionGeneration != 0 && snapshot.sessionGeneration != operation.sessionGeneration) ||
        snapshot.processId != operation.processId || snapshot.nativeRuntimeId != operation.nativeRuntimeId ||
        snapshot.threadId != operation.threadId || snapshot.targetAddress.value != operation.rawReturnAddress ||
        snapshot.breakpointBindingId != operation.temporaryBindingId ||
        (snapshot.internalBreakpointPurpose != 0 &&
         snapshot.internalBreakpointPurpose != static_cast<uint32_t>(HostedDebugInternalBreakpointPurpose::StepOut))) {
        controller->error = DebugErrorCode::StaleStepOut;
        clearStepOut(controller, DebugStepOutStatus::Failed, "Rejected stale Step Out return trap");
        return false;
    }
    appendEvent(controller, DebugEventKind::StepOutReturnReached, DebugSessionState::Stepping,
                DebugStopReason::Step, "Step Out return breakpoint reached");
    if (!removeStepOutBreakpoint(controller, backend, operation)) {
        controller->error = DebugErrorCode::StepOutFailed;
        clearStepOut(controller, DebugStepOutStatus::Failed,
                     "Step Out temporary breakpoint cleanup failed");
        return false;
    }
    operation.temporaryInstalled = false;
    const int userBreakpointIndex = findBreakpointAtAddress(controller, snapshot.targetAddress.value,
                                                             snapshot.breakpointBindingId);
    if (userBreakpointIndex >= 0) {
        DebugBackendSnapshot userSnapshot = snapshot;
        userSnapshot.internalBreakpointTrap = false;
        userSnapshot.internalBreakpointPurpose = static_cast<uint32_t>(HostedDebugInternalBreakpointPurpose::None);
        userSnapshot.stopReason = DebugStopReason::Breakpoint;
        userSnapshot.executionState = DebugBackendExecutionState::PausedAtBreakpoint;
        controller->state = DebugSessionState::Paused;
        if (!applyOwnedBreakpointTrap(controller, userSnapshot)) return false;
        clearStepOut(controller, DebugStepOutStatus::Cancelled,
                     "Step Out stopped by a persistent user breakpoint at the return address");
        if (mapper && backend.readTargetMemory)
            DebugControllerBuildCallStack(controller, backend, mapper, nullptr);
        return true;
    }
    DebugSourceLocation location = {};
    const bool mapped = operation.callerLookupAddress != 0 &&
        mapSourceStepLocation(controller, mapper, operation.callerLookupAddress, &location);
    operation.callerSource = location;
    publishStepOutStop(controller, snapshot, mapped ? location : DebugSourceLocation(),
                       DebugStepOutStatus::Completed,
                       "Step Out completed at the caller return address");
    if (!mapper || !backend.readTargetMemory ||
        !DebugControllerBuildCallStack(controller, backend, mapper, nullptr)) {
        controller->error = DebugErrorCode::StaleStopContext;
        setMessage(controller, "Step Out completed; Call Stack refresh was unavailable");
    }
    return true;
}

} // namespace

bool DebugDecodeAmd64Instruction(const uint8_t* bytes, uint32_t byteCount,
                                 uint64_t address, uint64_t executableEnd,
                                 DebugAmd64Instruction* instruction) {
    if (!instruction) return false;
    *instruction = DebugAmd64Instruction();
    instruction->kind = DebugAmd64InstructionKind::Invalid;
    if (!bytes || byteCount == 0 || address == 0) return false;

    uint32_t offset = 0;
    while (offset < byteCount) {
        const uint8_t value = bytes[offset];
        const bool prefix = value == 0x66 || value == 0x67 || value == 0xF0 ||
            value == 0xF2 || value == 0xF3 || (value >= 0x40 && value <= 0x4F);
        if (!prefix) break;
        if (++offset > 15) return false;
    }
    if (offset >= byteCount) return false;
    const uint8_t opcode = bytes[offset];
    uint32_t length = 0;
    if (opcode == 0xE8) {
        length = offset + 5;
        if (byteCount < length) return false;
        instruction->kind = DebugAmd64InstructionKind::Call;
    } else if (opcode == 0xFF) {
        if (byteCount <= offset + 1) return false;
        const uint8_t modrm = bytes[offset + 1];
        if (((modrm >> 3) & 7u) != 2u) {
            instruction->kind = DebugAmd64InstructionKind::Unsupported;
            return true;
        }
        const uint8_t mod = static_cast<uint8_t>(modrm >> 6);
        const uint8_t rm = static_cast<uint8_t>(modrm & 7u);
        length = offset + 2;
        if (mod != 3 && rm == 4) {
            if (byteCount <= length) return false;
            const uint8_t sib = bytes[length++];
            const uint8_t base = static_cast<uint8_t>(sib & 7u);
            if (mod == 0 && base == 5) length += 4;
        } else if (mod == 0 && rm == 5) {
            length += 4;
        }
        if (mod == 1) length += 1;
        else if (mod == 2) length += 4;
        if (length > byteCount) return false;
        instruction->kind = DebugAmd64InstructionKind::Call;
    } else {
        instruction->kind = DebugAmd64InstructionKind::NonCall;
        return true;
    }
    if (length == 0 || address > UINT64_MAX - length) return false;
    const uint64_t returnAddress = address + length;
    if (executableEnd != 0 && returnAddress > executableEnd) return false;
    instruction->instructionLength = length;
    instruction->returnAddress = returnAddress;
    return true;
}

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
    case DebugErrorCode::InstructionReadFailed: return "instruction_read_failed";
    case DebugErrorCode::UnsupportedCallEncoding: return "unsupported_call_encoding";
    case DebugErrorCode::StepOverLimitExceeded: return "step_over_limit_exceeded";
    case DebugErrorCode::StepOverFailed: return "step_over_failed";
    case DebugErrorCode::StaleStepOver: return "stale_step_over";
    case DebugErrorCode::NoCallerFrame: return "no_caller_frame";
    case DebugErrorCode::InvalidReturnAddress: return "invalid_return_address";
    case DebugErrorCode::StepOutFailed: return "step_out_failed";
    case DebugErrorCode::StaleStepOut: return "stale_step_out";
    case DebugErrorCode::InvalidCondition: return "invalid_condition";
    case DebugErrorCode::ConditionError: return "condition_error";
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

const char* DebugBreakpointConditionEvaluationName(DebugBreakpointConditionEvaluation evaluation) {
    switch (evaluation) {
    case DebugBreakpointConditionEvaluation::NotEvaluated: return "NotEvaluated";
    case DebugBreakpointConditionEvaluation::True: return "True";
    case DebugBreakpointConditionEvaluation::False: return "False";
    case DebugBreakpointConditionEvaluation::Error: return "Error";
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
    case DebugEventKind::StepOverStarted: return "Step over started";
    case DebugEventKind::StepOverCallDetected: return "Step over call detected";
    case DebugEventKind::StepOverTemporaryBreakpointBound: return "Step over temporary breakpoint bound";
    case DebugEventKind::StepOverReturnReached: return "Step over return reached";
    case DebugEventKind::StepOverCompleted: return "Step over completed";
    case DebugEventKind::StepOverInterrupted: return "Step over interrupted";
    case DebugEventKind::StepOverLimitExceeded: return "Step over limit exceeded";
    case DebugEventKind::StepOutStarted: return "Step out started";
    case DebugEventKind::StepOutReturnReached: return "Step out return reached";
    case DebugEventKind::StepOutCompleted: return "Step out completed";
    case DebugEventKind::StepOutInterrupted: return "Step out interrupted";
    case DebugEventKind::BreakpointConditionFalse: return "Breakpoint condition false";
    case DebugEventKind::BreakpointConditionError: return "Breakpoint condition error";
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
    releaseConditionStorages(controller);
    // Do not value-initialize this large bounded controller through a stack
    // temporary: Native ELF application stacks are intentionally small.
    unsigned char* bytes = reinterpret_cast<unsigned char*>(controller);
    for (uint32_t i = 0; i < sizeof(DebugController); ++i) bytes[i] = 0;
    controller->state = DebugSessionState::Idle;
    controller->error = DebugErrorCode::None;
    controller->nextBreakpointId = 1;
    controller->nextTemporaryBreakpointId = 0x8000000000000001ull;
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
    for (uint32_t i = 0; i < controller->breakpointCount; ++i)
        releaseConditionStorage(controller, controller->breakpoints[i]);
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
    controller->conditionResumePending = false;
    controller->conditionEvaluationPending = false;
    if (controller->watches) DebugWatchCollectionMarkStale(controller->watches);
    clearCallStack(controller);
    controller->currentInstructionAddress = DebugAddress();
    controller->currentLocation = DebugSourceLocation();
    controller->currentThreadId = 0;
    controller->reportedInstructionPointer = 0;
    controller->lastBreakpointId = 0;
    if (controller->nextTemporaryBreakpointId < 0x8000000000000001ull)
        controller->nextTemporaryBreakpointId = 0x8000000000000001ull;
    controller->sourceStep = DebugSourceStepOperation();
    controller->stepOver = DebugStepOverOperation();
    controller->stepOut = DebugStepOutOperation();
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

static bool processPendingBreakpointCondition(DebugController* controller,
                                              const DebugBackend& backend,
                                              const DebugDwarfMapper* mapper) {
    if (!controller || !controller->conditionEvaluationPending) return true;
    const int breakpointIndex = findBreakpoint(controller, controller->lastBreakpointId);
    if (breakpointIndex < 0) {
        controller->conditionEvaluationPending = false;
        controller->error = DebugErrorCode::ConditionError;
        setMessage(controller, "ConditionError: breakpoint identity is stale");
        appendEvent(controller, DebugEventKind::BreakpointConditionError, controller->state,
                    DebugStopReason::Breakpoint, controller->lastMessage);
        return true;
    }

    DebugBreakpointConditionDecision decision = DebugBreakpointConditionDecision::Error;
    if (!DebugControllerEvaluateBreakpointCondition(controller, backend, mapper, &decision))
        decision = DebugBreakpointConditionDecision::Error;

    controller->conditionEvaluationPending = false;
    if (decision == DebugBreakpointConditionDecision::NoCondition ||
        decision == DebugBreakpointConditionDecision::True) {
        publishBreakpointHit(controller);
        return true;
    }
    if (decision == DebugBreakpointConditionDecision::Error) {
        appendEvent(controller, DebugEventKind::BreakpointConditionError, controller->state,
                    DebugStopReason::Breakpoint, controller->lastMessage);
        return true;
    }

    appendEvent(controller, DebugEventKind::BreakpointConditionFalse, controller->state,
                DebugStopReason::Breakpoint, "Conditional breakpoint filtered; continuing");
    DebugErrorCode continueError = DebugErrorCode::None;
    if (!DebugControllerContinue(controller, backend, &continueError)) {
        controller->error = continueError == DebugErrorCode::None ? DebugErrorCode::BackendError : continueError;
        setMessage(controller, "Condition false; automatic resume was rejected; target remains stopped");
        appendEvent(controller, DebugEventKind::BreakpointConditionError, controller->state,
                    DebugStopReason::Breakpoint, controller->lastMessage);
        return true;
    }
    controller->conditionResumePending = true;
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
    if (controller->conditionEvaluationPending) {
        const int breakpointIndex = findBreakpoint(controller, controller->lastBreakpointId);
        if (breakpointIndex >= 0 && (!controller->breakpoints[breakpointIndex].condition ||
                                     controller->breakpoints[breakpointIndex].condition[0] == '\0')) {
            controller->conditionEvaluationPending = false;
            publishBreakpointHit(controller);
        }
    }
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
    if (snapshot.breakpointTrap && snapshot.internalBreakpointTrap) {
        if (controller->stepOut.active) {
            if (!processStepOutInternalTrap(controller, backend, mapper, snapshot)) {
                if (controller->error == DebugErrorCode::None) controller->error = DebugErrorCode::StaleStepOut;
                return false;
            }
            return processPendingBreakpointCondition(controller, backend, mapper);
        }
        if (controller->stepOver.active) {
            if (!processStepOverInternalTrap(controller, backend, mapper, snapshot)) {
                if (controller->error == DebugErrorCode::None) controller->error = DebugErrorCode::StaleStepOver;
                return false;
            }
            return processPendingBreakpointCondition(controller, backend, mapper);
        }
        // The physical trap remains held while an internal Step Over stop is
        // presented to the user. Do not re-publish it on every UI poll.
        if (controller->state == DebugSessionState::Paused &&
            (controller->backendExecutionState == DebugBackendExecutionState::PausedAtStepOver ||
             controller->stopReason == DebugStopReason::Breakpoint)) return true;
        controller->error = DebugErrorCode::StaleStepOver;
        setMessage(controller, "Rejected stale internal Step Over trap");
        return false;
    }
    if (snapshot.breakpointTrap && controller->stepOut.active) {
        if (controller->stepOut.temporaryInstalled &&
            !removeStepOutBreakpoint(controller, backend, controller->stepOut)) {
            controller->error = DebugErrorCode::StepOutFailed;
            clearStepOut(controller, DebugStepOutStatus::Failed,
                         "Step Out cleanup failed after a user breakpoint");
            return false;
        }
        controller->stepOut.temporaryInstalled = false;
        appendEvent(controller, DebugEventKind::StepOutInterrupted, DebugSessionState::Paused,
                    DebugStopReason::Breakpoint, "Step Out interrupted by a user breakpoint");
        clearStepOut(controller, DebugStepOutStatus::Cancelled,
                     "Step Out interrupted by a user breakpoint");
    }
    if (snapshot.executionState == DebugBackendExecutionState::SingleStepPending &&
        controller->stepOut.active) {
        controller->state = DebugSessionState::Stepping;
        controller->active = true;
        controller->stopReason = DebugStopReason::None;
        controller->backendExecutionState = DebugBackendExecutionState::StepOutPending;
        clearStoppedContext(controller);
        return true;
    }
    if (snapshot.breakpointTrap && controller->stepOver.active) {
        if (controller->stepOver.temporaryInstalled &&
            !removeStepOverBreakpoint(controller, backend, controller->stepOver)) {
            controller->error = DebugErrorCode::StepOverFailed;
            clearStepOver(controller, DebugStepOverStatus::Failed,
                          "Step over cleanup failed after a user breakpoint");
            return false;
        }
        controller->stepOver.temporaryInstalled = false;
        appendEvent(controller, DebugEventKind::StepOverInterrupted, DebugSessionState::Paused,
                    DebugStopReason::Breakpoint, "Step over interrupted by a user breakpoint");
        clearStepOver(controller, DebugStepOverStatus::Cancelled,
                      "Step over interrupted by a user breakpoint");
    }
    if (snapshot.singleStepTrap &&
        snapshot.singleStepKind == static_cast<uint32_t>(HostedDebugSingleStepKind::UserSource)) {
        if (controller->stepOver.active) {
            if (!processStepOverSourceTrap(controller, backend, mapper, snapshot)) {
                if (controller->error == DebugErrorCode::None) controller->error = DebugErrorCode::StepOverFailed;
                return false;
            }
            return true;
        }
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
    if (!DebugControllerApplySnapshot(controller, controller->sessionGeneration, snapshot)) return false;
    return processPendingBreakpointCondition(controller, backend, mapper);
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
    clearStepOver(controller, DebugStepOverStatus::Cancelled, "Step over cancelled by stop request");
    clearStepOut(controller, DebugStepOutStatus::Cancelled, "Step out cancelled by stop request");
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
        return (controller->backendExecutionState == DebugBackendExecutionState::PausedAtSourceStep ||
                controller->backendExecutionState == DebugBackendExecutionState::PausedAtStepOver ||
                controller->backendExecutionState == DebugBackendExecutionState::PausedAtStepOut) &&
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
         (controller->backendExecutionState == DebugBackendExecutionState::PausedAtSourceStep ||
          controller->backendExecutionState == DebugBackendExecutionState::PausedAtStepOver ||
          controller->backendExecutionState == DebugBackendExecutionState::PausedAtStepOut));
}

bool DebugControllerCanStepOver(const DebugController* controller) {
    if (!controller || !controller->active || controller->state != DebugSessionState::Paused ||
        !controller->capabilities.canStepOver || !DebugRegisterContextIsValid(controller->stoppedContext) ||
        !controller->currentInstructionAddress.valid || !controller->currentLocation.relativePath[0] ||
        controller->currentLocation.line == 0 || controller->sourceStep.active || controller->stepOver.active)
        return false;
    return (controller->stopReason == DebugStopReason::Breakpoint &&
            controller->backendExecutionState == DebugBackendExecutionState::PausedAtBreakpoint) ||
        (controller->stopReason == DebugStopReason::Step &&
         (controller->backendExecutionState == DebugBackendExecutionState::PausedAtSourceStep ||
          controller->backendExecutionState == DebugBackendExecutionState::PausedAtStepOver ||
          controller->backendExecutionState == DebugBackendExecutionState::PausedAtStepOut));
}

bool DebugControllerCanStepOut(const DebugController* controller) {
    if (!controller || !controller->active || controller->state != DebugSessionState::Paused ||
        !controller->capabilities.canStepOut || !DebugRegisterContextIsValid(controller->stoppedContext) ||
        !controller->currentInstructionAddress.valid || controller->sourceStep.active ||
        controller->stepOver.active || controller->stepOut.active || !currentCallStackIsFresh(controller) ||
        controller->callStack.result.frameCount < 2) return false;
    const DebugStackFrame& current = controller->callStack.result.frames[0];
    const DebugStackFrame& caller = controller->callStack.result.frames[1];
    return current.current && caller.hasReturnAddress && caller.confidence != DebugStackFrameConfidence::Invalid &&
        canonicalAmd64Address(caller.rawReturnAddress);
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

bool DebugControllerStepOver(DebugController* controller, const DebugBackend& backend,
                             const DebugDwarfMapper* mapper, DebugErrorCode* error) {
    if (error) *error = DebugErrorCode::None;
    if (!controller || !mapper || !DebugControllerCanStepOver(controller) ||
        !DebugDwarfMapperIsReady(mapper) || !backend.readMemory || !backend.bindSoftwareBreakpoint ||
        !backend.stepOverCall || !backend.debugCommand) {
        if (error) *error = DebugErrorCode::CapabilityUnavailable;
        return false;
    }
    if (controller->stoppedContext.sessionGeneration != controller->sessionGeneration ||
        controller->stoppedContext.processId != controller->processId ||
        controller->stoppedContext.nativeRuntimeId != controller->nativeRuntimeId ||
        controller->stoppedContext.threadId != controller->currentThreadId ||
        controller->stoppedContext.stopGeneration != controller->stopGeneration) {
        controller->error = DebugErrorCode::StaleStopContext;
        if (error) *error = controller->error;
        return false;
    }
    DebugStepOverOperation operation = DebugStepOverOperation();
    operation.active = true;
    operation.status = DebugStepOverStatus::Active;
    operation.mode = DebugStepOverMode::SourceSingleStep;
    operation.sessionGeneration = controller->sessionGeneration;
    operation.stopGeneration = controller->stopGeneration;
    operation.processId = controller->processId;
    operation.nativeRuntimeId = controller->nativeRuntimeId;
    operation.threadId = controller->currentThreadId;
    operation.startingAddress = controller->currentInstructionAddress.value;
    operation.startingSourceLocation = controller->currentLocation;
    operation.startingRsp = controller->stoppedContext.rsp;
    operation.startingRbp = controller->stoppedContext.rbp;
    operation.lastAddress = operation.startingAddress;
    operation.lastSourceLocation = operation.startingSourceLocation;
    operation.maxInstructionCount = kDebugMaxSourceStepInstructions;
    operation.maxCallCount = kDebugMaxStepOverCalls;
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
    controller->stepOver = operation;
    appendEvent(controller, DebugEventKind::StepOverStarted, DebugSessionState::Stepping,
                DebugStopReason::Step, "User step over requested");
    uint8_t bytes[kDebugMaxInstructionBytes] = {};
    uint32_t byteCount = 0;
    DebugAmd64Instruction instruction = {};
    if (!readStepOverInstruction(controller, backend, operation.startingAddress, bytes, &byteCount,
                                 &instruction, error)) {
        clearStepOver(controller, DebugStepOverStatus::Failed, "Step over could not inspect the current instruction");
        return false;
    }
    (void)byteCount;
    if (instruction.kind == DebugAmd64InstructionKind::Call) {
        if (!startStepOverCall(controller, backend, &controller->stepOver, controller->stoppedContext,
                               operation.startingAddress, instruction.returnAddress, error)) {
            clearStepOver(controller, DebugStepOverStatus::Failed, controller->lastMessage);
            return false;
        }
        return true;
    }
    // Unsupported/unknown non-call encodings use the proven bounded source
    // stepping engine. The call path is selected only after a safe decoder
    // result, so F10 never invents a return address.
    if (!startStepOverSourceInstruction(controller, backend, &controller->stepOver,
                                        controller->stoppedContext, error)) {
        clearStepOver(controller, DebugStepOverStatus::Failed, controller->lastMessage);
        return false;
    }
    controller->error = DebugErrorCode::None;
    return true;
}

bool DebugControllerStepOut(DebugController* controller, const DebugBackend& backend,
                            const DebugDwarfMapper* mapper, DebugErrorCode* error) {
    if (error) *error = DebugErrorCode::None;
    if (!controller || !mapper || !DebugDwarfMapperIsReady(mapper) ||
        !backend.bindSoftwareBreakpoint || !backend.debugCommand || !backend.stepOutReturn ||
        !controller->active || controller->state != DebugSessionState::Paused ||
        !controller->capabilities.canStepOut || !DebugRegisterContextIsValid(controller->stoppedContext) ||
        controller->sourceStep.active || controller->stepOver.active || controller->stepOut.active) {
        if (error) *error = DebugErrorCode::CapabilityUnavailable;
        return false;
    }
    if (!currentCallStackIsFresh(controller)) {
        if (!backend.readTargetMemory || !DebugControllerBuildCallStack(controller, backend, mapper, error)) {
            if (error && *error == DebugErrorCode::None) *error = DebugErrorCode::StaleStopContext;
            controller->error = error ? *error : DebugErrorCode::StaleStopContext;
            setMessage(controller, "Step Out rejected: current Call Stack is stale");
            return false;
        }
    }
    if (!DebugControllerCanStepOut(controller)) {
        if (error) *error = controller->callStack.result.frameCount < 2 ?
            DebugErrorCode::NoCallerFrame : DebugErrorCode::StaleStopContext;
        controller->error = error ? *error : DebugErrorCode::NoCallerFrame;
        setMessage(controller, controller->callStack.result.frameCount < 2 ?
                   "Step Out rejected: no validated caller frame" :
                   "Step Out rejected: current frame is not a valid execution frame");
        return false;
    }
    if (controller->stoppedContext.sessionGeneration != controller->sessionGeneration ||
        controller->stoppedContext.processId != controller->processId ||
        controller->stoppedContext.nativeRuntimeId != controller->nativeRuntimeId ||
        controller->stoppedContext.threadId != controller->currentThreadId ||
        controller->stoppedContext.stopGeneration != controller->stopGeneration) {
        controller->error = DebugErrorCode::StaleStopContext;
        if (error) *error = controller->error;
        setMessage(controller, "Step Out rejected: stopped context is stale");
        return false;
    }

    const DebugStackFrame& current = controller->callStack.result.frames[0];
    const DebugStackFrame& caller = controller->callStack.result.frames[1];
    if (!caller.hasReturnAddress || !canonicalAmd64Address(caller.rawReturnAddress) ||
        !DebugDwarfMapperIsExecutableAddress(mapper, caller.rawReturnAddress)) {
        controller->error = DebugErrorCode::InvalidReturnAddress;
        if (error) *error = controller->error;
        setMessage(controller, "Step Out rejected: caller return address is not a validated target address");
        return false;
    }
    if (!backend.readMemory) {
        controller->error = DebugErrorCode::InstructionReadFailed;
        if (error) *error = controller->error;
        return false;
    }
    uint8_t returnByte = 0;
    uint32_t returned = 0;
    if (!backend.readMemory(backend.userData, controller->sessionGeneration, controller->processId,
                            controller->nativeRuntimeId, caller.rawReturnAddress, &returnByte, 1, &returned) ||
        returned != 1) {
        controller->error = DebugErrorCode::InstructionReadFailed;
        if (error) *error = controller->error;
        setMessage(controller, "Step Out rejected: caller return address cannot be read");
        return false;
    }

    DebugStepOutOperation operation = DebugStepOutOperation();
    operation.active = true;
    operation.status = DebugStepOutStatus::Active;
    operation.sessionGeneration = controller->sessionGeneration;
    operation.stopGeneration = controller->stopGeneration;
    operation.processId = controller->processId;
    operation.nativeRuntimeId = controller->nativeRuntimeId;
    operation.threadId = controller->currentThreadId;
    operation.startingRip = controller->stoppedContext.rip;
    operation.startingRsp = controller->stoppedContext.rsp;
    operation.startingRbp = controller->stoppedContext.rbp;
    operation.startingSource = controller->currentLocation;
    copyText(operation.startingFunction, sizeof(operation.startingFunction), current.functionName);
    operation.rawReturnAddress = caller.rawReturnAddress;
    operation.callerLookupAddress = caller.rawReturnAddress - 1;
    copyText(operation.callerFunction, sizeof(operation.callerFunction), caller.functionName);
    operation.callerSource.instructionAddress.valid = true;
    operation.callerSource.instructionAddress.value = caller.rawReturnAddress;
    operation.callerSource.mapping = caller.mapping == DebugStackFrameMappingState::Mapped ?
        DebugMappingState::Mapped : DebugMappingState::Unavailable;
    copyText(operation.callerSource.relativePath, sizeof(operation.callerSource.relativePath), caller.sourcePath);
    operation.callerSource.line = caller.sourceLine;
    operation.callerSource.column = caller.sourceColumn;
    operation.callerSource.projectGeneration = controller->target.projectGeneration;
    const bool fromBreakpoint = controller->stopReason == DebugStopReason::Breakpoint &&
        controller->backendExecutionState == DebugBackendExecutionState::PausedAtBreakpoint;
    if (fromBreakpoint) {
        const int breakpointIndex = findBreakpoint(controller, controller->lastBreakpointId);
        if (breakpointIndex < 0) {
            if (error) *error = DebugErrorCode::StaleStopContext;
            return false;
        }
        const DebugBreakpoint& breakpoint = controller->breakpoints[breakpointIndex];
        if (breakpoint.sessionGeneration != controller->sessionGeneration || breakpoint.backendBindingId == 0 ||
            !breakpoint.location.instructionAddress.valid ||
            breakpoint.location.instructionAddress.value != controller->currentInstructionAddress.value) {
            controller->error = DebugErrorCode::StaleStopContext;
            if (error) *error = controller->error;
            return false;
        }
        operation.breakpointId = breakpoint.id;
        operation.bindingId = breakpoint.backendBindingId;
        operation.breakpointAddress = breakpoint.location.instructionAddress.value;
        operation.reinstallBreakpoint = breakpoint.enabled;
    }
    uint64_t temporaryId = controller->nextTemporaryBreakpointId++;
    if (temporaryId == 0 || temporaryId < 0x8000000000000001ull)
        temporaryId = controller->nextTemporaryBreakpointId++;
    DebugBreakpoint temporary = {};
    temporary.id = temporaryId;
    temporary.enabled = true;
    temporary.state = DebugBreakpointState::Mapped;
    temporary.sessionGeneration = controller->sessionGeneration;
    temporary.location.instructionAddress.valid = true;
    temporary.location.instructionAddress.value = operation.rawReturnAddress;
    DebugBackendBinding binding = {};
    if (!backend.bindSoftwareBreakpoint(backend.userData, controller->target, controller->sessionGeneration,
                                        controller->processId, controller->nativeRuntimeId, temporary, &binding) ||
        !binding.accepted) {
        controller->error = DebugErrorCode::StepOutFailed;
        if (error) *error = controller->error;
        setMessage(controller, binding.message[0] ? binding.message :
                   "Step Out temporary return breakpoint bind failed");
        return false;
    }
    operation.temporaryBreakpointId = temporaryId;
    operation.temporaryBindingId = binding.bindingId;
    operation.temporaryInstalled = true;
    controller->stepOut = operation;
    appendEvent(controller, DebugEventKind::StepOutStarted, DebugSessionState::Stepping,
                DebugStopReason::Step, "User Step Out requested");
    if (!backend.stepOutReturn(backend.userData, controller->sessionGeneration,
                               controller->stoppedContext, operation.breakpointId,
                               operation.bindingId, controller->currentInstructionAddress.value,
                               operation.reinstallBreakpoint, operation.rawReturnAddress,
                               operation.temporaryBreakpointId)) {
        removeStepOutBreakpoint(controller, backend, controller->stepOut);
        controller->stepOut.temporaryInstalled = false;
        clearStepOut(controller, DebugStepOutStatus::Failed, "Step Out resume was rejected");
        controller->error = DebugErrorCode::StepOutFailed;
        if (error) *error = controller->error;
        setMessage(controller, controller->stepOut.reason);
        return false;
    }
    controller->state = DebugSessionState::Stepping;
    controller->stopReason = DebugStopReason::None;
    controller->backendExecutionState = DebugBackendExecutionState::StepOutPending;
    clearStoppedContext(controller);
    setMessage(controller, "Step Out running to the caller return address");
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
    releaseConditionStorage(controller, controller->breakpoints[index]);
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

bool DebugControllerSetBreakpointCondition(DebugController* controller, uint64_t breakpointId,
                                           const char* expression, DebugErrorCode* error) {
    if (error) *error = DebugErrorCode::None;
    const int index = findBreakpoint(controller, breakpointId);
    if (index < 0 || !expression || expression[0] == '\0') {
        if (error) *error = index < 0 ? DebugErrorCode::BreakpointNotFound : DebugErrorCode::InvalidCondition;
        return false;
    }
    DebugExpressionAst ast = {};
    DebugExpressionParse(expression, &ast);
    DebugBreakpoint& breakpoint = controller->breakpoints[index];
    DebugBreakpointConditionStorage* storage = conditionStorage(controller, breakpoint);
    if (!storage) storage = reserveConditionStorage(controller, breakpoint.id);
    if (!storage) {
        if (error) *error = DebugErrorCode::InvalidRequest;
        return false;
    }
    copyText(storage->expression, sizeof(storage->expression), expression);
    copyText(storage->parseDiagnostic, sizeof(storage->parseDiagnostic), ast.diagnostic);
    breakpoint.condition = storage->expression;
    breakpoint.conditionParseDiagnostic = storage->parseDiagnostic;
    breakpoint.conditionParseState = ast.state;
    breakpoint.conditionLastEvaluation = DebugBreakpointConditionEvaluation::NotEvaluated;
    breakpoint.conditionLastValueState = DebugWatchState::Empty;
    breakpoint.conditionLastValueKind = DebugDwarfValueKind::Unavailable;
    breakpoint.conditionLastTruthValue = false;
    if (!ast.valid) {
        if (error) *error = DebugErrorCode::InvalidCondition;
        return false;
    }
    return true;
}

bool DebugControllerClearBreakpointCondition(DebugController* controller, uint64_t breakpointId,
                                             DebugErrorCode* error) {
    if (error) *error = DebugErrorCode::None;
    const int index = findBreakpoint(controller, breakpointId);
    if (index < 0) {
        if (error) *error = DebugErrorCode::BreakpointNotFound;
        return false;
    }
    DebugBreakpoint& breakpoint = controller->breakpoints[index];
    releaseConditionStorage(controller, breakpoint);
    breakpoint.condition = nullptr;
    breakpoint.conditionParseDiagnostic = nullptr;
    breakpoint.conditionParseState = DebugExpressionParseState::Empty;
    breakpoint.conditionLastEvaluation = DebugBreakpointConditionEvaluation::NotEvaluated;
    breakpoint.conditionLastValueState = DebugWatchState::Empty;
    breakpoint.conditionLastValueKind = DebugDwarfValueKind::Unavailable;
    breakpoint.conditionLastTruthValue = false;
    return true;
}

bool DebugControllerIsConditionResumePending(const DebugController* controller) {
    return controller && controller->conditionResumePending;
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
    if (controller->projectGeneration != projectGeneration) clearCallStack(controller);
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
    clearCallStack(controller);
    if (controller->watches) DebugWatchCollectionMarkStale(controller->watches);
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
