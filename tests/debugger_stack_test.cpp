#include "developer_studio_debugger.h"
#include "developer_studio_debug_symbols.h"

#include <cassert>
#include <cstring>
#include <iostream>

using namespace guidexos::developer_studio;

namespace {

struct StackMemory {
    uint64_t base = 0x700000;
    uint64_t high = 0x702000;
    uint64_t words[1024] = {};
    bool fail = false;
};

static bool readTarget(void* userData, uint64_t, uint64_t processId, uint64_t runtimeId,
                       uint64_t threadId, uint64_t stopGeneration, uint64_t address,
                       uint8_t* bytes, uint32_t requested, uint32_t* returned) {
    StackMemory* memory = static_cast<StackMemory*>(userData);
    if (returned) *returned = 0;
    if (!memory || processId != 42 || runtimeId != 77 || threadId != 11 || stopGeneration != 5 ||
        !bytes || requested != 16 || memory->fail || address < memory->base ||
        address > memory->high - requested || (address - memory->base) % 8 != 0) return false;
    const uint64_t index = (address - memory->base) / 8;
    if (index + 1 >= sizeof(memory->words) / sizeof(memory->words[0])) return false;
    for (uint32_t i = 0; i < 8; ++i) bytes[i] = static_cast<uint8_t>(memory->words[index] >> (i * 8));
    for (uint32_t i = 0; i < 8; ++i) bytes[8 + i] = static_cast<uint8_t>(memory->words[index + 1] >> (i * 8));
    if (returned) *returned = requested;
    return true;
}

static void prepareMapper(DebugDwarfMapper* mapper) {
    if (!mapper) return;
    DebugDwarfMapperReset(mapper);
    mapper->state = DebugDwarfMapperState::Ready;
    mapper->identity.mapperGeneration = 3;
    mapper->sourceFileCount = 4;
    std::strcpy(mapper->sourceFiles[0].relativePath, "src/math.cpp");
    std::strcpy(mapper->sourceFiles[1].relativePath, "src/engine.cpp");
    std::strcpy(mapper->sourceFiles[2].relativePath, "src/main.cpp");
    std::strcpy(mapper->sourceFiles[3].relativePath, "src/startup.cpp");
    mapper->executableSegmentCount = 1;
    mapper->executableSegments[0].startAddress = 0x401000;
    mapper->executableSegments[0].endAddress = 0x402000;
    mapper->functionSymbolCount = 4;
    mapper->functionSymbols[0].startAddress = 0x401000;
    mapper->functionSymbols[0].size = 0x40;
    std::strcpy(mapper->functionSymbols[0].name, "add");
    mapper->functionSymbols[1].startAddress = 0x401100;
    mapper->functionSymbols[1].size = 0x40;
    std::strcpy(mapper->functionSymbols[1].name, "calculate");
    mapper->functionSymbols[2].startAddress = 0x401200;
    mapper->functionSymbols[2].size = 0x40;
    std::strcpy(mapper->functionSymbols[2].name, "main");
    mapper->functionSymbols[3].startAddress = 0x401300;
    mapper->functionSymbols[3].size = 0x40;
    std::strcpy(mapper->functionSymbols[3].name, "startup_stub");
    mapper->lineRowCount = 4;
    mapper->addressOrderCount = 4;
    mapper->rows[0] = { 0, 14, 1, 0, 0, 0x401010, 0x401040, true };
    mapper->rows[1] = { 1, 63, 1, 0, 1, 0x40111f, 0x401120, true };
    mapper->rows[2] = { 2, 28, 1, 0, 2, 0x40121f, 0x401220, true };
    mapper->rows[3] = { 3, 7, 1, 0, 3, 0x40131f, 0x401320, true };
    mapper->addressOrder[0] = 0;
    mapper->addressOrder[1] = 1;
    mapper->addressOrder[2] = 2;
    mapper->addressOrder[3] = 3;
}

static DebugRegisterContext context() {
    DebugRegisterContext value = {};
    value.valid = true;
    value.architecture = DebugArchitecture::Amd64;
    value.processId = 42;
    value.nativeRuntimeId = 77;
    value.threadId = 11;
    value.sessionGeneration = 9;
    value.stopGeneration = 5;
    value.rip = 0x401010;
    value.rsp = 0x700080;
    value.rbp = 0x700100;
    value.stackLow = 0x700000;
    value.stackHigh = 0x702000;
    return value;
}

static void link(StackMemory& memory, uint64_t rbp, uint64_t previous, uint64_t returnAddress) {
    const uint64_t index = (rbp - memory.base) / 8;
    memory.words[index] = previous;
    memory.words[index + 1] = returnAddress;
}

} // namespace

