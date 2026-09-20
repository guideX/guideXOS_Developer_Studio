#pragma once

#include <stdint.h>

#include "developer_studio_models.h"

namespace guidexos {
namespace developer_studio {

// The source editor intentionally mirrors the public manager limit, not the
// debugger core's larger internal breakpoint table.  The manager snapshot is
// the source of truth; this is only the bounded render/navigation projection.
static const uint32_t kDebugEditorMaxBreakpoints = 8u;

enum class DebugEditorBreakpointVisualState {
    None = 0,
    Enabled,
    Disabled,
    Unresolved
};

struct DebugEditorBreakpoint {
    bool used;
    // Configured rows are persisted pre-Debug rows and may have no live ID.
    bool configured;
    bool unresolved;
    uint64_t id;
    char projectId[kMaxProjectIdBytes];
    char sourcePath[kMaxProjectPathBytes];
    uint32_t line;
    uint32_t column;
    bool enabled;
    uint32_t action;
};

struct DebugEditorLocation {
    bool valid;
    uint64_t sessionGeneration;
    uint64_t stopGeneration;
    char projectId[kMaxProjectIdBytes];
    char sourcePath[kMaxProjectPathBytes];
    uint32_t line;
    uint32_t column;
};

struct DebugEditorNavigationTarget {
    bool valid;
    bool sourceMapped;
    char projectId[kMaxProjectIdBytes];
    char sourcePath[kMaxProjectPathBytes];
    uint32_t line;
    uint32_t column;
};

struct DebugEditorModel {
    uint64_t sessionGeneration;
    uint64_t stopGeneration;
    bool sessionActive;
    bool paused;
    bool sourceTrustworthy;
    uint32_t breakpointCount;
    bool breakpointsTruncated;
    DebugEditorBreakpoint breakpoints[kDebugEditorMaxBreakpoints];
    DebugEditorLocation execution;
    DebugEditorLocation inspection;
};

void DebugEditorModelInit(DebugEditorModel* model);
void DebugEditorModelResetRuntime(DebugEditorModel* model);

// Refreshes the render model from configured and live rows. Rows with the
// same canonical project/path/line are collapsed so a duplicate or conflict
// can never create a second visual marker; a live row replaces a configured
// row at the same identity.
void DebugEditorModelRefreshBreakpoints(DebugEditorModel* model,
                                        const DebugEditorBreakpoint* rows,
                                        uint32_t rowCount);
const DebugEditorBreakpoint* DebugEditorModelBreakpointAt(const DebugEditorModel* model,
                                                           uint32_t index);
int DebugEditorModelFindBreakpoint(const DebugEditorModel* model,
                                   const char* projectId, const char* sourcePath,
                                   uint32_t line);
DebugEditorBreakpointVisualState DebugEditorModelLineState(const DebugEditorModel* model,
                                                            const char* projectId,
                                                            const char* sourcePath,
                                                            uint32_t line);

bool DebugEditorModelSetExecution(DebugEditorModel* model, uint64_t sessionGeneration,
                                  uint64_t stopGeneration, bool sessionActive, bool paused,
                                  bool sourceTrustworthy, const char* projectId,
                                  const char* sourcePath, uint32_t line, uint32_t column);
void DebugEditorModelClearExecution(DebugEditorModel* model);
bool DebugEditorModelSetInspection(DebugEditorModel* model, uint64_t sessionGeneration,
                                   uint64_t stopGeneration, const char* projectId,
                                   const char* sourcePath, uint32_t line, uint32_t column);
void DebugEditorModelClearInspection(DebugEditorModel* model);
bool DebugEditorModelExecutionMatches(const DebugEditorModel* model,
                                      uint64_t sessionGeneration, uint64_t stopGeneration,
                                      const char* projectId, const char* sourcePath,
                                      uint32_t line);

bool DebugEditorNavigationTargetFromSource(DebugEditorNavigationTarget* target,
                                           const char* projectId, const char* sourcePath,
                                           uint32_t line, uint32_t column, bool sourceMapped);

} // namespace developer_studio
} // namespace guidexos
