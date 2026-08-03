#include "developer_studio_output.h"

namespace guidexos {
namespace developer_studio {
namespace {

static void clearBytes(void* value, uint32_t size) {
    if (!value) return;
    unsigned char* bytes = static_cast<unsigned char*>(value);
    for (uint32_t i = 0; i < size; ++i) bytes[i] = 0;
}

static uint32_t lengthOf(const char* value, uint32_t capacity) {
    if (!value) return 0;
    uint32_t length = 0;
    while (length < capacity && value[length] != '\0') ++length;
    return length;
}

static bool equalText(const char* left, const char* right) {
    if (!left || !right) return left == right;
    uint32_t index = 0;
    while (left[index] != '\0' && right[index] != '\0') {
        if (left[index] != right[index]) return false;
        ++index;
    }
    return left[index] == right[index];
}

static bool startsWithInsensitive(const char* value, const char* prefix) {
    if (!value || !prefix) return false;
    uint32_t index = 0;
    while (prefix[index] != '\0') {
        if (value[index] == '\0') return false;
        char a = value[index];
        char b = prefix[index];
        if (a >= 'A' && a <= 'Z') a = static_cast<char>(a + ('a' - 'A'));
        if (b >= 'A' && b <= 'Z') b = static_cast<char>(b + ('a' - 'A'));
        if (a != b) return false;
        ++index;
    }
    return true;
}

static bool copyBounded(char* destination, uint32_t capacity, const char* value, bool normalizeControls, bool* truncated) {
    if (!destination || capacity == 0) return false;
    uint32_t inputLength = lengthOf(value, kMaxOutputTextBytes * 4u);
    bool wasTruncated = inputLength >= capacity;
    uint32_t count = wasTruncated ? capacity - 1 : inputLength;
    for (uint32_t i = 0; i < count; ++i) {
        char valueChar = value ? value[i] : '\0';
        if (normalizeControls && static_cast<unsigned char>(valueChar) < 0x20u && valueChar != '\t') valueChar = '?';
        destination[i] = valueChar;
    }
    destination[count] = '\0';
    if (truncated && wasTruncated) *truncated = true;
    return !wasTruncated;
}

static int operationIndex(const OutputService* service, uint64_t operationId) {
    if (!service || operationId == 0) return -1;
    for (uint32_t i = 0; i < service->operationCount; ++i) {
        if (service->operations[i].used && service->operations[i].operationId == operationId) return static_cast<int>(i);
    }
    return -1;
}

static bool isProblem(const OutputRecord& record) {
    return (record.severity == OutputSeverity::Warning || record.severity == OutputSeverity::Error || record.severity == OutputSeverity::Fatal) &&
        (record.category == OutputCategory::BuildDiagnostic || record.category == OutputCategory::RuntimeDiagnostic);
}

static bool channelIncludes(OutputChannel channel, OutputSource source) {
    if (channel == OutputChannel::All) return true;
    if (channel == OutputChannel::Build) {
        return source == OutputSource::Build || source == OutputSource::Compiler || source == OutputSource::Linker || source == OutputSource::ArtifactValidator;
    }
    if (channel == OutputChannel::Run) {
        return source == OutputSource::Run || source == OutputSource::AppModel || source == OutputSource::Runtime || source == OutputSource::Application;
    }
    if (channel == OutputChannel::Application) return source == OutputSource::Application;
    return source == OutputSource::DeveloperStudio || source == OutputSource::System;
}

static void recalculateOperationCounts(OutputService* service) {
    if (!service) return;
    for (uint32_t i = 0; i < service->operationCount; ++i) service->operations[i].retainedRecordCount = 0;
    for (uint32_t i = 0; i < service->recordCount; ++i) {
        const int index = operationIndex(service, service->records[i].operationId);
        if (index >= 0) ++service->operations[index].retainedRecordCount;
    }
}

static void eraseRecordAt(OutputService* service, uint32_t index) {
    if (!service || index >= service->recordCount) return;
    for (uint32_t i = index + 1; i < service->recordCount; ++i) service->records[i - 1] = service->records[i];
    --service->recordCount;
    recalculateOperationCounts(service);
}

static void eraseOperationAt(OutputService* service, uint32_t index) {
    if (!service || index >= service->operationCount) return;
    const uint64_t operationId = service->operations[index].operationId;
    for (uint32_t i = 0; i < service->recordCount;) {
        if (service->records[i].operationId == operationId) eraseRecordAt(service, i);
        else ++i;
    }
    for (uint32_t i = index + 1; i < service->operationCount; ++i) service->operations[i - 1] = service->operations[i];
    --service->operationCount;
    recalculateOperationCounts(service);
}

static bool removeOldestRecordForOperation(OutputService* service, uint64_t operationId) {
    if (!service) return false;
    for (uint32_t i = 0; i < service->recordCount; ++i) {
        if (service->records[i].operationId == operationId) {
            eraseRecordAt(service, i);
            return true;
        }
    }
    return false;
}

static bool reserveRecord(OutputService* service) {
    if (!service) return false;
    if (service->recordCount < kMaxOutputRecords) return true;
    for (uint32_t i = 0; i < service->recordCount; ++i) {
        const int index = operationIndex(service, service->records[i].operationId);
        if (index >= 0 && service->operations[index].completed) {
            eraseRecordAt(service, i);
            return true;
        }
    }
    if (service->recordCount > 0) {
        eraseRecordAt(service, 0);
        return true;
    }
    return false;
}

static void setError(OutputErrorCode* error, OutputErrorCode value) {
    if (error) *error = value;
}

static bool appendRecordUnbounded(OutputService* service, int operation, const OutputRecord& input, OutputErrorCode* error) {
    if (!service || operation < 0 || static_cast<uint32_t>(operation) >= service->operationCount) { setError(error, OutputErrorCode::OperationNotFound); return false; }
    if (!reserveRecord(service)) { setError(error, OutputErrorCode::RecordLimit); return false; }
    OutputRecord record = {};
    record.sequence = service->nextSequence++;
    record.operationId = service->operations[operation].operationId;
    record.source = input.source;
    record.severity = input.severity;
    record.category = input.category;
    record.stream = input.stream;
    bool truncated = input.isTruncated;
    copyBounded(record.text, sizeof(record.text), input.text, true, &truncated);
    copyBounded(record.projectId, sizeof(record.projectId), input.projectId[0] ? input.projectId : service->operations[operation].projectId, false, &truncated);
    copyBounded(record.relativeFilePath, sizeof(record.relativeFilePath), input.relativeFilePath, false, &truncated);
    copyBounded(record.diagnosticCode, sizeof(record.diagnosticCode), input.diagnosticCode, false, &truncated);
    record.line = input.line;
    record.column = input.column;
    record.endLine = input.endLine;
    record.endColumn = input.endColumn;
    record.hasLocation = input.hasLocation;
    record.isTruncated = truncated;
    record.isTerminal = input.isTerminal;
    service->records[service->recordCount++] = record;
    ++service->operations[operation].retainedRecordCount;
    setError(error, truncated ? OutputErrorCode::TextTruncated : OutputErrorCode::None);
    return true;
}

static bool appendTruncationNotice(OutputService* service, int operation) {
    if (!service || operation < 0 || service->operations[operation].truncationPublished) return true;
    const uint64_t id = service->operations[operation].operationId;
    if (service->operations[operation].retainedRecordCount >= kMaxOutputRecordsPerOperation) removeOldestRecordForOperation(service, id);
    OutputRecord record = {};
    record.source = OutputSource::DeveloperStudio;
    record.severity = OutputSeverity::Warning;
    record.category = OutputCategory::Internal;
    record.stream = OutputStream::Unknown;
    copyBounded(record.text, sizeof(record.text), "Output truncated: record limit reached.", true, nullptr);
    record.isTruncated = true;
    if (!appendRecordUnbounded(service, operation, record, nullptr)) return false;
    service->operations[operation].truncationPublished = true;
    return true;
}

static bool appendNormal(OutputService* service, int operation, const OutputRecord& input, OutputErrorCode* error) {
    if (!service || operation < 0) { setError(error, OutputErrorCode::OperationNotFound); return false; }
    if (service->operations[operation].completed || service->operations[operation].terminalPublished) { setError(error, OutputErrorCode::OperationStale); return false; }
    if (service->operations[operation].retainedRecordCount >= kMaxOutputRecordsPerOperation || service->recordCount >= kMaxOutputRecords) {
        appendTruncationNotice(service, operation);
        setError(error, OutputErrorCode::RecordLimit);
        return false;
    }
    return appendRecordUnbounded(service, operation, input, error);
}

static bool parseUnsigned(const char* value, uint32_t start, uint32_t end, uint32_t* output) {
    if (!value || !output || start >= end) return false;
    uint32_t result = 0;
    for (uint32_t i = start; i < end; ++i) {
        if (value[i] < '0' || value[i] > '9') return false;
        const uint32_t digit = static_cast<uint32_t>(value[i] - '0');
        if (result > (0xFFFFFFFFu - digit) / 10u) return false;
        result = result * 10u + digit;
    }
    *output = result;
    return true;
}

static int findChar(const char* value, char needle, uint32_t start, uint32_t end) {
    for (uint32_t i = start; i < end; ++i) if (value[i] == needle) return static_cast<int>(i);
    return -1;
}

static bool copyRange(char* output, uint32_t capacity, const char* value, uint32_t start, uint32_t end) {
    if (!output || capacity == 0 || !value || start >= end) return false;
    while (start < end && (value[start] == ' ' || value[start] == '\t' || value[start] == '"')) ++start;
    while (end > start && (value[end - 1] == ' ' || value[end - 1] == '\t' || value[end - 1] == '"')) --end;
    if (start >= end) return false;
    const uint32_t length = end - start;
    if (length >= capacity) return false;
    for (uint32_t i = 0; i < length; ++i) output[i] = value[start + i] == '\\' ? '/' : value[start + i];
    output[length] = '\0';
    return true;
}

static bool hasTraversal(const char* path) {
    if (!path || path[0] == '\0') return true;
    uint32_t start = 0;
    const uint32_t length = lengthOf(path, kMaxOutputPathBytes);
    for (uint32_t i = 0; i <= length; ++i) {
        if (i < length && path[i] != '/') continue;
        if (i == start) return true;
        if (i - start == 1 && path[start] == '.') return true;
        if (i - start == 2 && path[start] == '.' && path[start + 1] == '.') return true;
        start = i + 1;
    }
    return false;
}

static bool projectRelativePath(const char* projectRoot, const char* candidate, char* output, uint32_t capacity, OutputErrorCode* error) {
    if (!projectRoot || !candidate || !output || capacity == 0) { setError(error, OutputErrorCode::InvalidDiagnosticPath); return false; }
    char root[kMaxOutputPathBytes] = {};
    char path[kMaxOutputPathBytes] = {};
    bool truncated = false;
    copyBounded(root, sizeof(root), projectRoot, false, &truncated);
    copyBounded(path, sizeof(path), candidate, false, &truncated);
    for (uint32_t i = 0; root[i] != '\0'; ++i) if (root[i] == '\\') root[i] = '/';
    for (uint32_t i = 0; path[i] != '\0'; ++i) if (path[i] == '\\') path[i] = '/';
    if (root[0] == '\0' || path[0] == '\0' || hasTraversal(path)) { setError(error, OutputErrorCode::InvalidDiagnosticPath); return false; }
    bool absolute = path[0] == '/' || (path[1] == ':');
    if (!absolute) {
        copyBounded(output, capacity, path, false, nullptr);
        if (hasTraversal(output)) { setError(error, OutputErrorCode::InvalidDiagnosticPath); return false; }
        return true;
    }
    const uint32_t rootLength = lengthOf(root, sizeof(root));
    const uint32_t pathLength = lengthOf(path, sizeof(path));
    if (rootLength > pathLength) { setError(error, OutputErrorCode::DiagnosticPathOutsideProject); return false; }
    for (uint32_t i = 0; i < rootLength; ++i) {
        char a = root[i];
        char b = path[i];
        if (a >= 'A' && a <= 'Z') a = static_cast<char>(a + ('a' - 'A'));
        if (b >= 'A' && b <= 'Z') b = static_cast<char>(b + ('a' - 'A'));
        if (a != b) { setError(error, OutputErrorCode::DiagnosticPathOutsideProject); return false; }
    }
    if (pathLength != rootLength && path[rootLength] != '/') { setError(error, OutputErrorCode::DiagnosticPathOutsideProject); return false; }
    const uint32_t relativeStart = pathLength == rootLength ? rootLength : rootLength + 1;
    if (relativeStart >= pathLength || pathLength - relativeStart >= capacity) { setError(error, OutputErrorCode::InvalidDiagnosticPath); return false; }
    for (uint32_t i = relativeStart; i < pathLength; ++i) output[i - relativeStart] = path[i];
    output[pathLength - relativeStart] = '\0';
    return true;
}

static bool severityAt(const char* text, uint32_t start, uint32_t end, OutputSeverity* severity, uint32_t* after) {
    while (start < end && (text[start] == ' ' || text[start] == '\t')) ++start;
    if (start + 11 <= end && startsWithInsensitive(text + start, "fatal error")) {
        *severity = OutputSeverity::Fatal;
        *after = start + 11;
        return true;
    }
    if (start + 5 <= end && startsWithInsensitive(text + start, "error")) {
        *severity = OutputSeverity::Error;
        *after = start + 5;
        return true;
    }
    if (start + 7 <= end && startsWithInsensitive(text + start, "warning")) {
        *severity = OutputSeverity::Warning;
        *after = start + 7;
        return true;
    }
    return false;
}

static bool extractCodeAndMessage(const char* text, uint32_t start, uint32_t end, char* code, uint32_t codeCapacity, char* message, uint32_t messageCapacity) {
    while (start < end && (text[start] == ' ' || text[start] == '\t')) ++start;
    uint32_t tokenEnd = start;
    while (tokenEnd < end && text[tokenEnd] != ':' && text[tokenEnd] != ' ' && text[tokenEnd] != '\t') ++tokenEnd;
    uint32_t messageStart = start;
    if (tokenEnd > start && tokenEnd - start < codeCapacity) {
        bool plausible = false;
        for (uint32_t i = start; i < tokenEnd; ++i) {
            if ((text[i] >= '0' && text[i] <= '9') || text[i] == '-' || (text[i] >= 'A' && text[i] <= 'Z')) plausible = true;
        }
        if (plausible) {
            for (uint32_t i = start; i < tokenEnd; ++i) code[i - start] = text[i];
            code[tokenEnd - start] = '\0';
            messageStart = tokenEnd;
            while (messageStart < end && (text[messageStart] == ':' || text[messageStart] == ' ' || text[messageStart] == '\t')) ++messageStart;
        }
    }
    return copyRange(message, messageCapacity, text, messageStart, end);
}

static bool parseLinkerDiagnostic(const char* text, uint32_t length, OutputRecord* record) {
    uint32_t cursor = 0;
    while (cursor < length && (text[cursor] == ' ' || text[cursor] == '\t')) ++cursor;
    if (!startsWithInsensitive(text + cursor, "link") && !startsWithInsensitive(text + cursor, "ld.lld")) return false;
    int colon = findChar(text, ':', cursor, length);
    if (colon < 0) return false;
    int secondColon = findChar(text, ':', static_cast<uint32_t>(colon + 1), length);
    uint32_t severityStart = static_cast<uint32_t>(colon + 1);
    OutputSeverity severity = OutputSeverity::Information;
    uint32_t after = 0;
    if (!severityAt(text, severityStart, secondColon < 0 ? length : static_cast<uint32_t>(secondColon), &severity, &after)) return false;
    record->source = OutputSource::Linker;
    record->severity = severity;
    record->category = OutputCategory::BuildDiagnostic;
    record->stream = OutputStream::StandardError;
    int messageColon = findChar(text, ':', after, length);
    const uint32_t messageEnd = messageColon >= 0 ? length : length;
    extractCodeAndMessage(text, after, messageColon >= 0 ? static_cast<uint32_t>(messageColon) : length,
                          record->diagnosticCode, sizeof(record->diagnosticCode), record->text, sizeof(record->text));
    if (messageColon >= 0) copyRange(record->text, sizeof(record->text), text, static_cast<uint32_t>(messageColon + 1), messageEnd);
    record->hasLocation = false;
    return true;
}

} // namespace

const char* OutputSourceName(OutputSource source) {
    switch (source) {
    case OutputSource::DeveloperStudio: return "Developer Studio";
    case OutputSource::Build: return "Build";
    case OutputSource::Compiler: return "Compiler";
    case OutputSource::Linker: return "Linker";
    case OutputSource::ArtifactValidator: return "Artifact Validator";
    case OutputSource::Run: return "Run";
    case OutputSource::Runtime: return "Runtime";
    case OutputSource::Application: return "Application";
    case OutputSource::AppModel: return "App Model";
    case OutputSource::System: return "System";
    }
    return "Unknown";
}

const char* OutputSeverityName(OutputSeverity severity) {
    switch (severity) {
    case OutputSeverity::Trace: return "Trace";
    case OutputSeverity::Information: return "Information";
    case OutputSeverity::Success: return "Success";
    case OutputSeverity::Warning: return "Warning";
    case OutputSeverity::Error: return "Error";
    case OutputSeverity::Fatal: return "Fatal";
    }
    return "Unknown";
}

const char* OutputCategoryName(OutputCategory category) {
    switch (category) {
    case OutputCategory::General: return "General";
    case OutputCategory::BuildLifecycle: return "Build Lifecycle";
    case OutputCategory::BuildDiagnostic: return "Build Diagnostic";
    case OutputCategory::Artifact: return "Artifact";
    case OutputCategory::RunLifecycle: return "Run Lifecycle";
    case OutputCategory::RuntimeDiagnostic: return "Runtime Diagnostic";
    case OutputCategory::ApplicationOutput: return "Application Output";
    case OutputCategory::Internal: return "Internal";
    }
    return "Unknown";
}

const char* OutputChannelName(OutputChannel channel) {
    switch (channel) {
    case OutputChannel::All: return "All";
    case OutputChannel::Build: return "Build";
    case OutputChannel::Run: return "Run";
    case OutputChannel::Application: return "Application";
    case OutputChannel::DeveloperStudio: return "Developer Studio";
    }
    return "Unknown";
}

const char* OutputOperationTypeName(OutputOperationType type) {
    switch (type) {
    case OutputOperationType::Build: return "BUILD";
    case OutputOperationType::Run: return "RUN";
    case OutputOperationType::Internal: return "INTERNAL";
    }
    return "UNKNOWN";
}

const char* OutputErrorName(OutputErrorCode error) {
    switch (error) {
    case OutputErrorCode::None: return "none";
    case OutputErrorCode::OperationNotFound: return "OUTPUT_OPERATION_NOT_FOUND";
    case OutputErrorCode::OperationStale: return "OUTPUT_OPERATION_STALE";
    case OutputErrorCode::RecordLimit: return "OUTPUT_RECORD_LIMIT";
    case OutputErrorCode::TextTruncated: return "OUTPUT_TEXT_TRUNCATED";
    case OutputErrorCode::InvalidUtf8: return "OUTPUT_INVALID_UTF8";
    case OutputErrorCode::InvalidDiagnosticPath: return "DIAGNOSTIC_INVALID_PATH";
    case OutputErrorCode::DiagnosticPathOutsideProject: return "DIAGNOSTIC_PATH_OUTSIDE_PROJECT";
    case OutputErrorCode::InvalidDiagnosticLine: return "DIAGNOSTIC_INVALID_LINE";
    case OutputErrorCode::InvalidDiagnosticColumn: return "DIAGNOSTIC_INVALID_COLUMN";
    case OutputErrorCode::DiagnosticFileNotFound: return "DIAGNOSTIC_FILE_NOT_FOUND";
    case OutputErrorCode::DiagnosticProjectMismatch: return "DIAGNOSTIC_PROJECT_MISMATCH";
    case OutputErrorCode::NavigationNoProject: return "NAVIGATION_NO_PROJECT";
    case OutputErrorCode::NavigationOpenFailed: return "NAVIGATION_OPEN_FAILED";
    case OutputErrorCode::NavigationLocationClamped: return "NAVIGATION_LOCATION_CLAMPED";
    }
    return "OUTPUT_UNKNOWN";
}

void OutputServiceInit(OutputService* service) {
    if (!service) return;
    clearBytes(service, sizeof(*service));
    service->nextSequence = 1;
    service->nextOperationId = 1;
    service->activeChannel = OutputChannel::All;
}

uint64_t OutputServiceBeginOperation(OutputService* service, OutputOperationType type, const char* projectId) {
    if (!service) return 0;
    if (service->operationCount >= kMaxOutputOperations) {
        int evict = -1;
        for (uint32_t i = 0; i < service->operationCount; ++i) if (service->operations[i].completed) { evict = static_cast<int>(i); break; }
        if (evict < 0) return 0;
        eraseOperationAt(service, static_cast<uint32_t>(evict));
    }
    OutputOperation operation = {};
    operation.used = true;
    operation.type = type;
    operation.operationId = service->nextOperationId++;
    if (operation.operationId == 0) operation.operationId = service->nextOperationId++;
    copyBounded(operation.projectId, sizeof(operation.projectId), projectId, false, nullptr);
    service->operations[service->operationCount++] = operation;
    return operation.operationId;
}

bool OutputServiceAppendRecord(OutputService* service, uint64_t operationId, const OutputRecord& record, OutputErrorCode* error) {
    setError(error, OutputErrorCode::None);
    const int operation = operationIndex(service, operationId);
    if (operation < 0) { setError(error, OutputErrorCode::OperationNotFound); return false; }
    OutputRecord normalized = record;
    if (normalized.projectId[0] == '\0') copyBounded(normalized.projectId, sizeof(normalized.projectId), service->operations[operation].projectId, false, nullptr);
    return appendNormal(service, operation, normalized, error);
}

bool OutputServiceAppendText(OutputService* service, uint64_t operationId, OutputSource source, OutputSeverity severity,
                             OutputCategory category, OutputStream stream, const char* text,
                             const char* projectId, OutputErrorCode* error) {
    OutputRecord record = {};
    record.source = source;
    record.severity = severity;
    record.category = category;
    record.stream = stream;
    copyBounded(record.text, sizeof(record.text), text, true, &record.isTruncated);
    copyBounded(record.projectId, sizeof(record.projectId), projectId, false, &record.isTruncated);
    return OutputServiceAppendRecord(service, operationId, record, error);
}

bool ParseBuildDiagnostic(const char* projectRoot, const char* projectId, const char* text, OutputStream stream,
                          OutputRecord* record, OutputErrorCode* error) {
    setError(error, OutputErrorCode::None);
    if (!text || !record) return false;
    *record = OutputRecord();
    record->stream = stream;
    record->category = OutputCategory::BuildDiagnostic;
    const uint32_t length = lengthOf(text, kMaxOutputTextBytes * 4u);
    if (parseLinkerDiagnostic(text, length, record)) {
        copyBounded(record->projectId, sizeof(record->projectId), projectId, false, nullptr);
        return true;
    }

    for (uint32_t cursor = 0; cursor < length; ++cursor) {
        if (text[cursor] != '(') continue;
        int comma = findChar(text, ',', cursor + 1, length);
        int close = findChar(text, ')', cursor + 1, length);
        if (comma < 0 || close < 0 || comma > close) continue;
        int colon = findChar(text, ':', static_cast<uint32_t>(close + 1), length);
        if (colon < 0) continue;
        OutputSeverity severity;
        uint32_t after = 0;
        if (!severityAt(text, static_cast<uint32_t>(colon + 1), length, &severity, &after)) continue;
        uint32_t line = 0;
        uint32_t column = 0;
        if (!parseUnsigned(text, cursor + 1, static_cast<uint32_t>(comma), &line)) { setError(error, OutputErrorCode::InvalidDiagnosticLine); return true; }
        if (!parseUnsigned(text, static_cast<uint32_t>(comma + 1), static_cast<uint32_t>(close), &column)) { setError(error, OutputErrorCode::InvalidDiagnosticColumn); return true; }
        record->source = OutputSource::Compiler;
        record->severity = severity;
        record->line = line;
        record->column = column;
        char candidate[kMaxOutputPathBytes] = {};
        if (!copyRange(candidate, sizeof(candidate), text, 0, cursor)) { setError(error, OutputErrorCode::InvalidDiagnosticPath); return true; }
        record->hasLocation = projectRelativePath(projectRoot, candidate, record->relativeFilePath, sizeof(record->relativeFilePath), error);
        if (!record->hasLocation && error && *error == OutputErrorCode::None) *error = OutputErrorCode::DiagnosticPathOutsideProject;
        uint32_t messageStart = after;
        while (messageStart < length && (text[messageStart] == ' ' || text[messageStart] == '\t')) ++messageStart;
        const int messageColon = findChar(text, ':', messageStart, length);
        extractCodeAndMessage(text, messageStart, messageColon >= 0 ? static_cast<uint32_t>(messageColon) : length,
                              record->diagnosticCode, sizeof(record->diagnosticCode), record->text, sizeof(record->text));
        if (messageColon >= 0) copyRange(record->text, sizeof(record->text), text, static_cast<uint32_t>(messageColon + 1), length);
        copyBounded(record->projectId, sizeof(record->projectId), projectId, false, &record->isTruncated);
        return true;
    }

    for (uint32_t cursor = 0; cursor < length; ++cursor) {
        if (text[cursor] != ':') continue;
        uint32_t line = 0;
        uint32_t lineStart = cursor + 1;
        uint32_t lineEnd = lineStart;
        while (lineEnd < length && text[lineEnd] >= '0' && text[lineEnd] <= '9') ++lineEnd;
        if (lineEnd == lineStart) continue;
        if (!parseUnsigned(text, lineStart, lineEnd, &line)) {
            uint32_t possibleSeverityColon = lineEnd;
            if (possibleSeverityColon < length && text[possibleSeverityColon] == ':') {
                uint32_t possibleColumnStart = possibleSeverityColon + 1;
                uint32_t possibleColumnEnd = possibleColumnStart;
                while (possibleColumnEnd < length && text[possibleColumnEnd] >= '0' && text[possibleColumnEnd] <= '9') ++possibleColumnEnd;
                if (possibleColumnEnd > possibleColumnStart) possibleSeverityColon = possibleColumnEnd;
                OutputSeverity ignoredSeverity = OutputSeverity::Information;
                uint32_t ignoredAfter = 0;
                if (possibleSeverityColon < length && text[possibleSeverityColon] == ':' &&
                    severityAt(text, possibleSeverityColon + 1, length, &ignoredSeverity, &ignoredAfter)) {
                    setError(error, OutputErrorCode::InvalidDiagnosticLine);
                    return true;
                }
            }
            continue;
        }
        uint32_t column = 0;
        uint32_t severityColon = lineEnd;
        if (severityColon < length && text[severityColon] == ':') {
            uint32_t columnStart = severityColon + 1;
            uint32_t columnEnd = columnStart;
            while (columnEnd < length && text[columnEnd] >= '0' && text[columnEnd] <= '9') ++columnEnd;
            if (columnEnd > columnStart) {
                if (!parseUnsigned(text, columnStart, columnEnd, &column)) { setError(error, OutputErrorCode::InvalidDiagnosticColumn); return true; }
                severityColon = columnEnd;
            }
        }
        if (severityColon >= length || text[severityColon] != ':') continue;
        OutputSeverity severity;
        uint32_t after = 0;
        if (!severityAt(text, severityColon + 1, length, &severity, &after)) continue;
        record->source = OutputSource::Compiler;
        record->severity = severity;
        record->line = line;
        record->column = column;
        char candidate[kMaxOutputPathBytes] = {};
        if (!copyRange(candidate, sizeof(candidate), text, 0, cursor)) { setError(error, OutputErrorCode::InvalidDiagnosticPath); return true; }
        record->hasLocation = projectRelativePath(projectRoot, candidate, record->relativeFilePath, sizeof(record->relativeFilePath), error);
        uint32_t messageStart = after;
        while (messageStart < length && (text[messageStart] == ' ' || text[messageStart] == '\t')) ++messageStart;
        const int messageColon = findChar(text, ':', messageStart, length);
        extractCodeAndMessage(text, messageStart, messageColon >= 0 ? static_cast<uint32_t>(messageColon) : length,
                              record->diagnosticCode, sizeof(record->diagnosticCode), record->text, sizeof(record->text));
        if (messageColon >= 0) copyRange(record->text, sizeof(record->text), text, static_cast<uint32_t>(messageColon + 1), length);
        copyBounded(record->projectId, sizeof(record->projectId), projectId, false, &record->isTruncated);
        return true;
    }
    return false;
}

bool OutputServiceAppendBuildLine(OutputService* service, uint64_t operationId, const char* projectRoot,
                                  const char* projectId, OutputStream stream, const char* text, OutputErrorCode* error) {
    OutputRecord diagnostic = {};
    OutputErrorCode parseError = OutputErrorCode::None;
    if (ParseBuildDiagnostic(projectRoot, projectId, text, stream, &diagnostic, &parseError)) {
        const bool result = OutputServiceAppendRecord(service, operationId, diagnostic, error);
        if (error && !result && *error == OutputErrorCode::None) *error = parseError;
        return result;
    }
    return OutputServiceAppendText(service, operationId, OutputSource::Build, OutputSeverity::Information,
                                   OutputCategory::General, stream, text, projectId, error);
}

bool OutputServiceCompleteOperation(OutputService* service, uint64_t operationId, bool succeeded,
                                    const char* text, OutputErrorCode* error) {
    setError(error, OutputErrorCode::None);
    const int operation = operationIndex(service, operationId);
    if (operation < 0) { setError(error, OutputErrorCode::OperationNotFound); return false; }
    if (service->operations[operation].completed || service->operations[operation].terminalPublished) { setError(error, OutputErrorCode::OperationStale); return false; }
    if (service->operations[operation].retainedRecordCount >= kMaxOutputRecordsPerOperation) removeOldestRecordForOperation(service, operationId);
    OutputRecord record = {};
    record.source = service->operations[operation].type == OutputOperationType::Build ? OutputSource::Build : OutputSource::Run;
    record.severity = succeeded ? OutputSeverity::Success : OutputSeverity::Error;
    record.category = service->operations[operation].type == OutputOperationType::Build ? OutputCategory::BuildLifecycle : OutputCategory::RunLifecycle;
    record.isTerminal = true;
    copyBounded(record.text, sizeof(record.text), text, true, &record.isTruncated);
    if (!appendRecordUnbounded(service, operation, record, error)) return false;
    service->operations[operation].terminalPublished = true;
    service->operations[operation].completed = true;
    return true;
}

bool OutputServiceClearOperation(OutputService* service, uint64_t operationId) {
    const int operation = operationIndex(service, operationId);
    if (operation < 0) return false;
    eraseOperationAt(service, static_cast<uint32_t>(operation));
    return true;
}

uint32_t OutputServiceClearCategory(OutputService* service, OutputCategory category) {
    if (!service) return 0;
    uint32_t removed = 0;
    for (uint32_t i = 0; i < service->recordCount;) {
        if (service->records[i].category == category) { eraseRecordAt(service, i); ++removed; }
        else ++i;
    }
    return removed;
}

uint32_t OutputServiceClearProblemsForProject(OutputService* service, const char* projectId) {
    if (!service || !projectId) return 0;
    uint32_t removed = 0;
    for (uint32_t i = 0; i < service->recordCount;) {
        const OutputRecord& record = service->records[i];
        if (record.category == OutputCategory::BuildDiagnostic && equalText(record.projectId, projectId)) { eraseRecordAt(service, i); ++removed; }
        else ++i;
    }
    return removed;
}

uint32_t OutputServiceClearChannel(OutputService* service, OutputChannel channel) {
    if (!service) return 0;
    uint32_t removed = 0;
    for (uint32_t i = 0; i < service->recordCount;) {
        if (channelIncludes(channel, service->records[i].source)) { eraseRecordAt(service, i); ++removed; }
        else ++i;
    }
    return removed;
}

void OutputServiceSelectActiveChannel(OutputService* service, OutputChannel channel) {
    if (service) service->activeChannel = channel;
}

OutputChannel OutputServiceActiveChannel(const OutputService* service) {
    return service ? service->activeChannel : OutputChannel::All;
}

uint32_t OutputServiceRecordCount(const OutputService* service) { return service ? service->recordCount : 0; }
const OutputRecord* OutputServiceRecordAt(const OutputService* service, uint32_t index) {
    return service && index < service->recordCount ? &service->records[index] : nullptr;
}

uint32_t OutputServiceFilteredCount(const OutputService* service, OutputChannel channel) {
    if (!service) return 0;
    uint32_t count = 0;
    for (uint32_t i = 0; i < service->recordCount; ++i) if (channelIncludes(channel, service->records[i].source)) ++count;
    return count;
}

const OutputRecord* OutputServiceFilteredAt(const OutputService* service, OutputChannel channel, uint32_t index) {
    if (!service) return nullptr;
    uint32_t current = 0;
    for (uint32_t i = 0; i < service->recordCount; ++i) {
        if (!channelIncludes(channel, service->records[i].source)) continue;
        if (current++ == index) return &service->records[i];
    }
    return nullptr;
}

uint32_t OutputServiceProblemCount(const OutputService* service, const char* projectId) {
    if (!service) return 0;
    uint32_t count = 0;
    for (uint32_t i = 0; i < service->recordCount; ++i) {
        if (!isProblem(service->records[i])) continue;
        if (projectId && projectId[0] != '\0' && !equalText(service->records[i].projectId, projectId)) continue;
        ++count;
    }
    return count;
}

const OutputRecord* OutputServiceProblemAt(const OutputService* service, const char* projectId, uint32_t index) {
    if (!service) return nullptr;
    uint32_t current = 0;
    for (uint32_t i = 0; i < service->recordCount; ++i) {
        if (!isProblem(service->records[i])) continue;
        if (projectId && projectId[0] != '\0' && !equalText(service->records[i].projectId, projectId)) continue;
        if (current++ == index) return &service->records[i];
    }
    return nullptr;
}

void OutputServiceProblemCounts(const OutputService* service, const char* projectId, uint32_t* warnings, uint32_t* errors) {
    if (warnings) *warnings = 0;
    if (errors) *errors = 0;
    if (!service) return;
    for (uint32_t i = 0; i < service->recordCount; ++i) {
        if (!isProblem(service->records[i])) continue;
        if (projectId && projectId[0] != '\0' && !equalText(service->records[i].projectId, projectId)) continue;
        if (service->records[i].severity == OutputSeverity::Warning) { if (warnings) ++*warnings; }
        else { if (errors) ++*errors; }
    }
}

} // namespace developer_studio
} // namespace guidexos
