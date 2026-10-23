#pragma once
#include "omok/match/controller.hpp"
#include <array>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>

namespace omok::match {
struct HintInput { Token token; state::GameState game; };
struct HintResult {
    Token token;
    std::array<bool, state::board_area> forbidden{};
    double milliseconds{};
    bool complete{};
    bool failed{};
    bool operator==(const HintResult&) const = default;
};
// Cancellation is checked BETWEEN candidates, never inside/truncating Renju recursion.
[[nodiscard]] HintResult compute_forbidden(const HintInput& input,
    const std::function<bool()>& cancelled = {});
class HintCache {
public:
    [[nodiscard]] bool accept(const HintResult& result, Token current, bool enabled);
    void clear() noexcept { value_.reset(); }
    [[nodiscard]] const std::optional<HintResult>& value() const noexcept { return value_; }
    bool operator==(const HintCache&) const = default;
private:
    std::optional<HintResult> value_;
};
// One persistent worker with latest-job coalescing; no HWND, mutable controller or UI callbacks.
class HintWorker {
public:
    HintWorker();
    ~HintWorker();
    void submit(HintInput input);
    void cancel();
    [[nodiscard]] std::optional<HintResult> take_result();
private:
    void run();
    std::mutex mutex_;
    std::condition_variable ready_;
    std::optional<HintInput> pending_;
    std::optional<HintResult> result_;
    std::atomic<std::uint64_t> generation_{};
    bool stopping_{};
    std::thread thread_;
};
}
