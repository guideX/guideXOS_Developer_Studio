#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "developer_studio_completion.h"
#include "developer_studio_types.h"

using namespace guidexos::developer_studio;

static void prepareDocument(Document* document, const char* path, const char* text) {
    *document = Document();
    document->used = true;
    document->documentId = 41;
    for (uint32_t i = 0; i + 1 < sizeof(document->path) && path[i] != '\0'; ++i) document->path[i] = path[i];
    TextBufferInit(&document->buffer);
    assert(TextBufferSet(&document->buffer, text, 0 == text ? 0 : static_cast<uint32_t>(strlen(text))));
    document->syntax.language = DetectSyntaxLanguage(path);
    assert(SyntaxCacheBuild(&document->syntax, document->syntax.language,
                            document->buffer.data, document->buffer.length,
                            document->buffer.generation));
}

static void setCaret(Document* document, uint32_t offset) {
    assert(SetCaretOffset(&document->buffer, offset));
}

static int findCandidate(const CompletionSession& session, const char* text) {
    for (uint32_t i = 0; i < session.candidateCount; ++i) {
        const CompletionCandidate& candidate = session.candidates[i];
        uint32_t j = 0;
        while (candidate.insertionText[j] != '\0' && text[j] != '\0' && candidate.insertionText[j] == text[j]) ++j;
        if (candidate.insertionText[j] == '\0' && text[j] == '\0') return static_cast<int>(i);
    }
    return -1;
}

static void buildIndex(const Document& document, const char* text,
                       SymbolDatabase* database, ProjectSymbol* projectStorage,
                       SymbolDocument* documentStorage, DocumentSymbol* scratchStorage) {
    SymbolDatabaseInit(database, projectStorage, 256, documentStorage, 16, scratchStorage, 256);
    assert(SymbolDatabaseIndexDocument(database, document.path, document.documentId,
                                       document.buffer.generation, true, text,
                                       static_cast<uint32_t>(strlen(text))));
}

static void buildTypeIndex(const Document& document, const char* text,
                           TypeDatabase* database, TypeRecord* records,
                           TypeDocument* documents, TypeMemberBucket* buckets,
                           uint32_t* memberIndices, uint32_t memberIndexCapacity = 1024) {
    TypeDatabaseInit(database, records, 1024, documents, 8, buckets, 64,
                     memberIndices, memberIndexCapacity);
    assert(TypeDatabaseIndexDocument(database, "", document.path, document.documentId,
                                     document.buffer.generation, 8, text,
                                     static_cast<uint32_t>(strlen(text))));
}

static void buildMemberSession(const char* text, CompletionSession* session,
                               CompletionCandidate* candidates, TypeDatabase* typeDatabase,
                               TypeRecord* records, TypeDocument* documents,
                               TypeMemberBucket* buckets, uint32_t* memberIndices,
                               CompletionErrorCode* error) {
    static Document document = {};
    prepareDocument(&document, "src/member.cpp", text);
    setCaret(&document, document.buffer.length);
    buildTypeIndex(document, text, typeDatabase, records, documents, buckets, memberIndices);
    CompletionSessionInit(session, candidates, 128);
    assert(CompletionBuildSession(session, document, CompletionProjectId("demo"), 8,
                                  nullptr, nullptr, true, typeDatabase, error));
}

static void testTypeAwareDirectMembers() {
    const char text[] =
        "struct Window {\n"
        "    int width;\n"
        "    int height;\n"
        "};\n"
        "Window window;\n"
        "window.";
    static CompletionCandidate candidates[128] = {};
    static CompletionSession session = {};
    static TypeDatabase typeDatabase = {};
    static TypeRecord records[1024] = {};
    static TypeDocument documents[8] = {};
    static TypeMemberBucket buckets[64] = {};
    static uint32_t memberIndices[1024] = {};
    CompletionErrorCode error = CompletionErrorCode::None;
    buildMemberSession(text, &session, candidates, &typeDatabase, records, documents,
                       buckets, memberIndices, &error);
    assert(session.active);
    assert(session.context.memberResolution == CompletionMemberResolution::Exact);
    assert(findCandidate(session, "width") >= 0);
    assert(findCandidate(session, "height") >= 0);
    assert(session.candidates[findCandidate(session, "width")].kind == CompletionCandidateKind::Member);

    const char bracedText[] =
        "struct Window { int width; int height; };\n"
        "void draw() { Window window{}; window.";
    buildMemberSession(bracedText, &session, candidates, &typeDatabase, records, documents,
                       buckets, memberIndices, &error);
    assert(session.active && session.context.memberResolution == CompletionMemberResolution::Exact);
    assert(findCandidate(session, "width") >= 0 && findCandidate(session, "height") >= 0);
}

