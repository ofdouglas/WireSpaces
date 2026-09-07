/*
 * @file  simulator_tests.cpp
 * @brief GoogleTest coverage for the simulator shell.
 *
 * Test plan:
 * - Accept valid CLI input and reject missing, duplicate, malformed, or
 *   out-of-range values.
 * - Preserve network/application initialization and polling order.
 * - Exit cleanly when shutdown is requested.
 * - Schedule against monotonic deadlines without catch-up iteration bursts.
 * - Report network initialization and polling failures through exit codes.
 */

#include "sim/command_line.h"
#include "sim/ecu_application.h"
#include "sim/ecu_logger.h"
#include "sim/ecu_process.h"
#include "sim/monotonic_clock.h"
#include "sim/network_runtime.h"
#include "sim/shutdown_source.h"

#include <gtest/gtest.h>

#include <chrono>
#include <string>
#include <utility>
#include <vector>

namespace {

class FakeShutdown final : public wirespaces::sim::ShutdownSource {
public:
    bool isShutdownRequested() const override {
        return requested_;
    }

    void request() {
        requested_ = true;
    }

private:
    bool requested_{false};
};

class FakeClock final : public wirespaces::sim::MonotonicClock {
public:
    TimePoint now() const override {
        return current_time_;
    }

    void sleepUntil(const TimePoint deadline) override {
        sleep_deadlines_.push_back(deadline);
        current_time_ = deadline;
    }

    void advance(const std::chrono::microseconds duration) {
        current_time_ += duration;
    }

    const std::vector<TimePoint>& sleepDeadlines() const {
        return sleep_deadlines_;
    }

private:
    TimePoint current_time_{};
    std::vector<TimePoint> sleep_deadlines_{};
};

class FakeNetwork final : public wirespaces::sim::NetworkRuntime {
public:
    explicit FakeNetwork(std::vector<std::string>& events)
        : events_{events} {}

    bool init() override {
        events_.emplace_back("network.init");
        return init_result;
    }

    bool poll() override {
        events_.emplace_back("network.poll");
        return poll_result;
    }

    bool init_result{true};
    bool poll_result{true};

private:
    std::vector<std::string>& events_;
};

class FakeApplication final : public wirespaces::sim::EcuApplication {
public:
    FakeApplication(
        std::vector<std::string>& events,
        FakeShutdown& shutdown,
        const int shutdown_after_periodic_calls,
        FakeClock* const clock = nullptr,
        const std::chrono::microseconds work_duration =
            std::chrono::microseconds{0})
        : events_{events}
        , shutdown_{shutdown}
        , shutdown_after_periodic_calls_{shutdown_after_periodic_calls}
        , clock_{clock}
        , work_duration_{work_duration} {}

    void init() override {
        events_.emplace_back("application.init");
    }

    void periodic() override {
        events_.emplace_back("application.periodic");
        ++periodic_calls_;

        if (clock_ != nullptr) {
            clock_->advance(work_duration_);
        }
        if (periodic_calls_ >= shutdown_after_periodic_calls_) {
            shutdown_.request();
        }
    }

private:
    std::vector<std::string>& events_;
    FakeShutdown& shutdown_;
    int shutdown_after_periodic_calls_;
    FakeClock* clock_;
    std::chrono::microseconds work_duration_;
    int periodic_calls_{0};
};

wirespaces::sim::EcuProcessConfig testConfig() {
    wirespaces::sim::EcuProcessConfig config{};
    config.ecu_name = "test-ecu";
    config.loop_period = std::chrono::microseconds{1000};
    return config;
}

std::chrono::microseconds deadlineAsMicroseconds(
    const wirespaces::sim::MonotonicClock::TimePoint deadline) {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        deadline.time_since_epoch());
}

