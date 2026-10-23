#include "omok/state/game_state.hpp"

#include <algorithm>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

using namespace omok::state;

namespace {
// Unlike assert(), these checks are active in Release builds with NDEBUG.
#define CHECK(...) do { if (!(__VA_ARGS__)) { \
    throw std::runtime_error(std::string(__func__) + ":" + std::to_string(__LINE__) + ": " #__VA_ARGS__); \
} } while (false)

constexpr StoneColor invalid_color = static_cast<StoneColor>(99);
constexpr Participant invalid_participant = static_cast<Participant>(99);

void initial_state()
{
    const GameState state;
    CHECK(state.board().size() == 225);
    CHECK(std::all_of(state.board().begin(), state.board().end(), [](Cell c) { return c == Cell::Empty; }));
    CHECK(state.history().empty());
    CHECK(state.next_color() == StoneColor::Black);
    CHECK(!state.next_participant());
    CHECK(!state.colors().color_of(Participant::A));
    CHECK(!state.colors().color_of(Participant::B));
    CHECK(std::holds_alternative<InProgress>(state.result()));
    static_assert(!std::is_convertible_v<Participant, StoneColor>);
    static_assert(!std::is_convertible_v<StoneColor, RelativeStone>);
    static_assert(std::is_same_v<decltype(state.board()[0]), const Cell&>);
    static_assert(std::is_same_v<decltype(state.history()[0]), const Placement&>);
}

void coordinates()
{
    for (int row = 0; row < board_size; ++row) {
        for (int column = 0; column < board_size; ++column) {
            const Coordinate coordinate{row, column};
            const auto expected = static_cast<std::size_t>(row * 15 + column);
            CHECK(coordinate.is_on_board());
            CHECK(coordinate.index() == expected);
            CHECK(Coordinate::from_index(expected) == coordinate);
        }
    }
    for (const Coordinate coordinate : {Coordinate{-1, 0}, {0, -1}, {15, 0}, {0, 15},
            {std::numeric_limits<int>::min(), 4}, {4, std::numeric_limits<int>::max()}}) {
        CHECK(!coordinate.is_on_board());
        CHECK(!coordinate.index());
        CHECK(!GameState{}.cell_at(coordinate));
    }
    CHECK(!Coordinate::from_index(225));
    CHECK(!Coordinate::from_index(std::numeric_limits<std::size_t>::max()));
    CHECK(Coordinate{0, 14}.index() == 14);
    CHECK(Coordinate{14, 0}.index() == 210);
}

void boundaries()
{
    GameState state;
    for (const Coordinate corner : {Coordinate{0, 0}, {0, 14}, {14, 0}, {14, 14}}) {
        CHECK(state.cell_at(corner) == Cell::Empty);
        CHECK(state.commit_placement({corner, StoneColor::Black, Participant::A}, StoneColor::Black)
            == StateError::None);
        CHECK(state.cell_at(corner) == Cell::Black);
    }
    CHECK(state.commit_placement({{2, 11}, StoneColor::Black, Participant::B}, StoneColor::White)
        == StateError::None);
    CHECK(state.commit_placement({{11, 2}, StoneColor::White, Participant::A}, StoneColor::Black)
        == StateError::None);
    CHECK(state.cell_at({2, 11}) == Cell::Black);
    CHECK(state.cell_at({11, 2}) == Cell::White);
}

void rejected_requests()
{
    GameState state;
    CHECK(state.assign_colors(Participant::B) == StateError::None);
    CHECK(state.commit_placement({{7, 7}, StoneColor::Black, Participant::B}, StoneColor::White)
        == StateError::None);
    const GameState before = state;
    for (const Coordinate outside : {Coordinate{-1, 7}, {7, -1}, {15, 7}, {7, 15},
            {std::numeric_limits<int>::min(), std::numeric_limits<int>::max()}}) {
        CHECK(state.commit_placement({outside, StoneColor::White, Participant::A}, StoneColor::Black)
            == StateError::OutOfBounds);
        CHECK(state == before);
    }
    CHECK(state.commit_placement({{7, 7}, StoneColor::White, Participant::A}, StoneColor::Black)
        == StateError::Occupied);
    CHECK(state == before);
    CHECK(state.commit_placement({{1, 1}, invalid_color, Participant::A}, StoneColor::Black)
        == StateError::InvalidColor);
    CHECK(state == before);
    CHECK(state.commit_placement({{1, 1}, StoneColor::White, Participant::A}, invalid_color)
        == StateError::InvalidColor);
    CHECK(state == before);
    CHECK(state.commit_placement({{1, 1}, StoneColor::White, invalid_participant}, StoneColor::Black)
        == StateError::InvalidParticipant);
    CHECK(state == before);
    CHECK(state.commit_placement({{1, 1}, StoneColor::Black, Participant::A}, StoneColor::White)
        == StateError::UnexpectedColor);
    CHECK(state == before);
    CHECK(state.assign_colors(invalid_participant) == StateError::InvalidParticipant);
    CHECK(state == before);
}

