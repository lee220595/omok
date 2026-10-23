#include "omok/match/forbidden.hpp"
#include <chrono>

namespace omok::match {
HintResult compute_forbidden(const HintInput& input, const std::function<bool()>& cancelled)
{
    const auto start = std::chrono::steady_clock::now();
    HintResult result{input.token};
    for (std::size_t i = 0; i < state::board_area; ++i) {
        if (cancelled && cancelled()) { return result; }
        if (input.game.board()[i] == state::Cell::Empty) {
            result.forbidden[i] = rules::analyze_board(input.game.board(),
                {*state::Coordinate::from_index(i), state::StoneColor::Black}).verdict == rules::Verdict::Forbidden;
        }
    }
    result.milliseconds = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    result.complete = true;
    return result;
}
bool HintCache::accept(const HintResult& result, Token current, bool enabled)
{
    if (!enabled || result.token != current || !result.complete || result.failed) { return false; }
    value_ = result;
    return true;
}
HintWorker::HintWorker() : thread_([this] { run(); }) {}
HintWorker::~HintWorker()
{
    {
        std::lock_guard lock(mutex_);
        stopping_ = true;
        ++generation_;
        pending_.reset();
    }
    ready_.notify_one();
    thread_.join();
}
void HintWorker::submit(HintInput input)
{
    {
        std::lock_guard lock(mutex_);
        ++generation_;
        pending_ = std::move(input);
        result_.reset();
    }
    ready_.notify_one();
}
void HintWorker::cancel()
{
    std::lock_guard lock(mutex_);
    ++generation_;
    pending_.reset();
    result_.reset();
}
std::optional<HintResult> HintWorker::take_result()
{
    std::lock_guard lock(mutex_);
    auto result = result_;
    result_.reset();
    return result;
}
void HintWorker::run()
{
    for (;;) {
        std::unique_lock lock(mutex_);
        ready_.wait(lock, [this] { return stopping_ || pending_.has_value(); });
        if (stopping_) { return; }
        const auto input = std::move(*pending_);
        pending_.reset();
        const auto generation = generation_.load();
        lock.unlock();
        HintResult result{input.token};
        try {
            result = compute_forbidden(input, [this, generation] { return generation_.load() != generation; });
        } catch (...) { result.failed = true; }
        lock.lock();
        if (!stopping_ && generation_.load() == generation) { result_ = result; }
    }
}
}