// Verifies valid options produce the requested name and period.
TEST(CommandLineTest, AcceptsValidConfiguration) {
    const char* const arguments[]{
        "ecu",
        "--loop-period-us",
        "2500",
        "--name",
        "sensor"};
    const auto result{wirespaces::sim::parseCommandLine(5, arguments)};

    EXPECT_EQ(result.status, wirespaces::sim::CommandLineStatus::kOk);
    EXPECT_EQ(result.config.ecu_name, "sensor");
    EXPECT_EQ(
        result.config.loop_period,
        std::chrono::microseconds{2500});
}

// Verifies the default loop period and explicit help behavior.
TEST(CommandLineTest, ProvidesDefaultsAndHelp) {
    const char* const default_arguments[]{"ecu", "--name", "consumer"};
    const auto default_result{
        wirespaces::sim::parseCommandLine(3, default_arguments)};
    EXPECT_EQ(
        default_result.status,
        wirespaces::sim::CommandLineStatus::kOk);
    EXPECT_EQ(
        default_result.config.loop_period,
        std::chrono::milliseconds{1});

    const char* const help_arguments[]{"ecu", "--help"};
    const auto help_result{
        wirespaces::sim::parseCommandLine(2, help_arguments)};
    EXPECT_EQ(
        help_result.status,
        wirespaces::sim::CommandLineStatus::kHelpRequested);
}

// Verifies malformed, missing, duplicate, and overflowing values are rejected.
TEST(CommandLineTest, RejectsInvalidConfiguration) {
    const char* const missing_name[]{"ecu"};
    const char* const zero_period[]{
        "ecu", "--name", "sensor", "--loop-period-us", "0"};
    const char* const malformed_period[]{
        "ecu", "--name", "sensor", "--loop-period-us", "10ms"};
    const char* const overflow_period[]{
        "ecu",
        "--name",
        "sensor",
        "--loop-period-us",
        "184467440737095516160"};
    const char* const duplicate_name[]{
        "ecu", "--name", "sensor", "--name", "other"};
    const char* const unknown_option[]{"ecu", "--name", "sensor", "--can"};

    struct InvalidCommandLine {
        const char* const* arguments;
        int argument_count;
        const char* description;
    };

    const InvalidCommandLine cases[]{
        {missing_name, 1, "missing name"},
        {zero_period, 5, "zero period"},
        {malformed_period, 5, "malformed period"},
        {overflow_period, 5, "overflowing period"},
        {duplicate_name, 5, "duplicate name"},
        {unknown_option, 4, "unknown option"},
    };

    for (const auto& test_case : cases) {
        SCOPED_TRACE(test_case.description);
        const auto result{
            wirespaces::sim::parseCommandLine(
                test_case.argument_count,
                test_case.arguments)};
        EXPECT_EQ(
            result.status,
            wirespaces::sim::CommandLineStatus::kError);
        EXPECT_FALSE(result.diagnostic.empty());
    }
}

// Verifies lifecycle ordering and immediate clean shutdown after periodic work.
TEST(EcuProcessTest, PreservesLifecycleOrderAndShutsDownCleanly) {
    std::vector<std::string> events{};
    FakeShutdown shutdown{};
    FakeClock clock{};
    FakeNetwork network{events};
    FakeApplication application{events, shutdown, 1};
    wirespaces::sim::EcuLogger logger{"test-ecu", nullptr, nullptr};
    wirespaces::sim::EcuProcess process{
        network, application, clock, shutdown, logger, testConfig()};

    const int result{process.run()};
    const std::vector<std::string> expected{
        "network.init",
        "application.init",
        "network.poll",
        "application.periodic"};

    EXPECT_EQ(
        result,
        static_cast<int>(wirespaces::sim::EcuProcessExitCode::kSuccess));
    EXPECT_EQ(events, expected);
    EXPECT_TRUE(clock.sleepDeadlines().empty());
}