static void testTypeAwarePointerReferenceAndParameter() {
    const char pointerText[] =
        "struct Window { int width; void show(); };\n"
        "Window* window;\n"
        "window->";
    static CompletionCandidate candidates[128] = {};
    static CompletionSession session = {};
    static TypeDatabase typeDatabase = {};
    static TypeRecord records[1024] = {};
    static TypeDocument documents[8] = {};
    static TypeMemberBucket buckets[64] = {};
    static uint32_t memberIndices[1024] = {};
    CompletionErrorCode error = CompletionErrorCode::None;
    buildMemberSession(pointerText, &session, candidates, &typeDatabase, records, documents,
                       buckets, memberIndices, &error);
    assert(session.active && findCandidate(session, "width") >= 0);
    assert(findCandidate(session, "show") >= 0);
    const int method = findCandidate(session, "show");
    assert(method >= 0 && session.candidates[method].kind == CompletionCandidateKind::Method);

    const char referenceText[] =
        "struct Window { int width; };\n"
        "void draw(Window& window) { window.";
    buildMemberSession(referenceText, &session, candidates, &typeDatabase, records, documents,
                       buckets, memberIndices, &error);
    assert(session.active && findCandidate(session, "width") >= 0);
}

static void testTypeAwareAliasesAutoAndShadowing() {
    const char text[] =
        "struct A { int fromA; };\n"
        "struct B { int fromB; };\n"
        "using MainWindow = A;\n"
        "using WindowPtr = B*;\n"
        "B* currentWindow();\n"
        "MainWindow window;\n"
        "WindowPtr pointer;\n"
        "auto inferred = currentWindow();\n"
        "void test() { B value; value.";
    static CompletionCandidate candidates[128] = {};
    static CompletionSession session = {};
    static TypeDatabase typeDatabase = {};
    static TypeRecord records[1024] = {};
    static TypeDocument documents[8] = {};
    static TypeMemberBucket buckets[64] = {};
    static uint32_t memberIndices[1024] = {};
    CompletionErrorCode error = CompletionErrorCode::None;

    buildMemberSession("struct A { int fromA; };\nstruct B { int fromB; };\nusing MainWindow = A;\nMainWindow window;\nwindow.",
                       &session, candidates, &typeDatabase, records, documents, buckets,
                       memberIndices, &error);
    assert(session.active && findCandidate(session, "fromA") >= 0 && findCandidate(session, "fromB") < 0);

    buildMemberSession("struct B { int fromB; };\nusing WindowPtr = B*;\nWindowPtr pointer;\npointer->",
                       &session, candidates, &typeDatabase, records, documents, buckets,
                       memberIndices, &error);
    assert(session.active && findCandidate(session, "fromB") >= 0);

    buildMemberSession("struct B { int fromB; };\nB* currentWindow();\nauto inferred = currentWindow();\ninferred->",
                       &session, candidates, &typeDatabase, records, documents, buckets,
                       memberIndices, &error);
    assert(session.active && session.context.memberResolution == CompletionMemberResolution::Conservative &&
           findCandidate(session, "fromB") >= 0);

    buildMemberSession(text, &session, candidates, &typeDatabase, records, documents, buckets,
                       memberIndices, &error);
    assert(session.active && findCandidate(session, "fromB") >= 0 && findCandidate(session, "fromA") < 0);
}

