#pragma once

#include <stdint.h>

#include "developer_studio_debug_watches.h"
#include "developer_studio_models.h"
#include "developer_studio_projects.h"

// The host model deliberately does not include the UI/SDK headers. Keep the
// ABI limit name while providing the same value for standalone host builds.
#ifndef GX_DEVELOPMENT_DEBUG_MAX_LOG_TEMPLATE_BYTES
#define GX_DEVELOPMENT_DEBUG_MAX_LOG_TEMPLATE_BYTES 256u
#endif

namespace guidexos {
namespace developer_studio {

static const uint32_t kDebuggerWorkspaceMaxBreakpoints = 8;
static const uint32_t kDebuggerWorkspaceMaxWatches = 8;
static const uint32_t kDebuggerWorkspaceMaxFileBytes = 16u * 1024u;

enum class DebuggerWorkspaceErrorCode {
    None = 0,
    NullInput,
    MalformedJson,
    UnsupportedVersion,
    MissingField,
    UnknownField,
    DuplicateField,
    InvalidPath,
    InvalidLine,
    InvalidColumn,
    InvalidEnum,
    InvalidThreshold,
    OverCapacity,
    DuplicateBreakpoint,
    StringTooLong,
    OutputTooSmall,
    FileError
};

struct DebuggerWorkspaceBreakpoint {
    char sourcePath[kMaxProjectPathBytes];
    uint32_t line;
    uint32_t column;
    bool enabled;
    uint32_t action;
    uint32_t hitPolicy;
    uint32_t hitThreshold;
    char condition[kDebugWatchMaxExpressionBytes + 1];
    char logTemplate[GX_DEVELOPMENT_DEBUG_MAX_LOG_TEMPLATE_BYTES + 1u];
};

struct DebuggerWorkspace {
    DebuggerWorkspaceBreakpoint breakpoints[kDebuggerWorkspaceMaxBreakpoints];
    uint32_t breakpointCount;
    char watches[kDebuggerWorkspaceMaxWatches][kDebugWatchMaxExpressionBytes + 1];
    uint32_t watchCount;
    DebuggerWorkspaceErrorCode lastError;
    char lastErrorMessage[96];
};

// An ordered command plan, not a live breakpoint table. The application owns
// all manager identities, mapping results and per-generation associations.
struct DebuggerWorkspaceMaterializationEntry {
    char sourcePath[kMaxProjectPathBytes];
    uint32_t line;
    uint32_t column;
    uint32_t action;
    uint32_t hitPolicy;
    uint32_t hitThreshold;
    char condition[kDebugWatchMaxExpressionBytes + 1];
    char logTemplate[GX_DEVELOPMENT_DEBUG_MAX_LOG_TEMPLATE_BYTES + 1u];
};

// Copies up to capacity enabled records in persisted order; returns the count.
uint32_t BuildDebuggerWorkspaceMaterializationPlan(const DebuggerWorkspace& workspace,
    DebuggerWorkspaceMaterializationEntry* entries, uint32_t capacity);

void DebuggerWorkspaceInit(DebuggerWorkspace* workspace);
bool DebuggerWorkspaceAddBreakpoint(DebuggerWorkspace* workspace, const char* sourcePath,
                                    uint32_t line, uint32_t column, bool enabled,
                                    uint32_t action, uint32_t hitPolicy, uint32_t hitThreshold,
                                    const char* condition, const char* logTemplate);
bool DebuggerWorkspaceToggleBreakpoint(DebuggerWorkspace* workspace, const char* sourcePath,
                                       uint32_t line, uint32_t column);
bool DebuggerWorkspaceRemoveBreakpoint(DebuggerWorkspace* workspace, const char* sourcePath,
                                       uint32_t line);
bool DebuggerWorkspaceSetBreakpointEnabled(DebuggerWorkspace* workspace, const char* sourcePath,
                                           uint32_t line, bool enabled);
bool DebuggerWorkspaceUpdateBreakpoint(DebuggerWorkspace* workspace, const char* sourcePath,
                                       uint32_t line, uint32_t action, uint32_t hitPolicy,
                                       uint32_t hitThreshold, const char* condition,
                                       const char* logTemplate);
bool DebuggerWorkspaceAddWatch(DebuggerWorkspace* workspace, const char* expression);
bool DebuggerWorkspaceEditWatch(DebuggerWorkspace* workspace, uint32_t index, const char* expression);
bool DebuggerWorkspaceRemoveWatch(DebuggerWorkspace* workspace, uint32_t index);
int DebuggerWorkspaceFindBreakpoint(const DebuggerWorkspace* workspace, const char* sourcePath,
                                    uint32_t line);

bool SerializeDebuggerWorkspace(const DebuggerWorkspace& workspace, char* output, uint32_t outputSize,
                                uint32_t* outBytes, DebuggerWorkspaceErrorCode* error);
bool ParseDebuggerWorkspace(const char* bytes, uint32_t length, DebuggerWorkspace* output,
                            DebuggerWorkspaceErrorCode* error);

bool DebuggerWorkspaceStoragePath(const char* projectRoot, char* output, uint32_t outputSize);
bool DebuggerWorkspaceStorageBackupPath(const char* projectRoot, char* output, uint32_t outputSize);
bool DebuggerWorkspaceStorageLoad(const WorkspaceFileSystem& fileSystem, const char* projectRoot,
                                  const char* projectId, DebuggerWorkspace* output,
                                  char* status, uint32_t statusSize);
bool DebuggerWorkspaceStorageSave(const WorkspaceFileSystem& fileSystem, const char* projectRoot,
                                  const char* projectId, const DebuggerWorkspace* workspace,
                                  char* status, uint32_t statusSize);

} // namespace developer_studio
} // namespace guidexos
