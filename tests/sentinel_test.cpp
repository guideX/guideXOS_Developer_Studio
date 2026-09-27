#include "developer_studio_startup.h"

#include <cassert>
#include <cstring>
#include <iostream>

using namespace guidexos::developer_studio;

struct FakeFileSystem {
    DiagnosticSentinelIoResult statResult;
    DiagnosticSentinelIoResult readResult;
    bool regularFile;
    uint64_t size;
    char contents[64];
    uint32_t statCalls;
    uint32_t readCalls;
};

static DiagnosticSentinelIoResult fakeStat(void* userData, const char* path,
                                           bool* outRegularFile, uint64_t* outSize) {
    FakeFileSystem* fs = static_cast<FakeFileSystem*>(userData);
    ++fs->statCalls;
    assert(std::strcmp(path, GUIDEXOS_PHASE28Q_DIAGNOSTIC_SENTINEL_PATH) == 0);
    if (fs->statResult == DiagnosticSentinelIoResult::Found) {
        *outRegularFile = fs->regularFile;
        *outSize = fs->size;
    }
    return fs->statResult;
}

static DiagnosticSentinelIoResult fakeRead(void* userData, const char* path,
                                           char* buffer, uint32_t capacity, uint32_t* outBytes) {
    FakeFileSystem* fs = static_cast<FakeFileSystem*>(userData);
    ++fs->readCalls;
    assert(std::strcmp(path, GUIDEXOS_PHASE28Q_DIAGNOSTIC_SENTINEL_PATH) == 0);
    *outBytes = 0;
    if (fs->readResult != DiagnosticSentinelIoResult::Found) return fs->readResult;
    uint32_t count = 0;
    while (fs->contents[count] && count < capacity) {
        buffer[count] = fs->contents[count];
        ++count;
    }
    *outBytes = count;
    return fs->readResult;
}

static FakeFileSystem makePresentFileSystem() {
    FakeFileSystem fs = {};
    fs.statResult = DiagnosticSentinelIoResult::Found;
    fs.readResult = DiagnosticSentinelIoResult::Found;
    fs.regularFile = true;
    fs.size = sizeof(GUIDEXOS_PHASE28Q_DIAGNOSTIC_SENTINEL_CONTENT) - 1;
    std::strcpy(fs.contents, GUIDEXOS_PHASE28Q_DIAGNOSTIC_SENTINEL_CONTENT);
    return fs;
}

static DiagnosticSentinelIo makeIo(FakeFileSystem* fs) {
    DiagnosticSentinelIo io = { fs, fakeStat, fakeRead };
    return io;
}

static void testNotReadyIsPendingAndDoesNotLookup() {
    DiagnosticSentinelDetection detection = {};
    DiagnosticSentinelDetectionBegin(&detection, 101);
    FakeFileSystem fs = makePresentFileSystem();
    DiagnosticSentinelIo io = makeIo(&fs);
    assert(DiagnosticSentinelDetectionEvaluate(&detection, 101, false,
        DiagnosticSentinelMountState::NotReady, &io));
    assert(detection.state == DiagnosticSentinelState::WaitingForFilesystem);
    assert(detection.reason == DiagnosticSentinelReason::FilesystemNotReady);
    assert(fs.statCalls == 0 && fs.readCalls == 0);

    assert(DiagnosticSentinelDetectionEvaluate(&detection, 101, true,
        DiagnosticSentinelMountState::Ready, &io));
    assert(detection.state == DiagnosticSentinelState::Present);
    assert(fs.statCalls == 1 && fs.readCalls == 1);
}

static void testCorrectMountPresentAndPositiveLatch() {
    DiagnosticSentinelDetection detection = {};
    DiagnosticSentinelDetectionBegin(&detection, 102);
    FakeFileSystem fs = makePresentFileSystem();
    DiagnosticSentinelIo io = makeIo(&fs);
    assert(DiagnosticSentinelDetectionEvaluate(&detection, 102, true,
        DiagnosticSentinelMountState::Ready, &io));
    assert(detection.state == DiagnosticSentinelState::Present);
    assert(detection.reason == DiagnosticSentinelReason::None);
    assert(DiagnosticSentinelDetectionEvaluate(&detection, 102, true,
        DiagnosticSentinelMountState::WrongMount, &io));
    assert(detection.state == DiagnosticSentinelState::Present);
    assert(fs.statCalls == 1 && fs.readCalls == 1);
}