static void testTypeAwareSafetyAndFiltering() {
    static CompletionCandidate candidates[128] = {};
    static CompletionSession session = {};
    static TypeDatabase typeDatabase = {};
    static TypeRecord records[1024] = {};
    static TypeDocument documents[8] = {};
    static TypeMemberBucket buckets[64] = {};
    static uint32_t memberIndices[1024] = {};
    CompletionErrorCode error = CompletionErrorCode::None;

    buildMemberSession("struct Window { int width; };\nWindow* window;\nwindow.",
                       &session, candidates, &typeDatabase, records, documents, buckets,
                       memberIndices, &error);
    assert(!session.active && session.context.memberResolution == CompletionMemberResolution::WrongOperator);
    buildMemberSession("struct Window { int width; };\nWindow window;\nwindow->",
                       &session, candidates, &typeDatabase, records, documents, buckets,
                       memberIndices, &error);
    assert(!session.active && session.context.memberResolution == CompletionMemberResolution::WrongOperator);
    buildMemberSession("struct Window { int width; };\nWindow** window;\nwindow->",
                       &session, candidates, &typeDatabase, records, documents, buckets,
                       memberIndices, &error);
    assert(!session.active && session.context.memberResolution == CompletionMemberResolution::PointerDepthUnsupported);
    buildMemberSession("struct Window { int width; };\nconst Window window;\nwindow.wi",
                       &session, candidates, &typeDatabase, records, documents, buckets, memberIndices, &error);
    assert(session.active && session.candidateCount == 1 && findCandidate(session, "width") >= 0);
    buildMemberSession("auto window = somethingUnknown();\nwindow.",
                       &session, candidates, &typeDatabase, records, documents, buckets, memberIndices, &error);
    assert(!session.active && session.context.memberResolution == CompletionMemberResolution::Unknown);
    buildMemberSession("struct A { int fromA; };\nA make();\nA make();\nauto window = make();\nwindow.",
                       &session, candidates, &typeDatabase, records, documents, buckets, memberIndices, &error);
    assert(!session.active && session.context.memberResolution == CompletionMemberResolution::Ambiguous);
    buildMemberSession("// window.\nconst char* value = \"window.\";\n.",
                       &session, candidates, &typeDatabase, records, documents, buckets, memberIndices, &error);
    assert(!session.active);

    static Document staleDocument = {};
    const char staleText[] = "struct Window { int width; };\nWindow window;\nwindow.";
    prepareDocument(&staleDocument, "src/member.cpp", staleText);
    setCaret(&staleDocument, staleDocument.buffer.length);
    buildTypeIndex(staleDocument, staleText, &typeDatabase, records, documents, buckets, memberIndices);
    assert(TextBufferInsert(&staleDocument.buffer, "x", 1));
    CompletionSessionInit(&session, candidates, 128);
    assert(CompletionBuildSession(&session, staleDocument, CompletionProjectId("demo"), 8,
                                  nullptr, nullptr, true, &typeDatabase, &error));
    assert(!session.active && session.context.memberResolution == CompletionMemberResolution::Stale);
}

static void testTypeAwareOverloadsAndTruncation() {
    const char text[] =
        "struct Window {\n"
        "    int width;\n"
        "    int weight;\n"
        "    void setValue(int);\n"
        "    void setValue(float);\n"
        "};\n"
        "Window window;\n"
        "window.se";
    static CompletionCandidate candidates[128] = {};
    static CompletionSession session = {};
    static TypeDatabase typeDatabase = {};
    static TypeRecord records[1024] = {};
    static TypeDocument documents[8] = {};
    static TypeMemberBucket buckets[64] = {};
    static uint32_t memberIndices[1024] = {};
    CompletionErrorCode error = CompletionErrorCode::None;
    buildMemberSession(text, &session, candidates, &typeDatabase, records, documents,
                       buckets, memberIndices, &error);
    assert(session.active && session.candidateCount == 1);
    assert(strcmp(session.candidates[0].insertionText, "setValue") == 0);
    assert(session.candidates[0].overloadCount >= 2);

    buildMemberSession("struct Window { int zeta; int alpha; int middle; };\nWindow window;\nwindow.",
                       &session, candidates, &typeDatabase, records, documents, buckets,
                       memberIndices, &error);
    assert(session.active && session.candidateCount == 3);
    assert(strcmp(session.candidates[0].insertionText, "alpha") == 0);

    const char truncatedText[] =
        "struct Window { int zeta; int alpha; int middle; int omega; };\n"
        "Window window;\nwindow.";
    static Document truncatedDocument = {};
    prepareDocument(&truncatedDocument, "src/member.cpp", truncatedText);
    setCaret(&truncatedDocument, truncatedDocument.buffer.length);
    buildTypeIndex(truncatedDocument, truncatedText, &typeDatabase, records, documents, buckets,
                   memberIndices, 2);
    CompletionSessionInit(&session, candidates, 128);
    assert(CompletionBuildSession(&session, truncatedDocument, CompletionProjectId("demo"), 8,
                                  nullptr, nullptr, true, &typeDatabase, &error));
    assert(session.active && session.truncated && session.candidateCount == 2);
}

