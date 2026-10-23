#include "omok/rules/renju.hpp"

#include <algorithm>
#include <array>
#include <set>

namespace omok::rules {
namespace {
using namespace state;
using Board = std::array<Cell, board_area>;
constexpr std::array<Coordinate, 4> directions{{{0, 1}, {1, 0}, {1, 1}, {1, -1}}};
Coordinate offset(Coordinate p, Coordinate d, int n)
{
    return {p.row + d.row * n, p.column + d.column * n};
}
bool is(const Board& board, Coordinate p, Cell cell)
{
    const auto i = p.index();
    return i && board[*i] == cell;
}
int run_length(const Board& board, Coordinate p, Coordinate d, Cell cell)
{
    int length = 1;
    for (int sign : {-1, 1}) {
        for (int n = 1; is(board, offset(p, d, sign * n), cell); ++n) { ++length; }
    }
    return length;
}
bool has_run(const Board& board, Coordinate p, int length, bool at_least = false)
{
    return std::any_of(directions.begin(), directions.end(), [&](Coordinate d) {
        const int n = run_length(board, p, d, Cell::Black);
        return at_least ? n >= length : n == length;
    });
}
// Identity is the FOUR stones, not winning endpoints: .XXXX. is one four.
// Different sets on one axis (X.XXX.X) remain distinct fours.
std::size_t four_count(const Board& board, Coordinate origin)
{
    std::set<std::array<std::size_t, 4>> fours;
    for (const auto d : directions) {
        for (int start = -4; start <= 0; ++start) {
            const auto first = offset(origin, d, start);
            if (!first.is_on_board() || !offset(first, d, 4).is_on_board()
                || is(board, offset(first, d, -1), Cell::Black)
                || is(board, offset(first, d, 5), Cell::Black)) { continue; }
            std::array<std::size_t, 4> stones{};
            std::size_t count = 0;
            int empty = 0;
            for (int n = 0; n < 5; ++n) {
                const auto index = *offset(first, d, n).index();
                if (board[index] == Cell::Black) {
                    if (count < stones.size()) { stones[count] = index; }
                    ++count;
                } else if (board[index] == Cell::Empty) { ++empty; }
            }
            if (count == 4 && empty == 1) {
                std::sort(stones.begin(), stones.end());
                fours.insert(stones);
            }
        }
    }
    return fours.size();
}
Reason black_reason(Board& board, Coordinate origin);
std::size_t real_three_count(Board& board, Coordinate origin)
{
    std::set<std::array<std::size_t, 3>> threes;
    for (const auto d : directions) {
        for (int start = -3; start <= 0; ++start) {
            const auto first = offset(origin, d, start);
            // Straight four needs two empty ends, BOTH giving exact five.
            if (!is(board, offset(first, d, -1), Cell::Empty)
                || !is(board, offset(first, d, 4), Cell::Empty)
                || is(board, offset(first, d, -2), Cell::Black)
                || is(board, offset(first, d, 5), Cell::Black)) { continue; }
            std::array<std::size_t, 3> stones{};
            std::size_t count = 0;
            int empty = 0;
            Coordinate extension{};
            for (int n = 0; n < 4; ++n) {
                const auto p = offset(first, d, n);
                const auto index = *p.index(); // endpoints imply this is on-board
                if (board[index] == Cell::Black) {
                    if (count < stones.size()) { stones[count] = index; }
                    ++count;
                } else if (board[index] == Cell::Empty) { extension = p; ++empty; }
            }
            if (count != 3 || empty != 1) { continue; }
            std::sort(stones.begin(), stones.end());
            if (threes.contains(stones)) { continue; }
            auto& cell = board[*extension.index()];
            cell = Cell::Black;
            // RIF 3 excludes simultaneous five; 9.3 recursively checks extension fouls.
            const bool real = !has_run(board, extension, 5)
                && black_reason(board, extension) == Reason::None;
            cell = Cell::Empty;
            if (real) {
                threes.insert(stones);
                if (threes.size() >= 2) { return threes.size(); }
            }
        }
    }
    return threes.size();
}
Reason black_reason(Board& board, Coordinate origin)
{
    // RIF 9.2 gives exact five precedence even over a simultaneous overline.
    if (has_run(board, origin, 5)) { return Reason::ExactFive; }
    if (has_run(board, origin, 6, true)) { return Reason::Overline; }
    if (four_count(board, origin) >= 2) { return Reason::DoubleFour; }
    if (real_three_count(board, origin) >= 2) { return Reason::DoubleThree; }
    return Reason::None;
}
} // namespace
Reason validate_board_candidate(std::span<const state::Cell, state::board_area> board,
    Candidate candidate) noexcept
{
    using namespace state;
    const auto index = candidate.position.index();
    if (!index) { return Reason::OutOfBounds; }
    if (candidate.color != StoneColor::Black && candidate.color != StoneColor::White) {
        return Reason::InvalidColor;
    }
    if (std::any_of(board.begin(), board.end(), [](Cell c) {
            return c != Cell::Empty && c != Cell::Black && c != Cell::White;
        })) { return Reason::InvalidCell; }
    if (board[*index] != Cell::Empty) { return Reason::Occupied; }
    return Reason::None;
}
Decision analyze_board(std::span<const state::Cell, state::board_area> source, Candidate candidate)
{
    using namespace state;
    const auto error = validate_board_candidate(source, candidate);
    if (error != Reason::None) { return {Verdict::Invalid, error}; }
    Board board{};
    std::copy(source.begin(), source.end(), board.begin());
    const Cell cell = candidate.color == StoneColor::Black ? Cell::Black : Cell::White;
    board[*candidate.position.index()] = cell;
    if (cell == Cell::Black) {
        const auto reason = black_reason(board, candidate.position);
        if (reason == Reason::ExactFive) { return {Verdict::Win, reason}; }
        if (reason != Reason::None) { return {Verdict::Forbidden, reason}; }
    } else {
        for (const auto d : directions) {
            if (run_length(board, candidate.position, d, cell) >= 5) {
                return {Verdict::Win, Reason::WhiteFiveOrMore};
            }
        }
    }
    if (std::find(board.begin(), board.end(), Cell::Empty) == board.end()) {
        return {Verdict::Draw, Reason::BoardFull};
    }
    return {};
}
Decision evaluate(const state::GameState& state, Candidate candidate)
{
    if (!std::holds_alternative<state::InProgress>(state.result())) {
        return {Verdict::Invalid, Reason::AlreadyFinished};
    }
    const auto error = validate_board_candidate(state.board(), candidate);
    if (error != Reason::None) { return {Verdict::Invalid, error}; }
    if (candidate.color != state.next_color()) {
        return {Verdict::Invalid, Reason::UnexpectedColor};
    }
    if (state.history().size() >= tournament_move_limit) {
        return {Verdict::Invalid, Reason::MoveLimitReached};
    }
    auto decision = analyze_board(state.board(), candidate);
    if (decision.verdict == Verdict::Legal
        && state.history().size() + 1 == tournament_move_limit) {
        decision = {Verdict::Draw, Reason::MoveLimit};
    }
    return decision;
}
} // namespace omok::rules
