#include "developer_studio_debugger.h"

#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>

using namespace guidexos::developer_studio;

struct Memory {
    uint64_t base = 0x700000;
    uint8_t bytes[0x30000] = {};
    uint32_t reads = 0;
    uint32_t writes = 0;
    uint32_t targetCalls = 0;
};

struct FakeBackend {
    Memory* memory = nullptr;
    uint64_t inspectAddress = 0;
    uint32_t polls = 0;
    uint32_t trapRequests = 0;
    uint32_t trapsEmitted = 0;
    bool continuePending = false;
    uint32_t continueCalls = 0;
    uint32_t restoreCommands = 0;
    uint32_t bindCalls = 0;
    uint32_t physicalBinds = 0;
    uint64_t sharedBindingId = 9001;
};

static bool readMemoryImpl(Memory* memory, uint64_t sessionGeneration, uint64_t processId,
                           uint64_t runtimeId, uint64_t threadId, uint64_t stopGeneration,
                           uint64_t address, uint8_t* bytes, uint32_t requested,
                           uint32_t* returned) {
    if (returned) *returned = 0;
    if (!memory || sessionGeneration == 0 || processId != 12 || runtimeId != 77 ||
        threadId != 44 || stopGeneration == 0 || !bytes || requested == 0 ||
        address < memory->base || requested > sizeof(memory->bytes) ||
        address - memory->base > sizeof(memory->bytes) - requested) {
        return false;
    }
    ++memory->reads;
    for (uint32_t i = 0; i < requested; ++i) bytes[i] = memory->bytes[address - memory->base + i];
    if (returned) *returned = requested;
    return true;
}

static bool readMemoryDirect(void* userData, uint64_t sessionGeneration, uint64_t processId,
                             uint64_t runtimeId, uint64_t threadId, uint64_t stopGeneration,
                             uint64_t address, uint8_t* bytes, uint32_t requested,
                             uint32_t* returned) {
    return readMemoryImpl(static_cast<Memory*>(userData), sessionGeneration, processId,
                          runtimeId, threadId, stopGeneration, address, bytes, requested, returned);
}

static bool readTargetMemory(void* userData, uint64_t sessionGeneration, uint64_t processId,
                             uint64_t runtimeId, uint64_t threadId, uint64_t stopGeneration,
                             uint64_t address, uint8_t* bytes, uint32_t requested,
                             uint32_t* returned) {
    FakeBackend* fake = static_cast<FakeBackend*>(userData);
    return readMemoryImpl(fake ? fake->memory : nullptr, sessionGeneration, processId,
                          runtimeId, threadId, stopGeneration, address, bytes, requested, returned);
}

static std::vector<unsigned char> readFile(const char* path, const char* fallback) {
    std::ifstream input(path, std::ios::binary);
    if (!input.good()) { input.clear(); input.open(fallback, std::ios::binary); }
    assert(input.good());
    input.seekg(0, std::ios::end);
    const std::streamsize size = input.tellg();
    input.seekg(0, std::ios::beg);
    std::vector<unsigned char> bytes(static_cast<size_t>(size));
    input.read(reinterpret_cast<char*>(bytes.data()), size);
    assert(input.good() || input.eof());
    return bytes;
}

static void put32(Memory* memory, uint64_t address, uint32_t value) {
    assert(memory && address >= memory->base && address - memory->base + 4 <= sizeof(memory->bytes));
    for (uint32_t i = 0; i < 4; ++i) memory->bytes[address - memory->base + i] = static_cast<uint8_t>(value >> (i * 8));
}

static void put64(Memory* memory, uint64_t address, uint64_t value) {
    assert(memory && address >= memory->base && address - memory->base + 8 <= sizeof(memory->bytes));
    for (uint32_t i = 0; i < 8; ++i) memory->bytes[address - memory->base + i] = static_cast<uint8_t>(value >> (i * 8));
}

