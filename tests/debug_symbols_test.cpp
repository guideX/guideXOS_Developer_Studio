#include "developer_studio_debug_symbols.h"
#include "developer_studio_debugger.h"

#include <cassert>
#include <cstring>
#include <iostream>
#include <vector>

using namespace guidexos::developer_studio;

static void u32(std::vector<unsigned char>& bytes, uint32_t value) {
    bytes.push_back(static_cast<unsigned char>(value));
    bytes.push_back(static_cast<unsigned char>(value >> 8));
    bytes.push_back(static_cast<unsigned char>(value >> 16));
    bytes.push_back(static_cast<unsigned char>(value >> 24));
}

static void u16(std::vector<unsigned char>& bytes, uint16_t value) {
    bytes.push_back(static_cast<unsigned char>(value));
    bytes.push_back(static_cast<unsigned char>(value >> 8));
}

static void u64(std::vector<unsigned char>& bytes, uint64_t value) {
    for (uint32_t i = 0; i < 8; ++i) bytes.push_back(static_cast<unsigned char>(value >> (i * 8)));
}

static void patchU32(std::vector<unsigned char>& bytes, uint32_t offset, uint32_t value) {
    bytes[offset] = static_cast<unsigned char>(value);
    bytes[offset + 1] = static_cast<unsigned char>(value >> 8);
    bytes[offset + 2] = static_cast<unsigned char>(value >> 16);
    bytes[offset + 3] = static_cast<unsigned char>(value >> 24);
}

static void patchU16(std::vector<unsigned char>& bytes, uint32_t offset, uint16_t value) {
    bytes[offset] = static_cast<unsigned char>(value);
    bytes[offset + 1] = static_cast<unsigned char>(value >> 8);
}

static void patchU64(std::vector<unsigned char>& bytes, uint32_t offset, uint64_t value) {
    for (uint32_t i = 0; i < 8; ++i) bytes[offset + i] = static_cast<unsigned char>(value >> (i * 8));
}

static uint64_t readU64At(const std::vector<unsigned char>& bytes, uint32_t offset) {
    uint64_t value = 0;
    for (uint32_t i = 0; i < 8; ++i) value |= static_cast<uint64_t>(bytes[offset + i]) << (i * 8);
    return value;
}

static void sleb(std::vector<unsigned char>& bytes, int32_t value) {
    bool more = true;
    while (more) {
        unsigned char byte = static_cast<unsigned char>(value & 0x7f);
        value >>= 7;
        const bool sign = (byte & 0x40) != 0;
        more = !((value == 0 && !sign) || (value == -1 && sign));
        if (more) byte |= 0x80;
        bytes.push_back(byte);
    }
}

