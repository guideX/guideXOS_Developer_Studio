#include "developer_studio_models.h"

#include <cassert>
#include <cstring>
#include <iostream>

using namespace guidexos::developer_studio;

static SyntaxLineResult tokenize(SyntaxLanguage language, const char* text, SyntaxLineState input,
                                 SyntaxTokenSpan* spans, uint32_t capacity) {
    SyntaxLineResult result = {};
    const uint32_t length = static_cast<uint32_t>(std::strlen(text));
    assert(SyntaxTokenizeLine(language, text, length, input, &result, spans, capacity));
    assert(result.spanValidationPassed);
    return result;
}

static bool hasKind(const SyntaxTokenSpan* spans, uint32_t count, SyntaxTokenKind kind) {
    for (uint32_t i = 0; i < count; ++i) if (spans[i].kind == kind) return true;
    return false;
}

static bool hasText(const char* line, const SyntaxTokenSpan* spans, uint32_t count, SyntaxTokenKind kind, const char* expected) {
    const uint32_t expectedLength = static_cast<uint32_t>(std::strlen(expected));
    for (uint32_t i = 0; i < count; ++i) {
        if (spans[i].kind != kind || spans[i].length != expectedLength) continue;
        if (std::strncmp(line + spans[i].start, expected, expectedLength) == 0) return true;
    }
    return false;
}

static SyntaxLineState normalState() {
    SyntaxLineState state = { SyntaxLineStateKind::Normal, 0 };
    return state;
}

static void testLexicalClasses() {
    SyntaxTokenSpan spans[128] = {};
    SyntaxLineResult result = tokenize(SyntaxLanguage::Cpp,
        "if (count >= 0x1.fp3 && value != 1'000u) return \"// /* not comment */\";", normalState(), spans, 128);
    assert(hasText("if (count >= 0x1.fp3 && value != 1'000u) return \"// /* not comment */\";", spans, result.spanCount,
                   SyntaxTokenKind::Keyword, "if"));
    assert(hasText("if (count >= 0x1.fp3 && value != 1'000u) return \"// /* not comment */\";", spans, result.spanCount,
                   SyntaxTokenKind::Number, "0x1.fp3"));
    assert(hasText("if (count >= 0x1.fp3 && value != 1'000u) return \"// /* not comment */\";", spans, result.spanCount,
                   SyntaxTokenKind::StringLiteral, "\"// /* not comment */\""));
    assert(hasKind(spans, result.spanCount, SyntaxTokenKind::Operator));
    assert(hasKind(spans, result.spanCount, SyntaxTokenKind::Punctuation));

    result = tokenize(SyntaxLanguage::C, "unsigned int sample_2 = 0b1010;", normalState(), spans, 128);
    assert(hasText("unsigned int sample_2 = 0b1010;", spans, result.spanCount, SyntaxTokenKind::TypeKeyword, "unsigned"));
    assert(hasText("unsigned int sample_2 = 0b1010;", spans, result.spanCount, SyntaxTokenKind::Identifier, "sample_2"));
    assert(hasText("unsigned int sample_2 = 0b1010;", spans, result.spanCount, SyntaxTokenKind::Number, "0b1010"));
    assert(!hasText("unsigned int sample_2 = 0b1010;", spans, result.spanCount, SyntaxTokenKind::Keyword, "int"));

    result = tokenize(SyntaxLanguage::Cpp, "#include \"header.h\" // directive comment", normalState(), spans, 128);
    assert(hasKind(spans, result.spanCount, SyntaxTokenKind::Preprocessor));
    assert(hasKind(spans, result.spanCount, SyntaxTokenKind::StringLiteral));
    assert(hasKind(spans, result.spanCount, SyntaxTokenKind::Comment));

    result = tokenize(SyntaxLanguage::Cpp, "R\"tag(raw // text /* \" quotes)tag\" int", normalState(), spans, 128);
    assert(hasText("R\"tag(raw // text /* \" quotes)tag\" int", spans, result.spanCount,
                   SyntaxTokenKind::StringLiteral, "R\"tag(raw // text /* \" quotes)tag\""));
    assert(hasText("R\"tag(raw // text /* \" quotes)tag\" int", spans, result.spanCount,
                   SyntaxTokenKind::TypeKeyword, "int"));

    result = tokenize(SyntaxLanguage::Cpp, "'\\n' L\"wide\" u8\"utf\" \"unterminated", normalState(), spans, 128);
    assert(hasKind(spans, result.spanCount, SyntaxTokenKind::CharacterLiteral));
    assert(hasKind(spans, result.spanCount, SyntaxTokenKind::StringLiteral));
    assert(SyntaxValidateSpans(spans, result.spanCount, 38));
}

