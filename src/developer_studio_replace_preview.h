#pragma once

#include "developer_studio_project_search.h"

namespace guidexos {
namespace developer_studio {

static const uint32_t kReplacePreviewMaxFindBytes = kFindMaxQueryBytes;
static const uint32_t kReplacePreviewMaxReplacementBytes = 256u;
static const uint32_t kReplacePreviewMaxPreviewBytes = 96u;
static const uint32_t kReplacePreviewMaxDisplayMatches = 8u;
#if defined(GXOS_DEVELOPER_STUDIO_BARE_METAL)
static const uint32_t kReplacePreviewMaxAffectedFiles = 8u;
static const uint32_t kReplacePreviewMaxMatches = 64u;
#else
static const uint32_t kReplacePreviewMaxAffectedFiles = 32u;
static const uint32_t kReplacePreviewMaxMatches = 128u;
#endif

enum class ReplacePreviewError {
    None = 0,
    EmptyFind,
    FindTooLong,
    ReplacementTooLong,
    NoProject
};

struct ReplacePreviewFile {
    char relativePath[kMaxPathBytes];
    uint64_t sourceSize;
    uint64_t sourceFingerprint;
    uint64_t documentId;
    uint32_t documentGeneration;
    uint32_t matchCount;
    ProjectSearchSourceKind sourceKind;
};

struct ReplacePreviewMatch {
    uint32_t fileIndex;
    uint64_t byteOffset;
    uint32_t line;
    uint32_t column;
    uint32_t matchLength;
    char before[kReplacePreviewMaxPreviewBytes + 1];
    char after[kReplacePreviewMaxPreviewBytes + 1];
    bool beforeLeftTruncated;
    bool beforeRightTruncated;
    bool afterLeftTruncated;
    bool afterRightTruncated;
};

// The replacement plan is only a description of proposed byte edits. It has
// no filesystem write handle and cannot apply the retained edits.
struct ReplacePreviewPlan {
    char projectId[kMaxProjectIdBytes];
    uint64_t projectGeneration;
    char findText[kReplacePreviewMaxFindBytes + 1];
    char replacementText[kReplacePreviewMaxReplacementBytes + 1];
    bool caseSensitive;
    bool configured;
    bool complete;
    bool stale;
    bool capacityExceeded;
    uint32_t filesAffected;
    uint32_t matchCount;
    uint32_t matchesObserved;
    ReplacePreviewFile files[kReplacePreviewMaxAffectedFiles];
    ReplacePreviewMatch matches[kReplacePreviewMaxMatches];
};

void ReplacePreviewPlanInit(ReplacePreviewPlan* plan);
ReplacePreviewError ReplacePreviewPlanBegin(ReplacePreviewPlan* plan,
                                            const char* projectId,
                                            uint64_t projectGeneration,
                                            const char* findText,
                                            const char* replacementText,
                                            bool caseSensitive);
void ReplacePreviewPlanClear(ReplacePreviewPlan* plan);
void ReplacePreviewPlanFinish(ReplacePreviewPlan* plan, bool scanComplete);
void ReplacePreviewPlanMarkStale(ReplacePreviewPlan* plan);
bool ReplacePreviewPlanVisitFile(void* userData, const ProjectSearchScanFile* file);
const ReplacePreviewFile* ReplacePreviewPlanFileAt(const ReplacePreviewPlan* plan,
                                                   uint32_t index);
const ReplacePreviewMatch* ReplacePreviewPlanMatchAt(const ReplacePreviewPlan* plan,
                                                     uint32_t index);
uint64_t ReplacePreviewFingerprint(const char* data, uint32_t length);
bool ReplacePreviewFileFingerprintMatches(const ReplacePreviewFile* file,
                                          const char* data, uint32_t length);

} // namespace developer_studio
} // namespace guidexos
