/*
 * @file  command_line.cpp
 * @brief Strict command-line parsing for a simulated ECU process.
 */

#include "sim/command_line.h"

#include <charconv>
#include <chrono>
#include <cstdint>
#include <limits>
#include <string_view>
#include <system_error>

namespace wirespaces::sim {
namespace {

[[nodiscard]] CommandLineResult errorResult(std::string diagnostic) {
    CommandLineResult result{};
    result.status = CommandLineStatus::kError;
    result.diagnostic = std::move(diagnostic);
    return result;
}

[[nodiscard]] bool parseLoopPeriod(
    std::string_view text,
    std::chrono::microseconds& loop_period) {
    std::uint64_t value{0U};
    const char* const begin{text.data()};
    const char* const end{text.data() + text.size()};
    const auto conversion{std::from_chars(begin, end, value)};

    using PeriodRepresentation = std::chrono::microseconds::rep;
    constexpr auto kMaximumPeriod{
        static_cast<std::uint64_t>(
            std::numeric_limits<PeriodRepresentation>::max())};

    if ((conversion.ec != std::errc{}) || (conversion.ptr != end)
        || (value == 0U) || (value > kMaximumPeriod)) {
        return false;
    }

    loop_period = std::chrono::microseconds{
        static_cast<PeriodRepresentation>(value)};
    return true;
}

}  // namespace

CommandLineResult parseCommandLine(
    const int argc,
    const char* const argv[]) {
    if ((argc < 1) || (argv == nullptr)) {
        return errorResult("invalid process argument array");
    }

    CommandLineResult result{};
    result.status = CommandLineStatus::kOk;

    bool name_supplied{false};
    bool loop_period_supplied{false};

    for (int index{1}; index < argc; ++index) {
        const char* const raw_argument{argv[index]};
        if (raw_argument == nullptr) {
            return errorResult("encountered a null command-line argument");
        }

        const std::string_view argument{raw_argument};
        if ((argument == "--help") || (argument == "-h")) {
            result.status = CommandLineStatus::kHelpRequested;
            return result;
        }

        if (argument == "--name") {
            if (name_supplied) {
                return errorResult("--name may be supplied only once");
            }
            if ((index + 1) >= argc) {
                return errorResult("--name requires a non-empty value");
            }

            const char* const name{argv[++index]};
            if ((name == nullptr) || (name[0] == '\0')) {
                return errorResult("--name requires a non-empty value");
            }

            result.config.ecu_name = name;
            name_supplied = true;
            continue;
        }

        if (argument == "--loop-period-us") {
            if (loop_period_supplied) {
                return errorResult(
                    "--loop-period-us may be supplied only once");
            }
            if ((index + 1) >= argc) {
                return errorResult(
                    "--loop-period-us requires a positive integer");
            }

            const char* const period_text{argv[++index]};
            if ((period_text == nullptr)
                || !parseLoopPeriod(period_text, result.config.loop_period)) {
                return errorResult(
                    "--loop-period-us must be a positive integer in range");
            }

            loop_period_supplied = true;
            continue;
        }

        return errorResult("unknown option: " + std::string{argument});
    }

    if (!name_supplied) {
        return errorResult("missing required option: --name <ecu-name>");
    }

    return result;
}

std::string commandLineUsage(const char* const executable_name) {
    const char* const safe_name{
        ((executable_name != nullptr) && (executable_name[0] != '\0'))
            ? executable_name
            : "ws_sim_counter_ecu"};

    return "Usage: " + std::string{safe_name}
        + " --name <ecu-name> [--loop-period-us <positive-integer>]\n"
          "       "
        + std::string{safe_name} + " --help\n";
}

}  // namespace wirespaces::sim
