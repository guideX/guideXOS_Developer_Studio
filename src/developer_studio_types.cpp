#include "developer_studio_types.h"

namespace guidexos {
namespace developer_studio {
namespace {

enum TypeTokenKind {
    TypeTokenIdentifier = 0,
    TypeTokenNumber,
    TypeTokenString,
    TypeTokenCharacter,
    TypeTokenPunctuation
};

struct TypeToken {
    uint32_t offset;
    uint32_t length;
    TypeTokenKind kind;
    uint16_t braceDepth;
};

static TypeToken g_tokens[kTypeMaxParserTokens];
static uint32_t g_tokenCount = 0;
static uint32_t g_matchingBraces[kTypeMaxParserTokens];
static char g_typeReadBuffer[kTypeMaxParserDocumentBytes + 1];

static uint32_t lengthOf(const char* text, uint32_t limit) {
    if (!text) return 0;
    uint32_t length = 0;
    while (length < limit && text[length] != '\0') ++length;
    return length;
}

static void clearBytes(void* bytes, uint32_t count) {
    if (!bytes) return;
    char* output = static_cast<char*>(bytes);
    for (uint32_t i = 0; i < count; ++i) output[i] = 0;
}

static void copyText(char* output, uint32_t capacity, const char* input, bool* truncated = nullptr) {
    if (truncated) *truncated = false;
    if (!output || capacity == 0) return;
    uint32_t i = 0;
    if (input) {
        while (input[i] != '\0') {
            if (i + 1 >= capacity) {
                if (truncated) *truncated = true;
                break;
            }
            output[i] = input[i];
            ++i;
        }
    }
    output[i < capacity ? i : capacity - 1] = '\0';
}

static void copyRange(char* output, uint32_t capacity, const char* text,
                      uint32_t start, uint32_t length, bool* truncated = nullptr) {
    if (truncated) *truncated = false;
    if (!output || capacity == 0) return;
    uint32_t i = 0;
    while (i < length && start + i < kTypeMaxParserDocumentBytes && text && text[start + i] != '\0') {
        if (i + 1 >= capacity) {
            if (truncated) *truncated = true;
            break;
        }
        output[i] = text[start + i];
        ++i;
    }
    output[i < capacity ? i : capacity - 1] = '\0';
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

static bool tokenIs(const char* text, const TypeToken& token, const char* value) {
    if (!text || !value) return false;
    uint32_t length = lengthOf(value, kTypeMaxDeclaratorBytes);
    if (token.length != length) return false;
    for (uint32_t i = 0; i < length; ++i) if (text[token.offset + i] != value[i]) return false;
    return true;
}

static bool tokenIdentifier(const TypeToken& token) { return token.kind == TypeTokenIdentifier; }
static bool tokenNumber(const TypeToken& token) { return token.kind == TypeTokenNumber; }
static bool isIdentifierStart(char value) {
    return value == '_' || (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z');
}
static bool isIdentifierPart(char value) {
    return isIdentifierStart(value) || (value >= '0' && value <= '9');
}
static bool isSpace(char value) {
    return value == ' ' || value == '\t' || value == '\r' || value == '\n' || value == '\f' || value == '\v';
}

static bool appendChar(char* output, uint32_t capacity, uint32_t& length, char value, bool* truncated = nullptr) {
    if (!output || length + 1 >= capacity) {
        if (truncated) *truncated = true;
        return false;
    }
    output[length++] = value;
    output[length] = '\0';
    return true;
}

static bool appendText(char* output, uint32_t capacity, uint32_t& length,
                       const char* input, bool* truncated = nullptr) {
    if (!input) return true;
    for (uint32_t i = 0; input[i] != '\0'; ++i)
        if (!appendChar(output, capacity, length, input[i], truncated)) return false;
    return true;
}

static bool appendTokenText(char* output, uint32_t capacity, uint32_t& length,
                            const char* text, const TypeToken& token, bool* truncated = nullptr) {
    for (uint32_t i = 0; i < token.length; ++i)
        if (!appendChar(output, capacity, length, text[token.offset + i], truncated)) return false;
    return true;
}

static bool isPunctuation(const char* text, const TypeToken& token, const char* value) {
    return token.kind == TypeTokenPunctuation && tokenIs(text, token, value);
}

static bool lexText(const char* text, uint32_t length, bool* truncated) {
    g_tokenCount = 0;
    if (truncated) *truncated = false;
    if (!text) return false;
    if (length > kTypeMaxParserDocumentBytes) {
        length = kTypeMaxParserDocumentBytes;
        if (truncated) *truncated = true;
    }
    uint32_t cursor = 0;
    uint32_t braceDepth = 0;
    bool blockComment = false;
    while (cursor < length) {
        if (blockComment) {
            if (cursor + 1 < length && text[cursor] == '*' && text[cursor + 1] == '/') {
                blockComment = false;
                cursor += 2;
            } else ++cursor;
            continue;
        }
        if (text[cursor] == '/' && cursor + 1 < length && text[cursor + 1] == '*') {
            blockComment = true;
            cursor += 2;
            continue;
        }
        if (text[cursor] == '/' && cursor + 1 < length && text[cursor + 1] == '/') {
            cursor += 2;
            while (cursor < length && text[cursor] != '\n') ++cursor;
            continue;
        }
        if (text[cursor] == '#') {
            uint32_t lineStart = cursor;
            while (lineStart > 0 && text[lineStart - 1] != '\n') --lineStart;
            uint32_t first = lineStart;
            while (first < cursor && (text[first] == ' ' || text[first] == '\t')) ++first;
            if (first == cursor) {
                while (cursor < length && text[cursor] != '\n') ++cursor;
                continue;
            }
        }
        if (isSpace(text[cursor])) { ++cursor; continue; }
        if (text[cursor] == '"' || text[cursor] == '\'') {
            const char quote = text[cursor++];
            while (cursor < length) {
                if (text[cursor] == '\\' && cursor + 1 < length) { cursor += 2; continue; }
                if (text[cursor] == quote) { ++cursor; break; }
                ++cursor;
            }
            if (g_tokenCount < kTypeMaxParserTokens) {
                g_tokens[g_tokenCount++] = { cursor ? cursor - 1 : 0, 1, quote == '"' ? TypeTokenString : TypeTokenCharacter,
                                              static_cast<uint16_t>(braceDepth > 65535u ? 65535u : braceDepth) };
            } else if (truncated) *truncated = true;
            continue;
        }
        if (isIdentifierStart(text[cursor])) {
            const uint32_t start = cursor++;
            while (cursor < length && isIdentifierPart(text[cursor])) ++cursor;
            if (g_tokenCount < kTypeMaxParserTokens)
                g_tokens[g_tokenCount++] = { start, cursor - start, TypeTokenIdentifier,
                                              static_cast<uint16_t>(braceDepth > 65535u ? 65535u : braceDepth) };
            else if (truncated) *truncated = true;
            continue;
        }
        if (text[cursor] >= '0' && text[cursor] <= '9') {
            const uint32_t start = cursor++;
            while (cursor < length && (isIdentifierPart(text[cursor]) || text[cursor] == '.' ||
                                       text[cursor] == '\'' || text[cursor] == '_')) ++cursor;
            if (g_tokenCount < kTypeMaxParserTokens)
                g_tokens[g_tokenCount++] = { start, cursor - start, TypeTokenNumber,
                                              static_cast<uint16_t>(braceDepth > 65535u ? 65535u : braceDepth) };
            else if (truncated) *truncated = true;
            continue;
        }
        uint32_t punctuationLength = 1;
        if (cursor + 1 < length && ((text[cursor] == ':' && text[cursor + 1] == ':') ||
                                    (text[cursor] == '&' && text[cursor + 1] == '&') ||
                                    (text[cursor] == '-' && text[cursor + 1] == '>'))) punctuationLength = 2;
        else if (cursor + 2 < length && text[cursor] == '.' && text[cursor + 1] == '.' && text[cursor + 2] == '.') punctuationLength = 3;
        if (g_tokenCount < kTypeMaxParserTokens)
            g_tokens[g_tokenCount++] = { cursor, punctuationLength, TypeTokenPunctuation,
                                          static_cast<uint16_t>(braceDepth > 65535u ? 65535u : braceDepth) };
        else if (truncated) *truncated = true;
        if (text[cursor] == '{') ++braceDepth;
        else if (text[cursor] == '}' && braceDepth > 0) --braceDepth;
        cursor += punctuationLength;
    }
    for (uint32_t i = 0; i < g_tokenCount; ++i) g_matchingBraces[i] = UINT32_MAX;
    uint32_t stack[kTypeMaxScopeNesting * 16u] = {};
    uint32_t stackCount = 0;
    for (uint32_t i = 0; i < g_tokenCount; ++i) {
        if (!isPunctuation(text, g_tokens[i], "{") && !isPunctuation(text, g_tokens[i], "}")) continue;
        if (isPunctuation(text, g_tokens[i], "{")) {
            if (stackCount < sizeof(stack) / sizeof(stack[0])) stack[stackCount++] = i;
            else if (truncated) *truncated = true;
        } else if (stackCount > 0) {
            const uint32_t open = stack[--stackCount];
            g_matchingBraces[open] = i;
            g_matchingBraces[i] = open;
        }
    }
    if (stackCount > 0 && truncated) *truncated = true;
    return true;
}

static void clearTypeInfo(TypeInfo& type) {
    type = TypeInfo();
    type.baseKind = TypeBaseKind::Unknown;
    type.referenceKind = TypeReferenceKind::None;
    type.confidence = TypeConfidence::Unknown;
    type.source = TypeSource::None;
    type.declarationLocation = TypeLocation();
}

static void setLocation(TypeLocation& location, const char* path, uint64_t documentId,
                        uint32_t documentGeneration, uint64_t projectGeneration,
                        const char* text, uint32_t length, uint32_t offset, uint32_t identifierLength) {
    location = TypeLocation();
    location.documentId = documentId;
    location.documentGeneration = documentGeneration;
    location.projectGeneration = projectGeneration;
    location.identifierOffset = offset;
    location.identifierLength = identifierLength;
    copyText(location.relativePath, sizeof(location.relativePath), path);
    uint32_t line = 1;
    uint32_t lineStart = 0;
    for (uint32_t i = 0; i < offset && i < length; ++i) if (text[i] == '\n') { ++line; lineStart = i + 1; }
    location.line = line;
    location.column = offset >= lineStart ? offset - lineStart + 1 : 1;
}

static bool makeRelativePath(const char* root, const char* path, char* output, uint32_t capacity) {
    if (!path || !output || capacity == 0) return false;
    output[0] = '\0';
    if (!root || root[0] == '\0') { copyText(output, capacity, path); return true; }
    uint32_t rootLength = lengthOf(root, kMaxPathBytes);
    uint32_t pathLength = lengthOf(path, kMaxPathBytes);
    if (pathLength >= rootLength && pathLength > 0) {
        bool same = true;
        for (uint32_t i = 0; i < rootLength; ++i) {
            char a = root[i]; char b = path[i];
            if (a == '\\') a = '/';
            if (b == '\\') b = '/';
            if (a >= 'A' && a <= 'Z') a = static_cast<char>(a + ('a' - 'A'));
            if (b >= 'A' && b <= 'Z') b = static_cast<char>(b + ('a' - 'A'));
            if (a != b) { same = false; break; }
        }
        if (same && (pathLength == rootLength || path[rootLength] == '/' || path[rootLength] == '\\')) {
            const uint32_t start = pathLength == rootLength ? pathLength : rootLength + 1;
            copyRange(output, capacity, path, start, pathLength - start);
            for (uint32_t i = 0; output[i] != '\0'; ++i) if (output[i] == '\\') output[i] = '/';
            return true;
        }
    }
    copyText(output, capacity, path);
    for (uint32_t i = 0; output[i] != '\0'; ++i) if (output[i] == '\\') output[i] = '/';
    return true;
}

static void tokenName(const char* text, const TypeToken& token, char* output, uint32_t capacity) {
    copyRange(output, capacity, text, token.offset, token.length);
}

static bool tokenTextEqual(const char* text, uint32_t tokenIndex, const char* value) {
    return tokenIndex < g_tokenCount && tokenIs(text, g_tokens[tokenIndex], value);
}

static bool isQualifier(const char* text, const TypeToken& token) {
    return tokenIs(text, token, "const") || tokenIs(text, token, "volatile");
}

static bool isStorageWord(const char* text, const TypeToken& token) {
    return tokenIs(text, token, "static") || tokenIs(text, token, "extern") ||
        tokenIs(text, token, "constexpr") || tokenIs(text, token, "inline") ||
        tokenIs(text, token, "register") || tokenIs(text, token, "mutable") ||
        tokenIs(text, token, "thread_local");
}

static bool isPrimitiveWord(const char* text, const TypeToken& token) {
    static const char* const words[] = {
        "void", "bool", "char", "signed", "unsigned", "short", "int", "long",
        "float", "double", "size_t", "int8_t", "int16_t", "int32_t", "int64_t",
        "uint8_t", "uint16_t", "uint32_t", "uint64_t"
    };
    for (uint32_t i = 0; i < sizeof(words) / sizeof(words[0]); ++i) if (tokenIs(text, token, words[i])) return true;
    return false;
}

static bool isControlWord(const char* text, const TypeToken& token) {
    static const char* const words[] = { "if", "for", "while", "switch", "catch", "return", "throw", "case", "delete", "new" };
    for (uint32_t i = 0; i < sizeof(words) / sizeof(words[0]); ++i) if (tokenIs(text, token, words[i])) return true;
    return false;
}

static bool canonicalPrimitive(const char* base, char* output, uint32_t capacity) {
    bool hasUnsigned = false, hasSigned = false, hasShort = false, hasLong = false;
    uint32_t longCount = 0;
    bool hasInt = false, hasChar = false, hasBool = false, hasVoid = false;
    bool hasFloat = false, hasDouble = false;
    uint32_t start = 0;
    const uint32_t length = lengthOf(base, kTypeMaxCanonicalNameBytes);
    while (start < length) {
        while (start < length && base[start] == ' ') ++start;
        uint32_t end = start;
        while (end < length && base[end] != ' ') ++end;
        char word[32] = {};
        copyRange(word, sizeof(word), base, start, end - start);
        if (equalText(word, "unsigned")) hasUnsigned = true;
        else if (equalText(word, "signed")) hasSigned = true;
        else if (equalText(word, "short")) hasShort = true;
        else if (equalText(word, "long")) { hasLong = true; ++longCount; }
        else if (equalText(word, "int")) hasInt = true;
        else if (equalText(word, "char")) hasChar = true;
        else if (equalText(word, "bool")) hasBool = true;
        else if (equalText(word, "void")) hasVoid = true;
        else if (equalText(word, "float")) hasFloat = true;
        else if (equalText(word, "double")) hasDouble = true;
        else if (equalText(word, "size_t") || equalText(word, "int8_t") || equalText(word, "int16_t") ||
                 equalText(word, "int32_t") || equalText(word, "int64_t") || equalText(word, "uint8_t") ||
                 equalText(word, "uint16_t") || equalText(word, "uint32_t") || equalText(word, "uint64_t")) {
            copyText(output, capacity, word);
            return true;
        } else return false;
        start = end;
    }
    output[0] = '\0';
    uint32_t out = 0;
    if (hasVoid) copyText(output, capacity, "void");
    else if (hasBool) copyText(output, capacity, "bool");
    else if (hasFloat) copyText(output, capacity, "float");
    else if (hasDouble) copyText(output, capacity, hasLong ? "long double" : "double");
    else if (hasChar) { if (hasUnsigned) copyText(output, capacity, "unsigned char"); else if (hasSigned) copyText(output, capacity, "signed char"); else copyText(output, capacity, "char"); }
    else if (hasShort) copyText(output, capacity, hasUnsigned ? "unsigned short" : hasSigned ? "signed short" : "short");
    else if (longCount >= 2) copyText(output, capacity, hasUnsigned ? "unsigned long long" : "long long");
    else if (hasLong) copyText(output, capacity, hasUnsigned ? "unsigned long" : hasSigned ? "signed long" : "long");
    else if (hasUnsigned) copyText(output, capacity, "unsigned int");
    else if (hasSigned) copyText(output, capacity, "signed int");
    else if (hasInt) copyText(output, capacity, "int");
    else return false;
    (void)out;
    return output[0] != '\0';
}

struct ParseContext;
static void setTypeLocation(TypeInfo& type, const ParseContext& context, uint32_t offset, uint32_t length);

struct ParseContext {
    TypeDatabase* database;
    const char* text;
    uint32_t length;
    uint32_t documentIndex;
    uint64_t documentId;
    uint32_t documentGeneration;
    uint64_t projectGeneration;
    char relativePath[kMaxPathBytes];
    bool truncated;
};

static void setTypeLocation(TypeInfo& type, const ParseContext& context, uint32_t offset, uint32_t length) {
    setLocation(type.declarationLocation, context.relativePath, context.documentId,
                context.documentGeneration, context.projectGeneration, context.text,
                context.length, offset, length);
}

static void makeBaseType(const ParseContext& context, uint32_t first, uint32_t end,
                         TypeSource source, TypeInfo& output) {
    clearTypeInfo(output);
    output.confidence = TypeConfidence::Exact;
    output.source = source;
    if (first >= end || end > g_tokenCount) { output.confidence = TypeConfidence::Unknown; return; }
    bool copyTruncated = false;
    uint32_t start = g_tokens[first].offset;
    uint32_t finish = g_tokens[end - 1].offset + g_tokens[end - 1].length;
    if (finish < start || finish - start > kTypeMaxDeclaratorBytes) {
        output.truncated = true;
        finish = start + kTypeMaxDeclaratorBytes < context.length ? start + kTypeMaxDeclaratorBytes : context.length;
    }
    copyRange(output.spelling, sizeof(output.spelling), context.text, start, finish - start, &copyTruncated);
    output.truncated = output.truncated || copyTruncated;
    char base[sizeof(output.baseName)] = {};
    bool haveBase = false;
    bool primitiveOnly = true;
    uint32_t pointerCount = 0;
    for (uint32_t i = first; i < end; ++i) {
        const TypeToken& token = g_tokens[i];
        if (tokenIs(context.text, token, "const")) { output.constQualified = true; continue; }
        if (tokenIs(context.text, token, "volatile")) { output.volatileQualified = true; continue; }
        if (isStorageWord(context.text, token)) continue;
        if (isPunctuation(context.text, token, "*")) { if (pointerCount < 65535u) ++pointerCount; continue; }
        if (isPunctuation(context.text, token, "&")) { output.referenceKind = TypeReferenceKind::LValue; continue; }
        if (isPunctuation(context.text, token, "&&")) { output.referenceKind = TypeReferenceKind::RValue; continue; }
        if (tokenIs(context.text, token, "struct") || tokenIs(context.text, token, "class") ||
            tokenIs(context.text, token, "enum") || tokenIs(context.text, token, "union") ||
            tokenIs(context.text, token, "typename")) continue;
        if (!tokenIdentifier(token)) continue;
        if (!haveBase) { haveBase = true; base[0] = '\0'; }
        if (!isPrimitiveWord(context.text, token)) primitiveOnly = false;
    }
    // Rebuild the base text with an independent length.  Keeping this second
    // pass simple avoids making the persistent TypeInfo depend on token data.
    base[0] = '\0';
    uint32_t baseLength = 0;
    for (uint32_t i = first; i < end; ++i) {
        const TypeToken& token = g_tokens[i];
        if (!tokenIdentifier(token) || isStorageWord(context.text, token) || isQualifier(context.text, token) ||
            tokenIs(context.text, token, "struct") || tokenIs(context.text, token, "class") ||
            tokenIs(context.text, token, "enum") || tokenIs(context.text, token, "union") || tokenIs(context.text, token, "typename")) continue;
        if (baseLength > 0) appendChar(base, sizeof(base), baseLength, ' ');
        appendTokenText(base, sizeof(base), baseLength, context.text, token);
    }
    if (!haveBase) { output.confidence = TypeConfidence::Unknown; return; }
    char canonical[sizeof(output.canonicalName)] = {};
    if (primitiveOnly && canonicalPrimitive(base, canonical, sizeof(canonical))) {
        copyText(output.baseName, sizeof(output.baseName), canonical);
        copyText(output.canonicalName, sizeof(output.canonicalName), canonical);
        output.baseKind = equalText(canonical, "void") ? TypeBaseKind::Named : TypeBaseKind::Primitive;
    } else {
        uint32_t lastStart = 0;
        for (uint32_t i = first; i < end; ++i) if (tokenIdentifier(g_tokens[i]) && !isQualifier(context.text, g_tokens[i])) lastStart = i;
        tokenName(context.text, g_tokens[lastStart], output.baseName, sizeof(output.baseName));
        copyText(output.canonicalName, sizeof(output.canonicalName), output.baseName);
        output.baseKind = TypeBaseKind::Named;
    }
    output.pointerDepth = static_cast<uint16_t>(pointerCount);
    setTypeLocation(output, context, g_tokens[first].offset, g_tokens[first].length);
}

static bool likelyTypeRange(const ParseContext& context, uint32_t first, uint32_t end) {
    if (first >= end || end > g_tokenCount) return false;
    if (isControlWord(context.text, g_tokens[first])) return false;
    bool hasPointerOrReference = false;
    uint32_t identifiers = 0;
    bool hasAuto = false;
    for (uint32_t i = first; i < end; ++i) {
        if (tokenIdentifier(g_tokens[i])) ++identifiers;
        if (isPunctuation(context.text, g_tokens[i], "*") || isPunctuation(context.text, g_tokens[i], "&") || isPunctuation(context.text, g_tokens[i], "&&")) hasPointerOrReference = true;
        if (tokenIs(context.text, g_tokens[i], "auto")) hasAuto = true;
    }
    if (hasAuto || hasPointerOrReference || identifiers >= 1) return true;
    return false;
}

static TypeRecord* addRecord(ParseContext& context, TypeDeclarationKind kind,
                             const char* name, uint32_t nameOffset, uint32_t nameLength,
                             uint32_t scopeStart, uint32_t scopeEnd, const char* container,
                             const TypeInfo& type) {
    TypeDatabase* database = context.database;
    if (!database || database->recordCount >= database->recordCapacity) {
        if (database) { database->truncated = true; ++database->droppedRecords; }
        context.truncated = true;
        return nullptr;
    }
    TypeRecord& record = database->records[database->recordCount];
    record = TypeRecord();
    record.used = true;
    record.kind = kind;
    record.ordinal = database->recordCount;
    record.documentIndex = context.documentIndex;
    record.documentId = context.documentId;
    record.documentGeneration = context.documentGeneration;
    record.declarationOffset = nameOffset;
    record.scopeStart = scopeStart;
    record.scopeEnd = scopeEnd;
    record.scopeDepth = g_tokens[0].braceDepth;
    copyText(record.name, sizeof(record.name), name);
    copyText(record.container, sizeof(record.container), container ? container : "");
    if (record.container[0] != '\0') {
        copyText(record.qualifiedName, sizeof(record.qualifiedName), record.container);
        uint32_t q = lengthOf(record.qualifiedName, sizeof(record.qualifiedName));
        appendText(record.qualifiedName, sizeof(record.qualifiedName), q, "::");
        appendText(record.qualifiedName, sizeof(record.qualifiedName), q, record.name);
    } else copyText(record.qualifiedName, sizeof(record.qualifiedName), record.name);
    record.type = type;
    setTypeLocation(record.type, context, nameOffset, nameLength);
    ++database->recordCount;
    return &record;
}

static int32_t findDocument(const TypeDatabase* database, const char* path) {
    if (!database || !path) return -1;
    for (uint32_t i = 0; i < database->documentCount; ++i)
        if (database->documents[i].used && equalText(database->documents[i].absolutePath, path)) return static_cast<int32_t>(i);
    return -1;
}

static void removeDocumentRecords(TypeDatabase* database, uint32_t documentIndex) {
    if (!database) return;
    uint32_t write = 0;
    for (uint32_t i = 0; i < database->recordCount; ++i) {
        if (database->records[i].documentIndex == documentIndex) continue;
        if (write != i) database->records[write] = database->records[i];
        database->records[write].ordinal = write;
        ++write;
    }
    database->recordCount = write;
}

static int32_t ensureDocument(ParseContext& context, const char* absolutePath) {
    TypeDatabase* database = context.database;
    int32_t existing = findDocument(database, absolutePath);
    if (existing >= 0) {
        removeDocumentRecords(database, static_cast<uint32_t>(existing));
        TypeDocument& document = database->documents[existing];
        document.documentId = context.documentId;
        document.generation = context.documentGeneration;
        document.recordCount = 0;
        copyText(document.relativePath, sizeof(document.relativePath), context.relativePath);
        return existing;
    }
    if (!database || database->documentCount >= database->documentCapacity) {
        if (database) { database->truncated = true; ++database->droppedDocuments; }
        context.truncated = true;
        return -1;
    }
    const uint32_t index = database->documentCount++;
    TypeDocument& document = database->documents[index];
    document = TypeDocument();
    document.used = true;
    document.documentId = context.documentId;
    document.generation = context.documentGeneration;
    copyText(document.relativePath, sizeof(document.relativePath), context.relativePath);
    copyText(document.absolutePath, sizeof(document.absolutePath), absolutePath);
    return static_cast<int32_t>(index);
}

static void classForOffset(const TypeDatabase* database, uint32_t documentIndex, uint32_t offset,
                           char* output, uint32_t capacity) {
    output[0] = '\0';
    uint32_t bestSize = UINT32_MAX;
    for (uint32_t i = 0; i < database->recordCount; ++i) {
        const TypeRecord& record = database->records[i];
        if (!record.used || record.documentIndex != documentIndex ||
            (record.kind != TypeDeclarationKind::Class && record.kind != TypeDeclarationKind::Struct &&
             record.kind != TypeDeclarationKind::Enum && record.kind != TypeDeclarationKind::Union)) continue;
        if (record.scopeStart <= offset && offset < record.scopeEnd) {
            const uint32_t size = record.scopeEnd > record.scopeStart ? record.scopeEnd - record.scopeStart : UINT32_MAX;
            if (size < bestSize) { bestSize = size; copyText(output, capacity, record.qualifiedName); }
        }
    }
}

static int32_t functionForOffset(const TypeDatabase* database, uint32_t documentIndex, uint32_t offset) {
    int32_t found = -1;
    uint32_t bestSize = UINT32_MAX;
    for (uint32_t i = 0; i < database->recordCount; ++i) {
        const TypeRecord& record = database->records[i];
        if (!record.used || record.documentIndex != documentIndex || record.kind != TypeDeclarationKind::Function) continue;
        if (record.scopeStart <= offset && offset < record.scopeEnd) {
            const uint32_t size = record.scopeEnd > record.scopeStart ? record.scopeEnd - record.scopeStart : UINT32_MAX;
            if (size < bestSize) { bestSize = size; found = static_cast<int32_t>(i); }
        }
    }
    return found;
}

static bool inferLiteral(const ParseContext& context, uint32_t first, uint32_t end, TypeInfo& output) {
    if (first >= end || first >= g_tokenCount) return false;
    const TypeToken& token = g_tokens[first];
    clearTypeInfo(output);
    output.confidence = TypeConfidence::Conservative;
    output.source = TypeSource::LiteralInference;
    setTypeLocation(output, context, token.offset, token.length);
    char literal[kTypeMaxTypeSpellingBytes + 1] = {};
    tokenName(context.text, token, literal, sizeof(literal));
    if (token.kind == TypeTokenNumber) {
        bool floating = false;
        for (uint32_t i = 0; literal[i] != '\0'; ++i) if (literal[i] == '.' || literal[i] == 'e' || literal[i] == 'E') floating = true;
        if (floating && (literal[lengthOf(literal, sizeof(literal)) - 1] == 'f' || literal[lengthOf(literal, sizeof(literal)) - 1] == 'F')) copyText(output.spelling, sizeof(output.spelling), "float");
        else if (floating) copyText(output.spelling, sizeof(output.spelling), "double");
        else if (literal[lengthOf(literal, sizeof(literal)) - 1] == 'u' || literal[lengthOf(literal, sizeof(literal)) - 1] == 'U') copyText(output.spelling, sizeof(output.spelling), "unsigned int");
        else if (lengthOf(literal, sizeof(literal)) >= 2 && (literal[lengthOf(literal, sizeof(literal)) - 2] == 'L' || literal[lengthOf(literal, sizeof(literal)) - 2] == 'l')) copyText(output.spelling, sizeof(output.spelling), "unsigned long long");
        else copyText(output.spelling, sizeof(output.spelling), "int");
        copyText(output.baseName, sizeof(output.baseName), output.spelling);
        copyText(output.canonicalName, sizeof(output.canonicalName), output.spelling);
        output.baseKind = TypeBaseKind::Primitive;
        return true;
    }
    if (tokenIs(context.text, token, "true") || tokenIs(context.text, token, "false")) {
        copyText(output.spelling, sizeof(output.spelling), "bool");
        copyText(output.baseName, sizeof(output.baseName), "bool");
        copyText(output.canonicalName, sizeof(output.canonicalName), "bool");
        output.baseKind = TypeBaseKind::Primitive;
        return true;
    }
    if (token.kind == TypeTokenCharacter) {
        copyText(output.spelling, sizeof(output.spelling), "char");
        copyText(output.baseName, sizeof(output.baseName), "char");
        copyText(output.canonicalName, sizeof(output.canonicalName), "char");
        output.baseKind = TypeBaseKind::Primitive;
        return true;
    }
    if (token.kind == TypeTokenString) {
        copyText(output.spelling, sizeof(output.spelling), "const char*");
        copyText(output.baseName, sizeof(output.baseName), "char");
        copyText(output.canonicalName, sizeof(output.canonicalName), "char");
        output.baseKind = TypeBaseKind::Primitive;
        output.pointerDepth = 1;
        output.constQualified = true;
        return true;
    }
    if (tokenIs(context.text, token, "nullptr")) {
        copyText(output.spelling, sizeof(output.spelling), "nullptr_t");
        copyText(output.baseName, sizeof(output.baseName), "nullptr_t");
        copyText(output.canonicalName, sizeof(output.canonicalName), "nullptr_t");
        output.baseKind = TypeBaseKind::NullptrLiteral;
        return true;
    }
    return false;
}

static void parseTypeDeclarations(ParseContext& context) {
    for (uint32_t i = 0; i < g_tokenCount; ++i) {
        if (!tokenIdentifier(g_tokens[i])) continue;
        TypeDeclarationKind kind = TypeDeclarationKind::Unknown;
        if (tokenTextEqual(context.text, i, "class")) kind = TypeDeclarationKind::Class;
        else if (tokenTextEqual(context.text, i, "struct")) kind = TypeDeclarationKind::Struct;
        else if (tokenTextEqual(context.text, i, "enum")) kind = TypeDeclarationKind::Enum;
        else if (tokenTextEqual(context.text, i, "union")) kind = TypeDeclarationKind::Union;
        else continue;
        uint32_t nameIndex = i + 1;
        if (nameIndex < g_tokenCount && tokenTextEqual(context.text, nameIndex, "class")) ++nameIndex;
        if (nameIndex >= g_tokenCount || !tokenIdentifier(g_tokens[nameIndex])) continue;
        char name[kTypeMaxCanonicalNameBytes + 1] = {};
        tokenName(context.text, g_tokens[nameIndex], name, sizeof(name));
        uint32_t body = nameIndex + 1;
        while (body < g_tokenCount && !isPunctuation(context.text, g_tokens[body], "{") &&
               !isPunctuation(context.text, g_tokens[body], ";")) ++body;
        uint32_t scopeStart = 0;
        uint32_t scopeEnd = UINT32_MAX;
        if (body < g_tokenCount && isPunctuation(context.text, g_tokens[body], "{")) {
            scopeStart = g_tokens[body].offset + g_tokens[body].length;
            if (g_matchingBraces[body] != UINT32_MAX) scopeEnd = g_tokens[g_matchingBraces[body]].offset;
        }
        TypeInfo type;
        clearTypeInfo(type);
        type.baseKind = TypeBaseKind::Named;
        type.confidence = TypeConfidence::Exact;
        type.source = TypeSource::GlobalDeclaration;
        copyText(type.spelling, sizeof(type.spelling), name);
        copyText(type.baseName, sizeof(type.baseName), name);
        copyText(type.canonicalName, sizeof(type.canonicalName), name);
        setTypeLocation(type, context, g_tokens[nameIndex].offset, g_tokens[nameIndex].length);
        char container[kTypeMaxQualifiedNameBytes + 1] = {};
        classForOffset(context.database, context.documentIndex, g_tokens[nameIndex].offset, container, sizeof(container));
        addRecord(context, kind, name, g_tokens[nameIndex].offset, g_tokens[nameIndex].length,
                  scopeStart, scopeEnd, container, type);
    }
}

static uint32_t statementStart(const char* text, uint32_t tokenIndex) {
    uint32_t start = tokenIndex;
    while (start > 0 && !isPunctuation(text, g_tokens[start - 1], ";") &&
           !isPunctuation(text, g_tokens[start - 1], "{") &&
           !isPunctuation(text, g_tokens[start - 1], "}")) --start;
    return start;
}

static void parseAliases(ParseContext& context) {
    for (uint32_t i = 0; i < g_tokenCount; ++i) {
        if (!tokenIdentifier(g_tokens[i])) continue;
        const bool typedefForm = tokenTextEqual(context.text, i, "typedef");
        const bool usingForm = tokenTextEqual(context.text, i, "using");
        if (!typedefForm && !usingForm) continue;
        uint32_t end = i + 1;
        uint32_t equal = UINT32_MAX;
        while (end < g_tokenCount && !isPunctuation(context.text, g_tokens[end], ";")) {
            if (isPunctuation(context.text, g_tokens[end], "=")) equal = end;
            ++end;
        }
        if (end >= g_tokenCount) continue;
        uint32_t nameIndex = UINT32_MAX;
        uint32_t typeStart = i + 1;
        if (usingForm) {
            if (i + 1 >= g_tokenCount || !tokenIdentifier(g_tokens[i + 1]) || equal == UINT32_MAX) continue;
            nameIndex = i + 1;
            typeStart = equal + 1;
        } else {
            for (uint32_t j = i + 1; j < end; ++j) if (tokenIdentifier(g_tokens[j])) nameIndex = j;
            if (nameIndex == UINT32_MAX) continue;
        }
        if (typeStart >= nameIndex && !usingForm) continue;
        const uint32_t typeEnd = usingForm ? end : nameIndex;
        if (typeStart >= typeEnd || !likelyTypeRange(context, typeStart, typeEnd)) continue;
        TypeInfo type;
        makeBaseType(context, typeStart, typeEnd, usingForm ? TypeSource::UsingAlias : TypeSource::Typedef, type);
        char name[kTypeMaxCanonicalNameBytes + 1] = {};
        tokenName(context.text, g_tokens[nameIndex], name, sizeof(name));
        char target[kTypeMaxCanonicalNameBytes + 1] = {};
        copyText(target, sizeof(target), type.baseName);
        TypeRecord* record = addRecord(context, usingForm ? TypeDeclarationKind::UsingAlias : TypeDeclarationKind::Typedef,
                                       name, g_tokens[nameIndex].offset, g_tokens[nameIndex].length,
                                       0, UINT32_MAX, "", type);
        if (record) copyText(record->aliasTarget, sizeof(record->aliasTarget), target);
    }
}

static uint32_t matchingParen(const char* text, uint32_t open) {
    uint32_t depth = 0;
    for (uint32_t i = open; i < g_tokenCount; ++i) {
        if (isPunctuation(text, g_tokens[i], "(")) ++depth;
        else if (isPunctuation(text, g_tokens[i], ")")) {
            if (depth == 0) return UINT32_MAX;
            --depth;
            if (depth == 0) return i;
        }
    }
    return UINT32_MAX;
}

static void parseParameters(ParseContext& context, uint32_t open, uint32_t close,
                            uint32_t scopeStart, uint32_t scopeEnd, uint32_t ownerOffset,
                            const char* container) {
    uint32_t segmentStart = open + 1;
    uint32_t parenDepth = 0;
    for (uint32_t i = open + 1; i <= close; ++i) {
        if (i < close && isPunctuation(context.text, g_tokens[i], "(")) ++parenDepth;
        else if (i < close && isPunctuation(context.text, g_tokens[i], ")") && parenDepth > 0) --parenDepth;
        const bool separator = i == close || (i < close && isPunctuation(context.text, g_tokens[i], ",") && parenDepth == 0);
        if (!separator) continue;
        if (segmentStart < i && !(segmentStart + 1 == i && tokenTextEqual(context.text, segmentStart, "void"))) {
            uint32_t equal = i;
            for (uint32_t j = segmentStart; j < i; ++j) if (isPunctuation(context.text, g_tokens[j], "=")) { equal = j; break; }
            uint32_t nameIndex = UINT32_MAX;
            for (uint32_t j = segmentStart; j < equal; ++j) if (tokenIdentifier(g_tokens[j])) nameIndex = j;
            if (nameIndex != UINT32_MAX && likelyTypeRange(context, segmentStart, nameIndex)) {
                char name[kTypeMaxCanonicalNameBytes + 1] = {};
                tokenName(context.text, g_tokens[nameIndex], name, sizeof(name));
                TypeInfo type;
                makeBaseType(context, segmentStart, nameIndex, TypeSource::FunctionParameter, type);
                TypeRecord* record = addRecord(context, TypeDeclarationKind::Parameter, name,
                                               g_tokens[nameIndex].offset, g_tokens[nameIndex].length,
                                               scopeStart, scopeEnd, container, type);
                if (record) record->ownerOffset = ownerOffset;
            }
        }
        segmentStart = i + 1;
    }
}

static void parseFunctions(ParseContext& context) {
    for (uint32_t open = 0; open < g_tokenCount; ++open) {
        if (!isPunctuation(context.text, g_tokens[open], "(") || open == 0 || !tokenIdentifier(g_tokens[open - 1])) continue;
        const uint32_t close = matchingParen(context.text, open);
        if (close == UINT32_MAX) { context.truncated = true; continue; }
        if (isControlWord(context.text, g_tokens[open - 1]) || tokenTextEqual(context.text, open - 1, "sizeof")) continue;
        uint32_t start = statementStart(context.text, open - 1);
        while (start < open && (isStorageWord(context.text, g_tokens[start]) || tokenTextEqual(context.text, start, "friend"))) ++start;
        bool hasInitializerMarker = false;
        for (uint32_t j = start; j < open - 1; ++j)
            if (isPunctuation(context.text, g_tokens[j], "=")) hasInitializerMarker = true;
        if (hasInitializerMarker) continue;
        if (start >= open - 1 || !likelyTypeRange(context, start, open - 1)) continue;
        TypeInfo returnType;
        makeBaseType(context, start, open - 1, TypeSource::FunctionReturn, returnType);
        char name[kTypeMaxCanonicalNameBytes + 1] = {};
        tokenName(context.text, g_tokens[open - 1], name, sizeof(name));
        uint32_t after = close + 1;
        while (after < g_tokenCount && (tokenTextEqual(context.text, after, "const") ||
               tokenTextEqual(context.text, after, "noexcept") || tokenTextEqual(context.text, after, "override") ||
               tokenTextEqual(context.text, after, "final") || isPunctuation(context.text, g_tokens[after], "&") ||
               isPunctuation(context.text, g_tokens[after], "&&"))) ++after;
        uint32_t scopeStart = g_tokens[open].offset;
        uint32_t scopeEnd = UINT32_MAX;
        if (after < g_tokenCount && isPunctuation(context.text, g_tokens[after], "{")) {
            scopeStart = g_tokens[after].offset + g_tokens[after].length;
            if (g_matchingBraces[after] != UINT32_MAX) scopeEnd = g_tokens[g_matchingBraces[after]].offset;
        } else if (after < g_tokenCount && isPunctuation(context.text, g_tokens[after], ";")) {
            scopeEnd = g_tokens[after].offset + g_tokens[after].length;
        }
        char container[kTypeMaxQualifiedNameBytes + 1] = {};
        classForOffset(context.database, context.documentIndex, g_tokens[open - 1].offset, container, sizeof(container));
        TypeDeclarationKind kind = container[0] ? TypeDeclarationKind::Function : TypeDeclarationKind::Function;
        TypeRecord* function = addRecord(context, kind, name, g_tokens[open - 1].offset, g_tokens[open - 1].length,
                                          scopeStart, scopeEnd, container, returnType);
        if (function) parseParameters(context, open, close, scopeStart, scopeEnd,
                                      g_tokens[open - 1].offset, container);
    }
}

static void parseVariableSegment(ParseContext& context, uint32_t first, uint32_t end,
                                  const TypeInfo* inheritedType, const char* container) {
    if (first >= end || end > g_tokenCount) return;
    if (g_tokens[end - 1].offset + g_tokens[end - 1].length - g_tokens[first].offset > kTypeMaxDeclaratorBytes) {
        context.truncated = true;
        return;
    }
    if (tokenTextEqual(context.text, first, "typedef") || tokenTextEqual(context.text, first, "using") ||
        isControlWord(context.text, g_tokens[first])) return;
    uint32_t equal = end;
    for (uint32_t i = first; i < end; ++i) if (isPunctuation(context.text, g_tokens[i], "=")) { equal = i; break; }
    uint32_t nameIndex = UINT32_MAX;
    for (uint32_t i = first; i < equal; ++i) if (tokenIdentifier(g_tokens[i])) nameIndex = i;
    if (nameIndex == UINT32_MAX || (nameIndex == first && equal == first + 1)) return;
    for (uint32_t i = first; i < (equal < end ? equal : end); ++i)
        if (isPunctuation(context.text, g_tokens[i], "(")) return;
    TypeInfo type;
    if (inheritedType) type = *inheritedType;
    else {
        if (!likelyTypeRange(context, first, nameIndex)) return;
        makeBaseType(context, first, nameIndex, TypeSource::GlobalDeclaration, type);
        if (tokenTextEqual(context.text, first, "auto")) {
            clearTypeInfo(type);
            copyText(type.spelling, sizeof(type.spelling), "auto");
            copyText(type.baseName, sizeof(type.baseName), "auto");
            copyText(type.canonicalName, sizeof(type.canonicalName), "auto");
            type.confidence = TypeConfidence::Unknown;
            type.source = TypeSource::UnknownInference;
            type.baseKind = TypeBaseKind::Unknown;
            setTypeLocation(type, context, g_tokens[first].offset, g_tokens[first].length);
        }
    }
    char name[kTypeMaxCanonicalNameBytes + 1] = {};
    tokenName(context.text, g_tokens[nameIndex], name, sizeof(name));
    int32_t functionIndex = functionForOffset(context.database, context.documentIndex, g_tokens[nameIndex].offset);
    uint32_t scopeStart = 0, scopeEnd = UINT32_MAX;
    TypeDeclarationKind kind = TypeDeclarationKind::GlobalVariable;
    if (functionIndex >= 0) {
        const TypeRecord& function = context.database->records[functionIndex];
        scopeStart = function.scopeStart; scopeEnd = function.scopeEnd;
        kind = TypeDeclarationKind::LocalVariable;
        type.source = TypeSource::LocalDeclaration;
    } else if (container && container[0]) {
        kind = TypeDeclarationKind::Member;
        type.source = TypeSource::MemberDeclaration;
    } else type.source = TypeSource::GlobalDeclaration;
    TypeRecord* record = addRecord(context, kind, name, g_tokens[nameIndex].offset, g_tokens[nameIndex].length,
                                   scopeStart, scopeEnd, container, type);
    if (!record) return;
    uint32_t initializer = equal < end ? equal + 1 : end;
    if (initializer < end) {
        record->hasInitializer = true;
        bool initTruncated = false;
        copyRange(record->initializer, sizeof(record->initializer), context.text,
                  g_tokens[initializer].offset,
                  g_tokens[end - 1].offset + g_tokens[end - 1].length - g_tokens[initializer].offset,
                  &initTruncated);
        record->type.truncated = record->type.truncated || initTruncated;
        TypeInfo literal;
        if (inferLiteral(context, initializer, end, literal) && tokenTextEqual(context.text, first, "auto")) {
            record->type = literal;
            setTypeLocation(record->type, context, g_tokens[nameIndex].offset, g_tokens[nameIndex].length);
        }
    }
    if (nameIndex + 2 < end && isPunctuation(context.text, g_tokens[nameIndex + 1], "[")) {
        if (tokenNumber(g_tokens[nameIndex + 2])) {
            char extent[16] = {};
            tokenName(context.text, g_tokens[nameIndex + 2], extent, sizeof(extent));
            uint32_t value = 0;
            for (uint32_t i = 0; extent[i] >= '0' && extent[i] <= '9'; ++i) value = value * 10u + static_cast<uint32_t>(extent[i] - '0');
            record->type.hasArrayExtent = true;
            record->type.arrayExtent = value;
        }
    }
}

static void parseVariables(ParseContext& context) {
    uint32_t start = 0;
    uint32_t parenDepth = 0;
    for (uint32_t i = 0; i < g_tokenCount; ++i) {
        if (isPunctuation(context.text, g_tokens[i], "(")) ++parenDepth;
        else if (isPunctuation(context.text, g_tokens[i], ")") && parenDepth > 0) --parenDepth;
        if (isPunctuation(context.text, g_tokens[i], "{")) start = i + 1;
        else if (isPunctuation(context.text, g_tokens[i], "}")) { start = i + 1; parenDepth = 0; }
        else if (isPunctuation(context.text, g_tokens[i], ";") && parenDepth == 0) {
            uint32_t segmentStart = start;
            uint32_t commaDepth = 0;
            TypeInfo inherited;
            bool haveInherited = false;
            for (uint32_t j = start; j <= i; ++j) {
                if (j < i && (isPunctuation(context.text, g_tokens[j], "(") || isPunctuation(context.text, g_tokens[j], "["))) ++commaDepth;
                else if (j < i && (isPunctuation(context.text, g_tokens[j], ")") || isPunctuation(context.text, g_tokens[j], "]")) && commaDepth > 0) --commaDepth;
                const bool separator = j == i || (j < i && isPunctuation(context.text, g_tokens[j], ",") && commaDepth == 0);
                if (!separator) continue;
                char container[kTypeMaxQualifiedNameBytes + 1] = {};
                classForOffset(context.database, context.documentIndex,
                               segmentStart < j ? g_tokens[segmentStart].offset : 0,
                               container, sizeof(container));
                parseVariableSegment(context, segmentStart, j, haveInherited ? &inherited : nullptr, container);
                if (!haveInherited && context.database->recordCount > 0) {
                    const TypeRecord& latest = context.database->records[context.database->recordCount - 1];
                    if (latest.documentIndex == context.documentIndex && latest.declarationOffset >= (segmentStart < j ? g_tokens[segmentStart].offset : 0)) {
                        inherited = latest.type;
                        haveInherited = true;
                    }
                }
                segmentStart = j + 1;
            }
            start = i + 1;
        }
    }
}

static int32_t findAlias(const TypeDatabase* database, const char* name) {
    for (uint32_t i = 0; i < database->recordCount; ++i) {
        const TypeRecord& record = database->records[i];
        if ((record.kind == TypeDeclarationKind::Typedef || record.kind == TypeDeclarationKind::UsingAlias) && equalText(record.name, name)) return static_cast<int32_t>(i);
    }
    return -1;
}

static void resolveAliases(TypeDatabase* database) {
    for (uint32_t i = 0; i < database->recordCount; ++i) {
        TypeRecord& record = database->records[i];
        if (!record.used) continue;
        TypeInfo& type = record.type;
        const char* base = type.baseName;
        uint32_t visited[kTypeMaxAliasDepth] = {};
        uint32_t visitedCount = 0;
        for (uint32_t depth = 0; depth < kTypeMaxAliasDepth; ++depth) {
            int32_t aliasIndex = findAlias(database, base);
            if (aliasIndex < 0) break;
            bool seen = false;
            for (uint32_t v = 0; v < visitedCount; ++v) if (visited[v] == static_cast<uint32_t>(aliasIndex)) seen = true;
            if (seen) { type.aliasCycle = true; type.confidence = TypeConfidence::Unknown; type.source = TypeSource::UnknownInference; break; }
            if (visitedCount < kTypeMaxAliasDepth) visited[visitedCount++] = static_cast<uint32_t>(aliasIndex);
            if (type.aliasName[0] == '\0') copyText(type.aliasName, sizeof(type.aliasName), base);
            const TypeInfo target = database->records[aliasIndex].type;
            if (target.baseName[0] == '\0' || equalText(target.baseName, base)) { type.aliasCycle = true; type.confidence = TypeConfidence::Unknown; break; }
            if (type.pointerDepth == 0) type.pointerDepth = target.pointerDepth;
            type.constQualified = type.constQualified || target.constQualified;
            type.volatileQualified = type.volatileQualified || target.volatileQualified;
            copyText(type.baseName, sizeof(type.baseName), target.baseName);
            copyText(type.canonicalName, sizeof(type.canonicalName), target.canonicalName);
            copyText(type.resolvedAlias, sizeof(type.resolvedAlias), target.resolvedAlias[0] ? target.resolvedAlias : target.baseName);
            base = type.baseName;
        }
        if (visitedCount >= kTypeMaxAliasDepth && findAlias(database, base) >= 0) { type.truncated = true; type.confidence = TypeConfidence::Unknown; }
        if (record.kind == TypeDeclarationKind::Typedef || record.kind == TypeDeclarationKind::UsingAlias) {
            copyText(type.resolvedAlias, sizeof(type.resolvedAlias), type.resolvedAlias[0] ? type.resolvedAlias : type.baseName);
        }
    }
}

static void inferAutoTypes(TypeDatabase* database) {
    for (uint32_t i = 0; i < database->recordCount; ++i) {
        TypeRecord& record = database->records[i];
        if (!record.used || !record.hasInitializer || record.type.spelling[0] == '\0' || !equalText(record.type.spelling, "auto")) continue;
        char callName[kTypeMaxCanonicalNameBytes + 1] = {};
        const char* initializer = record.initializer;
        uint32_t pos = 0;
        while (initializer[pos] != '\0' && !isIdentifierStart(initializer[pos])) ++pos;
        uint32_t end = pos;
        while (initializer[end] != '\0' && isIdentifierPart(initializer[end])) ++end;
        if (end == pos) { record.type.confidence = TypeConfidence::Unknown; record.type.source = TypeSource::UnknownInference; continue; }
        copyRange(callName, sizeof(callName), initializer, pos, end - pos);
        while (initializer[end] == ' ' || initializer[end] == '\t') ++end;
        if (initializer[end] != '(') { record.type.confidence = TypeConfidence::Unknown; record.type.source = TypeSource::UnknownInference; continue; }
        int32_t found = -1;
        uint32_t matches = 0;
        for (uint32_t j = 0; j < database->recordCount; ++j) {
            if (database->records[j].kind != TypeDeclarationKind::Function || !equalText(database->records[j].name, callName)) continue;
            ++matches;
            found = static_cast<int32_t>(j);
            if (matches > kTypeMaxLookupCandidates) break;
        }
        if (matches != 1 || found < 0) {
            record.type.confidence = matches > 1 ? TypeConfidence::Ambiguous : TypeConfidence::Unknown;
            record.type.source = matches > 1 ? TypeSource::UnknownInference : TypeSource::UnknownInference;
            continue;
        }
        const TypeLocation declarationLocation = record.type.declarationLocation;
        TypeInfo inferred = database->records[found].type;
        inferred.confidence = TypeConfidence::Conservative;
        inferred.source = TypeSource::FunctionReturnInference;
        inferred.declarationLocation = database->records[found].type.declarationLocation;
        copyText(inferred.spelling, sizeof(inferred.spelling), database->records[found].type.spelling);
        inferred.declarationLocation = declarationLocation;
        record.type = inferred;
    }
}

static void parseDocument(ParseContext& context) {
    bool lexTruncated = false;
    if (!lexText(context.text, context.length, &lexTruncated)) return;
    context.truncated = context.truncated || lexTruncated;
    parseTypeDeclarations(context);
    parseAliases(context);
    parseFunctions(context);
    parseVariables(context);
    if (context.truncated) context.database->truncated = true;
    TypeDocument& document = context.database->documents[context.documentIndex];
    document.recordCount = 0;
    for (uint32_t i = 0; i < context.database->recordCount; ++i)
        if (context.database->records[i].documentIndex == context.documentIndex) ++document.recordCount;
}

static void updateRecordLocations(TypeDatabase* database) {
    for (uint32_t i = 0; i < database->recordCount; ++i) {
        TypeRecord& record = database->records[i];
        if (record.documentIndex >= database->documentCount) continue;
        const TypeDocument& document = database->documents[record.documentIndex];
        copyText(record.type.declarationLocation.relativePath, sizeof(record.type.declarationLocation.relativePath), document.relativePath);
        record.type.declarationLocation.documentId = record.documentId;
        record.type.declarationLocation.documentGeneration = record.documentGeneration;
        record.type.declarationLocation.projectGeneration = database->projectGeneration;
    }
}

static bool memberRecord(const TypeRecord& record) {
    return record.used && record.container[0] != '\0' &&
        (record.kind == TypeDeclarationKind::Member || record.kind == TypeDeclarationKind::Function);
}

static bool memberIndexBefore(const TypeDatabase* database, uint32_t leftIndex, uint32_t rightIndex) {
    const TypeRecord& left = database->records[leftIndex];
    const TypeRecord& right = database->records[rightIndex];
    int32_t owner = 0;
    const uint32_t ownerLength = lengthOf(left.container, sizeof(left.container));
    const uint32_t rightOwnerLength = lengthOf(right.container, sizeof(right.container));
    const uint32_t common = ownerLength < rightOwnerLength ? ownerLength : rightOwnerLength;
    for (uint32_t i = 0; i < common; ++i) {
        if (left.container[i] < right.container[i]) { owner = -1; break; }
        if (left.container[i] > right.container[i]) { owner = 1; break; }
    }
    if (owner == 0 && ownerLength != rightOwnerLength) owner = ownerLength < rightOwnerLength ? -1 : 1;
    if (owner != 0) return owner < 0;
    if (left.declarationOffset != right.declarationOffset)
        return left.declarationOffset < right.declarationOffset;
    return left.ordinal < right.ordinal;
}

static int32_t findMemberBucket(const TypeDatabase* database, const char* ownerName) {
    if (!database || !ownerName || ownerName[0] == '\0') return -1;
    for (uint32_t i = 0; i < database->memberBucketCount; ++i)
        if (equalText(database->memberBuckets[i].ownerName, ownerName)) return static_cast<int32_t>(i);
    return -1;
}

static void buildMemberIndex(TypeDatabase* database) {
    if (!database) return;
    database->memberBucketCount = 0;
    database->memberIndexCount = 0;
    database->memberIndexTruncated = false;
    if (!database->memberBuckets || database->memberBucketCapacity == 0 ||
        !database->memberIndices || database->memberIndexCapacity == 0) {
        return;
    }
    for (uint32_t i = 0; i < database->recordCount; ++i) {
        if (!memberRecord(database->records[i])) continue;
        if (database->memberIndexCount >= database->memberIndexCapacity) {
            database->memberIndexTruncated = true;
            continue;
        }
        database->memberIndices[database->memberIndexCount++] = i;
    }
    for (uint32_t i = 1; i < database->memberIndexCount; ++i) {
        const uint32_t value = database->memberIndices[i];
        uint32_t cursor = i;
        while (cursor > 0 && memberIndexBefore(database, value, database->memberIndices[cursor - 1])) {
            database->memberIndices[cursor] = database->memberIndices[cursor - 1];
            --cursor;
        }
        database->memberIndices[cursor] = value;
    }
    for (uint32_t i = 0; i < database->memberIndexCount; ++i) {
        const TypeRecord& record = database->records[database->memberIndices[i]];
        int32_t bucketIndex = findMemberBucket(database, record.container);
        if (bucketIndex < 0) {
            if (database->memberBucketCount >= database->memberBucketCapacity) {
                database->memberIndexTruncated = true;
                continue;
            }
            bucketIndex = static_cast<int32_t>(database->memberBucketCount++);
            TypeMemberBucket& bucket = database->memberBuckets[bucketIndex];
            bucket = TypeMemberBucket();
            copyText(bucket.ownerName, sizeof(bucket.ownerName), record.container);
            bucket.firstIndex = i;
        }
        TypeMemberBucket& bucket = database->memberBuckets[bucketIndex];
        ++bucket.memberCount;
    }
    if (database->memberIndexTruncated) database->truncated = true;
}

static int32_t recordAtIdentifier(const TypeDatabase* /*database*/, const Document& document, uint32_t offset,
                                  char* identifier, uint32_t identifierCapacity) {
    bool lexicalTruncated = false;
    if (!lexText(document.buffer.data, document.buffer.length, &lexicalTruncated)) return -1;
    (void)lexicalTruncated;
    for (uint32_t i = 0; i < g_tokenCount; ++i) {
        const TypeToken& token = g_tokens[i];
        if (!tokenIdentifier(token)) continue;
        if (offset >= token.offset && offset <= token.offset + token.length) {
            tokenName(document.buffer.data, token, identifier, identifierCapacity);
            return static_cast<int32_t>(token.offset);
        }
        if (offset > 0 && offset - 1 >= token.offset && offset - 1 < token.offset + token.length) {
            tokenName(document.buffer.data, token, identifier, identifierCapacity);
            return static_cast<int32_t>(token.offset);
        }
    }
    return -1;
}

static bool recordIsInScope(const TypeRecord& record, const Document& document, uint32_t offset) {
    if (record.documentId == document.documentId && record.documentGeneration != document.buffer.generation &&
        (record.kind == TypeDeclarationKind::LocalVariable || record.kind == TypeDeclarationKind::Parameter || record.kind == TypeDeclarationKind::Member)) return false;
    const uint32_t nameLength = lengthOf(record.name, sizeof(record.name));
    if (offset >= record.declarationOffset && offset <= record.declarationOffset + nameLength) return true;
    if (record.kind == TypeDeclarationKind::GlobalVariable || record.kind == TypeDeclarationKind::Function ||
        record.kind == TypeDeclarationKind::Class || record.kind == TypeDeclarationKind::Struct ||
        record.kind == TypeDeclarationKind::Enum || record.kind == TypeDeclarationKind::Union ||
        record.kind == TypeDeclarationKind::Typedef || record.kind == TypeDeclarationKind::UsingAlias) return true;
    if (record.scopeEnd != UINT32_MAX && !(record.scopeStart <= offset && offset < record.scopeEnd)) return false;
    return record.declarationOffset <= offset;
}

static bool betterScoped(const TypeRecord& candidate, const TypeRecord* current, uint32_t offset) {
    if (!current) return true;
    const bool candidateLocal = candidate.kind == TypeDeclarationKind::LocalVariable;
    const bool currentLocal = current->kind == TypeDeclarationKind::LocalVariable;
    const bool candidateParam = candidate.kind == TypeDeclarationKind::Parameter;
    const bool currentParam = current->kind == TypeDeclarationKind::Parameter;
    if (candidateLocal != currentLocal) return candidateLocal;
    if (candidateParam != currentParam && !candidateLocal && !currentLocal) return candidateParam;
    const uint32_t candidateSize = candidate.scopeEnd > candidate.scopeStart ? candidate.scopeEnd - candidate.scopeStart : UINT32_MAX;
    const uint32_t currentSize = current->scopeEnd > current->scopeStart ? current->scopeEnd - current->scopeStart : UINT32_MAX;
    if (candidateSize != currentSize) return candidateSize < currentSize;
    const bool candidateBefore = candidate.declarationOffset <= offset;
    const bool currentBefore = current->declarationOffset <= offset;
    if (candidateBefore != currentBefore) return candidateBefore;
    return candidate.declarationOffset > current->declarationOffset;
}

static TypeInspectionState inspectionState(TypeConfidence confidence) {
    switch (confidence) {
    case TypeConfidence::Exact: return TypeInspectionState::Exact;
    case TypeConfidence::Conservative: return TypeInspectionState::Conservative;
    case TypeConfidence::Ambiguous: return TypeInspectionState::Ambiguous;
    default: return TypeInspectionState::Unknown;
    }
}

} // namespace

const char* TypeConfidenceName(TypeConfidence confidence) {
    switch (confidence) {
    case TypeConfidence::Exact: return "Exact";
    case TypeConfidence::Conservative: return "Conservative";
    case TypeConfidence::Ambiguous: return "Ambiguous";
    default: return "Unknown";
    }
}
const char* TypeSourceName(TypeSource source) {
    switch (source) {
    case TypeSource::LocalDeclaration: return "local declaration";
    case TypeSource::FunctionParameter: return "function parameter";
    case TypeSource::MemberDeclaration: return "member declaration";
    case TypeSource::GlobalDeclaration: return "global declaration";
    case TypeSource::FunctionReturn: return "function return type";
    case TypeSource::Typedef: return "typedef";
    case TypeSource::UsingAlias: return "using alias";
    case TypeSource::LiteralInference: return "literal inference";
    case TypeSource::FunctionReturnInference: return "function return inference";
    default: return "unknown";
    }
}
const char* TypeDeclarationKindName(TypeDeclarationKind kind) {
    switch (kind) {
    case TypeDeclarationKind::LocalVariable: return "Local variable";
    case TypeDeclarationKind::Parameter: return "Function parameter";
    case TypeDeclarationKind::Member: return "Member";
    case TypeDeclarationKind::GlobalVariable: return "Global variable";
    case TypeDeclarationKind::Function: return "Function";
    case TypeDeclarationKind::Class: return "Class";
    case TypeDeclarationKind::Struct: return "Struct";
    case TypeDeclarationKind::Enum: return "Enum";
    case TypeDeclarationKind::Union: return "Union";
    case TypeDeclarationKind::Typedef: return "Typedef";
    case TypeDeclarationKind::UsingAlias: return "Using alias";
    default: return "Unknown";
    }
}
const char* TypeInspectionStateName(TypeInspectionState state) {
    switch (state) {
    case TypeInspectionState::Exact: return "Exact";
    case TypeInspectionState::Conservative: return "Conservative";
    case TypeInspectionState::Ambiguous: return "Ambiguous";
    case TypeInspectionState::Stale: return "Stale";
    default: return "Unknown";
    }
}
const char* TypeBaseKindName(TypeBaseKind kind) {
    switch (kind) {
    case TypeBaseKind::Primitive: return "primitive";
    case TypeBaseKind::Named: return "named";
    case TypeBaseKind::NullptrLiteral: return "nullptr literal";
    default: return "unknown";
    }
}
const char* TypeReferenceKindName(TypeReferenceKind kind) {
    switch (kind) {
    case TypeReferenceKind::LValue: return "lvalue reference";
    case TypeReferenceKind::RValue: return "rvalue reference";
    default: return "none";
    }
}

void TypeDatabaseInit(TypeDatabase* database, TypeRecord* recordStorage, uint32_t recordCapacity,
                      TypeDocument* documentStorage, uint32_t documentCapacity) {
    TypeDatabaseInit(database, recordStorage, recordCapacity, documentStorage, documentCapacity,
                     nullptr, 0, nullptr, 0);
}

void TypeDatabaseInit(TypeDatabase* database, TypeRecord* recordStorage, uint32_t recordCapacity,
                      TypeDocument* documentStorage, uint32_t documentCapacity,
                      TypeMemberBucket* memberBucketStorage, uint32_t memberBucketCapacity,
                      uint32_t* memberIndexStorage, uint32_t memberIndexCapacity) {
    if (!database) return;
    *database = TypeDatabase();
    database->records = recordStorage;
    database->documents = documentStorage;
    database->recordCapacity = recordCapacity;
    database->documentCapacity = documentCapacity;
    database->memberBuckets = memberBucketStorage;
    database->memberIndices = memberIndexStorage;
    database->memberBucketCapacity = memberBucketCapacity > kTypeMaxMemberBuckets
        ? kTypeMaxMemberBuckets : memberBucketCapacity;
    database->memberIndexCapacity = memberIndexCapacity > kTypeMaxRecords
        ? kTypeMaxRecords : memberIndexCapacity;
    clearBytes(recordStorage, recordCapacity * sizeof(TypeRecord));
    clearBytes(documentStorage, documentCapacity * sizeof(TypeDocument));
    clearBytes(memberBucketStorage, database->memberBucketCapacity * sizeof(TypeMemberBucket));
    clearBytes(memberIndexStorage, database->memberIndexCapacity * sizeof(uint32_t));
}

void TypeDatabaseClear(TypeDatabase* database) {
    if (!database) return;
    database->recordCount = 0;
    database->documentCount = 0;
    database->memberBucketCount = 0;
    database->memberIndexCount = 0;
    database->droppedRecords = 0;
    database->droppedDocuments = 0;
    database->projectGeneration = 0;
    database->symbolDatabaseGeneration = 0;
    database->current = false;
    database->truncated = false;
    database->memberIndexTruncated = false;
    database->rootPath[0] = '\0';
}

bool TypeDatabaseIndexDocument(TypeDatabase* database, const char* rootPath, const char* path,
                               uint64_t documentId, uint32_t documentGeneration,
                               uint64_t projectGeneration, const char* text, uint32_t length) {
    if (!database || !path || !text || !database->records || !database->documents) return false;
    if (length > kTypeMaxParserDocumentBytes) { database->truncated = true; length = kTypeMaxParserDocumentBytes; }
    ParseContext context = {};
    context.database = database;
    context.text = text;
    context.length = length;
    context.documentId = documentId;
    context.documentGeneration = documentGeneration;
    context.projectGeneration = projectGeneration;
    copyText(context.relativePath, sizeof(context.relativePath), path);
    makeRelativePath(rootPath, path, context.relativePath, sizeof(context.relativePath));
    int32_t documentIndex = ensureDocument(context, path);
    if (documentIndex < 0) return false;
    context.documentIndex = static_cast<uint32_t>(documentIndex);
    parseDocument(context);
    database->projectGeneration = projectGeneration;
    database->current = true;
    ++database->buildSequence;
    resolveAliases(database);
    inferAutoTypes(database);
    updateRecordLocations(database);
    buildMemberIndex(database);
    return true;
}

bool TypeDatabaseIndexProject(TypeDatabase* database, const WorkspaceFileSystem& fileSystem,
                              const char* rootPath, const Document* dirtyDocuments,
                              uint32_t dirtyDocumentCount, uint64_t projectGeneration,
                              const SymbolDatabase* symbolDatabase) {
    if (!database || !rootPath || !fileSystem.read || !symbolDatabase) return false;
    TypeDatabaseClear(database);
    copyText(database->rootPath, sizeof(database->rootPath), rootPath);
    bool traversed = true;
    uint32_t indexed = 0;
    for (uint32_t i = 0; i < SymbolDatabaseDocumentCount(symbolDatabase) && indexed < database->documentCapacity; ++i) {
        const SymbolDocument* document = SymbolDatabaseDocumentAt(symbolDatabase, i);
        if (!document || !document->used || !IsSymbolSourcePath(document->path)) continue;
        const Document* dirty = nullptr;
        for (uint32_t d = 0; d < dirtyDocumentCount; ++d)
            if (dirtyDocuments && dirtyDocuments[d].used && dirtyDocuments[d].buffer.dirty && equalText(dirtyDocuments[d].path, document->path)) dirty = &dirtyDocuments[d];
        uint32_t bytes = 0;
        uint64_t id = document->documentId;
        uint32_t generation = document->generation;
        const char* source = nullptr;
        if (dirty) { source = dirty->buffer.data; bytes = dirty->buffer.length; id = dirty->documentId; generation = dirty->buffer.generation; }
        else {
            FileInfo info = {};
            if (fileSystem.stat && fileSystem.stat(fileSystem.userData, document->path, &info) && info.size > kTypeMaxParserDocumentBytes) { database->truncated = true; continue; }
            if (!fileSystem.read(fileSystem.userData, document->path, g_typeReadBuffer, kTypeMaxParserDocumentBytes, &bytes) || bytes > kTypeMaxParserDocumentBytes) { database->truncated = true; traversed = false; continue; }
            source = g_typeReadBuffer;
        }
        if (TypeDatabaseIndexDocument(database, rootPath, document->path, id, generation, projectGeneration, source, bytes)) ++indexed;
    }
    for (uint32_t d = 0; d < dirtyDocumentCount && indexed < database->documentCapacity; ++d) {
        if (!dirtyDocuments || !dirtyDocuments[d].used || !dirtyDocuments[d].buffer.dirty || !IsSymbolSourcePath(dirtyDocuments[d].path)) continue;
        if (findDocument(database, dirtyDocuments[d].path) >= 0) continue;
        if (TypeDatabaseIndexDocument(database, rootPath, dirtyDocuments[d].path, dirtyDocuments[d].documentId,
                                       dirtyDocuments[d].buffer.generation, projectGeneration,
                                       dirtyDocuments[d].buffer.data, dirtyDocuments[d].buffer.length)) ++indexed;
    }
    database->projectGeneration = projectGeneration;
    database->symbolDatabaseGeneration = symbolDatabase->symbolDatabaseGeneration;
    database->current = traversed;
    resolveAliases(database);
    inferAutoTypes(database);
    updateRecordLocations(database);
    buildMemberIndex(database);
    return traversed;
}

bool TypeDatabaseIsCurrent(const TypeDatabase* database, uint64_t projectGeneration,
                           uint64_t symbolDatabaseGeneration) {
    return database && database->current && database->projectGeneration == projectGeneration &&
        database->symbolDatabaseGeneration == symbolDatabaseGeneration;
}
bool TypeDatabaseIsTruncated(const TypeDatabase* database) { return database && database->truncated; }
uint32_t TypeDatabaseRecordCount(const TypeDatabase* database) { return database ? database->recordCount : 0; }
const TypeRecord* TypeDatabaseRecordAt(const TypeDatabase* database, uint32_t index) {
    return database && index < database->recordCount ? &database->records[index] : nullptr;
}

uint32_t TypeDatabaseLookupDirectMembers(const TypeDatabase* database, const char* ownerName,
                                         uint32_t* indices, uint32_t capacity, bool* truncated) {
    if (truncated) *truncated = false;
    if (!database || !ownerName || ownerName[0] == '\0' || !database->memberBuckets ||
        !database->memberIndices || !indices || capacity == 0) return 0;
    const int32_t bucketIndex = findMemberBucket(database, ownerName);
    if (bucketIndex < 0) return 0;
    const TypeMemberBucket& bucket = database->memberBuckets[bucketIndex];
    const uint32_t retained = bucket.memberCount < capacity ? bucket.memberCount : capacity;
    for (uint32_t i = 0; i < retained; ++i)
        indices[i] = database->memberIndices[bucket.firstIndex + i];
    if (truncated) *truncated = database->memberIndexTruncated || bucket.memberCount > capacity;
    return retained;
}

bool TypeDatabaseInspectAt(const TypeDatabase* database, const Document& document,
                           uint64_t projectGeneration, uint32_t caretOffset,
                           TypeInspection* output) {
    if (!output) return false;
    *output = TypeInspection();
    output->state = TypeInspectionState::Unknown;
    output->truncated = database && database->truncated;
    char identifier[kTypeMaxCanonicalNameBytes + 1] = {};
    const int32_t offset = recordAtIdentifier(database, document, caretOffset, identifier, sizeof(identifier));
    copyText(output->identifier, sizeof(output->identifier), identifier);
    if (offset < 0 || !database || !database->current || database->projectGeneration != projectGeneration) {
        if (database && database->projectGeneration != projectGeneration) output->state = TypeInspectionState::Stale;
        return false;
    }
    const TypeRecord* best = nullptr;
    uint32_t matches = 0;
    for (uint32_t i = 0; i < database->recordCount; ++i) {
        const TypeRecord& record = database->records[i];
        if (!record.used || !equalText(record.name, identifier) || !recordIsInScope(record, document, static_cast<uint32_t>(offset))) continue;
        if (record.kind == TypeDeclarationKind::GlobalVariable || record.kind == TypeDeclarationKind::Function ||
            record.kind == TypeDeclarationKind::Class || record.kind == TypeDeclarationKind::Struct ||
            record.kind == TypeDeclarationKind::Enum || record.kind == TypeDeclarationKind::Union ||
            record.kind == TypeDeclarationKind::Typedef || record.kind == TypeDeclarationKind::UsingAlias) ++matches;
        if (betterScoped(record, best, static_cast<uint32_t>(offset))) best = &record;
    }
    if (matches == 0 && best) matches = 1;
    if (!best) return false;
    output->available = true;
    output->declarationKind = best->kind;
    output->type = best->type;
    if (best->kind == TypeDeclarationKind::Typedef || best->kind == TypeDeclarationKind::UsingAlias) {
        copyText(output->type.spelling, sizeof(output->type.spelling), best->name);
        copyText(output->type.aliasName, sizeof(output->type.aliasName), best->name);
        if (output->type.resolvedAlias[0] == '\0') copyText(output->type.resolvedAlias, sizeof(output->type.resolvedAlias), best->type.baseName);
    }
    if (matches > 1 && (best->kind == TypeDeclarationKind::Function || best->kind == TypeDeclarationKind::GlobalVariable ||
                        best->kind == TypeDeclarationKind::Typedef || best->kind == TypeDeclarationKind::UsingAlias)) {
        output->available = false;
        output->state = TypeInspectionState::Ambiguous;
        copyText(output->detail, sizeof(output->detail), "Multiple declarations match.");
        return true;
    }
    output->state = inspectionState(output->type.confidence);
    if (output->state == TypeInspectionState::Unknown) {
        output->available = false;
        copyText(output->detail, sizeof(output->detail), output->type.aliasCycle ? "Alias resolution cycle detected." : "Type information unavailable.");
    }
    copyText(output->displayType, sizeof(output->displayType), output->type.spelling);
    return true;
}

bool TypeInspectionIsCurrent(const TypeInspection* inspection, const Document& document,
                             uint64_t projectGeneration) {
    if (!inspection || !inspection->available) return false;
    const TypeLocation& location = inspection->type.declarationLocation;
    return location.projectGeneration == projectGeneration &&
        (location.documentId != document.documentId || location.documentGeneration == document.buffer.generation);
}

} // namespace developer_studio
} // namespace guidexos