static void testReadyMountNotFoundIsFinalAbsent() {
    DiagnosticSentinelDetection detection = {};
    DiagnosticSentinelDetectionBegin(&detection, 103);
    FakeFileSystem fs = makePresentFileSystem();
    fs.statResult = DiagnosticSentinelIoResult::NotFound;
    DiagnosticSentinelIo io = makeIo(&fs);
    assert(DiagnosticSentinelDetectionEvaluate(&detection, 103, true,
        DiagnosticSentinelMountState::Ready, &io));
    assert(detection.state == DiagnosticSentinelState::Absent);
    assert(detection.reason == DiagnosticSentinelReason::NotFound);
    assert(fs.statCalls == 1 && fs.readCalls == 0);
}

static void testWrongMountCannotProducePositive() {
    DiagnosticSentinelDetection detection = {};
    DiagnosticSentinelDetectionBegin(&detection, 104);
    FakeFileSystem fs = makePresentFileSystem();
    DiagnosticSentinelIo io = makeIo(&fs);
    assert(DiagnosticSentinelDetectionEvaluate(&detection, 104, true,
        DiagnosticSentinelMountState::WrongMount, &io));
    assert(detection.state == DiagnosticSentinelState::Error);
    assert(detection.reason == DiagnosticSentinelReason::WrongMount);
    assert(fs.statCalls == 0 && fs.readCalls == 0);
}

static void testCanonicalPathNormalizationAndValidation() {
    char normalized[96] = {};
    assert(DiagnosticSentinelNormalizePath("//Apps\\DeveloperStudio//.phase28q-diagnostic/",
        normalized, sizeof(normalized)));
    assert(std::strcmp(normalized, GUIDEXOS_PHASE28Q_DIAGNOSTIC_SENTINEL_PATH) == 0);
    assert(!DiagnosticSentinelNormalizePath("Apps/DeveloperStudio/.phase28q-diagnostic",
        normalized, sizeof(normalized)));
    assert(!DiagnosticSentinelNormalizePath("/Apps/../.phase28q-diagnostic",
        normalized, sizeof(normalized)));

    DiagnosticSentinelDetection detection = {};
    DiagnosticSentinelDetectionBegin(&detection, 105);
    FakeFileSystem fs = makePresentFileSystem();
    DiagnosticSentinelIo io = makeIo(&fs);
    assert(DiagnosticSentinelDetectionEvaluate(&detection, 105, true,
        DiagnosticSentinelMountState::Ready, &io));
    assert(detection.pathNormalized);
    assert(std::strcmp(detection.normalizedPath, GUIDEXOS_PHASE28Q_DIAGNOSTIC_SENTINEL_PATH) == 0);
}

static void testStaleGenerationCannotPublishAndRelaunchIsFresh() {
    DiagnosticSentinelDetection detection = {};
    DiagnosticSentinelDetectionBegin(&detection, 106);
    FakeFileSystem fs = makePresentFileSystem();
    DiagnosticSentinelIo io = makeIo(&fs);
    assert(!DiagnosticSentinelDetectionEvaluate(&detection, 105, true,
        DiagnosticSentinelMountState::Ready, &io));
    assert(detection.state == DiagnosticSentinelState::WaitingForFilesystem);
    assert(fs.statCalls == 0);

    assert(DiagnosticSentinelDetectionEvaluate(&detection, 106, true,
        DiagnosticSentinelMountState::Ready, &io));
    assert(detection.state == DiagnosticSentinelState::Present);
    DiagnosticSentinelDetectionBegin(&detection, 107);
    assert(detection.startupGeneration == 107);
    assert(detection.state == DiagnosticSentinelState::WaitingForFilesystem);
    assert(detection.reason == DiagnosticSentinelReason::FilesystemNotReady);
}

static void testMountWaitAndExplicitFilesystemErrors() {
    DiagnosticSentinelDetection detection = {};
    DiagnosticSentinelDetectionBegin(&detection, 108);
    FakeFileSystem fs = makePresentFileSystem();
    DiagnosticSentinelIo io = makeIo(&fs);
    assert(DiagnosticSentinelDetectionEvaluate(&detection, 108, true,
        DiagnosticSentinelMountState::NotReady, &io));
    assert(detection.state == DiagnosticSentinelState::WaitingForFilesystem);
    assert(detection.reason == DiagnosticSentinelReason::MountNotReady);
    assert(fs.statCalls == 0);

    assert(DiagnosticSentinelDetectionEvaluate(&detection, 108, true,
        DiagnosticSentinelMountState::Ready, &io));
    assert(detection.state == DiagnosticSentinelState::Present);

    DiagnosticSentinelDetectionBegin(&detection, 109);
    fs = makePresentFileSystem();
    fs.statResult = DiagnosticSentinelIoResult::IoError;
    io = makeIo(&fs);
    assert(DiagnosticSentinelDetectionEvaluate(&detection, 109, true,
        DiagnosticSentinelMountState::Ready, &io));
    assert(detection.state == DiagnosticSentinelState::Error);
    assert(detection.reason == DiagnosticSentinelReason::IoError);
}

