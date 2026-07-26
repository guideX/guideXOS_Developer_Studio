#pragma once

#include "developer_studio_projects.h"
#include "developer_studio_output.h"

namespace guidexos {
namespace developer_studio {

struct WorkspaceController {
    WorkspaceModel model;
    WorkspaceFileSystem fileSystem;
    bool listingTruncated;
    ModelErrorCode lastError;
    ProjectErrorCode lastProjectError;
};

void WorkspaceControllerInit(WorkspaceController* controller, const WorkspaceFileSystem& fileSystem);
bool WorkspaceControllerOpenWorkspace(WorkspaceController* controller, const char* path);
bool WorkspaceControllerOpenProject(WorkspaceController* controller, const char* path);
bool WorkspaceControllerCreateProject(WorkspaceController* controller, const ProjectCreateRequest& request, ProjectOperationResult* result);
bool WorkspaceControllerReloadProject(WorkspaceController* controller);
bool WorkspaceControllerRefresh(WorkspaceController* controller);
bool WorkspaceControllerEnterSelected(WorkspaceController* controller);
bool WorkspaceControllerGoUp(WorkspaceController* controller);
bool WorkspaceControllerOpenDocument(WorkspaceController* controller, const char* path);
bool WorkspaceControllerOpenDocumentAtLocation(WorkspaceController* controller, const char* projectId, const char* relativePath,
                                               uint32_t line, uint32_t column, uint32_t* outDocumentIndex, OutputErrorCode* error);
bool WorkspaceControllerSetCaretPosition(WorkspaceController* controller, uint32_t documentIndex, uint32_t line, uint32_t column,
                                          bool* outLocationClamped, OutputErrorCode* error);
bool WorkspaceControllerSaveDocument(WorkspaceController* controller, uint32_t documentIndex);
bool WorkspaceControllerSaveActive(WorkspaceController* controller);
bool WorkspaceControllerSaveAll(WorkspaceController* controller);
bool WorkspaceControllerHasDirtyProjectDocuments(const WorkspaceController* controller);
bool WorkspaceControllerSaveAllProjectDocuments(WorkspaceController* controller);
bool WorkspaceControllerCloseDocument(WorkspaceController* controller, uint32_t documentIndex, CloseDecision decision);
bool WorkspaceControllerCloseWorkspace(WorkspaceController* controller, CloseDecision decision);
Document* WorkspaceControllerActiveDocument(WorkspaceController* controller);
const Document* WorkspaceControllerActiveDocumentConst(const WorkspaceController* controller);
const char* WorkspaceControllerError(const WorkspaceController* controller);
const char* WorkspaceControllerProjectError(const WorkspaceController* controller);

} // namespace developer_studio
} // namespace guidexos
