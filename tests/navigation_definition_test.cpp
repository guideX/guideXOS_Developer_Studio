#include "developer_studio_navigation.h"

#include <cassert>
#include <string>

using namespace guidexos::developer_studio;

static void testIdentifierExtraction() {
    DefinitionIdentifier result = {};
    const char* text = "  utf8: \xC3\xA9 Renderer::Draw_2  ";
    const uint32_t length = static_cast<uint32_t>(std::string(text).size());
    const uint32_t draw = static_cast<uint32_t>(std::string(text).find("Draw_2"));
    assert(ExtractDefinitionIdentifier(text, length, draw, false, 0, 0, &result));
    assert(std::string(result.text) == "Draw_2");
    assert(result.start == draw);
    assert(ExtractDefinitionIdentifier(text, length, draw + 6, false, 0, 0, &result));
    assert(std::string(result.text) == "Draw_2");
    const uint32_t renderer = static_cast<uint32_t>(std::string(text).find("Renderer"));
    assert(ExtractDefinitionIdentifier(text, length, renderer + 2, true, renderer, renderer + 8, &result));
    assert(result.fromSelection && std::string(result.text) == "Renderer");
    assert(!ExtractDefinitionIdentifier("   ->  \n", 9, 3, false, 0, 0, &result));
    assert(ExtractDefinitionIdentifier("~Renderer", 9, 5, false, 0, 0, &result));
    assert(std::string(result.text) == "~Renderer");
    assert(!ExtractDefinitionIdentifier("123name", 7, 4, false, 0, 0, &result));
    std::string oversized(1025, 'a');
    assert(!ExtractDefinitionIdentifier(oversized.c_str(), 1025, 500, false, 0, 0, &result));
    assert(result.tooLong);
}

static void testRolesAndQualifiedNames() {
    const char* source =
        "namespace guideXOS { class Renderer; class Renderer { public: void Draw(int x); };\n"
        "void Renderer::Draw(int x) { } using Handle = int; int BuildProject(); int BuildProject() { return 1; } }";
    DocumentSymbol symbols[64] = {};
    SymbolScanResult scan = {};
    assert(ScanDocumentSymbols(source, static_cast<uint32_t>(std::string(source).size()), 7, 3,
                               symbols, 64, &scan));
    const DocumentSymbol* forward = nullptr;
    const DocumentSymbol* renderer = nullptr;
    const DocumentSymbol* methodDefinition = nullptr;
    const DocumentSymbol* alias = nullptr;
    const DocumentSymbol* functionDefinition = nullptr;
    for (uint32_t i = 0; i < scan.symbolCount; ++i) {
        if (std::string(symbols[i].name) == "Renderer" && symbols[i].declarationRole == SymbolDeclarationRole::ForwardDeclaration) forward = &symbols[i];
        if (std::string(symbols[i].name) == "Renderer" && symbols[i].declarationRole == SymbolDeclarationRole::Definition) renderer = &symbols[i];
        if (std::string(symbols[i].name) == "Draw" && symbols[i].declarationRole == SymbolDeclarationRole::Definition) methodDefinition = &symbols[i];
        if (std::string(symbols[i].name) == "Handle") alias = &symbols[i];
        if (std::string(symbols[i].name) == "BuildProject" && symbols[i].declarationRole == SymbolDeclarationRole::Definition) functionDefinition = &symbols[i];
    }
    assert(forward && renderer && methodDefinition && alias && functionDefinition);
    assert(std::string(renderer->qualifiedName) == "guideXOS::Renderer");
    assert(std::string(methodDefinition->qualifiedName) == "guideXOS::Renderer::Draw");
    assert(std::string(methodDefinition->signature).find("Draw(") == 0);
    assert(alias->declarationRole == SymbolDeclarationRole::Alias);
    assert(functionDefinition->declarationRole == SymbolDeclarationRole::Definition);
}

