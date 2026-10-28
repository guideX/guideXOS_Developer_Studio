#pragma once

#include "developer_studio_project_search.h"

namespace guidexos {
namespace developer_studio {

#if defined(GXOS_DEVELOPER_STUDIO_BARE_METAL)
static const uint32_t kProjectTaskListMaxTasks = 32u;
#else
static const uint32_t kProjectTaskListMaxTasks = 128u;
#endif
static const uint32_t kProjectTaskListMaxDescriptionBytes = 128u;

enum class ProjectTaskAnnotation : uint8_t {
    Todo = 0,
    Fixme = 1
};

struct ProjectTask {
    ProjectTaskAnnotation annotation;
    uint32_t line;
    char relativePath[kMaxPathBytes];
    char description[kProjectTaskListMaxDescriptionBytes + 1u];
};

struct ProjectTaskList {
    ProjectTask tasks[kProjectTaskListMaxTasks];
    uint32_t count;
    uint32_t todoCount;
    uint32_t fixmeCount;
    uint32_t filesScanned;
    bool truncated;
};

void ProjectTaskListInit(ProjectTaskList* taskList);
bool ProjectTaskListVisitFile(void* userData, const ProjectSearchScanFile* file);
void ProjectTaskListSort(ProjectTaskList* taskList);
const char* ProjectTaskAnnotationName(ProjectTaskAnnotation annotation);

} // namespace developer_studio
} // namespace guidexos
