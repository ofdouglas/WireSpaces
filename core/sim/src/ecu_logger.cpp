/*
 * @file  ecu_logger.cpp
 * @brief Minimal ECU-name-prefixed process logging.
 */

#include "wirespaces/sim/ecu_logger.h"

#include <utility>

namespace wirespaces::sim {

EcuLogger::EcuLogger(
    std::string ecu_name,
    std::FILE* const output_stream,
    std::FILE* const error_stream)
    : ecu_name_{std::move(ecu_name)}
    , output_stream_{output_stream}
    , error_stream_{error_stream} {}

void EcuLogger::info(const std::string_view message) const {
    write(output_stream_, message);
}

void EcuLogger::error(const std::string_view message) const {
    write(error_stream_, message);
}

void EcuLogger::write(
    std::FILE* const stream,
    const std::string_view message) const {
    if (stream == nullptr) {
        return;
    }

    static constexpr char kPrefixStart{'['};
    static constexpr char kPrefixEnd[]{"] "};
    static constexpr char kNewline{'\n'};

    static_cast<void>(std::fwrite(&kPrefixStart, sizeof(kPrefixStart), 1U, stream));
    static_cast<void>(
        std::fwrite(ecu_name_.data(), sizeof(char), ecu_name_.size(), stream));
    static_cast<void>(std::fwrite(kPrefixEnd, sizeof(char), 2U, stream));
    static_cast<void>(
        std::fwrite(message.data(), sizeof(char), message.size(), stream));
    static_cast<void>(std::fwrite(&kNewline, sizeof(kNewline), 1U, stream));
    static_cast<void>(std::fflush(stream));
}

}  // namespace wirespaces::sim
