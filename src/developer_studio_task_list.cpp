#include "developer_studio_task_list.h"

namespace guidexos {
namespace developer_studio {
namespace {

static uint32_t textLength(const char* text, uint32_t capacity) {
    if (!text) return 0;
    uint32_t length = 0;
    while (length < capacity && text[length] != '\0') ++length;
    return length;
}

static bool copyText(char* output, uint32_t outputSize, const char* input) {
    if (!output || outputSize == 0 || !input) return false;
    uint32_t i = 0;
    while (i + 1u < outputSize && input[i] != '\0') {
        output[i] = input[i];
        ++i;
    }
    if (input[i] != '\0') {
        output[0] = '\0';
        return false;
    }
    output[i] = '\0';
    return true;
}

static char lowerAscii(char value) {
    return value >= 'A' && value <= 'Z' ? static_cast<char>(value + ('a' - 'A')) : value;
}

static bool isIdentifierByte(char value) {
    return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') ||
        (value >= '0' && value <= '9') || value == '_';
}

static bool isCppSourcePath(const char* path) {
    if (!path || path[0] == '\0') return false;
    const char* base = path;
    for (const char* cursor = path; *cursor; ++cursor)
        if (*cursor == '/' || *cursor == static_cast<char>(92)) base = cursor + 1;
    const char* dot = nullptr;
    for (const char* cursor = base; *cursor; ++cursor) if (*cursor == '.') dot = cursor;
    if (!dot) return false;
    static const char* const extensions[] = {
        ".c", ".h", ".cc", ".cpp", ".cxx", ".hh", ".hpp", ".hxx"
    };
    for (uint32_t extensionIndex = 0;
         extensionIndex < sizeof(extensions) / sizeof(extensions[0]); ++extensionIndex) {
        const char* extension = extensions[extensionIndex];
        uint32_t i = 0;
        while (extension[i] && dot[i] && lowerAscii(dot[i]) == extension[i]) ++i;
        if (extension[i] == '\0' && dot[i] == '\0') return true;
    }
    return false;
}

static bool validRelativePath(const char* path) {
    if (!path || path[0] == '\0' || path[0] == '/' || path[0] == static_cast<char>(92) ||
        path[1] == ':') return false;
    uint32_t componentStart = 0;
    const uint32_t length = textLength(path, kMaxPathBytes);
    if (length == 0 || length >= kMaxPathBytes) return false;
    for (uint32_t i = 0; i <= length; ++i) {
        if (i < length) {
            const unsigned char value = static_cast<unsigned char>(path[i]);
            if (value < 0x20u || path[i] == ':' || path[i] == '\\') return false;
            if (path[i] != '/') continue;
        }
        const uint32_t componentLength = i - componentStart;
        if (componentLength == 0 ||
            (componentLength == 1 && path[componentStart] == '.') ||
            (componentLength == 2 && path[componentStart] == '.' && path[componentStart + 1] == '.'))
            return false;
        componentStart = i + 1u;
    }
    return true;
}

static bool tokenMatches(const char* data, uint32_t length, uint32_t start,
                         const char* token, uint32_t tokenLength) {
    if (!data || !token || start > length || tokenLength > length - start) return false;
    if (start > 0 && isIdentifierByte(data[start - 1u])) return false;
    if (start + tokenLength < length && isIdentifierByte(data[start + tokenLength])) return false;
    for (uint32_t i = 0; i < tokenLength; ++i)
        if (lowerAscii(data[start + i]) != token[i]) return false;
    return true;
}

static void appendDescription(ProjectTask* task, const char* data,
                              uint32_t start, uint32_t end) {
    if (!task || !data || start > end) return;
    const uint32_t maximum = kProjectTaskListMaxDescriptionBytes;
    const bool clipped = end - start > maximum;
    uint32_t copyLength = clipped ? maximum : end - start;
    if (clipped && copyLength >= 3u) copyLength -= 3u;
    uint32_t output = 0;
    for (uint32_t i = 0; i < copyLength && output < maximum; ++i) {
        const unsigned char value = static_cast<unsigned char>(data[start + i]);
        task->description[output++] = value >= 0x20u && value <= 0x7Eu ?
            static_cast<char>(value) : ' ';
    }
    if (clipped && output + 3u <= maximum) {
        task->description[output++] = '.';
        task->description[output++] = '.';
        task->description[output++] = '.';
    }
    task->description[output] = '\0';
}

static bool addAnnotation(ProjectTaskList* taskList, const char* path,
                          const char* data, uint32_t tokenOffset, uint32_t tokenLength,
                          uint32_t lineEnd, uint32_t line) {
    if (taskList->count >= kProjectTaskListMaxTasks) {
        taskList->truncated = true;
        return false;
    }
    ProjectTask& task = taskList->tasks[taskList->count];
    task.annotation = tokenLength == 4u ? ProjectTaskAnnotation::Todo : ProjectTaskAnnotation::Fixme;
    task.line = line;
    if (!copyText(task.relativePath, sizeof(task.relativePath), path)) {
        taskList->truncated = true;
        return false;
    }
    appendDescription(&task, data, tokenOffset, lineEnd);
    ++taskList->count;
    if (task.annotation == ProjectTaskAnnotation::Todo) ++taskList->todoCount;
    else ++taskList->fixmeCount;
    return true;
}

static bool processCommentSegment(ProjectTaskList* taskList, const char* path,
                                  const char* data, uint32_t length,
                                  uint32_t segmentStart, uint32_t segmentEnd,
                                  uint32_t line) {
    if (!taskList || !data) return false;
    for (uint32_t i = segmentStart; i < segmentEnd; ++i) {
        if (tokenMatches(data, length, i, "todo", 4u)) {
            if (!addAnnotation(taskList, path, data, i, 4u, segmentEnd, line)) return false;
            i += 3u;
        } else if (tokenMatches(data, length, i, "fixme", 5u)) {
            if (!addAnnotation(taskList, path, data, i, 5u, segmentEnd, line)) return false;
            i += 4u;
        }
    }
    return true;
}

static bool isRawDelimiterByte(unsigned char value) {
    return value > 0x20u && value != '(' && value != ')' && value != '\\' && value != 0x7Fu;
}

static bool skipRawString(const char* data, uint32_t length, uint32_t* cursor,
                          uint32_t* line, uint32_t* lineStart) {
    if (!data || !cursor || !line || !lineStart || *cursor + 1u >= length ||
        data[*cursor] != 'R' || data[*cursor + 1u] != '"') return false;
    const uint32_t delimiterStart = *cursor + 2u;
    uint32_t open = delimiterStart;
    while (open < length && data[open] != '(' && open - delimiterStart <= 16u &&
           isRawDelimiterByte(static_cast<unsigned char>(data[open]))) ++open;
    if (open >= length || data[open] != '(' || open - delimiterStart > 16u) return false;
    const uint32_t delimiterLength = open - delimiterStart;
    uint32_t i = open + 1u;
    while (i < length) {
        if (data[i] == ')' && delimiterLength + 1u < length - i) {
            uint32_t j = 0;
            while (j < delimiterLength && data[i + 1u + j] == data[delimiterStart + j]) ++j;
            if (j == delimiterLength && data[i + 1u + delimiterLength] == '"') {
                *cursor = i + delimiterLength + 2u;
                return true;
            }
        }
        if (data[i] == '\n') {
            ++(*line);
            *lineStart = i + 1u;
        }
        ++i;
    }
    *cursor = length;
    return true;
}

static void skipQuotedLiteral(const char* data, uint32_t length, uint32_t* cursor,
                              uint32_t* line, uint32_t* lineStart) {
    const char quote = data[*cursor];
    ++(*cursor);
    while (*cursor < length) {
        const char value = data[*cursor];
        if (value == quote) {
            ++(*cursor);
            return;
        }
        if (value == '\n') return;
        if (value == '\\') {
            if (*cursor + 1u < length && data[*cursor + 1u] == '\n') {
                ++(*line);
                *lineStart = *cursor + 2u;
                *cursor += 2u;
            } else if (*cursor + 2u < length && data[*cursor + 1u] == '\r' &&
                       data[*cursor + 2u] == '\n') {
                ++(*line);
                *lineStart = *cursor + 3u;
                *cursor += 3u;
            } else if (*cursor + 1u < length) {
                *cursor += 2u;
            } else {
                ++(*cursor);
            }
        } else {
            ++(*cursor);
        }
    }
}

static bool scanCppComments(ProjectTaskList* taskList, const char* path,
                            const char* data, uint32_t length) {
    uint32_t i = 0;
    uint32_t line = 1;
    uint32_t lineStart = 0;
    while (i < length) {
        if (data[i] == '\n') {
            ++line;
            lineStart = i + 1u;
            ++i;
            continue;
        }
        if (data[i] == 'R' && i + 1u < length && data[i + 1u] == '"' &&
            skipRawString(data, length, &i, &line, &lineStart)) continue;
        if (data[i] == '"' || data[i] == '\'') {
            skipQuotedLiteral(data, length, &i, &line, &lineStart);
            continue;
        }
        if (data[i] != '/' || i + 1u >= length) {
            ++i;
            continue;
        }
        if (data[i + 1u] == '/') {
            uint32_t cursor = i + 2u;
            uint32_t segmentStart = cursor;
            while (cursor < length) {
                while (cursor < length && data[cursor] != '\n') ++cursor;
                uint32_t segmentEnd = cursor;
                if (segmentEnd > segmentStart && data[segmentEnd - 1u] == '\r') --segmentEnd;
                if (!processCommentSegment(taskList, path, data, length, segmentStart, segmentEnd, line))
                    return false;
                if (cursor >= length) {
                    i = length;
                    break;
                }
                const bool continued = segmentEnd > segmentStart && data[segmentEnd - 1u] == '\\';
                if (!continued) {
                    i = cursor;
                    break;
                }
                ++line;
                lineStart = cursor + 1u;
                segmentStart = lineStart;
                ++cursor;
            }
            continue;
        }
        if (data[i + 1u] == '*') {
            uint32_t cursor = i + 2u;
            uint32_t segmentStart = cursor;
            bool closed = false;
            while (cursor < length) {
                if (data[cursor] == '*' && cursor + 1u < length && data[cursor + 1u] == '/') {
                    uint32_t segmentEnd = cursor;
                    if (!processCommentSegment(taskList, path, data, length, segmentStart, segmentEnd,
                                               line)) return false;
                    i = cursor + 2u;
                    closed = true;
                    break;
                }
                if (data[cursor] == '\n') {
                    uint32_t segmentEnd = cursor;
                    if (segmentEnd > segmentStart && data[segmentEnd - 1u] == '\r') --segmentEnd;
                    if (!processCommentSegment(taskList, path, data, length, segmentStart, segmentEnd,
                                               line)) return false;
                    ++line;
                    lineStart = cursor + 1u;
                    segmentStart = lineStart;
                }
                ++cursor;
            }
            if (!closed) {
                if (!processCommentSegment(taskList, path, data, length, segmentStart, length,
                                           line)) return false;
                i = length;
            }
            continue;
        }
        ++i;
    }
    return true;
}

static bool pathComesBefore(const ProjectTask& left, const ProjectTask& right) {
    uint32_t i = 0;
    while (left.relativePath[i] && right.relativePath[i] &&
           lowerAscii(left.relativePath[i]) == lowerAscii(right.relativePath[i])) ++i;
    if (left.relativePath[i] != right.relativePath[i])
        return lowerAscii(left.relativePath[i]) < lowerAscii(right.relativePath[i]);
    if (left.relativePath[i] != '\0') return left.relativePath[i] < right.relativePath[i];
    if (left.line != right.line) return left.line < right.line;
    if (left.annotation != right.annotation)
        return static_cast<uint8_t>(left.annotation) < static_cast<uint8_t>(right.annotation);
    return false;
}

} // namespace

void ProjectTaskListInit(ProjectTaskList* taskList) {
    if (!taskList) return;
    taskList->count = 0;
    taskList->todoCount = 0;
    taskList->fixmeCount = 0;
    taskList->filesScanned = 0;
    taskList->truncated = false;
    for (uint32_t i = 0; i < kProjectTaskListMaxTasks; ++i) {
        taskList->tasks[i].relativePath[0] = '\0';
        taskList->tasks[i].description[0] = '\0';
    }
}

bool ProjectTaskListVisitFile(void* userData, const ProjectSearchScanFile* file) {
    ProjectTaskList* taskList = static_cast<ProjectTaskList*>(userData);
    if (!taskList) return false;
    if (!file || !file->data || !validRelativePath(file->relativePath) ||
        file->length > kProjectSearchMaxFileBytes) {
        taskList->truncated = true;
        return false;
    }
    if (!isCppSourcePath(file->relativePath)) return true;
    if (taskList->filesScanned != 0xFFFFFFFFu) ++taskList->filesScanned;
    return scanCppComments(taskList, file->relativePath, file->data, file->length);
}

void ProjectTaskListSort(ProjectTaskList* taskList) {
    if (!taskList) return;
    for (uint32_t i = 1; i < taskList->count; ++i) {
        ProjectTask saved = taskList->tasks[i];
        uint32_t j = i;
        while (j > 0 && pathComesBefore(saved, taskList->tasks[j - 1u])) {
            taskList->tasks[j] = taskList->tasks[j - 1u];
            --j;
        }
        taskList->tasks[j] = saved;
    }
}

const char* ProjectTaskAnnotationName(ProjectTaskAnnotation annotation) {
    return annotation == ProjectTaskAnnotation::Todo ? "TODO" : "FIXME";
}

} // namespace developer_studio
} // namespace guidexos
