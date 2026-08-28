#pragma once

#include <stdint.h>

namespace guidexos {
namespace developer_studio {

/* The output store deliberately uses fixed-size storage.  These limits are
 * part of the hosted-development contract and keep the Native ELF image
 * independent of an allocator or an unbounded process log. */
#if defined(GXOS_DEVELOPER_STUDIO_BARE_METAL)
static const uint32_t kMaxOutputRecords = 64;
static const uint32_t kMaxOutputRecordsPerOperation = 32;
static const uint32_t kMaxOutputOperations = 4;
#else
static const uint32_t kMaxOutputRecords = 512;
static const uint32_t kMaxOutputRecordsPerOperation = 128;
static const uint32_t kMaxOutputOperations = 20;
#endif
static const uint32_t kMaxOutputTextBytes = 512;
static const uint32_t kMaxOutputProjectIdBytes = 96;
static const uint32_t kMaxOutputPathBytes = 160;
static const uint32_t kMaxOutputDiagnosticCodeBytes = 32;
static const uint32_t kMaxOutputVisibleRows = 6;

enum class OutputSource {
    DeveloperStudio = 0,
    Build,
    Compiler,
    Linker,
    ArtifactValidator,
    Run,
    Runtime,
    Application,
    AppModel,
    System
};

enum class OutputSeverity {
    Trace = 0,
    Information,
    Success,
    Warning,
    Error,
    Fatal
};

enum class OutputCategory {
    General = 0,
    BuildLifecycle,
    BuildDiagnostic,
    Artifact,
    RunLifecycle,
    RuntimeDiagnostic,
    ApplicationOutput,
    Internal
};

enum class OutputChannel {
    All = 0,
    Build,
    Run,
    Application,
    DeveloperStudio
};

enum class OutputOperationType {
    Build = 0,
    Run,
    Internal
};

enum class OutputStream {
    Unknown = 0,
    StandardOutput,
    StandardError
};

enum class OutputErrorCode {
    None = 0,
    OperationNotFound,
    OperationStale,
    RecordLimit,
    TextTruncated,
    InvalidUtf8,
    InvalidDiagnosticPath,
    DiagnosticPathOutsideProject,
    InvalidDiagnosticLine,
    InvalidDiagnosticColumn,
    DiagnosticFileNotFound,
    DiagnosticProjectMismatch,
    NavigationNoProject,
    NavigationOpenFailed,
    NavigationLocationClamped
};

struct OutputRecord {
    uint64_t sequence;
    uint64_t operationId;
    OutputSource source;
    OutputSeverity severity;
    OutputCategory category;
    OutputStream stream;
    char text[kMaxOutputTextBytes];
    char projectId[kMaxOutputProjectIdBytes];
    char relativeFilePath[kMaxOutputPathBytes];
    uint32_t line;
    uint32_t column;
    uint32_t endLine;
    uint32_t endColumn;
    char diagnosticCode[kMaxOutputDiagnosticCodeBytes];
    bool hasLocation;
    bool isTruncated;
    bool isTerminal;
};

struct OutputOperation {
    bool used;
    bool completed;
    bool terminalPublished;
    bool truncationPublished;
    uint64_t operationId;
    OutputOperationType type;
    uint32_t retainedRecordCount;
    char projectId[kMaxOutputProjectIdBytes];
};

struct OutputService {
    OutputRecord records[kMaxOutputRecords];
    OutputOperation operations[kMaxOutputOperations];
    uint32_t recordCount;
    uint32_t operationCount;
    uint64_t nextSequence;
    uint64_t nextOperationId;
    OutputChannel activeChannel;
};

const char* OutputSourceName(OutputSource source);
const char* OutputSeverityName(OutputSeverity severity);
const char* OutputCategoryName(OutputCategory category);
const char* OutputChannelName(OutputChannel channel);
const char* OutputOperationTypeName(OutputOperationType type);
const char* OutputErrorName(OutputErrorCode error);

void OutputServiceInit(OutputService* service);
uint64_t OutputServiceBeginOperation(OutputService* service, OutputOperationType type, const char* projectId);
bool OutputServiceAppendRecord(OutputService* service, uint64_t operationId, const OutputRecord& record, OutputErrorCode* error = nullptr);
bool OutputServiceAppendText(OutputService* service, uint64_t operationId, OutputSource source, OutputSeverity severity,
                             OutputCategory category, OutputStream stream, const char* text,
                             const char* projectId = nullptr, OutputErrorCode* error = nullptr);
bool OutputServiceAppendBuildLine(OutputService* service, uint64_t operationId, const char* projectRoot,
                                  const char* projectId, OutputStream stream, const char* text,
                                  OutputErrorCode* error = nullptr);
bool OutputServiceCompleteOperation(OutputService* service, uint64_t operationId, bool succeeded,
                                    const char* text, OutputErrorCode* error = nullptr);
bool OutputServiceClearOperation(OutputService* service, uint64_t operationId);
uint32_t OutputServiceClearCategory(OutputService* service, OutputCategory category);
uint32_t OutputServiceClearProblemsForProject(OutputService* service, const char* projectId);
uint32_t OutputServiceClearChannel(OutputService* service, OutputChannel channel);
void OutputServiceSelectActiveChannel(OutputService* service, OutputChannel channel);
OutputChannel OutputServiceActiveChannel(const OutputService* service);

uint32_t OutputServiceRecordCount(const OutputService* service);
const OutputRecord* OutputServiceRecordAt(const OutputService* service, uint32_t index);
uint32_t OutputServiceFilteredCount(const OutputService* service, OutputChannel channel);
const OutputRecord* OutputServiceFilteredAt(const OutputService* service, OutputChannel channel, uint32_t index);
uint32_t OutputServiceProblemCount(const OutputService* service, const char* projectId);
const OutputRecord* OutputServiceProblemAt(const OutputService* service, const char* projectId, uint32_t index);
void OutputServiceProblemCounts(const OutputService* service, const char* projectId, uint32_t* warnings, uint32_t* errors);

/* Returns true only for the narrow compiler/linker formats supported by this
 * phase.  A recognized diagnostic with a rejected location is still returned
 * as a Problem, but hasLocation remains false. */
bool ParseBuildDiagnostic(const char* projectRoot, const char* projectId, const char* text, OutputStream stream,
                          OutputRecord* record, OutputErrorCode* error = nullptr);

} // namespace developer_studio
} // namespace guidexos
