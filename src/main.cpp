#include <guidexos/ui.h>

#include "developer_studio_models.h"
#include "developer_studio_build.h"
#include "developer_studio_run.h"
#include "developer_studio_output.h"
#include "developer_studio_workspace.h"
#include "developer_studio_project_search.h"
#include "developer_studio_symbols.h"
#include "developer_studio_navigation.h"
#include "developer_studio_references.h"
#include "developer_studio_rename.h"
#include "developer_studio_completion.h"
#include "developer_studio_signature.h"
#include "developer_studio_include_graph.h"
#include "developer_studio_relationships.h"
#include "developer_studio_ownership.h"
#include "developer_studio_types.h"
#include "developer_studio_debugger.h"
#include "developer_studio_debug_symbols.h"
#include "developer_studio_debugger_hosted.h"

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
using guidexos::developer_studio::OffsetToLineColumn;
using guidexos::developer_studio::ValidateTextRange;
using guidexos::developer_studio::DefaultSyntaxPalette;
using guidexos::developer_studio::DetectSyntaxLanguage;
using guidexos::developer_studio::DocumentUpdateSyntax;
using guidexos::developer_studio::FileInfo;
using guidexos::developer_studio::FileInfoKind;
using guidexos::developer_studio::FileListEntry;
using guidexos::developer_studio::InitialTargetProfile;
using guidexos::developer_studio::IsSupportedTextPath;
using guidexos::developer_studio::IsSymbolSourcePath;
using guidexos::developer_studio::IsValidTargetProfile;
using guidexos::developer_studio::JoinWorkspacePath;
using guidexos::developer_studio::PathContainsTraversal;
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
using guidexos::developer_studio::TextBufferSet;
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
using guidexos::developer_studio::ReferenceConfidence;
using guidexos::developer_studio::ReferenceConfidenceName;
using guidexos::developer_studio::ReferenceFileGroup;
using guidexos::developer_studio::ReferenceKind;
using guidexos::developer_studio::ReferenceKindName;
using guidexos::developer_studio::ReferenceMatch;
using guidexos::developer_studio::ReferenceSearchCancel;
using guidexos::developer_studio::ReferenceSearchErrorCode;
using guidexos::developer_studio::ReferenceSearchErrorName;
using guidexos::developer_studio::ReferenceSearchIsActive;
using guidexos::developer_studio::ReferenceSearchOperation;
using guidexos::developer_studio::ReferenceSearchOperationInfo;
using guidexos::developer_studio::ReferenceSearchPoll;
using guidexos::developer_studio::ReferenceSearchRelease;
using guidexos::developer_studio::ReferenceSearchRequest;
using guidexos::developer_studio::ReferenceSearchResultGroupAt;
using guidexos::developer_studio::ReferenceSearchResultGroups;
using guidexos::developer_studio::ReferenceSearchResultMatchAt;
using guidexos::developer_studio::ReferenceSearchService;
using guidexos::developer_studio::ReferenceSearchServiceInit;
using guidexos::developer_studio::ReferenceSearchStart;
using guidexos::developer_studio::ReferenceSearchState;
using guidexos::developer_studio::ReferenceSearchStateName;
using guidexos::developer_studio::ReferenceTarget;
using guidexos::developer_studio::ReferenceTargetFromDefinitionCandidate;
using guidexos::developer_studio::ReferenceTargetFromDefinitionQuery;
using guidexos::developer_studio::ReferenceTargetResolution;
using guidexos::developer_studio::ReferenceTargetResolutionKind;
using guidexos::developer_studio::ResolveReferenceTarget;
using guidexos::developer_studio::kReferenceMaxCandidates;
using guidexos::developer_studio::kReferenceMaxVisibleCandidates;
using guidexos::developer_studio::DocumentSymbol;
using guidexos::developer_studio::ProjectSymbol;
using guidexos::developer_studio::SymbolDatabase;
using guidexos::developer_studio::SymbolDocument;
using guidexos::developer_studio::SymbolDatabaseDocumentPath;
using guidexos::developer_studio::SymbolDatabaseDocumentCount;
using guidexos::developer_studio::SymbolDatabaseDocumentSymbolAt;
using guidexos::developer_studio::SymbolDatabaseFindDocumentById;
using guidexos::developer_studio::SymbolDatabaseFindSymbols;
using guidexos::developer_studio::SymbolDatabaseInit;
using guidexos::developer_studio::SymbolDatabaseProjectSymbolAt;
using guidexos::developer_studio::SymbolDatabaseProjectSymbolCount;
using guidexos::developer_studio::SymbolDatabaseIsTruncated;
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
using guidexos::developer_studio::SymbolRelationship;
using guidexos::developer_studio::SymbolRelationshipGroup;
using guidexos::developer_studio::SymbolRelationshipEndpoint;
using guidexos::developer_studio::SymbolRelationshipGraph;
using guidexos::developer_studio::SymbolRelationshipGraphBuildInfo;
using guidexos::developer_studio::SymbolRelationshipGraphBuildIsActive;
using guidexos::developer_studio::SymbolRelationshipGraphBuildPoll;
using guidexos::developer_studio::SymbolRelationshipGraphBuildStart;
using guidexos::developer_studio::SymbolRelationshipGraphInit;
using guidexos::developer_studio::SymbolRelationshipGraphIsCurrent;
using guidexos::developer_studio::SymbolRelationshipGraphService;
using guidexos::developer_studio::SymbolRelationshipGraphServiceInit;
using guidexos::developer_studio::SymbolRelationshipGraphStorage;
using guidexos::developer_studio::SymbolRelationshipGraphStorageInit;
using guidexos::developer_studio::SymbolRelationshipConfidence;
using guidexos::developer_studio::RelationshipGraphState;
using guidexos::developer_studio::SymbolRelationshipSymbolId;
using guidexos::developer_studio::SymbolRelationshipGraphFindDefinitions;
using guidexos::developer_studio::SymbolRelationshipGraphFindDeclarations;
using guidexos::developer_studio::FileOwnershipEndpoint;
using guidexos::developer_studio::FileOwnershipGroup;
using guidexos::developer_studio::FileOwnershipGraph;
using guidexos::developer_studio::OwnershipBucket;
using guidexos::developer_studio::OwnershipBucketRef;
using guidexos::developer_studio::OwnershipEvidence;
using guidexos::developer_studio::OwnershipPairSlot;
using guidexos::developer_studio::OwnershipRelationshipIdentity;
using guidexos::developer_studio::OwnershipBuildRequest;
using guidexos::developer_studio::OwnershipBuildScratch;
using guidexos::developer_studio::OwnershipBuildState;
using guidexos::developer_studio::OwnershipBuildStateName;
using guidexos::developer_studio::OwnershipClassifyPath;
using guidexos::developer_studio::OwnershipEvidenceKindName;
using guidexos::developer_studio::OwnershipFileRecord;
using guidexos::developer_studio::OwnershipGraphBuildCancel;
using guidexos::developer_studio::OwnershipGraphBuildInfo;
using guidexos::developer_studio::OwnershipGraphBuildIsActive;
using guidexos::developer_studio::OwnershipGraphBuildPoll;
using guidexos::developer_studio::OwnershipGraphBuildStart;
using guidexos::developer_studio::OwnershipGraphCandidateAt;
using guidexos::developer_studio::OwnershipGraphCandidatesForFile;
using guidexos::developer_studio::OwnershipGraphInit;
using guidexos::developer_studio::OwnershipGraphIsCurrent;
using guidexos::developer_studio::OwnershipGraphService;
using guidexos::developer_studio::OwnershipGraphServiceInit;
using guidexos::developer_studio::OwnershipGraphStorage;
using guidexos::developer_studio::OwnershipGraphStorageInit;
using guidexos::developer_studio::OwnershipHashText;
using guidexos::developer_studio::OwnershipResolution;
using guidexos::developer_studio::OwnershipResolutionKind;
using guidexos::developer_studio::OwnershipResolveFile;
using guidexos::developer_studio::ProjectCodeFileKind;
using guidexos::developer_studio::ProjectCodeFileKindName;
using guidexos::developer_studio::CounterpartConfidenceName;
using guidexos::developer_studio::kOwnershipMaxPathBytes;
using guidexos::developer_studio::kOwnershipMaxVisiblePickerCandidates;
using guidexos::developer_studio::kOwnershipMaxEvidenceDetailBytes;
using guidexos::developer_studio::FileCounterpartCandidate;
using guidexos::developer_studio::DefinitionReasonExactQualifiedName;
using guidexos::developer_studio::DefinitionReasonMatchingKind;
using guidexos::developer_studio::SymbolDeclarationRole;
using guidexos::developer_studio::DefinitionCandidate;
using guidexos::developer_studio::DefinitionIdentifier;
using guidexos::developer_studio::DefinitionQuery;
using guidexos::developer_studio::DefinitionResolution;
using guidexos::developer_studio::DefinitionResolutionKind;
using guidexos::developer_studio::ExtractDefinitionIdentifier;
using guidexos::developer_studio::BuildDefinitionQuery;
using guidexos::developer_studio::ResolveDefinition;
using guidexos::developer_studio::DefinitionResolutionKindName;
using guidexos::developer_studio::NavigationHistory;
using guidexos::developer_studio::NavigationLocation;
using guidexos::developer_studio::NavigationHistoryInit;
using guidexos::developer_studio::NavigationHistoryBack;
using guidexos::developer_studio::NavigationHistoryForward;
using guidexos::developer_studio::NavigationHistoryPush;
using guidexos::developer_studio::kDefinitionMaxCandidates;
using guidexos::developer_studio::kDefinitionMaxVisibleCandidates;
using guidexos::developer_studio::SymbolDeclarationRoleName;
using guidexos::developer_studio::RenameApply;
using guidexos::developer_studio::RenameCandidateState;
using guidexos::developer_studio::RenameCandidateStateName;
using guidexos::developer_studio::RenameErrorName;
using guidexos::developer_studio::RenameEditCandidate;
using guidexos::developer_studio::RenameModel;
using guidexos::developer_studio::RenameModelBuildFromReferences;
using guidexos::developer_studio::RenameModelClearSelection;
using guidexos::developer_studio::RenameModelInit;
using guidexos::developer_studio::RenameModelSelectExact;
using guidexos::developer_studio::RenameModelSelectedCount;
using guidexos::developer_studio::RenameModelSetCandidateSelected;
using guidexos::developer_studio::RenameModelSetNewName;
using guidexos::developer_studio::RenameModelStatus;
using guidexos::developer_studio::RenameState;
using guidexos::developer_studio::RenameSymbolKindSupported;
using guidexos::developer_studio::RenameUndoLast;
using guidexos::developer_studio::RenameUndoManager;
using guidexos::developer_studio::RenameUndoManagerInit;
using guidexos::developer_studio::RenameUndoAvailable;
using guidexos::developer_studio::RenameValidateNewName;
using guidexos::developer_studio::kRenameMaxIdentifierBytes;
using guidexos::developer_studio::RenameErrorCode;
using guidexos::developer_studio::CompletionCandidate;
using guidexos::developer_studio::CompletionCandidateKindPrefix;
using guidexos::developer_studio::CompletionContextKindName;
using guidexos::developer_studio::CompletionContext;
using guidexos::developer_studio::CompletionExtractContext;
using guidexos::developer_studio::CompletionMemberResolutionStatusText;
using guidexos::developer_studio::CompletionErrorCode;
using guidexos::developer_studio::CompletionErrorName;
using guidexos::developer_studio::CompletionProjectId;
using guidexos::developer_studio::CompletionSession;
using guidexos::developer_studio::CompletionBuildSession;
using guidexos::developer_studio::CompletionSessionDismiss;
using guidexos::developer_studio::CompletionSessionEnd;
using guidexos::developer_studio::CompletionSessionHome;
using guidexos::developer_studio::CompletionSessionInit;
using guidexos::developer_studio::CompletionSessionIsCurrent;
using guidexos::developer_studio::CompletionSessionMove;
using guidexos::developer_studio::CompletionSessionPage;
using guidexos::developer_studio::CompletionSessionRefresh;
using guidexos::developer_studio::CompletionSessionSelected;
using guidexos::developer_studio::CompletionSessionTextMatches;
using guidexos::developer_studio::DocumentWordCache;
using guidexos::developer_studio::DocumentWordCacheInit;
using guidexos::developer_studio::DocumentWordEntry;
using guidexos::developer_studio::kCompletionMaxDocumentWords;
using guidexos::developer_studio::kCompletionMaxRetainedCandidates;
using guidexos::developer_studio::kCompletionMaxVisibleCandidates;
using guidexos::developer_studio::SignatureCandidate;
using guidexos::developer_studio::SignatureContextKind;
using guidexos::developer_studio::SignatureErrorCode;
using guidexos::developer_studio::SignatureErrorName;
using guidexos::developer_studio::SignatureHelpSession;
using guidexos::developer_studio::SignatureHelpSessionActiveParameter;
using guidexos::developer_studio::SignatureHelpSessionDismiss;
using guidexos::developer_studio::SignatureHelpSessionEnd;
using guidexos::developer_studio::SignatureHelpSessionHome;
using guidexos::developer_studio::SignatureHelpSessionInit;
using guidexos::developer_studio::SignatureHelpSessionMove;
using guidexos::developer_studio::SignatureHelpSessionPage;
using guidexos::developer_studio::SignatureHelpSessionRefresh;
using guidexos::developer_studio::SignatureHelpSessionSelected;
using guidexos::developer_studio::SignatureHelpBuildSession;
using guidexos::developer_studio::SignatureStatusText;
using guidexos::developer_studio::SignatureProjectId;
using guidexos::developer_studio::kSignatureMaxParameters;
using guidexos::developer_studio::kSignatureMaxRetainedCandidates;
using guidexos::developer_studio::IncludeDelimiterKind;
using guidexos::developer_studio::IncludeDirectiveState;
using guidexos::developer_studio::IncludeResolutionState;
using guidexos::developer_studio::IncludeGraph;
using guidexos::developer_studio::IncludeGraphStorage;
using guidexos::developer_studio::IncludeGraphBuildOperation;
using guidexos::developer_studio::IncludeGraphBuildState;
using guidexos::developer_studio::IncludeGraphErrorCode;
using guidexos::developer_studio::IncludeEdge;
using guidexos::developer_studio::IncludeNode;
using guidexos::developer_studio::IncludeGraphTraversalDirection;
using guidexos::developer_studio::IncludeGraphTraversalResult;
using guidexos::developer_studio::IncludeGraphDocumentSnapshot;
using guidexos::developer_studio::IncludeGraphRequest;
using guidexos::developer_studio::IncludeGraphStorageInit;
using guidexos::developer_studio::IncludeGraphBuildOperationInit;
using guidexos::developer_studio::IncludeGraphInit;
using guidexos::developer_studio::IncludeGraphStart;
using guidexos::developer_studio::IncludeGraphPoll;
using guidexos::developer_studio::IncludeGraphCancel;
using guidexos::developer_studio::IncludeGraphIsActive;
using guidexos::developer_studio::IncludeGraphOperationInfo;
using guidexos::developer_studio::IncludeGraphRelease;
using guidexos::developer_studio::IncludeGraphIsCurrent;
using guidexos::developer_studio::IncludeGraphNodeAt;
using guidexos::developer_studio::IncludeGraphEdgeAt;
using guidexos::developer_studio::IncludeGraphCycleAt;
using guidexos::developer_studio::IncludeGraphCandidateAt;
using guidexos::developer_studio::IncludeGraphBuildTraversal;
using guidexos::developer_studio::IncludeGraphFindDirectiveAt;
using guidexos::developer_studio::IncludeGraphRescanDocument;
using guidexos::developer_studio::IncludeGraphBuildStateName;
using guidexos::developer_studio::IncludeGraphErrorName;
using guidexos::developer_studio::IncludeGraphStatusText;
using guidexos::developer_studio::IncludeDelimiterName;
using guidexos::developer_studio::IncludeDirectiveStateName;
using guidexos::developer_studio::IncludeResolutionStateName;
using guidexos::developer_studio::IsIncludeGraphSourcePath;
using guidexos::developer_studio::IsIncludeGraphHeaderPath;
using guidexos::developer_studio::kIncludeGraphMaxNodes;
using guidexos::developer_studio::kIncludeGraphMaxEdges;
using guidexos::developer_studio::kIncludeGraphMaxAmbiguousCandidates;
using guidexos::developer_studio::TypeDatabase;
using guidexos::developer_studio::TypeDocument;
using guidexos::developer_studio::TypeDatabaseClear;
using guidexos::developer_studio::TypeDatabaseIndexDocument;
using guidexos::developer_studio::TypeDatabaseIndexProject;
using guidexos::developer_studio::TypeDatabaseInit;
using guidexos::developer_studio::TypeDatabaseInspectAt;
using guidexos::developer_studio::TypeDatabaseIsCurrent;
using guidexos::developer_studio::TypeDatabaseIsTruncated;
using guidexos::developer_studio::TypeDeclarationKindName;
using guidexos::developer_studio::TypeInspection;
using guidexos::developer_studio::TypeInspectionState;
using guidexos::developer_studio::TypeInspectionStateName;
using guidexos::developer_studio::TypeRecord;
using guidexos::developer_studio::TypeMemberBucket;
using guidexos::developer_studio::TypeSourceName;
using guidexos::developer_studio::kTypeMaxHoverTextBytes;
using guidexos::developer_studio::kTypeMaxRecords;
using guidexos::developer_studio::kTypeMaxMemberBuckets;
using guidexos::developer_studio::DebugBackend;
using guidexos::developer_studio::DebugBreakpoint;
using guidexos::developer_studio::DebugBreakpointState;
using guidexos::developer_studio::DebugController;
using guidexos::developer_studio::DebugControllerBreakpointAt;
using guidexos::developer_studio::DebugControllerCanContinue;
using guidexos::developer_studio::DebugControllerCanPause;
using guidexos::developer_studio::DebugControllerCanStart;
using guidexos::developer_studio::DebugControllerCanStop;
using guidexos::developer_studio::DebugControllerClearBreakpoints;
using guidexos::developer_studio::DebugControllerContinue;
using guidexos::developer_studio::DebugControllerDeleteBreakpoint;
using guidexos::developer_studio::DebugControllerInit;
using guidexos::developer_studio::DebugControllerIsActive;
using guidexos::developer_studio::DebugControllerMarkProjectGeneration;
using guidexos::developer_studio::DebugControllerMarkSourceGeneration;
using guidexos::developer_studio::DebugControllerPause;
using guidexos::developer_studio::DebugControllerPoll;
using guidexos::developer_studio::DebugControllerRequestStop;
using guidexos::developer_studio::DebugControllerSetBreakpointEnabled;
using guidexos::developer_studio::DebugControllerSetProjectContext;
using guidexos::developer_studio::DebugControllerStart;
using guidexos::developer_studio::DebugControllerToggleBreakpoint;
using guidexos::developer_studio::DebugErrorCode;
using guidexos::developer_studio::DebugErrorName;
using guidexos::developer_studio::DebugRelativeSourcePath;
using guidexos::developer_studio::DebugSessionState;
using guidexos::developer_studio::DebugSessionStateName;
using guidexos::developer_studio::DebugTarget;
using guidexos::developer_studio::DebugTargetFromBuild;
using guidexos::developer_studio::BuildRequestEnableDebugInfo;
using guidexos::developer_studio::DebugDwarfError;
using guidexos::developer_studio::DebugDwarfMapper;
using guidexos::developer_studio::DebugDwarfMapperLoad;
using guidexos::developer_studio::DebugDwarfMapperReset;
using guidexos::developer_studio::DebugDwarfMapperStateName;
using guidexos::developer_studio::DebugDwarfErrorName;
using guidexos::developer_studio::DebugDwarfMapperIsReady;
using guidexos::developer_studio::DebugControllerMapBreakpoints;
using guidexos::developer_studio::DebugControllerMarkArtifactStale;
using guidexos::developer_studio::HostedDebugBackend;
using guidexos::developer_studio::HostedDebugBackendCreate;
using guidexos::developer_studio::HostedDebugBackendInit;
using guidexos::developer_studio::HostedDebugCommand;
using guidexos::developer_studio::HostedDebugResult;
using guidexos::developer_studio::PathsEqual;
using guidexos::developer_studio::kMaxProjectPathBytes;

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
static const int kReferencesPanelTop = 48;
static const int kReferencesPanelResultsTop = 190;
static const int kReferencesPanelRowHeight = 17;
static const int kReferencesPanelMaxRows = 25;
static const int kReferencesFieldX = 86;
static const int kReferencesFieldWidth = 510;
static const int kCompletionPopupWidth = 430;
static const int kCompletionPopupRowHeight = 20;
static const int kCompletionPopupMaxRows = 9;
static const int kSignaturePopupWidth = 620;
static const int kSignaturePopupRowHeight = 34;
static const int kSignaturePopupMaxRows = 5;
static const int kTypePopupWidth = 610;
static const int kTypePopupRowHeight = 22;
static const int kTypePopupMaxRows = 8;
static const int kIncludeGraphPanelTop = 48;
static const int kIncludeGraphPanelResultsTop = 190;
static const int kIncludeGraphPanelRowHeight = 24;
static const int kIncludeGraphPanelMaxRows = 17;
static const int kIncludeGraphPanelModeWidth = 138;
static const int kDebugPanelTop = 56;
static const int kDebugPanelRowHeight = 22;
static const int kDebugPanelMaxRows = 20;

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
    ConfirmRunClose,
    ConfirmDebug,
    ConfirmDebugClose
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
static CompletionCandidate g_completionCandidateStorage[kCompletionMaxRetainedCandidates] = {};
static DocumentWordEntry g_completionWordStorage[kCompletionMaxDocumentWords] = {};
static CompletionSession g_completionSession = {};
static DocumentWordCache g_completionWordCache = {};
static bool g_completionPopupOpen = false;
static SignatureCandidate g_signatureCandidateStorage[kSignatureMaxRetainedCandidates] = {};
static guidexos::developer_studio::SignatureParameter g_signatureParameterStorage[
    kSignatureMaxRetainedCandidates * kSignatureMaxParameters] = {};
static SignatureHelpSession g_signatureSession = {};
static bool g_signaturePopupOpen = false;
static uint32_t g_signatureScroll = 0;
static char g_signatureStatus[192] = {};
static TypeDatabase g_typeDatabase = {};
static TypeRecord g_typeRecordStorage[kTypeMaxRecords] = {};
static TypeDocument g_typeDocumentStorage[256] = {};
static TypeMemberBucket g_typeMemberBucketStorage[kTypeMaxMemberBuckets] = {};
static uint32_t g_typeMemberIndexStorage[kTypeMaxRecords] = {};
static TypeInspection g_typeInspection = {};
static bool g_typePopupOpen = false;
static char g_typeStatus[160] = {};
static char g_typeDisplayScratch[kTypeMaxHoverTextBytes + 1] = {};
static IncludeGraphStorage g_includeGraphStorage = {};
static IncludeGraphStorage g_includeGraphBuildingStorage = {};
static IncludeGraph g_includeGraph = {};
static IncludeGraph g_includeGraphBuilding = {};
static IncludeGraphBuildOperation g_includeGraphOperation = {};
static uint64_t g_includeGraphOperationId = 0;
static bool g_includeGraphPanelOpen = false;
static bool g_includeGraphResultsFocused = false;
static bool g_includeGraphTerminalReported = false;
static uint32_t g_includeGraphMode = 0;
static uint32_t g_includeGraphScroll = 0;
static uint32_t g_includeGraphSelectedRow = 0;
static uint32_t g_includeTraversalNodes[kIncludeGraphMaxNodes] = {};
static uint32_t g_includeTraversalDepths[kIncludeGraphMaxNodes] = {};
static uint32_t g_includeTraversalParents[kIncludeGraphMaxNodes] = {};
static IncludeGraphTraversalResult g_includeTraversal = {
    g_includeTraversalNodes, g_includeTraversalDepths, g_includeTraversalParents, 0,
    kIncludeGraphMaxNodes, 0, 0, false
};
static char g_includeGraphStatus[192] = {};
static bool g_includeTargetPickerOpen = false;
static uint32_t g_includeTargetEdgeIndex = 0;
static uint32_t g_includeTargetSelected = 0;
static uint32_t g_includeTargetScroll = 0;
static NavigationLocation g_includeNavigationOrigin = {};
static bool g_includeNavigationOriginValid = false;
static char g_completionStatus[160] = {};
static char g_completionProjectId[kMaxProjectIdBytes] = {};
static bool g_completionUndoAvailable = false;
static uint64_t g_completionUndoDocumentId = 0;
static uint32_t g_completionUndoGeneration = 0;
static uint32_t g_completionUndoLength = 0;
static bool g_completionUndoDirty = false;
static uint32_t g_completionUndoCaret = 0;
static uint32_t g_completionUndoSelectionAnchor = 0;
static bool g_completionUndoSelectionActive = false;
static char g_completionUndoText[kMaxEditorBytes + 1] = {};
static gx_file_entry g_fsListEntries[kMaxWorkspaceEntries] = {};
static NativeFileSystemContext g_fileSystemContext = {};
static gx_handle g_window = 0;
static InputMode g_inputMode = InputMode::Normal;
static bool g_editorFocused = false;
static bool g_fileMenuOpen = false;
static bool g_buildMenuOpen = false;
static bool g_debugMenuOpen = false;
static bool g_debugPanelOpen = false;
static uint32_t g_debugPanelTab = 0;
static uint32_t g_debugSelectedBreakpoint = 0;
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
static DebugController g_debugController = {};
static DebugDwarfMapper g_debugMapper = {};
static unsigned char g_debugArtifactBytes[guidexos::developer_studio::kDebugMapperMaxElfBytes] = {};
static HostedDebugBackend g_hostedDebugBackend = {};
static DebugBackend g_debugBackend = {};
static bool g_debugWaitingForBuild = false;
static bool g_debugTerminalReported = false;
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
static ReferenceSearchService g_referenceSearch = {};
static ReferenceTarget g_referenceTarget = {};
static ReferenceTargetResolution g_referenceTargetResolution = {};
static DefinitionCandidate g_referenceCandidates[kReferenceMaxCandidates] = {};
static DefinitionQuery g_referenceQuery = {};
static NavigationLocation g_referenceOrigin = {};
static bool g_referenceOriginValid = false;
static bool g_referencesPanelOpen = false;
static bool g_referencesResultsFocused = false;
static bool g_referencePickerOpen = false;
static uint32_t g_referenceSelectedCandidate = 0;
static uint32_t g_referenceCandidateScroll = 0;
static uint32_t g_referencesScroll = 0;
static uint32_t g_referencesSelectedGroup = 0;
static uint32_t g_referencesSelectedMatch = 0;
static uint64_t g_referencesOperationId = 0;
static uint64_t g_referencesProjectGeneration = 0;
static char g_referencesProjectId[kMaxProjectIdBytes] = {};
static char g_referencesStatus[160] = {};
static bool g_referencesTerminalReported = false;
static uint64_t g_nextReferenceQueryId = 1;
static RenameModel g_renameModel = {};
static RenameUndoManager g_renameUndo = {};
static bool g_renamePanelOpen = false;
static bool g_renameSearchPending = false;
static bool g_renameTargetPickerPending = false;
static bool g_renameNameFocused = true;
static uint32_t g_renameNameCaret = 0;
static uint32_t g_renameSelectedCandidate = 0;
static uint32_t g_renameScroll = 0;
static uint64_t g_nextRenameTransactionId = 1;
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
static NavigationHistory g_navigationHistory = {};
static DefinitionCandidate g_definitionCandidates[kDefinitionMaxCandidates] = {};
static DefinitionResolution g_definitionResolution = {};
static DefinitionQuery g_definitionQuery = {};
static NavigationLocation g_definitionOrigin = {};
static bool g_definitionOriginValid = false;
static bool g_definitionPickerOpen = false;
static uint32_t g_definitionSelected = 0;
static uint32_t g_definitionScroll = 0;
static char g_definitionStatus[160] = {};
static uint64_t g_nextDefinitionQueryId = 1;
static const uint32_t kStudioRelationshipGroupCapacity = 1024u;
static const uint32_t kStudioRelationshipEdgeCapacity = 2048u;
static const uint32_t kStudioRelationshipEndpointCapacity = 8192u;
static SymbolRelationshipGroup g_relationshipGroups[kStudioRelationshipGroupCapacity] = {};
static SymbolRelationship g_relationshipEdges[kStudioRelationshipEdgeCapacity] = {};
static uint32_t g_relationshipDeclarations[kStudioRelationshipEndpointCapacity] = {};
static uint32_t g_relationshipDefinitions[kStudioRelationshipEndpointCapacity] = {};
static uint32_t g_relationshipForwards[kStudioRelationshipEndpointCapacity] = {};
static uint32_t g_relationshipSymbolGroups[kSymbolMaxProjectSymbols] = {};
static SymbolRelationshipGroup g_relationshipBuildingGroups[kStudioRelationshipGroupCapacity] = {};
static SymbolRelationship g_relationshipBuildingEdges[kStudioRelationshipEdgeCapacity] = {};
static uint32_t g_relationshipBuildingDeclarations[kStudioRelationshipEndpointCapacity] = {};
static uint32_t g_relationshipBuildingDefinitions[kStudioRelationshipEndpointCapacity] = {};
static uint32_t g_relationshipBuildingForwards[kStudioRelationshipEndpointCapacity] = {};
static uint32_t g_relationshipBuildingSymbolGroups[kSymbolMaxProjectSymbols] = {};
static SymbolRelationshipGraphStorage g_relationshipStorage = {};
static SymbolRelationshipGraphStorage g_relationshipBuildingStorage = {};
static SymbolRelationshipGraph g_relationshipGraph = {};
static SymbolRelationshipGraph g_relationshipBuildingGraph = {};
static SymbolRelationshipGraphService g_relationshipService = {};
static bool g_relationshipNavigationToDeclaration = false;

// Embedded ownership storage is intentionally smaller than the public model
// limits.  A truncated graph remains explicit and never falls back to a
// header x source Cartesian scan.
static const uint32_t kStudioOwnershipFileCapacity = 1024u;
static const uint32_t kStudioOwnershipCandidateCapacity = 256u;
static const uint32_t kStudioOwnershipGroupCapacity = 128u;
static const uint32_t kStudioOwnershipEvidenceCapacity = 4096u;
static const uint32_t kStudioOwnershipEndpointCapacity = 256u;
static OwnershipFileRecord g_ownershipInventory[kStudioOwnershipFileCapacity] = {};
static FileOwnershipEndpoint g_ownershipCompletedFiles[kStudioOwnershipFileCapacity] = {};
static FileOwnershipEndpoint g_ownershipBuildingFiles[kStudioOwnershipFileCapacity] = {};
static FileCounterpartCandidate g_ownershipCompletedCandidates[kStudioOwnershipCandidateCapacity] = {};
static FileCounterpartCandidate g_ownershipBuildingCandidates[kStudioOwnershipCandidateCapacity] = {};
static guidexos::developer_studio::FileOwnershipGroup g_ownershipCompletedGroups[kStudioOwnershipGroupCapacity] = {};
static guidexos::developer_studio::FileOwnershipGroup g_ownershipBuildingGroups[kStudioOwnershipGroupCapacity] = {};
static guidexos::developer_studio::OwnershipEvidence g_ownershipCompletedEvidence[kStudioOwnershipEvidenceCapacity] = {};
static guidexos::developer_studio::OwnershipEvidence g_ownershipBuildingEvidence[kStudioOwnershipEvidenceCapacity] = {};
static FileOwnershipEndpoint g_ownershipCompletedHeaders[kStudioOwnershipEndpointCapacity] = {};
static FileOwnershipEndpoint g_ownershipBuildingHeaders[kStudioOwnershipEndpointCapacity] = {};
static FileOwnershipEndpoint g_ownershipCompletedSources[kStudioOwnershipEndpointCapacity] = {};
static FileOwnershipEndpoint g_ownershipBuildingSources[kStudioOwnershipEndpointCapacity] = {};
static uint64_t g_ownershipCompletedCandidateIds[kStudioOwnershipCandidateCapacity * 2u] = {};
static uint64_t g_ownershipBuildingCandidateIds[kStudioOwnershipCandidateCapacity * 2u] = {};
static OwnershipGraphStorage g_ownershipCompletedStorage = {};
static OwnershipGraphStorage g_ownershipBuildingStorage = {};
static FileOwnershipGraph g_ownershipCompletedGraph = {};
static FileOwnershipGraph g_ownershipBuildingGraph = {};
static OwnershipGraphService g_ownershipService = {};
static OwnershipBuildScratch g_ownershipScratch = {};
static OwnershipBucketRef g_ownershipExactRefs[kStudioOwnershipFileCapacity] = {};
static OwnershipBucketRef g_ownershipNormalizedRefs[kStudioOwnershipFileCapacity] = {};
static OwnershipBucketRef g_ownershipModuleRefs[kStudioOwnershipFileCapacity] = {};
static OwnershipBucket g_ownershipExactBuckets[kStudioOwnershipFileCapacity] = {};
static OwnershipBucket g_ownershipNormalizedBuckets[kStudioOwnershipFileCapacity] = {};
static OwnershipBucket g_ownershipModuleBuckets[kStudioOwnershipFileCapacity] = {};
static OwnershipPairSlot g_ownershipPairSlots[kStudioOwnershipCandidateCapacity * 4u] = {};
static uint32_t g_ownershipCandidateCounts[kStudioOwnershipFileCapacity] = {};
static OwnershipRelationshipIdentity g_ownershipRelationshipIdentities[kStudioOwnershipCandidateCapacity * 16u] = {};
static uint32_t g_ownershipIncludeQueue[kIncludeGraphMaxNodes] = {};
static bool g_ownershipIncludeVisited[kIncludeGraphMaxNodes] = {};
static uint32_t g_ownershipCandidateIndices[kOwnershipMaxVisiblePickerCandidates] = {};
static OwnershipResolution g_ownershipResolution = {};
static uint64_t g_ownershipOperationId = 0;
static bool g_ownershipPanelOpen = false;
static bool g_ownershipPickerOpen = false;
static bool g_ownershipTerminalReported = false;
static uint32_t g_ownershipSelectedCandidate = 0;
static uint32_t g_ownershipScroll = 0;
static char g_ownershipStatus[192] = {};

static char mapKeyToChar(int keyCode, int modifiers);
static void compose(char* output, uint32_t size, const char* prefix, const char* value, const char* suffix);
static void stopProjectSearch(gx_app_context* ctx);
static uint32_t captureDirtyProjectSearchDocuments();
static void stopReferenceSearch(gx_app_context* ctx);
static void keepCaretVisible(const Document* document);
static bool startRenameSearchWithTarget(gx_app_context* ctx, const ReferenceTarget& target);
static void dismissCompletion(gx_app_context* ctx, const char* reason, bool showStatus);
static bool openCompletion(gx_app_context* ctx, bool manuallyInvoked = true);
static bool refreshCompletion(gx_app_context* ctx);
static bool acceptCompletion(gx_app_context* ctx);
static bool undoCompletion(gx_app_context* ctx);
static void dismissTypeInfo(gx_app_context* ctx, const char* reason, bool showStatus);
static bool openTypeInfo(gx_app_context* ctx);
static bool handleTypeInfoKey(gx_app_context* ctx, int keyCode, int action, int modifiers);
static bool typePopupBounds(gx_rect* output);
static void drawTypePopup(gx_app_context* ctx);
static void dismissSignatureHelp(gx_app_context* ctx, const char* reason, bool showStatus);
static bool openSignatureHelp(gx_app_context* ctx);
static bool refreshSignatureHelp(gx_app_context* ctx);
static void pollIncludeGraph(gx_app_context* ctx);
static bool startIncludeGraph(gx_app_context* ctx);
static bool refreshIncludeGraphDocument(gx_app_context* ctx, Document* document);
static void closeIncludeGraphPanel(gx_app_context* ctx);
static bool handleIncludeGraphKey(gx_app_context* ctx, int keyCode, int action, int modifiers);
static bool handleIncludeTargetPickerKey(gx_app_context* ctx, int keyCode, int action);
static void drawIncludeGraphPanel(gx_app_context* ctx);
static void drawIncludeTargetPicker(gx_app_context* ctx);
static bool tryIncludeGraphDefinition(gx_app_context* ctx);
static void openDefinitionPicker(gx_app_context* ctx);
static uint32_t captureOwnershipInventory();
static bool startOwnershipBuild(gx_app_context* ctx);
static void pollOwnership(gx_app_context* ctx);
static void openFileOwnership(gx_app_context* ctx);
static bool switchHeaderSource(gx_app_context* ctx);
static bool handleOwnershipKey(gx_app_context* ctx, int keyCode, int action);
static void drawOwnershipPanel(gx_app_context* ctx);
static bool activateOwnershipCandidate(gx_app_context* ctx, uint32_t index);
static bool beginDebugBuild(gx_app_context* ctx, BuildDirtyDecision dirtyDecision);
static bool beginDebugSession(gx_app_context* ctx);
static void pollDebug(gx_app_context* ctx);
static void requestDebug(gx_app_context* ctx);
static void requestDebugStop(gx_app_context* ctx);
static bool toggleBreakpointAtCaret(gx_app_context* ctx);
static bool toggleBreakpointAtMouse(gx_app_context* ctx, int x, int y);
static void drawDebugPanel(gx_app_context* ctx);
static bool handleDebugPanelKey(gx_app_context* ctx, int keyCode, int action);
static void reportDebugMessage(gx_app_context* ctx, const char* message);
static void drawText(gx_app_context* ctx, int x, int y, const char* text);
static void drawPanel(gx_app_context* ctx, gx_rect rect, uint32_t color);

static void refreshDebugMappings() {
    if (!DebugDwarfMapperIsReady(&g_debugMapper)) return;
    DebugErrorCode error = DebugErrorCode::None;
    DebugControllerMapBreakpoints(&g_debugController, &g_debugMapper, &error);
}

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

