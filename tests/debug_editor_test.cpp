#include "developer_studio_debug_editor.h"

#include <cassert>
#include <cstring>
#include <iostream>

using namespace guidexos::developer_studio;

static DebugEditorBreakpoint row(uint64_t id, const char* project, const char* path,
                                 uint32_t line, bool enabled, uint32_t action) {
    DebugEditorBreakpoint value = {};
    value.used = true;
    value.id = id;
    std::strcpy(value.projectId, project);
    std::strcpy(value.sourcePath, path);
    value.line = line;
    value.column = 1;
    value.enabled = enabled;
    value.action = action;
    return value;
}

static DebugEditorBreakpoint configuredRow(const char* project, const char* path,
                                          uint32_t line, bool enabled, uint32_t action,
                                          bool unresolved = false) {
    DebugEditorBreakpoint value = row(0, project, path, line, enabled, action);
    value.configured = true;
    value.unresolved = unresolved;
    return value;
}

int main() {
    DebugEditorModel model = {};
    DebugEditorModelInit(&model);
    DebugEditorBreakpoint rows[10] = {};
    rows[0] = row(11, "demo", "src/main.cpp", 4, true, 1);
    rows[1] = row(12, "demo", "tests/main.cpp", 4, false, 1);
    rows[2] = row(13, "demo", "src/main.cpp", 4, true, 1); // duplicate identity
    rows[3] = row(14, "demo", "src/helper.cpp", 8, true, 2);
    rows[4] = row(15, "demo", "src/four.cpp", 1, true, 1);
    rows[5] = row(16, "demo", "src/five.cpp", 1, true, 1);
    rows[6] = row(17, "demo", "src/six.cpp", 1, true, 1);
    rows[7] = row(18, "demo", "src/seven.cpp", 1, true, 1);
    rows[8] = row(19, "demo", "src/eight.cpp", 1, true, 1);
    rows[9] = row(20, "demo", "src/nine.cpp", 1, true, 1);
    DebugEditorModelRefreshBreakpoints(&model, rows, 10);
    assert(model.breakpointCount == kDebugEditorMaxBreakpoints);
    assert(model.breakpointsTruncated);
    assert(DebugEditorModelFindBreakpoint(&model, "DEMO", "src/main.cpp", 4) == 0);
    assert(DebugEditorModelFindBreakpoint(&model, "demo", "tests/main.cpp", 4) == 1);
    assert(DebugEditorModelLineState(&model, "demo", "tests/main.cpp", 4) ==
           DebugEditorBreakpointVisualState::Disabled);
    assert(DebugEditorModelLineState(&model, "demo", "src/helper.cpp", 8) ==
           DebugEditorBreakpointVisualState::Enabled);
    assert(DebugEditorModelBreakpointAt(&model, 8) == nullptr);

    DebugEditorBreakpoint configuredRows[4] = {};
    configuredRows[0] = configuredRow("demo", "src/configured-enabled.cpp", 3, true, 1);
    configuredRows[1] = configuredRow("demo", "src/configured-disabled.cpp", 5, false, 1);
    configuredRows[2] = configuredRow("demo", "src/configured-unresolved.cpp", 7, true, 1, true);
    configuredRows[3] = row(91, "demo", "src/configured-enabled.cpp", 3, false, 2);
    DebugEditorModelRefreshBreakpoints(&model, configuredRows, 3);
    assert(model.breakpointCount == 3);
    assert(DebugEditorModelBreakpointAt(&model, 0)->id == 0);
    assert(DebugEditorModelBreakpointAt(&model, 0)->configured);
    assert(DebugEditorModelLineState(&model, "demo", "src/configured-enabled.cpp", 3) ==
           DebugEditorBreakpointVisualState::Enabled);
    DebugEditorModelResetRuntime(&model);
    assert(model.breakpointCount == 3);
    assert(DebugEditorModelBreakpointAt(&model, 0)->id == 0);
    assert(DebugEditorModelBreakpointAt(&model, 0)->configured);

    DebugEditorModelRefreshBreakpoints(&model, configuredRows, 4);
    assert(model.breakpointCount == 3);
    const int configuredEnabled = DebugEditorModelFindBreakpoint(
        &model, "demo", "src/configured-enabled.cpp", 3);
    assert(configuredEnabled >= 0);
    assert(DebugEditorModelBreakpointAt(&model, static_cast<uint32_t>(configuredEnabled))->id == 91);
    assert(!DebugEditorModelBreakpointAt(&model, static_cast<uint32_t>(configuredEnabled))->configured);
    assert(DebugEditorModelLineState(&model, "demo", "src/configured-enabled.cpp", 3) ==
           DebugEditorBreakpointVisualState::Disabled);
    assert(DebugEditorModelLineState(&model, "demo", "src/configured-disabled.cpp", 5) ==
           DebugEditorBreakpointVisualState::Disabled);
    assert(DebugEditorModelLineState(&model, "demo", "src/configured-unresolved.cpp", 7) ==
           DebugEditorBreakpointVisualState::Unresolved);

    assert(DebugEditorModelSetExecution(&model, 7, 3, true, true, true,
                                        "demo", "src/helper.cpp", 8, 1));
    assert(DebugEditorModelExecutionMatches(&model, 7, 3, "demo", "src/helper.cpp", 8));
    assert(!DebugEditorModelExecutionMatches(&model, 7, 3, "demo", "src/main.cpp", 8));
    assert(DebugEditorModelSetInspection(&model, 7, 3, "demo", "tests/main.cpp", 4, 1));
    assert(model.inspection.valid && model.execution.sourcePath[0] != '\0');

    DebugEditorModelSetExecution(&model, 7, 3, true, false, false,
                                 "demo", "src/helper.cpp", 8, 1);
    assert(!model.execution.valid);
    DebugEditorModelResetRuntime(&model);
    assert(!model.execution.valid && !model.inspection.valid && model.breakpointCount == 3);
    assert(DebugEditorModelBreakpointAt(&model, static_cast<uint32_t>(configuredEnabled))->id == 91);

    // The application discards its live snapshot and rebuilds from persisted
    // rows at a generation boundary; the editor never invents generation B IDs.
    configuredRows[2].unresolved = false;
    DebugEditorModelRefreshBreakpoints(&model, configuredRows, 3);
    assert(model.sessionGeneration == 0 && model.stopGeneration == 0);
    assert(!model.sessionActive && !model.paused && !model.sourceTrustworthy);
    for (uint32_t i = 0; i < 3; ++i) {
        assert(model.breakpoints[i].configured && model.breakpoints[i].id == 0);
        assert(!model.breakpoints[i].unresolved);
    }
    assert(!model.breakpoints[1].enabled);
    configuredRows[2].unresolved = true; // failed again, retained in order
    configuredRows[3] = row(192, "demo", "src/configured-enabled.cpp", 3, true, 1);
    DebugEditorModelRefreshBreakpoints(&model, configuredRows, 4);
    assert(model.breakpointCount == 3 && model.breakpoints[0].id == 192);
    assert(model.breakpoints[1].id == 0 && !model.breakpoints[1].enabled);
    assert(model.breakpoints[2].id == 0 && model.breakpoints[2].unresolved);
    assert(DebugEditorModelSetExecution(&model, 8, 1, true, true, true,
                                        "demo", "src/configured-enabled.cpp", 3, 1));
    assert(!DebugEditorModelExecutionMatches(&model, 7, 3,
                                             "demo", "src/configured-enabled.cpp", 3));
    assert(!model.inspection.valid);

    DebugEditorNavigationTarget target = {};
    assert(DebugEditorNavigationTargetFromSource(&target, "demo", "src/helper.cpp", 8, 1, true));
    assert(!DebugEditorNavigationTargetFromSource(&target, "demo", "src/helper.cpp", 8, 1, false));
    std::cout << "Developer Studio source debugger editor model PASS\n";
    return 0;
}
