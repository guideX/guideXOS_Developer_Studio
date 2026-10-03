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
    bool exitBeforeStepStop = false;
    bool pauseRequested = false;
    uint32_t stepCalls = 0;
    uint32_t resumeCalls = 0;
    uint32_t pauseCalls = 0;
    uint64_t nextRip = 0x104;
    uint64_t nextStopGeneration = 4;
    int32_t exitCode = 7;
    DebugRegisterContext lastContext = {};
};

struct StepOverFake {
    bool trapPending = false;
    bool exitBeforeStepStop = false;
    uint32_t pollCalls = 0;
    uint32_t readCalls = 0;
    uint32_t bindCalls = 0;
    uint32_t removeCalls = 0;
    uint32_t callCalls = 0;
    uint32_t resumeCalls = 0;
    uint64_t commandGeneration = 0;
};

struct StepOutFake {
    bool overlapUserBreakpoint = false;
    bool exitBeforeStepStop = false;
    uint32_t bindCalls = 0;
    uint32_t removeCalls = 0;
    uint32_t stepOutCalls = 0;
    uint32_t pollCalls = 0;
    uint32_t resumeCalls = 0;
    uint64_t returnAddress = 0x201;
    uint64_t bindingId = 600;
    uint64_t temporaryId = 0;
    uint64_t commandGeneration = 0;
};

static bool stepOutPoll(void* userData, uint64_t generation, DebugBackendSnapshot* snapshot) {
    StepOutFake* fake = static_cast<StepOutFake*>(userData);
    ++fake->pollCalls;
    *snapshot = DebugBackendSnapshot();
    snapshot->sessionGeneration = generation;
    if (fake->exitBeforeStepStop) {
        snapshot->state = DebugSessionState::Exited;
        snapshot->stopReason = DebugStopReason::Exited;
        snapshot->processId = 12;
        snapshot->nativeRuntimeId = 77;
        snapshot->exitCode = 7;
        snapshot->cleanupComplete = true;
        return true;
    }
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
    snapshot->bindingOwnerCount = fake->overlapUserBreakpoint ? 2 : 0;
    snapshot->bindingInstalled = fake->overlapUserBreakpoint;
    snapshot->instructionPointer = fake->returnAddress + 1;
    snapshot->stopGeneration = 4;
    snapshot->commandGeneration = fake->commandGeneration;
    snapshot->executionState = DebugBackendExecutionState::PausedAtStepOut;
    snapshot->registerContext.valid = true;
    snapshot->registerContext.architecture = DebugArchitecture::Amd64;
    snapshot->registerContext.processId = 12;
    snapshot->registerContext.nativeRuntimeId = 77;
    snapshot->registerContext.threadId = 44;
    snapshot->registerContext.sessionGeneration = generation;
    snapshot->registerContext.stopGeneration = 4;
    snapshot->registerContext.commandGeneration = fake->commandGeneration;
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
    binding->ownerCount = fake->overlapUserBreakpoint ? 2 : 1;
    return true;
}

static bool stepOutCommand(void* userData, HostedDebugCommand command, uint64_t, uint64_t,
                           uint64_t, uint64_t, uint64_t, uint64_t, const char*, uint64_t,
                           uint64_t, bool, uint64_t, uint32_t, uint64_t, HostedDebugResult* result) {
    StepOutFake* fake = static_cast<StepOutFake*>(userData);
    *result = HostedDebugResult();
    if (command == HostedDebugCommand::RemoveSoftwareBreakpointOwner) ++fake->removeCalls;
    result->status = command == HostedDebugCommand::RemoveSoftwareBreakpointOwner ? 4 : 1;
    result->bindingId = fake->bindingId;
    result->bindingCount = command == HostedDebugCommand::RemoveSoftwareBreakpointOwner && fake->overlapUserBreakpoint ? 1 : 0;
    result->bindingInstalled = result->bindingCount != 0;
    return true;
}

