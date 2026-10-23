#include "omok/rules/renju.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace omok::state;
using namespace omok::rules;
namespace {
#define CHECK(...) do { if (!(__VA_ARGS__)) { throw std::runtime_error( \
    std::string(__func__) + ":" + std::to_string(__LINE__) + ": " #__VA_ARGS__); } } while (false)
using Board = std::array<Cell, board_area>;
const std::vector<std::string> verdicts{"Invalid", "Forbidden", "Legal", "Win", "Draw"};
const std::vector<std::string> reasons{"None", "OutOfBounds", "Occupied", "InvalidColor", "InvalidCell",
    "AlreadyFinished", "UnexpectedColor", "MoveLimitReached", "ExactFive", "WhiteFiveOrMore",
    "Overline", "DoubleFour", "DoubleThree", "BoardFull", "MoveLimit"};
std::vector<std::string> split(const std::string& text, char delimiter)
{
    std::vector<std::string> fields;
    std::size_t start = 0;
    for (;;) {
        const auto end = text.find(delimiter, start);
        fields.push_back(text.substr(start, end - start));
        if (end == std::string::npos) { break; }
        start = end + 1;
    }
    return fields;
}
struct Fixture { std::string id; Board board{}; Candidate candidate; Decision expected; };
Fixture parse(const std::string& line)
{
    const auto f = split(line, '|');
    CHECK(f.size() == 8);
    const auto v = std::find(verdicts.begin(), verdicts.end(), f[4]);
    const auto r = std::find(reasons.begin(), reasons.end(), f[5]);
    CHECK(v != verdicts.end() && r != reasons.end());
    CHECK(f[1] == "B" || f[1] == "W");
    Fixture fixture{f[0], {}, {{std::stoi(f[2]), std::stoi(f[3])},
        f[1] == "B" ? StoneColor::Black : StoneColor::White},
        {static_cast<Verdict>(v - verdicts.begin()), static_cast<Reason>(r - reasons.begin())}};
    for (std::size_t n : {6U, 7U}) {
        if (f[n].empty()) { continue; }
        for (const auto& token : split(f[n], ';')) {
            const auto rc = split(token, ',');
            CHECK(rc.size() == 2);
            const Coordinate p{std::stoi(rc[0]), std::stoi(rc[1])};
            CHECK(p.index() && fixture.board[*p.index()] == Cell::Empty);
            fixture.board[*p.index()] = n == 6 ? Cell::Black : Cell::White;
        }
    }
    return fixture;
}
Coordinate transform(Coordinate p, int symmetry)
{
    if (symmetry >= 4) { p.column = 14 - p.column; }
    for (int n = 0; n < symmetry % 4; ++n) { p = {p.column, 14 - p.row}; }
    return p;
}
void cases(const std::string& path, const std::string& id)
{
    std::ifstream file(path);
    CHECK(file.good());
    bool found = false;
    for (std::string line; std::getline(file, line);) {
        if (line.empty() || line[0] == '#') { continue; }
        const auto fixture = parse(line);
        if (fixture.id != id) { continue; }
        found = true;
        for (int symmetry = 0; symmetry < 8; ++symmetry) {
            Board board{};
            for (std::size_t n = 0; n < board_area; ++n) {
                board[*transform(*Coordinate::from_index(n), symmetry).index()] = fixture.board[n];
            }
            const Candidate c{transform(fixture.candidate.position, symmetry), fixture.candidate.color};
            const auto before = board;
            const auto result = analyze_board(board, c);
            if (result != fixture.expected) {
                throw std::runtime_error(id + " symmetry=" + std::to_string(symmetry) + " got "
                    + verdicts[static_cast<std::size_t>(result.verdict)] + "/"
                    + reasons[static_cast<std::size_t>(result.reason)]);
            }
            CHECK(board == before);
        }
    }
    CHECK(found);
}
void structural()
{
    Board board{};
    for (const Coordinate p : {Coordinate{0, 0}, {0, 14}, {14, 0}, {14, 14}}) {
        CHECK(analyze_board(board, {p, StoneColor::Black}).verdict == Verdict::Legal);
    }
    for (const Coordinate p : {Coordinate{15, 0}, {0, -1}, {0, 15},
            {std::numeric_limits<int>::min(), 0}, {0, std::numeric_limits<int>::max()}}) {
        CHECK(analyze_board(board, {p, StoneColor::Black}).reason == Reason::OutOfBounds);
    }
    CHECK(analyze_board(board, {{7, 7}, static_cast<StoneColor>(99)}).reason == Reason::InvalidColor);
    board[0] = static_cast<Cell>(99);
    const auto before = board;
    CHECK(analyze_board(board, {{7, 7}, StoneColor::Black}).reason == Reason::InvalidCell);
    CHECK(board == before);
}
GameState from_board(const Board& board, StoneColor next)
{
    GameState state;
    // Deliberately exercise the storage API; these synthetic positions need not be legal games.
    std::vector<Placement> placements;
    for (std::size_t i = 0; i < board_area; ++i) {
        if (board[i] != Cell::Empty) {
            placements.push_back({*Coordinate::from_index(i), board[i] == Cell::Black
                ? StoneColor::Black : StoneColor::White, Participant::A});
        }
    }
    CHECK(state.reset({{}, placements.empty() ? next : placements.front().color}) == StateError::None);
    for (std::size_t i = 0; i < placements.size(); ++i) {
        CHECK(state.commit_placement(placements[i], i + 1 < placements.size()
            ? placements[i + 1].color : next) == StateError::None);
    }
    return state;
}
void state_adapter(const std::string& path)
{
    std::ifstream file(path);
    CHECK(file.good());
    for (std::string line; std::getline(file, line);) {
        if (line.empty() || line[0] == '#') { continue; }
        const auto f = parse(line);
        auto state = from_board(f.board, f.candidate.color);
        CHECK(state.assign_colors(Participant::B) == StateError::None);
        const auto before = state;
        CHECK(evaluate(state, f.candidate) == f.expected);
        CHECK(state == before);
    }
    GameState state;
    const auto initial = state;
    CHECK(evaluate(state, {{7, 7}, StoneColor::White}).reason == Reason::UnexpectedColor);
    CHECK(state == initial);
    CHECK(state.finish(Draw{}) == StateError::None);
    const auto finished = state;
    CHECK(evaluate(state, {{7, 7}, StoneColor::Black}).reason == Reason::AlreadyFinished);
    CHECK(state == finished);
}
// A filled board without any five: two-column stripes shifted on alternate rows.
Board draw_pattern()
{
    Board board{};
    for (int r = 0; r < 15; ++r) {
        for (int c = 0; c < 15; ++c) {
            board[static_cast<std::size_t>(r * 15 + c)] = ((c / 2 + r) % 2 == 0)
                ? Cell::Black : Cell::White;
        }
    }
    return board;
}
void move_limit()
{
    const Coordinate candidate{14, 14};
    auto board = draw_pattern();
    board[*candidate.index()] = Cell::Empty;
    // 198 and 199 recorded placements, including any prepared opening.
    for (std::size_t count : {198U, 199U, 200U}) {
        auto partial = board;
        for (std::size_t i = count; i < 224; ++i) { partial[i] = Cell::Empty; }
        auto state = from_board(partial, StoneColor::White);
        CHECK(state.history().size() == count);
        const auto before = state;
        const auto d = evaluate(state, {candidate, StoneColor::White});
        CHECK(d == (count == 198 ? Decision{Verdict::Legal, Reason::None}
            : count == 199 ? Decision{Verdict::Draw, Reason::MoveLimit}
            : Decision{Verdict::Invalid, Reason::MoveLimitReached}));
        CHECK(state == before);
    }
    for (const auto color : {StoneColor::Black, StoneColor::White}) {
        Board winning{};
        for (int c = 0; c < 4; ++c) { winning[static_cast<std::size_t>(14 * 15 + c)]
            = color == StoneColor::Black ? Cell::Black : Cell::White; }
        // Fill unrelated rows to reach exactly 199, leaving candidate (14,4) free.
        for (std::size_t i = 0; i < 195; ++i) { winning[i] = board[i]; }
        auto state = from_board(winning, color);
        CHECK(state.history().size() == 199);
        const auto before = state;
        CHECK(evaluate(state, {{14, 4}, color}).verdict == Verdict::Win);
        CHECK(state == before);
    }
    auto foul_board = draw_pattern();
    // Leave last row free except five blacks; candidate makes black overline at ply 200.
    for (std::size_t i = 195; i < 225; ++i) { foul_board[i] = Cell::Empty; }
    for (std::size_t i = 210; i < 215; ++i) { foul_board[i] = Cell::Black; }
    foul_board[194] = Cell::Empty;
    auto state = from_board(foul_board, StoneColor::Black);
    CHECK(state.history().size() == 199);
    CHECK(evaluate(state, {{14, 5}, StoneColor::Black}).verdict == Verdict::Forbidden);
}
void full_board()
{
    auto board = draw_pattern();
    // Every position in this pattern has no five; independently check all last-move choices.
    for (std::size_t i = 0; i < board_area; ++i) {
        const auto cell = board[i];
        board[i] = Cell::Empty;
        const auto before = board;
        CHECK(analyze_board(board, {*Coordinate::from_index(i), cell == Cell::Black
            ? StoneColor::Black : StoneColor::White}) == Decision{Verdict::Draw, Reason::BoardFull});
        CHECK(board == before);
        board[i] = cell;
    }
    // A full board must not hide the last move's victory.
    board.fill(Cell::White);
    for (std::size_t i = 210; i < 214; ++i) { board[i] = Cell::Black; }
    board[214] = Cell::Empty;
    CHECK(analyze_board(board, {{14, 4}, StoneColor::Black})
        == Decision{Verdict::Win, Reason::ExactFive});
    board.fill(Cell::White);
    board[112] = Cell::Empty;
    CHECK(analyze_board(board, {{7, 7}, StoneColor::White})
        == Decision{Verdict::Win, Reason::WhiteFiveOrMore});
}
} // namespace

int main(int argc, char** argv)
{
    try {
        if (argc == 2 && std::string(argv[1]) == "--probe") {
            for (std::string line; std::getline(std::cin, line);) {
                if (line.empty() || line[0] == '#') { continue; }
                const auto f = parse(line);
                const auto d = analyze_board(f.board, f.candidate);
                std::cout << f.id << '|' << verdicts[static_cast<std::size_t>(d.verdict)]
                    << '|' << reasons[static_cast<std::size_t>(d.reason)] << '\n';
            }
        } else if (argc == 3) {
            const std::string id = argv[1];
            if (id == "structural") { structural(); }
            else if (id == "state_adapter") { state_adapter(argv[2]); }
            else if (id == "move_limit") { move_limit(); }
            else if (id == "full_board") { full_board(); }
            else { cases(argv[2], id); }
        } else { return 2; }
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
