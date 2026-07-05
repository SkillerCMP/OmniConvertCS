#pragma once
#include <optional>
namespace omni::cli {
[[nodiscard]] std::optional<int> try_run_analyze_cli(int argc, char** argv);
void print_analyze_usage();
}
