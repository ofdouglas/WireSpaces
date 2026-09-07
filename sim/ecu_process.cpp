/*
 * @file  ecu_process.cpp
 * @brief Synchronous lifecycle and superloop for one simulated ECU.
 */

#include "sim/ecu_process.h"

#include <utility>

namespace wirespaces::sim {

EcuProcess::EcuProcess(
    NetworkRuntime& network,
    EcuApplication& application,
    MonotonicClock& clock,
    ShutdownSource& shutdown,
    EcuLogger& logger,
    EcuProcessConfig config)
    : network_{network}
    , application_{application}
    , clock_{clock}
    , shutdown_{shutdown}
    , logger_{logger}
    , config_{std::move(config)} {}

int EcuProcess::run() {
    if (!config_.isValid()) {
        logger_.error("invalid ECU process configuration");
        return exit(EcuProcessExitCode::kInvalidConfiguration);
    }

    logger_.info("initializing network");
    if (!network_.init()) {
        logger_.error("network initialization failed");
        return exit(EcuProcessExitCode::kNetworkInitializationFailed);
    }

    application_.init();
    logger_.info("ECU process started");

    auto next_deadline{clock_.now()};

    while (!shutdown_.isShutdownRequested()) {
        if (!network_.poll()) {
            logger_.error("network poll failed");
            return exit(EcuProcessExitCode::kNetworkPollFailed);
        }

        application_.periodic();
        if (shutdown_.isShutdownRequested()) {
            break;
        }

        next_deadline += config_.loop_period;
        const auto current_time{clock_.now()};
        if (next_deadline <= current_time) {
            // Skip missed slots rather than executing catch-up iterations.
            next_deadline = current_time + config_.loop_period;
        }

        clock_.sleepUntil(next_deadline);
    }

    logger_.info("shutdown complete");
    return exit(EcuProcessExitCode::kSuccess);
}

int EcuProcess::exit(const EcuProcessExitCode exit_code) const {
    return static_cast<int>(exit_code);
}

}  // namespace wirespaces::sim
