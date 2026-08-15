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

bool DebugControllerBuildVariables(DebugController* controller, const DebugBackend& backend,
                                   const DebugDwarfMapper* mapper, DebugErrorCode* error) {
    if (error) *error = DebugErrorCode::None;
    if (!controller || controller->state != DebugSessionState::Paused ||
        !controller->callStack.valid || controller->callStack.stale || !mapper ||
        !DebugDwarfMapperIsReady(mapper) || !backend.readTargetMemory ||
        !DebugRegisterContextIsValid(controller->stoppedContext)) {
        if (error) *error = DebugErrorCode::StaleStopContext;
        if (controller) controller->variables = DebugDwarfVariableView();
        return false;
    }
    const uint32_t selected = controller->callStack.selectedFrameIndex;
    if (selected >= controller->callStack.result.frameCount) {
        if (error) *error = DebugErrorCode::StaleStopContext;
        controller->variables = DebugDwarfVariableView();
        return false;
    }
    const DebugStackFrame& stackFrame = controller->callStack.result.frames[selected];
    DebugDwarfFrameContext frame = {};
    frame.frameIndex = selected;
    frame.instructionAddress = stackFrame.current ? controller->stoppedContext.rip : stackFrame.lookupAddress;
    frame.processId = controller->processId;
    frame.nativeRuntimeId = controller->nativeRuntimeId;
    frame.threadId = controller->currentThreadId;
    frame.sessionGeneration = controller->sessionGeneration;
    frame.stopGeneration = controller->stopGeneration;
    frame.frameBaseKnown = false;
    DebugDwarfError dwarfError = DebugDwarfError::None;
    uint32_t functionIndex = 0;
    if (DebugDwarfMapperLookupDebugFunction(mapper, frame.instructionAddress, &functionIndex, &dwarfError)) {
        const DebugDwarfFunctionInfo& function = mapper->debugFunctions[functionIndex];
        // The known Clang debug-build frame-base expression is DW_OP_reg6.
        // It is checked per function and never assumed globally.
        if (function.frameBaseLength == 1 && function.frameBase[0] == 0x56) {
            frame.frameBase = stackFrame.rbp;
            frame.frameBaseKnown = frame.frameBase != 0;
        }
    }
    if (stackFrame.current) {
        const DebugRegisterContext& context = controller->stoppedContext;
        frame.registers.valid = true;
        frame.registers.rax = context.rax; frame.registers.rbx = context.rbx;
        frame.registers.rcx = context.rcx; frame.registers.rdx = context.rdx;
        frame.registers.rsi = context.rsi; frame.registers.rdi = context.rdi;
        frame.registers.rbp = context.rbp; frame.registers.rsp = context.rsp;
        frame.registers.r8 = context.r8; frame.registers.r9 = context.r9;
        frame.registers.r10 = context.r10; frame.registers.r11 = context.r11;
        frame.registers.r12 = context.r12; frame.registers.r13 = context.r13;
        frame.registers.r14 = context.r14; frame.registers.r15 = context.r15;
        frame.registers.rip = context.rip; frame.registers.rflags = context.rflags;
    }
    if (!DebugDwarfInspectVariables(mapper, frame, backend.readTargetMemory, backend.userData,
                                    &controller->variables)) {
        if (error) *error = DebugErrorCode::SourceMappingUnavailable;
        return false;
    }
    DebugWatchEvaluationContext watchContext = {};
    watchContext.mapper = mapper;
    watchContext.frame = frame;
    watchContext.readMemory = backend.readTargetMemory;
    watchContext.userData = backend.userData;
    // Watch refresh is intentionally independent from the Locals result. A
    // malformed or unavailable expression must never hide otherwise valid
    // locals/arguments for the selected stopped frame.
    if (controller->watches) DebugWatchCollectionRefresh(controller->watches, watchContext);
    return true;
}

