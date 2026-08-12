#include "developer_studio_debugger.h"
#include "developer_studio_debug_symbols.h"

namespace guidexos {
namespace developer_studio {
namespace {

static bool canonicalAmd64(uint64_t address) {
    const uint64_t upper = address >> 48;
    return upper == 0 || upper == 0xffffu;
}

static bool addWithoutOverflow(uint64_t value, uint64_t amount, uint64_t* result) {
    if (!result || value > UINT64_MAX - amount) return false;
    *result = value + amount;
    return true;
}

static void setStatus(DebugUnwindResult* result, const char* status) {
    if (!result) return;
    if (!status) status = "";
    uint32_t index = 0;
    while (status[index] && index + 1 < sizeof(result->status)) {
        result->status[index] = status[index];
        ++index;
    }
    result->status[index] = '\0';
}

static void copyText(char* destination, uint32_t capacity, const char* source) {
    if (!destination || capacity == 0) return;
    if (!source) source = "";
    uint32_t index = 0;
    while (source[index] && index + 1 < capacity) {
        destination[index] = source[index];
        ++index;
    }
    destination[index] = '\0';
}

static void clearFrame(DebugStackFrame* frame, uint32_t index) {
    if (!frame) return;
    *frame = DebugStackFrame();
    frame->index = index;
    frame->mapping = DebugStackFrameMappingState::Unmapped;
    frame->confidence = DebugStackFrameConfidence::Invalid;
}

static void mapFrame(DebugStackFrame* frame, const DebugDwarfMapper* mapper,
                     uint64_t address, uint64_t lookupAddress) {
    if (!frame) return;
    frame->instructionAddress = address;
    frame->lookupAddress = lookupAddress;
    if (!mapper || !DebugDwarfMapperIsReady(mapper)) {
        frame->mapping = DebugStackFrameMappingState::Stale;
        return;
    }
    DebugDwarfError symbolError = DebugDwarfError::None;
    const uint64_t symbolAddress = lookupAddress != 0 ? lookupAddress : address;
    DebugDwarfMapperLookupFunction(mapper, symbolAddress, frame->functionName,
                                   sizeof(frame->functionName), &frame->functionStartAddress,
                                   &frame->functionSize, &symbolError);
    DebugDwarfError sourceError = DebugDwarfError::None;
    uint64_t sourceAddress = lookupAddress;
    if (sourceAddress == 0) sourceAddress = address;
    if (DebugDwarfMapperMapAddressToSource(mapper, sourceAddress, frame->sourcePath,
                                           sizeof(frame->sourcePath), &frame->sourceLine,
                                           &frame->sourceColumn, &sourceError)) {
        frame->mapping = DebugStackFrameMappingState::Mapped;
    } else {
        frame->mapping = DebugStackFrameMappingState::Unmapped;
    }
}

static bool validStackRange(const DebugRegisterContext& context) {
    return context.stackLow != 0 && context.stackHigh > context.stackLow &&
        context.stackHigh - context.stackLow >= 16 &&
        canonicalAmd64(context.stackLow) && canonicalAmd64(context.stackHigh - 1);
}

static bool validStackSlot(const DebugRegisterContext& context, uint64_t address) {
    return validStackRange(context) && canonicalAmd64(address) &&
        address >= context.stackLow && address <= context.stackHigh - 16;
}

static bool readStackSlot(DebugBackendReadTargetMemoryFn readMemory, void* userData,
                          uint64_t sessionGeneration, const DebugRegisterContext& context,
                          uint64_t address, uint64_t* first, uint64_t* second) {
    if (!readMemory || !first || !second || !validStackSlot(context, address)) return false;
    uint8_t bytes[16] = {};
    uint32_t returned = 0;
    if (!readMemory(userData, sessionGeneration, context.processId, context.nativeRuntimeId,
                    context.threadId, context.stopGeneration, address, bytes, sizeof(bytes),
                    &returned) || returned != sizeof(bytes)) return false;
    *first = 0;
    *second = 0;
    for (uint32_t i = 0; i < 8; ++i) {
        *first |= static_cast<uint64_t>(bytes[i]) << (i * 8);
        *second |= static_cast<uint64_t>(bytes[8 + i]) << (i * 8);
    }
    return true;
}

static bool seenFramePointer(const uint64_t* seen, uint32_t count, uint64_t value) {
    if (!seen) return false;
    for (uint32_t i = 0; i < count; ++i) if (seen[i] == value) return true;
    return false;
}

static void appendFrame(DebugUnwindResult* result, uint64_t address, uint64_t rawReturnAddress,
                        uint64_t rsp, uint64_t rbp, bool current,
                        const DebugDwarfMapper* mapper) {
    if (!result || result->frameCount >= kDebugMaxStackFrames) return;
    DebugStackFrame& frame = result->frames[result->frameCount];
    clearFrame(&frame, result->frameCount);
    frame.current = current;
    frame.hasReturnAddress = !current && rawReturnAddress != 0;
    frame.rawReturnAddress = rawReturnAddress;
    frame.rsp = rsp;
    frame.rbp = rbp;
    frame.confidence = current ? DebugStackFrameConfidence::ExactCurrent :
        DebugStackFrameConfidence::FramePointer;
    const uint64_t lookupAddress = !current && rawReturnAddress > 0 ? rawReturnAddress - 1 : address;
    mapFrame(&frame, mapper, address, lookupAddress);
    if (frame.mapping == DebugStackFrameMappingState::Unmapped && !frame.functionName[0] && !current)
        frame.confidence = DebugStackFrameConfidence::Unmapped;
    ++result->frameCount;
}

} // namespace

const char* DebugStackFrameMappingStateName(DebugStackFrameMappingState state) {
    switch (state) {
    case DebugStackFrameMappingState::Unmapped: return "Unmapped";
    case DebugStackFrameMappingState::Mapped: return "Mapped";
    case DebugStackFrameMappingState::External: return "External";
    case DebugStackFrameMappingState::Stale: return "Stale";
    }
    return "Unknown";
}

const char* DebugStackFrameConfidenceName(DebugStackFrameConfidence confidence) {
    switch (confidence) {
    case DebugStackFrameConfidence::ExactCurrent: return "ExactCurrent";
    case DebugStackFrameConfidence::FramePointer: return "FramePointer";
    case DebugStackFrameConfidence::Unmapped: return "Unmapped";
    case DebugStackFrameConfidence::Invalid: return "Invalid";
    }
    return "Unknown";
}

const char* DebugUnwindTerminationReasonName(DebugUnwindTerminationReason reason) {
    switch (reason) {
    case DebugUnwindTerminationReason::None: return "None";
    case DebugUnwindTerminationReason::EndOfStack: return "EndOfStack";
    case DebugUnwindTerminationReason::FrameLimit: return "FrameLimit";
    case DebugUnwindTerminationReason::InvalidFramePointer: return "InvalidFramePointer";
    case DebugUnwindTerminationReason::ReadFailure: return "ReadFailure";
    case DebugUnwindTerminationReason::Cycle: return "Cycle";
    case DebugUnwindTerminationReason::OutsideStack: return "OutsideStack";
    case DebugUnwindTerminationReason::OutsideTarget: return "OutsideTarget";
    case DebugUnwindTerminationReason::NoFramePointer: return "NoFramePointer";
    case DebugUnwindTerminationReason::StaleContext: return "StaleContext";
    case DebugUnwindTerminationReason::UnsupportedArchitecture: return "UnsupportedArchitecture";
    }
    return "Unknown";
}

bool DebugUnwindAmd64FramePointer(const DebugRegisterContext& context,
                                  const DebugDwarfMapper* mapper,
                                  DebugBackendReadTargetMemoryFn readMemory,
                                  void* userData, uint64_t sessionGeneration,
                                  DebugUnwindResult* result) {
    if (!result) return false;
    *result = DebugUnwindResult();
    result->terminationReason = DebugUnwindTerminationReason::None;
    if (!DebugRegisterContextIsValid(context) || context.architecture != DebugArchitecture::Amd64 ||
        context.sessionGeneration != sessionGeneration || !mapper || !DebugDwarfMapperIsReady(mapper)) {
        result->terminationReason = !DebugRegisterContextIsValid(context) ||
            context.sessionGeneration != sessionGeneration ? DebugUnwindTerminationReason::StaleContext :
            DebugUnwindTerminationReason::UnsupportedArchitecture;
        setStatus(result, DebugUnwindTerminationReasonName(result->terminationReason));
        return false;
    }
    if (!canonicalAmd64(context.rip) || !canonicalAmd64(context.rsp)) {
        result->terminationReason = DebugUnwindTerminationReason::StaleContext;
        setStatus(result, "Invalid canonical stopped register context");
        return false;
    }
    appendFrame(result, context.rip, 0, context.rsp, context.rbp, true, mapper);
    if (!validStackRange(context) || context.rbp == 0) {
        result->terminationReason = DebugUnwindTerminationReason::NoFramePointer;
        setStatus(result, "Frame pointer chain unavailable; current frame retained");
        return true;
    }
    if (!validStackSlot(context, context.rbp) || (context.rbp & 7u) != 0) {
        result->terminationReason = DebugUnwindTerminationReason::OutsideStack;
        setStatus(result, "Current frame pointer is outside the validated target stack");
        return true;
    }

    uint64_t seen[kDebugMaxStackFrames] = {};
    uint32_t seenCount = 0;
    uint64_t currentRbp = context.rbp;
    uint64_t currentRsp = context.rsp;
    while (result->frameCount < kDebugMaxStackFrames) {
        if (seenFramePointer(seen, seenCount, currentRbp)) {
            result->terminationReason = DebugUnwindTerminationReason::Cycle;
            setStatus(result, "Repeated frame pointer detected");
            return true;
        }
        seen[seenCount++] = currentRbp;
        uint64_t previousRbp = 0;
        uint64_t returnAddress = 0;
        if (!readStackSlot(readMemory, userData, sessionGeneration, context, currentRbp,
                           &previousRbp, &returnAddress)) {
            result->terminationReason = DebugUnwindTerminationReason::ReadFailure;
            setStatus(result, "Target stack read failed");
            return true;
        }
        if (returnAddress == 0 || !canonicalAmd64(returnAddress)) {
            result->terminationReason = DebugUnwindTerminationReason::OutsideTarget;
            setStatus(result, "Saved return address is not a canonical target address");
            return true;
        }
        uint64_t callerRsp = 0;
        if (!addWithoutOverflow(currentRbp, 16, &callerRsp)) {
            result->terminationReason = DebugUnwindTerminationReason::InvalidFramePointer;
            setStatus(result, "Caller stack pointer overflowed");
            return true;
        }
        appendFrame(result, returnAddress, returnAddress, callerRsp, previousRbp, false, mapper);
        DebugStackFrame& appended = result->frames[result->frameCount - 1];
        if (!DebugDwarfMapperIsExecutableAddress(mapper, returnAddress)) {
            appended.confidence = DebugStackFrameConfidence::Invalid;
            appended.mapping = DebugStackFrameMappingState::Unmapped;
            appended.functionName[0] = '\0';
            appended.sourcePath[0] = '\0';
            appended.sourceLine = 0;
            appended.sourceColumn = 0;
            result->terminationReason = DebugUnwindTerminationReason::OutsideTarget;
            setStatus(result, "Saved return address is outside target executable segments");
            return true;
        }
        if (previousRbp == 0) {
            result->terminationReason = DebugUnwindTerminationReason::EndOfStack;
            setStatus(result, "Frame pointer chain ended at a zero sentinel");
            return true;
        }
        if (seenFramePointer(seen, seenCount, previousRbp)) {
            result->terminationReason = DebugUnwindTerminationReason::Cycle;
            setStatus(result, "Repeated frame pointer detected");
            return true;
        }
        if (!canonicalAmd64(previousRbp) || (previousRbp & 7u) != 0 ||
            !validStackSlot(context, previousRbp) || previousRbp <= currentRbp) {
            result->terminationReason = previousRbp < context.stackLow || previousRbp >= context.stackHigh ?
                DebugUnwindTerminationReason::OutsideStack : DebugUnwindTerminationReason::InvalidFramePointer;
            setStatus(result, "Frame pointer chain is non-monotonic or invalid");
            return true;
        }
        if (previousRbp - currentRbp > kDebugMaxStackFrameDelta) {
            result->terminationReason = DebugUnwindTerminationReason::InvalidFramePointer;
            setStatus(result, "Frame pointer chain contains an excessive frame delta");
            return true;
        }
        currentRsp = callerRsp;
        currentRbp = previousRbp;
        if (result->frameCount == kDebugMaxStackFrames) {
            result->truncated = true;
            result->terminationReason = DebugUnwindTerminationReason::FrameLimit;
            setStatus(result, "Maximum call-stack frame count reached");
            return true;
        }
    }
    (void)currentRsp;
    result->truncated = true;
    result->terminationReason = DebugUnwindTerminationReason::FrameLimit;
    setStatus(result, "Maximum call-stack frame count reached");
    return true;
}

bool DebugControllerBuildCallStack(DebugController* controller, const DebugBackend& backend,
                                   const DebugDwarfMapper* mapper, DebugErrorCode* error) {
    if (error) *error = DebugErrorCode::None;
    if (!controller || controller->state != DebugSessionState::Paused ||
        controller->stopReason == DebugStopReason::None ||
        !DebugRegisterContextIsValid(controller->stoppedContext) || !mapper ||
        !DebugDwarfMapperIsReady(mapper) || !backend.readTargetMemory) {
        if (error) *error = DebugErrorCode::StaleStopContext;
        return false;
    }
    DebugCallStack stack = DebugCallStack();
    stack.sessionGeneration = controller->sessionGeneration;
    stack.processId = controller->processId;
    stack.nativeRuntimeId = controller->nativeRuntimeId;
    stack.threadId = controller->currentThreadId;
    stack.stopGeneration = controller->stopGeneration;
    stack.mapperGeneration = mapper->identity.mapperGeneration;
    copyText(stack.artifactSha256, sizeof(stack.artifactSha256), controller->target.artifactSha256);
    copyText(stack.unwinderName, sizeof(stack.unwinderName), "AMD64 Frame Pointer");
    stack.selectedFrameIndex = 0;
    if (!DebugUnwindAmd64FramePointer(controller->stoppedContext, mapper,
                                      backend.readTargetMemory, backend.userData,
                                      controller->sessionGeneration, &stack.result)) {
        if (error) *error = DebugErrorCode::StaleStopContext;
        controller->callStack = stack;
        controller->callStack.valid = false;
        controller->callStack.stale = false;
        return false;
    }
    stack.valid = true;
    stack.stale = false;
    controller->callStack = stack;
    return true;
}

bool DebugControllerSelectCallStackFrame(DebugController* controller, uint32_t frameIndex,
                                         DebugErrorCode* error) {
    if (error) *error = DebugErrorCode::None;
    if (!controller || controller->state != DebugSessionState::Paused ||
        !controller->callStack.valid || controller->callStack.stale ||
        controller->callStack.sessionGeneration != controller->sessionGeneration ||
        controller->callStack.stopGeneration != controller->stopGeneration ||
        frameIndex >= controller->callStack.result.frameCount) {
        if (error) *error = DebugErrorCode::StaleStopContext;
        return false;
    }
    controller->callStack.selectedFrameIndex = frameIndex;
    return true;
}

const DebugStackFrame* DebugControllerCallStackFrameAt(const DebugController* controller,
                                                       uint32_t index) {
    return controller && controller->state == DebugSessionState::Paused &&
        controller->callStack.valid && !controller->callStack.stale &&
        index < controller->callStack.result.frameCount ? &controller->callStack.result.frames[index] : nullptr;
}

} // namespace developer_studio
} // namespace guidexos
