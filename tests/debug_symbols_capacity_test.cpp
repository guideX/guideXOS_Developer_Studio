#include "developer_studio_debug_symbols.h"

#include <cstring>
#include <fstream>
#include <iostream>
#include <cstdlib>
#include <string>
#include <vector>

static bool readFile(const char* path, std::vector<unsigned char>* bytes) {
    if (!path || !bytes) return false;
    std::ifstream input(path, std::ios::binary);
    if (!input) return false;
    input.seekg(0, std::ios::end);
    const std::streamoff size = input.tellg();
    if (size <= 0) return false;
    input.seekg(0, std::ios::beg);
    bytes->resize(static_cast<size_t>(size));
    input.read(reinterpret_cast<char*>(&(*bytes)[0]), size);
    return input.good() || input.eof();
}

static uint16_t readU16At(const std::vector<unsigned char>& bytes, uint32_t offset) {
    return static_cast<uint16_t>(bytes[offset]) |
        static_cast<uint16_t>(static_cast<uint16_t>(bytes[offset + 1]) << 8);
}

static uint32_t readU32At(const std::vector<unsigned char>& bytes, uint32_t offset) {
    return static_cast<uint32_t>(bytes[offset]) |
        (static_cast<uint32_t>(bytes[offset + 1]) << 8) |
        (static_cast<uint32_t>(bytes[offset + 2]) << 16) |
        (static_cast<uint32_t>(bytes[offset + 3]) << 24);
}

static uint64_t readU64At(const std::vector<unsigned char>& bytes, uint32_t offset) {
    uint64_t value = 0;
    for (uint32_t i = 0; i < 8; ++i)
        value |= static_cast<uint64_t>(bytes[offset + i]) << (i * 8);
    return value;
}

static void patchU16(std::vector<unsigned char>& bytes, uint32_t offset, uint16_t value) {
    bytes[offset] = static_cast<unsigned char>(value);
    bytes[offset + 1] = static_cast<unsigned char>(value >> 8);
}

static void patchU32(std::vector<unsigned char>& bytes, uint32_t offset, uint32_t value) {
    for (uint32_t i = 0; i < 4; ++i) bytes[offset + i] = static_cast<unsigned char>(value >> (i * 8));
}

static void patchU64(std::vector<unsigned char>& bytes, uint32_t offset, uint64_t value) {
    for (uint32_t i = 0; i < 8; ++i) bytes[offset + i] = static_cast<unsigned char>(value >> (i * 8));
}

static bool copySection(const std::vector<unsigned char>& elf, const char* wantedName,
                        std::vector<unsigned char>* section) {
    if (!wantedName || !section || elf.size() < 64) return false;
    const uint64_t tableOffset = readU64At(elf, 40);
    const uint16_t entrySize = readU16At(elf, 58);
    const uint16_t count = readU16At(elf, 60);
    const uint16_t stringIndex = readU16At(elf, 62);
    if (entrySize < 64 || stringIndex >= count ||
        tableOffset > elf.size() || static_cast<uint64_t>(count) * entrySize > elf.size() - tableOffset)
        return false;
    const uint32_t stringHeader = static_cast<uint32_t>(tableOffset + static_cast<uint64_t>(stringIndex) * entrySize);
    const uint64_t stringsOffset = readU64At(elf, stringHeader + 24);
    const uint64_t stringsSize = readU64At(elf, stringHeader + 32);
    if (stringsOffset > elf.size() || stringsSize > elf.size() - stringsOffset) return false;
    for (uint16_t i = 1; i < count; ++i) {
        const uint32_t header = static_cast<uint32_t>(tableOffset + static_cast<uint64_t>(i) * entrySize);
        const uint32_t nameOffset = readU32At(elf, header);
        if (nameOffset >= stringsSize) return false;
        const char* name = reinterpret_cast<const char*>(&elf[static_cast<size_t>(stringsOffset) + nameOffset]);
        const uint64_t remaining = stringsSize - nameOffset;
        uint64_t length = 0;
        while (length < remaining && name[length] != '\0') ++length;
        if (length == remaining) return false;
        bool matches = true;
        uint32_t wantedLength = 0;
        while (wantedName[wantedLength] != '\0') ++wantedLength;
        if (length != wantedLength) matches = false;
        else for (uint32_t j = 0; j < wantedLength; ++j)
            if (name[j] != wantedName[j]) { matches = false; break; }
        if (!matches) continue;
        const uint64_t offset = readU64At(elf, header + 24);
        const uint64_t size = readU64At(elf, header + 32);
        if (offset > elf.size() || size > elf.size() - offset) return false;
        section->assign(elf.begin() + static_cast<size_t>(offset),
                        elf.begin() + static_cast<size_t>(offset + size));
        return true;
    }
    return false;
}

