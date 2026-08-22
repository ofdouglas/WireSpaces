/*
 * @file  counter_ecu.cpp
 * @brief Minimal lifecycle-only simulated ECU example.
 */

#include "wirespaces/sim/command_line.h"
#include "wirespaces/sim/ecu_application.h"
#include "wirespaces/sim/ecu_logger.h"
#include "wirespaces/sim/ecu_process.h"
#include "wirespaces/sim/monotonic_clock.h"
#include "wirespaces/sim/network_runtime.h"
#include "wirespaces/sim/shutdown_source.h"

#include <cstdio>
#include <cstdint>
#include <string>

namespace {

class NoOpNetworkRuntime final : public wirespaces::sim::NetworkRuntime {
public:
    bool init() override {
        return true;
    }

    bool poll() override {
        return true;
    }
};

class CounterApplication final : public wirespaces::sim::EcuApplication {
public:
    explicit CounterApplication(wirespaces::sim::EcuLogger& logger)
        : logger_{logger} {}

    void init() override {
        logger_.info("counter application initialized");
    }

    void periodic() override {
        ++periodic_count_;
        if ((periodic_count_ % kLogInterval) == 0U) {
            logger_.info(
                "counter periodic count=" + std::to_string(periodic_count_));
        }
    }

private:
    static constexpr std::uint64_t kLogInterval{1000U};

    wirespaces::sim::EcuLogger& logger_;
    std::uint64_t periodic_count_{0U};
};

}  // namespace

int main(const int argc, const char* const argv[]) {
    const auto command_line{wirespaces::sim::parseCommandLine(argc, argv)};
    const char* const executable_name{
        ((argc > 0) && (argv != nullptr)) ? argv[0] : nullptr};

    if (command_line.status == wirespaces::sim::CommandLineStatus::kHelpRequested) {
        const auto usage{wirespaces::sim::commandLineUsage(executable_name)};
        static_cast<void>(std::fputs(usage.c_str(), stdout));
        return 0;
    }

    if (command_line.status == wirespaces::sim::CommandLineStatus::kError) {
        static_cast<void>(
            std::fprintf(stderr, "error: %s\n", command_line.diagnostic.c_str()));
        const auto usage{wirespaces::sim::commandLineUsage(executable_name)};
        static_cast<void>(std::fputs(usage.c_str(), stderr));
        return 2;
    }

    wirespaces::sim::EcuLogger logger{command_line.config.ecu_name};
    wirespaces::sim::SignalShutdown shutdown{};
    shutdown.reset();
    if (!shutdown.install()) {
        logger.error("failed to install SIGINT/SIGTERM handlers");
        return 3;
    }

    NoOpNetworkRuntime network{};
    CounterApplication application{logger};
    wirespaces::sim::SystemMonotonicClock clock{};
    wirespaces::sim::EcuProcess process{
        network,
        application,
        clock,
        shutdown,
        logger,
        command_line.config};
    return process.run();
}
