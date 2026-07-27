#include "developer_studio_find.h"
#include "developer_studio_models.h"

#include <assert.h>
#include <string>
#include <vector>

using namespace guidexos::developer_studio;

static FindSession g_session;
static TextBuffer g_buffer;
static Document g_document;

static void setQuery(const char* text) {
    assert(FindSetQuery(&g_session, text));
}

static void search(const char* text) {
    assert(FindSearch(&g_session, 7, 3, text, static_cast<uint32_t>(text ? std::string(text).size() : 0)));
}

static void testMatching() {
    FindSessionInit(&g_session);
    setQuery("aa");
    search("aaaa");
    assert(g_session.matchCount == 2);
    assert(g_session.matches[0].start == 0 && g_session.matches[0].length == 2);
    assert(g_session.matches[1].start == 2 && g_session.matches[1].length == 2);

    setQuery("");
    search("anything");
    assert(g_session.matchCount == 0);

    setQuery("line two");
    search("line one\nline two\nline two");
    assert(g_session.matchCount == 2);
    assert(g_session.matches[0].start == 9);

    char longQuery[kFindMaxQueryBytes + 2] = {};
    for (uint32_t i = 0; i < kFindMaxQueryBytes + 1; ++i) longQuery[i] = 'q';
    assert(!FindSetQuery(&g_session, longQuery));
    assert(g_session.error == FindErrorCode::QueryTooLarge);

    setQuery("x");
    std::vector<char> tooLarge(kFindMaxSearchableDocumentBytes + 1u, 'x');
    assert(!FindSearch(&g_session, 7, 4, tooLarge.data(), static_cast<uint32_t>(tooLarge.size())));
    assert(g_session.error == FindErrorCode::DocumentTooLarge);

    std::string many(kFindMaxRetainedMatches + 1u, 'x');
    search(many.c_str());
    assert(g_session.matchCount == kFindMaxRetainedMatches);
    assert(g_session.truncated);
    assert(g_session.error == FindErrorCode::MatchLimitReached);
    assert(!FindCanReplaceAll(&g_session));
}

static void testCaseAndWholeWord() {
    FindSessionInit(&g_session);
    setQuery("Test");
    FindOptions options = g_session.options;
    options.caseSensitive = true;
    FindSetOptions(&g_session, options);
    search("Test test TEST TeSt");
    assert(g_session.matchCount == 1);
    options.caseSensitive = false;
    FindSetOptions(&g_session, options);
    search("Test test TEST TeSt");
    assert(g_session.matchCount == 4);

    setQuery("cat");
    options.wholeWord = true;
    FindSetOptions(&g_session, options);
    const char* words = "cat catalog cat2 my_cat cat() cat-cat _cat cat_";
    search(words);
    assert(g_session.matchCount == 4);

    setQuery("Test");
    search("\xC3\xA9Test");
    assert(g_session.matchCount == 1);
    assert(g_session.matches[0].start == 2);
}

static void testNavigationAndVisibleRanges() {
    FindSessionInit(&g_session);
    setQuery("one");
    search("one two one three one");
    bool wrapped = false;
    assert(FindNavigate(&g_session, 0, FindDirection::Forward, &wrapped) == 0 && !wrapped);
    assert(FindNavigate(&g_session, 0, FindDirection::Forward, &wrapped) == 1 && !wrapped);
    assert(FindNavigate(&g_session, 0, FindDirection::Forward, &wrapped) == 2 && !wrapped);
    assert(FindNavigate(&g_session, 0, FindDirection::Forward, &wrapped) == 0 && wrapped);
    {
        FindOptions noWrap = g_session.options;
        noWrap.wrapAround = false;
        FindSetOptions(&g_session, noWrap);
        FindSetCurrentMatch(&g_session, 2);
        assert(FindNavigate(&g_session, 0, FindDirection::Forward, &wrapped) == -1);
        assert(!wrapped);
        assert(FindSetCurrentMatch(&g_session, 0));
        assert(FindNavigate(&g_session, 0, FindDirection::Backward, &wrapped) == -1);
    }
    FindOptions wrap = g_session.options;
    wrap.wrapAround = true;
    FindSetOptions(&g_session, wrap);
    FindSetCurrentMatch(&g_session, 0);
    assert(FindNavigate(&g_session, 0, FindDirection::Backward, &wrapped) == 2 && wrapped);
    setQuery("absent");
    search("one two one");
    assert(FindNavigate(&g_session, 0, FindDirection::Forward, &wrapped) == -1);
    assert(!wrapped);
    setQuery("one");
    search("one two one three one");
    uint32_t indices[4] = {};
    assert(FindVisibleMatchIndices(&g_session, 8, 16, indices, 4) == 1);
    assert(indices[0] == 1);
    assert(FindSessionIsStale(&g_session, 7, 99));
}