static uint64_t findFunctionAddress(const DebugDwarfMapper& mapper, const char* name) {
    for (uint32_t i = 0; i < mapper.debugInfoFunctionCount; ++i)
        if (std::strcmp(mapper.debugFunctions[i].name, name) == 0) return mapper.debugFunctions[i].lowPc;
    return 0;
}

static const DebugDwarfVariable* variableNamed(const DebugDwarfVariableView& view, const char* name) {
    for (uint32_t i = 0; i < view.variableCount; ++i)
        if (std::strcmp(view.variables[i].name, name) == 0) return &view.variables[i];
    return nullptr;
}

static uint64_t variableAddress(const DebugDwarfMapper& mapper, const DebugDwarfVariable& variable,
                                uint64_t frameBase) {
    for (uint32_t i = 0; i < mapper.debugInfoVariableCount; ++i) {
        if (mapper.debugVariables[i].dieOffset != variable.dieOffset) continue;
        const DebugDwarfDieInfo& die = mapper.dies[mapper.debugVariables[i].dieIndex];
        assert(die.hasLocation && !die.locationIsList && die.locationLength >= 2 && die.location[0] == 0x91);
        int64_t value = 0;
        uint32_t shift = 0;
        uint32_t cursor = 1;
        uint8_t byte = 0;
        do {
            assert(cursor < die.locationLength);
            byte = die.location[cursor++];
            value |= static_cast<int64_t>(byte & 0x7f) << shift;
            shift += 7;
        } while ((byte & 0x80) != 0 && shift < 64);
        if (shift < 64 && (byte & 0x40) != 0) value |= static_cast<int64_t>(UINT64_MAX << shift);
        return value >= 0 ? frameBase + static_cast<uint64_t>(value) : frameBase - static_cast<uint64_t>(-(value + 1)) - 1u;
    }
    assert(false);
    return 0;
}

static DebugDwarfMemberInfo memberNamed(const DebugDwarfMapper& mapper, uint64_t typeOffset,
                                        const char* name) {
    for (uint32_t depth = 0; depth < kDebugDwarfMaxTypeDepth; ++depth) {
        const DebugDwarfDieInfo* die = nullptr;
        for (uint32_t i = 0; i < mapper.debugInfoDieCount; ++i)
            if (mapper.dies[i].offset == typeOffset) { die = &mapper.dies[i]; break; }
        assert(die);
        if (die->tag == 0x0fu || die->tag == 0x16u || die->tag == 0x26u || die->tag == 0x35u) {
            assert(die->hasType);
            typeOffset = die->typeReference;
        } else break;
    }
    DebugDwarfTypeInfo type = {};
    assert(DebugDwarfDescribeType(&mapper, typeOffset, &type));
    for (uint32_t i = 0; i < type.memberCount; ++i) {
        DebugDwarfMemberInfo member = {};
        assert(DebugDwarfDescribeMember(&mapper, typeOffset, i, &member));
        if (std::strcmp(member.name, name) == 0) return member;
    }
    assert(false);
    return DebugDwarfMemberInfo();
}

static uint64_t memberAddress(const DebugDwarfMapper& mapper, uint64_t base, uint64_t typeOffset,
                              const char* name) {
    const DebugDwarfMemberInfo member = memberNamed(mapper, typeOffset, name);
    assert(member.hasByteOffset && !member.byteOffsetIsExpression && member.byteOffset >= 0);
    return base + static_cast<uint64_t>(member.byteOffset);
}