static void appendHexAddress(char* output, uint32_t outputSize, uint64_t value) {
    static const char digits[] = "0123456789ABCDEF";
    appendText(output, outputSize, "0x");
    for (int32_t shift = 60; shift >= 0 && lengthOf(output, outputSize) + 1 < outputSize; shift -= 4) {
        char digit[2] = { digits[(value >> shift) & 0xfu], '\0' };
        appendText(output, outputSize, digit);
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

static void definitionSetStatus(const char* text) {
    copyText(g_definitionStatus, sizeof(g_definitionStatus), text ? text : "");
}

static bool captureNavigationLocation(const Document& document, NavigationLocation* output) {
    if (!output || !g_controller.model.hasProject) return false;
    *output = {};
    copyText(output->projectId, sizeof(output->projectId), g_controller.model.project.projectId);
    output->projectGeneration = g_controller.model.projectGeneration;
    output->documentId = document.documentId;
    output->documentGeneration = document.buffer.generation;
    if (!copyProjectRelativePath(g_controller.model, document.path, output->relativePath, sizeof(output->relativePath))) return false;
    output->caretByteOffset = document.buffer.caret;
    output->selectionStart = document.buffer.selectionActive && document.buffer.selectionAnchor < document.buffer.caret ?
        document.buffer.selectionAnchor : (document.buffer.selectionActive ? document.buffer.caret : document.buffer.caret);
    output->selectionEnd = document.buffer.selectionActive && document.buffer.selectionAnchor > document.buffer.caret ?
        document.buffer.selectionAnchor : document.buffer.caret;
    OffsetToLineColumn(&document.buffer, document.buffer.caret, &output->line, &output->column);
    ++output->line;
    ++output->column;
    output->viewportTopLine = g_editorScrollLine;
    return true;
}

static bool restoreNavigationLocation(gx_app_context* ctx, const NavigationLocation& location) {
    if (!g_controller.model.open || !g_controller.model.hasProject ||
        !searchTextEqual(location.projectId, g_controller.model.project.projectId) ||
        location.projectGeneration != g_controller.model.projectGeneration) {
        definitionSetStatus("Navigation history belongs to a stale project.");
        if (ctx) logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER navigation_back=FAIL reason=NAVIGATION_HISTORY_PROJECT_STALE");
        return false;
    }
    if (location.relativePath[0] == '\0' || PathContainsTraversal(location.relativePath) ||
        location.relativePath[0] == '/' || location.relativePath[0] == '\\' || location.relativePath[1] == ':') {
        definitionSetStatus("Navigation history path is invalid.");
        if (ctx) logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER navigation_back=FAIL reason=NAVIGATION_HISTORY_PATH_INVALID");
        return false;
    }
    uint32_t documentIndex = kMaxOpenDocuments;
    OutputErrorCode error = OutputErrorCode::None;
    if (!WorkspaceControllerOpenDocumentAtLocation(&g_controller, g_controller.model.project.projectId,
                                                   location.relativePath, location.line, location.column,
                                                   &documentIndex, &error)) {
        definitionSetStatus("Navigation history location is unavailable.");
        if (ctx) markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER navigation_back=FAIL", OutputErrorName(error));
        return false;
    }
    Document* document = WorkspaceControllerActiveDocument(&g_controller);
    if (!document) return false;
    if (document->documentId == location.documentId && document->buffer.generation == location.documentGeneration) {
        uint32_t caret = location.caretByteOffset > document->buffer.length ? document->buffer.length : static_cast<uint32_t>(location.caretByteOffset);
        SetCaretOffset(&document->buffer, caret);
        if (location.selectionEnd > location.selectionStart && location.selectionStart <= document->buffer.length &&
            location.selectionEnd <= document->buffer.length && location.selectionEnd > location.selectionStart)
            SelectTextRange(&document->buffer, location.selectionStart, location.selectionEnd - location.selectionStart);
    }
    g_editorScrollLine = location.viewportTopLine;
    g_editorFocused = true;
    g_outputFocused = false;
    keepCaretVisible(document);
    definitionSetStatus("");
    if (ctx) logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER navigation_back=PASS");
    return true;
}

static bool selectDefinitionIdentifier(Document* document, const DefinitionCandidate& candidate, bool* stale) {
    if (stale) *stale = false;
    if (!document) return false;
    const char* name = candidate.symbol.symbol.name;
    const uint32_t nameLength = lengthOf(name, kSymbolMaxNameBytes);
    if (nameLength == 0) return false;
    const SymbolLocation& location = candidate.symbol.symbol.location;
    if (location.documentId == document->documentId && location.generation == document->buffer.generation &&
        location.identifierOffset <= document->buffer.length && nameLength <= document->buffer.length - location.identifierOffset &&
        textAtOffset(document->buffer, location.identifierOffset, name))
        return SelectTextRange(&document->buffer, location.identifierOffset, nameLength);
    if (stale) *stale = true;
    const uint32_t zeroLine = location.line > 0 ? location.line - 1 : 0;
    const uint32_t lineCount = TextBufferLineCount(&document->buffer);
    if (zeroLine < lineCount) {
        const uint32_t start = TextBufferLineStart(&document->buffer, zeroLine);
        const uint32_t end = TextBufferLineEnd(&document->buffer, zeroLine);
        uint32_t matches = 0;
        uint32_t matchOffset = 0;
        for (uint32_t offset = start; offset + nameLength <= end; ++offset) {
            if (textAtOffset(document->buffer, offset, name)) { ++matches; matchOffset = offset; }
        }
        if (matches == 1) return SelectTextRange(&document->buffer, matchOffset, nameLength);
    }
    const uint32_t expected = location.identifierOffset;
    const uint32_t start = expected > 16u * 1024u ? expected - 16u * 1024u : 0;
    const uint32_t end = document->buffer.length < expected + 16u * 1024u ?
        document->buffer.length : expected + 16u * 1024u;
    uint32_t matches = 0;
    uint32_t matchOffset = 0;
    for (uint32_t offset = start; offset + nameLength <= end; ++offset) {
        if (textAtOffset(document->buffer, offset, name)) { ++matches; matchOffset = offset; }
    }
    if (matches == 1) return SelectTextRange(&document->buffer, matchOffset, nameLength);
    return false;
}

static bool activateDefinitionCandidate(gx_app_context* ctx, uint32_t index) {
    if (!g_definitionPickerOpen && index >= g_definitionResolution.candidateCount) return false;
    if (index >= g_definitionResolution.candidateCount || !g_controller.model.open || !g_controller.model.hasProject) return false;
    if (g_definitionQuery.projectGeneration != g_controller.model.projectGeneration ||
        !searchTextEqual(g_definitionQuery.projectId, g_controller.model.project.projectId)) {
        definitionSetStatus("Definition result belongs to a stale project.");
        markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER goto_definition_activate=FAIL", "GOTO_DEFINITION_PROJECT_STALE");
        return false;
    }
    const DefinitionCandidate candidate = g_definitionResolution.candidates[index];
    if (candidate.relativePath[0] == '\0' || PathContainsTraversal(candidate.relativePath) ||
        candidate.relativePath[0] == '/' || candidate.relativePath[0] == '\\' || candidate.relativePath[1] == ':') {
        definitionSetStatus("Definition path is outside the project.");
        markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER goto_definition_activate=FAIL", "GOTO_DEFINITION_PATH_OUTSIDE_PROJECT");
        return false;
    }
    if (!g_definitionOriginValid) {
        Document* origin = WorkspaceControllerActiveDocument(&g_controller);
        if (!origin || !captureNavigationLocation(*origin, &g_definitionOrigin)) return false;
        g_definitionOriginValid = true;
    }
    uint32_t documentIndex = kMaxOpenDocuments;
    OutputErrorCode error = OutputErrorCode::None;
    if (!WorkspaceControllerOpenDocumentAtLocation(&g_controller, g_controller.model.project.projectId,
                                                   candidate.relativePath, candidate.symbol.symbol.location.line,
                                                   candidate.symbol.symbol.location.column, &documentIndex, &error)) {
        definitionSetStatus(g_relationshipNavigationToDeclaration ? "Declaration not found in the active project." : "Definition not found in the active project.");
        markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER relationship_activation=FAIL", OutputErrorName(error));
        return false;
    }
    Document* document = WorkspaceControllerActiveDocument(&g_controller);
    if (!document) return false;
    bool stale = false;
    const bool selected = selectDefinitionIdentifier(document, candidate, &stale);
    if (!selected) WorkspaceControllerSetCaretPosition(&g_controller, documentIndex,
                                                       candidate.symbol.symbol.location.line,
                                                       candidate.symbol.symbol.location.column, nullptr, &error);
    NavigationHistoryPush(&g_navigationHistory, g_definitionOrigin);
    g_definitionOriginValid = false;
    g_definitionPickerOpen = false;
    g_editorFocused = true;
    g_outputFocused = false;
    keepCaretVisible(document);
    if (stale) {
        definitionSetStatus(g_relationshipNavigationToDeclaration ? "Declaration location may be stale." : "Definition location may be stale.");
        logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER relationship_activation=STALE");
    } else if (g_definitionResolution.declarationsOnly) {
        definitionSetStatus("Definition not found; showing declarations.");
    } else {
        definitionSetStatus(g_relationshipNavigationToDeclaration ? "Declaration found." : "");
        copyText(g_textScratch, sizeof(g_textScratch), g_relationshipNavigationToDeclaration ?
                 "GUIDEXOS_DEVELOPER_STUDIO_MARKER relationship_activate=PASS path=" :
                 "GUIDEXOS_DEVELOPER_STUDIO_MARKER goto_definition_activate=PASS path=");
        appendText(g_textScratch, sizeof(g_textScratch), candidate.relativePath);
        appendText(g_textScratch, sizeof(g_textScratch), " line=");
        appendUnsigned(g_textScratch, sizeof(g_textScratch), candidate.symbol.symbol.location.line);
        appendText(g_textScratch, sizeof(g_textScratch), " column=");
        appendUnsigned(g_textScratch, sizeof(g_textScratch), candidate.symbol.symbol.location.column);
        logMarker(ctx, g_textScratch);
    }
    return true;
}

static const ProjectSymbol* relationshipProjectSymbolAt(uint64_t symbolId, uint32_t* index) {
    for (uint32_t i = 0; i < SymbolDatabaseProjectSymbolCount(&g_symbolDatabase); ++i) {
        const ProjectSymbol* symbol = SymbolDatabaseProjectSymbolAt(&g_symbolDatabase, i);
        if (!symbol) continue;
        const char* path = SymbolDatabaseDocumentPath(&g_symbolDatabase, symbol->documentIndex);
        if (SymbolRelationshipSymbolId(*symbol, path) == symbolId) {
            if (index) *index = i;
            return symbol;
        }
    }
    return nullptr;
}

static int32_t relationshipSymbolUnderCaret(const Document& document) {
    for (uint32_t i = 0; i < SymbolDatabaseProjectSymbolCount(&g_symbolDatabase); ++i) {
        const ProjectSymbol* symbol = SymbolDatabaseProjectSymbolAt(&g_symbolDatabase, i);
        if (!symbol || symbol->symbol.location.documentId != document.documentId) continue;
        const uint32_t start = symbol->symbol.location.identifierOffset;
        const uint32_t length = symbol->symbol.location.identifierLength;
        if (document.buffer.caret >= start && document.buffer.caret <= start + length) return static_cast<int32_t>(i);
        if (document.buffer.selectionActive && document.buffer.selectionAnchor >= start &&
            document.buffer.selectionAnchor <= start + length) return static_cast<int32_t>(i);
    }
    return -1;
}

static bool refreshRelationshipGraph(gx_app_context* ctx) {
    if (!g_controller.model.hasProject) return false;
    const IncludeGraph* includeGraph = IncludeGraphIsCurrent(&g_includeGraph,
        g_controller.model.project.projectId, g_controller.model.projectGeneration) ? &g_includeGraph : nullptr;
    SymbolRelationshipGraph* graph = g_relationshipService.completedGraph;
    if (graph && SymbolRelationshipGraphIsCurrent(graph, g_controller.model.project.projectId,
                                                   g_controller.model.projectGeneration,
                                                   g_symbolDatabase.symbolDatabaseGeneration)) return true;
    uint64_t operationId = 0;
    if (!SymbolRelationshipGraphBuildStart(&g_relationshipService, &g_symbolDatabase, includeGraph,
                                           g_controller.model.project.projectId,
                                           g_controller.model.projectGeneration,
                                           g_controller.model.rootPath,
                                           ctx ? gx_get_ticks_ms(ctx) : 0, &operationId)) {
        definitionSetStatus("Relationship graph is unavailable.");
        return false;
    }
    while (SymbolRelationshipGraphBuildIsActive(&g_relationshipService)) {
        if (!SymbolRelationshipGraphBuildPoll(&g_relationshipService, operationId, 4096,
                                              ctx ? gx_get_ticks_ms(ctx) : 0)) {
            definitionSetStatus("Relationship graph build failed.");
            return false;
        }
    }
    const RelationshipGraphState state = SymbolRelationshipGraphBuildInfo(&g_relationshipService)->state;
    if (state != RelationshipGraphState::Completed) {
        definitionSetStatus("Relationship graph build cancelled.");
        return false;
    }
    copyText(g_textScratch, sizeof(g_textScratch), "GUIDEXOS_DEVELOPER_STUDIO_MARKER relationship_build=PASS groups=");
    appendUnsigned(g_textScratch, sizeof(g_textScratch), g_relationshipService.completedGraph->groupCount);
    appendText(g_textScratch, sizeof(g_textScratch), " relationships=");
    appendUnsigned(g_textScratch, sizeof(g_textScratch), g_relationshipService.completedGraph->relationshipCount);
    if (ctx) logMarker(ctx, g_textScratch);
    return true;
}

static bool buildRelationshipDefinitionCandidate(const SymbolRelationshipEndpoint& endpoint,
                                                 int32_t rankScore, DefinitionCandidate* output) {
    if (!output) return false;
    uint32_t symbolIndex = 0;
    const ProjectSymbol* symbol = relationshipProjectSymbolAt(endpoint.symbolId, &symbolIndex);
    if (!symbol) return false;
    *output = DefinitionCandidate();
    output->candidateId = endpoint.symbolId;
    output->symbol = *symbol;
    copyText(output->relativePath, sizeof(output->relativePath), endpoint.relativePath);
    output->rankScore = rankScore;
    output->isDefinition = endpoint.declarationRole == SymbolDeclarationRole::Definition;
    output->isDeclaration = endpoint.declarationRole == SymbolDeclarationRole::Declaration;
    output->isForwardDeclaration = endpoint.declarationRole == SymbolDeclarationRole::ForwardDeclaration;
    output->stale = endpoint.documentGeneration == 0;
    output->reasonFlags = DefinitionReasonExactQualifiedName | DefinitionReasonMatchingKind;
    return true;
}

static bool startRelationshipNavigation(gx_app_context* ctx, bool toDeclaration) {
    Document* document = WorkspaceControllerActiveDocument(&g_controller);
    if (!document || !g_controller.model.hasProject) return false;
    if (g_controller.model.activeDocument < kMaxOpenDocuments)
        WorkspaceControllerUpdateDocumentSymbols(&g_controller, g_controller.model.activeDocument);
    const int32_t symbolIndex = relationshipSymbolUnderCaret(*document);
    if (symbolIndex < 0 || !refreshRelationshipGraph(ctx)) return false;
    const ProjectSymbol* current = SymbolDatabaseProjectSymbolAt(&g_symbolDatabase, static_cast<uint32_t>(symbolIndex));
    if (!current) return false;
    const uint64_t symbolId = SymbolRelationshipSymbolId(*current,
        SymbolDatabaseDocumentPath(&g_symbolDatabase, current->documentIndex));
    if ((!toDeclaration && current->symbol.declarationRole == SymbolDeclarationRole::Definition) ||
        (toDeclaration && (current->symbol.declarationRole == SymbolDeclarationRole::Declaration ||
                           current->symbol.declarationRole == SymbolDeclarationRole::ForwardDeclaration))) {
        g_relationshipNavigationToDeclaration = toDeclaration;
        definitionSetStatus(toDeclaration ? "Already at declaration." : "Already at definition.");
        if (ctx) logMarker(ctx, toDeclaration ?
            "GUIDEXOS_DEVELOPER_STUDIO_MARKER relationship_resolve_declaration=ALREADY" :
            "GUIDEXOS_DEVELOPER_STUDIO_MARKER relationship_resolve_definition=ALREADY");
        return true;
    }
    static SymbolRelationship relations[kDefinitionMaxCandidates] = {};
    const SymbolRelationshipGraph* graph = g_relationshipService.completedGraph;
    const uint32_t total = toDeclaration ?
        SymbolRelationshipGraphFindDeclarations(graph, symbolId, relations, kDefinitionMaxCandidates) :
        SymbolRelationshipGraphFindDefinitions(graph, symbolId, relations, kDefinitionMaxCandidates);
    if (total == 0) {
        if (toDeclaration) definitionSetStatus("Declaration not found in active project.");
        return toDeclaration;
    }
    g_relationshipNavigationToDeclaration = toDeclaration;
    const uint32_t retained = total < kDefinitionMaxCandidates ? total : kDefinitionMaxCandidates;
    g_definitionResolution = {};
    g_definitionResolution.queryId = g_nextDefinitionQueryId++;
    g_definitionResolution.candidates = g_definitionCandidates;
    g_definitionResolution.candidateCount = 0;
    g_definitionResolution.visibleCandidateCount = 0;
    g_definitionResolution.truncated = total > retained;
    g_definitionResolution.kind = total == 1 ? DefinitionResolutionKind::Direct : DefinitionResolutionKind::Multiple;
    for (uint32_t i = 0; i < retained; ++i) {
        const SymbolRelationship& relation = relations[i];
        const SymbolRelationshipEndpoint& endpoint = relation.source.symbolId == symbolId ? relation.target : relation.source;
        if (!buildRelationshipDefinitionCandidate(endpoint, relation.rankScore, &g_definitionCandidates[g_definitionResolution.candidateCount])) continue;
        ++g_definitionResolution.candidateCount;
    }
    g_definitionResolution.visibleCandidateCount = g_definitionResolution.candidateCount;
    if (g_definitionResolution.candidateCount == 0) return toDeclaration;
    char relativePath[kMaxPathBytes] = {};
    if (!copyProjectRelativePath(g_controller.model, document->path, relativePath, sizeof(relativePath)) ||
        !BuildDefinitionQuery(*document, g_controller.model.project.projectId, g_controller.model.projectGeneration,
                              g_controller.model.rootPath, relativePath, g_definitionResolution.queryId, &g_definitionQuery)) return false;
    if (g_definitionResolution.kind == DefinitionResolutionKind::Direct &&
        relations[0].confidence != SymbolRelationshipConfidence::Ambiguous) {
        if (!captureNavigationLocation(*document, &g_definitionOrigin)) return false;
        g_definitionOriginValid = true;
        activateDefinitionCandidate(ctx, 0);
        return true;
    }
    definitionSetStatus(toDeclaration ? "Choose a declaration candidate." : "Choose a definition candidate.");
    if (!captureNavigationLocation(*document, &g_definitionOrigin)) return false;
    g_definitionOriginValid = true;
    openDefinitionPicker(ctx);
    return true;
}

static uint32_t captureOwnershipInventory() {
    uint32_t count = 0;
    if (!g_controller.model.hasProject) return 0;
    for (uint32_t i = 0; i < SymbolDatabaseDocumentCount(&g_symbolDatabase) && count < kStudioOwnershipFileCapacity; ++i) {
        const SymbolDocument* indexed = SymbolDatabaseDocumentAt(&g_symbolDatabase, i);
        const char* absolute = indexed ? SymbolDatabaseDocumentPath(&g_symbolDatabase, i) : nullptr;
        if (!indexed || !absolute) continue;
        char relative[kOwnershipMaxPathBytes + 1] = {};
        if (!copyProjectRelativePath(g_controller.model, absolute, relative, sizeof(relative))) continue;
        ProjectCodeFileKind kind = ProjectCodeFileKind::Unknown;
        if (!OwnershipClassifyPath(relative, &kind)) continue;
        bool duplicate = false;
        for (uint32_t j = 0; j < count; ++j) if (searchTextEqual(g_ownershipInventory[j].relativePath, relative)) { duplicate = true; break; }
        if (duplicate) continue;
        OwnershipFileRecord& record = g_ownershipInventory[count++];
        record = {};
        record.fileId = OwnershipHashText(relative);
        copyText(record.relativePath, sizeof(record.relativePath), relative);
        record.fileKind = kind;
        record.documentId = indexed->documentId;
        record.documentGeneration = indexed->generation;
        record.open = false;
        record.dirty = indexed->dirty;
        for (uint32_t d = 0; d < kMaxOpenDocuments; ++d) {
            const Document& document = g_controller.model.documents[d];
            char openRelative[kOwnershipMaxPathBytes + 1] = {};
            if (!document.used || !copyProjectRelativePath(g_controller.model, document.path, openRelative, sizeof(openRelative)) ||
                !searchTextEqual(openRelative, relative)) continue;
            record.open = true;
            record.dirty = document.buffer.dirty;
            record.documentId = document.documentId;
            record.documentGeneration = document.buffer.generation;
            break;
        }
    }
    for (uint32_t d = 0; d < kMaxOpenDocuments && count < kStudioOwnershipFileCapacity; ++d) {
        const Document& document = g_controller.model.documents[d];
        if (!document.used) continue;
        char relative[kOwnershipMaxPathBytes + 1] = {};
        if (!copyProjectRelativePath(g_controller.model, document.path, relative, sizeof(relative))) continue;
        bool duplicate = false;
        for (uint32_t j = 0; j < count; ++j) if (searchTextEqual(g_ownershipInventory[j].relativePath, relative)) { duplicate = true; break; }
        if (duplicate) continue;
        ProjectCodeFileKind kind = ProjectCodeFileKind::Unknown;
        if (!OwnershipClassifyPath(relative, &kind)) continue;
        OwnershipFileRecord& record = g_ownershipInventory[count++];
        record = {};
        record.fileId = OwnershipHashText(relative);
        copyText(record.relativePath, sizeof(record.relativePath), relative);
        record.fileKind = kind;
        record.documentId = document.documentId;
        record.documentGeneration = document.buffer.generation;
        record.open = true;
        record.dirty = document.buffer.dirty;
    }
    return count;
}

static const FileOwnershipGraph* currentOwnershipGraph() {
    return g_ownershipService.completedGraph;
}

static bool ownershipGraphCurrent() {
    const FileOwnershipGraph* graph = currentOwnershipGraph();
    const bool includeCurrent = IncludeGraphIsCurrent(&g_includeGraph,
        g_controller.model.hasProject ? g_controller.model.project.projectId : "",
        g_controller.model.projectGeneration);
    const bool relationshipCurrent = SymbolRelationshipGraphIsCurrent(
        g_relationshipService.completedGraph,
        g_controller.model.hasProject ? g_controller.model.project.projectId : "",
        g_controller.model.projectGeneration, g_symbolDatabase.symbolDatabaseGeneration);
    const uint64_t includeGeneration = includeCurrent ? g_includeGraph.graphGeneration : 0;
    const uint64_t relationshipGeneration = relationshipCurrent ? g_relationshipService.completedGraph->graphId : 0;
    return graph && OwnershipGraphIsCurrent(graph,
        g_controller.model.hasProject ? g_controller.model.project.projectId : "",
        g_controller.model.projectGeneration, g_symbolDatabase.symbolDatabaseGeneration,
        includeGeneration, relationshipGeneration);
}

static bool startOwnershipBuild(gx_app_context* ctx) {
    if (!g_controller.model.open || !g_controller.model.hasProject) {
        copyText(g_ownershipStatus, sizeof(g_ownershipStatus), "Open a project before using File Ownership.");
        if (ctx) markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER ownership_build=FAIL", "OWNERSHIP_NO_PROJECT");
        return false;
    }
    const bool includeCurrent = IncludeGraphIsCurrent(&g_includeGraph,
        g_controller.model.project.projectId, g_controller.model.projectGeneration);
    if (!SymbolRelationshipGraphIsCurrent(g_relationshipService.completedGraph,
            g_controller.model.project.projectId, g_controller.model.projectGeneration,
            g_symbolDatabase.symbolDatabaseGeneration)) {
        refreshRelationshipGraph(ctx);
    }
    const bool relationshipCurrent = SymbolRelationshipGraphIsCurrent(
        g_relationshipService.completedGraph, g_controller.model.project.projectId,
        g_controller.model.projectGeneration, g_symbolDatabase.symbolDatabaseGeneration);
    const uint32_t fileCount = captureOwnershipInventory();
    OwnershipBuildRequest request = {};
    copyText(request.projectIdText, sizeof(request.projectIdText), g_controller.model.project.projectId);
    copyText(request.projectRoot, sizeof(request.projectRoot), g_controller.model.rootPath);
    request.projectId = OwnershipHashText(request.projectIdText);
    request.projectGeneration = g_controller.model.projectGeneration;
    request.symbolGeneration = g_symbolDatabase.symbolDatabaseGeneration;
    request.files = g_ownershipInventory;
    request.fileCount = fileCount;
    request.inventoryTruncated = SymbolDatabaseIsTruncated(&g_symbolDatabase);
    request.includeGraph = includeCurrent ? &g_includeGraph : nullptr;
    request.relationshipGraph = relationshipCurrent ? g_relationshipService.completedGraph : nullptr;
    request.symbolDatabase = &g_symbolDatabase;
    request.scratch = &g_ownershipScratch;
    uint64_t operationId = 0;
    if (!OwnershipGraphBuildStart(&g_ownershipService, request, ctx ? gx_get_ticks_ms(ctx) : 0, &operationId)) {
        copyText(g_ownershipStatus, sizeof(g_ownershipStatus), "File Ownership build could not start.");
        if (ctx) markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER ownership_build=FAIL", "OWNERSHIP_INTERNAL");
        return false;
    }
    g_ownershipOperationId = operationId;
    g_ownershipTerminalReported = false;
    copyText(g_ownershipStatus, sizeof(g_ownershipStatus), "Building File Ownership from indexed records...");
    g_ownershipPanelOpen = true;
    g_ownershipPickerOpen = false;
    g_ownershipSelectedCandidate = 0;
    g_ownershipScroll = 0;
    g_editorFocused = false;
    if (ctx) {
        logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER ownership_build_begin=PASS");
        copyText(g_textScratch, sizeof(g_textScratch), "GUIDEXOS_DEVELOPER_STUDIO_MARKER ownership_files=PASS total=");
        appendUnsigned(g_textScratch, sizeof(g_textScratch), fileCount);
        logMarker(ctx, g_textScratch);
    }
    return true;
}

static void pollOwnership(gx_app_context* ctx) {
    if (g_ownershipOperationId == 0) return;
    if (OwnershipGraphBuildIsActive(&g_ownershipService) &&
        (!g_controller.model.open || !g_controller.model.hasProject ||
         g_ownershipService.operation.request.projectGeneration != g_controller.model.projectGeneration)) {
        OwnershipGraphBuildCancel(&g_ownershipService, g_ownershipOperationId);
    }
    OwnershipGraphBuildPoll(&g_ownershipService, g_ownershipOperationId, 8, ctx ? gx_get_ticks_ms(ctx) : 0);
    const auto* operation = OwnershipGraphBuildInfo(&g_ownershipService);
    if (!operation || OwnershipGraphBuildIsActive(&g_ownershipService) || g_ownershipTerminalReported) return;
    g_ownershipTerminalReported = true;
    if (operation->state == OwnershipBuildState::Completed) {
        const FileOwnershipGraph* graph = currentOwnershipGraph();
        copyText(g_ownershipStatus, sizeof(g_ownershipStatus), graph && graph->truncated ?
                 "File Ownership complete (truncated)." : "File Ownership complete.");
        if (ctx && graph) {
            copyText(g_textScratch, sizeof(g_textScratch), "GUIDEXOS_DEVELOPER_STUDIO_MARKER ownership_candidates=PASS total=");
            appendUnsigned(g_textScratch, sizeof(g_textScratch), graph->candidateCount);
            logMarker(ctx, g_textScratch);
            copyText(g_textScratch, sizeof(g_textScratch), "GUIDEXOS_DEVELOPER_STUDIO_MARKER ownership_groups=PASS groups=");
            appendUnsigned(g_textScratch, sizeof(g_textScratch), graph->groupCount);
            logMarker(ctx, g_textScratch);
            copyText(g_textScratch, sizeof(g_textScratch), graph->truncated ?
                     "GUIDEXOS_DEVELOPER_STUDIO_MARKER ownership_build=TRUNCATED reason=OWNERSHIP_RESULTS_TRUNCATED" :
                     "GUIDEXOS_DEVELOPER_STUDIO_MARKER ownership_build=PASS groups=");
            if (!graph->truncated) {
                appendUnsigned(g_textScratch, sizeof(g_textScratch), graph->groupCount);
                appendText(g_textScratch, sizeof(g_textScratch), " pairs=");
                appendUnsigned(g_textScratch, sizeof(g_textScratch), graph->candidateCount);
            }
            logMarker(ctx, g_textScratch);
        }
    } else if (operation->state == OwnershipBuildState::Cancelled) {
        copyText(g_ownershipStatus, sizeof(g_ownershipStatus), "File Ownership build cancelled; last completed graph retained.");
        if (ctx) logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER ownership_build=CANCELLED");
    } else {
        copyText(g_ownershipStatus, sizeof(g_ownershipStatus), "File Ownership build failed.");
        if (ctx) markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER ownership_build=FAIL", "OWNERSHIP_INTERNAL");
    }
}

static bool activeOwnershipPath(char* output, uint32_t capacity) {
    Document* document = WorkspaceControllerActiveDocument(&g_controller);
    return document && copyProjectRelativePath(g_controller.model, document->path, output, capacity);
}

static uint32_t ownershipRelationshipDestination(const char* targetPath, uint64_t symbolId,
                                                 uint32_t* line, uint32_t* column) {
    const SymbolRelationshipGraph* graph = g_relationshipService.completedGraph;
    if (!graph || !targetPath || symbolId == 0) return 0;
    for (uint32_t i = 0; i < graph->relationshipCount; ++i) {
        const SymbolRelationship* relationship = SymbolRelationshipGraphRelationshipAt(graph, i);
        if (!relationship || relationship->stale) continue;
        const SymbolRelationshipEndpoint* endpoint = nullptr;
        if (relationship->source.symbolId == symbolId && searchTextEqual(relationship->target.relativePath, targetPath)) endpoint = &relationship->target;
        else if (relationship->target.symbolId == symbolId && searchTextEqual(relationship->source.relativePath, targetPath)) endpoint = &relationship->source;
        if (!endpoint) continue;
        if (line) *line = endpoint->line;
        if (column) *column = endpoint->column;
        return 1;
    }
    return 0;
}

static bool activateOwnershipCandidate(gx_app_context* ctx, uint32_t index) {
    const FileOwnershipGraph* graph = currentOwnershipGraph();
    if (!graph || !ownershipGraphCurrent() || index >= graph->candidateCount) {
        copyText(g_ownershipStatus, sizeof(g_ownershipStatus), "File Ownership result is stale.");
        if (ctx) logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER ownership_activate=STALE");
        return false;
    }
    const FileCounterpartCandidate& candidate = graph->candidates[index];
    char currentPath[kOwnershipMaxPathBytes + 1] = {};
    if (!activeOwnershipPath(currentPath, sizeof(currentPath))) return false;
    const char* destinationPath = nullptr;
    if (searchTextEqual(candidate.source.relativePath, currentPath)) destinationPath = candidate.target.relativePath;
    else if (searchTextEqual(candidate.target.relativePath, currentPath)) destinationPath = candidate.source.relativePath;
    if (!destinationPath || destinationPath[0] == '\0' || PathContainsTraversal(destinationPath) || destinationPath[0] == '/' || destinationPath[0] == '\\') {
        copyText(g_ownershipStatus, sizeof(g_ownershipStatus), "Counterpart path is outside the project.");
        if (ctx) markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER ownership_activate=FAIL", "OWNERSHIP_TARGET_OUTSIDE_PROJECT");
        return false;
    }
    Document* origin = WorkspaceControllerActiveDocument(&g_controller);
    NavigationLocation originLocation = {};
    if (!origin || !captureNavigationLocation(*origin, &originLocation)) return false;
    uint32_t line = 1;
    uint32_t column = 1;
    const int32_t symbolIndex = relationshipSymbolUnderCaret(*origin);
    if (symbolIndex >= 0) {
        const ProjectSymbol* symbol = SymbolDatabaseProjectSymbolAt(&g_symbolDatabase, static_cast<uint32_t>(symbolIndex));
        if (symbol) ownershipRelationshipDestination(destinationPath,
            SymbolRelationshipSymbolId(*symbol, SymbolDatabaseDocumentPath(&g_symbolDatabase, symbol->documentIndex)), &line, &column);
    }
    uint32_t documentIndex = kMaxOpenDocuments;
    OutputErrorCode error = OutputErrorCode::None;
    if (!WorkspaceControllerOpenDocumentAtLocation(&g_controller, g_controller.model.project.projectId,
                                                   destinationPath, line, column, &documentIndex, &error)) {
        copyText(g_ownershipStatus, sizeof(g_ownershipStatus), "Counterpart file could not be opened.");
        if (ctx) markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER ownership_activate=FAIL", OutputErrorName(error));
        return false;
    }
    NavigationHistoryPush(&g_navigationHistory, originLocation);
    g_ownershipPanelOpen = false;
    g_ownershipPickerOpen = false;
    g_editorFocused = true;
    g_outputFocused = false;
    copyText(g_ownershipStatus, sizeof(g_ownershipStatus), "Switched Header / Source.");
    if (ctx) {
        copyText(g_textScratch, sizeof(g_textScratch), "GUIDEXOS_DEVELOPER_STUDIO_MARKER ownership_activate=PASS path=");
        appendText(g_textScratch, sizeof(g_textScratch), destinationPath);
        logMarker(ctx, g_textScratch);
    }
    return true;
}

static void openFileOwnership(gx_app_context* ctx) {
    if (!g_controller.model.hasProject) { startOwnershipBuild(ctx); return; }
    if (!ownershipGraphCurrent()) { startOwnershipBuild(ctx); return; }
    char path[kOwnershipMaxPathBytes + 1] = {};
    if (!activeOwnershipPath(path, sizeof(path))) return;
    g_ownershipResolution = {};
    g_ownershipResolution.candidateIndices = g_ownershipCandidateIndices;
    g_ownershipResolution.candidateCapacity = kOwnershipMaxVisiblePickerCandidates;
    OwnershipResolveFile(currentOwnershipGraph(), path, &g_ownershipResolution);
    g_ownershipSelectedCandidate = 0;
    g_ownershipScroll = 0;
    g_ownershipPanelOpen = true;
    g_ownershipPickerOpen = g_ownershipResolution.kind == OwnershipResolutionKind::Multiple;
    g_editorFocused = false;
}

static bool switchHeaderSource(gx_app_context* ctx) {
    if (!g_controller.model.hasProject) { copyText(g_ownershipStatus, sizeof(g_ownershipStatus), "Open a project before switching Header / Source."); return false; }
    if (!ownershipGraphCurrent()) { startOwnershipBuild(ctx); return true; }
    char path[kOwnershipMaxPathBytes + 1] = {};
    if (!activeOwnershipPath(path, sizeof(path))) return false;
    g_ownershipResolution = {};
    g_ownershipResolution.candidateIndices = g_ownershipCandidateIndices;
    g_ownershipResolution.candidateCapacity = kOwnershipMaxVisiblePickerCandidates;
    OwnershipResolveFile(currentOwnershipGraph(), path, &g_ownershipResolution);
    if (g_ownershipResolution.kind == OwnershipResolutionKind::Direct && g_ownershipResolution.candidateCount == 1)
        return activateOwnershipCandidate(ctx, g_ownershipCandidateIndices[0]);
    g_ownershipPanelOpen = true;
    g_ownershipPickerOpen = g_ownershipResolution.kind == OwnershipResolutionKind::Multiple;
    g_ownershipSelectedCandidate = 0;
    g_ownershipScroll = 0;
    g_editorFocused = false;
    if (g_ownershipResolution.kind == OwnershipResolutionKind::HeaderOnly) copyText(g_ownershipStatus, sizeof(g_ownershipStatus), "Header-only file; no external source counterpart.");
    else if (g_ownershipResolution.kind == OwnershipResolutionKind::SourceOnly) copyText(g_ownershipStatus, sizeof(g_ownershipStatus), "Source-only file; no credible header counterpart.");
    else if (g_ownershipResolution.candidateCount == 0) copyText(g_ownershipStatus, sizeof(g_ownershipStatus), "No Header / Source counterpart found.");
    else copyText(g_ownershipStatus, sizeof(g_ownershipStatus), "Choose a Header / Source counterpart.");
    return true;
}

static bool handleOwnershipKey(gx_app_context* ctx, int keyCode, int action) {
    if (!g_ownershipPanelOpen || action != GX_KEY_ACTION_DOWN) return false;
    if (keyCode == 27) { g_ownershipPanelOpen = false; g_ownershipPickerOpen = false; g_editorFocused = true; return true; }
    if (keyCode == 82 || keyCode == 114) { startOwnershipBuild(ctx); return true; }
    if (!g_ownershipPickerOpen) return true;
    const uint32_t count = g_ownershipResolution.visibleCandidateCount;
    if (keyCode == GX_KEY_UP && g_ownershipSelectedCandidate > 0) --g_ownershipSelectedCandidate;
    else if (keyCode == GX_KEY_DOWN && g_ownershipSelectedCandidate + 1 < count) ++g_ownershipSelectedCandidate;
    else if (keyCode == 33) g_ownershipSelectedCandidate = g_ownershipSelectedCandidate > 15 ? g_ownershipSelectedCandidate - 15 : 0;
    else if (keyCode == 34) g_ownershipSelectedCandidate = g_ownershipSelectedCandidate + 15 < count ? g_ownershipSelectedCandidate + 15 : (count ? count - 1 : 0);
    else if (keyCode == 36) g_ownershipSelectedCandidate = 0;
    else if (keyCode == 35) g_ownershipSelectedCandidate = count ? count - 1 : 0;
    else if (keyCode == 13 && count > 0) { activateOwnershipCandidate(ctx, g_ownershipCandidateIndices[g_ownershipSelectedCandidate]); return true; }
    if (g_ownershipSelectedCandidate < g_ownershipScroll) g_ownershipScroll = g_ownershipSelectedCandidate;
    if (g_ownershipSelectedCandidate >= g_ownershipScroll + 15u) g_ownershipScroll = g_ownershipSelectedCandidate - 14u;
    return true;
}

static void drawOwnershipPanel(gx_app_context* ctx) {
    if (!g_ownershipPanelOpen) return;
    drawPanel(ctx, { 128, 58, 704, 560 }, 0x2A3852u);
    drawText(ctx, 150, 86, g_ownershipPickerOpen ? "Choose Header / Source Counterpart" : "File Ownership");
    char path[kOwnershipMaxPathBytes + 1] = {};
    Document* document = WorkspaceControllerActiveDocument(&g_controller);
    if (document && copyProjectRelativePath(g_controller.model, document->path, path, sizeof(path))) {
        drawText(ctx, 150, 112, "Current file:");
        drawText(ctx, 270, 112, path);
        ProjectCodeFileKind kind = ProjectCodeFileKind::Unknown;
        OwnershipClassifyPath(path, &kind);
        drawText(ctx, 150, 136, "Classification:");
        drawText(ctx, 270, 136, ProjectCodeFileKindName(kind));
    }
    if (OwnershipGraphBuildIsActive(&g_ownershipService)) {
        drawText(ctx, 150, 164, OwnershipBuildStateName(g_ownershipService.operation.state));
        drawText(ctx, 150, 188, "Indexed records are authoritative; source content is not rescanned.");
    } else if (g_ownershipStatus[0] != '\0') drawText(ctx, 150, 164, g_ownershipStatus);
    if (g_ownershipResolution.candidateCount == 0) {
        if (g_ownershipStatus[0] == '\0') drawText(ctx, 150, 220, "No unique primary counterpart.");
        drawText(ctx, 150, 588, "R Refresh   Esc Close");
        return;
    }
    const uint32_t first = g_ownershipScroll;
    const uint32_t end = first + 15u < g_ownershipResolution.visibleCandidateCount ? first + 15u : g_ownershipResolution.visibleCandidateCount;
    for (uint32_t row = first; row < end; ++row) {
        const uint32_t candidateIndex = g_ownershipCandidateIndices[row];
        const FileCounterpartCandidate* candidate = OwnershipGraphCandidateAt(currentOwnershipGraph(), candidateIndex);
        if (!candidate) continue;
        const int y = 224 + static_cast<int>(row - first) * 24;
        if (g_ownershipPickerOpen && row == g_ownershipSelectedCandidate) drawPanel(ctx, { 142, y - 16, 676, 22 }, 0x405775u);
        const char* displayPath = searchTextEqual(candidate->source.relativePath, path) ? candidate->target.relativePath : candidate->source.relativePath;
        drawText(ctx, 150, y, displayPath);
        drawText(ctx, 650, y, CounterpartConfidenceName(candidate->confidence));
        if (candidate->evidenceCount > 0) drawText(ctx, 150, y + 14, candidate->evidence[0].detail);
    }
    if (g_ownershipResolution.truncated) drawText(ctx, 150, 582, "Results truncated; showing strongest deterministic candidates.");
    else drawText(ctx, 150, 582, g_ownershipPickerOpen ? "Up/Down Page Home/End Enter Select   Esc Close" : "Esc Close");
}

static void closeDefinitionPicker() {
    g_definitionPickerOpen = false;
    g_definitionOriginValid = false;
}

static void ensureDefinitionSelectionVisible() {
    if (g_definitionResolution.candidateCount == 0) { g_definitionSelected = 0; g_definitionScroll = 0; return; }
    if (g_definitionSelected >= g_definitionResolution.candidateCount) g_definitionSelected = g_definitionResolution.candidateCount - 1;
    if (g_definitionSelected < g_definitionScroll) g_definitionScroll = g_definitionSelected;
    const uint32_t rows = 18;
    if (g_definitionSelected >= g_definitionScroll + rows) g_definitionScroll = g_definitionSelected - rows + 1;
}

static void openDefinitionPicker(gx_app_context* ctx) {
    g_definitionPickerOpen = true;
    g_definitionSelected = 0;
    g_definitionScroll = 0;
    ensureDefinitionSelectionVisible();
    g_editorFocused = false;
    copyText(g_textScratch, sizeof(g_textScratch),
             g_definitionResolution.truncated
                 ? "GUIDEXOS_DEVELOPER_STUDIO_MARKER goto_definition_resolution=TRUNCATED candidates="
                 : "GUIDEXOS_DEVELOPER_STUDIO_MARKER goto_definition_resolution=MULTIPLE candidates=");
    appendUnsigned(g_textScratch, sizeof(g_textScratch), g_definitionResolution.candidateCount);
    logMarker(ctx, g_textScratch);
}

static bool handleDefinitionPickerKey(gx_app_context* ctx, int keyCode, int action) {
    if (!g_definitionPickerOpen || action != GX_KEY_ACTION_DOWN) return false;
    if (keyCode == 27) { closeDefinitionPicker(); return true; }
    if (keyCode == GX_KEY_UP) { if (g_definitionSelected > 0) --g_definitionSelected; ensureDefinitionSelectionVisible(); return true; }
    if (keyCode == GX_KEY_DOWN) { if (g_definitionSelected + 1 < g_definitionResolution.candidateCount) ++g_definitionSelected; ensureDefinitionSelectionVisible(); return true; }
    if (keyCode == 33) {
        g_definitionSelected = g_definitionSelected > 18 ? g_definitionSelected - 18 : 0;
        ensureDefinitionSelectionVisible(); return true;
    }
    if (keyCode == 34) {
        const uint32_t last = g_definitionResolution.candidateCount == 0 ? 0 : g_definitionResolution.candidateCount - 1;
        g_definitionSelected = g_definitionSelected + 18 < last ? g_definitionSelected + 18 : last;
        ensureDefinitionSelectionVisible(); return true;
    }
    if (keyCode == 36) { g_definitionSelected = 0; ensureDefinitionSelectionVisible(); return true; }
    if (keyCode == 35) { g_definitionSelected = g_definitionResolution.candidateCount == 0 ? 0 : g_definitionResolution.candidateCount - 1; ensureDefinitionSelectionVisible(); return true; }
    if (keyCode == 13 && g_definitionResolution.candidateCount > 0) { activateDefinitionCandidate(ctx, g_definitionSelected); return true; }
    return true;
}

static void startGoToDefinition(gx_app_context* ctx) {
    Document* document = WorkspaceControllerActiveDocument(&g_controller);
    if (!g_controller.model.hasProject) { definitionSetStatus("No active project."); markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER goto_definition_activate=FAIL", "GOTO_DEFINITION_NO_PROJECT"); return; }
    if (!document) { definitionSetStatus("No active document."); markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER goto_definition_activate=FAIL", "GOTO_DEFINITION_NO_DOCUMENT"); return; }
    DefinitionIdentifier identifier = {};
    if (!ExtractDefinitionIdentifier(document->buffer.data, document->buffer.length, document->buffer.caret,
                                     document->buffer.selectionActive, document->buffer.selectionAnchor,
                                     document->buffer.caret, &identifier)) {
        definitionSetStatus(identifier.tooLong ? "Identifier is too long." : "No symbol under caret.");
        markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER goto_definition_activate=FAIL",
                      identifier.tooLong ? "GOTO_DEFINITION_IDENTIFIER_TOO_LONG" : "GOTO_DEFINITION_NO_IDENTIFIER");
        return;
    }
    if (g_controller.model.activeDocument < kMaxOpenDocuments)
        WorkspaceControllerUpdateDocumentSymbols(&g_controller, g_controller.model.activeDocument);
    if (startRelationshipNavigation(ctx, false)) return;
    char relativePath[kMaxPathBytes] = {};
    if (!copyProjectRelativePath(g_controller.model, document->path, relativePath, sizeof(relativePath))) {
        definitionSetStatus("Active document path is invalid.");
        markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER goto_definition_activate=FAIL", "GOTO_DEFINITION_PATH_INVALID");
        return;
    }
    if (!BuildDefinitionQuery(*document, g_controller.model.project.projectId, g_controller.model.projectGeneration,
                              g_controller.model.rootPath, relativePath, g_nextDefinitionQueryId++, &g_definitionQuery)) {
        definitionSetStatus("No symbol under caret.");
        return;
    }
    copyText(g_textScratch, sizeof(g_textScratch), "GUIDEXOS_DEVELOPER_STUDIO_MARKER goto_definition_begin=PASS identifier=");
    appendText(g_textScratch, sizeof(g_textScratch), g_definitionQuery.identifier);
    logMarker(ctx, g_textScratch);
    g_definitionResolution = {};
    if (!ResolveDefinition(&g_symbolDatabase, g_definitionQuery, g_definitionCandidates,
                           kDefinitionMaxCandidates, &g_definitionResolution)) {
        definitionSetStatus("Symbol index is not ready.");
        markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER goto_definition_activate=FAIL", "GOTO_DEFINITION_INDEX_NOT_READY");
        return;
    }
    if (g_definitionResolution.kind == DefinitionResolutionKind::None) {
        definitionSetStatus("Definition not found.");
        logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER goto_definition_resolution=NONE");
        return;
    }
    copyText(g_textScratch, sizeof(g_textScratch), "GUIDEXOS_DEVELOPER_STUDIO_MARKER goto_definition_lookup=PASS candidates=");
    appendUnsigned(g_textScratch, sizeof(g_textScratch), g_definitionResolution.candidateCount);
    logMarker(ctx, g_textScratch);
    if (g_definitionResolution.kind == DefinitionResolutionKind::Direct) {
        const DefinitionCandidate& candidate = g_definitionResolution.candidates[0];
        const uint32_t candidateLength = lengthOf(candidate.symbol.symbol.name, kSymbolMaxNameBytes);
        const bool alreadyAtDefinition = candidate.isDefinition &&
            searchTextEqual(g_definitionQuery.relativePath, candidate.relativePath) &&
            candidate.symbol.symbol.location.documentId == document->documentId &&
            candidate.symbol.symbol.location.generation == document->buffer.generation &&
            document->buffer.caret >= candidate.symbol.symbol.location.identifierOffset &&
            document->buffer.caret <= candidate.symbol.symbol.location.identifierOffset + candidateLength;
        if (alreadyAtDefinition) {
            definitionSetStatus("Already at definition.");
            logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER goto_definition_resolution=DIRECT");
            return;
        }
        if (!captureNavigationLocation(*document, &g_definitionOrigin)) return;
        g_definitionOriginValid = true;
        logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER goto_definition_resolution=DIRECT");
        activateDefinitionCandidate(ctx, 0);
        return;
    }
    if (g_definitionResolution.kind == DefinitionResolutionKind::Stale) definitionSetStatus("Definition location may be stale.");
    else if (g_definitionResolution.declarationsOnly) definitionSetStatus("Definition not found; showing declarations.");
    else if (g_definitionResolution.truncated) definitionSetStatus("Definition candidates truncated.");
    else definitionSetStatus("Choose a definition candidate.");
    if (!captureNavigationLocation(*document, &g_definitionOrigin)) return;
    g_definitionOriginValid = true;
    openDefinitionPicker(ctx);
}

static void projectSearchSetStatus(const char* text) {
    copyText(g_projectSearchStatus, sizeof(g_projectSearchStatus), text ? text : "");
}

static void referencesSetStatus(const char* text) {
    copyText(g_referencesStatus, sizeof(g_referencesStatus), text ? text : "");
}

static void ensureReferenceCandidateVisible() {
    const uint32_t count = g_referenceTargetResolution.visibleCandidateCount;
    if (count == 0) { g_referenceSelectedCandidate = 0; g_referenceCandidateScroll = 0; return; }
    if (g_referenceSelectedCandidate >= count) g_referenceSelectedCandidate = count - 1;
    if (g_referenceSelectedCandidate < g_referenceCandidateScroll)
        g_referenceCandidateScroll = g_referenceSelectedCandidate;
    if (g_referenceSelectedCandidate >= g_referenceCandidateScroll + 18u)
        g_referenceCandidateScroll = g_referenceSelectedCandidate - 17u;
}

static void openReferencePicker(gx_app_context* ctx) {
    g_referencePickerOpen = true;
    g_referenceSelectedCandidate = 0;
    g_referenceCandidateScroll = 0;
    ensureReferenceCandidateVisible();
    g_editorFocused = false;
    copyText(g_textScratch, sizeof(g_textScratch),
             "GUIDEXOS_DEVELOPER_STUDIO_MARKER references_target=MULTIPLE candidates=");
    appendUnsigned(g_textScratch, sizeof(g_textScratch), g_referenceTargetResolution.candidateCount);
    logMarker(ctx, g_textScratch);
}

static void closeReferencePicker() {
    g_referencePickerOpen = false;
    g_referenceCandidateScroll = 0;
}

static void referenceSearchInitializeOptions(ProjectSearchOptions* options) {
    if (!options) return;
    ProjectSearchOptionsInit(options);
    copyText(options->includePattern, sizeof(options->includePattern),
             "*.c;*.h;*.cc;*.cpp;*.cxx;*.hh;*.hpp;*.hxx");
    copyText(options->excludePattern, sizeof(options->excludePattern),
             ".git/*;.vs/*;.idea/*;out/*;build/*;bin/*;obj/*;dist/*;node_modules/*");
}

static bool startReferenceSearchWithTarget(gx_app_context* ctx, const ReferenceTarget& target,
                                           bool lexicalFallback) {
    if (!g_controller.model.open || !g_controller.model.hasProject) {
        referencesSetStatus("Open a project before finding references.");
        markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER references_complete=FAIL",
                      ReferenceSearchErrorName(ReferenceSearchErrorCode::NoProject));
        return false;
    }
    if (!ReferenceSearchIsActive(&g_referenceSearch) && g_referencesOperationId != 0)
        ReferenceSearchRelease(&g_referenceSearch, g_referencesOperationId);
    if (ReferenceSearchIsActive(&g_referenceSearch)) stopReferenceSearch(ctx);
    if (!lexicalFallback) refreshRelationshipGraph(ctx);
    ProjectSearchOptions options = {};
    referenceSearchInitializeOptions(&options);
    ReferenceSearchRequest request = {};
    request.target = target;
    copyText(request.projectId, sizeof(request.projectId), g_controller.model.project.projectId);
    request.projectGeneration = g_controller.model.projectGeneration;
    copyText(request.rootPath, sizeof(request.rootPath), g_controller.model.rootPath);
    request.scanOptions = options;
    request.fileSystem = g_controller.fileSystem;
    request.dirtyDocuments = g_projectSearchSnapshots;
    request.dirtyDocumentCount = captureDirtyProjectSearchDocuments();
    request.symbolDatabase = &g_symbolDatabase;
    request.relationshipGraph = SymbolRelationshipGraphIsCurrent(
        g_relationshipService.completedGraph, g_controller.model.project.projectId,
        g_controller.model.projectGeneration, g_symbolDatabase.symbolDatabaseGeneration)
        ? g_relationshipService.completedGraph : nullptr;
    request.includeDeclarations = true;
    request.includeAmbiguous = true;
    request.lexicalFallback = lexicalFallback;
    ReferenceSearchErrorCode error = ReferenceSearchErrorCode::None;
    uint64_t operationId = 0;
    if (!ReferenceSearchStart(&g_referenceSearch, &request, gx_get_ticks_ms(ctx), &operationId, &error)) {
        g_referencesOperationId = operationId;
        g_referencesTerminalReported = false;
        referencesSetStatus("Reference search failed: ");
        appendText(g_referencesStatus, sizeof(g_referencesStatus), ReferenceSearchErrorName(error));
        markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER references_complete=FAIL",
                      ReferenceSearchErrorName(error));
        return false;
    }
    g_referenceTarget = target;
    g_referencesOperationId = operationId;
    g_referencesProjectGeneration = request.projectGeneration;
    copyText(g_referencesProjectId, sizeof(g_referencesProjectId), request.projectId);
    g_referencesPanelOpen = true;
    g_referencesResultsFocused = false;
    g_referencesSelectedGroup = 0;
    g_referencesSelectedMatch = 0;
    g_referencesScroll = 0;
    g_referencesTerminalReported = false;
    g_editorFocused = false;
    g_outputFocused = false;
    referencesSetStatus(lexicalFallback ?
                        "No indexed symbol was resolved. Showing lexical identifier matches." :
                        "Searching active project...");
    copyText(g_textScratch, sizeof(g_textScratch),
             "GUIDEXOS_DEVELOPER_STUDIO_MARKER references_search_begin=PASS");
    logMarker(ctx, g_textScratch);
    writeStudioOutput(lexicalFallback ? "Find All References started in lexical fallback mode" :
                      "Find All References started");
    return true;
}

static bool activateReferenceTargetCandidate(gx_app_context* ctx, uint32_t index) {
    if (index >= g_referenceTargetResolution.candidateCount) return false;
    if (!g_controller.model.hasProject) return false;
    const DefinitionCandidate& candidate = g_referenceCandidates[index];
    ReferenceTarget target = {};
    if (!ReferenceTargetFromDefinitionCandidate(candidate, g_controller.model.project.projectId,
                                                g_controller.model.projectGeneration, &target)) {
        referencesSetStatus("Selected symbol cannot be used as a reference target.");
        return false;
    }
    Document* origin = WorkspaceControllerActiveDocument(&g_controller);
    if (origin && !g_referenceOriginValid) {
        if (!captureNavigationLocation(*origin, &g_referenceOrigin)) return false;
        g_referenceOriginValid = true;
    }
    closeReferencePicker();
    if (g_renameTargetPickerPending) {
        g_renameTargetPickerPending = false;
        return startRenameSearchWithTarget(ctx, target);
    }
    return startReferenceSearchWithTarget(ctx, target, false);
}

static bool startFindAllReferences(gx_app_context* ctx) {
    Document* document = WorkspaceControllerActiveDocument(&g_controller);
    if (!g_controller.model.hasProject) {
        referencesSetStatus("No active project.");
        markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER references_complete=FAIL",
                      ReferenceSearchErrorName(ReferenceSearchErrorCode::NoProject));
        return false;
    }
    if (!document) {
        referencesSetStatus("No active document.");
        markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER references_complete=FAIL",
                      ReferenceSearchErrorName(ReferenceSearchErrorCode::NoDocument));
        return false;
    }
    DefinitionIdentifier identifier = {};
    if (!ExtractDefinitionIdentifier(document->buffer.data, document->buffer.length, document->buffer.caret,
                                     document->buffer.selectionActive, document->buffer.selectionAnchor,
                                     document->buffer.caret, &identifier)) {
        referencesSetStatus(identifier.tooLong ? "Identifier is too long." : "No identifier under caret.");
        markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER references_complete=FAIL",
                      identifier.tooLong ? ReferenceSearchErrorName(ReferenceSearchErrorCode::IdentifierTooLong) :
                      ReferenceSearchErrorName(ReferenceSearchErrorCode::NoIdentifier));
        return false;
    }
    if (g_controller.model.activeDocument < kMaxOpenDocuments)
        WorkspaceControllerUpdateDocumentSymbols(&g_controller, g_controller.model.activeDocument);
    char relativePath[kMaxPathBytes] = {};
    if (!copyProjectRelativePath(g_controller.model, document->path, relativePath, sizeof(relativePath))) {
        referencesSetStatus("Active document path is invalid.");
        return false;
    }
    if (!BuildDefinitionQuery(*document, g_controller.model.project.projectId,
                              g_controller.model.projectGeneration, g_controller.model.rootPath,
                              relativePath, g_nextReferenceQueryId++, &g_referenceQuery)) {
        referencesSetStatus("No identifier under caret.");
        return false;
    }
    copyText(g_textScratch, sizeof(g_textScratch),
             "GUIDEXOS_DEVELOPER_STUDIO_MARKER references_begin=PASS identifier=");
    appendText(g_textScratch, sizeof(g_textScratch), g_referenceQuery.identifier);
    logMarker(ctx, g_textScratch);
    g_referenceTargetResolution = {};
    if (!ResolveReferenceTarget(&g_symbolDatabase, g_referenceQuery, g_referenceCandidates,
                               kReferenceMaxCandidates, true, &g_referenceTargetResolution)) {
        referencesSetStatus("Symbol index is not ready.");
        markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER references_complete=FAIL",
                      ReferenceSearchErrorName(ReferenceSearchErrorCode::IndexNotReady));
        return false;
    }
    stopProjectSearch(ctx);
    g_projectSearchPanelOpen = false;
    g_findBarOpen = false;
    if (g_referenceTargetResolution.kind == ReferenceTargetResolutionKind::LexicalFallback) {
        if (!ReferenceTargetFromDefinitionQuery(g_referenceQuery, g_controller.model.project.projectId,
                                                g_controller.model.projectGeneration, &g_referenceTarget)) {
            referencesSetStatus("Lexical fallback target is invalid.");
            return false;
        }
        g_referenceOriginValid = captureNavigationLocation(*document, &g_referenceOrigin);
        g_referencePickerOpen = false;
        return startReferenceSearchWithTarget(ctx, g_referenceTarget, true);
    }
    if (g_referenceTargetResolution.kind == ReferenceTargetResolutionKind::Direct) {
        const DefinitionCandidate& candidate = g_referenceCandidates[0];
        if (!ReferenceTargetFromDefinitionCandidate(candidate, g_controller.model.project.projectId,
                                                    g_controller.model.projectGeneration, &g_referenceTarget))
            return false;
        g_referenceOriginValid = captureNavigationLocation(*document, &g_referenceOrigin);
        return startReferenceSearchWithTarget(ctx, g_referenceTarget, false);
    }
    if (g_referenceTargetResolution.kind == ReferenceTargetResolutionKind::Multiple ||
        g_referenceTargetResolution.kind == ReferenceTargetResolutionKind::Stale) {
        referencesSetStatus("Choose Symbol for Find All References");
        g_referencesPanelOpen = true;
        openReferencePicker(ctx);
        return true;
    }
    referencesSetStatus("No indexed symbol was resolved.");
    return false;
}

