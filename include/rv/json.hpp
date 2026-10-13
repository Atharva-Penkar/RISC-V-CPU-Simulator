// JSON documents for tools and the web frontend. They carry data only (ids, registers, values,
// enum names); explanation sentences are left to the viewer. docs/timeline-format.md describes them.
#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "rv/assembler.hpp"
#include "rv/timeline.hpp"

namespace rv {

inline constexpr int kJsonFormatVersion = 2;

/*{"format":"rv-timeline","ok":true,...}: the program, every instance, every cycle, outcome, stats, final state*/
std::string toJson(const Timeline& timeline);

/*{"format":"rv-program","ok":true,"instructions":[...]}: an assembled program with its source map*/
std::string toJson(const AssembledProgram& program);

/*{"format":format,"ok":false,"error":{"stage":"assemble","diagnostics":[...]}}*/
std::string diagnosticsToJson(std::string_view format, const std::vector<Diagnostic>& diagnostics);

}  // namespace rv