bool DebugControllerExpandVariable(DebugController* controller, const DebugBackend& backend,
                                   const DebugDwarfMapper* mapper, uint64_t nodeId,
                                   DebugErrorCode* error) {
    if (error) *error = DebugErrorCode::None;
    if (!controller || controller->state != DebugSessionState::Paused ||
        !controller->callStack.valid || controller->callStack.stale || !controller->variables.valid ||
        !mapper || !DebugDwarfMapperIsReady(mapper) || !backend.readTargetMemory ||
        !DebugRegisterContextIsValid(controller->stoppedContext)) {
        if (error) *error = DebugErrorCode::StaleStopContext;
        return false;
    }
    const uint32_t selected = controller->callStack.selectedFrameIndex;
    if (selected >= controller->callStack.result.frameCount ||
        controller->variables.frameIndex != selected ||
        controller->variables.sessionGeneration != controller->sessionGeneration ||
        controller->variables.stopGeneration != controller->stopGeneration) {
        if (error) *error = DebugErrorCode::StaleStopContext;
        return false;
    }
    const DebugStackFrame& stackFrame = controller->callStack.result.frames[selected];
    DebugDwarfFrameContext frame = {};
    frame.frameIndex = selected;
    frame.instructionAddress = stackFrame.current ? controller->stoppedContext.rip : stackFrame.lookupAddress;
    frame.processId = controller->processId;
    frame.nativeRuntimeId = controller->nativeRuntimeId;
    frame.threadId = controller->currentThreadId;
    frame.sessionGeneration = controller->sessionGeneration;
    frame.stopGeneration = controller->stopGeneration;
    uint32_t functionIndex = 0;
    DebugDwarfError dwarfError = DebugDwarfError::None;
    if (DebugDwarfMapperLookupDebugFunction(mapper, frame.instructionAddress, &functionIndex, &dwarfError)) {
        const DebugDwarfFunctionInfo& function = mapper->debugFunctions[functionIndex];
        if (function.frameBaseLength == 1 && function.frameBase[0] == 0x56) {
            frame.frameBase = stackFrame.rbp;
            frame.frameBaseKnown = frame.frameBase != 0;
        }
    }
    if (stackFrame.current) {
        const DebugRegisterContext& context = controller->stoppedContext;
        frame.registers.valid = true;
        frame.registers.rax = context.rax; frame.registers.rbx = context.rbx;
        frame.registers.rcx = context.rcx; frame.registers.rdx = context.rdx;
        frame.registers.rsi = context.rsi; frame.registers.rdi = context.rdi;
        frame.registers.rbp = context.rbp; frame.registers.rsp = context.rsp;
        frame.registers.r8 = context.r8; frame.registers.r9 = context.r9;
        frame.registers.r10 = context.r10; frame.registers.r11 = context.r11;
        frame.registers.r12 = context.r12; frame.registers.r13 = context.r13;
        frame.registers.r14 = context.r14; frame.registers.r15 = context.r15;
        frame.registers.rip = context.rip; frame.registers.rflags = context.rflags;
    }
    if (!DebugDwarfExpandValue(mapper, frame, backend.readTargetMemory, backend.userData,
                               &controller->variables, nodeId)) {
        if (error) *error = DebugErrorCode::StaleStopContext;
        return false;
    }
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

static bool fillWatchContext(const DebugController* controller, const DebugBackend& backend,
                             const DebugDwarfMapper* mapper, uint32_t frameIndex,
                             uint64_t instructionAddress, uint64_t frameBase,
                             bool frameBaseKnown, bool current,
                             DebugWatchEvaluationContext* context) {
    if (!controller || !context || controller->state != DebugSessionState::Paused || !mapper ||
        !DebugDwarfMapperIsReady(mapper) || !backend.readTargetMemory ||
        !DebugRegisterContextIsValid(controller->stoppedContext) || instructionAddress == 0) return false;
    context->mapper = mapper;
    context->frame = DebugDwarfFrameContext();
    context->frame.frameIndex = frameIndex;
    context->frame.instructionAddress = instructionAddress;
    context->frame.processId = controller->processId;
    context->frame.nativeRuntimeId = controller->nativeRuntimeId;
    context->frame.threadId = controller->currentThreadId;
    context->frame.sessionGeneration = controller->sessionGeneration;
    context->frame.stopGeneration = controller->stopGeneration;
    context->frame.frameBase = frameBase;
    context->frame.frameBaseKnown = frameBaseKnown;
    context->readMemory = backend.readTargetMemory;
    context->userData = backend.userData;
    if (current) {
        const DebugRegisterContext& registers = controller->stoppedContext;
        context->frame.registers.valid = true;
        context->frame.registers.rax = registers.rax; context->frame.registers.rbx = registers.rbx;
        context->frame.registers.rcx = registers.rcx; context->frame.registers.rdx = registers.rdx;
        context->frame.registers.rsi = registers.rsi; context->frame.registers.rdi = registers.rdi;
        context->frame.registers.rbp = registers.rbp; context->frame.registers.rsp = registers.rsp;
        context->frame.registers.r8 = registers.r8; context->frame.registers.r9 = registers.r9;
        context->frame.registers.r10 = registers.r10; context->frame.registers.r11 = registers.r11;
        context->frame.registers.r12 = registers.r12; context->frame.registers.r13 = registers.r13;
        context->frame.registers.r14 = registers.r14; context->frame.registers.r15 = registers.r15;
        context->frame.registers.rip = registers.rip; context->frame.registers.rflags = registers.rflags;
    }
    return true;
}

static bool controllerWatchContext(const DebugController* controller, const DebugBackend& backend,
                                   const DebugDwarfMapper* mapper,
                                   DebugWatchEvaluationContext* context) {
    if (!controller || !context || !controller->callStack.valid || controller->callStack.stale) return false;
    const uint32_t selected = controller->callStack.selectedFrameIndex;
    if (selected >= controller->callStack.result.frameCount) return false;
    const DebugStackFrame& stackFrame = controller->callStack.result.frames[selected];
    uint64_t instructionAddress = stackFrame.current ? controller->stoppedContext.rip : stackFrame.lookupAddress;
    bool frameBaseKnown = false;
    uint64_t frameBase = 0;
    uint32_t functionIndex = 0;
    DebugDwarfError dwarfError = DebugDwarfError::None;
    if (mapper && DebugDwarfMapperLookupDebugFunction(mapper, instructionAddress, &functionIndex, &dwarfError)) {
        const DebugDwarfFunctionInfo& function = mapper->debugFunctions[functionIndex];
        if (function.frameBaseLength == 1 && function.frameBase[0] == 0x56) {
            frameBase = stackFrame.rbp;
            frameBaseKnown = frameBase != 0;
        }
    }
    return fillWatchContext(controller, backend, mapper, selected, instructionAddress,
                            frameBase, frameBaseKnown, stackFrame.current, context);
}

static bool breakpointWatchContext(const DebugController* controller, const DebugBackend& backend,
                                  const DebugDwarfMapper* mapper,
                                  DebugWatchEvaluationContext* context) {
    if (!controller || !DebugRegisterContextIsValid(controller->stoppedContext)) return false;
    const uint64_t instructionAddress = controller->stoppedContext.rip != 0 ?
        controller->stoppedContext.rip : controller->currentInstructionAddress.value;
    uint64_t frameBase = 0;
    bool frameBaseKnown = false;
    uint32_t functionIndex = 0;
    DebugDwarfError dwarfError = DebugDwarfError::None;
    if (mapper && DebugDwarfMapperLookupDebugFunction(mapper, instructionAddress, &functionIndex, &dwarfError)) {
        const DebugDwarfFunctionInfo& function = mapper->debugFunctions[functionIndex];
        if (function.frameBaseLength == 1 && function.frameBase[0] == 0x56) {
            frameBase = controller->stoppedContext.rbp;
            frameBaseKnown = frameBase != 0;
        }
    }
    return fillWatchContext(controller, backend, mapper, 0, instructionAddress,
                            frameBase, frameBaseKnown, true, context);
}

static void setConditionDiagnostic(DebugController* controller, DebugBreakpoint& breakpoint,
                                   DebugBreakpointConditionEvaluation evaluation,
                                   DebugWatchState valueState, DebugDwarfValueKind valueKind,
                                   uint64_t scalarValue, bool truth, uint32_t frameIndex,
                                   const char* diagnostic) {
    (void)controller;
    (void)scalarValue;
    (void)frameIndex;
    breakpoint.conditionLastEvaluation = evaluation;
    breakpoint.conditionLastValueState = valueState;
    breakpoint.conditionLastValueKind = valueKind;
    breakpoint.conditionLastTruthValue = truth;
    (void)diagnostic;
}

static void setConditionError(DebugController* controller, DebugBreakpoint& breakpoint,
                              DebugWatchState valueState, DebugDwarfValueKind valueKind,
                              uint64_t scalarValue, uint32_t frameIndex, const char* diagnostic) {
    setConditionDiagnostic(controller, breakpoint, DebugBreakpointConditionEvaluation::Error,
                           valueState, valueKind, scalarValue, false, frameIndex, diagnostic);
    if (controller) {
        controller->error = DebugErrorCode::ConditionError;
        copyText(controller->lastMessage, sizeof(controller->lastMessage), "ConditionError: ");
        uint32_t length = 0;
        while (length + 1 < sizeof(controller->lastMessage) && controller->lastMessage[length]) ++length;
        const char* text = breakpoint.condition && breakpoint.condition[0] ? breakpoint.condition : "<condition>";
        for (uint32_t i = 0; length + 1 < sizeof(controller->lastMessage) && text[i]; ++i)
            controller->lastMessage[length++] = text[i];
        const char separator[] = " | ";
        for (uint32_t i = 0; length + 1 < sizeof(controller->lastMessage) && separator[i]; ++i)
            controller->lastMessage[length++] = separator[i];
        text = diagnostic && diagnostic[0] ? diagnostic : "condition could not be evaluated safely";
        for (uint32_t i = 0; length + 1 < sizeof(controller->lastMessage) && text[i]; ++i)
            controller->lastMessage[length++] = text[i];
        controller->lastMessage[length] = '\0';
    }
}

bool DebugControllerEvaluateBreakpointCondition(DebugController* controller,
                                                const DebugBackend& backend,
                                                const DebugDwarfMapper* mapper,
                                                DebugBreakpointConditionDecision* decision) {
    if (decision) *decision = DebugBreakpointConditionDecision::Error;
    if (!controller || controller->state != DebugSessionState::Paused ||
        !DebugRegisterContextIsValid(controller->stoppedContext)) return false;
    int breakpointIndex = -1;
    for (uint32_t i = 0; i < controller->breakpointCount; ++i)
        if (controller->breakpoints[i].id == controller->lastBreakpointId) { breakpointIndex = static_cast<int>(i); break; }
    if (breakpointIndex < 0) return false;
    DebugBreakpoint& breakpoint = controller->breakpoints[breakpointIndex];
    if (!breakpoint.condition || breakpoint.condition[0] == '\0') {
        if (decision) *decision = DebugBreakpointConditionDecision::NoCondition;
        return true;
    }

    DebugExpressionAst ast = {};
    if (!DebugExpressionParse(breakpoint.condition, &ast)) {
        breakpoint.conditionParseState = ast.state;
        if (breakpoint.conditionParseDiagnostic)
            copyText(breakpoint.conditionParseDiagnostic, kDebugWatchMaxDiagnosticBytes, ast.diagnostic);
        setConditionError(controller, breakpoint, DebugWatchState::ParseError, DebugDwarfValueKind::Unavailable,
                          0, 0, ast.diagnostic[0] ? ast.diagnostic : "invalid condition expression");
        return true;
    }
    breakpoint.conditionParseState = DebugExpressionParseState::Valid;
    if (breakpoint.conditionParseDiagnostic)
        breakpoint.conditionParseDiagnostic[0] = '\0';
    DebugWatchEvaluationContext context = {};
    if (!breakpointWatchContext(controller, backend, mapper, &context)) {
        setConditionError(controller, breakpoint, DebugWatchState::Stale, DebugDwarfValueKind::Unavailable,
                          0, 0, "stopped condition context is stale or unavailable");
        return true;
    }
    if (!DebugDwarfInspectVariables(context.mapper, context.frame, context.readMemory,
                                    context.userData, &controller->variables)) {
        setConditionError(controller, breakpoint, DebugWatchState::MalformedDebugInfo,
                          DebugDwarfValueKind::Unavailable, 0, 0,
                          "condition variables could not be materialized safely");
        return true;
    }
    DebugWatchResult result = {};
    if (!DebugWatchEvaluateExpression(ast, breakpoint.condition, context,
                                      &controller->variables, &result)) {
        setConditionError(controller, breakpoint, result.state, result.valueKind,
                          result.scalarValue, result.frameIndex,
                          result.diagnostic[0] ? result.diagnostic : "condition evaluation failed");
        return true;
    }
    bool truth = false;
    char truthDiagnostic[kDebugWatchMaxDiagnosticBytes] = {};
    if (!DebugWatchConvertToTruth(result, &truth, truthDiagnostic, sizeof(truthDiagnostic))) {
        setConditionError(controller, breakpoint, result.state, result.valueKind,
                          result.scalarValue, result.frameIndex,
                          truthDiagnostic[0] ? truthDiagnostic : "condition value is not scalar");
        return true;
    }
    setConditionDiagnostic(controller, breakpoint,
                           truth ? DebugBreakpointConditionEvaluation::True : DebugBreakpointConditionEvaluation::False,
                           result.state, result.valueKind, result.scalarValue, truth,
                           result.frameIndex, "");
    if (decision) *decision = truth ? DebugBreakpointConditionDecision::True : DebugBreakpointConditionDecision::False;
    return true;
}

bool DebugControllerAddWatch(DebugController* controller, const char* expression,
                             uint64_t* outWatchId, DebugErrorCode* error) {
    if (error) *error = DebugErrorCode::None;
    if (!controller || !controller->watches || !expression || !DebugWatchCollectionAdd(controller->watches, expression, outWatchId)) {
        if (error) *error = DebugErrorCode::InvalidRequest;
        return false;
    }
    if (controller->state == DebugSessionState::Paused) {
        // BuildVariables is the normal refresh path. The direct API remains
        // useful to UI/model clients that add a watch after a stop.
        controller->watches->treeStale = true;
    }
    return true;
}

bool DebugControllerEditWatch(DebugController* controller, uint64_t watchId,
                              const char* expression, DebugErrorCode* error) {
    if (error) *error = DebugErrorCode::None;
    if (!controller || !controller->watches || !DebugWatchCollectionEdit(controller->watches, watchId, expression)) {
        if (error) *error = DebugErrorCode::InvalidRequest;
        return false;
    }
    return true;
}

bool DebugControllerRemoveWatch(DebugController* controller, uint64_t watchId,
                                DebugErrorCode* error) {
    if (error) *error = DebugErrorCode::None;
    if (!controller || !controller->watches || !DebugWatchCollectionRemove(controller->watches, watchId)) {
        if (error) *error = DebugErrorCode::InvalidRequest;
        return false;
    }
    return true;
}

bool DebugControllerExpandWatch(DebugController* controller, const DebugBackend& backend,
                                const DebugDwarfMapper* mapper, uint64_t watchId,
                                DebugErrorCode* error) {
    if (error) *error = DebugErrorCode::None;
    DebugWatchEvaluationContext context = {};
    if (!controllerWatchContext(controller, backend, mapper, &context) ||
        !controller->watches || !DebugWatchCollectionExpand(controller->watches, context, watchId)) {
        if (error) *error = DebugErrorCode::StaleStopContext;
        return false;
    }
    return true;
}

} // namespace developer_studio
} // namespace guidexos