static bool startRenameSearchWithTarget(gx_app_context* ctx, const ReferenceTarget& target) {
    g_renameSearchPending = true;
    if (!startReferenceSearchWithTarget(ctx, target, false)) {
        g_renameSearchPending = false;
        return false;
    }
    referencesSetStatus("Rename Symbol: scanning project references...");
    logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER rename_references_begin=PASS");
    return true;
}

static bool startRenameSymbol(gx_app_context* ctx) {
    Document* document = WorkspaceControllerActiveDocument(&g_controller);
    if (!g_controller.model.open || !g_controller.model.hasProject) {
        referencesSetStatus("Open a project before using Rename Symbol.");
        markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER rename_apply=FAIL", RenameErrorName(RenameErrorCode::NoProject));
        return false;
    }
    if (!document) {
        referencesSetStatus("Rename Symbol requires an active source document.");
        markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER rename_apply=FAIL", RenameErrorName(RenameErrorCode::NoTarget));
        return false;
    }
    if (!IsSymbolSourcePath(document->path)) {
        referencesSetStatus("Rename Symbol is available only in C/C++ source files.");
        return false;
    }
    DefinitionIdentifier identifier = {};
    if (!ExtractDefinitionIdentifier(document->buffer.data, document->buffer.length, document->buffer.caret,
                                     document->buffer.selectionActive, document->buffer.selectionAnchor,
                                     document->buffer.caret, &identifier)) {
        referencesSetStatus("Rename requires an identifier under the caret.");
        markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER rename_apply=FAIL", RenameErrorName(RenameErrorCode::NoTarget));
        return false;
    }
    if (g_controller.model.activeDocument < kMaxOpenDocuments)
        WorkspaceControllerUpdateDocumentSymbols(&g_controller, g_controller.model.activeDocument);
    char relativePath[kMaxPathBytes] = {};
    if (!copyProjectRelativePath(g_controller.model, document->path, relativePath, sizeof(relativePath))) {
        referencesSetStatus("Active document path is invalid.");
        return false;
    }
    if (!BuildDefinitionQuery(*document, g_controller.model.project.projectId,
                              g_controller.model.projectGeneration, g_controller.model.rootPath,
                              relativePath, g_nextReferenceQueryId++, &g_referenceQuery)) {
        referencesSetStatus("Rename requires an indexed symbol.");
        return false;
    }
    logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER rename_begin=PASS");
    g_referenceTargetResolution = {};
    if (!ResolveReferenceTarget(&g_symbolDatabase, g_referenceQuery, g_referenceCandidates,
                               kReferenceMaxCandidates, false, &g_referenceTargetResolution)) {
        referencesSetStatus("Rename requires an indexed symbol.");
        markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER rename_apply=FAIL", RenameErrorName(RenameErrorCode::NoTarget));
        return false;
    }
    if (g_referenceTargetResolution.kind == ReferenceTargetResolutionKind::Direct) {
        if (!ReferenceTargetFromDefinitionCandidate(g_referenceCandidates[0], g_controller.model.project.projectId,
                                                    g_controller.model.projectGeneration, &g_referenceTarget)) {
            referencesSetStatus("Rename target could not be resolved.");
            return false;
        }
        return startRenameSearchWithTarget(ctx, g_referenceTarget);
    }
    if (g_referenceTargetResolution.kind == ReferenceTargetResolutionKind::Multiple ||
        g_referenceTargetResolution.kind == ReferenceTargetResolutionKind::Stale) {
        g_renameTargetPickerPending = true;
        g_referencesPanelOpen = true;
        referencesSetStatus("Choose the indexed symbol to rename.");
        openReferencePicker(ctx);
        logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER rename_target=MULTIPLE");
        return true;
    }
    referencesSetStatus("Rename requires an indexed symbol.");
    markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER rename_apply=FAIL", RenameErrorName(RenameErrorCode::NoTarget));
    return false;
}

static void pollReferences(gx_app_context* ctx) {
    if (g_referencesOperationId == 0) return;
    if (ReferenceSearchIsActive(&g_referenceSearch) &&
        (!g_controller.model.open || !g_controller.model.hasProject ||
         g_controller.model.projectGeneration != g_referencesProjectGeneration ||
         !searchTextEqual(g_controller.model.project.projectId, g_referencesProjectId))) {
        ReferenceSearchCancel(&g_referenceSearch, g_referencesOperationId);
        referencesSetStatus("Project changed; cancelling reference search.");
    }
    ReferenceSearchPoll(&g_referenceSearch, g_referencesOperationId, 16, gx_get_ticks_ms(ctx));
    const ReferenceSearchOperation* operation = ReferenceSearchOperationInfo(&g_referenceSearch);
    if (!operation || ReferenceSearchIsActive(&g_referenceSearch) || g_referencesTerminalReported) return;
    g_referencesTerminalReported = true;
    if (g_renameSearchPending) {
        g_renameSearchPending = false;
        if (operation->state == ReferenceSearchState::Completed) {
            if (RenameModelBuildFromReferences(&g_renameModel, &g_referenceSearch)) {
                g_renamePanelOpen = true;
                g_renameNameFocused = true;
                g_renameNameCaret = lengthOf(g_renameModel.newName, sizeof(g_renameModel.newName));
                g_renameSelectedCandidate = 0;
                g_renameScroll = 0;
                g_referencesPanelOpen = false;
                g_referencesResultsFocused = false;
                g_editorFocused = false;
                copyText(g_textScratch, sizeof(g_textScratch), "GUIDEXOS_DEVELOPER_STUDIO_MARKER rename_references_complete=PASS exact=");
                appendUnsigned(g_textScratch, sizeof(g_textScratch), g_renameModel.exactCount);
                appendText(g_textScratch, sizeof(g_textScratch), " likely=");
                appendUnsigned(g_textScratch, sizeof(g_textScratch), g_renameModel.likelyCount);
                appendText(g_textScratch, sizeof(g_textScratch), " ambiguous=");
                appendUnsigned(g_textScratch, sizeof(g_textScratch), g_renameModel.ambiguousCount);
                logMarker(ctx, g_textScratch);
                logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER rename_preview=READY");
            } else {
                referencesSetStatus(RenameErrorName(g_renameModel.error));
                g_referencesPanelOpen = false;
            }
        } else if (operation->state == ReferenceSearchState::Cancelled) {
            referencesSetStatus("Rename Symbol search cancelled.");
            logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER rename_apply=FAIL reason=RENAME_CANCELLED");
            g_referencesPanelOpen = false;
        } else {
            referencesSetStatus("Rename Symbol search failed: ");
            appendText(g_referencesStatus, sizeof(g_referencesStatus), ReferenceSearchErrorName(operation->error));
            markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER rename_apply=FAIL", ReferenceSearchErrorName(operation->error));
            g_referencesPanelOpen = false;
        }
        ReferenceSearchRelease(&g_referenceSearch, g_referencesOperationId);
        g_referencesOperationId = 0;
        g_referencesTerminalReported = false;
        return;
    }
    if (operation->state == ReferenceSearchState::Completed) {
        copyText(g_referencesStatus, sizeof(g_referencesStatus), operation->referencesFound == 0 ?
                 "No references found." : "Search complete: ");
        if (operation->referencesFound != 0) {
            appendUnsigned(g_referencesStatus, sizeof(g_referencesStatus),
                           static_cast<uint32_t>(operation->referencesFound));
            appendText(g_referencesStatus, sizeof(g_referencesStatus),
                       operation->truncated ? " references (truncated)" : " references");
        }
        copyText(g_textScratch, sizeof(g_textScratch), operation->truncated ?
                 "GUIDEXOS_DEVELOPER_STUDIO_MARKER references_complete=TRUNCATED files=" :
                 "GUIDEXOS_DEVELOPER_STUDIO_MARKER references_complete=PASS files=");
        appendUnsigned(g_textScratch, sizeof(g_textScratch), static_cast<uint32_t>(operation->filesSearched));
        appendText(g_textScratch, sizeof(g_textScratch), " references=");
        appendUnsigned(g_textScratch, sizeof(g_textScratch), static_cast<uint32_t>(operation->referencesFound));
        logMarker(ctx, g_textScratch);
        writeStudioOutput(operation->truncated ? "Find All References completed with truncated results" :
                          "Find All References completed");
    } else if (operation->state == ReferenceSearchState::Cancelled) {
        referencesSetStatus("Reference search cancelled; partial results retained.");
        logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER references_complete=CANCELLED");
        writeStudioOutput("Find All References cancelled");
    } else {
        copyText(g_referencesStatus, sizeof(g_referencesStatus), "Reference search failed: ");
        appendText(g_referencesStatus, sizeof(g_referencesStatus), ReferenceSearchErrorName(operation->error));
        markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER references_complete=FAIL",
                      ReferenceSearchErrorName(operation->error));
        writeStudioOutput("Find All References failed");
    }
}

static void stopReferenceSearch(gx_app_context* ctx) {
    if (g_referencesOperationId == 0) return;
    if (ReferenceSearchIsActive(&g_referenceSearch)) {
        ReferenceSearchCancel(&g_referenceSearch, g_referencesOperationId);
        ReferenceSearchPoll(&g_referenceSearch, g_referencesOperationId, 1, gx_get_ticks_ms(ctx));
    }
    const ReferenceSearchOperation* operation = ReferenceSearchOperationInfo(&g_referenceSearch);
    if (operation && operation->state != ReferenceSearchState::Idle)
        ReferenceSearchRelease(&g_referenceSearch, g_referencesOperationId);
    g_referencesOperationId = 0;
    g_referencesTerminalReported = false;
}

static uint32_t referencesTotalRows() {
    uint32_t rows = 0;
    const uint32_t groups = ReferenceSearchResultGroups(&g_referenceSearch);
    for (uint32_t i = 0; i < groups; ++i) {
        const ReferenceFileGroup* group = ReferenceSearchResultGroupAt(&g_referenceSearch, i);
        if (group) rows += 1 + group->matchCount;
    }
    return rows;
}

static bool referenceMatchForRow(uint32_t row, uint32_t* groupIndex, uint32_t* matchIndex) {
    uint32_t current = 0;
    const uint32_t groups = ReferenceSearchResultGroups(&g_referenceSearch);
    for (uint32_t group = 0; group < groups; ++group) {
        const ReferenceFileGroup* value = ReferenceSearchResultGroupAt(&g_referenceSearch, group);
        if (!value) continue;
        if (row == current) return false;
        ++current;
        for (uint32_t match = 0; match < value->matchCount; ++match) {
            if (row == current) {
                if (groupIndex) *groupIndex = group;
                if (matchIndex) *matchIndex = match;
                return true;
            }
            ++current;
        }
    }
    return false;
}

static uint32_t selectedReferenceRow() {
    uint32_t row = 0;
    const uint32_t groups = ReferenceSearchResultGroups(&g_referenceSearch);
    for (uint32_t group = 0; group < groups; ++group) {
        const ReferenceFileGroup* value = ReferenceSearchResultGroupAt(&g_referenceSearch, group);
        if (!value) continue;
        if (group == g_referencesSelectedGroup) return row + 1 + g_referencesSelectedMatch;
        row += 1 + value->matchCount;
    }
    return 0;
}

static void ensureReferenceSelectionVisible() {
    const uint32_t row = selectedReferenceRow();
    if (row < g_referencesScroll) g_referencesScroll = row;
    if (row >= g_referencesScroll + static_cast<uint32_t>(kReferencesPanelMaxRows))
        g_referencesScroll = row - kReferencesPanelMaxRows + 1;
}

static void moveReferenceSelection(int32_t delta) {
    const uint32_t total = referencesTotalRows();
    if (total == 0) return;
    uint32_t row = selectedReferenceRow();
    if (delta > 0 && row + 1 < total) ++row;
    if (delta < 0 && row > 0) --row;
    uint32_t group = 0, match = 0;
    if (referenceMatchForRow(row, &group, &match)) {
        g_referencesSelectedGroup = group;
        g_referencesSelectedMatch = match;
    } else if (row == 0) {
        g_referencesSelectedGroup = 0;
        g_referencesSelectedMatch = 0;
    }
    ensureReferenceSelectionVisible();
}

static bool referenceIdentifierBoundary(const TextBuffer& buffer, uint32_t offset, uint32_t length) {
    if (offset > buffer.length || length > buffer.length - offset) return false;
    const bool left = offset > 0 && ((buffer.data[offset - 1] >= 'A' && buffer.data[offset - 1] <= 'Z') ||
        (buffer.data[offset - 1] >= 'a' && buffer.data[offset - 1] <= 'z') ||
        (buffer.data[offset - 1] >= '0' && buffer.data[offset - 1] <= '9') || buffer.data[offset - 1] == '_');
    const uint32_t end = offset + length;
    const bool right = end < buffer.length && ((buffer.data[end] >= 'A' && buffer.data[end] <= 'Z') ||
        (buffer.data[end] >= 'a' && buffer.data[end] <= 'z') ||
        (buffer.data[end] >= '0' && buffer.data[end] <= '9') || buffer.data[end] == '_');
    return !left && !right;
}

static bool selectReferenceIdentifier(Document* document, const ReferenceMatch& match, bool* stale) {
    if (stale) *stale = false;
    if (!document) return false;
    const uint32_t length = lengthOf(g_referenceTarget.identifier, sizeof(g_referenceTarget.identifier));
    if (match.sourceDocumentId == document->documentId &&
        match.sourceDocumentGeneration == document->buffer.generation &&
        match.byteOffset <= document->buffer.length && length <= document->buffer.length - match.byteOffset &&
        textAtOffset(document->buffer, static_cast<uint32_t>(match.byteOffset), g_referenceTarget.identifier) &&
        referenceIdentifierBoundary(document->buffer, static_cast<uint32_t>(match.byteOffset), length))
        return SelectTextRange(&document->buffer, match.byteOffset, length);
    if (stale) *stale = true;
    const uint32_t zeroLine = match.line > 0 ? match.line - 1 : 0;
    const uint32_t lineCount = TextBufferLineCount(&document->buffer);
    if (zeroLine >= lineCount) return false;
    const uint32_t start = TextBufferLineStart(&document->buffer, zeroLine);
    const uint32_t end = TextBufferLineEnd(&document->buffer, zeroLine);
    uint32_t nearest = start;
    uint32_t nearestDistance = 0xFFFFFFFFu;
    bool found = false;
    for (uint32_t offset = start; offset + length <= end; ++offset) {
        if (!textAtOffset(document->buffer, offset, g_referenceTarget.identifier) ||
            !referenceIdentifierBoundary(document->buffer, offset, length)) continue;
        const uint32_t distance = offset > match.byteOffset ?
            offset - static_cast<uint32_t>(match.byteOffset) : static_cast<uint32_t>(match.byteOffset) - offset;
        if (!found || distance < nearestDistance) { found = true; nearest = offset; nearestDistance = distance; }
    }
    return found && SelectTextRange(&document->buffer, nearest, length);
}

