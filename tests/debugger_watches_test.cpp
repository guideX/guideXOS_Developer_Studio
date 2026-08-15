#include "developer_studio_debug_watches.h"

#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>

using namespace guidexos::developer_studio;

struct Memory {
    uint64_t base;
    uint8_t bytes[0x30000];
    uint32_t reads;
    uint32_t writes;
};

static bool readMemory(void* userData, uint64_t sessionGeneration, uint64_t processId,
                       uint64_t runtimeId, uint64_t threadId, uint64_t stopGeneration,
                       uint64_t address, uint8_t* bytes, uint32_t requested, uint32_t* returned) {
    Memory* memory = static_cast<Memory*>(userData);
    if (returned) *returned = 0;
    if (!memory || sessionGeneration != 9 || processId != 42 || runtimeId != 77 || threadId != 11 ||
        stopGeneration != 5 || !bytes || requested == 0 || address < memory->base ||
        requested > sizeof(memory->bytes) || address - memory->base > sizeof(memory->bytes) - requested) return false;
    ++memory->reads;
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

static uint64_t findFunctionAddress(const DebugDwarfMapper& mapper, const char* name) {
    for (uint32_t i = 0; i < mapper.debugInfoFunctionCount; ++i)
        if (std::strcmp(mapper.debugFunctions[i].name, name) == 0) return mapper.debugFunctions[i].lowPc;
    return 0;
}

static const DebugDwarfVariable* variableNamed(const DebugDwarfVariableView& view, const char* name) {
    for (uint32_t i = 0; i < view.variableCount; ++i)
        if (std::strcmp(view.variables[i].name, name) == 0) return &view.variables[i];
    return 0;
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
        const DebugDwarfDieInfo* die = 0;
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

static DebugWatchItem* watchNamed(DebugWatchCollection* watches, const char* expression) {
    for (uint32_t i = 0; i < kDebugWatchMaxWatches; ++i) {
        DebugWatchItem* item = &watches->items[i];
        if (item->used && std::strcmp(item->expression, expression) == 0) return item;
    }
    return 0;
}

static DebugDwarfValueNode* watchChildByName(DebugDwarfVariableView* view, uint64_t parentId,
                                             const char* name) {
    if (!view || parentId == 0 || parentId > view->nodeCount || !name) return 0;
    DebugDwarfValueNode* parent = &view->nodes[parentId - 1u];
    for (uint32_t i = 0; i < parent->childCount; ++i) {
        const uint64_t childId = parent->childNodeIds[i];
        if (childId == 0 || childId > view->nodeCount) continue;
        DebugDwarfValueNode* child = &view->nodes[childId - 1u];
        if (std::strcmp(child->name, name) == 0) return child;
    }
    return 0;
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
                                elf.size(), sha, 9, elf.data(), elf.size(), 11, &error));
    const uint64_t inspectAddress = findFunctionAddress(mapper, "inspect");
    assert(inspectAddress != 0);

    Memory memory = {};
    memory.base = 0x700000;
    DebugDwarfFrameContext frame = {};
    frame.frameIndex = 0;
    frame.instructionAddress = inspectAddress;
    frame.processId = 42;
    frame.nativeRuntimeId = 77;
    frame.threadId = 11;
    frame.sessionGeneration = 9;
    frame.stopGeneration = 5;
    frame.frameBaseKnown = true;
    frame.frameBase = 0x700100;
    DebugDwarfVariableView roots = {};
    assert(DebugDwarfInspectVariables(&mapper, frame, readMemory, &memory, &roots));
    const DebugDwarfVariable* rect = variableNamed(roots, "rect");
    const DebugDwarfVariable* values = variableNamed(roots, "values");
    const DebugDwarfVariable* config = variableNamed(roots, "config");
    const DebugDwarfVariable* node = variableNamed(roots, "node");
    const DebugDwarfVariable* nothing = variableNamed(roots, "nothing");
    const DebugDwarfVariable* doubled = variableNamed(roots, "doubled");
    const DebugDwarfVariable* ptr = variableNamed(roots, "ptr");
    assert(rect && values && config && node && nothing && doubled && ptr);
    const uint64_t rectAddress = variableAddress(mapper, *rect, frame.frameBase);
    const uint64_t valuesAddress = variableAddress(mapper, *values, frame.frameBase);
    const uint64_t configAddress = variableAddress(mapper, *config, frame.frameBase);
    const uint64_t nodeAddress = variableAddress(mapper, *node, frame.frameBase);
    const uint64_t nothingAddress = variableAddress(mapper, *nothing, frame.frameBase);
    const uint64_t doubledAddress = variableAddress(mapper, *doubled, frame.frameBase);
    const uint64_t ptrAddress = variableAddress(mapper, *ptr, frame.frameBase);
    const uint64_t originAddress = memberAddress(mapper, rectAddress, rect->typeDieOffset, "origin");
    put32(&memory, memberAddress(mapper, originAddress, memberNamed(mapper, rect->typeDieOffset, "origin").typeDieOffset, "x"), 10);
    put32(&memory, memberAddress(mapper, originAddress, memberNamed(mapper, rect->typeDieOffset, "origin").typeDieOffset, "y"), 20);
    put32(&memory, memberAddress(mapper, rectAddress, rect->typeDieOffset, "width"), 100);
    put32(&memory, memberAddress(mapper, rectAddress, rect->typeDieOffset, "height"), 50);
    for (uint32_t i = 0; i < 4; ++i) put32(&memory, valuesAddress + i * 4u, i + 1u);
    const uint64_t configTarget = 0x710000;
    put32(&memory, memberAddress(mapper, configTarget, config->typeDieOffset, "enabled"), 1);
    put32(&memory, memberAddress(mapper, configTarget, config->typeDieOffset, "count"), 4);
    put64(&memory, configAddress, configTarget);
    put32(&memory, memberAddress(mapper, nodeAddress, node->typeDieOffset, "value"), 77);
    put64(&memory, memberAddress(mapper, nodeAddress, node->typeDieOffset, "next"), nodeAddress);
    put64(&memory, nothingAddress, 0);
    put32(&memory, doubledAddress, 42);
    put64(&memory, ptrAddress, doubledAddress);

    DebugWatchCollection watches = {};
    assert(DebugWatchCollectionInit(&watches));
    const char* expressions[] = {
        "rect", "rect.width", "rect.origin.x", "config->count", "values[2]", "*ptr", "&doubled", "node",
        "doesNotExist", "nothing->count", "values[99]", "inspect()", "rect.width = 5",
        "doubled == 42", "doubled == 41", "doubled != 41", "doubled < 43", "doubled <= 42",
        "doubled > 41", "doubled >= 42", "rect.origin.x < rect.width", "config->count >= 4",
        "values[2] != 0", "*ptr == 42", "config != 0", "config == 0", "nothing == 0",
        "values[99] == 3", "*nothing == 5", "rect == 1", "doubled < 18446744073709551615"
    };
    for (uint32_t i = 0; i < sizeof(expressions) / sizeof(expressions[0]); ++i)
        assert(DebugWatchCollectionAdd(&watches, expressions[i], nullptr));
    DebugWatchEvaluationContext context = {};
    context.mapper = &mapper;
    context.frame = frame;
    context.readMemory = readMemory;
    context.userData = &memory;
    assert(DebugWatchCollectionRefresh(&watches, context));
    assert(watchNamed(&watches, "rect")->result.state == DebugWatchState::Available);
    assert(watchNamed(&watches, "rect")->result.structured);
    assert(std::strcmp(watchNamed(&watches, "rect.width")->result.valueDisplay, "100") == 0);
    assert(std::strcmp(watchNamed(&watches, "rect.origin.x")->result.valueDisplay, "10") == 0);
    assert(watchNamed(&watches, "config->count")->result.state == DebugWatchState::Available);
    assert(std::strcmp(watchNamed(&watches, "config->count")->result.valueDisplay, "4") == 0);
    assert(std::strcmp(watchNamed(&watches, "values[2]")->result.valueDisplay, "3") == 0);
    assert(std::strcmp(watchNamed(&watches, "*ptr")->result.valueDisplay, "42") == 0);
    assert(watchNamed(&watches, "&doubled")->result.hasScalar && watchNamed(&watches, "&doubled")->result.address == doubledAddress);
    assert(watchNamed(&watches, "node")->result.structured);
    assert(watchNamed(&watches, "doesNotExist")->result.state == DebugWatchState::UnknownIdentifier);
    assert(watchNamed(&watches, "nothing->count")->result.state == DebugWatchState::NullPointer);
    assert(watchNamed(&watches, "values[99]")->result.state == DebugWatchState::IndexOutOfRange);
    assert(watchNamed(&watches, "inspect()")->result.state == DebugWatchState::UnsupportedExpression);
    assert(watchNamed(&watches, "rect.width = 5")->result.state == DebugWatchState::UnsupportedExpression);
    assert(std::strcmp(watchNamed(&watches, "doubled == 42")->result.valueDisplay, "true") == 0);
    assert(std::strcmp(watchNamed(&watches, "doubled == 41")->result.valueDisplay, "false") == 0);
    assert(std::strcmp(watchNamed(&watches, "doubled != 41")->result.valueDisplay, "true") == 0);
    assert(std::strcmp(watchNamed(&watches, "doubled < 43")->result.valueDisplay, "true") == 0);
    assert(std::strcmp(watchNamed(&watches, "doubled <= 42")->result.valueDisplay, "true") == 0);
    assert(std::strcmp(watchNamed(&watches, "doubled > 41")->result.valueDisplay, "true") == 0);
    assert(std::strcmp(watchNamed(&watches, "doubled >= 42")->result.valueDisplay, "true") == 0);
    assert(std::strcmp(watchNamed(&watches, "rect.origin.x < rect.width")->result.valueDisplay, "true") == 0);
    assert(std::strcmp(watchNamed(&watches, "config->count >= 4")->result.valueDisplay, "true") == 0);
    assert(std::strcmp(watchNamed(&watches, "values[2] != 0")->result.valueDisplay, "true") == 0);
    assert(std::strcmp(watchNamed(&watches, "*ptr == 42")->result.valueDisplay, "true") == 0);
    assert(std::strcmp(watchNamed(&watches, "config != 0")->result.valueDisplay, "true") == 0);
    assert(std::strcmp(watchNamed(&watches, "config == 0")->result.valueDisplay, "false") == 0);
    assert(std::strcmp(watchNamed(&watches, "nothing == 0")->result.valueDisplay, "true") == 0);
    assert(watchNamed(&watches, "values[99] == 3")->result.state == DebugWatchState::IndexOutOfRange);
    assert(watchNamed(&watches, "*nothing == 5")->result.state == DebugWatchState::NullPointer);
    assert(watchNamed(&watches, "rect == 1")->result.state == DebugWatchState::TypeMismatch);
    assert(std::strcmp(watchNamed(&watches, "doubled < 18446744073709551615")->result.valueDisplay, "true") == 0);
    assert(!watchNamed(&watches, "doubled == 42")->result.structured &&
           watchNamed(&watches, "doubled == 42")->result.nodeId == 0);

    put64(&memory, configAddress, 0x0001000000000000ull);
    assert(DebugWatchCollectionRefresh(&watches, context));
    assert(watchNamed(&watches, "config == 0")->result.state == DebugWatchState::UnreadableTarget);
    put64(&memory, configAddress, 0);
    assert(DebugWatchCollectionRefresh(&watches, context));
    assert(std::strcmp(watchNamed(&watches, "config == 0")->result.valueDisplay, "true") == 0);
    assert(std::strcmp(watchNamed(&watches, "config != 0")->result.valueDisplay, "false") == 0);
    put64(&memory, configAddress, configTarget);
    assert(DebugWatchCollectionRefresh(&watches, context));
    assert(DebugWatchCollectionExpand(&watches, context, watchNamed(&watches, "rect")->id));
    assert(watches.tree.nodes[watchNamed(&watches, "rect")->result.nodeId - 1u].childCount > 0);
    assert(DebugWatchCollectionExpand(&watches, context, watchNamed(&watches, "node")->id));
    DebugDwarfValueNode* next = watchChildByName(&watches.tree, watchNamed(&watches, "node")->result.nodeId, "next");
    assert(next && next->expandable);
    const uint32_t readsBeforeCycle = memory.reads;
    assert(DebugDwarfExpandValue(&mapper, frame, readMemory, &memory, &watches.tree, next->nodeId));
    assert(memory.reads == readsBeforeCycle && next->childCount == 1 &&
           std::strcmp(watches.tree.nodes[next->childNodeIds[0] - 1u].valueDisplay, "<cycle>") == 0);
    const uint32_t readsBeforeRunning = memory.reads;
    DebugWatchCollectionMarkRunning(&watches);
    assert(watchNamed(&watches, "rect.width")->result.state == DebugWatchState::Running);
    assert(!DebugWatchCollectionExpand(&watches, context, watchNamed(&watches, "rect")->id));
    assert(memory.reads == readsBeforeRunning);
    DebugWatchCollectionMarkStale(&watches);
    assert(watchNamed(&watches, "rect")->result.state == DebugWatchState::Stale);
    assert(DebugWatchCollectionEdit(&watches, watchNamed(&watches, "rect")->id, "rect.height"));
    assert(DebugWatchCollectionRemove(&watches, watchNamed(&watches, "rect.height")->id));

    const char* invalid[] = { "foo.bar.", "foo->", "values[2", "(foo", "foo()", "a+b", "*",
        "counter = 5", "counter === 5", "counter < < 5", "a < b < c", "a == b == c", "!counter",
        "inspect() == 0", "18446744073709551616" };
    for (uint32_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        DebugExpressionAst ast = {};
        assert(!DebugExpressionParse(invalid[i], &ast));
        assert(ast.state == DebugExpressionParseState::ParseError ||
               ast.state == DebugExpressionParseState::UnsupportedExpression);
    }
    DebugExpressionAst valid = {};
    assert(DebugExpressionParse("(*config).count", &valid) && valid.nodeCount >= 3);
    assert(DebugExpressionParse("(doubled == 42)", &valid));
    assert(DebugExpressionParse("(doubled == 42) == 1", &valid));
    assert(valid.nodeCount >= 3);
    std::cout << "Phase11 watch proof: expressions=" << watches.count
              << " reads=" << memory.reads << " writes=" << memory.writes
              << " rectWidth=100 rectOriginX=10 configCount=4 values2=3 ptr=42\n";
    std::cout << "Developer Studio debugger watches test PASS\n";
    return 0;
}
