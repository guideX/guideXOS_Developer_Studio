#include "developer_studio_ownership.h"

namespace guidexos {
namespace developer_studio {

namespace {

static const uint32_t kInvalidIndex = 0xffffffffu;

static uint32_t textLength(const char* text, uint32_t limit) {
    if (!text) return 0;
    uint32_t length = 0;
    while (length < limit && text[length] != '\0') ++length;
    return length;
}

static void copyText(char* output, uint32_t capacity, const char* input) {
    if (!output || capacity == 0) return;
    uint32_t i = 0;
    if (input) while (i + 1 < capacity && input[i] != '\0') { output[i] = input[i]; ++i; }
    output[i] = '\0';
}

static char lowerAscii(char value) {
    return value >= 'A' && value <= 'Z' ? static_cast<char>(value + ('a' - 'A')) : value;
}

static bool textEqual(const char* left, const char* right) {
    if (!left || !right) return left == right;
    uint32_t i = 0;
    while (left[i] != '\0' && right[i] != '\0') {
        if (left[i] != right[i]) return false;
        ++i;
    }
    return left[i] == right[i];
}

static int textCompare(const char* left, const char* right) {
    if (!left || !right) return left == right ? 0 : (left ? 1 : -1);
    uint32_t i = 0;
    while (left[i] != '\0' && right[i] != '\0') {
        if (left[i] != right[i]) return left[i] < right[i] ? -1 : 1;
        ++i;
    }
    if (left[i] == right[i]) return 0;
    return left[i] == '\0' ? -1 : 1;
}

static bool pathEqual(const char* left, const char* right) {
    if (!left || !right) return left == right;
    uint32_t i = 0;
    while (left[i] != '\0' && right[i] != '\0') {
        char a = left[i] == '\\' ? '/' : left[i];
        char b = right[i] == '\\' ? '/' : right[i];
        if (lowerAscii(a) != lowerAscii(b)) return false;
        ++i;
    }
    return left[i] == right[i];
}

static bool hasText(const char* text, const char* needle) {
    if (!text || !needle || needle[0] == '\0') return false;
    const uint32_t length = textLength(text, kOwnershipMaxEvidenceDetailBytes + 1u);
    const uint32_t needleLength = textLength(needle, kOwnershipMaxEvidenceDetailBytes + 1u);
    if (needleLength == 0 || needleLength > length) return false;
    for (uint32_t i = 0; i + needleLength <= length; ++i) {
        uint32_t j = 0;
        while (j < needleLength && text[i + j] == needle[j]) ++j;
        if (j == needleLength) return true;
    }
    return false;
}

static bool sourceKind(ProjectCodeFileKind kind) {
    return kind == ProjectCodeFileKind::CSource || kind == ProjectCodeFileKind::CppSource;
}

static bool headerKind(ProjectCodeFileKind kind) {
    return kind == ProjectCodeFileKind::Header || kind == ProjectCodeFileKind::InlineHeader;
}

static bool compatibleSourceKinds(ProjectCodeFileKind left, ProjectCodeFileKind right) {
    if (!sourceKind(left) || !sourceKind(right)) return true;
    return left == right;
}

static bool extensionEqual(const char* extension, const char* expected) {
    if (!extension || !expected) return false;
    uint32_t i = 0;
    while (extension[i] != '\0' && expected[i] != '\0') {
        if (lowerAscii(extension[i]) != lowerAscii(expected[i])) return false;
        ++i;
    }
    return extension[i] == expected[i];
}

static const char* lastSlash(const char* path) {
    const char* result = path;
    if (!path) return nullptr;
    for (const char* p = path; *p != '\0'; ++p)
        if (*p == '/' || *p == '\\') result = p + 1;
    return result;
}

static const char* lastDot(const char* path, const char* base) {
    const char* result = nullptr;
    if (!path || !base) return nullptr;
    for (const char* p = base; *p != '\0'; ++p) if (*p == '.') result = p;
    return result;
}

static bool suffixEqual(const char* value, uint32_t valueLength, const char* suffix) {
    const uint32_t suffixLength = textLength(suffix, 64u);
    if (suffixLength == 0 || suffixLength > valueLength) return false;
    const uint32_t offset = valueLength - suffixLength;
    for (uint32_t i = 0; i < suffixLength; ++i)
        if (lowerAscii(value[offset + i]) != lowerAscii(suffix[i])) return false;
    return true;
}

static const char* const kNormalizedSuffixes[] = {
    "_windows", "_guidexos", "_private", "_internal", "_native", "_hosted",
    "_riscv", "_arm64", "_amd64", "_linux", "_posix", "_unix", "_impl",
    "_x86", "_arm", "_win"
};

static bool platformSuffix(const char* suffix, uint32_t length, const char** label) {
    static const char* const suffixes[] = {
        "_win", "_windows", "_linux", "_posix", "_unix", "_guidexos",
        "_hosted", "_native", "_amd64", "_x86", "_arm", "_arm64", "_riscv"
    };
    for (uint32_t i = 0; i < sizeof(suffixes) / sizeof(suffixes[0]); ++i) {
        if (!suffixEqual(suffix, length, suffixes[i])) continue;
        if (label) *label = suffixes[i] + 1;
        return true;
    }
    return false;
}

static bool roleDirectory(const char* value, bool* headerRole) {
    if (headerRole) *headerRole = false;
    if (!value) return false;
    struct RoleName { const char* name; bool header; };
    static const RoleName roles[] = {
        { "include", true }, { "includes", true }, { "inc", true },
        { "header", true }, { "headers", true },
        { "src", false }, { "source", false }, { "sources", false },
        { "lib", false }, { "core", false }
    };
    for (uint32_t i = 0; i < sizeof(roles) / sizeof(roles[0]); ++i) {
        if (textEqual(value, roles[i].name)) {
            if (headerRole) *headerRole = roles[i].header;
            return true;
        }
    }
    return false;
}

static uint64_t hashBytes(const char* text) {
    uint64_t hash = 1469598103934665603ull;
    if (!text) return hash;
    for (uint32_t i = 0; text[i] != '\0'; ++i) {
        hash ^= static_cast<uint8_t>(text[i]);
        hash *= 1099511628211ull;
    }
    return hash == 0 ? 1 : hash;
}

static uint64_t pairHash(uint32_t source, uint32_t target) {
    uint64_t value = (static_cast<uint64_t>(source) << 32) ^ target;
    value ^= value >> 29;
    value *= 0x9e3779b97f4a7c15ull;
    return value ^ (value >> 31);
}

static void clearPairSlots(OwnershipBuildScratch* scratch) {
    if (!scratch || !scratch->pairSlots) return;
    for (uint32_t i = 0; i < scratch->pairSlotCapacity; ++i) scratch->pairSlots[i] = {};
}

static void quickSortRefs(OwnershipBucketRef* refs, int32_t left, int32_t right) {
    if (!refs || left >= right) return;
    int32_t i = left;
    int32_t j = right;
    const OwnershipBucketRef pivot = refs[left + (right - left) / 2];
    while (i <= j) {
        while (refs[i].hash < pivot.hash || (refs[i].hash == pivot.hash && refs[i].fileIndex < pivot.fileIndex)) ++i;
        while (refs[j].hash > pivot.hash || (refs[j].hash == pivot.hash && refs[j].fileIndex > pivot.fileIndex)) --j;
        if (i <= j) {
            OwnershipBucketRef value = refs[i]; refs[i] = refs[j]; refs[j] = value;
            ++i; --j;
        }
    }
    if (left < j) quickSortRefs(refs, left, j);
    if (i < right) quickSortRefs(refs, i, right);
}

static void quickSortCandidates(FileCounterpartCandidate* candidates, int32_t left, int32_t right) {
    if (!candidates || left >= right) return;
    int32_t i = left;
    int32_t j = right;
    const FileCounterpartCandidate pivot = candidates[left + (right - left) / 2];
    const auto before = [](const FileCounterpartCandidate& a, const FileCounterpartCandidate& b) {
        if (a.rankScore != b.rankScore) return a.rankScore > b.rankScore;
        if (a.confidence != b.confidence) return static_cast<int>(a.confidence) < static_cast<int>(b.confidence);
        if (a.exactStem != b.exactStem) return a.exactStem;
        if (a.exactRelationshipCount != b.exactRelationshipCount)
            return a.exactRelationshipCount > b.exactRelationshipCount;
        if (a.directInclude != b.directInclude) return a.directInclude;
        int path = textCompare(a.source.relativePath, b.source.relativePath);
        if (path != 0) return path < 0;
        path = textCompare(a.target.relativePath, b.target.relativePath);
        if (path != 0) return path < 0;
        if (a.source.fileKind != b.source.fileKind)
            return static_cast<int>(a.source.fileKind) < static_cast<int>(b.source.fileKind);
        return a.source.fileId < b.source.fileId;
    };
    while (i <= j) {
        while (before(candidates[i], pivot)) ++i;
        while (before(pivot, candidates[j])) --j;
        if (i <= j) {
            FileCounterpartCandidate value = candidates[i]; candidates[i] = candidates[j]; candidates[j] = value;
            ++i; --j;
        }
    }
    if (left < j) quickSortCandidates(candidates, left, j);
    if (i < right) quickSortCandidates(candidates, i, right);
}

static uint32_t confidenceOrder(CounterpartConfidence value) {
    switch (value) {
    case CounterpartConfidence::Exact: return 4;
    case CounterpartConfidence::Strong: return 3;
    case CounterpartConfidence::Possible: return 2;
    case CounterpartConfidence::Ambiguous: return 1;
    default: return 0;
    }
}

static uint32_t findFile(const FileOwnershipGraph* graph, const char* path) {
    if (!graph || !path || path[0] == '\0') return kInvalidIndex;
    for (uint32_t i = 0; i < graph->fileCount; ++i)
        if (pathEqual(graph->files[i].relativePath, path)) return i;
    return kInvalidIndex;
}

static bool isCommonUnqualifiedName(const char* name) {
    static const char* const common[] = {
        "Init", "Create", "Destroy", "Result", "Config", "Open", "Close", "Reset"
    };
    for (uint32_t i = 0; i < sizeof(common) / sizeof(common[0]); ++i)
        if (textEqual(name, common[i])) return true;
    return false;
}

static bool filePathMatchesSymbol(const OwnershipBuildRequest& request,
                                  const char* symbolPath, const char* relativePath) {
    if (!symbolPath || !relativePath) return false;
    if (pathEqual(symbolPath, relativePath)) return true;
    const uint32_t rootLength = textLength(request.projectRoot, kOwnershipMaxPathBytes + 1u);
    const uint32_t relativeLength = textLength(relativePath, kOwnershipMaxPathBytes + 1u);
    if (rootLength == 0 || rootLength + 1u + relativeLength >= kOwnershipMaxPathBytes + 1u) return false;
    char absolute[kOwnershipMaxPathBytes + 1u] = {};
    copyText(absolute, sizeof(absolute), request.projectRoot);
    uint32_t offset = textLength(absolute, sizeof(absolute));
    if (offset > 0 && absolute[offset - 1] != '/' && absolute[offset - 1] != '\\') absolute[offset++] = '/';
    for (uint32_t i = 0; i < relativeLength && offset + 1 < sizeof(absolute); ++i)
        absolute[offset++] = relativePath[i] == '\\' ? '/' : relativePath[i];
    absolute[offset] = '\0';
    return pathEqual(absolute, symbolPath);
}

static bool endpointHasEvidence(const FileCounterpartCandidate& candidate,
                                OwnershipEvidenceKind kind) {
    for (uint32_t i = 0; i < candidate.evidenceCount; ++i)
        if (candidate.evidence[i].kind == kind) return true;
    return false;
}

static void addEvidence(FileOwnershipGraph* graph, FileCounterpartCandidate* candidate,
                        OwnershipEvidenceKind kind, int32_t score, const char* detail,
                        uint32_t relationshipCount, uint32_t includeEdgeCount,
                        uint32_t sharedSymbolCount, bool positive, bool conflicting) {
    if (!graph || !candidate) return;
    OwnershipEvidence* existing = nullptr;
    for (uint32_t i = 0; i < candidate->evidenceCount; ++i) {
        OwnershipEvidence& value = candidate->evidence[i];
        if (value.kind == kind && value.conflicting == conflicting) { existing = &value; break; }
    }
    if (existing) {
        if (score > existing->score) existing->score = score;
        existing->relationshipCount += relationshipCount;
        existing->includeEdgeCount += includeEdgeCount;
        existing->sharedSymbolCount += sharedSymbolCount;
        if (detail && existing->detail[0] == '\0') copyText(existing->detail, sizeof(existing->detail), detail);
        return;
    }
    if (candidate->evidenceCount >= candidate->evidenceCapacity) {
        graph->truncated = true;
        return;
    }
    OwnershipEvidence* value = &candidate->evidence[candidate->evidenceCount];
    *value = {};
    value->kind = kind;
    value->score = score;
    copyText(value->detail, sizeof(value->detail), detail);
    value->relationshipCount = relationshipCount;
    value->includeEdgeCount = includeEdgeCount;
    value->sharedSymbolCount = sharedSymbolCount;
    value->positive = positive;
    value->conflicting = conflicting;
    ++candidate->evidenceCount;
    ++graph->evidenceCount;
    ++graph->evidencePoolUsed;
}

static FileCounterpartCandidate* findOrAddCandidate(FileOwnershipGraph* graph,
                                                     OwnershipBuildScratch* scratch,
                                                     uint32_t sourceIndex, uint32_t targetIndex,
                                                     uint64_t* generated) {
    if (!graph || !scratch || sourceIndex >= graph->fileCount || targetIndex >= graph->fileCount) return nullptr;
    if (!scratch->pairSlots || scratch->pairSlotCapacity == 0) {
        graph->truncated = true;
        return nullptr;
    }
    const uint64_t hash = pairHash(sourceIndex, targetIndex);
    const uint32_t start = static_cast<uint32_t>(hash % scratch->pairSlotCapacity);
    for (uint32_t probe = 0; probe < scratch->pairSlotCapacity; ++probe) {
        OwnershipPairSlot& slot = scratch->pairSlots[(start + probe) % scratch->pairSlotCapacity];
        if (!slot.used) {
            if (graph->candidateCount >= graph->candidateCapacity ||
                graph->candidateCount >= kOwnershipMaxCandidatePairs) {
                graph->truncated = true;
                return nullptr;
            }
            if (sourceIndex >= scratch->candidateCountsByFileCapacity ||
                targetIndex >= scratch->candidateCountsByFileCapacity ||
                scratch->candidateCountsByFile[sourceIndex] >= kOwnershipMaxRetainedCandidatesPerFile ||
                scratch->candidateCountsByFile[targetIndex] >= kOwnershipMaxRetainedCandidatesPerFile) {
                graph->truncated = true;
                return nullptr;
            }
            slot.used = true;
            slot.sourceFileIndex = sourceIndex;
            slot.targetFileIndex = targetIndex;
            slot.candidateIndex = graph->candidateCount;
            FileCounterpartCandidate& candidate = graph->candidates[graph->candidateCount++];
            candidate = {};
            candidate.candidateId = graph->candidateCount;
            candidate.source = graph->files[sourceIndex];
            candidate.target = graph->files[targetIndex];
            const uint32_t reserve = graph->evidencePoolUsed < graph->evidenceCapacity ?
                (graph->evidenceCapacity - graph->evidencePoolUsed < kOwnershipMaxEvidencePerCandidate ?
                 graph->evidenceCapacity - graph->evidencePoolUsed : kOwnershipMaxEvidencePerCandidate) : 0;
            candidate.evidence = graph->evidence + graph->evidencePoolUsed;
            candidate.evidenceCapacity = reserve;
            graph->evidencePoolUsed += reserve;
            ++scratch->candidateCountsByFile[sourceIndex];
            ++scratch->candidateCountsByFile[targetIndex];
            if (candidate.source.platformVariant || candidate.target.platformVariant) {
                candidate.platformVariant = true;
                addEvidence(graph, &candidate, OwnershipEvidenceKind::PlatformVariant, 0,
                            "Platform variant", 0, 0, 0, true, false);
            }
            if (candidate.source.generated || candidate.target.generated) {
                candidate.generatedVariant = true;
                addEvidence(graph, &candidate, OwnershipEvidenceKind::GeneratedVariant, 0,
                            "Generated variant", 0, 0, 0, true, false);
            }
            if (generated) ++*generated;
            return &candidate;
        }
        if (slot.sourceFileIndex == sourceIndex && slot.targetFileIndex == targetIndex)
            return &graph->candidates[slot.candidateIndex];
    }
    graph->truncated = true;
    return nullptr;
}

static void addNameEvidence(FileOwnershipGraph* graph, OwnershipBuildScratch* scratch,
                            uint32_t first, uint32_t second, OwnershipEvidenceKind kind,
                            int32_t score, const char* detail, bool exact, bool normalized,
                            bool relativeDirectory) {
    const FileOwnershipEndpoint& a = graph->files[first];
    const FileOwnershipEndpoint& b = graph->files[second];
    uint32_t source = sourceKind(a.fileKind) ? first : second;
    uint32_t target = source == first ? second : first;
    if (!sourceKind(graph->files[source].fileKind) || !headerKind(graph->files[target].fileKind)) return;
    FileCounterpartCandidate* candidate = findOrAddCandidate(graph, scratch, source, target, nullptr);
    if (!candidate) return;
    addEvidence(graph, candidate, kind, score, detail, 0, 0, 0, true, false);
    candidate->exactStem = candidate->exactStem || exact;
    candidate->normalizedStem = candidate->normalizedStem || normalized;
    candidate->relativeDirectory = candidate->relativeDirectory || relativeDirectory;
    if (!compatibleSourceKinds(a.fileKind, b.fileKind))
        addEvidence(graph, candidate, OwnershipEvidenceKind::ConflictingEvidence, -800,
                    "C/C++ family differs", 0, 0, 0, false, true);
}

static bool buildKeyForFile(const FileOwnershipEndpoint& file, uint32_t kind, char* output, uint32_t capacity) {
    char exact[kOwnershipMaxPathBytes + 1u] = {};
    char normalized[kOwnershipMaxPathBytes + 1u] = {};
    char module[kOwnershipMaxPathBytes + 1u] = {};
    bool platform = false;
    char label[kMaxNameBytes] = {};
    if (!OwnershipBuildStemKeys(file.relativePath, file.fileKind, exact, sizeof(exact), normalized,
                                sizeof(normalized), module, sizeof(module), &platform, label, sizeof(label))) return false;
    copyText(output, capacity, kind == 0 ? exact : (kind == 1 ? normalized : module));
    return output[0] != '\0';
}

static bool sameBucketKey(const FileOwnershipEndpoint& left, const FileOwnershipEndpoint& right, uint32_t kind) {
    char a[kOwnershipMaxPathBytes + 1u] = {};
    char b[kOwnershipMaxPathBytes + 1u] = {};
    if (!buildKeyForFile(left, kind, a, sizeof(a)) || !buildKeyForFile(right, kind, b, sizeof(b))) return false;
    return kind == 0 ? textEqual(a, b) : pathEqual(a, b);
}

static void buildBuckets(OwnershipBucketRef* refs, uint32_t refCount,
                         OwnershipBucket* buckets, uint32_t bucketCapacity,
                         bool* truncated) {
    if (!refs || !buckets || bucketCapacity == 0) { if (truncated) *truncated = true; return; }
    if (refCount > 0) quickSortRefs(refs, 0, static_cast<int32_t>(refCount - 1));
    uint32_t bucketCount = 0;
    uint32_t index = 0;
    while (index < refCount) {
        const uint64_t hash = refs[index].hash;
        uint32_t end = index + 1;
        while (end < refCount && refs[end].hash == hash) ++end;
        if (bucketCount >= bucketCapacity) { if (truncated) *truncated = true; break; }
        buckets[bucketCount].hash = hash;
        buckets[bucketCount].first = index;
        buckets[bucketCount].count = end - index;
        ++bucketCount;
        index = end;
    }
}

static uint32_t bucketCount(const OwnershipBucket* buckets, uint32_t capacity) {
    if (!buckets) return 0;
    uint32_t count = 0;
    while (count < capacity && buckets[count].count != 0) ++count;
    return count;
}

static int32_t endpointNode(const IncludeGraph* graph, const char* path) {
    if (!graph || !path) return -1;
    for (uint32_t i = 0; i < graph->nodeCount; ++i)
        if (pathEqual(graph->nodes[i].relativePath, path)) return static_cast<int32_t>(i);
    return -1;
}

static void addIncludeEvidence(FileOwnershipGraph* graph, OwnershipBuildScratch* scratch,
                               const IncludeEdge& edge, bool transitive) {
    if (!graph || !scratch) return;
    uint32_t sourceIndex = findFile(graph, edge.sourceRelativePath);
    uint32_t targetIndex = findFile(graph, edge.targetRelativePath);
    if (sourceIndex == kInvalidIndex || targetIndex == kInvalidIndex) return;
    const bool sourceIsSource = sourceKind(graph->files[sourceIndex].fileKind);
    const bool targetIsHeader = headerKind(graph->files[targetIndex].fileKind);
    const bool reverse = headerKind(graph->files[sourceIndex].fileKind) && sourceKind(graph->files[targetIndex].fileKind);
    if (!sourceIsSource && !reverse) return;
    const bool active = edge.directive.directiveState != IncludeDirectiveState::InactiveIfZero;
    if (reverse) {
        uint32_t source = targetIndex;
        uint32_t target = sourceIndex;
        FileCounterpartCandidate* candidate = findOrAddCandidate(graph, scratch, source, target, nullptr);
        if (candidate) {
            candidate->stale = candidate->stale || edge.stale;
            addEvidence(graph, candidate, OwnershipEvidenceKind::ConflictingEvidence, -1000,
                        "Header includes source", 0, 1, 0, false, true);
        }
        return;
    }
    if (!targetIsHeader) return;
    FileCounterpartCandidate* candidate = findOrAddCandidate(graph, scratch, sourceIndex, targetIndex, nullptr);
    if (!candidate) return;
    candidate->stale = candidate->stale || edge.stale;
    const OwnershipEvidenceKind kind = transitive ? OwnershipEvidenceKind::IncludeGraphTransitive : OwnershipEvidenceKind::IncludeGraphDirect;
    const int32_t score = active ? (transitive ? 700 : 1700) : 0;
    addEvidence(graph, candidate, kind, score, transitive ? "Source transitively includes header" : "Source directly includes header",
                0, 1, 0, active, false);
    if (transitive) candidate->transitiveInclude = true;
    else candidate->directInclude = true;
}

static void addTransitiveIncludes(FileOwnershipGraph* graph, const IncludeGraph* includeGraph,
                                  OwnershipBuildScratch* scratch, const char* sourcePath) {
    if (!graph || !includeGraph || !scratch || !sourcePath || !scratch->includeQueue ||
        !scratch->includeVisited || scratch->includeQueueCapacity == 0 || scratch->includeVisitedCapacity < includeGraph->nodeCount) return;
    const int32_t start = endpointNode(includeGraph, sourcePath);
    if (start < 0) return;
    for (uint32_t i = 0; i < includeGraph->nodeCount && i < scratch->includeVisitedCapacity; ++i) scratch->includeVisited[i] = false;
    uint32_t head = 0;
    uint32_t tail = 0;
    scratch->includeQueue[tail++] = static_cast<uint32_t>(start);
    scratch->includeVisited[start] = true;
    uint32_t depth = 0;
    while (head < tail && depth < kIncludeGraphMaxTraversalDepth) {
        const uint32_t levelEnd = tail;
        while (head < levelEnd) {
            const uint32_t nodeIndex = scratch->includeQueue[head++];
            const IncludeNode* node = IncludeGraphNodeAt(includeGraph, nodeIndex);
            if (!node) continue;
            for (uint32_t edgeOffset = 0; edgeOffset < node->outgoingEdgeCount; ++edgeOffset) {
                const uint32_t edgeIndex = includeGraph->outgoingEdgeIndices[node->outgoingEdgeOffset + edgeOffset];
                const IncludeEdge* edge = IncludeGraphEdgeAt(includeGraph, edgeIndex);
                if (!edge || edge->resolution.state != IncludeResolutionState::Resolved) continue;
                const int32_t target = endpointNode(includeGraph, edge->targetRelativePath);
                if (target < 0 || static_cast<uint32_t>(target) >= scratch->includeVisitedCapacity) continue;
                if (depth >= 1) addIncludeEvidence(graph, scratch, *edge, true);
                if (!scratch->includeVisited[target] && tail < scratch->includeQueueCapacity) {
                    scratch->includeVisited[target] = true;
                    scratch->includeQueue[tail++] = static_cast<uint32_t>(target);
                } else if (!scratch->includeVisited[target]) graph->truncated = true;
            }
        }
        ++depth;
    }
}

static void addRelationshipEvidence(FileOwnershipGraph* graph, OwnershipBuildScratch* scratch,
                                    const SymbolRelationship& relationship) {
    if (!graph || !scratch) return;
    uint32_t first = findFile(graph, relationship.source.relativePath);
    uint32_t second = findFile(graph, relationship.target.relativePath);
    if (first == kInvalidIndex || second == kInvalidIndex || first == second) return;
    uint32_t source = kInvalidIndex;
    uint32_t target = kInvalidIndex;
    if (sourceKind(graph->files[first].fileKind) && headerKind(graph->files[second].fileKind)) { source = first; target = second; }
    else if (sourceKind(graph->files[second].fileKind) && headerKind(graph->files[first].fileKind)) { source = second; target = first; }
    if (source == kInvalidIndex) return;
    FileCounterpartCandidate* candidate = findOrAddCandidate(graph, scratch, source, target, nullptr);
    if (!candidate) return;
    candidate->relationshipLinked = true;
    candidate->stale = candidate->stale || relationship.stale;
    const bool exact = relationship.confidence == SymbolRelationshipConfidence::Exact;
    const bool strong = relationship.confidence == SymbolRelationshipConfidence::Strong;
    const bool possible = relationship.confidence == SymbolRelationshipConfidence::Possible;
    uint64_t identity = hashBytes(relationship.source.qualifiedName);
    identity ^= hashBytes(relationship.source.normalizedSignature);
    identity ^= hashBytes(relationship.target.qualifiedName) << 1;
    identity ^= hashBytes(relationship.target.normalizedSignature) << 2;
    bool duplicate = false;
    if (scratch->relationshipIdentities) {
        for (uint32_t i = 0; i < scratch->relationshipIdentityCapacity; ++i) {
            const OwnershipRelationshipIdentity& value = scratch->relationshipIdentities[i];
            if (value.candidateId == 0) break;
            if (value.candidateId == candidate->candidateId && value.identity == identity) { duplicate = true; break; }
        }
        if (!duplicate) {
            for (uint32_t i = 0; i < scratch->relationshipIdentityCapacity; ++i) {
                if (scratch->relationshipIdentities[i].candidateId != 0) continue;
                scratch->relationshipIdentities[i].candidateId = candidate->candidateId;
                scratch->relationshipIdentities[i].identity = identity;
                break;
            }
        }
    }
    if (duplicate) return;
    if (exact) ++candidate->exactRelationshipCount;
    else if (strong) ++candidate->strongRelationshipCount;
    else if (possible) ++candidate->possibleRelationshipCount;
    else ++candidate->conflictingRelationshipCount;
    const int32_t score = exact ? 3500 : (strong ? 2500 : (possible ? 1200 : -900));
    addEvidence(graph, candidate, OwnershipEvidenceKind::DeclarationDefinitionRelationship, score,
                exact ? "Exact declaration-definition relationship" :
                (strong ? "Strong declaration-definition relationship" : "Possible declaration-definition relationship"),
                1, 0, 0, exact || strong || possible, !exact && !strong && !possible);
}

static bool symbolsForFile(const OwnershipBuildRequest& request, const char* path,
                           uint32_t* indices, uint32_t capacity, uint32_t* outCount) {
    if (outCount) *outCount = 0;
    if (!request.symbolDatabase || !indices || capacity == 0 || !path) return false;
    uint32_t count = 0;
    for (uint32_t i = 0; i < SymbolDatabaseProjectSymbolCount(request.symbolDatabase); ++i) {
        const ProjectSymbol* symbol = SymbolDatabaseProjectSymbolAt(request.symbolDatabase, i);
        if (!symbol) continue;
        const char* symbolPath = SymbolDatabaseDocumentPath(request.symbolDatabase, symbol->documentIndex);
        if (!filePathMatchesSymbol(request, symbolPath, path)) continue;
        if (count < capacity) indices[count++] = i;
        else break;
    }
    if (outCount) *outCount = count;
    return true;
}

static void addSymbolOverlap(const OwnershipBuildRequest& request, FileOwnershipGraph* graph,
                             FileCounterpartCandidate* candidate) {
    if (!request.symbolDatabase || !graph || !candidate) return;
    static const uint32_t kLocalSymbolCapacity = 2000u;
    uint32_t sourceSymbols[kLocalSymbolCapacity] = {};
    uint32_t targetSymbols[kLocalSymbolCapacity] = {};
    uint32_t sourceCount = 0;
    uint32_t targetCount = 0;
    symbolsForFile(request, candidate->source.relativePath, sourceSymbols, kLocalSymbolCapacity, &sourceCount);
    symbolsForFile(request, candidate->target.relativePath, targetSymbols, kLocalSymbolCapacity, &targetCount);
    uint32_t examined = 0;
    uint32_t shared = 0;
    uint32_t namespaces = 0;
    uint32_t classes = 0;
    for (uint32_t i = 0; i < sourceCount && examined < kOwnershipMaxSharedSymbolsPerPair; ++i) {
        const ProjectSymbol* source = SymbolDatabaseProjectSymbolAt(request.symbolDatabase, sourceSymbols[i]);
        if (!source) continue;
        for (uint32_t j = 0; j < targetCount && examined < kOwnershipMaxSharedSymbolsPerPair; ++j, ++examined) {
            const ProjectSymbol* target = SymbolDatabaseProjectSymbolAt(request.symbolDatabase, targetSymbols[j]);
            if (!target) continue;
            const char* sourceName = source->symbol.qualifiedName[0] ? source->symbol.qualifiedName : source->symbol.name;
            const char* targetName = target->symbol.qualifiedName[0] ? target->symbol.qualifiedName : target->symbol.name;
            if (!textEqual(sourceName, targetName)) continue;
            if (!hasText(sourceName, "::") && isCommonUnqualifiedName(sourceName)) continue;
            ++shared;
            if (source->symbol.kind == SymbolKind::Namespace || target->symbol.kind == SymbolKind::Namespace) ++namespaces;
            if (source->symbol.kind == SymbolKind::Class || source->symbol.kind == SymbolKind::Struct ||
                target->symbol.kind == SymbolKind::Class || target->symbol.kind == SymbolKind::Struct) ++classes;
        }
    }
    if (shared == 0) return;
    candidate->sharedSymbolCount += shared;
    addEvidence(graph, candidate, OwnershipEvidenceKind::SymbolOverlap, 1000,
                "Qualified symbol overlap", 0, 0, shared, true, false);
    if (namespaces) addEvidence(graph, candidate, OwnershipEvidenceKind::SharedNamespace, 500,
                                 "Shared namespace", 0, 0, namespaces, true, false);
    if (classes) addEvidence(graph, candidate, OwnershipEvidenceKind::SharedClass, 400,
                             "Shared class", 0, 0, classes, true, false);
}

static void addMetadataEvidence(FileOwnershipGraph* graph, OwnershipBuildScratch* scratch,
                                const OwnershipProjectMetadataPair& pair) {
    if (!graph || !scratch) return;
    const uint32_t header = findFile(graph, pair.headerPath);
    const uint32_t source = findFile(graph, pair.sourcePath);
    if (header == kInvalidIndex || source == kInvalidIndex || !headerKind(graph->files[header].fileKind) ||
        !sourceKind(graph->files[source].fileKind)) return;
    FileCounterpartCandidate* candidate = findOrAddCandidate(graph, scratch, source, header, nullptr);
    if (!candidate) return;
    addEvidence(graph, candidate, OwnershipEvidenceKind::ExplicitProjectMetadata, 5000,
                "Explicit project pairing", 0, 0, 0, true, false);
    if (pair.generated) {
        candidate->generatedVariant = true;
        addEvidence(graph, candidate, OwnershipEvidenceKind::GeneratedVariant, 0,
                    "Generated variant", 0, 0, 0, true, false);
    }
    if (pair.platformVariant) {
        candidate->platformVariant = true;
        addEvidence(graph, candidate, OwnershipEvidenceKind::PlatformVariant, 0,
                    "Platform variant", 0, 0, 0, true, false);
    }
}

static bool fileHasDefinition(const OwnershipBuildRequest& request, const char* path) {
    if (!request.symbolDatabase) return false;
    for (uint32_t i = 0; i < SymbolDatabaseProjectSymbolCount(request.symbolDatabase); ++i) {
        const ProjectSymbol* symbol = SymbolDatabaseProjectSymbolAt(request.symbolDatabase, i);
        if (!symbol || symbol->symbol.declarationRole != SymbolDeclarationRole::Definition) continue;
        if (filePathMatchesSymbol(request, SymbolDatabaseDocumentPath(request.symbolDatabase, symbol->documentIndex), path)) return true;
    }
    return false;
}

static void rankCandidate(FileCounterpartCandidate* candidate) {
    if (!candidate) return;
    int32_t score = 0;
    if (endpointHasEvidence(*candidate, OwnershipEvidenceKind::ExplicitProjectMetadata)) score += 5000;
    score += static_cast<int32_t>(candidate->exactRelationshipCount) * 3500;
    score += static_cast<int32_t>(candidate->strongRelationshipCount) * 2500;
    score += static_cast<int32_t>(candidate->possibleRelationshipCount) * 1200;
    if (candidate->exactStem) score += 2200;
    if (candidate->normalizedStem) score += 1400;
    if (candidate->relativeDirectory) score += 1800;
    if (candidate->directInclude) score += 1700;
    if (candidate->transitiveInclude) score += 700;
    if (endpointHasEvidence(*candidate, OwnershipEvidenceKind::SymbolOverlap)) score += 1000;
    if (endpointHasEvidence(*candidate, OwnershipEvidenceKind::SharedNamespace)) score += 500;
    if (endpointHasEvidence(*candidate, OwnershipEvidenceKind::SharedClass)) score += 400;
    if (candidate->platformVariant || candidate->source.platformVariant || candidate->target.platformVariant) score -= 800;
    if (candidate->generatedVariant || candidate->source.generated || candidate->target.generated) score -= 100;
    if (candidate->stale) score -= 700;
    if (candidate->conflictingRelationshipCount != 0 || endpointHasEvidence(*candidate, OwnershipEvidenceKind::ConflictingEvidence)) score -= 1500;
    if (score < 0) score = 0;
    candidate->rankScore = score;
    const bool metadata = endpointHasEvidence(*candidate, OwnershipEvidenceKind::ExplicitProjectMetadata);
    const bool strongIdentity = metadata || candidate->directInclude || candidate->exactRelationshipCount != 0;
    const uint32_t independent = (candidate->exactStem ? 1u : 0u) + (candidate->normalizedStem ? 1u : 0u) +
        (candidate->relativeDirectory ? 1u : 0u) + (candidate->directInclude || candidate->transitiveInclude ? 1u : 0u) +
        (candidate->relationshipLinked ? 1u : 0u) + (candidate->sharedSymbolCount != 0 ? 1u : 0u) + (metadata ? 1u : 0u);
    if (candidate->conflictingRelationshipCount != 0 || endpointHasEvidence(*candidate, OwnershipEvidenceKind::ConflictingEvidence)) {
        candidate->confidence = CounterpartConfidence::Ambiguous;
        candidate->ambiguous = true;
    } else if (candidate->exactStem && strongIdentity) candidate->confidence = CounterpartConfidence::Exact;
    else if (independent >= 2u && score >= 2500) candidate->confidence = CounterpartConfidence::Strong;
    else if (score > 0) candidate->confidence = CounterpartConfidence::Possible;
    else candidate->confidence = CounterpartConfidence::None;
}

static void markMetadataConflicts(FileOwnershipGraph* graph) {
    if (!graph) return;
    for (uint32_t i = 0; i < graph->candidateCount; ++i) {
        FileCounterpartCandidate& metadata = graph->candidates[i];
        if (!endpointHasEvidence(metadata, OwnershipEvidenceKind::ExplicitProjectMetadata)) continue;
        for (uint32_t j = 0; j < graph->candidateCount; ++j) {
            const FileCounterpartCandidate& relationship = graph->candidates[j];
            if (i == j || !pathEqual(metadata.target.relativePath, relationship.target.relativePath) ||
                pathEqual(metadata.source.relativePath, relationship.source.relativePath) ||
                relationship.exactRelationshipCount == 0) continue;
            addEvidence(graph, &metadata, OwnershipEvidenceKind::ConflictingEvidence, -1500,
                        "Metadata conflicts with relationship evidence", relationship.exactRelationshipCount, 0, 0, false, true);
            metadata.ambiguous = true;
            break;
        }
    }
}

static bool groupHasPath(const FileOwnershipGroup& group, const char* path, bool header) {
    const FileOwnershipEndpoint* values = header ? group.headers : group.sources;
    const uint32_t count = header ? group.headerCount : group.sourceCount;
    for (uint32_t i = 0; i < count; ++i) if (pathEqual(values[i].relativePath, path)) return true;
    return false;
}

static bool appendGroupEndpoint(FileOwnershipGraph* graph, FileOwnershipGroup* group,
                                const FileOwnershipEndpoint& endpoint, bool header) {
    if (!graph || !group) return false;
    if (groupHasPath(*group, endpoint.relativePath, header)) return true;
    if (header) {
        if (graph->groupHeaderCount >= graph->groupHeaderCapacity || group->headerCount >= group->headerCapacity) { group->truncated = true; graph->truncated = true; return false; }
        group->headers[group->headerCount++] = endpoint;
        ++graph->groupHeaderCount;
    } else {
        if (graph->groupSourceCount >= graph->groupSourceCapacity || group->sourceCount >= group->sourceCapacity) { group->truncated = true; graph->truncated = true; return false; }
        group->sources[group->sourceCount++] = endpoint;
        ++graph->groupSourceCount;
    }
    return true;
}

static bool appendGroupCandidate(FileOwnershipGraph* graph, FileOwnershipGroup* group,
                                 uint64_t candidateId) {
    if (!graph || !group) return false;
    for (uint32_t i = 0; i < group->candidateCount; ++i) if (group->candidateIds[i] == candidateId) return true;
    if (graph->groupCandidateIdCount >= graph->groupCandidateIdCapacity || group->candidateCount >= group->candidateCapacity) { group->truncated = true; graph->truncated = true; return false; }
    group->candidateIds[group->candidateCount++] = candidateId;
    ++graph->groupCandidateIdCount;
    return true;
}

static FileOwnershipGroup* createGroup(FileOwnershipGraph* graph, const FileOwnershipEndpoint* endpoint,
                                       bool header, uint64_t groupId) {
    if (!graph || graph->groupCount >= graph->groupCapacity || !endpoint) { if (graph) graph->truncated = true; return nullptr; }
    FileOwnershipGroup& group = graph->groups[graph->groupCount++];
    group = {};
    group.groupId = groupId;
    group.headers = graph->groupHeaders + graph->groupHeaderCount;
    group.headerCapacity = graph->groupHeaderCapacity - graph->groupHeaderCount;
    group.sources = graph->groupSources + graph->groupSourceCount;
    group.sourceCapacity = graph->groupSourceCapacity - graph->groupSourceCount;
    group.candidateIds = graph->groupCandidateIds + graph->groupCandidateIdCount;
    group.candidateCapacity = graph->groupCandidateIdCapacity - graph->groupCandidateIdCount;
    appendGroupEndpoint(graph, &group, *endpoint, header);
    return &group;
}

static FileOwnershipGroup* groupForCandidate(FileOwnershipGraph* graph,
                                             const FileCounterpartCandidate& candidate) {
    FileOwnershipGroup* first = nullptr;
    for (uint32_t i = 0; i < graph->groupCount; ++i) {
        FileOwnershipGroup& group = graph->groups[i];
        if (group.groupId == 0) continue;
        if (groupHasPath(group, candidate.target.relativePath, true) || groupHasPath(group, candidate.source.relativePath, false)) {
            if (!first) first = &group;
            else {
                // Merge a bridge between two previously discovered groups.
                FileOwnershipGroup* other = &group;
                for (uint32_t j = 0; j < other->headerCount; ++j) appendGroupEndpoint(graph, first, other->headers[j], true);
                for (uint32_t j = 0; j < other->sourceCount; ++j) appendGroupEndpoint(graph, first, other->sources[j], false);
                for (uint32_t j = 0; j < other->candidateCount; ++j) appendGroupCandidate(graph, first, other->candidateIds[j]);
                other->groupId = 0;
            }
        }
    }
    return first;
}

static void finalizeGroup(FileOwnershipGroup* group, const FileOwnershipGraph* graph) {
    if (!group || !graph) return;
    group->oneToOne = group->headerCount == 1 && group->sourceCount == 1;
    group->oneToMany = group->headerCount == 1 && group->sourceCount > 1;
    group->manyToOne = group->headerCount > 1 && group->sourceCount == 1;
    group->manyToMany = group->headerCount > 1 && group->sourceCount > 1;
    group->ambiguous = group->ambiguous || group->oneToMany || group->manyToOne || group->manyToMany || group->candidateCount > 1;
    if (group->headerCount != 0 && group->sourceCount == 0) {
        if (group->role != FileOwnershipRole::HeaderOnly) group->role = FileOwnershipRole::Header;
    } else if (group->headerCount == 0 && group->sourceCount != 0) {
        if (group->role != FileOwnershipRole::SourceOnly) group->role = FileOwnershipRole::Source;
    }
    else if (group->headerCount != 0 || group->sourceCount != 0) group->role = FileOwnershipRole::Mixed;
    else group->role = FileOwnershipRole::Unknown;
    CounterpartConfidence best = CounterpartConfidence::None;
    for (uint32_t i = 0; i < group->candidateCount; ++i) {
        for (uint32_t j = 0; j < graph->candidateCount; ++j) {
            if (graph->candidates[j].candidateId != group->candidateIds[i]) continue;
            if (confidenceOrder(graph->candidates[j].confidence) > confidenceOrder(best)) best = graph->candidates[j].confidence;
            if (graph->candidates[j].ambiguous) group->ambiguous = true;
            break;
        }
    }
    if (group->ambiguous) best = CounterpartConfidence::Ambiguous;
    group->confidence = best;
}

static bool pathHasCandidateForFile(const FileOwnershipGraph* graph, const char* path, bool* headerSide) {
    if (headerSide) *headerSide = false;
    for (uint32_t i = 0; i < graph->candidateCount; ++i) {
        if (pathEqual(graph->candidates[i].source.relativePath, path)) { if (headerSide) *headerSide = false; return true; }
        if (pathEqual(graph->candidates[i].target.relativePath, path)) { if (headerSide) *headerSide = true; return true; }
    }
    return false;
}

static void compactGroups(FileOwnershipGraph* graph) {
    if (!graph) return;
    uint32_t out = 0;
    for (uint32_t i = 0; i < graph->groupCount; ++i) {
        if (graph->groups[i].groupId == 0) continue;
        if (out != i) graph->groups[out] = graph->groups[i];
        ++out;
    }
    graph->groupCount = out;
    for (uint32_t i = 0; i < graph->groupCount; ++i) finalizeGroup(&graph->groups[i], graph);
}

static void beginOperation(OwnershipGraphService* service, const OwnershipBuildRequest& request,
                           uint64_t operationId, uint64_t nowMs) {
    OwnershipBuildOperation& operation = service->operation;
    operation = {};
    operation.operationId = operationId;
    operation.state = OwnershipBuildState::CollectingFiles;
    operation.error = OwnershipStatusCode::None;
    operation.buildingGraph = service->buildingGraph;
    operation.request = request;
    operation.startedAtMs = nowMs;
    OwnershipGraphStorage graphStorage = {};
    if (service->buildingGraph && service->buildingGraph->files) {
        graphStorage.files = service->buildingGraph->files;
        graphStorage.fileCapacity = service->buildingGraph->fileCapacity;
        graphStorage.candidates = service->buildingGraph->candidates;
        graphStorage.candidateCapacity = service->buildingGraph->candidateCapacity;
        graphStorage.groups = service->buildingGraph->groups;
        graphStorage.groupCapacity = service->buildingGraph->groupCapacity;
        graphStorage.evidence = service->buildingGraph->evidence;
        graphStorage.evidenceCapacity = service->buildingGraph->evidenceCapacity;
        graphStorage.groupHeaders = service->buildingGraph->groupHeaders;
        graphStorage.groupHeaderCapacity = service->buildingGraph->groupHeaderCapacity;
        graphStorage.groupSources = service->buildingGraph->groupSources;
        graphStorage.groupSourceCapacity = service->buildingGraph->groupSourceCapacity;
        graphStorage.groupCandidateIds = service->buildingGraph->groupCandidateIds;
        graphStorage.groupCandidateIdCapacity = service->buildingGraph->groupCandidateIdCapacity;
    } else {
        graphStorage = *service->buildingStorage;
    }
    OwnershipGraphInit(service->buildingGraph, &graphStorage,
                       request.projectIdText, request.projectGeneration, request.symbolGeneration,
                       request.includeGraph ? request.includeGraph->graphGeneration : 0,
                       request.relationshipGraph ? request.relationshipGraph->graphId : 0);
    if (request.scratch) {
        clearPairSlots(request.scratch);
        if (request.scratch->candidateCountsByFile)
            for (uint32_t i = 0; i < request.scratch->candidateCountsByFileCapacity; ++i) request.scratch->candidateCountsByFile[i] = 0;
        if (request.scratch->relationshipIdentities)
            for (uint32_t i = 0; i < request.scratch->relationshipIdentityCapacity; ++i) request.scratch->relationshipIdentities[i] = {};
    }
}

static void failOperation(OwnershipBuildOperation* operation, OwnershipStatusCode error) {
    if (!operation || operation->state == OwnershipBuildState::Completed || operation->state == OwnershipBuildState::Cancelled || operation->state == OwnershipBuildState::Failed) return;
    operation->state = OwnershipBuildState::Failed;
    operation->error = error;
    operation->terminalReported = false;
}

static bool requestValid(const OwnershipBuildRequest& request) {
    return request.projectIdText[0] != '\0' && request.files && request.scratch &&
        request.scratch->candidateCountsByFile && request.scratch->pairSlots;
}

} // namespace

const char* ProjectCodeFileKindName(ProjectCodeFileKind kind) {
    switch (kind) {
    case ProjectCodeFileKind::CSource: return "C source";
    case ProjectCodeFileKind::CppSource: return "C++ source";
    case ProjectCodeFileKind::Header: return "Header";
    case ProjectCodeFileKind::InlineHeader: return "Inline header";
    default: return "Unknown";
    }
}

const char* FileOwnershipRoleName(FileOwnershipRole role) {
    switch (role) {
    case FileOwnershipRole::Header: return "Header";
    case FileOwnershipRole::Source: return "Source";
    case FileOwnershipRole::HeaderOnly: return "Header-only";
    case FileOwnershipRole::SourceOnly: return "Source-only";
    case FileOwnershipRole::Mixed: return "Mixed";
    default: return "Unknown";
    }
}

const char* CounterpartConfidenceName(CounterpartConfidence confidence) {
    switch (confidence) {
    case CounterpartConfidence::Exact: return "Exact";
    case CounterpartConfidence::Strong: return "Strong";
    case CounterpartConfidence::Possible: return "Possible";
    case CounterpartConfidence::Ambiguous: return "Ambiguous";
    default: return "None";
    }
}

const char* OwnershipEvidenceKindName(OwnershipEvidenceKind kind) {
    switch (kind) {
    case OwnershipEvidenceKind::ExplicitProjectMetadata: return "Explicit project metadata";
    case OwnershipEvidenceKind::ExactStemMatch: return "Exact filename stem";
    case OwnershipEvidenceKind::NormalizedStemMatch: return "Normalized stem";
    case OwnershipEvidenceKind::RelativeDirectoryMatch: return "Module-relative path";
    case OwnershipEvidenceKind::IncludeGraphDirect: return "Direct include";
    case OwnershipEvidenceKind::IncludeGraphTransitive: return "Transitive include";
    case OwnershipEvidenceKind::DeclarationDefinitionRelationship: return "Declaration-definition relationship";
    case OwnershipEvidenceKind::SymbolOverlap: return "Qualified symbol overlap";
    case OwnershipEvidenceKind::SharedNamespace: return "Shared namespace";
    case OwnershipEvidenceKind::SharedClass: return "Shared class";
    case OwnershipEvidenceKind::PlatformVariant: return "Platform variant";
    case OwnershipEvidenceKind::GeneratedVariant: return "Generated variant";
    case OwnershipEvidenceKind::ConflictingEvidence: return "Conflicting evidence";
    default: return "Unknown evidence";
    }
}

const char* OwnershipBuildStateName(OwnershipBuildState state) {
    switch (state) {
    case OwnershipBuildState::CollectingFiles: return "Collecting files";
    case OwnershipBuildState::GeneratingCandidates: return "Generating candidates";
    case OwnershipBuildState::CollectingEvidence: return "Collecting evidence";
    case OwnershipBuildState::Ranking: return "Ranking";
    case OwnershipBuildState::Grouping: return "Grouping";
    case OwnershipBuildState::Completed: return "Completed";
    case OwnershipBuildState::Cancelling: return "Cancelling";
    case OwnershipBuildState::Cancelled: return "Cancelled";
    case OwnershipBuildState::Failed: return "Failed";
    default: return "Idle";
    }
}

const char* OwnershipStatusName(OwnershipStatusCode code) {
    switch (code) {
    case OwnershipStatusCode::OWNERSHIP_NO_PROJECT: return "OWNERSHIP_NO_PROJECT";
    case OwnershipStatusCode::OWNERSHIP_FILE_NOT_ELIGIBLE: return "OWNERSHIP_FILE_NOT_ELIGIBLE";
    case OwnershipStatusCode::OWNERSHIP_PROJECT_STALE: return "OWNERSHIP_PROJECT_STALE";
    case OwnershipStatusCode::OWNERSHIP_SYMBOL_INDEX_STALE: return "OWNERSHIP_SYMBOL_INDEX_STALE";
    case OwnershipStatusCode::OWNERSHIP_INCLUDE_GRAPH_STALE: return "OWNERSHIP_INCLUDE_GRAPH_STALE";
    case OwnershipStatusCode::OWNERSHIP_RELATIONSHIP_GRAPH_STALE: return "OWNERSHIP_RELATIONSHIP_GRAPH_STALE";
    case OwnershipStatusCode::OWNERSHIP_GRAPH_STALE: return "OWNERSHIP_GRAPH_STALE";
    case OwnershipStatusCode::OWNERSHIP_BUILD_CANCELLED: return "OWNERSHIP_BUILD_CANCELLED";
    case OwnershipStatusCode::OWNERSHIP_BUILD_TIMEOUT: return "OWNERSHIP_BUILD_TIMEOUT";
    case OwnershipStatusCode::OWNERSHIP_FILE_LIMIT: return "OWNERSHIP_FILE_LIMIT";
    case OwnershipStatusCode::OWNERSHIP_STEM_BUCKET_LIMIT: return "OWNERSHIP_STEM_BUCKET_LIMIT";
    case OwnershipStatusCode::OWNERSHIP_CANDIDATE_LIMIT: return "OWNERSHIP_CANDIDATE_LIMIT";
    case OwnershipStatusCode::OWNERSHIP_EVIDENCE_LIMIT: return "OWNERSHIP_EVIDENCE_LIMIT";
    case OwnershipStatusCode::OWNERSHIP_PATH_TOO_LONG: return "OWNERSHIP_PATH_TOO_LONG";
    case OwnershipStatusCode::OWNERSHIP_NO_COUNTERPART: return "OWNERSHIP_NO_COUNTERPART";
    case OwnershipStatusCode::OWNERSHIP_HEADER_ONLY: return "OWNERSHIP_HEADER_ONLY";
    case OwnershipStatusCode::OWNERSHIP_SOURCE_ONLY: return "OWNERSHIP_SOURCE_ONLY";
    case OwnershipStatusCode::OWNERSHIP_MULTIPLE_COUNTERPARTS: return "OWNERSHIP_MULTIPLE_COUNTERPARTS";
    case OwnershipStatusCode::OWNERSHIP_AMBIGUOUS: return "OWNERSHIP_AMBIGUOUS";
    case OwnershipStatusCode::OWNERSHIP_RESULTS_TRUNCATED: return "OWNERSHIP_RESULTS_TRUNCATED";
    case OwnershipStatusCode::OWNERSHIP_TARGET_MISSING: return "OWNERSHIP_TARGET_MISSING";
    case OwnershipStatusCode::OWNERSHIP_TARGET_OUTSIDE_PROJECT: return "OWNERSHIP_TARGET_OUTSIDE_PROJECT";
    case OwnershipStatusCode::OWNERSHIP_ACTIVATION_STALE: return "OWNERSHIP_ACTIVATION_STALE";
    case OwnershipStatusCode::OWNERSHIP_ACTIVATION_FAILED: return "OWNERSHIP_ACTIVATION_FAILED";
    case OwnershipStatusCode::OWNERSHIP_SHORTCUT_CONFLICT: return "OWNERSHIP_SHORTCUT_CONFLICT";
    case OwnershipStatusCode::OWNERSHIP_CHORD_CANCELLED: return "OWNERSHIP_CHORD_CANCELLED";
    case OwnershipStatusCode::OWNERSHIP_INTERNAL: return "OWNERSHIP_INTERNAL";
    default: return "NONE";
    }
}

const char* OwnershipResolutionKindName(OwnershipResolutionKind kind) {
    switch (kind) {
    case OwnershipResolutionKind::Direct: return "Direct";
    case OwnershipResolutionKind::Multiple: return "Multiple";
    case OwnershipResolutionKind::HeaderOnly: return "Header-only";
    case OwnershipResolutionKind::SourceOnly: return "Source-only";
    case OwnershipResolutionKind::Stale: return "Stale";
    case OwnershipResolutionKind::Failed: return "Failed";
    default: return "None";
    }
}

uint64_t OwnershipHashText(const char* text) { return hashBytes(text); }

bool OwnershipClassifyPath(const char* path, ProjectCodeFileKind* output) {
    if (output) *output = ProjectCodeFileKind::Unknown;
    if (!path || path[0] == '\0' || textLength(path, kOwnershipMaxPathBytes + 1u) >= kOwnershipMaxPathBytes + 1u) return false;
    const char* base = lastSlash(path);
    const char* dot = lastDot(path, base);
    if (!dot || dot == base) return false;
    ProjectCodeFileKind kind = ProjectCodeFileKind::Unknown;
    if (extensionEqual(dot, ".c")) kind = ProjectCodeFileKind::CSource;
    else if (extensionEqual(dot, ".cc") || extensionEqual(dot, ".cpp") || extensionEqual(dot, ".cxx")) kind = ProjectCodeFileKind::CppSource;
    else if (extensionEqual(dot, ".h") || extensionEqual(dot, ".hh") || extensionEqual(dot, ".hpp") || extensionEqual(dot, ".hxx")) kind = ProjectCodeFileKind::Header;
    else if (extensionEqual(dot, ".inc") || extensionEqual(dot, ".inl")) kind = ProjectCodeFileKind::InlineHeader;
    if (output) *output = kind;
    return kind != ProjectCodeFileKind::Unknown;
}

bool OwnershipIsEligiblePath(const char* path) {
    ProjectCodeFileKind kind = ProjectCodeFileKind::Unknown;
    return OwnershipClassifyPath(path, &kind);
}

bool OwnershipBuildStemKeys(const char* path, ProjectCodeFileKind kind,
                            char* exactStem, uint32_t exactCapacity,
                            char* normalizedStem, uint32_t normalizedCapacity,
                            char* modulePath, uint32_t moduleCapacity,
                            bool* platformVariant, char* platformLabel,
                            uint32_t platformLabelCapacity) {
    if (exactStem && exactCapacity) exactStem[0] = '\0';
    if (normalizedStem && normalizedCapacity) normalizedStem[0] = '\0';
    if (modulePath && moduleCapacity) modulePath[0] = '\0';
    if (platformVariant) *platformVariant = false;
    if (platformLabel && platformLabelCapacity) platformLabel[0] = '\0';
    if (!path || (!sourceKind(kind) && !headerKind(kind))) return false;
    const char* base = lastSlash(path);
    const char* dot = lastDot(path, base);
    if (!base || !dot || dot == base) return false;
    const uint32_t stemLength = static_cast<uint32_t>(dot - base);
    if (stemLength == 0 || stemLength + 1 >= exactCapacity || stemLength + 1 >= normalizedCapacity) return false;
    for (uint32_t i = 0; i < stemLength; ++i) exactStem[i] = base[i];
    exactStem[stemLength] = '\0';
    uint32_t normalizedLength = stemLength;
    for (uint32_t i = 0; i < sizeof(kNormalizedSuffixes) / sizeof(kNormalizedSuffixes[0]); ++i) {
        if (suffixEqual(exactStem, normalizedLength, kNormalizedSuffixes[i])) {
            normalizedLength -= textLength(kNormalizedSuffixes[i], 64u);
            break;
        }
    }
    if (normalizedLength == 0) normalizedLength = stemLength;
    for (uint32_t i = 0; i < normalizedLength; ++i) normalizedStem[i] = exactStem[i];
    normalizedStem[normalizedLength] = '\0';
    const uint32_t suffixStart = normalizedLength;
    if (platformVariant && suffixStart < stemLength) {
        const char* label = nullptr;
        if (platformSuffix(exactStem + suffixStart, stemLength - suffixStart, &label)) {
            *platformVariant = true;
            if (platformLabel) copyText(platformLabel, platformLabelCapacity, label);
        }
    }
    uint32_t moduleLength = static_cast<uint32_t>(dot - path);
    if (moduleLength + 1 >= moduleCapacity) return false;
    for (uint32_t i = 0; i < moduleLength; ++i) {
        char value = path[i] == '\\' ? '/' : path[i];
        modulePath[i] = lowerAscii(value);
    }
    modulePath[moduleLength] = '\0';
    const char* moduleBase = modulePath;
    const char* slash = nullptr;
    for (uint32_t i = 0; i < moduleLength; ++i) if (modulePath[i] == '/') { slash = modulePath + i; break; }
    if (slash) {
        char root[64] = {};
        const uint32_t rootLength = static_cast<uint32_t>(slash - modulePath);
        if (rootLength < sizeof(root)) {
            for (uint32_t i = 0; i < rootLength; ++i) root[i] = modulePath[i];
            root[rootLength] = '\0';
            if (roleDirectory(root, nullptr)) moduleBase = slash + 1;
        }
    }
    if (moduleBase != modulePath) {
        uint32_t offset = static_cast<uint32_t>(moduleBase - modulePath);
        for (uint32_t i = 0; moduleBase[i] != '\0'; ++i) modulePath[i] = moduleBase[i];
        modulePath[moduleLength - offset] = '\0';
    }
    return true;
}

bool OwnershipGraphIsCurrent(const FileOwnershipGraph* graph, const char* projectId,
                             uint64_t projectGeneration, uint64_t symbolGeneration,
                             uint64_t includeGraphGeneration,
                             uint64_t relationshipGraphGeneration) {
    return graph && graph->complete && !graph->stale && projectId &&
        textEqual(graph->projectIdText, projectId) && graph->projectGeneration == projectGeneration &&
        graph->symbolGeneration == symbolGeneration && graph->includeGraphGeneration == includeGraphGeneration &&
        graph->relationshipGraphGeneration == relationshipGraphGeneration;
}

void OwnershipBuildRequestInit(OwnershipBuildRequest* request) { if (request) *request = {}; }

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
                               bool* includeVisited, uint32_t includeVisitedCapacity) {
    if (!scratch) return;
    *scratch = {};
    scratch->exactRefs = exactRefs; scratch->exactRefCapacity = exactRefCapacity;
    scratch->normalizedRefs = normalizedRefs; scratch->normalizedRefCapacity = normalizedRefCapacity;
    scratch->moduleRefs = moduleRefs; scratch->moduleRefCapacity = moduleRefCapacity;
    scratch->exactBuckets = exactBuckets; scratch->exactBucketCapacity = exactBucketCapacity;
    scratch->normalizedBuckets = normalizedBuckets; scratch->normalizedBucketCapacity = normalizedBucketCapacity;
    scratch->moduleBuckets = moduleBuckets; scratch->moduleBucketCapacity = moduleBucketCapacity;
    scratch->pairSlots = pairSlots; scratch->pairSlotCapacity = pairSlotCapacity;
    scratch->candidateCountsByFile = candidateCountsByFile; scratch->candidateCountsByFileCapacity = candidateCountsByFileCapacity;
    scratch->relationshipIdentities = relationshipIdentities; scratch->relationshipIdentityCapacity = relationshipIdentityCapacity;
    scratch->includeQueue = includeQueue; scratch->includeQueueCapacity = includeQueueCapacity;
    scratch->includeVisited = includeVisited; scratch->includeVisitedCapacity = includeVisitedCapacity;
}

void OwnershipGraphStorageInit(OwnershipGraphStorage* storage,
                               FileOwnershipEndpoint* files, uint32_t fileCapacity,
                               FileCounterpartCandidate* candidates, uint32_t candidateCapacity,
                               FileOwnershipGroup* groups, uint32_t groupCapacity,
                               OwnershipEvidence* evidence, uint32_t evidenceCapacity,
                               FileOwnershipEndpoint* groupHeaders, uint32_t groupHeaderCapacity,
                               FileOwnershipEndpoint* groupSources, uint32_t groupSourceCapacity,
                               uint64_t* groupCandidateIds, uint32_t groupCandidateIdCapacity) {
    if (!storage) return;
    *storage = {};
    storage->files = files; storage->fileCapacity = fileCapacity;
    storage->candidates = candidates; storage->candidateCapacity = candidateCapacity;
    storage->groups = groups; storage->groupCapacity = groupCapacity;
    storage->evidence = evidence; storage->evidenceCapacity = evidenceCapacity;
    storage->groupHeaders = groupHeaders; storage->groupHeaderCapacity = groupHeaderCapacity;
    storage->groupSources = groupSources; storage->groupSourceCapacity = groupSourceCapacity;
    storage->groupCandidateIds = groupCandidateIds; storage->groupCandidateIdCapacity = groupCandidateIdCapacity;
}

void OwnershipGraphInit(FileOwnershipGraph* graph, const OwnershipGraphStorage* storage,
                        const char* projectId, uint64_t projectGeneration,
                        uint64_t symbolGeneration, uint64_t includeGraphGeneration,
                        uint64_t relationshipGraphGeneration) {
    if (!graph || !storage) return;
    FileOwnershipEndpoint* files = storage->files;
    FileCounterpartCandidate* candidates = storage->candidates;
    FileOwnershipGroup* groups = storage->groups;
    OwnershipEvidence* evidence = storage->evidence;
    FileOwnershipEndpoint* groupHeaders = storage->groupHeaders;
    FileOwnershipEndpoint* groupSources = storage->groupSources;
    uint64_t* groupCandidateIds = storage->groupCandidateIds;
    *graph = {};
    graph->graphId = hashBytes(projectId) ^ projectGeneration ^ (symbolGeneration << 1) ^ (includeGraphGeneration << 2) ^ (relationshipGraphGeneration << 3);
    if (graph->graphId == 0) graph->graphId = 1;
    graph->projectId = hashBytes(projectId);
    graph->projectGeneration = projectGeneration;
    graph->symbolGeneration = symbolGeneration;
    graph->includeGraphGeneration = includeGraphGeneration;
    graph->relationshipGraphGeneration = relationshipGraphGeneration;
    copyText(graph->projectIdText, sizeof(graph->projectIdText), projectId);
    graph->files = files; graph->fileCapacity = storage->fileCapacity;
    graph->candidates = candidates; graph->candidateCapacity = storage->candidateCapacity;
    graph->groups = groups; graph->groupCapacity = storage->groupCapacity;
    graph->evidence = evidence; graph->evidenceCapacity = storage->evidenceCapacity;
    graph->groupHeaders = groupHeaders; graph->groupHeaderCapacity = storage->groupHeaderCapacity;
    graph->groupSources = groupSources; graph->groupSourceCapacity = storage->groupSourceCapacity;
    graph->groupCandidateIds = groupCandidateIds; graph->groupCandidateIdCapacity = storage->groupCandidateIdCapacity;
    graph->complete = false;
}

void OwnershipGraphServiceInit(OwnershipGraphService* service,
                               FileOwnershipGraph* completedGraph,
                               FileOwnershipGraph* buildingGraph,
                               OwnershipGraphStorage* buildingStorage) {
    if (!service) return;
    *service = {};
    service->completedGraph = completedGraph;
    service->buildingGraph = buildingGraph;
    service->buildingStorage = buildingStorage;
    service->nextOperationId = 1;
    service->operation.state = OwnershipBuildState::Idle;
}

bool OwnershipGraphBuildStart(OwnershipGraphService* service,
                              const OwnershipBuildRequest& request, uint64_t nowMs,
                              uint64_t* outOperationId) {
    if (outOperationId) *outOperationId = 0;
    if (!service || !requestValid(request) || !service->buildingGraph || !service->buildingStorage) return false;
    if (OwnershipGraphBuildIsActive(service)) {
        service->pendingRequest = request;
        service->pendingStartedAtMs = nowMs;
        service->pendingOperationId = ++service->nextOperationId;
        service->hasPendingRequest = true;
        service->operation.cancellationRequested = true;
        service->operation.pendingSupersession = true;
        service->operation.state = OwnershipBuildState::Cancelling;
        if (outOperationId) *outOperationId = service->pendingOperationId;
        return true;
    }
    const uint64_t operationId = ++service->nextOperationId;
    beginOperation(service, request, operationId, nowMs);
    if (outOperationId) *outOperationId = operationId;
    return true;
}

bool OwnershipGraphBuildCancel(OwnershipGraphService* service, uint64_t operationId) {
    if (!service || !OwnershipGraphBuildIsActive(service)) return false;
    if (service->operation.operationId != operationId && service->pendingOperationId != operationId) return false;
    service->operation.cancellationRequested = true;
    service->operation.state = OwnershipBuildState::Cancelling;
    return true;
}

bool OwnershipGraphBuildIsActive(const OwnershipGraphService* service) {
    if (!service) return false;
    const OwnershipBuildState state = service->operation.state;
    return state == OwnershipBuildState::CollectingFiles || state == OwnershipBuildState::GeneratingCandidates ||
        state == OwnershipBuildState::CollectingEvidence || state == OwnershipBuildState::Ranking ||
        state == OwnershipBuildState::Grouping || state == OwnershipBuildState::Cancelling;
}

const OwnershipBuildOperation* OwnershipGraphBuildInfo(const OwnershipGraphService* service) {
    return service ? &service->operation : nullptr;
}

bool OwnershipGraphBuildPoll(OwnershipGraphService* service, uint64_t operationId,
                             uint32_t workBudget, uint64_t nowMs) {
    if (!service || !OwnershipGraphBuildIsActive(service)) return false;
    if (operationId != service->operation.operationId && operationId != service->pendingOperationId) return false;
    OwnershipBuildOperation& operation = service->operation;
    if (operation.cancellationRequested) {
        operation.state = OwnershipBuildState::Cancelling;
        operation.state = OwnershipBuildState::Cancelled;
        operation.error = OwnershipStatusCode::OWNERSHIP_BUILD_CANCELLED;
        operation.terminalReported = false;
        if (service->hasPendingRequest) {
            const OwnershipBuildRequest pending = service->pendingRequest;
            const uint64_t pendingId = service->pendingOperationId;
            const uint64_t pendingStart = service->pendingStartedAtMs;
            service->hasPendingRequest = false;
            service->pendingOperationId = 0;
            beginOperation(service, pending, pendingId, pendingStart);
        }
        return true;
    }
    if (nowMs >= operation.startedAtMs && nowMs - operation.startedAtMs > kOwnershipMaxBuildDurationMs) {
        failOperation(&operation, OwnershipStatusCode::OWNERSHIP_BUILD_TIMEOUT);
        return true;
    }
    FileOwnershipGraph* graph = operation.buildingGraph;
    OwnershipBuildScratch* scratch = operation.request.scratch;
    if (!graph || !scratch) { failOperation(&operation, OwnershipStatusCode::OWNERSHIP_INTERNAL); return true; }
    if (workBudget == 0) workBudget = 1;
    while (workBudget-- > 0 && OwnershipGraphBuildIsActive(service)) {
        if (operation.state == OwnershipBuildState::CollectingFiles) {
            if (operation.fileIndex < operation.request.fileCount) {
                const OwnershipFileRecord& record = operation.request.files[operation.fileIndex++];
                ProjectCodeFileKind kind = record.fileKind;
                if (kind == ProjectCodeFileKind::Unknown) OwnershipClassifyPath(record.relativePath, &kind);
                if (!sourceKind(kind) && !headerKind(kind)) continue;
                if (textLength(record.relativePath, kOwnershipMaxPathBytes + 1u) >= kOwnershipMaxPathBytes + 1u) { graph->truncated = true; continue; }
                if (graph->fileCount >= graph->fileCapacity || graph->fileCount >= kOwnershipMaxProjectCodeFiles) { graph->truncated = true; continue; }
                FileOwnershipEndpoint& endpoint = graph->files[graph->fileCount++];
                endpoint = record;
                endpoint.fileKind = kind;
                if (endpoint.fileId == 0) endpoint.fileId = hashBytes(endpoint.relativePath);
                ++operation.filesCollected;
            } else {
                if (graph->fileCount > 1) {
                    // Deterministic quicksort by project-relative path then ID.
                    for (uint32_t i = 1; i < graph->fileCount; ++i) {
                        FileOwnershipEndpoint value = graph->files[i];
                        uint32_t j = i;
                        while (j > 0 && (textCompare(value.relativePath, graph->files[j - 1].relativePath) < 0 ||
                               (textCompare(value.relativePath, graph->files[j - 1].relativePath) == 0 && value.fileId < graph->files[j - 1].fileId))) {
                            graph->files[j] = graph->files[j - 1]; --j;
                        }
                        graph->files[j] = value;
                    }
                }
                if (operation.request.inventoryTruncated) graph->truncated = true;
                operation.state = OwnershipBuildState::GeneratingCandidates;
                operation.fileIndex = 0;
                operation.indexKind = 0;
                operation.bucketIndex = 0;
                clearPairSlots(scratch);
            }
        } else if (operation.state == OwnershipBuildState::GeneratingCandidates) {
            if (operation.indexKind < 3u) {
                OwnershipBucketRef* refs = operation.indexKind == 0 ? scratch->exactRefs : (operation.indexKind == 1 ? scratch->normalizedRefs : scratch->moduleRefs);
                uint32_t refCapacity = operation.indexKind == 0 ? scratch->exactRefCapacity : (operation.indexKind == 1 ? scratch->normalizedRefCapacity : scratch->moduleRefCapacity);
                if (operation.fileIndex < graph->fileCount) {
                    char key[kOwnershipMaxPathBytes + 1u] = {};
                    if (refs && operation.fileIndex < refCapacity && buildKeyForFile(graph->files[operation.fileIndex], operation.indexKind, key, sizeof(key))) {
                        refs[operation.fileIndex] = { hashBytes(key), operation.fileIndex };
                    } else graph->truncated = true;
                    ++operation.fileIndex;
                } else {
                    if (operation.indexKind == 0) buildBuckets(scratch->exactRefs, graph->fileCount, scratch->exactBuckets, scratch->exactBucketCapacity, &graph->truncated);
                    else if (operation.indexKind == 1) buildBuckets(scratch->normalizedRefs, graph->fileCount, scratch->normalizedBuckets, scratch->normalizedBucketCapacity, &graph->truncated);
                    else buildBuckets(scratch->moduleRefs, graph->fileCount, scratch->moduleBuckets, scratch->moduleBucketCapacity, &graph->truncated);
                    operation.fileIndex = 0;
                    ++operation.indexKind;
                    operation.bucketIndex = 0;
                    operation.bucketLeft = 0;
                    operation.bucketRight = 1;
                }
            } else {
                const OwnershipBucket* buckets = operation.indexKind == 3u ? scratch->exactBuckets : (operation.indexKind == 4u ? scratch->normalizedBuckets : scratch->moduleBuckets);
                const OwnershipBucketRef* refs = operation.indexKind == 3u ? scratch->exactRefs : (operation.indexKind == 4u ? scratch->normalizedRefs : scratch->moduleRefs);
                const uint32_t bucketCapacity = operation.indexKind == 3u ? scratch->exactBucketCapacity : (operation.indexKind == 4u ? scratch->normalizedBucketCapacity : scratch->moduleBucketCapacity);
                const uint32_t totalBuckets = bucketCount(buckets, bucketCapacity);
                if (operation.bucketIndex < totalBuckets) {
                    const OwnershipBucket& bucket = buckets[operation.bucketIndex];
                    const uint32_t count = bucket.count > kOwnershipMaxFilesPerStemBucket ? kOwnershipMaxFilesPerStemBucket : bucket.count;
                    if (bucket.count > count) graph->truncated = true;
                    if (count < 2 || operation.bucketLeft + 1 >= count) {
                        ++operation.bucketIndex; operation.bucketLeft = 0; operation.bucketRight = 1; continue;
                    }
                    if (operation.bucketRight >= count) { ++operation.bucketLeft; operation.bucketRight = operation.bucketLeft + 1; continue; }
                    const uint32_t left = refs[bucket.first + operation.bucketLeft].fileIndex;
                    const uint32_t right = refs[bucket.first + operation.bucketRight].fileIndex;
                    ++operation.bucketRight;
                    ++operation.pairComparisons;
                    if (left == right || !sameBucketKey(graph->files[left], graph->files[right], operation.indexKind - 3u)) continue;
                    const OwnershipEvidenceKind evidenceKind = operation.indexKind == 3u ? OwnershipEvidenceKind::ExactStemMatch :
                        (operation.indexKind == 4u ? OwnershipEvidenceKind::NormalizedStemMatch : OwnershipEvidenceKind::RelativeDirectoryMatch);
                    const int32_t score = operation.indexKind == 3u ? 2200 : (operation.indexKind == 4u ? 1400 : 1800);
                    const char* detail = operation.indexKind == 3u ? "Exact filename stem" : (operation.indexKind == 4u ? "Conservative normalized stem" : "Module-relative path matches");
                    addNameEvidence(graph, scratch, left, right, evidenceKind, score, detail,
                                    operation.indexKind == 3u, operation.indexKind == 4u, operation.indexKind == 5u);
                    ++operation.candidatePairsGenerated;
                } else if (operation.indexKind == 5u) {
                    operation.state = OwnershipBuildState::CollectingEvidence;
                    operation.includeIndex = 0;
                    operation.fileIndex = 0;
                    operation.relationshipIndex = 0;
                    operation.metadataIndex = 0;
                } else {
                    ++operation.indexKind;
                    operation.bucketIndex = 0;
                    operation.bucketLeft = 0;
                    operation.bucketRight = 1;
                }
            }
        } else if (operation.state == OwnershipBuildState::CollectingEvidence) {
            if (operation.request.includeGraph && operation.includeIndex < operation.request.includeGraph->edgeCount) {
                const IncludeEdge* edge = IncludeGraphEdgeAt(operation.request.includeGraph, operation.includeIndex++);
                if (edge) addIncludeEvidence(graph, scratch, *edge, false);
            } else if (operation.request.includeGraph && operation.fileIndex < graph->fileCount) {
                const FileOwnershipEndpoint& file = graph->files[operation.fileIndex++];
                if (sourceKind(file.fileKind)) addTransitiveIncludes(graph, operation.request.includeGraph, scratch, file.relativePath);
            } else if (operation.request.relationshipGraph && operation.relationshipIndex < operation.request.relationshipGraph->relationshipCount) {
                const SymbolRelationship* relationship = SymbolRelationshipGraphRelationshipAt(operation.request.relationshipGraph, operation.relationshipIndex++);
                if (relationship) addRelationshipEvidence(graph, scratch, *relationship);
            } else if (operation.request.metadata && operation.metadataIndex < operation.request.metadataCount) {
                addMetadataEvidence(graph, scratch, operation.request.metadata[operation.metadataIndex++]);
            } else if (operation.evidenceIndex < graph->candidateCount) {
                addSymbolOverlap(operation.request, graph, &graph->candidates[operation.evidenceIndex++]);
            } else {
                operation.state = OwnershipBuildState::Ranking;
                operation.rankingIndex = 0;
            }
        } else if (operation.state == OwnershipBuildState::Ranking) {
            if (operation.rankingIndex < graph->candidateCount) rankCandidate(&graph->candidates[operation.rankingIndex++]);
            else {
                markMetadataConflicts(graph);
                for (uint32_t i = 0; i < graph->candidateCount; ++i) rankCandidate(&graph->candidates[i]);
                if (graph->candidateCount > 1) quickSortCandidates(graph->candidates, 0, static_cast<int32_t>(graph->candidateCount - 1));
                for (uint32_t i = 0; i < graph->candidateCount; ++i) {
                    uint32_t equalSource = 0;
                    uint32_t equalTarget = 0;
                    int32_t bestSource = graph->candidates[i].rankScore;
                    int32_t bestTarget = graph->candidates[i].rankScore;
                    for (uint32_t j = 0; j < graph->candidateCount; ++j) {
                        if (pathEqual(graph->candidates[i].source.relativePath, graph->candidates[j].source.relativePath)) { ++equalSource; if (graph->candidates[j].rankScore > bestSource) bestSource = graph->candidates[j].rankScore; }
                        if (pathEqual(graph->candidates[i].target.relativePath, graph->candidates[j].target.relativePath)) { ++equalTarget; if (graph->candidates[j].rankScore > bestTarget) bestTarget = graph->candidates[j].rankScore; }
                    }
                    if ((equalSource > 1 || equalTarget > 1) &&
                        ((bestSource > graph->candidates[i].rankScore && bestSource - graph->candidates[i].rankScore <= 400) ||
                         (bestTarget > graph->candidates[i].rankScore && bestTarget - graph->candidates[i].rankScore <= 400))) {
                        graph->candidates[i].ambiguous = true;
                        if (graph->candidates[i].confidence == CounterpartConfidence::Exact) graph->candidates[i].confidence = CounterpartConfidence::Strong;
                    }
                }
                operation.state = OwnershipBuildState::Grouping;
                operation.groupingCandidateIndex = 0;
                operation.groupingIndex = 0;
            }
        } else if (operation.state == OwnershipBuildState::Grouping) {
            if (operation.groupingCandidateIndex < graph->candidateCount) {
                FileCounterpartCandidate& candidate = graph->candidates[operation.groupingCandidateIndex++];
                FileOwnershipGroup* group = groupForCandidate(graph, candidate);
                if (!group) group = createGroup(graph, &candidate.target, true, graph->groupCount + 1u);
                if (group) {
                    appendGroupEndpoint(graph, group, candidate.target, true);
                    appendGroupEndpoint(graph, group, candidate.source, false);
                    appendGroupCandidate(graph, group, candidate.candidateId);
                    group->ambiguous = group->ambiguous || candidate.ambiguous;
                }
            } else if (operation.groupingIndex < graph->fileCount) {
                const FileOwnershipEndpoint& file = graph->files[operation.groupingIndex++];
                if (pathHasCandidateForFile(graph, file.relativePath, nullptr)) continue;
                const bool isHeader = headerKind(file.fileKind);
                FileOwnershipGroup* group = createGroup(graph, &file, isHeader, graph->groupCount + 1u);
                if (group) {
                    if (isHeader) {
                        if (fileHasDefinition(operation.request, file.relativePath)) group->role = FileOwnershipRole::HeaderOnly;
                        else group->role = FileOwnershipRole::Header;
                    } else group->role = FileOwnershipRole::SourceOnly;
                    group->confidence = CounterpartConfidence::None;
                }
            } else {
                compactGroups(graph);
                graph->complete = true;
                graph->stale = false;
                if (operation.request.includeGraph && !IncludeGraphIsCurrent(operation.request.includeGraph,
                        operation.request.projectIdText, operation.request.projectGeneration)) graph->stale = true;
                if (operation.request.relationshipGraph && !SymbolRelationshipGraphIsCurrent(operation.request.relationshipGraph,
                        operation.request.projectIdText, operation.request.projectGeneration, operation.request.symbolGeneration)) graph->stale = true;
                operation.state = OwnershipBuildState::Completed;
                operation.error = graph->truncated ? OwnershipStatusCode::OWNERSHIP_RESULTS_TRUNCATED : OwnershipStatusCode::None;
                FileOwnershipGraph* old = service->completedGraph;
                service->completedGraph = graph;
                service->buildingGraph = old;
                operation.buildingGraph = service->buildingGraph;
                // The old completed graph is now the reusable build target.
                if (service->buildingStorage) {
                    // storage ownership is tied to the build graph by the caller;
                    // the next start reinitializes it before writing.
                }
                return true;
            }
        }
    }
    return true;
}

const FileCounterpartCandidate* OwnershipGraphCandidateAt(const FileOwnershipGraph* graph, uint32_t index) {
    return graph && index < graph->candidateCount ? &graph->candidates[index] : nullptr;
}

const FileOwnershipGroup* OwnershipGraphGroupAt(const FileOwnershipGraph* graph, uint32_t index) {
    return graph && index < graph->groupCount ? &graph->groups[index] : nullptr;
}

uint32_t OwnershipGraphCandidatesForFile(const FileOwnershipGraph* graph, const char* relativePath,
                                         uint32_t* indices, uint32_t capacity) {
    if (!graph || !relativePath) return 0;
    uint32_t count = 0;
    for (uint32_t i = 0; i < graph->candidateCount; ++i) {
        const FileCounterpartCandidate& candidate = graph->candidates[i];
        if (!pathEqual(candidate.source.relativePath, relativePath) && !pathEqual(candidate.target.relativePath, relativePath)) continue;
        if (indices && count < capacity) indices[count] = i;
        ++count;
    }
    return count;
}

bool OwnershipResolveFile(const FileOwnershipGraph* graph, const char* relativePath,
                          OwnershipResolution* output) {
    if (!output) return false;
    const uint32_t capacity = output->candidateCapacity;
    uint32_t* indices = output->candidateIndices;
    *output = {};
    output->candidateIndices = indices;
    output->candidateCapacity = capacity;
    if (!graph || !relativePath) { output->kind = OwnershipResolutionKind::Failed; output->status = OwnershipStatusCode::OWNERSHIP_INTERNAL; return false; }
    if (!graph->complete) { output->kind = OwnershipResolutionKind::Stale; output->updating = true; output->stale = true; output->status = OwnershipStatusCode::OWNERSHIP_GRAPH_STALE; return false; }
    output->candidateCount = OwnershipGraphCandidatesForFile(graph, relativePath, indices, capacity);
    output->visibleCandidateCount = output->candidateCount < kOwnershipMaxVisiblePickerCandidates ? output->candidateCount : kOwnershipMaxVisiblePickerCandidates;
    output->truncated = output->candidateCount > capacity || output->candidateCount > kOwnershipMaxPickerCandidates;
    if (output->candidateCount == 0) {
        ProjectCodeFileKind kind = ProjectCodeFileKind::Unknown;
        OwnershipClassifyPath(relativePath, &kind);
        if (headerKind(kind)) {
            for (uint32_t i = 0; i < graph->groupCount; ++i) if (graph->groups[i].headerCount != 0 &&
                pathEqual(graph->groups[i].headers[0].relativePath, relativePath)) {
                if (graph->groups[i].role == FileOwnershipRole::HeaderOnly) { output->kind = OwnershipResolutionKind::HeaderOnly; output->status = OwnershipStatusCode::OWNERSHIP_HEADER_ONLY; return true; }
            }
        } else if (sourceKind(kind)) { output->kind = OwnershipResolutionKind::SourceOnly; output->status = OwnershipStatusCode::OWNERSHIP_SOURCE_ONLY; return true; }
        output->kind = OwnershipResolutionKind::None;
        output->status = OwnershipStatusCode::OWNERSHIP_NO_COUNTERPART;
        return true;
    }
    if (output->candidateCount == 1 && indices && indices[0] < graph->candidateCount &&
        (graph->candidates[indices[0]].confidence == CounterpartConfidence::Exact || graph->candidates[indices[0]].confidence == CounterpartConfidence::Strong) &&
        !graph->candidates[indices[0]].ambiguous) {
        output->kind = OwnershipResolutionKind::Direct;
        return true;
    }
    output->kind = OwnershipResolutionKind::Multiple;
    output->status = output->candidateCount > 1 ? OwnershipStatusCode::OWNERSHIP_MULTIPLE_COUNTERPARTS : OwnershipStatusCode::OWNERSHIP_AMBIGUOUS;
    return true;
}

} // namespace developer_studio
} // namespace guidexos