static bool stepOutReturn(void* userData, uint64_t, const DebugRegisterContext& context,
                          uint64_t, uint64_t, uint64_t, bool, uint64_t returnAddress,
                          uint64_t temporaryBreakpointId) {
    StepOutFake* fake = static_cast<StepOutFake*>(userData);
    ++fake->stepOutCalls;
    fake->returnAddress = returnAddress;
    fake->temporaryId = temporaryBreakpointId;
    fake->commandGeneration = context.commandGeneration;
    return true;
}

static bool stepOutResume(void* userData, uint64_t, const DebugRegisterContext&) {
    StepOutFake* fake = static_cast<StepOutFake*>(userData);
    ++fake->resumeCalls;
    return true;
}

static bool stepOutContinue(void* userData, uint64_t, const DebugRegisterContext&, uint64_t,
                            uint64_t, uint64_t, bool) {
    ++static_cast<StepOutFake*>(userData)->resumeCalls;
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
    backend.continueExecution = stepOutContinue;
    return backend;
}

static bool poll(void* userData, uint64_t generation, DebugBackendSnapshot* snapshot) {
    StepFake* fake = static_cast<StepFake*>(userData);
    *snapshot = DebugBackendSnapshot();
    snapshot->sessionGeneration = generation;
    snapshot->processId = 12;
    snapshot->nativeRuntimeId = 77;
    if (fake->pauseRequested) {
        fake->pauseRequested = false;
        snapshot->state = DebugSessionState::Paused;
        snapshot->stopReason = DebugStopReason::UserPause;
        snapshot->executionState = DebugBackendExecutionState::PausedAtUserPause;
        snapshot->threadId = 44;
        snapshot->stopGeneration = fake->nextStopGeneration++;
        snapshot->instructionPointer = 0x108;
        snapshot->registerContext = fake->lastContext;
        snapshot->registerContext.valid = true;
        snapshot->registerContext.threadId = 44;
        snapshot->registerContext.stopGeneration = snapshot->stopGeneration;
        snapshot->registerContext.commandGeneration = 0;
        snapshot->registerContext.rip = 0x108;
        snapshot->registerContext.rsp = 0x700000;
        snapshot->registerContext.rbp = 0x700100;
        snapshot->registerContext.stackLow = 0x700000;
        snapshot->registerContext.stackHigh = 0x702000;
        return true;
    }
    if (!fake->pending) {
        snapshot->state = DebugSessionState::Stepping;
        snapshot->executionState = DebugBackendExecutionState::UserSourceStepPending;
        snapshot->threadId = 44;
        return true;
    }
    if (fake->exitBeforeStepStop) {
        fake->pending = false;
        snapshot->state = DebugSessionState::Exited;
        snapshot->stopReason = DebugStopReason::Exited;
        snapshot->executionState = DebugBackendExecutionState::None;
        snapshot->exitCode = fake->exitCode;
        snapshot->cleanupComplete = true;
        return true;
    }
    fake->pending = false;
    snapshot->state = DebugSessionState::Stepping;
    snapshot->executionState = DebugBackendExecutionState::UserSourceStepPending;
    snapshot->singleStepTrap = true;
    snapshot->singleStepKind = static_cast<uint32_t>(HostedDebugSingleStepKind::UserSource);
    snapshot->stopGeneration = fake->nextStopGeneration++;
    snapshot->commandGeneration = fake->lastContext.commandGeneration;
    snapshot->threadId = fake->wrongThread ? 45 : 44;
    snapshot->instructionPointer = fake->nextRip;
    snapshot->registerContext = fake->lastContext;
    snapshot->registerContext.threadId = snapshot->threadId;
    snapshot->registerContext.rip = fake->nextRip;
    snapshot->registerContext.stopGeneration = snapshot->stopGeneration;
    snapshot->registerContext.commandGeneration = snapshot->commandGeneration;
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

static bool pauseExecution(void* userData, uint64_t) {
    StepFake* fake = static_cast<StepFake*>(userData);
    fake->pauseRequested = true;
    ++fake->pauseCalls;
    return true;
}

static DebugBackend makeBackend(StepFake* fake) {
    DebugBackend backend = {};
    backend.userData = fake;
    backend.capabilities.canStepInto = true;
    backend.capabilities.canContinue = true;
    backend.capabilities.canPause = true;
    backend.poll = poll;
    backend.stepInstruction = stepInstruction;
    backend.pause = pauseExecution;
    backend.resumeExecution = [](void* userData, uint64_t, const DebugRegisterContext&) {
        ++static_cast<StepFake*>(userData)->resumeCalls;
        return true;
    };
    return backend;
}

static bool overPoll(void* userData, uint64_t generation, DebugBackendSnapshot* snapshot) {
    StepOverFake* fake = static_cast<StepOverFake*>(userData);
    ++fake->pollCalls;
    *snapshot = DebugBackendSnapshot();
    snapshot->sessionGeneration = generation;
    if (fake->exitBeforeStepStop) {
        snapshot->state = DebugSessionState::Exited;
        snapshot->stopReason = DebugStopReason::Exited;
        snapshot->processId = 12;
        snapshot->nativeRuntimeId = 77;
        snapshot->exitCode = 7;
        snapshot->cleanupComplete = true;
        return true;
    }
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
    snapshot->commandGeneration = fake->commandGeneration;
    snapshot->executionState = DebugBackendExecutionState::PausedAtStepOver;
    snapshot->registerContext.valid = true;
    snapshot->registerContext.architecture = DebugArchitecture::Amd64;
    snapshot->registerContext.processId = 12;
    snapshot->registerContext.nativeRuntimeId = 77;
    snapshot->registerContext.threadId = 44;
    snapshot->registerContext.sessionGeneration = generation;
    snapshot->registerContext.stopGeneration = 4;
    snapshot->registerContext.commandGeneration = fake->commandGeneration;
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
                        uint64_t, bool, uint64_t, uint32_t, uint64_t, HostedDebugResult* result) {
    StepOverFake* fake = static_cast<StepOverFake*>(userData);
    *result = HostedDebugResult();
    if (command == HostedDebugCommand::RemoveSoftwareBreakpointOwner) ++fake->removeCalls;
    result->status = command == HostedDebugCommand::RemoveSoftwareBreakpointOwner ? 4 : 1;
    return true;
}

static bool overCall(void* userData, uint64_t, const DebugRegisterContext& context, uint64_t,
                     uint64_t, uint64_t) {
    StepOverFake* fake = static_cast<StepOverFake*>(userData);
    ++fake->callCalls;
    fake->commandGeneration = context.commandGeneration;
    return true;
}

static bool overResume(void* userData, uint64_t, const DebugRegisterContext&) {
    ++static_cast<StepOverFake*>(userData)->resumeCalls;
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
    backend.resumeExecution = overResume;
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
    controller->lastStopGeneration = 3;
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
    const uint64_t stepIntoCommandGeneration = controller.sourceStep.commandGeneration;
    assert(stepIntoCommandGeneration == 1 && fake.lastContext.commandGeneration == stepIntoCommandGeneration);
    assert(controller.state == DebugSessionState::Stepping);
    assert(!DebugRegisterContextIsValid(controller.stoppedContext));
    assert(controller.sourceStep.active && controller.sourceStep.stepCount == 0);
    controller.capabilities.canStepOver = true;
    const uint32_t stepCallsBeforeRejectedCommands = fake.stepCalls;
    DebugBackend unavailableWhileStepping = {};
    assert(!DebugControllerCanStepInto(&controller) && !DebugControllerCanStepOver(&controller) &&
           !DebugControllerCanContinue(&controller));
    assert(!DebugControllerStepInto(&controller, backend, &mapper, &error));
    assert(!DebugControllerStepOver(&controller, unavailableWhileStepping, &mapper, &error));
    assert(!DebugControllerContinue(&controller, backend, &error));
    assert(controller.sourceStep.active && controller.sourceStep.commandGeneration == stepIntoCommandGeneration &&
           fake.stepCalls == stepCallsBeforeRejectedCommands);
    assert(!DebugControllerPause(&controller, backend, &error));
    assert(error == DebugErrorCode::TargetNotRunning && fake.pauseCalls == 0 &&
           controller.sourceStep.active && controller.sourceStep.commandGeneration == stepIntoCommandGeneration);
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
    assert(controller.stopGeneration == 5 && controller.lastStopGeneration == 5 &&
           controller.stoppedContext.stopGeneration == 5 &&
           controller.stoppedContext.commandGeneration == stepIntoCommandGeneration &&
           controller.reportedInstructionPointer == 0x108 &&
           controller.currentInstructionAddress.value == controller.stoppedContext.rip &&
           controller.stepCompletionGeneration == 5);
    assert(DebugControllerCanStepInto(&controller));
    assert(DebugControllerCanContinue(&controller));
    assert(DebugControllerContinue(&controller, backend, &error));
    assert(fake.resumeCalls == 1 && controller.state == DebugSessionState::Running &&
           controller.stopReason == DebugStopReason::None &&
           controller.backendExecutionState == DebugBackendExecutionState::Running &&
           controller.stopGeneration == 0 && controller.lastStopGeneration == 5 &&
           !controller.stoppedContext.valid && !controller.callStack.valid && !controller.variables.valid);
    assert(DebugControllerPause(&controller, backend, &error));
    assert(fake.pauseCalls == 1 && fake.pauseRequested && controller.pauseRequestPending);
    assert(DebugControllerPoll(&controller, backend, &mapper));
    assert(controller.state == DebugSessionState::Paused && controller.stopReason == DebugStopReason::UserPause &&
           controller.backendExecutionState == DebugBackendExecutionState::PausedAtUserPause &&
           controller.stopGeneration == 6 && controller.lastStopGeneration == 6 &&
           controller.stoppedContext.valid && controller.stoppedContext.commandGeneration == 0 &&
           !controller.sourceStep.active && controller.sourceStep.status == DebugSourceStepStatus::Cancelled &&
           controller.stepCompletionGeneration == 5 && !controller.pauseRequestPending && fake.stepCalls == 2);

    static DebugController exitDuringStep = {};
    preparePausedController(&exitDuringStep, true);
    StepFake exitFake;
    exitFake.exitBeforeStepStop = true;
    DebugBackend exitBackend = makeBackend(&exitFake);
    assert(DebugControllerStepInto(&exitDuringStep, exitBackend, &mapper, &error));
    assert(exitDuringStep.state == DebugSessionState::Stepping && exitDuringStep.sourceStep.active);
    assert(DebugControllerPoll(&exitDuringStep, exitBackend, &mapper));
    assert(exitDuringStep.state == DebugSessionState::Exited);
    assert(exitDuringStep.exitCode == 7 && exitDuringStep.cleanupComplete);
    assert(!exitDuringStep.active);
    assert(exitDuringStep.stopReason == DebugStopReason::Exited);
    assert(exitDuringStep.sourceStep.status == DebugSourceStepStatus::Cancelled);
    assert(!exitDuringStep.sourceStep.active);
    assert(exitDuringStep.stopGeneration == 0);
    assert(exitDuringStep.lastStopGeneration == 3);
    assert(exitDuringStep.stepCompletionGeneration == 0);
    assert(!exitDuringStep.stoppedContext.valid);
    assert(!exitDuringStep.callStack.valid);
    assert(!exitDuringStep.variables.valid);

    DebugController stale = {};
    preparePausedController(&stale, true);
    StepFake staleFake;
    staleFake.wrongThread = true;
    DebugBackend staleBackend = makeBackend(&staleFake);
    assert(DebugControllerStepInto(&stale, staleBackend, &mapper, &error));
    assert(DebugControllerPoll(&stale, staleBackend, &mapper));
    assert(stale.state == DebugSessionState::Failed);
    assert(stale.error == DebugErrorCode::WrongStepThread);

    DebugController staleCommand = {};
    preparePausedController(&staleCommand, true);
    StepFake staleCommandFake;
    DebugBackend staleCommandBackend = makeBackend(&staleCommandFake);
    assert(DebugControllerStepInto(&staleCommand, staleCommandBackend, &mapper, &error));
    const uint64_t requestedCommandGeneration = staleCommand.sourceStep.commandGeneration;
    staleCommandFake.lastContext.commandGeneration = requestedCommandGeneration + 1;
    assert(DebugControllerPoll(&staleCommand, staleCommandBackend, &mapper));
    assert(staleCommand.state == DebugSessionState::Failed &&
           staleCommand.error == DebugErrorCode::StaleSourceStep &&
           staleCommand.stopGeneration == 0 && !staleCommand.stoppedContext.valid);

    DebugController staleStop = {};
    preparePausedController(&staleStop, true);
    staleStop.stopGeneration = 4;
    assert(!DebugControllerCanStepInto(&staleStop));
    StepFake staleStopFake;
    DebugBackend staleStopBackend = makeBackend(&staleStopFake);
    assert(!DebugControllerStepInto(&staleStop, staleStopBackend, &mapper, &error));
    assert(error == DebugErrorCode::StaleStopContext && !staleStop.sourceStep.active);

    StepOverFake overFake;
    DebugBackend overBackend = makeStepOverBackend(&overFake);
    DebugController overController = {};
    preparePausedController(&overController, true);
    overController.capabilities.canStepOver = true;
    overController.currentInstructionAddress = { true, 0x103 };
    overController.stoppedContext.rip = 0x103;
    assert(DebugControllerCanStepOver(&overController));
    assert(DebugControllerStepOver(&overController, overBackend, &mapper, &error));
    const uint64_t stepOverCommandGeneration = overController.stepOver.commandGeneration;
    assert(overController.state == DebugSessionState::Stepping && overController.stepOver.active);
    assert(stepOverCommandGeneration == 1 && overFake.bindCalls == 1 && overFake.callCalls == 1 &&
           overFake.commandGeneration == stepOverCommandGeneration &&
           overController.stepOver.mode == DebugStepOverMode::TemporaryReturnBreakpoint &&
           overController.stepOver.temporaryBreakpointId >= 0x8000000000000001ull &&
           overController.stepOver.returnAddress == 0x108);
    assert(DebugControllerPoll(&overController, overBackend, &mapper));
    assert(overController.state == DebugSessionState::Paused && overController.stopReason == DebugStopReason::Step);
    assert(overController.currentLocation.line == 11 && overController.currentInstructionAddress.value == 0x108);
    assert(!overController.stepOver.active && overController.stepOver.status == DebugStepOverStatus::Completed);
    assert(overFake.removeCalls == 1 && !overController.stepOver.temporaryInstalled &&
           overController.stopGeneration == 4 && overController.lastStopGeneration == 4 &&
           overController.stoppedContext.commandGeneration == stepOverCommandGeneration &&
           overController.reportedInstructionPointer == 0x109 &&
           overController.currentInstructionAddress.value == 0x108 &&
           overController.stepCompletionGeneration == 4);
    assert(DebugControllerCanContinue(&overController));
    assert(DebugControllerContinue(&overController, overBackend, &error));
    assert(overFake.resumeCalls == 1 && overController.state == DebugSessionState::Running &&
           overController.stopReason == DebugStopReason::None &&
           overController.backendExecutionState == DebugBackendExecutionState::Running &&
           overController.stopGeneration == 0 && !overController.callStack.valid &&
           !overController.variables.valid);

    static DebugController exitDuringStepOver = {};
    preparePausedController(&exitDuringStepOver, true);
    exitDuringStepOver.capabilities.canStepOver = true;
    exitDuringStepOver.currentInstructionAddress = { true, 0x103 };
    exitDuringStepOver.stoppedContext.rip = 0x103;
    StepOverFake exitOverFake;
    exitOverFake.exitBeforeStepStop = true;
    DebugBackend exitOverBackend = makeStepOverBackend(&exitOverFake);
    assert(DebugControllerStepOver(&exitDuringStepOver, exitOverBackend, &mapper, &error));
    const uint64_t exitOverGeneration = exitDuringStepOver.stepOver.commandGeneration;
    assert(exitDuringStepOver.state == DebugSessionState::Stepping &&
           exitDuringStepOver.stepOver.active && exitDuringStepOver.stepOver.temporaryInstalled);
    assert(DebugControllerPoll(&exitDuringStepOver, exitOverBackend, &mapper));
    assert(exitDuringStepOver.state == DebugSessionState::Exited && !exitDuringStepOver.active &&
           exitDuringStepOver.exitCode == 7 && exitDuringStepOver.cleanupComplete &&
           exitDuringStepOver.stepOver.commandGeneration == exitOverGeneration &&
           !exitDuringStepOver.stepOver.active && !exitDuringStepOver.stepOver.temporaryInstalled &&
           exitDuringStepOver.stepOver.temporaryBreakpointId == 0 &&
           exitDuringStepOver.stepOver.status == DebugStepOverStatus::Cancelled &&
           exitDuringStepOver.stepCompletionGeneration == 0);
    uint32_t exitOverCompletionEvents = 0;
    for (uint32_t i = 0; i < exitDuringStepOver.eventCount; ++i) {
        const DebugEvent* event = DebugControllerEventAt(&exitDuringStepOver, i);
        if (event && event->kind == DebugEventKind::StepOverCompleted) ++exitOverCompletionEvents;
    }
    assert(exitOverFake.pollCalls == 1 && exitOverCompletionEvents == 0);

    DebugController staleOverCommand = {};
    preparePausedController(&staleOverCommand, true);
    staleOverCommand.capabilities.canStepOver = true;
    staleOverCommand.currentInstructionAddress = { true, 0x103 };
    staleOverCommand.stoppedContext.rip = 0x103;
    StepOverFake staleOverFake;
    DebugBackend staleOverBackend = makeStepOverBackend(&staleOverFake);
    assert(DebugControllerStepOver(&staleOverCommand, staleOverBackend, &mapper, &error));
    staleOverFake.commandGeneration = staleOverCommand.stepOver.commandGeneration + 1;
    assert(!DebugControllerPoll(&staleOverCommand, staleOverBackend, &mapper));
    assert(staleOverCommand.error == DebugErrorCode::StaleStepOver &&
           !staleOverCommand.stepOver.active && staleOverFake.removeCalls == 1);

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
    DebugController overlapController = outController;
    StepOutFake overlapFake;
    overlapFake.overlapUserBreakpoint = true;
    DebugBackend overlapBackend = makeStepOutBackend(&overlapFake);
    overlapController.breakpointCount = 1;
    overlapController.breakpoints[0] = DebugBreakpoint();
    overlapController.breakpoints[0].id = 700;
    overlapController.breakpoints[0].enabled = true;
    overlapController.breakpoints[0].state = DebugBreakpointState::Mapped;
    overlapController.breakpoints[0].sessionGeneration = overlapController.sessionGeneration;
    overlapController.breakpoints[0].backendBindingId = overlapFake.bindingId;
    overlapController.breakpoints[0].location.instructionAddress = { true, overlapFake.returnAddress };
    assert(DebugControllerCanStepOut(&overlapController));
    assert(DebugControllerStepOut(&overlapController, overlapBackend, &mapper, &error));
    assert(overlapFake.bindCalls == 1 && overlapController.lastBindingOwnerCount == 2);
    assert(DebugControllerPoll(&overlapController, overlapBackend, &mapper));
    assert(overlapController.stopReason == DebugStopReason::Breakpoint &&
           overlapController.backendExecutionState == DebugBackendExecutionState::PausedAtBreakpoint &&
           !overlapController.stepOut.active && overlapController.stepCleanupCount == 1 &&
           overlapController.lastBindingOwnerCount == 1 && overlapController.lastBindingInstalled);
    assert(DebugControllerPoll(&overlapController, overlapBackend, &mapper));
    assert(overlapFake.removeCalls == 1 && overlapController.stepCleanupCount == 1 &&
           overlapController.error == DebugErrorCode::None &&
           overlapController.stopReason == DebugStopReason::Breakpoint &&
           overlapController.backendExecutionState == DebugBackendExecutionState::PausedAtBreakpoint);
    assert(DebugControllerCanContinue(&overlapController));
    assert(DebugControllerContinue(&overlapController, overlapBackend, &error));
    assert(overlapFake.resumeCalls == 1 &&
           overlapController.backendExecutionState == DebugBackendExecutionState::SingleStepPending);
    assert(DebugControllerCanStepOut(&outController));

    static DebugController exitDuringStepOut = {};
    exitDuringStepOut = outController;
    StepOutFake exitOutFake;
    exitOutFake.exitBeforeStepStop = true;
    DebugBackend exitOutBackend = makeStepOutBackend(&exitOutFake);
    assert(DebugControllerStepOut(&exitDuringStepOut, exitOutBackend, &mapper, &error));
    const uint64_t exitOutGeneration = exitDuringStepOut.stepOut.commandGeneration;
    assert(exitDuringStepOut.state == DebugSessionState::Stepping &&
           exitDuringStepOut.stepOut.active && exitDuringStepOut.stepOut.temporaryInstalled);
    assert(DebugControllerPoll(&exitDuringStepOut, exitOutBackend, &mapper));
    assert(exitDuringStepOut.state == DebugSessionState::Exited && !exitDuringStepOut.active &&
           exitDuringStepOut.exitCode == 7 && exitDuringStepOut.cleanupComplete &&
           exitDuringStepOut.stepOut.commandGeneration == exitOutGeneration &&
           !exitDuringStepOut.stepOut.active && !exitDuringStepOut.stepOut.temporaryInstalled &&
           exitDuringStepOut.stepOut.temporaryBreakpointId == 0 &&
           exitDuringStepOut.stepOut.status == DebugStepOutStatus::Cancelled &&
           exitDuringStepOut.stepCompletionGeneration == 0);
    uint32_t exitOutCompletionEvents = 0;
    for (uint32_t i = 0; i < exitDuringStepOut.eventCount; ++i) {
        const DebugEvent* event = DebugControllerEventAt(&exitDuringStepOut, i);
        if (event && event->kind == DebugEventKind::StepOutCompleted) ++exitOutCompletionEvents;
    }
    assert(exitOutFake.pollCalls == 1 && exitOutCompletionEvents == 0);

    assert(DebugControllerStepOut(&outController, outBackend, &mapper, &error));
    const uint64_t stepOutCommandGeneration = outController.stepOut.commandGeneration;
    assert(outController.state == DebugSessionState::Stepping && outController.stepOut.active);
    assert(outController.stepOut.rawReturnAddress == 0x201 &&
           outController.stepOut.callerLookupAddress == 0x200 && outFake.bindCalls == 1 &&
           outFake.stepOutCalls == 1 && stepOutCommandGeneration == 1 &&
           outFake.commandGeneration == stepOutCommandGeneration &&
           outController.stepOut.temporaryBreakpointId == outFake.temporaryId);
    assert(DebugControllerPoll(&outController, outBackend, &mapper));
    assert(outController.state == DebugSessionState::Paused && outController.stopReason == DebugStopReason::Step);
    assert(outController.backendExecutionState == DebugBackendExecutionState::PausedAtStepOut);
    assert(!outController.stepOut.active && outController.stepOut.status == DebugStepOutStatus::Completed);
    assert(outController.stepOut.rawReturnAddress == 0x201 && outController.stepOut.callerLookupAddress == 0x200);
    assert(outFake.removeCalls == 1 && !outController.stepOut.temporaryInstalled &&
           outController.currentInstructionAddress.value == 0x201 &&
           outController.reportedInstructionPointer == 0x202 &&
           outController.stoppedContext.commandGeneration == stepOutCommandGeneration &&
           outController.stopGeneration == 4 && outController.lastStopGeneration == 4);
    assert(outController.stepCleanupCount == 1 && outController.stepCompletionGeneration == 4);
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
    assert(!DebugControllerContinue(&outController, outBackend, &error));
    assert(outController.lastRejectedTransition != DebugErrorCode::None);

    std::cout << "Developer Studio source-step model PASS\n";
    return 0;
}
