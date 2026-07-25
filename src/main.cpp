#include <guidexos/ui.h>

#include "developer_studio_models.h"
#include "developer_studio_workspace.h"

namespace {

using guidexos::developer_studio::CloseDecision;
using guidexos::developer_studio::Document;
using guidexos::developer_studio::FileInfo;
using guidexos::developer_studio::FileInfoKind;
using guidexos::developer_studio::FileListEntry;
using guidexos::developer_studio::InitialTargetProfile;
using guidexos::developer_studio::IsSupportedTextPath;
using guidexos::developer_studio::IsValidTargetProfile;
using guidexos::developer_studio::JoinWorkspacePath;
using guidexos::developer_studio::ModelErrorCode;
using guidexos::developer_studio::ModelErrorName;
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
using guidexos::developer_studio::kMaxWorkspaceEntries;

static const gx_rect kWindowRect = { 0, 0, 960, 640 };
static const gx_rect kCommandRect = { 0, 0, 960, 48 };
static const gx_rect kExplorerRect = { 0, 48, 270, 472 };
static const gx_rect kEditorRect = { 270, 48, 690, 472 };
static const gx_rect kOutputRect = { 0, 520, 960, 60 };
static const gx_rect kStatusRect = { 0, 580, 960, 60 };
static const int kEntryTop = 104;
static const int kEntryHeight = 18;
static const int kEditorTop = 86;
static const int kEditorLineHeight = 16;
static const int kEditorLineNumberX = 282;
static const int kEditorTextX = 320;
static const int kVisibleEditorLines = 26;
static const int kMaxOutputLines = 4;
static const int kMaxPromptBytes = 240;

enum class InputMode {
    Normal = 0,
    WorkspacePath,
    ConfirmDocument,
    ConfirmWorkspace,
    ConfirmApplication
};

struct NativeFileSystemContext {
    gx_app_context* app;
};

static WorkspaceController g_controller;
static NativeFileSystemContext g_fileSystemContext = {};
static gx_handle g_window = 0;
static InputMode g_inputMode = InputMode::Normal;
static bool g_editorFocused = false;
static bool g_fileMenuOpen = false;
static bool g_requestExit = false;
static bool g_workspaceSwitchPending = false;
static uint32_t g_pendingDocument = kMaxOpenDocuments;
static uint32_t g_editorScrollLine = 0;
static uint32_t g_lastExplorerClick = 0;
static uint64_t g_lastExplorerClickTick = 0;
static char g_prompt[kMaxPathBytes] = {};
static char g_pendingWorkspacePath[kMaxPathBytes] = {};
static char g_output[kMaxOutputLines][96] = {};
static uint32_t g_outputCount = 0;
static uint32_t g_outputNext = 0;
static char g_textScratch[256] = {};
static char g_lineScratch[144] = {};

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

static void writeOutput(const char* message) {
    if (!message) return;
    copyText(g_output[g_outputNext], sizeof(g_output[g_outputNext]), message);
    g_outputNext = (g_outputNext + 1) % kMaxOutputLines;
    if (g_outputCount < kMaxOutputLines) ++g_outputCount;
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
        if (duplicate) logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER document_open=PASS duplicate=TRUE");
        else logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER document_open=PASS");
    } else {
        outputError("Document open failed");
        markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER document_open=FAIL", currentError());
    }
}

