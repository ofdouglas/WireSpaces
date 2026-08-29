#pragma once
/*
 * @file  ecu_process.h
 * @brief Synchronous lifecycle and superloop for one simulated ECU.
 */

#include "sim/ecu_application.h"
#include "sim/ecu_logger.h"
#include "sim/ecu_process_config.h"
#include "sim/monotonic_clock.h"
#include "sim/network_runtime.h"
#include "sim/shutdown_source.h"

namespace wirespaces::sim {

enum class EcuProcessExitCode : int {
    kSuccess = 0,
    kInvalidConfiguration = 2,
    kNetworkInitializationFailed = 3,
    kNetworkPollFailed = 4,
};

/**
 * @brief Owns no dependencies and drives them synchronously on one thread.
 *
 * Every injected object and the configuration must remain valid throughout
 * run(). A single EcuProcess instance is not reentrant or thread-safe.
 */
class EcuProcess {
public:
    EcuProcess(
        NetworkRuntime& network,
        EcuApplication& application,
        MonotonicClock& clock,
        ShutdownSource& shutdown,
        EcuLogger& logger,
        EcuProcessConfig config);

    /**
     * @brief Initialize dependencies and run until shutdown or network failure.
     *
     * The network initializes before the application. Each iteration polls the
     * network before calling application periodic work.
     *
     * @return An integer representation of EcuProcessExitCode.
     */
    [[nodiscard]] int run();

private:
    [[nodiscard]] int exit(EcuProcessExitCode exit_code) const;

    NetworkRuntime& network_;
    EcuApplication& application_;
    MonotonicClock& clock_;
    ShutdownSource& shutdown_;
    EcuLogger& logger_;
    EcuProcessConfig config_;
};

}  // namespace wirespaces::sim