void color_assignment()
{
    GameState state;
    CHECK(!state.colors().owner_of(StoneColor::Black));
    CHECK(!state.colors().owner_of(StoneColor::White));
    for (const Participant black : {Participant::A, Participant::B}) {
        const Participant white = black == Participant::A ? Participant::B : Participant::A;
        CHECK(state.assign_colors(black) == StateError::None);
        CHECK(state.colors().owner_of(StoneColor::Black) == black);
        CHECK(state.colors().owner_of(StoneColor::White) == white);
        CHECK(state.colors().color_of(black) == StoneColor::Black);
        CHECK(state.colors().color_of(white) == StoneColor::White);
        CHECK(state.next_participant() == black);
        CHECK(!state.colors().owner_of(invalid_color));
        CHECK(!state.colors().color_of(invalid_participant));
    }
}

void relative_stones()
{
    GameState state;
    CHECK(!state.colors().relative_to(StoneColor::Black, Participant::A));
    for (const Participant black : {Participant::A, Participant::B}) {
        const Participant white = black == Participant::A ? Participant::B : Participant::A;
        CHECK(state.assign_colors(black) == StateError::None);
        CHECK(state.colors().relative_to(StoneColor::Black, black) == RelativeStone::Own);
        CHECK(state.colors().relative_to(StoneColor::White, black) == RelativeStone::Opponent);
        CHECK(state.colors().relative_to(StoneColor::White, white) == RelativeStone::Own);
        CHECK(state.colors().relative_to(StoneColor::Black, white) == RelativeStone::Opponent);
        CHECK(!state.colors().relative_to(invalid_color, black));
        CHECK(!state.colors().relative_to(StoneColor::Black, invalid_participant));
    }
}

void recoloring()
{
    GameState state;
    CHECK(state.assign_colors(Participant::A) == StateError::None);
    CHECK(state.commit_placement({{4, 9}, StoneColor::Black, Participant::A}, StoneColor::White)
        == StateError::None);
    const Placement first = state.history()[0];
    CHECK(state.next_participant() == Participant::B);
    CHECK(state.assign_colors(Participant::B) == StateError::None);
    CHECK(state.cell_at({4, 9}) == Cell::Black);
    CHECK(state.history().size() == 1);
    CHECK(state.history()[0] == first);
    CHECK(state.history()[0].placed_by == Participant::A);
    CHECK(state.colors().owner_of(StoneColor::Black) == Participant::B);
    CHECK(state.next_color() == StoneColor::White);
    CHECK(state.next_participant() == Participant::A);
    CHECK(std::holds_alternative<InProgress>(state.result()));
}

void history()
{
    GameState state;
    const std::array placements{
        Placement{{7, 7}, StoneColor::Black, Participant::A},
        Placement{{6, 8}, StoneColor::White, Participant::A},
        Placement{{8, 6}, StoneColor::Black, Participant::B}};
    for (std::size_t i = 0; i < placements.size(); ++i) {
        const StoneColor next = placements[i].color == StoneColor::Black ? StoneColor::White : StoneColor::Black;
        CHECK(state.commit_placement(placements[i], next) == StateError::None);
        CHECK(state.history().size() == i + 1);
        CHECK(state.history()[i] == placements[i]);
    }
    const auto before = state;
    CHECK(state.commit_placement({{7, 7}, StoneColor::White, Participant::A}, StoneColor::Black)
        == StateError::Occupied);
    CHECK(state == before);
    GameState copy = state;
    CHECK(copy.reset() == StateError::None);
    CHECK(state == before); // Copies do not share board/history storage.
}

void explicit_turn()
{
    GameState state;
    CHECK(state.reset({Participant::A, StoneColor::White}) == StateError::None);
    CHECK(state.next_participant() == Participant::B);
    // This low-level transaction records actual actors, not input authorization.
    CHECK(state.commit_placement({{0, 1}, StoneColor::White, Participant::A}, StoneColor::White)
        == StateError::None);
    CHECK(state.next_color() == StoneColor::White); // No hidden auto-alternation.
    CHECK(state.next_participant() == Participant::B);
    CHECK(state.history()[0].placed_by == Participant::A);
    CHECK(state.colors().owner_of(StoneColor::White) == Participant::B);
}

void terminal_results()
{
    for (const GameResult terminal : {GameResult{Won{Participant::B}}, GameResult{Draw{}}, GameResult{Aborted{}}}) {
        GameState state;
        CHECK(state.assign_colors(Participant::A) == StateError::None);
        CHECK(state.commit_placement({{3, 4}, StoneColor::Black, Participant::A}, StoneColor::White)
            == StateError::None);
        CHECK(state.finish(terminal) == StateError::None);
        CHECK(state.result() == terminal);
        CHECK(!state.next_color());
        CHECK(!state.next_participant());
        CHECK(state.cell_at({3, 4}) == Cell::Black);
        CHECK(state.history().size() == 1);
        const auto finished = state;
        CHECK(state.commit_placement({{3, 5}, StoneColor::White, Participant::B}, StoneColor::Black)
            == StateError::AlreadyFinished);
        CHECK(state == finished);
        CHECK(state.assign_colors(Participant::B) == StateError::AlreadyFinished);
        CHECK(state == finished);
        CHECK(state.finish(Draw{}) == StateError::AlreadyFinished);
        CHECK(state == finished);
    }
}

