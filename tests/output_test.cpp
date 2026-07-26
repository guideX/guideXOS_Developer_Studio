#include "developer_studio_output.h"

#include <cassert>
#include <cstring>
#include <iostream>

using namespace guidexos::developer_studio;

static OutputRecord record(OutputSeverity severity, OutputCategory category, const char* text) {
    OutputRecord value = {};
    value.source = OutputSource::Build;
    value.severity = severity;
    value.category = category;
    std::strcpy(value.text, text);
    return value;
}

int main() {
    static OutputService service;
    OutputServiceInit(&service);
    const uint64_t build = OutputServiceBeginOperation(&service, OutputOperationType::Build, "com.example.output");
    const uint64_t run = OutputServiceBeginOperation(&service, OutputOperationType::Run, "com.example.output");
    assert(build != 0 && run != 0);

    OutputErrorCode error = OutputErrorCode::None;
    assert(OutputServiceAppendRecord(&service, build, record(OutputSeverity::Information, OutputCategory::General, "one"), &error));
    assert(OutputServiceAppendRecord(&service, build, record(OutputSeverity::Warning, OutputCategory::BuildDiagnostic, "warning"), &error));
    OutputRecord runRecord = record(OutputSeverity::Information, OutputCategory::RunLifecycle, "running");
    runRecord.source = OutputSource::Run;
    assert(OutputServiceAppendRecord(&service, run, runRecord, &error));
    assert(OutputServiceRecordAt(&service, 0)->sequence < OutputServiceRecordAt(&service, 1)->sequence);
    assert(OutputServiceRecordAt(&service, 0)->operationId == build);
    assert(OutputServiceProblemCount(&service, "com.example.output") == 1);
    uint32_t warnings = 0;
    uint32_t errors = 0;
    OutputServiceProblemCounts(&service, "com.example.output", &warnings, &errors);
    assert(warnings == 1 && errors == 0);
    assert(OutputServiceFilteredCount(&service, OutputChannel::Build) == 2);
    assert(OutputServiceFilteredCount(&service, OutputChannel::Run) == 1);

    assert(OutputServiceCompleteOperation(&service, build, false, "Build Failed", &error));
    assert(!OutputServiceCompleteOperation(&service, build, false, "duplicate", &error));
    assert(error == OutputErrorCode::OperationStale);
    assert(!OutputServiceAppendRecord(&service, build, record(OutputSeverity::Information, OutputCategory::General, "stale"), &error));
    assert(error == OutputErrorCode::OperationStale);

    static OutputService bounded;
    OutputServiceInit(&bounded);
    const uint64_t boundedOperation = OutputServiceBeginOperation(&bounded, OutputOperationType::Build, "com.example.bounded");
    for (uint32_t i = 0; i < kMaxOutputRecordsPerOperation + 3; ++i) {
        char text[32] = {};
        std::strcpy(text, "line");
        assert(!OutputServiceAppendText(&bounded, boundedOperation, OutputSource::Build, OutputSeverity::Information,
                                        OutputCategory::General, OutputStream::StandardOutput, text, "com.example.bounded", &error) ||
               error == OutputErrorCode::RecordLimit || error == OutputErrorCode::None);
    }
    assert(OutputServiceRecordCount(&bounded) <= kMaxOutputRecordsPerOperation);
    uint32_t truncationCount = 0;
    for (uint32_t i = 0; i < OutputServiceRecordCount(&bounded); ++i) {
        const OutputRecord* value = OutputServiceRecordAt(&bounded, i);
        if (value && value->isTruncated) ++truncationCount;
    }
    assert(truncationCount == 1);

    static OutputService diagnostics;
    OutputServiceInit(&diagnostics);
    const uint64_t diagnosticOperation = OutputServiceBeginOperation(&diagnostics, OutputOperationType::Build, "com.example.diag");
    assert(OutputServiceAppendBuildLine(&diagnostics, diagnosticOperation, "D:/work/project", "com.example.diag",
                                        OutputStream::StandardError, "src/main.cpp:12:5: error: expected ';'", &error));
    assert(OutputServiceAppendBuildLine(&diagnostics, diagnosticOperation, "D:/work/project", "com.example.diag",
                                        OutputStream::StandardError, "src\\main.cpp(13,7): warning C1234: old API", &error));
    assert(OutputServiceAppendBuildLine(&diagnostics, diagnosticOperation, "D:/work/project", "com.example.diag",
                                        OutputStream::StandardError, "LINK : fatal error LNK1104: cannot open file", &error));
    assert(OutputServiceAppendBuildLine(&diagnostics, diagnosticOperation, "D:/work/project", "com.example.diag",
                                        OutputStream::StandardOutput, "ordinary sentence containing error words", &error));
    assert(OutputServiceProblemCount(&diagnostics, "com.example.diag") == 3);
    const OutputRecord* compiler = OutputServiceProblemAt(&diagnostics, "com.example.diag", 0);
    assert(compiler && compiler->hasLocation && std::strcmp(compiler->relativeFilePath, "src/main.cpp") == 0 && compiler->line == 12 && compiler->column == 5);
    const OutputRecord* linker = OutputServiceProblemAt(&diagnostics, "com.example.diag", 2);
    assert(linker && linker->source == OutputSource::Linker && !linker->hasLocation && std::strcmp(linker->diagnosticCode, "LNK1104") == 0);

    OutputRecord parsed = {};
    assert(ParseBuildDiagnostic("D:/work/project", "com.example.diag", "\"src/file.cpp\":22: warning: old", OutputStream::StandardError, &parsed, &error));
    assert(parsed.hasLocation && std::strcmp(parsed.relativeFilePath, "src/file.cpp") == 0 && parsed.line == 22 && parsed.column == 0);
    assert(ParseBuildDiagnostic("D:/work/project", "com.example.diag", "D:/work/project/src/main.cpp:5:34: error: expected expression", OutputStream::StandardError, &parsed, &error));
    assert(parsed.hasLocation && std::strcmp(parsed.relativeFilePath, "src/main.cpp") == 0 && parsed.line == 5 && parsed.column == 34);
    assert(ParseBuildDiagnostic("D:/work/project", "com.example.diag", "D:/outside/file.cpp:22:5: error: bad", OutputStream::StandardError, &parsed, &error));
    assert(!parsed.hasLocation && error == OutputErrorCode::DiagnosticPathOutsideProject);
    assert(ParseBuildDiagnostic("D:/work/project", "com.example.diag", "src dir/main.cpp(4,2): warning C1: old", OutputStream::StandardError, &parsed, &error));
    assert(parsed.hasLocation && std::strcmp(parsed.relativeFilePath, "src dir/main.cpp") == 0 && parsed.line == 4 && parsed.column == 2);
    assert(ParseBuildDiagnostic("D:/work/project", "com.example.diag", "src/main.cpp:42949672960:5: error: huge", OutputStream::StandardError, &parsed, &error));
    assert(!parsed.hasLocation && error == OutputErrorCode::InvalidDiagnosticLine);
    assert(!ParseBuildDiagnostic("D:/work/project", "com.example.diag", "src/main.cpp:5: ^", OutputStream::StandardError, &parsed, &error));
    assert(!ParseBuildDiagnostic("D:/work/project", "com.example.diag", "the error count is zero", OutputStream::StandardOutput, &parsed, &error));

    assert(OutputServiceClearProblemsForProject(&diagnostics, "com.example.diag") == 3);
    assert(OutputServiceProblemCount(&diagnostics, "com.example.diag") == 0);
    std::cout << "Developer Studio output service and diagnostic parser PASS\n";
    return 0;
}