static void testEditorPrimitivesAndReplacement() {
    TextBufferInit(&g_buffer);
    assert(TextBufferSet(&g_buffer, "cat cat", 7));
    assert(SelectTextRange(&g_buffer, 0, 3));
    char selected[16] = {};
    assert(GetSelectedText(&g_buffer, selected, sizeof(selected)) == 3);
    assert(std::string(selected) == "cat");
    assert(ReplaceTextRange(&g_buffer, 0, 3, "catalog", 7));
    assert(std::string(g_buffer.data) == "catalog cat");
    assert(g_buffer.dirty);
    uint32_t line = 0;
    uint32_t column = 0;
    assert(OffsetToLineColumn(&g_buffer, 8, &line, &column) && line == 0 && column == 8);
    uint32_t offset = 0;
    assert(LineColumnToOffset(&g_buffer, 0, 8, &offset) && offset == 8);

    TextBufferInit(&g_buffer);
    assert(TextBufferSet(&g_buffer, "same", 4));
    assert(ReplaceTextRange(&g_buffer, 0, 4, "same", 4));
    assert(std::string(g_buffer.data) == "same");
    assert(!g_buffer.dirty);

    TextBufferInit(&g_buffer);
    assert(TextBufferSet(&g_buffer, "remove me", 9));
    assert(ReplaceTextRange(&g_buffer, 0, 7, nullptr, 0));
    assert(std::string(g_buffer.data) == "me");

    TextBufferInit(&g_buffer);
    assert(TextBufferSet(&g_buffer, "aaaa", 4));
    FindSessionInit(&g_session);
    setQuery("aa");
    search(g_buffer.data);
    assert(FindSetReplacement(&g_session, "x"));
    assert(ReplaceTextRanges(&g_buffer, g_session.matches, g_session.matchCount, g_session.replacement, 1));
    assert(std::string(g_buffer.data) == "xx");

    TextBufferInit(&g_buffer);
    assert(TextBufferSet(&g_buffer, "a a a", 5));
    FindSessionInit(&g_session);
    setQuery("a");
    search(g_buffer.data);
    assert(FindSetReplacement(&g_session, "aa"));
    assert(ReplaceTextRanges(&g_buffer, g_session.matches, g_session.matchCount, g_session.replacement, 2));
    assert(std::string(g_buffer.data) == "aa aa aa");
    assert(g_session.matchCount == 3);

    TextBufferInit(&g_buffer);
    assert(TextBufferSet(&g_buffer, "Test test catalog test", 22));
    FindSessionInit(&g_session);
    setQuery("test");
    FindOptions options = g_session.options;
    options.caseSensitive = false;
    options.wholeWord = true;
    FindSetOptions(&g_session, options);
    search(g_buffer.data);
    assert(g_session.matchCount == 3);
    assert(FindSetReplacement(&g_session, "X"));
    assert(ReplaceTextRanges(&g_buffer, g_session.matches, g_session.matchCount, g_session.replacement, 1));
    assert(std::string(g_buffer.data) == "X X catalog X");

    TextBufferInit(&g_buffer);
    assert(TextBufferSet(&g_buffer, "a\nb", 3));
    FindSessionInit(&g_session);
    setQuery("a\nb");
    search(g_buffer.data);
    assert(g_session.matchCount == 1);
    assert(FindSetReplacement(&g_session, "x\ny"));
    assert(ReplaceTextRange(&g_buffer, 0, 3, g_session.replacement, 3));
    assert(std::string(g_buffer.data) == "x\ny");
}

static void testSyntaxGenerationAndStaleSafety() {
    Document& document = g_document;
    document.used = true;
    document.documentId = 42;
    document.path[0] = 'a';
    document.path[1] = '.';
    document.path[2] = 'c';
    document.path[3] = '\0';
    FindDocumentStateInit(&document.find);
    TextBufferInit(&document.buffer);
    assert(TextBufferSet(&document.buffer, "int cat;", 8));
    DocumentUpdateSyntax(&document);
    const uint32_t oldGeneration = document.buffer.generation;
    FindSessionInit(&g_session);
    setQuery("cat");
    assert(FindSearch(&g_session, document.documentId, document.buffer.generation,
                      document.buffer.data, document.buffer.length));
    assert(ReplaceTextRange(&document.buffer, 4, 3, "dog", 3));
    assert(document.buffer.dirty);
    assert(FindSessionIsStale(&g_session, document.documentId, document.buffer.generation));
    DocumentUpdateSyntax(&document);
    assert(document.syntax.generation == document.buffer.generation);
    assert(document.buffer.generation != oldGeneration);
}

int main() {
    testMatching();
    testCaseAndWholeWord();
    testNavigationAndVisibleRanges();
    testEditorPrimitivesAndReplacement();
    testSyntaxGenerationAndStaleSafety();
    return 0;
}