int main() {
    static DebugDwarfMapper mapper = {};
    prepareMapper(&mapper);
    StackMemory memory;
    link(memory, 0x700100, 0x700140, 0x401120);
    link(memory, 0x700140, 0x700180, 0x401220);
    link(memory, 0x700180, 0, 0x401320);
    DebugUnwindResult result = {};
    assert(DebugUnwindAmd64FramePointer(context(), &mapper, readTarget, &memory, 9, &result));
    assert(result.frameCount == 4 && result.terminationReason == DebugUnwindTerminationReason::EndOfStack);
    assert(std::strcmp(result.frames[0].functionName, "add") == 0);
    assert(std::strcmp(result.frames[1].functionName, "calculate") == 0);
    assert(std::strcmp(result.frames[2].functionName, "main") == 0);
    assert(std::strcmp(result.frames[3].functionName, "startup_stub") == 0);
    assert(std::strcmp(result.frames[1].sourcePath, "src/engine.cpp") == 0 && result.frames[1].sourceLine == 63);
    assert(result.frames[1].rawReturnAddress == 0x401120 && result.frames[1].lookupAddress == 0x40111f);
    assert(DebugDwarfMapperIsExecutableAddress(&mapper, 0x401010));
    assert(!DebugDwarfMapperIsExecutableAddress(&mapper, 0x501010));
    char functionName[kDebugMaxFunctionNameBytes] = {};
    uint64_t start = 0, size = 0;
    DebugDwarfError symbolError = DebugDwarfError::None;
    assert(DebugDwarfMapperLookupFunction(&mapper, 0x401125, functionName, sizeof(functionName), &start, &size, &symbolError));
    assert(std::strcmp(functionName, "calculate") == 0 && start == 0x401100 && size == 0x40);
    assert(!DebugDwarfMapperLookupFunction(&mapper, 0x401080, functionName, sizeof(functionName), &start, &size, &symbolError));

    DebugRegisterContext noFrame = context();
    noFrame.rbp = 0;
    assert(DebugUnwindAmd64FramePointer(noFrame, &mapper, readTarget, &memory, 9, &result));
    assert(result.frameCount == 1 && result.terminationReason == DebugUnwindTerminationReason::NoFramePointer);

    StackMemory cycle = memory;
    link(cycle, 0x700100, 0x700140, 0x401120);
    link(cycle, 0x700140, 0x700100, 0x401220);
    assert(DebugUnwindAmd64FramePointer(context(), &mapper, readTarget, &cycle, 9, &result));
    assert(result.terminationReason == DebugUnwindTerminationReason::Cycle);

    DebugRegisterContext misaligned = context();
    misaligned.rbp = 0x700103;
    assert(DebugUnwindAmd64FramePointer(misaligned, &mapper, readTarget, &memory, 9, &result));
    assert(result.frameCount == 1 && result.terminationReason == DebugUnwindTerminationReason::OutsideStack);

    StackMemory failed = memory;
    failed.fail = true;
    assert(DebugUnwindAmd64FramePointer(context(), &mapper, readTarget, &failed, 9, &result));
    assert(result.frameCount == 1 && result.terminationReason == DebugUnwindTerminationReason::ReadFailure);

    DebugRegisterContext outside = context();
    outside.rbp = 0x702000;
    assert(DebugUnwindAmd64FramePointer(outside, &mapper, readTarget, &memory, 9, &result));
    assert(result.frameCount == 1 && result.terminationReason == DebugUnwindTerminationReason::OutsideStack);

    StackMemory badReturn = memory;
    link(badReturn, 0x700100, 0, 0x503000);
    assert(DebugUnwindAmd64FramePointer(context(), &mapper, readTarget, &badReturn, 9, &result));
    assert(result.frameCount == 2 && result.frames[1].confidence == DebugStackFrameConfidence::Invalid &&
           result.terminationReason == DebugUnwindTerminationReason::OutsideTarget);

    DebugController controller = {};
    assert(DebugControllerInit(&controller));
    controller.state = DebugSessionState::Paused;
    controller.active = true;
    controller.stopReason = DebugStopReason::Breakpoint;
    controller.sessionGeneration = 9;
    controller.processId = 42;
    controller.nativeRuntimeId = 77;
    controller.currentThreadId = 11;
    controller.stopGeneration = 5;
    controller.stoppedContext = context();
    controller.capabilities.canReadCallStack = true;
    std::strcpy(controller.target.artifactSha256, "stack-artifact");
    DebugBackend backend = {};
    backend.userData = &memory;
    backend.readTargetMemory = readTarget;
    DebugErrorCode controllerError = DebugErrorCode::None;
    assert(DebugControllerBuildCallStack(&controller, backend, &mapper, &controllerError));
    assert(controller.callStack.valid && controller.callStack.selectedFrameIndex == 0);
    assert(DebugControllerSelectCallStackFrame(&controller, 2, &controllerError));
    assert(controller.callStack.selectedFrameIndex == 2);
    assert(!DebugControllerSelectCallStackFrame(&controller, 20, &controllerError));
    controller.state = DebugSessionState::Running;
    assert(!DebugControllerSelectCallStackFrame(&controller, 0, &controllerError));

    std::cout << "Developer Studio AMD64 frame-pointer stack test PASS\n";
    return 0;
}
