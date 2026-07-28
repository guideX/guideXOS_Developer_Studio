#include <guidexos/ui.h>

#include "developer_studio_models.h"
#include "developer_studio_build.h"
#include "developer_studio_run.h"
#include "developer_studio_output.h"
#include "developer_studio_workspace.h"
#include "developer_studio_project_search.h"
#include "developer_studio_symbols.h"

namespace {

using guidexos::developer_studio::CloseDecision;
using guidexos::developer_studio::BuildController;
using guidexos::developer_studio::BuildControllerInit;
using guidexos::developer_studio::BuildControllerIsActive;
using guidexos::developer_studio::BuildControllerPoll;
using guidexos::developer_studio::BuildControllerStart;
using guidexos::developer_studio::BuildDirtyDecision;
using guidexos::developer_studio::BuildErrorCode;
using guidexos::developer_studio::BuildErrorName;
using guidexos::developer_studio::BuildResult;
using guidexos::developer_studio::BuildState;
using guidexos::developer_studio::BuildStateName;
using guidexos::developer_studio::HostedBuildService;
using guidexos::developer_studio::HostedDevelopmentRunService;
using guidexos::developer_studio::RunController;
using guidexos::developer_studio::RunControllerInit;
using guidexos::developer_studio::RunControllerIsActive;
using guidexos::developer_studio::RunControllerPoll;
using guidexos::developer_studio::RunControllerPrepare;
using guidexos::developer_studio::RunControllerRequestClose;
using guidexos::developer_studio::RunControllerStart;
using guidexos::developer_studio::RunErrorCode;
using guidexos::developer_studio::RunErrorName;
using guidexos::developer_studio::RunRequest;
using guidexos::developer_studio::RunResult;
using guidexos::developer_studio::RunState;
using guidexos::developer_studio::RunStateName;
using guidexos::developer_studio::OutputCategory;
using guidexos::developer_studio::OutputChannel;
using guidexos::developer_studio::OutputErrorCode;
using guidexos::developer_studio::OutputOperationType;
using guidexos::developer_studio::OutputRecord;
using guidexos::developer_studio::OutputService;
using guidexos::developer_studio::OutputServiceActiveChannel;
using guidexos::developer_studio::OutputServiceAppendText;
using guidexos::developer_studio::OutputServiceBeginOperation;
using guidexos::developer_studio::OutputServiceClearChannel;
using guidexos::developer_studio::OutputServiceCompleteOperation;
using guidexos::developer_studio::OutputServiceFilteredAt;
using guidexos::developer_studio::OutputServiceFilteredCount;
using guidexos::developer_studio::OutputServiceInit;
using guidexos::developer_studio::OutputServiceProblemAt;
using guidexos::developer_studio::OutputServiceProblemCount;
using guidexos::developer_studio::OutputServiceProblemCounts;
using guidexos::developer_studio::OutputServiceSelectActiveChannel;
using guidexos::developer_studio::OutputSeverity;
using guidexos::developer_studio::OutputSource;
using guidexos::developer_studio::OutputStream;
using guidexos::developer_studio::OutputSeverityName;
using guidexos::developer_studio::OutputSourceName;
using guidexos::developer_studio::OutputChannelName;
using guidexos::developer_studio::OutputErrorName;
using guidexos::developer_studio::Document;
using guidexos::developer_studio::FindCopyStateFromSession;
using guidexos::developer_studio::FindCopyStateToSession;
using guidexos::developer_studio::FindCurrentMatch;
using guidexos::developer_studio::FindCanReplaceAll;
using guidexos::developer_studio::FindDirection;
using guidexos::developer_studio::FindErrorCode;
using guidexos::developer_studio::FindErrorName;
using guidexos::developer_studio::FindMatch;
using guidexos::developer_studio::FindNavigate;
using guidexos::developer_studio::FindSearch;
using guidexos::developer_studio::FindLiteralMatchesAt;
using guidexos::developer_studio::FindSelectInitial;
using guidexos::developer_studio::FindSession;
using guidexos::developer_studio::FindSessionInit;
using guidexos::developer_studio::FindSessionIsStale;
using guidexos::developer_studio::FindSetCurrentMatch;
using guidexos::developer_studio::FindSetOptions;
using guidexos::developer_studio::FindSetQuery;
using guidexos::developer_studio::FindSetReplacement;
using guidexos::developer_studio::FindVisibleMatchIndices;
using guidexos::developer_studio::FindOptions;
using guidexos::developer_studio::kFindMaxQueryBytes;
using guidexos::developer_studio::GetCaretOffset;
using guidexos::developer_studio::GetSelectedText;
using guidexos::developer_studio::ReplaceTextRange;
using guidexos::developer_studio::ReplaceTextRanges;
using guidexos::developer_studio::SelectTextRange;
using guidexos::developer_studio::SetCaretOffset;
using guidexos::developer_studio::ValidateTextRange;
using guidexos::developer_studio::DefaultSyntaxPalette;
using guidexos::developer_studio::DetectSyntaxLanguage;
using guidexos::developer_studio::DocumentUpdateSyntax;
using guidexos::developer_studio::FileInfo;
using guidexos::developer_studio::FileInfoKind;
using guidexos::developer_studio::FileListEntry;
using guidexos::developer_studio::InitialTargetProfile;
using guidexos::developer_studio::IsSupportedTextPath;
using guidexos::developer_studio::IsValidTargetProfile;
using guidexos::developer_studio::JoinWorkspacePath;
using guidexos::developer_studio::ModelErrorCode;
using guidexos::developer_studio::ModelErrorName;
using guidexos::developer_studio::ProjectCreateRequest;
using guidexos::developer_studio::ProjectErrorCode;
using guidexos::developer_studio::ProjectErrorName;
using guidexos::developer_studio::ProjectKind;
using guidexos::developer_studio::ProjectOperationResult;
using guidexos::developer_studio::TextBuffer;
using guidexos::developer_studio::TextBufferBackspace;
using guidexos::developer_studio::TextBufferDelete;
using guidexos::developer_studio::TextBufferEnd;
using guidexos::developer_studio::TextBufferHome;
using guidexos::developer_studio::TextBufferInsert;
using guidexos::developer_studio::TextBufferLineCount;
using guidexos::developer_studio::TextBufferLineEnd;
using guidexos::developer_studio::TextBufferLineStart;
using guidexos::developer_studio::TextBufferMoveDown;
using guidexos::developer_studio::TextBufferMoveLeft;
using guidexos::developer_studio::TextBufferMoveRight;
using guidexos::developer_studio::TextBufferMoveUp;
using guidexos::developer_studio::TextBufferOffsetForVisualColumn;
using guidexos::developer_studio::TextBufferVisualColumn;
using guidexos::developer_studio::SyntaxBuildRenderRuns;
using guidexos::developer_studio::SyntaxCacheLineSpans;
using guidexos::developer_studio::SyntaxCacheValidate;
using guidexos::developer_studio::SyntaxErrorName;
using guidexos::developer_studio::SyntaxLanguageName;
using guidexos::developer_studio::SyntaxPalette;
using guidexos::developer_studio::SyntaxPaletteColor;
using guidexos::developer_studio::SyntaxRenderRun;
using guidexos::developer_studio::SyntaxSelection;
using guidexos::developer_studio::SyntaxTokenKind;
using guidexos::developer_studio::SyntaxTokenSpan;
using guidexos::developer_studio::WorkspaceController;
using guidexos::developer_studio::WorkspaceControllerActiveDocument;
using guidexos::developer_studio::WorkspaceControllerCloseDocument;
using guidexos::developer_studio::WorkspaceControllerCloseWorkspace;
using guidexos::developer_studio::WorkspaceControllerEnterSelected;
using guidexos::developer_studio::WorkspaceControllerGoUp;
using guidexos::developer_studio::WorkspaceControllerInit;
using guidexos::developer_studio::WorkspaceControllerOpenDocument;
using guidexos::developer_studio::WorkspaceControllerOpenWorkspace;
using guidexos::developer_studio::WorkspaceControllerRefresh;
using guidexos::developer_studio::WorkspaceControllerSaveActive;
using guidexos::developer_studio::WorkspaceControllerSaveAll;
using guidexos::developer_studio::WorkspaceControllerSaveDocument;
using guidexos::developer_studio::WorkspaceFileSystem;
using guidexos::developer_studio::WorkspaceEntryKind;
using guidexos::developer_studio::kMaxEditorBytes;
using guidexos::developer_studio::kMaxNameBytes;
using guidexos::developer_studio::kMaxOpenDocuments;
using guidexos::developer_studio::kMaxPathBytes;
using guidexos::developer_studio::kMaxProjectDisplayNameBytes;
using guidexos::developer_studio::kMaxProjectIdBytes;
using guidexos::developer_studio::kMaxProjectFileBytes;
using guidexos::developer_studio::kMaxWorkspaceEntries;
using guidexos::developer_studio::WorkspaceControllerOpenDocumentAtLocation;
using guidexos::developer_studio::WorkspaceControllerSetCaretPosition;
using guidexos::developer_studio::WorkspaceModel;
using guidexos::developer_studio::ProjectSearchCancel;
using guidexos::developer_studio::ProjectSearchErrorCode;
using guidexos::developer_studio::ProjectSearchErrorName;
using guidexos::developer_studio::ProjectSearchFileGroup;
using guidexos::developer_studio::ProjectSearchIsActive;
using guidexos::developer_studio::ProjectSearchMatch;
using guidexos::developer_studio::ProjectSearchOperation;
using guidexos::developer_studio::ProjectSearchOperationInfo;
using guidexos::developer_studio::ProjectSearchOptions;
using guidexos::developer_studio::ProjectSearchOptionsInit;
using guidexos::developer_studio::ProjectSearchPoll;
using guidexos::developer_studio::ProjectSearchQuery;
using guidexos::developer_studio::ProjectSearchQueryResultGroups;
using guidexos::developer_studio::ProjectSearchRelease;
using guidexos::developer_studio::ProjectSearchRequest;
using guidexos::developer_studio::ProjectSearchResultGroupAt;
using guidexos::developer_studio::ProjectSearchResultMatchAt;
using guidexos::developer_studio::ProjectSearchService;
using guidexos::developer_studio::ProjectSearchServiceInit;
using guidexos::developer_studio::ProjectSearchStart;
using guidexos::developer_studio::ProjectSearchState;
using guidexos::developer_studio::ProjectSearchStateName;
using guidexos::developer_studio::ProjectSearchDocumentSnapshot;
using guidexos::developer_studio::kProjectSearchMaxFileBytes;
using guidexos::developer_studio::DocumentSymbol;
using guidexos::developer_studio::ProjectSymbol;
using guidexos::developer_studio::SymbolDatabase;
using guidexos::developer_studio::SymbolDocument;
using guidexos::developer_studio::SymbolDatabaseDocumentPath;
using guidexos::developer_studio::SymbolDatabaseDocumentSymbolAt;
using guidexos::developer_studio::SymbolDatabaseFindDocumentById;
using guidexos::developer_studio::SymbolDatabaseFindSymbols;
using guidexos::developer_studio::SymbolDatabaseInit;
using guidexos::developer_studio::SymbolDatabaseProjectSymbolAt;
using guidexos::developer_studio::SymbolDatabaseProjectSymbolCount;
using guidexos::developer_studio::SymbolDatabaseDocumentAt;
using guidexos::developer_studio::SymbolKindPrefix;
using guidexos::developer_studio::SymbolKindName;
using guidexos::developer_studio::SymbolLocation;
using guidexos::developer_studio::kSymbolMaxDocumentSymbols;
using guidexos::developer_studio::kSymbolMaxDocuments;
using guidexos::developer_studio::kSymbolMaxNameBytes;
using guidexos::developer_studio::kSymbolMaxProjectSymbols;
using guidexos::developer_studio::kSymbolMaxQueryBytes;
using guidexos::developer_studio::kSymbolMaxVisibleResults;

static const gx_rect kWindowRect = { 0, 0, 960, 700 };
static const gx_rect kCommandRect = { 0, 0, 960, 48 };
static const gx_rect kExplorerRect = { 0, 48, 270, 472 };
static const gx_rect kEditorRect = { 270, 48, 690, 472 };
static const gx_rect kOutputRect = { 0, 520, 960, 120 };
static const gx_rect kStatusRect = { 0, 640, 960, 60 };
static const int kEntryTop = 104;
static const int kEntryHeight = 18;
static const int kEditorTop = 86;
static const int kEditorLineHeight = 16;
static const int kEditorLineNumberX = 282;
static const int kEditorTextX = 320;
static const int kVisibleEditorLines = 26;
static const uint32_t kEditorTabWidth = 4;
static const uint32_t kVisibleEditorColumns = 78;
static const int kOutlineTop = 304;
static const int kOutlineRowHeight = 17;
static const int kOutlineMaxRows = 11;
static const int kSymbolSearchTop = 76;
static const int kSymbolSearchRowHeight = 18;
static const int kSymbolSearchMaxRows = 22;
static const int kMaxPromptBytes = 240;
static const int kFindFieldX = 326;
static const int kFindFieldWidth = 250;
static const int kFindQueryY = 17;
static const int kFindReplaceY = 39;
static const int kFindPreviousX = 590;
static const int kFindNextX = 650;
static const int kFindCaseX = 696;
static const int kFindWordX = 752;
static const int kFindWrapX = 808;
static const int kFindCloseX = 864;
static const int kFindReplaceButtonX = 590;
static const int kFindReplaceAllX = 660;
static const uint32_t kFindVisibleOverlayCapacity = 256;
static const int kSearchPanelTop = 48;
static const int kSearchPanelResultsTop = 190;
static const int kSearchPanelRowHeight = 17;
static const int kSearchPanelMaxRows = 25;
static const int kSearchFieldX = 86;
static const int kSearchFieldWidth = 350;
static const int kSearchIncludeY = 126;
static const int kSearchExcludeY = 150;

enum class InputMode {
    Normal = 0,
    WorkspacePath,
    ProjectPath,
    ProjectCreate,
    ConfirmDocument,
    ConfirmWorkspace,
    ConfirmApplication,
    ConfirmBuild,
    ConfirmRun,
    ConfirmRunClose
};

struct NativeFileSystemContext {
    gx_app_context* app;
};

struct ProjectDialog {
    char displayName[kMaxProjectDisplayNameBytes];
    char parentPath[kMaxPathBytes];
    char folderName[kMaxNameBytes];
    char projectId[kMaxProjectIdBytes];
    uint32_t field;
};

static WorkspaceController g_controller;
static SymbolDatabase g_symbolDatabase = {};
static ProjectSymbol g_symbolProjectStorage[kSymbolMaxProjectSymbols] = {};
static SymbolDocument g_symbolDocumentStorage[kSymbolMaxDocuments] = {};
static DocumentSymbol g_symbolScratchStorage[kSymbolMaxDocumentSymbols] = {};
static NativeFileSystemContext g_fileSystemContext = {};
static gx_handle g_window = 0;
static InputMode g_inputMode = InputMode::Normal;
static bool g_editorFocused = false;
static bool g_fileMenuOpen = false;
static bool g_buildMenuOpen = false;
static bool g_requestExit = false;
static bool g_workspaceSwitchPending = false;
static uint32_t g_pendingDocument = kMaxOpenDocuments;
static uint32_t g_editorScrollLine = 0;
static uint32_t g_editorScrollColumn = 0;
static uint32_t g_lastExplorerClick = 0;
static uint64_t g_lastExplorerClickTick = 0;
static char g_prompt[kMaxPathBytes] = {};
static char g_pendingWorkspacePath[kMaxPathBytes] = {};
static ProjectDialog g_projectDialog = {};
static OutputService g_outputService = {};
static uint64_t g_studioOperationId = 0;
static uint64_t g_runOperationId = 0;
static bool g_outputProblemsTab = false;
static bool g_outputFocused = false;
static uint32_t g_outputScroll = 0;
static bool g_outputFollowTail = true;
static uint32_t g_problemSelected = 0;
static char g_textScratch[256] = {};
static char g_lineScratch[256] = {};
static SyntaxRenderRun g_renderRuns[9000] = {};
static BuildController g_buildController = {};
static bool g_buildTerminalReported = false;
static RunController g_runController = {};
static bool g_runWaitingForBuild = false;
static RunState g_lastRunState = RunState::Idle;
static bool g_runTerminalReported = false;
static bool g_syntaxIncrementalMarkerReported = false;
static bool g_syntaxConvergenceMarkerReported = false;
static bool g_syntaxFallbackMarkerReported = false;
static bool g_syntaxRenderMarkerReported = false;

enum class FindField {
    Query = 0,
    Replacement
};

enum class ProjectSearchField {
    Query = 0,
    Include,
    Exclude
};

static FindSession g_findSession = {};
static bool g_findBarOpen = false;
static bool g_findReplaceMode = false;
static FindField g_findField = FindField::Query;
static uint32_t g_findFieldCaret = 0;
static char g_findTransientStatus[96] = {};
static uint32_t g_findVisibleIndices[kFindVisibleOverlayCapacity] = {};
static ProjectSearchService g_projectSearch = {};
static ProjectSearchOptions g_projectSearchDraft = {};
static ProjectSearchDocumentSnapshot g_projectSearchSnapshots[kMaxOpenDocuments] = {};
static bool g_projectSearchPanelOpen = false;
static bool g_projectSearchResultsFocused = false;
static ProjectSearchField g_projectSearchField = ProjectSearchField::Query;
static uint32_t g_projectSearchFieldCaret = 0;
static uint32_t g_projectSearchScroll = 0;
static uint32_t g_projectSearchSelectedGroup = 0;
static uint32_t g_projectSearchSelectedMatch = 0;
static uint64_t g_projectSearchOperationId = 0;
static uint64_t g_projectSearchProjectGeneration = 0;
static char g_projectSearchProjectId[kMaxProjectIdBytes] = {};
static char g_projectSearchStatus[128] = {};
static char g_lastProjectSearchQuery[kFindMaxQueryBytes + 1] = {};
static bool g_projectSearchTerminalReported = false;
static bool g_symbolSearchOpen = false;
static bool g_symbolSearchCaseSensitive = false;
static uint32_t g_symbolSearchCaret = 0;
static uint32_t g_symbolSearchSelected = 0;
static uint32_t g_symbolSearchScroll = 0;
static uint32_t g_symbolSearchResultCount = 0;
static uint32_t g_symbolSearchResults[kSymbolMaxVisibleResults] = {};
static char g_symbolSearchQuery[kSymbolMaxQueryBytes + 1] = {};
static uint32_t g_outlineSelected = 0;
static uint32_t g_outlineScroll = 0;

static char mapKeyToChar(int keyCode, int modifiers);
static void compose(char* output, uint32_t size, const char* prefix, const char* value, const char* suffix);
static void stopProjectSearch(gx_app_context* ctx);
static void keepCaretVisible(const Document* document);

static void clear_event(gx_event* event) {
    if (!event) return;
    event->size = 0;
    event->type = GX_EVENT_NONE;
    event->window = 0;
    event->param1 = 0;
    event->param2 = 0;
    event->param3 = 0;
    event->param4 = 0;
}

static uint32_t lengthOf(const char* text, uint32_t limit) {
    if (!text) return 0;
    uint32_t length = 0;
    while (length < limit && text[length] != '\0') ++length;
    return length;
}

static void copyText(char* output, uint32_t outputSize, const char* input) {
    if (!output || outputSize == 0) return;
    uint32_t i = 0;
    if (input) while (i + 1 < outputSize && input[i] != '\0') { output[i] = input[i]; ++i; }
    output[i] = '\0';
}

static void appendText(char* output, uint32_t outputSize, const char* text) {
    if (!output || outputSize == 0 || !text) return;
    uint32_t offset = lengthOf(output, outputSize);
    uint32_t i = 0;
    while (offset + 1 < outputSize && text[i] != '\0') output[offset++] = text[i++];
    output[offset] = '\0';
}

static void appendUnsigned(char* output, uint32_t outputSize, uint32_t value) {
    char digits[12];
    uint32_t count = 0;
    if (value == 0) digits[count++] = '0';
    while (value > 0 && count < sizeof(digits)) { digits[count++] = static_cast<char>('0' + (value % 10)); value /= 10; }
    while (count > 0 && lengthOf(output, outputSize) + 1 < outputSize) {
        char digit = digits[--count];
        uint32_t offset = lengthOf(output, outputSize);
        output[offset] = digit;
        output[offset + 1] = '\0';
    }
}

static void appendSigned(char* output, uint32_t outputSize, int32_t value) {
    if (value < 0) {
        appendText(output, outputSize, "-");
        const uint32_t magnitude = static_cast<uint32_t>(-(value + 1)) + 1u;
        appendUnsigned(output, outputSize, magnitude);
    } else {
        appendUnsigned(output, outputSize, static_cast<uint32_t>(value));
    }
}

static uint64_t activeOutputOperation() {
    if (g_runOperationId != 0 && (RunControllerIsActive(&g_runController) || g_runWaitingForBuild)) return g_runOperationId;
    if (g_buildController.operationId != 0 && BuildControllerIsActive(&g_buildController)) return g_buildController.operationId;
    return g_studioOperationId;
}

static void writeOutput(const char* message) {
    if (!message) return;
    const uint64_t operationId = activeOutputOperation();
    OutputSource source = OutputSource::DeveloperStudio;
    OutputCategory category = OutputCategory::General;
    if (operationId == g_buildController.operationId && operationId != 0) { source = OutputSource::Build; category = OutputCategory::BuildLifecycle; }
    else if (operationId == g_runOperationId && operationId != 0) { source = OutputSource::Run; category = OutputCategory::RunLifecycle; }
    OutputServiceAppendText(&g_outputService, operationId, source, OutputSeverity::Information, category,
                            OutputStream::Unknown, message,
                            g_controller.model.hasProject ? g_controller.model.project.projectId : nullptr, nullptr);
    g_outputFollowTail = true;
}

static void writeStudioOutput(const char* message) {
    if (!message || g_studioOperationId == 0) return;
    OutputServiceAppendText(&g_outputService, g_studioOperationId, OutputSource::DeveloperStudio,
                            OutputSeverity::Information, OutputCategory::General, OutputStream::Unknown, message,
                            g_controller.model.hasProject ? g_controller.model.project.projectId : nullptr, nullptr);
    g_outputFollowTail = true;
}

static void logMarker(gx_app_context* ctx, const char* marker) {
    if (ctx && ctx->host && ctx->host->log && marker) ctx->host->log(ctx, marker);
}

static void markerFailure(gx_app_context* ctx, const char* prefix, const char* reason) {
    char message[128] = {};
    copyText(message, sizeof(message), prefix);
    appendText(message, sizeof(message), " reason=");
    appendText(message, sizeof(message), reason ? reason : "unknown");
    logMarker(ctx, message);
}

static char lowerSearchAscii(char value) {
    return value >= 'A' && value <= 'Z' ? static_cast<char>(value + ('a' - 'A')) : value;
}

static bool searchTextEqual(const char* left, const char* right) {
    if (!left || !right) return left == right;
    uint32_t i = 0;
    while (left[i] != '\0' && right[i] != '\0') {
        if (left[i] != right[i]) return false;
        ++i;
    }
    return left[i] == right[i];
}

static bool copyProjectRelativePath(const WorkspaceModel& model, const char* absolutePath,
                                    char* output, uint32_t outputSize) {
    if (!absolutePath || !output || !model.rootPath[0]) return false;
    char root[kMaxPathBytes] = {};
    char path[kMaxPathBytes] = {};
    if (!guidexos::developer_studio::NormalizePath(model.rootPath, root, sizeof(root)) ||
        !guidexos::developer_studio::NormalizePath(absolutePath, path, sizeof(path))) return false;
    const uint32_t rootLength = lengthOf(root, sizeof(root));
    const uint32_t pathLength = lengthOf(path, sizeof(path));
    if (rootLength >= pathLength) return false;
    for (uint32_t i = 0; i < rootLength; ++i)
        if (lowerSearchAscii(root[i]) != lowerSearchAscii(path[i])) return false;
    if (path[rootLength] != '/') return false;
    if (lengthOf(path + rootLength + 1, outputSize) >= outputSize) return false;
    copyText(output, outputSize, path + rootLength + 1);
    return true;
}

static uint32_t activeOutlineDocumentIndex() {
    const Document* document = WorkspaceControllerActiveDocumentConst(&g_controller);
    if (!document) return kSymbolMaxDocuments;
    const int32_t index = SymbolDatabaseFindDocumentById(&g_symbolDatabase, document->documentId);
    return index < 0 ? kSymbolMaxDocuments : static_cast<uint32_t>(index);
}

static uint32_t outlineSymbolCount() {
    const uint32_t documentIndex = activeOutlineDocumentIndex();
    const SymbolDocument* document = SymbolDatabaseDocumentAt(&g_symbolDatabase, documentIndex);
    return document ? document->symbolCount : 0;
}

static void ensureOutlineSelectionVisible() {
    const uint32_t count = outlineSymbolCount();
    if (count == 0) { g_outlineSelected = 0; g_outlineScroll = 0; return; }
    if (g_outlineSelected >= count) g_outlineSelected = count - 1;
    if (g_outlineSelected < g_outlineScroll) g_outlineScroll = g_outlineSelected;
    if (g_outlineSelected >= g_outlineScroll + static_cast<uint32_t>(kOutlineMaxRows))
        g_outlineScroll = g_outlineSelected - static_cast<uint32_t>(kOutlineMaxRows) + 1;
}

static void symbolSearchRefresh() {
    g_symbolSearchResultCount = SymbolDatabaseFindSymbols(&g_symbolDatabase, g_symbolSearchQuery,
                                                          g_symbolSearchCaseSensitive,
                                                          g_symbolSearchResults, kSymbolMaxVisibleResults);
    if (g_symbolSearchResultCount > kSymbolMaxVisibleResults) g_symbolSearchResultCount = kSymbolMaxVisibleResults;
    if (g_symbolSearchSelected >= g_symbolSearchResultCount) g_symbolSearchSelected = g_symbolSearchResultCount == 0 ? 0 : g_symbolSearchResultCount - 1;
    if (g_symbolSearchSelected < g_symbolSearchScroll) g_symbolSearchScroll = g_symbolSearchSelected;
    if (g_symbolSearchSelected >= g_symbolSearchScroll + static_cast<uint32_t>(kSymbolSearchMaxRows))
        g_symbolSearchScroll = g_symbolSearchSelected - static_cast<uint32_t>(kSymbolSearchMaxRows) + 1;
}

static void openSymbolSearch(gx_app_context* ctx) {
    g_symbolSearchOpen = true;
    g_symbolSearchCaseSensitive = false;
    g_symbolSearchCaret = 0;
    g_symbolSearchSelected = 0;
    g_symbolSearchScroll = 0;
    g_symbolSearchQuery[0] = '\0';
    g_editorFocused = false;
    g_outputFocused = false;
    g_findBarOpen = false;
    if (g_projectSearchPanelOpen) stopProjectSearch(ctx);
    g_projectSearchPanelOpen = false;
    symbolSearchRefresh();
}

static void closeSymbolSearch() {
    g_symbolSearchOpen = false;
    g_symbolSearchQuery[0] = '\0';
    g_symbolSearchCaret = 0;
    g_symbolSearchResultCount = 0;
}

static bool textAtOffset(const TextBuffer& buffer, uint32_t offset, const char* value) {
    if (!value) return false;
    const uint32_t length = lengthOf(value, kSymbolMaxNameBytes);
    if (offset > buffer.length || length > buffer.length - offset) return false;
    for (uint32_t i = 0; i < length; ++i) if (buffer.data[offset + i] != value[i]) return false;
    return true;
}

static bool selectSymbolIdentifier(Document* document, const ProjectSymbol& symbol) {
    if (!document) return false;
    const SymbolLocation& location = symbol.symbol.location;
    const uint32_t symbolLength = lengthOf(symbol.symbol.name, kSymbolMaxNameBytes);
    if (location.documentId == document->documentId && location.generation == document->buffer.generation &&
        location.identifierOffset <= document->buffer.length && symbolLength <= document->buffer.length - location.identifierOffset &&
        textAtOffset(document->buffer, location.identifierOffset, symbol.symbol.name))
        return SelectTextRange(&document->buffer, location.identifierOffset, symbolLength);
    const uint32_t zeroLine = location.line > 0 ? location.line - 1 : 0;
    const uint32_t lineCount = TextBufferLineCount(&document->buffer);
    if (zeroLine >= lineCount) return false;
    const uint32_t start = TextBufferLineStart(&document->buffer, zeroLine);
    const uint32_t end = TextBufferLineEnd(&document->buffer, zeroLine);
    for (uint32_t offset = start; offset + symbolLength <= end; ++offset)
        if (textAtOffset(document->buffer, offset, symbol.symbol.name))
            return SelectTextRange(&document->buffer, offset, symbolLength);
    return false;
}

static bool navigateSymbol(gx_app_context* ctx, uint32_t projectSymbolIndex) {
    const ProjectSymbol* stored = SymbolDatabaseProjectSymbolAt(&g_symbolDatabase, projectSymbolIndex);
    if (!stored || !g_controller.model.open || !g_controller.model.hasProject) return false;
    const ProjectSymbol symbol = *stored;
    const char* absolutePath = SymbolDatabaseDocumentPath(&g_symbolDatabase, symbol.documentIndex);
    char relativePath[kMaxPathBytes] = {};
    if (!copyProjectRelativePath(g_controller.model, absolutePath, relativePath, sizeof(relativePath))) return false;
    uint32_t documentIndex = kMaxOpenDocuments;
    OutputErrorCode error = OutputErrorCode::None;
    if (!WorkspaceControllerOpenDocumentAtLocation(&g_controller, g_controller.model.project.projectId,
                                                   relativePath, symbol.symbol.location.line,
                                                   symbol.symbol.location.column, &documentIndex, &error)) {
        writeOutput("Unable to open symbol location");
        markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER symbol_navigation=FAIL", OutputErrorName(error));
        return false;
    }
    Document* document = WorkspaceControllerActiveDocument(&g_controller);
    if (!document) return false;
    selectSymbolIdentifier(document, symbol);
    g_editorFocused = true;
    g_outputFocused = false;
    keepCaretVisible(document);
    closeSymbolSearch();
    logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER symbol_navigation=PASS");
    return true;
}

static void projectSearchSetStatus(const char* text) {
    copyText(g_projectSearchStatus, sizeof(g_projectSearchStatus), text ? text : "");
}

static char* projectSearchFieldBuffer() {
    if (g_projectSearchField == ProjectSearchField::Include) return g_projectSearchDraft.includePattern;
    if (g_projectSearchField == ProjectSearchField::Exclude) return g_projectSearchDraft.excludePattern;
    return g_projectSearchDraft.query;
}

static uint32_t projectSearchFieldCapacity() {
    if (g_projectSearchField == ProjectSearchField::Include) return sizeof(g_projectSearchDraft.includePattern);
    if (g_projectSearchField == ProjectSearchField::Exclude) return sizeof(g_projectSearchDraft.excludePattern);
    return sizeof(g_projectSearchDraft.query);
}

static void projectSearchInitializeDraft(const Document* document) {
    ProjectSearchOptionsInit(&g_projectSearchDraft);
    copyText(g_projectSearchDraft.includePattern, sizeof(g_projectSearchDraft.includePattern),
             "*.c;*.h;*.cc;*.cpp;*.cxx;*.hh;*.hpp;*.hxx");
    copyText(g_projectSearchDraft.excludePattern, sizeof(g_projectSearchDraft.excludePattern),
             ".git/*;.vs/*;.idea/*;out/*;build/*;bin/*;obj/*;dist/*;node_modules/*");
    bool usedSelection = false;
    if (document && document->buffer.selectionActive) {
        const uint32_t start = document->buffer.selectionAnchor < document->buffer.caret ?
            document->buffer.selectionAnchor : document->buffer.caret;
        const uint32_t end = document->buffer.selectionAnchor < document->buffer.caret ?
            document->buffer.caret : document->buffer.selectionAnchor;
        if (end > start && end - start <= kFindMaxQueryBytes) {
            usedSelection = true;
            for (uint32_t i = start; i < end; ++i) {
                if (document->buffer.data[i] == '\r' || document->buffer.data[i] == '\n') { usedSelection = false; break; }
            }
            if (usedSelection) {
                for (uint32_t i = start; i < end; ++i) g_projectSearchDraft.query[i - start] = document->buffer.data[i];
                g_projectSearchDraft.query[end - start] = '\0';
            }
        }
    }
    if (!usedSelection && g_lastProjectSearchQuery[0] != '\0')
        copyText(g_projectSearchDraft.query, sizeof(g_projectSearchDraft.query), g_lastProjectSearchQuery);
    if (!usedSelection && g_lastProjectSearchQuery[0] == '\0' && g_findSession.query[0] != '\0')
        copyText(g_projectSearchDraft.query, sizeof(g_projectSearchDraft.query), g_findSession.query);
    g_projectSearchField = ProjectSearchField::Query;
    g_projectSearchFieldCaret = lengthOf(g_projectSearchDraft.query, sizeof(g_projectSearchDraft.query));
}

static uint32_t captureDirtyProjectSearchDocuments() {
    uint32_t count = 0;
    for (uint32_t i = 0; i < kMaxOpenDocuments && count < kMaxOpenDocuments; ++i) {
        const Document& document = g_controller.model.documents[i];
        if (!document.used || !document.buffer.dirty) continue;
        ProjectSearchDocumentSnapshot& snapshot = g_projectSearchSnapshots[count];
        if (!copyProjectRelativePath(g_controller.model, document.path, snapshot.relativePath,
                                     sizeof(snapshot.relativePath))) continue;
        if (document.buffer.length > kProjectSearchMaxFileBytes) continue;
        snapshot.length = document.buffer.length;
        snapshot.documentId = document.documentId;
        snapshot.documentGeneration = document.buffer.generation;
        for (uint32_t j = 0; j < snapshot.length; ++j) snapshot.data[j] = document.buffer.data[j];
        snapshot.data[snapshot.length] = '\0';
        ++count;
    }
    return count;
}

static void pollProjectSearch(gx_app_context* ctx) {
    if (g_projectSearchOperationId == 0) return;
    if (ProjectSearchIsActive(&g_projectSearch) &&
        (!g_controller.model.open || !g_controller.model.hasProject ||
         g_controller.model.projectGeneration != g_projectSearchProjectGeneration ||
         !searchTextEqual(g_controller.model.project.projectId, g_projectSearchProjectId))) {
        ProjectSearchCancel(&g_projectSearch, g_projectSearchOperationId);
        projectSearchSetStatus("Project changed; cancelling search");
    }
    ProjectSearchPoll(&g_projectSearch, g_projectSearchOperationId, 16, gx_get_ticks_ms(ctx));
    const ProjectSearchOperation* operation = ProjectSearchOperationInfo(&g_projectSearch);
    if (!operation || ProjectSearchIsActive(&g_projectSearch) || g_projectSearchTerminalReported) return;
    g_projectSearchTerminalReported = true;
    if (operation->state == ProjectSearchState::Completed) {
        copyText(g_projectSearchStatus, sizeof(g_projectSearchStatus), "Search complete: ");
        appendUnsigned(g_projectSearchStatus, sizeof(g_projectSearchStatus), operation->resultFileCount);
        appendText(g_projectSearchStatus, sizeof(g_projectSearchStatus), " files, ");
        appendUnsigned(g_projectSearchStatus, sizeof(g_projectSearchStatus), operation->resultMatchCount);
        appendText(g_projectSearchStatus, sizeof(g_projectSearchStatus), operation->truncated ? " matches (truncated)" : " matches");
        if (operation->truncated) {
            markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER project_search_complete=TRUNCATED", ProjectSearchErrorName(operation->error));
        } else {
            logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER project_search_enumeration=PASS");
            logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER project_search_file_scan=PASS");
            logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER project_search_complete=PASS");
        }
        writeStudioOutput(operation->truncated ? "Find in Files completed with truncated results" : "Find in Files completed");
    } else if (operation->state == ProjectSearchState::Cancelled) {
        projectSearchSetStatus("Search cancelled; partial results retained");
        logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER project_search_complete=CANCELLED");
        writeStudioOutput("Find in Files cancelled");
    } else {
        copyText(g_projectSearchStatus, sizeof(g_projectSearchStatus), "Search failed: ");
        appendText(g_projectSearchStatus, sizeof(g_projectSearchStatus), ProjectSearchErrorName(operation->error));
        markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER project_search_complete=FAIL", ProjectSearchErrorName(operation->error));
        writeStudioOutput("Find in Files failed");
    }
}

static void stopProjectSearch(gx_app_context* ctx) {
    if (g_projectSearchOperationId == 0) return;
    if (ProjectSearchIsActive(&g_projectSearch)) {
        ProjectSearchCancel(&g_projectSearch, g_projectSearchOperationId);
        ProjectSearchPoll(&g_projectSearch, g_projectSearchOperationId, 1, gx_get_ticks_ms(ctx));
    }
    const ProjectSearchOperation* operation = ProjectSearchOperationInfo(&g_projectSearch);
    if (operation && operation->state != ProjectSearchState::Idle)
        ProjectSearchRelease(&g_projectSearch, g_projectSearchOperationId);
    g_projectSearchOperationId = 0;
    g_projectSearchTerminalReported = false;
}

static bool startProjectSearch(gx_app_context* ctx) {
    if (!g_controller.model.open || !g_controller.model.hasProject) {
        projectSearchSetStatus("Open a project before searching");
        return false;
    }
    ProjectSearchRequest request = {};
    copyText(request.projectId, sizeof(request.projectId), g_controller.model.project.projectId);
    request.projectGeneration = g_controller.model.projectGeneration;
    copyText(request.rootPath, sizeof(request.rootPath), g_controller.model.rootPath);
    request.options = g_projectSearchDraft;
    request.fileSystem = g_controller.fileSystem;
    request.dirtyDocuments = g_projectSearchSnapshots;
    request.dirtyDocumentCount = captureDirtyProjectSearchDocuments();
    ProjectSearchErrorCode error = ProjectSearchErrorCode::None;
    uint64_t operationId = 0;
    if (!ProjectSearchStart(&g_projectSearch, &request, gx_get_ticks_ms(ctx), &operationId, &error)) {
        g_projectSearchOperationId = operationId;
        g_projectSearchTerminalReported = false;
        projectSearchSetStatus("Search failed: ");
        appendText(g_projectSearchStatus, sizeof(g_projectSearchStatus), ProjectSearchErrorName(error));
        return false;
    }
    g_projectSearchOperationId = operationId;
    g_projectSearchProjectGeneration = request.projectGeneration;
    copyText(g_projectSearchProjectId, sizeof(g_projectSearchProjectId), request.projectId);
    copyText(g_lastProjectSearchQuery, sizeof(g_lastProjectSearchQuery), request.options.query);
    g_projectSearchSelectedGroup = 0;
    g_projectSearchSelectedMatch = 0;
    g_projectSearchScroll = 0;
    g_projectSearchResultsFocused = false;
    g_projectSearchTerminalReported = false;
    projectSearchSetStatus("Searching...");
    writeStudioOutput("Find in Files started");
    logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER project_search_begin=PASS");
    return true;
}

static bool fsStat(void* userData, const char* path, FileInfo* outInfo) {
    NativeFileSystemContext* context = static_cast<NativeFileSystemContext*>(userData);
    if (!context || !context->app || !context->app->host || !context->app->host->file_stat || !outInfo) return false;
    gx_file_info info = {};
    if (context->app->host->file_stat(context->app, path, &info) != GX_OK) return false;
    outInfo->kind = info.type == GX_FILE_TYPE_DIRECTORY ? FileInfoKind::Directory :
        (info.type == GX_FILE_TYPE_REGULAR ? FileInfoKind::RegularFile : FileInfoKind::Unknown);
    outInfo->size = info.size;
    return true;
}

static bool fsList(void* userData, const char* path, FileListEntry* entries, uint32_t capacity, uint32_t* outCount, bool* outTruncated) {
    NativeFileSystemContext* context = static_cast<NativeFileSystemContext*>(userData);
    if (outCount) *outCount = 0;
    if (outTruncated) *outTruncated = false;
    if (!context || !context->app || !context->app->host || !context->app->host->file_list || !entries || capacity == 0 || !outCount) return false;
    if (capacity > kMaxWorkspaceEntries) capacity = kMaxWorkspaceEntries;
    gx_file_entry nativeEntries[kMaxWorkspaceEntries] = {};
    uint32_t count = 0;
    uint32_t truncated = 0;
    if (context->app->host->file_list(context->app, path, nativeEntries, capacity, &count, &truncated) != GX_OK) return false;
    for (uint32_t i = 0; i < count && i < capacity; ++i) {
        copyText(entries[i].name, sizeof(entries[i].name), nativeEntries[i].name);
        entries[i].kind = nativeEntries[i].type == GX_FILE_TYPE_DIRECTORY ? FileInfoKind::Directory :
            (nativeEntries[i].type == GX_FILE_TYPE_REGULAR ? FileInfoKind::RegularFile : FileInfoKind::Unknown);
        entries[i].size = nativeEntries[i].size;
    }
    *outCount = count;
    if (outTruncated) *outTruncated = truncated != 0;
    return true;
}

static bool fsRead(void* userData, const char* path, char* buffer, uint32_t capacity, uint32_t* outBytes) {
    NativeFileSystemContext* context = static_cast<NativeFileSystemContext*>(userData);
    if (!context || !context->app || !context->app->host || !context->app->host->file_read_workspace || !buffer || !outBytes) return false;
    return context->app->host->file_read_workspace(context->app, path, buffer, capacity, outBytes) == GX_OK;
}

static bool fsWrite(void* userData, const char* path, const char* buffer, uint32_t bytes, uint32_t* outBytes) {
    NativeFileSystemContext* context = static_cast<NativeFileSystemContext*>(userData);
    if (!context || !context->app || !context->app->host || !context->app->host->file_write_all || !buffer || !outBytes) return false;
    return context->app->host->file_write_all(context->app, path, buffer, bytes, outBytes) == GX_OK;
}

static bool fsCreateDirectory(void* userData, const char* path) {
    NativeFileSystemContext* context = static_cast<NativeFileSystemContext*>(userData);
    if (!context || !context->app || !context->app->host || !context->app->host->file_create_directory) return false;
    return context->app->host->file_create_directory(context->app, path) == GX_OK;
}

static bool fsRemovePath(void* userData, const char* path) {
    NativeFileSystemContext* context = static_cast<NativeFileSystemContext*>(userData);
    if (!context || !context->app || !context->app->host || !context->app->host->file_remove) return false;
    return context->app->host->file_remove(context->app, path) == GX_OK;
}

static BuildErrorCode mapBuildError(uint32_t error) {
    switch (error) {
    case GX_BUILD_ERROR_NONE: return BuildErrorCode::None;
    case GX_BUILD_ERROR_SDK_NOT_FOUND: return BuildErrorCode::SdkNotFound;
    case GX_BUILD_ERROR_TOOLCHAIN_NOT_FOUND: return BuildErrorCode::ToolchainNotFound;
    case GX_BUILD_ERROR_POWERSHELL_NOT_FOUND: return BuildErrorCode::PowerShellNotFound;
    case GX_BUILD_ERROR_BUILD_SCRIPT_MISSING: return BuildErrorCode::BuildScriptMissing;
    case GX_BUILD_ERROR_INVALID_PROJECT_ROOT: return BuildErrorCode::InvalidProjectRoot;
    case GX_BUILD_ERROR_PROCESS_START_FAILED: return BuildErrorCode::ProcessStartFailed;
    case GX_BUILD_ERROR_PROCESS_FAILED: return BuildErrorCode::ProcessFailed;
    case GX_BUILD_ERROR_BUILD_TIMEOUT: return BuildErrorCode::BuildTimeout;
    case GX_BUILD_ERROR_ARTIFACT_MISSING: return BuildErrorCode::ArtifactMissing;
    case GX_BUILD_ERROR_ARTIFACT_INVALID: return BuildErrorCode::ArtifactInvalid;
    case GX_BUILD_ERROR_ARTIFACT_WRONG_ARCHITECTURE: return BuildErrorCode::ArtifactWrongArchitecture;
    case GX_BUILD_ERROR_ENTRY_POINT_MISSING: return BuildErrorCode::EntryPointMissing;
    case GX_BUILD_ERROR_MANIFEST_ARTIFACT_MISMATCH: return BuildErrorCode::ManifestArtifactMismatch;
    case GX_BUILD_ERROR_OUTPUT_TRUNCATED: return BuildErrorCode::OutputTruncated;
    case GX_BUILD_ERROR_BUSY: return BuildErrorCode::AlreadyRunning;
    case GX_BUILD_ERROR_INVALID_REQUEST: return BuildErrorCode::InvalidRequest;
    default: return BuildErrorCode::ServiceError;
    }
}

static bool hostBuildStart(void* userData, const guidexos::developer_studio::BuildRequest& request, uint64_t* outHandle, BuildErrorCode* error) {
    NativeFileSystemContext* context = static_cast<NativeFileSystemContext*>(userData);
    if (error) *error = BuildErrorCode::None;
    if (!context || !context->app || !context->app->host || !context->app->host->build_project_start || !outHandle) {
        if (error) *error = BuildErrorCode::HostUnavailable;
        return false;
    }
    gx_build_request nativeRequest = {};
    nativeRequest.size = sizeof(nativeRequest);
    nativeRequest.version = GX_BUILD_API_VERSION;
    nativeRequest.projectRoot = request.projectRoot;
    nativeRequest.projectId = request.projectId;
    nativeRequest.projectKind = request.projectKind;
    nativeRequest.targetProfile = request.targetProfile;
    nativeRequest.buildSystem = request.buildSystem;
    nativeRequest.buildScript = request.buildScript;
    nativeRequest.expectedArtifact = request.expectedArtifact;
    nativeRequest.configuration = request.configuration;
    const gx_result result = context->app->host->build_project_start(context->app, &nativeRequest, outHandle);
    if (result != GX_OK) {
        if (error) *error = result == GX_ERROR_BUSY ? BuildErrorCode::AlreadyRunning : (result == GX_ERROR_FAILED ? BuildErrorCode::ServiceError : BuildErrorCode::HostUnavailable);
        return false;
    }
    return true;
}

static bool hostBuildPoll(void* userData, uint64_t handle, BuildResult* result, bool* completed, BuildErrorCode* error) {
    NativeFileSystemContext* context = static_cast<NativeFileSystemContext*>(userData);
    if (error) *error = BuildErrorCode::None;
    if (completed) *completed = false;
    if (!context || !context->app || !context->app->host || !context->app->host->build_project_poll || !result || !completed) {
        if (error) *error = BuildErrorCode::HostUnavailable;
        return false;
    }
    gx_build_snapshot snapshot = {};
    snapshot.size = sizeof(snapshot);
    snapshot.version = GX_BUILD_API_VERSION;
    const gx_result hostResult = context->app->host->build_project_poll(context->app, handle, &snapshot);
    if (hostResult != GX_OK) {
        if (error) *error = BuildErrorCode::ServiceError;
        return false;
    }
    *result = BuildResult();
    result->state = snapshot.state <= GX_BUILD_CANCELLED ? static_cast<BuildState>(snapshot.state) : BuildState::Failed;
    result->exitCode = snapshot.processExitCode;
    result->error = mapBuildError(snapshot.errorCode);
    result->elapsedMilliseconds = snapshot.elapsedMilliseconds;
    result->warningCount = snapshot.warningCount;
    result->errorCount = snapshot.errorCount;
    result->outputTruncated = snapshot.outputTruncated != 0;
    result->artifactSize = snapshot.artifactSize;
    result->artifactValid = snapshot.artifactValid != 0;
    result->artifactEntryPoint = snapshot.artifactEntryPoint != 0;
    copyText(result->artifactPath, sizeof(result->artifactPath), snapshot.artifactPath);
    copyText(result->artifactSha256, sizeof(result->artifactSha256), snapshot.artifactSha256);
    copyText(result->artifactArchitecture, sizeof(result->artifactArchitecture), snapshot.artifactArchitecture);
    copyText(result->errorMessage, sizeof(result->errorMessage), snapshot.errorMessage);
    result->outputCount = snapshot.outputCount > guidexos::developer_studio::kMaxBuildLines ? guidexos::developer_studio::kMaxBuildLines : snapshot.outputCount;
    for (uint32_t i = 0; i < result->outputCount; ++i) {
        result->output[i].standardError = snapshot.output[i].stream == 2;
        copyText(result->output[i].text, sizeof(result->output[i].text), snapshot.output[i].text);
    }
    *completed = result->state == BuildState::Succeeded || result->state == BuildState::Failed || result->state == BuildState::Cancelled;
    return true;
}

static bool hostBuildRelease(void* userData, uint64_t handle) {
    NativeFileSystemContext* context = static_cast<NativeFileSystemContext*>(userData);
    if (!context || !context->app || !context->app->host || !context->app->host->build_project_release) return false;
    return context->app->host->build_project_release(context->app, handle) == GX_OK;
}

static HostedBuildService buildService() {
    HostedBuildService service = {};
    service.userData = &g_fileSystemContext;
    service.start = hostBuildStart;
    service.poll = hostBuildPoll;
    service.release = hostBuildRelease;
    return service;
}

static RunState mapRunState(uint32_t state) {
    return state <= GX_DEVELOPMENT_RUN_FAILED ? static_cast<RunState>(state) : RunState::Failed;
}

static RunErrorCode mapRunError(uint32_t error) {
    switch (error) {
    case GX_DEVELOPMENT_RUN_ERROR_NONE: return RunErrorCode::None;
    case GX_DEVELOPMENT_RUN_ERROR_BUILD_REQUIRED: return RunErrorCode::BuildRequired;
    case GX_DEVELOPMENT_RUN_ERROR_ARTIFACT_INVALID:
    case GX_DEVELOPMENT_RUN_ERROR_ARTIFACT_MISSING:
    case GX_DEVELOPMENT_RUN_ERROR_ARTIFACT_CHANGED:
    case GX_DEVELOPMENT_RUN_ERROR_ENTRY_POINT_MISSING:
    case GX_DEVELOPMENT_RUN_ERROR_MANIFEST_MISMATCH: return RunErrorCode::ArtifactInvalid;
    case GX_DEVELOPMENT_RUN_ERROR_DEPLOYMENT_ALREADY_ACTIVE:
    case GX_DEVELOPMENT_RUN_ERROR_APPLICATION_ID_IN_USE: return RunErrorCode::AlreadyActive;
    case GX_DEVELOPMENT_RUN_ERROR_OWNER_MISMATCH: return RunErrorCode::OwnerMismatch;
    case GX_DEVELOPMENT_RUN_ERROR_STALE_DEPLOYMENT: return RunErrorCode::StaleDeployment;
    case GX_DEVELOPMENT_RUN_ERROR_LAUNCH_FAILED:
    case GX_DEVELOPMENT_RUN_ERROR_LAUNCH_UNAVAILABLE: return RunErrorCode::LaunchFailed;
    case GX_DEVELOPMENT_RUN_ERROR_INVALID_REQUEST: return RunErrorCode::InvalidRequest;
    default: return RunErrorCode::ServiceUnavailable;
    }
}

static void copyRunSnapshot(const gx_development_run_snapshot& snapshot, RunResult* result) {
    if (!result) return;
    *result = RunResult();
    result->state = mapRunState(snapshot.state);
    result->error = mapRunError(snapshot.errorCode);
    result->handle = snapshot.handle;
    result->processId = snapshot.processId;
    result->nativeRuntimeId = snapshot.nativeRuntimeId;
    result->windowCount = snapshot.windowCount;
    result->createdWindowCount = snapshot.createdWindowCount;
    result->exitCode = snapshot.exitCode;
    result->cleanupComplete = snapshot.cleanupComplete != 0;
    copyText(result->applicationId, sizeof(result->applicationId), snapshot.applicationId);
    copyText(result->displayName, sizeof(result->displayName), snapshot.displayName);
    copyText(result->artifactSha256, sizeof(result->artifactSha256), snapshot.artifactSha256);
    copyText(result->errorMessage, sizeof(result->errorMessage), snapshot.errorMessage);
}

static bool hostRunPrepare(void* userData, const guidexos::developer_studio::RunRequest& request, uint64_t* outHandle, RunResult* outResult) {
    NativeFileSystemContext* context = static_cast<NativeFileSystemContext*>(userData);
    if (outHandle) *outHandle = 0;
    if (outResult) *outResult = RunResult();
    if (!context || !context->app || !context->app->host || !context->app->host->development_run_prepare || !outHandle || !outResult) {
        if (outResult) outResult->error = RunErrorCode::ServiceUnavailable;
        return false;
    }
    gx_development_run_request nativeRequest = {};
    nativeRequest.size = sizeof(nativeRequest);
    nativeRequest.version = GX_DEVELOPMENT_RUN_API_VERSION;
    nativeRequest.projectRoot = request.projectRoot;
    nativeRequest.projectId = request.projectId;
    nativeRequest.projectKind = request.projectKind;
    nativeRequest.targetProfile = request.targetProfile;
    nativeRequest.manifestPath = request.manifestPath;
    nativeRequest.artifactPath = request.artifactPath;
    nativeRequest.artifactSha256 = request.artifactSha256;
    gx_development_run_snapshot snapshot = {};
    snapshot.size = sizeof(snapshot);
    snapshot.version = GX_DEVELOPMENT_RUN_API_VERSION;
    const gx_result result = context->app->host->development_run_prepare(context->app, &nativeRequest, outHandle, &snapshot);
    copyRunSnapshot(snapshot, outResult);
    if (result != GX_OK) {
        outResult->error = mapRunError(snapshot.errorCode);
        if (outResult->error == RunErrorCode::None) outResult->error = RunErrorCode::ServiceUnavailable;
        return false;
    }
    return true;
}

static bool hostRunStart(void* userData, uint64_t handle, RunResult* outResult) {
    NativeFileSystemContext* context = static_cast<NativeFileSystemContext*>(userData);
    if (!context || !context->app || !context->app->host || !context->app->host->development_run_start || !outResult || handle == 0) return false;
    const gx_result result = context->app->host->development_run_start(context->app, handle);
    if (result != GX_OK) {
        outResult->error = result == GX_ERROR_BUSY ? RunErrorCode::AlreadyActive : RunErrorCode::LaunchFailed;
        outResult->state = RunState::Failed;
        return false;
    }
    outResult->state = RunState::Launching;
    outResult->handle = handle;
    return true;
}

static bool hostRunPoll(void* userData, uint64_t handle, RunResult* outResult) {
    NativeFileSystemContext* context = static_cast<NativeFileSystemContext*>(userData);
    if (!context || !context->app || !context->app->host || !context->app->host->development_run_poll || !outResult || handle == 0) return false;
    gx_development_run_snapshot snapshot = {};
    snapshot.size = sizeof(snapshot);
    snapshot.version = GX_DEVELOPMENT_RUN_API_VERSION;
    const gx_result result = context->app->host->development_run_poll(context->app, handle, &snapshot);
    if (result != GX_OK) {
        outResult->error = RunErrorCode::ServiceUnavailable;
        outResult->state = RunState::Failed;
        return false;
    }
    copyRunSnapshot(snapshot, outResult);
    return true;
}

static bool hostRunRequestClose(void* userData, uint64_t handle) {
    NativeFileSystemContext* context = static_cast<NativeFileSystemContext*>(userData);
    return context && context->app && context->app->host && context->app->host->development_run_request_close && handle != 0 &&
        context->app->host->development_run_request_close(context->app, handle) == GX_OK;
}

static bool hostRunRelease(void* userData, uint64_t handle) {
    NativeFileSystemContext* context = static_cast<NativeFileSystemContext*>(userData);
    return context && context->app && context->app->host && context->app->host->development_run_release && handle != 0 &&
        context->app->host->development_run_release(context->app, handle) == GX_OK;
}

static HostedDevelopmentRunService developmentRunService() {
    HostedDevelopmentRunService service = {};
    service.userData = &g_fileSystemContext;
    service.prepare = hostRunPrepare;
    service.start = hostRunStart;
    service.poll = hostRunPoll;
    service.requestClose = hostRunRequestClose;
    service.release = hostRunRelease;
    return service;
}

static const char* currentError() {
    return ModelErrorName(g_controller.lastError);
}

static void outputError(const char* prefix) {
    copyText(g_textScratch, sizeof(g_textScratch), prefix);
    appendText(g_textScratch, sizeof(g_textScratch), ": ");
    appendText(g_textScratch, sizeof(g_textScratch), currentError());
    writeOutput(g_textScratch);
}

static void markDirtyIfNeeded(gx_app_context* ctx, bool wasDirty) {
    Document* document = WorkspaceControllerActiveDocument(&g_controller);
    if (document && document->buffer.dirty && !wasDirty) {
        writeOutput("Document modified");
        logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER document_dirty=TRUE");
    }
}

static void reportWorkspaceOpen(gx_app_context* ctx, bool success) {
    if (success) {
        writeOutput("Workspace opened");
        logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER workspace_open=PASS");
    } else {
        writeOutput("Workspace open failed");
        markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER workspace_open=FAIL", currentError());
    }
}

static void reportDocumentOpen(gx_app_context* ctx, bool success, bool duplicate) {
    if (success) {
        writeOutput(duplicate ? "Document already open" : "Document opened");
        Document* document = WorkspaceControllerActiveDocument(&g_controller);
        if (document && !duplicate) {
            const char* languageMarker = document->syntax.language == guidexos::developer_studio::SyntaxLanguage::Cpp ? "CPP" :
                (document->syntax.language == guidexos::developer_studio::SyntaxLanguage::C ? "C" : "NONE");
            copyText(g_textScratch, sizeof(g_textScratch), "GUIDEXOS_DEVELOPER_STUDIO_MARKER syntax_language=");
            appendText(g_textScratch, sizeof(g_textScratch), languageMarker);
            logMarker(ctx, g_textScratch);
            logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER syntax_cache_initialize=PASS");
            if (!document->syntax.fallback) {
                copyText(g_textScratch, sizeof(g_textScratch), "GUIDEXOS_DEVELOPER_STUDIO_MARKER syntax_full_tokenize=PASS lines=");
                appendUnsigned(g_textScratch, sizeof(g_textScratch), document->syntax.lineCount);
                logMarker(ctx, g_textScratch);
                if (SyntaxCacheValidate(&document->syntax, document->buffer.data, document->buffer.length))
                    logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER syntax_span_validation=PASS");
            }
        }
        if (duplicate) logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER document_open=PASS duplicate=TRUE");
        else logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER document_open=PASS");
    } else {
        outputError("Document open failed");
        markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER document_open=FAIL", currentError());
    }
}

static bool isProjectMetadataDocument(const Document* document) {
    if (!document) return false;
    const char* name = guidexos::developer_studio::BaseName(document->path);
    const char expected[] = "guidexos.project";
    uint32_t i = 0;
    while (name[i] != '\0' && expected[i] != '\0') {
        char a = name[i];
        char b = expected[i];
        if (a >= 'A' && a <= 'Z') a = static_cast<char>(a + ('a' - 'A'));
        if (b >= 'A' && b <= 'Z') b = static_cast<char>(b + ('a' - 'A'));
        if (a != b) return false;
        ++i;
    }
    return expected[i] == '\0' && name[i] == '\0';
}

static bool saveDocument(gx_app_context* ctx, uint32_t index) {
    if (!WorkspaceControllerSaveDocument(&g_controller, index)) {
        writeOutput("Save failed");
        markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER document_save=FAIL", currentError());
        return false;
    }
    writeOutput("Document saved");
    logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER document_save=PASS");
    if (g_controller.model.hasProject && isProjectMetadataDocument(&g_controller.model.documents[index])) {
        if (WorkspaceControllerReloadProject(&g_controller)) {
            writeOutput("Project metadata valid");
            logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER project_metadata_parse=PASS");
        } else {
            writeOutput("Project metadata invalid");
            markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER project_metadata_parse=FAIL", WorkspaceControllerProjectError(&g_controller));
        }
    }
    return true;
}

static bool saveAll(gx_app_context* ctx) {
    if (!WorkspaceControllerSaveAll(&g_controller)) {
        writeOutput("Save failed");
        markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER document_save=FAIL", currentError());
        return false;
    }
    writeOutput("All documents saved");
    logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER document_save=PASS all=TRUE");
    return true;
}

static bool isArtifactValidationError(BuildErrorCode error) {
    return error == BuildErrorCode::ArtifactMissing || error == BuildErrorCode::ArtifactInvalid ||
        error == BuildErrorCode::ArtifactWrongArchitecture || error == BuildErrorCode::EntryPointMissing ||
        error == BuildErrorCode::ManifestArtifactMismatch;
}

static void reportBuildResult(gx_app_context* ctx) {
    const BuildResult& result = g_buildController.result;
    const bool success = result.state == BuildState::Succeeded;
    if (success) {
        logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER build_artifact_validation=PASS");
        logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER build_complete=SUCCEEDED");
    } else {
        if (isArtifactValidationError(result.error)) markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER build_artifact_validation=FAIL", BuildErrorName(result.error));
        else {
            copyText(g_textScratch, sizeof(g_textScratch), "GUIDEXOS_DEVELOPER_STUDIO_MARKER build_artifact_validation=SKIPPED reason=");
            appendText(g_textScratch, sizeof(g_textScratch), BuildErrorName(result.error));
            logMarker(ctx, g_textScratch);
        }
        markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER build_complete=FAILED", BuildErrorName(result.error));
        if (result.error == BuildErrorCode::BuildTimeout) logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER build_timeout=TRUE");
    }
    char marker[96] = {};
    copyText(marker, sizeof(marker), "GUIDEXOS_DEVELOPER_STUDIO_MARKER build_process_exit=");
    appendSigned(marker, sizeof(marker), result.exitCode);
    logMarker(ctx, marker);
    copyText(marker, sizeof(marker), "GUIDEXOS_DEVELOPER_STUDIO_MARKER build_warning_count=");
    appendUnsigned(marker, sizeof(marker), result.warningCount);
    logMarker(ctx, marker);
    copyText(marker, sizeof(marker), "GUIDEXOS_DEVELOPER_STUDIO_MARKER build_error_count=");
    appendUnsigned(marker, sizeof(marker), result.errorCount);
    logMarker(ctx, marker);
    g_buildTerminalReported = true;
}

static bool beginBuild(gx_app_context* ctx, BuildDirtyDecision dirtyDecision) {
    if (BuildControllerIsActive(&g_buildController)) {
        writeOutput("Build already running");
        markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER build_precondition=FAIL", BuildErrorName(BuildErrorCode::AlreadyRunning));
        return false;
    }
    BuildErrorCode error = BuildErrorCode::None;
    logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER build_request=PASS");
    g_buildTerminalReported = false;
    if (!BuildControllerStart(&g_buildController, &g_controller, buildService(), dirtyDecision, &error, &g_outputService)) {
        markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER build_precondition=FAIL", BuildErrorName(error));
        if (g_buildController.result.state == BuildState::Failed || g_buildController.result.state == BuildState::Cancelled) reportBuildResult(ctx);
        return false;
    }
    OutputServiceSelectActiveChannel(&g_outputService, OutputChannel::Build);
    logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER output_operation_begin=PASS type=BUILD");
    logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER build_precondition=PASS");
    logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER build_start=PASS");
    return true;
}

static void beginRunOperation(gx_app_context* ctx) {
    if (g_runOperationId != 0 || !g_controller.model.hasProject) return;
    g_runOperationId = OutputServiceBeginOperation(&g_outputService, OutputOperationType::Run, g_controller.model.project.projectId);
    RunControllerAttachOutput(&g_runController, &g_outputService, g_runOperationId);
    OutputServiceSelectActiveChannel(&g_outputService, OutputChannel::Run);
    if (g_runOperationId != 0) {
        OutputServiceAppendText(&g_outputService, g_runOperationId, OutputSource::Run, OutputSeverity::Information,
                                OutputCategory::RunLifecycle, OutputStream::Unknown, "Run requested", g_controller.model.project.projectId, nullptr);
        OutputServiceAppendText(&g_outputService, g_runOperationId, OutputSource::Run, OutputSeverity::Information,
                                OutputCategory::RunLifecycle, OutputStream::Unknown, "Build required", g_controller.model.project.projectId, nullptr);
        logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER output_operation_begin=PASS type=RUN");
    }
}

static void completeRunWithoutDeployment(const char* message) {
    if (g_runOperationId == 0) return;
    OutputServiceAppendText(&g_outputService, g_runOperationId, OutputSource::Run, OutputSeverity::Error,
                            OutputCategory::RunLifecycle, OutputStream::Unknown, message, g_controller.model.project.projectId, nullptr);
    OutputServiceCompleteOperation(&g_outputService, g_runOperationId, false, "Run Failed | deployment skipped", nullptr);
    g_runOperationId = 0;
}

static void reportRunTerminal(gx_app_context* ctx) {
    if (g_runTerminalReported) return;
    const RunResult& result = g_runController.result;
    if (result.state == RunState::Completed && result.exitCode == 0) {
        logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER run_complete=SUCCEEDED");
    } else {
        markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER run_complete=FAILED", RunErrorName(result.error));
    }
    if (result.exitCode != 0) {
        char marker[96] = {};
        copyText(marker, sizeof(marker), "GUIDEXOS_DEVELOPER_STUDIO_MARKER run_process_exit=");
        appendSigned(marker, sizeof(marker), result.exitCode);
        logMarker(ctx, marker);
    }
    if (result.cleanupComplete) logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER run_cleanup=PASS");
    logMarker(ctx, result.state == RunState::Completed ? "GUIDEXOS_DEVELOPER_STUDIO_MARKER output_operation_complete=SUCCEEDED" : "GUIDEXOS_DEVELOPER_STUDIO_MARKER output_operation_complete=FAILED");
    g_runTerminalReported = true;
    g_runOperationId = 0;
}

static bool beginRunDeployment(gx_app_context* ctx) {
    RunRequest request = {};
    RunErrorCode error = RunErrorCode::None;
    if (!RunRequestFromBuild(g_controller.model.project, g_buildController.result, &request, &error)) {
        markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER run_artifact_validation=FAIL", RunErrorName(error));
        markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER run_complete=FAILED", RunErrorName(error));
        completeRunWithoutDeployment("Run blocked: artifact validation failed");
        return false;
    }
    logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER run_artifact_validation=PASS");
    RunControllerAttachOutput(&g_runController, &g_outputService, g_runOperationId);
    if (!RunControllerPrepare(&g_runController, developmentRunService(), request, &error)) {
        markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER run_deployment_prepare=FAIL", RunErrorName(error));
        markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER run_complete=FAILED", RunErrorName(error));
        g_runOperationId = 0;
        return false;
    }
    logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER run_deployment_prepare=PASS");
    if (!RunControllerStart(&g_runController, developmentRunService(), &error)) {
        markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER run_launch=FAIL", RunErrorName(error));
        reportRunTerminal(ctx);
        return false;
    }
    g_lastRunState = g_runController.state;
    g_runTerminalReported = false;
    logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER run_launch=PASS");
    return true;
}

static void pollBuild(gx_app_context* ctx) {
    if (!BuildControllerIsActive(&g_buildController)) return;
    BuildControllerPoll(&g_buildController, buildService());
    if (!BuildControllerIsActive(&g_buildController) && !g_buildTerminalReported) {
        reportBuildResult(ctx);
        if (g_runWaitingForBuild) {
            g_runWaitingForBuild = false;
            if (g_buildController.result.state == BuildState::Succeeded) {
                if (g_runOperationId != 0) OutputServiceAppendText(&g_outputService, g_runOperationId, OutputSource::Run,
                    OutputSeverity::Information, OutputCategory::RunLifecycle, OutputStream::Unknown,
                    "Build completed", g_controller.model.project.projectId, nullptr);
                beginRunDeployment(ctx);
            }
            else {
                markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER run_complete=FAILED", BuildErrorName(g_buildController.result.error));
                completeRunWithoutDeployment("Build failed; deployment skipped");
            }
        }
    }
}

static void requestBuild(gx_app_context* ctx) {
    if (RunControllerIsActive(&g_runController)) {
        writeOutput("Run in progress; build blocked");
        markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER build_precondition=FAIL", BuildErrorName(BuildErrorCode::AlreadyRunning));
        return;
    }
    if (!g_controller.model.open || !g_controller.model.hasProject) {
        markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER build_precondition=FAIL", BuildErrorName(g_controller.model.open ? BuildErrorCode::WorkspaceOnly : BuildErrorCode::NoProject));
        writeOutput("Build requires an open project");
        return;
    }
    if (guidexos::developer_studio::WorkspaceControllerHasDirtyProjectDocuments(&g_controller)) {
        g_inputMode = InputMode::ConfirmBuild;
        g_fileMenuOpen = false;
        g_buildMenuOpen = false;
        writeOutput("Save All or Cancel build");
        return;
    }
    beginBuild(ctx, BuildDirtyDecision::SaveAll);
}

static void requestRun(gx_app_context* ctx) {
    if (RunControllerIsActive(&g_runController) || g_runWaitingForBuild) {
        writeOutput("Run already active");
        markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER run_precondition=FAIL", RunErrorName(RunErrorCode::AlreadyActive));
        return;
    }
    if (BuildControllerIsActive(&g_buildController)) {
        writeOutput("Build in progress; run queued after build is not supported");
        markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER run_precondition=FAIL", RunErrorName(RunErrorCode::ServiceUnavailable));
        return;
    }
    logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER run_request=PASS");
    if (!g_controller.model.open || !g_controller.model.hasProject) {
        markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER run_precondition=FAIL", g_controller.model.open ? "workspace_only" : "no_project");
        writeOutput("Run requires an open project");
        return;
    }
    if (g_controller.model.project.kind != ProjectKind::NativeGuiApplication) {
        markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER run_precondition=FAIL", "unsupported_project_kind");
        writeOutput("Run supports Native GUI Application projects only");
        return;
    }
    logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER run_precondition=PASS");
    logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER run_build_required=TRUE");
    if (guidexos::developer_studio::WorkspaceControllerHasDirtyProjectDocuments(&g_controller)) {
        g_inputMode = InputMode::ConfirmRun;
        g_fileMenuOpen = false;
        g_buildMenuOpen = false;
        writeOutput("Save All or Cancel run");
        return;
    }
    beginRunOperation(ctx);
    g_runWaitingForBuild = true;
    if (!beginBuild(ctx, BuildDirtyDecision::SaveAll)) {
        g_runWaitingForBuild = false;
        completeRunWithoutDeployment("Build could not start; deployment skipped");
    }
}

static void pollRun(gx_app_context* ctx) {
    if (!RunControllerIsActive(&g_runController)) return;
    const RunState previous = g_runController.state;
    RunControllerPoll(&g_runController, developmentRunService());
    const RunResult& result = g_runController.result;
    if (result.state != previous) {
        g_lastRunState = result.state;
        if (result.state == RunState::Running) logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER run_application_state=RUNNING");
        else if (result.state == RunState::Exited) logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER run_application_state=EXITED");
        else if (result.state == RunState::CleaningUp) {
            logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER run_cleanup=START");
        }
    }
    if (!RunControllerIsActive(&g_runController) && (result.state == RunState::Completed || result.state == RunState::Failed)) reportRunTerminal(ctx);
}

static void requestRunClose(gx_app_context* ctx) {
    if (!RunControllerIsActive(&g_runController)) {
        writeOutput("No running project application");
        return;
    }
    if (!RunControllerRequestClose(&g_runController, developmentRunService())) {
        markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER run_close=FAIL", RunErrorName(RunErrorCode::ServiceUnavailable));
        writeOutput("Close request failed");
        return;
    }
    logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER run_close=REQUESTED");
}

static void showWorkspacePrompt() {
    if (BuildControllerIsActive(&g_buildController)) { writeOutput("Build in progress"); return; }
    if (RunControllerIsActive(&g_runController)) { writeOutput("Run in progress"); return; }
    g_inputMode = InputMode::WorkspacePath;
    g_fileMenuOpen = false;
    g_buildMenuOpen = false;
    g_buildMenuOpen = false;
    g_workspaceSwitchPending = true;
    copyText(g_prompt, sizeof(g_prompt), "");
}

static void showOpenProjectPrompt() {
    if (BuildControllerIsActive(&g_buildController)) { writeOutput("Build in progress"); return; }
    if (RunControllerIsActive(&g_runController)) { writeOutput("Run in progress"); return; }
    g_inputMode = InputMode::ProjectPath;
    g_fileMenuOpen = false;
    g_buildMenuOpen = false;
    g_workspaceSwitchPending = false;
    copyText(g_prompt, sizeof(g_prompt), "");
}

static void showNewProjectPrompt(gx_app_context* ctx) {
    if (BuildControllerIsActive(&g_buildController)) { writeOutput("Build in progress"); return; }
    if (RunControllerIsActive(&g_runController)) { writeOutput("Run in progress"); return; }
    if (g_controller.model.open && guidexos::developer_studio::WorkspaceModelHasDirtyDocuments(&g_controller.model)) {
        writeOutput("Save or close the current workspace first");
        markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER project_create=FAIL", "unsaved_changes");
        return;
    }
    copyText(g_projectDialog.displayName, sizeof(g_projectDialog.displayName), "Hello guideXOS");
    copyText(g_projectDialog.parentPath, sizeof(g_projectDialog.parentPath), "");
    copyText(g_projectDialog.folderName, sizeof(g_projectDialog.folderName), "");
    copyText(g_projectDialog.projectId, sizeof(g_projectDialog.projectId), "com.example.helloguidexos");
    g_projectDialog.field = 0;
    g_inputMode = InputMode::ProjectCreate;
    g_fileMenuOpen = false;
    g_buildMenuOpen = false;
    logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER project_create_request=PASS");
}

static char* projectDialogField(uint32_t field, uint32_t* capacity) {
    if (capacity) *capacity = 0;
    if (field == 0) { if (capacity) *capacity = sizeof(g_projectDialog.displayName); return g_projectDialog.displayName; }
    if (field == 1) { if (capacity) *capacity = sizeof(g_projectDialog.parentPath); return g_projectDialog.parentPath; }
    if (field == 2) { if (capacity) *capacity = sizeof(g_projectDialog.folderName); return g_projectDialog.folderName; }
    if (field == 3) { if (capacity) *capacity = sizeof(g_projectDialog.projectId); return g_projectDialog.projectId; }
    return nullptr;
}

static void projectDialogBackspace() {
    uint32_t capacity = 0;
    char* field = projectDialogField(g_projectDialog.field, &capacity);
    uint32_t length = lengthOf(field, capacity);
    if (length > 0) field[length - 1] = '\0';
}

static void projectDialogAppend(int keyCode, int modifiers) {
    uint32_t capacity = 0;
    char* field = projectDialogField(g_projectDialog.field, &capacity);
    if (!field) return;
    char value = mapKeyToChar(keyCode, modifiers);
    uint32_t length = lengthOf(field, capacity);
    if (value != '\0' && length + 1 < capacity) { field[length] = value; field[length + 1] = '\0'; }
}

static void reportProjectFailure(gx_app_context* ctx, const char* marker, ProjectErrorCode error) {
    const char* reason = ProjectErrorName(error);
    writeOutput(reason);
    markerFailure(ctx, marker, reason);
}

static bool openCreatedProject(gx_app_context* ctx, const ProjectOperationResult& created) {
    stopProjectSearch(ctx);
    logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER project_create_validation=PASS");
    logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER project_create=PASS");
    if (created.rollbackAttempted && created.rollbackSucceeded) logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER project_rollback=PASS");
    if (!WorkspaceControllerOpenProject(&g_controller, created.project.rootPath)) {
        writeOutput("Project created; project open failed");
        reportProjectFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER project_open=FAIL", g_controller.lastProjectError);
        return false;
    }
    writeOutput("Project created");
    logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER project_open=PASS");
    logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER project_metadata_parse=PASS");
    logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER project_target=guidexos.amd64.hosted.native");
    logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER project_template=native-gui-application");
    if (!WorkspaceControllerOpenDocument(&g_controller, "src/main.cpp")) {
        reportDocumentOpen(ctx, false, false);
        return false;
    }
    return true;
}

static void commitNewProject(gx_app_context* ctx) {
    stopProjectSearch(ctx);
    ProjectCreateRequest request = {};
    copyText(request.parentPath, sizeof(request.parentPath), g_projectDialog.parentPath);
    copyText(request.folderName, sizeof(request.folderName), g_projectDialog.folderName);
    copyText(request.projectId, sizeof(request.projectId), g_projectDialog.projectId);
    copyText(request.displayName, sizeof(request.displayName), g_projectDialog.displayName);
    request.kind = ProjectKind::NativeGuiApplication;
    ProjectOperationResult result;
    if (!WorkspaceControllerCreateProject(&g_controller, request, &result)) {
        if (result.rollbackAttempted && result.rollbackSucceeded) logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER project_rollback=PASS");
        reportProjectFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER project_create=FAIL", result.error);
        g_inputMode = InputMode::ProjectCreate;
        return;
    }
    g_inputMode = InputMode::Normal;
    openCreatedProject(ctx, result);
}

static void commitProjectOpen(gx_app_context* ctx) {
    stopProjectSearch(ctx);
    if (WorkspaceControllerOpenProject(&g_controller, g_prompt)) {
        writeOutput("Project opened");
        logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER project_open=PASS");
        logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER project_metadata_parse=PASS");
        logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER project_target=guidexos.amd64.hosted.native");
        if (!WorkspaceControllerOpenDocument(&g_controller, "src/main.cpp")) reportDocumentOpen(ctx, false, false);
    } else {
        reportProjectFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER project_open=FAIL", g_controller.lastProjectError);
    }
    g_inputMode = InputMode::Normal;
}

static bool commitWorkspaceOpen(gx_app_context* ctx) {
    stopProjectSearch(ctx);
    bool success = WorkspaceControllerOpenWorkspace(&g_controller, g_pendingWorkspacePath);
    reportWorkspaceOpen(ctx, success);
    g_inputMode = InputMode::Normal;
    return success;
}

static void requestWorkspaceOpen(gx_app_context* ctx) {
    if (g_controller.model.open && guidexos::developer_studio::WorkspaceModelHasDirtyDocuments(&g_controller.model)) {
        copyText(g_pendingWorkspacePath, sizeof(g_pendingWorkspacePath), g_prompt);
        g_inputMode = InputMode::ConfirmWorkspace;
        writeOutput("Unsaved changes: Save, Discard, or Cancel");
        return;
    }
    copyText(g_pendingWorkspacePath, sizeof(g_pendingWorkspacePath), g_prompt);
    commitWorkspaceOpen(ctx);
}

static void closeActiveDocument(gx_app_context* ctx, CloseDecision decision) {
    if (g_pendingDocument >= kMaxOpenDocuments) return;
    bool success = WorkspaceControllerCloseDocument(&g_controller, g_pendingDocument, decision);
    if (success) {
        writeOutput("Document closed");
        logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER document_close=PASS");
    } else {
        outputError("Document close failed");
    }
}

static void finishApplicationClose(gx_app_context* ctx, bool success) {
    if (!success) {
        writeOutput("Save failed; application remains open");
        markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER application_close=FAIL", currentError());
        return;
    }
    if (RunControllerIsActive(&g_runController)) {
        g_inputMode = InputMode::ConfirmRunClose;
        writeOutput("Project application is running: Close it first or keep Studio open");
        return;
    }
    logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER application_close=PASS");
    logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER clean_close=PASS");
    stopProjectSearch(ctx);
    g_inputMode = InputMode::Normal;
    g_requestExit = true;
}

static char mapKeyToChar(int keyCode, int modifiers) {
    bool shift = (modifiers & GX_KEY_MOD_SHIFT) != 0;
    if (keyCode >= 65 && keyCode <= 90) return shift ? static_cast<char>(keyCode) : static_cast<char>(keyCode + ('a' - 'A'));
    if (keyCode >= 48 && keyCode <= 57) {
        if (!shift) return static_cast<char>(keyCode);
        const char* symbols = ")!@#$%^&*(";
        return symbols[keyCode - 48];
    }
    switch (keyCode) {
    case 32: return ' ';
    case 186: return shift ? ':' : ';';
    case 187: return shift ? '+' : '=';
    case 188: return shift ? '<' : ',';
    case 189: return shift ? '_' : '-';
    case 190: return shift ? '>' : '.';
    case 191: return shift ? '?' : '/';
    case 192: return shift ? '~' : '`';
    case 219: return shift ? '{' : '[';
    case 220: return shift ? '|' : static_cast<char>(92);
    case 221: return shift ? '}' : ']';
    case 222: return shift ? '"' : '\'';
    default: return '\0';
    }
}

static void promptBackspace() {
    uint32_t length = lengthOf(g_prompt, sizeof(g_prompt));
    if (length > 0) g_prompt[length - 1] = '\0';
}

static void promptAppend(int keyCode, int modifiers) {
    char value = mapKeyToChar(keyCode, modifiers);
    uint32_t length = lengthOf(g_prompt, sizeof(g_prompt));
    if (value != '\0' && length + 1 < kMaxPromptBytes && length + 1 < sizeof(g_prompt)) {
        g_prompt[length] = value;
        g_prompt[length + 1] = '\0';
    }
}

static void drawText(gx_app_context* ctx, int x, int y, const char* text) {
    if (ctx && ctx->host && ctx->host->draw_text && text) ctx->host->draw_text(ctx, g_window, x, y, text);
}

static void drawPanel(gx_app_context* ctx, gx_rect rect, uint32_t color) {
    if (ctx && ctx->host && ctx->host->draw_rect) ctx->host->draw_rect(ctx, g_window, rect.x, rect.y, rect.width, rect.height, color);
}

static void compose(char* output, uint32_t size, const char* prefix, const char* value, const char* suffix) {
    copyText(output, size, prefix);
    appendText(output, size, value ? value : "-");
    appendText(output, size, suffix);
}

static void findFieldDisplay(char* output, uint32_t outputSize, const char* field) {
    if (!output || outputSize == 0) return;
    copyText(output, outputSize, field ? field : "");
    const uint32_t limit = outputSize > 3 ? outputSize - 3 : 0;
    if (limit != 0 && lengthOf(output, outputSize) > limit) {
        output[limit] = '.';
        output[limit + 1] = '.';
        output[limit + 2] = '\0';
    }
}

static void findStatusText(char* output, uint32_t outputSize) {
    if (!output || outputSize == 0) return;
    output[0] = '\0';
    if (g_findTransientStatus[0] != '\0') {
        copyText(output, outputSize, g_findTransientStatus);
        return;
    }
    if (g_findSession.error == FindErrorCode::MatchLimitReached) {
        copyText(output, outputSize, "Search results truncated.");
    } else if (g_findSession.error == FindErrorCode::QueryTooLarge) {
        copyText(output, outputSize, "Query too large");
    } else if (g_findSession.error == FindErrorCode::DocumentTooLarge) {
        copyText(output, outputSize, "Document too large");
    } else if (g_findSession.matchCount == 0) {
        copyText(output, outputSize, "No matches");
    } else {
        appendUnsigned(output, outputSize, g_findSession.currentMatchIndex >= 0 ?
                       static_cast<uint32_t>(g_findSession.currentMatchIndex + 1) : 0);
        appendText(output, outputSize, " / ");
        appendUnsigned(output, outputSize, g_findSession.matchCount);
    }
}

static void drawFindBar(gx_app_context* ctx) {
    if (!g_findBarOpen) return;
    drawPanel(ctx, { 270, 0, 690, 48 }, 0x304365u);
    drawText(ctx, 278, kFindQueryY, "Find:");
    drawPanel(ctx, { kFindFieldX, 4, kFindFieldWidth, 18 }, g_findField == FindField::Query ? 0x405775u : 0x202A36u);
    findFieldDisplay(g_textScratch, sizeof(g_textScratch), g_findSession.query);
    drawText(ctx, kFindFieldX + 5, kFindQueryY, g_textScratch);
    if (g_findReplaceMode) {
        drawText(ctx, 278, kFindReplaceY, "Replace:");
        drawPanel(ctx, { kFindFieldX, 26, kFindFieldWidth, 18 }, g_findField == FindField::Replacement ? 0x405775u : 0x202A36u);
        findFieldDisplay(g_textScratch, sizeof(g_textScratch), g_findSession.replacement);
        drawText(ctx, kFindFieldX + 5, kFindReplaceY, g_textScratch);
        drawText(ctx, kFindReplaceButtonX, kFindReplaceY, "Replace");
        drawText(ctx, kFindReplaceAllX, kFindReplaceY,
                 FindCanReplaceAll(&g_findSession) ? "Replace All" : "Replace All(disabled)");
    }
    drawText(ctx, kFindPreviousX, kFindQueryY, "Previous");
    drawText(ctx, kFindNextX, kFindQueryY, "Next");
    drawText(ctx, kFindCaseX, kFindQueryY, g_findSession.options.caseSensitive ? "[x]Case" : "[ ]Case");
    drawText(ctx, kFindWordX, kFindQueryY, g_findSession.options.wholeWord ? "[x]Word" : "[ ]Word");
    drawText(ctx, kFindWrapX, kFindQueryY, g_findSession.options.wrapAround ? "[x]Wrap" : "[ ]Wrap");
    drawText(ctx, kFindCloseX, kFindQueryY, "Close");
    findStatusText(g_textScratch, sizeof(g_textScratch));
    drawText(ctx, 890, kFindReplaceY, g_textScratch);
}

static uint32_t projectSearchTotalRows() {
    uint32_t rows = 0;
    const uint32_t groups = ProjectSearchQueryResultGroups(&g_projectSearch);
    for (uint32_t i = 0; i < groups; ++i) {
        const ProjectSearchFileGroup* group = ProjectSearchResultGroupAt(&g_projectSearch, i);
        if (group) rows += 1 + group->matchCount;
    }
    return rows;
}

static bool projectSearchMatchForRow(uint32_t row, uint32_t* outGroup, uint32_t* outMatch) {
    uint32_t current = 0;
    const uint32_t groups = ProjectSearchQueryResultGroups(&g_projectSearch);
    for (uint32_t groupIndex = 0; groupIndex < groups; ++groupIndex) {
        const ProjectSearchFileGroup* group = ProjectSearchResultGroupAt(&g_projectSearch, groupIndex);
        if (!group) continue;
        if (row == current) return false; // file header
        ++current;
        if (row < current + group->matchCount) {
            if (outGroup) *outGroup = groupIndex;
            if (outMatch) *outMatch = row - current;
            return true;
        }
        current += group->matchCount;
    }
    return false;
}

static uint32_t projectSearchSelectedRow() {
    uint32_t row = 0;
    const uint32_t groups = ProjectSearchQueryResultGroups(&g_projectSearch);
    for (uint32_t i = 0; i < groups; ++i) {
        const ProjectSearchFileGroup* group = ProjectSearchResultGroupAt(&g_projectSearch, i);
        if (!group) continue;
        if (i == g_projectSearchSelectedGroup) return row + 1 + g_projectSearchSelectedMatch;
        row += 1 + group->matchCount;
    }
    return 0;
}

static void projectSearchEnsureSelectionVisible() {
    const uint32_t row = projectSearchSelectedRow();
    if (row < g_projectSearchScroll) g_projectSearchScroll = row;
    if (row >= g_projectSearchScroll + kSearchPanelMaxRows)
        g_projectSearchScroll = row - kSearchPanelMaxRows + 1;
}

static void projectSearchMoveSelection(int32_t delta) {
    const uint32_t groups = ProjectSearchQueryResultGroups(&g_projectSearch);
    if (groups == 0) return;
    uint32_t groupIndex = g_projectSearchSelectedGroup;
    uint32_t matchIndex = g_projectSearchSelectedMatch;
    if (groupIndex >= groups) { groupIndex = 0; matchIndex = 0; }
    if (delta > 0) {
        const ProjectSearchFileGroup* group = ProjectSearchResultGroupAt(&g_projectSearch, groupIndex);
        if (group && matchIndex + 1 < group->matchCount) ++matchIndex;
        else if (groupIndex + 1 < groups) { ++groupIndex; matchIndex = 0; }
    } else {
        if (matchIndex > 0) --matchIndex;
        else if (groupIndex > 0) {
            --groupIndex;
            const ProjectSearchFileGroup* group = ProjectSearchResultGroupAt(&g_projectSearch, groupIndex);
            matchIndex = group && group->matchCount > 0 ? group->matchCount - 1 : 0;
        }
    }
    g_projectSearchSelectedGroup = groupIndex;
    g_projectSearchSelectedMatch = matchIndex;
    projectSearchEnsureSelectionVisible();
}

static void drawProjectSearchPanel(gx_app_context* ctx) {
    if (!g_projectSearchPanelOpen) return;
    drawPanel(ctx, { 8, kSearchPanelTop + 4, 944, 580 }, 0x263650u);
    drawText(ctx, 24, 78, "Find in Files");
    drawText(ctx, 24, 103, "Find:");
    drawText(ctx, 24, kSearchIncludeY, "Include:");
    drawText(ctx, 24, kSearchExcludeY, "Exclude:");
    drawPanel(ctx, { kSearchFieldX, 87, kSearchFieldWidth, 20 },
              g_projectSearchField == ProjectSearchField::Query && !g_projectSearchResultsFocused ? 0x405775u : 0x202A36u);
    drawPanel(ctx, { kSearchFieldX, kSearchIncludeY - 17, kSearchFieldWidth, 20 },
              g_projectSearchField == ProjectSearchField::Include && !g_projectSearchResultsFocused ? 0x405775u : 0x202A36u);
    drawPanel(ctx, { kSearchFieldX, kSearchExcludeY - 17, kSearchFieldWidth, 20 },
              g_projectSearchField == ProjectSearchField::Exclude && !g_projectSearchResultsFocused ? 0x405775u : 0x202A36u);
    findFieldDisplay(g_textScratch, sizeof(g_textScratch), g_projectSearchDraft.query);
    drawText(ctx, kSearchFieldX + 5, 102, g_textScratch);
    findFieldDisplay(g_textScratch, sizeof(g_textScratch), g_projectSearchDraft.includePattern);
    drawText(ctx, kSearchFieldX + 5, kSearchIncludeY - 2, g_textScratch);
    findFieldDisplay(g_textScratch, sizeof(g_textScratch), g_projectSearchDraft.excludePattern);
    drawText(ctx, kSearchFieldX + 5, kSearchExcludeY - 2, g_textScratch);
    drawText(ctx, 470, 103, g_projectSearchDraft.caseSensitive ? "[x]Case" : "[ ]Case");
    drawText(ctx, 560, 103, g_projectSearchDraft.wholeWord ? "[x]Whole word" : "[ ]Whole word");
    const ProjectSearchOperation* operation = ProjectSearchOperationInfo(&g_projectSearch);
    const bool active = ProjectSearchIsActive(&g_projectSearch);
    drawPanel(ctx, { 760, 87, 72, 20 }, 0x34496Au);
    drawText(ctx, 770, 102, active ? "Running" : "Search");
    drawPanel(ctx, { 840, 87, 72, 20 }, active ? 0xA96F2Au : 0x34496Au);
    drawText(ctx, 851, 102, "Cancel");
    if (active && operation) {
        copyText(g_textScratch, sizeof(g_textScratch), ProjectSearchStateName(operation->state));
        appendText(g_textScratch, sizeof(g_textScratch), "  ");
        appendUnsigned(g_textScratch, sizeof(g_textScratch), static_cast<uint32_t>(operation->filesSearched));
        appendText(g_textScratch, sizeof(g_textScratch), " / ");
        appendUnsigned(g_textScratch, sizeof(g_textScratch), static_cast<uint32_t>(operation->filesEnumerated));
        appendText(g_textScratch, sizeof(g_textScratch), " files, ");
        appendUnsigned(g_textScratch, sizeof(g_textScratch), operation->resultMatchCount);
        appendText(g_textScratch, sizeof(g_textScratch), " matches");
        drawText(ctx, 24, 177, g_textScratch);
    } else if (g_projectSearchStatus[0] != '\0') {
        drawText(ctx, 24, 177, g_projectSearchStatus);
    } else if (!g_controller.model.hasProject) {
        drawText(ctx, 24, 177, "Open a project to search its files.");
    }
    drawPanel(ctx, { 16, kSearchPanelResultsTop - 12, 928, 1 }, 0x405775u);
    uint32_t row = 0;
    const uint32_t firstRow = g_projectSearchScroll;
    const uint32_t lastRow = firstRow + kSearchPanelMaxRows;
    const uint32_t groups = ProjectSearchQueryResultGroups(&g_projectSearch);
    for (uint32_t groupIndex = 0; groupIndex < groups; ++groupIndex) {
        const ProjectSearchFileGroup* group = ProjectSearchResultGroupAt(&g_projectSearch, groupIndex);
        if (!group) continue;
        if (row >= firstRow && row < lastRow) {
            copyText(g_textScratch, sizeof(g_textScratch), group->relativePath);
            appendText(g_textScratch, sizeof(g_textScratch), " (");
            appendUnsigned(g_textScratch, sizeof(g_textScratch), group->matchCount);
            appendText(g_textScratch, sizeof(g_textScratch), ")");
            drawText(ctx, 24, kSearchPanelResultsTop + static_cast<int>((row - firstRow) * kSearchPanelRowHeight), g_textScratch);
        }
        ++row;
        for (uint32_t matchIndex = 0; matchIndex < group->matchCount; ++matchIndex, ++row) {
            if (row < firstRow || row >= lastRow) continue;
            const ProjectSearchMatch* match = ProjectSearchResultMatchAt(&g_projectSearch, group, matchIndex);
            if (!match) continue;
            const bool selected = g_projectSearchResultsFocused && groupIndex == g_projectSearchSelectedGroup &&
                matchIndex == g_projectSearchSelectedMatch;
            const int y = kSearchPanelResultsTop + static_cast<int>((row - firstRow) * kSearchPanelRowHeight);
            if (selected) drawPanel(ctx, { 20, y - 12, 920, kSearchPanelRowHeight }, 0x34496Au);
            copyText(g_textScratch, sizeof(g_textScratch), "  ");
            appendUnsigned(g_textScratch, sizeof(g_textScratch), match->line);
            appendText(g_textScratch, sizeof(g_textScratch), ":");
            appendUnsigned(g_textScratch, sizeof(g_textScratch), match->column);
            appendText(g_textScratch, sizeof(g_textScratch), "  ");
            if (match->previewLeftTruncated) appendText(g_textScratch, sizeof(g_textScratch), "...");
            appendText(g_textScratch, sizeof(g_textScratch), match->preview);
            if (match->previewRightTruncated) appendText(g_textScratch, sizeof(g_textScratch), "...");
            drawText(ctx, 32, y, g_textScratch);
        }
    }
    if (groups == 0 && operation && !active) drawText(ctx, 24, kSearchPanelResultsTop, "No matches.");
    if (groups == 0 && active) drawText(ctx, 24, kSearchPanelResultsTop, "Scanning project files...");
    if (groups != 0 && projectSearchTotalRows() > kSearchPanelMaxRows)
        drawText(ctx, 760, 177, "Scroll: mouse wheel / Up / Down");
}

static void drawEditorRangeOverlay(gx_app_context* ctx, const TextBuffer& buffer, uint32_t lineStart,
                                   uint32_t lineEnd, uint64_t rangeStart, uint64_t rangeEnd,
                                   uint32_t color, uint32_t rowY) {
    const uint32_t clippedStart = rangeStart > lineStart ? static_cast<uint32_t>(rangeStart) : lineStart;
    const uint32_t clippedEnd = rangeEnd < lineEnd ? static_cast<uint32_t>(rangeEnd) : lineEnd;
    if (clippedEnd <= clippedStart) return;
    const uint32_t visibleStart = g_editorScrollColumn;
    const uint32_t visibleEnd = visibleStart + kVisibleEditorColumns;
    const uint32_t visualStart = TextBufferVisualColumn(&buffer, lineStart, clippedStart, kEditorTabWidth);
    const uint32_t visualEnd = TextBufferVisualColumn(&buffer, lineStart, clippedEnd, kEditorTabWidth);
    if (visualEnd <= visibleStart || visualStart >= visibleEnd) return;
    const uint32_t drawStart = visualStart > visibleStart ? visualStart : visibleStart;
    const uint32_t drawStop = visualEnd < visibleEnd ? visualEnd : visibleEnd;
    uint32_t output = 0;
    uint32_t visual = visualStart;
    for (uint32_t offset = clippedStart; offset < clippedEnd && output + 1 < sizeof(g_lineScratch); ++offset) {
        const char value = buffer.data[offset];
        const uint32_t width = value == '\t' ? kEditorTabWidth - (visual % kEditorTabWidth) : 1;
        const uint32_t characterStart = visual;
        const uint32_t characterEnd = visual + width;
        if (characterEnd > drawStart && characterStart < drawStop) {
            const uint32_t from = characterStart < drawStart ? drawStart : characterStart;
            const uint32_t to = characterEnd > drawStop ? drawStop : characterEnd;
            for (uint32_t column = from; column < to && output + 1 < sizeof(g_lineScratch); ++column)
                g_lineScratch[output++] = value == '\t' ? ' ' : value;
        }
        visual = characterEnd;
    }
    if (output == 0) return;
    g_lineScratch[output] = '\0';
    drawPanel(ctx, { kEditorTextX + static_cast<int>((drawStart - visibleStart) * 8u),
                     static_cast<int>(rowY) - 12, static_cast<int>(output * 8u), 14 }, color);
    drawText(ctx, kEditorTextX + static_cast<int>((drawStart - visibleStart) * 8u), rowY, g_lineScratch);
}

static void drawFindOverlays(gx_app_context* ctx, const TextBuffer& buffer, uint32_t lineStart,
                             uint32_t lineEnd, uint32_t rowY) {
    if (!g_findBarOpen || g_findSession.matchCount == 0 || lineEnd <= lineStart) return;
    const uint32_t count = FindVisibleMatchIndices(&g_findSession, lineStart, lineEnd,
                                                   g_findVisibleIndices, kFindVisibleOverlayCapacity);
    for (uint32_t i = 0; i < count; ++i) {
        const uint32_t matchIndex = g_findVisibleIndices[i];
        const FindMatch& match = g_findSession.matches[matchIndex];
        const bool current = static_cast<int32_t>(matchIndex) == g_findSession.currentMatchIndex;
        drawEditorRangeOverlay(ctx, buffer, lineStart, lineEnd, match.start,
                                match.start + match.length, current ? 0xA96F2Au : 0x405775u, rowY);
    }
}

static void drawExplorer(gx_app_context* ctx) {
    drawText(ctx, 16, 66, "EXPLORER");
    if (!g_controller.model.open) {
        drawText(ctx, 16, 98, "No workspace open");
        drawText(ctx, 16, 126, "File -> New/Open Project");
        drawText(ctx, 16, 144, "or Open Workspace");
        return;
    }
    compose(g_textScratch, sizeof(g_textScratch), g_controller.model.hasProject ? "Project: " : "Workspace: ", g_controller.model.displayName, "");
    drawText(ctx, 16, 86, g_textScratch);
    if (g_controller.model.hasProject) {
        compose(g_textScratch, sizeof(g_textScratch), "Kind: ", guidexos::developer_studio::ToString(g_controller.model.project.kind), "");
        drawText(ctx, 16, 102, g_textScratch);
    }
    compose(g_textScratch, sizeof(g_textScratch), "Path: ", g_controller.model.browsePath[0] ? g_controller.model.browsePath : "/", "");
    drawText(ctx, 16, g_controller.model.hasProject ? 118 : 102, g_textScratch);
    const uint32_t visibleExplorerEntries = g_controller.model.hasProject ? 9u : 10u;
    for (uint32_t i = 0; i < g_controller.model.entryCount && i < visibleExplorerEntries; ++i) {
        int y = kEntryTop + static_cast<int>(i) * kEntryHeight;
        if (i == g_controller.model.selectedEntry) drawPanel(ctx, { 8, y - 13, 252, 18 }, 0x34496Au);
        const char* prefix = g_controller.model.entries[i].kind == WorkspaceEntryKind::Directory ? "[DIR] " :
            (g_controller.model.entries[i].kind == WorkspaceEntryKind::SupportedTextFile ? "[TXT] " : "[BIN] ");
        compose(g_textScratch, sizeof(g_textScratch), prefix, g_controller.model.entries[i].name, "");
        drawText(ctx, 16, y, g_textScratch);
    }
    if (g_controller.listingTruncated) drawText(ctx, 16, 484, "Entry limit reached");
    drawText(ctx, 16, 502, "Enter: open   Backspace: up   F5: refresh");
}

static void drawOutline(gx_app_context* ctx) {
    drawPanel(ctx, { 8, kOutlineTop - 24, 252, 202 }, 0x202A36u);
    drawText(ctx, 16, kOutlineTop - 7, "OUTLINE");
    const uint32_t documentIndex = activeOutlineDocumentIndex();
    const SymbolDocument* document = SymbolDatabaseDocumentAt(&g_symbolDatabase, documentIndex);
    if (!document || document->symbolCount == 0) {
        drawText(ctx, 16, kOutlineTop + 15, "No symbols");
        return;
    }
    ensureOutlineSelectionVisible();
    const uint32_t end = g_outlineScroll + static_cast<uint32_t>(kOutlineMaxRows) < document->symbolCount ?
        g_outlineScroll + static_cast<uint32_t>(kOutlineMaxRows) : document->symbolCount;
    for (uint32_t row = g_outlineScroll; row < end; ++row) {
        const DocumentSymbol* symbol = SymbolDatabaseDocumentSymbolAt(&g_symbolDatabase, documentIndex, row);
        if (!symbol) continue;
        const int y = kOutlineTop + 15 + static_cast<int>(row - g_outlineScroll) * kOutlineRowHeight;
        if (row == g_outlineSelected) drawPanel(ctx, { 10, y - 13, 246, kOutlineRowHeight }, 0x34496Au);
        copyText(g_textScratch, sizeof(g_textScratch), SymbolKindPrefix(symbol->kind));
        appendText(g_textScratch, sizeof(g_textScratch), " ");
        appendText(g_textScratch, sizeof(g_textScratch), symbol->name);
        drawText(ctx, 16 + static_cast<int>(symbol->depth > 4 ? 4 : symbol->depth) * 6, y, g_textScratch);
    }
}

static void drawSymbolSearch(gx_app_context* ctx) {
    if (!g_symbolSearchOpen) return;
    drawPanel(ctx, { 176, 62, 608, 536 }, 0x2A3852u);
    drawText(ctx, 198, 90, "Search Symbols");
    drawText(ctx, 600, 90, g_symbolSearchCaseSensitive ? "[x]Case" : "[ ]Case");
    drawPanel(ctx, { 198, 102, 560, 24 }, 0x111722u);
    findFieldDisplay(g_textScratch, sizeof(g_textScratch), g_symbolSearchQuery);
    drawText(ctx, 206, 120, g_textScratch);
    if (g_symbolSearchResultCount == 0) {
        drawText(ctx, 206, 154, "No symbols found.");
        return;
    }
    const uint32_t end = g_symbolSearchScroll + static_cast<uint32_t>(kSymbolSearchMaxRows) < g_symbolSearchResultCount ?
        g_symbolSearchScroll + static_cast<uint32_t>(kSymbolSearchMaxRows) : g_symbolSearchResultCount;
    for (uint32_t row = g_symbolSearchScroll; row < end; ++row) {
        const ProjectSymbol* symbol = SymbolDatabaseProjectSymbolAt(&g_symbolDatabase, g_symbolSearchResults[row]);
        if (!symbol) continue;
        const int y = 150 + static_cast<int>(row - g_symbolSearchScroll) * kSymbolSearchRowHeight;
        if (row == g_symbolSearchSelected) drawPanel(ctx, { 198, y - 13, 560, kSymbolSearchRowHeight }, 0x405775u);
        copyText(g_textScratch, sizeof(g_textScratch), SymbolKindPrefix(symbol->symbol.kind));
        appendText(g_textScratch, sizeof(g_textScratch), " ");
        if (symbol->symbol.container[0] != '\0') {
            appendText(g_textScratch, sizeof(g_textScratch), symbol->symbol.container);
            appendText(g_textScratch, sizeof(g_textScratch), "::");
        }
        appendText(g_textScratch, sizeof(g_textScratch), symbol->symbol.name);
        drawText(ctx, 206, y, g_textScratch);
    }
    drawText(ctx, 206, 576, "Up/Down Select   Enter Open   Esc Close   Ctrl+I Case");
}

static uint32_t activeLine(const TextBuffer& buffer) {
    uint32_t line = 0;
    for (uint32_t i = 0; i < buffer.caret && i < buffer.length; ++i) if (buffer.data[i] == '\n') ++line;
    return line;
}

static uint32_t activeColumn(const TextBuffer& buffer, uint32_t line) {
    return buffer.caret - TextBufferLineStart(&buffer, line);
}

static void keepCaretVisible(const Document* document) {
    if (!document) return;
    const uint32_t line = activeLine(document->buffer);
    const uint32_t lineStart = TextBufferLineStart(&document->buffer, line);
    const uint32_t column = TextBufferVisualColumn(&document->buffer, lineStart, document->buffer.caret, kEditorTabWidth);
    if (line < g_editorScrollLine) g_editorScrollLine = line;
    if (line >= g_editorScrollLine + kVisibleEditorLines) g_editorScrollLine = line - kVisibleEditorLines + 1;
    if (column < g_editorScrollColumn) g_editorScrollColumn = column;
    if (column >= g_editorScrollColumn + kVisibleEditorColumns) g_editorScrollColumn = column - kVisibleEditorColumns + 1;
}

static Document* findDocumentById(uint64_t documentId) {
    if (documentId == 0) return nullptr;
    for (uint32_t i = 0; i < kMaxOpenDocuments; ++i) {
        if (g_controller.model.documents[i].used && g_controller.model.documents[i].documentId == documentId)
            return &g_controller.model.documents[i];
    }
    return nullptr;
}

static void findSetStatus(const char* text) {
    copyText(g_findTransientStatus, sizeof(g_findTransientStatus), text ? text : "");
}

static void saveFindSessionToDocument() {
    Document* document = findDocumentById(g_findSession.documentId);
    if (document) FindCopyStateFromSession(&document->find, g_findSession);
}

static void findRecompute(Document* document, bool preserveCurrent) {
    if (!document) return;
    uint64_t previousStart = 0;
    bool hadPrevious = false;
    const FindMatch* previous = FindCurrentMatch(&g_findSession);
    if (preserveCurrent && previous) { previousStart = previous->start; hadPrevious = true; }
    FindSearch(&g_findSession, document->documentId, document->buffer.generation,
               document->buffer.data, document->buffer.length);
    if (hadPrevious) {
        for (uint32_t i = 0; i < g_findSession.matchCount; ++i) {
            if (g_findSession.matches[i].start == previousStart) {
                FindSetCurrentMatch(&g_findSession, static_cast<int32_t>(i));
                break;
            }
        }
    }
}

static void ensureFindDocument(Document* document) {
    if (!document) {
        if (g_findSession.documentId != 0) FindSessionInit(&g_findSession);
        return;
    }
    if (g_findSession.documentId != document->documentId) {
        saveFindSessionToDocument();
        FindSessionInit(&g_findSession);
        FindCopyStateToSession(&g_findSession, document->find);
        findRecompute(document, false);
    } else if (FindSessionIsStale(&g_findSession, document->documentId, document->buffer.generation)) {
        // A byte edit can shift an old range onto an unrelated occurrence.
        // Recompute the retained list but clear the current selection; the
        // next explicit navigation must establish a fresh current match.
        findRecompute(document, false);
    }
}

static void findSelectMatch(Document* document, int32_t index) {
    if (!document || index < 0 || index >= static_cast<int32_t>(g_findSession.matchCount)) return;
    const FindMatch& match = g_findSession.matches[index];
    if (!ValidateTextRange(&document->buffer, match.start, match.length)) return;
    if (!FindSetCurrentMatch(&g_findSession, index)) return;
    SelectTextRange(&document->buffer, match.start, match.length);
    g_editorFocused = true;
    keepCaretVisible(document);
}

static void findRefreshFromCaret(Document* document, bool selectInitial) {
    if (!document) return;
    const uint32_t caret = GetCaretOffset(&document->buffer);
    FindSearch(&g_findSession, document->documentId, document->buffer.generation,
               document->buffer.data, document->buffer.length);
    if (selectInitial && g_findSession.matchCount != 0) {
        bool wrapped = false;
        const int32_t index = FindNavigate(&g_findSession, caret, FindDirection::Forward, &wrapped);
        if (index >= 0) {
            findSelectMatch(document, index);
            if (wrapped) findSetStatus("Wrapped to beginning");
        }
    }
    saveFindSessionToDocument();
}

static void findNavigate(Document* document, FindDirection direction) {
    if (!document) { findSetStatus("No active document"); return; }
    ensureFindDocument(document);
    bool wrapped = false;
    const int32_t index = FindNavigate(&g_findSession, GetCaretOffset(&document->buffer), direction, &wrapped);
    if (index < 0) {
        findSetStatus(g_findSession.error == FindErrorCode::MatchLimitReached ?
                      "Search results truncated." : "No matches");
        return;
    }
    findSelectMatch(document, index);
    if (wrapped) findSetStatus(direction == FindDirection::Forward ? "Wrapped to beginning" : "Wrapped to end");
    else findSetStatus("");
    saveFindSessionToDocument();
}

static char* findFieldBuffer() {
    return g_findField == FindField::Query ? g_findSession.query : g_findSession.replacement;
}

static uint32_t findFieldCapacity() {
    return g_findField == FindField::Query ? sizeof(g_findSession.query) : sizeof(g_findSession.replacement);
}

static void findFieldChanged(Document* document) {
    if (g_findField == FindField::Query) {
        FindSetQuery(&g_findSession, g_findSession.query);
        if (document) findRefreshFromCaret(document, true);
    } else {
        FindSetReplacement(&g_findSession, g_findSession.replacement);
        saveFindSessionToDocument();
    }
}

static void findInsertCharacter(Document* document, char value) {
    if (value == '\0') return;
    char* field = findFieldBuffer();
    const uint32_t capacity = findFieldCapacity();
    const uint32_t length = lengthOf(field, capacity);
    if (g_findFieldCaret > length || length + 1 >= capacity) return;
    for (uint32_t i = length; i > g_findFieldCaret; --i) field[i] = field[i - 1];
    field[g_findFieldCaret++] = value;
    field[length + 1] = '\0';
    findFieldChanged(document);
}

static void findBackspace(Document* document) {
    char* field = findFieldBuffer();
    const uint32_t length = lengthOf(field, findFieldCapacity());
    if (g_findFieldCaret == 0 || g_findFieldCaret > length) return;
    for (uint32_t i = g_findFieldCaret - 1; i < length; ++i) field[i] = field[i + 1];
    --g_findFieldCaret;
    findFieldChanged(document);
}

static void findDelete(Document* document) {
    char* field = findFieldBuffer();
    const uint32_t length = lengthOf(field, findFieldCapacity());
    if (g_findFieldCaret >= length) return;
    for (uint32_t i = g_findFieldCaret; i < length; ++i) field[i] = field[i + 1];
    findFieldChanged(document);
}

static void openFindBar(bool replaceMode, bool initializeQuery) {
    Document* document = WorkspaceControllerActiveDocument(&g_controller);
    ensureFindDocument(document);
    findSetStatus("");
    g_findBarOpen = true;
    g_findReplaceMode = replaceMode;
    g_findField = FindField::Query;
    if (initializeQuery && document) {
        bool useSelection = document->buffer.selectionActive;
        uint32_t selectionStart = document->buffer.selectionAnchor < document->buffer.caret ?
            document->buffer.selectionAnchor : document->buffer.caret;
        uint32_t selectionEnd = document->buffer.selectionAnchor < document->buffer.caret ?
            document->buffer.caret : document->buffer.selectionAnchor;
        if (!useSelection || selectionEnd <= selectionStart || selectionEnd - selectionStart > kFindMaxQueryBytes) useSelection = false;
        if (useSelection) {
            for (uint32_t i = selectionStart; i < selectionEnd; ++i) {
                if (document->buffer.data[i] == '\n' || document->buffer.data[i] == '\r') { useSelection = false; break; }
            }
        }
        if (useSelection) {
            uint32_t length = selectionEnd - selectionStart;
            for (uint32_t i = 0; i < length; ++i) g_findSession.query[i] = document->buffer.data[selectionStart + i];
            g_findSession.query[length] = '\0';
            FindSetQuery(&g_findSession, g_findSession.query);
            findRefreshFromCaret(document, true);
        } else {
            findRefreshFromCaret(document, true);
        }
    }
    g_findFieldCaret = lengthOf(g_findSession.query, sizeof(g_findSession.query));
    g_editorFocused = false;
}

static void closeFindBar() {
    saveFindSessionToDocument();
    g_findBarOpen = false;
    g_findReplaceMode = false;
    g_editorFocused = true;
    findSetStatus("");
}

static bool activateProjectSearchResult(gx_app_context* ctx, uint32_t groupIndex, uint32_t matchIndex) {
    const ProjectSearchOperation* operation = ProjectSearchOperationInfo(&g_projectSearch);
    const ProjectSearchFileGroup* group = ProjectSearchResultGroupAt(&g_projectSearch, groupIndex);
    const ProjectSearchMatch* match = ProjectSearchResultMatchAt(&g_projectSearch, group, matchIndex);
    if (!operation || !group || !match || !g_controller.model.open || !g_controller.model.hasProject) {
        projectSearchSetStatus("Search result cannot be activated: no active project");
        return false;
    }
    if (operation->projectGeneration != g_controller.model.projectGeneration ||
        !searchTextEqual(g_projectSearchProjectId, g_controller.model.project.projectId)) {
        projectSearchSetStatus("Search result belongs to a stale project");
        markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER project_search_result_activate=STALE", ProjectSearchErrorName(ProjectSearchErrorCode::ProjectStale));
        return false;
    }
    if (group->relativePath[0] == '\0' || guidexos::developer_studio::PathContainsTraversal(group->relativePath) ||
        group->relativePath[0] == '/' || group->relativePath[0] == '\\' || group->relativePath[1] == ':') {
        projectSearchSetStatus("Search result path rejected");
        markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER project_search_result_activate=FAIL", ProjectSearchErrorName(ProjectSearchErrorCode::PathOutsideProject));
        return false;
    }
    char absolutePath[kMaxPathBytes] = {};
    if (!JoinWorkspacePath(g_controller.model.rootPath, group->relativePath, absolutePath, sizeof(absolutePath))) {
        projectSearchSetStatus("Search result path is outside the project");
        markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER project_search_result_activate=FAIL", ProjectSearchErrorName(ProjectSearchErrorCode::PathOutsideProject));
        return false;
    }
    uint32_t documentIndex = kMaxOpenDocuments;
    OutputErrorCode navigationError = OutputErrorCode::None;
    if (!WorkspaceControllerOpenDocumentAtLocation(&g_controller, g_controller.model.project.projectId,
                                                   group->relativePath, match->line, match->column,
                                                   &documentIndex, &navigationError)) {
        projectSearchSetStatus("Unable to open search result");
        markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER project_search_result_activate=FAIL", OutputErrorName(navigationError));
        return false;
    }
    (void)absolutePath;
    Document* document = WorkspaceControllerActiveDocument(&g_controller);
    if (!document) {
        projectSearchSetStatus("Search result opened without a document");
        return false;
    }
    const uint32_t queryLength = lengthOf(ProjectSearchQuery(&g_projectSearch), kFindMaxQueryBytes + 1u);
    bool selectedExact = match->byteOffset <= document->buffer.length &&
        match->matchLength == queryLength &&
        match->matchLength <= document->buffer.length - static_cast<uint32_t>(match->byteOffset) &&
        FindLiteralMatchesAt(document->buffer.data, document->buffer.length, match->byteOffset,
                             ProjectSearchQuery(&g_projectSearch), queryLength,
                             operation->options.caseSensitive, operation->options.wholeWord);
    if (selectedExact && SelectTextRange(&document->buffer, match->byteOffset, match->matchLength)) {
        g_editorFocused = true;
        keepCaretVisible(document);
        g_projectSearchPanelOpen = false;
        g_projectSearchResultsFocused = false;
        logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER project_search_result_activate=PASS");
        return true;
    }
    const uint32_t line = match->line > 0 ? match->line - 1 : 0;
    const uint32_t lineStart = TextBufferLineStart(&document->buffer, line);
    uint32_t nearbyEnd = TextBufferLineEnd(&document->buffer, line);
    if (nearbyEnd < document->buffer.length) nearbyEnd += 1;
    if (nearbyEnd > lineStart + 8192u) nearbyEnd = lineStart + 8192u;
    bool selectedNearby = false;
    uint32_t nearbyOffset = lineStart;
    for (; nearbyOffset < nearbyEnd; ++nearbyOffset) {
        if (FindLiteralMatchesAt(document->buffer.data, document->buffer.length, nearbyOffset,
                                 ProjectSearchQuery(&g_projectSearch), queryLength,
                                 operation->options.caseSensitive, operation->options.wholeWord)) {
            selectedNearby = SelectTextRange(&document->buffer, nearbyOffset, queryLength);
            if (selectedNearby) break;
        }
    }
    g_editorFocused = true;
    if (selectedNearby) {
        keepCaretVisible(document);
        g_projectSearchPanelOpen = false;
        g_projectSearchResultsFocused = false;
        projectSearchSetStatus("Search result was stale; nearby match selected");
        logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER project_search_result_activate=STALE");
        return true;
    }
    bool clamped = false;
    WorkspaceControllerSetCaretPosition(&g_controller, documentIndex, match->line, match->column,
                                        &clamped, &navigationError);
    keepCaretVisible(document);
    g_projectSearchPanelOpen = false;
    g_projectSearchResultsFocused = false;
    projectSearchSetStatus("Search result may be stale");
    logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER project_search_result_activate=STALE");
    return true;
}

static bool replaceCurrentMatch(Document* document) {
    if (!document) { findSetStatus("No active document"); return false; }
    ensureFindDocument(document);
    const FindMatch* current = FindCurrentMatch(&g_findSession);
    if (!current || !ValidateTextRange(&document->buffer, current->start, current->length)) {
        findSetStatus("No current match");
        return false;
    }
    const uint64_t start = current->start;
    const uint32_t oldLength = static_cast<uint32_t>(current->length);
    if (!guidexos::developer_studio::FindMatchTextStillValid(&g_findSession, document->buffer.data,
                                                              document->buffer.length, *current)) {
        findRecompute(document, false);
        findSetStatus("Search changed; match refreshed");
        return false;
    }
    const uint32_t replacementLength = lengthOf(g_findSession.replacement, sizeof(g_findSession.replacement));
    if (!ReplaceTextRange(&document->buffer, start, oldLength, g_findSession.replacement, replacementLength)) {
        findSetStatus("Replacement rejected");
        return false;
    }
    DocumentUpdateSyntax(document);
    if (g_controller.model.activeDocument < kMaxOpenDocuments &&
        &g_controller.model.documents[g_controller.model.activeDocument] == document)
        WorkspaceControllerUpdateDocumentSymbols(&g_controller, g_controller.model.activeDocument);
    ensureFindDocument(document);
    FindSearch(&g_findSession, document->documentId, document->buffer.generation,
               document->buffer.data, document->buffer.length);
    g_findSession.currentMatchIndex = -1;
    bool wrapped = false;
    const int32_t next = FindNavigate(&g_findSession, start + replacementLength, FindDirection::Forward, &wrapped);
    if (next >= 0) findSelectMatch(document, next);
    if (wrapped) findSetStatus("Wrapped to beginning");
    else findSetStatus("");
    saveFindSessionToDocument();
    return true;
}

static uint32_t replaceAllMatches(Document* document) {
    if (!document) { findSetStatus("No active document"); return 0; }
    ensureFindDocument(document);
    if (g_findSession.truncated) {
        findSetStatus("Search results truncated.");
        return 0;
    }
    if (g_findSession.matchCount == 0) { findSetStatus("No matches"); return 0; }
    const uint32_t count = g_findSession.matchCount;
    const uint32_t replacementLength = lengthOf(g_findSession.replacement, sizeof(g_findSession.replacement));
    if (!ReplaceTextRanges(&document->buffer, g_findSession.matches, count,
                           g_findSession.replacement, replacementLength)) {
        findSetStatus("Replace All rejected");
        return 0;
    }
    DocumentUpdateSyntax(document);
    if (g_controller.model.activeDocument < kMaxOpenDocuments &&
        &g_controller.model.documents[g_controller.model.activeDocument] == document)
        WorkspaceControllerUpdateDocumentSymbols(&g_controller, g_controller.model.activeDocument);
    const uint32_t caret = GetCaretOffset(&document->buffer);
    FindSearch(&g_findSession, document->documentId, document->buffer.generation,
               document->buffer.data, document->buffer.length);
    g_findSession.currentMatchIndex = -1;
    bool wrapped = false;
    const int32_t next = FindNavigate(&g_findSession, caret, FindDirection::Forward, &wrapped);
    if (next >= 0) findSelectMatch(document, next);
    copyText(g_findTransientStatus, sizeof(g_findTransientStatus), "Replaced ");
    appendUnsigned(g_findTransientStatus, sizeof(g_findTransientStatus), count);
    appendText(g_findTransientStatus, sizeof(g_findTransientStatus), count == 1 ? " match" : " matches");
    saveFindSessionToDocument();
    return count;
}

static void updateSyntaxAfterEdit(gx_app_context* ctx, Document* document) {
    if (!document || !document->buffer.lastMutationValid) return;
    const uint32_t startLine = document->buffer.lastMutationFirstLine;
    DocumentUpdateSyntax(document);
    if (g_controller.model.activeDocument < kMaxOpenDocuments &&
        &g_controller.model.documents[g_controller.model.activeDocument] == document)
        WorkspaceControllerUpdateDocumentSymbols(&g_controller, g_controller.model.activeDocument);
    if (document->syntax.lastUpdateWasIncremental && !g_syntaxIncrementalMarkerReported) {
        g_syntaxIncrementalMarkerReported = true;
        copyText(g_textScratch, sizeof(g_textScratch), "GUIDEXOS_DEVELOPER_STUDIO_MARKER syntax_incremental_tokenize=PASS start=");
        appendUnsigned(g_textScratch, sizeof(g_textScratch), startLine);
        appendText(g_textScratch, sizeof(g_textScratch), " count=");
        appendUnsigned(g_textScratch, sizeof(g_textScratch), document->syntax.lastRetokenizedLineCount);
        logMarker(ctx, g_textScratch);
        if (document->syntax.lastUpdateConverged && !g_syntaxConvergenceMarkerReported) {
            g_syntaxConvergenceMarkerReported = true;
            copyText(g_textScratch, sizeof(g_textScratch), "GUIDEXOS_DEVELOPER_STUDIO_MARKER syntax_state_converged=PASS line=");
            appendUnsigned(g_textScratch, sizeof(g_textScratch), document->syntax.lastConvergenceLine);
            logMarker(ctx, g_textScratch);
        }
    } else if (document->syntax.fallback && !g_syntaxFallbackMarkerReported) {
        g_syntaxFallbackMarkerReported = true;
        copyText(g_textScratch, sizeof(g_textScratch), "GUIDEXOS_DEVELOPER_STUDIO_MARKER syntax_fallback=TRUE reason=");
        appendText(g_textScratch, sizeof(g_textScratch), SyntaxErrorName(document->syntax.fallbackCode));
        logMarker(ctx, g_textScratch);
    }
}

static void drawTabs(gx_app_context* ctx) {
    int x = 278;
    for (uint32_t i = 0; i < kMaxOpenDocuments; ++i) {
        if (!g_controller.model.documents[i].used) continue;
        int width = 126;
        drawPanel(ctx, { x, 52, width, 26 }, i == g_controller.model.activeDocument ? 0x34496Au : 0x202A36u);
        compose(g_textScratch, sizeof(g_textScratch), g_controller.model.documents[i].buffer.dirty ? "* " : "", g_controller.model.documents[i].name, "");
        drawText(ctx, x + 6, 69, g_textScratch);
        x += width + 4;
    }
}

static void drawEditorTextRun(gx_app_context* ctx, const TextBuffer& buffer, uint32_t lineStart,
                              uint32_t lineEnd, const SyntaxRenderRun& run, uint32_t rowY) {
    if (run.length == 0 || run.start > lineEnd - lineStart || run.length > lineEnd - lineStart - run.start) return;
    const uint32_t runStart = lineStart + run.start;
    const uint32_t runEnd = runStart + run.length;
    const uint32_t visualStart = TextBufferVisualColumn(&buffer, lineStart, runStart, kEditorTabWidth);
    const uint32_t visualEnd = TextBufferVisualColumn(&buffer, lineStart, runEnd, kEditorTabWidth);
    const uint32_t visibleStart = g_editorScrollColumn;
    const uint32_t visibleEnd = visibleStart + kVisibleEditorColumns;
    if (visualEnd <= visibleStart || visualStart >= visibleEnd) return;
    const uint32_t drawStart = visualStart > visibleStart ? visualStart : visibleStart;
    const uint32_t drawStop = visualEnd < visibleEnd ? visualEnd : visibleEnd;
    uint32_t output = 0;
    uint32_t visual = visualStart;
    for (uint32_t offset = runStart; offset < runEnd && output + 1 < sizeof(g_lineScratch); ++offset) {
        const char value = buffer.data[offset];
        const uint32_t width = value == '\t' ? kEditorTabWidth - (visual % kEditorTabWidth) : 1;
        const uint32_t characterStart = visual;
        const uint32_t characterEnd = visual + width;
        if (characterEnd > drawStart && characterStart < drawStop) {
            const uint32_t from = characterStart < drawStart ? drawStart : characterStart;
            const uint32_t to = characterEnd > drawStop ? drawStop : characterEnd;
            for (uint32_t column = from; column < to && output + 1 < sizeof(g_lineScratch); ++column)
                g_lineScratch[output++] = value == '\t' ? ' ' : value;
        }
        visual = characterEnd;
    }
    if (output == 0) return;
    g_lineScratch[output] = '\0';
    const int x = kEditorTextX + static_cast<int>((drawStart - visibleStart) * 8u);
    const SyntaxPalette& palette = DefaultSyntaxPalette();
    const uint32_t color = SyntaxPaletteColor(palette, run.kind, run.selected);
    if (run.kind != SyntaxTokenKind::PlainText || run.selected)
        drawPanel(ctx, { x, static_cast<int>(rowY) - 12, static_cast<int>(output * 8u), 14 }, color);
    drawText(ctx, x, rowY, g_lineScratch);
}

static void drawEditor(gx_app_context* ctx) {
    drawTabs(ctx);
    Document* document = WorkspaceControllerActiveDocument(&g_controller);
    if (!document) {
        drawText(ctx, 300, 120, g_controller.model.hasProject ? "Project loaded" : "Welcome to guideXOS Developer Studio");
        if (g_controller.model.hasProject) {
            compose(g_textScratch, sizeof(g_textScratch), "Application ID: ", g_controller.model.project.projectId, "");
            drawText(ctx, 300, 150, g_textScratch);
            compose(g_textScratch, sizeof(g_textScratch), "Target: ", g_controller.model.project.targetProfileId, "");
            drawText(ctx, 300, 180, g_textScratch);
        } else {
            drawText(ctx, 300, 150, "Open a workspace, or create/open a project.");
            drawText(ctx, 300, 180, "Highlighting is lexical, deterministic, and bounded.");
        }
        return;
    }
    DocumentUpdateSyntax(document);
    compose(g_textScratch, sizeof(g_textScratch), "Document: ", document->name, document->buffer.dirty ? "  Modified" : "");
    appendText(g_textScratch, sizeof(g_textScratch), "  Syntax: ");
    appendText(g_textScratch, sizeof(g_textScratch), SyntaxLanguageName(document->syntax.language));
    if (document->syntax.fallback) {
        appendText(g_textScratch, sizeof(g_textScratch), " (disabled: ");
        appendText(g_textScratch, sizeof(g_textScratch), SyntaxErrorName(document->syntax.fallbackCode));
        appendText(g_textScratch, sizeof(g_textScratch), ")");
    }
    drawText(ctx, 300, 96, g_textScratch);
    const uint32_t lineCount = TextBufferLineCount(&document->buffer);
    if (g_editorScrollLine >= lineCount) g_editorScrollLine = lineCount == 0 ? 0 : lineCount - 1;
    uint32_t visibleLinesDrawn = 0;
    for (uint32_t row = 0; row < kVisibleEditorLines; ++row) {
        const uint32_t line = g_editorScrollLine + row;
        if (line >= lineCount) break;
        ++visibleLinesDrawn;
        const uint32_t start = TextBufferLineStart(&document->buffer, line);
        const uint32_t end = TextBufferLineEnd(&document->buffer, line);
        const uint32_t length = end > start ? end - start : 0;
        compose(g_textScratch, sizeof(g_textScratch), "", "", "");
        appendUnsigned(g_textScratch, sizeof(g_textScratch), line + 1);
        appendText(g_textScratch, sizeof(g_textScratch), " ");
        drawText(ctx, kEditorLineNumberX, kEditorTop + static_cast<int>(row) * kEditorLineHeight, g_textScratch);
        uint32_t spanCount = 0;
        const SyntaxTokenSpan* spans = SyntaxCacheLineSpans(&document->syntax, line, &spanCount);
        uint32_t selectionStart = document->buffer.selectionAnchor < document->buffer.caret ?
            document->buffer.selectionAnchor : document->buffer.caret;
        uint32_t selectionEnd = document->buffer.selectionAnchor < document->buffer.caret ?
            document->buffer.caret : document->buffer.selectionAnchor;
        SyntaxSelection selection = { document->buffer.selectionActive && selectionEnd > selectionStart,
                                      selectionStart > start ? selectionStart - start : 0,
                                      selectionEnd > start ? selectionEnd - start : 0 };
        if (selection.end > length) selection.end = length;
        if (selection.start > selection.end) selection.start = selection.end;
        uint32_t runCount = SyntaxBuildRenderRuns(spans, spanCount, length, selection, g_renderRuns,
                                                  sizeof(g_renderRuns) / sizeof(g_renderRuns[0]));
        if (runCount == 0 && length != 0) {
            g_renderRuns[0].start = 0;
            g_renderRuns[0].length = length;
            g_renderRuns[0].kind = SyntaxTokenKind::PlainText;
            g_renderRuns[0].selected = false;
            runCount = 1;
        }
        for (uint32_t run = 0; run < runCount; ++run)
            drawEditorTextRun(ctx, document->buffer, start, end, g_renderRuns[run],
                              kEditorTop + static_cast<int>(row) * kEditorLineHeight);
        drawFindOverlays(ctx, document->buffer, start, end,
                         kEditorTop + static_cast<int>(row) * kEditorLineHeight);
    }
    if (!g_syntaxRenderMarkerReported) {
        g_syntaxRenderMarkerReported = true;
        copyText(g_textScratch, sizeof(g_textScratch), "GUIDEXOS_DEVELOPER_STUDIO_MARKER syntax_render_visible=PASS lines=");
        appendUnsigned(g_textScratch, sizeof(g_textScratch), visibleLinesDrawn);
        logMarker(ctx, g_textScratch);
    }
    const uint32_t line = activeLine(document->buffer);
    const uint32_t column = TextBufferVisualColumn(&document->buffer, TextBufferLineStart(&document->buffer, line),
                                                   document->buffer.caret, kEditorTabWidth);
    if (line >= g_editorScrollLine && line < g_editorScrollLine + kVisibleEditorLines) {
        const uint32_t caretColumn = column > g_editorScrollColumn ? column - g_editorScrollColumn : 0;
        if (caretColumn <= kVisibleEditorColumns)
            drawPanel(ctx, { kEditorTextX + static_cast<int>(caretColumn * 8u),
                             kEditorTop - 12 + static_cast<int>(line - g_editorScrollLine) * kEditorLineHeight, 2, 14 }, 0xD6E4FFu);
    }
}

static void drawOutputAndStatus(gx_app_context* ctx) {
    drawText(ctx, 16, 536, g_outputProblemsTab ? "PROBLEMS" : "OUTPUT");
    drawText(ctx, 112, 536, "Output");
    drawText(ctx, 172, 536, "Problems");
    if (!g_outputProblemsTab) {
        compose(g_textScratch, sizeof(g_textScratch), "Channel: ", OutputChannelName(OutputServiceActiveChannel(&g_outputService)), "");
        drawText(ctx, 260, 536, g_textScratch);
    } else {
        uint32_t warnings = 0;
        uint32_t errors = 0;
        const char* projectId = g_controller.model.hasProject ? g_controller.model.project.projectId : nullptr;
        OutputServiceProblemCounts(&g_outputService, projectId, &warnings, &errors);
        compose(g_textScratch, sizeof(g_textScratch), "Errors: ", "", "");
        appendUnsigned(g_textScratch, sizeof(g_textScratch), errors);
        appendText(g_textScratch, sizeof(g_textScratch), "  Warnings: ");
        appendUnsigned(g_textScratch, sizeof(g_textScratch), warnings);
        drawText(ctx, 260, 536, g_textScratch);
    }
    drawText(ctx, 850, 536, "Clear");
    const uint32_t visibleRows = 5;
    const char* projectId = g_controller.model.hasProject ? g_controller.model.project.projectId : nullptr;
    const uint32_t count = g_outputProblemsTab ? OutputServiceProblemCount(&g_outputService, projectId) :
        OutputServiceFilteredCount(&g_outputService, OutputServiceActiveChannel(&g_outputService));
    const uint32_t maximumScroll = count > visibleRows ? count - visibleRows : 0;
    if (g_outputFollowTail) g_outputScroll = maximumScroll;
    if (g_outputScroll > maximumScroll) g_outputScroll = maximumScroll;
    for (uint32_t row = 0; row < visibleRows; ++row) {
        const uint32_t index = g_outputScroll + row;
        const OutputRecord* record = g_outputProblemsTab ? OutputServiceProblemAt(&g_outputService, projectId, index) :
            OutputServiceFilteredAt(&g_outputService, OutputServiceActiveChannel(&g_outputService), index);
        if (!record) continue;
        if (g_outputProblemsTab) {
            copyText(g_textScratch, sizeof(g_textScratch), OutputSeverityName(record->severity));
            appendText(g_textScratch, sizeof(g_textScratch), " ");
            appendText(g_textScratch, sizeof(g_textScratch), record->diagnosticCode[0] ? record->diagnosticCode : "-");
            appendText(g_textScratch, sizeof(g_textScratch), " ");
            appendText(g_textScratch, sizeof(g_textScratch), record->text);
            if (record->hasLocation) {
                appendText(g_textScratch, sizeof(g_textScratch), " | ");
                appendText(g_textScratch, sizeof(g_textScratch), record->relativeFilePath);
                appendText(g_textScratch, sizeof(g_textScratch), ":");
                appendUnsigned(g_textScratch, sizeof(g_textScratch), record->line);
                appendText(g_textScratch, sizeof(g_textScratch), ":");
                appendUnsigned(g_textScratch, sizeof(g_textScratch), record->column);
            }
            appendText(g_textScratch, sizeof(g_textScratch), " [");
            appendText(g_textScratch, sizeof(g_textScratch), OutputSourceName(record->source));
            appendText(g_textScratch, sizeof(g_textScratch), "]");
        } else {
            copyText(g_textScratch, sizeof(g_textScratch), "[");
            appendText(g_textScratch, sizeof(g_textScratch), OutputSourceName(record->source));
            appendText(g_textScratch, sizeof(g_textScratch), "] ");
            appendText(g_textScratch, sizeof(g_textScratch), OutputSeverityName(record->severity));
            appendText(g_textScratch, sizeof(g_textScratch), ": ");
            appendText(g_textScratch, sizeof(g_textScratch), record->text);
            if (record->stream == OutputStream::StandardError) appendText(g_textScratch, sizeof(g_textScratch), " [stderr]");
        }
        if (g_outputProblemsTab && index == g_problemSelected) drawPanel(ctx, { 8, 544 + static_cast<int>(row) * 15, 940, 15 }, 0x34496Au);
        drawText(ctx, 16, 556 + static_cast<int>(row) * 15, g_textScratch);
    }
    if (g_outputProblemsTab && count == 0) drawText(ctx, 16, 556, "No problems found.");
    compose(g_textScratch, sizeof(g_textScratch), "Target: ", InitialTargetProfile().displayName, " | Experimental");
    drawText(ctx, 16, 660, g_textScratch);
    compose(g_textScratch, sizeof(g_textScratch), "Build: ", BuildStateName(g_buildController.state), BuildControllerIsActive(&g_buildController) ? " (active)" : "");
    drawText(ctx, 650, 660, g_textScratch);
    compose(g_textScratch, sizeof(g_textScratch), "Run: ", RunStateName(g_runController.state), RunControllerIsActive(&g_runController) ? " (active)" : "");
    drawText(ctx, 650, 678, g_textScratch);
    compose(g_textScratch, sizeof(g_textScratch), g_controller.model.hasProject ? "Project: " : "Workspace: ", g_controller.model.open ? g_controller.model.displayName : "-", "");
    drawText(ctx, 16, 678, g_textScratch);
    if (g_controller.model.hasProject) {
        compose(g_textScratch, sizeof(g_textScratch), "ID: ", g_controller.model.project.projectId, "");
        drawText(ctx, 300, 660, g_textScratch);
    }
    Document* document = WorkspaceControllerActiveDocument(&g_controller);
    if (document) {
        uint32_t line = activeLine(document->buffer);
        uint32_t column = activeColumn(document->buffer, line);
        compose(g_textScratch, sizeof(g_textScratch), "Document: ", document->name, document->buffer.dirty ? " Modified" : "");
        appendText(g_textScratch, sizeof(g_textScratch), "  Line ");
        appendUnsigned(g_textScratch, sizeof(g_textScratch), line + 1);
        appendText(g_textScratch, sizeof(g_textScratch), ", Column ");
        appendUnsigned(g_textScratch, sizeof(g_textScratch), column + 1);
        drawText(ctx, 300, 678, g_textScratch);
    }
}

static void drawModal(gx_app_context* ctx) {
    if (g_inputMode == InputMode::Normal) return;
    drawPanel(ctx, { 180, 180, 600, 180 }, 0x2A3852u);
    if (g_inputMode == InputMode::WorkspacePath || g_inputMode == InputMode::ProjectPath) {
        drawText(ctx, 210, 220, g_inputMode == InputMode::ProjectPath ? "Open Project" : "Open Workspace (path entry)");
        drawText(ctx, 210, 250, "Enter an absolute hosted root or metadata path:");
        drawPanel(ctx, { 210, 265, 540, 30 }, 0x111722u);
        drawText(ctx, 220, 285, g_prompt);
        drawText(ctx, 210, 325, "Enter: open   Escape: cancel");
        return;
    }
    if (g_inputMode == InputMode::ProjectCreate) {
        drawPanel(ctx, { 150, 110, 660, 410 }, 0x2A3852u);
        drawText(ctx, 180, 142, "New Project");
        drawText(ctx, 180, 166, "Native GUI Application (only supported template)");
        const char* labels[] = { "Display name", "Parent location", "Folder name (optional)", "Application ID" };
        const char* values[] = { g_projectDialog.displayName, g_projectDialog.parentPath, g_projectDialog.folderName, g_projectDialog.projectId };
        for (uint32_t i = 0; i < 4; ++i) {
            int y = 196 + static_cast<int>(i) * 56;
            drawText(ctx, 180, y, labels[i]);
            drawPanel(ctx, { 180, y + 8, 600, 28 }, i == g_projectDialog.field ? 0x111722u : 0x202A36u);
            drawText(ctx, 190, y + 28, values[i]);
        }
        drawText(ctx, 180, 450, "Tab/Enter: next field   Enter on Application ID: create");
        drawText(ctx, 180, 474, "Folder is derived from the display name when blank; Escape cancels");
        return;
    }
    if (g_inputMode == InputMode::ConfirmBuild) {
        drawText(ctx, 210, 220, "Build Project");
        drawText(ctx, 210, 246, "Project documents have unsaved changes.");
        drawText(ctx, 210, 270, "Save All before building?");
        drawText(ctx, 230, 316, "[S] Save All     [C] Cancel");
        return;
    }
    if (g_inputMode == InputMode::ConfirmRun) {
        drawText(ctx, 210, 220, "Run Project");
        drawText(ctx, 210, 246, "Run builds the project before every launch.");
        drawText(ctx, 210, 270, "Project documents have unsaved changes.");
        drawText(ctx, 230, 316, "[S] Save All and Run     [C] Cancel");
        return;
    }
    if (g_inputMode == InputMode::ConfirmRunClose) {
        drawText(ctx, 210, 220, "Project Application is running");
        drawText(ctx, 210, 246, "Request a close for the temporary development app?");
        drawText(ctx, 230, 292, "[C] Close Project Application     [K] Keep Studio Open");
        return;
    }
    drawText(ctx, 210, 220, "Unsaved changes");
    if (g_inputMode == InputMode::ConfirmDocument) drawText(ctx, 210, 246, "Save changes before closing this document?");
    else if (g_inputMode == InputMode::ConfirmWorkspace) drawText(ctx, 210, 246, "Save changes before opening another workspace?");
    else drawText(ctx, 210, 246, "Save changes before closing Developer Studio?");
    drawText(ctx, 230, 292, "[S] Save     [D] Discard     [C] Cancel");
}

static void drawShell(gx_app_context* ctx) {
    drawPanel(ctx, kWindowRect, 0x151B28u);
    drawPanel(ctx, kCommandRect, 0x243451u);
    drawPanel(ctx, kExplorerRect, 0x1D2636u);
    drawPanel(ctx, kEditorRect, 0x111722u);
    drawPanel(ctx, kOutputRect, 0x202A36u);
    drawPanel(ctx, kStatusRect, 0x243451u);
    drawText(ctx, 16, 30, "guideXOS Developer Studio");
    drawText(ctx, 290, 30, "File");
    drawText(ctx, 700, 30, "Build");
    drawText(ctx, 340, 30, "Save");
    drawText(ctx, 400, 30, "Save All");
    drawText(ctx, 490, 30, "Refresh");
    drawText(ctx, 575, 30, "Ctrl+N New Project");
    drawExplorer(ctx);
    drawOutline(ctx);
    drawEditor(ctx);
    drawOutputAndStatus(ctx);
    drawFindBar(ctx);
    drawProjectSearchPanel(ctx);
    drawSymbolSearch(ctx);
    if (g_fileMenuOpen) {
        drawPanel(ctx, { 8, 42, 250, 158 }, 0x34496Au);
        drawText(ctx, 20, 64, "New Project");
        drawText(ctx, 20, 86, "Open Project");
        drawText(ctx, 20, 108, "Open Workspace");
        drawText(ctx, 20, 130, "Close Document");
        drawText(ctx, 20, 152, "Close Workspace");
        drawText(ctx, 20, 174, "Exit");
    }
    if (g_buildMenuOpen) {
        drawPanel(ctx, { 300, 42, 260, 92 }, 0x34496Au);
        drawText(ctx, 312, 68, "Build Project");
        drawText(ctx, 312, 92, "Run Project (F5)");
        drawText(ctx, 312, 116, "Request Project Close");
    }
    drawModal(ctx);
}

static void selectDocumentTab(int x) {
    int tabX = 278;
    for (uint32_t i = 0; i < kMaxOpenDocuments; ++i) {
        if (!g_controller.model.documents[i].used) continue;
        if (x >= tabX && x < tabX + 126) {
            g_controller.model.activeDocument = i;
            g_editorFocused = true;
            keepCaretVisible(&g_controller.model.documents[i]);
            return;
        }
        tabX += 130;
    }
}

static void placeCaretFromMouse(Document* document, int x, int y) {
    if (!document) return;
    int row = (y - kEditorTop + 12) / kEditorLineHeight;
    if (row < 0) row = 0;
    uint32_t line = g_editorScrollLine + static_cast<uint32_t>(row);
    uint32_t lineCount = TextBufferLineCount(&document->buffer);
    if (line >= lineCount) line = lineCount == 0 ? 0 : lineCount - 1;
    int column = (x - kEditorTextX + 4) / 8;
    if (column < 0) column = 0;
    uint32_t start = TextBufferLineStart(&document->buffer, line);
    uint32_t end = TextBufferLineEnd(&document->buffer, line);
    const uint32_t requested = TextBufferOffsetForVisualColumn(&document->buffer, start, end,
                                                               g_editorScrollColumn + static_cast<uint32_t>(column), kEditorTabWidth);
    SetCaretOffset(&document->buffer, requested > end ? end : requested);
    keepCaretVisible(document);
}

static bool navigateSelectedProblem(gx_app_context* ctx) {
    const char* projectId = g_controller.model.hasProject ? g_controller.model.project.projectId : nullptr;
    const OutputRecord* record = OutputServiceProblemAt(&g_outputService, projectId, g_problemSelected);
    if (!record) return false;
    if (!g_controller.model.hasProject) {
        writeStudioOutput("Unable to open diagnostic location: no active project.");
        markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER problem_navigation=FAIL", OutputErrorName(OutputErrorCode::NavigationNoProject));
        return false;
    }
    if (!record->hasLocation) {
        writeStudioOutput("Unable to open diagnostic location: no navigable file.");
        markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER problem_navigation=FAIL", OutputErrorName(OutputErrorCode::NavigationOpenFailed));
        return false;
    }
    uint32_t documentIndex = kMaxOpenDocuments;
    OutputErrorCode error = OutputErrorCode::None;
    if (!WorkspaceControllerOpenDocumentAtLocation(&g_controller, record->projectId, record->relativeFilePath,
                                                   record->line, record->column, &documentIndex, &error)) {
        copyText(g_textScratch, sizeof(g_textScratch), "Unable to open diagnostic location: ");
        appendText(g_textScratch, sizeof(g_textScratch), OutputErrorName(error));
        writeStudioOutput(g_textScratch);
        markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER problem_navigation=FAIL", OutputErrorName(error));
        return false;
    }
    Document* document = WorkspaceControllerActiveDocument(&g_controller);
    if (!document) {
        markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER problem_navigation=FAIL", OutputErrorName(OutputErrorCode::NavigationOpenFailed));
        return false;
    }
    g_editorFocused = true;
    g_outputFocused = false;
    uint32_t line = record->line > 0 ? record->line - 1 : 0;
    const uint32_t lineCount = TextBufferLineCount(&document->buffer);
    if (line >= lineCount) line = lineCount > 0 ? lineCount - 1 : 0;
    if (line < g_editorScrollLine) g_editorScrollLine = line;
    if (line >= g_editorScrollLine + kVisibleEditorLines) g_editorScrollLine = line - kVisibleEditorLines + 1;
    logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER problem_navigation=PASS");
    return true;
}

static uint32_t outputVisibleCount() {
    const char* projectId = g_controller.model.hasProject ? g_controller.model.project.projectId : nullptr;
    return g_outputProblemsTab ? OutputServiceProblemCount(&g_outputService, projectId) :
        OutputServiceFilteredCount(&g_outputService, OutputServiceActiveChannel(&g_outputService));
}

static void handleOutputKey(gx_app_context* ctx, int keyCode) {
    const uint32_t count = outputVisibleCount();
    if (keyCode == GX_KEY_UP) {
        if (g_outputProblemsTab && g_problemSelected > 0) --g_problemSelected;
        if (g_outputScroll > 0) { --g_outputScroll; g_outputFollowTail = false; }
    } else if (keyCode == GX_KEY_DOWN) {
        if (g_outputProblemsTab && g_problemSelected + 1 < count) ++g_problemSelected;
        if (g_outputScroll + 5 < count) { ++g_outputScroll; g_outputFollowTail = false; }
    } else if (keyCode == 13 && g_outputProblemsTab) {
        navigateSelectedProblem(ctx);
    } else if (keyCode == 27) {
        g_outputFocused = false;
    }
}

static void handleModalKey(gx_app_context* ctx, int keyCode, int action, int modifiers) {
    if (action != GX_KEY_ACTION_DOWN) return;
    if (g_inputMode == InputMode::ProjectCreate) {
        if (keyCode == 27) { g_inputMode = InputMode::Normal; writeOutput("Project creation canceled"); return; }
        if (keyCode == 9) {
            if (modifiers & GX_KEY_MOD_SHIFT) g_projectDialog.field = g_projectDialog.field == 0 ? 3 : g_projectDialog.field - 1;
            else g_projectDialog.field = (g_projectDialog.field + 1) % 4;
            return;
        }
        if (keyCode == 13) {
            if (g_projectDialog.field < 3) { ++g_projectDialog.field; return; }
            commitNewProject(ctx);
            return;
        }
        if (keyCode == 8) { projectDialogBackspace(); return; }
        projectDialogAppend(keyCode, modifiers);
        return;
    }
    if (g_inputMode == InputMode::ProjectPath) {
        if (keyCode == 27) { g_inputMode = InputMode::Normal; return; }
        if (keyCode == 13) { commitProjectOpen(ctx); return; }
        if (keyCode == 8) { promptBackspace(); return; }
        promptAppend(keyCode, modifiers);
        return;
    }
    if (g_inputMode == InputMode::WorkspacePath) {
        if (keyCode == 27) { g_inputMode = InputMode::Normal; return; }
        if (keyCode == 13) {
            copyText(g_pendingWorkspacePath, sizeof(g_pendingWorkspacePath), g_prompt);
            requestWorkspaceOpen(ctx);
            return;
        }
        if (keyCode == 8) { promptBackspace(); return; }
        promptAppend(keyCode, modifiers);
        return;
    }
    if (g_inputMode == InputMode::ConfirmBuild) {
        if (keyCode == 27 || keyCode == 67 || keyCode == 99) {
            g_inputMode = InputMode::Normal;
            markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER build_precondition=FAIL", BuildErrorName(BuildErrorCode::UserCancelled));
            writeOutput("Build canceled");
            return;
        }
        if (keyCode == 83 || keyCode == 115) {
            g_inputMode = InputMode::Normal;
            beginBuild(ctx, BuildDirtyDecision::SaveAll);
        }
        return;
    }
    if (g_inputMode == InputMode::ConfirmRun) {
        if (keyCode == 27 || keyCode == 67 || keyCode == 99) {
            g_inputMode = InputMode::Normal;
            markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER run_precondition=FAIL", RunErrorName(RunErrorCode::UserCancelled));
            writeOutput("Run canceled");
            return;
        }
        if (keyCode == 83 || keyCode == 115) {
            g_inputMode = InputMode::Normal;
            beginRunOperation(ctx);
            g_runWaitingForBuild = true;
            if (!beginBuild(ctx, BuildDirtyDecision::SaveAll)) {
                g_runWaitingForBuild = false;
                completeRunWithoutDeployment("Build could not start; deployment skipped");
            }
        }
        return;
    }
    if (g_inputMode == InputMode::ConfirmRunClose) {
        if (keyCode == 67 || keyCode == 99) {
            g_inputMode = InputMode::Normal;
            requestRunClose(ctx);
        } else if (keyCode == 75 || keyCode == 107 || keyCode == 27) {
            g_inputMode = InputMode::Normal;
            writeOutput("Studio remains open");
            logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER run_close=CANCEL");
        }
        return;
    }
    if (keyCode == 27 || keyCode == 67 || keyCode == 99) {
        if (g_inputMode == InputMode::ConfirmDocument) logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER unsaved_close=CANCEL");
        else if (g_inputMode == InputMode::ConfirmWorkspace) logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER unsaved_close=CANCEL");
        else logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER unsaved_close=CANCEL");
        g_inputMode = InputMode::Normal;
        return;
    }
    bool save = keyCode == 83 || keyCode == 115;
    bool discard = keyCode == 68 || keyCode == 100;
    if (!save && !discard) return;
    if (g_inputMode == InputMode::ConfirmDocument) {
        logMarker(ctx, save ? "GUIDEXOS_DEVELOPER_STUDIO_MARKER unsaved_close=SAVE" : "GUIDEXOS_DEVELOPER_STUDIO_MARKER unsaved_close=DISCARD");
        closeActiveDocument(ctx, save ? CloseDecision::Save : CloseDecision::Discard);
        g_inputMode = InputMode::Normal;
    } else if (g_inputMode == InputMode::ConfirmWorkspace) {
        logMarker(ctx, save ? "GUIDEXOS_DEVELOPER_STUDIO_MARKER unsaved_close=SAVE" : "GUIDEXOS_DEVELOPER_STUDIO_MARKER unsaved_close=DISCARD");
        if (save && !saveAll(ctx)) return;
        stopProjectSearch(ctx);
        WorkspaceControllerCloseWorkspace(&g_controller, CloseDecision::Discard);
        if (g_workspaceSwitchPending) commitWorkspaceOpen(ctx);
        else {
            g_inputMode = InputMode::Normal;
            writeOutput("Workspace closed");
        }
    } else if (g_inputMode == InputMode::ConfirmApplication) {
        logMarker(ctx, save ? "GUIDEXOS_DEVELOPER_STUDIO_MARKER unsaved_close=SAVE" : "GUIDEXOS_DEVELOPER_STUDIO_MARKER unsaved_close=DISCARD");
        if (save && !saveAll(ctx)) return;
        finishApplicationClose(ctx, true);
    }
}

static bool handleFindKey(int keyCode, int action, int modifiers) {
    if (!g_findBarOpen || action != GX_KEY_ACTION_DOWN) return false;
    if (keyCode == 27) { closeFindBar(); return true; }
    if (modifiers & GX_KEY_MOD_CTRL) return false;
    Document* document = WorkspaceControllerActiveDocument(&g_controller);
    if (keyCode == 114) {
        findNavigate(document, (modifiers & GX_KEY_MOD_SHIFT) ? FindDirection::Backward : FindDirection::Forward);
        return true;
    }
    if (keyCode == 13) {
        if (g_findField == FindField::Replacement && g_findReplaceMode) replaceCurrentMatch(document);
        else findNavigate(document, (modifiers & GX_KEY_MOD_SHIFT) ? FindDirection::Backward : FindDirection::Forward);
        return true;
    }
    if (keyCode == 9 && g_findReplaceMode) {
        g_findField = g_findField == FindField::Query ? FindField::Replacement : FindField::Query;
        g_findFieldCaret = lengthOf(findFieldBuffer(), findFieldCapacity());
        return true;
    }
    if (keyCode == 8) { findBackspace(document); return true; }
    if (keyCode == 46) { findDelete(document); return true; }
    if (keyCode == GX_KEY_LEFT) {
        if (g_findFieldCaret > 0) --g_findFieldCaret;
        return true;
    }
    if (keyCode == GX_KEY_RIGHT) {
        const uint32_t length = lengthOf(findFieldBuffer(), findFieldCapacity());
        if (g_findFieldCaret < length) ++g_findFieldCaret;
        return true;
    }
    if (keyCode == 36) { g_findFieldCaret = 0; return true; }
    if (keyCode == 35) { g_findFieldCaret = lengthOf(findFieldBuffer(), findFieldCapacity()); return true; }
    const char value = mapKeyToChar(keyCode, modifiers);
    if (value != '\0') { findInsertCharacter(document, value); return true; }
    return false;
}

static void projectSearchInsertCharacter(char value) {
    if (value == '\0') return;
    char* field = projectSearchFieldBuffer();
    const uint32_t capacity = projectSearchFieldCapacity();
    const uint32_t length = lengthOf(field, capacity);
    if (g_projectSearchFieldCaret > length || length + 1 >= capacity) return;
    for (uint32_t i = length; i > g_projectSearchFieldCaret; --i) field[i] = field[i - 1];
    field[g_projectSearchFieldCaret++] = value;
    field[length + 1] = '\0';
}

static void projectSearchBackspace() {
    char* field = projectSearchFieldBuffer();
    const uint32_t length = lengthOf(field, projectSearchFieldCapacity());
    if (g_projectSearchFieldCaret == 0 || g_projectSearchFieldCaret > length) return;
    for (uint32_t i = g_projectSearchFieldCaret - 1; i < length; ++i) field[i] = field[i + 1];
    --g_projectSearchFieldCaret;
}

static void projectSearchDelete() {
    char* field = projectSearchFieldBuffer();
    const uint32_t length = lengthOf(field, projectSearchFieldCapacity());
    if (g_projectSearchFieldCaret >= length) return;
    for (uint32_t i = g_projectSearchFieldCaret; i < length; ++i) field[i] = field[i + 1];
}

static bool handleProjectSearchKey(gx_app_context* ctx, int keyCode, int action, int modifiers) {
    if (!g_projectSearchPanelOpen || action != GX_KEY_ACTION_DOWN) return false;
    const bool active = ProjectSearchIsActive(&g_projectSearch);
    if (keyCode == 27) {
        if (active) {
            ProjectSearchCancel(&g_projectSearch, g_projectSearchOperationId);
            projectSearchSetStatus("Cancelling search...");
            logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER project_search_cancel=REQUESTED");
        } else {
            g_projectSearchPanelOpen = false;
            g_projectSearchResultsFocused = false;
        }
        return true;
    }
    if (g_projectSearchResultsFocused) {
        if (keyCode == GX_KEY_UP) { projectSearchMoveSelection(-1); return true; }
        if (keyCode == GX_KEY_DOWN) { projectSearchMoveSelection(1); return true; }
        if (keyCode == 13) {
            activateProjectSearchResult(ctx, g_projectSearchSelectedGroup, g_projectSearchSelectedMatch);
            return true;
        }
        if (keyCode == 9) {
            g_projectSearchResultsFocused = false;
            g_projectSearchField = ProjectSearchField::Query;
            g_projectSearchFieldCaret = lengthOf(g_projectSearchDraft.query, sizeof(g_projectSearchDraft.query));
            return true;
        }
        return false;
    }
    if (keyCode == 13) {
        startProjectSearch(ctx);
        return true;
    }
    if (keyCode == 9) {
        if (modifiers & GX_KEY_MOD_SHIFT) {
            if (g_projectSearchField == ProjectSearchField::Query) {
                g_projectSearchResultsFocused = true;
                projectSearchEnsureSelectionVisible();
            } else {
                g_projectSearchField = static_cast<ProjectSearchField>(static_cast<int>(g_projectSearchField) - 1);
                g_projectSearchFieldCaret = lengthOf(projectSearchFieldBuffer(), projectSearchFieldCapacity());
            }
        } else if (g_projectSearchField == ProjectSearchField::Exclude) {
            g_projectSearchResultsFocused = true;
            projectSearchEnsureSelectionVisible();
        } else {
            g_projectSearchField = static_cast<ProjectSearchField>(static_cast<int>(g_projectSearchField) + 1);
            g_projectSearchFieldCaret = lengthOf(projectSearchFieldBuffer(), projectSearchFieldCapacity());
        }
        return true;
    }
    if (keyCode == 8) { projectSearchBackspace(); return true; }
    if (keyCode == 46) { projectSearchDelete(); return true; }
    if (keyCode == GX_KEY_LEFT) {
        if (g_projectSearchFieldCaret > 0) --g_projectSearchFieldCaret;
        return true;
    }
    if (keyCode == GX_KEY_RIGHT) {
        const uint32_t length = lengthOf(projectSearchFieldBuffer(), projectSearchFieldCapacity());
        if (g_projectSearchFieldCaret < length) ++g_projectSearchFieldCaret;
        return true;
    }
    if (keyCode == 36) { g_projectSearchFieldCaret = 0; return true; }
    if (keyCode == 35) {
        g_projectSearchFieldCaret = lengthOf(projectSearchFieldBuffer(), projectSearchFieldCapacity());
        return true;
    }
    const char value = mapKeyToChar(keyCode, modifiers);
    if (value != '\0') { projectSearchInsertCharacter(value); return true; }
    return false;
}

static void symbolSearchInsert(char value) {
    const uint32_t length = lengthOf(g_symbolSearchQuery, sizeof(g_symbolSearchQuery));
    if (value == '\0' || length >= kSymbolMaxQueryBytes || g_symbolSearchCaret > length) return;
    for (uint32_t i = length; i > g_symbolSearchCaret; --i) g_symbolSearchQuery[i] = g_symbolSearchQuery[i - 1];
    g_symbolSearchQuery[g_symbolSearchCaret++] = value;
    g_symbolSearchQuery[length + 1] = '\0';
    symbolSearchRefresh();
}

static void symbolSearchBackspace() {
    const uint32_t length = lengthOf(g_symbolSearchQuery, sizeof(g_symbolSearchQuery));
    if (g_symbolSearchCaret == 0 || g_symbolSearchCaret > length) return;
    for (uint32_t i = g_symbolSearchCaret - 1; i < length; ++i) g_symbolSearchQuery[i] = g_symbolSearchQuery[i + 1];
    --g_symbolSearchCaret;
    symbolSearchRefresh();
}

static void symbolSearchDelete() {
    const uint32_t length = lengthOf(g_symbolSearchQuery, sizeof(g_symbolSearchQuery));
    if (g_symbolSearchCaret >= length) return;
    for (uint32_t i = g_symbolSearchCaret; i < length; ++i) g_symbolSearchQuery[i] = g_symbolSearchQuery[i + 1];
    symbolSearchRefresh();
}

static bool handleSymbolSearchKey(gx_app_context* ctx, int keyCode, int action, int modifiers) {
    if (!g_symbolSearchOpen || action != GX_KEY_ACTION_DOWN) return false;
    if (keyCode == 27) { closeSymbolSearch(); return true; }
    if ((modifiers & GX_KEY_MOD_CTRL) && (keyCode == 73 || keyCode == 105)) {
        g_symbolSearchCaseSensitive = !g_symbolSearchCaseSensitive;
        symbolSearchRefresh();
        return true;
    }
    if (keyCode == GX_KEY_UP) {
        if (g_symbolSearchSelected > 0) --g_symbolSearchSelected;
        symbolSearchRefresh();
        return true;
    }
    if (keyCode == GX_KEY_DOWN) {
        if (g_symbolSearchSelected + 1 < g_symbolSearchResultCount) ++g_symbolSearchSelected;
        symbolSearchRefresh();
        return true;
    }
    if (keyCode == 13) {
        if (g_symbolSearchResultCount > 0) navigateSymbol(ctx, g_symbolSearchResults[g_symbolSearchSelected]);
        return true;
    }
    if (keyCode == 8) { symbolSearchBackspace(); return true; }
    if (keyCode == 46) { symbolSearchDelete(); return true; }
    if (keyCode == GX_KEY_LEFT) { if (g_symbolSearchCaret > 0) --g_symbolSearchCaret; return true; }
    if (keyCode == GX_KEY_RIGHT) {
        const uint32_t length = lengthOf(g_symbolSearchQuery, sizeof(g_symbolSearchQuery));
        if (g_symbolSearchCaret < length) ++g_symbolSearchCaret;
        return true;
    }
    if (keyCode == 36) { g_symbolSearchCaret = 0; return true; }
    if (keyCode == 35) { g_symbolSearchCaret = lengthOf(g_symbolSearchQuery, sizeof(g_symbolSearchQuery)); return true; }
    const char value = mapKeyToChar(keyCode, modifiers);
    if (value != '\0') { symbolSearchInsert(value); return true; }
    return false;
}

static void handleNormalKey(gx_app_context* ctx, int keyCode, int action, int modifiers, bool& running) {
    if (action != GX_KEY_ACTION_DOWN) return;
    if ((modifiers & GX_KEY_MOD_CTRL) && !(modifiers & GX_KEY_MOD_SHIFT) && (keyCode == 84 || keyCode == 116)) {
        openSymbolSearch(ctx);
        return;
    }
    if (g_symbolSearchOpen && handleSymbolSearchKey(ctx, keyCode, action, modifiers)) return;
    if ((modifiers & GX_KEY_MOD_CTRL) && (modifiers & GX_KEY_MOD_SHIFT) && (keyCode == 70 || keyCode == 102)) {
        if (g_findBarOpen) closeFindBar();
        if (!g_projectSearchPanelOpen) projectSearchInitializeDraft(WorkspaceControllerActiveDocument(&g_controller));
        g_projectSearchPanelOpen = true;
        g_projectSearchResultsFocused = false;
        g_projectSearchField = ProjectSearchField::Query;
        g_projectSearchFieldCaret = lengthOf(g_projectSearchDraft.query, sizeof(g_projectSearchDraft.query));
        g_editorFocused = false;
        return;
    }
    if (g_projectSearchPanelOpen && handleProjectSearchKey(ctx, keyCode, action, modifiers)) return;
    if ((modifiers & GX_KEY_MOD_CTRL) && (keyCode == 70 || keyCode == 102)) {
        openFindBar(false, true);
        return;
    }
    if ((modifiers & GX_KEY_MOD_CTRL) && (keyCode == 72 || keyCode == 104)) {
        openFindBar(true, true);
        return;
    }
    if (g_findBarOpen && handleFindKey(keyCode, action, modifiers)) return;
    if (g_outputFocused) {
        handleOutputKey(ctx, keyCode);
        return;
    }
    if ((modifiers & GX_KEY_MOD_CTRL) && (modifiers & GX_KEY_MOD_SHIFT) && (keyCode == 66 || keyCode == 98)) { requestBuild(ctx); return; }
    if ((modifiers & GX_KEY_MOD_CTRL) && (keyCode == 78 || keyCode == 110)) { showNewProjectPrompt(ctx); return; }
    if ((modifiers & GX_KEY_MOD_CTRL) && (modifiers & GX_KEY_MOD_SHIFT) && (keyCode == 79 || keyCode == 111)) { showOpenProjectPrompt(); return; }
    if ((modifiers & GX_KEY_MOD_CTRL) && (keyCode == 79 || keyCode == 111)) { showWorkspacePrompt(); return; }
    if ((modifiers & GX_KEY_MOD_CTRL) && (keyCode == 83 || keyCode == 115)) {
        if (modifiers & GX_KEY_MOD_SHIFT) saveAll(ctx);
        else if (g_controller.model.activeDocument < kMaxOpenDocuments) saveDocument(ctx, g_controller.model.activeDocument);
        else { writeOutput("No active document"); markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER document_save=FAIL", "no_active_document"); }
        return;
    }
    if ((modifiers & GX_KEY_MOD_CTRL) && (keyCode == 87 || keyCode == 119)) {
        if (RunControllerIsActive(&g_runController)) { writeOutput("Run in progress; close the project application first"); return; }
        if (g_controller.model.activeDocument < kMaxOpenDocuments) {
            g_pendingDocument = g_controller.model.activeDocument;
            if (g_controller.model.documents[g_pendingDocument].buffer.dirty) g_inputMode = InputMode::ConfirmDocument;
            else closeActiveDocument(ctx, CloseDecision::Discard);
        }
        return;
    }
    if (keyCode == 116) { requestRun(ctx); return; }
    if (keyCode == 114) {
        openFindBar(false, false);
        findNavigate(WorkspaceControllerActiveDocument(&g_controller),
                     (modifiers & GX_KEY_MOD_SHIFT) ? FindDirection::Backward : FindDirection::Forward);
        return;
    }
    Document* document = WorkspaceControllerActiveDocument(&g_controller);
    if (!document || !g_editorFocused) {
        if (keyCode == GX_KEY_UP && g_controller.model.selectedEntry > 0) --g_controller.model.selectedEntry;
        else if (keyCode == GX_KEY_DOWN && g_controller.model.selectedEntry + 1 < g_controller.model.entryCount) ++g_controller.model.selectedEntry;
        else if (keyCode == 13 && g_controller.model.selectedEntry < g_controller.model.entryCount) {
            WorkspaceEntryKind selectedKind = g_controller.model.entries[g_controller.model.selectedEntry].kind;
            bool success = WorkspaceControllerEnterSelected(&g_controller);
            if (selectedKind != WorkspaceEntryKind::Directory) reportDocumentOpen(ctx, success, g_controller.lastError == ModelErrorCode::DuplicateDocument);
        }
        else if (keyCode == 8) WorkspaceControllerGoUp(&g_controller);
        return;
    }
    bool wasDirty = document->buffer.dirty;
    if (keyCode == GX_KEY_LEFT) TextBufferMoveLeft(&document->buffer);
    else if (keyCode == GX_KEY_RIGHT) TextBufferMoveRight(&document->buffer);
    else if (keyCode == GX_KEY_UP) TextBufferMoveUp(&document->buffer);
    else if (keyCode == GX_KEY_DOWN) TextBufferMoveDown(&document->buffer);
    else if (keyCode == 36) TextBufferHome(&document->buffer);
    else if (keyCode == 35) TextBufferEnd(&document->buffer);
    else if (keyCode == 8) TextBufferBackspace(&document->buffer);
    else if (keyCode == 46) TextBufferDelete(&document->buffer);
    else if (keyCode == 13) TextBufferInsert(&document->buffer, "\n", 1);
    else {
        char value = mapKeyToChar(keyCode, modifiers);
        if (value != '\0') TextBufferInsert(&document->buffer, &value, 1);
    }
    updateSyntaxAfterEdit(ctx, document);
    markDirtyIfNeeded(ctx, wasDirty);
    keepCaretVisible(document);
    (void)running;
}

static void handleMouse(gx_app_context* ctx, const gx_event& event) {
    int x = event.param1;
    int y = event.param2;
    int action = GX_MOUSE_ACTION(event.param3);
    int button = GX_MOUSE_BUTTON(event.param3);
    if (g_symbolSearchOpen) {
        if (action == GX_MOUSE_ACTION_WHEEL) {
            if (event.param4 > 0) g_symbolSearchScroll = g_symbolSearchScroll > 3 ? g_symbolSearchScroll - 3 : 0;
            else if (g_symbolSearchScroll + static_cast<uint32_t>(kSymbolSearchMaxRows) < g_symbolSearchResultCount)
                ++g_symbolSearchScroll;
            return;
        }
        if (button != GX_MOUSE_BUTTON_LEFT ||
            (action != GX_MOUSE_ACTION_DOWN && action != GX_MOUSE_ACTION_DOUBLE_CLICK)) return;
        if (x >= 198 && x < 758 && y >= 102 && y < 128) {
            g_symbolSearchCaret = lengthOf(g_symbolSearchQuery, sizeof(g_symbolSearchQuery));
        } else if (x >= 580 && y >= 78 && y < 96) {
            g_symbolSearchCaseSensitive = !g_symbolSearchCaseSensitive;
            symbolSearchRefresh();
        } else if (x >= 198 && x < 758 && y >= 137 && y < 550 && g_symbolSearchResultCount > 0) {
            const uint32_t row = g_symbolSearchScroll + static_cast<uint32_t>((y - 137) / kSymbolSearchRowHeight);
            if (row < g_symbolSearchResultCount) {
                g_symbolSearchSelected = row;
                if (action == GX_MOUSE_ACTION_DOUBLE_CLICK) navigateSymbol(ctx, g_symbolSearchResults[row]);
            }
        }
        return;
    }
    if (g_projectSearchPanelOpen) {
        if (action == GX_MOUSE_ACTION_WHEEL) {
            const uint32_t total = projectSearchTotalRows();
            const uint32_t maximum = total > static_cast<uint32_t>(kSearchPanelMaxRows) ?
                total - static_cast<uint32_t>(kSearchPanelMaxRows) : 0;
            if (event.param4 > 0) g_projectSearchScroll = g_projectSearchScroll > 3 ? g_projectSearchScroll - 3 : 0;
            else g_projectSearchScroll = g_projectSearchScroll + 3 < maximum ? g_projectSearchScroll + 3 : maximum;
            drawShell(ctx);
            return;
        }
        if (button != GX_MOUSE_BUTTON_LEFT ||
            (action != GX_MOUSE_ACTION_DOWN && action != GX_MOUSE_ACTION_DOUBLE_CLICK)) return;
        if (y >= 87 && y < 107 && x >= kSearchFieldX && x < kSearchFieldX + kSearchFieldWidth) {
            g_projectSearchResultsFocused = false;
            g_projectSearchField = ProjectSearchField::Query;
            g_projectSearchFieldCaret = lengthOf(g_projectSearchDraft.query, sizeof(g_projectSearchDraft.query));
        } else if (y >= 109 && y < 129 && x >= kSearchFieldX && x < kSearchFieldX + kSearchFieldWidth) {
            g_projectSearchResultsFocused = false;
            g_projectSearchField = ProjectSearchField::Include;
            g_projectSearchFieldCaret = lengthOf(g_projectSearchDraft.includePattern, sizeof(g_projectSearchDraft.includePattern));
        } else if (y >= 133 && y < 153 && x >= kSearchFieldX && x < kSearchFieldX + kSearchFieldWidth) {
            g_projectSearchResultsFocused = false;
            g_projectSearchField = ProjectSearchField::Exclude;
            g_projectSearchFieldCaret = lengthOf(g_projectSearchDraft.excludePattern, sizeof(g_projectSearchDraft.excludePattern));
        } else if (y >= 87 && y < 108 && x >= 470 && x < 550) {
            g_projectSearchDraft.caseSensitive = !g_projectSearchDraft.caseSensitive;
        } else if (y >= 87 && y < 108 && x >= 550 && x < 730) {
            g_projectSearchDraft.wholeWord = !g_projectSearchDraft.wholeWord;
        } else if (y >= 87 && y < 108 && x >= 760 && x < 832) {
            startProjectSearch(ctx);
        } else if (y >= 87 && y < 108 && x >= 840 && x < 912) {
            if (ProjectSearchIsActive(&g_projectSearch)) {
                ProjectSearchCancel(&g_projectSearch, g_projectSearchOperationId);
                projectSearchSetStatus("Cancelling search...");
                logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER project_search_cancel=REQUESTED");
            } else {
                g_projectSearchPanelOpen = false;
                g_projectSearchResultsFocused = false;
            }
        } else if (y >= kSearchPanelResultsTop - 4 && y < kSearchPanelTop + 580) {
            const uint32_t row = g_projectSearchScroll + static_cast<uint32_t>((y - kSearchPanelResultsTop) / kSearchPanelRowHeight);
            uint32_t groupIndex = 0;
            uint32_t matchIndex = 0;
            if (projectSearchMatchForRow(row, &groupIndex, &matchIndex)) {
                g_projectSearchResultsFocused = true;
                g_projectSearchSelectedGroup = groupIndex;
                g_projectSearchSelectedMatch = matchIndex;
                if (action == GX_MOUSE_ACTION_DOUBLE_CLICK) activateProjectSearchResult(ctx, groupIndex, matchIndex);
            }
        }
        drawShell(ctx);
        return;
    }
    if (x < kExplorerRect.width && y >= kOutlineTop - 24 && y < kOutlineTop + 178) {
        const uint32_t count = outlineSymbolCount();
        if (action == GX_MOUSE_ACTION_WHEEL) {
            if (event.param4 > 0) g_outlineScroll = g_outlineScroll > 3 ? g_outlineScroll - 3 : 0;
            else if (g_outlineScroll + static_cast<uint32_t>(kOutlineMaxRows) < count) ++g_outlineScroll;
            return;
        }
        if (button == GX_MOUSE_BUTTON_LEFT &&
            (action == GX_MOUSE_ACTION_DOWN || action == GX_MOUSE_ACTION_DOUBLE_CLICK) && y >= kOutlineTop) {
            const uint32_t row = g_outlineScroll + static_cast<uint32_t>((y - kOutlineTop) / kOutlineRowHeight);
            if (row < count) {
                g_outlineSelected = row;
                ensureOutlineSelectionVisible();
                const uint32_t documentIndex = activeOutlineDocumentIndex();
                const SymbolDocument* document = SymbolDatabaseDocumentAt(&g_symbolDatabase, documentIndex);
                if (document && row < document->symbolCount)
                    navigateSymbol(ctx, document->symbolStart + row);
            }
            return;
        }
    }
    if (action == GX_MOUSE_ACTION_WHEEL) {
        if (y >= kOutputRect.y && y < kOutputRect.y + kOutputRect.height) {
            const uint32_t count = outputVisibleCount();
            const uint32_t maximum = count > 5 ? count - 5 : 0;
            if (event.param4 > 0) g_outputScroll = g_outputScroll > 2 ? g_outputScroll - 2 : 0;
            else g_outputScroll = g_outputScroll + 2 < maximum ? g_outputScroll + 2 : maximum;
            g_outputFollowTail = g_outputScroll >= maximum;
            g_outputFocused = true;
            drawShell(ctx);
        } else if (x >= kEditorRect.x && y >= kEditorRect.y && y < kEditorRect.y + kEditorRect.height) {
            int delta = event.param4 > 0 ? -3 : 3;
            if (delta < 0) g_editorScrollLine = g_editorScrollLine > static_cast<uint32_t>(-delta) ? g_editorScrollLine - static_cast<uint32_t>(-delta) : 0;
            else g_editorScrollLine += static_cast<uint32_t>(delta);
            drawShell(ctx);
        }
        return;
    }
    if (g_findBarOpen && y < 48 && x >= 270 &&
        (action == GX_MOUSE_ACTION_DOWN || action == GX_MOUSE_ACTION_DOUBLE_CLICK)) {
        Document* document = WorkspaceControllerActiveDocument(&g_controller);
        if (x >= kFindFieldX && x < kFindFieldX + kFindFieldWidth && y < 24) {
            g_findField = FindField::Query;
            g_findFieldCaret = lengthOf(g_findSession.query, sizeof(g_findSession.query));
        } else if (g_findReplaceMode && x >= kFindFieldX && x < kFindFieldX + kFindFieldWidth && y >= 24) {
            g_findField = FindField::Replacement;
            g_findFieldCaret = lengthOf(g_findSession.replacement, sizeof(g_findSession.replacement));
        } else if (y < 24 && x >= kFindPreviousX && x < kFindNextX) {
            findNavigate(document, FindDirection::Backward);
        } else if (y < 24 && x >= kFindNextX && x < kFindCaseX) {
            findNavigate(document, FindDirection::Forward);
        } else if (y < 24 && x >= kFindCaseX && x < kFindWordX) {
            FindOptions options = g_findSession.options;
            options.caseSensitive = !options.caseSensitive;
            FindSetOptions(&g_findSession, options);
            findRefreshFromCaret(document, true);
            saveFindSessionToDocument();
        } else if (y < 24 && x >= kFindWordX && x < kFindWrapX) {
            FindOptions options = g_findSession.options;
            options.wholeWord = !options.wholeWord;
            FindSetOptions(&g_findSession, options);
            findRefreshFromCaret(document, true);
            saveFindSessionToDocument();
        } else if (y < 24 && x >= kFindWrapX && x < kFindCloseX) {
            FindOptions options = g_findSession.options;
            options.wrapAround = !options.wrapAround;
            FindSetOptions(&g_findSession, options);
            saveFindSessionToDocument();
        } else if (y < 24 && x >= kFindCloseX) {
            closeFindBar();
        } else if (g_findReplaceMode && y >= 24 && x >= kFindReplaceButtonX && x < kFindReplaceAllX) {
            replaceCurrentMatch(document);
        } else if (g_findReplaceMode && y >= 24 && x >= kFindReplaceAllX && x < 770) {
            replaceAllMatches(document);
        }
        drawShell(ctx);
        return;
    }
    if (button != GX_MOUSE_BUTTON_LEFT || (action != GX_MOUSE_ACTION_DOWN && action != GX_MOUSE_ACTION_DOUBLE_CLICK)) return;
    if (y >= kOutputRect.y && y < kOutputRect.y + kOutputRect.height) {
        g_outputFocused = true;
        if (y < 546) {
            if (x >= 104 && x < 168) { g_outputProblemsTab = false; g_problemSelected = 0; }
            else if (x >= 168 && x < 250) { g_outputProblemsTab = true; g_problemSelected = 0; }
            else if (!g_outputProblemsTab && x >= 250 && x < 470) {
                OutputChannel channel = OutputServiceActiveChannel(&g_outputService);
                channel = channel == OutputChannel::All ? OutputChannel::Build :
                    channel == OutputChannel::Build ? OutputChannel::Run :
                    channel == OutputChannel::Run ? OutputChannel::Application :
                    channel == OutputChannel::Application ? OutputChannel::DeveloperStudio : OutputChannel::All;
                OutputServiceSelectActiveChannel(&g_outputService, channel);
                g_outputScroll = 0;
                g_outputFollowTail = true;
            } else if (x >= 820) {
                const OutputChannel channel = OutputServiceActiveChannel(&g_outputService);
                OutputServiceClearChannel(&g_outputService, channel);
                if (channel == OutputChannel::All) g_studioOperationId = OutputServiceBeginOperation(&g_outputService, OutputOperationType::Internal, nullptr);
                g_outputScroll = 0;
                g_problemSelected = 0;
                g_outputFollowTail = true;
            }
            drawShell(ctx);
            return;
        }
        const uint32_t row = static_cast<uint32_t>((y - 546) / 15);
        const uint32_t index = g_outputScroll + row;
        if (g_outputProblemsTab && index < outputVisibleCount()) {
            g_problemSelected = index;
            if (action == GX_MOUSE_ACTION_DOUBLE_CLICK) navigateSelectedProblem(ctx);
        }
        drawShell(ctx);
        return;
    }
    if (y < 48 && x >= 8 && x < 70) { g_fileMenuOpen = !g_fileMenuOpen; g_buildMenuOpen = false; drawShell(ctx); return; }
    if (y < 48 && x >= 70 && x < 130) { if (g_controller.model.activeDocument < kMaxOpenDocuments) saveDocument(ctx, g_controller.model.activeDocument); drawShell(ctx); return; }
    if (y < 48 && x >= 130 && x < 220) { saveAll(ctx); drawShell(ctx); return; }
    if (y < 48 && x >= 220 && x < 300) { WorkspaceControllerRefresh(&g_controller); writeOutput("Workspace refresh completed"); drawShell(ctx); return; }
    if (y < 48 && x >= 700 && x < 800) { g_buildMenuOpen = !g_buildMenuOpen; g_fileMenuOpen = false; drawShell(ctx); return; }
    if (g_buildMenuOpen && x >= 300 && x < 560 && y >= 42 && y < 134) {
        if (y < 66) requestBuild(ctx);
        else if (y < 90) requestRun(ctx);
        else requestRunClose(ctx);
        g_buildMenuOpen = false;
        drawShell(ctx);
        return;
    }
    if (g_fileMenuOpen && x >= 8 && x < 258 && y >= 42 && y < 200) {
        if (y < 68) showNewProjectPrompt(ctx);
        else if (y < 92) showOpenProjectPrompt();
        else if (y < 116) showWorkspacePrompt();
        else if (y < 140 && g_controller.model.activeDocument < kMaxOpenDocuments) {
            g_pendingDocument = g_controller.model.activeDocument;
            if (g_controller.model.documents[g_pendingDocument].buffer.dirty) g_inputMode = InputMode::ConfirmDocument;
            else closeActiveDocument(ctx, CloseDecision::Discard);
        } else if (y < 164) {
            g_workspaceSwitchPending = false;
            if (BuildControllerIsActive(&g_buildController)) writeOutput("Build in progress");
            else if (RunControllerIsActive(&g_runController)) writeOutput("Run in progress");
            else if (g_controller.model.open && guidexos::developer_studio::WorkspaceModelHasDirtyDocuments(&g_controller.model)) g_inputMode = InputMode::ConfirmWorkspace;
            else { stopProjectSearch(ctx); WorkspaceControllerCloseWorkspace(&g_controller, CloseDecision::Discard); }
        } else if (y < 188) {
            if (BuildControllerIsActive(&g_buildController)) writeOutput("Build in progress; close blocked");
            else if (RunControllerIsActive(&g_runController)) { g_inputMode = InputMode::ConfirmRunClose; writeOutput("Project application is running: Close it first or keep Studio open"); }
            else if (guidexos::developer_studio::WorkspaceModelHasDirtyDocuments(&g_controller.model)) g_inputMode = InputMode::ConfirmApplication;
            else { logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER application_close=PASS"); logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER clean_close=PASS"); g_requestExit = true; }
        }
        g_fileMenuOpen = false;
        drawShell(ctx);
        return;
    }
    if (y >= 52 && y < 80 && x >= 270) { selectDocumentTab(x); drawShell(ctx); return; }
    if (x < kExplorerRect.width && y >= kEntryTop - 14 && y < kEntryTop + 20 * kEntryHeight) {
        int row = (y - (kEntryTop - 14)) / kEntryHeight;
        if (row >= 0 && static_cast<uint32_t>(row) < g_controller.model.entryCount) {
            uint64_t now = gx_get_ticks_ms(ctx);
            bool activate = action == GX_MOUSE_ACTION_DOUBLE_CLICK || (static_cast<uint32_t>(row) == g_lastExplorerClick && now - g_lastExplorerClickTick < 500);
            g_controller.model.selectedEntry = static_cast<uint32_t>(row);
            g_lastExplorerClick = static_cast<uint32_t>(row);
            g_lastExplorerClickTick = now;
            if (activate) {
                WorkspaceEntryKind selectedKind = g_controller.model.entries[row].kind;
                bool success = WorkspaceControllerEnterSelected(&g_controller);
                bool duplicate = g_controller.lastError == ModelErrorCode::DuplicateDocument;
                if (selectedKind != WorkspaceEntryKind::Directory) reportDocumentOpen(ctx, success, duplicate);
            }
            drawShell(ctx);
        }
        return;
    }
    if (x >= kEditorRect.x && y >= kEditorTop && y < kEditorRect.y + kEditorRect.height) {
        g_editorFocused = true;
        placeCaretFromMouse(WorkspaceControllerActiveDocument(&g_controller), x, y);
        drawShell(ctx);
    }
}

} // namespace

