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

    std::cout << "Developer Studio source-step model PASS\n";
    return 0;
}
