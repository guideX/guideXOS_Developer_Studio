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
bool DebugDwarfNormalizeSourcePath(const char* projectRoot, const char* rawPath,
                                   char* relativePath, uint32_t relativePathSize,
                                   bool* external);

} // namespace developer_studio
} // namespace guidexos
