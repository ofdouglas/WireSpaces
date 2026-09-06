#pragma once
/*
 * @file  ecu_process_config.h
 * @brief Configuration values for one simulated ECU process.
 */

#include <chrono>
#include <string>

namespace wirespaces::sim {

/**
 * @brief Host-only process configuration supplied by command-line parsing.
 */
struct EcuProcessConfig {
    std::string ecu_name{};
    std::chrono::microseconds loop_period{std::chrono::milliseconds{1}};

    /**
     * @brief Check invariants required by EcuProcess.
     * @return true when the name is non-empty and loop period is positive.
     */
    bool isValid() const;
};

inline bool EcuProcessConfig::isValid() const {
    return !ecu_name.empty() && (loop_period.count() > 0);
}

}  // namespace wirespaces::sim
