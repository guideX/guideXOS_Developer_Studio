#include "developer_studio_relationships.h"

#include <cassert>
#include <cstring>

using namespace guidexos::developer_studio;

namespace {

static ProjectSymbol g_symbols[256] = {};
static SymbolDocument g_documents[16] = {};
static DocumentSymbol g_scratch[256] = {};
static SymbolRelationshipGroup g_groups[64] = {};
static SymbolRelationship g_edges[256] = {};
static uint32_t g_declarations[256] = {};
static uint32_t g_definitions[256] = {};
static uint32_t g_forwards[256] = {};
static uint32_t g_symbolGroups[256] = {};

static void index(SymbolDatabase* database, const char* path, uint64_t id, const char* text) {
    assert(SymbolDatabaseIndexDocument(database, path, id, 1, false, text,
                                       static_cast<uint32_t>(std::strlen(text))));
}

static const ProjectSymbol* findSymbol(const SymbolDatabase& database, const char* name,
                                       SymbolDeclarationRole role) {
    for (uint32_t i = 0; i < SymbolDatabaseProjectSymbolCount(&database); ++i) {
        const ProjectSymbol* symbol = SymbolDatabaseProjectSymbolAt(&database, i);
        if (symbol && std::strcmp(symbol->symbol.name, name) == 0 && symbol->symbol.declarationRole == role) return symbol;
    }
    return nullptr;
}

static const ProjectSymbol* findSymbolKind(const SymbolDatabase& database, const char* name,
                                           SymbolDeclarationRole role, SymbolKind kind) {
    for (uint32_t i = 0; i < SymbolDatabaseProjectSymbolCount(&database); ++i) {
        const ProjectSymbol* symbol = SymbolDatabaseProjectSymbolAt(&database, i);
        if (symbol && std::strcmp(symbol->symbol.name, name) == 0 && symbol->symbol.declarationRole == role &&
            symbol->symbol.kind == kind) return symbol;
    }
    return nullptr;
}

static uint32_t build(SymbolDatabase* database, SymbolRelationshipGraph* graph) {
    SymbolRelationshipGraphStorage storage = {};
    SymbolRelationshipGraphStorageInit(&storage, g_groups, 64, g_edges, 256,
                                       g_declarations, 256, g_definitions, 256,
                                       g_forwards, 256, g_symbolGroups, 256);
    SymbolRelationshipGraphInit(graph, &storage, "relationship-test", 7,
                                database->symbolDatabaseGeneration);
    SymbolRelationshipGraph building = {};
    SymbolRelationshipGraphInit(&building, &storage, "relationship-test", 7,
                                database->symbolDatabaseGeneration);
    // Use separate graph metadata over the same storage only for this model
    // test; the service swaps graph ownership after completion.
    SymbolRelationshipGraphService service = {};
    SymbolRelationshipGraphServiceInit(&service, graph, &building);
    uint64_t operation = 0;
    assert(SymbolRelationshipGraphBuildStart(&service, database, nullptr, "relationship-test", 7, "", 0, &operation));
    while (SymbolRelationshipGraphBuildIsActive(&service)) assert(SymbolRelationshipGraphBuildPoll(&service, operation, 8, 0));
    assert(SymbolRelationshipGraphBuildInfo(&service)->state == RelationshipGraphState::Completed);
    *graph = *service.completedGraph;
    return service.completedGraph->relationshipCount;
}

} // namespace

