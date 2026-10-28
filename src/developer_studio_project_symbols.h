#pragma once

#include "developer_studio_document_outline.h"
#include "developer_studio_project_search.h"

namespace guidexos {
namespace developer_studio {

#if defined(GXOS_DEVELOPER_STUDIO_BARE_METAL)
static const uint32_t kProjectSymbolsMaxCandidates = 32u;
static const uint32_t kProjectSymbolsMaxDisplayedResults = 24u;
static const uint32_t kProjectSymbolsMaxFiles = 64u;
static const uint64_t kProjectSymbolsMaxBytes = 2ull * 1024ull * 1024ull;
static const uint64_t kProjectSymbolsMaxDurationMs = 20000ull;
#else
static const uint32_t kProjectSymbolsMaxCandidates = 256u;
static const uint32_t kProjectSymbolsMaxDisplayedResults = 100u;
static const uint32_t kProjectSymbolsMaxFiles = 512u;
static const uint64_t kProjectSymbolsMaxBytes = 64ull * 1024ull * 1024ull;
static const uint64_t kProjectSymbolsMaxDurationMs = 60000ull;
#endif
static const uint32_t kProjectSymbolsMaxNameFilterBytes = 64u;
static const uint32_t kProjectSymbolsMaxFileFilterBytes = 128u;
static const uint32_t kProjectSymbolsMaxPathBytes = kMaxPathBytes;
static const uint32_t kProjectSymbolsMaxSignatureBytes = kDocumentOutlineMaxSignatureBytes;

struct ProjectSymbolCandidate {
    char name[kDocumentOutlineMaxIdentifierBytes + 1u];
    char signature[kDocumentOutlineMaxSignatureBytes + 1u];
    char relativePath[kMaxPathBytes];
    DocumentOutlineCategory category;
    uint32_t line;
    uint32_t offset;
    uint32_t duplicateOrdinal;
};

struct ProjectSymbolResults {
    ProjectSymbolCandidate candidates[kProjectSymbolsMaxCandidates];
    uint32_t count;
    uint64_t totalRecognized;
    uint32_t filesProcessed;
    uint32_t filesWithSymbols;
    uint32_t filesTruncated;
    uint32_t candidatesDropped;
    uint64_t bytesProcessed;
    bool truncated;
    bool incomplete;
    DocumentOutline outlineScratch;
};

void ProjectSymbolResultsInit(ProjectSymbolResults* results);
// The visitor consumes the bounded Project Search file view synchronously.
// It calls the TOOLS9 Document Outline scanner; it owns no input bytes.
bool ProjectSymbolsVisitFile(void* userData, const ProjectSearchScanFile* file);
void ProjectSymbolResultsSort(ProjectSymbolResults* results);
bool ProjectSymbolFilterValid(const char* nameFilter, const char* fileFilter);
bool ProjectSymbolsProjectIdentityMatches(const char* expectedProjectId,
                                         uint64_t expectedGeneration,
                                         const char* expectedRoot,
                                         const char* actualProjectId,
                                         uint64_t actualGeneration,
                                         const char* actualRoot);
// Resolves a relative candidate only when it remains inside the normalized
// active project root. Absolute, drive-prefixed, traversal, and sibling-prefix
// paths are rejected.
bool ProjectSymbolsResolvePath(const char* projectRoot, const char* relativePath,
                               char* absolutePath, uint32_t outputSize);
uint32_t ProjectSymbolFilteredCount(const ProjectSymbolResults* results,
                                    const char* nameFilter, const char* fileFilter);
const ProjectSymbolCandidate* ProjectSymbolFilteredAt(const ProjectSymbolResults* results,
                                                       const char* nameFilter,
                                                       const char* fileFilter,
                                                       uint32_t index);
// Rescans current source bytes and verifies the selected lexical candidate.
// The signature and duplicate ordinal must still match; line changes alone are
// allowed and the current line/offset are returned.
bool ProjectSymbolValidateCurrent(ProjectSymbolResults* results,
                                  const ProjectSymbolCandidate* candidate,
                                  const char* path, const char* data, uint32_t length,
                                  uint32_t* outLine, uint32_t* outOffset);

} // namespace developer_studio
} // namespace guidexos