static void testSharedKeywords() {
    assert(SyntaxIsKeyword("bool"));
    assert(SyntaxKeywordCount(SyntaxLanguage::Cpp) > 20);
    bool foundReturn = false;
    bool foundBool = false;
    for (uint32_t i = 0; i < SyntaxKeywordCount(SyntaxLanguage::Cpp); ++i) {
        const char* word = SyntaxKeywordAt(SyntaxLanguage::Cpp, i);
        if (strcmp(word, "return") == 0) foundReturn = true;
        if (strcmp(word, "bool") == 0) foundBool = true;
    }
    assert(foundReturn && foundBool);
}

static void testContextExtraction() {
    static Document document = {};
    const char text[] = "namespace guideXOS {\n  BuildPro\n}\n";
    prepareDocument(&document, "src/context.cpp", text);
    setCaret(&document, static_cast<uint32_t>(strlen("namespace guideXOS {\n  BuildPro")));
    CompletionContext context = {};
    CompletionErrorCode error = CompletionErrorCode::None;
    assert(CompletionExtractContext(document, 7, 3, 9, true, &context, &error));
    assert(strcmp(context.prefix, "BuildPro") == 0);
    assert(context.replacementEnd - context.replacementStart == 8);
    assert(context.kind == CompletionContextKind::Identifier);

    const char qualified[] = "guideXOS::Bui";
    prepareDocument(&document, "src/context.cpp", qualified);
    setCaret(&document, static_cast<uint32_t>(strlen(qualified)));
    assert(CompletionExtractContext(document, 7, 3, 10, true, &context, &error));
    assert(context.hasExplicitQualifier);
    assert(strcmp(context.explicitQualifier, "guideXOS") == 0);
    assert(strcmp(context.prefix, "Bui") == 0);

    const char member[] = "renderer.Dr";
    prepareDocument(&document, "src/context.cpp", member);
    setCaret(&document, static_cast<uint32_t>(strlen(member)));
    assert(CompletionExtractContext(document, 7, 3, 11, true, &context, &error));
    assert(context.kind == CompletionContextKind::MemberAccessLexical);

    const char comment[] = "// BuildPro";
    prepareDocument(&document, "src/context.cpp", comment);
    setCaret(&document, static_cast<uint32_t>(strlen(comment)));
    assert(!CompletionExtractContext(document, 7, 3, 12, true, &context, &error));
    assert(error == CompletionErrorCode::InComment);

    const char inactive[] = "#if 0\nBuildPro\n#endif\n";
    prepareDocument(&document, "src/context.cpp", inactive);
    setCaret(&document, 14);
    assert(!CompletionExtractContext(document, 7, 3, 13, true, &context, &error));
    assert(error == CompletionErrorCode::UnsupportedContext);
}

