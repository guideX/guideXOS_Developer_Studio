#pragma once

#include <stdint.h>

#include "developer_studio_include_graph.h"
#include "developer_studio_symbols.h"

namespace guidexos {
namespace developer_studio {

// Declaration/definition matching is deliberately lexical and bounded.  The
// larger limits describe the model contract; callers provide the actual
// storage, so the embedded application can choose a smaller working set.
static const uint32_t kRelationshipMaxNormalizedSignatureBytes = 2048u;
static const uint32_t kRelationshipMaxGroups = 100000u;
static const uint32_t kRelationshipMaxEndpointsPerGroup = 1000u;
static const uint32_t kRelationshipMaxEdges = 500000u;
static const uint32_t kRelationshipMaxPickerCandidates = 1000u;

enum class SymbolRelationshipKind {
    DeclarationToDefinition = 0,
    DefinitionToDeclaration,
    ForwardToDefinition,
    DefinitionToForward,
    DeclarationGroup,
    DefinitionGroup,
    PossibleRelationship
};

enum class SymbolRelationshipConfidence {
    Exact = 0,
    Strong,
    Possible,
    Ambiguous
};

enum class RelationshipGraphState {
    Idle = 0,
    CollectingSymbols,
    GroupingCandidates,
    MatchingRelationships,
    Completed,
    Cancelling,
    Cancelled,
    Failed
};

enum class RelationshipErrorCode {
    None = 0,
    NoProject,
    IndexNotReady,
    ProjectStale,
    SymbolStale,
    GraphStale,
    BuildCancelled,
    BuildTimeout,
    KeyLimit,
    EndpointLimit,
    EdgeLimit,
    SignatureTooLong,
    NormalizationApproximate,
    NoDefinition,
    NoDeclaration,
    MultipleDefinitions,
    MultipleDeclarations,
    Ambiguous,
    AlreadyAtDefinition,
    AlreadyAtDeclaration,
    PathInvalid,
    PathOutsideProject,
    FileMissing,
    LocationStale,
    ActivationFailed,
    Internal
};

enum SymbolRelationshipReasonFlag {
    RelationshipReasonExactQualifiedName = 1u << 0,
    RelationshipReasonExactSignature = 1u << 1,
    RelationshipReasonCompatibleRole = 1u << 2,
    RelationshipReasonSameScope = 1u << 3,
    RelationshipReasonExactKind = 1u << 4,
    RelationshipReasonMethodQualifiers = 1u << 5,
    RelationshipReasonDirectInclude = 1u << 6,
    RelationshipReasonHeaderSourceStem = 1u << 7,
    RelationshipReasonSameModule = 1u << 8,
    RelationshipReasonApproximateSignature = 1u << 9,
    RelationshipReasonClassKeyMismatch = 1u << 10,
    RelationshipReasonMultipleCandidates = 1u << 11,
    RelationshipReasonStaleEndpoint = 1u << 12
};

struct SymbolRelationshipKey {
    SymbolKind kind;
    char name[kSymbolMaxNameBytes];
    char qualifiedName[kSymbolMaxQualifiedNameBytes];
    char normalizedSignature[kRelationshipMaxNormalizedSignatureBytes + 1u];
    char containingScope[kSymbolMaxContainerBytes];
    bool isStatic;
    bool isConstQualified;
    bool isVolatileQualified;
    bool isLValueRefQualified;
    bool isRValueRefQualified;
    bool isNoexcept;
    uint32_t parameterCount;
    bool signatureComplete;
    bool lexicallyApproximate;
};

struct SymbolRelationshipEndpoint {
    uint64_t symbolId;
    uint64_t documentId;
    uint64_t documentGeneration;
    char relativePath[kMaxPathBytes];
    uint64_t byteOffset;
    uint32_t line;
    uint32_t column;
    uint32_t identifierLength;
    SymbolKind symbolKind;
    SymbolDeclarationRole declarationRole;
    char name[kSymbolMaxNameBytes];
    char qualifiedName[kSymbolMaxQualifiedNameBytes];
    char normalizedSignature[kRelationshipMaxNormalizedSignatureBytes + 1u];
};

struct SymbolRelationship {
    uint64_t relationshipId;
    SymbolRelationshipKind kind;
    SymbolRelationshipConfidence confidence;
    SymbolRelationshipEndpoint source;
    SymbolRelationshipEndpoint target;
    int32_t rankScore;
    uint32_t reasonFlags;
    bool stale;
};

// Endpoint indices are packed into caller-owned flat arrays.  This avoids a
// per-group fixed array while retaining the requested bounded endpoint cap.
struct SymbolRelationshipGroup {
    SymbolRelationshipKey key;
    uint32_t declarationOffset;
    uint32_t declarationCount;
    uint32_t definitionOffset;
    uint32_t definitionCount;
    uint32_t forwardOffset;
    uint32_t forwardCount;
    bool ambiguous;
    bool truncated;
};

struct SymbolRelationshipGraphStorage {
    SymbolRelationshipGroup* groups;
    uint32_t groupCapacity;
    SymbolRelationship* relationships;
    uint32_t relationshipCapacity;
    uint32_t* declarations;
    uint32_t declarationCapacity;
    uint32_t* definitions;
    uint32_t definitionCapacity;
    uint32_t* forwardDeclarations;
    uint32_t forwardCapacity;
    uint32_t* symbolGroupIndices;
    uint32_t symbolGroupCapacity;
};

struct SymbolRelationshipGraph {
    uint64_t graphId;
    uint64_t projectId;
    uint64_t projectGeneration;
    uint64_t symbolDatabaseGeneration;
    char projectIdText[kMaxProjectIdBytes];
    char projectRoot[kMaxPathBytes];
    SymbolRelationshipGroup* groups;
    uint32_t groupCount;
    uint32_t groupCapacity;
    SymbolRelationship* relationships;
    uint32_t relationshipCount;
    uint32_t relationshipCapacity;
    uint32_t* declarations;
    uint32_t declarationCount;
    uint32_t declarationCapacity;
    uint32_t* definitions;
    uint32_t definitionCount;
    uint32_t definitionCapacity;
    uint32_t* forwardDeclarations;
    uint32_t forwardCount;
    uint32_t forwardCapacity;
    const SymbolDatabase* database;
    uint32_t* symbolGroupIndices;
    uint32_t symbolGroupCapacity;
    bool complete;
    bool truncated;
};

struct SymbolRelationshipGraphBuildOperation {
    uint64_t operationId;
    RelationshipGraphState state;
    RelationshipErrorCode error;
    const SymbolDatabase* database;
    const IncludeGraph* includeGraph;
    SymbolRelationshipGraph* buildingGraph;
    uint32_t* symbolGroupIndices;
    uint32_t symbolGroupCapacity;
    uint32_t collectIndex;
    uint32_t packIndex;
    uint32_t matchGroup;
    uint32_t matchDeclaration;
    uint32_t matchDefinition;
    uint64_t projectGeneration;
    uint64_t symbolDatabaseGeneration;
    char projectId[kMaxProjectIdBytes];
    bool cancellationRequested;
    bool terminalReported;
};

struct SymbolRelationshipGraphService {
    SymbolRelationshipGraph* completedGraph;
    SymbolRelationshipGraph* buildingGraph;
    SymbolRelationshipGraphBuildOperation operation;
    uint64_t nextOperationId;
};

const char* SymbolRelationshipKindName(SymbolRelationshipKind kind);
const char* SymbolRelationshipConfidenceName(SymbolRelationshipConfidence confidence);
const char* RelationshipGraphStateName(RelationshipGraphState state);
const char* RelationshipErrorName(RelationshipErrorCode error);

uint64_t SymbolRelationshipSymbolId(const ProjectSymbol& symbol, const char* relativePath);
bool NormalizeRelationshipSignature(const char* signature, char* output, uint32_t outputCapacity,
                                    uint32_t* parameterCount, bool* signatureComplete,
                                    bool* lexicallyApproximate);
bool SymbolRelationshipGraphHasRelationship(const SymbolRelationshipGraph* graph,
                                            uint64_t firstSymbolId, uint64_t secondSymbolId);
bool BuildSymbolRelationshipKey(const ProjectSymbol& symbol, const char* relativePath,
                                SymbolRelationshipKey* output);

void SymbolRelationshipGraphStorageInit(SymbolRelationshipGraphStorage* storage,
                                        SymbolRelationshipGroup* groups, uint32_t groupCapacity,
                                        SymbolRelationship* relationships, uint32_t relationshipCapacity,
                                        uint32_t* declarations, uint32_t declarationCapacity,
                                        uint32_t* definitions, uint32_t definitionCapacity,
                                        uint32_t* forwardDeclarations, uint32_t forwardCapacity,
                                        uint32_t* symbolGroupIndices, uint32_t symbolGroupCapacity);
void SymbolRelationshipGraphInit(SymbolRelationshipGraph* graph,
                                 const SymbolRelationshipGraphStorage* storage,
                                 const char* projectId, uint64_t projectGeneration,
                                 uint64_t symbolDatabaseGeneration);
void SymbolRelationshipGraphClear(SymbolRelationshipGraph* graph);
bool SymbolRelationshipGraphIsCurrent(const SymbolRelationshipGraph* graph,
                                      const char* projectId, uint64_t projectGeneration,
                                      uint64_t symbolDatabaseGeneration);

void SymbolRelationshipGraphServiceInit(SymbolRelationshipGraphService* service,
                                        SymbolRelationshipGraph* completedGraph,
                                        SymbolRelationshipGraph* buildingGraph);
bool SymbolRelationshipGraphBuildStart(SymbolRelationshipGraphService* service,
                                       const SymbolDatabase* database,
                                       const IncludeGraph* includeGraph,
                                       const char* projectId, uint64_t projectGeneration,
                                       const char* projectRoot,
                                       uint64_t nowMs, uint64_t* operationId);
bool SymbolRelationshipGraphBuildPoll(SymbolRelationshipGraphService* service,
                                      uint64_t operationId, uint32_t workBudget,
                                      uint64_t nowMs);
bool SymbolRelationshipGraphBuildCancel(SymbolRelationshipGraphService* service,
                                        uint64_t operationId);
bool SymbolRelationshipGraphBuildIsActive(const SymbolRelationshipGraphService* service);
const SymbolRelationshipGraphBuildOperation* SymbolRelationshipGraphBuildInfo(
    const SymbolRelationshipGraphService* service);

uint32_t SymbolRelationshipGraphFindDefinitions(const SymbolRelationshipGraph* graph,
                                                uint64_t symbolId,
                                                SymbolRelationship* output,
                                                uint32_t capacity);
uint32_t SymbolRelationshipGraphFindDeclarations(const SymbolRelationshipGraph* graph,
                                                 uint64_t symbolId,
                                                 SymbolRelationship* output,
                                                 uint32_t capacity);
uint32_t SymbolRelationshipGraphFindForwards(const SymbolRelationshipGraph* graph,
                                             uint64_t symbolId,
                                             SymbolRelationship* output,
                                             uint32_t capacity);
const SymbolRelationshipGroup* SymbolRelationshipGraphGroupAt(const SymbolRelationshipGraph* graph,
                                                               uint32_t index);
const SymbolRelationship* SymbolRelationshipGraphRelationshipAt(const SymbolRelationshipGraph* graph,
                                                                 uint32_t index);

} // namespace developer_studio
} // namespace guidexos
