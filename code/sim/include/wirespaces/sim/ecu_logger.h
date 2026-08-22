#pragma once
/*
 * @file  ecu_logger.h
 * @brief Minimal ECU-name-prefixed process logging.
 */

#include <cstdio>
#include <string>
#include <string_view>

namespace wirespaces::sim {

/**
 * @brief Writes inspectable simulator logs without a logging framework.
 *
 * Calls are synchronous and intended for the single-threaded MVP superloop.
 * The referenced FILE streams must remain valid for this object's lifetime.
 */
class EcuLogger {
public:
    EcuLogger(
        std::string ecu_name,
        std::FILE* output_stream = stdout,
        std::FILE* error_stream = stderr);

    /**
     * @brief Write an informational line to the configured output stream.
     */
    void info(std::string_view message) const;

    /**
     * @brief Write an error line to the configured error stream.
     */
    void error(std::string_view message) const;

private:
    void write(std::FILE* stream, std::string_view message) const;

    std::string ecu_name_;
    std::FILE* output_stream_;
    std::FILE* error_stream_;
};

}  // namespace wirespaces::sim
