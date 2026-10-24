#include "omok/opening/opening.hpp"

#include <array>
#include <type_traits>
#include <utility>

namespace omok::opening {
namespace {
using namespace state;
bool valid(Participant actor) { return actor == Participant::A || actor == Participant::B; }
StoneColor other(StoneColor color)
{
    return color == StoneColor::Black ? StoneColor::White : StoneColor::Black;
}
Status check_move(const GameState& game, const Move& move)
{
    if (move.number != game.history().size() + 1) { return {Error::WrongNumber, move.number}; }
    if ((move.color != StoneColor::Black && move.color != StoneColor::White)
        || move.color != game.next_color()) { return {Error::WrongColor, move.number}; }
    const auto reason = rules::validate_board_candidate(game.board(), {move.position, move.color});
    return reason == rules::Reason::None ? Status{} : Status{Error::InvalidPlacement, move.number, reason};
}
} // namespace

Status load_compact(std::string_view id, std::string_view text, PreparedData& output)
{
    if (id.empty()) { return {Error::MissingId}; }
    PreparedData candidate{std::string(id), {}};
    std::array<bool, state::board_area> seen{};
    std::size_t cursor = 0;
    while (cursor < text.size()) {
        const auto number = candidate.moves.size() + 1;
        if (number > rules::tournament_move_limit) { return {Error::TooLong, number}; }
        char letter = text[cursor++];
        if (letter >= 'A' && letter <= 'O') { letter = static_cast<char>(letter - 'A' + 'a'); }
        if (letter < 'a' || letter > 'o' || cursor == text.size()
            || text[cursor] < '1' || text[cursor] > '9') { return {Error::MalformedData, number}; }
        int row = text[cursor++] - '0';
        if (cursor < text.size() && text[cursor] >= '0' && text[cursor] <= '9') {
            row = row * 10 + text[cursor++] - '0';
        }
        if (cursor < text.size() && text[cursor] >= '0' && text[cursor] <= '9') {
            return {Error::MalformedData, number};
        }
        if (row > 15) { return {Error::InvalidPlacement, number, rules::Reason::OutOfBounds}; }
        const state::Coordinate position{row - 1, letter - 'a'};
        const auto index = *position.index();
        if (seen[index]) { return {Error::InvalidPlacement, number, rules::Reason::Occupied}; }
        seen[index] = true;
        candidate.moves.push_back({number, position, number % 2 == 1
            ? state::StoneColor::Black : state::StoneColor::White});
    }
    output = std::move(candidate);
    return {};
}

void Session::record(EventType type, std::optional<state::Participant> actor,
    std::optional<Choice> choice, std::optional<state::Participant> black_owner)
{
    events_.push_back({type, actor, choice, black_owner, game_.history().size()});
}

Handoff Session::handoff() const noexcept
{
    std::optional<state::Participant> input;
    Continuation continuation = Continuation::OpeningInput;
    switch (stage_) {
    case Stage::Idle: continuation = Continuation::NotStarted; break;
    case Stage::FirstThree: input = state::Participant::A; break;
    case Stage::SecondChoice: input = state::Participant::B; break;
    case Stage::FinalChoice: input = state::Participant::A; break;
    case Stage::Finished: continuation = Continuation::Terminal; break;
    case Stage::Complete:
        continuation = (mode_ == Mode::PreparedRenju || mode_ == Mode::DirectRenju) ? Continuation::RenjuReady
            : Continuation::RuleContractRequired;
        break;
    }
    return {mode_, stage_, continuation, game_.history().size(), input,
        game_.next_color(), game_.next_participant()};
}

Status Session::restore_prepared(const PreparedData& data, state::Participant black_owner)
{
    using namespace state;
    if (data.id.empty()) { return {Error::MissingId}; }
    if (!valid(black_owner)) { return {Error::InvalidParticipant}; }
    if (data.moves.size() > rules::tournament_move_limit) { return {Error::TooLong}; }
    Session candidate;
    candidate.mode_ = Mode::PreparedRenju;
    candidate.data_id_ = data.id;
    // Validated above; state storage performs no Renju adjudication.
    static_cast<void>(candidate.game_.reset({black_owner, StoneColor::Black}));
    candidate.record(EventType::Started);
    candidate.record(EventType::ColorsAssigned, {}, {}, black_owner);
    for (const auto& move : data.moves) {
        if (!std::holds_alternative<InProgress>(candidate.game_.result())) {
            return {Error::AfterTerminal, move.number};
        }
        const auto structure = check_move(candidate.game_, move);
        if (!structure.ok()) { return structure; }
        const auto decision = rules::analyze_board(candidate.game_.board(), {move.position, move.color});
        if (decision.verdict == rules::Verdict::Invalid) {
            return {Error::InvalidPlacement, move.number, decision.reason};
        }
        if (decision.verdict == rules::Verdict::Forbidden) {
            // A prepared data error is not a participant's foul/defeat.
            return {Error::ForbiddenPlacement, move.number, decision.reason};
        }
        if (candidate.game_.commit_placement({move.position, move.color, std::nullopt}, other(move.color))
            != StateError::None) { return {Error::StorageFailure, move.number}; }
        if (decision.verdict == rules::Verdict::Win) {
            if (candidate.game_.finish(Won{*candidate.game_.colors().owner_of(move.color)})
                != StateError::None) { return {Error::StorageFailure, move.number}; }
        } else if (decision.verdict == rules::Verdict::Draw
            || move.number == rules::tournament_move_limit) {
            if (candidate.game_.finish(Draw{}) != StateError::None) { return {Error::StorageFailure, move.number}; }
        }
    }
    candidate.record(EventType::BatchPlaced); // no real participant placed imported stones
    candidate.stage_ = std::holds_alternative<InProgress>(candidate.game_.result())
        ? Stage::Complete : Stage::Finished;
    candidate.record(EventType::Completed);
    *this = std::move(candidate);
    return {};
}

Status Session::begin_direct_renju(state::Participant black_owner)
{
    if (!valid(black_owner)) { return {Error::InvalidParticipant}; }
    Session candidate;
    candidate.mode_ = Mode::DirectRenju;
    candidate.stage_ = Stage::Complete;
    candidate.data_id_ = "direct-renju-v1";
    if (candidate.game_.reset({black_owner, state::StoneColor::Black}) != state::StateError::None) {
        return {Error::StorageFailure};
    }
    candidate.record(EventType::Started);
    candidate.record(EventType::ColorsAssigned, {}, {}, black_owner);
    candidate.record(EventType::Completed);
    *this = std::move(candidate);
    return {};
}

void Session::begin_swap2()
{
    Session candidate;
    candidate.mode_ = Mode::Swap2Protocol;
    candidate.stage_ = Stage::FirstThree;
    candidate.record(EventType::Started);
    *this = std::move(candidate);
}

Status Session::append_structural(state::Participant actor, std::span<const Move> moves)
{
    for (const auto& move : moves) {
        const auto status = check_move(game_, move);
        if (!status.ok()) { return status; }
        // Swap2 rule binding is unresolved: only structure/sequence, never implicit Renju.
        if (game_.commit_placement({move.position, move.color, actor}, other(move.color))
            != state::StateError::None) { return {Error::StorageFailure, move.number}; }
    }
    record(EventType::BatchPlaced, actor);
    return {};
}

Status Session::place_first_three(state::Participant actor, std::span<const Move> moves)
{
    if (!valid(actor)) { return {Error::InvalidParticipant}; }
    if (mode_ != Mode::Swap2Protocol || stage_ != Stage::FirstThree) { return {Error::WrongStage}; }
    if (actor != state::Participant::A) { return {Error::WrongActor}; }
    if (moves.size() != 3) { return {Error::WrongCount}; }
    Session candidate = *this;
    const auto status = candidate.append_structural(actor, moves);
    if (!status.ok()) { return status; }
    candidate.stage_ = Stage::SecondChoice;
    *this = std::move(candidate);
    return {};
}

Status Session::choose(state::Participant actor, Choice choice, std::span<const Move> moves)
{
    if (!valid(actor)) { return {Error::InvalidParticipant}; }
    if (mode_ != Mode::Swap2Protocol || (stage_ != Stage::SecondChoice && stage_ != Stage::FinalChoice)) {
        return {Error::WrongStage};
    }
    const auto expected_actor = stage_ == Stage::SecondChoice ? state::Participant::B : state::Participant::A;
    if (actor != expected_actor) { return {Error::WrongActor}; }
    if (choice != Choice::TakeBlack && choice != Choice::TakeWhite && choice != Choice::OfferTwo) {
        return {Error::InvalidChoice};
    }
    if (choice == Choice::OfferTwo && stage_ != Stage::SecondChoice) { return {Error::InvalidChoice}; }
    const std::size_t count = choice == Choice::TakeBlack ? 0 : choice == Choice::TakeWhite ? 1 : 2;
    if (moves.size() != count) { return {Error::WrongCount}; }
    Session candidate = *this;
    candidate.record(EventType::ChoiceMade, actor, choice);
    if (!moves.empty()) {
        const auto status = candidate.append_structural(actor, moves);
        if (!status.ok()) { return status; }
    }
    if (choice == Choice::OfferTwo) {
        candidate.stage_ = Stage::FinalChoice;
    } else {
        const auto black_owner = choice == Choice::TakeBlack ? actor
            : actor == state::Participant::A ? state::Participant::B : state::Participant::A;
        if (candidate.game_.assign_colors(black_owner) != state::StateError::None) { return {Error::StorageFailure}; }
        candidate.record(EventType::ColorsAssigned, actor, {}, black_owner);
        candidate.stage_ = Stage::Complete;
        candidate.record(EventType::Completed);
    }
    *this = std::move(candidate);
    return {};
}

// Publishing a fully built candidate must never expose a partially updated session.
static_assert(std::is_nothrow_move_assignable_v<Session>);
static_assert(std::is_nothrow_move_assignable_v<PreparedData>);
} // namespace omok::opening