static void testMultilineState() {
    SyntaxTokenSpan spans[64] = {};
    SyntaxLineResult first = tokenize(SyntaxLanguage::Cpp, "int x; /* open", normalState(), spans, 64);
    assert(first.outputState.kind == SyntaxLineStateKind::InBlockComment);
    SyntaxLineResult second = tokenize(SyntaxLanguage::Cpp, "still // comment */ int y;", first.outputState, spans, 64);
    assert(second.outputState.kind == SyntaxLineStateKind::Normal);
    assert(hasKind(spans, second.spanCount, SyntaxTokenKind::Comment));
    assert(hasText("still // comment */ int y;", spans, second.spanCount, SyntaxTokenKind::TypeKeyword, "int"));

    SyntaxLineResult rawFirst = tokenize(SyntaxLanguage::Cpp, "R\"(raw", normalState(), spans, 64);
    assert(rawFirst.outputState.kind == SyntaxLineStateKind::InRawString);
    SyntaxLineResult rawSecond = tokenize(SyntaxLanguage::Cpp, " // text)\";", rawFirst.outputState, spans, 64);
    assert(rawSecond.outputState.kind == SyntaxLineStateKind::Normal);
    assert(hasKind(spans, rawSecond.spanCount, SyntaxTokenKind::StringLiteral));
}

static void testDetectionAndRendering() {
    assert(DetectSyntaxLanguage("src/main.c") == SyntaxLanguage::C);
    assert(DetectSyntaxLanguage("src/main.cpp") == SyntaxLanguage::Cpp);
    assert(DetectSyntaxLanguage("src/main.cc") == SyntaxLanguage::Cpp);
    assert(DetectSyntaxLanguage("src/header.h") == SyntaxLanguage::Cpp);
    assert(DetectSyntaxLanguage("README.txt") == SyntaxLanguage::None);
    assert(DetectSyntaxLanguage("C:\\work\\main.HXX") == SyntaxLanguage::Cpp);

    SyntaxTokenSpan spans[3] = {
        { 2, 2, SyntaxTokenKind::Keyword },
        { 7, 3, SyntaxTokenKind::StringLiteral },
        { 12, 2, SyntaxTokenKind::Comment }
    };
    SyntaxRenderRun runs[16] = {};
    SyntaxSelection selection = { true, 3, 8 };
    const uint32_t count = SyntaxBuildRenderRuns(spans, 3, 14, selection, runs, 16);
    assert(count >= 5);
    assert(runs[0].kind == SyntaxTokenKind::PlainText && !runs[0].selected);
    bool selected = false;
    bool gap = false;
    for (uint32_t i = 0; i < count; ++i) {
        if (runs[i].selected) selected = true;
        if (runs[i].kind == SyntaxTokenKind::PlainText) gap = true;
    }
    assert(selected && gap);
    assert(!SyntaxValidateSpans(spans, 3, 13));
}