static void fillTrap(DebugBackendSnapshot* snapshot, uint64_t generation, uint64_t inspectAddress,
                     uint64_t bindingId) {
    *snapshot = DebugBackendSnapshot();
    snapshot->sessionGeneration = generation;
    snapshot->state = DebugSessionState::Paused;
    snapshot->stopReason = DebugStopReason::Breakpoint;
    snapshot->breakpointTrap = true;
    snapshot->processId = 12;
    snapshot->nativeRuntimeId = 77;
    snapshot->threadId = 44;
    snapshot->instructionPointer = inspectAddress;
    snapshot->targetAddress.valid = true;
    snapshot->targetAddress.value = 0x401010;
    snapshot->breakpointBindingId = bindingId;
    snapshot->stopGeneration = 1 + inspectAddress % 1000;
    snapshot->registerContext.valid = true;
    snapshot->registerContext.architecture = DebugArchitecture::Amd64;
    snapshot->registerContext.processId = 12;
    snapshot->registerContext.nativeRuntimeId = 77;
    snapshot->registerContext.threadId = 44;
    snapshot->registerContext.sessionGeneration = generation;
    snapshot->registerContext.stopGeneration = snapshot->stopGeneration;
    snapshot->registerContext.rip = inspectAddress;
    snapshot->registerContext.rflags = 0x202;
    snapshot->registerContext.rsp = 0x700080;
    snapshot->registerContext.rbp = 0x700100;
}

static bool launch(void* userData, const DebugTarget&, uint64_t generation, DebugBackendSnapshot* snapshot) {
    FakeBackend* fake = static_cast<FakeBackend*>(userData);
    *snapshot = DebugBackendSnapshot();
    snapshot->sessionGeneration = generation;
    snapshot->state = DebugSessionState::Launching;
    snapshot->processId = 12;
    snapshot->nativeRuntimeId = 77;
    return fake != nullptr;
}

static bool poll(void* userData, uint64_t generation, DebugBackendSnapshot* snapshot) {
    FakeBackend* fake = static_cast<FakeBackend*>(userData);
    if (!fake || !snapshot) return false;
    ++fake->polls;
    if (fake->polls == 1) {
        *snapshot = DebugBackendSnapshot();
        snapshot->sessionGeneration = generation;
        snapshot->state = DebugSessionState::Running;
        snapshot->stopReason = DebugStopReason::None;
        snapshot->processId = 12;
        snapshot->nativeRuntimeId = 77;
        return true;
    }
    if (fake->continuePending) {
        fake->continuePending = false;
        *snapshot = DebugBackendSnapshot();
        snapshot->sessionGeneration = generation;
        snapshot->state = DebugSessionState::Running;
        snapshot->stopReason = DebugStopReason::None;
        snapshot->processId = 12;
        snapshot->nativeRuntimeId = 77;
        snapshot->executionState = DebugBackendExecutionState::Running;
        return true;
    }
    if (fake->trapsEmitted < fake->trapRequests) {
        ++fake->trapsEmitted;
        fillTrap(snapshot, generation, fake->inspectAddress, fake->sharedBindingId);
        return true;
    }
    *snapshot = DebugBackendSnapshot();
    snapshot->sessionGeneration = generation;
    snapshot->state = DebugSessionState::Running;
    snapshot->stopReason = DebugStopReason::None;
    snapshot->processId = 12;
    snapshot->nativeRuntimeId = 77;
    return true;
}

static bool stop(void*, uint64_t) { return true; }

static bool bindBreakpoint(void* userData, const DebugTarget&, uint64_t, uint64_t, uint64_t,
                           const DebugBreakpoint& breakpoint, DebugBackendBinding* binding) {
    FakeBackend* fake = static_cast<FakeBackend*>(userData);
    if (!fake || !binding || !breakpoint.location.instructionAddress.valid) return false;
    ++fake->bindCalls;
    ++fake->physicalBinds;
    *binding = DebugBackendBinding();
    binding->accepted = true;
    binding->bindingId = fake->sharedBindingId;
    binding->originalByte = 0x55;
    binding->installedByte = 0xCC;
    binding->originalByteValid = true;
    std::strcpy(binding->message, "Bound / Verified");
    return true;
}

