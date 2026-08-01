#include <stdio.h>
#include <string.h>

#include "developer_studio_signature.h"

using namespace guidexos::developer_studio;

namespace {

static int g_failures = 0;
static Document g_document = {};
static ProjectSymbol g_projectSymbols[128] = {};
static SymbolDocument g_symbolDocuments[16] = {};
static DocumentSymbol g_scratchSymbols[256] = {};
static SignatureCandidate g_candidates[kSignatureMaxRetainedCandidates] = {};
static SignatureParameter g_parameters[kSignatureMaxRetainedCandidates * kSignatureMaxParameters] = {};
static SymbolDatabase g_database = {};

static void expect(bool condition, const char* message) {
    if (condition) return;
    ++g_failures;
    fprintf(stderr, "FAIL: %s\n", message);
}

static uint32_t findMarker(const char* text) {
    uint32_t i = 0;
    while (text && text[i] != '\0' && text[i] != '|') ++i;
    return i;
}

static void loadDocument(const char* textWithMarker) {
    g_document = {};
    g_document.used = true;
    g_document.documentId = 7;
    strcpy(g_document.path, "C:/project/main.cpp");
    strcpy(g_document.name, "main.cpp");
    const uint32_t marker = findMarker(textWithMarker);
    char bytes[kMaxEditorBytes + 1] = {};
    uint32_t output = 0;
    for (uint32_t i = 0; textWithMarker[i] != '\0'; ++i) if (textWithMarker[i] != '|') bytes[output++] = textWithMarker[i];
    TextBufferInit(&g_document.buffer);
    TextBufferSet(&g_document.buffer, bytes, output);
    g_document.buffer.caret = marker <= output ? marker : output;
    DocumentUpdateSyntax(&g_document);
}

static void indexDocument(const char* source) {
    SymbolDatabaseInit(&g_database, g_projectSymbols, 128, g_symbolDocuments, 16, g_scratchSymbols, 256);
    const bool indexed = SymbolDatabaseIndexDocument(&g_database, "C:/project/library.hpp", 99, 1, false,
                                                     source, static_cast<uint32_t>(strlen(source)));
    expect(indexed, "fixture source should index");
}

static SignatureHelpSession buildSession(SignatureErrorCode* error) {
    SignatureHelpSession session = {};
    SignatureHelpSessionInit(&session, g_candidates, kSignatureMaxRetainedCandidates,
                             g_parameters, kSignatureMaxRetainedCandidates * kSignatureMaxParameters);
    session.sessionId = 4;
    SignatureHelpBuildSession(&session, g_document, SignatureProjectId("project"), 1, &g_database, error);
    return session;
}

static void testSimpleAndIndex() {
    indexDocument("namespace guideXOS { void BuildProject(Project& project, const BuildOptions& options, BuildResult* result) {} }\n");
    loadDocument("BuildProject(project, |");
    SignatureErrorCode error = SignatureErrorCode::None;
    SignatureHelpSession session = buildSession(&error);
    expect(session.active, "simple call opens Signature Help");
    expect(session.context.callableName[0] == 'B', "callable name is extracted");
    expect(session.context.activeArgumentIndex == 1, "comma advances active argument");
    expect(session.candidateCount == 1, "stored function signature is found");
    expect(SignatureHelpSessionActiveParameter(&session) != nullptr, "active parameter is available");
    expect(SignatureHelpSessionActiveParameter(&session) &&
               strcmp(SignatureHelpSessionActiveParameter(&session)->name, "options") == 0,
           "parameter name is parsed");
}

static void testNestedAndIgnoredCommas() {
    indexDocument("void BuildProject(Project& project, const BuildOptions& options, BuildResult* result) {}\n");
    loadDocument("BuildProject(CreateProject(a, b), {1, 2, 3}, \"a,b\", |");
    SignatureErrorCode error = SignatureErrorCode::None;
    SignatureHelpSession session = buildSession(&error);
    expect(session.active, "nested call remains active");
    expect(session.context.activeArgumentIndex == 3, "nested delimiters and string commas are ignored");
    loadDocument("BuildProject(CreateProject(a, b))|");
    session = buildSession(&error);
    expect(!session.active, "caret after nested call closes outer context when no outer argument is active");
}

static void testQualifiedAndMemberCalls() {
    indexDocument("namespace guideXOS { void BuildProject(Project& project) {} }\nclass Renderer { void Draw(int x, int y) {} };\n");
    loadDocument("guideXOS::BuildProject(|");
    SignatureErrorCode error = SignatureErrorCode::None;
    SignatureHelpSession session = buildSession(&error);
    expect(session.active, "qualified function call opens");
    expect(session.context.kind == SignatureContextKind::QualifiedFunctionCall, "qualified context is labeled");
    expect(strcmp(session.context.explicitQualifier, "guideXOS") == 0, "qualifier is extracted");
    loadDocument("renderer.Draw(|");
    session = buildSession(&error);
    expect(session.active, "member call opens with lexical lookup");
    expect(session.context.kind == SignatureContextKind::MethodCallLexical, "member call is lexical");
    expect(session.candidates[0].lexicallyAmbiguous, "member candidate carries ambiguity");
}

static void testOverloadsAndNavigation() {
    indexDocument("void BuildProject(Project& project) {}\nvoid BuildProject(Project& project, int flags) {}\nvoid BuildProject(Project& project, int flags, BuildResult* result) {}\n");
    loadDocument("BuildProject(project, |");
    SignatureErrorCode error = SignatureErrorCode::None;
    SignatureHelpSession session = buildSession(&error);
    expect(session.active && session.candidateCount == 3, "overloads remain separate");
    const uint64_t first = SignatureHelpSessionSelected(&session)->candidateId;
    SignatureHelpSessionMove(&session, 1);
    expect(SignatureHelpSessionSelected(&session)->candidateId != first, "Down selects another overload");
    SignatureHelpSessionEnd(&session);
    expect(session.selectedSignatureIndex == session.candidateCount - 1, "End selects last overload");
    SignatureHelpSessionHome(&session);
    expect(session.selectedSignatureIndex == 0, "Home selects first overload");
}

static void testParametersAndBounds() {
    indexDocument("void SetCallback(void (*callback)(int, int), const std::vector<std::pair<int, int>>& values, Args&&... args) {}\n");
    loadDocument("SetCallback(callback, |");
    SignatureErrorCode error = SignatureErrorCode::None;
    SignatureHelpSession session = buildSession(&error);
    expect(session.active, "complex parameter signature opens");
    expect(session.candidates[0].parameterCount == 3, "nested parameter commas are ignored");
    expect(session.candidates[0].parameterParseFailed == false, "complex parameter signature is precise");
    expect(session.parameters[session.candidates[0].parameterStart + 2].variadic, "variadic parameter is marked");
    loadDocument("BuildProject(|");
    session = buildSession(&error);
    expect(!session.active, "unknown callable has no candidates");
    expect(error == SignatureErrorCode::NoCandidates, "unknown callable reports no candidates");
}

static void testExcludedContextsAndStale() {
    indexDocument("void BuildProject(Project& project) {}\n");
    loadDocument("// BuildProject(|\n");
    SignatureErrorCode error = SignatureErrorCode::None;
    SignatureHelpSession session = buildSession(&error);
    expect(!session.active && error == SignatureErrorCode::InComment, "comments are excluded");
    loadDocument("const char* text = \"BuildProject(|\";\n");
    session = buildSession(&error);
    expect(!session.active && error == SignatureErrorCode::InString, "strings are excluded");
    loadDocument("const char* text = R\"(BuildProject(|)\";\n");
    session = buildSession(&error);
    expect(!session.active && error == SignatureErrorCode::InRawString, "raw strings are excluded");
    loadDocument("char value = '(|';\n");
    session = buildSession(&error);
    expect(!session.active && error == SignatureErrorCode::InCharacter, "character literals are excluded");
    loadDocument("#if 0\nBuildProject(|\n#endif\n");
    session = buildSession(&error);
    expect(!session.active && error == SignatureErrorCode::InPreprocessor, "inactive if-zero region is excluded");
    loadDocument("BuildProject(|");
    session = buildSession(&error);
    expect(SignatureHelpSessionIsCurrent(&session, g_document, SignatureProjectId("project"), 1, &error), "current session generation is valid");
    ++g_document.buffer.generation;
    expect(!SignatureHelpSessionIsCurrent(&session, g_document, SignatureProjectId("project"), 1, &error) &&
               error == SignatureErrorCode::DocumentStale, "document generation invalidates session");
}

} // namespace

int main() {
    testSimpleAndIndex();
    testNestedAndIgnoredCommas();
    testQualifiedAndMemberCalls();
    testOverloadsAndNavigation();
    testParametersAndBounds();
    testExcludedContextsAndStale();
    if (g_failures != 0) {
        fprintf(stderr, "%d signature help tests failed\n", g_failures);
        return 1;
    }
    printf("Developer Studio signature help model PASS\n");
    return 0;
}
