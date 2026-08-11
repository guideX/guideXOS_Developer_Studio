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

    std::cout << "Developer Studio source-step model PASS\n";
    return 0;
}
