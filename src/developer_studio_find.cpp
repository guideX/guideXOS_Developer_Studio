#include "developer_studio_find.h"

namespace guidexos {
namespace developer_studio {

namespace {

static uint32_t textLength(const char* text, uint32_t limit) {
    if (!text) return 0;
    uint32_t length = 0;
    while (length < limit && text[length] != '\0') ++length;
    return length;
}

static void copyText(char* output, uint32_t outputSize, const char* input) {
    if (!output || outputSize == 0) return;
    uint32_t i = 0;
    if (input) {
        while (i + 1 < outputSize && input[i] != '\0') {
            output[i] = input[i];
            ++i;
        }
    }
    output[i] = '\0';
}

static char foldAscii(char value) {
    return value >= 'A' && value <= 'Z' ? static_cast<char>(value + ('a' - 'A')) : value;
}

static bool byteEqual(char left, char right, bool caseSensitive) {
    return caseSensitive ? left == right : foldAscii(left) == foldAscii(right);
}

static uint32_t queryLength(const FindSession* session) {
    return session ? textLength(session->query, kFindMaxQueryBytes + 1u) : 0;
}

static bool isWholeWordMatch(const char* text, uint32_t length, uint64_t start,
                             uint32_t queryLengthValue, bool wholeWord) {
    if (!text || !wholeWord) return true;
    const uint64_t end = start + queryLengthValue;
    if (start > 0 && FindIsAsciiWordByte(text[start - 1])) return false;
    if (end < length && FindIsAsciiWordByte(text[end])) return false;
    return true;
}

static int32_t firstAtOrAfter(const FindSession* session, uint64_t offset) {
    if (!session) return -1;
    uint32_t low = 0;
    uint32_t high = session->matchCount;
    while (low < high) {
        const uint32_t middle = low + (high - low) / 2u;
        if (session->matches[middle].start < offset) low = middle + 1u;
        else high = middle;
    }
    return low < session->matchCount ? static_cast<int32_t>(low) : -1;
}

static int32_t lastBefore(const FindSession* session, uint64_t offset) {
    if (!session || session->matchCount == 0) return -1;
    uint32_t low = 0;
    uint32_t high = session->matchCount;
    while (low < high) {
        const uint32_t middle = low + (high - low) / 2u;
        if (session->matches[middle].start < offset) low = middle + 1u;
        else high = middle;
    }
    return low == 0 ? -1 : static_cast<int32_t>(low - 1u);
}

static bool rangesOverlap(uint64_t leftStart, uint64_t leftEnd,
                          uint64_t rightStart, uint64_t rightEnd) {
    return leftStart < rightEnd && rightStart < leftEnd;
}

} // namespace

const char* FindErrorName(FindErrorCode code) {
    switch (code) {
    case FindErrorCode::None: return "FIND_NONE";
    case FindErrorCode::QueryTooLarge: return "FIND_QUERY_TOO_LARGE";
    case FindErrorCode::ReplacementTooLarge: return "FIND_REPLACEMENT_TOO_LARGE";
    case FindErrorCode::DocumentTooLarge: return "FIND_DOCUMENT_TOO_LARGE";
    case FindErrorCode::MatchLimitReached: return "FIND_MATCH_LIMIT_REACHED";
    case FindErrorCode::StaleSession: return "FIND_STALE_SESSION";
    case FindErrorCode::InvalidRange: return "FIND_INVALID_RANGE";
    case FindErrorCode::NoCurrentMatch: return "FIND_NO_CURRENT_MATCH";
    default: return "FIND_UNKNOWN";
    }
}

void FindDocumentStateInit(FindDocumentState* state) {
    if (!state) return;
    state->query[0] = '\0';
    state->replacement[0] = '\0';
    state->options.caseSensitive = false;
    state->options.wholeWord = false;
    state->options.wrapAround = true;
}

void FindSessionInit(FindSession* session) {
    if (!session) return;
    session->documentId = 0;
    session->documentGeneration = 0;
    session->query[0] = '\0';
    session->replacement[0] = '\0';
    session->options.caseSensitive = false;
    session->options.wholeWord = false;
    session->options.wrapAround = true;
    session->matchCount = 0;
    session->currentMatchIndex = -1;
    session->wrappedLastNavigation = false;
    session->truncated = false;
    session->error = FindErrorCode::None;
}

void FindSessionClearMatches(FindSession* session) {
    if (!session) return;
    session->matchCount = 0;
    session->currentMatchIndex = -1;
    session->wrappedLastNavigation = false;
    session->truncated = false;
    session->error = FindErrorCode::None;
}

bool FindSetQuery(FindSession* session, const char* query) {
    if (!session) return false;
    const uint32_t length = textLength(query, kFindMaxQueryBytes + 2u);
    if (length > kFindMaxQueryBytes) {
        session->error = FindErrorCode::QueryTooLarge;
        return false;
    }
    copyText(session->query, sizeof(session->query), query);
    session->error = FindErrorCode::None;
    return true;
}

bool FindSetReplacement(FindSession* session, const char* replacement) {
    if (!session) return false;
    const uint32_t length = textLength(replacement, kFindMaxReplacementBytes + 2u);
    if (length > kFindMaxReplacementBytes) {
        session->error = FindErrorCode::ReplacementTooLarge;
        return false;
    }
    copyText(session->replacement, sizeof(session->replacement), replacement);
    session->error = FindErrorCode::None;
    return true;
}

void FindSetOptions(FindSession* session, const FindOptions& options) {
    if (!session) return;
    session->options = options;
}

void FindCopyStateToSession(FindSession* session, const FindDocumentState& state) {
    if (!session) return;
    copyText(session->query, sizeof(session->query), state.query);
    copyText(session->replacement, sizeof(session->replacement), state.replacement);
    session->options = state.options;
    session->error = FindErrorCode::None;
}

void FindCopyStateFromSession(FindDocumentState* state, const FindSession& session) {
    if (!state) return;
    copyText(state->query, sizeof(state->query), session.query);
    copyText(state->replacement, sizeof(state->replacement), session.replacement);
    state->options = session.options;
}

bool FindTextMatchesAt(const FindSession* session, const char* text, uint32_t length, uint64_t start) {
    if (!session || !text || start > length) return false;
    const uint32_t needleLength = queryLength(session);
    return FindLiteralMatchesAt(text, length, start, session->query, needleLength,
                                session->options.caseSensitive, session->options.wholeWord);
}

bool FindLiteralMatchesAt(const char* text, uint32_t length, uint64_t start,
                          const char* query, uint32_t queryLengthValue,
                          bool caseSensitive, bool wholeWord) {
    if (!text || !query || start > length || queryLengthValue == 0 ||
        start + queryLengthValue > length) return false;
    for (uint32_t i = 0; i < queryLengthValue; ++i) {
        if (!byteEqual(text[start + i], query[i], caseSensitive)) return false;
    }
    return isWholeWordMatch(text, length, start, queryLengthValue, wholeWord);
}

bool FindSearch(FindSession* session, uint64_t documentId, uint64_t documentGeneration,
                const char* text, uint32_t length) {
    if (!session || (!text && length != 0)) return false;
    session->documentId = documentId;
    session->documentGeneration = documentGeneration;
    session->matchCount = 0;
    session->currentMatchIndex = -1;
    session->wrappedLastNavigation = false;
    session->truncated = false;
    session->error = FindErrorCode::None;
    const uint32_t needleLength = queryLength(session);
    if (needleLength > kFindMaxQueryBytes) {
        session->error = FindErrorCode::QueryTooLarge;
        return false;
    }
    if (length > kFindMaxSearchableDocumentBytes) {
        session->error = FindErrorCode::DocumentTooLarge;
        return false;
    }
    if (needleLength == 0) return true;
    for (uint64_t offset = 0; offset + needleLength <= length;) {
        if (FindTextMatchesAt(session, text, length, offset)) {
            if (session->matchCount >= kFindMaxRetainedMatches) {
                session->truncated = true;
                session->error = FindErrorCode::MatchLimitReached;
                break;
            }
            session->matches[session->matchCount].start = offset;
            session->matches[session->matchCount].length = needleLength;
            ++session->matchCount;
            offset += needleLength;
        } else {
            ++offset;
        }
    }
    return true;
}

bool FindSessionIsStale(const FindSession* session, uint64_t documentId, uint64_t documentGeneration) {
    return !session || session->documentId != documentId || session->documentGeneration != documentGeneration;
}

bool FindMatchTextStillValid(const FindSession* session, const char* text, uint32_t length,
                             const FindMatch& match) {
    if (!session || match.start > length || match.length != queryLength(session) ||
        match.length > length - match.start) return false;
    return FindTextMatchesAt(session, text, length, match.start);
}

int32_t FindSelectInitial(const FindSession* session, uint64_t caretOffset, FindDirection direction) {
    if (!session || session->matchCount == 0) return -1;
    return direction == FindDirection::Forward ? firstAtOrAfter(session, caretOffset) : lastBefore(session, caretOffset);
}

int32_t FindNavigate(FindSession* session, uint64_t caretOffset, FindDirection direction, bool* wrapped) {
    if (wrapped) *wrapped = false;
    if (!session || session->matchCount == 0) return -1;
    int32_t target = -1;
    if (session->currentMatchIndex >= 0 &&
        session->currentMatchIndex < static_cast<int32_t>(session->matchCount)) {
        if (direction == FindDirection::Forward) {
            target = session->currentMatchIndex + 1 < static_cast<int32_t>(session->matchCount)
                ? session->currentMatchIndex + 1 : -1;
            if (target < 0 && session->options.wrapAround) {
                target = 0;
                if (wrapped) *wrapped = true;
            }
        } else {
            target = session->currentMatchIndex > 0 ? session->currentMatchIndex - 1 : -1;
            if (target < 0 && session->options.wrapAround) {
                target = static_cast<int32_t>(session->matchCount - 1u);
                if (wrapped) *wrapped = true;
            }
        }
    } else {
        target = FindSelectInitial(session, caretOffset, direction);
        if (target < 0 && session->options.wrapAround) {
            target = direction == FindDirection::Forward ? 0 : static_cast<int32_t>(session->matchCount - 1u);
            if (wrapped) *wrapped = true;
        }
    }
    if (target >= 0) session->currentMatchIndex = target;
    session->wrappedLastNavigation = wrapped && *wrapped;
    return target;
}

bool FindSetCurrentMatch(FindSession* session, int32_t index) {
    if (!session || index < 0 || index >= static_cast<int32_t>(session->matchCount)) {
        if (session) {
            session->currentMatchIndex = -1;
            session->error = FindErrorCode::NoCurrentMatch;
        }
        return false;
    }
    session->currentMatchIndex = index;
    session->wrappedLastNavigation = false;
    session->error = FindErrorCode::None;
    return true;
}

const FindMatch* FindCurrentMatch(const FindSession* session) {
    if (!session || session->currentMatchIndex < 0 ||
        session->currentMatchIndex >= static_cast<int32_t>(session->matchCount)) return nullptr;
    return &session->matches[session->currentMatchIndex];
}

uint32_t FindVisibleMatchIndices(const FindSession* session, uint64_t lineStart,
                                 uint64_t lineEnd, uint32_t* indices, uint32_t capacity) {
    if (!session || !indices || capacity == 0 || lineEnd <= lineStart || session->matchCount == 0) return 0;
    uint32_t low = 0;
    uint32_t high = session->matchCount;
    while (low < high) {
        const uint32_t middle = low + (high - low) / 2u;
        const FindMatch& match = session->matches[middle];
        if (match.start + match.length <= lineStart) low = middle + 1u;
        else high = middle;
    }
    uint32_t written = 0;
    for (uint32_t i = low; i < session->matchCount; ++i) {
        const FindMatch& match = session->matches[i];
        if (match.start >= lineEnd) break;
        if (rangesOverlap(match.start, match.start + match.length, lineStart, lineEnd)) {
            if (written >= capacity) break;
            indices[written++] = i;
        }
    }
    return written;
}

bool FindCanReplaceAll(const FindSession* session) {
    return session && !session->truncated && session->error == FindErrorCode::None && session->matchCount > 0;
}

bool FindIsAsciiWordByte(char value) {
    return (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z') ||
        (value >= '0' && value <= '9') || value == '_';
}

} // namespace developer_studio
} // namespace guidexos
