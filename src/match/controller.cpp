#include "omok/match/controller.hpp"
#include <limits>
#include <type_traits>
#include <utility>

namespace omok::match {
namespace {
bool valid(state::Participant p) { return p == state::Participant::A || p == state::Participant::B; }
state::Participant other(state::Participant p)
{
    return p == state::Participant::A ? state::Participant::B : state::Participant::A;
}
}
Response Controller::start(const opening::Session& source)
{
    const auto continuation = source.handoff().continuation;
    if (source.mode() != opening::Mode::PreparedRenju
        || (continuation != opening::Continuation::RenjuReady && continuation != opening::Continuation::Terminal)) {
        return {Error::NotReady};
    }
    if (source.game().history().empty()) { return {Error::EmptyOpening}; }
    if (token_.session == std::numeric_limits<std::uint64_t>::max()) { return {Error::VersionExhausted}; }
    Controller candidate;
    candidate.opening_ = source;
    candidate.game_ = source.game();
    candidate.token_ = {token_.session + 1, 0};
    candidate.phase_ = continuation == opening::Continuation::Terminal ? Phase::Finished : Phase::Playing;
    candidate.end_reason_ = candidate.phase_ == Phase::Finished ? EndReason::OpeningResult : EndReason::None;
    candidate.events_.push_back({EventKind::Started, candidate.token_, {}, {}, {}, candidate.end_reason_});
    if (candidate.phase_ == Phase::Finished) {
        candidate.events_.push_back({EventKind::Ended, candidate.token_, {}, {}, {}, candidate.end_reason_});
    }
    *this = std::move(candidate);
    return {};
}
Response Controller::start_compact(std::string_view id, std::string_view text, state::Participant owner)
{
    opening::PreparedData data;
    auto status = opening::load_compact(id, text, data);
    if (!status.ok()) { return {Error::InvalidOpening, {}, status}; }
    opening::Session source;
    status = source.restore_prepared(data, owner);
    if (!status.ok()) { return {Error::InvalidOpening, {}, status}; }
    return start(source);
}
Error Controller::input_error(Token expected) const noexcept
{
    if (expected != token_) { return Error::StaleInput; }
    if (phase_ != Phase::Playing) { return Error::NotPlaying; }
    if (token_.revision == std::numeric_limits<std::uint64_t>::max()) { return Error::VersionExhausted; }
    return Error::None;
}
Response Controller::restart(Token expected)
{
    if (expected != token_) { return {Error::StaleInput}; }
    Controller candidate = *this;
    const auto response = candidate.start(opening_);
    if (!response.ok()) { return response; }
    candidate.events_[0].kind = EventKind::Restarted;
    *this = std::move(candidate);
    return {};
}
Response Controller::place(Token expected, state::Participant actor, state::Coordinate position)
{
    const auto error = input_error(expected);
    if (error != Error::None) { return {error}; }
    if (!valid(actor)) { return {Error::InvalidParticipant}; }
    if (game_.next_participant() != actor || !game_.colors().color_of(actor)) { return {Error::WrongParticipant}; }
    const auto color = *game_.colors().color_of(actor);
    const auto decision = rules::evaluate(game_, {position, color});
    // Structural failures have no side effects; Forbidden is an adjudicated loss, not a retry.
    if (decision.verdict == rules::Verdict::Invalid) { return {Error::InvalidPlacement, decision}; }
    Controller candidate = *this;
    ++candidate.token_.revision;
    if (decision.verdict == rules::Verdict::Forbidden) {
        if (candidate.game_.finish(state::Won{other(actor)}) != state::StateError::None) { return {Error::StorageFailure}; }
        candidate.end_reason_ = EndReason::Forbidden;
    } else {
        const auto next = color == state::StoneColor::Black ? state::StoneColor::White : state::StoneColor::Black;
        if (candidate.game_.commit_placement({position, color, actor}, next) != state::StateError::None) {
            return {Error::StorageFailure};
        }
        candidate.events_.push_back({EventKind::Placement, candidate.token_, actor, position, decision, EndReason::None});
        if (decision.verdict == rules::Verdict::Win) {
            if (candidate.game_.finish(state::Won{actor}) != state::StateError::None) { return {Error::StorageFailure}; }
            candidate.end_reason_ = EndReason::Five;
        } else if (decision.verdict == rules::Verdict::Draw) {
            if (candidate.game_.finish(state::Draw{}) != state::StateError::None) { return {Error::StorageFailure}; }
            candidate.end_reason_ = decision.reason == rules::Reason::MoveLimit ? EndReason::MoveLimit : EndReason::BoardFull;
        }
    }
    if (decision.verdict != rules::Verdict::Legal) {
        candidate.phase_ = Phase::Finished;
        candidate.rule_reason_ = decision.reason;
        candidate.events_.push_back({EventKind::Ended, candidate.token_, actor, position, decision, candidate.end_reason_});
    }
    *this = std::move(candidate);
    return {Error::None, decision};
}
Response Controller::resign(Token expected, state::Participant actor)
{
    const auto error = input_error(expected);
    if (error != Error::None) { return {error}; }
    if (!valid(actor)) { return {Error::InvalidParticipant}; }
    Controller candidate = *this;
    if (candidate.game_.finish(state::Won{other(actor)}) != state::StateError::None) { return {Error::StorageFailure}; }
    ++candidate.token_.revision;
    candidate.phase_ = Phase::Finished;
    candidate.end_reason_ = EndReason::Resigned;
    candidate.events_.push_back({EventKind::Ended, candidate.token_, actor, {}, {}, candidate.end_reason_});
    *this = std::move(candidate);
    return {};
}
Response Controller::close(Token expected)
{
    if (expected != token_) { return {Error::StaleInput}; }
    if (phase_ == Phase::Closed || phase_ == Phase::NotStarted) { return {Error::NotPlaying}; }
    if (token_.revision == std::numeric_limits<std::uint64_t>::max()) { return {Error::VersionExhausted}; }
    Controller candidate = *this;
    ++candidate.token_.revision;
    if (phase_ == Phase::Playing) {
        if (candidate.game_.finish(state::Aborted{}) != state::StateError::None) { return {Error::StorageFailure}; }
        candidate.end_reason_ = EndReason::Closed;
        candidate.events_.push_back({EventKind::Ended, candidate.token_, {}, {}, {}, EndReason::Closed});
    }
    candidate.phase_ = Phase::Closed;
    *this = std::move(candidate);
    return {};
}
Response Controller::engine_failure(Token expected, state::Participant actor, EngineFailure failure)
{
    const auto error = input_error(expected);
    if (error != Error::None) { return {error}; }
    if (!valid(actor)) { return {Error::InvalidParticipant}; }
    if (game_.next_participant() != actor) { return {Error::WrongParticipant}; }
    EndReason reason;
    switch (failure) {
    case EngineFailure::Start: reason = EndReason::EngineStartFailure; break;
    case EngineFailure::Timeout: reason = EndReason::EngineTimeout; break;
    case EngineFailure::Exited: reason = EndReason::EngineExited; break;
    case EngineFailure::BadOutput: reason = EndReason::EngineBadOutput; break;
    case EngineFailure::IllegalMove: reason = EndReason::EngineIllegalMove; break;
    default: return {Error::InvalidPlacement};
    }
    Controller candidate = *this;
    const state::GameResult result = failure == EngineFailure::Start
        ? state::GameResult{state::Aborted{}} : state::GameResult{state::Won{other(actor)}};
    if (candidate.game_.finish(result) != state::StateError::None) { return {Error::StorageFailure}; }
    ++candidate.token_.revision;
    candidate.phase_ = Phase::Finished; candidate.end_reason_ = reason;
    candidate.events_.push_back({EventKind::Ended, candidate.token_, actor, {}, {}, reason});
    *this = std::move(candidate);
    return {};
}
static_assert(std::is_nothrow_move_assignable_v<Controller>);
} // namespace omok::match