static void testLookupRanking() {
    static ProjectSymbol projectStorage[128];
    static SymbolDocument documentStorage[8];
    static DocumentSymbol scratchStorage[128];
    SymbolDatabase database = {};
    SymbolDatabaseInit(&database, projectStorage, 128, documentStorage, 8, scratchStorage, 128);
    const char* header = "namespace A { class Renderer { public: void Draw(); }; void BuildProject(); }";
    const char* source = "namespace A { void Renderer::Draw() {} void BuildProject() {} }";
    assert(SymbolDatabaseIndexDocument(&database, "C:/project/include/api.h", 1, 1, false,
                                      header, static_cast<uint32_t>(std::string(header).size())));
    assert(SymbolDatabaseIndexDocument(&database, "C:/project/src/api.cpp", 2, 1, false,
                                      source, static_cast<uint32_t>(std::string(source).size())));
    static Document document = {};
    document.used = true;
    document.documentId = 3;
    TextBufferInit(&document.buffer);
    const char* call = "namespace A { void use() { BuildProject(); } }";
    assert(TextBufferSet(&document.buffer, call, static_cast<uint32_t>(std::string(call).size())));
    document.buffer.caret = static_cast<uint32_t>(std::string(call).find("BuildProject")) + 3;
    DefinitionQuery query = {};
    assert(BuildDefinitionQuery(document, "project", 1, "C:/project", "src/use.cpp", 9, &query));
    assert(std::string(query.identifier) == "BuildProject");
    DefinitionCandidate candidates[16] = {};
    DefinitionResolution resolution = {};
    assert(ResolveDefinition(&database, query, candidates, 16, &resolution));
    assert(resolution.kind == DefinitionResolutionKind::Direct);
    assert(resolution.candidateCount >= 2);
    assert(candidates[0].isDefinition);
    assert(std::string(candidates[0].relativePath) == "src/api.cpp");

    document.buffer.caret = static_cast<uint32_t>(std::string(call).find("BuildProject")) + 3;
    document.buffer.selectionActive = false;
    assert(BuildDefinitionQuery(document, "project", 1, "C:/project", "src/use.cpp", 10, &query));
    assert(std::string(query.containingScope) == "A");
}

static void makeLocation(NavigationLocation& location, const char* path, uint64_t offset) {
    location = {};
    location.projectId[0] = 'p'; location.projectId[1] = '\0';
    location.projectGeneration = 1;
    location.documentId = 1;
    location.documentGeneration = 1;
    for (uint32_t i = 0; path[i] != '\0'; ++i) location.relativePath[i] = path[i];
    location.caretByteOffset = offset;
}

static void testHistory() {
    static NavigationHistory history = {};
    NavigationHistoryInit(&history);
    NavigationLocation first = {}, second = {}, current = {}, destination = {};
    makeLocation(first, "a.cpp", 1);
    makeLocation(second, "b.cpp", 2);
    makeLocation(current, "c.cpp", 3);
    NavigationHistoryPush(&history, first);
    NavigationHistoryPush(&history, first);
    assert(history.backCount == 1);
    NavigationHistoryPush(&history, second);
    assert(NavigationHistoryBack(&history, current, &destination));
    assert(std::string(destination.relativePath) == "b.cpp");
    assert(NavigationHistoryForward(&history, second, &destination));
    assert(std::string(destination.relativePath) == "c.cpp");
    NavigationHistoryPush(&history, first);
    assert(history.forwardCount == 0);
    for (uint32_t i = 0; i < kNavigationHistoryCapacity + 5; ++i) {
        NavigationLocation value = {};
        makeLocation(value, "a.cpp", i + 10);
        NavigationHistoryPush(&history, value);
    }
    assert(history.backCount <= kNavigationHistoryCapacity);
}

int main() {
    testIdentifierExtraction();
    testRolesAndQualifiedNames();
    testLookupRanking();
    testHistory();
    return 0;
}
