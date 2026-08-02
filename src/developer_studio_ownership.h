#pragma once

#include <stdint.h>

#include "developer_studio_include_graph.h"
#include "developer_studio_relationships.h"

namespace guidexos {
namespace developer_studio {

// The public limits describe the ownership contract.  Embedded callers may
// provide smaller caller-owned storage; a small Native ELF configuration is
// used by Developer Studio itself.
static const uint32_t kOwnershipMaxProjectCodeFiles = 100000u;
static const uint32_t kOwnershipMaxStemBuckets = 100000u;
static const uint32_t kOwnershipMaxFilesPerStemBucket = 1000u;
static const uint32_t kOwnershipMaxInitialCandidatesPerFile = 1000u;
static const uint32_t kOwnershipMaxRetainedCandidatesPerFile = 256u;
static const uint32_t kOwnershipMaxCandidatePairs = 500000u;
static const uint32_t kOwnershipMaxEvidencePerCandidate = 32u;
static const uint32_t kOwnershipMaxRelationshipEndpointsPerPair = 2000u;
static const uint32_t kOwnershipMaxSharedSymbolsPerPair = 2000u;
static const uint32_t kOwnershipMaxPickerCandidates = 1000u;
static const uint32_t kOwnershipMaxVisiblePickerCandidates = 100u;
static const uint32_t kOwnershipMaxPathBytes = 2048u;
static const uint32_t kOwnershipMaxEvidenceDetailBytes = 512u;
static const uint64_t kOwnershipMaxBuildDurationMs = 5ull * 60ull * 1000ull;
static const uint64_t kOwnershipMaxChordDurationMs = 2000ull;

enum class ProjectCodeFileKind {
    Unknown = 0,
    CSource,
    CppSource,
    Header,
    InlineHeader
};

enum class FileOwnershipRole {
    Unknown = 0,
    Header,
    Source,
    HeaderOnly,
    SourceOnly,
    Mixed
};

enum class CounterpartConfidence {
    None = 0,
    Exact,
    Strong,
    Possible,
    Ambiguous
};

enum class OwnershipEvidenceKind {
    ExplicitProjectMetadata = 0,
    ExactStemMatch,
    NormalizedStemMatch,
    RelativeDirectoryMatch,
    IncludeGraphDirect,
    IncludeGraphTransitive,
    DeclarationDefinitionRelationship,
    SymbolOverlap,
    SharedNamespace,
    SharedClass,
    PlatformVariant,
    GeneratedVariant,
    ConflictingEvidence
};

struct OwnershipEvidence {
    OwnershipEvidenceKind kind;
    int32_t score;
    char detail[kOwnershipMaxEvidenceDetailBytes + 1u];
    uint32_t relationshipCount;
    uint32_t includeEdgeCount;
    uint32_t sharedSymbolCount;
    bool positive;
    bool conflicting;
};

struct FileOwnershipEndpoint {
    uint64_t fileId;
    char relativePath[kOwnershipMaxPathBytes + 1u];
    ProjectCodeFileKind fileKind;
    uint64_t documentId;
    uint64_t documentGeneration;
    bool open;
    bool dirty;
    bool generated;
    bool platformVariant;
    char platformLabel[kMaxNameBytes];
};

// File inventory entries are intentionally independent of Documents.  A
// caller can describe an open dirty file without handing ownership an editor
// pointer or a filesystem handle.
using OwnershipFileRecord = FileOwnershipEndpoint;

struct OwnershipFileInventory {
    const OwnershipFileRecord* files;
    uint32_t fileCount;
    uint64_t projectGeneration;
    bool truncated;
};

struct OwnershipProjectMetadataPair {
    char headerPath[kOwnershipMaxPathBytes + 1u];
    char sourcePath[kOwnershipMaxPathBytes + 1u];
    bool generated;
    bool platformVariant;
};

struct FileCounterpartCandidate {
    uint64_t candidateId;
    FileOwnershipEndpoint source;
    FileOwnershipEndpoint target;
    CounterpartConfidence confidence;
    int32_t rankScore;
    OwnershipEvidence* evidence;
    uint32_t evidenceCount;
    uint32_t evidenceCapacity;
    uint32_t exactRelationshipCount;
    uint32_t strongRelationshipCount;
    uint32_t possibleRelationshipCount;
    uint32_t conflictingRelationshipCount;
    uint32_t includeEdgeCount;
    uint32_t sharedSymbolCount;
    bool exactStem;
    bool normalizedStem;
    bool relativeDirectory;
    bool directInclude;
    bool transitiveInclude;
    bool relationshipLinked;
    bool platformVariant;
    bool generatedVariant;
    bool ambiguous;
    bool stale;
};

// Flat caller-owned ranges keep the model usable in the freestanding Native
// ELF while exposing vector-like counts to hosted callers.
struct FileOwnershipGroup {
    uint64_t groupId;
    FileOwnershipEndpoint* headers;
    uint32_t headerCount;
    uint32_t headerCapacity;
    FileOwnershipEndpoint* sources;
    uint32_t sourceCount;
    uint32_t sourceCapacity;
    uint64_t* candidateIds;
    uint32_t candidateCount;
    uint32_t candidateCapacity;
    FileOwnershipRole role;
    CounterpartConfidence confidence;
    bool oneToOne;
    bool oneToMany;
    bool manyToOne;
    bool manyToMany;
    bool ambiguous;
    bool truncated;
};

struct FileOwnershipGraph {
    uint64_t graphId;
    uint64_t projectId;
    uint64_t projectGeneration;
    uint64_t symbolGeneration;
    uint64_t includeGraphGeneration;
    uint64_t relationshipGraphGeneration;
    char projectIdText[kMaxProjectIdBytes];
    FileOwnershipEndpoint* files;
    uint32_t fileCount;
    uint32_t fileCapacity;
    FileCounterpartCandidate* candidates;
    uint32_t candidateCount;
    uint32_t candidateCapacity;
    FileOwnershipGroup* groups;
    uint32_t groupCount;
    uint32_t groupCapacity;
    OwnershipEvidence* evidence;
    uint32_t evidenceCount;
    uint32_t evidenceCapacity;
    uint32_t evidencePoolUsed;
    FileOwnershipEndpoint* groupHeaders;
    uint32_t groupHeaderCapacity;
    uint32_t groupHeaderCount;
    FileOwnershipEndpoint* groupSources;
    uint32_t groupSourceCapacity;
    uint32_t groupSourceCount;
    uint64_t* groupCandidateIds;
    uint32_t groupCandidateIdCapacity;
    uint32_t groupCandidateIdCount;
    bool complete;
    bool stale;
    bool truncated;
};

enum class OwnershipBuildState {
    Idle = 0,
    CollectingFiles,
    GeneratingCandidates,
    CollectingEvidence,
    Ranking,
    Grouping,
    Completed,
    Cancelling,
    Cancelled,
    Failed
};

enum class OwnershipStatusCode {
    None = 0,
    OWNERSHIP_NO_PROJECT,
    OWNERSHIP_FILE_NOT_ELIGIBLE,
    OWNERSHIP_PROJECT_STALE,
    OWNERSHIP_SYMBOL_INDEX_STALE,
    OWNERSHIP_INCLUDE_GRAPH_STALE,
    OWNERSHIP_RELATIONSHIP_GRAPH_STALE,
    OWNERSHIP_GRAPH_STALE,
    OWNERSHIP_BUILD_CANCELLED,
    OWNERSHIP_BUILD_TIMEOUT,
    OWNERSHIP_FILE_LIMIT,
    OWNERSHIP_STEM_BUCKET_LIMIT,
    OWNERSHIP_CANDIDATE_LIMIT,
    OWNERSHIP_EVIDENCE_LIMIT,
    OWNERSHIP_PATH_TOO_LONG,
    OWNERSHIP_NO_COUNTERPART,
    OWNERSHIP_HEADER_ONLY,
    OWNERSHIP_SOURCE_ONLY,
    OWNERSHIP_MULTIPLE_COUNTERPARTS,
    OWNERSHIP_AMBIGUOUS,
    OWNERSHIP_RESULTS_TRUNCATED,
    OWNERSHIP_TARGET_MISSING,
    OWNERSHIP_TARGET_OUTSIDE_PROJECT,
    OWNERSHIP_ACTIVATION_STALE,
    OWNERSHIP_ACTIVATION_FAILED,
    OWNERSHIP_SHORTCUT_CONFLICT,
    OWNERSHIP_CHORD_CANCELLED,
    OWNERSHIP_INTERNAL
};

enum class OwnershipResolutionKind {
    None = 0,
    Direct,
    Multiple,
    HeaderOnly,
    SourceOnly,
    Stale,
    Failed
};

struct OwnershipResolution {
    OwnershipResolutionKind kind;
    uint32_t candidateCount;
    uint32_t visibleCandidateCount;
    uint32_t* candidateIndices;
    uint32_t candidateCapacity;
    bool truncated;
    bool updating;
    bool stale;
    OwnershipStatusCode status;
};

struct OwnershipBucketRef {
    uint64_t hash;
    uint32_t fileIndex;
};

struct OwnershipBucket {
    uint64_t hash;
    uint32_t first;
    uint32_t count;
};

struct OwnershipPairSlot {
    uint32_t sourceFileIndex;
    uint32_t targetFileIndex;
    uint32_t candidateIndex;
    bool used;
};

struct OwnershipRelationshipIdentity {
    uint64_t candidateId;
    uint64_t identity;
};

// The application supplies this scratch storage.  No ownership build reads
// source content; all evidence is derived from indexed records and graphs.
struct OwnershipBuildScratch {
    OwnershipBucketRef* exactRefs;
    uint32_t exactRefCapacity;
    OwnershipBucketRef* normalizedRefs;
    uint32_t normalizedRefCapacity;
    OwnershipBucketRef* moduleRefs;
    uint32_t moduleRefCapacity;
    OwnershipBucket* exactBuckets;
    uint32_t exactBucketCapacity;
    OwnershipBucket* normalizedBuckets;
    uint32_t normalizedBucketCapacity;
    OwnershipBucket* moduleBuckets;
    uint32_t moduleBucketCapacity;
    OwnershipPairSlot* pairSlots;
    uint32_t pairSlotCapacity;
    uint32_t* candidateCountsByFile;
    uint32_t candidateCountsByFileCapacity;
    OwnershipRelationshipIdentity* relationshipIdentities;
    uint32_t relationshipIdentityCapacity;
    uint32_t* includeQueue;
    uint32_t includeQueueCapacity;
    bool* includeVisited;
    uint32_t includeVisitedCapacity;
};

struct OwnershipGraphStorage {
    FileOwnershipEndpoint* files;
    uint32_t fileCapacity;
    FileCounterpartCandidate* candidates;
    uint32_t candidateCapacity;
    FileOwnershipGroup* groups;
    uint32_t groupCapacity;
    OwnershipEvidence* evidence;
    uint32_t evidenceCapacity;
    FileOwnershipEndpoint* groupHeaders;
    uint32_t groupHeaderCapacity;
    FileOwnershipEndpoint* groupSources;
    uint32_t groupSourceCapacity;
    uint64_t* groupCandidateIds;
    uint32_t groupCandidateIdCapacity;
};

struct OwnershipBuildRequest {
    char projectIdText[kMaxProjectIdBytes];
    char projectRoot[kOwnershipMaxPathBytes + 1u];
    uint64_t projectId;
    uint64_t projectGeneration;
    uint64_t symbolGeneration;
    const OwnershipFileRecord* files;
    uint32_t fileCount;
    bool inventoryTruncated;
    const IncludeGraph* includeGraph;
    const SymbolRelationshipGraph* relationshipGraph;
    const SymbolDatabase* symbolDatabase;
    const OwnershipProjectMetadataPair* metadata;
    uint32_t metadataCount;
    OwnershipBuildScratch* scratch;
};

struct OwnershipBuildOperation {
    uint64_t operationId;
    OwnershipBuildState state;
    OwnershipStatusCode error;
    FileOwnershipGraph* buildingGraph;
    OwnershipBuildRequest request;
    uint64_t startedAtMs;
    uint32_t fileIndex;
    uint32_t indexKind;
    uint32_t bucketIndex;
    uint32_t bucketLeft;
    uint32_t bucketRight;
    uint32_t includeIndex;
    uint32_t relationshipIndex;
    uint32_t metadataIndex;
    uint32_t evidenceIndex;
    uint32_t rankingIndex;
    uint32_t groupingIndex;
    uint32_t groupingCandidateIndex;
    uint32_t groupingHeaderIndex;
    uint32_t groupingSourceIndex;
    uint64_t filesCollected;
    uint64_t candidatePairsGenerated;
    uint64_t candidatePairsRetained;
    uint64_t pairComparisons;
    uint64_t evidenceRecords;
    bool cancellationRequested;
    bool terminalReported;
    bool pendingSupersession;
};

struct OwnershipGraphService {
    FileOwnershipGraph* completedGraph;
    FileOwnershipGraph* buildingGraph;
    OwnershipGraphStorage* buildingStorage;
    OwnershipBuildOperation operation;
    OwnershipBuildRequest pendingRequest;
    uint64_t pendingStartedAtMs;
    uint64_t pendingOperationId;
    uint64_t nextOperationId;
    bool hasPendingRequest;
};

const char* ProjectCodeFileKindName(ProjectCodeFileKind kind);
const char* FileOwnershipRoleName(FileOwnershipRole role);
const char* CounterpartConfidenceName(CounterpartConfidence confidence);
const char* OwnershipEvidenceKindName(OwnershipEvidenceKind kind);
const char* OwnershipBuildStateName(OwnershipBuildState state);
const char* OwnershipStatusName(OwnershipStatusCode code);
const char* OwnershipResolutionKindName(OwnershipResolutionKind kind);

uint64_t OwnershipHashText(const char* text);
bool OwnershipClassifyPath(const char* path, ProjectCodeFileKind* output);
bool OwnershipIsEligiblePath(const char* path);
bool OwnershipBuildStemKeys(const char* path, ProjectCodeFileKind kind,
                            char* exactStem, uint32_t exactCapacity,
                            char* normalizedStem, uint32_t normalizedCapacity,
                            char* modulePath, uint32_t moduleCapacity,
                            bool* platformVariant, char* platformLabel,
                            uint32_t platformLabelCapacity);
bool OwnershipGraphIsCurrent(const FileOwnershipGraph* graph, const char* projectId,
                             uint64_t projectGeneration, uint64_t symbolGeneration,
                             uint64_t includeGraphGeneration,
                             uint64_t relationshipGraphGeneration);

void OwnershipBuildRequestInit(OwnershipBuildRequest* request);
void OwnershipBuildScratchInit(OwnershipBuildScratch* scratch,
                               OwnershipBucketRef* exactRefs, uint32_t exactRefCapacity,
                               OwnershipBucketRef* normalizedRefs, uint32_t normalizedRefCapacity,
                               OwnershipBucketRef* moduleRefs, uint32_t moduleRefCapacity,
                               OwnershipBucket* exactBuckets, uint32_t exactBucketCapacity,
                               OwnershipBucket* normalizedBuckets, uint32_t normalizedBucketCapacity,
                               OwnershipBucket* moduleBuckets, uint32_t moduleBucketCapacity,
                               OwnershipPairSlot* pairSlots, uint32_t pairSlotCapacity,
                               uint32_t* candidateCountsByFile, uint32_t candidateCountsByFileCapacity,
                               OwnershipRelationshipIdentity* relationshipIdentities,
                               uint32_t relationshipIdentityCapacity,
                               uint32_t* includeQueue, uint32_t includeQueueCapacity,
                               bool* includeVisited, uint32_t includeVisitedCapacity);
void OwnershipGraphStorageInit(OwnershipGraphStorage* storage,
                               FileOwnershipEndpoint* files, uint32_t fileCapacity,
                               FileCounterpartCandidate* candidates, uint32_t candidateCapacity,
                               FileOwnershipGroup* groups, uint32_t groupCapacity,
                               OwnershipEvidence* evidence, uint32_t evidenceCapacity,
                               FileOwnershipEndpoint* groupHeaders, uint32_t groupHeaderCapacity,
                               FileOwnershipEndpoint* groupSources, uint32_t groupSourceCapacity,
                               uint64_t* groupCandidateIds, uint32_t groupCandidateIdCapacity);
void OwnershipGraphInit(FileOwnershipGraph* graph, const OwnershipGraphStorage* storage,
                        const char* projectId, uint64_t projectGeneration,
                        uint64_t symbolGeneration, uint64_t includeGraphGeneration,
                        uint64_t relationshipGraphGeneration);
void OwnershipGraphServiceInit(OwnershipGraphService* service,
                               FileOwnershipGraph* completedGraph,
                               FileOwnershipGraph* buildingGraph,
                               OwnershipGraphStorage* buildingStorage);

bool OwnershipGraphBuildStart(OwnershipGraphService* service,
                              const OwnershipBuildRequest& request, uint64_t nowMs,
                              uint64_t* outOperationId);
bool OwnershipGraphBuildPoll(OwnershipGraphService* service, uint64_t operationId,
                             uint32_t workBudget, uint64_t nowMs);
bool OwnershipGraphBuildCancel(OwnershipGraphService* service, uint64_t operationId);
bool OwnershipGraphBuildIsActive(const OwnershipGraphService* service);
const OwnershipBuildOperation* OwnershipGraphBuildInfo(const OwnershipGraphService* service);

const FileCounterpartCandidate* OwnershipGraphCandidateAt(const FileOwnershipGraph* graph,
                                                          uint32_t index);
const FileOwnershipGroup* OwnershipGraphGroupAt(const FileOwnershipGraph* graph,
                                                uint32_t index);
uint32_t OwnershipGraphCandidatesForFile(const FileOwnershipGraph* graph, const char* relativePath,
                                         uint32_t* indices, uint32_t capacity);
bool OwnershipResolveFile(const FileOwnershipGraph* graph, const char* relativePath,
                          OwnershipResolution* output);

} // namespace developer_studio
} // namespace guidexos
