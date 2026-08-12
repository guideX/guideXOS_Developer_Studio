#pragma once

#include "developer_studio_types.h"

namespace guidexos {
namespace developer_studio {

// Phase 2 deliberately supports the artifact shape emitted by the current
// hosted Native ELF toolchain. These limits keep malformed or unexpectedly
// large debug files from becoming an unbounded allocation surface.
static const uint32_t kDebugMapperMaxElfBytes = 16u * 1024u * 1024u;
static const uint32_t kDebugMapperMaxSectionBytes = 16u * 1024u * 1024u;
static const uint32_t kDebugMapperMaxSections = 256;
static const uint32_t kDebugMapperMaxDirectories = 128;
static const uint32_t kDebugMapperMaxFiles = 1024;
static const uint32_t kDebugMapperMaxSourceFiles = 512;
static const uint32_t kDebugMapperMaxLineRows = 131072;
static const uint32_t kDebugMapperMaxLineKeys = 32768;
static const uint32_t kDebugMapperMaxAddressesPerLine = 8;
static const uint32_t kDebugMapperMaxSequences = 2048;
static const uint32_t kDebugMapperMaxPathBytes = kMaxProjectPathBytes;
static const uint32_t kDebugMapperMaxStringBytes = 1024;
static const uint32_t kDebugMapperMaxSha256Bytes = 65;
static const uint32_t kDebugMapperMaxArchitectureBytes = 32;
static const uint32_t kDebugMapperMaxFunctionSymbols = 16384;
static const uint32_t kDebugMapperMaxExecutableSegments = 32;
static const uint32_t kDebugMapperMaxFunctionNameBytes = 128;
static const uint32_t kDebugDwarfMaxCompilationUnits = 32;
static const uint32_t kDebugDwarfMaxDies = 8192;
static const uint32_t kDebugDwarfMaxAbbreviations = 256;
static const uint32_t kDebugDwarfMaxAttributes = 32;
static const uint32_t kDebugDwarfMaxFunctions = 512;
static const uint32_t kDebugDwarfMaxVariables = 2048;
static const uint32_t kDebugDwarfMaxDisplayedVariables = 32;
static const uint32_t kDebugDwarfMaxExpressionBytes = 128;
static const uint32_t kDebugDwarfMaxTypeDepth = 32;
static const uint32_t kDebugDwarfMaxTypeDisplayBytes = 128;
static const uint32_t kDebugDwarfMaxVariableNameBytes = 128;
static const uint32_t kDebugDwarfMaxVariableValueBytes = 16;
static const uint32_t kDebugDwarfMaxExpressionOperations = 64;
static const uint32_t kDebugDwarfMaxExpressionStack = 16;
static const uint32_t kDebugDwarfMaxDereferences = 4;

enum class DebugDwarfMapperState {
    Empty = 0,
    Ready,
    Failed,
    Stale
};

enum class DebugDwarfError {
    None = 0,
    NoDebugInfo,
    MissingLineSection,
    MalformedElf,
    MalformedDwarf,
    UnsupportedDwarfVersion,
    UnsupportedForm,
    UnsupportedArchitecture,
    ArtifactChanged,
    SourceNotFound,
    LineNotMapped,
    Truncated,
    LimitExceeded,
    UnsupportedOpcode
};

struct DebugDwarfArtifactIdentity {
    char executablePath[kMaxProjectPathBytes];
    uint64_t executableSize;
    char sha256[kDebugMapperMaxSha256Bytes];
    uint64_t projectGeneration;
    char projectId[kMaxProjectIdBytes];
    char targetProfile[kMaxNameBytes];
    char architecture[kDebugMapperMaxArchitectureBytes];
    uint64_t loadBias;
    uint32_t mapperGeneration;
};

struct DebugDwarfSourceFile {
    char relativePath[kDebugMapperMaxPathBytes];
    bool external;
};

struct DebugDwarfLineKey {
    uint16_t sourceFileIndex;
    uint32_t line;
    uint32_t addressCount;
    uint64_t addresses[kDebugMapperMaxAddressesPerLine];
    uint64_t primaryAddress;
    bool hasStmtAddress;
};

struct DebugDwarfLineRow {
    uint16_t sourceFileIndex;
    uint32_t line;
    uint32_t column;
    uint32_t sequence;
    uint32_t order;
    uint64_t address;
    uint64_t endAddress;
    bool isStmt;
};

struct DebugDwarfFunctionSymbol {
    uint64_t startAddress;
    uint64_t size;
    char name[kDebugMapperMaxFunctionNameBytes];
};

enum class DebugDwarfVariableKind {
    Argument = 0,
    Local
};

enum class DebugDwarfVariableState {
    Available = 0,
    Unavailable,
    UnsupportedLocation,
    OutOfScope,
    Stale,
    ReadFailure,
    MalformedDebugInfo
};

enum class DebugDwarfValueKind {
    Unavailable = 0,
    SignedInteger,
    UnsignedInteger,
    Boolean,
    Pointer,
    FloatingPoint,
    Address,
    Aggregate,
    Array,
    Bytes
};

enum class DebugDwarfLocationKind {
    Unavailable = 0,
    Register,
    MemoryAddress,
    ImmediateValue,
    Unsupported,
    Malformed
};

// These values intentionally retain only the bounded DIE data needed by the
// first locals/arguments implementation. No raw DIE pointers survive a load.
struct DebugDwarfDieInfo {
    uint64_t offset;
    uint32_t unitIndex;
    uint32_t parentIndex;
    uint16_t tag;
    uint16_t depth;
    bool hasChildren;
    bool hasName;
    bool nameIsStringIndex;
    uint64_t nameStringIndex;
    char name[kDebugDwarfMaxVariableNameBytes];
    bool hasType;
    uint64_t typeReference;
    bool hasLowPc;
    bool lowPcIsAddressIndex;
    uint64_t lowPc;
    uint64_t lowPcIndex;
    bool hasHighPc;
    uint64_t highPcOffset;
    bool highPcIsAddress;
    bool hasRanges;
    uint64_t rangesOffset;
    bool hasFrameBase;
    uint32_t frameBaseLength;
    uint8_t frameBase[kDebugDwarfMaxExpressionBytes];
    bool hasLocation;
    bool locationIsList;
    uint32_t locationLength;
    uint8_t location[kDebugDwarfMaxExpressionBytes];
    bool locationAddressIsIndex;
    uint64_t locationAddressIndex;
    uint64_t locationAddress;
    bool hasByteSize;
    uint64_t byteSize;
    bool hasEncoding;
    uint64_t encoding;
    bool hasDeclFile;
    uint32_t declFile;
    bool hasDeclLine;
    uint32_t declLine;
    bool hasConstValue;
    bool constValueSigned;
    uint64_t constValue;
    bool hasCount;
    uint64_t count;
    bool hasStrOffsetsBase;
    uint32_t strOffsetsBase;
    bool hasAddrBase;
    uint32_t addrBase;
    bool artificial;
};

struct DebugDwarfCompilationUnitInfo {
    uint64_t sectionOffset;
    uint64_t unitEnd;
    uint32_t rootDieIndex;
    uint32_t dieCount;
    uint32_t abbrevOffset;
    uint32_t strOffsetsBase;
    uint32_t addrBase;
    uint8_t addressSize;
    uint16_t version;
    uint8_t unitType;
};

struct DebugDwarfFunctionInfo {
    uint64_t dieOffset;
    uint32_t dieIndex;
    uint64_t lowPc;
    uint64_t highPc;
    bool hasRange;
    char name[kDebugMapperMaxFunctionNameBytes];
    uint32_t frameBaseLength;
    uint8_t frameBase[kDebugDwarfMaxExpressionBytes];
};

struct DebugDwarfVariableInfo {
    uint64_t dieOffset;
    uint32_t dieIndex;
    uint32_t functionIndex;
    DebugDwarfVariableKind kind;
    uint16_t scopeDepth;
    char name[kDebugDwarfMaxVariableNameBytes];
    uint64_t typeDieOffset;
    uint32_t declFile;
    uint32_t declLine;
    bool artificial;
};

struct DebugDwarfRegisterSet {
    bool valid;
    uint64_t rax;
    uint64_t rbx;
    uint64_t rcx;
    uint64_t rdx;
    uint64_t rsi;
    uint64_t rdi;
    uint64_t rbp;
    uint64_t rsp;
    uint64_t r8;
    uint64_t r9;
    uint64_t r10;
    uint64_t r11;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
    uint64_t rip;
    uint64_t rflags;
};

struct DebugDwarfFrameContext {
    uint32_t frameIndex;
    uint64_t instructionAddress;
    uint64_t processId;
    uint64_t nativeRuntimeId;
    uint64_t threadId;
    uint64_t sessionGeneration;
    uint64_t stopGeneration;
    bool frameBaseKnown;
    uint64_t frameBase;
    DebugDwarfRegisterSet registers;
};

struct DebugDwarfVariable {
    uint64_t dieOffset;
    DebugDwarfVariableKind kind;
    DebugDwarfVariableState state;
    DebugDwarfValueKind valueKind;
    DebugDwarfLocationKind locationKind;
    uint64_t typeDieOffset;
    uint64_t address;
    uint16_t registerNumber;
    uint16_t scopeDepth;
    uint32_t rawByteCount;
    uint8_t rawBytes[kDebugDwarfMaxVariableValueBytes];
    char name[kDebugDwarfMaxVariableNameBytes];
    char typeDisplay[kDebugDwarfMaxTypeDisplayBytes];
    char valueDisplay[kDebugDwarfMaxTypeDisplayBytes];
    char locationDisplay[kDebugDwarfMaxTypeDisplayBytes];
    char status[kDebugDwarfMaxTypeDisplayBytes];
};

struct DebugDwarfVariableView {
    bool valid;
    bool stale;
    uint32_t frameIndex;
    uint64_t sessionGeneration;
    uint64_t stopGeneration;
    uint64_t frameInstructionAddress;
    uint32_t functionIndex;
    char functionName[kDebugMapperMaxFunctionNameBytes];
    uint32_t variableCount;
    uint32_t argumentCount;
    uint32_t localCount;
    char status[kDebugDwarfMaxTypeDisplayBytes];
    DebugDwarfVariable variables[kDebugDwarfMaxDisplayedVariables];
};

typedef bool (*DebugDwarfReadMemoryFn)(void* userData, uint64_t sessionGeneration,
                                       uint64_t processId, uint64_t nativeRuntimeId,
                                       uint64_t threadId, uint64_t stopGeneration,
                                       uint64_t address, uint8_t* bytes, uint32_t requested,
                                       uint32_t* returned);

struct DebugDwarfExecutableSegment {
    uint64_t startAddress;
    uint64_t endAddress;
};

struct DebugDwarfMapper {
    DebugDwarfMapperState state;
    DebugDwarfError error;
    DebugDwarfArtifactIdentity identity;
    uint16_t dwarfVersion;
    uint8_t addressSize;
    uint32_t lineSectionBytes;
    uint32_t sourceFileCount;
    uint32_t externalSourceCount;
    uint32_t lineRowCount;
    uint32_t lineKeyCount;
    uint32_t addressOrderCount;
    uint32_t sequenceCount;
    uint32_t functionSymbolCount;
    uint32_t executableSegmentCount;
    uint64_t ehFrameSectionBytes;
    uint64_t ehFrameHeaderSectionBytes;
    uint64_t debugFrameSectionBytes;
    uint64_t debugInfoSectionBytes;
    uint64_t debugAbbrevSectionBytes;
    uint64_t debugStringSectionBytes;
    uint64_t debugStringOffsetsSectionBytes;
    uint64_t debugLineStringSectionBytes;
    uint64_t debugAddressSectionBytes;
    uint64_t debugLocationListsSectionBytes;
    uint32_t debugInfoDieCount;
    uint32_t debugInfoCompilationUnitCount;
    uint32_t debugInfoFunctionCount;
    uint32_t debugInfoVariableCount;
    uint32_t debugInfoParseMilliseconds;
    bool debugInfoReady;
    DebugDwarfCompilationUnitInfo compilationUnits[kDebugDwarfMaxCompilationUnits];
    DebugDwarfDieInfo dies[kDebugDwarfMaxDies];
    DebugDwarfFunctionInfo debugFunctions[kDebugDwarfMaxFunctions];
    DebugDwarfVariableInfo debugVariables[kDebugDwarfMaxVariables];
    bool truncated;
    char statusText[160];
    DebugDwarfSourceFile sourceFiles[kDebugMapperMaxSourceFiles];
    DebugDwarfLineKey lineKeys[kDebugMapperMaxLineKeys];
    DebugDwarfLineRow rows[kDebugMapperMaxLineRows];
    uint32_t addressOrder[kDebugMapperMaxLineRows];
    DebugDwarfFunctionSymbol functionSymbols[kDebugMapperMaxFunctionSymbols];
    DebugDwarfExecutableSegment executableSegments[kDebugMapperMaxExecutableSegments];

