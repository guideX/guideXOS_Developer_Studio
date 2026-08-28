#pragma once

#include <stdint.h>

namespace guidexos {
namespace developer_studio {

// Find and editor offsets are UTF-8 byte offsets.  Matching is deliberately
// literal and byte-oriented; case folding only affects ASCII A-Z bytes.
#if defined(GXOS_DEVELOPER_STUDIO_BARE_METAL)
static const uint32_t kFindMaxQueryBytes = 256u;
static const uint32_t kFindMaxReplacementBytes = 1024u;
static const uint32_t kFindMaxRetainedMatches = 256u;
static const uint32_t kFindMaxSearchableDocumentBytes = 16u * 1024u;
#else
static const uint32_t kFindMaxQueryBytes = 1024u;
static const uint32_t kFindMaxReplacementBytes = 16384u;
static const uint32_t kFindMaxRetainedMatches = 20000u;
static const uint32_t kFindMaxSearchableDocumentBytes = 8u * 1024u * 1024u;
#endif

enum class FindDirection {
    Forward = 0,
    Backward
};

enum class FindErrorCode {
    None = 0,
    QueryTooLarge,
    ReplacementTooLarge,
    DocumentTooLarge,
    MatchLimitReached,
    StaleSession,
    InvalidRange,
    NoCurrentMatch
};

struct FindOptions {
    bool caseSensitive;
    bool wholeWord;
    bool wrapAround;
};

struct FindMatch {
    uint64_t start;
    uint64_t length;
};

// Per-document state is intentionally small enough to live beside the
// existing bounded Document model.  It contains no pointers into text or UI.
struct FindDocumentState {
    char query[kFindMaxQueryBytes + 1];
    char replacement[kFindMaxReplacementBytes + 1];
    FindOptions options;
};

// A session owns retained results and refers to a document only by its stable
// ID and text generation.  It never retains a Document* or a TextBuffer*.
struct FindSession {
    uint64_t documentId;
    uint64_t documentGeneration;
    char query[kFindMaxQueryBytes + 1];
    char replacement[kFindMaxReplacementBytes + 1];
    FindOptions options;
    FindMatch matches[kFindMaxRetainedMatches];
    uint32_t matchCount;
    int32_t currentMatchIndex;
    bool wrappedLastNavigation;
    bool truncated;
    FindErrorCode error;
};

const char* FindErrorName(FindErrorCode code);

void FindDocumentStateInit(FindDocumentState* state);
void FindSessionInit(FindSession* session);
void FindSessionClearMatches(FindSession* session);

bool FindSetQuery(FindSession* session, const char* query);
bool FindSetReplacement(FindSession* session, const char* replacement);
void FindSetOptions(FindSession* session, const FindOptions& options);
void FindCopyStateToSession(FindSession* session, const FindDocumentState& state);
void FindCopyStateFromSession(FindDocumentState* state, const FindSession& session);

bool FindSearch(FindSession* session, uint64_t documentId, uint64_t documentGeneration,
                const char* text, uint32_t length);
bool FindSessionIsStale(const FindSession* session, uint64_t documentId, uint64_t documentGeneration);
bool FindMatchTextStillValid(const FindSession* session, const char* text, uint32_t length,
                             const FindMatch& match);

int32_t FindSelectInitial(const FindSession* session, uint64_t caretOffset,
                          FindDirection direction);
int32_t FindNavigate(FindSession* session, uint64_t caretOffset, FindDirection direction,
                     bool* wrapped);
bool FindSetCurrentMatch(FindSession* session, int32_t index);
const FindMatch* FindCurrentMatch(const FindSession* session);

// Returns the number of retained match indices that overlap [lineStart,lineEnd).
// The binary-search entry point means callers do not scan all off-screen
// matches for every rendered line.
uint32_t FindVisibleMatchIndices(const FindSession* session, uint64_t lineStart,
                                 uint64_t lineEnd, uint32_t* indices, uint32_t capacity);

bool FindCanReplaceAll(const FindSession* session);
bool FindIsAsciiWordByte(char value);
bool FindTextMatchesAt(const FindSession* session, const char* text, uint32_t length,
                       uint64_t start);

// Shared byte-oriented literal matching primitive.  Project search uses this
// directly so it does not need to construct a per-document FindSession.
bool FindLiteralMatchesAt(const char* text, uint32_t length, uint64_t start,
                          const char* query, uint32_t queryLength,
                          bool caseSensitive, bool wholeWord);

} // namespace developer_studio
} // namespace guidexos
