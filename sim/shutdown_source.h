#pragma once
/*
 * @file  shutdown_source.h
 * @brief Injectable shutdown request contract and POSIX signal implementation.
 */

#include <csignal>

namespace wirespaces::sim {

/**
 * @brief Read-only shutdown request observed by EcuProcess.
 */
class ShutdownSource {
public:
    virtual ~ShutdownSource() = default;

    /**
     * @brief Check whether the process should leave its superloop.
     */
    virtual bool isShutdownRequested() const = 0;
};

/**
 * @brief Process-local SIGINT/SIGTERM shutdown request source.
 *
 * Signal handlers only assign a sig_atomic_t flag. install() and reset() must
 * be called from normal process context, not from a signal handler.
 */
class SignalShutdown final : public ShutdownSource {
public:
    /**
     * @brief Install shutdown handlers for SIGINT and SIGTERM.
     * @return true when both handlers were installed successfully.
     */
    [[nodiscard]] bool install();

    /**
     * @brief Clear a previous shutdown request before starting a process loop.
     */
    void reset();

    bool isShutdownRequested() const override;

private:
    static void handleSignal(int signal_number);
    static volatile std::sig_atomic_t shutdown_requested_;
};

}  // namespace wirespaces::sim