static bool debugCommand(void* userData, HostedDebugCommand command, uint64_t, uint64_t,
                         uint64_t, uint64_t, uint64_t, uint64_t, const char*, uint64_t,
                         uint64_t, bool, uint64_t, uint32_t, HostedDebugResult* result) {
    FakeBackend* fake = static_cast<FakeBackend*>(userData);
    if (!fake || !result) return false;
    *result = HostedDebugResult();
    result->status = command == HostedDebugCommand::ReleaseExecution ? 1 :
        (command == HostedDebugCommand::RestoreAll ? 4 : 2);
    result->bindingId = fake->sharedBindingId;
    result->originalByte = 0x55;
    result->installedByte = 0xCC;
    result->originalByteValid = true;
    if (command == HostedDebugCommand::RestoreAll) ++fake->restoreCommands;
    return true;
}

static bool continueExecution(void* userData, uint64_t generation, const DebugRegisterContext& context,
                              uint64_t, uint64_t, uint64_t, bool reinstall) {
    FakeBackend* fake = static_cast<FakeBackend*>(userData);
    if (!fake || !context.valid || context.sessionGeneration != generation || !reinstall) return false;
    ++fake->continueCalls;
    fake->continuePending = true;
    return true;
}

static DebugBackend makeBackend(FakeBackend* fake) {
    DebugBackend backend = {};
    backend.userData = fake;
    backend.capabilities.canLaunch = true;
    backend.capabilities.canStop = true;
    backend.capabilities.canContinue = true;
    backend.capabilities.canSetSourceBreakpoint = true;
    backend.capabilities.canBindSoftwareBreakpoint = true;
    backend.capabilities.canObserveBreakpointTrap = true;
    backend.capabilities.canRestoreBreakpoint = true;
    backend.launch = launch;
    backend.poll = poll;
    backend.stop = stop;
    backend.bindSoftwareBreakpoint = bindBreakpoint;
    backend.debugCommand = debugCommand;
    backend.continueExecution = continueExecution;
    backend.readTargetMemory = readTargetMemory;
    return backend;
}

static void prepareMappedBreakpoint(DebugController* controller, uint64_t id) {
    assert(controller && controller->breakpointCount == 1 && controller->breakpoints[0].id == id);
    DebugBreakpoint& breakpoint = controller->breakpoints[0];
    breakpoint.state = DebugBreakpointState::Mapped;
    breakpoint.location.mapping = DebugMappingState::Mapped;
    breakpoint.location.instructionAddress.valid = true;
    breakpoint.location.instructionAddress.value = 0x401010;
    breakpoint.mappedAddressCount = 1;
    breakpoint.mappedAddresses[0] = breakpoint.location.instructionAddress;
}

static Project validProject() {
    Project project = {};
    project.valid = true;
    project.formatVersion = 1;
    project.kind = ProjectKind::NativeGuiApplication;
    std::strcpy(project.projectId, "com.example.debugger");
    std::strcpy(project.displayName, "Debugger Fixture");
    std::strcpy(project.rootPath, "D:/work/debugger");
    std::strcpy(project.manifestPath, "app/app.json");
    std::strcpy(project.targetProfileId, "guidexos.amd64.hosted.native");
    std::strcpy(project.sourceRoot, "src");
    std::strcpy(project.entryPoint, "gx_main");
    std::strcpy(project.architecture, "amd64");
    std::strcpy(project.abi, "guidexos-c-abi-v1");
    std::strcpy(project.outputName, "debugger-fixture");
    return project;
}

static BuildResult validBuild() {
    BuildResult build = {};
    build.state = BuildState::Succeeded;
    build.artifactValid = true;
    build.artifactEntryPoint = true;
    std::strcpy(build.artifactPath, "build/bin/amd64/debugger-fixture.elf");
    std::strcpy(build.artifactSha256, "0123456789012345678901234567890123456789012345678901234567890123");
    return build;
}

static bool sawEvent(const DebugController& controller, DebugEventKind kind) {
    for (uint32_t i = 0; i < controller.eventCount; ++i)
        if (controller.events[i].kind == kind) return true;
    return false;
}