    // These are parser scratch fields. They are cleared before Load returns;
    // no result retains a pointer into the ELF buffer.
    char directories[kDebugMapperMaxDirectories][kDebugMapperMaxPathBytes];
    uint32_t directoryCount;
    uint16_t currentFileSources[kDebugMapperMaxFiles + 1];
    uint32_t currentFileCount;
};

const char* DebugDwarfMapperStateName(DebugDwarfMapperState state);
const char* DebugDwarfErrorName(DebugDwarfError error);

// Computes the artifact identity from the exact bytes captured by the host.
// The debugger uses this before mapping so a rebuild or replacement between
// build completion and symbol loading cannot silently reuse the old identity.
bool DebugDwarfComputeSha256(const unsigned char* bytes, uint64_t size,
                             char* output, uint32_t outputSize);

void DebugDwarfMapperReset(DebugDwarfMapper* mapper);
bool DebugDwarfMapperLoad(DebugDwarfMapper* mapper, const char* projectRoot,
                          const char* projectId, const char* targetProfile,
                          const char* architecture, const char* executablePath,
                          uint64_t executableSize, const char* artifactSha256,
                          uint64_t projectGeneration, const unsigned char* elfBytes,
                          uint64_t elfSize, uint32_t mapperGeneration,
                          DebugDwarfError* error);
bool DebugDwarfMapperIsReady(const DebugDwarfMapper* mapper);
bool DebugDwarfMapperMatchesArtifact(const DebugDwarfMapper* mapper,
                                     const char* projectRoot, const char* projectId,
                                     const char* targetProfile, const char* architecture,
                                     const char* executablePath, uint64_t executableSize,
                                     const char* artifactSha256,
                                     uint64_t projectGeneration);
bool DebugDwarfMapperMapSourceToAddresses(const DebugDwarfMapper* mapper,
                                          const char* relativePath, uint32_t line,
                                          uint64_t* addresses, uint32_t capacity,
                                          uint32_t* outCount, uint64_t* outPrimary,
                                          DebugDwarfError* error);
bool DebugDwarfMapperMapAddressToSource(const DebugDwarfMapper* mapper,
                                        uint64_t address, char* relativePath,
                                        uint32_t relativePathSize, uint32_t* line,
                                        uint32_t* column, DebugDwarfError* error);
bool DebugDwarfMapperLookupFunction(const DebugDwarfMapper* mapper, uint64_t address,
                                    char* name, uint32_t nameSize, uint64_t* startAddress,
                                    uint64_t* size, DebugDwarfError* error);
bool DebugDwarfMapperIsExecutableAddress(const DebugDwarfMapper* mapper, uint64_t address);
bool DebugDwarfParseVariables(DebugDwarfMapper* mapper, const unsigned char* elfBytes,
                              uint64_t elfSize);
const char* DebugDwarfVariableKindName(DebugDwarfVariableKind kind);
const char* DebugDwarfVariableStateName(DebugDwarfVariableState state);
const char* DebugDwarfValueKindName(DebugDwarfValueKind kind);
const char* DebugDwarfLocationKindName(DebugDwarfLocationKind kind);
bool DebugDwarfMapperLookupDebugFunction(const DebugDwarfMapper* mapper, uint64_t address,
                                         uint32_t* functionIndex, DebugDwarfError* error);
bool DebugDwarfInspectVariables(const DebugDwarfMapper* mapper,
                                const DebugDwarfFrameContext& frame,
                                DebugDwarfReadMemoryFn readMemory, void* userData,
                                DebugDwarfVariableView* view);
bool DebugDwarfNormalizeSourcePath(const char* projectRoot, const char* rawPath,
                                   char* relativePath, uint32_t relativePathSize,
                                   bool* external);

} // namespace developer_studio
} // namespace guidexos
