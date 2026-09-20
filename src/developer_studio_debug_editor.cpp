#include "developer_studio_debug_editor.h"

namespace guidexos {
namespace developer_studio {
namespace {

static bool copyText(char* output, uint32_t outputSize, const char* input) {
    if (!output || outputSize == 0 || !input) return false;
    uint32_t i = 0;
    while (i + 1 < outputSize && input[i] != '\0') {
        output[i] = input[i];
        ++i;
    }
    if (input[i] != '\0') {
        output[0] = '\0';
        return false;
    }
    output[i] = '\0';
    return true;
}

static void clearBreakpoint(DebugEditorBreakpoint* breakpoint) {
    if (!breakpoint) return;
    *breakpoint = DebugEditorBreakpoint();
}

static void clearLocation(DebugEditorLocation* location) {
    if (!location) return;
    *location = DebugEditorLocation();
}

static bool sourceEqual(const char* leftProject, const char* leftPath,
                        const char* rightProject, const char* rightPath) {
    return leftProject && rightProject && leftPath && rightPath &&
        PathsEqual(leftProject, rightProject) && PathsEqual(leftPath, rightPath);
}

static bool isAcceptedBreakpoint(const DebugEditorBreakpoint& breakpoint) {
    return breakpoint.used && (breakpoint.configured || breakpoint.id != 0) &&
        breakpoint.projectId[0] != '\0' && breakpoint.sourcePath[0] != '\0' &&
        breakpoint.line != 0;
}

static bool copyLocation(DebugEditorLocation* location, uint64_t sessionGeneration,
                         uint64_t stopGeneration, const char* projectId,
                         const char* sourcePath, uint32_t line, uint32_t column) {
    if (!location || sessionGeneration == 0 || stopGeneration == 0 || !projectId ||
        !sourcePath || projectId[0] == '\0' || sourcePath[0] == '\0' || line == 0 ||
        !copyText(location->projectId, sizeof(location->projectId), projectId) ||
        !copyText(location->sourcePath, sizeof(location->sourcePath), sourcePath)) {
        if (location) clearLocation(location);
        return false;
    }
    location->valid = true;
    location->sessionGeneration = sessionGeneration;
    location->stopGeneration = stopGeneration;
    location->line = line;
    location->column = column;
    return true;
}

} // namespace

void DebugEditorModelInit(DebugEditorModel* model) {
    if (!model) return;
    *model = DebugEditorModel();
    for (uint32_t i = 0; i < kDebugEditorMaxBreakpoints; ++i)
        clearBreakpoint(&model->breakpoints[i]);
}

void DebugEditorModelResetRuntime(DebugEditorModel* model) {
    if (!model) return;
    model->sessionGeneration = 0;
    model->stopGeneration = 0;
    model->sessionActive = false;
    model->paused = false;
    model->sourceTrustworthy = false;
    clearLocation(&model->execution);
    clearLocation(&model->inspection);
}

void DebugEditorModelRefreshBreakpoints(DebugEditorModel* model,
                                        const DebugEditorBreakpoint* rows,
                                        uint32_t rowCount) {
    if (!model) return;
    for (uint32_t i = 0; i < kDebugEditorMaxBreakpoints; ++i)
        clearBreakpoint(&model->breakpoints[i]);
    model->breakpointCount = 0;
    model->breakpointsTruncated = false;
    if (!rows) return;

    const uint32_t boundedRows = rowCount > kDebugEditorMaxBreakpoints
        ? kDebugEditorMaxBreakpoints : rowCount;
    for (uint32_t i = 0; i < rowCount; ++i) {
        const DebugEditorBreakpoint& candidate = rows[i];
        if (!isAcceptedBreakpoint(candidate)) continue;
        const int existing = DebugEditorModelFindBreakpoint(model, candidate.projectId,
                                                            candidate.sourcePath, candidate.line);
        if (existing >= 0) {
            DebugEditorBreakpoint& prior = model->breakpoints[static_cast<uint32_t>(existing)];
            if (prior.configured && !candidate.configured) prior = candidate;
            continue;
        }
        if (model->breakpointCount >= kDebugEditorMaxBreakpoints) {
            model->breakpointsTruncated = true;
            break;
        }
        model->breakpoints[model->breakpointCount] = candidate;
        model->breakpoints[model->breakpointCount].used = true;
        ++model->breakpointCount;
    }
    if (rowCount > boundedRows) model->breakpointsTruncated = true;
}

const DebugEditorBreakpoint* DebugEditorModelBreakpointAt(const DebugEditorModel* model,
                                                           uint32_t index) {
    if (!model || index >= model->breakpointCount || index >= kDebugEditorMaxBreakpoints ||
        !model->breakpoints[index].used) return nullptr;
    return &model->breakpoints[index];
}

int DebugEditorModelFindBreakpoint(const DebugEditorModel* model,
                                   const char* projectId, const char* sourcePath,
                                   uint32_t line) {
    if (!model || !projectId || !sourcePath || line == 0) return -1;
    for (uint32_t i = 0; i < model->breakpointCount && i < kDebugEditorMaxBreakpoints; ++i) {
        const DebugEditorBreakpoint& breakpoint = model->breakpoints[i];
        if (breakpoint.used && breakpoint.line == line &&
            sourceEqual(breakpoint.projectId, breakpoint.sourcePath, projectId, sourcePath))
            return static_cast<int>(i);
    }
    return -1;
}

DebugEditorBreakpointVisualState DebugEditorModelLineState(const DebugEditorModel* model,
                                                            const char* projectId,
                                                            const char* sourcePath,
                                                            uint32_t line) {
    const int index = DebugEditorModelFindBreakpoint(model, projectId, sourcePath, line);
    if (index < 0) return DebugEditorBreakpointVisualState::None;
    if (model->breakpoints[static_cast<uint32_t>(index)].unresolved)
        return DebugEditorBreakpointVisualState::Unresolved;
    return model->breakpoints[static_cast<uint32_t>(index)].enabled
        ? DebugEditorBreakpointVisualState::Enabled
        : DebugEditorBreakpointVisualState::Disabled;
}

bool DebugEditorModelSetExecution(DebugEditorModel* model, uint64_t sessionGeneration,
                                  uint64_t stopGeneration, bool sessionActive, bool paused,
                                  bool sourceTrustworthy, const char* projectId,
                                  const char* sourcePath, uint32_t line, uint32_t column) {
    if (!model) return false;
    model->sessionGeneration = sessionGeneration;
    model->stopGeneration = stopGeneration;
    model->sessionActive = sessionActive;
    model->paused = paused;
    model->sourceTrustworthy = sourceTrustworthy;
    if (!sessionActive || !paused || !sourceTrustworthy) {
        clearLocation(&model->execution);
        return false;
    }
    return copyLocation(&model->execution, sessionGeneration, stopGeneration,
                        projectId, sourcePath, line, column);
}

void DebugEditorModelClearExecution(DebugEditorModel* model) {
    if (!model) return;
    clearLocation(&model->execution);
    model->paused = false;
    model->sourceTrustworthy = false;
}

bool DebugEditorModelSetInspection(DebugEditorModel* model, uint64_t sessionGeneration,
                                   uint64_t stopGeneration, const char* projectId,
                                   const char* sourcePath, uint32_t line, uint32_t column) {
    if (!model) return false;
    return copyLocation(&model->inspection, sessionGeneration, stopGeneration,
                        projectId, sourcePath, line, column);
}

void DebugEditorModelClearInspection(DebugEditorModel* model) {
    if (model) clearLocation(&model->inspection);
}

bool DebugEditorModelExecutionMatches(const DebugEditorModel* model,
                                      uint64_t sessionGeneration, uint64_t stopGeneration,
                                      const char* projectId, const char* sourcePath,
                                      uint32_t line) {
    return model && model->execution.valid &&
        model->execution.sessionGeneration == sessionGeneration &&
        model->execution.stopGeneration == stopGeneration && model->execution.line == line &&
        sourceEqual(model->execution.projectId, model->execution.sourcePath,
                    projectId, sourcePath);
}

bool DebugEditorNavigationTargetFromSource(DebugEditorNavigationTarget* target,
                                           const char* projectId, const char* sourcePath,
                                           uint32_t line, uint32_t column, bool sourceMapped) {
    if (!target) return false;
    *target = DebugEditorNavigationTarget();
    if (!projectId || !sourcePath || projectId[0] == '\0' || sourcePath[0] == '\0' ||
        line == 0 || !sourceMapped ||
        !copyText(target->projectId, sizeof(target->projectId), projectId) ||
        !copyText(target->sourcePath, sizeof(target->sourcePath), sourcePath)) return false;
    target->valid = true;
    target->sourceMapped = true;
    target->line = line;
    target->column = column;
    return true;
}

} // namespace developer_studio
} // namespace guidexos