static void testInvalidStatReadAndContentResults() {
    DiagnosticSentinelDetection detection = {};
    FakeFileSystem fs = makePresentFileSystem();
    DiagnosticSentinelIo io = makeIo(&fs);

    DiagnosticSentinelDetectionBegin(&detection, 110);
    fs.statResult = DiagnosticSentinelIoResult::InvalidPath;
    assert(DiagnosticSentinelDetectionEvaluate(&detection, 110, true,
        DiagnosticSentinelMountState::Ready, &io));
    assert(detection.state == DiagnosticSentinelState::Error);
    assert(detection.reason == DiagnosticSentinelReason::PathInvalid);

    DiagnosticSentinelDetectionBegin(&detection, 111);
    fs = makePresentFileSystem(); fs.regularFile = false; io = makeIo(&fs);
    assert(DiagnosticSentinelDetectionEvaluate(&detection, 111, true,
        DiagnosticSentinelMountState::Ready, &io));
    assert(detection.state == DiagnosticSentinelState::Error);
    assert(detection.reason == DiagnosticSentinelReason::NotRegularFile);

    DiagnosticSentinelDetectionBegin(&detection, 112);
    fs = makePresentFileSystem(); fs.readResult = DiagnosticSentinelIoResult::IoError; io = makeIo(&fs);
    assert(DiagnosticSentinelDetectionEvaluate(&detection, 112, true,
        DiagnosticSentinelMountState::Ready, &io));
    assert(detection.state == DiagnosticSentinelState::Error);
    assert(detection.reason == DiagnosticSentinelReason::IoError);

    DiagnosticSentinelDetectionBegin(&detection, 113);
    fs = makePresentFileSystem(); std::strcpy(fs.contents, "wrong-content"); io = makeIo(&fs);
    fs.size = std::strlen(fs.contents);
    assert(DiagnosticSentinelDetectionEvaluate(&detection, 113, true,
        DiagnosticSentinelMountState::Ready, &io));
    assert(detection.state == DiagnosticSentinelState::Error);
    assert(detection.reason == DiagnosticSentinelReason::InvalidContent);
}

static void testDetectionFeedsSingleStartupRequestOwner() {
    DiagnosticSentinelDetection detection = {};
    DiagnosticSentinelDetectionBegin(&detection, 114);
    FakeFileSystem fs = makePresentFileSystem();
    DiagnosticSentinelIo io = makeIo(&fs);
    assert(DiagnosticSentinelDetectionEvaluate(&detection, 114, true,
        DiagnosticSentinelMountState::Ready, &io));

    DiagnosticStartupOwner owner = {};
    DiagnosticStartupOwnerInit(&owner);
    assert(DiagnosticStartupBegin(&owner, 5, 114));
    assert(DiagnosticStartupResolveFixture(&owner, 114,
        detection.state == DiagnosticSentinelState::Present));
    assert(DiagnosticStartupMarkUiReady(&owner, 114));
    assert(DiagnosticStartupMarkPumpEntered(&owner, 114));
    uint64_t requestId = 0;
    assert(DiagnosticStartupConstructProjectRequest(&owner, 114, "/P28Q", &requestId));
    uint64_t duplicateRequestId = 0;
    assert(!DiagnosticStartupConstructProjectRequest(&owner, 114, "/P28Q", &duplicateRequestId));
    assert(requestId == 114 && duplicateRequestId == 0);
}

int main() {
    testNotReadyIsPendingAndDoesNotLookup();
    testCorrectMountPresentAndPositiveLatch();
    testReadyMountNotFoundIsFinalAbsent();
    testWrongMountCannotProducePositive();
    testCanonicalPathNormalizationAndValidation();
    testStaleGenerationCannotPublishAndRelaunchIsFresh();
    testMountWaitAndExplicitFilesystemErrors();
    testInvalidStatReadAndContentResults();
    testDetectionFeedsSingleStartupRequestOwner();
    std::cout << "Developer Studio Phase 29I sentinel detection test passed (12 focused cases)\n";
    return 0;
}