extern "C" gx_result GX_CALL gx_main(gx_app_context* ctx) {
    if (!ctx || !ctx->host) return GX_ERROR_INVALID_ARGUMENT;
    if (!ctx->host->get_api_version || !ctx->host->log || !ctx->host->request_window) return GX_ERROR_UNSUPPORTED;
    const guidexos::developer_studio::TargetProfile& target = InitialTargetProfile();
    if (!IsValidTargetProfile(target)) return GX_ERROR_FAILED;

    g_fileSystemContext.app = ctx;
    WorkspaceFileSystem fileSystem = { &g_fileSystemContext, fsStat, fsList, fsRead, fsWrite, fsCreateDirectory, fsRemovePath };
    WorkspaceControllerInit(&g_controller, fileSystem);
    SymbolDatabaseInit(&g_symbolDatabase, g_symbolProjectStorage, kSymbolMaxProjectSymbols,
                       g_symbolDocumentStorage, kSymbolMaxDocuments,
                       g_symbolScratchStorage, kSymbolMaxDocumentSymbols);
    WorkspaceControllerAttachSymbolDatabase(&g_controller, &g_symbolDatabase);
    g_inputMode = InputMode::Normal;
    g_editorFocused = false;
    g_fileMenuOpen = false;
    g_requestExit = false;
    g_workspaceSwitchPending = false;
    g_pendingDocument = kMaxOpenDocuments;
    g_editorScrollLine = 0;
    g_editorScrollColumn = 0;
    g_syntaxIncrementalMarkerReported = false;
    g_syntaxConvergenceMarkerReported = false;
    g_syntaxFallbackMarkerReported = false;
    g_syntaxRenderMarkerReported = false;
    FindSessionInit(&g_findSession);
    g_findBarOpen = false;
    g_findReplaceMode = false;
    g_findField = FindField::Query;
    g_findFieldCaret = 0;
    findSetStatus("");
    ProjectSearchServiceInit(&g_projectSearch);
    projectSearchInitializeDraft(nullptr);
    g_projectSearchPanelOpen = false;
    g_projectSearchResultsFocused = false;
    g_projectSearchOperationId = 0;
    g_projectSearchProjectGeneration = 0;
    g_projectSearchProjectId[0] = '\0';
    g_projectSearchStatus[0] = '\0';
    g_lastProjectSearchQuery[0] = '\0';
    g_projectSearchTerminalReported = false;
    g_symbolSearchOpen = false;
    g_symbolSearchCaseSensitive = false;
    g_symbolSearchCaret = 0;
    g_symbolSearchSelected = 0;
    g_symbolSearchScroll = 0;
    g_symbolSearchResultCount = 0;
    g_symbolSearchQuery[0] = '\0';
    g_outlineSelected = 0;
    g_outlineScroll = 0;
    OutputServiceInit(&g_outputService);
    g_studioOperationId = OutputServiceBeginOperation(&g_outputService, OutputOperationType::Internal, nullptr);
    g_runOperationId = 0;
    g_outputProblemsTab = false;
    g_outputFocused = false;
    g_outputScroll = 0;
    g_outputFollowTail = true;
    g_problemSelected = 0;
    BuildControllerInit(&g_buildController);
    g_buildTerminalReported = false;
    RunControllerInit(&g_runController);
    g_runWaitingForBuild = false;
    g_lastRunState = RunState::Idle;
    g_runTerminalReported = false;
    writeOutput("Ready");

    logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER application_construction=PASS");
    logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER target_profile=guidexos.amd64.hosted.native maturity=experimental");
    logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER filesystem_api=workspace_extensions");

    gx_result windowResult = GX_ERROR_FAILED;
    if (ctx->host->request_window_ex) {
        windowResult = ctx->host->request_window_ex(ctx, "guideXOS Developer Studio", kWindowRect.width, kWindowRect.height, GX_WINDOW_FLAG_RESIZABLE | GX_WINDOW_FLAG_CENTERED, &g_window);
    } else {
        windowResult = ctx->host->request_window(ctx, "guideXOS Developer Studio", kWindowRect.width, kWindowRect.height, &g_window);
    }
    if (windowResult != GX_OK) {
        logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER main_window_creation=FAIL");
        return windowResult;
    }
    logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER main_window_creation=PASS");
    drawShell(ctx);
    logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER initial_render=PASS");

    bool running = true;
    if (ctx->host->poll_event) {
        while (running) {
            pollProjectSearch(ctx);
            pollBuild(ctx);
            pollRun(ctx);
            gx_event event;
            clear_event(&event);
            gx_result result = ctx->host->poll_event(ctx, &event, ProjectSearchIsActive(&g_projectSearch) ? 50 : 500);
            if (result == GX_OK && event.window == g_window) {
                if (gx_event_is_paint(&event)) drawShell(ctx);
                else if (gx_event_is_close(&event)) {
                    if (BuildControllerIsActive(&g_buildController)) {
                        writeOutput("Build in progress; close blocked");
                        logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER build_close=BLOCKED");
                    } else if (RunControllerIsActive(&g_runController)) {
                        g_inputMode = InputMode::ConfirmRunClose;
                        writeOutput("Project application is running: Close it first or keep Studio open");
                    } else if (guidexos::developer_studio::WorkspaceModelHasDirtyDocuments(&g_controller.model)) {
                        g_inputMode = InputMode::ConfirmApplication;
                        writeOutput("Unsaved changes: Save, Discard, or Cancel");
                        drawShell(ctx);
                    } else {
                        logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER application_close=PASS");
                        logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER clean_close=PASS");
                        running = false;
                    }
                } else if (event.type == GX_EVENT_KEY) {
                    if (g_inputMode != InputMode::Normal) handleModalKey(ctx, event.param1, event.param2, event.param3);
                    else if (gx_event_is_escape_down(&event) && !g_findBarOpen && !g_projectSearchPanelOpen && !g_symbolSearchOpen) {
                        if (BuildControllerIsActive(&g_buildController)) writeOutput("Build in progress; close blocked");
                        else if (RunControllerIsActive(&g_runController)) { g_inputMode = InputMode::ConfirmRunClose; writeOutput("Project application is running: Close it first or keep Studio open"); }
                        else if (guidexos::developer_studio::WorkspaceModelHasDirtyDocuments(&g_controller.model)) g_inputMode = InputMode::ConfirmApplication;
                        else { logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER application_close=PASS"); logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER clean_close=PASS"); running = false; }
                    } else handleNormalKey(ctx, event.param1, event.param2, event.param3, running);
                    if (g_requestExit) running = false;
                    drawShell(ctx);
                } else if (event.type == GX_EVENT_MOUSE) {
                    handleMouse(ctx, event);
                    if (g_requestExit) running = false;
                }
                pollBuild(ctx);
                pollRun(ctx);
                pollProjectSearch(ctx);
                drawShell(ctx);
            } else if (result != GX_OK && result != GX_ERROR_TIMEOUT) {
                markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER event_loop=FAIL", "poll_event");
                running = false;
            }
        }
    } else if (ctx->host->wait_for_close) {
        gx_result waitResult = ctx->host->wait_for_close(ctx, g_window, 300000);
        if (waitResult == GX_OK) logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER clean_close=PASS");
        else markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER clean_close=FAIL", "wait_for_close");
    }

    if (ctx->host->exit) return ctx->host->exit(ctx, GX_OK);
    return GX_OK;
}