static bool activateReferenceResult(gx_app_context* ctx, uint32_t groupIndex, uint32_t matchIndex) {
    const ReferenceSearchOperation* operation = ReferenceSearchOperationInfo(&g_referenceSearch);
    const ReferenceFileGroup* group = ReferenceSearchResultGroupAt(&g_referenceSearch, groupIndex);
    const ReferenceMatch* match = ReferenceSearchResultMatchAt(&g_referenceSearch, group, matchIndex);
    if (!operation || !group || !match || !g_controller.model.open || !g_controller.model.hasProject) {
        referencesSetStatus("Reference result cannot be activated.");
        return false;
    }
    if (operation->target.projectGeneration != g_controller.model.projectGeneration ||
        !searchTextEqual(operation->target.projectIdText, g_controller.model.project.projectId)) {
        referencesSetStatus("Reference result belongs to a stale project.");
        markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER references_activate=STALE",
                      ReferenceSearchErrorName(ReferenceSearchErrorCode::ActivationStale));
        return false;
    }
    if (group->relativePath[0] == '\0' || PathContainsTraversal(group->relativePath) ||
        group->relativePath[0] == '/' || group->relativePath[0] == '\\' || group->relativePath[1] == ':') {
        referencesSetStatus("Reference result path rejected.");
        markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER references_activate=FAIL",
                      ReferenceSearchErrorName(ReferenceSearchErrorCode::PathOutsideProject));
        return false;
    }
    char absolutePath[kMaxPathBytes] = {};
    if (!JoinWorkspacePath(g_controller.model.rootPath, group->relativePath, absolutePath, sizeof(absolutePath))) {
        referencesSetStatus("Reference result is outside the project.");
        return false;
    }
    (void)absolutePath;
    if (!g_referenceOriginValid) {
        Document* origin = WorkspaceControllerActiveDocument(&g_controller);
        if (origin && !captureNavigationLocation(*origin, &g_referenceOrigin)) return false;
        g_referenceOriginValid = origin != nullptr;
    }
    uint32_t documentIndex = kMaxOpenDocuments;
    OutputErrorCode navigationError = OutputErrorCode::None;
    if (!WorkspaceControllerOpenDocumentAtLocation(&g_controller, g_controller.model.project.projectId,
                                                   group->relativePath, match->line, match->column,
                                                   &documentIndex, &navigationError)) {
        referencesSetStatus("Unable to open reference result.");
        markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER references_activate=FAIL",
                      OutputErrorName(navigationError));
        return false;
    }
    Document* document = WorkspaceControllerActiveDocument(&g_controller);
    if (!document) return false;
    bool stale = false;
    const bool selected = selectReferenceIdentifier(document, *match, &stale);
    if (!selected) WorkspaceControllerSetCaretPosition(&g_controller, documentIndex, match->line,
                                                       match->column, nullptr, &navigationError);
    if (g_referenceOriginValid) NavigationHistoryPush(&g_navigationHistory, g_referenceOrigin);
    g_referenceOriginValid = false;
    g_referencesPanelOpen = false;
    g_referencesResultsFocused = false;
    g_editorFocused = true;
    g_outputFocused = false;
    keepCaretVisible(document);
    if (stale || !selected) {
        referencesSetStatus("Reference location was stale; exact selection was not available.");
        logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER references_activate=STALE");
    } else {
        referencesSetStatus("");
        copyText(g_textScratch, sizeof(g_textScratch),
                 "GUIDEXOS_DEVELOPER_STUDIO_MARKER references_activate=PASS path=");
        appendText(g_textScratch, sizeof(g_textScratch), group->relativePath);
        appendText(g_textScratch, sizeof(g_textScratch), " line=");
        appendUnsigned(g_textScratch, sizeof(g_textScratch), match->line);
        appendText(g_textScratch, sizeof(g_textScratch), " column=");
        appendUnsigned(g_textScratch, sizeof(g_textScratch), match->column);
        logMarker(ctx, g_textScratch);
    }
    return true;
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
    if (g_referencesOperationId != 0) {
        stopReferenceSearch(ctx);
        g_referencesPanelOpen = false;
        g_referencesResultsFocused = false;
        g_referencePickerOpen = false;
    }
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
    gx_file_entry* nativeEntries = g_fsListEntries;
    for (uint32_t i = 0; i < kMaxWorkspaceEntries; ++i) nativeEntries[i] = {};
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
    nativeRequest.flags = request.debugControlled ? GX_DEVELOPMENT_RUN_FLAG_DEBUG_CONTROLLED : 0u;
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

static bool hostRunPoll(void* userData, uint64_t handle, RunResult* outResult);

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
    // Start publishes the process-table identity synchronously on the Server,
    // but the old adapter discarded that start-time snapshot. Poll once here
    // so the debugger receives the exact target PID before binding memory.
    RunResult launched = *outResult;
    if (!hostRunPoll(userData, handle, &launched)) return false;
    *outResult = launched;
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

static bool hostDebugCommand(void* userData, HostedDebugCommand command, uint64_t handle,
                             uint64_t sessionGeneration, uint64_t processId, uint64_t nativeRuntimeId,
                             uint64_t breakpointId, uint64_t targetAddress, const char* artifactSha256,
                             HostedDebugResult* outResult) {
    NativeFileSystemContext* context = static_cast<NativeFileSystemContext*>(userData);
    if (outResult) *outResult = HostedDebugResult();
    if (!context || !context->app || !context->app->host || !context->app->host->development_debug || !outResult) return false;
    gx_development_debug_request request = {};
    request.size = sizeof(request);
    request.version = GX_DEVELOPMENT_DEBUG_API_VERSION;
    request.command = static_cast<uint32_t>(command);
    request.handle = handle;
    request.sessionGeneration = sessionGeneration;
    request.processId = processId;
    request.nativeRuntimeId = nativeRuntimeId;
    request.breakpointId = breakpointId;
    request.targetAddress = targetAddress;
    request.artifactSha256 = artifactSha256;
    gx_development_debug_snapshot snapshot = {};
    snapshot.size = sizeof(snapshot);
    snapshot.version = GX_DEVELOPMENT_DEBUG_API_VERSION;
    const gx_result result = context->app->host->development_debug(context->app, &request, &snapshot);
    if (result != GX_OK) {
        copyText(outResult->errorMessage, sizeof(outResult->errorMessage), snapshot.errorMessage);
        return false;
    }
    outResult->status = snapshot.status;
    outResult->trapKind = snapshot.trapKind;
    outResult->bindingId = snapshot.bindingId;
    outResult->processId = snapshot.processId;
    outResult->nativeRuntimeId = snapshot.nativeRuntimeId;
    outResult->threadId = snapshot.threadId;
    outResult->instructionPointer = snapshot.instructionPointer;
    outResult->targetAddress = snapshot.targetAddress;
    outResult->originalByte = snapshot.originalByte;
    outResult->installedByte = snapshot.installedByte;
    outResult->originalByteValid = snapshot.originalByteValid != 0;
    outResult->bindingInstalled = snapshot.bindingInstalled != 0;
    outResult->bindingCount = snapshot.bindingCount;
    copyText(outResult->errorMessage, sizeof(outResult->errorMessage), snapshot.errorMessage);
    return true;
}

