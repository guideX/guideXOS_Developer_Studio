#include "developer_studio_ownership.h"

#include <assert.h>
#include <string.h>

using namespace guidexos::developer_studio;

namespace {

struct Fixture {
    OwnershipFileRecord files[16];
    FileOwnershipEndpoint completedFiles[16];
    FileOwnershipEndpoint buildingFiles[16];
    FileCounterpartCandidate completedCandidates[32];
    FileCounterpartCandidate buildingCandidates[32];
    FileOwnershipGroup completedGroups[16];
    FileOwnershipGroup buildingGroups[16];
    OwnershipEvidence completedEvidence[1024];
    OwnershipEvidence buildingEvidence[1024];
    FileOwnershipEndpoint completedHeaders[32];
    FileOwnershipEndpoint buildingHeaders[32];
    FileOwnershipEndpoint completedSources[32];
    FileOwnershipEndpoint buildingSources[32];
    uint64_t completedCandidateIds[64];
    uint64_t buildingCandidateIds[64];
    OwnershipGraphStorage completedStorage;
    OwnershipGraphStorage buildingStorage;
    FileOwnershipGraph completedGraph;
    FileOwnershipGraph buildingGraph;
    OwnershipGraphService service;
    OwnershipBucketRef exactRefs[16];
    OwnershipBucketRef normalizedRefs[16];
    OwnershipBucketRef moduleRefs[16];
    OwnershipBucket exactBuckets[16];
    OwnershipBucket normalizedBuckets[16];
    OwnershipBucket moduleBuckets[16];
    OwnershipPairSlot pairSlots[64];
    uint32_t candidateCounts[16];
    OwnershipRelationshipIdentity relationshipIdentities[128];
    uint32_t includeQueue[32];
    bool includeVisited[32];
    OwnershipBuildScratch scratch;