static std::vector<unsigned char> syntheticDieFixture(const std::vector<unsigned char>& baseElf,
                                                       uint32_t dieCount,
                                                       bool malformedRoot = false) {
    if (dieCount == 0 || baseElf.size() < 120) return std::vector<unsigned char>();
    std::vector<unsigned char> line, lineStrings;
    if (!copySection(baseElf, ".debug_line", &line) ||
        !copySection(baseElf, ".debug_line_str", &lineStrings)) return std::vector<unsigned char>();

    std::vector<unsigned char> info;
    info.push_back(0); info.push_back(0); info.push_back(0); info.push_back(0); // unit length
    info.push_back(5); info.push_back(0); // DWARF 5
    info.push_back(1); // DW_UT_compile
    info.push_back(8); // address size
    info.push_back(0); info.push_back(0); info.push_back(0); info.push_back(0); // abbrev offset
    info.push_back(malformedRoot ? 3 : 1); // compile-unit abbreviation
    for (uint32_t i = 1; i < dieCount; ++i) info.push_back(2); // leaf lexical-block DIEs
    info.push_back(0); // close the root's child list
    const uint32_t infoLength = static_cast<uint32_t>(info.size() - 4);
    for (uint32_t i = 0; i < 4; ++i) info[i] = static_cast<unsigned char>(infoLength >> (i * 8));

    std::vector<unsigned char> abbrev = {
        1, 0x11, 1, 0, 0, // code 1: DW_TAG_compile_unit, with children, no attributes
        2, 0x0b, 0, 0, 0, // code 2: DW_TAG_lexical_block, no children or attributes
        0
    };
    std::vector<unsigned char> debugStrings(1, 0);
    std::vector<unsigned char> stringOffsets(4, 0);
    std::vector<unsigned char> addresses;
    std::vector<unsigned char> shStrings(1, 0);
    auto appendName = [&shStrings](const char* name) -> uint32_t {
        const uint32_t offset = static_cast<uint32_t>(shStrings.size());
        while (*name) shStrings.push_back(static_cast<unsigned char>(*name++));
        shStrings.push_back(0);
        return offset;
    };
    const uint32_t lineName = appendName(".debug_line");
    const uint32_t lineStringName = appendName(".debug_line_str");
    const uint32_t infoName = appendName(".debug_info");
    const uint32_t abbrevName = appendName(".debug_abbrev");
    const uint32_t debugStringName = appendName(".debug_str");
    const uint32_t stringOffsetsName = appendName(".debug_str_offsets");
    const uint32_t addressName = appendName(".debug_addr");
    const uint32_t shStringName = appendName(".shstrtab");

    std::vector<unsigned char> elf(baseElf.begin(), baseElf.begin() + 64);
    elf.resize(120, 0); // ELF header plus one synthetic executable PT_LOAD.
    auto appendSection = [&elf](const std::vector<unsigned char>& data) -> uint64_t {
        const uint64_t offset = elf.size();
        elf.insert(elf.end(), data.begin(), data.end());
        return offset;
    };
    const uint64_t lineOffset = appendSection(line);
    const uint64_t lineStringOffset = appendSection(lineStrings);
    const uint64_t infoOffset = appendSection(info);
    const uint64_t abbrevOffset = appendSection(abbrev);
    const uint64_t debugStringOffset = appendSection(debugStrings);
    const uint64_t stringOffsetsOffset = appendSection(stringOffsets);
    const uint64_t addressOffset = appendSection(addresses);
    const uint64_t shStringOffset = appendSection(shStrings);
    const uint64_t sectionTableOffset = elf.size();
    const uint16_t sectionCount = 9;
    elf.resize(static_cast<size_t>(sectionTableOffset + sectionCount * 64), 0);
    auto putSection = [&elf, sectionTableOffset](uint32_t index, uint32_t name, uint32_t type,
                                                uint64_t offset, uint64_t size) {
        const uint32_t header = static_cast<uint32_t>(sectionTableOffset + static_cast<uint64_t>(index) * 64);
        patchU32(elf, header, name);
        patchU32(elf, header + 4, type);
        patchU64(elf, header + 24, offset);
        patchU64(elf, header + 32, size);
    };
    putSection(1, lineName, 1, lineOffset, line.size());
    putSection(2, lineStringName, 3, lineStringOffset, lineStrings.size());
    putSection(3, infoName, 1, infoOffset, info.size());
    putSection(4, abbrevName, 1, abbrevOffset, abbrev.size());
    putSection(5, debugStringName, 3, debugStringOffset, debugStrings.size());
    putSection(6, stringOffsetsName, 1, stringOffsetsOffset, stringOffsets.size());
    putSection(7, addressName, 1, addressOffset, addresses.size());
    putSection(8, shStringName, 3, shStringOffset, shStrings.size());
    patchU64(elf, 40, sectionTableOffset);
    patchU64(elf, 32, 64);
    patchU16(elf, 54, 56);
    patchU16(elf, 56, 1);
    patchU16(elf, 58, 64);
    patchU16(elf, 60, sectionCount);
    patchU16(elf, 62, 8);
    patchU32(elf, 64, 1); // PT_LOAD
    patchU32(elf, 68, 5); // PF_R | PF_X
    patchU64(elf, 72, 0);
    patchU64(elf, 80, 0x401000);
    patchU64(elf, 88, 0x401000);
    patchU64(elf, 96, elf.size());
    patchU64(elf, 104, elf.size() + 0x1000u);
    patchU64(elf, 112, 0x1000);
    return elf;
}