void rejected_results()
{
    GameState state;
    const auto before = state;
    CHECK(state.finish(InProgress{}) == StateError::InvalidResult);
    CHECK(state == before);
    CHECK(state.finish(Won{invalid_participant}) == StateError::InvalidParticipant);
    CHECK(state == before);
    CHECK(state.finish(Won{Participant::A}) == StateError::None); // External outcome, even on empty board.
}

void reset()
{
    GameState state;
    CHECK(state.assign_colors(Participant::B) == StateError::None);
    CHECK(state.commit_placement({{0, 14}, StoneColor::Black, Participant::A}, StoneColor::White)
        == StateError::None);
    CHECK(state.finish(Won{Participant::A}) == StateError::None);
    const auto before = state;
    CHECK(state.reset({invalid_participant, StoneColor::Black}) == StateError::InvalidParticipant);
    CHECK(state == before);
    CHECK(state.reset({Participant::A, invalid_color}) == StateError::InvalidColor);
    CHECK(state == before);
    CHECK(state.reset() == StateError::None);
    CHECK(state == GameState{});
    CHECK(state.reset({Participant::B, StoneColor::White}) == StateError::None);
    CHECK(state.colors().color_of(Participant::B) == StoneColor::Black);
    CHECK(state.next_color() == StoneColor::White);
    CHECK(state.next_participant() == Participant::A);
    CHECK(state.history().empty());
    CHECK(state.cell_at({0, 14}) == Cell::Empty);
    CHECK(std::holds_alternative<InProgress>(state.result()));
}

void no_adjudication()
{
    GameState state;
    for (std::size_t i = 0; i < board_area; ++i) {
        const auto coordinate = Coordinate::from_index(i);
        CHECK(coordinate);
        // Deliberately creates lines/overlines and passes 200 placements. Storage accepts them.
        CHECK(state.commit_placement({*coordinate, StoneColor::Black, Participant::A}, StoneColor::Black)
            == StateError::None);
        CHECK(std::holds_alternative<InProgress>(state.result()));
        CHECK(state.history().size() == i + 1);
    }
    CHECK(std::all_of(state.board().begin(), state.board().end(), [](Cell c) { return c == Cell::Black; }));
    const auto full = state;
    CHECK(state.commit_placement({{14, 14}, StoneColor::Black, Participant::A}, StoneColor::White)
        == StateError::Occupied);
    CHECK(state == full);
    CHECK(state.reset() == StateError::None);
    CHECK(state == GameState{});
}

void prepared_actor()
{
    GameState state;
    CHECK(state.commit_placement({{7, 7}, StoneColor::Black, std::nullopt}, StoneColor::White) == StateError::None);
    CHECK(!state.history()[0].placed_by);
    CHECK(state.assign_colors(Participant::B) == StateError::None);
    CHECK(!state.history()[0].placed_by); // assigning an owner does not fabricate an actor
    const auto before = state;
    CHECK(state.commit_placement({{7, 8}, StoneColor::White, invalid_participant}, StoneColor::Black)
        == StateError::InvalidParticipant);
    CHECK(state == before);
    CHECK(state.finish(Draw{}) == StateError::None);
    const auto terminal = state;
    CHECK(state.commit_placement({{7, 8}, StoneColor::White, std::nullopt}, StoneColor::Black)
        == StateError::AlreadyFinished);
    CHECK(state == terminal);
    CHECK(state.reset() == StateError::None);
    CHECK(state == GameState{});
}

struct TestCase { std::string_view name; void (*run)(); };
constexpr TestCase cases[]{
    {"initial_state", initial_state}, {"coordinates", coordinates}, {"boundaries", boundaries},
    {"rejected_requests", rejected_requests}, {"color_assignment", color_assignment},
    {"relative_stones", relative_stones}, {"recoloring", recoloring}, {"history", history},
    {"explicit_turn", explicit_turn}, {"terminal_results", terminal_results},
    {"rejected_results", rejected_results}, {"reset", reset}, {"no_adjudication", no_adjudication},
    {"prepared_actor", prepared_actor}};
} // namespace

int main(int argc, char** argv)
{
    if (argc != 2) {
        std::cerr << "Expected one test case name\n";
        return 2;
    }
    for (const auto& test : cases) {
        if (test.name == argv[1]) {
            try {
                test.run();
                std::cout << "PASS " << test.name << '\n';
                return 0;
            } catch (const std::exception& error) {
                std::cerr << error.what() << '\n';
                return 1;
            }
        }
    }
    std::cerr << "Unknown test case: " << argv[1] << '\n';
    return 2;
}