static std::vector<unsigned char> fixtureElf() {
    const char lineStrings[] = "\0D:/fixture\0src\0main.cpp\0";
    const uint32_t rootOffset = 1;
    const uint32_t srcOffset = rootOffset + 11;
    const uint32_t fileOffset = srcOffset + 4;
    const char sectionStrings[] = "\0.debug_line\0.debug_line_str\0.shstrtab\0";
    const uint32_t lineName = 1;
    const uint32_t lineStringName = 13;
    const uint32_t shStringName = 29;

    std::vector<unsigned char> line;
    const uint32_t unitLengthOffset = 0;
    u32(line, 0);
    u16(line, 5);
    line.push_back(8);
    line.push_back(0);
    const uint32_t headerLengthOffset = static_cast<uint32_t>(line.size());
    u32(line, 0);
    line.push_back(1); line.push_back(1); line.push_back(1);
    line.push_back(static_cast<unsigned char>(-5)); line.push_back(14); line.push_back(13);
    for (uint32_t i = 1; i < 13; ++i) line.push_back(i == 2 || i == 3 || i == 4 || i == 5 || i == 9 || i == 12 ? 1 : 0);
    line.push_back(1); // one directory format
    line.push_back(1); line.push_back(0x1f);
    line.push_back(2); // two directories
    u32(line, rootOffset);
    u32(line, srcOffset);
    line.push_back(3); // file format count
    line.push_back(1); line.push_back(0x1f);
    line.push_back(2); line.push_back(0x0f);
    line.push_back(6); line.push_back(0x1e);
    line.push_back(1); // one file
    u32(line, fileOffset); line.push_back(2);
    for (uint32_t i = 0; i < 16; ++i) line.push_back(0);
    const uint32_t programOffset = static_cast<uint32_t>(line.size());
    patchU32(line, headerLengthOffset, programOffset - (headerLengthOffset + 4));
    line.push_back(0); line.push_back(9); line.push_back(2); u64(line, 0x401000);
    line.push_back(3); sleb(line, 41); line.push_back(1); // main.cpp:42 @ 0x401000
    line.push_back(2); line.push_back(4); line.push_back(1); // same line @ 0x401004
    line.push_back(2); line.push_back(4); line.push_back(3); sleb(line, 1); line.push_back(1); // line 43 @ 0x401008
    line.push_back(2); line.push_back(4);
    line.push_back(0); line.push_back(1); line.push_back(1); // end sequence at 0x40100c
    line.push_back(0); line.push_back(9); line.push_back(2); u64(line, 0x402000);
    line.push_back(3); sleb(line, 41); line.push_back(1); // second sequence: main.cpp:42 @ 0x402000
    line.push_back(0); line.push_back(1); line.push_back(1);
    patchU32(line, unitLengthOffset, static_cast<uint32_t>(line.size() - 4));

    std::vector<unsigned char> bytes(64, 0);
    bytes[0] = 0x7f; bytes[1] = 'E'; bytes[2] = 'L'; bytes[3] = 'F'; bytes[4] = 2; bytes[5] = 1;
    bytes[6] = 1;
    bytes[16] = 2; bytes[18] = 62;
    const uint32_t lineOffset = static_cast<uint32_t>(bytes.size());
    bytes.insert(bytes.end(), line.begin(), line.end());
    const uint32_t lineStringOffset = static_cast<uint32_t>(bytes.size());
    bytes.insert(bytes.end(), lineStrings, lineStrings + sizeof(lineStrings));
    const uint32_t shStringOffset = static_cast<uint32_t>(bytes.size());
    bytes.insert(bytes.end(), sectionStrings, sectionStrings + sizeof(sectionStrings));
    const uint32_t sectionOffset = static_cast<uint32_t>(bytes.size());
    bytes.resize(bytes.size() + 4 * 64, 0);
    // Section table: null, .debug_line, .debug_line_str, .shstrtab.
    // Rebuild the section headers with direct byte writes to keep the fixture explicit.
    auto putSection = [&](uint32_t index, uint32_t name, uint32_t type, uint64_t offset, uint64_t size) {
        const uint32_t at = sectionOffset + index * 64;
        patchU32(bytes, at, name); patchU32(bytes, at + 4, type);
        for (uint32_t i = 8; i < 24; ++i) bytes[at + i] = 0;
        for (uint32_t i = 0; i < 8; ++i) bytes[at + 24 + i] = static_cast<unsigned char>(offset >> (i * 8));
        for (uint32_t i = 0; i < 8; ++i) bytes[at + 32 + i] = static_cast<unsigned char>(size >> (i * 8));
    };
    putSection(1, lineName, 1, lineOffset, line.size());
    putSection(2, lineStringName, 3, lineStringOffset, sizeof(lineStrings));
    putSection(3, shStringName, 3, shStringOffset, sizeof(sectionStrings));
    for (uint32_t i = 0; i < 8; ++i) bytes[40 + i] = static_cast<unsigned char>(static_cast<uint64_t>(sectionOffset) >> (i * 8));
    bytes[58] = 64; bytes[60] = 4; bytes[62] = 3;
    return bytes;
}