// Verifies normal iterations sleep on an absolute monotonic schedule.
TEST(EcuProcessTest, SleepsOnAbsoluteMonotonicDeadlines) {
    std::vector<std::string> events{};
    FakeShutdown shutdown{};
    FakeClock clock{};
    FakeNetwork network{events};
    FakeApplication application{events, shutdown, 3};
    wirespaces::sim::EcuLogger logger{"test-ecu", nullptr, nullptr};
    wirespaces::sim::EcuProcess process{
        network, application, clock, shutdown, logger, testConfig()};

    static_cast<void>(process.run());
    const auto& deadlines{clock.sleepDeadlines()};

    ASSERT_EQ(deadlines.size(), 2U);
    EXPECT_EQ(
        deadlineAsMicroseconds(deadlines[0]),
        std::chrono::microseconds{1000});
    EXPECT_EQ(
        deadlineAsMicroseconds(deadlines[1]),
        std::chrono::microseconds{2000});
}

// Verifies an overrun skips missed slots instead of running catch-up loops.
TEST(EcuProcessTest, ResynchronizesAfterLoopOverrun) {
    std::vector<std::string> events{};
    FakeShutdown shutdown{};
    FakeClock clock{};
    FakeNetwork network{events};
    FakeApplication application{
        events,
        shutdown,
        2,
        &clock,
        std::chrono::microseconds{2500}};
    wirespaces::sim::EcuLogger logger{"test-ecu", nullptr, nullptr};
    wirespaces::sim::EcuProcess process{
        network, application, clock, shutdown, logger, testConfig()};

    static_cast<void>(process.run());
    const auto& deadlines{clock.sleepDeadlines()};

    ASSERT_EQ(deadlines.size(), 1U);
    EXPECT_EQ(
        deadlineAsMicroseconds(deadlines[0]),
        std::chrono::microseconds{3500});
}

// Verifies SIGINT and SIGTERM only set the process-local shutdown request.
TEST(SignalShutdownTest, ConvertsSignalsToShutdownRequests) {
    wirespaces::sim::SignalShutdown shutdown{};
    shutdown.reset();

    EXPECT_FALSE(shutdown.isShutdownRequested());
    ASSERT_TRUE(shutdown.install());

    EXPECT_EQ(std::raise(SIGINT), 0);
    EXPECT_TRUE(shutdown.isShutdownRequested());

    shutdown.reset();
    EXPECT_EQ(std::raise(SIGTERM), 0);
    EXPECT_TRUE(shutdown.isShutdownRequested());
    shutdown.reset();
}

// Verifies network initialization failure prevents application startup.
TEST(EcuProcessTest, StopsWhenNetworkInitializationFails) {
    std::vector<std::string> events{};
    FakeShutdown shutdown{};
    FakeClock clock{};
    FakeNetwork network{events};
    network.init_result = false;
    FakeApplication application{events, shutdown, 1};
    wirespaces::sim::EcuLogger logger{"test-ecu", nullptr, nullptr};
    wirespaces::sim::EcuProcess process{
        network, application, clock, shutdown, logger, testConfig()};

    const int result{process.run()};

    EXPECT_EQ(
        result,
        static_cast<int>(
            wirespaces::sim::EcuProcessExitCode::
                kNetworkInitializationFailed));
    EXPECT_EQ(events, std::vector<std::string>{"network.init"});
}

// Verifies polling failure terminates before periodic application work.
TEST(EcuProcessTest, StopsBeforePeriodicWorkWhenPollingFails) {
    std::vector<std::string> events{};
    FakeShutdown shutdown{};
    FakeClock clock{};
    FakeNetwork network{events};
    network.poll_result = false;
    FakeApplication application{events, shutdown, 1};
    wirespaces::sim::EcuLogger logger{"test-ecu", nullptr, nullptr};
    wirespaces::sim::EcuProcess process{
        network, application, clock, shutdown, logger, testConfig()};

    const int result{process.run()};
    const std::vector<std::string> expected{
        "network.init",
        "application.init",
        "network.poll"};

    EXPECT_EQ(
        result,
        static_cast<int>(
            wirespaces::sim::EcuProcessExitCode::kNetworkPollFailed));
    EXPECT_EQ(events, expected);
}

}  // namespace