static HostedDevelopmentRunService developmentRunService() {
    HostedDevelopmentRunService service = {};
    service.userData = &g_fileSystemContext;
    service.prepare = hostRunPrepare;
    service.start = hostRunStart;
    service.poll = hostRunPoll;
    service.requestClose = hostRunRequestClose;
    service.release = hostRunRelease;
    service.debugCommand = hostDebugCommand;
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

static bool beginBuild(gx_app_context* ctx, BuildDirtyDecision dirtyDecision, bool debugInfo = false) {
    dismissCompletion(ctx, "build", false);
    dismissSignatureHelp(ctx, "build", false);
    if (BuildControllerIsActive(&g_buildController)) {
        writeOutput("Build already running");
        markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER build_precondition=FAIL", BuildErrorName(BuildErrorCode::AlreadyRunning));
        return false;
    }
    BuildErrorCode error = BuildErrorCode::None;
    logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER build_request=PASS");
    g_buildTerminalReported = false;
    if (!BuildControllerStart(&g_buildController, &g_controller, buildService(), dirtyDecision, &error, &g_outputService, debugInfo)) {
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
        if (g_debugWaitingForBuild) {
            g_debugWaitingForBuild = false;
            if (g_buildController.result.state == BuildState::Succeeded) {
                writeStudioOutput("Debug: build completed");
                if (!beginDebugSession(ctx)) writeStudioOutput("Debug: deployment skipped");
            } else {
                copyText(g_textScratch, sizeof(g_textScratch), "Debug: build failed | ");
                appendText(g_textScratch, sizeof(g_textScratch), BuildErrorName(g_buildController.result.error));
                reportDebugMessage(ctx, g_textScratch);
                DebugDwarfMapperReset(&g_debugMapper);
                DebugControllerMarkArtifactStale(&g_debugController, "Stale: executable rebuild failed");
            }
        }
    }
}

static void requestBuild(gx_app_context* ctx) {
    dismissSignatureHelp(ctx, "build", false);
    if (DebugControllerIsActive(&g_debugController) || g_debugWaitingForBuild) {
        writeOutput("Debug session in progress; build blocked");
        return;
    }
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
    dismissCompletion(ctx, "run", false);
    dismissSignatureHelp(ctx, "run", false);
    if (DebugControllerIsActive(&g_debugController) || g_debugWaitingForBuild) {
        writeOutput("Debug session in progress; run blocked");
        return;
    }
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

static void reportDebugMessage(gx_app_context* ctx, const char* message) {
    if (!message) return;
    writeStudioOutput(message);
    logMarker(ctx, message);
}

static bool loadDebugSymbolsForTarget(gx_app_context* ctx, DebugTarget* target) {
    if (!target) return false;
    char absolutePath[kMaxPathBytes] = {};
    if (!JoinWorkspacePath(target->projectRoot, target->executablePath, absolutePath, sizeof(absolutePath))) {
        DebugDwarfMapperReset(&g_debugMapper);
        g_debugMapper.state = guidexos::developer_studio::DebugDwarfMapperState::Failed;
        g_debugMapper.error = DebugDwarfError::ArtifactChanged;
        reportDebugMessage(ctx, "Debug info: artifact path rejected");
        return false;
    }
    FileInfo info = {};
    if (!fsStat(&g_fileSystemContext, absolutePath, &info) || info.kind != FileInfoKind::RegularFile ||
        info.size == 0 || info.size > guidexos::developer_studio::kDebugMapperMaxElfBytes) {
        DebugDwarfMapperReset(&g_debugMapper);
        g_debugMapper.state = guidexos::developer_studio::DebugDwarfMapperState::Failed;
        g_debugMapper.error = info.size > guidexos::developer_studio::kDebugMapperMaxElfBytes ?
            DebugDwarfError::LimitExceeded : DebugDwarfError::ArtifactChanged;
        reportDebugMessage(ctx, "Debug info: executable could not be read");
        return false;
    }
    uint32_t bytesRead = 0;
    if (!fsRead(&g_fileSystemContext, absolutePath, reinterpret_cast<char*>(g_debugArtifactBytes),
                static_cast<uint32_t>(info.size), &bytesRead) || bytesRead != info.size) {
        DebugDwarfMapperReset(&g_debugMapper);
        g_debugMapper.state = guidexos::developer_studio::DebugDwarfMapperState::Failed;
        g_debugMapper.error = DebugDwarfError::ArtifactChanged;
        reportDebugMessage(ctx, "Debug info: executable read was incomplete");
        return false;
    }
    target->artifactSize = info.size;
    DebugDwarfError error = DebugDwarfError::None;
    const bool loaded = DebugDwarfMapperLoad(&g_debugMapper, target->projectRoot, target->projectId,
        target->targetProfile, target->architecture, target->executablePath, info.size,
        target->artifactSha256, target->projectGeneration, g_debugArtifactBytes, bytesRead,
        static_cast<uint32_t>(target->projectGeneration), &error);
    if (!loaded) {
        copyText(g_textScratch, sizeof(g_textScratch), "Debug info: ");
        appendText(g_textScratch, sizeof(g_textScratch), DebugDwarfErrorName(error));
        reportDebugMessage(ctx, g_textScratch);
        return false;
    }
    copyText(g_textScratch, sizeof(g_textScratch), "Debug info: ");
    appendText(g_textScratch, sizeof(g_textScratch), DebugDwarfMapperStateName(g_debugMapper.state));
    appendText(g_textScratch, sizeof(g_textScratch), " | DWARF ");
    appendUnsigned(g_textScratch, sizeof(g_textScratch), g_debugMapper.dwarfVersion);
    appendText(g_textScratch, sizeof(g_textScratch), " | files=");
    appendUnsigned(g_textScratch, sizeof(g_textScratch), g_debugMapper.sourceFileCount);
    appendText(g_textScratch, sizeof(g_textScratch), " | rows=");
    appendUnsigned(g_textScratch, sizeof(g_textScratch), g_debugMapper.lineRowCount);
    if (g_debugMapper.truncated) appendText(g_textScratch, sizeof(g_textScratch), " | truncated");
    reportDebugMessage(ctx, g_textScratch);
    return true;
}

static bool beginDebugSession(gx_app_context* ctx) {
    if (!g_controller.model.open || !g_controller.model.hasProject) {
        writeStudioOutput("Debug requires an open project");
        return false;
    }
    DebugTarget target = {};
    DebugErrorCode error = DebugErrorCode::None;
    if (!DebugTargetFromBuild(g_controller.model.project, g_buildController.result,
                              g_controller.model.projectGeneration, &target, &error)) {
        copyText(g_textScratch, sizeof(g_textScratch), "Debug target unavailable: ");
        appendText(g_textScratch, sizeof(g_textScratch), DebugErrorName(error));
        reportDebugMessage(ctx, g_textScratch);
        return false;
    }
    loadDebugSymbolsForTarget(ctx, &target);
    DebugControllerSetProjectContext(&g_debugController, target.projectId, target.projectRoot, target.projectGeneration);
    g_debugController.target = target;
    DebugErrorCode mappingError = DebugErrorCode::None;
    DebugControllerMapBreakpoints(&g_debugController, &g_debugMapper, &mappingError);
    if (!DebugControllerStart(&g_debugController, g_debugBackend, target, &error)) {
        copyText(g_textScratch, sizeof(g_textScratch), "Debug launch failed: ");
        appendText(g_textScratch, sizeof(g_textScratch), DebugErrorName(error));
        reportDebugMessage(ctx, g_textScratch);
        return false;
    }
    g_debugTerminalReported = false;
    copyText(g_textScratch, sizeof(g_textScratch), "Debug: launching ");
    appendText(g_textScratch, sizeof(g_textScratch), target.projectId);
    reportDebugMessage(ctx, g_textScratch);
    logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_start=PASS");
    return true;
}

static bool beginDebugBuild(gx_app_context* ctx, BuildDirtyDecision dirtyDecision) {
    if (!g_controller.model.open || !g_controller.model.hasProject) {
        writeStudioOutput("Debug requires an open project");
        return false;
    }
    if (BuildControllerIsActive(&g_buildController) || RunControllerIsActive(&g_runController) ||
        DebugControllerIsActive(&g_debugController)) {
        writeStudioOutput("Debug start blocked by an active operation");
        return false;
    }
    g_debugWaitingForBuild = true;
    g_debugTerminalReported = false;
    if (!beginBuild(ctx, dirtyDecision, true)) {
        g_debugWaitingForBuild = false;
        writeStudioOutput("Debug build could not start");
        return false;
    }
    writeStudioOutput("Debug: build started");
    return true;
}

static void requestDebug(gx_app_context* ctx) {
    dismissCompletion(ctx, "debug", false);
    dismissSignatureHelp(ctx, "debug", false);
    if (DebugControllerIsActive(&g_debugController) || g_debugWaitingForBuild) {
        writeStudioOutput("Debug session already active");
        return;
    }
    if (RunControllerIsActive(&g_runController)) {
        writeStudioOutput("Run in progress; debug start blocked");
        return;
    }
    if (BuildControllerIsActive(&g_buildController)) {
        writeStudioOutput("Build in progress; debug start blocked");
        return;
    }
    if (!g_controller.model.open || !g_controller.model.hasProject) {
        writeStudioOutput("Debug requires an open project");
        return;
    }
    if (g_controller.model.project.kind != ProjectKind::NativeGuiApplication) {
        writeStudioOutput("Hosted Native ELF debugging supports Native GUI Application projects only");
        return;
    }
    if (guidexos::developer_studio::WorkspaceControllerHasDirtyProjectDocuments(&g_controller)) {
        g_inputMode = InputMode::ConfirmDebug;
        g_fileMenuOpen = false;
        g_buildMenuOpen = false;
        g_debugMenuOpen = false;
        writeStudioOutput("Save All or Cancel debug start");
        return;
    }
    beginDebugBuild(ctx, BuildDirtyDecision::SaveAll);
}

static void requestDebugStop(gx_app_context* ctx) {
    if (!DebugControllerIsActive(&g_debugController)) {
        writeStudioOutput("No active debug session");
        return;
    }
    DebugErrorCode error = DebugErrorCode::None;
    if (!DebugControllerRequestStop(&g_debugController, g_debugBackend, &error)) {
        copyText(g_textScratch, sizeof(g_textScratch), "Debug stop unavailable: ");
        appendText(g_textScratch, sizeof(g_textScratch), DebugErrorName(error));
        reportDebugMessage(ctx, g_textScratch);
        return;
    }
    reportDebugMessage(ctx, "Debug: stop requested");
    logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_stop=requested");
}

static void pollDebug(gx_app_context* ctx) {
    if (!DebugControllerIsActive(&g_debugController)) return;
    if (!g_controller.model.hasProject || !PathsEqual(g_controller.model.project.projectId, g_debugController.projectId)) {
        if (g_debugController.state != DebugSessionState::Stopping) requestDebugStop(ctx);
        return;
    }
    DebugControllerMarkProjectGeneration(&g_debugController, g_controller.model.projectGeneration);
    for (uint32_t i = 0; i < kMaxOpenDocuments; ++i) {
        const Document& document = g_controller.model.documents[i];
        if (!document.used) continue;
        char relative[kMaxProjectPathBytes] = {};
        if (DebugRelativeSourcePath(g_controller.model.project.rootPath, document.path,
                                    relative, sizeof(relative))) {
            DebugControllerMarkSourceGeneration(&g_debugController, g_controller.model.project.projectId,
                                                relative, document.buffer.generation);
        }
    }
    const DebugSessionState previous = g_debugController.state;
    if (!DebugControllerPoll(&g_debugController, g_debugBackend)) {
        if (!g_debugTerminalReported) {
            copyText(g_textScratch, sizeof(g_textScratch), "Debug: backend poll failed | ");
            appendText(g_textScratch, sizeof(g_textScratch), DebugErrorName(g_debugController.error));
            reportDebugMessage(ctx, g_textScratch);
            g_debugTerminalReported = true;
        }
        return;
    }
    if (g_debugController.state != previous) {
        if (g_debugController.state == DebugSessionState::Running) {
            reportDebugMessage(ctx, "Debug: process running");
            logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_state=RUNNING");
        } else if (g_debugController.state == DebugSessionState::Stopping) {
            reportDebugMessage(ctx, "Debug: stopping target");
        } else if (g_debugController.state == DebugSessionState::Paused) {
            DebugErrorCode stopMappingError = DebugErrorCode::None;
            DebugControllerResolveCurrentStop(&g_debugController, &g_debugMapper, &stopMappingError);
            copyText(g_textScratch, sizeof(g_textScratch), "Debug: paused | ");
            appendText(g_textScratch, sizeof(g_textScratch), DebugStopReasonName(g_debugController.stopReason));
            if (g_debugController.currentLocation.relativePath[0]) {
                appendText(g_textScratch, sizeof(g_textScratch), " | ");
                appendText(g_textScratch, sizeof(g_textScratch), g_debugController.currentLocation.relativePath);
                appendText(g_textScratch, sizeof(g_textScratch), ":");
                appendUnsigned(g_textScratch, sizeof(g_textScratch), g_debugController.currentLocation.line);
            }
            reportDebugMessage(ctx, g_textScratch);
            logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_state=PAUSED_BREAKPOINT");
        } else if (g_debugController.state == DebugSessionState::Exited) {
            reportDebugMessage(ctx, "Debug: process exited");
            logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_state=EXITED");
        } else if (g_debugController.state == DebugSessionState::Failed) {
            copyText(g_textScratch, sizeof(g_textScratch), "Debug: session failed | ");
            appendText(g_textScratch, sizeof(g_textScratch), g_debugController.lastMessage);
            reportDebugMessage(ctx, g_textScratch);
            g_debugTerminalReported = true;
        }
    }
    if (!DebugControllerIsActive(&g_debugController) && g_debugController.state == DebugSessionState::Exited)
        g_debugTerminalReported = true;
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
    if (DebugControllerIsActive(&g_debugController) || g_debugWaitingForBuild) { writeStudioOutput("Debug in progress"); return; }
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
    if (DebugControllerIsActive(&g_debugController) || g_debugWaitingForBuild) { writeStudioOutput("Debug in progress"); return; }
    g_inputMode = InputMode::ProjectPath;
    g_fileMenuOpen = false;
    g_buildMenuOpen = false;
    g_workspaceSwitchPending = false;
    copyText(g_prompt, sizeof(g_prompt), "");
}

static void showNewProjectPrompt(gx_app_context* ctx) {
    if (BuildControllerIsActive(&g_buildController)) { writeOutput("Build in progress"); return; }
    if (RunControllerIsActive(&g_runController)) { writeOutput("Run in progress"); return; }
    if (DebugControllerIsActive(&g_debugController) || g_debugWaitingForBuild) { writeStudioOutput("Debug in progress"); return; }
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
    if (IncludeGraphIsActive(&g_includeGraphOperation)) IncludeGraphCancel(&g_includeGraphOperation, g_includeGraphOperationId);
    if (OwnershipGraphBuildIsActive(&g_ownershipService)) OwnershipGraphBuildCancel(&g_ownershipService, g_ownershipOperationId);
    g_includeGraphPanelOpen = false;
    g_includeTargetPickerOpen = false;
    g_ownershipPanelOpen = false;
    stopProjectSearch(ctx);
    logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER project_create_validation=PASS");
    logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER project_create=PASS");
    if (created.rollbackAttempted && created.rollbackSucceeded) logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER project_rollback=PASS");
    if (!WorkspaceControllerOpenProject(&g_controller, created.project.rootPath)) {
        writeOutput("Project created; project open failed");
        reportProjectFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER project_open=FAIL", g_controller.lastProjectError);
        return false;
    }
    DebugControllerClearBreakpoints(&g_debugController);
    g_debugPanelOpen = false;
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
    dismissTypeInfo(ctx, "project_changed", false);
    if (IncludeGraphIsActive(&g_includeGraphOperation)) IncludeGraphCancel(&g_includeGraphOperation, g_includeGraphOperationId);
    if (OwnershipGraphBuildIsActive(&g_ownershipService)) OwnershipGraphBuildCancel(&g_ownershipService, g_ownershipOperationId);
    g_includeGraphPanelOpen = false;
    g_includeTargetPickerOpen = false;
    g_ownershipPanelOpen = false;
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
    dismissCompletion(ctx, "project_changed", false);
    dismissSignatureHelp(ctx, "project_changed", false);
    dismissTypeInfo(ctx, "project_changed", false);
    if (IncludeGraphIsActive(&g_includeGraphOperation)) IncludeGraphCancel(&g_includeGraphOperation, g_includeGraphOperationId);
    if (OwnershipGraphBuildIsActive(&g_ownershipService)) OwnershipGraphBuildCancel(&g_ownershipService, g_ownershipOperationId);
    g_includeGraphPanelOpen = false;
    g_includeTargetPickerOpen = false;
    g_ownershipPanelOpen = false;
    stopProjectSearch(ctx);
    if (WorkspaceControllerOpenProject(&g_controller, g_prompt)) {
        DebugControllerClearBreakpoints(&g_debugController);
        g_debugPanelOpen = false;
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
    dismissCompletion(ctx, "workspace_changed", false);
    dismissSignatureHelp(ctx, "workspace_changed", false);
    dismissTypeInfo(ctx, "workspace_changed", false);
    if (IncludeGraphIsActive(&g_includeGraphOperation)) IncludeGraphCancel(&g_includeGraphOperation, g_includeGraphOperationId);
    if (OwnershipGraphBuildIsActive(&g_ownershipService)) OwnershipGraphBuildCancel(&g_ownershipService, g_ownershipOperationId);
    g_includeGraphPanelOpen = false;
    g_includeTargetPickerOpen = false;
    g_ownershipPanelOpen = false;
    stopProjectSearch(ctx);
    bool success = WorkspaceControllerOpenWorkspace(&g_controller, g_pendingWorkspacePath);
    if (success) {
        DebugControllerClearBreakpoints(&g_debugController);
        g_debugPanelOpen = false;
    }
    reportWorkspaceOpen(ctx, success);
    g_inputMode = InputMode::Normal;
    return success;
}

static void requestWorkspaceOpen(gx_app_context* ctx) {
    dismissCompletion(ctx, "workspace_changed", false);
    dismissSignatureHelp(ctx, "workspace_changed", false);
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
    dismissCompletion(ctx, "document_changed", false);
    dismissSignatureHelp(ctx, "document_changed", false);
    dismissTypeInfo(ctx, "document_changed", false);
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
    dismissCompletion(ctx, "application_close", false);
    dismissSignatureHelp(ctx, "application_close", false);
    dismissTypeInfo(ctx, "application_close", false);
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
    if (g_debugWaitingForBuild) {
        writeStudioOutput("Debug build in progress: close blocked");
        return;
    }
    if (DebugControllerIsActive(&g_debugController)) {
        g_inputMode = InputMode::ConfirmDebugClose;
        writeStudioOutput("Debug session is active: Stop it before closing Studio");
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

static void drawRenamePanel(gx_app_context* ctx) {
    if (!g_renamePanelOpen) return;
    drawPanel(ctx, { 24, 58, 912, 580 }, 0x2A3852u);
    drawText(ctx, 48, 88, "Rename Symbol");
    copyText(g_textScratch, sizeof(g_textScratch), "Target: ");
    appendText(g_textScratch, sizeof(g_textScratch), g_renameModel.target.qualifiedName[0]
               ? g_renameModel.target.qualifiedName : g_renameModel.target.identifier);
    appendText(g_textScratch, sizeof(g_textScratch), "  [");
    appendText(g_textScratch, sizeof(g_textScratch), SymbolKindName(g_renameModel.target.kind));
    appendText(g_textScratch, sizeof(g_textScratch), "]");
    drawText(ctx, 48, 112, g_textScratch);
    drawText(ctx, 48, 138, "New name:");
    drawPanel(ctx, { 122, 120, 320, 26 }, g_renameNameFocused ? 0x405775u : 0x202A36u);
    drawText(ctx, 130, 139, g_renameModel.newName);
    copyText(g_textScratch, sizeof(g_textScratch), "Files: ");
    appendUnsigned(g_textScratch, sizeof(g_textScratch), g_renameModel.fileCount);
    appendText(g_textScratch, sizeof(g_textScratch), "  Exact selected: ");
    appendUnsigned(g_textScratch, sizeof(g_textScratch), RenameModelSelectedCount(&g_renameModel));
    appendText(g_textScratch, sizeof(g_textScratch), "  Likely: ");
    appendUnsigned(g_textScratch, sizeof(g_textScratch), g_renameModel.likelyCount);
    appendText(g_textScratch, sizeof(g_textScratch), "  Ambiguous: ");
    appendUnsigned(g_textScratch, sizeof(g_textScratch), g_renameModel.ambiguousCount);
    drawText(ctx, 468, 138, g_textScratch);
    if (g_renameModel.conflictSeverity != guidexos::developer_studio::RenameConflictSeverity::None)
        drawText(ctx, 48, 166, g_renameModel.conflictMessage);
    else drawText(ctx, 48, 166, "Exact references are selected. Likely and ambiguous references are excluded by default.");
    drawText(ctx, 48, 190, "Tab: name/list   Space: toggle   E: select exact   C: clear   Enter: Apply   Escape: Cancel");
    const uint32_t end = g_renameScroll + 10u < g_renameModel.candidateCount
        ? g_renameScroll + 10u : g_renameModel.candidateCount;
    int y = 218;
    for (uint32_t index = g_renameScroll; index < end; ++index, y += 38) {
        const RenameEditCandidate& candidate = g_renameModel.candidates[index];
        if (index == g_renameSelectedCandidate && !g_renameNameFocused)
            drawPanel(ctx, { 40, y - 14, 880, 18 }, 0x405775u);
        const bool selected = candidate.state == RenameCandidateState::Selected;
        copyText(g_textScratch, sizeof(g_textScratch), selected ? "[x] " :
                 (candidate.state == RenameCandidateState::Disabled ? "[-] " : "[ ] "));
        appendText(g_textScratch, sizeof(g_textScratch), candidate.relativePath);
        appendText(g_textScratch, sizeof(g_textScratch), ":");
        appendUnsigned(g_textScratch, sizeof(g_textScratch), candidate.line);
        appendText(g_textScratch, sizeof(g_textScratch), " ");
        appendText(g_textScratch, sizeof(g_textScratch), ReferenceKindName(candidate.referenceKind));
        appendText(g_textScratch, sizeof(g_textScratch), " ");
        appendText(g_textScratch, sizeof(g_textScratch), ReferenceConfidenceName(candidate.confidence));
        if (candidate.stale) appendText(g_textScratch, sizeof(g_textScratch), " STALE");
        drawText(ctx, 48, y, g_textScratch);
        copyText(g_lineScratch, sizeof(g_lineScratch), "- ");
        appendText(g_lineScratch, sizeof(g_lineScratch), candidate.previewBefore);
        drawText(ctx, 70, y + 14, g_lineScratch);
        copyText(g_lineScratch, sizeof(g_lineScratch), "+ ");
        appendText(g_lineScratch, sizeof(g_lineScratch), candidate.previewAfter);
        drawText(ctx, 70, y + 28, g_lineScratch);
    }
    copyText(g_textScratch, sizeof(g_textScratch), "Status: ");
    appendText(g_textScratch, sizeof(g_textScratch), RenameModelStatus(&g_renameModel));
    drawText(ctx, 48, 620, g_textScratch);
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

static void drawReferencePicker(gx_app_context* ctx) {
    if (!g_referencePickerOpen) return;
    drawPanel(ctx, { 112, 72, 736, 520 }, 0x2A3852u);
    drawText(ctx, 140, 104, "Choose Symbol for Find All References");
    drawText(ctx, 140, 128, "Enter: search selected symbol   Escape: cancel");
    const uint32_t end = g_referenceCandidateScroll + 18u < g_referenceTargetResolution.visibleCandidateCount ?
        g_referenceCandidateScroll + 18u : g_referenceTargetResolution.visibleCandidateCount;
    for (uint32_t row = g_referenceCandidateScroll; row < end; ++row) {
        const DefinitionCandidate& candidate = g_referenceCandidates[row];
        const int y = 158 + static_cast<int>((row - g_referenceCandidateScroll) * 24);
        if (row == g_referenceSelectedCandidate) drawPanel(ctx, { 132, y - 16, 696, 22 }, 0x405775u);
        copyText(g_textScratch, sizeof(g_textScratch), SymbolKindPrefix(candidate.symbol.symbol.kind));
        appendText(g_textScratch, sizeof(g_textScratch), " ");
        appendText(g_textScratch, sizeof(g_textScratch), candidate.symbol.symbol.qualifiedName[0] ?
                   candidate.symbol.symbol.qualifiedName : candidate.symbol.symbol.name);
        appendText(g_textScratch, sizeof(g_textScratch), "  ");
        appendText(g_textScratch, sizeof(g_textScratch), candidate.relativePath);
        appendText(g_textScratch, sizeof(g_textScratch), ":");
        appendUnsigned(g_textScratch, sizeof(g_textScratch), candidate.symbol.symbol.location.line);
        appendText(g_textScratch, sizeof(g_textScratch), "  ");
        appendText(g_textScratch, sizeof(g_textScratch), SymbolDeclarationRoleName(candidate.symbol.symbol.declarationRole));
        drawText(ctx, 144, y, g_textScratch);
    }
}

static void drawReferencesPanel(gx_app_context* ctx) {
    if (!g_referencesPanelOpen) return;
    drawPanel(ctx, { 8, kReferencesPanelTop + 4, 944, 580 }, 0x263650u);
    drawText(ctx, 24, 78, "Find All References");
    drawText(ctx, 24, 103, "Target:");
    drawPanel(ctx, { kReferencesFieldX, 87, kReferencesFieldWidth, 20 }, 0x202A36u);
    copyText(g_textScratch, sizeof(g_textScratch), g_referenceTarget.qualifiedName[0] ?
             g_referenceTarget.qualifiedName : g_referenceTarget.identifier);
    drawText(ctx, kReferencesFieldX + 5, 102, g_textScratch);
    drawText(ctx, 620, 103, "[x] declarations");
    drawText(ctx, 748, 103, "[x] ambiguous");
    const ReferenceSearchOperation* operation = ReferenceSearchOperationInfo(&g_referenceSearch);
    const bool active = ReferenceSearchIsActive(&g_referenceSearch);
    drawPanel(ctx, { 760, 120, 72, 20 }, 0x34496Au);
    drawText(ctx, 770, 135, active ? "Running" : "Search");
    drawPanel(ctx, { 840, 120, 72, 20 }, active ? 0xA96F2Au : 0x34496Au);
    drawText(ctx, 851, 135, "Cancel");
    if (active && operation) {
        copyText(g_textScratch, sizeof(g_textScratch), ReferenceSearchStateName(operation->state));
        appendText(g_textScratch, sizeof(g_textScratch), "  ");
        appendUnsigned(g_textScratch, sizeof(g_textScratch), static_cast<uint32_t>(operation->filesSearched));
        appendText(g_textScratch, sizeof(g_textScratch), " files, ");
        appendUnsigned(g_textScratch, sizeof(g_textScratch), static_cast<uint32_t>(operation->referencesFound));
        appendText(g_textScratch, sizeof(g_textScratch), " references");
        drawText(ctx, 24, 177, g_textScratch);
    } else if (g_referencesStatus[0] != '\0') {
        drawText(ctx, 24, 177, g_referencesStatus);
    }
    if (operation && operation->lexicalFallback)
        drawText(ctx, 24, 195, "Lexical fallback: results are identifier matches, not semantic bindings.");
    drawPanel(ctx, { 16, kReferencesPanelResultsTop - 12, 928, 1 }, 0x405775u);
    uint32_t row = 0;
    const uint32_t firstRow = g_referencesScroll;
    const uint32_t lastRow = firstRow + kReferencesPanelMaxRows;
    const uint32_t groups = ReferenceSearchResultGroups(&g_referenceSearch);
    for (uint32_t groupIndex = 0; groupIndex < groups; ++groupIndex) {
        const ReferenceFileGroup* group = ReferenceSearchResultGroupAt(&g_referenceSearch, groupIndex);
        if (!group) continue;
        if (row >= firstRow && row < lastRow) {
            copyText(g_textScratch, sizeof(g_textScratch), group->relativePath);
            appendText(g_textScratch, sizeof(g_textScratch), " (");
            appendUnsigned(g_textScratch, sizeof(g_textScratch), group->matchCount);
            appendText(g_textScratch, sizeof(g_textScratch), ")");
            drawText(ctx, 24, kReferencesPanelResultsTop + static_cast<int>((row - firstRow) * kReferencesPanelRowHeight), g_textScratch);
        }
        ++row;
        for (uint32_t matchIndex = 0; matchIndex < group->matchCount; ++matchIndex, ++row) {
            if (row < firstRow || row >= lastRow) continue;
            const ReferenceMatch* match = ReferenceSearchResultMatchAt(&g_referenceSearch, group, matchIndex);
            if (!match) continue;
            const bool selected = g_referencesResultsFocused && groupIndex == g_referencesSelectedGroup &&
                matchIndex == g_referencesSelectedMatch;
            const int y = kReferencesPanelResultsTop + static_cast<int>((row - firstRow) * kReferencesPanelRowHeight);
            if (selected) drawPanel(ctx, { 20, y - 12, 920, kReferencesPanelRowHeight }, 0x34496Au);
            copyText(g_textScratch, sizeof(g_textScratch), "  ");
            appendUnsigned(g_textScratch, sizeof(g_textScratch), match->line);
            appendText(g_textScratch, sizeof(g_textScratch), ":");
            appendUnsigned(g_textScratch, sizeof(g_textScratch), match->column);
            appendText(g_textScratch, sizeof(g_textScratch), "  ");
            appendText(g_textScratch, sizeof(g_textScratch), ReferenceKindName(match->kind));
            appendText(g_textScratch, sizeof(g_textScratch), " [");
            appendText(g_textScratch, sizeof(g_textScratch), ReferenceConfidenceName(match->confidence));
            appendText(g_textScratch, sizeof(g_textScratch), "]  ");
            if (match->previewLeftTruncated) appendText(g_textScratch, sizeof(g_textScratch), "...");
            appendText(g_textScratch, sizeof(g_textScratch), match->previewText);
            if (match->previewRightTruncated) appendText(g_textScratch, sizeof(g_textScratch), "...");
            drawText(ctx, 32, y, g_textScratch);
        }
    }
    if (groups == 0 && operation && !active)
        drawText(ctx, 24, kReferencesPanelResultsTop, "No references found.");
    if (groups == 0 && active)
        drawText(ctx, 24, kReferencesPanelResultsTop, "Scanning active project files...");
    if (groups != 0 && referencesTotalRows() > static_cast<uint32_t>(kReferencesPanelMaxRows))
        drawText(ctx, 720, 177, "Scroll: mouse wheel / Up / Down");
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
    drawText(ctx, 16, 520, "Alt+O: Switch Header / Source");
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

static void drawDefinitionPicker(gx_app_context* ctx) {
    if (!g_definitionPickerOpen) return;
    drawPanel(ctx, { 136, 58, 688, 560 }, 0x2A3852u);
    drawText(ctx, 158, 86, g_relationshipNavigationToDeclaration ? "Go To Declaration" : "Go To Definition");
    copyText(g_textScratch, sizeof(g_textScratch), g_definitionQuery.identifier);
    if (g_definitionQuery.lexicalQualifier[0] != '\0') {
        appendText(g_textScratch, sizeof(g_textScratch), "  (");
        appendText(g_textScratch, sizeof(g_textScratch), g_definitionQuery.lexicalQualifier);
        appendText(g_textScratch, sizeof(g_textScratch), ")");
    }
    drawText(ctx, 158, 108, g_textScratch);
    if (g_definitionResolution.truncated) drawText(ctx, 560, 86, "Truncated");
    const uint32_t rows = 18;
    const uint32_t end = g_definitionScroll + rows < g_definitionResolution.visibleCandidateCount ?
        g_definitionScroll + rows : g_definitionResolution.visibleCandidateCount;
    for (uint32_t row = g_definitionScroll; row < end; ++row) {
        const DefinitionCandidate& candidate = g_definitionResolution.candidates[row];
        const int y = 132 + static_cast<int>(row - g_definitionScroll) * 24;
        if (row == g_definitionSelected) drawPanel(ctx, { 148, y - 14, 664, 22 }, 0x405775u);
        copyText(g_textScratch, sizeof(g_textScratch), SymbolKindPrefix(candidate.symbol.symbol.kind));
        appendText(g_textScratch, sizeof(g_textScratch), " ");
        appendText(g_textScratch, sizeof(g_textScratch), candidate.symbol.symbol.qualifiedName[0] ?
                   candidate.symbol.symbol.qualifiedName : candidate.symbol.symbol.name);
        appendText(g_textScratch, sizeof(g_textScratch), "  ");
        appendText(g_textScratch, sizeof(g_textScratch), candidate.relativePath);
        appendText(g_textScratch, sizeof(g_textScratch), ":");
        appendUnsigned(g_textScratch, sizeof(g_textScratch), candidate.symbol.symbol.location.line);
        appendText(g_textScratch, sizeof(g_textScratch), ":");
        appendUnsigned(g_textScratch, sizeof(g_textScratch), candidate.symbol.symbol.location.column);
        drawText(ctx, 158, y, g_textScratch);
        copyText(g_textScratch, sizeof(g_textScratch), "    ");
        appendText(g_textScratch, sizeof(g_textScratch), SymbolDeclarationRoleName(candidate.symbol.symbol.declarationRole));
        if (candidate.symbol.symbol.signature[0] != '\0') {
            appendText(g_textScratch, sizeof(g_textScratch), "  ");
            appendText(g_textScratch, sizeof(g_textScratch), candidate.symbol.symbol.signature);
        }
        drawText(ctx, 174, y + 12, g_textScratch);
    }
    if (g_definitionStatus[0] != '\0') drawText(ctx, 158, 584, g_definitionStatus);
    drawText(ctx, 158, 604, "Up/Down  PageUp/PageDown  Enter Open  Esc Close");
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
    if (g_includeGraphPanelOpen) refreshIncludeGraphDocument(ctx, document);
}

static void completionSetStatus(const char* text) {
    copyText(g_completionStatus, sizeof(g_completionStatus), text ? text : "");
}

static void signatureSetStatus(const char* text) {
    copyText(g_signatureStatus, sizeof(g_signatureStatus), text ? text : "");
}

static void signatureStatusForError(SignatureErrorCode error) {
    const char* status = SignatureStatusText(error);
    if (status[0] != '\0') signatureSetStatus(status);
    else {
        copyText(g_signatureStatus, sizeof(g_signatureStatus), "Signature Help unavailable: ");
        appendText(g_signatureStatus, sizeof(g_signatureStatus), SignatureErrorName(error));
    }
}

static void ensureSignatureSelectionVisible() {
    if (g_signatureSession.selectedSignatureIndex < g_signatureScroll) g_signatureScroll = g_signatureSession.selectedSignatureIndex;
    if (g_signatureSession.selectedSignatureIndex >= g_signatureScroll + static_cast<uint32_t>(kSignaturePopupMaxRows))
        g_signatureScroll = g_signatureSession.selectedSignatureIndex - static_cast<uint32_t>(kSignaturePopupMaxRows) + 1;
    if (g_signatureScroll > g_signatureSession.candidateCount) g_signatureScroll = g_signatureSession.candidateCount;
}

static void signatureUpdateStatus() {
    if (g_signatureSession.truncated) signatureSetStatus("Signature results truncated.");
    else if (g_signatureSession.context.kind == SignatureContextKind::MethodCallLexical)
        signatureSetStatus("Receiver type unresolved; showing lexical method signatures.");
    else if (g_signatureSession.context.parameterPositionApproximate)
        signatureSetStatus("Parameter position is approximate.");
    else if (SignatureHelpSessionSelected(&g_signatureSession) &&
             SignatureHelpSessionSelected(&g_signatureSession)->parameterParseFailed)
        signatureSetStatus("Parameter highlighting unavailable for this signature.");
    else signatureSetStatus("");
}

static void dismissSignatureHelp(gx_app_context* ctx, const char* reason, bool showStatus) {
    if (!g_signaturePopupOpen && !g_signatureSession.active) return;
    SignatureHelpSessionDismiss(&g_signatureSession);
    g_signaturePopupOpen = false;
    g_signatureScroll = 0;
    if (showStatus && reason) {
        signatureSetStatus(reason);
        copyText(g_textScratch, sizeof(g_textScratch), "GUIDEXOS_DEVELOPER_STUDIO_MARKER signature_dismiss=PASS reason=");
        appendText(g_textScratch, sizeof(g_textScratch), reason);
        logMarker(ctx, g_textScratch);
    } else signatureSetStatus("");
}

static bool openSignatureHelp(gx_app_context* ctx) {
    dismissCompletion(ctx, "signature_help", false);
    Document* document = WorkspaceControllerActiveDocument(&g_controller);
    if (!g_controller.model.open || !g_controller.model.hasProject) {
        signatureStatusForError(SignatureErrorCode::NoProject);
        markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER signature_fail=FAIL reason=", SignatureErrorName(SignatureErrorCode::NoProject));
        return false;
    }
    if (!document || !g_editorFocused) {
        signatureStatusForError(SignatureErrorCode::NoDocument);
        markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER signature_fail=FAIL reason=", SignatureErrorName(SignatureErrorCode::NoDocument));
        return false;
    }
    if (!document->syntax.valid || document->syntax.generation != document->buffer.generation) DocumentUpdateSyntax(document);
    if (g_signatureSession.sessionId == 0) g_signatureSession.sessionId = 1;
    else ++g_signatureSession.sessionId;
    SignatureErrorCode error = SignatureErrorCode::None;
    if (!SignatureHelpBuildSession(&g_signatureSession, *document,
                                   SignatureProjectId(g_controller.model.project.projectId),
                                   g_controller.model.projectGeneration, &g_symbolDatabase, &error)) {
        signatureStatusForError(error);
        markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER signature_fail=FAIL reason=", SignatureErrorName(error));
        g_signaturePopupOpen = false;
        return false;
    }
    g_signatureScroll = 0;
    g_signaturePopupOpen = g_signatureSession.active;
    copyText(g_textScratch, sizeof(g_textScratch), "GUIDEXOS_DEVELOPER_STUDIO_MARKER signature_begin=PASS");
    logMarker(ctx, g_textScratch);
    copyText(g_textScratch, sizeof(g_textScratch), "GUIDEXOS_DEVELOPER_STUDIO_MARKER signature_context=PASS callable=");
    appendText(g_textScratch, sizeof(g_textScratch), g_signatureSession.context.callableName);
    appendText(g_textScratch, sizeof(g_textScratch), " argument=");
    appendUnsigned(g_textScratch, sizeof(g_textScratch), g_signatureSession.context.activeArgumentIndex);
    logMarker(ctx, g_textScratch);
    if (g_signatureSession.context.hasExplicitQualifier) {
        copyText(g_textScratch, sizeof(g_textScratch), "GUIDEXOS_DEVELOPER_STUDIO_MARKER signature_context=QUALIFIED qualifier=");
        appendText(g_textScratch, sizeof(g_textScratch), g_signatureSession.context.explicitQualifier);
        logMarker(ctx, g_textScratch);
    }
    if (g_signatureSession.context.kind == SignatureContextKind::MethodCallLexical)
        logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER signature_context=MEMBER_LEXICAL");
    copyText(g_textScratch, sizeof(g_textScratch), "GUIDEXOS_DEVELOPER_STUDIO_MARKER signature_candidates=PASS total=");
    appendUnsigned(g_textScratch, sizeof(g_textScratch), g_signatureSession.collectedCount);
    appendText(g_textScratch, sizeof(g_textScratch), " retained=");
    appendUnsigned(g_textScratch, sizeof(g_textScratch), g_signatureSession.candidateCount);
    logMarker(ctx, g_textScratch);
    if (g_signatureSession.truncated) {
        copyText(g_textScratch, sizeof(g_textScratch), "GUIDEXOS_DEVELOPER_STUDIO_MARKER signature_candidates=TRUNCATED retained=");
        appendUnsigned(g_textScratch, sizeof(g_textScratch), g_signatureSession.candidateCount);
        logMarker(ctx, g_textScratch);
    }
    if (g_signatureSession.activeParameterIndex != 0xFFFFFFFFu) {
        copyText(g_textScratch, sizeof(g_textScratch), "GUIDEXOS_DEVELOPER_STUDIO_MARKER signature_parameter=");
        appendText(g_textScratch, sizeof(g_textScratch), g_signatureSession.context.parameterPositionApproximate ? "APPROXIMATE index=" : "PASS index=");
        appendUnsigned(g_textScratch, sizeof(g_textScratch), g_signatureSession.activeParameterIndex);
        logMarker(ctx, g_textScratch);
    }
    signatureUpdateStatus();
    if (error == SignatureErrorCode::ParseApproximate) signatureSetStatus("Parameter position is approximate.");
    return true;
}

static void typeSetStatus(const char* text) {
    copyText(g_typeStatus, sizeof(g_typeStatus), text ? text : "");
}

static bool ensureTypeDatabase(Document* document) {
    if (!document || !g_controller.model.hasProject) return false;
    const uint64_t projectGeneration = g_controller.model.projectGeneration;
    const uint64_t symbolsGeneration = g_symbolDatabase.symbolDatabaseGeneration;
    if (TypeDatabaseIsCurrent(&g_typeDatabase, projectGeneration, symbolsGeneration)) return true;
    // An editor mutation updates only the active document and advances the
    // symbol generation. Refresh that document in-place so member completion
    // stays bounded during typing; a project-generation change still takes
    // the normal full-project path below.
    if (g_typeDatabase.current && g_typeDatabase.projectGeneration == projectGeneration &&
        TypeDatabaseIndexDocument(&g_typeDatabase, g_controller.model.project.rootPath,
                                  document->path, document->documentId, document->buffer.generation,
                                  projectGeneration, document->buffer.data, document->buffer.length)) {
        g_typeDatabase.symbolDatabaseGeneration = symbolsGeneration;
        g_typeDatabase.current = true;
        return true;
    }
    const bool indexed = TypeDatabaseIndexProject(&g_typeDatabase, g_controller.fileSystem,
                                                  g_controller.model.project.rootPath,
                                                  g_controller.model.documents, kMaxOpenDocuments,
                                                  projectGeneration, &g_symbolDatabase);
    if (indexed && TypeDatabaseIsCurrent(&g_typeDatabase, projectGeneration, symbolsGeneration)) return true;
    if (TypeDatabaseIndexDocument(&g_typeDatabase, g_controller.model.project.rootPath,
                                  document->path, document->documentId, document->buffer.generation,
                                  projectGeneration, document->buffer.data, document->buffer.length)) {
        g_typeDatabase.symbolDatabaseGeneration = symbolsGeneration;
        g_typeDatabase.current = true;
        return true;
    }
    return false;
}

static void dismissTypeInfo(gx_app_context* ctx, const char* reason, bool showStatus) {
    if (!g_typePopupOpen) return;
    g_typePopupOpen = false;
    g_typeInspection = TypeInspection();
    if (showStatus && reason) {
        typeSetStatus(reason);
        copyText(g_textScratch, sizeof(g_textScratch), "GUIDEXOS_DEVELOPER_STUDIO_MARKER type_info_dismiss=PASS reason=");
        appendText(g_textScratch, sizeof(g_textScratch), reason);
        logMarker(ctx, g_textScratch);
    } else typeSetStatus("");
}

static bool openTypeInfo(gx_app_context* ctx) {
    dismissCompletion(ctx, "type_info", false);
    dismissSignatureHelp(ctx, "type_info", false);
    Document* document = WorkspaceControllerActiveDocument(&g_controller);
    if (!g_controller.model.open || !g_controller.model.hasProject) {
        typeSetStatus("Open a project before using Quick Type Info.");
        markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER type_info=FAIL", "no_project");
        return false;
    }
    if (!document || !g_editorFocused) {
        typeSetStatus("Focus the source editor before using Quick Type Info.");
        markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER type_info=FAIL", "no_document");
        return false;
    }
    if (!ensureTypeDatabase(document)) {
        typeSetStatus("Type information is unavailable while the project index is stale.");
        markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER type_info=FAIL", "index_stale");
        return false;
    }
    TypeInspection inspection = {};
    const bool resolved = TypeDatabaseInspectAt(&g_typeDatabase, *document,
                                               g_controller.model.projectGeneration,
                                               document->buffer.caret, &inspection);
    g_typeInspection = inspection;
    if (inspection.identifier[0] == '\0') {
        typeSetStatus(resolved ? "No identifier under the caret." : "Type information unavailable.");
        g_typePopupOpen = false;
        return false;
    }
    g_typePopupOpen = true;
    typeSetStatus("");
    copyText(g_textScratch, sizeof(g_textScratch), "GUIDEXOS_DEVELOPER_STUDIO_MARKER type_info=PASS state=");
    appendText(g_textScratch, sizeof(g_textScratch), TypeInspectionStateName(inspection.state));
    appendText(g_textScratch, sizeof(g_textScratch), " identifier=");
    appendText(g_textScratch, sizeof(g_textScratch), inspection.identifier);
    logMarker(ctx, g_textScratch);
    if (TypeDatabaseIsTruncated(&g_typeDatabase))
        logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER type_info=TRUNCATED");
    return true;
}

static bool handleTypeInfoKey(gx_app_context* ctx, int keyCode, int action, int modifiers) {
    if (!g_typePopupOpen || action != GX_KEY_ACTION_DOWN) return false;
    if (keyCode == 27) { dismissTypeInfo(ctx, "escape", false); return true; }
    if ((modifiers & GX_KEY_MOD_CTRL) && (modifiers & GX_KEY_MOD_ALT) &&
        (keyCode == 84 || keyCode == 116)) { openTypeInfo(ctx); return true; }
    dismissTypeInfo(ctx, "input", false);
    return false;
}

static bool typePopupBounds(gx_rect* output) {
    if (!output || !g_typePopupOpen || g_typeInspection.identifier[0] == '\0') return false;
    Document* document = WorkspaceControllerActiveDocument(&g_controller);
    if (!document) return false;
    const uint32_t line = activeLine(document->buffer);
    const uint32_t column = activeColumn(document->buffer, line);
    const uint32_t visibleColumn = column > g_editorScrollColumn ? column - g_editorScrollColumn : 0;
    const uint32_t visibleLine = line > g_editorScrollLine ? line - g_editorScrollLine : 0;
    const int caretX = kEditorTextX + static_cast<int>(visibleColumn * 8u);
    const int caretY = kEditorTop + static_cast<int>(visibleLine * kEditorLineHeight);
    const int rows = g_typeInspection.state == TypeInspectionState::Exact ||
        g_typeInspection.state == TypeInspectionState::Conservative ? 7 : 2;
    const int height = 30 + rows * kTypePopupRowHeight + 28;
    int x = caretX;
    int y = caretY + kEditorLineHeight;
    if (x + kTypePopupWidth > kEditorRect.x + kEditorRect.width) x = kEditorRect.x + kEditorRect.width - kTypePopupWidth;
    if (x < kEditorRect.x) x = kEditorRect.x;
    if (y + height > kEditorRect.y + kEditorRect.height) y = caretY - height;
    if (y < kEditorRect.y) y = kEditorRect.y;
    *output = { x, y, kTypePopupWidth, height };
    return true;
}

static void typeLine(char* output, uint32_t capacity, const char* label, const char* value) {
    output[0] = '\0';
    appendText(output, capacity, label);
    appendText(output, capacity, value ? value : "");
}

static void drawTypePopup(gx_app_context* ctx) {
    if (!ctx) return;
    gx_rect bounds = {};
    if (!typePopupBounds(&bounds)) return;
    drawPanel(ctx, bounds, 0x202A36u);
    copyText(g_typeDisplayScratch, sizeof(g_typeDisplayScratch), "QUICK TYPE INFO  ");
    appendText(g_typeDisplayScratch, sizeof(g_typeDisplayScratch), g_typeInspection.identifier);
    drawText(ctx, bounds.x + 8, bounds.y + 15, g_typeDisplayScratch);
    uint32_t row = 0;
    if (g_typeInspection.state == TypeInspectionState::Ambiguous ||
        g_typeInspection.state == TypeInspectionState::Unknown ||
        g_typeInspection.state == TypeInspectionState::Stale) {
        drawText(ctx, bounds.x + 8, bounds.y + 30 + static_cast<int>(row++) * kTypePopupRowHeight,
                 g_typeInspection.state == TypeInspectionState::Ambiguous ? "Type unavailable" : "Type information unavailable.");
        drawText(ctx, bounds.x + 8, bounds.y + 30 + static_cast<int>(row++) * kTypePopupRowHeight,
                 g_typeInspection.detail[0] ? g_typeInspection.detail :
                 (g_typeInspection.state == TypeInspectionState::Ambiguous ? "Multiple declarations match." : "No truthful bounded type determination.") );
    } else {
        typeLine(g_typeDisplayScratch, sizeof(g_typeDisplayScratch), "Type: ",
                 g_typeInspection.displayType[0] ? g_typeInspection.displayType : g_typeInspection.type.spelling);
        drawText(ctx, bounds.x + 8, bounds.y + 30 + static_cast<int>(row++) * kTypePopupRowHeight, g_typeDisplayScratch);
        typeLine(g_typeDisplayScratch, sizeof(g_typeDisplayScratch), "Declared in: ", g_typeInspection.type.declarationLocation.relativePath);
        appendText(g_typeDisplayScratch, sizeof(g_typeDisplayScratch), ":");
        appendUnsigned(g_typeDisplayScratch, sizeof(g_typeDisplayScratch), g_typeInspection.type.declarationLocation.line);
        drawText(ctx, bounds.x + 8, bounds.y + 30 + static_cast<int>(row++) * kTypePopupRowHeight, g_typeDisplayScratch);
        typeLine(g_typeDisplayScratch, sizeof(g_typeDisplayScratch), "Kind: ", TypeDeclarationKindName(g_typeInspection.declarationKind));
        drawText(ctx, bounds.x + 8, bounds.y + 30 + static_cast<int>(row++) * kTypePopupRowHeight, g_typeDisplayScratch);
        typeLine(g_typeDisplayScratch, sizeof(g_typeDisplayScratch), "Type source: ", TypeSourceName(g_typeInspection.type.source));
        drawText(ctx, bounds.x + 8, bounds.y + 30 + static_cast<int>(row++) * kTypePopupRowHeight, g_typeDisplayScratch);
        if (g_typeInspection.type.aliasName[0] != '\0' && g_typeInspection.type.resolvedAlias[0] != '\0') {
            typeLine(g_typeDisplayScratch, sizeof(g_typeDisplayScratch), "Alias: ", g_typeInspection.type.aliasName);
            appendText(g_typeDisplayScratch, sizeof(g_typeDisplayScratch), " -> ");
            appendText(g_typeDisplayScratch, sizeof(g_typeDisplayScratch), g_typeInspection.type.resolvedAlias);
            drawText(ctx, bounds.x + 8, bounds.y + 30 + static_cast<int>(row++) * kTypePopupRowHeight, g_typeDisplayScratch);
        } else if (g_typeInspection.type.source == guidexos::developer_studio::TypeSource::FunctionReturnInference) {
            drawText(ctx, bounds.x + 8, bounds.y + 30 + static_cast<int>(row++) * kTypePopupRowHeight, "Inferred from: function return type");
        } else {
            drawText(ctx, bounds.x + 8, bounds.y + 30 + static_cast<int>(row++) * kTypePopupRowHeight, "Type source: direct declaration");
        }
        typeLine(g_typeDisplayScratch, sizeof(g_typeDisplayScratch), "Confidence: ", TypeInspectionStateName(g_typeInspection.state));
        drawText(ctx, bounds.x + 8, bounds.y + 30 + static_cast<int>(row++) * kTypePopupRowHeight, g_typeDisplayScratch);
        if (g_typeInspection.truncated || g_typeInspection.type.truncated)
            drawText(ctx, bounds.x + 8, bounds.y + 30 + static_cast<int>(row++) * kTypePopupRowHeight, "Bounded result truncated.");
    }
    drawText(ctx, bounds.x + 8, bounds.y + bounds.height - 10, "Ctrl+Alt+T Refresh  Esc Close");
}

static bool refreshSignatureHelp(gx_app_context* ctx) {
    if (!g_signaturePopupOpen) return false;
    Document* document = WorkspaceControllerActiveDocument(&g_controller);
    if (!document) {
        signatureStatusForError(SignatureErrorCode::NoDocument);
        dismissSignatureHelp(ctx, "document_missing", true);
        return false;
    }
    SignatureErrorCode error = SignatureErrorCode::None;
    if (!SignatureHelpSessionRefresh(&g_signatureSession, *document,
                                     SignatureProjectId(g_controller.model.project.projectId),
                                     g_controller.model.projectGeneration, &g_symbolDatabase, &error)) {
        signatureStatusForError(error == SignatureErrorCode::None ? SignatureErrorCode::SessionStale : error);
        dismissSignatureHelp(ctx, "stale", true);
        return false;
    }
    g_signaturePopupOpen = g_signatureSession.active;
    ensureSignatureSelectionVisible();
    signatureUpdateStatus();
    copyText(g_textScratch, sizeof(g_textScratch), "GUIDEXOS_DEVELOPER_STUDIO_MARKER signature_refresh=PASS argument=");
    appendUnsigned(g_textScratch, sizeof(g_textScratch), g_signatureSession.context.activeArgumentIndex);
    logMarker(ctx, g_textScratch);
    return g_signaturePopupOpen;
}

static bool handleSignatureKey(gx_app_context* ctx, int keyCode, int action, int modifiers) {
    if (!g_signaturePopupOpen || action != GX_KEY_ACTION_DOWN) return false;
    if (keyCode == 27) { dismissSignatureHelp(ctx, "escape", false); return true; }
    if (keyCode == GX_KEY_UP) { SignatureHelpSessionMove(&g_signatureSession, -1); ensureSignatureSelectionVisible(); signatureUpdateStatus(); return true; }
    if (keyCode == GX_KEY_DOWN) { SignatureHelpSessionMove(&g_signatureSession, 1); ensureSignatureSelectionVisible(); signatureUpdateStatus(); return true; }
    if (keyCode == 33) { SignatureHelpSessionPage(&g_signatureSession, -1); ensureSignatureSelectionVisible(); signatureUpdateStatus(); return true; }
    if (keyCode == 34) { SignatureHelpSessionPage(&g_signatureSession, 1); ensureSignatureSelectionVisible(); signatureUpdateStatus(); return true; }
    if (keyCode == 36) { SignatureHelpSessionHome(&g_signatureSession); ensureSignatureSelectionVisible(); signatureUpdateStatus(); return true; }
    if (keyCode == 35) { SignatureHelpSessionEnd(&g_signatureSession); ensureSignatureSelectionVisible(); signatureUpdateStatus(); return true; }
    if (keyCode == GX_KEY_LEFT || keyCode == GX_KEY_RIGHT) { dismissSignatureHelp(ctx, "caret_moved", false); return false; }
    if (keyCode == 46 && !(modifiers & GX_KEY_MOD_CTRL)) { dismissSignatureHelp(ctx, "caret_moved", false); return false; }
    return false;
}

static bool signaturePopupBounds(gx_rect* output) {
    if (!output || !g_signaturePopupOpen || !g_signatureSession.active) return false;
    Document* document = WorkspaceControllerActiveDocument(&g_controller);
    if (!document) return false;
    const uint32_t line = activeLine(document->buffer);
    const uint32_t column = activeColumn(document->buffer, line);
    const uint32_t visibleColumn = column > g_editorScrollColumn ? column - g_editorScrollColumn : 0;
    const uint32_t visibleLine = line > g_editorScrollLine ? line - g_editorScrollLine : 0;
    const int caretX = kEditorTextX + static_cast<int>(visibleColumn * 8u);
    const int caretY = kEditorTop + static_cast<int>(visibleLine * kEditorLineHeight);
    const uint32_t remaining = g_signatureSession.candidateCount > g_signatureScroll ?
        g_signatureSession.candidateCount - g_signatureScroll : 0;
    const uint32_t rows = remaining < static_cast<uint32_t>(kSignaturePopupMaxRows) ? remaining : static_cast<uint32_t>(kSignaturePopupMaxRows);
    if (rows == 0) return false;
    const int height = 30 + static_cast<int>(rows) * kSignaturePopupRowHeight + 28;
    int x = caretX;
    int y = caretY + kEditorLineHeight;
    if (x + kSignaturePopupWidth > kEditorRect.x + kEditorRect.width) x = kEditorRect.x + kEditorRect.width - kSignaturePopupWidth;
    if (x < kEditorRect.x) x = kEditorRect.x;
    if (y + height > kEditorRect.y + kEditorRect.height) y = caretY - height;
    if (y < kEditorRect.y) y = kEditorRect.y;
    *output = { x, y, kSignaturePopupWidth, height };
    return true;
}

static void drawSignaturePopup(gx_app_context* ctx) {
    if (!ctx) return;
    gx_rect bounds = {};
    if (!signaturePopupBounds(&bounds)) return;
    const uint32_t rows = (static_cast<uint32_t>(bounds.height) - 58u) / static_cast<uint32_t>(kSignaturePopupRowHeight);
    drawPanel(ctx, bounds, 0x202A36u);
    copyText(g_textScratch, sizeof(g_textScratch), "SIGNATURE HELP  ");
    appendUnsigned(g_textScratch, sizeof(g_textScratch), g_signatureSession.selectedSignatureIndex + 1);
    appendText(g_textScratch, sizeof(g_textScratch), " of ");
    appendUnsigned(g_textScratch, sizeof(g_textScratch), g_signatureSession.candidateCount);
    appendText(g_textScratch, sizeof(g_textScratch), " overloads");
    drawText(ctx, bounds.x + 8, bounds.y + 15, g_textScratch);
    for (uint32_t row = 0; row < rows; ++row) {
        const uint32_t index = g_signatureScroll + row;
        if (index >= g_signatureSession.candidateCount) break;
        const SignatureCandidate& candidate = g_signatureSession.candidates[index];
        const int rowY = bounds.y + 30 + static_cast<int>(row) * kSignaturePopupRowHeight;
        if (index == g_signatureSession.selectedSignatureIndex) drawPanel(ctx, { bounds.x + 2, rowY - 15, kSignaturePopupWidth - 4, kSignaturePopupRowHeight }, 0x34496Au);
        drawText(ctx, bounds.x + 8, rowY, candidate.displaySignature);
        drawText(ctx, bounds.x + 8, rowY + 13, candidate.detailText);
    }
    const SignatureCandidate* selected = SignatureHelpSessionSelected(&g_signatureSession);
    const guidexos::developer_studio::SignatureParameter* parameter = SignatureHelpSessionActiveParameter(&g_signatureSession);
    if (parameter) {
        copyText(g_textScratch, sizeof(g_textScratch), "Parameter ");
        appendUnsigned(g_textScratch, sizeof(g_textScratch), g_signatureSession.activeParameterIndex + 1);
        appendText(g_textScratch, sizeof(g_textScratch), " of ");
        appendUnsigned(g_textScratch, sizeof(g_textScratch), selected ? selected->parameterCount : 0);
        appendText(g_textScratch, sizeof(g_textScratch), ": ");
        appendText(g_textScratch, sizeof(g_textScratch), parameter->displayText);
        drawText(ctx, bounds.x + 8, bounds.y + bounds.height - 10, g_textScratch);
    } else if (selected && selected->parameterParseFailed) {
        drawText(ctx, bounds.x + 8, bounds.y + bounds.height - 10, "Parameter highlighting unavailable for this signature.");
    } else if (g_signatureSession.context.parameterPositionApproximate) {
        drawText(ctx, bounds.x + 8, bounds.y + bounds.height - 10, "Parameter position is approximate.");
    }
}

static void includeGraphSetStatus(const char* text) {
    copyText(g_includeGraphStatus, sizeof(g_includeGraphStatus), text ? text : "");
}

static uint32_t captureDirtyIncludeGraphDocuments() {
    uint32_t count = 0;
    for (uint32_t i = 0; i < kMaxOpenDocuments && count < kMaxOpenDocuments; ++i) {
        const Document& document = g_controller.model.documents[i];
        if (!document.used || !document.buffer.dirty || document.buffer.length > kMaxEditorBytes) continue;
        IncludeGraphDocumentSnapshot& snapshot = g_includeGraphOperation.dirtyDocuments[count];
        if (!copyProjectRelativePath(g_controller.model, document.path, snapshot.relativePath, sizeof(snapshot.relativePath))) continue;
        snapshot.length = document.buffer.length;
        snapshot.documentId = document.documentId;
        snapshot.documentGeneration = document.buffer.generation;
        for (uint32_t j = 0; j < snapshot.length; ++j) snapshot.data[j] = document.buffer.data[j];
        snapshot.data[snapshot.length] = '\0';
        ++count;
    }
    return count;
}

static void fillIncludeGraphRequest(IncludeGraphRequest* request) {
    if (!request) return;
    *request = IncludeGraphRequest();
    if (!g_controller.model.hasProject) return;
    copyText(request->projectId, sizeof(request->projectId), g_controller.model.project.projectId);
    request->projectGeneration = g_controller.model.projectGeneration;
    copyText(request->rootPath, sizeof(request->rootPath), g_controller.model.rootPath);
    if (g_controller.model.project.sourceRoot[0] != '\0') {
        copyText(request->includeRoots[0], sizeof(request->includeRoots[0]), g_controller.model.project.sourceRoot);
        request->includeRootCount = 1;
    }
    // Version 1 project metadata has no separate include-root list. The
    // project root is an intentional final fallback for paths such as
    // "src/header.h", never an unrestricted host search.
    request->allowProjectRoot = true;
    request->fileSystem = g_controller.fileSystem;
    request->dirtyDocuments = g_includeGraphOperation.dirtyDocuments;
    request->dirtyDocumentCount = captureDirtyIncludeGraphDocuments();
}

static bool startIncludeGraph(gx_app_context* ctx) {
    if (!g_controller.model.open || !g_controller.model.hasProject) {
        includeGraphSetStatus("Open a project before using Include Graph.");
        markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER include_graph_begin=FAIL", IncludeGraphErrorName(IncludeGraphErrorCode::NoProject));
        return false;
    }
    IncludeGraphInit(&g_includeGraphBuilding, &g_includeGraphBuildingStorage,
                     g_controller.model.project.projectId, g_controller.model.projectGeneration);
    IncludeGraphRequest request = {};
    fillIncludeGraphRequest(&request);
    uint64_t operationId = 0;
    IncludeGraphErrorCode error = IncludeGraphErrorCode::None;
    if (!IncludeGraphStart(&g_includeGraphOperation, &g_includeGraphBuilding, request,
                           gx_get_ticks_ms(ctx), &operationId, &error)) {
        includeGraphSetStatus(IncludeGraphStatusText(error));
        markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER include_graph_begin=FAIL", IncludeGraphErrorName(error));
        return false;
    }
    g_includeGraphOperationId = operationId;
    g_includeGraphTerminalReported = false;
    g_includeGraphPanelOpen = true;
    g_includeGraphResultsFocused = false;
    g_includeGraphScroll = 0;
    g_includeGraphSelectedRow = 0;
    g_editorFocused = false;
    includeGraphSetStatus("Building Include Graph...");
    writeStudioOutput("Include Graph build started");
    logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER include_graph_begin=PASS");
    return true;
}

static void pollIncludeGraph(gx_app_context* ctx) {
    if (g_includeGraphOperationId == 0) return;
    if (IncludeGraphIsActive(&g_includeGraphOperation) &&
        (!g_controller.model.open || !g_controller.model.hasProject ||
         g_includeGraphOperation.projectGeneration != g_controller.model.projectGeneration)) {
        IncludeGraphCancel(&g_includeGraphOperation, g_includeGraphOperationId);
    }
    IncludeGraphPoll(&g_includeGraphOperation, g_includeGraphOperationId, 8, gx_get_ticks_ms(ctx));
    const IncludeGraphBuildOperation* operation = IncludeGraphOperationInfo(&g_includeGraphOperation);
    if (!operation || IncludeGraphIsActive(operation) || g_includeGraphTerminalReported) {
        if (operation && IncludeGraphIsActive(operation)) {
            copyText(g_includeGraphStatus, sizeof(g_includeGraphStatus), IncludeGraphBuildStateName(operation->state));
            appendText(g_includeGraphStatus, sizeof(g_includeGraphStatus), "  files=");
            appendUnsigned(g_includeGraphStatus, sizeof(g_includeGraphStatus), static_cast<uint32_t>(operation->filesScanned));
            appendText(g_includeGraphStatus, sizeof(g_includeGraphStatus), " directives=");
            appendUnsigned(g_includeGraphStatus, sizeof(g_includeGraphStatus), static_cast<uint32_t>(operation->directivesFound));
        }
        return;
    }
    g_includeGraphTerminalReported = true;
    if (operation->state == IncludeGraphBuildState::Completed) {
        IncludeGraph oldGraph = g_includeGraph;
        g_includeGraph = g_includeGraphBuilding;
        g_includeGraphBuilding = oldGraph;
        includeGraphSetStatus(g_includeGraph.truncated ? "Include Graph complete (truncated)." : "Include Graph complete.");
        copyText(g_textScratch, sizeof(g_textScratch), "GUIDEXOS_DEVELOPER_STUDIO_MARKER include_graph_enumeration=PASS files=");
        appendUnsigned(g_textScratch, sizeof(g_textScratch), static_cast<uint32_t>(operation->filesEnumerated));
        logMarker(ctx, g_textScratch);
        copyText(g_textScratch, sizeof(g_textScratch), "GUIDEXOS_DEVELOPER_STUDIO_MARKER include_graph_scan=PASS files=");
        appendUnsigned(g_textScratch, sizeof(g_textScratch), static_cast<uint32_t>(operation->filesScanned));
        appendText(g_textScratch, sizeof(g_textScratch), " directives=");
        appendUnsigned(g_textScratch, sizeof(g_textScratch), static_cast<uint32_t>(operation->directivesFound));
        logMarker(ctx, g_textScratch);
        copyText(g_textScratch, sizeof(g_textScratch), "GUIDEXOS_DEVELOPER_STUDIO_MARKER include_graph_resolution=PASS resolved=");
        appendUnsigned(g_textScratch, sizeof(g_textScratch), static_cast<uint32_t>(operation->edgesResolved));
        appendText(g_textScratch, sizeof(g_textScratch), " unresolved=");
        appendUnsigned(g_textScratch, sizeof(g_textScratch), static_cast<uint32_t>(operation->unresolvedEdges));
        logMarker(ctx, g_textScratch);
        logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER include_graph_reverse_edges=PASS");
        logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER include_graph_cycles=PASS");
        writeStudioOutput("Include Graph build completed");
    } else if (operation->state == IncludeGraphBuildState::Cancelled) {
        includeGraphSetStatus("Include Graph build cancelled; last completed graph retained.");
        writeStudioOutput("Include Graph build cancelled");
        logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER include_graph_cancelled=PASS");
    } else {
        copyText(g_includeGraphStatus, sizeof(g_includeGraphStatus), "Include Graph build failed: ");
        appendText(g_includeGraphStatus, sizeof(g_includeGraphStatus), IncludeGraphErrorName(operation->error));
        markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER include_graph_build=FAIL", IncludeGraphErrorName(operation->error));
    }
}

static bool includeGraphActivePath(char* output, uint32_t outputSize) {
    Document* document = WorkspaceControllerActiveDocument(&g_controller);
    return document && copyProjectRelativePath(g_controller.model, document->path, output, outputSize);
}

static uint32_t includeGraphRowCount() {
    if (!IncludeGraphIsCurrent(&g_includeGraph, g_controller.model.hasProject ? g_controller.model.project.projectId : "",
                               g_controller.model.projectGeneration)) return 0;
    char activePath[kMaxPathBytes] = {};
    if (!includeGraphActivePath(activePath, sizeof(activePath))) return 0;
    if (g_includeGraphMode == 4) return g_includeGraph.cycleCount;
    if (g_includeGraphMode == 5) {
        uint32_t count = 0;
        for (uint32_t i = 0; i < g_includeGraph.edgeCount; ++i)
            if (g_includeGraph.edges[i].resolution.state != IncludeResolutionState::Resolved) ++count;
        return count;
    }
    const uint32_t nodeIndex = [&]() -> uint32_t { for (uint32_t i = 0; i < g_includeGraph.nodeCount; ++i) if (searchTextEqual(g_includeGraph.nodes[i].relativePath, activePath)) return i; return kIncludeGraphMaxNodes; }();
    if (nodeIndex >= g_includeGraph.nodeCount) return 0;
    if (g_includeGraphMode == 0) return g_includeGraph.nodes[nodeIndex].outgoingEdgeCount;
    if (g_includeGraphMode == 1) return g_includeGraph.nodes[nodeIndex].incomingEdgeCount;
    g_includeTraversal = { g_includeTraversalNodes, g_includeTraversalDepths, g_includeTraversalParents, 0,
                           kIncludeGraphMaxNodes, 0, 0, false };
    IncludeGraphBuildTraversal(&g_includeGraph, activePath,
        g_includeGraphMode == 2 ? IncludeGraphTraversalDirection::Includes : IncludeGraphTraversalDirection::IncludedBy,
        true, &g_includeTraversal);
    return g_includeTraversal.nodeCount;
}

static const IncludeEdge* includeGraphEdgeForRow(uint32_t row, uint32_t* outIndex) {
    if (outIndex) *outIndex = 0;
    char activePath[kMaxPathBytes] = {};
    if (!includeGraphActivePath(activePath, sizeof(activePath))) return nullptr;
    uint32_t nodeIndex = kIncludeGraphMaxNodes;
    for (uint32_t i = 0; i < g_includeGraph.nodeCount; ++i)
        if (searchTextEqual(g_includeGraph.nodes[i].relativePath, activePath)) { nodeIndex = i; break; }
    if (g_includeGraphMode == 4) {
        if (row >= g_includeGraph.cycleCount) return nullptr;
        const auto& cycle = g_includeGraph.cycles[row];
        if (cycle.edgeCount == 0) return nullptr;
        const uint32_t edgeIndex = g_includeGraph.cycleEdges[cycle.edgeOffset];
        if (outIndex) *outIndex = edgeIndex;
        return IncludeGraphEdgeAt(&g_includeGraph, edgeIndex);
    }
    if (g_includeGraphMode == 5) {
        uint32_t found = 0;
        for (uint32_t i = 0; i < g_includeGraph.edgeCount; ++i) {
            if (g_includeGraph.edges[i].resolution.state == IncludeResolutionState::Resolved) continue;
            if (found++ == row) { if (outIndex) *outIndex = i; return &g_includeGraph.edges[i]; }
        }
        return nullptr;
    }
    if (g_includeGraphMode == 2 || g_includeGraphMode == 3) {
        if (row >= g_includeTraversal.nodeCount) return nullptr;
        const uint32_t targetNode = g_includeTraversal.nodeIndices[row];
        for (uint32_t i = 0; i < g_includeGraph.edgeCount; ++i) {
            const auto& edge = g_includeGraph.edges[i];
            if (edge.resolution.state != IncludeResolutionState::Resolved) continue;
            const uint32_t source = [&]() -> uint32_t { for (uint32_t n = 0; n < g_includeGraph.nodeCount; ++n) if (searchTextEqual(g_includeGraph.nodes[n].relativePath, edge.sourceRelativePath)) return n; return kIncludeGraphMaxNodes; }();
            const uint32_t target = [&]() -> uint32_t { for (uint32_t n = 0; n < g_includeGraph.nodeCount; ++n) if (searchTextEqual(g_includeGraph.nodes[n].relativePath, edge.targetRelativePath)) return n; return kIncludeGraphMaxNodes; }();
            if ((g_includeGraphMode == 2 && target == targetNode) || (g_includeGraphMode == 3 && source == targetNode)) {
                if (outIndex) *outIndex = i;
                return &edge;
            }
        }
        return nullptr;
    }
    if (nodeIndex >= g_includeGraph.nodeCount) return nullptr;
    const IncludeNode& node = g_includeGraph.nodes[nodeIndex];
    if (g_includeGraphMode == 0 && row < node.outgoingEdgeCount) {
        const uint32_t edgeIndex = g_includeGraph.outgoingEdgeIndices[node.outgoingEdgeOffset + row];
        if (outIndex) *outIndex = edgeIndex;
        return &g_includeGraph.edges[edgeIndex];
    }
    if (g_includeGraphMode == 1 && row < node.incomingEdgeCount) {
        const uint32_t edgeIndex = g_includeGraph.incomingEdgeIndices[node.incomingEdgeOffset + row];
        if (outIndex) *outIndex = edgeIndex;
        return &g_includeGraph.edges[edgeIndex];
    }
    return nullptr;
}

static bool activateIncludeEdge(gx_app_context* ctx, uint32_t edgeIndex) {
    if (!IncludeGraphIsCurrent(&g_includeGraph, g_controller.model.hasProject ? g_controller.model.project.projectId : "",
                               g_controller.model.projectGeneration) || edgeIndex >= g_includeGraph.edgeCount) {
        includeGraphSetStatus("Include Graph result is stale.");
        return false;
    }
    const auto& edge = g_includeGraph.edges[edgeIndex];
    Document* origin = WorkspaceControllerActiveDocument(&g_controller);
    if (!origin || !captureNavigationLocation(*origin, &g_includeNavigationOrigin)) return false;
    g_includeNavigationOriginValid = true;
    const bool outgoing = g_includeGraphMode == 0 || g_includeGraphMode == 2;
    const char* path = outgoing && edge.resolution.state == IncludeResolutionState::Resolved ? edge.targetRelativePath : edge.sourceRelativePath;
    const uint32_t line = outgoing && edge.resolution.state == IncludeResolutionState::Resolved ? 1 : edge.directive.line;
    const uint32_t column = outgoing && edge.resolution.state == IncludeResolutionState::Resolved ? 1 : edge.directive.column;
    if (!path || path[0] == '\0') {
        includeGraphSetStatus(IncludeResolutionStateName(edge.resolution.state));
        g_includeNavigationOriginValid = false;
        return false;
    }
    uint32_t documentIndex = kMaxOpenDocuments;
    OutputErrorCode error = OutputErrorCode::None;
    if (!WorkspaceControllerOpenDocumentAtLocation(&g_controller, g_controller.model.project.projectId,
                                                   path, line, column, &documentIndex, &error)) {
        includeGraphSetStatus("Unable to open Include Graph location.");
        g_includeNavigationOriginValid = false;
        markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER include_graph_activation=FAIL", OutputErrorName(error));
        return false;
    }
    NavigationHistoryPush(&g_navigationHistory, g_includeNavigationOrigin);
    g_includeNavigationOriginValid = false;
    g_includeGraphPanelOpen = false;
    g_includeTargetPickerOpen = false;
    g_editorFocused = true;
    g_outputFocused = false;
    keepCaretVisible(WorkspaceControllerActiveDocument(&g_controller));
    logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER include_graph_activation=PASS");
    return true;
}

static bool activateIncludeTargetCandidate(gx_app_context* ctx, uint32_t selected) {
    if (!g_includeTargetPickerOpen || g_includeTargetEdgeIndex >= g_includeGraph.edgeCount) return false;
    const auto& edge = g_includeGraph.edges[g_includeTargetEdgeIndex];
    const char* path = IncludeGraphCandidateAt(&g_includeGraph, edge.resolution, selected);
    if (!path || !g_includeNavigationOriginValid) return false;
    uint32_t documentIndex = kMaxOpenDocuments;
    OutputErrorCode error = OutputErrorCode::None;
    if (!WorkspaceControllerOpenDocumentAtLocation(&g_controller, g_controller.model.project.projectId,
                                                   path, 1, 1, &documentIndex, &error)) {
        includeGraphSetStatus("Included file is no longer available.");
        return false;
    }
    NavigationHistoryPush(&g_navigationHistory, g_includeNavigationOrigin);
    g_includeNavigationOriginValid = false;
    g_includeTargetPickerOpen = false;
    g_includeGraphPanelOpen = false;
    g_editorFocused = true;
    keepCaretVisible(WorkspaceControllerActiveDocument(&g_controller));
    logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER include_graph_picker_activation=PASS");
    return true;
}

static bool handleIncludeTargetPickerKey(gx_app_context* ctx, int keyCode, int action) {
    if (!g_includeTargetPickerOpen || action != GX_KEY_ACTION_DOWN) return false;
    if (keyCode == 27) { g_includeTargetPickerOpen = false; g_includeNavigationOriginValid = false; return true; }
    if (keyCode == GX_KEY_UP) { if (g_includeTargetSelected > 0) --g_includeTargetSelected; return true; }
    const auto* edge = IncludeGraphEdgeAt(&g_includeGraph, g_includeTargetEdgeIndex);
    const uint32_t count = edge ? edge->resolution.ambiguousCandidateCount : 0;
    if (keyCode == GX_KEY_DOWN) { if (g_includeTargetSelected + 1 < count) ++g_includeTargetSelected; return true; }
    if (keyCode == 33) { g_includeTargetSelected = g_includeTargetSelected > 10 ? g_includeTargetSelected - 10 : 0; return true; }
    if (keyCode == 34) { g_includeTargetSelected = g_includeTargetSelected + 10 < count ? g_includeTargetSelected + 10 : (count ? count - 1 : 0); return true; }
    if (keyCode == 36) { g_includeTargetSelected = 0; return true; }
    if (keyCode == 35) { g_includeTargetSelected = count ? count - 1 : 0; return true; }
    if (keyCode == 13 && count > 0) { activateIncludeTargetCandidate(ctx, g_includeTargetSelected); return true; }
    return true;
}

static bool tryIncludeGraphDefinition(gx_app_context* ctx) {
    if (!g_controller.model.open || !g_controller.model.hasProject) return false;
    Document* document = WorkspaceControllerActiveDocument(&g_controller);
    char relativePath[kMaxPathBytes] = {};
    if (!document || !copyProjectRelativePath(g_controller.model, document->path, relativePath, sizeof(relativePath)) ||
        !IncludeGraphIsCurrent(&g_includeGraph, g_controller.model.project.projectId, g_controller.model.projectGeneration)) return false;
    const auto* edge = IncludeGraphFindDirectiveAt(&g_includeGraph, relativePath, document->buffer.caret);
    if (!edge) return false;
    uint32_t edgeIndex = 0;
    for (uint32_t i = 0; i < g_includeGraph.edgeCount; ++i) if (&g_includeGraph.edges[i] == edge) { edgeIndex = i; break; }
    if (edge->resolution.state == IncludeResolutionState::Ambiguous) {
        if (!captureNavigationLocation(*document, &g_includeNavigationOrigin)) return true;
        g_includeNavigationOriginValid = true;
        g_includeTargetEdgeIndex = edgeIndex;
        g_includeTargetSelected = 0;
        g_includeTargetScroll = 0;
        g_includeTargetPickerOpen = true;
        g_editorFocused = false;
        return true;
    }
    if (edge->resolution.state != IncludeResolutionState::Resolved) {
        includeGraphSetStatus(edge->resolution.state == IncludeResolutionState::ExternalUnresolved ?
                              "External include is not indexed." : IncludeResolutionStateName(edge->resolution.state));
        return true;
    }
    g_includeGraphMode = 0;
    return activateIncludeEdge(ctx, edgeIndex);
}

static bool refreshIncludeGraphDocument(gx_app_context* ctx, Document* document) {
    if (!document || !g_includeGraphPanelOpen || !g_controller.model.hasProject ||
        !IncludeGraphIsCurrent(&g_includeGraph, g_controller.model.project.projectId, g_controller.model.projectGeneration)) return false;
    char relativePath[kMaxPathBytes] = {};
    if (!copyProjectRelativePath(g_controller.model, document->path, relativePath, sizeof(relativePath))) return false;
    IncludeGraphRequest request = {};
    fillIncludeGraphRequest(&request);
    request.dirtyDocuments = nullptr;
    request.dirtyDocumentCount = 0;
    IncludeGraphErrorCode error = IncludeGraphErrorCode::None;
    if (!IncludeGraphRescanDocument(&g_includeGraph, request, relativePath, document->documentId,
                                    document->buffer.generation, document->buffer.data, document->buffer.length,
                                    document->buffer.dirty, &error)) {
        includeGraphSetStatus(IncludeGraphStatusText(error));
        return false;
    }
    includeGraphSetStatus("Include Graph updated for active document.");
    logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER include_graph_incremental_update=PASS");
    return true;
}

static void closeIncludeGraphPanel(gx_app_context* ctx) {
    if (IncludeGraphIsActive(&g_includeGraphOperation)) {
        IncludeGraphCancel(&g_includeGraphOperation, g_includeGraphOperationId);
        includeGraphSetStatus("Cancelling Include Graph build...");
    }
    g_includeGraphPanelOpen = false;
    g_includeGraphResultsFocused = false;
    g_includeTargetPickerOpen = false;
    g_includeNavigationOriginValid = false;
    g_editorFocused = true;
    if (ctx) logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER include_graph_panel=HIDDEN");
}

static bool handleIncludeGraphKey(gx_app_context* ctx, int keyCode, int action, int modifiers) {
    if (!g_includeGraphPanelOpen || action != GX_KEY_ACTION_DOWN) return false;
    if (g_includeTargetPickerOpen) return handleIncludeTargetPickerKey(ctx, keyCode, action);
    if ((modifiers & GX_KEY_MOD_CTRL) && (modifiers & GX_KEY_MOD_SHIFT) && (keyCode == 73 || keyCode == 105)) {
        startIncludeGraph(ctx);
        return true;
    }
    if (keyCode == 27) { closeIncludeGraphPanel(ctx); return true; }
    if (keyCode == 82 || keyCode == 114) { startIncludeGraph(ctx); return true; }
    if (keyCode == 67 || keyCode == 99) {
        if (IncludeGraphIsActive(&g_includeGraphOperation)) {
            IncludeGraphCancel(&g_includeGraphOperation, g_includeGraphOperationId);
            includeGraphSetStatus("Cancelling Include Graph build...");
        }
        return true;
    }
    if (keyCode == 9 || keyCode == GX_KEY_RIGHT) { g_includeGraphMode = (g_includeGraphMode + 1) % 6; g_includeGraphScroll = 0; g_includeGraphSelectedRow = 0; return true; }
    if (keyCode == GX_KEY_LEFT) { g_includeGraphMode = g_includeGraphMode == 0 ? 5 : g_includeGraphMode - 1; g_includeGraphScroll = 0; g_includeGraphSelectedRow = 0; return true; }
    const uint32_t total = includeGraphRowCount();
    if (keyCode == GX_KEY_UP) { if (g_includeGraphSelectedRow > 0) --g_includeGraphSelectedRow; }
    else if (keyCode == GX_KEY_DOWN) { if (g_includeGraphSelectedRow + 1 < total) ++g_includeGraphSelectedRow; }
    else if (keyCode == 33) { g_includeGraphSelectedRow = g_includeGraphSelectedRow > kIncludeGraphPanelMaxRows ? g_includeGraphSelectedRow - kIncludeGraphPanelMaxRows : 0; }
    else if (keyCode == 34) { g_includeGraphSelectedRow = g_includeGraphSelectedRow + kIncludeGraphPanelMaxRows < total ? g_includeGraphSelectedRow + kIncludeGraphPanelMaxRows : (total ? total - 1 : 0); }
    else if (keyCode == 36) g_includeGraphSelectedRow = 0;
    else if (keyCode == 35) g_includeGraphSelectedRow = total ? total - 1 : 0;
    else if (keyCode == 13 && total > 0) {
        uint32_t edgeIndex = 0;
        const auto* edge = includeGraphEdgeForRow(g_includeGraphSelectedRow, &edgeIndex);
        if (edge && edge->resolution.state == IncludeResolutionState::Ambiguous &&
            (g_includeGraphMode == 0 || g_includeGraphMode == 2)) {
            Document* document = WorkspaceControllerActiveDocument(&g_controller);
            if (document) captureNavigationLocation(*document, &g_includeNavigationOrigin);
            g_includeNavigationOriginValid = true;
            g_includeTargetEdgeIndex = edgeIndex;
            g_includeTargetSelected = 0;
            g_includeTargetPickerOpen = true;
            g_editorFocused = false;
        } else if (edge) activateIncludeEdge(ctx, edgeIndex);
    } else return true;
    if (g_includeGraphSelectedRow < g_includeGraphScroll) g_includeGraphScroll = g_includeGraphSelectedRow;
    if (g_includeGraphSelectedRow >= g_includeGraphScroll + kIncludeGraphPanelMaxRows)
        g_includeGraphScroll = g_includeGraphSelectedRow - kIncludeGraphPanelMaxRows + 1;
    (void)modifiers;
    return true;
}

static void drawIncludeGraphPanel(gx_app_context* ctx) {
    if (!ctx || !g_includeGraphPanelOpen) return;
    drawPanel(ctx, { 8, kIncludeGraphPanelTop + 4, 944, 580 }, 0x263650u);
    drawText(ctx, 24, 72, "INCLUDE GRAPH");
    char activePath[kMaxPathBytes] = {};
    includeGraphActivePath(activePath, sizeof(activePath));
    copyText(g_textScratch, sizeof(g_textScratch), "File: ");
    appendText(g_textScratch, sizeof(g_textScratch), activePath[0] ? activePath : "No active C/C++ file");
    drawText(ctx, 24, 96, g_textScratch);
    const char* modes[] = { "Direct Includes", "Included By", "Transitive Includes", "Transitive Included By", "Cycles", "Unresolved" };
    for (uint32_t i = 0; i < 6; ++i) {
        const int x = 18 + static_cast<int>(i) * kIncludeGraphPanelModeWidth;
        drawPanel(ctx, { x, 110, kIncludeGraphPanelModeWidth - 4, 25 }, i == g_includeGraphMode ? 0x405775u : 0x202A36u);
        drawText(ctx, x + 5, 127, modes[i]);
    }
    const auto* operation = IncludeGraphOperationInfo(&g_includeGraphOperation);
    copyText(g_textScratch, sizeof(g_textScratch), "Graph: ");
    if (operation && IncludeGraphIsActive(operation)) appendText(g_textScratch, sizeof(g_textScratch), IncludeGraphBuildStateName(operation->state));
    else if (!g_includeGraph.complete) appendText(g_textScratch, sizeof(g_textScratch), "No completed graph");
    else appendText(g_textScratch, sizeof(g_textScratch), g_includeGraph.truncated ? "Current (truncated)" : "Current");
    appendText(g_textScratch, sizeof(g_textScratch), "   [R] Refresh  [C] Cancel  [Esc] Close");
    drawText(ctx, 24, 155, g_textScratch);
    const uint32_t total = includeGraphRowCount();
    drawPanel(ctx, { 16, kIncludeGraphPanelResultsTop - 12, 928, 1 }, 0x405775u);
    const uint32_t end = g_includeGraphScroll + kIncludeGraphPanelMaxRows < total ?
        g_includeGraphScroll + kIncludeGraphPanelMaxRows : total;
    for (uint32_t row = g_includeGraphScroll; row < end; ++row) {
        uint32_t edgeIndex = 0;
        const auto* edge = includeGraphEdgeForRow(row, &edgeIndex);
        const int y = kIncludeGraphPanelResultsTop + static_cast<int>((row - g_includeGraphScroll) * kIncludeGraphPanelRowHeight);
        if (row == g_includeGraphSelectedRow) drawPanel(ctx, { 20, y - 15, 920, kIncludeGraphPanelRowHeight }, 0x34496Au);
        if (g_includeGraphMode == 4) {
            const auto& cycle = g_includeGraph.cycles[row];
            copyText(g_textScratch, sizeof(g_textScratch), "Cycle ");
            appendUnsigned(g_textScratch, sizeof(g_textScratch), row + 1);
            appendText(g_textScratch, sizeof(g_textScratch), cycle.selfCycle ? "  Self-cycle  " : (cycle.containsConditionalEdge ? "  Possible conditional  " : "  Definite  "));
            appendText(g_textScratch, sizeof(g_textScratch), cycle.representativePath);
        } else if (!edge) {
            copyText(g_textScratch, sizeof(g_textScratch), "Dependency target");
        } else {
            const char* path = g_includeGraphMode == 1 || g_includeGraphMode == 3 ? edge->sourceRelativePath :
                (edge->resolution.state == IncludeResolutionState::Resolved ? edge->targetRelativePath : edge->directive.requestedPath);
            copyText(g_textScratch, sizeof(g_textScratch), "[");
            appendText(g_textScratch, sizeof(g_textScratch), IncludeDelimiterName(edge->directive.delimiterKind));
            appendText(g_textScratch, sizeof(g_textScratch), "] ");
            appendUnsigned(g_textScratch, sizeof(g_textScratch), edge->directive.line);
            appendText(g_textScratch, sizeof(g_textScratch), ":");
            appendUnsigned(g_textScratch, sizeof(g_textScratch), edge->directive.column);
            appendText(g_textScratch, sizeof(g_textScratch), "  ");
            appendText(g_textScratch, sizeof(g_textScratch), path);
            appendText(g_textScratch, sizeof(g_textScratch), "  ");
            appendText(g_textScratch, sizeof(g_textScratch), IncludeResolutionStateName(edge->resolution.state));
            if (edge->directive.directiveState != IncludeDirectiveState::Active) {
                appendText(g_textScratch, sizeof(g_textScratch), "  ");
                appendText(g_textScratch, sizeof(g_textScratch), IncludeDirectiveStateName(edge->directive.directiveState));
            }
        }
        drawText(ctx, 28, y, g_textScratch);
    }
    if (total == 0) drawText(ctx, 28, kIncludeGraphPanelResultsTop, g_includeGraphStatus[0] ? g_includeGraphStatus : "No rows for this view.");
    else drawText(ctx, 28, 612, g_includeGraphStatus);
    if (g_includeGraph.truncated) drawText(ctx, 600, 612, "TRUNCATED");
}

static void drawIncludeTargetPicker(gx_app_context* ctx) {
    if (!ctx || !g_includeTargetPickerOpen) return;
    const auto* edge = IncludeGraphEdgeAt(&g_includeGraph, g_includeTargetEdgeIndex);
    if (!edge) return;
    drawPanel(ctx, { 132, 92, 696, 500 }, 0x202A36u);
    drawText(ctx, 150, 120, "CHOOSE INCLUDED FILE");
    drawText(ctx, 150, 144, edge->directive.requestedPath);
    const uint32_t count = edge->resolution.ambiguousCandidateCount;
    const uint32_t end = g_includeTargetScroll + 18 < count ? g_includeTargetScroll + 18 : count;
    for (uint32_t row = g_includeTargetScroll; row < end; ++row) {
        const int y = 174 + static_cast<int>((row - g_includeTargetScroll) * 22);
        if (row == g_includeTargetSelected) drawPanel(ctx, { 144, y - 15, 672, 20 }, 0x34496Au);
        drawText(ctx, 152, y, IncludeGraphCandidateAt(&g_includeGraph, edge->resolution, row));
    }
    drawText(ctx, 150, 566, "Up/Down Select  Enter Open  Escape Cancel");
}

static void completionStatusForError(CompletionErrorCode error) {
    const char* status = guidexos::developer_studio::CompletionStatusText(error);
    if (status[0] != '\0') completionSetStatus(status);
    else {
        copyText(g_completionStatus, sizeof(g_completionStatus), "Completion unavailable: ");
        appendText(g_completionStatus, sizeof(g_completionStatus), CompletionErrorName(error));
    }
}

static void dismissCompletion(gx_app_context* ctx, const char* reason, bool showStatus) {
    if (!g_completionPopupOpen && !g_completionSession.active) return;
    CompletionSessionDismiss(&g_completionSession);
    g_completionPopupOpen = false;
    if (showStatus && reason) {
        completionSetStatus(reason);
        copyText(g_textScratch, sizeof(g_textScratch), "GUIDEXOS_DEVELOPER_STUDIO_MARKER completion_dismiss=PASS reason=");
        appendText(g_textScratch, sizeof(g_textScratch), reason);
        logMarker(ctx, g_textScratch);
    }
}

static void completionLogContext(gx_app_context* ctx) {
    copyText(g_textScratch, sizeof(g_textScratch), "GUIDEXOS_DEVELOPER_STUDIO_MARKER completion_context=");
    appendText(g_textScratch, sizeof(g_textScratch), CompletionContextKindName(g_completionSession.context.kind));
    if (g_completionSession.context.hasExplicitQualifier) {
        appendText(g_textScratch, sizeof(g_textScratch), " qualifier=");
        appendText(g_textScratch, sizeof(g_textScratch), g_completionSession.context.explicitQualifier);
    }
    logMarker(ctx, g_textScratch);
}

static bool openCompletion(gx_app_context* ctx, bool manuallyInvoked) {
    dismissSignatureHelp(ctx, "completion", false);
    Document* document = WorkspaceControllerActiveDocument(&g_controller);
    if (!g_controller.model.open || !g_controller.model.hasProject) {
        completionSetStatus("Open a project before using Code Completion.");
        markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER completion_accept=FAIL", CompletionErrorName(CompletionErrorCode::NoProject));
        return false;
    }
    if (!document || !g_editorFocused) {
        completionSetStatus("Focus the source editor before using Code Completion.");
        markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER completion_accept=FAIL", CompletionErrorName(CompletionErrorCode::NoDocument));
        return false;
    }
    if (g_completionSession.sessionId == 0) g_completionSession.sessionId = 1;
    else ++g_completionSession.sessionId;
    CompletionErrorCode error = CompletionErrorCode::None;
    copyText(g_completionProjectId, sizeof(g_completionProjectId), g_controller.model.project.projectId);
    CompletionContext preflight = {};
    CompletionErrorCode preflightError = CompletionErrorCode::None;
    const bool memberContext = CompletionExtractContext(
        *document, CompletionProjectId(g_controller.model.project.projectId),
        g_controller.model.projectGeneration, g_completionSession.sessionId,
        manuallyInvoked, &preflight, &preflightError) &&
        preflight.kind == guidexos::developer_studio::CompletionContextKind::MemberAccessLexical;
    const TypeDatabase* typeDatabase = memberContext && ensureTypeDatabase(document)
        ? &g_typeDatabase : nullptr;
    if (!CompletionBuildSession(&g_completionSession, *document,
                                        CompletionProjectId(g_controller.model.project.projectId),
                                        g_controller.model.projectGeneration, &g_symbolDatabase,
                                        &g_completionWordCache, manuallyInvoked, typeDatabase, &error)) {
        completionStatusForError(error);
        markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER completion_accept=FAIL", CompletionErrorName(error));
        g_completionPopupOpen = false;
        return false;
    }
    copyText(g_textScratch, sizeof(g_textScratch), "GUIDEXOS_DEVELOPER_STUDIO_MARKER completion_begin=PASS prefix=");
    appendText(g_textScratch, sizeof(g_textScratch), g_completionSession.context.prefix);
    logMarker(ctx, g_textScratch);
    completionLogContext(ctx);
    copyText(g_textScratch, sizeof(g_textScratch), "GUIDEXOS_DEVELOPER_STUDIO_MARKER completion_candidates=PASS total=");
    appendUnsigned(g_textScratch, sizeof(g_textScratch), g_completionSession.collectedCount);
    appendText(g_textScratch, sizeof(g_textScratch), " retained=");
    appendUnsigned(g_textScratch, sizeof(g_textScratch), g_completionSession.candidateCount);
    logMarker(ctx, g_textScratch);
    g_completionPopupOpen = g_completionSession.active;
    if (!g_completionPopupOpen) {
        const char* memberStatus = g_completionSession.context.kind ==
            guidexos::developer_studio::CompletionContextKind::MemberAccessLexical
            ? CompletionMemberResolutionStatusText(g_completionSession.context.memberResolution) : "";
        if (memberStatus[0] != '\0') completionSetStatus(memberStatus);
        else completionStatusForError(error == CompletionErrorCode::None ? CompletionErrorCode::NoResults : error);
        return true;
    }
    if (g_completionSession.truncated) {
        completionSetStatus("Completion results truncated.");
        copyText(g_textScratch, sizeof(g_textScratch), "GUIDEXOS_DEVELOPER_STUDIO_MARKER completion_candidates=TRUNCATED retained=");
        appendUnsigned(g_textScratch, sizeof(g_textScratch), g_completionSession.candidateCount);
        logMarker(ctx, g_textScratch);
    } else if (g_completionSession.context.kind == guidexos::developer_studio::CompletionContextKind::MemberAccessLexical) {
        completionSetStatus(CompletionMemberResolutionStatusText(g_completionSession.context.memberResolution));
    } else {
        completionSetStatus("");
    }
    return true;
}

static bool refreshCompletion(gx_app_context* ctx) {
    if (!g_completionPopupOpen) return false;
    Document* document = WorkspaceControllerActiveDocument(&g_controller);
    if (!document) {
        completionSetStatus("Completion session expired.");
        dismissCompletion(ctx, "document_missing", true);
        return false;
    }
    CompletionErrorCode error = CompletionErrorCode::None;
    CompletionContext preflight = {};
    CompletionErrorCode preflightError = CompletionErrorCode::None;
    const bool memberContext = CompletionExtractContext(
        *document, CompletionProjectId(g_controller.model.project.projectId),
        g_controller.model.projectGeneration, g_completionSession.sessionId,
        true, &preflight, &preflightError) &&
        preflight.kind == guidexos::developer_studio::CompletionContextKind::MemberAccessLexical;
    const TypeDatabase* typeDatabase = memberContext && ensureTypeDatabase(document)
        ? &g_typeDatabase : nullptr;
    if (!CompletionSessionRefresh(&g_completionSession, *document,
                                  CompletionProjectId(g_controller.model.project.projectId),
                                  g_controller.model.projectGeneration, &g_symbolDatabase,
                                  &g_completionWordCache, typeDatabase, &error)) {
        completionStatusForError(error == CompletionErrorCode::None ? CompletionErrorCode::SessionStale : error);
        dismissCompletion(ctx, "stale", true);
        return false;
    }
    g_completionPopupOpen = g_completionSession.active;
    if (!g_completionPopupOpen) {
        const char* memberStatus = g_completionSession.context.kind ==
            guidexos::developer_studio::CompletionContextKind::MemberAccessLexical
            ? CompletionMemberResolutionStatusText(g_completionSession.context.memberResolution) : "";
        completionSetStatus(memberStatus[0] != '\0' ? memberStatus : "No completion suggestions.");
        return true;
    }
    if (g_completionSession.truncated) completionSetStatus("Completion results truncated.");
    else if (g_completionSession.context.kind == guidexos::developer_studio::CompletionContextKind::MemberAccessLexical)
        completionSetStatus(CompletionMemberResolutionStatusText(g_completionSession.context.memberResolution));
    return true;
}

static bool acceptCompletion(gx_app_context* ctx) {
    if (!g_completionPopupOpen) return false;
    Document* document = WorkspaceControllerActiveDocument(&g_controller);
    const CompletionCandidate* candidate = CompletionSessionSelected(&g_completionSession);
    CompletionErrorCode error = CompletionErrorCode::None;
    if (!document || !candidate ||
        !CompletionSessionIsCurrent(&g_completionSession, *document,
                                    CompletionProjectId(g_controller.model.project.projectId),
                                    g_controller.model.projectGeneration, &error) ||
        !CompletionSessionTextMatches(&g_completionSession, *document, &error)) {
        completionSetStatus("Completion session expired.");
        markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER completion_accept=STALE", CompletionErrorName(error));
        dismissCompletion(ctx, "stale", false);
        return false;
    }
    const uint32_t insertionLength = lengthOf(candidate->insertionText, sizeof(candidate->insertionText));
    if (insertionLength > guidexos::developer_studio::kCompletionMaxInsertionBytes) {
        completionSetStatus("Completion insertion rejected.");
        markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER completion_accept=FAIL", CompletionErrorName(CompletionErrorCode::InsertionTooLong));
        return false;
    }
    g_completionUndoAvailable = false;
    if (document->buffer.length <= kMaxEditorBytes) {
        for (uint32_t i = 0; i <= document->buffer.length; ++i) g_completionUndoText[i] = document->buffer.data[i];
        g_completionUndoDocumentId = document->documentId;
        g_completionUndoGeneration = document->buffer.generation;
        g_completionUndoLength = document->buffer.length;
        g_completionUndoDirty = document->buffer.dirty;
        g_completionUndoCaret = document->buffer.caret;
        g_completionUndoSelectionAnchor = document->buffer.selectionAnchor;
        g_completionUndoSelectionActive = document->buffer.selectionActive;
        g_completionUndoAvailable = true;
    }
    const uint32_t oldLength = g_completionSession.context.replacementEnd - g_completionSession.context.replacementStart;
    if (!ReplaceTextRange(&document->buffer, g_completionSession.context.replacementStart, oldLength,
                          candidate->insertionText, insertionLength)) {
        g_completionUndoAvailable = false;
        completionSetStatus("Completion insertion rejected.");
        markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER completion_accept=FAIL", CompletionErrorName(CompletionErrorCode::InsertionFailed));
        return false;
    }
    updateSyntaxAfterEdit(ctx, document);
    g_completionWordCache.valid = false;
    CompletionErrorCode wordError = CompletionErrorCode::None;
    DocumentWordCacheRefresh(&g_completionWordCache, *document, &wordError);
    keepCaretVisible(document);
    copyText(g_textScratch, sizeof(g_textScratch), "GUIDEXOS_DEVELOPER_STUDIO_MARKER completion_accept=PASS text=");
    appendText(g_textScratch, sizeof(g_textScratch), candidate->insertionText);
    logMarker(ctx, g_textScratch);
    CompletionSessionDismiss(&g_completionSession);
    g_completionPopupOpen = false;
    completionSetStatus("");
    return true;
}

static bool undoCompletion(gx_app_context* ctx) {
    if (!g_completionUndoAvailable) return false;
    Document* document = WorkspaceControllerActiveDocument(&g_controller);
    if (!document || document->documentId != g_completionUndoDocumentId ||
        document->buffer.generation != g_completionUndoGeneration + 1u) {
        g_completionUndoAvailable = false;
        return false;
    }
    if (!TextBufferSet(&document->buffer, g_completionUndoText, g_completionUndoLength)) {
        g_completionUndoAvailable = false;
        markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER completion_accept=FAIL", CompletionErrorName(CompletionErrorCode::InsertionFailed));
        return false;
    }
    document->buffer.dirty = g_completionUndoDirty;
    document->buffer.caret = g_completionUndoCaret <= document->buffer.length ? g_completionUndoCaret : document->buffer.length;
    document->buffer.selectionAnchor = g_completionUndoSelectionAnchor <= document->buffer.length ? g_completionUndoSelectionAnchor : document->buffer.length;
    document->buffer.selectionActive = g_completionUndoSelectionActive && document->buffer.selectionAnchor <= document->buffer.caret;
    updateSyntaxAfterEdit(ctx, document);
    g_completionWordCache.valid = false;
    CompletionErrorCode wordError = CompletionErrorCode::None;
    DocumentWordCacheRefresh(&g_completionWordCache, *document, &wordError);
    g_completionUndoAvailable = false;
    dismissCompletion(ctx, "undo", false);
    logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER completion_undo=PASS");
    return true;
}

static bool handleCompletionKey(gx_app_context* ctx, int keyCode, int action, int modifiers) {
    if (!g_completionPopupOpen || action != GX_KEY_ACTION_DOWN) return false;
    if (keyCode == 27) { dismissCompletion(ctx, "escape", false); return true; }
    if (keyCode == GX_KEY_UP) { CompletionSessionMove(&g_completionSession, -1); return true; }
    if (keyCode == GX_KEY_DOWN) { CompletionSessionMove(&g_completionSession, 1); return true; }
    if (keyCode == 33) { CompletionSessionPage(&g_completionSession, -1); return true; }
    if (keyCode == 34) { CompletionSessionPage(&g_completionSession, 1); return true; }
    if (keyCode == 36) { CompletionSessionHome(&g_completionSession); return true; }
    if (keyCode == 35) { CompletionSessionEnd(&g_completionSession); return true; }
    if ((keyCode == 13 || keyCode == 9) && !(modifiers & GX_KEY_MOD_CTRL)) { acceptCompletion(ctx); return true; }
    if (keyCode == GX_KEY_LEFT || keyCode == GX_KEY_RIGHT || keyCode == 46) {
        dismissCompletion(ctx, "caret_moved", false);
        return false;
    }
    if ((modifiers & GX_KEY_MOD_CTRL) && keyCode == 32) return refreshCompletion(ctx);
    return false;
}

static bool completionPopupBounds(gx_rect* output) {
    if (!output || !g_completionPopupOpen || !g_completionSession.active) return false;
    Document* document = WorkspaceControllerActiveDocument(&g_controller);
    if (!document) return false;
    const uint32_t line = activeLine(document->buffer);
    const uint32_t column = activeColumn(document->buffer, line);
    const uint32_t visibleColumn = column > g_editorScrollColumn ? column - g_editorScrollColumn : 0;
    const uint32_t visibleLine = line > g_editorScrollLine ? line - g_editorScrollLine : 0;
    const int caretX = kEditorTextX + static_cast<int>(visibleColumn * 8u);
    const int caretY = kEditorTop + static_cast<int>(visibleLine * kEditorLineHeight);
    const uint32_t remaining = g_completionSession.candidateCount > g_completionSession.visibleStart
        ? g_completionSession.candidateCount - g_completionSession.visibleStart : 0;
    const uint32_t rows = remaining < static_cast<uint32_t>(kCompletionPopupMaxRows) ? remaining : static_cast<uint32_t>(kCompletionPopupMaxRows);
    if (rows == 0) return false;
    const int height = 22 + static_cast<int>(rows) * kCompletionPopupRowHeight;
    int x = caretX;
    int y = caretY + kEditorLineHeight;
    if (x + kCompletionPopupWidth > kEditorRect.x + kEditorRect.width) x = kEditorRect.x + kEditorRect.width - kCompletionPopupWidth;
    if (x < kEditorRect.x) x = kEditorRect.x;
    if (y + height > kEditorRect.y + kEditorRect.height) y = caretY - height;
    if (y < kEditorRect.y) y = kEditorRect.y;
    *output = { x, y, kCompletionPopupWidth, height };
    return true;
}

static void drawCompletionPopup(gx_app_context* ctx) {
    if (!ctx) return;
    gx_rect bounds = {};
    if (!completionPopupBounds(&bounds)) return;
    const uint32_t rows = (static_cast<uint32_t>(bounds.height) - 22u) / static_cast<uint32_t>(kCompletionPopupRowHeight);
    drawPanel(ctx, bounds, 0x202A36u);
    drawText(ctx, bounds.x + 8, bounds.y + 15, "CODE COMPLETION");
    for (uint32_t row = 0; row < rows; ++row) {
        const uint32_t index = g_completionSession.visibleStart + row;
        const CompletionCandidate& candidate = g_completionSession.candidates[index];
        const int rowY = bounds.y + 22 + static_cast<int>(row) * kCompletionPopupRowHeight;
        if (index == g_completionSession.selectedIndex) drawPanel(ctx, { bounds.x + 2, rowY - 15, kCompletionPopupWidth - 4, kCompletionPopupRowHeight }, 0x34496Au);
        copyText(g_textScratch, sizeof(g_textScratch), CompletionCandidateKindPrefix(candidate.kind));
        appendText(g_textScratch, sizeof(g_textScratch), " ");
        appendText(g_textScratch, sizeof(g_textScratch), candidate.displayText);
        drawText(ctx, bounds.x + 8, rowY, g_textScratch);
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

static int debugBreakpointIndexForDocumentLine(const Document& document, uint32_t line) {
    if (!g_controller.model.hasProject) return -1;
    char relative[kMaxProjectPathBytes] = {};
    if (!DebugRelativeSourcePath(g_controller.model.project.rootPath, document.path,
                                 relative, sizeof(relative))) return -1;
    for (uint32_t i = 0; i < g_debugController.breakpointCount; ++i) {
        const DebugBreakpoint* breakpoint = DebugControllerBreakpointAt(&g_debugController, i);
        if (breakpoint && breakpoint->location.line == line + 1 &&
            PathsEqual(breakpoint->location.relativePath, relative) &&
            PathsEqual(breakpoint->projectId, g_controller.model.project.projectId)) return static_cast<int>(i);
    }
    return -1;
}

static uint32_t debugBreakpointColor(DebugBreakpointState state) {
    switch (state) {
    case DebugBreakpointState::Mapped: return 0x78B7E8u;
    case DebugBreakpointState::Verified: return 0x56C596u;
    case DebugBreakpointState::Rejected: return 0xD05A5Au;
    case DebugBreakpointState::Disabled: return 0x6F7888u;
    case DebugBreakpointState::Stale: return 0xD59B49u;
    case DebugBreakpointState::Pending: return 0xD8C15Au;
    }
    return 0xD8C15Au;
}

static bool toggleBreakpointAtCaret(gx_app_context* ctx) {
    Document* document = WorkspaceControllerActiveDocument(&g_controller);
    if (!document || !g_controller.model.hasProject) {
        writeStudioOutput("Breakpoint requires an active project document");
        return false;
    }
    const uint32_t line = activeLine(document->buffer) + 1;
    char relative[kMaxProjectPathBytes] = {};
    if (!DebugRelativeSourcePath(g_controller.model.project.rootPath, document->path,
                                 relative, sizeof(relative))) {
        writeStudioOutput("Breakpoint path is outside the active project");
        return false;
    }
    uint64_t breakpointId = 0;
    DebugErrorCode error = DebugErrorCode::None;
    if (!DebugControllerToggleBreakpoint(&g_debugController, g_controller.model.project.projectId,
                                         g_controller.model.project.rootPath, g_controller.model.projectGeneration,
                                         relative, line, activeColumn(document->buffer, line - 1),
                                         document->buffer.generation, &breakpointId, &error)) {
        copyText(g_textScratch, sizeof(g_textScratch), "Breakpoint change failed: ");
        appendText(g_textScratch, sizeof(g_textScratch), DebugErrorName(error));
        writeStudioOutput(g_textScratch);
        return false;
    }
    for (uint32_t i = 0; i < g_debugController.breakpointCount; ++i) {
        const DebugBreakpoint* breakpoint = DebugControllerBreakpointAt(&g_debugController, i);
        if (breakpoint && breakpoint->id == breakpointId) g_debugSelectedBreakpoint = i;
    }
    refreshDebugMappings();
    const DebugBreakpoint* breakpoint = DebugControllerBreakpointAt(&g_debugController, g_debugSelectedBreakpoint);
    copyText(g_textScratch, sizeof(g_textScratch), "Breakpoint ");
    appendText(g_textScratch, sizeof(g_textScratch), breakpoint ? DebugBreakpointStateName(breakpoint->state) : "updated");
    appendText(g_textScratch, sizeof(g_textScratch), ": ");
    appendText(g_textScratch, sizeof(g_textScratch), relative);
    appendText(g_textScratch, sizeof(g_textScratch), ":");
    appendUnsigned(g_textScratch, sizeof(g_textScratch), line);
    writeStudioOutput(g_textScratch);
    if (breakpoint && breakpoint->state == DebugBreakpointState::Pending)
        logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_breakpoint=PENDING");
    else if (breakpoint && breakpoint->state == DebugBreakpointState::Mapped)
        logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_breakpoint=MAPPED");
    return true;
}

static bool toggleBreakpointAtMouse(gx_app_context* ctx, int x, int y) {
    if (x < kEditorRect.x || x >= kEditorTextX || y < kEditorTop ||
        y >= kEditorRect.y + kEditorRect.height) return false;
    Document* document = WorkspaceControllerActiveDocument(&g_controller);
    if (!document) return false;
    int row = (y - kEditorTop + 12) / kEditorLineHeight;
    if (row < 0) row = 0;
    const uint32_t line = g_editorScrollLine + static_cast<uint32_t>(row);
    if (line >= TextBufferLineCount(&document->buffer)) return false;
    const uint32_t oldCaret = document->buffer.caret;
    const uint32_t start = TextBufferLineStart(&document->buffer, line);
    const uint32_t end = TextBufferLineEnd(&document->buffer, line);
    document->buffer.caret = start;
    const bool toggled = toggleBreakpointAtCaret(ctx);
    document->buffer.caret = oldCaret <= document->buffer.length ? oldCaret : document->buffer.length;
    (void)end;
    return toggled;
}

static bool navigateSelectedBreakpoint(gx_app_context* ctx) {
    const DebugBreakpoint* breakpoint = DebugControllerBreakpointAt(&g_debugController, g_debugSelectedBreakpoint);
    if (!breakpoint || !g_controller.model.hasProject) return false;
    uint32_t documentIndex = kMaxOpenDocuments;
    OutputErrorCode error = OutputErrorCode::None;
    if (!WorkspaceControllerOpenDocumentAtLocation(&g_controller, breakpoint->projectId,
                                                   breakpoint->location.relativePath, breakpoint->location.line,
                                                   breakpoint->location.column, &documentIndex, &error)) {
        copyText(g_textScratch, sizeof(g_textScratch), "Breakpoint navigation failed: ");
        appendText(g_textScratch, sizeof(g_textScratch), OutputErrorName(error));
        writeStudioOutput(g_textScratch);
        return false;
    }
    g_editorFocused = true;
    g_debugPanelOpen = false;
    logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_breakpoint_navigation=PASS");
    return true;
}

static bool handleDebugPanelKey(gx_app_context* ctx, int keyCode, int action) {
    if (!g_debugPanelOpen || action != GX_KEY_ACTION_DOWN) return false;
    if (keyCode == 27) { g_debugPanelOpen = false; g_editorFocused = true; return true; }
    if (keyCode == 9) { g_debugPanelTab = g_debugPanelTab == 0 ? 1 : 0; g_debugSelectedBreakpoint = 0; return true; }
    if (g_debugPanelTab != 0) return true;
    if (keyCode == GX_KEY_UP) {
        if (g_debugSelectedBreakpoint > 0) --g_debugSelectedBreakpoint;
        return true;
    }
    if (keyCode == GX_KEY_DOWN) {
        if (g_debugSelectedBreakpoint + 1 < g_debugController.breakpointCount) ++g_debugSelectedBreakpoint;
        return true;
    }
    const DebugBreakpoint* breakpoint = DebugControllerBreakpointAt(&g_debugController, g_debugSelectedBreakpoint);
    if (keyCode == 13) { navigateSelectedBreakpoint(ctx); return true; }
    if (keyCode == 32 && breakpoint) {
        DebugErrorCode error = DebugErrorCode::None;
        DebugControllerSetBreakpointEnabled(&g_debugController, breakpoint->id, !breakpoint->enabled, &error);
        refreshDebugMappings();
        return true;
    }
    if (keyCode == 46 && breakpoint) {
        DebugErrorCode error = DebugErrorCode::None;
        DebugControllerDeleteBreakpoint(&g_debugController, breakpoint->id, &error);
        if (g_debugSelectedBreakpoint >= g_debugController.breakpointCount && g_debugSelectedBreakpoint > 0) --g_debugSelectedBreakpoint;
        return true;
    }
    return true;
}

static void drawDebugPanel(gx_app_context* ctx) {
    if (!g_debugPanelOpen) return;
    drawPanel(ctx, { 72, kDebugPanelTop, 816, 552 }, 0x263650u);
    drawText(ctx, 92, 82, "DEBUGGER FOUNDATION");
    drawText(ctx, 112, 108, "Breakpoints");
    drawText(ctx, 260, 108, "Debug Session");
    drawPanel(ctx, { g_debugPanelTab == 0 ? 96 : 244, 116, 136, 2 }, 0xD6E4FFu);
    if (g_debugPanelTab == 0) {
        if (g_debugController.breakpointCount == 0) drawText(ctx, 100, 150, "No source breakpoints. F9 toggles the current line.");
        const uint32_t rows = g_debugController.breakpointCount < kDebugPanelMaxRows ? g_debugController.breakpointCount : kDebugPanelMaxRows;
        for (uint32_t row = 0; row < rows; ++row) {
            const DebugBreakpoint* breakpoint = DebugControllerBreakpointAt(&g_debugController, row);
            if (!breakpoint) continue;
            const int y = 148 + static_cast<int>(row) * kDebugPanelRowHeight;
            if (row == g_debugSelectedBreakpoint) drawPanel(ctx, { 88, y - 15, 784, 20 }, 0x34496Au);
            copyText(g_textScratch, sizeof(g_textScratch), breakpoint->enabled ? "[on] " : "[off] ");
            appendText(g_textScratch, sizeof(g_textScratch), DebugBreakpointStateName(breakpoint->state));
            appendText(g_textScratch, sizeof(g_textScratch), " ");
            appendText(g_textScratch, sizeof(g_textScratch), breakpoint->location.relativePath);
            appendText(g_textScratch, sizeof(g_textScratch), ":");
            appendUnsigned(g_textScratch, sizeof(g_textScratch), breakpoint->location.line);
            appendText(g_textScratch, sizeof(g_textScratch), " | ");
            appendText(g_textScratch, sizeof(g_textScratch), breakpoint->message);
            if (breakpoint->location.instructionAddress.valid) {
                appendText(g_textScratch, sizeof(g_textScratch), " @ ");
                char addressText[32] = {};
                appendHexAddress(addressText, sizeof(addressText), breakpoint->location.instructionAddress.value);
                appendText(g_textScratch, sizeof(g_textScratch), addressText);
            }
            drawText(ctx, 100, y, g_textScratch);
        }
        drawText(ctx, 100, 584, "Up/Down Select   Enter Navigate   Space Enable   Delete Remove   Tab Session   Esc Close");
    } else {
        drawText(ctx, 100, 148, "Backend:");
        drawText(ctx, 220, 148, g_debugController.backendName[0] ? g_debugController.backendName : "(none)");
        drawText(ctx, 100, 172, "State:");
        drawText(ctx, 220, 172, DebugSessionStateName(g_debugController.state));
        drawText(ctx, 100, 196, "Target:");
        drawText(ctx, 220, 196, g_debugController.target.applicationId[0] ? g_debugController.target.applicationId : "(none)");
        drawText(ctx, 100, 220, "Architecture:");
        drawText(ctx, 220, 220, g_debugController.target.architecture[0] ? g_debugController.target.architecture : "(none)");
        drawText(ctx, 100, 244, "Process ID:");
        if (g_debugController.processId != 0) { copyText(g_textScratch, sizeof(g_textScratch), ""); appendUnsigned(g_textScratch, sizeof(g_textScratch), static_cast<uint32_t>(g_debugController.processId)); drawText(ctx, 220, 244, g_textScratch); }
        else drawText(ctx, 220, 244, "(not assigned)");
        drawText(ctx, 100, 268, "Native runtime ID:");
        if (g_debugController.nativeRuntimeId != 0) { copyText(g_textScratch, sizeof(g_textScratch), ""); appendUnsigned(g_textScratch, sizeof(g_textScratch), static_cast<uint32_t>(g_debugController.nativeRuntimeId)); drawText(ctx, 220, 268, g_textScratch); }
        else drawText(ctx, 220, 268, "(not assigned)");
        drawText(ctx, 100, 304, "Capabilities:");
        drawText(ctx, 220, 304, g_debugController.capabilities.canLaunch ? "Launch" : "Launch unavailable");
        drawText(ctx, 220, 328, g_debugController.capabilities.canStop ? "Stop" : "Stop unavailable");
        drawText(ctx, 220, 352, g_debugController.capabilities.canPause ? "Pause" : "Pause unavailable");
        drawText(ctx, 220, 376, g_debugController.capabilities.canContinue ? "Continue" : "Continue unavailable");
        drawText(ctx, 220, 400, g_debugController.capabilities.canSetSourceBreakpoint ? "Source breakpoints" : "Source breakpoints unavailable");
        drawText(ctx, 100, 428, "Debug info:");
        copyText(g_textScratch, sizeof(g_textScratch), g_debugMapper.state == guidexos::developer_studio::DebugDwarfMapperState::Empty ? "(none)" : DebugDwarfMapperStateName(g_debugMapper.state));
        if (g_debugMapper.truncated) appendText(g_textScratch, sizeof(g_textScratch), " (truncated)");
        drawText(ctx, 220, 428, g_textScratch);
        drawText(ctx, 100, 452, "Source files:");
        copyText(g_textScratch, sizeof(g_textScratch), "");
        appendUnsigned(g_textScratch, sizeof(g_textScratch), g_debugMapper.sourceFileCount);
        drawText(ctx, 220, 452, g_textScratch);
        drawText(ctx, 100, 476, "Line rows:");
        copyText(g_textScratch, sizeof(g_textScratch), "");
        appendUnsigned(g_textScratch, sizeof(g_textScratch), g_debugMapper.lineRowCount);
        drawText(ctx, 220, 476, g_textScratch);
        drawText(ctx, 100, 500, "Artifact:");
        drawText(ctx, 220, 500, g_debugController.target.executablePath[0] ? g_debugController.target.executablePath : "(none)");
        drawText(ctx, 100, 524, "Last event:");
        if (g_debugController.eventCount > 0) {
            const guidexos::developer_studio::DebugEvent* event = guidexos::developer_studio::DebugControllerEventAt(&g_debugController, g_debugController.eventCount - 1);
            drawText(ctx, 220, 524, event ? guidexos::developer_studio::DebugEventKindName(event->kind) : "(none)");
        } else drawText(ctx, 220, 524, "(none)");
        drawText(ctx, 100, 548, "Last status:");
        drawText(ctx, 220, 548, g_debugController.lastMessage[0] ? g_debugController.lastMessage : "(none)");
        drawText(ctx, 100, 572, "Last stop:");
        drawText(ctx, 220, 572, DebugStopReasonName(g_debugController.stopReason));
        drawText(ctx, 100, 596, "Address:");
        if (g_debugController.currentInstructionAddress.valid) {
            copyText(g_textScratch, sizeof(g_textScratch), "");
            appendHexAddress(g_textScratch, sizeof(g_textScratch), g_debugController.currentInstructionAddress.value);
            drawText(ctx, 220, 596, g_textScratch);
        } else drawText(ctx, 220, 596, "(none)");
        drawText(ctx, 100, 620, "Tab Breakpoints   Esc Close");
    }
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
        const int breakpointIndex = debugBreakpointIndexForDocumentLine(*document, line);
        if (breakpointIndex >= 0) {
            const DebugBreakpoint* breakpoint = DebugControllerBreakpointAt(&g_debugController, static_cast<uint32_t>(breakpointIndex));
            if (breakpoint) drawPanel(ctx, { 272, kEditorTop - 10 + static_cast<int>(row) * kEditorLineHeight, 7, 8 }, debugBreakpointColor(breakpoint->state));
        }
        if (g_debugController.state == DebugSessionState::Paused &&
            g_debugController.currentLocation.line == line + 1) {
            char relative[kMaxProjectPathBytes] = {};
            if (DebugRelativeSourcePath(g_controller.model.project.rootPath, document->path, relative, sizeof(relative)) &&
                PathsEqual(relative, g_debugController.currentLocation.relativePath))
                drawPanel(ctx, { 262, kEditorTop - 10 + static_cast<int>(row) * kEditorLineHeight, 7, 8 }, 0xFFD166u);
        }
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
    if (g_signatureStatus[0] != '\0') drawText(ctx, 16, 646, g_signatureStatus);
    else if (g_completionStatus[0] != '\0') drawText(ctx, 16, 646, g_completionStatus);
    else if (g_typeStatus[0] != '\0') drawText(ctx, 16, 646, g_typeStatus);
    else if (g_definitionStatus[0] != '\0') drawText(ctx, 16, 646, g_definitionStatus);
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
    if (g_inputMode == InputMode::ConfirmDebug) {
        drawText(ctx, 210, 220, "Start Debugging");
        drawText(ctx, 210, 246, "Project documents have unsaved changes.");
        drawText(ctx, 210, 270, "Save All before building the debug target?");
        drawText(ctx, 230, 316, "[S] Save All and Start     [C] Cancel");
        return;
    }
    if (g_inputMode == InputMode::ConfirmDebugClose) {
        drawText(ctx, 210, 220, "Debug Session is active");
        drawText(ctx, 210, 246, "Stop the hosted target before closing Studio?");
        drawText(ctx, 230, 292, "[S] Stop Debugging     [K] Keep Studio Open");
        return;
    }
    drawText(ctx, 210, 220, "Unsaved changes");
    if (g_inputMode == InputMode::ConfirmDocument) drawText(ctx, 210, 246, "Save changes before closing this document?");
    else if (g_inputMode == InputMode::ConfirmWorkspace) drawText(ctx, 210, 246, "Save changes before opening another workspace?");
    else drawText(ctx, 210, 246, "Save changes before closing Developer Studio?");
    drawText(ctx, 230, 292, "[S] Save     [D] Discard     [C] Cancel");
}

static void drawShell(gx_app_context* ctx) {
    // Native compositor drawing is retained between redraws. Clear the
    // previous frame before publishing the current bounded surface so
    // repeated input/render cycles do not grow the retained layer forever.
    drawText(ctx, 0, 0, "\f");
    drawPanel(ctx, kWindowRect, 0x151B28u);
    drawPanel(ctx, kCommandRect, 0x243451u);
    drawPanel(ctx, kExplorerRect, 0x1D2636u);
    drawPanel(ctx, kEditorRect, 0x111722u);
    drawPanel(ctx, kOutputRect, 0x202A36u);
    drawPanel(ctx, kStatusRect, 0x243451u);
    drawText(ctx, 16, 30, "guideXOS Developer Studio");
    drawText(ctx, 290, 30, "File");
    drawText(ctx, 610, 30, "Debug");
    drawText(ctx, 700, 30, "Build");
    drawText(ctx, 340, 30, "Save");
    drawText(ctx, 400, 30, "Save All");
    drawText(ctx, 490, 30, "Refresh");
    drawText(ctx, 545, 30, "Ctrl+N");
    drawExplorer(ctx);
    drawOutline(ctx);
    drawEditor(ctx);
    drawSignaturePopup(ctx);
    drawCompletionPopup(ctx);
    drawTypePopup(ctx);
    drawOutputAndStatus(ctx);
    drawFindBar(ctx);
    drawProjectSearchPanel(ctx);
    drawReferencesPanel(ctx);
    drawSymbolSearch(ctx);
    drawDefinitionPicker(ctx);
    drawReferencePicker(ctx);
    drawRenamePanel(ctx);
    drawIncludeGraphPanel(ctx);
    drawIncludeTargetPicker(ctx);
    drawOwnershipPanel(ctx);
    drawDebugPanel(ctx);
    if (g_fileMenuOpen) {
        drawPanel(ctx, { 8, 42, 270, 202 }, 0x34496Au);
        drawText(ctx, 20, 64, "New Project");
        drawText(ctx, 20, 86, "Open Project");
        drawText(ctx, 20, 108, "Open Workspace");
        drawText(ctx, 20, 130, "Close Document");
        drawText(ctx, 20, 152, "Close Workspace");
        drawText(ctx, 20, 174, "Switch Header / Source (Alt+O)");
        drawText(ctx, 20, 196, "File Ownership (Alt+Shift+O)");
        drawText(ctx, 20, 218, "Exit");
    }
    if (g_buildMenuOpen) {
        drawPanel(ctx, { 300, 42, 260, 92 }, 0x34496Au);
        drawText(ctx, 312, 68, "Build Project");
        drawText(ctx, 312, 92, "Run Project (F5)");
        drawText(ctx, 312, 116, "Request Project Close");
    }
    if (g_debugMenuOpen) {
        drawPanel(ctx, { 580, 42, 360, 176 }, 0x34496Au);
        drawText(ctx, 592, 64, "Start Debugging (Ctrl+F5)");
        drawText(ctx, 592, 86, g_debugController.capabilities.canContinue ? "Continue" : "Continue (unavailable)");
        drawText(ctx, 592, 108, g_debugController.capabilities.canPause ? "Pause" : "Pause (unavailable)");
        drawText(ctx, 592, 130, "Stop Debugging");
        drawText(ctx, 592, 152, "Toggle Breakpoint (F9)");
        drawText(ctx, 592, 174, "Breakpoints");
        drawText(ctx, 592, 196, "Debug Session");
    }
    drawModal(ctx);
}

static void selectDocumentTab(int x) {
    int tabX = 278;
    for (uint32_t i = 0; i < kMaxOpenDocuments; ++i) {
        if (!g_controller.model.documents[i].used) continue;
        if (x >= tabX && x < tabX + 126) {
            dismissCompletion(nullptr, "document_changed", false);
            dismissSignatureHelp(nullptr, "document_changed", false);
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
    dismissCompletion(nullptr, "caret_moved", false);
    dismissSignatureHelp(nullptr, "caret_moved", false);
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
    if (g_inputMode == InputMode::ConfirmDebug) {
        if (keyCode == 27 || keyCode == 67 || keyCode == 99) {
            g_inputMode = InputMode::Normal;
            writeStudioOutput("Debug start canceled");
            return;
        }
        if (keyCode == 83 || keyCode == 115) {
            g_inputMode = InputMode::Normal;
            beginDebugBuild(ctx, BuildDirtyDecision::SaveAll);
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
    if (g_inputMode == InputMode::ConfirmDebugClose) {
        if (keyCode == 83 || keyCode == 115) {
            g_inputMode = InputMode::Normal;
            requestDebugStop(ctx);
        } else if (keyCode == 75 || keyCode == 107 || keyCode == 27) {
            g_inputMode = InputMode::Normal;
            writeStudioOutput("Studio remains open");
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
        dismissSignatureHelp(ctx, "workspace_changed", false);
        dismissTypeInfo(ctx, "workspace_changed", false);
        if (OwnershipGraphBuildIsActive(&g_ownershipService)) OwnershipGraphBuildCancel(&g_ownershipService, g_ownershipOperationId);
        g_ownershipPanelOpen = false;
        if (WorkspaceControllerCloseWorkspace(&g_controller, CloseDecision::Discard)) {
            DebugControllerClearBreakpoints(&g_debugController);
            DebugDwarfMapperReset(&g_debugMapper);
            g_debugPanelOpen = false;
        }
        TypeDatabaseClear(&g_typeDatabase);
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

static bool handleReferencePickerKey(gx_app_context* ctx, int keyCode, int action) {
    if (!g_referencePickerOpen || action != GX_KEY_ACTION_DOWN) return false;
    if (keyCode == 27) {
        closeReferencePicker();
        g_referencesPanelOpen = false;
        g_renameTargetPickerPending = false;
        return true;
    }
    if (keyCode == GX_KEY_UP) {
        if (g_referenceSelectedCandidate > 0) --g_referenceSelectedCandidate;
        ensureReferenceCandidateVisible();
        return true;
    }
    if (keyCode == GX_KEY_DOWN) {
        if (g_referenceSelectedCandidate + 1 < g_referenceTargetResolution.visibleCandidateCount) ++g_referenceSelectedCandidate;
        ensureReferenceCandidateVisible();
        return true;
    }
    if (keyCode == 33) {
        g_referenceSelectedCandidate = g_referenceSelectedCandidate > 18 ? g_referenceSelectedCandidate - 18 : 0;
        ensureReferenceCandidateVisible();
        return true;
    }
    if (keyCode == 34) {
        const uint32_t last = g_referenceTargetResolution.visibleCandidateCount == 0 ? 0 :
            g_referenceTargetResolution.visibleCandidateCount - 1;
        g_referenceSelectedCandidate = g_referenceSelectedCandidate + 18 < last ?
            g_referenceSelectedCandidate + 18 : last;
        ensureReferenceCandidateVisible();
        return true;
    }
    if (keyCode == 36) { g_referenceSelectedCandidate = 0; ensureReferenceCandidateVisible(); return true; }
    if (keyCode == 35) {
        g_referenceSelectedCandidate = g_referenceTargetResolution.visibleCandidateCount == 0 ? 0 :
            g_referenceTargetResolution.visibleCandidateCount - 1;
        ensureReferenceCandidateVisible();
        return true;
    }
    if (keyCode == 13 && g_referenceTargetResolution.candidateCount > 0) {
        activateReferenceTargetCandidate(ctx, g_referenceSelectedCandidate);
        return true;
    }
    return true;
}

static bool handleReferencesKey(gx_app_context* ctx, int keyCode, int action) {
    if (!g_referencesPanelOpen || action != GX_KEY_ACTION_DOWN) return false;
    const bool active = ReferenceSearchIsActive(&g_referenceSearch);
    if (keyCode == 27) {
        if (active) {
            ReferenceSearchCancel(&g_referenceSearch, g_referencesOperationId);
            referencesSetStatus(g_renameSearchPending ? "Cancelling Rename Symbol search..." : "Cancelling reference search...");
            logMarker(ctx, g_renameSearchPending
                      ? "GUIDEXOS_DEVELOPER_STUDIO_MARKER rename_cancel=REQUESTED"
                      : "GUIDEXOS_DEVELOPER_STUDIO_MARKER references_cancel=REQUESTED");
        } else {
            g_referencesPanelOpen = false;
            g_referencesResultsFocused = false;
            g_renameSearchPending = false;
            g_renameTargetPickerPending = false;
            stopReferenceSearch(ctx);
        }
        return true;
    }
    if (g_referencesResultsFocused) {
        if (keyCode == GX_KEY_UP) { moveReferenceSelection(-1); return true; }
        if (keyCode == GX_KEY_DOWN) { moveReferenceSelection(1); return true; }
        if (keyCode == 13) {
            activateReferenceResult(ctx, g_referencesSelectedGroup, g_referencesSelectedMatch);
            return true;
        }
        if (keyCode == 9) { g_referencesResultsFocused = false; g_editorFocused = true; return true; }
        return false;
    }
    if (keyCode == GX_KEY_UP || keyCode == GX_KEY_DOWN) {
        g_referencesResultsFocused = true;
        moveReferenceSelection(keyCode == GX_KEY_UP ? -1 : 1);
        return true;
    }
    if (keyCode == 13 && referencesTotalRows() > 0) {
        g_referencesResultsFocused = true;
        activateReferenceResult(ctx, g_referencesSelectedGroup, g_referencesSelectedMatch);
        return true;
    }
    return true;
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

static void renameEnsureSelectionVisible() {
    if (g_renameModel.candidateCount == 0) { g_renameSelectedCandidate = 0; g_renameScroll = 0; return; }
    if (g_renameSelectedCandidate >= g_renameModel.candidateCount)
        g_renameSelectedCandidate = g_renameModel.candidateCount - 1;
    if (g_renameSelectedCandidate < g_renameScroll) g_renameScroll = g_renameSelectedCandidate;
    if (g_renameSelectedCandidate >= g_renameScroll + 10u)
        g_renameScroll = g_renameSelectedCandidate - 9u;
}

static void renameInsertCharacter(char value) {
    if (value == '\0') return;
    const uint32_t length = lengthOf(g_renameModel.newName, sizeof(g_renameModel.newName));
    if (length >= kRenameMaxIdentifierBytes || g_renameNameCaret > length) return;
    for (uint32_t i = length; i > g_renameNameCaret; --i)
        g_renameModel.newName[i] = g_renameModel.newName[i - 1];
    g_renameModel.newName[g_renameNameCaret++] = value;
    g_renameModel.newName[length + 1] = '\0';
    RenameModelSetNewName(&g_renameModel, g_renameModel.newName, &g_symbolDatabase);
}

static void renameBackspace() {
    const uint32_t length = lengthOf(g_renameModel.newName, sizeof(g_renameModel.newName));
    if (g_renameNameCaret == 0 || g_renameNameCaret > length) return;
    for (uint32_t i = g_renameNameCaret - 1; i < length; ++i)
        g_renameModel.newName[i] = g_renameModel.newName[i + 1];
    --g_renameNameCaret;
    RenameModelSetNewName(&g_renameModel, g_renameModel.newName, &g_symbolDatabase);
}

static void renameDelete() {
    const uint32_t length = lengthOf(g_renameModel.newName, sizeof(g_renameModel.newName));
    if (g_renameNameCaret >= length) return;
    for (uint32_t i = g_renameNameCaret; i < length; ++i)
        g_renameModel.newName[i] = g_renameModel.newName[i + 1];
    RenameModelSetNewName(&g_renameModel, g_renameModel.newName, &g_symbolDatabase);
}

static bool applyRenameFromUi(gx_app_context* ctx) {
    RenameErrorCode validation = RenameErrorCode::None;
    if (!RenameValidateNewName(g_renameModel.currentName, g_renameModel.newName, &validation)) {
        referencesSetStatus(RenameErrorName(validation));
        g_renameModel.error = validation;
        return false;
    }
    if (g_renameModel.conflictSeverity == guidexos::developer_studio::RenameConflictSeverity::Blocking) {
        referencesSetStatus(g_renameModel.conflictMessage);
        return false;
    }
    logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER rename_apply_begin=PASS");
    const uint64_t transactionId = g_nextRenameTransactionId == 0 ? 1 : g_nextRenameTransactionId++;
    if (!RenameApply(&g_renameModel, &g_controller.model, g_controller.fileSystem, &g_symbolDatabase,
                     g_controller.model.projectGeneration, &g_renameUndo, transactionId)) {
        referencesSetStatus(RenameErrorName(g_renameModel.error));
        markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER rename_apply=FAIL", RenameErrorName(g_renameModel.error));
        return false;
    }
    logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER rename_revalidate=PASS");
    copyText(g_textScratch, sizeof(g_textScratch), "GUIDEXOS_DEVELOPER_STUDIO_MARKER rename_apply=PASS files=");
    appendUnsigned(g_textScratch, sizeof(g_textScratch), g_renameModel.plan.fileCount);
    appendText(g_textScratch, sizeof(g_textScratch), " edits=");
    appendUnsigned(g_textScratch, sizeof(g_textScratch), g_renameModel.plan.totalEdits);
    logMarker(ctx, g_textScratch);
    writeStudioOutput("Rename Symbol applied as one workspace operation");
    g_renamePanelOpen = false;
    g_editorFocused = true;
    referencesSetStatus("Rename Symbol applied. Ctrl+Z undoes the whole operation.");
    return true;
}

static bool handleRenameKey(gx_app_context* ctx, int keyCode, int action, int modifiers) {
    if (!g_renamePanelOpen || action != GX_KEY_ACTION_DOWN) return false;
    if (keyCode == 27) {
        g_renamePanelOpen = false;
        g_renameModel.state = RenameState::Cancelled;
        g_editorFocused = true;
        logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER rename_apply=FAIL reason=RENAME_CANCELLED");
        return true;
    }
    if (keyCode == 9) {
        g_renameNameFocused = !g_renameNameFocused;
        return true;
    }
    if (g_renameNameFocused) {
        if (keyCode == GX_KEY_LEFT) { if (g_renameNameCaret > 0) --g_renameNameCaret; return true; }
        if (keyCode == GX_KEY_RIGHT) {
            const uint32_t length = lengthOf(g_renameModel.newName, sizeof(g_renameModel.newName));
            if (g_renameNameCaret < length) ++g_renameNameCaret;
            return true;
        }
        if (keyCode == 36) { g_renameNameCaret = 0; return true; }
        if (keyCode == 35) { g_renameNameCaret = lengthOf(g_renameModel.newName, sizeof(g_renameModel.newName)); return true; }
        if (keyCode == 8) { renameBackspace(); return true; }
        if (keyCode == 46) { renameDelete(); return true; }
        if (keyCode == 13) { applyRenameFromUi(ctx); return true; }
        const char value = mapKeyToChar(keyCode, modifiers);
        if (value != '\0') { renameInsertCharacter(value); return true; }
        return true;
    }
    if (keyCode == GX_KEY_UP) {
        if (g_renameSelectedCandidate > 0) --g_renameSelectedCandidate;
        renameEnsureSelectionVisible();
        return true;
    }
    if (keyCode == GX_KEY_DOWN) {
        if (g_renameSelectedCandidate + 1 < g_renameModel.candidateCount) ++g_renameSelectedCandidate;
        renameEnsureSelectionVisible();
        return true;
    }
    if (keyCode == 32) {
        const RenameEditCandidate& candidate = g_renameModel.candidates[g_renameSelectedCandidate];
        RenameModelSetCandidateSelected(&g_renameModel, g_renameSelectedCandidate,
                                         candidate.state != RenameCandidateState::Selected);
        return true;
    }
    if (keyCode == 69 || keyCode == 101) { RenameModelSelectExact(&g_renameModel); return true; }
    if (keyCode == 67 || keyCode == 99) { RenameModelClearSelection(&g_renameModel); return true; }
    if (keyCode == 13) { applyRenameFromUi(ctx); return true; }
    return true;
}

static void handleNormalKey(gx_app_context* ctx, int keyCode, int action, int modifiers, bool& running) {
    if (action != GX_KEY_ACTION_DOWN) return;
    // Debugging is a global command: output/panel focus must not consume the
    // established Ctrl+F5 shortcut before it reaches the session controller.
    if (keyCode == 116 && modifiers == GX_KEY_MOD_CTRL) {
        requestDebug(ctx);
        return;
    }
    if (g_debugPanelOpen && handleDebugPanelKey(ctx, keyCode, action)) return;
    // F9 is unused by the existing editor shortcuts and follows the standard
    // debugger convention without changing the established F5 Run command.
    if (keyCode == 120 && modifiers == 0) {
        toggleBreakpointAtCaret(ctx);
        return;
    }
    if (g_typePopupOpen && handleTypeInfoKey(ctx, keyCode, action, modifiers)) return;
    if ((modifiers & GX_KEY_MOD_CTRL) && (modifiers & GX_KEY_MOD_ALT) &&
        (keyCode == 84 || keyCode == 116)) {
        openTypeInfo(ctx);
        return;
    }
    if (g_completionPopupOpen && (modifiers & GX_KEY_MOD_CTRL) && (modifiers & GX_KEY_MOD_SHIFT) && keyCode == 32) {
        dismissCompletion(ctx, "signature_help", false);
        openSignatureHelp(ctx);
        return;
    }
    if (g_completionPopupOpen && handleCompletionKey(ctx, keyCode, action, modifiers)) return;
    if (g_signaturePopupOpen && (modifiers & GX_KEY_MOD_CTRL) && !(modifiers & GX_KEY_MOD_SHIFT) && keyCode == 32) {
        dismissSignatureHelp(ctx, "completion", false);
        openCompletion(ctx);
        return;
    }
    if (g_signaturePopupOpen && handleSignatureKey(ctx, keyCode, action, modifiers)) return;
    if ((modifiers & GX_KEY_MOD_CTRL) && (modifiers & GX_KEY_MOD_SHIFT) && keyCode == 32) {
        openSignatureHelp(ctx);
        return;
    }
    if ((modifiers & GX_KEY_MOD_CTRL) && !(modifiers & GX_KEY_MOD_SHIFT) && keyCode == 32) {
        if (g_completionPopupOpen) refreshCompletion(ctx);
        else openCompletion(ctx);
        return;
    }
    if (g_ownershipPanelOpen && handleOwnershipKey(ctx, keyCode, action)) return;
    // Ctrl+O is an existing workspace command.  The ownership audit therefore
    // selects the non-conflicting Alt+O fallback instead of stealing either
    // Ctrl+K or Ctrl+O for a chord.
    if ((modifiers & GX_KEY_MOD_ALT) && keyCode == 79 && !(modifiers & GX_KEY_MOD_SHIFT)) {
        switchHeaderSource(ctx);
        return;
    }
    if ((modifiers & GX_KEY_MOD_ALT) && (modifiers & GX_KEY_MOD_SHIFT) && (keyCode == 79 || keyCode == 111)) {
        openFileOwnership(ctx);
        return;
    }
    if (g_includeTargetPickerOpen && handleIncludeTargetPickerKey(ctx, keyCode, action)) return;
    if (g_includeGraphPanelOpen && handleIncludeGraphKey(ctx, keyCode, action, modifiers)) return;
    if ((modifiers & GX_KEY_MOD_CTRL) && (modifiers & GX_KEY_MOD_SHIFT) && (keyCode == 73 || keyCode == 105)) {
        startIncludeGraph(ctx);
        return;
    }
    if (g_renamePanelOpen && handleRenameKey(ctx, keyCode, action, modifiers)) return;
    if (g_definitionPickerOpen && handleDefinitionPickerKey(ctx, keyCode, action)) return;
    if (g_referencePickerOpen && handleReferencePickerKey(ctx, keyCode, action)) return;
    if ((modifiers & GX_KEY_MOD_ALT) && keyCode == 123) {
        Document* current = WorkspaceControllerActiveDocument(&g_controller);
        const int32_t index = current ? relationshipSymbolUnderCaret(*current) : -1;
        const ProjectSymbol* symbol = index >= 0 ? SymbolDatabaseProjectSymbolAt(&g_symbolDatabase, static_cast<uint32_t>(index)) : nullptr;
        if (!symbol || !startRelationshipNavigation(ctx, symbol->symbol.declarationRole != SymbolDeclarationRole::Definition))
            definitionSetStatus("No declaration/definition relationship under caret.");
        return;
    }
    if ((modifiers & GX_KEY_MOD_CTRL) && !(modifiers & GX_KEY_MOD_SHIFT) && keyCode == 123) {
        if (!startRelationshipNavigation(ctx, true)) definitionSetStatus("Declaration not found in active project.");
        return;
    }
    if ((modifiers & GX_KEY_MOD_SHIFT) && keyCode == 123) {
        startFindAllReferences(ctx);
        return;
    }
    if (keyCode == 113 && g_editorFocused) {
        startRenameSymbol(ctx);
        return;
    }
    if ((modifiers & GX_KEY_MOD_CTRL) && !(modifiers & GX_KEY_MOD_SHIFT) && (keyCode == 84 || keyCode == 116)) {
        openSymbolSearch(ctx);
        return;
    }
    if (g_symbolSearchOpen && handleSymbolSearchKey(ctx, keyCode, action, modifiers)) return;
    if (g_symbolSearchOpen) return;
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
    if (g_projectSearchPanelOpen) return;
    if (g_referencesPanelOpen && handleReferencesKey(ctx, keyCode, action)) return;
    if (g_referencesPanelOpen) return;
    if ((modifiers & GX_KEY_MOD_CTRL) && (keyCode == 70 || keyCode == 102)) {
        openFindBar(false, true);
        return;
    }
    if ((modifiers & GX_KEY_MOD_CTRL) && (keyCode == 72 || keyCode == 104)) {
        openFindBar(true, true);
        return;
    }
    if (g_findBarOpen && handleFindKey(keyCode, action, modifiers)) return;
    if ((modifiers & GX_KEY_MOD_ALT) && keyCode == GX_KEY_LEFT) {
        Document* current = WorkspaceControllerActiveDocument(&g_controller);
        NavigationLocation currentLocation = {};
        NavigationLocation destination = {};
        if (!current || !captureNavigationLocation(*current, &currentLocation) ||
            !NavigationHistoryBack(&g_navigationHistory, currentLocation, &destination)) {
            definitionSetStatus("Navigation history is empty.");
            logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER navigation_back=EMPTY");
        } else {
            restoreNavigationLocation(ctx, destination);
        }
        return;
    }
    if ((modifiers & GX_KEY_MOD_ALT) && keyCode == GX_KEY_RIGHT) {
        Document* current = WorkspaceControllerActiveDocument(&g_controller);
        NavigationLocation currentLocation = {};
        NavigationLocation destination = {};
        if (!current || !captureNavigationLocation(*current, &currentLocation) ||
            !NavigationHistoryForward(&g_navigationHistory, currentLocation, &destination)) {
            definitionSetStatus("Forward navigation history is empty.");
            logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER navigation_forward=EMPTY");
        } else {
            restoreNavigationLocation(ctx, destination);
            logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER navigation_forward=PASS");
        }
        return;
    }
    if (keyCode == 123) {
        if (!g_editorFocused) { definitionSetStatus("Focus the editor before going to a definition."); return; }
        if (tryIncludeGraphDefinition(ctx)) return;
        startGoToDefinition(ctx);
        return;
    }
    if ((modifiers & GX_KEY_MOD_CTRL) && !(modifiers & GX_KEY_MOD_SHIFT) && (keyCode == 90 || keyCode == 122)) {
        if (undoCompletion(ctx)) return;
        if (!RenameUndoAvailable(&g_renameUndo)) return;
        RenameErrorCode undoError = RenameErrorCode::None;
        if (RenameUndoLast(&g_renameUndo, &g_controller.model, g_controller.fileSystem, &g_symbolDatabase,
                           g_controller.model.hasProject ? g_controller.model.project.projectId : "",
                           g_controller.model.projectGeneration, &undoError)) {
            logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER rename_undo=PASS");
            writeStudioOutput("Rename Symbol undone");
            referencesSetStatus("Rename Symbol undone.");
        } else {
            markerFailure(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER rename_undo=FAIL", RenameErrorName(undoError));
            referencesSetStatus(RenameErrorName(undoError));
        }
        return;
    }
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
        if (DebugControllerIsActive(&g_debugController) || g_debugWaitingForBuild) { writeStudioOutput("Debug in progress; stop the debug session first"); return; }
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
    const bool completionWasOpen = g_completionPopupOpen;
    const bool signatureWasOpen = g_signaturePopupOpen;
    bool changed = false;
    char typedValue = '\0';
    bool wasDirty = document->buffer.dirty;
    if (keyCode == GX_KEY_LEFT) { dismissCompletion(ctx, "caret_moved", false); dismissSignatureHelp(ctx, "caret_moved", false); TextBufferMoveLeft(&document->buffer); }
    else if (keyCode == GX_KEY_RIGHT) { dismissCompletion(ctx, "caret_moved", false); dismissSignatureHelp(ctx, "caret_moved", false); TextBufferMoveRight(&document->buffer); }
    else if (keyCode == GX_KEY_UP) { dismissCompletion(ctx, "caret_moved", false); dismissSignatureHelp(ctx, "caret_moved", false); TextBufferMoveUp(&document->buffer); }
    else if (keyCode == GX_KEY_DOWN) { dismissCompletion(ctx, "caret_moved", false); dismissSignatureHelp(ctx, "caret_moved", false); TextBufferMoveDown(&document->buffer); }
    else if (keyCode == 36) { dismissCompletion(ctx, "caret_moved", false); dismissSignatureHelp(ctx, "caret_moved", false); TextBufferHome(&document->buffer); }
    else if (keyCode == 35) { dismissCompletion(ctx, "caret_moved", false); dismissSignatureHelp(ctx, "caret_moved", false); TextBufferEnd(&document->buffer); }
    else if (keyCode == 8) { g_completionUndoAvailable = false; changed = TextBufferBackspace(&document->buffer); }
    else if (keyCode == 46) { g_completionUndoAvailable = false; dismissCompletion(ctx, "caret_moved", false); changed = TextBufferDelete(&document->buffer); }
    else if (keyCode == 13) { g_completionUndoAvailable = false; changed = TextBufferInsert(&document->buffer, "\n", 1); }
    else {
        g_completionUndoAvailable = false;
        typedValue = mapKeyToChar(keyCode, modifiers);
        if (typedValue != '\0') changed = TextBufferInsert(&document->buffer, &typedValue, 1);
    }
    updateSyntaxAfterEdit(ctx, document);
    markDirtyIfNeeded(ctx, wasDirty);
    if (changed) dismissTypeInfo(ctx, "document_changed", false);
    keepCaretVisible(document);
    const bool memberTrigger = changed && (typedValue == '.' ||
        (typedValue == '>' && document->buffer.caret >= 2 &&
         document->buffer.data[document->buffer.caret - 2] == '-'));
    if (completionWasOpen && changed && (keyCode == 8 || keyCode == 13 || typedValue != '\0'))
        refreshCompletion(ctx);
    else if (memberTrigger) openCompletion(ctx, false);
    if (signatureWasOpen && changed && (keyCode == 8 || keyCode == 13 || mapKeyToChar(keyCode, modifiers) != '\0'))
        refreshSignatureHelp(ctx);
    (void)running;
}

static void handleMouse(gx_app_context* ctx, const gx_event& event) {
    int x = event.param1;
    int y = event.param2;
    int action = GX_MOUSE_ACTION(event.param3);
    int button = GX_MOUSE_BUTTON(event.param3);
    if (g_ownershipPanelOpen) {
        if (action == GX_MOUSE_ACTION_WHEEL) {
            const uint32_t count = g_ownershipResolution.visibleCandidateCount;
            if (event.param4 > 0) g_ownershipScroll = g_ownershipScroll > 3 ? g_ownershipScroll - 3 : 0;
            else if (g_ownershipScroll + 15 < count) ++g_ownershipScroll;
            return;
        }
        if (button == GX_MOUSE_BUTTON_LEFT &&
            (action == GX_MOUSE_ACTION_DOWN || action == GX_MOUSE_ACTION_DOUBLE_CLICK) &&
            x >= 142 && x < 820 && y >= 208 && y < 580) {
            const uint32_t row = g_ownershipScroll + static_cast<uint32_t>((y - 208) / 24);
            if (row < g_ownershipResolution.visibleCandidateCount) {
                g_ownershipSelectedCandidate = row;
                if (action == GX_MOUSE_ACTION_DOUBLE_CLICK && g_ownershipResolution.candidateCount > row)
                    activateOwnershipCandidate(ctx, g_ownershipCandidateIndices[row]);
            }
        }
        return;
    }
    if (g_debugPanelOpen) {
        if (action == GX_MOUSE_ACTION_WHEEL) return;
        if (button == GX_MOUSE_BUTTON_LEFT &&
            (action == GX_MOUSE_ACTION_DOWN || action == GX_MOUSE_ACTION_DOUBLE_CLICK)) {
            if (y >= 94 && y < 122 && x >= 90 && x < 390) {
                g_debugPanelTab = x < 230 ? 0 : 1;
                g_debugSelectedBreakpoint = 0;
            } else if (g_debugPanelTab == 0 && y >= 130 && y < 580 && x >= 88 && x < 872) {
                const uint32_t row = static_cast<uint32_t>((y - 130) / kDebugPanelRowHeight);
                if (row < g_debugController.breakpointCount) {
                    g_debugSelectedBreakpoint = row;
                    if (action == GX_MOUSE_ACTION_DOUBLE_CLICK) navigateSelectedBreakpoint(ctx);
                }
            }
        }
        return;
    }
    if (g_includeTargetPickerOpen) {
        const auto* edge = IncludeGraphEdgeAt(&g_includeGraph, g_includeTargetEdgeIndex);
        const uint32_t count = edge ? edge->resolution.ambiguousCandidateCount : 0;
        if (action == GX_MOUSE_ACTION_WHEEL) {
            if (event.param4 > 0) g_includeTargetScroll = g_includeTargetScroll > 3 ? g_includeTargetScroll - 3 : 0;
            else if (g_includeTargetScroll + 18 < count) ++g_includeTargetScroll;
            return;
        }
        if (button == GX_MOUSE_BUTTON_LEFT &&
            (action == GX_MOUSE_ACTION_DOWN || action == GX_MOUSE_ACTION_DOUBLE_CLICK) &&
            x >= 144 && x < 816 && y >= 158 && y < 558) {
            const uint32_t row = g_includeTargetScroll + static_cast<uint32_t>((y - 158) / 22);
            if (row < count) {
                g_includeTargetSelected = row;
                if (action == GX_MOUSE_ACTION_DOUBLE_CLICK) activateIncludeTargetCandidate(ctx, row);
            }
        }
        return;
    }
    if (g_includeGraphPanelOpen) {
        if (action == GX_MOUSE_ACTION_WHEEL) {
            const uint32_t total = includeGraphRowCount();
            const uint32_t maximum = total > static_cast<uint32_t>(kIncludeGraphPanelMaxRows) ?
                total - static_cast<uint32_t>(kIncludeGraphPanelMaxRows) : 0;
            if (event.param4 > 0) g_includeGraphScroll = g_includeGraphScroll > 3 ? g_includeGraphScroll - 3 : 0;
            else g_includeGraphScroll = g_includeGraphScroll + 3 < maximum ? g_includeGraphScroll + 3 : maximum;
            return;
        }
        if (button == GX_MOUSE_BUTTON_LEFT &&
            (action == GX_MOUSE_ACTION_DOWN || action == GX_MOUSE_ACTION_DOUBLE_CLICK)) {
            if (y >= 110 && y < 136 && x >= 18 && x < 18 + 6 * kIncludeGraphPanelModeWidth) {
                g_includeGraphMode = static_cast<uint32_t>((x - 18) / kIncludeGraphPanelModeWidth);
                g_includeGraphScroll = 0;
                g_includeGraphSelectedRow = 0;
            } else if (y >= 140 && y < 174 && x >= 760) {
                startIncludeGraph(ctx);
            } else if (y >= kIncludeGraphPanelResultsTop - 12 && y < 612) {
                const uint32_t row = g_includeGraphScroll + static_cast<uint32_t>((y - (kIncludeGraphPanelResultsTop - 12)) / kIncludeGraphPanelRowHeight);
                const uint32_t total = includeGraphRowCount();
                if (row < total) {
                    g_includeGraphSelectedRow = row;
                    uint32_t edgeIndex = 0;
                    const auto* edge = includeGraphEdgeForRow(row, &edgeIndex);
                    if (action == GX_MOUSE_ACTION_DOUBLE_CLICK && edge) {
                        if (edge->resolution.state == IncludeResolutionState::Ambiguous && (g_includeGraphMode == 0 || g_includeGraphMode == 2)) {
                            Document* document = WorkspaceControllerActiveDocument(&g_controller);
                            if (document) captureNavigationLocation(*document, &g_includeNavigationOrigin);
                            g_includeNavigationOriginValid = true;
                            g_includeTargetEdgeIndex = edgeIndex;
                            g_includeTargetSelected = 0;
                            g_includeTargetScroll = 0;
                            g_includeTargetPickerOpen = true;
                            g_editorFocused = false;
                        } else activateIncludeEdge(ctx, edgeIndex);
                    }
                }
            }
        }
        return;
    }
    if (g_typePopupOpen) {
        gx_rect bounds = {};
        if (!typePopupBounds(&bounds)) { dismissTypeInfo(ctx, "popup_expired", false); return; }
        if (action == GX_MOUSE_ACTION_WHEEL) return;
        if (button == GX_MOUSE_BUTTON_LEFT && action == GX_MOUSE_ACTION_DOWN) {
            if (x >= bounds.x && x < bounds.x + bounds.width && y >= bounds.y && y < bounds.y + bounds.height) return;
            dismissTypeInfo(ctx, "mouse_dismiss", false);
        }
    }
    if (g_signaturePopupOpen) {
        gx_rect bounds = {};
        if (!signaturePopupBounds(&bounds)) { dismissSignatureHelp(ctx, "popup_expired", false); return; }
        if (action == GX_MOUSE_ACTION_WHEEL) {
            if (event.param4 > 0) g_signatureScroll = g_signatureScroll > 3 ? g_signatureScroll - 3 : 0;
            else if (g_signatureScroll + static_cast<uint32_t>(kSignaturePopupMaxRows) < g_signatureSession.candidateCount) ++g_signatureScroll;
            return;
        }
        if (button == GX_MOUSE_BUTTON_LEFT &&
            (action == GX_MOUSE_ACTION_DOWN || action == GX_MOUSE_ACTION_DOUBLE_CLICK) &&
            x >= bounds.x && x < bounds.x + bounds.width && y >= bounds.y + 30 &&
            y < bounds.y + bounds.height - 28) {
            const uint32_t row = g_signatureScroll + static_cast<uint32_t>((y - bounds.y - 30) / kSignaturePopupRowHeight);
            if (row < g_signatureSession.candidateCount) {
                g_signatureSession.selectedSignatureIndex = row;
                ensureSignatureSelectionVisible();
                signatureUpdateStatus();
            }
            return;
        }
        if (button == GX_MOUSE_BUTTON_LEFT && action == GX_MOUSE_ACTION_DOWN) {
            dismissSignatureHelp(ctx, "mouse_dismiss", false);
            return;
        }
    }
    if (g_completionPopupOpen) {
        gx_rect bounds = {};
        if (!completionPopupBounds(&bounds)) { dismissCompletion(ctx, "popup_expired", false); return; }
        if (action == GX_MOUSE_ACTION_WHEEL) {
            if (event.param4 > 0) g_completionSession.visibleStart = g_completionSession.visibleStart > 3 ? g_completionSession.visibleStart - 3 : 0;
            else if (g_completionSession.visibleStart + static_cast<uint32_t>(kCompletionPopupMaxRows) < g_completionSession.candidateCount)
                ++g_completionSession.visibleStart;
            return;
        }
        if (button == GX_MOUSE_BUTTON_LEFT &&
            (action == GX_MOUSE_ACTION_DOWN || action == GX_MOUSE_ACTION_DOUBLE_CLICK) &&
            x >= bounds.x && x < bounds.x + bounds.width && y >= bounds.y + 22 && y < bounds.y + bounds.height) {
            const uint32_t row = g_completionSession.visibleStart + static_cast<uint32_t>((y - bounds.y - 22) / kCompletionPopupRowHeight);
            if (row < g_completionSession.candidateCount) {
                g_completionSession.selectedIndex = row;
                if (action == GX_MOUSE_ACTION_DOUBLE_CLICK) acceptCompletion(ctx);
            }
            return;
        }
        if (button == GX_MOUSE_BUTTON_LEFT && action == GX_MOUSE_ACTION_DOWN) {
            dismissCompletion(ctx, "mouse_dismiss", false);
            return;
        }
    }
    if (g_renamePanelOpen) {
        if (action == GX_MOUSE_ACTION_WHEEL) {
            if (event.param4 > 0) g_renameScroll = g_renameScroll > 3 ? g_renameScroll - 3 : 0;
            else if (g_renameScroll + 10u < g_renameModel.candidateCount) ++g_renameScroll;
            return;
        }
        if (button != GX_MOUSE_BUTTON_LEFT ||
            (action != GX_MOUSE_ACTION_DOWN && action != GX_MOUSE_ACTION_DOUBLE_CLICK)) return;
        if (x >= 122 && x < 442 && y >= 120 && y < 148) {
            g_renameNameFocused = true;
            g_renameNameCaret = lengthOf(g_renameModel.newName, sizeof(g_renameModel.newName));
        } else if (y >= 204 && y < 620) {
            const uint32_t row = g_renameScroll + static_cast<uint32_t>((y - 204) / 38);
            if (row < g_renameModel.candidateCount) {
                g_renameNameFocused = false;
                g_renameSelectedCandidate = row;
                renameEnsureSelectionVisible();
                const RenameEditCandidate& candidate = g_renameModel.candidates[row];
                RenameModelSetCandidateSelected(&g_renameModel, row,
                    candidate.state != RenameCandidateState::Selected);
            }
        }
        drawShell(ctx);
        return;
    }
    if (g_definitionPickerOpen) {
        if (action == GX_MOUSE_ACTION_WHEEL) {
            if (event.param4 > 0) g_definitionScroll = g_definitionScroll > 3 ? g_definitionScroll - 3 : 0;
            else if (g_definitionScroll + 18 < g_definitionResolution.visibleCandidateCount) ++g_definitionScroll;
            ensureDefinitionSelectionVisible();
            return;
        }
        if (button == GX_MOUSE_BUTTON_LEFT &&
            (action == GX_MOUSE_ACTION_DOWN || action == GX_MOUSE_ACTION_DOUBLE_CLICK) &&
            x >= 148 && x < 812 && y >= 118 && y < 570) {
            const uint32_t row = g_definitionScroll + static_cast<uint32_t>((y - 118) / 24);
            if (row < g_definitionResolution.visibleCandidateCount) {
                g_definitionSelected = row;
                ensureDefinitionSelectionVisible();
                if (action == GX_MOUSE_ACTION_DOUBLE_CLICK) activateDefinitionCandidate(ctx, row);
            }
        }
        return;
    }
    if (g_referencePickerOpen) {
        if (action == GX_MOUSE_ACTION_WHEEL) {
            if (event.param4 > 0) g_referenceCandidateScroll = g_referenceCandidateScroll > 3 ?
                g_referenceCandidateScroll - 3 : 0;
            else if (g_referenceCandidateScroll + 18 < g_referenceTargetResolution.visibleCandidateCount)
                ++g_referenceCandidateScroll;
            ensureReferenceCandidateVisible();
            return;
        }
        if (button == GX_MOUSE_BUTTON_LEFT &&
            (action == GX_MOUSE_ACTION_DOWN || action == GX_MOUSE_ACTION_DOUBLE_CLICK) &&
            x >= 132 && x < 828 && y >= 142 && y < 590) {
            const uint32_t row = g_referenceCandidateScroll + static_cast<uint32_t>((y - 142) / 24);
            if (row < g_referenceTargetResolution.visibleCandidateCount) {
                g_referenceSelectedCandidate = row;
                ensureReferenceCandidateVisible();
                if (action == GX_MOUSE_ACTION_DOUBLE_CLICK)
                    activateReferenceTargetCandidate(ctx, row);
            }
        }
        return;
    }
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
    if (g_referencesPanelOpen) {
        if (action == GX_MOUSE_ACTION_WHEEL) {
            const uint32_t total = referencesTotalRows();
            const uint32_t maximum = total > static_cast<uint32_t>(kReferencesPanelMaxRows) ?
                total - static_cast<uint32_t>(kReferencesPanelMaxRows) : 0;
            if (event.param4 > 0) g_referencesScroll = g_referencesScroll > 3 ? g_referencesScroll - 3 : 0;
            else g_referencesScroll = g_referencesScroll + 3 < maximum ? g_referencesScroll + 3 : maximum;
            drawShell(ctx);
            return;
        }
        if (button != GX_MOUSE_BUTTON_LEFT ||
            (action != GX_MOUSE_ACTION_DOWN && action != GX_MOUSE_ACTION_DOUBLE_CLICK)) return;
        if (y >= 120 && y < 142 && x >= 840 && x < 912) {
            if (ReferenceSearchIsActive(&g_referenceSearch)) {
                ReferenceSearchCancel(&g_referenceSearch, g_referencesOperationId);
                referencesSetStatus("Cancelling reference search...");
                logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER references_cancel=REQUESTED");
            } else {
                g_referencesPanelOpen = false;
                g_referencesResultsFocused = false;
                stopReferenceSearch(ctx);
            }
        } else if (y >= kReferencesPanelResultsTop - 4 && y < kReferencesPanelTop + 580) {
            const uint32_t row = g_referencesScroll + static_cast<uint32_t>((y - kReferencesPanelResultsTop) / kReferencesPanelRowHeight);
            uint32_t groupIndex = 0;
            uint32_t matchIndex = 0;
            if (referenceMatchForRow(row, &groupIndex, &matchIndex)) {
                g_referencesResultsFocused = true;
                g_referencesSelectedGroup = groupIndex;
                g_referencesSelectedMatch = matchIndex;
                if (action == GX_MOUSE_ACTION_DOUBLE_CLICK)
                    activateReferenceResult(ctx, groupIndex, matchIndex);
            }
        }
        drawShell(ctx);
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
    if (y < 48 && x >= 580 && x < 700) { g_debugMenuOpen = !g_debugMenuOpen; g_fileMenuOpen = false; g_buildMenuOpen = false; drawShell(ctx); return; }
    if (y < 48 && x >= 700 && x < 800) { g_buildMenuOpen = !g_buildMenuOpen; g_fileMenuOpen = false; g_debugMenuOpen = false; drawShell(ctx); return; }
    if (g_debugMenuOpen && x >= 580 && x < 940 && y >= 42 && y < 218) {
        const uint32_t row = static_cast<uint32_t>((y - 42) / 22);
        if (row == 0) requestDebug(ctx);
        else if (row == 1) {
            DebugErrorCode error = DebugErrorCode::None;
            if (!DebugControllerContinue(&g_debugController, g_debugBackend, &error)) writeStudioOutput("Continue unavailable: backend capability is false");
        } else if (row == 2) {
            DebugErrorCode error = DebugErrorCode::None;
            if (!DebugControllerPause(&g_debugController, g_debugBackend, &error)) writeStudioOutput("Pause unavailable: backend capability is false");
        } else if (row == 3) requestDebugStop(ctx);
        else if (row == 4) toggleBreakpointAtCaret(ctx);
        else if (row == 5) { g_debugPanelTab = 0; g_debugPanelOpen = true; g_editorFocused = false; }
        else if (row == 6) { g_debugPanelTab = 1; g_debugPanelOpen = true; g_editorFocused = false; }
        g_debugMenuOpen = false;
        drawShell(ctx);
        return;
    }
    if (g_buildMenuOpen && x >= 300 && x < 560 && y >= 42 && y < 134) {
        if (y < 66) requestBuild(ctx);
        else if (y < 90) requestRun(ctx);
        else requestRunClose(ctx);
        g_buildMenuOpen = false;
        g_debugMenuOpen = false;
        drawShell(ctx);
        return;
    }
    if (g_fileMenuOpen && x >= 8 && x < 278 && y >= 42 && y < 244) {
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
            else if (DebugControllerIsActive(&g_debugController) || g_debugWaitingForBuild) writeStudioOutput("Debug in progress");
            else if (g_controller.model.open && guidexos::developer_studio::WorkspaceModelHasDirtyDocuments(&g_controller.model)) g_inputMode = InputMode::ConfirmWorkspace;
            else {
                stopProjectSearch(ctx);
                dismissSignatureHelp(ctx, "workspace_changed", false);
                dismissTypeInfo(ctx, "workspace_changed", false);
                if (IncludeGraphIsActive(&g_includeGraphOperation)) IncludeGraphCancel(&g_includeGraphOperation, g_includeGraphOperationId);
                if (OwnershipGraphBuildIsActive(&g_ownershipService)) OwnershipGraphBuildCancel(&g_ownershipService, g_ownershipOperationId);
                g_includeGraphPanelOpen = false;
                g_includeTargetPickerOpen = false;
                g_ownershipPanelOpen = false;
                if (WorkspaceControllerCloseWorkspace(&g_controller, CloseDecision::Discard)) {
                    DebugControllerClearBreakpoints(&g_debugController);
                    DebugDwarfMapperReset(&g_debugMapper);
                    g_debugPanelOpen = false;
                }
                TypeDatabaseClear(&g_typeDatabase);
            }
        } else if (y < 188) {
            switchHeaderSource(ctx);
        } else if (y < 210) {
            openFileOwnership(ctx);
        } else {
            if (BuildControllerIsActive(&g_buildController)) writeOutput("Build in progress; close blocked");
            else if (RunControllerIsActive(&g_runController)) { g_inputMode = InputMode::ConfirmRunClose; writeOutput("Project application is running: Close it first or keep Studio open"); }
            else if (g_debugWaitingForBuild) writeStudioOutput("Debug build in progress; close blocked");
            else if (DebugControllerIsActive(&g_debugController)) { g_inputMode = InputMode::ConfirmDebugClose; writeStudioOutput("Debug session is active: Stop it before closing Studio"); }
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
        if (x < kEditorTextX && (action == GX_MOUSE_ACTION_DOWN || action == GX_MOUSE_ACTION_DOUBLE_CLICK)) {
            if (toggleBreakpointAtMouse(ctx, x, y)) drawShell(ctx);
            return;
        }
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
    SymbolRelationshipGraphStorageInit(&g_relationshipStorage,
                                       g_relationshipGroups, kStudioRelationshipGroupCapacity,
                                       g_relationshipEdges, kStudioRelationshipEdgeCapacity,
                                       g_relationshipDeclarations, kStudioRelationshipEndpointCapacity,
                                       g_relationshipDefinitions, kStudioRelationshipEndpointCapacity,
                                       g_relationshipForwards, kStudioRelationshipEndpointCapacity,
                                       g_relationshipSymbolGroups, kSymbolMaxProjectSymbols);
    SymbolRelationshipGraphStorageInit(&g_relationshipBuildingStorage,
                                       g_relationshipBuildingGroups, kStudioRelationshipGroupCapacity,
                                       g_relationshipBuildingEdges, kStudioRelationshipEdgeCapacity,
                                       g_relationshipBuildingDeclarations, kStudioRelationshipEndpointCapacity,
                                       g_relationshipBuildingDefinitions, kStudioRelationshipEndpointCapacity,
                                       g_relationshipBuildingForwards, kStudioRelationshipEndpointCapacity,
                                       g_relationshipBuildingSymbolGroups, kSymbolMaxProjectSymbols);
    SymbolRelationshipGraphInit(&g_relationshipGraph, &g_relationshipStorage, "", 0, 0);
    SymbolRelationshipGraphInit(&g_relationshipBuildingGraph, &g_relationshipBuildingStorage, "", 0, 0);
    SymbolRelationshipGraphServiceInit(&g_relationshipService, &g_relationshipGraph, &g_relationshipBuildingGraph);
    OwnershipGraphStorageInit(&g_ownershipCompletedStorage,
                              g_ownershipCompletedFiles, kStudioOwnershipFileCapacity,
                              g_ownershipCompletedCandidates, kStudioOwnershipCandidateCapacity,
                              g_ownershipCompletedGroups, kStudioOwnershipGroupCapacity,
                              g_ownershipCompletedEvidence, kStudioOwnershipEvidenceCapacity,
                              g_ownershipCompletedHeaders, kStudioOwnershipEndpointCapacity,
                              g_ownershipCompletedSources, kStudioOwnershipEndpointCapacity,
                              g_ownershipCompletedCandidateIds, kStudioOwnershipCandidateCapacity * 2u);
    OwnershipGraphStorageInit(&g_ownershipBuildingStorage,
                              g_ownershipBuildingFiles, kStudioOwnershipFileCapacity,
                              g_ownershipBuildingCandidates, kStudioOwnershipCandidateCapacity,
                              g_ownershipBuildingGroups, kStudioOwnershipGroupCapacity,
                              g_ownershipBuildingEvidence, kStudioOwnershipEvidenceCapacity,
                              g_ownershipBuildingHeaders, kStudioOwnershipEndpointCapacity,
                              g_ownershipBuildingSources, kStudioOwnershipEndpointCapacity,
                              g_ownershipBuildingCandidateIds, kStudioOwnershipCandidateCapacity * 2u);
    OwnershipGraphInit(&g_ownershipCompletedGraph, &g_ownershipCompletedStorage, "", 0, 0, 0, 0);
    OwnershipGraphInit(&g_ownershipBuildingGraph, &g_ownershipBuildingStorage, "", 0, 0, 0, 0);
    OwnershipGraphServiceInit(&g_ownershipService, &g_ownershipCompletedGraph,
                              &g_ownershipBuildingGraph, &g_ownershipBuildingStorage);
    OwnershipBuildScratchInit(&g_ownershipScratch,
                              g_ownershipExactRefs, kStudioOwnershipFileCapacity,
                              g_ownershipNormalizedRefs, kStudioOwnershipFileCapacity,
                              g_ownershipModuleRefs, kStudioOwnershipFileCapacity,
                              g_ownershipExactBuckets, kStudioOwnershipFileCapacity,
                              g_ownershipNormalizedBuckets, kStudioOwnershipFileCapacity,
                              g_ownershipModuleBuckets, kStudioOwnershipFileCapacity,
                              g_ownershipPairSlots, kStudioOwnershipCandidateCapacity * 4u,
                              g_ownershipCandidateCounts, kStudioOwnershipFileCapacity,
                              g_ownershipRelationshipIdentities, kStudioOwnershipCandidateCapacity * 16u,
                              g_ownershipIncludeQueue, kIncludeGraphMaxNodes,
                              g_ownershipIncludeVisited, kIncludeGraphMaxNodes);
    g_ownershipOperationId = 0;
    g_ownershipPanelOpen = false;
    g_ownershipPickerOpen = false;
    g_ownershipTerminalReported = false;
    g_ownershipStatus[0] = '\0';
    CompletionSessionInit(&g_completionSession, g_completionCandidateStorage, kCompletionMaxRetainedCandidates);
    DocumentWordCacheInit(&g_completionWordCache, g_completionWordStorage, kCompletionMaxDocumentWords);
    g_completionPopupOpen = false;
    g_completionStatus[0] = '\0';
    g_completionProjectId[0] = '\0';
    g_completionUndoAvailable = false;
    SignatureHelpSessionInit(&g_signatureSession, g_signatureCandidateStorage, kSignatureMaxRetainedCandidates,
                             g_signatureParameterStorage, kSignatureMaxRetainedCandidates * kSignatureMaxParameters);
    g_signaturePopupOpen = false;
    g_signatureScroll = 0;
    g_signatureStatus[0] = '\0';
    TypeDatabaseInit(&g_typeDatabase, g_typeRecordStorage, kTypeMaxRecords,
                     g_typeDocumentStorage, sizeof(g_typeDocumentStorage) / sizeof(g_typeDocumentStorage[0]),
                     g_typeMemberBucketStorage, kTypeMaxMemberBuckets,
                     g_typeMemberIndexStorage, kTypeMaxRecords);
    g_typeInspection = TypeInspection();
    g_typePopupOpen = false;
    g_typeStatus[0] = '\0';
    g_typeDisplayScratch[0] = '\0';
    IncludeGraphStorageInit(&g_includeGraphStorage);
    IncludeGraphStorageInit(&g_includeGraphBuildingStorage);
    IncludeGraphInit(&g_includeGraph, &g_includeGraphStorage, "", 0);
    IncludeGraphInit(&g_includeGraphBuilding, &g_includeGraphBuildingStorage, "", 0);
    IncludeGraphBuildOperationInit(&g_includeGraphOperation);
    g_includeGraphOperationId = 0;
    g_includeGraphPanelOpen = false;
    g_includeGraphResultsFocused = false;
    g_includeGraphTerminalReported = false;
    g_includeGraphMode = 0;
    g_includeGraphScroll = 0;
    g_includeGraphSelectedRow = 0;
    g_includeGraphStatus[0] = '\0';
    g_includeTargetPickerOpen = false;
    g_includeNavigationOriginValid = false;
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
    ReferenceSearchServiceInit(&g_referenceSearch);
    ReferenceTargetInit(&g_referenceTarget);
    g_referenceTargetResolution = {};
    g_referenceQuery = {};
    g_referenceOrigin = {};
    g_referenceOriginValid = false;
    g_referencesPanelOpen = false;
    g_referencesResultsFocused = false;
    g_referencePickerOpen = false;
    g_referenceSelectedCandidate = 0;
    g_referenceCandidateScroll = 0;
    g_referencesScroll = 0;
    g_referencesSelectedGroup = 0;
    g_referencesSelectedMatch = 0;
    g_referencesOperationId = 0;
    g_referencesProjectGeneration = 0;
    g_referencesProjectId[0] = '\0';
    g_referencesStatus[0] = '\0';
    g_referencesTerminalReported = false;
    g_nextReferenceQueryId = 1;
    RenameModelInit(&g_renameModel);
    RenameUndoManagerInit(&g_renameUndo);
    g_renamePanelOpen = false;
    g_renameSearchPending = false;
    g_renameTargetPickerPending = false;
    g_renameNameFocused = true;
    g_renameNameCaret = 0;
    g_renameSelectedCandidate = 0;
    g_renameScroll = 0;
    g_nextRenameTransactionId = 1;
    g_symbolSearchOpen = false;
    g_symbolSearchCaseSensitive = false;
    g_symbolSearchCaret = 0;
    g_symbolSearchSelected = 0;
    g_symbolSearchScroll = 0;
    g_symbolSearchResultCount = 0;
    g_symbolSearchQuery[0] = '\0';
    g_outlineSelected = 0;
    g_outlineScroll = 0;
    NavigationHistoryInit(&g_navigationHistory);
    g_definitionResolution = {};
    g_definitionQuery = {};
    g_definitionOrigin = {};
    g_definitionOriginValid = false;
    g_definitionPickerOpen = false;
    g_definitionSelected = 0;
    g_definitionScroll = 0;
    g_definitionStatus[0] = '\0';
    g_nextDefinitionQueryId = 1;
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
    DebugControllerInit(&g_debugController);
    DebugDwarfMapperReset(&g_debugMapper);
    HostedDebugBackendInit(&g_hostedDebugBackend, developmentRunService());
    g_debugBackend = HostedDebugBackendCreate(&g_hostedDebugBackend);
    g_debugWaitingForBuild = false;
    g_debugTerminalReported = false;
    g_debugMenuOpen = false;
    g_debugPanelOpen = false;
    g_debugPanelTab = 0;
    g_debugSelectedBreakpoint = 0;
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
            pollIncludeGraph(ctx);
            pollOwnership(ctx);
            pollProjectSearch(ctx);
            pollReferences(ctx);
            pollBuild(ctx);
            pollRun(ctx);
            pollDebug(ctx);
            gx_event event;
            clear_event(&event);
            gx_result result = ctx->host->poll_event(ctx, &event,
                (IncludeGraphIsActive(&g_includeGraphOperation) || OwnershipGraphBuildIsActive(&g_ownershipService) ||
                 ProjectSearchIsActive(&g_projectSearch) || ReferenceSearchIsActive(&g_referenceSearch)) ? 50 : 500);
            if (result == GX_OK && event.window == g_window) {
                if (gx_event_is_paint(&event)) drawShell(ctx);
                else if (gx_event_is_close(&event)) {
                    if (BuildControllerIsActive(&g_buildController)) {
                        writeOutput("Build in progress; close blocked");
                        logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER build_close=BLOCKED");
                    } else if (RunControllerIsActive(&g_runController)) {
                        g_inputMode = InputMode::ConfirmRunClose;
                        writeOutput("Project application is running: Close it first or keep Studio open");
                    } else if (g_debugWaitingForBuild) {
                        writeStudioOutput("Debug build in progress; close blocked");
                    } else if (DebugControllerIsActive(&g_debugController)) {
                        g_inputMode = InputMode::ConfirmDebugClose;
                        writeStudioOutput("Debug session is active: Stop it before closing Studio");
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
                    else if (gx_event_is_escape_down(&event) && !g_findBarOpen && !g_projectSearchPanelOpen &&
                             !g_referencesPanelOpen && !g_referencePickerOpen && !g_symbolSearchOpen &&
                             !g_completionPopupOpen && !g_signaturePopupOpen && !g_typePopupOpen && !g_ownershipPanelOpen && !g_debugPanelOpen) {
                        if (BuildControllerIsActive(&g_buildController)) writeOutput("Build in progress; close blocked");
                        else if (RunControllerIsActive(&g_runController)) { g_inputMode = InputMode::ConfirmRunClose; writeOutput("Project application is running: Close it first or keep Studio open"); }
                        else if (g_debugWaitingForBuild) writeStudioOutput("Debug build in progress; close blocked");
                        else if (DebugControllerIsActive(&g_debugController)) { g_inputMode = InputMode::ConfirmDebugClose; writeStudioOutput("Debug session is active: Stop it before closing Studio"); }
                        else if (guidexos::developer_studio::WorkspaceModelHasDirtyDocuments(&g_controller.model)) g_inputMode = InputMode::ConfirmApplication;
                        else { logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER application_close=PASS"); logMarker(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER clean_close=PASS"); running = false; }
                    } else handleNormalKey(ctx, event.param1, event.param2, event.param3, running);
                    if (g_requestExit) running = false;
                    drawShell(ctx);
                } else if (event.type == GX_EVENT_MOUSE) {
                    handleMouse(ctx, event);
                    if (g_requestExit) running = false;
                }
                pollIncludeGraph(ctx);
                pollOwnership(ctx);
                pollBuild(ctx);
                pollRun(ctx);
                pollDebug(ctx);
                pollProjectSearch(ctx);
                pollReferences(ctx);
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
