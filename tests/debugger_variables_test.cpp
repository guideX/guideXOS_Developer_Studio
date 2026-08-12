#include "developer_studio_debug_symbols.h"
#include "developer_studio_debugger.h"

#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>

using namespace guidexos::developer_studio;

struct Memory {
    uint64_t base;
    uint8_t bytes[256];
};

static bool readMemory(void* userData, uint64_t sessionGeneration, uint64_t processId,
                       uint64_t runtimeId, uint64_t threadId, uint64_t stopGeneration,
                       uint64_t address, uint8_t* bytes, uint32_t requested, uint32_t* returned) {
    Memory* memory = static_cast<Memory*>(userData);
    if (returned) *returned = 0;
    if (!memory || sessionGeneration != 9 || processId != 42 || runtimeId != 77 || threadId != 11 ||
        stopGeneration != 5 || !bytes || requested == 0 || address < memory->base ||
        requested > sizeof(memory->bytes) || address - memory->base > sizeof(memory->bytes) - requested) return false;
    for (uint32_t i = 0; i < requested; ++i) bytes[i] = memory->bytes[address - memory->base + i];
    if (returned) *returned = requested;
    return true;
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

static void put32(uint8_t* bytes, uint32_t value) {
    for (uint32_t i = 0; i < 4; ++i) bytes[i] = static_cast<uint8_t>(value >> (i * 8));
}

static void put64(uint8_t* bytes, uint64_t value) {
    for (uint32_t i = 0; i < 8; ++i) bytes[i] = static_cast<uint8_t>(value >> (i * 8));
}

static uint16_t get16(const std::vector<unsigned char>& bytes, uint32_t offset) {
    return static_cast<uint16_t>(bytes[offset]) | static_cast<uint16_t>(bytes[offset + 1] << 8);
}

static uint32_t get32(const std::vector<unsigned char>& bytes, uint32_t offset) {
    return static_cast<uint32_t>(bytes[offset]) |
        (static_cast<uint32_t>(bytes[offset + 1]) << 8) |
        (static_cast<uint32_t>(bytes[offset + 2]) << 16) |
        (static_cast<uint32_t>(bytes[offset + 3]) << 24);
}

static uint64_t get64(const std::vector<unsigned char>& bytes, uint32_t offset) {
    uint64_t value = 0;
    for (uint32_t i = 0; i < 8; ++i) value |= static_cast<uint64_t>(bytes[offset + i]) << (i * 8);
    return value;
}

static uint64_t findSection(const std::vector<unsigned char>& bytes, const char* wanted, uint64_t* size) {
    const uint64_t sectionTable = get64(bytes, 40);
    const uint16_t entrySize = get16(bytes, 58);
    const uint16_t count = get16(bytes, 60);
    const uint16_t stringsIndex = get16(bytes, 62);
    const uint64_t stringsHeader = sectionTable + static_cast<uint64_t>(stringsIndex) * entrySize;
    const uint64_t stringsOffset = get64(bytes, static_cast<uint32_t>(stringsHeader + 24));
    const uint64_t stringsSize = get64(bytes, static_cast<uint32_t>(stringsHeader + 32));
    for (uint16_t i = 0; i < count; ++i) {
        const uint64_t header = sectionTable + static_cast<uint64_t>(i) * entrySize;
        const uint32_t nameOffset = get32(bytes, static_cast<uint32_t>(header));
        if (nameOffset >= stringsSize) continue;
        const char* name = reinterpret_cast<const char*>(bytes.data() + stringsOffset + nameOffset);
        if (std::strcmp(name, wanted) == 0) {
            if (size) *size = get64(bytes, static_cast<uint32_t>(header + 32));
            return get64(bytes, static_cast<uint32_t>(header + 24));
        }
    }
    return 0;
}

static const DebugDwarfVariable* findVariable(const DebugDwarfVariableView& view, const char* name) {
    for (uint32_t i = 0; i < view.variableCount; ++i) {
        if (std::strcmp(view.variables[i].name, name) == 0) return &view.variables[i];
    }
    return 0;
}

int main() {
    const std::vector<unsigned char> elf = readFile("tests/fixtures/debugger-phase8/build/bin/amd64/debugger-phase8.elf",
                                                     "../tests/fixtures/debugger-phase8/build/bin/amd64/debugger-phase8.elf");
    char sha[65] = {};
    assert(DebugDwarfComputeSha256(elf.data(), elf.size(), sha, sizeof(sha)));
    static DebugDwarfMapper mapper = {};
    DebugDwarfError error = DebugDwarfError::None;
    const bool loaded = DebugDwarfMapperLoad(&mapper, "D:/dev/guideXOS_Developer_Studio/tests/fixtures/debugger-phase8",
                                "phase8", "native", "amd64", "build/bin/amd64/debugger-phase8.elf",
                                elf.size(), sha, 9, elf.data(), elf.size(), 9, &error);
    assert(loaded);
    assert(mapper.debugInfoReady && mapper.debugInfoCompilationUnitCount == 2);
    assert(mapper.debugInfoFunctionCount >= 4 && mapper.debugInfoVariableCount >= 6);
    uint32_t function = 0;
    assert(DebugDwarfMapperLookupDebugFunction(&mapper, 0x200011c8, &function, &error));
    assert(std::strcmp(mapper.debugFunctions[function].name, "level2") == 0);

    Memory memory = {};
    memory.base = 0x7000f0;
    put32(memory.bytes + 0x0c, 2);
    put32(memory.bytes + 0x08, 4);
    DebugDwarfFrameContext frame = {};
    frame.frameIndex = 0;
    frame.instructionAddress = 0x200011c8;
    frame.processId = 42;
    frame.nativeRuntimeId = 77;
    frame.threadId = 11;
    frame.sessionGeneration = 9;
    frame.stopGeneration = 5;
    frame.frameBaseKnown = true;
    frame.frameBase = 0x700100;
    static DebugDwarfVariableView view = {};
    assert(DebugDwarfInspectVariables(&mapper, frame, readMemory, &memory, &view));
    assert(view.valid && view.argumentCount == 1 && view.localCount == 1);
    assert(std::strcmp(view.variables[0].name, "value") == 0);
    assert(std::strcmp(view.variables[0].valueDisplay, "2") == 0);
    assert(std::strcmp(view.variables[1].name, "result") == 0);
    assert(std::strcmp(view.variables[1].valueDisplay, "4") == 0);

    DebugDwarfFrameContext caller = frame;
    caller.frameIndex = 1;
    caller.instructionAddress = 0x200011a8;
    caller.frameBase = 0x700100;
    caller.registers.valid = false;
    assert(DebugDwarfInspectVariables(&mapper, caller, readMemory, &memory, &view));
    assert(view.valid && view.argumentCount == 1 && view.localCount == 0);
    assert(std::strcmp(view.variables[0].name, "value") == 0 &&
           std::strcmp(view.variables[0].valueDisplay, "2") == 0);

    const std::vector<unsigned char> phase9Elf = readFile("tests/fixtures/debugger-phase9/build/bin/amd64/debugger-phase9.elf",
                                                            "../tests/fixtures/debugger-phase9/build/bin/amd64/debugger-phase9.elf");
    char phase9Sha[65] = {};
    assert(DebugDwarfComputeSha256(phase9Elf.data(), phase9Elf.size(), phase9Sha, sizeof(phase9Sha)));
    static DebugDwarfMapper phase9Mapper = {};
    error = DebugDwarfError::None;
    assert(DebugDwarfMapperLoad(&phase9Mapper, "D:/dev/guideXOS_Developer_Studio/tests/fixtures/debugger-phase9",
                                "phase9", "native", "amd64", "build/bin/amd64/debugger-phase9.elf",
                                phase9Elf.size(), phase9Sha, 9, phase9Elf.data(), phase9Elf.size(), 9, &error));
    assert(phase9Mapper.debugInfoReady && phase9Mapper.debugInfoCompilationUnitCount == 2);
    uint32_t calculate = 0;
    assert(DebugDwarfMapperLookupDebugFunction(&phase9Mapper, 0x20001191, &calculate, &error));
    assert(std::strcmp(phase9Mapper.debugFunctions[calculate].name, "calculate") == 0);

    Memory phase9Memory = {};
    phase9Memory.base = 0x7001e0;
    const uint64_t phase9FrameBase = 0x700200;
    put64(phase9Memory.bytes + 0x00, 0x7001f0); // ptr -> doubled
    put32(phase9Memory.bytes + 0x0f, 1);        // positive
    put32(phase9Memory.bytes + 0x10, 42);       // doubled
    put32(phase9Memory.bytes + 0x14, 21);       // sum
    put32(phase9Memory.bytes + 0x18, 11);       // b
    put32(phase9Memory.bytes + 0x1c, 10);       // a
    DebugDwarfFrameContext phase9Frame = {};
    phase9Frame.frameIndex = 0;
    phase9Frame.instructionAddress = 0x20001191;
    phase9Frame.processId = 42;
    phase9Frame.nativeRuntimeId = 77;
    phase9Frame.threadId = 11;
    phase9Frame.sessionGeneration = 9;
    phase9Frame.stopGeneration = 5;
    phase9Frame.frameBaseKnown = true;
    phase9Frame.frameBase = phase9FrameBase;
    static DebugDwarfVariableView phase9View = {};
    assert(DebugDwarfInspectVariables(&phase9Mapper, phase9Frame, readMemory, &phase9Memory, &phase9View));
    assert(phase9View.valid && phase9View.argumentCount == 2 && phase9View.localCount == 4);
    const DebugDwarfVariable* a = findVariable(phase9View, "a");
    const DebugDwarfVariable* b = findVariable(phase9View, "b");
    const DebugDwarfVariable* sum = findVariable(phase9View, "sum");
    const DebugDwarfVariable* doubled = findVariable(phase9View, "doubled");
    const DebugDwarfVariable* positive = findVariable(phase9View, "positive");
    const DebugDwarfVariable* ptr = findVariable(phase9View, "ptr");
    assert(a && b && sum && doubled && positive && ptr);
    assert(std::strcmp(a->valueDisplay, "10") == 0);
    assert(std::strcmp(b->valueDisplay, "11") == 0);
    assert(std::strcmp(sum->valueDisplay, "21") == 0);
    assert(std::strcmp(doubled->valueDisplay, "42") == 0);
    assert(std::strcmp(positive->valueDisplay, "true") == 0);
    assert(std::strcmp(ptr->valueDisplay, "0x00000000007001f0") == 0);

    DebugDwarfFrameContext deniedFrame = phase9Frame;
    deniedFrame.processId = 999;
    static DebugDwarfVariableView deniedView = {};
    assert(DebugDwarfInspectVariables(&phase9Mapper, deniedFrame, readMemory, &phase9Memory, &deniedView));
    const DebugDwarfVariable* deniedA = findVariable(deniedView, "a");
    assert(deniedA && deniedA->state == DebugDwarfVariableState::ReadFailure &&
           std::strcmp(deniedA->valueDisplay, "<unavailable>") == 0);

    static DebugDwarfMapper malformedMapper = {};
    std::vector<unsigned char> malformed = phase9Elf;
    uint64_t debugInfoSize = 0;
    const uint64_t debugInfoOffset = findSection(malformed, ".debug_info", &debugInfoSize);
    assert(debugInfoOffset != 0 && debugInfoSize >= 4);
    put32(malformed.data() + debugInfoOffset, 0xffffffffu);
    char malformedSha[65] = {};
    assert(DebugDwarfComputeSha256(malformed.data(), malformed.size(), malformedSha, sizeof(malformedSha)));
    error = DebugDwarfError::None;
    assert(!DebugDwarfMapperLoad(&malformedMapper, "D:/dev/guideXOS_Developer_Studio/tests/fixtures/debugger-phase9",
                                 "phase9", "native", "amd64", "build/bin/amd64/debugger-phase9.elf",
                                 malformed.size(), malformedSha, 9, malformed.data(), malformed.size(), 10, &error));
    assert(error != DebugDwarfError::None);

    static DebugController controller = {};
    controller.state = DebugSessionState::Running;
    controller.variables.valid = true;
    controller.variables.stale = false;
    DebugBackend emptyBackend = {};
    DebugErrorCode controllerError = DebugErrorCode::None;
    assert(!DebugControllerBuildVariables(&controller, emptyBackend, &phase9Mapper, &controllerError));
    assert(!controller.variables.valid);

    std::cout << "Developer Studio DWARF locals/arguments test PASS\n";
    return 0;
}
