#include "developer_studio_debugger.h"
#include "developer_studio_debug_symbols.h"

#include <cassert>
#include <cstring>
#include <iostream>

using namespace guidexos::developer_studio;

namespace {

struct StepFake {
    bool pending = false;
    bool wrongThread = false;
    uint32_t stepCalls = 0;
    uint64_t nextRip = 0x104;
    DebugRegisterContext lastContext = {};
};

struct StepOverFake {
    bool trapPending = false;
    uint32_t readCalls = 0;
    uint32_t bindCalls = 0;
    uint32_t removeCalls = 0;
    uint32_t callCalls = 0;
};

struct StepOutFake {
    uint32_t bindCalls = 0;
    uint32_t removeCalls = 0;
    uint32_t stepOutCalls = 0;
    uint32_t pollCalls = 0;
    uint32_t resumeCalls = 0;
    uint64_t returnAddress = 0x201;
    uint64_t bindingId = 600;
    uint64_t temporaryId = 0;
};

static bool stepOutPoll(void* userData, uint64_t generation, DebugBackendSnapshot* snapshot) {
    StepOutFake* fake = static_cast<StepOutFake*>(userData);
    ++fake->pollCalls;
    *snapshot = DebugBackendSnapshot();
    snapshot->sessionGeneration = generation;
    snapshot->state = DebugSessionState::Paused;
    snapshot->stopReason = DebugStopReason::Step;
    snapshot->processId = 12;
    snapshot->nativeRuntimeId = 77;
    snapshot->threadId = 44;
    snapshot->breakpointTrap = true;
    snapshot->internalBreakpointTrap = true;
    snapshot->internalBreakpointPurpose = static_cast<uint32_t>(HostedDebugInternalBreakpointPurpose::StepOut);
    snapshot->internalBreakpointId = fake->temporaryId;
    snapshot->targetAddress = { true, fake->returnAddress };
    snapshot->breakpointBindingId = fake->bindingId;
    snapshot->instructionPointer = fake->returnAddress + 1;
    snapshot->stopGeneration = 4;
    snapshot->executionState = DebugBackendExecutionState::PausedAtStepOut;
    snapshot->registerContext.valid = true;
    snapshot->registerContext.architecture = DebugArchitecture::Amd64;
    snapshot->registerContext.processId = 12;
    snapshot->registerContext.nativeRuntimeId = 77;
    snapshot->registerContext.threadId = 44;
    snapshot->registerContext.sessionGeneration = generation;
    snapshot->registerContext.stopGeneration = 4;
    snapshot->registerContext.rip = fake->returnAddress + 1;
    snapshot->registerContext.rflags = 0x202;
    snapshot->registerContext.rsp = 0x700110;
    snapshot->registerContext.rbp = 0x700140;
    snapshot->registerContext.stackLow = 0x700000;
    snapshot->registerContext.stackHigh = 0x702000;
    return true;
}

static bool stepOutRead(void*, uint64_t, uint64_t, uint64_t, uint64_t address,
                        uint8_t* bytes, uint32_t requested, uint32_t* returned) {
    if (returned) *returned = 0;
    if (address != 0x201 || !bytes || requested != 1) return false;
    bytes[0] = 0x90;
    if (returned) *returned = 1;
    return true;
}

static bool stepOutReadTarget(void*, uint64_t, uint64_t processId, uint64_t runtimeId,
                              uint64_t threadId, uint64_t, uint64_t address,
                              uint8_t* bytes, uint32_t requested, uint32_t* returned) {
    if (returned) *returned = 0;
    if (processId != 12 || runtimeId != 77 || threadId != 44 || !bytes || requested != 16 ||
        address < 0x700000 || address > 0x702000 - 16 || (address - 0x700000) % 8 != 0) return false;
    uint64_t first = 0;
    uint64_t second = 0;
    if (address == 0x700140) second = 0x220;
    for (uint32_t i = 0; i < 8; ++i) bytes[i] = static_cast<uint8_t>(first >> (i * 8));
    for (uint32_t i = 0; i < 8; ++i) bytes[8 + i] = static_cast<uint8_t>(second >> (i * 8));
    if (returned) *returned = 16;
    return true;
}

static bool stepOutBind(void* userData, const DebugTarget&, uint64_t, uint64_t, uint64_t,
                        const DebugBreakpoint& breakpoint, DebugBackendBinding* binding) {
    StepOutFake* fake = static_cast<StepOutFake*>(userData);
    ++fake->bindCalls;
    fake->temporaryId = breakpoint.id;
    *binding = DebugBackendBinding();
    binding->accepted = true;
    binding->bindingId = fake->bindingId;
    binding->originalByte = 0x90;
    binding->installedByte = 0xCC;
    binding->originalByteValid = true;
    return true;
}

static bool stepOutCommand(void* userData, HostedDebugCommand command, uint64_t, uint64_t,
                           uint64_t, uint64_t, uint64_t, uint64_t, const char*, uint64_t,
                           uint64_t, bool, uint64_t, uint32_t, HostedDebugResult* result) {
    StepOutFake* fake = static_cast<StepOutFake*>(userData);
    *result = HostedDebugResult();
    if (command == HostedDebugCommand::RemoveSoftwareBreakpointOwner) ++fake->removeCalls;
    result->status = command == HostedDebugCommand::RemoveSoftwareBreakpointOwner ? 4 : 1;
    return true;
}

static bool stepOutReturn(void* userData, uint64_t, const DebugRegisterContext&,
                          uint64_t, uint64_t, uint64_t, bool, uint64_t returnAddress,
                          uint64_t temporaryBreakpointId) {
    StepOutFake* fake = static_cast<StepOutFake*>(userData);
    ++fake->stepOutCalls;
    fake->returnAddress = returnAddress;
    fake->temporaryId = temporaryBreakpointId;
    return true;
}

static bool stepOutResume(void* userData, uint64_t, const DebugRegisterContext&) {
    StepOutFake* fake = static_cast<StepOutFake*>(userData);
    ++fake->resumeCalls;
    return true;
}

static DebugBackend makeStepOutBackend(StepOutFake* fake) {
    DebugBackend backend = {};
    backend.userData = fake;
    backend.capabilities.canContinue = true;
    backend.capabilities.canStepOut = true;
    backend.poll = stepOutPoll;
    backend.readMemory = stepOutRead;
    backend.readTargetMemory = stepOutReadTarget;
    backend.bindSoftwareBreakpoint = stepOutBind;
    backend.debugCommand = stepOutCommand;
    backend.stepOutReturn = stepOutReturn;
    backend.resumeExecution = stepOutResume;
    return backend;
}

static bool poll(void* userData, uint64_t generation, DebugBackendSnapshot* snapshot) {
    StepFake* fake = static_cast<StepFake*>(userData);
    *snapshot = DebugBackendSnapshot();
    snapshot->sessionGeneration = generation;
    snapshot->processId = 12;
    snapshot->nativeRuntimeId = 77;
    if (!fake->pending) {
        snapshot->state = DebugSessionState::Stepping;
        snapshot->executionState = DebugBackendExecutionState::UserSourceStepPending;
        snapshot->threadId = 44;
        return true;
    }
    fake->pending = false;
    snapshot->state = DebugSessionState::Stepping;
    snapshot->executionState = DebugBackendExecutionState::UserSourceStepPending;
    snapshot->singleStepTrap = true;
    snapshot->singleStepKind = static_cast<uint32_t>(HostedDebugSingleStepKind::UserSource);
    snapshot->threadId = fake->wrongThread ? 45 : 44;
    snapshot->instructionPointer = fake->nextRip;
    snapshot->registerContext = fake->lastContext;
    snapshot->registerContext.threadId = snapshot->threadId;
    snapshot->registerContext.rip = fake->nextRip;
    snapshot->registerContext.stopGeneration = fake->lastContext.stopGeneration;
    if (!fake->wrongThread && fake->nextRip == 0x104) fake->nextRip = 0x108;
    return true;
}

static bool stepInstruction(void* userData, uint64_t, const DebugRegisterContext& context,
                            uint64_t, uint64_t, uint64_t, bool) {
    StepFake* fake = static_cast<StepFake*>(userData);
    fake->lastContext = context;
    fake->pending = true;
    ++fake->stepCalls;
    return true;
}

static DebugBackend makeBackend(StepFake* fake) {
    DebugBackend backend = {};
    backend.userData = fake;
    backend.capabilities.canStepInto = true;
    backend.capabilities.canContinue = true;
    backend.poll = poll;
    backend.stepInstruction = stepInstruction;
    return backend;
}

static bool overPoll(void* userData, uint64_t generation, DebugBackendSnapshot* snapshot) {
    StepOverFake* fake = static_cast<StepOverFake*>(userData);
    *snapshot = DebugBackendSnapshot();
    snapshot->sessionGeneration = generation;
    snapshot->processId = 12;
    snapshot->nativeRuntimeId = 77;
    snapshot->threadId = 44;
    fake->trapPending = true;
    snapshot->state = DebugSessionState::Paused;
    snapshot->stopReason = DebugStopReason::Step;
    snapshot->breakpointTrap = true;
    snapshot->internalBreakpointTrap = true;
    snapshot->internalBreakpointId = 0x8000000000000001ull;
    snapshot->targetAddress = { true, 0x108 };
    snapshot->breakpointBindingId = 500;
    snapshot->instructionPointer = 0x109;
    snapshot->stopGeneration = 4;
    snapshot->executionState = DebugBackendExecutionState::PausedAtStepOver;
    snapshot->registerContext.valid = true;
    snapshot->registerContext.architecture = DebugArchitecture::Amd64;
    snapshot->registerContext.processId = 12;
    snapshot->registerContext.nativeRuntimeId = 77;
    snapshot->registerContext.threadId = 44;
    snapshot->registerContext.sessionGeneration = generation;
    snapshot->registerContext.stopGeneration = 4;
    snapshot->registerContext.rip = 0x109;
    snapshot->registerContext.rflags = 0x202;
    snapshot->registerContext.rsp = 0x700000;
    return true;
}

static bool overRead(void* userData, uint64_t, uint64_t, uint64_t, uint64_t address,
                     uint8_t* bytes, uint32_t requested, uint32_t* returned) {
    StepOverFake* fake = static_cast<StepOverFake*>(userData);
    ++fake->readCalls;
    if (address != 0x103 || requested < 5) return false;
    bytes[0] = 0xE8;
    bytes[1] = bytes[2] = bytes[3] = bytes[4] = 0;
    *returned = 5;
    return true;
}

static bool overBind(void* userData, const DebugTarget&, uint64_t, uint64_t, uint64_t,
                     const DebugBreakpoint&, DebugBackendBinding* binding) {
    StepOverFake* fake = static_cast<StepOverFake*>(userData);
    ++fake->bindCalls;
    *binding = DebugBackendBinding();
    binding->accepted = true;
    binding->bindingId = 500;
    binding->originalByte = 0x90;
    binding->installedByte = 0xCC;
    binding->originalByteValid = true;
    return true;
}

static bool overCommand(void* userData, HostedDebugCommand command, uint64_t, uint64_t,
                        uint64_t, uint64_t, uint64_t, uint64_t, const char*, uint64_t,
                        uint64_t, bool, uint64_t, uint32_t, HostedDebugResult* result) {
    StepOverFake* fake = static_cast<StepOverFake*>(userData);
    *result = HostedDebugResult();
    if (command == HostedDebugCommand::RemoveSoftwareBreakpointOwner) ++fake->removeCalls;
    result->status = command == HostedDebugCommand::RemoveSoftwareBreakpointOwner ? 4 : 1;
    return true;
}

static bool overCall(void* userData, uint64_t, const DebugRegisterContext&, uint64_t,
                     uint64_t, uint64_t) {
    ++static_cast<StepOverFake*>(userData)->callCalls;
    return true;
}

static DebugBackend makeStepOverBackend(StepOverFake* fake) {
    DebugBackend backend = {};
    backend.userData = fake;
    backend.capabilities.canContinue = true;
    backend.capabilities.canStepOver = true;
    backend.poll = overPoll;
    backend.readMemory = overRead;
    backend.bindSoftwareBreakpoint = overBind;
    backend.debugCommand = overCommand;
    backend.stepOverCall = overCall;
    return backend;
}

static void prepareMapper(DebugDwarfMapper* mapper) {
    *mapper = DebugDwarfMapper();
    mapper->state = DebugDwarfMapperState::Ready;
    mapper->sourceFileCount = 2;
    std::strcpy(mapper->sourceFiles[0].relativePath, "src/main.cpp");
    std::strcpy(mapper->sourceFiles[1].relativePath, "src/math.cpp");
    mapper->lineRowCount = 4;
    mapper->addressOrderCount = 4;
    mapper->rows[0].sourceFileIndex = 0;
    mapper->rows[0].line = 10;
    mapper->rows[0].address = 0x100;
    mapper->rows[0].endAddress = 0x104;
    mapper->rows[0].sequence = 0;
    mapper->rows[1].sourceFileIndex = 0;
    mapper->rows[1].line = 10;
    mapper->rows[1].address = 0x104;
    mapper->rows[1].endAddress = 0x108;
    mapper->rows[1].sequence = 0;
    mapper->rows[2].sourceFileIndex = 0;
    mapper->rows[2].line = 11;
    mapper->rows[2].address = 0x108;
    mapper->rows[2].endAddress = 0x10c;
    mapper->rows[2].sequence = 0;
    mapper->rows[3].sourceFileIndex = 1;
    mapper->rows[3].line = 4;
    mapper->rows[3].address = 0x200;
    mapper->rows[3].endAddress = 0x204;
    mapper->rows[3].sequence = 1;
    for (uint32_t i = 0; i < 4; ++i) mapper->addressOrder[i] = i;
    mapper->executableSegmentCount = 1;
    mapper->executableSegments[0].startAddress = 0x100;
    mapper->executableSegments[0].endAddress = 0x300;
}

static void preparePausedController(DebugController* controller, bool canStep) {
    assert(DebugControllerInit(controller));
    controller->active = true;
    controller->state = DebugSessionState::Paused;
    controller->sessionGeneration = 9;
    controller->processId = 12;
    controller->nativeRuntimeId = 77;
    controller->currentThreadId = 44;
    controller->stopGeneration = 3;
    controller->currentInstructionAddress = { true, 0x100 };
    std::strcpy(controller->currentLocation.relativePath, "src/main.cpp");
    controller->currentLocation.line = 10;
    controller->currentLocation.mapping = DebugMappingState::Mapped;
    controller->stopReason = DebugStopReason::Step;
    controller->backendExecutionState = DebugBackendExecutionState::PausedAtSourceStep;
    controller->capabilities.canStepInto = canStep;
    controller->capabilities.canContinue = true;
    controller->stoppedContext.valid = true;
    controller->stoppedContext.architecture = DebugArchitecture::Amd64;
    controller->stoppedContext.processId = 12;
    controller->stoppedContext.nativeRuntimeId = 77;
    controller->stoppedContext.threadId = 44;
    controller->stoppedContext.sessionGeneration = 9;
    controller->stoppedContext.stopGeneration = 3;
    controller->stoppedContext.rip = 0x100;
    controller->stoppedContext.rflags = 0x202;
}

} // namespace

