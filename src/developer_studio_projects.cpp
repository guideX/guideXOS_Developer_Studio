#include "developer_studio_projects.h"

namespace guidexos {
namespace developer_studio {
namespace {

static const char kProjectFileName[] = "guidexos.project";
static const char kNativeGuiKind[] = "native-gui-application";
static const char kTargetId[] = "guidexos.amd64.hosted.native";
static const char kAbi[] = "guidexos-c-abi-v1";
static const char kArchitecture[] = "amd64";

static uint32_t lengthOf(const char* value, uint32_t limit, bool* terminated = nullptr) {
    if (terminated) *terminated = false;
    if (!value) return 0;
    uint32_t length = 0;
    while (length < limit && value[length] != '\0') ++length;
    if (terminated) *terminated = length < limit;
    return length;
}

static bool copyText(char* output, uint32_t outputSize, const char* input) {
    if (!output || outputSize == 0 || !input) return false;
    bool terminated = false;
    uint32_t length = lengthOf(input, outputSize, &terminated);
    if (!terminated) { output[0] = '\0'; return false; }
    for (uint32_t i = 0; i < length; ++i) output[i] = input[i];
    output[length] = '\0';
    return true;
}

static bool equalText(const char* left, const char* right) {
    if (!left || !right) return left == right;
    uint32_t i = 0;
    while (left[i] != '\0' && right[i] != '\0') {
        if (left[i] != right[i]) return false;
        ++i;
    }
    return left[i] == right[i];
}

static bool equalTextFolded(const char* left, const char* right) {
    if (!left || !right) return left == right;
    uint32_t i = 0;
    while (left[i] != '\0' && right[i] != '\0') {
        char a = left[i];
        char b = right[i];
        if (a >= 'A' && a <= 'Z') a = static_cast<char>(a + ('a' - 'A'));
        if (b >= 'A' && b <= 'Z') b = static_cast<char>(b + ('a' - 'A'));
        if (a != b) return false;
        ++i;
    }
    return left[i] == right[i];
}

static bool appendChar(char* output, uint32_t outputSize, uint32_t& length, char value) {
    if (!output || length + 1 >= outputSize) return false;
    output[length++] = value;
    output[length] = '\0';
    return true;
}

static bool appendText(char* output, uint32_t outputSize, uint32_t& length, const char* value) {
    if (!value) return false;
    for (uint32_t i = 0; value[i] != '\0'; ++i) if (!appendChar(output, outputSize, length, value[i])) return false;
    return true;
}

static bool appendNumber(char* output, uint32_t outputSize, uint32_t& length, uint32_t value) {
    char digits[12] = {};
    uint32_t count = 0;
    if (value == 0) digits[count++] = '0';
    while (value > 0 && count < sizeof(digits)) {
        digits[count++] = static_cast<char>('0' + value % 10);
        value /= 10;
    }
    while (count > 0) if (!appendChar(output, outputSize, length, digits[--count])) return false;
    return true;
}

static bool appendJsonString(char* output, uint32_t outputSize, uint32_t& length, const char* value) {
    if (!appendChar(output, outputSize, length, '"')) return false;
    for (uint32_t i = 0; value && value[i] != '\0'; ++i) {
        char valueChar = value[i];
        if (valueChar == '"' || valueChar == '\\') {
            if (!appendChar(output, outputSize, length, '\\') || !appendChar(output, outputSize, length, valueChar)) return false;
        } else if (valueChar == '\n') {
            if (!appendText(output, outputSize, length, "\\n")) return false;
        } else if (valueChar == '\r') {
            if (!appendText(output, outputSize, length, "\\r")) return false;
        } else if (valueChar == '\t') {
            if (!appendText(output, outputSize, length, "\\t")) return false;
        } else if (static_cast<unsigned char>(valueChar) < 0x20) {
            return false;
        } else if (!appendChar(output, outputSize, length, valueChar)) return false;
    }
    return appendChar(output, outputSize, length, '"');
}

static bool appendCppString(char* output, uint32_t outputSize, uint32_t& length, const char* value) {
    if (!appendChar(output, outputSize, length, '"')) return false;
    for (uint32_t i = 0; value && value[i] != '\0'; ++i) {
        char valueChar = value[i];
        if (valueChar == '"' || valueChar == '\\') {
            if (!appendChar(output, outputSize, length, '\\') || !appendChar(output, outputSize, length, valueChar)) return false;
        } else if (!appendChar(output, outputSize, length, valueChar)) return false;
    }
    return appendChar(output, outputSize, length, '"');
}

static void initializeProject(Project* project) {
    if (!project) return;
    *project = Project();
    project->kind = ProjectKind::NativeGuiApplication;
    project->loadState = ProjectLoadState::NotLoaded;
    project->validationState = ProjectValidationState::Unknown;
    project->error = ProjectErrorCode::None;
}

static bool isSlash(char value) { return value == '/' || value == static_cast<char>(92); }

static bool isAbsolutePath(const char* path) {
    bool terminated = false;
    uint32_t length = lengthOf(path, kMaxPathBytes, &terminated);
    if (!path || !terminated || length == 0) return false;
    if (isSlash(path[0])) return true;
    return length >= 3 && path[1] == ':' && isSlash(path[2]);
}

static bool isAsciiLowerLetter(char value) { return value >= 'a' && value <= 'z'; }
static bool isAsciiDigit(char value) { return value >= '0' && value <= '9'; }

static bool isSafeRelativePath(const char* path, char* normalized, uint32_t normalizedSize) {
    if (!path || path[0] == '\0' || isAbsolutePath(path) || path[1] == ':') return false;
    bool terminated = false;
    uint32_t length = lengthOf(path, kMaxProjectPathBytes, &terminated);
    if (!terminated || length == 0 || length >= kMaxProjectPathBytes || PathContainsTraversal(path)) return false;
    uint32_t out = 0;
    uint32_t segmentStart = 0;
    for (uint32_t i = 0; i <= length; ++i) {
        if (i < length && !isSlash(path[i])) {
            if (static_cast<unsigned char>(path[i]) < 0x20) return false;
            continue;
        }
        uint32_t segmentLength = i - segmentStart;
        if (segmentLength == 0) return false;
        if (segmentLength == 1 && path[segmentStart] == '.') return false;
        if (out > 0 && !appendChar(normalized, normalizedSize, out, '/')) return false;
        for (uint32_t j = 0; j < segmentLength; ++j) if (!appendChar(normalized, normalizedSize, out, path[segmentStart + j])) return false;
        segmentStart = i + 1;
    }
    return out > 0;
}

static bool isSafeOutputName(const char* value) {
    if (!value || value[0] == '\0') return false;
    uint32_t length = lengthOf(value, kMaxProjectOutputNameBytes, nullptr);
    if (length == 0 || length >= kMaxProjectOutputNameBytes || value[0] == '.') return false;
    for (uint32_t i = 0; i < length; ++i) {
        char c = value[i];
        if (!(isAsciiLowerLetter(c) || isAsciiDigit(c) || c == '-' || c == '_' || c == '.')) return false;
    }
    return true;
}

struct JsonCursor {
    const char* bytes;
    uint32_t length;
    uint32_t position;
    ProjectErrorCode error;
};

static void skipWhitespace(JsonCursor& cursor) {
    while (cursor.position < cursor.length) {
        char c = cursor.bytes[cursor.position];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') ++cursor.position;
        else break;
    }
}

static bool expect(JsonCursor& cursor, char expected) {
    skipWhitespace(cursor);
    if (cursor.position >= cursor.length || cursor.bytes[cursor.position] != expected) {
        cursor.error = ProjectErrorCode::MalformedJson;
        return false;
    }
    ++cursor.position;
    return true;
}

static bool parseJsonString(JsonCursor& cursor, char* output, uint32_t outputSize) {
    if (!expect(cursor, '"')) return false;
    uint32_t out = 0;
    while (cursor.position < cursor.length) {
        char c = cursor.bytes[cursor.position++];
        if (c == '"') {
            if (out >= outputSize) { cursor.error = ProjectErrorCode::MalformedJson; return false; }
            output[out] = '\0';
            return true;
        }
        if (static_cast<unsigned char>(c) < 0x20) { cursor.error = ProjectErrorCode::MalformedJson; return false; }
        if (c == '\\') {
            if (cursor.position >= cursor.length) { cursor.error = ProjectErrorCode::MalformedJson; return false; }
            char escaped = cursor.bytes[cursor.position++];
            if (escaped == '"' || escaped == '\\' || escaped == '/') c = escaped;
            else if (escaped == 'b') c = '\b';
            else if (escaped == 'f') c = '\f';
            else if (escaped == 'n') c = '\n';
            else if (escaped == 'r') c = '\r';
            else if (escaped == 't') c = '\t';
            else { cursor.error = ProjectErrorCode::MalformedJson; return false; }
        }
        if (out + 1 >= outputSize) { cursor.error = ProjectErrorCode::MalformedJson; return false; }
        output[out++] = c;
    }
    cursor.error = ProjectErrorCode::MalformedJson;
    return false;
}

static bool parseNumber(JsonCursor& cursor, uint32_t* output) {
    skipWhitespace(cursor);
    if (cursor.position >= cursor.length || cursor.bytes[cursor.position] < '0' || cursor.bytes[cursor.position] > '9') {
        cursor.error = ProjectErrorCode::MalformedJson;
        return false;
    }
    uint32_t value = 0;
    uint32_t digits = 0;
    while (cursor.position < cursor.length && cursor.bytes[cursor.position] >= '0' && cursor.bytes[cursor.position] <= '9') {
        uint32_t digit = static_cast<uint32_t>(cursor.bytes[cursor.position++] - '0');
        if (value > (0xFFFFFFFFu - digit) / 10u) { cursor.error = ProjectErrorCode::MalformedJson; return false; }
        value = value * 10u + digit;
        ++digits;
    }
    if (digits > 1 && cursor.bytes[cursor.position - digits] == '0') { cursor.error = ProjectErrorCode::MalformedJson; return false; }
    if (output) *output = value;
    return true;
}

static bool parseProjectKind(const char* value, ProjectKind* kind) {
    if (!equalText(value, kNativeGuiKind)) return false;
    if (kind) *kind = ProjectKind::NativeGuiApplication;
    return true;
}

static bool parseProjectObject(JsonCursor& cursor, Project* project) {
    if (!expect(cursor, '{')) return false;
    uint32_t fields = 0;
    bool closed = false;
    const uint32_t allFields = (1u << 11) - 1u;
    skipWhitespace(cursor);
    if (cursor.position < cursor.length && cursor.bytes[cursor.position] == '}') { cursor.error = ProjectErrorCode::MissingField; return false; }
    while (cursor.position < cursor.length) {
        char key[64] = {};
        if (!parseJsonString(cursor, key, sizeof(key)) || !expect(cursor, ':')) return false;
        uint32_t bit = 0;
        if (equalText(key, "formatVersion")) bit = 1u << 0;
        else if (equalText(key, "projectId")) bit = 1u << 1;
        else if (equalText(key, "displayName")) bit = 1u << 2;
        else if (equalText(key, "projectKind")) bit = 1u << 3;
        else if (equalText(key, "sourceRoot")) bit = 1u << 4;
        else if (equalText(key, "applicationManifest")) bit = 1u << 5;
        else if (equalText(key, "defaultTargetProfile")) bit = 1u << 6;
        else if (equalText(key, "entryPoint")) bit = 1u << 7;
        else if (equalText(key, "abi")) bit = 1u << 8;
        else if (equalText(key, "architecture")) bit = 1u << 9;
        else if (equalText(key, "outputName")) bit = 1u << 10;
        else { cursor.error = ProjectErrorCode::UnknownField; return false; }
        if ((fields & bit) != 0) { cursor.error = ProjectErrorCode::DuplicateField; return false; }
        fields |= bit;
        if (bit == (1u << 0)) {
            if (!parseNumber(cursor, &project->formatVersion)) return false;
        } else if (bit == (1u << 1)) {
            if (!parseJsonString(cursor, project->projectId, sizeof(project->projectId))) return false;
        } else if (bit == (1u << 2)) {
            if (!parseJsonString(cursor, project->displayName, sizeof(project->displayName))) return false;
        } else if (bit == (1u << 3)) {
            char value[64] = {};
            if (!parseJsonString(cursor, value, sizeof(value)) || !parseProjectKind(value, &project->kind)) {
                cursor.error = ProjectErrorCode::InvalidProjectKind;
                return false;
            }
        } else if (bit == (1u << 4) || bit == (1u << 5)) {
            char value[kMaxProjectPathBytes] = {};
            if (!parseJsonString(cursor, value, sizeof(value))) return false;
            char normalized[kMaxProjectPathBytes] = {};
            if (!isSafeRelativePath(value, normalized, sizeof(normalized))) { cursor.error = ProjectErrorCode::InvalidRelativePath; return false; }
            if (bit == (1u << 4)) copyText(project->sourceRoot, sizeof(project->sourceRoot), normalized);
            else copyText(project->manifestPath, sizeof(project->manifestPath), normalized);
        } else if (bit == (1u << 6)) {
            if (!parseJsonString(cursor, project->targetProfileId, sizeof(project->targetProfileId))) return false;
        } else if (bit == (1u << 7)) {
            if (!parseJsonString(cursor, project->entryPoint, sizeof(project->entryPoint))) return false;
        } else if (bit == (1u << 8)) {
            if (!parseJsonString(cursor, project->abi, sizeof(project->abi))) return false;
        } else if (bit == (1u << 9)) {
            if (!parseJsonString(cursor, project->architecture, sizeof(project->architecture))) return false;
        } else if (bit == (1u << 10)) {
            if (!parseJsonString(cursor, project->outputName, sizeof(project->outputName))) return false;
        }
        skipWhitespace(cursor);
        if (cursor.position < cursor.length && cursor.bytes[cursor.position] == ',') { ++cursor.position; continue; }
        if (cursor.position < cursor.length && cursor.bytes[cursor.position] == '}') { ++cursor.position; closed = true; break; }
        cursor.error = ProjectErrorCode::MalformedJson;
        return false;
    }
    if (!closed) { cursor.error = ProjectErrorCode::MalformedJson; return false; }
    if (fields != allFields) { cursor.error = ProjectErrorCode::MissingField; return false; }
    skipWhitespace(cursor);
    return cursor.position == cursor.length;
}

static bool appendProjectField(char* output, uint32_t size, uint32_t& length, const char* name, const char* value, bool& first) {
    if (!first && !appendText(output, size, length, ",\n")) return false;
    first = false;
    if (!appendText(output, size, length, "  ")) return false;
    if (!appendJsonString(output, size, length, name) || !appendText(output, size, length, ": ") || !appendJsonString(output, size, length, value)) return false;
    return true;
}

static bool appendProjectNumberField(char* output, uint32_t size, uint32_t& length, const char* name, uint32_t value, bool& first) {
    if (!first && !appendText(output, size, length, ",\n")) return false;
    first = false;
    return appendText(output, size, length, "  ") && appendJsonString(output, size, length, name) && appendText(output, size, length, ": ") && appendNumber(output, size, length, value);
}

struct ManifestInfo {
    uint32_t schemaVersion;
    char id[kMaxProjectIdBytes];
    char displayName[kMaxProjectDisplayNameBytes];
    char kind[32];
    char architecture[32];
    char path[kMaxProjectPathBytes];
    char entryPoint[kMaxNameBytes];
    char abi[kMaxNameBytes];
    char runtime[32];
    bool hasSchema;
    bool hasId;
    bool hasDisplayName;
    bool hasKind;
    bool hasEntry;
};

static bool skipJsonValue(JsonCursor& cursor, uint32_t depth);

static bool skipJsonString(JsonCursor& cursor) {
    if (!expect(cursor, '"')) return false;
    while (cursor.position < cursor.length) {
        char c = cursor.bytes[cursor.position++];
        if (c == '"') return true;
        if (static_cast<unsigned char>(c) < 0x20) { cursor.error = ProjectErrorCode::ManifestMalformed; return false; }
        if (c == '\\') {
            if (cursor.position >= cursor.length) { cursor.error = ProjectErrorCode::ManifestMalformed; return false; }
            char escaped = cursor.bytes[cursor.position++];
            if (escaped == 'u') { cursor.error = ProjectErrorCode::ManifestMalformed; return false; }
        }
    }
    cursor.error = ProjectErrorCode::ManifestMalformed;
    return false;
}

static bool skipJsonValue(JsonCursor& cursor, uint32_t depth) {
    if (depth > 8) { cursor.error = ProjectErrorCode::ManifestMalformed; return false; }
    skipWhitespace(cursor);
    if (cursor.position >= cursor.length) { cursor.error = ProjectErrorCode::ManifestMalformed; return false; }
    char c = cursor.bytes[cursor.position];
    if (c == '"') return skipJsonString(cursor);
    if (c == '{') {
        ++cursor.position;
        skipWhitespace(cursor);
        if (cursor.position < cursor.length && cursor.bytes[cursor.position] == '}') { ++cursor.position; return true; }
        while (cursor.position < cursor.length) {
            if (!skipJsonString(cursor) || !expect(cursor, ':') || !skipJsonValue(cursor, depth + 1)) return false;
            skipWhitespace(cursor);
            if (cursor.position < cursor.length && cursor.bytes[cursor.position] == ',') { ++cursor.position; continue; }
            if (cursor.position < cursor.length && cursor.bytes[cursor.position] == '}') { ++cursor.position; return true; }
            cursor.error = ProjectErrorCode::ManifestMalformed;
            return false;
        }
    } else if (c == '[') {
        ++cursor.position;
        skipWhitespace(cursor);
        if (cursor.position < cursor.length && cursor.bytes[cursor.position] == ']') { ++cursor.position; return true; }
        while (cursor.position < cursor.length) {
            if (!skipJsonValue(cursor, depth + 1)) return false;
            skipWhitespace(cursor);
            if (cursor.position < cursor.length && cursor.bytes[cursor.position] == ',') { ++cursor.position; continue; }
            if (cursor.position < cursor.length && cursor.bytes[cursor.position] == ']') { ++cursor.position; return true; }
            cursor.error = ProjectErrorCode::ManifestMalformed;
            return false;
        }
    } else if ((c >= '0' && c <= '9') || c == '-') {
        if (c == '-') ++cursor.position;
        uint32_t digits = 0;
        while (cursor.position < cursor.length && cursor.bytes[cursor.position] >= '0' && cursor.bytes[cursor.position] <= '9') { ++cursor.position; ++digits; }
        if (digits == 0) cursor.error = ProjectErrorCode::ManifestMalformed;
        return digits > 0;
    } else {
        const char* literals[] = { "true", "false", "null" };
        for (uint32_t i = 0; i < sizeof(literals) / sizeof(literals[0]); ++i) {
            uint32_t literalLength = lengthOf(literals[i], 8);
            bool matches = cursor.position + literalLength <= cursor.length;
            for (uint32_t j = 0; matches && j < literalLength; ++j) if (cursor.bytes[cursor.position + j] != literals[i][j]) matches = false;
            if (matches) { cursor.position += literalLength; return true; }
        }
        cursor.error = ProjectErrorCode::ManifestMalformed;
    }
    return false;
}

static bool parseManifestEntries(JsonCursor& cursor, ManifestInfo& manifest) {
    if (!expect(cursor, '[')) return false;
    uint32_t count = 0;
    skipWhitespace(cursor);
    if (cursor.position < cursor.length && cursor.bytes[cursor.position] == ']') { cursor.error = ProjectErrorCode::ManifestMalformed; return false; }
    while (cursor.position < cursor.length) {
        if (!expect(cursor, '{')) return false;
        char architecture[32] = {};
        char path[kMaxProjectPathBytes] = {};
        char entryPoint[kMaxNameBytes] = {};
        char abi[kMaxNameBytes] = {};
        char runtime[32] = {};
        uint32_t fields = 0;
        bool entryClosed = false;
        while (cursor.position < cursor.length) {
            char key[64] = {};
            if (!parseJsonString(cursor, key, sizeof(key)) || !expect(cursor, ':')) return false;
            uint32_t bit = 0;
            if (equalText(key, "architecture")) bit = 1u << 0;
            else if (equalText(key, "path")) bit = 1u << 1;
            else if (equalText(key, "entryPoint")) bit = 1u << 2;
            else if (equalText(key, "abi")) bit = 1u << 3;
            else if (equalText(key, "runtime")) bit = 1u << 4;
            if (bit == 0) {
                if (!skipJsonValue(cursor, 0)) return false;
            } else {
                if ((fields & bit) != 0) { cursor.error = ProjectErrorCode::ManifestMalformed; return false; }
                fields |= bit;
                char* destination = bit == (1u << 0) ? architecture : (bit == (1u << 1) ? path : (bit == (1u << 2) ? entryPoint : (bit == (1u << 3) ? abi : runtime)));
                uint32_t capacity = bit == (1u << 0) || bit == (1u << 4) ? 32u : (bit == (1u << 1) ? kMaxProjectPathBytes : kMaxNameBytes);
                if (!parseJsonString(cursor, destination, capacity)) return false;
            }
            skipWhitespace(cursor);
            if (cursor.position < cursor.length && cursor.bytes[cursor.position] == ',') { ++cursor.position; continue; }
            if (cursor.position < cursor.length && cursor.bytes[cursor.position] == '}') { ++cursor.position; entryClosed = true; break; }
            cursor.error = ProjectErrorCode::ManifestMalformed;
            return false;
        }
        if (!entryClosed) { cursor.error = ProjectErrorCode::ManifestMalformed; return false; }
        ++count;
        if (count == 1) {
            copyText(manifest.architecture, sizeof(manifest.architecture), architecture);
            copyText(manifest.path, sizeof(manifest.path), path);
            copyText(manifest.entryPoint, sizeof(manifest.entryPoint), entryPoint);
            copyText(manifest.abi, sizeof(manifest.abi), abi);
            copyText(manifest.runtime, sizeof(manifest.runtime), runtime);
        }
        skipWhitespace(cursor);
        if (cursor.position < cursor.length && cursor.bytes[cursor.position] == ',') { ++cursor.position; continue; }
        if (cursor.position < cursor.length && cursor.bytes[cursor.position] == ']') { ++cursor.position; break; }
        cursor.error = ProjectErrorCode::ManifestMalformed;
        return false;
    }
    return count == 1;
}

static bool parseManifest(const char* bytes, uint32_t length, ManifestInfo* output) {
    if (!bytes || !output || length == 0 || length > kMaxProjectFileBytes) return false;
    *output = ManifestInfo();
    JsonCursor cursor = { bytes, length, 0, ProjectErrorCode::ManifestMalformed };
    if (!expect(cursor, '{')) return false;
    bool closed = false;
    while (cursor.position < cursor.length) {
        char key[64] = {};
        if (!parseJsonString(cursor, key, sizeof(key)) || !expect(cursor, ':')) return false;
        if (equalText(key, "schemaVersion")) {
            if (output->hasSchema || !parseNumber(cursor, &output->schemaVersion)) return false;
            output->hasSchema = true;
        } else if (equalText(key, "id")) {
            if (output->hasId || !parseJsonString(cursor, output->id, sizeof(output->id))) return false;
            output->hasId = true;
        } else if (equalText(key, "displayName")) {
            if (output->hasDisplayName || !parseJsonString(cursor, output->displayName, sizeof(output->displayName))) return false;
            output->hasDisplayName = true;
        } else if (equalText(key, "kind")) {
            if (output->hasKind || !parseJsonString(cursor, output->kind, sizeof(output->kind))) return false;
            output->hasKind = true;
        } else if (equalText(key, "entries")) {
            if (output->hasEntry || !parseManifestEntries(cursor, *output)) return false;
            output->hasEntry = true;
        } else if (!skipJsonValue(cursor, 0)) return false;
        skipWhitespace(cursor);
        if (cursor.position < cursor.length && cursor.bytes[cursor.position] == ',') { ++cursor.position; continue; }
        if (cursor.position < cursor.length && cursor.bytes[cursor.position] == '}') { ++cursor.position; closed = true; break; }
        return false;
    }
    skipWhitespace(cursor);
    return closed && cursor.position == cursor.length && output->hasSchema && output->hasId && output->hasDisplayName && output->hasKind && output->hasEntry;
}

static bool expectedManifestPath(const Project& project, char* output, uint32_t outputSize) {
    if (!isSafeOutputName(project.outputName)) return false;
    uint32_t length = 0;
    output[0] = '\0';
    return appendText(output, outputSize, length, "bin/") && appendText(output, outputSize, length, project.architecture) &&
        appendText(output, outputSize, length, "/") && appendText(output, outputSize, length, project.outputName) &&
        appendText(output, outputSize, length, ".elf");
}

static bool joinProjectPath(const char* root, const char* relative, char* output, uint32_t outputSize) {
    char normalized[kMaxProjectPathBytes] = {};
    if (!isSafeRelativePath(relative, normalized, sizeof(normalized))) return false;
    return JoinWorkspacePath(root, normalized, output, outputSize);
}

static bool parentPath(const char* path, char* output, uint32_t outputSize) {
    char normalized[kMaxPathBytes] = {};
    if (!NormalizePath(path, normalized, sizeof(normalized))) return false;
    uint32_t length = lengthOf(normalized, sizeof(normalized));
    if (length == 0) return false;
    while (length > 0 && normalized[length - 1] != '/') --length;
    if (length == 0) return false;
    if (length == 1) return copyText(output, outputSize, "/");
    if (length == 3 && normalized[1] == ':' && normalized[2] == '/') return copyText(output, outputSize, normalized);
    normalized[length - 1] = '\0';
    return copyText(output, outputSize, normalized);
}

static void setResult(ProjectOperationResult* result, ProjectErrorCode error) {
    if (!result) return;
    result->success = false;
    result->error = error;
    result->project.error = error;
    result->project.valid = false;
    result->project.loadState = ProjectLoadState::Invalid;
    result->project.validationState = ProjectValidationState::Invalid;
}

static bool addTrackedPath(char paths[][kMaxPathBytes], uint32_t& count, const char* path) {
    if (count >= 16 || !copyText(paths[count], kMaxPathBytes, path)) return false;
    ++count;
    return true;
}

static bool rollback(const ProjectFileSystem& fileSystem, char files[][kMaxPathBytes], uint32_t fileCount, char directories[][kMaxPathBytes], uint32_t directoryCount) {
    bool success = true;
    if (!fileSystem.removePath) return false;
    for (uint32_t i = fileCount; i > 0; --i) if (!fileSystem.removePath(fileSystem.userData, files[i - 1])) success = false;
    for (uint32_t i = directoryCount; i > 0; --i) if (!fileSystem.removePath(fileSystem.userData, directories[i - 1])) success = false;
    return success;
}

static bool writeTrackedFile(const ProjectFileSystem& fileSystem, const char* path, const char* bytes, uint32_t length, char tracked[][kMaxPathBytes], uint32_t& trackedCount) {
    FileInfo info = {};
    if (fileSystem.stat(fileSystem.userData, path, &info)) return false;
    uint32_t written = 0;
    if (!fileSystem.write || !fileSystem.write(fileSystem.userData, path, bytes, length, &written) || written != length) return false;
    return addTrackedPath(tracked, trackedCount, path);
}

static void repairGeneratedPackagePath(char* output, uint32_t outputSize, uint32_t& length, const char* outputName) {
    const char prefix[] = "if (-not $PackageRoot) { $PackageRoot = Join-Path $ServerRoot \"Apps\\";
    uint32_t prefixLength = lengthOf(prefix, sizeof(prefix));
    uint32_t outputNameLength = lengthOf(outputName, kMaxProjectOutputNameBytes);
    for (uint32_t i = 0; i + prefixLength + outputNameLength < length; ++i) {
        bool matches = true;
        for (uint32_t j = 0; j < prefixLength && matches; ++j) if (output[i + j] != prefix[j]) matches = false;
        if (!matches) continue;
        if (output[i + prefixLength + outputNameLength] == '\"') return;
        if (length + 1 >= outputSize) return;
        for (uint32_t j = length + 1; j > i + prefixLength + outputNameLength; --j) output[j] = output[j - 1];
        output[i + prefixLength + outputNameLength] = '\"';
        ++length;
        return;
    }
}

static bool generateManifest(const Project& project, char* output, uint32_t outputSize, uint32_t* outBytes) {
    uint32_t length = 0;
    bool first = true;
    output[0] = '\0';
    if (!appendText(output, outputSize, length, "{\n") || !appendProjectNumberField(output, outputSize, length, "schemaVersion", 1, first) ||
        !appendProjectField(output, outputSize, length, "id", project.projectId, first) ||
        !appendProjectField(output, outputSize, length, "displayName", project.displayName, first) ||
        !appendProjectField(output, outputSize, length, "version", "0.1.0", first) ||
        !appendProjectField(output, outputSize, length, "publisher", "guideXOS Developer Studio template", first) ||
        !appendProjectField(output, outputSize, length, "description", "Minimal guideXOS Native GUI application.", first) ||
        !appendProjectField(output, outputSize, length, "category", "Applications", first) ||
        !appendProjectField(output, outputSize, length, "kind", "NativeElf", first) ||
        !appendProjectField(output, outputSize, length, "icon", "", first) ||
        !appendProjectField(output, outputSize, length, "minGuideXOSVersion", "0.5.0", first) ||
        !appendText(output, outputSize, length, ",\n  \"supportedArchitectures\": [\n    \"amd64\"\n  ],\n  \"entries\": [\n    {\n") ||
        !appendText(output, outputSize, length, "      \"architecture\": \"amd64\",\n      \"path\": \"bin/amd64/") ||
        !appendText(output, outputSize, length, project.outputName) || !appendText(output, outputSize, length, ".elf") || !appendChar(output, outputSize, length, '\"') || !appendText(output, outputSize, length, ",\n      \"entryPoint\": ") ||
        !appendJsonString(output, outputSize, length, project.entryPoint) || !appendText(output, outputSize, length, ",\n      \"abi\": ") ||
        !appendJsonString(output, outputSize, length, project.abi) || !appendText(output, outputSize, length, ",\n      \"runtime\": \"native-elf\"\n    }\n  ],\n  \"permissions\": [\n    \"log\",\n    \"window\",\n    \"draw\"\n  ],\n  \"fileAssociations\": [],\n  \"defaultWindow\": {\n    \"width\": 640,\n    \"height\": 360,\n    \"resizable\": true\n  }\n}\n")) return false;
    repairGeneratedPackagePath(output, outputSize, length, project.outputName);
    if (outBytes) *outBytes = length;
    return true;
}

static bool generateCMake(const Project& project, char* output, uint32_t outputSize, uint32_t* outBytes) {
    uint32_t length = 0;
    output[0] = '\0';
    if (!appendText(output, outputSize, length, "cmake_minimum_required(VERSION 3.16)\n\nproject(GuideXOSNativeGuiApplication LANGUAGES CXX)\n\nset(CMAKE_CXX_STANDARD 11)\nset(CMAKE_CXX_STANDARD_REQUIRED ON)\nset(CMAKE_CXX_EXTENSIONS OFF)\n\nset(GUIDEXOS_SERVER_ROOT \"\" CACHE PATH \"guideXOS Server checkout\")\nif(NOT GUIDEXOS_SERVER_ROOT)\n    message(FATAL_ERROR \"Pass -DGUIDEXOS_SERVER_ROOT=<server-checkout>\")\nendif()\n\nadd_executable(" ) ||
        !appendText(output, outputSize, length, project.outputName) || !appendText(output, outputSize, length, ".elf\n    src/main.cpp\n    src/freestanding_memory.cpp\n)\ntarget_include_directories(" ) || !appendText(output, outputSize, length, project.outputName) ||
        !appendText(output, outputSize, length, ".elf PRIVATE \"${GUIDEXOS_SERVER_ROOT}/sdk/include\")\ntarget_compile_options(" ) || !appendText(output, outputSize, length, project.outputName) ||
        !appendText(output, outputSize, length, ".elf PRIVATE --target=x86_64-unknown-elf -ffreestanding -fno-exceptions -fno-rtti -fno-stack-protector -fno-unwind-tables -fno-asynchronous-unwind-tables)\ntarget_link_options(" ) ||
        !appendText(output, outputSize, length, project.outputName) || !appendText(output, outputSize, length, ".elf PRIVATE --target=x86_64-unknown-elf -nostdlib -static -fuse-ld=lld -Wl,-e,gx_main)\nset_target_properties(" ) ||
        !appendText(output, outputSize, length, project.outputName) || !appendText(output, outputSize, length, ".elf PROPERTIES OUTPUT_NAME ") || !appendText(output, outputSize, length, project.outputName) ||
        !appendText(output, outputSize, length, " SUFFIX \".elf\")\n")) return false;
    if (outBytes) *outBytes = length;
    return true;
}

static bool generateBuildScript(const Project& project, char* output, uint32_t outputSize, uint32_t* outBytes) {
    uint32_t length = 0;
    output[0] = '\0';
    if (!appendText(output, outputSize, length, "[CmdletBinding()]\nparam(\n    [Parameter(Mandatory=$true)][string]$ServerRoot,\n    [string]$SdkInclude = \"\",\n    [string]$PackageRoot = \"\",\n    [switch]$SkipReadElf\n)\n\n$ErrorActionPreference = \"Stop\"\n$RepoRoot = Split-Path -Parent $MyInvocation.MyCommand.Path\n$ServerRoot = [IO.Path]::GetFullPath($ServerRoot)\nif (-not $SdkInclude) { $SdkInclude = Join-Path $ServerRoot \"sdk\\include\" }\nif (-not $PackageRoot) { $PackageRoot = Join-Path $ServerRoot \"Apps\\") ||
        !appendText(output, outputSize, length, project.outputName) || !appendText(output, outputSize, length, " }\n$PackageBin = Join-Path $PackageRoot \"bin\\amd64\"\n$Manifest = Join-Path $RepoRoot \"app\\app.json\"\n$ObjectRoot = Join-Path $RepoRoot \"build\\objects\"\nfunction Find-Tool([string[]]$Names) { foreach ($name in $Names) { $command = Get-Command $name -ErrorAction SilentlyContinue; if ($command) { return $command.Source } }; foreach ($root in @(\"C:\\Program Files\\LLVM\\bin\", \"C:\\mingw64\\bin\")) { foreach ($name in $Names) { $candidate = Join-Path $root $name; if (Test-Path -LiteralPath $candidate -PathType Leaf) { return $candidate } } }; return $null }\nfunction Invoke-Checked([string]$FilePath, [string[]]$Arguments) { & $FilePath @Arguments; if ($LASTEXITCODE -ne 0) { throw \"Command failed: $FilePath\" } }\nif (-not (Test-Path -LiteralPath $SdkInclude -PathType Container)) { throw \"SDK headers not found: $SdkInclude\" }\n$clang = Find-Tool @('clang++.exe','clang++'); $lld = Find-Tool @('ld.lld.exe','ld.lld'); $readElf = Find-Tool @('llvm-readelf.exe','llvm-readelf','readelf.exe','readelf')\nif (-not $clang -or -not $lld) { throw \"LLVM clang++ and ld.lld are required\" }\nNew-Item -ItemType Directory -Force -Path $PackageBin | Out-Null; New-Item -ItemType Directory -Force -Path $ObjectRoot | Out-Null\nCopy-Item -LiteralPath $Manifest -Destination (Join-Path $PackageRoot \"app.json\") -Force\n$flags = @('--target=x86_64-unknown-elf','-std=c++11','-ffreestanding','-fno-exceptions','-fno-rtti','-fno-stack-protector','-fno-unwind-tables','-fno-asynchronous-unwind-tables',\"-I$SdkInclude\",\"-I$RepoRoot\\src\")\ntry {\n    $mainObject = Join-Path $ObjectRoot \"main.o\"; $memoryObject = Join-Path $ObjectRoot \"freestanding_memory.o\"\n    Invoke-Checked $clang ($flags + @('-c',(Join-Path $RepoRoot \"src\\main.cpp\"),'-o',$mainObject))\n    Invoke-Checked $clang ($flags + @('-c',(Join-Path $RepoRoot \"src\\freestanding_memory.cpp\"),'-o',$memoryObject))\n    $elfPath = Join-Path $PackageBin \"") || !appendText(output, outputSize, length, project.outputName) || !appendText(output, outputSize, length, ".elf\"\n    Invoke-Checked $lld @('-m','elf_x86_64','-static','-e','gx_main',$mainObject,$memoryObject,'-o',$elfPath)\n    if (-not (Test-Path -LiteralPath $elfPath -PathType Leaf)) { throw \"Native ELF output was not produced\" }\n    if (-not $SkipReadElf -and $readElf) { $header = (& $readElf -h $elfPath 2>&1 | Out-String); if ($header -notmatch 'ELF64' -or $header -notmatch 'Advanced Micro Devices X86-64') { throw \"ELF64 AMD64 validation failed\" } }\n    Write-Host \"Native GUI Application build PASS: $elfPath\"\n} finally { if (Test-Path -LiteralPath $ObjectRoot) { Remove-Item -LiteralPath $ObjectRoot -Recurse -Force } }\n")) return false;
    if (outBytes) *outBytes = length;
    return true;
}

static bool generateReadme(const Project& project, char* output, uint32_t outputSize, uint32_t* outBytes) {
    uint32_t length = 0;
    output[0] = '\0';
    if (!appendText(output, outputSize, length, "# ") || !appendText(output, outputSize, length, project.displayName) || !appendText(output, outputSize, length, "\n\nThis is a generated guideXOS Native GUI Application project.\n\nProject identity:\n\n- Application ID: `") ||
        !appendText(output, outputSize, length, project.projectId) || !appendText(output, outputSize, length, "`\n- Project kind: `native-gui-application`\n- Target profile: `guidexos.amd64.hosted.native`\n- ABI: `guidexos-c-abi-v1`\n- Architecture: `amd64`\n\n## Layout\n\n- `guidexos.project` - version 1 project metadata.\n- `app/app.json` - Native ELF App Model manifest.\n- `src/main.cpp` - the `gx_main` starter application.\n- `src/freestanding_memory.cpp` - freestanding memory primitives required by the Native ELF link.\n- `CMakeLists.txt` and `build.ps1` - external build files.\n\n## External build\n\nDeveloper Studio does not build projects in this phase. Run the generated script from this directory and supply the Server checkout explicitly:\n\n```powershell\n.\\build.ps1 -ServerRoot D:\\path\\to\\guideXOSServer\n```\n\nThe generated app is manual-launch only and is limited to the proven AMD64 hosted Native ELF target.\n")) return false;
    if (outBytes) *outBytes = length;
    return true;
}

static bool generateMain(const Project& project, char* output, uint32_t outputSize, uint32_t* outBytes) {
    uint32_t length = 0;
    output[0] = '\0';
    if (!appendText(output, outputSize, length, "#include <guidexos/ui.h>\n\nnamespace {\nstatic gx_handle g_window = 0;\n\nstatic void clearEvent(gx_event* event) {\n    if (!event) return;\n    event->size = 0; event->type = GX_EVENT_NONE; event->window = 0;\n    event->param1 = 0; event->param2 = 0; event->param3 = 0; event->param4 = 0;\n}\n\nstatic void drawWelcome(gx_app_context* ctx) {\n    if (!ctx || !ctx->host) return;\n    if (ctx->host->draw_rect) ctx->host->draw_rect(ctx, g_window, 0, 0, 640, 360, 0x151B28u);\n    if (ctx->host->draw_text) {\n        ctx->host->draw_text(ctx, g_window, 24, 40, ") ||
        !appendCppString(output, outputSize, length, project.displayName) ||
        !appendText(output, outputSize, length, ");\n        ctx->host->draw_text(ctx, g_window, 24, 90, ") ||
        !appendCppString(output, outputSize, length, "Welcome to ") || !appendCppString(output, outputSize, length, project.displayName) ||
        !appendText(output, outputSize, length, ");\n    }\n}\n}\n\nextern \"C\" gx_result GX_CALL gx_main(gx_app_context* ctx) {\n    if (!ctx || !ctx->host || !ctx->host->request_window || !ctx->host->log) return GX_ERROR_INVALID_ARGUMENT;\n    ctx->host->log(ctx, \"GUIDEXOS_NATIVE_TEMPLATE_MARKER project_template=native-gui-application\");\n    ctx->host->log(ctx, \"GUIDEXOS_NATIVE_TEMPLATE_MARKER entry_point=gx_main\");\n    gx_result result = GX_ERROR_FAILED;\n    if (ctx->host->request_window_ex) result = ctx->host->request_window_ex(ctx, ") ||
        !appendCppString(output, outputSize, length, project.displayName) ||
        !appendText(output, outputSize, length, ", 640, 360, GX_WINDOW_FLAG_RESIZABLE | GX_WINDOW_FLAG_CENTERED, &g_window);\n    else result = ctx->host->request_window(ctx, ") ||
        !appendCppString(output, outputSize, length, project.displayName) ||
        !appendText(output, outputSize, length, ", 640, 360, &g_window);\n    if (result != GX_OK) return result;\n    ctx->host->log(ctx, \"GUIDEXOS_NATIVE_TEMPLATE_MARKER window_creation=PASS\");\n    drawWelcome(ctx);\n    ctx->host->log(ctx, \"GUIDEXOS_NATIVE_TEMPLATE_MARKER initial_render=PASS\");\n    if (ctx->host->poll_event) {\n        bool running = true;\n        while (running) {\n            gx_event event; clearEvent(&event);\n            gx_result poll = ctx->host->poll_event(ctx, &event, 500);\n            if (poll == GX_OK && event.window == g_window && (event.type == GX_EVENT_WINDOW_CLOSE || (event.type == GX_EVENT_KEY && event.param1 == GX_KEY_ESCAPE && event.param2 == GX_KEY_ACTION_DOWN))) running = false;\n            else if (poll != GX_OK && poll != GX_ERROR_TIMEOUT) return poll;\n        }\n    } else if (ctx->host->wait_for_close) {\n        result = ctx->host->wait_for_close(ctx, g_window, 300000);\n        if (result != GX_OK && result != GX_ERROR_TIMEOUT) return result;\n    }\n    ctx->host->log(ctx, \"GUIDEXOS_NATIVE_TEMPLATE_MARKER clean_close=PASS\");\n    return ctx->host->exit ? ctx->host->exit(ctx, GX_OK) : GX_OK;\n}\n")) return false;
    if (outBytes) *outBytes = length;
    return true;
}

static bool generateMemory(char* output, uint32_t outputSize, uint32_t* outBytes) {
    static const char text[] =
        "#include <stddef.h>\n\n"
        "extern \"C\" void* memcpy(void* destination, const void* source, size_t bytes) {\n"
        "    unsigned char* out = static_cast<unsigned char*>(destination);\n"
        "    const unsigned char* in = static_cast<const unsigned char*>(source);\n"
        "    for (size_t i = 0; i < bytes; ++i) out[i] = in[i];\n"
        "    return destination;\n}\n\n"
        "extern \"C\" void* memset(void* destination, int value, size_t bytes) {\n"
        "    unsigned char* out = static_cast<unsigned char*>(destination);\n"
        "    for (size_t i = 0; i < bytes; ++i) out[i] = static_cast<unsigned char>(value);\n"
        "    return destination;\n}\n\n"
        "extern \"C\" void* memmove(void* destination, const void* source, size_t bytes) {\n"
        "    unsigned char* out = static_cast<unsigned char*>(destination);\n"
        "    const unsigned char* in = static_cast<const unsigned char*>(source);\n"
        "    if (out < in) for (size_t i = 0; i < bytes; ++i) out[i] = in[i];\n"
        "    else if (out > in) for (size_t i = bytes; i > 0; --i) out[i - 1] = in[i - 1];\n"
        "    return destination;\n}\n";
    uint32_t length = lengthOf(text, 8192);
    if (length + 1 > outputSize) return false;
    for (uint32_t i = 0; i <= length; ++i) output[i] = text[i];
    if (outBytes) *outBytes = length;
    return true;
}

static bool validateManifestAgainstProject(const ManifestInfo& manifest, const Project& project) {
    char expectedPath[kMaxProjectPathBytes] = {};
    return manifest.schemaVersion == 1 && manifest.hasId && manifest.hasDisplayName && manifest.hasKind && manifest.hasEntry &&
        equalText(manifest.id, project.projectId) && equalText(manifest.displayName, project.displayName) && equalText(manifest.kind, "NativeElf") &&
        equalText(manifest.architecture, project.architecture) && equalText(manifest.entryPoint, project.entryPoint) && equalText(manifest.abi, project.abi) &&
        equalText(manifest.runtime, "native-elf") && expectedManifestPath(project, expectedPath, sizeof(expectedPath)) && equalText(manifest.path, expectedPath);
}

static bool verifyRequiredFiles(const ProjectFileSystem& fileSystem, const Project& project) {
    const char* files[] = { "guidexos.project", "CMakeLists.txt", "build.ps1", "README.md", "src/main.cpp", "src/freestanding_memory.cpp", "app/app.json" };
    for (uint32_t i = 0; i < sizeof(files) / sizeof(files[0]); ++i) {
        char path[kMaxPathBytes] = {};
        if (!joinProjectPath(project.rootPath, files[i], path, sizeof(path))) return false;
        FileInfo info = {};
        if (!fileSystem.stat || !fileSystem.stat(fileSystem.userData, path, &info) || info.kind != FileInfoKind::RegularFile) return false;
    }
    char sourcePath[kMaxPathBytes] = {};
    if (!joinProjectPath(project.rootPath, project.sourceRoot, sourcePath, sizeof(sourcePath))) return false;
    FileInfo sourceInfo = {};
    return fileSystem.stat && fileSystem.stat(fileSystem.userData, sourcePath, &sourceInfo) && sourceInfo.kind == FileInfoKind::Directory;
}

} // namespace

const char* ProjectErrorName(ProjectErrorCode code) {
    switch (code) {
    case ProjectErrorCode::None: return "none";
    case ProjectErrorCode::NullInput: return "null_input";
    case ProjectErrorCode::ProjectFileTooLarge: return "project_file_too_large";
    case ProjectErrorCode::MalformedJson: return "malformed_json";
    case ProjectErrorCode::DuplicateField: return "duplicate_field";
    case ProjectErrorCode::UnknownField: return "unknown_field";
    case ProjectErrorCode::MissingField: return "missing_field";
    case ProjectErrorCode::UnsupportedFormatVersion: return "unsupported_format_version";
    case ProjectErrorCode::InvalidProjectId: return "invalid_project_id";
    case ProjectErrorCode::InvalidDisplayName: return "invalid_display_name";
    case ProjectErrorCode::InvalidProjectKind: return "invalid_project_kind";
    case ProjectErrorCode::InvalidRelativePath: return "invalid_relative_path";
    case ProjectErrorCode::InvalidEntryPoint: return "invalid_entry_point";
    case ProjectErrorCode::InvalidAbi: return "invalid_abi";
    case ProjectErrorCode::InvalidArchitecture: return "invalid_architecture";
    case ProjectErrorCode::UnknownTargetProfile: return "unknown_target_profile";
    case ProjectErrorCode::InvalidTargetProfile: return "invalid_target_profile";
    case ProjectErrorCode::InvalidOutputName: return "invalid_output_name";
    case ProjectErrorCode::InvalidFolderName: return "invalid_folder_name";
    case ProjectErrorCode::InvalidParentPath: return "invalid_parent_path";
    case ProjectErrorCode::UnsavedChanges: return "unsaved_changes";
    case ProjectErrorCode::DestinationExists: return "destination_exists";
    case ProjectErrorCode::ParentNotFound: return "parent_not_found";
    case ProjectErrorCode::ParentNotDirectory: return "parent_not_directory";
    case ProjectErrorCode::DirectoryCreateFailed: return "directory_create_failed";
    case ProjectErrorCode::FileWriteFailed: return "file_write_failed";
    case ProjectErrorCode::FileReadFailed: return "file_read_failed";
    case ProjectErrorCode::RequiredFileMissing: return "required_file_missing";
    case ProjectErrorCode::ManifestMalformed: return "manifest_malformed";
    case ProjectErrorCode::ManifestIdentityMismatch: return "manifest_identity_mismatch";
    case ProjectErrorCode::ProjectIdCollision: return "project_id_collision";
    case ProjectErrorCode::RollbackFailed: return "rollback_failed";
    default: return "unknown";
    }
}

bool IsSupportedProjectKind(ProjectKind kind) { return kind == ProjectKind::NativeGuiApplication; }

bool ValidateProjectDisplayName(const char* value) {
    uint32_t length = lengthOf(value, kMaxProjectDisplayNameBytes, nullptr);
    if (!value || length == 0 || length >= kMaxProjectDisplayNameBytes) return false;
    if (value[0] == ' ' || value[length - 1] == ' ') return false;
    for (uint32_t i = 0; i < length; ++i) {
        unsigned char c = static_cast<unsigned char>(value[i]);
        if (c < 0x20 || value[i] == '/' || value[i] == static_cast<char>(92)) return false;
    }
    return true;
}

bool ValidateProjectId(const char* value) {
    uint32_t length = lengthOf(value, kMaxProjectIdBytes, nullptr);
    if (!value || length < 3 || length >= kMaxProjectIdBytes || value[0] == '.' || value[length - 1] == '.') return false;
    if (equalText(value, "com.guidexos") || (length > 12 && value[0] == 'c' && value[1] == 'o' && value[2] == 'm' && value[3] == '.' && value[4] == 'g' && value[5] == 'u' && value[6] == 'i' && value[7] == 'd' && value[8] == 'e' && value[9] == 'x' && value[10] == 'o' && value[11] == 's' && value[12] == '.')) return false;
    uint32_t segmentLength = 0;
    uint32_t segments = 0;
    for (uint32_t i = 0; i <= length; ++i) {
        if (i < length && value[i] != '.') {
            char c = value[i];
            if (segmentLength == 0) {
                if (!isAsciiLowerLetter(c)) return false;
            } else if (!(isAsciiLowerLetter(c) || isAsciiDigit(c) || c == '-')) return false;
            ++segmentLength;
            continue;
        }
        if (segmentLength == 0 || value[i - 1] == '-') return false;
        ++segments;
        segmentLength = 0;
    }
    return segments >= 2;
}

bool ValidateProjectFolderName(const char* value) {
    uint32_t length = lengthOf(value, kMaxNameBytes, nullptr);
    if (!value || length == 0 || length >= kMaxNameBytes || value[0] == '.' || value[length - 1] == '.' || value[0] == ' ' || value[length - 1] == ' ') return false;
    if (equalText(value, ".") || equalText(value, "..")) return false;
    const char* reserved[] = { "con", "prn", "aux", "nul", "com1", "com2", "com3", "com4", "com5", "com6", "com7", "com8", "com9", "lpt1", "lpt2", "lpt3", "lpt4", "lpt5", "lpt6", "lpt7", "lpt8", "lpt9" };
    for (uint32_t i = 0; i < sizeof(reserved) / sizeof(reserved[0]); ++i) if (equalTextFolded(value, reserved[i])) return false;
    for (uint32_t i = 0; i < length; ++i) {
        unsigned char c = static_cast<unsigned char>(value[i]);
        if (c < 0x20 || value[i] == '/' || value[i] == static_cast<char>(92) || value[i] == ':' || value[i] == '*' || value[i] == '?' || value[i] == '"' || value[i] == '<' || value[i] == '>' || value[i] == '|') return false;
    }
    return true;
}

bool DeriveProjectFolderName(const char* displayName, char* output, uint32_t outputSize) {
    if (!output || outputSize < 2 || !displayName) return false;
    uint32_t out = 0;
    bool previousDash = false;
    for (uint32_t i = 0; displayName[i] != '\0'; ++i) {
        char c = displayName[i];
        bool acceptable = isAsciiLowerLetter(c) || isAsciiDigit(c) || c == '-' || c == '_';
        if (c >= 'A' && c <= 'Z') { c = static_cast<char>(c + ('a' - 'A')); acceptable = true; }
        if (c == ' ' || c == '.') { if (!previousDash && out > 0) { if (out + 1 >= outputSize) return false; output[out++] = '-'; previousDash = true; } continue; }
        if (!acceptable) continue;
        if (c == '-' && previousDash) continue;
        if (out + 1 >= outputSize) return false;
        output[out++] = c;
        previousDash = c == '-';
    }
    while (out > 0 && output[out - 1] == '-') --out;
    if (out == 0) return false;
    output[out] = '\0';
    return ValidateProjectFolderName(output);
}

bool DeriveProjectOutputName(const char* folderName, char* output, uint32_t outputSize) {
    if (!output || outputSize < 2 || !folderName) return false;
    uint32_t out = 0;
    for (uint32_t i = 0; folderName[i] != '\0'; ++i) {
        char c = folderName[i];
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + ('a' - 'A'));
        if (!(isAsciiLowerLetter(c) || isAsciiDigit(c) || c == '-' || c == '_' || c == '.')) c = '-';
        if (out + 1 >= outputSize) return false;
        output[out++] = c;
    }
    const char suffix[] = "-guidexos";
    for (uint32_t i = 0; suffix[i] != '\0'; ++i) { if (out + 1 >= outputSize) return false; output[out++] = suffix[i]; }
    output[out] = '\0';
    return isSafeOutputName(output);
}

bool ValidateProjectMetadata(const Project& project, ProjectErrorCode* error) {
    if (error) *error = ProjectErrorCode::None;
    ProjectErrorCode local = ProjectErrorCode::None;
    if (project.formatVersion != 1) local = ProjectErrorCode::UnsupportedFormatVersion;
    else if (!ValidateProjectId(project.projectId)) local = ProjectErrorCode::InvalidProjectId;
    else if (!ValidateProjectDisplayName(project.displayName)) local = ProjectErrorCode::InvalidDisplayName;
    else if (!IsSupportedProjectKind(project.kind)) local = ProjectErrorCode::InvalidProjectKind;
    else {
        char normalizedSource[kMaxProjectPathBytes] = {};
        char normalizedManifest[kMaxProjectPathBytes] = {};
        if (!isSafeRelativePath(project.sourceRoot, normalizedSource, sizeof(normalizedSource)) ||
            !isSafeRelativePath(project.manifestPath, normalizedManifest, sizeof(normalizedManifest)) ||
            !equalText(project.sourceRoot, normalizedSource) || !equalText(project.manifestPath, normalizedManifest)) local = ProjectErrorCode::InvalidRelativePath;
        if (local == ProjectErrorCode::None && !equalText(project.targetProfileId, InitialTargetProfile().id)) local = ProjectErrorCode::UnknownTargetProfile;
        else if (local == ProjectErrorCode::None && !equalText(project.abi, kAbi)) local = ProjectErrorCode::InvalidAbi;
        else if (local == ProjectErrorCode::None && !equalText(project.architecture, kArchitecture)) local = ProjectErrorCode::InvalidArchitecture;
        else if (local == ProjectErrorCode::None && !isSafeOutputName(project.outputName)) local = ProjectErrorCode::InvalidOutputName;
        else if (local == ProjectErrorCode::None) {
            uint32_t entryLength = lengthOf(project.entryPoint, sizeof(project.entryPoint), nullptr);
            if (entryLength == 0 || entryLength >= sizeof(project.entryPoint) || !(isAsciiLowerLetter(project.entryPoint[0]) || project.entryPoint[0] == '_' || (project.entryPoint[0] >= 'A' && project.entryPoint[0] <= 'Z'))) local = ProjectErrorCode::InvalidEntryPoint;
            for (uint32_t i = 1; local == ProjectErrorCode::None && i < entryLength; ++i) {
                char c = project.entryPoint[i];
                if (!(isAsciiLowerLetter(c) || (c >= 'A' && c <= 'Z') || isAsciiDigit(c) || c == '_')) local = ProjectErrorCode::InvalidEntryPoint;
            }
        }
    }
    if (error) *error = local;
    return local == ProjectErrorCode::None;
}

bool ParseProjectMetadata(const char* bytes, uint32_t length, Project* output, ProjectErrorCode* error) {
    if (error) *error = ProjectErrorCode::None;
    if (!bytes || !output) { if (error) *error = ProjectErrorCode::NullInput; return false; }
    if (length > kMaxProjectFileBytes) { if (error) *error = ProjectErrorCode::ProjectFileTooLarge; return false; }
    Project project;
    initializeProject(&project);
    JsonCursor cursor = { bytes, length, 0, ProjectErrorCode::MalformedJson };
    if (!parseProjectObject(cursor, &project)) { if (error) *error = cursor.error; return false; }
    ProjectErrorCode validation = ProjectErrorCode::None;
    if (!ValidateProjectMetadata(project, &validation)) { if (error) *error = validation; return false; }
    project.valid = true;
    project.validationState = ProjectValidationState::Valid;
    project.error = ProjectErrorCode::None;
    *output = project;
    return true;
}

bool SerializeProjectMetadata(const Project& project, char* output, uint32_t outputSize, uint32_t* outBytes, ProjectErrorCode* error) {
    if (error) *error = ProjectErrorCode::None;
    if (!output || outputSize == 0) { if (error) *error = ProjectErrorCode::NullInput; return false; }
    ProjectErrorCode validation = ProjectErrorCode::None;
    if (!ValidateProjectMetadata(project, &validation)) { if (error) *error = validation; return false; }
    uint32_t length = 0;
    bool first = true;
    output[0] = '\0';
    if (!appendText(output, outputSize, length, "{\n") || !appendProjectNumberField(output, outputSize, length, "formatVersion", project.formatVersion, first) ||
        !appendProjectField(output, outputSize, length, "projectId", project.projectId, first) || !appendProjectField(output, outputSize, length, "displayName", project.displayName, first) ||
        !appendProjectField(output, outputSize, length, "projectKind", kNativeGuiKind, first) || !appendProjectField(output, outputSize, length, "sourceRoot", project.sourceRoot, first) ||
        !appendProjectField(output, outputSize, length, "applicationManifest", project.manifestPath, first) || !appendProjectField(output, outputSize, length, "defaultTargetProfile", project.targetProfileId, first) ||
        !appendProjectField(output, outputSize, length, "entryPoint", project.entryPoint, first) || !appendProjectField(output, outputSize, length, "abi", project.abi, first) ||
        !appendProjectField(output, outputSize, length, "architecture", project.architecture, first) || !appendProjectField(output, outputSize, length, "outputName", project.outputName, first) ||
        !appendText(output, outputSize, length, "\n}\n")) { if (error) *error = ProjectErrorCode::ProjectFileTooLarge; return false; }
    if (outBytes) *outBytes = length;
    return true;
}

bool ValidateProjectCreateRequest(const ProjectCreateRequest& request, ProjectErrorCode* error) {
    if (error) *error = ProjectErrorCode::None;
    char normalized[kMaxPathBytes] = {};
    char folder[kMaxNameBytes] = {};
    ProjectErrorCode local = ProjectErrorCode::None;
    if (!IsSupportedProjectKind(request.kind)) local = ProjectErrorCode::InvalidProjectKind;
    else if (!ValidateProjectDisplayName(request.displayName)) local = ProjectErrorCode::InvalidDisplayName;
    else if (!ValidateProjectId(request.projectId)) local = ProjectErrorCode::InvalidProjectId;
    else if (!isAbsolutePath(request.parentPath) || PathContainsTraversal(request.parentPath) || !NormalizePath(request.parentPath, normalized, sizeof(normalized))) local = ProjectErrorCode::InvalidParentPath;
    else if (request.folderName[0] != '\0' && !ValidateProjectFolderName(request.folderName)) local = ProjectErrorCode::InvalidFolderName;
    else if (request.folderName[0] == '\0' && !DeriveProjectFolderName(request.displayName, folder, sizeof(folder))) local = ProjectErrorCode::InvalidFolderName;
    if (error) *error = local;
    return local == ProjectErrorCode::None;
}

bool CreateNativeGuiProject(const ProjectFileSystem& fileSystem, const ProjectCreateRequest& request, ProjectOperationResult* result) {
    if (!result) return false;
    *result = ProjectOperationResult();
    initializeProject(&result->project);
    ProjectErrorCode validation = ProjectErrorCode::None;
    if (!fileSystem.stat || !fileSystem.list || !fileSystem.write || !fileSystem.createDirectory || !fileSystem.removePath) { setResult(result, ProjectErrorCode::NullInput); return false; }
    if (!ValidateProjectCreateRequest(request, &validation)) { setResult(result, validation); return false; }
    char parent[kMaxPathBytes] = {};
    if (!NormalizePath(request.parentPath, parent, sizeof(parent))) { setResult(result, ProjectErrorCode::InvalidParentPath); return false; }
    FileInfo parentInfo = {};
    if (!fileSystem.stat(fileSystem.userData, parent, &parentInfo)) { setResult(result, ProjectErrorCode::ParentNotFound); return false; }
    if (parentInfo.kind != FileInfoKind::Directory) { setResult(result, ProjectErrorCode::ParentNotDirectory); return false; }
    char folder[kMaxNameBytes] = {};
    if (request.folderName[0] != '\0') copyText(folder, sizeof(folder), request.folderName);
    else DeriveProjectFolderName(request.displayName, folder, sizeof(folder));
    char root[kMaxPathBytes] = {};
    if (!JoinWorkspacePath(parent, folder, root, sizeof(root))) { setResult(result, ProjectErrorCode::InvalidFolderName); return false; }
    FileInfo destination = {};
    bool rootExisted = fileSystem.stat(fileSystem.userData, root, &destination);
    if (rootExisted) {
        if (destination.kind != FileInfoKind::Directory) { setResult(result, ProjectErrorCode::DestinationExists); return false; }
        FileListEntry entries[1] = {};
        uint32_t count = 0;
        bool truncated = false;
        if (!fileSystem.list(fileSystem.userData, root, entries, 1, &count, &truncated) || count != 0 || truncated) { setResult(result, ProjectErrorCode::DestinationExists); return false; }
    }

    Project project;
    initializeProject(&project);
    project.loaded = true;
    project.valid = true;
    project.formatVersion = 1;
    project.kind = ProjectKind::NativeGuiApplication;
    copyText(project.projectId, sizeof(project.projectId), request.projectId);
    copyText(project.displayName, sizeof(project.displayName), request.displayName);
    copyText(project.rootPath, sizeof(project.rootPath), root);
    copyText(project.sourceRoot, sizeof(project.sourceRoot), "src");
    copyText(project.manifestPath, sizeof(project.manifestPath), "app/app.json");
    copyText(project.targetProfileId, sizeof(project.targetProfileId), kTargetId);
    copyText(project.entryPoint, sizeof(project.entryPoint), "gx_main");
    copyText(project.abi, sizeof(project.abi), kAbi);
    copyText(project.architecture, sizeof(project.architecture), kArchitecture);
    DeriveProjectOutputName(folder, project.outputName, sizeof(project.outputName));
    project.loadState = ProjectLoadState::Loaded;
    project.validationState = ProjectValidationState::Valid;
    if (!ValidateProjectMetadata(project, &validation)) { setResult(result, validation); return false; }

    char createdFiles[16][kMaxPathBytes] = {};
    char createdDirectories[16][kMaxPathBytes] = {};
    uint32_t fileCount = 0;
    uint32_t directoryCount = 0;
    auto fail = [&](ProjectErrorCode error) {
        result->rollbackAttempted = directoryCount > 0 || fileCount > 0;
        result->rollbackSucceeded = result->rollbackAttempted ? rollback(fileSystem, createdFiles, fileCount, createdDirectories, directoryCount) : true;
        setResult(result, result->rollbackSucceeded ? error : ProjectErrorCode::RollbackFailed);
        return false;
    };
    if (!rootExisted) {
        if (!fileSystem.createDirectory(fileSystem.userData, root) || !addTrackedPath(createdDirectories, directoryCount, root)) return fail(ProjectErrorCode::DirectoryCreateFailed);
    }
    char appDirectory[kMaxPathBytes] = {};
    char sourceDirectory[kMaxPathBytes] = {};
    if (!joinProjectPath(root, "app", appDirectory, sizeof(appDirectory)) || !joinProjectPath(root, "src", sourceDirectory, sizeof(sourceDirectory))) return fail(ProjectErrorCode::InvalidRelativePath);
    if (!fileSystem.createDirectory(fileSystem.userData, appDirectory) || !addTrackedPath(createdDirectories, directoryCount, appDirectory)) return fail(ProjectErrorCode::DirectoryCreateFailed);
    if (!fileSystem.createDirectory(fileSystem.userData, sourceDirectory) || !addTrackedPath(createdDirectories, directoryCount, sourceDirectory)) return fail(ProjectErrorCode::DirectoryCreateFailed);

    static char content[kMaxProjectFileBytes] = {};
    uint32_t bytes = 0;
    ProjectErrorCode contentError = ProjectErrorCode::None;
    char path[kMaxPathBytes] = {};
    if (!joinProjectPath(root, kProjectFileName, path, sizeof(path)) || !SerializeProjectMetadata(project, content, sizeof(content), &bytes, &contentError) || !writeTrackedFile(fileSystem, path, content, bytes, createdFiles, fileCount)) return fail(contentError == ProjectErrorCode::None ? ProjectErrorCode::FileWriteFailed : contentError);
    if (!joinProjectPath(root, "CMakeLists.txt", path, sizeof(path)) || !generateCMake(project, content, sizeof(content), &bytes) || !writeTrackedFile(fileSystem, path, content, bytes, createdFiles, fileCount)) return fail(ProjectErrorCode::FileWriteFailed);
    if (!joinProjectPath(root, "build.ps1", path, sizeof(path)) || !generateBuildScript(project, content, sizeof(content), &bytes)) return fail(ProjectErrorCode::FileWriteFailed);
    repairGeneratedPackagePath(content, sizeof(content), bytes, project.outputName);
    if (!writeTrackedFile(fileSystem, path, content, bytes, createdFiles, fileCount)) return fail(ProjectErrorCode::FileWriteFailed);
    if (!joinProjectPath(root, "README.md", path, sizeof(path)) || !generateReadme(project, content, sizeof(content), &bytes) || !writeTrackedFile(fileSystem, path, content, bytes, createdFiles, fileCount)) return fail(ProjectErrorCode::FileWriteFailed);
    if (!joinProjectPath(root, "app/app.json", path, sizeof(path)) || !generateManifest(project, content, sizeof(content), &bytes) || !writeTrackedFile(fileSystem, path, content, bytes, createdFiles, fileCount)) return fail(ProjectErrorCode::FileWriteFailed);
    if (!joinProjectPath(root, "src/main.cpp", path, sizeof(path)) || !generateMain(project, content, sizeof(content), &bytes) || !writeTrackedFile(fileSystem, path, content, bytes, createdFiles, fileCount)) return fail(ProjectErrorCode::FileWriteFailed);
    if (!joinProjectPath(root, "src/freestanding_memory.cpp", path, sizeof(path)) || !generateMemory(content, sizeof(content), &bytes) || !writeTrackedFile(fileSystem, path, content, bytes, createdFiles, fileCount)) return fail(ProjectErrorCode::FileWriteFailed);
    if (!verifyRequiredFiles(fileSystem, project)) return fail(ProjectErrorCode::RequiredFileMissing);
    if (!LoadProject(fileSystem, root, result)) return fail(result->error == ProjectErrorCode::None ? ProjectErrorCode::ManifestMalformed : result->error);
    result->success = true;
    result->rollbackAttempted = false;
    result->rollbackSucceeded = true;
    result->error = ProjectErrorCode::None;
    return true;
}

bool LoadProject(const ProjectFileSystem& fileSystem, const char* rootOrMetadataPath, ProjectOperationResult* result) {
    if (!result) return false;
    *result = ProjectOperationResult();
    initializeProject(&result->project);
    if (!fileSystem.stat || !fileSystem.read || !rootOrMetadataPath) { setResult(result, ProjectErrorCode::NullInput); return false; }
    char normalized[kMaxPathBytes] = {};
    if (PathContainsTraversal(rootOrMetadataPath) || !NormalizePath(rootOrMetadataPath, normalized, sizeof(normalized))) { setResult(result, ProjectErrorCode::InvalidParentPath); return false; }
    FileInfo inputInfo = {};
    if (!fileSystem.stat(fileSystem.userData, normalized, &inputInfo)) { setResult(result, ProjectErrorCode::ParentNotFound); return false; }
    char root[kMaxPathBytes] = {};
    if (inputInfo.kind == FileInfoKind::Directory) copyText(root, sizeof(root), normalized);
    else if (inputInfo.kind == FileInfoKind::RegularFile && equalTextFolded(BaseName(normalized), kProjectFileName)) {
        if (!parentPath(normalized, root, sizeof(root))) { setResult(result, ProjectErrorCode::InvalidParentPath); return false; }
    } else { setResult(result, ProjectErrorCode::RequiredFileMissing); return false; }
    char metadataPath[kMaxPathBytes] = {};
    if (!joinProjectPath(root, kProjectFileName, metadataPath, sizeof(metadataPath))) { setResult(result, ProjectErrorCode::InvalidRelativePath); return false; }
    FileInfo metadataInfo = {};
    if (!fileSystem.stat(fileSystem.userData, metadataPath, &metadataInfo) || metadataInfo.kind != FileInfoKind::RegularFile) { setResult(result, ProjectErrorCode::RequiredFileMissing); return false; }
    if (metadataInfo.size > kMaxProjectFileBytes) { setResult(result, ProjectErrorCode::ProjectFileTooLarge); return false; }
    static char metadata[kMaxProjectFileBytes + 1] = {};
    uint32_t bytes = 0;
    if (!fileSystem.read(fileSystem.userData, metadataPath, metadata, kMaxProjectFileBytes, &bytes) || bytes > kMaxProjectFileBytes) { setResult(result, ProjectErrorCode::FileReadFailed); return false; }
    Project project;
    ProjectErrorCode parseError = ProjectErrorCode::None;
    if (!ParseProjectMetadata(metadata, bytes, &project, &parseError)) { setResult(result, parseError); return false; }
    copyText(project.rootPath, sizeof(project.rootPath), root);
    project.loaded = true;
    project.loadState = ProjectLoadState::Loaded;
    char manifestPath[kMaxPathBytes] = {};
    if (!joinProjectPath(root, project.manifestPath, manifestPath, sizeof(manifestPath))) { setResult(result, ProjectErrorCode::InvalidRelativePath); return false; }
    if (!verifyRequiredFiles(fileSystem, project)) { setResult(result, ProjectErrorCode::RequiredFileMissing); return false; }
    FileInfo manifestFileInfo = {};
    if (!fileSystem.stat(fileSystem.userData, manifestPath, &manifestFileInfo) || manifestFileInfo.size > kMaxProjectFileBytes) { setResult(result, ProjectErrorCode::RequiredFileMissing); return false; }
    static char manifestBytes[kMaxProjectFileBytes + 1] = {};
    uint32_t manifestSize = 0;
    if (!fileSystem.read(fileSystem.userData, manifestPath, manifestBytes, kMaxProjectFileBytes, &manifestSize)) { setResult(result, ProjectErrorCode::FileReadFailed); return false; }
    ManifestInfo manifest;
    if (!parseManifest(manifestBytes, manifestSize, &manifest)) { setResult(result, ProjectErrorCode::ManifestMalformed); return false; }
    if (!validateManifestAgainstProject(manifest, project)) { setResult(result, ProjectErrorCode::ManifestIdentityMismatch); return false; }
    project.error = ProjectErrorCode::None;
    project.valid = true;
    project.validationState = ProjectValidationState::Valid;
    result->success = true;
    result->error = ProjectErrorCode::None;
    result->project = project;
    return true;
}

} // namespace developer_studio
} // namespace guidexos