    Fixture() {
        OwnershipGraphStorageInit(&completedStorage, completedFiles, 16, completedCandidates, 32,
                                  completedGroups, 16, completedEvidence, 1024,
                                  completedHeaders, 32, completedSources, 32,
                                  completedCandidateIds, 64);
        OwnershipGraphStorageInit(&buildingStorage, buildingFiles, 16, buildingCandidates, 32,
                                  buildingGroups, 16, buildingEvidence, 1024,
                                  buildingHeaders, 32, buildingSources, 32,
                                  buildingCandidateIds, 64);
        OwnershipGraphInit(&completedGraph, &completedStorage, "ownership-test", 7, 1, 0, 0);
        OwnershipGraphInit(&buildingGraph, &buildingStorage, "ownership-test", 7, 1, 0, 0);
        OwnershipGraphServiceInit(&service, &completedGraph, &buildingGraph, &buildingStorage);
        OwnershipBuildScratchInit(&scratch, exactRefs, 16, normalizedRefs, 16, moduleRefs, 16,
                                  exactBuckets, 16, normalizedBuckets, 16, moduleBuckets, 16,
                                  pairSlots, 64, candidateCounts, 16, relationshipIdentities, 128,
                                  includeQueue, 32, includeVisited, 32);
    }
};

static OwnershipFileRecord file(const char* path, uint64_t id) {
    OwnershipFileRecord result = {};
    result.fileId = id;
    strncpy(result.relativePath, path, sizeof(result.relativePath) - 1);
    return result;
}

static FileOwnershipGraph* build(Fixture& fixture, uint32_t count, const OwnershipProjectMetadataPair* metadata = nullptr,
                                 uint32_t metadataCount = 0) {
    OwnershipBuildRequest request = {};
    strncpy(request.projectIdText, "ownership-test", sizeof(request.projectIdText) - 1);
    strncpy(request.projectRoot, "/project", sizeof(request.projectRoot) - 1);
    request.projectId = OwnershipHashText(request.projectIdText);
    request.projectGeneration = 7;
    request.symbolGeneration = 1;
    request.files = fixture.files;
    request.fileCount = count;
    request.metadata = metadata;
    request.metadataCount = metadataCount;
    request.scratch = &fixture.scratch;
    uint64_t operationId = 0;
    assert(OwnershipGraphBuildStart(&fixture.service, request, 0, &operationId));
    while (OwnershipGraphBuildIsActive(&fixture.service)) assert(OwnershipGraphBuildPoll(&fixture.service, operationId, 64, 0));
    const OwnershipBuildOperation* operation = OwnershipGraphBuildInfo(&fixture.service);
    assert(operation && operation->state == OwnershipBuildState::Completed);
    assert(fixture.service.completedGraph);
    return fixture.service.completedGraph;
}

static const FileCounterpartCandidate* findCandidate(const FileOwnershipGraph* graph, const char* source, const char* header) {
    for (uint32_t i = 0; i < graph->candidateCount; ++i) {
        const FileCounterpartCandidate& candidate = graph->candidates[i];
        if (strcmp(candidate.source.relativePath, source) == 0 && strcmp(candidate.target.relativePath, header) == 0) return &candidate;
    }
    return nullptr;
}

static void testClassificationAndKeys() {
    const char* paths[] = { "a.c", "a.cc", "a.cpp", "a.cxx", "a.h", "a.hh", "a.hpp", "a.hxx", "a.INC", "a.InL" };
    for (uint32_t i = 0; i < sizeof(paths) / sizeof(paths[0]); ++i) {
        ProjectCodeFileKind kind = ProjectCodeFileKind::Unknown;
        assert(OwnershipClassifyPath(paths[i], &kind));
        assert(kind != ProjectCodeFileKind::Unknown);
    }
    ProjectCodeFileKind unknown = ProjectCodeFileKind::Header;
    assert(!OwnershipClassifyPath("notes.txt", &unknown));
    assert(unknown == ProjectCodeFileKind::Unknown);
    assert(!OwnershipClassifyPath("extensionless", &unknown));

    char exact[2049] = {};
    char normalized[2049] = {};
    char module[2049] = {};
    bool platform = false;
    char label[128] = {};
    ProjectCodeFileKind sourceKind = ProjectCodeFileKind::CppSource;
    assert(OwnershipBuildStemKeys("src/ui/renderer_hosted.cpp", sourceKind, exact, sizeof(exact),
                                  normalized, sizeof(normalized), module, sizeof(module),
                                  &platform, label, sizeof(label)));
    assert(strcmp(exact, "renderer_hosted") == 0);
    assert(strcmp(normalized, "renderer") == 0);
    assert(strcmp(module, "ui/renderer_hosted") == 0);
    assert(platform && strcmp(label, "hosted") == 0);

    assert(OwnershipBuildStemKeys("include/foo_bar.hpp", ProjectCodeFileKind::Header,
                                  exact, sizeof(exact), normalized, sizeof(normalized),
                                  module, sizeof(module), &platform, label, sizeof(label)));
    assert(strcmp(normalized, "foo_bar") == 0);
    assert(!OwnershipBuildStemKeys("include/.hpp", ProjectCodeFileKind::Header,
                                   exact, sizeof(exact), normalized, sizeof(normalized),
                                   module, sizeof(module), &platform, label, sizeof(label)));
}

static void testOwnershipForms() {
    Fixture fixture;
    fixture.files[0] = file("include/renderer.h", 1);
    fixture.files[1] = file("src/renderer.cpp", 2);
    fixture.files[2] = file("src/renderer_hosted.cpp", 3);
    fixture.files[3] = file("src/renderer_native.cpp", 4);
    fixture.files[4] = file("include/build/project.hpp", 5);
    fixture.files[5] = file("src/build/project.cc", 6);
    fixture.files[6] = file("src/main.cpp", 7);
    fixture.files[7] = file("notes.txt", 8);
    fixture.files[8] = file("include/header_only.hpp", 9);

    OwnershipProjectMetadataPair metadata = {};
    strncpy(metadata.headerPath, "include/renderer.h", sizeof(metadata.headerPath) - 1);
    strncpy(metadata.sourcePath, "src/renderer.cpp", sizeof(metadata.sourcePath) - 1);
    FileOwnershipGraph* graph = build(fixture, 9, &metadata, 1);
    assert(graph->complete);
    assert(graph->candidateCount >= 4);
    const FileCounterpartCandidate* exact = findCandidate(graph, "src/renderer.cpp", "include/renderer.h");
    assert(exact && exact->exactStem);
    assert(exact->confidence == CounterpartConfidence::Exact);
    assert(exact->evidenceCount > 0);
    const FileCounterpartCandidate* hosted = findCandidate(graph, "src/renderer_hosted.cpp", "include/renderer.h");
    assert(hosted && hosted->normalizedStem);

    uint32_t indices[8] = {};
    OwnershipResolution resolution = {};
    resolution.candidateIndices = indices;
    resolution.candidateCapacity = 8;
    assert(OwnershipResolveFile(graph, "include/renderer.h", &resolution));
    assert(resolution.kind == OwnershipResolutionKind::Multiple);
    assert(resolution.candidateCount >= 3);

    resolution = {};
    resolution.candidateIndices = indices;
    resolution.candidateCapacity = 8;
    assert(OwnershipResolveFile(graph, "src/main.cpp", &resolution));
    assert(resolution.kind == OwnershipResolutionKind::SourceOnly);
    assert(resolution.status == OwnershipStatusCode::OWNERSHIP_SOURCE_ONLY);
    bool sourceOnlyGroupFound = false;
    for (uint32_t i = 0; i < graph->groupCount; ++i) {
        const FileOwnershipGroup* group = OwnershipGraphGroupAt(graph, i);
        if (!group || group->sourceCount == 0) continue;
        if (strcmp(group->sources[0].relativePath, "src/main.cpp") == 0) {
            assert(group->role == FileOwnershipRole::SourceOnly);
            sourceOnlyGroupFound = true;
        }
    }
    assert(sourceOnlyGroupFound);
}

static void testTruncationAndSupersession() {
    Fixture fixture;
    for (uint32_t i = 0; i < 16; ++i) {
        char path[64] = {};
        if (i < 8) {
            strcpy(path, "include/shared.h");
        } else {
            strcpy(path, "src/shared.cpp");
        }
        fixture.files[i] = file(path, i + 1);
    }
    OwnershipBuildRequest request = {};
    strncpy(request.projectIdText, "ownership-test", sizeof(request.projectIdText) - 1);
    request.projectGeneration = 7;
    request.symbolGeneration = 1;
    request.files = fixture.files;
    request.fileCount = 16;
    request.scratch = &fixture.scratch;
    uint64_t first = 0;
    assert(OwnershipGraphBuildStart(&fixture.service, request, 0, &first));
    uint64_t second = 0;
    request.projectGeneration = 8;
    assert(OwnershipGraphBuildStart(&fixture.service, request, 0, &second));
    assert(second != 0 && second != first);
    while (OwnershipGraphBuildIsActive(&fixture.service)) assert(OwnershipGraphBuildPoll(&fixture.service, second, 128, 0));
    assert(OwnershipGraphBuildInfo(&fixture.service)->state == OwnershipBuildState::Completed);
    assert(fixture.service.completedGraph->projectGeneration == 8);
}

} // namespace

int main() {
    testClassificationAndKeys();
    testOwnershipForms();
    testTruncationAndSupersession();
    return 0;
}
