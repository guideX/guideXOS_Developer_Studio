#include "developer_studio_debugger_workspace.h"

namespace guidexos {
namespace developer_studio {
namespace {

static uint32_t workspaceStringLength(const char* input) {
    if (!input) return 0;
    uint32_t length = 0;
    while (input[length] != '\0') ++length;
    return length;
}

static int workspaceStringCompare(const char* left, const char* right) {
    if (!left || !right) return left == right ? 0 : (left ? 1 : -1);
    while (*left && *left == *right) { ++left; ++right; }
    return static_cast<unsigned char>(*left) - static_cast<unsigned char>(*right);
}

static int workspaceStringNCompare(const char* left, const char* right, uint32_t count) {
    if (!left || !right) return left == right ? 0 : (left ? 1 : -1);
    for (uint32_t i = 0; i < count; ++i) {
        if (left[i] != right[i] || left[i] == '\0')
            return static_cast<unsigned char>(left[i]) - static_cast<unsigned char>(right[i]);
    }
    return 0;
}

static char* workspaceStringCopy(char* output, const char* input) {
    if (!output || !input) return output;
    uint32_t index = 0;
    do { output[index] = input[index]; } while (input[index++] != '\0');
    return output;
}

static void* workspaceMemoryCopy(void* output, const void* input, uint32_t bytes) {
    unsigned char* destination = static_cast<unsigned char*>(output);
    const unsigned char* source = static_cast<const unsigned char*>(input);
    for (uint32_t i = 0; i < bytes; ++i) destination[i] = source[i];
    return output;
}

static void* workspaceMemorySet(void* output, int value, uint32_t bytes) {
    unsigned char* destination = static_cast<unsigned char*>(output);
    for (uint32_t i = 0; i < bytes; ++i) destination[i] = static_cast<unsigned char>(value);
    return output;
}

#define __builtin_strlen workspaceStringLength
#define __builtin_strcmp workspaceStringCompare
#define __builtin_strncmp workspaceStringNCompare
#define __builtin_strcpy workspaceStringCopy
#define __builtin_memcpy workspaceMemoryCopy
#define __builtin_memset workspaceMemorySet

static void setError(DebuggerWorkspaceErrorCode* error, DebuggerWorkspaceErrorCode value) {
    if (error) *error = value;
}

static const char* errorName(DebuggerWorkspaceErrorCode error) {
    switch (error) {
    case DebuggerWorkspaceErrorCode::None: return "None";
    case DebuggerWorkspaceErrorCode::NullInput: return "NullInput";
    case DebuggerWorkspaceErrorCode::MalformedJson: return "MalformedJson";
    case DebuggerWorkspaceErrorCode::UnsupportedVersion: return "UnsupportedVersion";
    case DebuggerWorkspaceErrorCode::MissingField: return "MissingField";
    case DebuggerWorkspaceErrorCode::UnknownField: return "UnknownField";
    case DebuggerWorkspaceErrorCode::DuplicateField: return "DuplicateField";
    case DebuggerWorkspaceErrorCode::InvalidPath: return "InvalidPath";
    case DebuggerWorkspaceErrorCode::InvalidLine: return "InvalidLine";
    case DebuggerWorkspaceErrorCode::InvalidColumn: return "InvalidColumn";
    case DebuggerWorkspaceErrorCode::InvalidEnum: return "InvalidEnum";
    case DebuggerWorkspaceErrorCode::InvalidThreshold: return "InvalidThreshold";
    case DebuggerWorkspaceErrorCode::OverCapacity: return "OverCapacity";
    case DebuggerWorkspaceErrorCode::DuplicateBreakpoint: return "DuplicateBreakpoint";
    case DebuggerWorkspaceErrorCode::StringTooLong: return "StringTooLong";
    case DebuggerWorkspaceErrorCode::OutputTooSmall: return "OutputTooSmall";
    case DebuggerWorkspaceErrorCode::FileError: return "FileError";
    }
    return "MalformedJson";
}

static bool copyBounded(char* output, uint32_t outputSize, const char* input) {
    if (!output || !input) return false;
    const uint32_t length = static_cast<uint32_t>(__builtin_strlen(input));
    if (length >= outputSize) return false;
    __builtin_memcpy(output, input, length + 1);
    return true;
}

static void setWorkspaceError(DebuggerWorkspace* workspace, DebuggerWorkspaceErrorCode error) {
    if (!workspace) return;
    workspace->lastError = error;
    copyBounded(workspace->lastErrorMessage, sizeof(workspace->lastErrorMessage), errorName(error));
}

static bool boundedLength(const char* input, uint32_t capacity, uint32_t* length) {
    if (!input || !length) return false;
    for (uint32_t index = 0; index < capacity; ++index) {
        if (input[index] == '\0') { *length = index; return true; }
    }
    return false;
}

static bool validSourcePath(const char* input, char* normalized) {
    uint32_t inputLength = 0;
    if (!boundedLength(input, kMaxProjectPathBytes, &inputLength) || inputLength == 0 ||
        input[0] == '/' || input[0] == '\\' ||
        (input[1] == ':') || PathContainsTraversal(input) ||
        !NormalizePath(input, normalized, kMaxProjectPathBytes) ||
        !normalized[0] || normalized[0] == '/' || normalized[0] == '\\' ||
        normalized[1] == ':' || PathContainsTraversal(normalized)) return false;
    return true;
}

static DebuggerWorkspaceErrorCode breakpointValidationError(
        const char* sourcePath, uint32_t line, uint32_t column, uint32_t action,
        uint32_t hitPolicy, uint32_t hitThreshold, const char* condition,
        const char* logTemplate, char* normalized) {
    uint32_t sourceLength = 0;
    uint32_t conditionLength = 0;
    uint32_t logLength = 0;
    if (!boundedLength(sourcePath, kMaxProjectPathBytes, &sourceLength) ||
        !boundedLength(condition, kDebugWatchMaxExpressionBytes + 1u, &conditionLength) ||
        !boundedLength(logTemplate, GX_DEVELOPMENT_DEBUG_MAX_LOG_TEMPLATE_BYTES + 1u, &logLength))
        return DebuggerWorkspaceErrorCode::StringTooLong;
    if (!validSourcePath(sourcePath, normalized)) return DebuggerWorkspaceErrorCode::InvalidPath;
    if (line == 0) return DebuggerWorkspaceErrorCode::InvalidLine;
    if (column == 0) return DebuggerWorkspaceErrorCode::InvalidColumn;
    if (action > 1 || hitPolicy > 3 || !condition || !logTemplate) return DebuggerWorkspaceErrorCode::InvalidEnum;
    (void)sourceLength;
    (void)conditionLength;
    (void)logLength;
    if ((hitPolicy == 0 && hitThreshold != 0) || (hitPolicy != 0 && hitThreshold == 0)) return DebuggerWorkspaceErrorCode::InvalidThreshold;
    return DebuggerWorkspaceErrorCode::None;
}

static bool validBreakpoint(const char* sourcePath, uint32_t line, uint32_t column,
                            uint32_t action, uint32_t hitPolicy, uint32_t hitThreshold,
                            const char* condition, const char* logTemplate, char* normalized) {
    return breakpointValidationError(sourcePath, line, column, action, hitPolicy,
                                     hitThreshold, condition, logTemplate, normalized) ==
           DebuggerWorkspaceErrorCode::None;
}

static void clearError(DebuggerWorkspace* workspace) {
    workspace->lastError = DebuggerWorkspaceErrorCode::None;
    workspace->lastErrorMessage[0] = '\0';
}

static void setStorageStatus(char* status, uint32_t statusSize, const char* text) {
    if (!status || statusSize == 0) return;
    uint32_t index = 0;
    while (text && text[index] && index + 1 < statusSize) {
        status[index] = text[index];
        ++index;
    }
    status[index] = '\0';
}

static bool validProjectId(const char* projectId) {
    return projectId && projectId[0] != '\0';
}

enum class StorageLoadResult { Loaded, Absent, Invalid, CallbackFailure };

static StorageLoadResult loadStorageFile(const WorkspaceFileSystem& fileSystem, const char* path,
                                         DebuggerWorkspace* output) {
    if (!fileSystem.stat || !fileSystem.read || !path || !output) return StorageLoadResult::CallbackFailure;
    FileInfo info = {};
    if (!fileSystem.stat(fileSystem.userData, path, &info)) return StorageLoadResult::Absent;
    if (info.kind != FileInfoKind::RegularFile || info.size > kDebuggerWorkspaceMaxFileBytes) {
        return StorageLoadResult::Invalid;
    }
    // Workspace persistence runs on the same bounded NativeElf stack as the
    // project loader. Keep the bounded file and parse storage out of that
    // stack frame while preserving the existing transactional output copy.
    static char bytes[kDebuggerWorkspaceMaxFileBytes] = {};
    uint32_t byteCount = 0;
    if (!fileSystem.read(fileSystem.userData, path, bytes, static_cast<uint32_t>(sizeof(bytes)), &byteCount) ||
        byteCount != info.size || byteCount > kDebuggerWorkspaceMaxFileBytes)
        return StorageLoadResult::CallbackFailure;
    static DebuggerWorkspace parsed = {};
    DebuggerWorkspaceInit(&parsed);
    DebuggerWorkspaceErrorCode error = DebuggerWorkspaceErrorCode::None;
    if (!ParseDebuggerWorkspace(bytes, byteCount, &parsed, &error)) return StorageLoadResult::Invalid;
    *output = parsed;
    return StorageLoadResult::Loaded;
}

static void appendChar(char* output, uint32_t outputSize, uint32_t* length, char value,
                       DebuggerWorkspaceErrorCode* error) {
    if (*length + 1 >= outputSize) { setError(error, DebuggerWorkspaceErrorCode::OutputTooSmall); return; }
    output[(*length)++] = value;
}

static void appendText(char* output, uint32_t outputSize, uint32_t* length, const char* text,
                       DebuggerWorkspaceErrorCode* error) {
    while (text && *text && *error != DebuggerWorkspaceErrorCode::OutputTooSmall) {
        appendChar(output, outputSize, length, *text++, error);
    }
}

static void appendUnsigned(char* output, uint32_t outputSize, uint32_t* length, uint32_t value,
                           DebuggerWorkspaceErrorCode* error) {
    char digits[11];
    uint32_t count = 0;
    do { digits[count++] = static_cast<char>('0' + value % 10); value /= 10; } while (value);
    while (count) appendChar(output, outputSize, length, digits[--count], error);
}

static void appendString(char* output, uint32_t outputSize, uint32_t* length, const char* value,
                         DebuggerWorkspaceErrorCode* error) {
    appendChar(output, outputSize, length, '"', error);
    for (const unsigned char* cursor = reinterpret_cast<const unsigned char*>(value); cursor && *cursor; ++cursor) {
        if (*cursor == '"' || *cursor == '\\') { appendChar(output, outputSize, length, '\\', error); appendChar(output, outputSize, length, static_cast<char>(*cursor), error); }
        else if (*cursor == '\n') appendText(output, outputSize, length, "\\n", error);
        else if (*cursor == '\r') appendText(output, outputSize, length, "\\r", error);
        else if (*cursor == '\t') appendText(output, outputSize, length, "\\t", error);
        else if (*cursor < 0x20) { setError(error, DebuggerWorkspaceErrorCode::MalformedJson); return; }
        else appendChar(output, outputSize, length, static_cast<char>(*cursor), error);
    }
    appendChar(output, outputSize, length, '"', error);
}

static const char* actionName(uint32_t action) { return action == 0 ? "BREAK" : "LOG"; }
static const char* policyName(uint32_t policy) {
    static const char* names[] = {"NONE", "EQUAL", "MULTIPLE", "AT_LEAST"};
    return policy < 4 ? names[policy] : "";
}

class Cursor {
public:
    Cursor(const char* bytes, uint32_t length) : bytes_(bytes), length_(length), position_(0), error_(DebuggerWorkspaceErrorCode::None) {}
    bool good() const { return error_ == DebuggerWorkspaceErrorCode::None; }
    DebuggerWorkspaceErrorCode error() const { return error_; }
    void fail(DebuggerWorkspaceErrorCode value) { if (good()) error_ = value; }
    void whitespace() { while (position_ < length_ && (bytes_[position_] == ' ' || bytes_[position_] == '\n' || bytes_[position_] == '\r' || bytes_[position_] == '\t')) ++position_; }
    bool take(char value) { whitespace(); if (position_ >= length_ || bytes_[position_] != value) { fail(DebuggerWorkspaceErrorCode::MalformedJson); return false; } ++position_; return true; }
    bool consume(char value) { whitespace(); if (position_ < length_ && bytes_[position_] == value) { ++position_; return true; } return false; }
    bool string(char* output, uint32_t outputSize) {
        whitespace(); if (position_ >= length_ || bytes_[position_++] != '"') { fail(DebuggerWorkspaceErrorCode::MalformedJson); return false; }
        uint32_t count = 0;
        while (position_ < length_) {
            char value = bytes_[position_++];
            if (value == '"') { if (count >= outputSize) { fail(DebuggerWorkspaceErrorCode::StringTooLong); return false; } output[count] = '\0'; return true; }
            if (static_cast<unsigned char>(value) < 0x20) { fail(DebuggerWorkspaceErrorCode::MalformedJson); return false; }
            if (value == '\\') {
                if (position_ >= length_) { fail(DebuggerWorkspaceErrorCode::MalformedJson); return false; }
                value = bytes_[position_++];
                if (value == '"' || value == '\\' || value == '/') {}
                else if (value == 'b') value = '\b'; else if (value == 'f') value = '\f';
                else if (value == 'n') value = '\n'; else if (value == 'r') value = '\r';
                else if (value == 't') value = '\t'; else { fail(DebuggerWorkspaceErrorCode::MalformedJson); return false; }
            }
            if (count + 1 >= outputSize) { fail(DebuggerWorkspaceErrorCode::StringTooLong); return false; }
            output[count++] = value;
        }
        fail(DebuggerWorkspaceErrorCode::MalformedJson); return false;
    }
    bool number(uint32_t* output) {
        whitespace(); if (position_ >= length_ || bytes_[position_] < '0' || bytes_[position_] > '9') { fail(DebuggerWorkspaceErrorCode::MalformedJson); return false; }
        uint32_t value = 0;
        while (position_ < length_ && bytes_[position_] >= '0' && bytes_[position_] <= '9') {
            const uint32_t digit = static_cast<uint32_t>(bytes_[position_++] - '0');
            if (value > (0xffffffffu - digit) / 10u) { fail(DebuggerWorkspaceErrorCode::InvalidLine); return false; }
            value = value * 10u + digit;
        }
        *output = value; return true;
    }
    bool boolean(bool* output) {
        whitespace();
        if (position_ + 4 <= length_ && __builtin_strncmp(bytes_ + position_, "true", 4) == 0) { position_ += 4; *output = true; return true; }
        if (position_ + 5 <= length_ && __builtin_strncmp(bytes_ + position_, "false", 5) == 0) { position_ += 5; *output = false; return true; }
        fail(DebuggerWorkspaceErrorCode::MalformedJson); return false;
    }
    bool end() { whitespace(); return position_ == length_; }
private:
    const char* bytes_; uint32_t length_; uint32_t position_; DebuggerWorkspaceErrorCode error_;
};

static bool parseBreakpoint(Cursor& cursor, DebuggerWorkspace* workspace) {
    if (workspace->breakpointCount >= kDebuggerWorkspaceMaxBreakpoints) { cursor.fail(DebuggerWorkspaceErrorCode::OverCapacity); return false; }
    if (!cursor.take('{')) return false;
    DebuggerWorkspaceBreakpoint value = {};
    bool source = false, line = false, column = false, enabled = false, action = false, condition = false, policy = false, threshold = false, log = false;
    while (cursor.good()) {
        char key[32] = {};
        if (!cursor.string(key, sizeof(key))) return false;
        if (!cursor.take(':')) return false;
        bool* seen = nullptr;
        if (__builtin_strcmp(key, "sourcePath") == 0) seen = &source;
        else if (__builtin_strcmp(key, "line") == 0) seen = &line;
        else if (__builtin_strcmp(key, "column") == 0) seen = &column;
        else if (__builtin_strcmp(key, "enabled") == 0) seen = &enabled;
        else if (__builtin_strcmp(key, "action") == 0) seen = &action;
        else if (__builtin_strcmp(key, "condition") == 0) seen = &condition;
        else if (__builtin_strcmp(key, "hitPolicy") == 0) seen = &policy;
        else if (__builtin_strcmp(key, "hitThreshold") == 0) seen = &threshold;
        else if (__builtin_strcmp(key, "logTemplate") == 0) seen = &log;
        else { cursor.fail(DebuggerWorkspaceErrorCode::UnknownField); return false; }
        if (*seen) { cursor.fail(DebuggerWorkspaceErrorCode::DuplicateField); return false; }
        *seen = true;
        if (__builtin_strcmp(key, "sourcePath") == 0) { if (!cursor.string(value.sourcePath, sizeof(value.sourcePath))) return false; }
        else if (__builtin_strcmp(key, "line") == 0) { if (!cursor.number(&value.line)) return false; }
        else if (__builtin_strcmp(key, "column") == 0) { if (!cursor.number(&value.column)) return false; }
        else if (__builtin_strcmp(key, "enabled") == 0) { if (!cursor.boolean(&value.enabled)) return false; }
        else if (__builtin_strcmp(key, "action") == 0) { char text[16] = {}; if (!cursor.string(text, sizeof(text))) return false; if (__builtin_strcmp(text, "BREAK") == 0) value.action = 0; else if (__builtin_strcmp(text, "LOG") == 0) value.action = 1; else { cursor.fail(DebuggerWorkspaceErrorCode::InvalidEnum); return false; } }
        else if (__builtin_strcmp(key, "condition") == 0) { if (!cursor.string(value.condition, sizeof(value.condition))) return false; }
        else if (__builtin_strcmp(key, "hitPolicy") == 0) { char text[16] = {}; if (!cursor.string(text, sizeof(text))) return false; if (__builtin_strcmp(text, "NONE") == 0) value.hitPolicy = 0; else if (__builtin_strcmp(text, "EQUAL") == 0) value.hitPolicy = 1; else if (__builtin_strcmp(text, "MULTIPLE") == 0) value.hitPolicy = 2; else if (__builtin_strcmp(text, "AT_LEAST") == 0) value.hitPolicy = 3; else { cursor.fail(DebuggerWorkspaceErrorCode::InvalidEnum); return false; } }
        else if (__builtin_strcmp(key, "hitThreshold") == 0) { if (!cursor.number(&value.hitThreshold)) return false; }
        else if (__builtin_strcmp(key, "logTemplate") == 0) { if (!cursor.string(value.logTemplate, sizeof(value.logTemplate))) return false; }
        cursor.whitespace();
        if (cursor.consume('}')) break;
        if (!cursor.take(',')) return false;
    }
    if (!source || !line || !column || !enabled || !action || !condition || !policy || !threshold || !log) { cursor.fail(DebuggerWorkspaceErrorCode::MissingField); return false; }
    char normalized[kMaxProjectPathBytes] = {};
    if (!validBreakpoint(value.sourcePath, value.line, value.column, value.action, value.hitPolicy, value.hitThreshold, value.condition, value.logTemplate, normalized)) {
        if (value.line == 0) cursor.fail(DebuggerWorkspaceErrorCode::InvalidLine);
        else if (value.column == 0) cursor.fail(DebuggerWorkspaceErrorCode::InvalidColumn);
        else if (value.action > 1 || value.hitPolicy > 3) cursor.fail(DebuggerWorkspaceErrorCode::InvalidEnum);
        else if ((value.hitPolicy == 0 && value.hitThreshold != 0) || (value.hitPolicy != 0 && value.hitThreshold == 0)) cursor.fail(DebuggerWorkspaceErrorCode::InvalidThreshold);
        else cursor.fail(DebuggerWorkspaceErrorCode::InvalidPath);
        return false;
    }
    __builtin_strcpy(value.sourcePath, normalized);
    for (uint32_t index = 0; index < workspace->breakpointCount; ++index) if (__builtin_strcmp(workspace->breakpoints[index].sourcePath, value.sourcePath) == 0 && workspace->breakpoints[index].line == value.line) { cursor.fail(DebuggerWorkspaceErrorCode::DuplicateBreakpoint); return false; }
    workspace->breakpoints[workspace->breakpointCount++] = value;
    return true;
}

} // namespace

void DebuggerWorkspaceInit(DebuggerWorkspace* workspace) {
    if (!workspace) return;
    __builtin_memset(workspace, 0, sizeof(*workspace));
    clearError(workspace);
}

uint32_t BuildDebuggerWorkspaceMaterializationPlan(const DebuggerWorkspace& workspace,
    DebuggerWorkspaceMaterializationEntry* entries, uint32_t capacity) {
    if (!entries) return 0;
    uint32_t count = 0;
    for (uint32_t i = 0; i < workspace.breakpointCount &&
         i < kDebuggerWorkspaceMaxBreakpoints && count < capacity; ++i) {
        const DebuggerWorkspaceBreakpoint& source = workspace.breakpoints[i];
        if (!source.enabled) continue;
        DebuggerWorkspaceMaterializationEntry& entry = entries[count++];
        entry = DebuggerWorkspaceMaterializationEntry();
        copyBounded(entry.sourcePath, sizeof(entry.sourcePath), source.sourcePath);
        entry.line = source.line;
        entry.column = source.column;
        entry.action = source.action;
        entry.hitPolicy = source.hitPolicy;
        entry.hitThreshold = source.hitThreshold;
        copyBounded(entry.condition, sizeof(entry.condition), source.condition);
        copyBounded(entry.logTemplate, sizeof(entry.logTemplate), source.logTemplate);
    }
    return count;
}

int DebuggerWorkspaceFindBreakpoint(const DebuggerWorkspace* workspace, const char* sourcePath, uint32_t line) {
    if (!workspace || !sourcePath) return -1;
    char normalized[kMaxProjectPathBytes] = {};
    if (!validSourcePath(sourcePath, normalized)) return -1;
    for (uint32_t index = 0; index < workspace->breakpointCount; ++index) if (__builtin_strcmp(workspace->breakpoints[index].sourcePath, normalized) == 0 && workspace->breakpoints[index].line == line) return static_cast<int>(index);
    return -1;
}

bool DebuggerWorkspaceAddBreakpoint(DebuggerWorkspace* workspace, const char* sourcePath, uint32_t line, uint32_t column, bool enabled, uint32_t action, uint32_t hitPolicy, uint32_t hitThreshold, const char* condition, const char* logTemplate) {
    if (!workspace || !sourcePath || !condition || !logTemplate) return false;
    char normalized[kMaxProjectPathBytes] = {};
    if (workspace->breakpointCount >= kDebuggerWorkspaceMaxBreakpoints || !validBreakpoint(sourcePath, line, column, action, hitPolicy, hitThreshold, condition, logTemplate, normalized) || DebuggerWorkspaceFindBreakpoint(workspace, normalized, line) >= 0) return false;
    DebuggerWorkspaceBreakpoint& value = workspace->breakpoints[workspace->breakpointCount++];
    __builtin_strcpy(value.sourcePath, normalized); value.line = line; value.column = column; value.enabled = enabled; value.action = action; value.hitPolicy = hitPolicy; value.hitThreshold = hitThreshold; __builtin_strcpy(value.condition, condition); __builtin_strcpy(value.logTemplate, logTemplate);
    return true;
}

bool DebuggerWorkspaceToggleBreakpoint(DebuggerWorkspace* workspace, const char* sourcePath,
                                       uint32_t line, uint32_t column) {
    if (!workspace || !sourcePath) return false;
    const int index = DebuggerWorkspaceFindBreakpoint(workspace, sourcePath, line);
    if (index >= 0) {
        workspace->breakpoints[index].enabled = !workspace->breakpoints[index].enabled;
        return true;
    }
    return DebuggerWorkspaceAddBreakpoint(workspace, sourcePath, line, column, true, 0, 0, 0, "", "");
}

bool DebuggerWorkspaceRemoveBreakpoint(DebuggerWorkspace* workspace, const char* sourcePath,
                                       uint32_t line) {
    const int index = DebuggerWorkspaceFindBreakpoint(workspace, sourcePath, line);
    if (!workspace || index < 0) return false;
    for (uint32_t i = static_cast<uint32_t>(index) + 1; i < workspace->breakpointCount; ++i)
        workspace->breakpoints[i - 1] = workspace->breakpoints[i];
    --workspace->breakpointCount;
    __builtin_memset(&workspace->breakpoints[workspace->breakpointCount], 0,
                sizeof(workspace->breakpoints[workspace->breakpointCount]));
    return true;
}

bool DebuggerWorkspaceSetBreakpointEnabled(DebuggerWorkspace* workspace, const char* sourcePath,
                                           uint32_t line, bool enabled) {
    const int index = DebuggerWorkspaceFindBreakpoint(workspace, sourcePath, line);
    if (!workspace || index < 0) return false;
    workspace->breakpoints[index].enabled = enabled;
    return true;
}

bool DebuggerWorkspaceUpdateBreakpoint(DebuggerWorkspace* workspace, const char* sourcePath,
                                       uint32_t line, uint32_t action, uint32_t hitPolicy,
                                       uint32_t hitThreshold, const char* condition,
                                       const char* logTemplate) {
    const int index = DebuggerWorkspaceFindBreakpoint(workspace, sourcePath, line);
    if (!workspace || index < 0 || !condition || !logTemplate) return false;
    DebuggerWorkspaceBreakpoint& breakpoint = workspace->breakpoints[index];
    char normalized[kMaxProjectPathBytes] = {};
    if (!validBreakpoint(breakpoint.sourcePath, breakpoint.line, breakpoint.column,
                         action, hitPolicy, hitThreshold, condition, logTemplate, normalized)) return false;
    breakpoint.action = action;
    breakpoint.hitPolicy = hitPolicy;
    breakpoint.hitThreshold = hitThreshold;
    __builtin_strcpy(breakpoint.condition, condition);
    __builtin_strcpy(breakpoint.logTemplate, logTemplate);
    return true;
}

bool DebuggerWorkspaceAddWatch(DebuggerWorkspace* workspace, const char* expression) {
    if (!workspace) return false;
    clearError(workspace);
    uint32_t expressionLength = 0;
    if (workspace->watchCount >= kDebuggerWorkspaceMaxWatches) {
        setWorkspaceError(workspace, DebuggerWorkspaceErrorCode::OverCapacity);
        return false;
    }
    if (!expression) {
        setWorkspaceError(workspace, DebuggerWorkspaceErrorCode::NullInput);
        return false;
    }
    if (!boundedLength(expression, kDebugWatchMaxExpressionBytes + 1u, &expressionLength)) {
        setWorkspaceError(workspace, DebuggerWorkspaceErrorCode::StringTooLong);
        return false;
    }
    if (!copyBounded(workspace->watches[workspace->watchCount], sizeof(workspace->watches[0]), expression)) {
        setWorkspaceError(workspace, DebuggerWorkspaceErrorCode::StringTooLong);
        return false;
    }
    ++workspace->watchCount;
    return true;
}

bool DebuggerWorkspaceEditWatch(DebuggerWorkspace* workspace, uint32_t index, const char* expression) {
    if (!workspace) return false;
    clearError(workspace);
    uint32_t expressionLength = 0;
    if (index >= workspace->watchCount) {
        setWorkspaceError(workspace, DebuggerWorkspaceErrorCode::MissingField);
        return false;
    }
    if (!expression) {
        setWorkspaceError(workspace, DebuggerWorkspaceErrorCode::NullInput);
        return false;
    }
    if (!boundedLength(expression, kDebugWatchMaxExpressionBytes + 1u, &expressionLength)) {
        setWorkspaceError(workspace, DebuggerWorkspaceErrorCode::StringTooLong);
        return false;
    }
    if (!copyBounded(workspace->watches[index], sizeof(workspace->watches[index]), expression)) {
        setWorkspaceError(workspace, DebuggerWorkspaceErrorCode::StringTooLong);
        return false;
    }
    return true;
}

bool DebuggerWorkspaceRemoveWatch(DebuggerWorkspace* workspace, uint32_t index) {
    if (!workspace || index >= workspace->watchCount) return false;
    for (uint32_t i = index + 1; i < workspace->watchCount; ++i)
        __builtin_memcpy(workspace->watches[i - 1], workspace->watches[i], sizeof(workspace->watches[i]));
    --workspace->watchCount;
    __builtin_memset(workspace->watches[workspace->watchCount], 0, sizeof(workspace->watches[0]));
    return true;
}

bool SerializeDebuggerWorkspace(const DebuggerWorkspace& workspace, char* output, uint32_t outputSize, uint32_t* outBytes, DebuggerWorkspaceErrorCode* error) {
    DebuggerWorkspaceErrorCode localError = DebuggerWorkspaceErrorCode::None;
    if (!error) error = &localError;
    setError(error, DebuggerWorkspaceErrorCode::None);
    if (!output || !outputSize || !outBytes) { setError(error, DebuggerWorkspaceErrorCode::NullInput); return false; }
    if (workspace.breakpointCount > kDebuggerWorkspaceMaxBreakpoints || workspace.watchCount > kDebuggerWorkspaceMaxWatches) { setError(error, DebuggerWorkspaceErrorCode::OverCapacity); return false; }
    for (uint32_t i = 0; i < workspace.breakpointCount; ++i) {
        DebuggerWorkspaceBreakpoint normalized = workspace.breakpoints[i];
        char canonicalPath[kMaxProjectPathBytes] = {};
        const DebuggerWorkspaceErrorCode validation = breakpointValidationError(
            normalized.sourcePath, normalized.line, normalized.column, normalized.action,
            normalized.hitPolicy, normalized.hitThreshold, normalized.condition,
            normalized.logTemplate, canonicalPath);
        if (validation != DebuggerWorkspaceErrorCode::None) { setError(error, validation); return false; }
        __builtin_strcpy(normalized.sourcePath, canonicalPath);
        for (uint32_t previous = 0; previous < i; ++previous) {
            char previousPath[kMaxProjectPathBytes] = {};
            const DebuggerWorkspaceErrorCode previousValidation = breakpointValidationError(
                workspace.breakpoints[previous].sourcePath, workspace.breakpoints[previous].line,
                workspace.breakpoints[previous].column, workspace.breakpoints[previous].action,
                workspace.breakpoints[previous].hitPolicy, workspace.breakpoints[previous].hitThreshold,
                workspace.breakpoints[previous].condition, workspace.breakpoints[previous].logTemplate,
                previousPath);
            if (previousValidation != DebuggerWorkspaceErrorCode::None) { setError(error, previousValidation); return false; }
            if (__builtin_strcmp(previousPath, normalized.sourcePath) == 0 &&
                workspace.breakpoints[previous].line == normalized.line) {
                setError(error, DebuggerWorkspaceErrorCode::DuplicateBreakpoint);
                return false;
            }
        }
    }
    for (uint32_t i = 0; i < workspace.watchCount; ++i) {
        uint32_t watchLength = 0;
        if (!boundedLength(workspace.watches[i], kDebugWatchMaxExpressionBytes + 1u, &watchLength)) {
            setError(error, DebuggerWorkspaceErrorCode::StringTooLong);
            return false;
        }
    }
    uint32_t length = 0; appendText(output, outputSize, &length, "{\"version\":1,\"breakpoints\":[", error);
    for (uint32_t i = 0; i < workspace.breakpointCount && *error == DebuggerWorkspaceErrorCode::None; ++i) {
        if (i) appendChar(output, outputSize, &length, ',', error);
        DebuggerWorkspaceBreakpoint b = workspace.breakpoints[i];
        char canonicalPath[kMaxProjectPathBytes] = {};
        __builtin_strcpy(canonicalPath, b.sourcePath);
        NormalizePath(b.sourcePath, canonicalPath, sizeof(canonicalPath));
        __builtin_strcpy(b.sourcePath, canonicalPath);
        appendText(output, outputSize, &length, "{\"sourcePath\":", error); appendString(output, outputSize, &length, b.sourcePath, error); appendText(output, outputSize, &length, ",\"line\":", error); appendUnsigned(output, outputSize, &length, b.line, error); appendText(output, outputSize, &length, ",\"column\":", error); appendUnsigned(output, outputSize, &length, b.column, error); appendText(output, outputSize, &length, ",\"enabled\":", error); appendText(output, outputSize, &length, b.enabled ? "true" : "false", error); appendText(output, outputSize, &length, ",\"action\":", error); appendString(output, outputSize, &length, actionName(b.action), error); appendText(output, outputSize, &length, ",\"condition\":", error); appendString(output, outputSize, &length, b.condition, error); appendText(output, outputSize, &length, ",\"hitPolicy\":", error); appendString(output, outputSize, &length, policyName(b.hitPolicy), error); appendText(output, outputSize, &length, ",\"hitThreshold\":", error); appendUnsigned(output, outputSize, &length, b.hitThreshold, error); appendText(output, outputSize, &length, ",\"logTemplate\":", error); appendString(output, outputSize, &length, b.logTemplate, error); appendChar(output, outputSize, &length, '}', error);
    }
    appendText(output, outputSize, &length, "],\"watches\":[", error);
    for (uint32_t i = 0; i < workspace.watchCount && *error == DebuggerWorkspaceErrorCode::None; ++i) { if (i) appendChar(output, outputSize, &length, ',', error); appendString(output, outputSize, &length, workspace.watches[i], error); }
    appendText(output, outputSize, &length, "]}", error);
    if (*error != DebuggerWorkspaceErrorCode::None) return false;
    output[length] = '\0'; *outBytes = length; return true;
}

bool ParseDebuggerWorkspace(const char* bytes, uint32_t length, DebuggerWorkspace* output, DebuggerWorkspaceErrorCode* error) {
    DebuggerWorkspaceErrorCode localError = DebuggerWorkspaceErrorCode::None;
    if (!error) error = &localError;
    setError(error, DebuggerWorkspaceErrorCode::None);
    if (!bytes || !output) { setError(error, DebuggerWorkspaceErrorCode::NullInput); return false; }
    if (length > kDebuggerWorkspaceMaxFileBytes) { DebuggerWorkspaceInit(output); setWorkspaceError(output, DebuggerWorkspaceErrorCode::FileError); setError(error, DebuggerWorkspaceErrorCode::FileError); return false; }
    static DebuggerWorkspace parsed = {};
    DebuggerWorkspaceInit(&parsed);
    Cursor cursor(bytes, length);
    if (!cursor.take('{')) goto failed;
    {
        bool version = false, breakpoints = false, watches = false;
        while (cursor.good()) {
            char key[32] = {};
            if (!cursor.string(key, sizeof(key)) || !cursor.take(':')) goto failed;
            if (__builtin_strcmp(key, "version") == 0) { if (version) { cursor.fail(DebuggerWorkspaceErrorCode::DuplicateField); goto failed; } version = true; uint32_t value = 0; if (!cursor.number(&value)) goto failed; if (value != 1) { cursor.fail(DebuggerWorkspaceErrorCode::UnsupportedVersion); goto failed; } }
            else if (__builtin_strcmp(key, "breakpoints") == 0) { if (breakpoints) { cursor.fail(DebuggerWorkspaceErrorCode::DuplicateField); goto failed; } breakpoints = true; if (!cursor.take('[')) goto failed; if (!cursor.consume(']')) { while (cursor.good()) { if (!parseBreakpoint(cursor, &parsed)) goto failed; if (cursor.consume(']')) break; if (!cursor.take(',')) goto failed; } } }
            else if (__builtin_strcmp(key, "watches") == 0) { if (watches) { cursor.fail(DebuggerWorkspaceErrorCode::DuplicateField); goto failed; } watches = true; if (!cursor.take('[')) goto failed; if (!cursor.consume(']')) { while (cursor.good()) { if (parsed.watchCount >= kDebuggerWorkspaceMaxWatches) { cursor.fail(DebuggerWorkspaceErrorCode::OverCapacity); goto failed; } if (!cursor.string(parsed.watches[parsed.watchCount], sizeof(parsed.watches[0]))) goto failed; ++parsed.watchCount; if (cursor.consume(']')) break; if (!cursor.take(',')) goto failed; } } }
            else { cursor.fail(DebuggerWorkspaceErrorCode::UnknownField); goto failed; }
            if (cursor.consume('}')) break; if (!cursor.take(',')) goto failed;
        }
        if (!version || !breakpoints || !watches) { cursor.fail(DebuggerWorkspaceErrorCode::MissingField); goto failed; }
    }
    if (!cursor.end()) { cursor.fail(DebuggerWorkspaceErrorCode::MalformedJson); goto failed; }
    *output = parsed; setError(error, DebuggerWorkspaceErrorCode::None); return true;
failed:
    DebuggerWorkspaceInit(output); setWorkspaceError(output, cursor.error()); setError(error, cursor.error()); return false;
}

bool DebuggerWorkspaceStoragePath(const char* projectRoot, char* output, uint32_t outputSize) {
    return JoinWorkspacePath(projectRoot, "guidexos.debugger.json", output, outputSize);
}

bool DebuggerWorkspaceStorageBackupPath(const char* projectRoot, char* output, uint32_t outputSize) {
    return JoinWorkspacePath(projectRoot, "guidexos.debugger.json.bak", output, outputSize);
}

bool DebuggerWorkspaceStorageLoad(const WorkspaceFileSystem& fileSystem, const char* projectRoot,
                                  const char* projectId, DebuggerWorkspace* output,
                                  char* status, uint32_t statusSize) {
    if (!output) { setStorageStatus(status, statusSize, "load null output"); return false; }
    DebuggerWorkspaceInit(output);
    if (!validProjectId(projectId)) { setStorageStatus(status, statusSize, "load invalid project id"); return false; }
    char primary[kMaxPathBytes] = {};
    char backup[kMaxPathBytes] = {};
    if (!DebuggerWorkspaceStoragePath(projectRoot, primary, sizeof(primary)) ||
        !DebuggerWorkspaceStorageBackupPath(projectRoot, backup, sizeof(backup))) {
        setStorageStatus(status, statusSize, "load invalid path");
        return false;
    }
    static DebuggerWorkspace loaded = {};
    DebuggerWorkspaceInit(&loaded);
    const StorageLoadResult primaryResult = loadStorageFile(fileSystem, primary, &loaded);
    if (primaryResult == StorageLoadResult::Loaded) {
        *output = loaded;
        setStorageStatus(status, statusSize, "loaded primary");
        return true;
    }
    if (primaryResult == StorageLoadResult::CallbackFailure) {
        setStorageStatus(status, statusSize, "load callback failure");
        return false;
    }
    const StorageLoadResult backupResult = loadStorageFile(fileSystem, backup, &loaded);
    if (backupResult == StorageLoadResult::Loaded) {
        *output = loaded;
        setStorageStatus(status, statusSize, "loaded backup");
        return true;
    }
    if (backupResult == StorageLoadResult::CallbackFailure) setStorageStatus(status, statusSize, "load callback failure");
    else if (primaryResult == StorageLoadResult::Invalid || backupResult == StorageLoadResult::Invalid)
        setStorageStatus(status, statusSize, "load parse failure");
    else setStorageStatus(status, statusSize, "load file absent");
    return false;
}

bool DebuggerWorkspaceStorageSave(const WorkspaceFileSystem& fileSystem, const char* projectRoot,
                                  const char* projectId, const DebuggerWorkspace* workspace,
                                  char* status, uint32_t statusSize) {
    if (!workspace) { setStorageStatus(status, statusSize, "save null workspace"); return false; }
    if (!validProjectId(projectId)) { setStorageStatus(status, statusSize, "save invalid project id"); return false; }
    char primary[kMaxPathBytes] = {};
    char backup[kMaxPathBytes] = {};
    if (!DebuggerWorkspaceStoragePath(projectRoot, primary, sizeof(primary)) ||
        !DebuggerWorkspaceStorageBackupPath(projectRoot, backup, sizeof(backup))) {
        setStorageStatus(status, statusSize, "save invalid path");
        return false;
    }
    static char bytes[kDebuggerWorkspaceMaxFileBytes + 1] = {};
    uint32_t serializedBytes = 0;
    DebuggerWorkspaceErrorCode error = DebuggerWorkspaceErrorCode::None;
    if (!SerializeDebuggerWorkspace(*workspace, bytes, sizeof(bytes), &serializedBytes, &error)) {
        setStorageStatus(status, statusSize, "save serialization failure");
        return false;
    }
    if (!fileSystem.write) { setStorageStatus(status, statusSize, "save callback failure"); return false; }
    uint32_t writtenBytes = 0;
    if (!fileSystem.write(fileSystem.userData, backup, bytes, serializedBytes, &writtenBytes)) {
        setStorageStatus(status, statusSize, "save callback failure");
        return false;
    }
    if (writtenBytes != serializedBytes) { setStorageStatus(status, statusSize, "save short write"); return false; }
    writtenBytes = 0;
    if (!fileSystem.write(fileSystem.userData, primary, bytes, serializedBytes, &writtenBytes)) {
        setStorageStatus(status, statusSize, "save callback failure");
        return false;
    }
    if (writtenBytes != serializedBytes) { setStorageStatus(status, statusSize, "save short write"); return false; }
    setStorageStatus(status, statusSize, "saved primary and backup");
    return true;
}

} // namespace developer_studio
} // namespace guidexos
