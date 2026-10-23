#include "omok/state/game_state.hpp"

namespace omok::state {
namespace {
constexpr bool valid(StoneColor color) noexcept
{
    return color == StoneColor::Black || color == StoneColor::White;
}
constexpr bool valid(Participant participant) noexcept
{
    return participant == Participant::A || participant == Participant::B;
}
constexpr Participant other(Participant participant) noexcept
{
    return participant == Participant::A ? Participant::B : Participant::A;
}
} // namespace

std::optional<Participant> ColorAssignment::owner_of(StoneColor color) const noexcept
{
    if (!black_owner_ || !valid(color)) {
        return std::nullopt;
    }
    return color == StoneColor::Black ? *black_owner_ : other(*black_owner_);
}

std::optional<StoneColor> ColorAssignment::color_of(Participant participant) const noexcept
{
    if (!black_owner_ || !valid(participant)) {
        return std::nullopt;
    }
    return participant == *black_owner_ ? StoneColor::Black : StoneColor::White;
}

std::optional<RelativeStone> ColorAssignment::relative_to(
    StoneColor stone, Participant observer) const noexcept
{
    const auto owner = owner_of(stone);
    if (!owner || !valid(observer)) {
        return std::nullopt;
    }
    return *owner == observer ? RelativeStone::Own : RelativeStone::Opponent;
}

std::optional<Cell> GameState::cell_at(Coordinate position) const noexcept
{
    const auto index = position.index();
    if (!index) {
        return std::nullopt;
    }
    return board_[*index];
}

std::optional<Participant> GameState::next_participant() const noexcept
{
    return next_color_ ? colors_.owner_of(*next_color_) : std::nullopt;
}

StateError GameState::commit_placement(Placement placement, StoneColor next_color) noexcept
{
    if (!std::holds_alternative<InProgress>(result_)) {
        return StateError::AlreadyFinished;
    }
    const auto index = placement.position.index();
    if (!index) {
        return StateError::OutOfBounds;
    }
    if (!valid(placement.color) || !valid(next_color)) {
        return StateError::InvalidColor;
    }
    if (placement.placed_by && !valid(*placement.placed_by)) {
        return StateError::InvalidParticipant;
    }
    if (placement.color != next_color_) {
        return StateError::UnexpectedColor;
    }
    if (board_[*index] != Cell::Empty) {
        return StateError::Occupied;
    }

    // All checks precede writes; fixed-capacity history cannot allocate or throw.
    // An empty point guarantees history_size_ < board_area by the state invariant.
    history_[history_size_] = placement;
    ++history_size_;
    board_[*index] = placement.color == StoneColor::Black ? Cell::Black : Cell::White;
    next_color_ = next_color;
    return StateError::None;
}

StateError GameState::assign_colors(Participant black_owner) noexcept
{
    if (!std::holds_alternative<InProgress>(result_)) {
        return StateError::AlreadyFinished;
    }
    if (!valid(black_owner)) {
        return StateError::InvalidParticipant;
    }
    colors_.black_owner_ = black_owner;
    return StateError::None;
}

StateError GameState::finish(GameResult result) noexcept
{
    if (!std::holds_alternative<InProgress>(result_)) {
        return StateError::AlreadyFinished;
    }
    if (std::holds_alternative<InProgress>(result)) {
        return StateError::InvalidResult;
    }
    if (const auto* won = std::get_if<Won>(&result); won && !valid(won->winner)) {
        return StateError::InvalidParticipant;
    }
    result_ = result;
    next_color_.reset();
    return StateError::None;
}

StateError GameState::reset(InitialState initial) noexcept
{
    if (!valid(initial.next_color)) {
        return StateError::InvalidColor;
    }
    if (initial.black_owner && !valid(*initial.black_owner)) {
        return StateError::InvalidParticipant;
    }
    board_.fill(Cell::Empty);
    history_.fill(Placement{});
    history_size_ = 0;
    colors_.black_owner_ = initial.black_owner;
    next_color_ = initial.next_color;
    result_ = InProgress{};
    return StateError::None;
}

} // namespace omok::state
