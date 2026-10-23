#pragma once

#include "omok/state/game_state.hpp"

namespace omok::rules {

struct Candidate {
    state::Coordinate position;
    state::StoneColor color;
};
enum class Verdict { Invalid, Forbidden, Legal, Win, Draw };
enum class Reason {
    None, OutOfBounds, Occupied, InvalidColor, InvalidCell,
    AlreadyFinished, UnexpectedColor, MoveLimitReached,
    ExactFive, WhiteFiveOrMore, Overline, DoubleFour, DoubleThree,
    BoardFull, MoveLimit
};
struct Decision {
    Verdict verdict{Verdict::Legal};
    Reason reason{Reason::None};
    bool operator==(const Decision&) const = default;
};
inline constexpr std::size_t tournament_move_limit = 200;

// Structural checks only: no turn, history, participant or rule adjudication.
[[nodiscard]] Reason validate_board_candidate(
    std::span<const state::Cell, state::board_area> board, Candidate candidate) noexcept;
// Board BEFORE the candidate. Pure local Renju + full-board draw, NOT 200-ply policy.
// Historical legality is not checked. All hypothetical placements use a private copy.
[[nodiscard]] Decision analyze_board(
    std::span<const state::Cell, state::board_area> board, Candidate candidate);
// Normal-game adapter: terminal/next-color/200-ply checks; never applies a decision.
// History includes ALL restored opening placements. Opening code uses analyze_board.
[[nodiscard]] Decision evaluate(const state::GameState& state, Candidate candidate);

} // namespace omok::rules