int main() {
    {
        DebugAmd64Instruction decoded = {};
        const uint8_t direct[] = { 0xE8, 0, 0, 0, 0 };
        assert(DebugDecodeAmd64Instruction(direct, sizeof(direct), 0x100, 0x200, &decoded));
        assert(decoded.kind == DebugAmd64InstructionKind::Call && decoded.instructionLength == 5 &&
               decoded.returnAddress == 0x105);
        const uint8_t registerCall[] = { 0x48, 0xFF, 0xD0 };
        assert(DebugDecodeAmd64Instruction(registerCall, sizeof(registerCall), 0x200, 0x300, &decoded));
        assert(decoded.kind == DebugAmd64InstructionKind::Call && decoded.instructionLength == 3 &&
               decoded.returnAddress == 0x203);
        const uint8_t memoryCall[] = { 0xFF, 0x15, 0, 0, 0, 0 };
        assert(DebugDecodeAmd64Instruction(memoryCall, sizeof(memoryCall), 0x300, 0x400, &decoded));
        assert(decoded.kind == DebugAmd64InstructionKind::Call && decoded.instructionLength == 6);
        const uint8_t nonCall[] = { 0x90 };
        assert(DebugDecodeAmd64Instruction(nonCall, sizeof(nonCall), 0x400, 0x500, &decoded) &&
               decoded.kind == DebugAmd64InstructionKind::NonCall);
        const uint8_t truncatedDirect[] = { 0xE8, 0, 0 };
        assert(!DebugDecodeAmd64Instruction(truncatedDirect, sizeof(truncatedDirect), 0x500, 0x600, &decoded));
        const uint8_t truncatedIndirect[] = { 0xFF };
        assert(!DebugDecodeAmd64Instruction(truncatedIndirect, sizeof(truncatedIndirect), 0x500, 0x600, &decoded));
        const uint8_t unsupported[] = { 0xFF, 0xE0 };
        assert(DebugDecodeAmd64Instruction(unsupported, sizeof(unsupported), 0x500, 0x600, &decoded) &&
               decoded.kind == DebugAmd64InstructionKind::Unsupported);
        const uint8_t patched[] = { 0xCC, 0, 0, 0, 0 };
        assert(DebugDecodeAmd64Instruction(patched, sizeof(patched), 0x600, 0x700, &decoded) &&
               decoded.kind == DebugAmd64InstructionKind::NonCall);
        assert(!DebugDecodeAmd64Instruction(direct, sizeof(direct), 0x6FD, 0x700, &decoded));
    }
    static DebugDwarfMapper mapper = {};
    prepareMapper(&mapper);

    DebugController unavailable = {};
    preparePausedController(&unavailable, false);
    assert(!DebugControllerCanStepInto(&unavailable));

    StepFake fake;
    DebugBackend backend = makeBackend(&fake);
    DebugController controller = {};
    preparePausedController(&controller, true);
    DebugErrorCode error = DebugErrorCode::None;
    assert(DebugControllerCanStepInto(&controller));
    assert(DebugControllerStepInto(&controller, backend, &mapper, &error));
    assert(controller.state == DebugSessionState::Stepping);
    assert(!DebugRegisterContextIsValid(controller.stoppedContext));
    assert(controller.sourceStep.active && controller.sourceStep.stepCount == 0);
    assert(DebugControllerPoll(&controller, backend, &mapper));
    assert(controller.state == DebugSessionState::Stepping);
    assert(controller.sourceStep.active && controller.sourceStep.stepCount == 1);
    assert(fake.stepCalls == 2); // Same-line instruction was suppressed.
    assert(DebugControllerPoll(&controller, backend, &mapper));
    assert(controller.state == DebugSessionState::Paused);
    assert(controller.stopReason == DebugStopReason::Step);
    assert(!controller.sourceStep.active);
    assert(controller.sourceStep.status == DebugSourceStepStatus::Completed);
    assert(controller.currentLocation.line == 11);
    assert(controller.currentInstructionAddress.value == 0x108);
    assert(controller.stoppedContext.rip == 0x108 && controller.stoppedContext.rflags == 0x202);
    assert(DebugControllerCanStepInto(&controller));
    assert(DebugControllerCanContinue(&controller));

    DebugController stale = {};
    preparePausedController(&stale, true);
    StepFake staleFake;
    staleFake.wrongThread = true;
    DebugBackend staleBackend = makeBackend(&staleFake);
    assert(DebugControllerStepInto(&stale, staleBackend, &mapper, &error));
    assert(DebugControllerPoll(&stale, staleBackend, &mapper));
    assert(stale.state == DebugSessionState::Failed);
    assert(stale.error == DebugErrorCode::WrongStepThread);

    StepOverFake overFake;
    DebugBackend overBackend = makeStepOverBackend(&overFake);
    DebugController overController = {};
    preparePausedController(&overController, true);
    overController.capabilities.canStepOver = true;
    overController.currentInstructionAddress = { true, 0x103 };
    overController.stoppedContext.rip = 0x103;
    assert(DebugControllerCanStepOver(&overController));
    assert(DebugControllerStepOver(&overController, overBackend, &mapper, &error));
    assert(overController.state == DebugSessionState::Stepping && overController.stepOver.active);
    assert(overFake.bindCalls == 1 && overFake.callCalls == 1);
    assert(DebugControllerPoll(&overController, overBackend, &mapper));
    assert(overController.state == DebugSessionState::Paused && overController.stopReason == DebugStopReason::Step);
    assert(overController.currentLocation.line == 11 && overController.currentInstructionAddress.value == 0x108);
    assert(!overController.stepOver.active && overController.stepOver.status == DebugStepOverStatus::Completed);
    assert(overFake.removeCalls == 1);

    StepOutFake outFake;
    DebugBackend outBackend = makeStepOutBackend(&outFake);
    DebugController outController = {};
    preparePausedController(&outController, true);
    outController.capabilities.canStepOut = true;
    outController.currentInstructionAddress = { true, 0x100 };
    outController.currentLocation.line = 10;
    outController.stoppedContext.rip = 0x100;
    outController.stoppedContext.rsp = 0x700080;
    outController.stoppedContext.rbp = 0x700100;
    outController.stoppedContext.stackLow = 0x700000;
    outController.stoppedContext.stackHigh = 0x702000;
    outController.callStack.valid = true;
    outController.callStack.sessionGeneration = 9;
    outController.callStack.processId = 12;
    outController.callStack.nativeRuntimeId = 77;
    outController.callStack.threadId = 44;
    outController.callStack.stopGeneration = 3;
    outController.callStack.result.frameCount = 3;
    outController.callStack.result.frames[0].current = true;
    std::strcpy(outController.callStack.result.frames[0].functionName, "level3");
    outController.callStack.result.frames[0].sourceLine = 10;
    outController.callStack.result.frames[1].hasReturnAddress = true;
    outController.callStack.result.frames[1].confidence = DebugStackFrameConfidence::FramePointer;
    outController.callStack.result.frames[1].rawReturnAddress = 0x201;
    outController.callStack.result.frames[1].lookupAddress = 0x200;
    outController.callStack.result.frames[1].mapping = DebugStackFrameMappingState::Mapped;
    std::strcpy(outController.callStack.result.frames[1].functionName, "level2");
    std::strcpy(outController.callStack.result.frames[1].sourcePath, "src/math.cpp");
    outController.callStack.result.frames[1].sourceLine = 4;
    outController.callStack.result.frames[2].hasReturnAddress = true;
    outController.callStack.result.frames[2].confidence = DebugStackFrameConfidence::FramePointer;
    outController.callStack.result.frames[2].rawReturnAddress = 0x220;
    outController.callStack.selectedFrameIndex = 0;
    assert(DebugControllerSelectCallStackFrame(&outController, 2, &error));
    assert(DebugControllerCanStepOut(&outController));
    assert(DebugControllerStepOut(&outController, outBackend, &mapper, &error));
    assert(outController.state == DebugSessionState::Stepping && outController.stepOut.active);
    assert(outController.stepOut.rawReturnAddress == 0x201 &&
           outController.stepOut.callerLookupAddress == 0x200 && outFake.bindCalls == 1 &&
           outFake.stepOutCalls == 1);
    assert(DebugControllerPoll(&outController, outBackend, &mapper));
    assert(outController.state == DebugSessionState::Paused && outController.stopReason == DebugStopReason::Step);
    assert(outController.backendExecutionState == DebugBackendExecutionState::PausedAtStepOut);
    assert(!outController.stepOut.active && outController.stepOut.status == DebugStepOutStatus::Completed);
    assert(outController.stepOut.rawReturnAddress == 0x201 && outController.stepOut.callerLookupAddress == 0x200);
    assert(outFake.removeCalls == 1 && outController.currentInstructionAddress.value == 0x201);
    assert(outController.currentLocation.line == 4 && outController.stoppedContext.rflags == 0x202);
    assert(outController.callStack.valid && outController.callStack.selectedFrameIndex == 0 &&
           outController.callStack.result.frameCount == 2 && outController.callStack.result.frames[0].current);
    assert(outController.callStack.result.frames[0].instructionAddress == 0x202);
    assert(DebugControllerPoll(&outController, outBackend, &mapper));
    assert(outController.error == DebugErrorCode::None && outFake.pollCalls == 2 &&
           outFake.removeCalls == 1 && outController.state == DebugSessionState::Paused &&
           outController.backendExecutionState == DebugBackendExecutionState::PausedAtStepOut);
    outController.callStack.result.frameCount = 1;
    assert(!DebugControllerCanStepOut(&outController));
    assert(!DebugControllerStepOut(&outController, outBackend, &mapper, &error));
    assert(error == DebugErrorCode::NoCallerFrame);

    outController.callStack.result.frameCount = 2;
    assert(DebugControllerCanContinue(&outController));
    assert(DebugControllerContinue(&outController, outBackend, &error));
    assert(outFake.resumeCalls == 1 && outController.state == DebugSessionState::Running &&
           outController.stopReason == DebugStopReason::None &&
           outController.backendExecutionState == DebugBackendExecutionState::Running);

    std::cout << "Developer Studio source-step model PASS\n";
    return 0;
}