static std::vector<unsigned char> symbolFixtureElf() {
    std::vector<unsigned char> bytes = fixtureElf();
    const uint64_t oldSectionOffset = readU64At(bytes, 40);
    bytes.insert(bytes.begin() + 64, 56, 0);
    patchU64(bytes, 40, oldSectionOffset + 56);

    // One executable PT_LOAD covers the synthetic line and symbol addresses.
    const uint32_t program = 64;
    patchU32(bytes, program + 0, 1);
    patchU32(bytes, program + 4, 5);
    patchU64(bytes, program + 8, 0);
    patchU64(bytes, program + 16, 0x401000);
    patchU64(bytes, program + 24, 0x401000);
    patchU64(bytes, program + 32, 0x3000);
    patchU64(bytes, program + 40, 0x3000);
    patchU64(bytes, program + 48, 0x1000);
    patchU16(bytes, 54, 56);
    patchU16(bytes, 56, 1);
    patchU64(bytes, 32, 64);

    const uint32_t sectionOffset = static_cast<uint32_t>(oldSectionOffset + 56);
    for (uint32_t index = 1; index < 4; ++index) {
        const uint32_t header = sectionOffset + index * 64;
        patchU64(bytes, header + 24, readU64At(bytes, header + 24) + 56);
    }

    const char shStrings[] = "\0.debug_line\0.debug_line_str\0.shstrtab\0.symtab\0.strtab\0";
    const char symStrings[] = "\0main\0helper\0";
    const uint32_t shStringsOffset = static_cast<uint32_t>(bytes.size());
    bytes.insert(bytes.end(), shStrings, shStrings + sizeof(shStrings));
    const uint32_t symStringsOffset = static_cast<uint32_t>(bytes.size());
    bytes.insert(bytes.end(), symStrings, symStrings + sizeof(symStrings));
    const uint32_t symtabOffset = static_cast<uint32_t>(bytes.size());
    bytes.resize(bytes.size() + 3 * 24, 0);
    // Null symbol, main at 0x401000, and a zero-sized helper at 0x401010.
    patchU32(bytes, symtabOffset + 24 + 0, 1);
    bytes[symtabOffset + 24 + 4] = 2;
    patchU16(bytes, symtabOffset + 24 + 6, 2);
    patchU64(bytes, symtabOffset + 24 + 8, 0x401000);
    patchU64(bytes, symtabOffset + 24 + 16, 0x10);
    patchU32(bytes, symtabOffset + 48 + 0, 6);
    bytes[symtabOffset + 48 + 4] = 2;
    patchU16(bytes, symtabOffset + 48 + 6, 2);
    patchU64(bytes, symtabOffset + 48 + 8, 0x401010);

    const uint32_t newSectionOffset = static_cast<uint32_t>(bytes.size());
    bytes.resize(bytes.size() + 6 * 64, 0);
    auto putSection = [&](uint32_t index, uint32_t name, uint32_t type,
                          uint64_t offset, uint64_t size, uint32_t link,
                          uint64_t entrySize) {
        const uint32_t header = newSectionOffset + index * 64;
        patchU32(bytes, header, name);
        patchU32(bytes, header + 4, type);
        patchU64(bytes, header + 24, offset);
        patchU64(bytes, header + 32, size);
        patchU32(bytes, header + 40, link);
        patchU64(bytes, header + 56, entrySize);
    };
    putSection(1, 1, 1, readU64At(bytes, sectionOffset + 64 + 24),
               readU64At(bytes, sectionOffset + 64 + 32), 0, 0);
    putSection(2, 13, 3, readU64At(bytes, sectionOffset + 128 + 24),
               readU64At(bytes, sectionOffset + 128 + 32), 0, 0);
    putSection(3, 29, 3, shStringsOffset, sizeof(shStrings), 0, 0);
    putSection(4, 39, 2, symtabOffset, 3 * 24, 5, 24);
    putSection(5, 47, 3, symStringsOffset, sizeof(symStrings), 0, 0);
    patchU64(bytes, 40, newSectionOffset);
    patchU16(bytes, 60, 6);
    patchU16(bytes, 62, 3);
    return bytes;
}

