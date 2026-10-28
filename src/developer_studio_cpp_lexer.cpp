#include "developer_studio_cpp_lexer.h"

namespace guidexos {
namespace developer_studio {
namespace {

static bool isIdentifierStart(unsigned char value) {
    return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') || value == '_';
}

static bool isIdentifierByte(unsigned char value) {
    return isIdentifierStart(value) || (value >= '0' && value <= '9');
}

static bool startsWith(const char* data, uint32_t length, uint32_t offset, const char* text) {
    if (!data || !text || offset > length) return false;
    uint32_t i = 0;
    while (text[i]) {
        if (i >= length - offset || data[offset + i] != text[i]) return false;
        ++i;
    }
    return true;
}

static bool rawPrefixLength(const char* data, uint32_t length, uint32_t offset,
                            uint32_t* prefixLength) {
    if (startsWith(data, length, offset, "u8R\"")) *prefixLength = 3u;
    else if (startsWith(data, length, offset, "uR\"") ||
             startsWith(data, length, offset, "UR\"") ||
             startsWith(data, length, offset, "LR\"")) *prefixLength = 2u;
    else if (startsWith(data, length, offset, "R\"")) *prefixLength = 1u;
    else return false;
    return true;
}

static bool quotedPrefixLength(const char* data, uint32_t length, uint32_t offset,
                               uint32_t* prefixLength, char* quote) {
    static const char* const prefixes[] = { "u8\"", "u8'", "u\"", "u'", "U\"", "U'", "L\"", "L'" };
    for (uint32_t i = 0; i < sizeof(prefixes) / sizeof(prefixes[0]); ++i) {
        if (startsWith(data, length, offset, prefixes[i])) {
            *prefixLength = prefixes[i][1] == '8' ? 2u : 1u;
            *quote = prefixes[i][*prefixLength];
            return true;
        }
    }
    if (offset < length && (data[offset] == '"' || data[offset] == '\'')) {
        *prefixLength = 0;
        *quote = data[offset];
        return true;
    }
    return false;
}

static bool isRawDelimiterByte(unsigned char value) {
    return value > 0x20u && value != '(' && value != ')' && value != '\\' && value != 0x7Fu;
}

static void advanceLine(CppLexer* lexer, uint32_t position) {
    if (lexer->data[position] == '\n') {
        ++lexer->line;
        lexer->lineStart = position + 1u;
    }
}

static uint32_t scanRawLiteral(CppLexer* lexer, uint32_t prefixLength) {
    const uint32_t openingQuote = lexer->offset + prefixLength;
    const uint32_t delimiterStart = openingQuote + 1u;
    uint32_t open = delimiterStart;
    while (open < lexer->length && lexer->data[open] != '(' &&
           open - delimiterStart <= 16u &&
           isRawDelimiterByte(static_cast<unsigned char>(lexer->data[open]))) ++open;
    if (open >= lexer->length || lexer->data[open] != '(' || open - delimiterStart > 16u) {
        // Treat malformed raw-string openers as consuming the remainder. This
        // avoids discovering declarations inside text whose lexical state is
        // already ambiguous.
        while (lexer->offset < lexer->length) {
            advanceLine(lexer, lexer->offset);
            ++lexer->offset;
        }
        return lexer->length;
    }
    const uint32_t delimiterLength = open - delimiterStart;
    uint32_t cursor = open + 1u;
    while (cursor < lexer->length) {
        if (lexer->data[cursor] == ')' && delimiterLength + 1u < lexer->length - cursor) {
            uint32_t matched = 0;
            while (matched < delimiterLength &&
                   lexer->data[cursor + 1u + matched] == lexer->data[delimiterStart + matched]) ++matched;
            if (matched == delimiterLength && lexer->data[cursor + 1u + delimiterLength] == '"') {
                lexer->offset = cursor + delimiterLength + 2u;
                return lexer->offset;
            }
        }
        advanceLine(lexer, cursor);
        ++cursor;
    }
    lexer->offset = lexer->length;
    return lexer->offset;
}

static uint32_t scanQuotedLiteral(CppLexer* lexer, uint32_t prefixLength, char quote) {
    lexer->offset += prefixLength + 1u;
    while (lexer->offset < lexer->length) {
        const char value = lexer->data[lexer->offset];
        if (value == quote) {
            ++lexer->offset;
            return lexer->offset;
        }
        if (value == '\n') {
            advanceLine(lexer, lexer->offset++);
            return lexer->offset;
        }
        if (value == '\\') {
            if (lexer->offset + 1u < lexer->length && lexer->data[lexer->offset + 1u] == '\n') {
                advanceLine(lexer, lexer->offset + 1u);
                lexer->offset += 2u;
            } else if (lexer->offset + 2u < lexer->length &&
                       lexer->data[lexer->offset + 1u] == '\r' &&
                       lexer->data[lexer->offset + 2u] == '\n') {
                advanceLine(lexer, lexer->offset + 2u);
                lexer->offset += 3u;
            } else if (lexer->offset + 1u < lexer->length) {
                lexer->offset += 2u;
            } else {
                ++lexer->offset;
            }
            continue;
        }
        advanceLine(lexer, lexer->offset);
        ++lexer->offset;
    }
    return lexer->offset;
}

static bool onlyWhitespaceOnLine(const CppLexer* lexer, uint32_t offset) {
    if (!lexer || offset < lexer->lineStart) return false;
    for (uint32_t i = lexer->lineStart; i < offset; ++i)
        if (lexer->data[i] != ' ' && lexer->data[i] != '\t' && lexer->data[i] != '\r') return false;
    return true;
}

static uint32_t scanPreprocessor(CppLexer* lexer) {
    uint32_t cursor = lexer->offset;
    while (cursor < lexer->length) {
        while (cursor < lexer->length && lexer->data[cursor] != '\n') ++cursor;
        if (cursor >= lexer->length) break;
        uint32_t last = cursor;
        if (last > lexer->offset && lexer->data[last - 1u] == '\r') --last;
        const bool continued = last > lexer->offset && lexer->data[last - 1u] == '\\';
        advanceLine(lexer, cursor);
        ++cursor;
        if (!continued) break;
    }
    lexer->offset = cursor;
    return cursor;
}

static char lowerAscii(char value) {
    return value >= 'A' && value <= 'Z' ? static_cast<char>(value + ('a' - 'A')) : value;
}

} // namespace

void CppLexerInit(CppLexer* lexer, const char* data, uint32_t length) {
    if (!lexer) return;
    lexer->data = data;
    lexer->length = data ? length : 0u;
    lexer->offset = 0;
    lexer->line = 1;
    lexer->lineStart = 0;
}

bool CppLexerNext(CppLexer* lexer, CppLexToken* token) {
    if (!lexer || !token || (!lexer->data && lexer->length != 0)) return false;
    while (lexer->offset < lexer->length) {
        const unsigned char value = static_cast<unsigned char>(lexer->data[lexer->offset]);
        if (value == ' ' || value == '\t' || value == '\r' || value == '\f' || value == '\v') {
            ++lexer->offset;
            continue;
        }
        if (value == '\n') {
            advanceLine(lexer, lexer->offset++);
            continue;
        }

        const uint32_t start = lexer->offset;
        const uint32_t line = lexer->line;
        uint32_t prefixLength = 0;
        if (rawPrefixLength(lexer->data, lexer->length, start, &prefixLength)) {
            scanRawLiteral(lexer, prefixLength);
            token->kind = CppLexTokenKind::Literal;
            token->start = start;
            token->length = lexer->offset - start;
            token->line = line;
            return true;
        }

        char quote = '\0';
        if (quotedPrefixLength(lexer->data, lexer->length, start, &prefixLength, &quote)) {
            scanQuotedLiteral(lexer, prefixLength, quote);
            token->kind = CppLexTokenKind::Literal;
            token->start = start;
            token->length = lexer->offset - start;
            token->line = line;
            return true;
        }

        if (value == '/' && start + 1u < lexer->length && lexer->data[start + 1u] == '/') {
            lexer->offset += 2u;
            while (lexer->offset < lexer->length) {
                while (lexer->offset < lexer->length && lexer->data[lexer->offset] != '\n') ++lexer->offset;
                if (lexer->offset >= lexer->length) break;
                uint32_t last = lexer->offset;
                if (last > start && lexer->data[last - 1u] == '\r') --last;
                const bool continued = last > start && lexer->data[last - 1u] == '\\';
                advanceLine(lexer, lexer->offset++);
                if (!continued) break;
            }
            token->kind = CppLexTokenKind::Comment;
            token->start = start;
            token->length = lexer->offset - start;
            token->line = line;
            return true;
        }
        if (value == '/' && start + 1u < lexer->length && lexer->data[start + 1u] == '*') {
            lexer->offset += 2u;
            while (lexer->offset < lexer->length) {
                if (lexer->data[lexer->offset] == '*' && lexer->offset + 1u < lexer->length &&
                    lexer->data[lexer->offset + 1u] == '/') {
                    lexer->offset += 2u;
                    break;
                }
                advanceLine(lexer, lexer->offset);
                ++lexer->offset;
            }
            token->kind = CppLexTokenKind::Comment;
            token->start = start;
            token->length = lexer->offset - start;
            token->line = line;
            return true;
        }

        if (value == '#' && onlyWhitespaceOnLine(lexer, start)) {
            scanPreprocessor(lexer);
            token->kind = CppLexTokenKind::Preprocessor;
            token->start = start;
            token->length = lexer->offset - start;
            token->line = line;
            return true;
        }

        if (isIdentifierStart(value)) {
            ++lexer->offset;
            while (lexer->offset < lexer->length &&
                   isIdentifierByte(static_cast<unsigned char>(lexer->data[lexer->offset]))) ++lexer->offset;
            token->kind = CppLexTokenKind::Identifier;
        } else if (value >= '0' && value <= '9') {
            ++lexer->offset;
            while (lexer->offset < lexer->length) {
                const unsigned char next = static_cast<unsigned char>(lexer->data[lexer->offset]);
                if (!isIdentifierByte(next) && next != '.' && next != '\'') break;
                ++lexer->offset;
            }
            token->kind = CppLexTokenKind::Number;
        } else {
            ++lexer->offset;
            token->kind = CppLexTokenKind::Punctuation;
        }
        token->start = start;
        token->length = lexer->offset - start;
        token->line = line;
        return true;
    }
    token->kind = CppLexTokenKind::End;
    token->start = lexer->length;
    token->length = 0;
    token->line = lexer->line;
    return true;
}

bool CppLexTokenEquals(const char* data, const CppLexToken& token, const char* word) {
    if (!data || !word || token.kind != CppLexTokenKind::Identifier) return false;
    uint32_t i = 0;
    while (word[i] && i < token.length) {
        if (data[token.start + i] != word[i]) return false;
        ++i;
    }
    return word[i] == '\0' && i == token.length;
}

bool CppIsSourcePath(const char* path) {
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

} // namespace developer_studio
} // namespace guidexos
