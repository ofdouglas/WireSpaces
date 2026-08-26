#pragma once
/*
 * @file  network_runtime.h
 * @brief Temporary simulator seam for polling the future WireSpaces stack.
 */

namespace wirespaces::sim {

/**
 * @brief Synchronous network lifecycle used by the simulated ECU superloop.
 *
 * This interface belongs to the host simulator, not to the WireSpaces protocol
 * core. A later increment will adapt the real Link Engine and routing stack to
 * this lifecycle without introducing a simulator-specific core transport.
 */
class NetworkRuntime {
public:
    virtual ~NetworkRuntime() = default;

    /**
     * @brief Initialize configured links and network state.
     * @return true on success; false when the ECU process must not start.
     */
    virtual bool init() = 0;

    /**
     * @brief Poll all configured links and process available network work.
     * @return true on success; false when the ECU process must terminate.
     */
    virtual bool poll() = 0;
};

}  // namespace wirespaces::sim