static bool saveDocument(gx_app_context* ctx, uint32_t index) {
    if (!WorkspaceControllerSaveDocument(&g_controller, index)) {
        writeOutput("Save failed");
        markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER document_save=FAIL", currentError());
        return false;
    }
    writeOutput("Document saved");
    logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER document_save=PASS");
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

static void showWorkspacePrompt() {
    g_inputMode = InputMode::WorkspacePath;
    g_fileMenuOpen = false;
    g_workspaceSwitchPending = true;
    copyText(g_prompt, sizeof(g_prompt), "");
}

static bool commitWorkspaceOpen(gx_app_context* ctx) {
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
    logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER application_close=PASS");
    logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER clean_close=PASS");
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

static void drawExplorer(gx_app_context* ctx) {
    drawText(ctx, 16, 66, "EXPLORER");
    if (!g_controller.model.open) {
        drawText(ctx, 16, 98, "No workspace open");
        drawText(ctx, 16, 126, "File -> Open Workspace");
        return;
    }
    compose(g_textScratch, sizeof(g_textScratch), "Workspace: ", g_controller.model.displayName, "");
    drawText(ctx, 16, 86, g_textScratch);
    compose(g_textScratch, sizeof(g_textScratch), "Path: ", g_controller.model.browsePath[0] ? g_controller.model.browsePath : "/", "");
    drawText(ctx, 16, 102, g_textScratch);
    for (uint32_t i = 0; i < g_controller.model.entryCount && i < 20; ++i) {
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

static uint32_t activeLine(const TextBuffer& buffer) {
    uint32_t line = 0;
    for (uint32_t i = 0; i < buffer.caret && i < buffer.length; ++i) if (buffer.data[i] == '\n') ++line;
    return line;
}

static uint32_t activeColumn(const TextBuffer& buffer, uint32_t line) {
    return buffer.caret - TextBufferLineStart(&buffer, line);
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

static void drawEditor(gx_app_context* ctx) {
    drawTabs(ctx);
    Document* document = WorkspaceControllerActiveDocument(&g_controller);
    if (!document) {
        drawText(ctx, 300, 120, "Welcome to guideXOS Developer Studio");
        drawText(ctx, 300, 150, "Open a workspace, then activate a supported text file.");
        drawText(ctx, 300, 180, "The editor is bounded and intentionally syntax-free.");
        return;
    }
    compose(g_textScratch, sizeof(g_textScratch), "Document: ", document->name, document->buffer.dirty ? "  Modified" : "");
    drawText(ctx, 300, 96, g_textScratch);
    uint32_t lineCount = TextBufferLineCount(&document->buffer);
    if (g_editorScrollLine >= lineCount) g_editorScrollLine = lineCount == 0 ? 0 : lineCount - 1;
    for (uint32_t row = 0; row < kVisibleEditorLines; ++row) {
        uint32_t line = g_editorScrollLine + row;
        if (line >= lineCount) break;
        uint32_t start = TextBufferLineStart(&document->buffer, line);
        uint32_t end = TextBufferLineEnd(&document->buffer, line);
        uint32_t count = end > start ? end - start : 0;
        if (count >= sizeof(g_lineScratch)) count = sizeof(g_lineScratch) - 1;
        for (uint32_t i = 0; i < count; ++i) g_lineScratch[i] = document->buffer.data[start + i];
        g_lineScratch[count] = '\0';
        compose(g_textScratch, sizeof(g_textScratch), "", "", "");
        appendUnsigned(g_textScratch, sizeof(g_textScratch), line + 1);
        appendText(g_textScratch, sizeof(g_textScratch), " ");
        drawText(ctx, kEditorLineNumberX, kEditorTop + static_cast<int>(row) * kEditorLineHeight, g_textScratch);
        drawText(ctx, kEditorTextX, kEditorTop + static_cast<int>(row) * kEditorLineHeight, g_lineScratch);
    }
    uint32_t line = activeLine(document->buffer);
    uint32_t column = activeColumn(document->buffer, line);
    if (line >= g_editorScrollLine && line < g_editorScrollLine + kVisibleEditorLines) {
        drawPanel(ctx, { kEditorTextX + static_cast<int>(column) * 8, kEditorTop - 12 + static_cast<int>(line - g_editorScrollLine) * kEditorLineHeight, 2, 14 }, 0xD6E4FFu);
    }
}

static void drawOutputAndStatus(gx_app_context* ctx) {
    drawText(ctx, 16, 540, "OUTPUT");
    uint32_t start = g_outputCount < kMaxOutputLines ? 0 : g_outputNext;
    for (uint32_t i = 0; i < g_outputCount; ++i) drawText(ctx, 112, 540 + static_cast<int>(i) * 12, g_output[(start + i) % kMaxOutputLines]);
    compose(g_textScratch, sizeof(g_textScratch), "Target: ", InitialTargetProfile().displayName, " | Experimental");
    drawText(ctx, 16, 600, g_textScratch);
    compose(g_textScratch, sizeof(g_textScratch), "Workspace: ", g_controller.model.open ? g_controller.model.displayName : "-", "");
    drawText(ctx, 16, 618, g_textScratch);
    Document* document = WorkspaceControllerActiveDocument(&g_controller);
    if (document) {
        uint32_t line = activeLine(document->buffer);
        uint32_t column = activeColumn(document->buffer, line);
        compose(g_textScratch, sizeof(g_textScratch), "Document: ", document->name, document->buffer.dirty ? " Modified" : "");
        appendText(g_textScratch, sizeof(g_textScratch), "  Line ");
        appendUnsigned(g_textScratch, sizeof(g_textScratch), line + 1);
        appendText(g_textScratch, sizeof(g_textScratch), ", Column ");
        appendUnsigned(g_textScratch, sizeof(g_textScratch), column + 1);
        drawText(ctx, 300, 618, g_textScratch);
    }
}

static void drawModal(gx_app_context* ctx) {
    if (g_inputMode == InputMode::Normal) return;
    drawPanel(ctx, { 180, 180, 600, 180 }, 0x2A3852u);
    if (g_inputMode == InputMode::WorkspacePath) {
        drawText(ctx, 210, 220, "Open Workspace (temporary path-entry facility)");
        drawText(ctx, 210, 250, "Enter an absolute hosted path:");
        drawPanel(ctx, { 210, 265, 540, 30 }, 0x111722u);
        drawText(ctx, 220, 285, g_prompt);
        drawText(ctx, 210, 325, "Enter: open   Escape: cancel");
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
    drawText(ctx, 340, 30, "Save");
    drawText(ctx, 400, 30, "Save All");
    drawText(ctx, 490, 30, "Refresh");
    drawText(ctx, 575, 30, "Ctrl+O Workspace");
    drawExplorer(ctx);
    drawEditor(ctx);
    drawOutputAndStatus(ctx);
    if (g_fileMenuOpen) {
        drawPanel(ctx, { 8, 42, 190, 110 }, 0x34496Au);
        drawText(ctx, 20, 68, "Open Workspace");
        drawText(ctx, 20, 92, "Close Document");
        drawText(ctx, 20, 116, "Close Workspace");
        drawText(ctx, 20, 140, "Exit");
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
    uint32_t requested = start + static_cast<uint32_t>(column);
    document->buffer.caret = requested > end ? end : requested;
}

static void handleModalKey(gx_app_context* ctx, int keyCode, int action, int modifiers) {
    if (action != GX_KEY_ACTION_DOWN) return;
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

static void handleNormalKey(gx_app_context* ctx, int keyCode, int action, int modifiers, bool& running) {
    if (action != GX_KEY_ACTION_DOWN) return;
    if ((modifiers & GX_KEY_MOD_CTRL) && (keyCode == 79 || keyCode == 111)) { showWorkspacePrompt(); return; }
    if ((modifiers & GX_KEY_MOD_CTRL) && (keyCode == 83 || keyCode == 115)) {
        if (modifiers & GX_KEY_MOD_SHIFT) saveAll(ctx);
        else if (g_controller.model.activeDocument < kMaxOpenDocuments) saveDocument(ctx, g_controller.model.activeDocument);
        else { writeOutput("No active document"); markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER document_save=FAIL", "no_active_document"); }
        return;
    }
    if ((modifiers & GX_KEY_MOD_CTRL) && (keyCode == 87 || keyCode == 119)) {
        if (g_controller.model.activeDocument < kMaxOpenDocuments) {
            g_pendingDocument = g_controller.model.activeDocument;
            if (g_controller.model.documents[g_pendingDocument].buffer.dirty) g_inputMode = InputMode::ConfirmDocument;
            else closeActiveDocument(ctx, CloseDecision::Discard);
        }
        return;
    }
    if (keyCode == 116) { WorkspaceControllerRefresh(&g_controller); writeOutput("Workspace refresh completed"); return; }
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
    markDirtyIfNeeded(ctx, wasDirty);
    uint32_t line = activeLine(document->buffer);
    if (line < g_editorScrollLine) g_editorScrollLine = line;
    if (line >= g_editorScrollLine + kVisibleEditorLines) g_editorScrollLine = line - kVisibleEditorLines + 1;
    (void)running;
}

static void handleMouse(gx_app_context* ctx, const gx_event& event) {
    int x = event.param1;
    int y = event.param2;
    int action = GX_MOUSE_ACTION(event.param3);
    int button = GX_MOUSE_BUTTON(event.param3);
    if (action == GX_MOUSE_ACTION_WHEEL) {
        if (x >= kEditorRect.x && y >= kEditorRect.y && y < kEditorRect.y + kEditorRect.height) {
            int delta = event.param4 > 0 ? -3 : 3;
            if (delta < 0) g_editorScrollLine = g_editorScrollLine > static_cast<uint32_t>(-delta) ? g_editorScrollLine - static_cast<uint32_t>(-delta) : 0;
            else g_editorScrollLine += static_cast<uint32_t>(delta);
            drawShell(ctx);
        }
        return;
    }
    if (button != GX_MOUSE_BUTTON_LEFT || (action != GX_MOUSE_ACTION_DOWN && action != GX_MOUSE_ACTION_DOUBLE_CLICK)) return;
    if (y < 48 && x >= 8 && x < 70) { g_fileMenuOpen = !g_fileMenuOpen; drawShell(ctx); return; }
    if (y < 48 && x >= 70 && x < 130) { if (g_controller.model.activeDocument < kMaxOpenDocuments) saveDocument(ctx, g_controller.model.activeDocument); drawShell(ctx); return; }
    if (y < 48 && x >= 130 && x < 220) { saveAll(ctx); drawShell(ctx); return; }
    if (y < 48 && x >= 220 && x < 300) { WorkspaceControllerRefresh(&g_controller); writeOutput("Workspace refresh completed"); drawShell(ctx); return; }
    if (g_fileMenuOpen && x >= 8 && x < 198 && y >= 42 && y < 152) {
        if (y < 76) showWorkspacePrompt();
        else if (y < 102 && g_controller.model.activeDocument < kMaxOpenDocuments) {
            g_pendingDocument = g_controller.model.activeDocument;
            if (g_controller.model.documents[g_pendingDocument].buffer.dirty) g_inputMode = InputMode::ConfirmDocument;
            else closeActiveDocument(ctx, CloseDecision::Discard);
        } else if (y < 128) {
            g_workspaceSwitchPending = false;
            if (g_controller.model.open && guidexos::developer_studio::WorkspaceModelHasDirtyDocuments(&g_controller.model)) g_inputMode = InputMode::ConfirmWorkspace;
            else WorkspaceControllerCloseWorkspace(&g_controller, CloseDecision::Discard);
        } else if (y < 152) {
            if (guidexos::developer_studio::WorkspaceModelHasDirtyDocuments(&g_controller.model)) g_inputMode = InputMode::ConfirmApplication;
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
    WorkspaceFileSystem fileSystem = { &g_fileSystemContext, fsStat, fsList, fsRead, fsWrite };
    WorkspaceControllerInit(&g_controller, fileSystem);
    g_inputMode = InputMode::Normal;
    g_editorFocused = false;
    g_fileMenuOpen = false;
    g_requestExit = false;
    g_workspaceSwitchPending = false;
    g_pendingDocument = kMaxOpenDocuments;
    g_editorScrollLine = 0;
    g_outputCount = 0;
    g_outputNext = 0;
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
            gx_event event;
            clear_event(&event);
            gx_result result = ctx->host->poll_event(ctx, &event, 500);
            if (result == GX_OK && event.window == g_window) {
                if (gx_event_is_paint(&event)) drawShell(ctx);
                else if (gx_event_is_close(&event)) {
                    if (guidexos::developer_studio::WorkspaceModelHasDirtyDocuments(&g_controller.model)) {
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
                    else if (gx_event_is_escape_down(&event)) {
                        if (guidexos::developer_studio::WorkspaceModelHasDirtyDocuments(&g_controller.model)) g_inputMode = InputMode::ConfirmApplication;
                        else { logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER application_close=PASS"); logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER clean_close=PASS"); running = false; }
                    } else handleNormalKey(ctx, event.param1, event.param2, event.param3, running);
                    if (g_requestExit) running = false;
                    drawShell(ctx);
                } else if (event.type == GX_EVENT_MOUSE) {
                    handleMouse(ctx, event);
                    if (g_requestExit) running = false;
                }
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