int main() {
    SymbolDatabase database = {};
    SymbolDatabaseInit(&database, g_symbols, 256, g_documents, 16, g_scratch, 256);
    index(&database, "include/renderer.h", 1,
          "class Renderer;\n"
          "class Renderer { public: void Draw(int x); Renderer(int mode); ~Renderer(); static int instanceCount; };\n"
          "int BuildProject(const BuildOptions& options = BuildOptions());\n");
    index(&database, "src/renderer.cpp", 2,
          "void Renderer::Draw(int x) {}\n"
          "Renderer::Renderer(int mode) {}\n"
          "Renderer::~Renderer() {}\n"
          "int Renderer::instanceCount = 0;\n"
          "int BuildProject(const BuildOptions& options) { return 1; }\n");

    char normalized[2049] = {};
    uint32_t parameterCount = 0;
    bool complete = false;
    bool approximate = false;
    assert(NormalizeRelationshipSignature("BuildProject( const BuildOptions &options = BuildOptions() )",
                                          normalized, sizeof(normalized), &parameterCount,
                                          &complete, &approximate));
    assert(std::strcmp(normalized, "BuildProject(const BuildOptions&)") == 0);
    assert(parameterCount == 1 && complete && !approximate);
    assert(NormalizeRelationshipSignature("Call(void (*callback)(int value), int values[4])",
                                          normalized, sizeof(normalized), &parameterCount,
                                          &complete, &approximate));
    assert(std::strcmp(normalized, "Call(void(*)(int),int[4])") == 0);

    SymbolRelationshipGraph graph = {};
    const uint32_t relationshipCount = build(&database, &graph);
    assert(relationshipCount >= 5);
    const ProjectSymbol* drawDeclaration = findSymbol(database, "Draw", SymbolDeclarationRole::Declaration);
    const ProjectSymbol* drawDefinition = findSymbol(database, "Draw", SymbolDeclarationRole::Definition);
    const ProjectSymbol* buildDeclaration = findSymbol(database, "BuildProject", SymbolDeclarationRole::Declaration);
    const ProjectSymbol* buildDefinition = findSymbol(database, "BuildProject", SymbolDeclarationRole::Definition);
    const ProjectSymbol* rendererForward = findSymbol(database, "Renderer", SymbolDeclarationRole::ForwardDeclaration);
    const ProjectSymbol* rendererDefinition = findSymbol(database, "Renderer", SymbolDeclarationRole::Definition);
    const ProjectSymbol* staticDeclaration = findSymbol(database, "instanceCount", SymbolDeclarationRole::Declaration);
    const ProjectSymbol* staticDefinition = findSymbol(database, "instanceCount", SymbolDeclarationRole::Definition);
    const ProjectSymbol* constructorDeclaration = findSymbolKind(database, "Renderer", SymbolDeclarationRole::Declaration, SymbolKind::Constructor);
    const ProjectSymbol* constructorDefinition = findSymbolKind(database, "Renderer", SymbolDeclarationRole::Definition, SymbolKind::Constructor);
    assert(drawDeclaration && drawDefinition && buildDeclaration && buildDefinition && rendererForward && rendererDefinition);
    assert(staticDeclaration && staticDefinition && constructorDeclaration && constructorDefinition);
    const uint64_t drawDeclarationId = SymbolRelationshipSymbolId(*drawDeclaration, "include/renderer.h");
    const uint64_t drawDefinitionId = SymbolRelationshipSymbolId(*drawDefinition, "src/renderer.cpp");
    const uint64_t buildDeclarationId = SymbolRelationshipSymbolId(*buildDeclaration, "include/renderer.h");
    const uint64_t rendererForwardId = SymbolRelationshipSymbolId(*rendererForward, "include/renderer.h");
    const uint64_t staticDeclarationId = SymbolRelationshipSymbolId(*staticDeclaration, "include/renderer.h");
    const uint64_t staticDefinitionId = SymbolRelationshipSymbolId(*staticDefinition, "src/renderer.cpp");
    SymbolRelationship candidates[8] = {};
    assert(SymbolRelationshipGraphFindDefinitions(&graph, drawDeclarationId, candidates, 8) >= 1);
    assert(candidates[0].target.symbolId == drawDefinitionId || candidates[0].source.symbolId == drawDefinitionId);
    assert(candidates[0].confidence == SymbolRelationshipConfidence::Exact);
    assert(SymbolRelationshipGraphHasRelationship(&graph, drawDeclarationId, drawDefinitionId));
    assert(SymbolRelationshipGraphFindDefinitions(&graph, buildDeclarationId, candidates, 8) >= 1);
    assert(SymbolRelationshipGraphFindDefinitions(&graph, rendererForwardId, candidates, 8) >= 1);
    assert(SymbolRelationshipGraphFindDefinitions(&graph, staticDeclarationId, candidates, 8) >= 1);
    assert(candidates[0].target.symbolId == staticDefinitionId || candidates[0].source.symbolId == staticDefinitionId);
    const uint64_t rendererDefinitionId = SymbolRelationshipSymbolId(*rendererDefinition, "include/renderer.h");
    assert(rendererDefinitionId != rendererForwardId);
    assert(SymbolRelationshipGraphFindDeclarations(&graph, drawDefinitionId, candidates, 8) >= 1);
    assert(SymbolRelationshipGraphIsCurrent(&graph, "relationship-test", 7, database.symbolDatabaseGeneration));
    assert(!SymbolRelationshipGraphIsCurrent(&graph, "relationship-test", 8, database.symbolDatabaseGeneration));

    SymbolRelationshipGraph cancelledCompleted = {};
    SymbolRelationshipGraph cancelledBuilding = {};
    SymbolRelationshipGraphStorage cancelStorage = {};
    SymbolRelationshipGraphStorageInit(&cancelStorage, g_groups, 64, g_edges, 256,
                                       g_declarations, 256, g_definitions, 256,
                                       g_forwards, 256, g_symbolGroups, 256);
    SymbolRelationshipGraphInit(&cancelledCompleted, &cancelStorage, "relationship-test", 7,
                                database.symbolDatabaseGeneration);
    SymbolRelationshipGraphInit(&cancelledBuilding, &cancelStorage, "relationship-test", 7,
                                database.symbolDatabaseGeneration);
    SymbolRelationshipGraphService cancelledService = {};
    SymbolRelationshipGraphServiceInit(&cancelledService, &cancelledCompleted, &cancelledBuilding);
    uint64_t cancelledOperation = 0;
    assert(SymbolRelationshipGraphBuildStart(&cancelledService, &database, nullptr, "relationship-test", 7, "", 0, &cancelledOperation));
    assert(SymbolRelationshipGraphBuildIsActive(&cancelledService));
    assert(SymbolRelationshipGraphBuildCancel(&cancelledService, cancelledOperation));
    assert(SymbolRelationshipGraphBuildPoll(&cancelledService, cancelledOperation, 1, 0));
    assert(SymbolRelationshipGraphBuildInfo(&cancelledService)->state == RelationshipGraphState::Cancelled);
    return 0;
}
