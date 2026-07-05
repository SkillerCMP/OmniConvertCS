#pragma once

namespace omni::cli {

// Runs the command-line frontend. The caller owns argv for the duration of the call.
[[nodiscard]] int run_cli(int argc, char** argv);

} // namespace omni::cli
