#pragma once

namespace guidexos {
namespace developer_studio {

enum class ProjectKind {
    NativeGuiApplication = 0,
    ConsoleApplication,
    ComponentLibrary,
    WebsiteOrHttpService,
    Game2D,
    SystemComponent,
    ExperimentalOther
};

enum class CapabilityMaturity {
    Unavailable = 0,
    Experimental,
    Partial,
    Supported,
    Deprecated
};

struct Capability {
    const char* id;
    const char* displayName;
    bool available;
    CapabilityMaturity maturity;
};

struct TargetProfile {
    const char* id;
    const char* displayName;
    const char* architecture;
    const char* abi;
    const char* machine;
    const char* sdk;
    const char* toolchain;
    const char* runner;
    const Capability* capabilities;
    unsigned capabilityCount;
    CapabilityMaturity maturity;
};

struct Project {
    const char* id;
    const char* displayName;
    ProjectKind kind;
};

struct Workspace {
    const char* id;
    const char* displayName;
    const Project* projects;
    unsigned projectCount;
    const char* const* openDocuments;
    unsigned openDocumentCount;
};

const TargetProfile& InitialTargetProfile();
bool IsValidTargetProfile(const TargetProfile& profile);
const char* ToString(ProjectKind kind);
const char* ToString(CapabilityMaturity maturity);

} // namespace developer_studio
} // namespace guidexos
