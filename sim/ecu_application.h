#pragma once
/*
 * @file  ecu_application.h
 * @brief Application lifecycle contract for a simulated ECU process.
 */

namespace wirespaces::sim {

/**
 * @brief ECU-specific application behavior driven by the simulator superloop.
 *
 * The simulator owns the process lifecycle but does not own the application.
 * Implementations must remain valid for the entire EcuProcess::run() call.
 * Methods execute synchronously on the process main thread.
 */
class EcuApplication {
public:
    virtual ~EcuApplication() = default;

    /**
     * @brief Initialize ECU-specific application state and services.
     *
     * Called exactly once after the injected network runtime initializes.
     */
    virtual void init() = 0;

    /**
     * @brief Perform one iteration of ECU-specific periodic work.
     *
     * Applications must not depend on exact host scheduling or loop frequency.
     */
    virtual void periodic() = 0;
};

}  // namespace wirespaces::sim