static bool loadSynthetic(const std::vector<unsigned char>& elf, const char* projectRoot,
                          const char* executablePath,
                          guidexos::developer_studio::DebugDwarfMapper* mapper,
                          guidexos::developer_studio::DebugDwarfError* error) {
    char hash[65] = {};
    if (!mapper || !error || elf.empty() ||
        !guidexos::developer_studio::DebugDwarfComputeSha256(elf.data(), elf.size(), hash, sizeof(hash)))
        return false;
    return guidexos::developer_studio::DebugDwarfMapperLoad(
        mapper, projectRoot, "debugger-synthetic-dwarf", "native", "amd64",
        executablePath, elf.size(), hash, 1, elf.data(), elf.size(), 1, error);
}

int main(int argc, char** argv) {
    if (argc < 2 || argc > 5) {
        std::cerr << "usage: debug_symbols_capacity_test <fixture.elf> [project-root] [minimum-line-addresses] [required-breakpoint-line]\n";
        return 2;
    }

    std::vector<unsigned char> elf;
    if (!readFile(argv[1], &elf)) {
        std::cerr << "unable to read fixture: " << argv[1] << "\n";
        return 2;
    }

    char sha256[65] = {};
    if (!guidexos::developer_studio::DebugDwarfComputeSha256(
            &elf[0], elf.size(), sha256, sizeof(sha256))) {
        std::cerr << "unable to hash fixture\n";
        return 1;
    }

    guidexos::developer_studio::DebugDwarfMapper mapper = {};
    guidexos::developer_studio::DebugDwarfError error =
        guidexos::developer_studio::DebugDwarfError::None;
    const char* projectRoot = argc >= 3 ? argv[2] :
        "D:/dev/guideXOS_Developer_Studio/tests/fixtures/debugger-phase3b";
    const uint32_t minimumLineAddresses = argc >= 4 ?
        static_cast<uint32_t>(std::strtoul(argv[3], nullptr, 10)) : 0;
    const uint32_t requiredBreakpointLine = argc >= 5 ?
        static_cast<uint32_t>(std::strtoul(argv[4], nullptr, 10)) : 0;
    const bool loaded = guidexos::developer_studio::DebugDwarfMapperLoad(
        &mapper,
        projectRoot,
        "debugger-fixture", "native", "amd64", argv[1], elf.size(), sha256,
        1, &elf[0], elf.size(), 1, &error);
    static uint64_t mappedAddresses[guidexos::developer_studio::kDebugMapperMaxLineRows] = {};
    uint32_t maximumLineAddressCount = 0;
    uint16_t maximumLineSourceIndex = 0;
    uint32_t maximumLineNumber = 0;
    uint64_t maximumPrimaryAddress = 0;
    for (uint32_t i = 0; i < mapper.lineKeyCount; ++i) {
        const guidexos::developer_studio::DebugDwarfLineKey& key = mapper.lineKeys[i];
        uint32_t addressCount = 0;
        uint64_t primaryAddress = 0;
        guidexos::developer_studio::DebugDwarfError mapError =
            guidexos::developer_studio::DebugDwarfError::None;
        if (!guidexos::developer_studio::DebugDwarfMapperMapSourceToAddresses(
                &mapper, mapper.sourceFiles[key.sourceFileIndex].relativePath, key.line,
                mappedAddresses, guidexos::developer_studio::kDebugMapperMaxLineRows,
                &addressCount, &primaryAddress, &mapError)) {
            std::cerr << "source-line lookup failed for "
                      << mapper.sourceFiles[key.sourceFileIndex].relativePath << ":"
                      << key.line << " error="
                      << guidexos::developer_studio::DebugDwarfErrorName(mapError) << "\n";
            return 1;
        }
        if (addressCount > maximumLineAddressCount) {
            maximumLineAddressCount = addressCount;
            maximumLineSourceIndex = key.sourceFileIndex;
            maximumLineNumber = key.line;
            maximumPrimaryAddress = primaryAddress;
        }
    }
    bool queryCapacityTruncationPreserved = true;
    if (maximumLineAddressCount > 1u) {
        uint32_t boundedCount = 0;
        uint64_t boundedPrimary = 0;
        guidexos::developer_studio::DebugDwarfError boundedError =
            guidexos::developer_studio::DebugDwarfError::None;
        const bool mapped = guidexos::developer_studio::DebugDwarfMapperMapSourceToAddresses(
            &mapper, mapper.sourceFiles[maximumLineSourceIndex].relativePath, maximumLineNumber,
            mappedAddresses, maximumLineAddressCount - 1u, &boundedCount,
            &boundedPrimary, &boundedError);
        queryCapacityTruncationPreserved = mapped &&
            boundedCount == maximumLineAddressCount - 1u &&
            boundedPrimary == maximumPrimaryAddress &&
            boundedError == guidexos::developer_studio::DebugDwarfError::Truncated;
    }
    uint32_t requiredBreakpointAddressCount = 0;
    uint64_t requiredBreakpointPrimary = 0;
    bool requiredBreakpointMapped = true;
    bool requiredBreakpointReverseMapped = true;
    char requiredBreakpointSourcePath[guidexos::developer_studio::kDebugMapperMaxPathBytes] = {};
    uint32_t requiredBreakpointMappedLine = 0;
    uint32_t requiredBreakpointMappedColumn = 0;
    if (requiredBreakpointLine != 0) {
        guidexos::developer_studio::DebugDwarfError breakpointError =
            guidexos::developer_studio::DebugDwarfError::None;
        requiredBreakpointMapped = guidexos::developer_studio::DebugDwarfMapperMapSourceToAddresses(
            &mapper, "src/main.cpp", requiredBreakpointLine, mappedAddresses,
            guidexos::developer_studio::kDebugMapperMaxLineRows,
            &requiredBreakpointAddressCount, &requiredBreakpointPrimary, &breakpointError) &&
            requiredBreakpointAddressCount != 0 &&
            breakpointError == guidexos::developer_studio::DebugDwarfError::None;
        if (requiredBreakpointMapped) {
            requiredBreakpointReverseMapped =
                guidexos::developer_studio::DebugDwarfMapperMapAddressToSource(
                    &mapper, requiredBreakpointPrimary, requiredBreakpointSourcePath,
                    sizeof(requiredBreakpointSourcePath), &requiredBreakpointMappedLine,
                    &requiredBreakpointMappedColumn, &breakpointError) &&
                std::strcmp(requiredBreakpointSourcePath, "src/main.cpp") == 0 &&
                requiredBreakpointMappedLine == requiredBreakpointLine;
        }
        std::cerr << "required source breakpoint mapping: line=" << requiredBreakpointLine
                  << " result=" << (requiredBreakpointMapped ? "PASS" : "FAIL")
                  << " source=src/main.cpp normalized="
                  << (requiredBreakpointReverseMapped ? requiredBreakpointSourcePath : "unmapped")
                  << " reverse_line=" << requiredBreakpointMappedLine
                  << " addresses=" << requiredBreakpointAddressCount
                  << " primary=0x" << std::hex << requiredBreakpointPrimary << std::dec
                  << " error=" << guidexos::developer_studio::DebugDwarfErrorName(breakpointError)
                  << "\n";
    }

    std::cerr << "bare DWARF parse: loaded=" << (loaded ? 1 : 0)
              << " state=" << guidexos::developer_studio::DebugDwarfMapperStateName(mapper.state)
              << " error=" << guidexos::developer_studio::DebugDwarfErrorName(error)
              << " truncated=" << (mapper.truncated ? 1 : 0)
              << " stage=" << (mapper.parseFailureStage[0] ? mapper.parseFailureStage : "none")
              << " reason=" << (mapper.parseFailureReason[0] ? mapper.parseFailureReason : "none")
              << " offset=" << mapper.parseFailureOffset
              << " dies=" << mapper.debugInfoDieCount
              << " CUs=" << mapper.debugInfoCompilationUnitCount
              << " functions=" << mapper.debugInfoFunctionCount
              << " variables=" << mapper.debugInfoVariableCount
              << " sources=" << mapper.sourceFileCount
              << " external_sources=" << mapper.externalSourceCount
              << " line_rows=" << mapper.lineRowCount
              << " line_keys=" << mapper.lineKeyCount
              << " sequences=" << mapper.sequenceCount
              << " debug_info_bytes=" << mapper.debugInfoSectionBytes
              << " debug_abbrev_bytes=" << mapper.debugAbbrevSectionBytes
              << " debug_line_bytes=" << mapper.lineSectionBytes
              << " dwarf=" << mapper.dwarfVersion
              << " max_addresses_on_line=" << maximumLineAddressCount
              << " max_address_line="
              << (maximumLineSourceIndex < mapper.sourceFileCount ?
                  mapper.sourceFiles[maximumLineSourceIndex].relativePath : "none")
              << ":" << maximumLineNumber
              << " sha256=" << sha256 << "\n";
    std::cerr << "bounded storage: mapper_bytes=" << sizeof(mapper)
              << " line_key_record_bytes=" << sizeof(mapper.lineKeys[0])
              << " line_key_capacity=" << guidexos::developer_studio::kDebugMapperMaxLineKeys
              << " line_key_bytes=" << sizeof(mapper.lineKeys)
              << " die_record_bytes=" << sizeof(mapper.dies[0])
              << " die_capacity=" << guidexos::developer_studio::kDebugDwarfMaxDies
              << " die_bytes=" << sizeof(mapper.dies)
              << " line_row_record_bytes=" << sizeof(mapper.rows[0])
              << " line_row_capacity=" << guidexos::developer_studio::kDebugMapperMaxLineRows
              << " line_row_bytes=" << sizeof(mapper.rows)
              << " query_capacity_truncation=" << (queryCapacityTruncationPreserved ? "PASS" : "FAIL")
              << "\n";
    for (uint32_t i = 0; i < mapper.sourceFileCount; ++i) {
        std::cerr << "source_table[" << i << "]=" << mapper.sourceFiles[i].relativePath << "\n";
    }
    static guidexos::developer_studio::DebugDwarfMapper belowLimitMapper = {};
    std::vector<unsigned char> belowLimitElf = syntheticDieFixture(
        elf, guidexos::developer_studio::kDebugDwarfMaxDies - 1u);
    const bool belowLimitLoaded = loadSynthetic(
        belowLimitElf, projectRoot, argv[1], &belowLimitMapper, &error);
    if (!belowLimitLoaded || error != guidexos::developer_studio::DebugDwarfError::None ||
        belowLimitMapper.debugInfoDieCount != guidexos::developer_studio::kDebugDwarfMaxDies - 1u ||
        belowLimitMapper.truncated) {
        std::cerr << "synthetic DIE-below-capacity failed: loaded=" << (belowLimitLoaded ? 1 : 0)
                  << " error=" << guidexos::developer_studio::DebugDwarfErrorName(error)
                  << " dies=" << belowLimitMapper.debugInfoDieCount
                  << " truncated=" << (belowLimitMapper.truncated ? 1 : 0) << "\n";
        return 1;
    }
    static guidexos::developer_studio::DebugDwarfMapper dieLimitMapper = {};
    std::vector<unsigned char> dieLimitElf = syntheticDieFixture(
        elf, guidexos::developer_studio::kDebugDwarfMaxDies);
    const bool dieLimitLoaded = loadSynthetic(
        dieLimitElf, projectRoot, argv[1], &dieLimitMapper, &error);
    if (!dieLimitLoaded || error != guidexos::developer_studio::DebugDwarfError::None ||
        dieLimitMapper.debugInfoDieCount != guidexos::developer_studio::kDebugDwarfMaxDies ||
        dieLimitMapper.truncated) {
        std::cerr << "synthetic DIE-at-capacity failed: loaded=" << (dieLimitLoaded ? 1 : 0)
                  << " error=" << guidexos::developer_studio::DebugDwarfErrorName(error)
                  << " dies=" << dieLimitMapper.debugInfoDieCount
                  << " stage=" << dieLimitMapper.parseFailureStage
                  << " truncated=" << (dieLimitMapper.truncated ? 1 : 0)
                  << " bytes=" << dieLimitElf.size()
                  << " phoff=" << readU64At(dieLimitElf, 32)
                  << " phentsize=" << readU16At(dieLimitElf, 54)
                  << " phnum=" << readU16At(dieLimitElf, 56) << "\n";
        return 1;
    }
    static guidexos::developer_studio::DebugDwarfMapper dieOverLimitMapper = {};
    std::vector<unsigned char> dieOverLimitElf = syntheticDieFixture(
        elf, guidexos::developer_studio::kDebugDwarfMaxDies + 1u);
    const bool dieOverLimitLoaded = loadSynthetic(
        dieOverLimitElf, projectRoot, argv[1], &dieOverLimitMapper, &error);
    const bool capacityErrorPreserved = !dieOverLimitLoaded &&
        error == guidexos::developer_studio::DebugDwarfError::LimitExceeded &&
        dieOverLimitMapper.debugInfoDieCount == guidexos::developer_studio::kDebugDwarfMaxDies &&
        dieOverLimitMapper.truncated &&
        std::strcmp(dieOverLimitMapper.parseFailureStage, "dwarf_die_capacity") == 0 &&
        std::strcmp(dieOverLimitMapper.parseFailureReason, "die_table_capacity_exceeded") == 0;
    if (!capacityErrorPreserved) {
        std::cerr << "synthetic DIE-over-capacity classification failed: loaded="
                  << (dieOverLimitLoaded ? 1 : 0)
                  << " error=" << guidexos::developer_studio::DebugDwarfErrorName(error)
                  << " retained_dies=" << dieOverLimitMapper.debugInfoDieCount
                  << " stage=" << dieOverLimitMapper.parseFailureStage
                  << " reason=" << dieOverLimitMapper.parseFailureReason
                  << " truncated=" << (dieOverLimitMapper.truncated ? 1 : 0) << "\n";
        return 1;
    }
    static guidexos::developer_studio::DebugDwarfMapper malformedMapper = {};
    std::vector<unsigned char> malformedElf = syntheticDieFixture(elf, 2, true);
    const bool malformedLoaded = loadSynthetic(
        malformedElf, projectRoot, argv[1], &malformedMapper, &error);
    const bool malformedRejected = !malformedLoaded &&
        error == guidexos::developer_studio::DebugDwarfError::MalformedDwarf &&
        !malformedMapper.truncated && malformedMapper.debugInfoDieCount == 0;
    if (!malformedRejected) {
        std::cerr << "malformed synthetic DWARF classification failed: loaded="
                  << (malformedLoaded ? 1 : 0)
                  << " error=" << guidexos::developer_studio::DebugDwarfErrorName(error)
                  << " stage=" << malformedMapper.parseFailureStage
                  << " truncated=" << (malformedMapper.truncated ? 1 : 0) << "\n";
        return 1;
    }
    std::cerr << "synthetic DIE boundary: valid_below="
              << belowLimitMapper.debugInfoDieCount << " valid="
              << dieLimitMapper.debugInfoDieCount << " capacity="
              << guidexos::developer_studio::kDebugDwarfMaxDies
              << " next_error=limit_exceeded malformed_error=malformed_dwarf\n";
    if (!loaded || mapper.debugInfoDieCount <= 256u || mapper.truncated ||
        maximumLineAddressCount < minimumLineAddresses || !queryCapacityTruncationPreserved ||
        !requiredBreakpointMapped || !requiredBreakpointReverseMapped) {
        std::cerr << "bare DWARF parse regression: loaded=" << (loaded ? 1 : 0)
                  << " error="
                  << guidexos::developer_studio::DebugDwarfErrorName(error)
                  << " dies=" << mapper.debugInfoDieCount << " truncated="
                  << (mapper.truncated ? 1 : 0) << " max_addresses_on_line="
                  << maximumLineAddressCount << " minimum_required="
                  << minimumLineAddresses << "\n";
        return 1;
    }

    std::cout << "Developer Studio bare DWARF parse PASS dies="
              << mapper.debugInfoDieCount << " max_addresses_on_line="
              << maximumLineAddressCount << "\n";
    return 0;
}
