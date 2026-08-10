#include "developer_studio_debug_symbols.h"

namespace guidexos {
namespace developer_studio {
namespace {

static const uint16_t kElfTypeExec = 2;
static const uint16_t kElfMachineAmd64 = 62;
static const uint32_t kSectionTypeString = 3;
static const uint32_t kSectionTypeNoBits = 8;
static const uint16_t kDwarfVersion4 = 4;
static const uint16_t kDwarfVersion5 = 5;
static const uint16_t kNoSourceFile = 0xffffu;

static uint32_t textLength(const char* value, uint32_t capacity) {
    if (!value) return 0;
    uint32_t length = 0;
    while (length < capacity && value[length] != '\0') ++length;
    return length;
}

static bool copyText(char* output, uint32_t outputSize, const char* input) {
    if (!output || outputSize == 0 || !input) return false;
    const uint32_t length = textLength(input, outputSize);
    if (length >= outputSize) { output[0] = '\0'; return false; }
    for (uint32_t i = 0; i < length; ++i) output[i] = input[i];
    output[length] = '\0';
    return true;
}

static bool equalText(const char* left, const char* right, bool insensitive) {
    if (!left || !right) return false;
    for (uint32_t i = 0;; ++i) {
        char a = left[i];
        char b = right[i];
        if (insensitive) {
            if (a >= 'A' && a <= 'Z') a = static_cast<char>(a + ('a' - 'A'));
            if (b >= 'A' && b <= 'Z') b = static_cast<char>(b + ('a' - 'A'));
        }
        if (a != b) return false;
        if (a == '\0') return true;
    }
}

static bool isSlash(char value) { return value == '/' || value == static_cast<char>(92); }

static bool checkedRange(uint64_t offset, uint64_t size, uint64_t total) {
    return offset <= total && size <= total - offset;
}

static uint16_t readU16(const unsigned char* bytes, uint64_t offset) {
    return static_cast<uint16_t>(bytes[offset]) |
        static_cast<uint16_t>(static_cast<uint16_t>(bytes[offset + 1]) << 8);
}

static uint32_t readU32(const unsigned char* bytes, uint64_t offset) {
    return static_cast<uint32_t>(bytes[offset]) |
        (static_cast<uint32_t>(bytes[offset + 1]) << 8) |
        (static_cast<uint32_t>(bytes[offset + 2]) << 16) |
        (static_cast<uint32_t>(bytes[offset + 3]) << 24);
}

static uint64_t readU64(const unsigned char* bytes, uint64_t offset) {
    uint64_t value = 0;
    for (uint32_t i = 0; i < 8; ++i) value |= static_cast<uint64_t>(bytes[offset + i]) << (i * 8);
    return value;
}

struct Cursor {
    const unsigned char* bytes;
    uint64_t position;
    uint64_t end;
};

static bool canRead(const Cursor& cursor, uint64_t count) {
    return count <= cursor.end - cursor.position;
}

static bool readByte(Cursor& cursor, uint8_t* value) {
    if (!value || !canRead(cursor, 1)) return false;
    *value = cursor.bytes[cursor.position++];
    return true;
}

static bool readU16Cursor(Cursor& cursor, uint16_t* value) {
    if (!value || !canRead(cursor, 2)) return false;
    *value = readU16(cursor.bytes, cursor.position);
    cursor.position += 2;
    return true;
}

static bool readU32Cursor(Cursor& cursor, uint32_t* value) {
    if (!value || !canRead(cursor, 4)) return false;
    *value = readU32(cursor.bytes, cursor.position);
    cursor.position += 4;
    return true;
}

static bool readU64Cursor(Cursor& cursor, uint64_t* value) {
    if (!value || !canRead(cursor, 8)) return false;
    *value = readU64(cursor.bytes, cursor.position);
    cursor.position += 8;
    return true;
}

static bool readULEB(Cursor& cursor, uint64_t* value) {
    if (!value) return false;
    *value = 0;
    for (uint32_t byteCount = 0; byteCount < 10; ++byteCount) {
        uint8_t byte = 0;
        if (!readByte(cursor, &byte)) return false;
        const uint32_t shift = byteCount * 7;
        if (shift >= 64 || (shift == 63 && (byte & 0x7e) != 0) ||
            (shift < 63 && static_cast<uint64_t>(byte & 0x7f) > (UINT64_MAX >> shift))) return false;
        *value |= static_cast<uint64_t>(byte & 0x7f) << shift;
        if ((byte & 0x80) == 0) return true;
    }
    return false;
}

static bool readSLEB(Cursor& cursor, int64_t* value) {
    if (!value) return false;
    *value = 0;
    for (uint32_t byteCount = 0; byteCount < 10; ++byteCount) {
        uint8_t byte = 0;
        if (!readByte(cursor, &byte)) return false;
        const uint32_t shift = byteCount * 7;
        if (shift >= 64) return false;
        const uint64_t payload = static_cast<uint64_t>(byte & 0x7f);
        if (shift == 63 && payload > 1) return false;
        *value |= static_cast<int64_t>(payload << shift);
        if ((byte & 0x80) == 0) {
            if ((byte & 0x40) != 0 && shift + 7 < 64) *value |= static_cast<int64_t>(UINT64_MAX << (shift + 7));
            return true;
        }
    }
    return false;
}

static bool readCString(Cursor& cursor, char* output, uint32_t outputSize) {
    if (!output || outputSize == 0) return false;
    uint32_t length = 0;
    while (cursor.position < cursor.end) {
        const char value = static_cast<char>(cursor.bytes[cursor.position++]);
        if (value == '\0') { output[length] = '\0'; return true; }
        if (length + 1 >= outputSize || length + 1 >= kDebugMapperMaxStringBytes) return false;
        output[length++] = value;
    }
    return false;
}

static bool appendPath(char* output, uint32_t outputSize, const char* left, const char* right) {
    if (!output || !left || !right) return false;
    output[0] = '\0';
    uint32_t length = 0;
    for (uint32_t i = 0; left[i] != '\0'; ++i) {
        if (length + 1 >= outputSize) return false;
        output[length++] = left[i] == static_cast<char>(92) ? '/' : left[i];
    }
    if (length != 0 && output[length - 1] != '/') {
        if (length + 1 >= outputSize) return false;
        output[length++] = '/';
    }
    for (uint32_t i = 0; right[i] != '\0'; ++i) {
        if (length + 1 >= outputSize) return false;
        output[length++] = right[i] == static_cast<char>(92) ? '/' : right[i];
    }
    output[length] = '\0';
    return true;
}

static bool normalizeGeneric(const char* input, char* output, uint32_t outputSize,
                             bool rejectUnresolvedParent, bool* absolute) {
    if (!input || !output || outputSize == 0) return false;
    output[0] = '\0';
    const bool drive = input[0] != '\0' && input[1] == ':';
    const bool rooted = drive || isSlash(input[0]);
    if (absolute) *absolute = rooted;
    uint32_t offset = 0;
    uint32_t length = 0;
    if (drive) {
        if (outputSize < 3) return false;
        output[length++] = input[0];
        output[length++] = ':';
        output[length++] = '/';
        offset = 2;
        while (isSlash(input[offset])) ++offset;
    } else if (isSlash(input[0])) {
        output[length++] = '/';
        offset = 1;
        while (isSlash(input[offset])) ++offset;
    }
    uint32_t segmentStarts[128] = {};
    uint32_t segmentCount = 0;
    while (input[offset] != '\0') {
        while (isSlash(input[offset])) ++offset;
        if (input[offset] == '\0') break;
        const uint32_t start = offset;
        while (input[offset] != '\0' && !isSlash(input[offset])) ++offset;
        const uint32_t count = offset - start;
        if (count == 1 && input[start] == '.') continue;
        if (count == 2 && input[start] == '.' && input[start + 1] == '.') {
            if (segmentCount > 0) {
                length = segmentStarts[segmentCount - 1];
                --segmentCount;
                if (length == 0 && rooted) length = drive ? 3 : 1;
            } else if (rooted || rejectUnresolvedParent) {
                return false;
            } else {
                if (length != 0 && output[length - 1] != '/') output[length++] = '/';
                if (segmentCount >= sizeof(segmentStarts) / sizeof(segmentStarts[0])) return false;
                segmentStarts[segmentCount++] = length;
                if (length + 2 >= outputSize) return false;
                output[length++] = '.'; output[length++] = '.'; output[length] = '\0';
            }
            output[length] = '\0';
            continue;
        }
        if (segmentCount >= sizeof(segmentStarts) / sizeof(segmentStarts[0])) return false;
        if (length != 0 && output[length - 1] != '/') output[length++] = '/';
        segmentStarts[segmentCount++] = length;
        if (length + count >= outputSize) return false;
        for (uint32_t i = 0; i < count; ++i)
            output[length++] = input[start + i] == static_cast<char>(92) ? '/' : input[start + i];
        output[length] = '\0';
    }
    if (length == 0 && rooted) {
        output[0] = '/';
        if (drive) { output[0] = input[0]; output[1] = ':'; output[2] = '/'; output[3] = '\0'; }
    }
    return true;
}

static bool pathContained(const char* root, const char* absolute, char* relative, uint32_t size) {
    const uint32_t rootLength = textLength(root, kDebugMapperMaxPathBytes);
    const uint32_t absoluteLength = textLength(absolute, kDebugMapperMaxPathBytes);
    if (absoluteLength <= rootLength) return false;
    for (uint32_t i = 0; i < rootLength; ++i) {
        char a = root[i]; char b = absolute[i];
        if (a >= 'A' && a <= 'Z') a = static_cast<char>(a + ('a' - 'A'));
        if (b >= 'A' && b <= 'Z') b = static_cast<char>(b + ('a' - 'A'));
        if (a != b) return false;
    }
    if (absolute[rootLength] != '/') return false;
    return copyText(relative, size, absolute + rootLength + 1);
}

static bool isAbsolutePath(const char* value) {
    return value && (isSlash(value[0]) || (value[0] != '\0' && value[1] == ':'));
}

static bool normalizeForProject(const char* projectRoot, const char* rawPath,
                                char* relativePath, uint32_t relativePathSize,
                                bool* external) {
    if (external) *external = false;
    if (!projectRoot || !rawPath || !relativePath || relativePathSize == 0) return false;
    char root[kDebugMapperMaxPathBytes] = {};
    char raw[kDebugMapperMaxPathBytes] = {};
    bool rootAbsolute = false;
    bool rawAbsolute = false;
    if (!normalizeGeneric(projectRoot, root, sizeof(root), true, &rootAbsolute) || !rootAbsolute ||
        !normalizeGeneric(rawPath, raw, sizeof(raw), false, &rawAbsolute)) return false;
    char candidate[kDebugMapperMaxPathBytes] = {};
    if (rawAbsolute) copyText(candidate, sizeof(candidate), raw);
    else if (!appendPath(candidate, sizeof(candidate), root, raw)) return false;
    char normalized[kDebugMapperMaxPathBytes] = {};
    bool candidateAbsolute = false;
    if (!normalizeGeneric(candidate, normalized, sizeof(normalized), true, &candidateAbsolute) || !candidateAbsolute) return false;
    if (pathContained(root, normalized, relativePath, relativePathSize)) return true;
    if (external) *external = true;
    relativePath[0] = '\0';
    return false;
}

static bool readSectionString(const unsigned char* section, uint64_t sectionSize,
                              uint64_t offset, char* output, uint32_t outputSize) {
    if (!section || offset >= sectionSize || !output || outputSize == 0) return false;
    uint32_t length = 0;
    while (offset < sectionSize) {
        const char value = static_cast<char>(section[offset++]);
        if (value == '\0') { output[length] = '\0'; return true; }
        if (length + 1 >= outputSize || length + 1 >= kDebugMapperMaxStringBytes) return false;
        output[length++] = value;
    }
    return false;
}

struct SectionView {
    const unsigned char* data;
    uint64_t size;
};

static bool addSourceFile(DebugDwarfMapper* mapper, const char* projectRoot,
                          const char* directory, const char* fileName,
                          uint16_t* outIndex) {
    if (!mapper || !projectRoot || !fileName || !outIndex) return false;
    char combined[kDebugMapperMaxPathBytes] = {};
    if (isAbsolutePath(fileName) || !directory || directory[0] == '\0' || equalText(directory, ".", false))
        copyText(combined, sizeof(combined), fileName);
    else if (!appendPath(combined, sizeof(combined), directory, fileName)) return false;
    char relative[kDebugMapperMaxPathBytes] = {};
    bool external = false;
    if (!normalizeForProject(projectRoot, combined, relative, sizeof(relative), &external)) {
        if (mapper->externalSourceCount == UINT32_MAX) return false;
        ++mapper->externalSourceCount;
        *outIndex = kNoSourceFile;
        return true;
    }
    for (uint32_t i = 0; i < mapper->sourceFileCount; ++i) {
        if (equalText(mapper->sourceFiles[i].relativePath, relative, true)) {
            *outIndex = static_cast<uint16_t>(i);
            return true;
        }
    }
    if (mapper->sourceFileCount >= kDebugMapperMaxSourceFiles) {
        mapper->truncated = true;
        mapper->error = DebugDwarfError::LimitExceeded;
        return false;
    }
    const uint32_t index = mapper->sourceFileCount++;
    copyText(mapper->sourceFiles[index].relativePath, sizeof(mapper->sourceFiles[index].relativePath), relative);
    mapper->sourceFiles[index].external = false;
    *outIndex = static_cast<uint16_t>(index);
    return true;
}

static int compareKey(uint16_t sourceFileIndex, uint32_t line, const DebugDwarfLineKey& key) {
    if (sourceFileIndex < key.sourceFileIndex) return -1;
    if (sourceFileIndex > key.sourceFileIndex) return 1;
    if (line < key.line) return -1;
    if (line > key.line) return 1;
    return 0;
}

static bool addLineAddress(DebugDwarfMapper* mapper, uint16_t sourceFileIndex,
                           uint32_t line, uint64_t address, bool isStmt) {
    uint32_t low = 0;
    uint32_t high = mapper->lineKeyCount;
    while (low < high) {
        const uint32_t middle = low + (high - low) / 2;
        if (compareKey(sourceFileIndex, line, mapper->lineKeys[middle]) <= 0) high = middle;
        else low = middle + 1;
    }
    uint32_t index = low;
    if (index == mapper->lineKeyCount || compareKey(sourceFileIndex, line, mapper->lineKeys[index]) != 0) {
        if (mapper->lineKeyCount >= kDebugMapperMaxLineKeys) {
            mapper->truncated = true;
            mapper->error = DebugDwarfError::LimitExceeded;
            return false;
        }
        for (uint32_t i = mapper->lineKeyCount; i > index; --i) mapper->lineKeys[i] = mapper->lineKeys[i - 1];
        mapper->lineKeys[index] = DebugDwarfLineKey();
        mapper->lineKeys[index].sourceFileIndex = sourceFileIndex;
        mapper->lineKeys[index].line = line;
        ++mapper->lineKeyCount;
    }
    DebugDwarfLineKey& key = mapper->lineKeys[index];
    for (uint32_t i = 0; i < key.addressCount; ++i) {
        if (key.addresses[i] == address) {
            if (isStmt && !key.hasStmtAddress) { key.hasStmtAddress = true; key.primaryAddress = address; }
            return true;
        }
    }
    if (key.addressCount < kDebugMapperMaxAddressesPerLine) {
        key.addresses[key.addressCount++] = address;
    } else {
        mapper->truncated = true;
        return true;
    }
    if (key.addressCount == 1 || (isStmt && !key.hasStmtAddress) ||
        (!key.hasStmtAddress && address < key.primaryAddress)) {
        key.primaryAddress = address;
        if (isStmt) key.hasStmtAddress = true;
    }
    return true;
}

static bool addLineRow(DebugDwarfMapper* mapper, uint16_t sourceFileIndex,
                       uint32_t line, uint32_t column, uint32_t sequence,
                       uint64_t address, bool isStmt) {
    if (sourceFileIndex == kNoSourceFile || line == 0) return true;
    if (mapper->lineRowCount > 0) {
        DebugDwarfLineRow& previous = mapper->rows[mapper->lineRowCount - 1];
        if (previous.sourceFileIndex == sourceFileIndex && previous.line == line &&
            previous.column == column && previous.sequence == sequence && previous.address == address &&
            previous.isStmt == isStmt) return true;
        if (previous.sequence == sequence && address > previous.address && previous.endAddress == 0)
            previous.endAddress = address;
    }
    if (mapper->lineRowCount >= kDebugMapperMaxLineRows) {
        mapper->truncated = true;
        mapper->error = DebugDwarfError::LimitExceeded;
        return false;
    }
    DebugDwarfLineRow& row = mapper->rows[mapper->lineRowCount];
    row = DebugDwarfLineRow();
    row.sourceFileIndex = sourceFileIndex;
    row.line = line;
    row.column = column;
    row.sequence = sequence;
    row.order = mapper->lineRowCount;
    row.address = address;
    row.isStmt = isStmt;
    ++mapper->lineRowCount;
    return addLineAddress(mapper, sourceFileIndex, line, address, isStmt);
}

static bool addAddressBoundary(DebugDwarfMapper* mapper, uint32_t sequence, uint64_t address) {
    if (mapper->lineRowCount == 0) return true;
    DebugDwarfLineRow& previous = mapper->rows[mapper->lineRowCount - 1];
    if (previous.sequence == sequence && address > previous.address) previous.endAddress = address;
    return true;
}

static bool addDirectory(DebugDwarfMapper* mapper, const char* value) {
    if (mapper->directoryCount >= kDebugMapperMaxDirectories) {
        mapper->truncated = true;
        mapper->error = DebugDwarfError::LimitExceeded;
        return false;
    }
    if (!copyText(mapper->directories[mapper->directoryCount], sizeof(mapper->directories[0]), value)) return false;
    ++mapper->directoryCount;
    return true;
}

static bool readLineForm(Cursor& cursor, uint64_t form, SectionView lineStrings,
                         SectionView debugStrings, char* stringValue, uint32_t stringSize,
                         uint64_t* numberValue) {
    if (stringValue && stringSize > 0) stringValue[0] = '\0';
    if (numberValue) *numberValue = 0;
    uint64_t value = 0;
    switch (form) {
    case 0x08: // DW_FORM_string
        return stringValue && readCString(cursor, stringValue, stringSize);
    case 0x1f: // DW_FORM_line_strp
        { uint32_t offset = 0; if (!readU32Cursor(cursor, &offset) ||
            !readSectionString(lineStrings.data, lineStrings.size, offset, stringValue, stringSize)) return false; }
        return true;
    case 0x0e: // DW_FORM_strp
        { uint32_t offset = 0; if (!readU32Cursor(cursor, &offset) ||
            !readSectionString(debugStrings.data, debugStrings.size, offset, stringValue, stringSize)) return false; }
        return true;
    case 0x0f: // DW_FORM_udata
        if (!readULEB(cursor, &value)) return false;
        break;
    case 0x0b: // DW_FORM_data1
        { uint8_t byte = 0; if (!readByte(cursor, &byte)) return false; value = byte; }
        break;
    case 0x05: // DW_FORM_data2
        { uint16_t number = 0; if (!readU16Cursor(cursor, &number)) return false; value = number; }
        break;
    case 0x06: // DW_FORM_data4
    case 0x17: // DW_FORM_sec_offset, 32-bit line sections on this target
        { uint32_t number = 0; if (!readU32Cursor(cursor, &number)) return false; value = number; }
        break;
    case 0x07: // DW_FORM_data8
        if (!readU64Cursor(cursor, &value)) return false;
        break;
    case 0x0c: // DW_FORM_flag
        { uint8_t byte = 0; if (!readByte(cursor, &byte)) return false; value = byte; }
        break;
    case 0x1e: // DW_FORM_data16, used by Clang for the optional file MD5
        if (!canRead(cursor, 16)) return false;
        cursor.position += 16;
        value = 0;
        break;
    default:
        return false;
    }
    if (numberValue) *numberValue = value;
    return true;
}

static bool resolveFileTableEntry(DebugDwarfMapper* mapper, const char* projectRoot,
                                  uint32_t fileIndex, const char* fileName,
                                  uint32_t directoryIndex) {
    if (fileIndex == 0 || fileIndex > kDebugMapperMaxFiles) return false;
    const char* directory = directoryIndex > 0 && directoryIndex <= mapper->directoryCount ?
        mapper->directories[directoryIndex - 1] : "";
    uint16_t sourceIndex = kNoSourceFile;
    if (!addSourceFile(mapper, projectRoot, directory, fileName, &sourceIndex)) return false;
    mapper->currentFileSources[fileIndex] = sourceIndex;
    if (fileIndex == 1) mapper->currentFileSources[0] = sourceIndex;
    if (fileIndex > mapper->currentFileCount) mapper->currentFileCount = fileIndex;
    return true;
}

static bool parseDwarf5Tables(DebugDwarfMapper* mapper, Cursor& cursor, uint64_t headerEnd,
                              const char* projectRoot, SectionView lineStrings,
                              SectionView debugStrings) {
    mapper->directoryCount = 0;
    mapper->currentFileCount = 0;
    for (uint32_t i = 0; i < sizeof(mapper->currentFileSources) / sizeof(mapper->currentFileSources[0]); ++i)
        mapper->currentFileSources[i] = kNoSourceFile;
    uint64_t formatCount = 0;
    if (!readULEB(cursor, &formatCount) || formatCount > 16) return false;
    uint64_t directoryContents[16] = {};
    uint64_t directoryForms[16] = {};
    for (uint32_t i = 0; i < formatCount; ++i) {
        if (!readULEB(cursor, &directoryContents[i]) || !readULEB(cursor, &directoryForms[i])) return false;
    }
    uint64_t directoryCount = 0;
    if (!readULEB(cursor, &directoryCount) || directoryCount > kDebugMapperMaxDirectories) return false;
    for (uint32_t entry = 0; entry < directoryCount; ++entry) {
        char path[kDebugMapperMaxPathBytes] = {};
        bool havePath = false;
        for (uint32_t i = 0; i < formatCount; ++i) {
            char value[kDebugMapperMaxPathBytes] = {};
            uint64_t number = 0;
            if (!readLineForm(cursor, directoryForms[i], lineStrings, debugStrings, value, sizeof(value), &number)) return false;
            if (directoryContents[i] == 0x1) { copyText(path, sizeof(path), value); havePath = true; }
        }
        if (!havePath || !addDirectory(mapper, path)) return false;
    }
    uint64_t formatFileCount = 0;
    if (!readULEB(cursor, &formatFileCount) || formatFileCount > 16) return false;
    uint64_t fileContents[16] = {};
    uint64_t fileForms[16] = {};
    for (uint32_t i = 0; i < formatFileCount; ++i) {
        if (!readULEB(cursor, &fileContents[i]) || !readULEB(cursor, &fileForms[i])) return false;
    }
    uint64_t fileCount = 0;
    if (!readULEB(cursor, &fileCount) || fileCount > kDebugMapperMaxFiles) return false;
    for (uint32_t entry = 0; entry < fileCount; ++entry) {
        char path[kDebugMapperMaxPathBytes] = {};
        uint64_t directoryIndex = 0;
        bool havePath = false;
        for (uint32_t i = 0; i < formatFileCount; ++i) {
            char value[kDebugMapperMaxPathBytes] = {};
            uint64_t number = 0;
            if (!readLineForm(cursor, fileForms[i], lineStrings, debugStrings, value, sizeof(value), &number)) return false;
            if (fileContents[i] == 0x1) { copyText(path, sizeof(path), value); havePath = true; }
            else if (fileContents[i] == 0x2) directoryIndex = number;
        }
        if (!havePath || !resolveFileTableEntry(mapper, projectRoot, entry + 1, path,
                                                static_cast<uint32_t>(directoryIndex))) return false;
    }
    if (cursor.position > headerEnd) return false;
    return true;
}

static bool parseDwarf4Tables(DebugDwarfMapper* mapper, Cursor& cursor,
                              const char* projectRoot) {
    mapper->directoryCount = 0;
    mapper->currentFileCount = 0;
    for (uint32_t i = 0; i < sizeof(mapper->currentFileSources) / sizeof(mapper->currentFileSources[0]); ++i)
        mapper->currentFileSources[i] = kNoSourceFile;
    while (cursor.position < cursor.end) {
        char value[kDebugMapperMaxPathBytes] = {};
        if (!readCString(cursor, value, sizeof(value))) return false;
        if (value[0] == '\0') break;
        if (!addDirectory(mapper, value)) return false;
    }
    uint32_t fileIndex = 1;
    while (cursor.position < cursor.end) {
        char fileName[kDebugMapperMaxPathBytes] = {};
        if (!readCString(cursor, fileName, sizeof(fileName))) return false;
        if (fileName[0] == '\0') break;
        uint64_t directoryIndex = 0;
        uint64_t timestamp = 0;
        uint64_t size = 0;
        if (!readULEB(cursor, &directoryIndex) || !readULEB(cursor, &timestamp) || !readULEB(cursor, &size) ||
            !resolveFileTableEntry(mapper, projectRoot, fileIndex++, fileName, static_cast<uint32_t>(directoryIndex))) return false;
    }
    return fileIndex > 1;
}

static bool parseLineProgram(DebugDwarfMapper* mapper, Cursor program, uint16_t version,
                             uint8_t addressSize, uint8_t minimumInstructionLength,
                             uint8_t maximumOperationsPerInstruction, bool defaultIsStmt,
                             int8_t lineBase, uint8_t lineRange, uint8_t opcodeBase,
                             const unsigned char* standardOpcodeLengths,
                             const char* projectRoot) {
    Cursor cursor = program;
    uint64_t address = 0;
    uint32_t file = 1;
    int64_t line = 1;
    uint32_t column = 0;
    bool isStmt = defaultIsStmt;
    uint32_t sequence = mapper->sequenceCount;
    if (sequence >= kDebugMapperMaxSequences) { mapper->truncated = true; mapper->error = DebugDwarfError::LimitExceeded; return false; }
    bool sequenceStarted = false;
    while (cursor.position < cursor.end) {
        uint8_t opcode = 0;
        if (!readByte(cursor, &opcode)) return false;
        if (opcode == 0) {
            uint64_t extendedLength = 0;
            if (!readULEB(cursor, &extendedLength) || extendedLength == 0 || extendedLength > cursor.end - cursor.position) return false;
            Cursor extended = { cursor.bytes, cursor.position, cursor.position + extendedLength };
            uint8_t extendedOpcode = 0;
            if (!readByte(extended, &extendedOpcode)) return false;
            if (extendedOpcode == 1) { // DW_LNE_end_sequence
                addAddressBoundary(mapper, sequence, address);
                ++sequence;
                if (sequence >= kDebugMapperMaxSequences && extended.position < extended.end) { mapper->truncated = true; mapper->error = DebugDwarfError::LimitExceeded; return false; }
                address = 0; file = 1; line = 1; column = 0; isStmt = defaultIsStmt; sequenceStarted = false;
            } else if (extendedOpcode == 2) { // DW_LNE_set_address
                if (addressSize != 8 || extended.end - extended.position < addressSize) return false;
                address = readU64(extended.bytes, extended.position);
                extended.position += addressSize;
                sequenceStarted = true;
            } else if (extendedOpcode == 3) { // DW_LNE_define_file (DWARF 4 compatibility)
                if (version >= kDwarfVersion5) return false;
                char fileName[kDebugMapperMaxPathBytes] = {};
                uint64_t directoryIndex = 0, timestamp = 0, size = 0;
                if (!readCString(extended, fileName, sizeof(fileName)) || !readULEB(extended, &directoryIndex) ||
                    !readULEB(extended, &timestamp) || !readULEB(extended, &size) ||
                    !resolveFileTableEntry(mapper, projectRoot, mapper->currentFileCount + 1, fileName,
                                           static_cast<uint32_t>(directoryIndex))) return false;
            } else if (extendedOpcode == 4) { // DW_LNE_set_discriminator
                uint64_t discriminator = 0;
                if (!readULEB(extended, &discriminator)) return false;
            } else {
                mapper->error = DebugDwarfError::UnsupportedOpcode;
                return false;
            }
            cursor.position = extended.end;
            continue;
        }
        if (opcode < opcodeBase) {
            uint64_t operand = 0;
            switch (opcode) {
            case 1: { // DW_LNS_copy
                if (line > 0 && file <= kDebugMapperMaxFiles)
                    if (!addLineRow(mapper, mapper->currentFileSources[file], static_cast<uint32_t>(line), column, sequence, address, isStmt)) return false;
                sequenceStarted = true;
                break;
            }
            case 2: // DW_LNS_advance_pc
                if (!readULEB(cursor, &operand)) return false;
                if (maximumOperationsPerInstruction == 0) return false;
                address += static_cast<uint64_t>(minimumInstructionLength) * operand;
                break;
            case 3: { // DW_LNS_advance_line
                int64_t delta = 0;
                if (!readSLEB(cursor, &delta)) return false;
                line += delta;
                break;
            }
            case 4: // DW_LNS_set_file
                if (!readULEB(cursor, &operand) || operand > kDebugMapperMaxFiles) return false;
                file = static_cast<uint32_t>(operand);
                break;
            case 5: // DW_LNS_set_column
                if (!readULEB(cursor, &operand) || operand > UINT32_MAX) return false;
                column = static_cast<uint32_t>(operand);
                break;
            case 6: // DW_LNS_negate_stmt
                isStmt = !isStmt;
                break;
            case 7: // DW_LNS_set_basic_block
            case 10: // DW_LNS_set_prologue_end
            case 11: // DW_LNS_set_epilogue_begin
                break;
            case 8: { // DW_LNS_const_add_pc
                if (lineRange == 0) return false;
                const uint32_t advance = (255u - opcodeBase) / lineRange;
                address += static_cast<uint64_t>(minimumInstructionLength) * advance;
                break;
            }
            case 9: { // DW_LNS_fixed_advance_pc
                uint16_t advance = 0;
                if (!readU16Cursor(cursor, &advance)) return false;
                address += advance;
                break;
            }
            case 12: // DW_LNS_set_isa
                if (!readULEB(cursor, &operand)) return false;
                break;
            default:
                if (opcode - 1 >= opcodeBase - 1 || !standardOpcodeLengths[opcode - 1]) {
                    mapper->error = DebugDwarfError::UnsupportedOpcode;
                    return false;
                }
                for (uint32_t i = 0; i < standardOpcodeLengths[opcode - 1]; ++i)
                    if (!readULEB(cursor, &operand)) return false;
                break;
            }
            continue;
        }
        if (lineRange == 0) return false;
        const uint32_t adjusted = static_cast<uint32_t>(opcode - opcodeBase);
        const uint32_t operationAdvance = adjusted / lineRange;
        const uint32_t lineAdvance = adjusted % lineRange;
        address += static_cast<uint64_t>(minimumInstructionLength) * operationAdvance;
        line += static_cast<int64_t>(lineBase) + static_cast<int64_t>(lineAdvance);
        if (line > 0 && file <= kDebugMapperMaxFiles)
            if (!addLineRow(mapper, mapper->currentFileSources[file], static_cast<uint32_t>(line), column, sequence, address, isStmt)) return false;
        sequenceStarted = true;
    }
    if (sequenceStarted) addAddressBoundary(mapper, sequence, address);
    mapper->sequenceCount = sequenceStarted ? sequence + 1 : sequence;
    return true;
}

static void sortAddressOrder(DebugDwarfMapper* mapper) {
    mapper->addressOrderCount = mapper->lineRowCount;
    for (uint32_t i = 0; i < mapper->lineRowCount; ++i) mapper->addressOrder[i] = i;
    for (uint32_t i = 1; i < mapper->addressOrderCount; ++i) {
        const uint32_t value = mapper->addressOrder[i];
        uint32_t j = i;
        while (j > 0) {
            const DebugDwarfLineRow& left = mapper->rows[mapper->addressOrder[j - 1]];
            const DebugDwarfLineRow& right = mapper->rows[value];
            if (left.address < right.address || (left.address == right.address && left.order <= right.order)) break;
            mapper->addressOrder[j] = mapper->addressOrder[j - 1];
            --j;
        }
        mapper->addressOrder[j] = value;
    }
}

static DebugDwarfError mapperErrorForQuery(bool sourceFound, bool lineFound) {
    if (!sourceFound) return DebugDwarfError::SourceNotFound;
    return lineFound ? DebugDwarfError::None : DebugDwarfError::LineNotMapped;
}

static bool parseElf(DebugDwarfMapper* mapper, const char* projectRoot,
                     const unsigned char* bytes, uint64_t size) {
    if (!bytes || size < 64 || size > kDebugMapperMaxElfBytes ||
        bytes[0] != 0x7f || bytes[1] != 'E' || bytes[2] != 'L' || bytes[3] != 'F' ||
        bytes[4] != 2 || bytes[5] != 1) { mapper->error = DebugDwarfError::MalformedElf; return false; }
    if (readU16(bytes, 16) != kElfTypeExec || readU16(bytes, 18) != kElfMachineAmd64) {
        mapper->error = readU16(bytes, 18) == kElfMachineAmd64 ? DebugDwarfError::MalformedElf : DebugDwarfError::UnsupportedArchitecture;
        return false;
    }
    const uint64_t sectionOffset = readU64(bytes, 40);
    const uint16_t sectionEntrySize = readU16(bytes, 58);
    const uint16_t sectionCount = readU16(bytes, 60);
    const uint16_t stringIndex = readU16(bytes, 62);
    if (sectionCount == 0 || sectionCount > kDebugMapperMaxSections || sectionEntrySize < 64 ||
        !checkedRange(sectionOffset, static_cast<uint64_t>(sectionEntrySize) * sectionCount, size) || stringIndex >= sectionCount) {
        mapper->error = DebugDwarfError::MalformedElf; return false;
    }
    const uint64_t stringHeader = sectionOffset + static_cast<uint64_t>(stringIndex) * sectionEntrySize;
    const uint64_t stringOffset = readU64(bytes, stringHeader + 24);
    const uint64_t stringSize = readU64(bytes, stringHeader + 32);
    const uint32_t stringType = readU32(bytes, stringHeader + 4);
    if (stringType != kSectionTypeString || !checkedRange(stringOffset, stringSize, size) || stringSize == 0 || bytes[stringOffset] != '\0') {
        mapper->error = DebugDwarfError::MalformedElf; return false;
    }
    SectionView line = { nullptr, 0 };
    SectionView lineStrings = { nullptr, 0 };
    SectionView debugStrings = { nullptr, 0 };
    for (uint16_t i = 0; i < sectionCount; ++i) {
        const uint64_t header = sectionOffset + static_cast<uint64_t>(i) * sectionEntrySize;
        const uint32_t nameOffset = readU32(bytes, header);
        if (nameOffset >= stringSize) { mapper->error = DebugDwarfError::MalformedElf; return false; }
        char name[64] = {};
        if (!readSectionString(bytes + stringOffset, stringSize, nameOffset, name, sizeof(name))) { mapper->error = DebugDwarfError::MalformedElf; return false; }
        const uint32_t type = readU32(bytes, header + 4);
        const uint64_t offset = readU64(bytes, header + 24);
        const uint64_t sectionSize = readU64(bytes, header + 32);
        if (type != kSectionTypeNoBits && !checkedRange(offset, sectionSize, size)) { mapper->error = DebugDwarfError::MalformedElf; return false; }
        if (type != kSectionTypeNoBits && sectionSize > kDebugMapperMaxSectionBytes) {
            mapper->error = DebugDwarfError::LimitExceeded;
            return false;
        }
        SectionView view = { type == kSectionTypeNoBits ? nullptr : bytes + offset, sectionSize };
        if (equalText(name, ".debug_line", false)) line = view;
        else if (equalText(name, ".debug_line_str", false)) lineStrings = view;
        else if (equalText(name, ".debug_str", false)) debugStrings = view;
    }
    if (!line.data || line.size == 0) { mapper->error = DebugDwarfError::MissingLineSection; return false; }
    mapper->lineSectionBytes = line.size > UINT32_MAX ? UINT32_MAX : static_cast<uint32_t>(line.size);
    Cursor units = { line.data, 0, line.size };
    while (units.position < units.end) {
        const uint64_t unitStart = units.position;
        uint32_t initialLength = 0;
        if (!readU32Cursor(units, &initialLength)) { mapper->error = DebugDwarfError::MalformedDwarf; return false; }
        uint64_t unitLength = initialLength;
        if (initialLength == 0xffffffffu) { mapper->error = DebugDwarfError::UnsupportedDwarfVersion; return false; }
        if (unitLength < 2 || !checkedRange(units.position, unitLength, units.end)) { mapper->error = DebugDwarfError::MalformedDwarf; return false; }
        const uint64_t unitEnd = units.position + unitLength;
        uint16_t version = 0;
        if (!readU16Cursor(units, &version)) { mapper->error = DebugDwarfError::MalformedDwarf; return false; }
        if (version != kDwarfVersion4 && version != kDwarfVersion5) { mapper->error = DebugDwarfError::UnsupportedDwarfVersion; return false; }
        if (mapper->dwarfVersion == 0) mapper->dwarfVersion = version;
        else if (mapper->dwarfVersion != version) { mapper->error = DebugDwarfError::UnsupportedDwarfVersion; return false; }
        uint8_t addressSize = 8;
        uint8_t segmentSelectorSize = 0;
        uint32_t headerLength = 0;
        if (version >= kDwarfVersion5) {
            if (!readByte(units, &addressSize) || !readByte(units, &segmentSelectorSize) || !readU32Cursor(units, &headerLength)) {
                mapper->error = DebugDwarfError::MalformedDwarf; return false;
            }
            if (addressSize != 8 || segmentSelectorSize != 0) { mapper->error = DebugDwarfError::UnsupportedArchitecture; return false; }
        } else if (!readU32Cursor(units, &headerLength)) { mapper->error = DebugDwarfError::MalformedDwarf; return false; }
        const uint64_t headerStart = units.position;
        if (!checkedRange(headerStart, headerLength, unitEnd)) { mapper->error = DebugDwarfError::MalformedDwarf; return false; }
        Cursor headerCursor = { line.data, headerStart, headerStart + headerLength };
        uint8_t minimumInstructionLength = 0, maximumOperationsPerInstruction = 1, defaultIsStmt = 0, lineRange = 0, opcodeBase = 0;
        int8_t lineBase = 0;
        if (!readByte(headerCursor, &minimumInstructionLength)) { mapper->error = DebugDwarfError::MalformedDwarf; return false; }
        if (version >= kDwarfVersion4 && !readByte(headerCursor, &maximumOperationsPerInstruction)) { mapper->error = DebugDwarfError::MalformedDwarf; return false; }
        if (!readByte(headerCursor, &defaultIsStmt) || !readByte(headerCursor, reinterpret_cast<uint8_t*>(&lineBase)) ||
            !readByte(headerCursor, &lineRange) || !readByte(headerCursor, &opcodeBase) || opcodeBase == 0) {
            mapper->error = DebugDwarfError::MalformedDwarf; return false;
        }
        unsigned char standardOpcodeLengths[256] = {};
        for (uint32_t i = 1; i < opcodeBase; ++i)
            if (!readByte(headerCursor, &standardOpcodeLengths[i - 1])) { mapper->error = DebugDwarfError::MalformedDwarf; return false; }
        const uint64_t tableStart = headerCursor.position;
        bool tablesOk = version >= kDwarfVersion5 ?
            parseDwarf5Tables(mapper, headerCursor, headerStart + headerLength, projectRoot, lineStrings, debugStrings) :
            parseDwarf4Tables(mapper, headerCursor, projectRoot);
        if (!tablesOk || headerCursor.position > headerStart + headerLength) { mapper->error = DebugDwarfError::UnsupportedForm; return false; }
        (void)tableStart;
        units.position = headerStart + headerLength;
        Cursor program = { line.data, units.position, unitEnd };
        if (!parseLineProgram(mapper, program, version, addressSize, minimumInstructionLength,
                              maximumOperationsPerInstruction, defaultIsStmt != 0, lineBase,
                              lineRange, opcodeBase, standardOpcodeLengths, projectRoot)) {
            if (mapper->error == DebugDwarfError::None) mapper->error = DebugDwarfError::MalformedDwarf;
            return false;
        }
        units.position = unitEnd;
        if (units.position <= unitStart) { mapper->error = DebugDwarfError::MalformedDwarf; return false; }
    }
    sortAddressOrder(mapper);
    mapper->identity.loadBias = 0;
    mapper->state = mapper->lineRowCount == 0 ? DebugDwarfMapperState::Failed : DebugDwarfMapperState::Ready;
    if (mapper->lineRowCount == 0) mapper->error = DebugDwarfError::LineNotMapped;
    return mapper->state == DebugDwarfMapperState::Ready;
}

} // namespace

const char* DebugDwarfMapperStateName(DebugDwarfMapperState state) {
    switch (state) {
    case DebugDwarfMapperState::Empty: return "Empty";
    case DebugDwarfMapperState::Ready: return "Ready";
    case DebugDwarfMapperState::Failed: return "Failed";
    case DebugDwarfMapperState::Stale: return "Stale";
    }
    return "Unknown";
}

const char* DebugDwarfErrorName(DebugDwarfError error) {
    switch (error) {
    case DebugDwarfError::None: return "none";
    case DebugDwarfError::NoDebugInfo: return "no_debug_info";
    case DebugDwarfError::MissingLineSection: return "missing_line_section";
    case DebugDwarfError::MalformedElf: return "malformed_elf";
    case DebugDwarfError::MalformedDwarf: return "malformed_dwarf";
    case DebugDwarfError::UnsupportedDwarfVersion: return "unsupported_dwarf_version";
    case DebugDwarfError::UnsupportedForm: return "unsupported_form";
    case DebugDwarfError::UnsupportedArchitecture: return "unsupported_architecture";
    case DebugDwarfError::ArtifactChanged: return "artifact_changed";
    case DebugDwarfError::SourceNotFound: return "source_not_found";
    case DebugDwarfError::LineNotMapped: return "line_not_mapped";
    case DebugDwarfError::Truncated: return "truncated";
    case DebugDwarfError::LimitExceeded: return "limit_exceeded";
    case DebugDwarfError::UnsupportedOpcode: return "unsupported_opcode";
    }
    return "unknown";
}

void DebugDwarfMapperReset(DebugDwarfMapper* mapper) {
    if (!mapper) return;
    unsigned char* bytes = reinterpret_cast<unsigned char*>(mapper);
    for (uint32_t i = 0; i < sizeof(DebugDwarfMapper); ++i) bytes[i] = 0;
    mapper->state = DebugDwarfMapperState::Empty;
    mapper->error = DebugDwarfError::None;
}

bool DebugDwarfMapperLoad(DebugDwarfMapper* mapper, const char* projectRoot,
                          const char* projectId, const char* targetProfile,
                          const char* architecture, const char* executablePath,
                          uint64_t executableSize, const char* artifactSha256,
                          uint64_t projectGeneration, const unsigned char* elfBytes,
                          uint64_t elfSize, uint32_t mapperGeneration,
                          DebugDwarfError* error) {
    if (error) *error = DebugDwarfError::None;
    if (!mapper || !projectRoot || !projectId || !targetProfile || !architecture ||
        !executablePath || !artifactSha256 || !elfBytes || elfSize == 0 || elfSize > kDebugMapperMaxElfBytes) {
        if (error) *error = DebugDwarfError::MalformedElf;
        return false;
    }
    DebugDwarfMapperReset(mapper);
    copyText(mapper->identity.executablePath, sizeof(mapper->identity.executablePath), executablePath);
    copyText(mapper->identity.sha256, sizeof(mapper->identity.sha256), artifactSha256);
    copyText(mapper->identity.projectId, sizeof(mapper->identity.projectId), projectId);
    copyText(mapper->identity.targetProfile, sizeof(mapper->identity.targetProfile), targetProfile);
    copyText(mapper->identity.architecture, sizeof(mapper->identity.architecture), architecture);
    mapper->identity.executableSize = executableSize;
    mapper->identity.projectGeneration = projectGeneration;
    mapper->identity.mapperGeneration = mapperGeneration;
    if (executableSize != 0 && executableSize != elfSize) mapper->error = DebugDwarfError::ArtifactChanged;
    else parseElf(mapper, projectRoot, elfBytes, elfSize);
    mapper->state = mapper->error == DebugDwarfError::None && mapper->lineRowCount > 0 ?
        DebugDwarfMapperState::Ready : DebugDwarfMapperState::Failed;
    if (mapper->state == DebugDwarfMapperState::Ready) mapper->error = DebugDwarfError::None;
    if (error) *error = mapper->error;
    return mapper->state == DebugDwarfMapperState::Ready;
}

bool DebugDwarfMapperIsReady(const DebugDwarfMapper* mapper) {
    return mapper && mapper->state == DebugDwarfMapperState::Ready;
}

bool DebugDwarfMapperMatchesArtifact(const DebugDwarfMapper* mapper,
                                     const char* projectRoot, const char* projectId,
                                     const char* targetProfile, const char* architecture,
                                     const char* executablePath, uint64_t executableSize,
                                     const char* artifactSha256,
                                     uint64_t projectGeneration) {
    if (!mapper || mapper->state != DebugDwarfMapperState::Ready || !projectRoot || !projectId ||
        !targetProfile || !architecture || !executablePath || !artifactSha256) return false;
    char root[kDebugMapperMaxPathBytes] = {};
    char path[kDebugMapperMaxPathBytes] = {};
    bool rootAbsolute = false, pathAbsolute = false;
    if (!normalizeGeneric(projectRoot, root, sizeof(root), true, &rootAbsolute) ||
        !normalizeGeneric(executablePath, path, sizeof(path), false, &pathAbsolute)) return false;
    if (pathAbsolute) {
        char relative[kDebugMapperMaxPathBytes] = {};
        if (!pathContained(root, path, relative, sizeof(relative))) return false;
        copyText(path, sizeof(path), relative);
    }
    return equalText(mapper->identity.projectId, projectId, true) &&
        equalText(mapper->identity.targetProfile, targetProfile, false) &&
        equalText(mapper->identity.architecture, architecture, true) &&
        equalText(mapper->identity.executablePath, path, true) &&
        equalText(mapper->identity.sha256, artifactSha256, false) &&
        mapper->identity.executableSize == executableSize &&
        mapper->identity.projectGeneration == projectGeneration;
}

bool DebugDwarfMapperMapSourceToAddresses(const DebugDwarfMapper* mapper,
                                          const char* relativePath, uint32_t line,
                                          uint64_t* addresses, uint32_t capacity,
                                          uint32_t* outCount, uint64_t* outPrimary,
                                          DebugDwarfError* error) {
    if (outCount) *outCount = 0;
    if (outPrimary) *outPrimary = 0;
    if (error) *error = DebugDwarfError::None;
    if (!mapper || !relativePath || line == 0 || !addresses || capacity == 0 || !outCount || !outPrimary ||
        mapper->state != DebugDwarfMapperState::Ready) {
        if (error) *error = mapper && mapper->state == DebugDwarfMapperState::Stale ? DebugDwarfError::ArtifactChanged : DebugDwarfError::NoDebugInfo;
        return false;
    }
    char normalized[kDebugMapperMaxPathBytes] = {};
    copyText(normalized, sizeof(normalized), relativePath);
    for (uint32_t i = 0; normalized[i] != '\0'; ++i) if (normalized[i] == static_cast<char>(92)) normalized[i] = '/';
    bool sourceFound = false;
    for (uint32_t i = 0; i < mapper->sourceFileCount; ++i) {
        if (!equalText(mapper->sourceFiles[i].relativePath, normalized, true)) continue;
        sourceFound = true;
        uint32_t low = 0, high = mapper->lineKeyCount;
        while (low < high) {
            const uint32_t middle = low + (high - low) / 2;
            const DebugDwarfLineKey& key = mapper->lineKeys[middle];
            if (key.sourceFileIndex < i || (key.sourceFileIndex == i && key.line < line)) low = middle + 1;
            else high = middle;
        }
        if (low < mapper->lineKeyCount && mapper->lineKeys[low].sourceFileIndex == i && mapper->lineKeys[low].line == line) {
            const DebugDwarfLineKey& key = mapper->lineKeys[low];
            const uint32_t count = key.addressCount < capacity ? key.addressCount : capacity;
            for (uint32_t address = 0; address < count; ++address) addresses[address] = key.addresses[address];
            *outCount = count;
            *outPrimary = key.primaryAddress;
            if (key.addressCount > capacity && error) *error = DebugDwarfError::Truncated;
            return count != 0;
        }
        break;
    }
    if (error) *error = mapperErrorForQuery(sourceFound, false);
    return false;
}

bool DebugDwarfMapperMapAddressToSource(const DebugDwarfMapper* mapper,
                                        uint64_t address, char* relativePath,
                                        uint32_t relativePathSize, uint32_t* line,
                                        uint32_t* column, DebugDwarfError* error) {
    if (error) *error = DebugDwarfError::None;
    if (relativePath && relativePathSize > 0) relativePath[0] = '\0';
    if (line) *line = 0;
    if (column) *column = 0;
    if (!mapper || mapper->state != DebugDwarfMapperState::Ready || !relativePath || relativePathSize == 0 || !line || !column) {
        if (error) *error = mapper && mapper->state == DebugDwarfMapperState::Stale ? DebugDwarfError::ArtifactChanged : DebugDwarfError::NoDebugInfo;
        return false;
    }
    uint32_t low = 0, high = mapper->addressOrderCount;
    while (low < high) {
        const uint32_t middle = low + (high - low) / 2;
        if (mapper->rows[mapper->addressOrder[middle]].address <= address) low = middle + 1;
        else high = middle;
    }
    bool found = false;
    const DebugDwarfLineRow* selected = nullptr;
    while (low > 0) {
        const DebugDwarfLineRow& row = mapper->rows[mapper->addressOrder[low - 1]];
        if (row.address > address) { --low; continue; }
        if (row.sourceFileIndex != kNoSourceFile &&
            (row.address == address || (row.endAddress > address && row.sequence == mapper->rows[mapper->addressOrder[low - 1]].sequence))) {
            if (!selected || (row.address == address && selected->address != address) ||
                (row.address == selected->address && row.isStmt && !selected->isStmt) ||
                (row.address == selected->address && row.order < selected->order)) selected = &row;
            found = true;
        }
        if (row.address < address && selected && row.endAddress <= address) break;
        --low;
    }
    if (!found || !selected || selected->sourceFileIndex >= mapper->sourceFileCount) {
        if (error) *error = DebugDwarfError::LineNotMapped;
        return false;
    }
    copyText(relativePath, relativePathSize, mapper->sourceFiles[selected->sourceFileIndex].relativePath);
    *line = selected->line;
    *column = selected->column;
    return true;
}

bool DebugDwarfNormalizeSourcePath(const char* projectRoot, const char* rawPath,
                                   char* relativePath, uint32_t relativePathSize,
                                   bool* external) {
    return normalizeForProject(projectRoot, rawPath, relativePath, relativePathSize, external);
}

} // namespace developer_studio
} // namespace guidexos
