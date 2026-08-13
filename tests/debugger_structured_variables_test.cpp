#include "developer_studio_debug_symbols.h"

#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>

using namespace guidexos::developer_studio;

struct Memory {
    uint64_t base;
    uint8_t bytes[0x30000];
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

static void put32(Memory* memory, uint64_t address, uint32_t value) {
    assert(memory && address >= memory->base && address - memory->base + 4 <= sizeof(memory->bytes));
    for (uint32_t i = 0; i < 4; ++i) memory->bytes[address - memory->base + i] = static_cast<uint8_t>(value >> (i * 8));
}

static void put64(Memory* memory, uint64_t address, uint64_t value) {
    assert(memory && address >= memory->base && address - memory->base + 8 <= sizeof(memory->bytes));
    for (uint32_t i = 0; i < 8; ++i) memory->bytes[address - memory->base + i] = static_cast<uint8_t>(value >> (i * 8));
}

static const DebugDwarfVariable* findVariable(const DebugDwarfMapper& mapper,
                                              const DebugDwarfVariableView& view, const char* name) {
    for (uint32_t i = 0; i < view.variableCount; ++i)
        if (std::strcmp(view.variables[i].name, name) == 0) return &view.variables[i];
    (void)mapper;
    return 0;
}

static DebugDwarfValueNode* childByName(DebugDwarfVariableView* view, uint64_t parentId, const char* name) {
    assert(view && parentId > 0 && parentId <= view->nodeCount);
    DebugDwarfValueNode& parent = view->nodes[parentId - 1];
    for (uint32_t i = 0; i < parent.childCount; ++i) {
        const uint64_t childId = parent.childNodeIds[i];
        if (childId > 0 && childId <= view->nodeCount && std::strcmp(view->nodes[childId - 1].name, name) == 0)
            return &view->nodes[childId - 1];
    }
    return 0;
}

static uint64_t findFunctionAddress(const DebugDwarfMapper& mapper, const char* name) {
    for (uint32_t i = 0; i < mapper.debugInfoFunctionCount; ++i)
        if (std::strcmp(mapper.debugFunctions[i].name, name) == 0) return mapper.debugFunctions[i].lowPc;
    return 0;
}

static uint64_t frameLocation(const DebugDwarfMapper& mapper, const DebugDwarfVariable& variable,
                             uint64_t frameBase) {
    assert(variable.dieOffset != 0);
    int index = -1;
    for (uint32_t i = 0; i < mapper.debugInfoVariableCount; ++i)
        if (mapper.debugVariables[i].dieOffset == variable.dieOffset) { index = static_cast<int>(mapper.debugVariables[i].dieIndex); break; }
    assert(index >= 0);
    const DebugDwarfDieInfo& die = mapper.dies[index];
    assert(die.hasLocation && !die.locationIsList && die.locationLength >= 2 && die.location[0] == 0x91);
    int64_t value = 0;
    uint32_t shift = 0;
    uint8_t byte = 0;
    uint32_t cursor = 1;
    do {
        assert(cursor < die.locationLength);
        byte = die.location[cursor++];
        value |= static_cast<int64_t>(byte & 0x7f) << shift;
        shift += 7;
    } while ((byte & 0x80) != 0 && shift < 64);
    if (shift < 64 && (byte & 0x40) != 0) value |= static_cast<int64_t>(UINT64_MAX << shift);
    return value >= 0 ? frameBase + static_cast<uint64_t>(value) : frameBase - static_cast<uint64_t>(-(value + 1)) - 1u;
}

static DebugDwarfMemberInfo memberNamed(const DebugDwarfMapper& mapper, uint64_t typeOffset, const char* name) {
    for (uint32_t depth = 0; depth < kDebugDwarfMaxTypeDepth; ++depth) {
        const DebugDwarfDieInfo* die = 0;
        for (uint32_t i = 0; i < mapper.debugInfoDieCount; ++i)
            if (mapper.dies[i].offset == typeOffset) { die = &mapper.dies[i]; break; }
        assert(die);
        if (die->tag == 0x0f || die->tag == 0x16 || die->tag == 0x26 || die->tag == 0x35) {
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

static uint64_t memberAddress(const DebugDwarfMapper& mapper, uint64_t base,
                              uint64_t typeOffset, const char* name) {
    const DebugDwarfMemberInfo member = memberNamed(mapper, typeOffset, name);
    assert(member.hasByteOffset && !member.byteOffsetIsExpression && member.byteOffset >= 0);
    return base + static_cast<uint64_t>(member.byteOffset);
}

int main() {
    const std::vector<unsigned char> elf = readFile(
        "tests/fixtures/debugger-phase10/build/bin/amd64/debugger-phase10.elf",
        "../tests/fixtures/debugger-phase10/build/bin/amd64/debugger-phase10.elf");
    char sha[65] = {};
    assert(DebugDwarfComputeSha256(elf.data(), elf.size(), sha, sizeof(sha)));
    static DebugDwarfMapper mapper = {};
    DebugDwarfError error = DebugDwarfError::None;
    assert(DebugDwarfMapperLoad(&mapper, "D:/dev/guideXOS_Developer_Studio/tests/fixtures/debugger-phase10",
                                "phase10", "native", "amd64", "build/bin/amd64/debugger-phase10.elf",
                                elf.size(), sha, 9, elf.data(), elf.size(), 10, &error));
    assert(mapper.debugInfoReady && mapper.dwarfVersion == 5);
    const uint64_t inspectAddress = findFunctionAddress(mapper, "inspect");
    assert(inspectAddress != 0);

    static Memory memory = {};
    memory.base = 0x700000;
    const uint64_t frameBase = 0x700100;
    const uint64_t configTarget = 0x710000;
    DebugDwarfFrameContext frame = {};
    frame.frameIndex = 0;
    frame.instructionAddress = inspectAddress;
    frame.processId = 42;
    frame.nativeRuntimeId = 77;
    frame.threadId = 11;
    frame.sessionGeneration = 9;
    frame.stopGeneration = 5;
    frame.frameBaseKnown = true;
    frame.frameBase = frameBase;

    static DebugDwarfVariableView view = {};
    assert(DebugDwarfInspectVariables(&mapper, frame, readMemory, &memory, &view));
    const DebugDwarfVariable* point = findVariable(mapper, view, "point");
    const DebugDwarfVariable* config = findVariable(mapper, view, "config");
    const DebugDwarfVariable* rect = findVariable(mapper, view, "rect");
    const DebugDwarfVariable* values = findVariable(mapper, view, "values");
    const DebugDwarfVariable* node = findVariable(mapper, view, "node");
    const DebugDwarfVariable* nothing = findVariable(mapper, view, "nothing");
    assert(point && config && rect && values && node && nothing);
    const uint64_t pointAddress = frameLocation(mapper, *point, frameBase);
    const uint64_t configAddress = frameLocation(mapper, *config, frameBase);
    const uint64_t rectAddress = frameLocation(mapper, *rect, frameBase);
    const uint64_t valuesAddress = frameLocation(mapper, *values, frameBase);
    const uint64_t nodeAddress = frameLocation(mapper, *node, frameBase);
    const uint64_t nothingAddress = frameLocation(mapper, *nothing, frameBase);

    put32(&memory, memberAddress(mapper, pointAddress, point->typeDieOffset, "x"), 10);
    put32(&memory, memberAddress(mapper, pointAddress, point->typeDieOffset, "y"), 20);
    const DebugDwarfMemberInfo origin = memberNamed(mapper, rect->typeDieOffset, "origin");
    const uint64_t originAddress = rectAddress + static_cast<uint64_t>(origin.byteOffset);
    put32(&memory, memberAddress(mapper, originAddress, origin.typeDieOffset, "x"), 10);
    put32(&memory, memberAddress(mapper, originAddress, origin.typeDieOffset, "y"), 20);
    put32(&memory, memberAddress(mapper, rectAddress, rect->typeDieOffset, "width"), 100);
    put32(&memory, memberAddress(mapper, rectAddress, rect->typeDieOffset, "height"), 50);
    for (uint32_t i = 0; i < 4; ++i) put32(&memory, valuesAddress + i * 4u, i + 1u);
    put32(&memory, memberAddress(mapper, configTarget, config->typeDieOffset, "enabled"), 1);
    put32(&memory, memberAddress(mapper, configTarget, config->typeDieOffset, "count"), 4);
    put32(&memory, memberAddress(mapper, nodeAddress, node->typeDieOffset, "value"), 77);
    put64(&memory, memberAddress(mapper, nodeAddress, node->typeDieOffset, "next"), nodeAddress);
    put64(&memory, configAddress, configTarget);
    put64(&memory, nothingAddress, 0);
    assert(DebugDwarfInspectVariables(&mapper, frame, readMemory, &memory, &view));

    assert(view.nodes[rect->nodeId - 1].childCount == 0);
    assert(DebugDwarfExpandValue(&mapper, frame, readMemory, &memory, &view, rect->nodeId));
    DebugDwarfValueNode* originNode = childByName(&view, rect->nodeId, "origin");
    DebugDwarfValueNode* widthNode = childByName(&view, rect->nodeId, "width");
    assert(originNode && widthNode && std::strcmp(widthNode->valueDisplay, "100") == 0);
    assert(DebugDwarfExpandValue(&mapper, frame, readMemory, &memory, &view, originNode->nodeId));
    DebugDwarfValueNode* xNode = childByName(&view, originNode->nodeId, "x");
    DebugDwarfValueNode* yNode = childByName(&view, originNode->nodeId, "y");
    assert(xNode && yNode && std::strcmp(xNode->valueDisplay, "10") == 0 && std::strcmp(yNode->valueDisplay, "20") == 0);

    assert(DebugDwarfExpandValue(&mapper, frame, readMemory, &memory, &view, values->nodeId));
    assert(childByName(&view, values->nodeId, "[0]") &&
           std::strcmp(childByName(&view, values->nodeId, "[0]")->valueDisplay, "1") == 0 &&
           std::strcmp(childByName(&view, values->nodeId, "[2]")->valueDisplay, "3") == 0);
    assert(DebugDwarfExpandValue(&mapper, frame, readMemory, &memory, &view, config->nodeId));
    DebugDwarfValueNode* enabled = childByName(&view, config->nodeId, "enabled");
    DebugDwarfValueNode* count = childByName(&view, config->nodeId, "count");
    if (!enabled || !count || std::strcmp(enabled ? enabled->valueDisplay : "", "true") != 0 ||
        std::strcmp(count ? count->valueDisplay : "", "4") != 0) {
        assert(false);
    }

    assert(DebugDwarfExpandValue(&mapper, frame, readMemory, &memory, &view, node->nodeId));
    DebugDwarfValueNode* next = childByName(&view, node->nodeId, "next");
    assert(next && next->expandable);
    const uint32_t readsBeforeCycle = view.targetMemoryReadCount;
    assert(DebugDwarfExpandValue(&mapper, frame, readMemory, &memory, &view, next->nodeId));
    assert(view.targetMemoryReadCount == readsBeforeCycle && next->childCount == 1 &&
           std::strcmp(view.nodes[next->childNodeIds[0] - 1].valueDisplay, "<cycle>") == 0);

    const uint32_t readsBeforeNull = view.targetMemoryReadCount;
    assert(DebugDwarfExpandValue(&mapper, frame, readMemory, &memory, &view, nothing->nodeId));
    assert(view.targetMemoryReadCount == readsBeforeNull &&
           std::strcmp(view.nodes[nothing->nodeId - 1].valueDisplay, "nullptr") == 0);

    put64(&memory, configAddress, 0x740000);
    assert(DebugDwarfInspectVariables(&mapper, frame, readMemory, &memory, &view));
    const DebugDwarfVariable* invalidConfig = findVariable(mapper, view, "config");
    assert(invalidConfig && DebugDwarfExpandValue(&mapper, frame, readMemory, &memory, &view,
                                                  invalidConfig->nodeId));
    assert(std::strcmp(view.nodes[invalidConfig->nodeId - 1].valueDisplay, "<unreadable>") == 0);

    DebugDwarfFrameContext stale = frame;
    stale.stopGeneration = 6;
    assert(!DebugDwarfExpandValue(&mapper, stale, readMemory, &memory, &view, rect->nodeId));

    std::cout << "Developer Studio structured variables test PASS\n";
    return 0;
}