static void testSymbolsWordsAndQualifiedCompletion() {
    const char text[] =
        "namespace guideXOS {\n"
        "void BuildProject();\n"
        "void BuildProject(int value);\n"
        "class Renderer { public: void Draw(); };\n"
        "}\n"
        "int localTemporary = 0;\n"
        "// commentWord\n"
        "const char* stringValue = \"stringWord\";\n";
    static Document document = {};
    prepareDocument(&document, "src/main.cpp", text);
    static ProjectSymbol projectStorage[256] = {};
    static SymbolDocument documentStorage[16] = {};
    static DocumentSymbol scratchStorage[256] = {};
    static SymbolDatabase database = {};
    buildIndex(document, text, &database, projectStorage, documentStorage, scratchStorage);
    static DocumentWordEntry words[128] = {};
    static DocumentWordCache wordCache = {};
    DocumentWordCacheInit(&wordCache, words, 128);
    static CompletionCandidate candidates[128] = {};
    static CompletionSession session = {};
    CompletionSessionInit(&session, candidates, 128);

    const char prefix[] = "guideXOS::BuildPro";
    TextBufferSet(&document.buffer, prefix, static_cast<uint32_t>(strlen(prefix)));
    document.syntax.language = SyntaxLanguage::Cpp;
    assert(SyntaxCacheBuild(&document.syntax, SyntaxLanguage::Cpp, document.buffer.data,
                            document.buffer.length, document.buffer.generation));
    setCaret(&document, document.buffer.length);
    CompletionErrorCode error = CompletionErrorCode::None;
    assert(CompletionBuildSession(&session, document, CompletionProjectId("demo"), 8,
                                  &database, &wordCache, true, &error));
    int buildIndexResult = findCandidate(session, "BuildProject");
    assert(buildIndexResult >= 0);
    assert(session.candidates[buildIndexResult].source == CompletionCandidateSource::CurrentDocument ||
           session.candidates[buildIndexResult].source == CompletionCandidateSource::CurrentScope);
    assert(session.candidates[buildIndexResult].overloadCount >= 2);

    TextBufferSet(&document.buffer, "localTem", 8);
    assert(SyntaxCacheBuild(&document.syntax, SyntaxLanguage::Cpp, document.buffer.data,
                            document.buffer.length, document.buffer.generation));
    setCaret(&document, document.buffer.length);
    assert(CompletionBuildSession(&session, document, CompletionProjectId("demo"), 8,
                                  &database, &wordCache, true, &error));
    // The cache is generation-aware and may be rebuilt from this active buffer.
    assert(findCandidate(session, "localTemporary") < 0 || findCandidate(session, "localTem") >= 0);
}

static void testDocumentWordFilteringAndStaleState() {
    const char text[] = "int localTemporary = 1;\n// commentWord\nconst char* value = \"stringWord\";\n";
    static Document document = {};
    prepareDocument(&document, "src/words.cpp", text);
    static DocumentWordEntry entries[32] = {};
    static DocumentWordCache cache = {};
    DocumentWordCacheInit(&cache, entries, 32);
    CompletionErrorCode error = CompletionErrorCode::None;
    assert(DocumentWordCacheRefresh(&cache, document, &error));
    bool local = false;
    bool comment = false;
    bool string = false;
    for (uint32_t i = 0; i < cache.count; ++i) {
        if (strcmp(cache.entries[i].word, "localTemporary") == 0) local = true;
        if (strcmp(cache.entries[i].word, "commentWord") == 0) comment = true;
        if (strcmp(cache.entries[i].word, "stringWord") == 0) string = true;
    }
    assert(local && !comment && !string);

    static CompletionCandidate candidates[32] = {};
    static CompletionSession session = {};
    CompletionSessionInit(&session, candidates, 32);
    setCaret(&document, 0);
    assert(CompletionBuildSession(&session, document, CompletionProjectId("demo"), 1,
                                  nullptr, &cache, true, &error));
    assert(session.active);
    TextBufferInsert(&document.buffer, "x", 1);
    assert(!CompletionSessionIsCurrent(&session, document, CompletionProjectId("demo"), 1, &error));
    assert(error == CompletionErrorCode::DocumentStale);
}

int main() {
    testSharedKeywords();
    testContextExtraction();
    testSymbolsWordsAndQualifiedCompletion();
    testDocumentWordFilteringAndStaleState();
    testTypeAwareDirectMembers();
    testTypeAwarePointerReferenceAndParameter();
    testTypeAwareAliasesAutoAndShadowing();
    testTypeAwareSafetyAndFiltering();
    testTypeAwareOverloadsAndTruncation();
    printf("Developer Studio completion model PASS\n");
    return 0;
}
