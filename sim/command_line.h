#pragma once
/*
 * @file  command_line.h
 * @brief Strict command-line parsing for a simulated ECU process.
 */

#include "sim/ecu_process_config.h"

#include <string>

namespace wirespaces::sim {

enum class CommandLineStatus {
    kOk,
    kHelpRequested,
    kError,
};

/**
 * @brief Result of parsing simulator process command-line options.
 */
struct CommandLineResult {
    CommandLineStatus status{CommandLineStatus::kError};
    EcuProcessConfig config{};
    std::string diagnostic{};
};

/**
 * @brief Parse ECU name and superloop period from command-line arguments.
 *
 * Supported options are --name <ecu-name>, --loop-period-us <positive integer>,
 * and --help. Options may appear in any order and may be supplied only once.
 *
 * @param[in] argc Number of entries in argv.
 * @param[in] argv Argument array whose strings remain owned by the caller.
 * @return Parsed configuration, help request, or an actionable diagnostic.
 */
[[nodiscard]] CommandLineResult parseCommandLine(
    int argc,
    const char* const argv[]);

/**
 * @brief Build usage text for the supplied executable name.
 */
std::string commandLineUsage(const char* executable_name);

}  // namespace wirespaces::sim