static void testCacheAndIncrementalConvergence() {
    static Document document = {};
    document.used = true;
    std::strncpy(document.path, "/workspace/main.cpp", sizeof(document.path) - 1);
    TextBufferInit(&document.buffer);
    const char* source = "int first = 1;\nint second = 2;\nint third = 3;\nint fourth = 4;\n";
    assert(TextBufferSet(&document.buffer, source, static_cast<uint32_t>(std::strlen(source))));
    DocumentUpdateSyntax(&document);
    assert(document.syntax.valid && !document.syntax.fallback);
    assert(document.syntax.fullRebuildCount == 1);
    assert(document.syntax.lineCount == 5);
    assert(SyntaxCacheValidate(&document.syntax, document.buffer.data, document.buffer.length));

    const uint32_t fullBefore = document.syntax.fullRebuildCount;
    document.buffer.caret = TextBufferLineStart(&document.buffer, 1);
    assert(TextBufferInsert(&document.buffer, "// ", 3));
    DocumentUpdateSyntax(&document);
    assert(document.syntax.lastUpdateWasIncremental);
    assert(document.syntax.lastUpdateConverged);
    assert(document.syntax.fullRebuildCount == fullBefore);
    assert(document.syntax.lastRetokenizedLineCount == 1);
    assert(SyntaxCacheValidate(&document.syntax, document.buffer.data, document.buffer.length));

    document.buffer.caret = TextBufferLineStart(&document.buffer, 0) + 4;
    assert(TextBufferInsert(&document.buffer, "/*", 2));
    DocumentUpdateSyntax(&document);
    assert(document.syntax.lastRetokenizedLineCount >= 4);
    assert(!document.syntax.lastUpdateConverged);
    assert(SyntaxCacheValidate(&document.syntax, document.buffer.data, document.buffer.length));

    document.buffer.caret = TextBufferLineEnd(&document.buffer, 0);
    assert(TextBufferInsert(&document.buffer, "*/", 2));
    DocumentUpdateSyntax(&document);
    assert(document.syntax.lastUpdateWasIncremental);
    assert(SyntaxCacheValidate(&document.syntax, document.buffer.data, document.buffer.length));

    document.buffer.caret = TextBufferLineStart(&document.buffer, 2);
    assert(TextBufferInsert(&document.buffer, "new\n", 4));
    DocumentUpdateSyntax(&document);
    assert(document.syntax.lineCount == 6);
    assert(document.syntax.lastUpdateWasIncremental);
    assert(SyntaxCacheValidate(&document.syntax, document.buffer.data, document.buffer.length));
    assert(document.syntax.fullRebuildCount == fullBefore);

    document.buffer.caret = TextBufferLineStart(&document.buffer, 2) + 3;
    assert(TextBufferDelete(&document.buffer));
    DocumentUpdateSyntax(&document);
    assert(document.syntax.lineCount == 5);
    assert(document.syntax.lastUpdateWasIncremental);
    assert(SyntaxCacheValidate(&document.syntax, document.buffer.data, document.buffer.length));

    const uint32_t lineStart = TextBufferLineStart(&document.buffer, 0);
    assert(TextBufferVisualColumn(&document.buffer, lineStart, lineStart + 1, 4) == 1);
    assert(TextBufferOffsetForVisualColumn(&document.buffer, lineStart, TextBufferLineEnd(&document.buffer, 0), 1, 4) == lineStart + 1);
}

static void testBounds() {
    static SyntaxCache cache;
    SyntaxCacheInit(&cache);
    static char longLine[kSyntaxMaxTokenizableLineBytes + 2] = {};
    for (uint32_t i = 0; i < kSyntaxMaxTokenizableLineBytes + 1; ++i) longLine[i] = 'a';
    assert(SyntaxCacheBuild(&cache, SyntaxLanguage::Cpp, longLine, kSyntaxMaxTokenizableLineBytes + 1, 1));
    assert(cache.fallback && cache.fallbackCode == SyntaxErrorCode::LineTooLong);

    static char manyTokens[12000] = {};
    uint32_t length = 0;
    for (uint32_t i = 0; i < kSyntaxMaxTokensPerLine + 1; ++i) { manyTokens[length++] = '+'; manyTokens[length++] = ' '; }
    assert(SyntaxCacheBuild(&cache, SyntaxLanguage::Cpp, manyTokens, length, 2));
    assert(cache.fallback && cache.fallbackCode == SyntaxErrorCode::TokenLimit);

    static char tooLarge[kSyntaxMaxHighlightedDocumentBytes + 1] = {};
    assert(SyntaxCacheBuild(&cache, SyntaxLanguage::Cpp, tooLarge, kSyntaxMaxHighlightedDocumentBytes + 1, 3));
    assert(cache.fallback && cache.fallbackCode == SyntaxErrorCode::DocumentTooLarge);
}

int main() {
    testLexicalClasses();
    testMultilineState();
    testDetectionAndRendering();
    testCacheAndIncrementalConvergence();
    testBounds();
    std::cout << "Developer Studio syntax tests PASS\n";
    return 0;
}