int main() {
    const unsigned char shaInput[] = {'a', 'b', 'c'};
    char shaOutput[65] = {};
    assert(DebugDwarfComputeSha256(shaInput, sizeof(shaInput), shaOutput, sizeof(shaOutput)));
    assert(std::strcmp(shaOutput, "BA7816BF8F01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD") == 0);

    std::vector<unsigned char> fixture = fixtureElf();
    static DebugDwarfMapper mapper = {};
    DebugDwarfError error = DebugDwarfError::None;
    const bool loaded = DebugDwarfMapperLoad(&mapper, "D:/fixture", "fixture", "target", "amd64",
                                "build/bin/fixture.elf", fixture.size(),
                                "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                                7, &fixture[0], fixture.size(), 1, &error);
    assert(loaded);
    assert(mapper.state == DebugDwarfMapperState::Ready);
    assert(mapper.dwarfVersion == 5 && mapper.sourceFileCount == 1 && mapper.lineRowCount >= 4);
    uint64_t addresses[kDebugMapperMaxAddressesPerLine] = {};
    uint32_t count = 0; uint64_t primary = 0;
    assert(DebugDwarfMapperMapSourceToAddresses(&mapper, "src\\main.cpp", 42, addresses, 8, &count, &primary, &error));
    assert(count == 3 && primary == 0x401000 && addresses[1] == 0x401004 && addresses[2] == 0x402000);
    assert(mapper.sequenceCount == 2);

    std::vector<unsigned char> symbolFixture = symbolFixtureElf();
    static DebugDwarfMapper symbolMapper = {};
    assert(DebugDwarfMapperLoad(&symbolMapper, "D:/fixture", "fixture", "target", "amd64",
                                "build/bin/fixture-symbols.elf", symbolFixture.size(),
                                "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                                7, &symbolFixture[0], symbolFixture.size(), 2, &error));
    assert(symbolMapper.functionSymbolCount == 2 && symbolMapper.executableSegmentCount == 1);
    char functionName[kDebugMapperMaxFunctionNameBytes] = {};
    uint64_t functionStart = 0;
    uint64_t functionSize = 0;
    assert(DebugDwarfMapperLookupFunction(&symbolMapper, 0x401008, functionName,
                                          sizeof(functionName), &functionStart, &functionSize, &error));
    assert(std::strcmp(functionName, "main") == 0 && functionStart == 0x401000 && functionSize == 0x10);
    assert(DebugDwarfMapperLookupFunction(&symbolMapper, 0x401010, functionName,
                                          sizeof(functionName), &functionStart, &functionSize, &error));
    assert(std::strcmp(functionName, "helper") == 0 && functionSize == 0);
    assert(DebugDwarfMapperIsExecutableAddress(&symbolMapper, 0x401020));
    assert(!DebugDwarfMapperIsExecutableAddress(&symbolMapper, 0x404000));
    char path[kMaxProjectPathBytes] = {}; uint32_t line = 0; uint32_t column = 0;
    assert(DebugDwarfMapperMapAddressToSource(&mapper, 0x401006, path, sizeof(path), &line, &column, &error));
    assert(std::strcmp(path, "src/main.cpp") == 0 && line == 42);
    assert(DebugDwarfMapperMapAddressToSource(&mapper, 0x401004, path, sizeof(path), &line, &column, &error));
    assert(line == 42);
    assert(!DebugDwarfMapperMapSourceToAddresses(&mapper, "src/main.cpp", 99, addresses, 8, &count, &primary, &error));
    assert(error == DebugDwarfError::LineNotMapped);
    assert(!DebugDwarfMapperMatchesArtifact(&mapper, "D:/fixture", "fixture", "target", "amd64",
                                            "build/bin/fixture.elf", fixture.size(),
                                            "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb", 7));
    char normalized[kMaxProjectPathBytes] = {}; bool external = false;
    assert(DebugDwarfNormalizeSourcePath("D:/fixture", "D:/fixture/src\\main.cpp", normalized, sizeof(normalized), &external));
    assert(std::strcmp(normalized, "src/main.cpp") == 0 && !external);
    assert(!DebugDwarfNormalizeSourcePath("D:/fixture", "D:/other/main.cpp", normalized, sizeof(normalized), &external) && external);
    assert(!DebugDwarfNormalizeSourcePath("D:/fixture", "../outside.cpp", normalized, sizeof(normalized), &external));

    DebugController controller = {};
    assert(DebugControllerInit(&controller));
    std::strcpy(controller.target.projectId, "fixture");
    std::strcpy(controller.target.projectRoot, "D:/fixture");
    controller.target.projectGeneration = 7;
    std::strcpy(controller.target.targetProfile, "target");
    std::strcpy(controller.target.architecture, "amd64");
    std::strcpy(controller.target.executablePath, "build/bin/fixture.elf");
    controller.target.artifactSize = fixture.size();
    std::strcpy(controller.target.artifactSha256, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
    uint64_t breakpointId = 0;
    DebugErrorCode controllerError = DebugErrorCode::None;
    assert(DebugControllerAddBreakpoint(&controller, "fixture", "D:/fixture", 7,
                                        "src/main.cpp", 42, 0, 1, &breakpointId, &controllerError));
    assert(DebugControllerMapBreakpoints(&controller, &mapper, &controllerError));
    const DebugBreakpoint* mapped = DebugControllerBreakpointAt(&controller, 0);
    assert(mapped && mapped->state == DebugBreakpointState::Mapped);
    assert(mapped->location.mapping == DebugMappingState::Mapped && mapped->mappedAddressCount == 3);
    assert(mapped->location.instructionAddress.valid && mapped->location.instructionAddress.value == 0x401000);
    controller.state = DebugSessionState::Paused;
    controller.stopReason = DebugStopReason::Breakpoint;
    controller.currentInstructionAddress.valid = true;
    controller.currentInstructionAddress.value = 0x401000;
    controller.lastBreakpointId = breakpointId;
    controller.breakpoints[0].location.line = 43;
    assert(!DebugControllerResolveCurrentStop(&controller, &mapper, &controllerError));
    assert(controllerError == DebugErrorCode::BackendError);
    controller.breakpoints[0].location.line = 42;
    assert(DebugControllerResolveCurrentStop(&controller, &mapper, &controllerError));
    assert(std::strcmp(controller.currentLocation.relativePath, "src/main.cpp") == 0 &&
           controller.currentLocation.line == 42);
    DebugControllerMarkArtifactStale(&controller, "test stale");
    mapped = DebugControllerBreakpointAt(&controller, 0);
    assert(mapped && mapped->state == DebugBreakpointState::Stale);
    assert(mapped->mappingError == DebugErrorCode::ArtifactChanged && mapped->mappedAddressCount == 0);
    assert(DebugControllerMapBreakpoints(&controller, &mapper, &controllerError));
    mapped = DebugControllerBreakpointAt(&controller, 0);
    assert(mapped && mapped->state == DebugBreakpointState::Mapped && mapped->mappedAddressCount == 3);

    std::vector<unsigned char> unsupportedVersion = fixture;
    patchU16(unsupportedVersion, 64 + 4, 3);
    static DebugDwarfMapper unsupported = {};
    assert(!DebugDwarfMapperLoad(&unsupported, "D:/fixture", "fixture", "target", "amd64",
                                 "fixture.elf", unsupportedVersion.size(), "a", 7,
                                 &unsupportedVersion[0], unsupportedVersion.size(), 1, &error));
    assert(error == DebugDwarfError::UnsupportedDwarfVersion);

    const uint32_t sectionOffset = static_cast<uint32_t>(readU64At(fixture, 40));
    std::vector<unsigned char> truncatedLine = fixture;
    patchU64(truncatedLine, sectionOffset + 64 + 32, 1);
    static DebugDwarfMapper truncated = {};
    assert(!DebugDwarfMapperLoad(&truncated, "D:/fixture", "fixture", "target", "amd64",
                                 "fixture.elf", truncatedLine.size(), "a", 7,
                                 &truncatedLine[0], truncatedLine.size(), 1, &error));
    assert(error == DebugDwarfError::MalformedDwarf);

    std::vector<unsigned char> missingLine = fixture;
    patchU64(missingLine, sectionOffset + 64 + 32, 0);
    static DebugDwarfMapper noLine = {};
    assert(!DebugDwarfMapperLoad(&noLine, "D:/fixture", "fixture", "target", "amd64",
                                 "fixture.elf", missingLine.size(), "a", 7,
                                 &missingLine[0], missingLine.size(), 1, &error));
    assert(error == DebugDwarfError::MissingLineSection);

    uint32_t programOffset = 64;
    while (programOffset + 2 < fixture.size() &&
           !(fixture[programOffset] == 0 && fixture[programOffset + 1] == 9 && fixture[programOffset + 2] == 2)) ++programOffset;
    assert(programOffset + 2 < fixture.size());
    const uint32_t lineSize = static_cast<uint32_t>(readU64At(fixture, sectionOffset + 64 + 32));
    std::vector<unsigned char> malformedLeb = fixture;
    for (uint32_t i = programOffset + 12; i < 64 + lineSize; ++i) malformedLeb[i] = 0x80;
    static DebugDwarfMapper lebFailure = {};
    assert(!DebugDwarfMapperLoad(&lebFailure, "D:/fixture", "fixture", "target", "amd64",
                                 "fixture.elf", malformedLeb.size(), "a", 7,
                                 &malformedLeb[0], malformedLeb.size(), 1, &error));
    assert(error == DebugDwarfError::MalformedDwarf);

    std::vector<unsigned char> unsupportedOpcode = fixture;
    unsupportedOpcode[programOffset + 2] = 0x7f;
    static DebugDwarfMapper opcodeFailure = {};
    assert(!DebugDwarfMapperLoad(&opcodeFailure, "D:/fixture", "fixture", "target", "amd64",
                                 "fixture.elf", unsupportedOpcode.size(), "a", 7,
                                 &unsupportedOpcode[0], unsupportedOpcode.size(), 1, &error));
    assert(error == DebugDwarfError::UnsupportedOpcode);

    std::vector<unsigned char> bad = fixture; bad[0] = 0;
    static DebugDwarfMapper malformed = {}; assert(!DebugDwarfMapperLoad(&malformed, "D:/fixture", "fixture", "target", "amd64", "fixture.elf", bad.size(), "a", 7, &bad[0], bad.size(), 1, &error));
    assert(error == DebugDwarfError::MalformedElf);
    std::cout << "Developer Studio DWARF source mapping PASS\n";
    return 0;
}
