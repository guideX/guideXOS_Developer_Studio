#pragma once

#include <stdint.h>

namespace guidexos {
namespace developer_studio {

enum class CppLexTokenKind : uint8_t {
    End = 0,
    Identifier,
    Number,
    Punctuation,
    Comment,
    Literal,
    Preprocessor
};

struct CppLexToken {
    CppLexTokenKind kind;
    uint32_t start;
    uint32_t length;
    uint32_t line;
};

struct CppLexer {
    const char* data;
    uint32_t length;
    uint32_t offset;
    uint32_t line;
    uint32_t lineStart;
};

void CppLexerInit(CppLexer* lexer, const char* data, uint32_t length);
bool CppLexerNext(CppLexer* lexer, CppLexToken* token);
bool CppLexTokenEquals(const char* data, const CppLexToken& token, const char* word);
bool CppIsSourcePath(const char* path);

} // namespace developer_studio
} // namespace guidexos