int main() {
    const std::vector<unsigned char> elf = readFile(
        "tests/fixtures/debugger-phase10/build/bin/amd64/debugger-phase10.elf",
        "../tests/fixtures/debugger-phase10/build/bin/amd64/debugger-phase10.elf");
    char sha[65] = {};
    assert(DebugDwarfComputeSha256(elf.data(), elf.size(), sha, sizeof(sha)));
    static DebugDwarfMapper mapper = {};
    DebugDwarfError dwarfError = DebugDwarfError::None;
    assert(DebugDwarfMapperLoad(&mapper, "D:/dev/guideXOS_Developer_Studio/tests/fixtures/debugger-phase10", "phase10", "native", "amd64",
                                "build/bin/amd64/debugger-phase10.elf", elf.size(), sha, 9,
                                elf.data(), elf.size(), 11, &dwarfError));
    const uint64_t inspectAddress = findFunctionAddress(mapper, "inspect");
    assert(inspectAddress != 0);
    uint32_t inspectFunctionIndex = 0;
    assert(DebugDwarfMapperLookupDebugFunction(&mapper, inspectAddress, &inspectFunctionIndex, &dwarfError));
    static Memory memory = {};
    DebugDwarfFrameContext seedFrame = {};
    seedFrame.frameIndex = 0;
    seedFrame.instructionAddress = inspectAddress;
    seedFrame.processId = 12;
    seedFrame.nativeRuntimeId = 77;
    seedFrame.threadId = 44;
    seedFrame.sessionGeneration = 1;
    seedFrame.stopGeneration = 1;
    seedFrame.frameBaseKnown = true;
    seedFrame.frameBase = 0x700100;
    static DebugDwarfVariableView roots = {};
    assert(DebugDwarfInspectVariables(&mapper, seedFrame, readMemoryDirect, &memory, &roots));
    const DebugDwarfVariable* doubled = variableNamed(roots, "doubled");
    const DebugDwarfVariable* ptr = variableNamed(roots, "ptr");
    const DebugDwarfVariable* values = variableNamed(roots, "values");
    const DebugDwarfVariable* rect = variableNamed(roots, "rect");
    const DebugDwarfVariable* config = variableNamed(roots, "config");
    const DebugDwarfVariable* nothing = variableNamed(roots, "nothing");
    assert(doubled && ptr && values && rect && config && nothing);
    const uint64_t doubledAddress = variableAddress(mapper, *doubled, seedFrame.frameBase);
    const uint64_t ptrAddress = variableAddress(mapper, *ptr, seedFrame.frameBase);
    const uint64_t valuesAddress = variableAddress(mapper, *values, seedFrame.frameBase);
    const uint64_t configAddress = variableAddress(mapper, *config, seedFrame.frameBase);
    const uint64_t nothingAddress = variableAddress(mapper, *nothing, seedFrame.frameBase);
    for (uint32_t i = 0; i < 4; ++i) put32(&memory, valuesAddress + i * 4u, i + 1u);
    const uint64_t configTarget = 0x710000;
    put32(&memory, memberAddress(mapper, configTarget, config->typeDieOffset, "enabled"), 1);
    put32(&memory, memberAddress(mapper, configTarget, config->typeDieOffset, "count"), 4);
    put64(&memory, configAddress, configTarget);
    put64(&memory, nothingAddress, 0);
    put32(&memory, doubledAddress, 0);
    put64(&memory, ptrAddress, doubledAddress);

    Project project = validProject();
    BuildResult build = validBuild();
    DebugTarget target = {};
    DebugErrorCode error = DebugErrorCode::None;
    assert(DebugTargetFromBuild(project, build, 9, &target, &error));
    static DebugController controller = {};
    assert(DebugControllerInit(&controller));
    assert(DebugControllerSetProjectContext(&controller, project.projectId, project.rootPath, 9));
    uint64_t breakpointId = 0;
    assert(DebugControllerAddBreakpoint(&controller, project.projectId, project.rootPath, 9,
                                        "src/main.cpp", 42, 0, 3, &breakpointId, &error));
    prepareMappedBreakpoint(&controller, breakpointId);
    assert(!DebugControllerSetBreakpointCondition(&controller, breakpointId, "inspect()", &error));
    assert(error == DebugErrorCode::InvalidCondition);
    assert(controller.breakpoints[0].enabled && controller.breakpoints[0].id == breakpointId &&
           controller.breakpoints[0].conditionParseState == DebugExpressionParseState::UnsupportedExpression);
    assert(DebugControllerSetBreakpointCondition(&controller, breakpointId, "doubled == 2", &error));
    assert(DebugControllerSetBreakpointEnabled(&controller, breakpointId, false, &error));
    assert(!controller.breakpoints[0].enabled && controller.breakpoints[0].condition &&
           std::strcmp(controller.breakpoints[0].condition, "doubled == 2") == 0);
    assert(DebugControllerSetBreakpointEnabled(&controller, breakpointId, true, &error));
    assert(controller.breakpoints[0].enabled && controller.breakpoints[0].condition &&
           std::strcmp(controller.breakpoints[0].condition, "doubled == 2") == 0);
    prepareMappedBreakpoint(&controller, breakpointId);
    FakeBackend fake = {};
    fake.memory = &memory;
    fake.inspectAddress = inspectAddress;
    fake.trapRequests = 1;
    DebugBackend backend = makeBackend(&fake);
    assert(DebugControllerStart(&controller, backend, target, &error));
    assert(DebugControllerPoll(&controller, backend, &mapper));
    assert(controller.state == DebugSessionState::Running);
    assert(DebugControllerPoll(&controller, backend, &mapper));
    assert(controller.state == DebugSessionState::Paused);
    assert(DebugControllerIsConditionResumePending(&controller));
    assert(controller.breakpoints[0].conditionLastEvaluation == DebugBreakpointConditionEvaluation::False);
    assert(fake.continueCalls == 1 && controller.breakpoints[0].backendBindingId == fake.sharedBindingId);
    assert(sawEvent(controller, DebugEventKind::BreakpointConditionFalse));
    assert(!sawEvent(controller, DebugEventKind::BreakpointHit));

    put32(&memory, doubledAddress, 1);
    fake.trapRequests = 2;
    assert(DebugControllerPoll(&controller, backend, &mapper));
    assert(controller.state == DebugSessionState::Running);
    assert(!DebugControllerIsConditionResumePending(&controller));
    assert(DebugControllerPoll(&controller, backend, &mapper));
    assert(controller.state == DebugSessionState::Paused);
    assert(DebugControllerIsConditionResumePending(&controller));
    assert(controller.breakpoints[0].conditionLastEvaluation == DebugBreakpointConditionEvaluation::False);
    assert(!sawEvent(controller, DebugEventKind::BreakpointHit));
    assert(fake.continueCalls == 2 && fake.physicalBinds == 1);

    put32(&memory, doubledAddress, 2);
    fake.trapRequests = 3;
    assert(DebugControllerPoll(&controller, backend, &mapper));
    assert(controller.state == DebugSessionState::Running);
    assert(!DebugControllerIsConditionResumePending(&controller));
    assert(DebugControllerPoll(&controller, backend, &mapper));
    assert(controller.state == DebugSessionState::Paused);
    assert(!DebugControllerIsConditionResumePending(&controller));
    assert(controller.breakpoints[0].conditionLastEvaluation == DebugBreakpointConditionEvaluation::True);
    assert(controller.breakpoints[0].conditionLastTruthValue);
    assert(sawEvent(controller, DebugEventKind::BreakpointHit));
    assert(fake.continueCalls == 2 && fake.physicalBinds == 1);
    assert(memory.writes == 0 && memory.targetCalls == 0);

    DebugBreakpointConditionDecision decision = DebugBreakpointConditionDecision::Error;
    assert(DebugControllerSetBreakpointCondition(&controller, breakpointId, "config->count >= 4", &error));
    assert(DebugControllerEvaluateBreakpointCondition(&controller, backend, &mapper, &decision) &&
           decision == DebugBreakpointConditionDecision::True);
    assert(DebugControllerSetBreakpointCondition(&controller, breakpointId, "values[2] == 3", &error));
    assert(DebugControllerEvaluateBreakpointCondition(&controller, backend, &mapper, &decision) &&
           decision == DebugBreakpointConditionDecision::True);
    assert(DebugControllerSetBreakpointCondition(&controller, breakpointId, "config != 0", &error));
    assert(DebugControllerEvaluateBreakpointCondition(&controller, backend, &mapper, &decision) &&
           decision == DebugBreakpointConditionDecision::True);
    assert(DebugControllerSetBreakpointCondition(&controller, breakpointId, "ptr == &doubled", &error));
    assert(DebugControllerEvaluateBreakpointCondition(&controller, backend, &mapper, &decision) &&
           decision == DebugBreakpointConditionDecision::True);
    assert(DebugControllerSetBreakpointCondition(&controller, breakpointId, "config < 1", &error));
    assert(DebugControllerEvaluateBreakpointCondition(&controller, backend, &mapper, &decision) &&
           decision == DebugBreakpointConditionDecision::Error && controller.error == DebugErrorCode::ConditionError);
    assert(DebugControllerSetBreakpointCondition(&controller, breakpointId, "nothing == 0", &error));
    assert(DebugControllerEvaluateBreakpointCondition(&controller, backend, &mapper, &decision) &&
           decision == DebugBreakpointConditionDecision::True);
    put32(&memory, doubledAddress, 0xfffffff9u);
    assert(DebugControllerSetBreakpointCondition(&controller, breakpointId, "doubled < 18446744073709551615", &error));
    assert(DebugControllerEvaluateBreakpointCondition(&controller, backend, &mapper, &decision) &&
           decision == DebugBreakpointConditionDecision::True);
    assert(DebugControllerSetBreakpointCondition(&controller, breakpointId, "18446744073709551615 == 18446744073709551615", &error));
    assert(DebugControllerEvaluateBreakpointCondition(&controller, backend, &mapper, &decision) &&
           decision == DebugBreakpointConditionDecision::True);
    assert(DebugControllerSetBreakpointCondition(&controller, breakpointId, "rect == 1", &error));
    assert(DebugControllerEvaluateBreakpointCondition(&controller, backend, &mapper, &decision) &&
           decision == DebugBreakpointConditionDecision::Error && controller.error == DebugErrorCode::ConditionError);
    assert(DebugControllerSetBreakpointCondition(&controller, breakpointId, "values[99] == 3", &error));
    assert(DebugControllerEvaluateBreakpointCondition(&controller, backend, &mapper, &decision) &&
           decision == DebugBreakpointConditionDecision::Error && controller.error == DebugErrorCode::ConditionError);
    assert(DebugControllerSetBreakpointCondition(&controller, breakpointId, "doesNotExist == 4", &error));
    assert(DebugControllerEvaluateBreakpointCondition(&controller, backend, &mapper, &decision) &&
           decision == DebugBreakpointConditionDecision::Error && controller.state == DebugSessionState::Paused &&
           controller.error == DebugErrorCode::ConditionError);
    assert(DebugControllerClearBreakpointCondition(&controller, breakpointId, &error));
    assert((!controller.breakpoints[0].condition || controller.breakpoints[0].condition[0] == '\0') &&
           controller.breakpoints[0].conditionParseState == DebugExpressionParseState::Empty);

    std::cout << "Phase13 conditional comparison proof: false_false_true=1 reinsertion=1 "
              << "member=1 array=1 pointer_null=1 signed_mixed=1 errors_stop=1 memory_writes=" << memory.writes
              << " target_calls=" << memory.targetCalls << "\n";
    std::cout << "Developer Studio debugger conditional breakpoints test PASS\n";
    return 0;
}
