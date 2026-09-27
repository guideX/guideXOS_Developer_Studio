#include "developer_studio_startup.h"

#include <stddef.h>

namespace guidexos {
namespace developer_studio {
namespace {

static void clearBytes(void* value, size_t size) {
    unsigned char* bytes = static_cast<unsigned char*>(value);
    for (size_t i = 0; i < size; ++i) bytes[i] = 0;
}

static bool copyText(char* destination, uint32_t capacity, const char* source) {
    if (!destination || capacity == 0 || !source || !source[0]) return false;
    uint32_t length = 0;
    while (source[length]) {
        if (length + 1 >= capacity) return false;
        ++length;
    }
    for (uint32_t i = 0; i <= length; ++i) destination[i] = source[i];
    return true;
}

static bool ownerMatches(const DiagnosticStartupOwner* owner, uint64_t startupGeneration) {
    return owner && owner->state != DiagnosticStartupState::Uninitialized &&
        startupGeneration != 0 && owner->startupGeneration == startupGeneration;
}

static bool isTerminal(DiagnosticStartupState state) {
    return state == DiagnosticStartupState::NoDiagnosticFixture ||
        state == DiagnosticStartupState::RequestAccepted ||
        state == DiagnosticStartupState::RequestRejected;
}

static bool sentinelIsTerminal(DiagnosticSentinelState state) {
    return state == DiagnosticSentinelState::Present ||
        state == DiagnosticSentinelState::Absent ||
        state == DiagnosticSentinelState::Error;
}

static uint32_t textLength(const char* text) {
    uint32_t length = 0;
    if (text) while (text[length]) ++length;
    return length;
}

static bool textEqualsBytes(const char* text, const char* bytes, uint32_t count) {
    if (!text || !bytes || textLength(text) != count) return false;
    for (uint32_t i = 0; i < count; ++i) if (text[i] != bytes[i]) return false;
    return true;
}

static void setSentinelResult(DiagnosticSentinelDetection* detection,
                              DiagnosticSentinelState state,
                              DiagnosticSentinelReason reason) {
    detection->state = state;
    detection->reason = reason;
}

} // namespace

void DiagnosticStartupOwnerInit(DiagnosticStartupOwner* owner) {
    if (!owner) return;
    clearBytes(owner, sizeof(*owner));
    owner->state = DiagnosticStartupState::Uninitialized;
}

bool DiagnosticStartupBegin(DiagnosticStartupOwner* owner, uint64_t applicationInstanceId,
                            uint64_t startupGeneration) {
    if (!owner || applicationInstanceId == 0 || startupGeneration == 0) return false;
    if (owner->state != DiagnosticStartupState::Uninitialized && !isTerminal(owner->state)) return false;
    clearBytes(owner, sizeof(*owner));
    owner->applicationInstanceId = applicationInstanceId;
    owner->startupGeneration = startupGeneration;
    owner->state = DiagnosticStartupState::Initializing;
    return true;
}

bool DiagnosticStartupResolveFixture(DiagnosticStartupOwner* owner, uint64_t startupGeneration,
                                     bool recognized) {
    if (!ownerMatches(owner, startupGeneration) ||
        owner->state != DiagnosticStartupState::Initializing) return false;
    owner->diagnosticFixtureRecognized = recognized;
    owner->state = recognized ? DiagnosticStartupState::DiagnosticFixtureRecognized :
        DiagnosticStartupState::NoDiagnosticFixture;
    return true;
}

bool DiagnosticStartupMarkUiReady(DiagnosticStartupOwner* owner, uint64_t startupGeneration) {
    if (!ownerMatches(owner, startupGeneration) ||
        owner->state != DiagnosticStartupState::DiagnosticFixtureRecognized) return false;
    owner->state = DiagnosticStartupState::UiReady;
    return true;
}

bool DiagnosticStartupMarkPumpEntered(DiagnosticStartupOwner* owner, uint64_t startupGeneration) {
    if (!ownerMatches(owner, startupGeneration) ||
        owner->state != DiagnosticStartupState::UiReady) return false;
    owner->state = DiagnosticStartupState::PumpEntered;
    return true;
}

bool DiagnosticStartupConstructProjectRequest(DiagnosticStartupOwner* owner,
                                              uint64_t startupGeneration,
                                              const char* projectPath,
                                              uint64_t* requestId) {
    if (requestId) *requestId = 0;
    if (!ownerMatches(owner, startupGeneration) || !requestId ||
        owner->state != DiagnosticStartupState::PumpEntered) return false;
    owner->request.requestId = startupGeneration;
    owner->request.startupGeneration = startupGeneration;
    owner->request.projectLifecycleRequestId = 0;
    if (!copyText(owner->request.projectPath, sizeof(owner->request.projectPath), projectPath)) {
        clearBytes(&owner->request, sizeof(owner->request));
        return false;
    }
    *requestId = owner->request.requestId;
    owner->state = DiagnosticStartupState::RequestPending;
    return true;
}

bool DiagnosticStartupMarkRequestSubmitted(DiagnosticStartupOwner* owner,
                                           uint64_t startupGeneration,
                                           uint64_t requestId) {
    if (!ownerMatches(owner, startupGeneration) || requestId == 0 ||
        owner->state != DiagnosticStartupState::RequestPending ||
        owner->request.requestId != requestId ||
        owner->request.startupGeneration != startupGeneration) return false;
    owner->state = DiagnosticStartupState::RequestSubmitted;
    return true;
}

bool DiagnosticStartupObserveProjectAccepted(DiagnosticStartupOwner* owner,
                                             uint64_t startupGeneration,
                                             uint64_t requestId,
                                             uint64_t projectLifecycleRequestId) {
    if (!ownerMatches(owner, startupGeneration) || requestId == 0 ||
        projectLifecycleRequestId == 0 ||
        owner->state != DiagnosticStartupState::RequestSubmitted ||
        owner->request.requestId != requestId ||
        owner->request.startupGeneration != startupGeneration) return false;
    owner->request.projectLifecycleRequestId = projectLifecycleRequestId;
    owner->state = DiagnosticStartupState::RequestAccepted;
    return true;
}

bool DiagnosticStartupRejectProjectRequest(DiagnosticStartupOwner* owner,
                                           uint64_t startupGeneration,
                                           uint64_t requestId) {
    if (!ownerMatches(owner, startupGeneration) || requestId == 0 ||
        (owner->state != DiagnosticStartupState::RequestPending &&
         owner->state != DiagnosticStartupState::RequestSubmitted) ||
        owner->request.requestId != requestId ||
        owner->request.startupGeneration != startupGeneration) return false;
    owner->state = DiagnosticStartupState::RequestRejected;
    return true;
}

const char* DiagnosticStartupStateName(DiagnosticStartupState state) {
    switch (state) {
    case DiagnosticStartupState::Uninitialized: return "uninitialized";
    case DiagnosticStartupState::Initializing: return "initializing";
    case DiagnosticStartupState::NoDiagnosticFixture: return "no_fixture";
    case DiagnosticStartupState::DiagnosticFixtureRecognized: return "fixture_recognized";
    case DiagnosticStartupState::UiReady: return "ui_ready";
    case DiagnosticStartupState::PumpEntered: return "pump_entered";
    case DiagnosticStartupState::RequestPending: return "request_pending";
    case DiagnosticStartupState::RequestSubmitted: return "request_submitted";
    case DiagnosticStartupState::RequestAccepted: return "request_accepted";
    case DiagnosticStartupState::RequestRejected: return "request_rejected";
    }
    return "unknown";
}

void DiagnosticSentinelDetectionBegin(DiagnosticSentinelDetection* detection,
                                      uint64_t startupGeneration) {
    if (!detection) return;
    clearBytes(detection, sizeof(*detection));
    detection->startupGeneration = startupGeneration;
    detection->state = startupGeneration ? DiagnosticSentinelState::WaitingForFilesystem :
        DiagnosticSentinelState::Uninitialized;
    detection->reason = startupGeneration ? DiagnosticSentinelReason::FilesystemNotReady :
        DiagnosticSentinelReason::None;
}

bool DiagnosticSentinelNormalizePath(const char* path, char* output, uint32_t capacity) {
    if (!path || !output || capacity < 2 || path[0] != '/') return false;
    uint32_t input = 0;
    uint32_t out = 0;
    bool previousSeparator = false;
    output[0] = '\0';
    while (path[input]) {
        char ch = path[input++];
        if (ch == '\\') ch = '/';
        if (ch == '/') {
            if (previousSeparator) continue;
            previousSeparator = true;
            if (out + 1 >= capacity) return false;
            output[out++] = '/';
            continue;
        }
        previousSeparator = false;
        if (out + 1 >= capacity) return false;
        output[out++] = ch;
    }
    while (out > 1 && output[out - 1] == '/') --out;
    output[out] = '\0';
    if (out < 2) return false;

    uint32_t componentStart = 1;
    for (uint32_t i = 1; i <= out; ++i) {
        if (i != out && output[i] != '/') continue;
        const uint32_t componentLength = i - componentStart;
        if ((componentLength == 1 && output[componentStart] == '.') ||
            (componentLength == 2 && output[componentStart] == '.' && output[componentStart + 1] == '.')) {
            output[0] = '\0';
            return false;
        }
        componentStart = i + 1;
    }
    return true;
}

bool DiagnosticSentinelDetectionEvaluate(DiagnosticSentinelDetection* detection,
                                         uint64_t startupGeneration,
                                         bool filesystemReady,
                                         DiagnosticSentinelMountState mountState,
                                         const DiagnosticSentinelIo* io) {
    if (!detection || startupGeneration == 0 ||
        detection->startupGeneration != startupGeneration ||
        detection->state == DiagnosticSentinelState::Uninitialized) return false;
    if (sentinelIsTerminal(detection->state)) return true;
    if (!filesystemReady) {
        setSentinelResult(detection, DiagnosticSentinelState::WaitingForFilesystem,
                          DiagnosticSentinelReason::FilesystemNotReady);
        return true;
    }
    if (mountState == DiagnosticSentinelMountState::NotReady) {
        setSentinelResult(detection, DiagnosticSentinelState::WaitingForFilesystem,
                          DiagnosticSentinelReason::MountNotReady);
        return true;
    }
    if (mountState == DiagnosticSentinelMountState::WrongMount) {
        setSentinelResult(detection, DiagnosticSentinelState::Error,
                          DiagnosticSentinelReason::WrongMount);
        return true;
    }
    if (!DiagnosticSentinelNormalizePath(GUIDEXOS_PHASE28Q_DIAGNOSTIC_SENTINEL_PATH,
                                         detection->normalizedPath,
                                         sizeof(detection->normalizedPath))) {
        setSentinelResult(detection, DiagnosticSentinelState::Error,
                          DiagnosticSentinelReason::PathInvalid);
        return true;
    }
    detection->pathNormalized = true;
    if (!io || !io->stat || !io->read) {
        setSentinelResult(detection, DiagnosticSentinelState::Error,
                          DiagnosticSentinelReason::IoError);
        return true;
    }

    bool regularFile = false;
    uint64_t fileSize = 0;
    const DiagnosticSentinelIoResult statResult = io->stat(
        io->userData, detection->normalizedPath, &regularFile, &fileSize);
    if (statResult == DiagnosticSentinelIoResult::NotFound) {
        setSentinelResult(detection, DiagnosticSentinelState::Absent,
                          DiagnosticSentinelReason::NotFound);
        return true;
    }
    if (statResult != DiagnosticSentinelIoResult::Found) {
        setSentinelResult(detection, DiagnosticSentinelState::Error,
            statResult == DiagnosticSentinelIoResult::MountUnavailable ? DiagnosticSentinelReason::MountNotReady :
            (statResult == DiagnosticSentinelIoResult::InvalidPath ? DiagnosticSentinelReason::PathInvalid :
             DiagnosticSentinelReason::IoError));
        return true;
    }

    if (!regularFile) {
        setSentinelResult(detection, DiagnosticSentinelState::Error,
                          DiagnosticSentinelReason::NotRegularFile);
        return true;
    }
    const uint32_t expectedBytes = textLength(GUIDEXOS_PHASE28Q_DIAGNOSTIC_SENTINEL_CONTENT);
    if (fileSize != expectedBytes) {
        setSentinelResult(detection, DiagnosticSentinelState::Error,
                          DiagnosticSentinelReason::InvalidContent);
        return true;
    }

    char contents[64] = {};
    uint32_t bytesRead = 0;
    const DiagnosticSentinelIoResult readResult = io->read(
        io->userData, detection->normalizedPath, contents, sizeof(contents) - 1, &bytesRead);
    if (readResult != DiagnosticSentinelIoResult::Found) {
        setSentinelResult(detection, DiagnosticSentinelState::Error,
            readResult == DiagnosticSentinelIoResult::MountUnavailable ? DiagnosticSentinelReason::MountNotReady :
            (readResult == DiagnosticSentinelIoResult::InvalidPath ? DiagnosticSentinelReason::PathInvalid :
             DiagnosticSentinelReason::IoError));
        return true;
    }
    if (!textEqualsBytes(contents, GUIDEXOS_PHASE28Q_DIAGNOSTIC_SENTINEL_CONTENT, bytesRead)) {
        setSentinelResult(detection, DiagnosticSentinelState::Error,
                          DiagnosticSentinelReason::InvalidContent);
        return true;
    }
    setSentinelResult(detection, DiagnosticSentinelState::Present,
                      DiagnosticSentinelReason::None);
    return true;
}

const char* DiagnosticSentinelStateName(DiagnosticSentinelState state) {
    switch (state) {
    case DiagnosticSentinelState::Uninitialized: return "unknown";
    case DiagnosticSentinelState::WaitingForFilesystem: return "waiting_for_filesystem";
    case DiagnosticSentinelState::Present: return "present";
    case DiagnosticSentinelState::Absent: return "absent";
    case DiagnosticSentinelState::Error: return "error";
    }
    return "unknown";
}

const char* DiagnosticSentinelReasonName(DiagnosticSentinelReason reason) {
    switch (reason) {
    case DiagnosticSentinelReason::None: return "none";
    case DiagnosticSentinelReason::FilesystemNotReady: return "SENTINEL_FS_NOT_READY";
    case DiagnosticSentinelReason::MountNotReady: return "SENTINEL_MOUNT_NOT_READY";
    case DiagnosticSentinelReason::WrongMount: return "SENTINEL_WRONG_MOUNT";
    case DiagnosticSentinelReason::PathInvalid: return "SENTINEL_PATH_INVALID";
    case DiagnosticSentinelReason::NotFound: return "SENTINEL_NOT_FOUND";
    case DiagnosticSentinelReason::IoError: return "SENTINEL_IO_ERROR";
    case DiagnosticSentinelReason::NotRegularFile: return "SENTINEL_NOT_REGULAR_FILE";
    case DiagnosticSentinelReason::InvalidContent: return "SENTINEL_INVALID_CONTENT";
    }
    return "SENTINEL_UNKNOWN_ERROR";
}

const char* DiagnosticSentinelIoResultName(DiagnosticSentinelIoResult result) {
    switch (result) {
    case DiagnosticSentinelIoResult::Found: return "found";
    case DiagnosticSentinelIoResult::NotFound: return "not_found";
    case DiagnosticSentinelIoResult::MountUnavailable: return "mount_unavailable";
    case DiagnosticSentinelIoResult::InvalidPath: return "invalid_path";
    case DiagnosticSentinelIoResult::IoError: return "io_error";
    }
    return "unknown";
}

} // namespace developer_studio
} // namespace guidexos
