/*
 * @file  signal_shutdown.cpp
 * @brief SIGINT/SIGTERM-backed shutdown request source.
 */

#include "sim/shutdown_source.h"

namespace wirespaces::sim {

volatile std::sig_atomic_t SignalShutdown::shutdown_requested_{0};

bool SignalShutdown::install() {
    const auto interrupt_result{std::signal(SIGINT, &SignalShutdown::handleSignal)};
    const auto terminate_result{
        std::signal(SIGTERM, &SignalShutdown::handleSignal)};
    return (interrupt_result != SIG_ERR) && (terminate_result != SIG_ERR);
}

void SignalShutdown::reset() {
    shutdown_requested_ = 0;
}

bool SignalShutdown::isShutdownRequested() const {
    return shutdown_requested_ != 0;
}

void SignalShutdown::handleSignal(const int signal_number) {
    static_cast<void>(signal_number);
    shutdown_requested_ = 1;
}

}  // namespace wirespaces::sim
