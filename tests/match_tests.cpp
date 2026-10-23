#include "omok/match/controller.hpp"
#include "omok/match/development_positions.hpp"
#include "omok/match/forbidden.hpp"
#include "board_geometry.hpp"
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <thread>

using namespace omok;
namespace st = state;
namespace mt = match;
namespace op = opening;
namespace {
#define CHECK(...) do { if (!(__VA_ARGS__)) { throw std::runtime_error( \
    std::string(__func__) + ":" + std::to_string(__LINE__) + ": " #__VA_ARGS__); } } while (false)
mt::Controller controller(mt::DevelopmentPosition position = mt::DevelopmentPosition::Basic,
    st::Participant black = st::Participant::A)
{
    mt::Controller c;
    const auto d = mt::development_data(position);
    CHECK(c.start_compact(d.id, d.compact, black).ok());
    return c;
}
void start_contract()
{
    auto c = controller();
    const auto before = c;
    op::Session idle;
    CHECK(c.start(idle).error == mt::Error::NotReady);
    CHECK(c == before);
    idle.begin_swap2();
    const std::array<op::Move, 3> moves{{{1, {7, 7}, st::StoneColor::Black},
        {2, {7, 8}, st::StoneColor::White}, {3, {8, 6}, st::StoneColor::Black}}};
    CHECK(idle.place_first_three(st::Participant::A, moves).ok());
    CHECK(c.start(idle).error == mt::Error::NotReady);
    CHECK(idle.choose(st::Participant::B, op::Choice::TakeBlack).ok());
    CHECK(c.start(idle).error == mt::Error::NotReady && c == before);
    op::PreparedData empty{"empty", {}};
    CHECK(idle.restore_prepared(empty, st::Participant::A).ok());
    CHECK(c.start(idle).error == mt::Error::EmptyOpening && c == before);
    const auto original = c.source_opening();
    CHECK(c.start(original).ok());
    CHECK(c.source_opening() == original && c.game() == original.game());
    CHECK(c.token().session == before.token().session + 1);
    auto terminal = controller(mt::DevelopmentPosition::Finished);
    const auto token = terminal.token();
    const auto finished = terminal;
    CHECK(terminal.phase() == mt::Phase::Finished && terminal.events().back().kind == mt::EventKind::Ended);
    CHECK(terminal.place(token, st::Participant::B, {1, 1}).error == mt::Error::NotPlaying);
    CHECK(terminal == finished);
}
void turns()
{
    for (const auto black : {st::Participant::A, st::Participant::B}) {
        auto c = controller(mt::DevelopmentPosition::Basic, black);
        const auto original = c.source_opening();
        const auto token = c.token();
        const auto white = *c.game().next_participant();
        CHECK(white != black && c.game().next_color() == st::StoneColor::White);
        const auto r = c.place(token, white, {8, 8});
        CHECK(r.ok() && r.decision.verdict == rules::Verdict::Legal);
        CHECK(c.game().next_participant() == black && c.game().next_color() == st::StoneColor::Black);
        CHECK(c.game().history().size() == 4 && c.game().history().back().placed_by == white);
        CHECK(c.token() == mt::Token{token.session, token.revision + 1});
        CHECK(c.source_opening() == original);
        CHECK(c.place(c.token(), black, {6, 7}).ok());
        CHECK(c.game().next_participant() == white && c.game().history().size() == 5);
    }
}
void rejected()
{
    auto c = controller();
    const auto before = c;
    const auto current = *c.game().next_participant();
    CHECK(c.place(c.token(), st::Participant::A, {1, 1}).error == mt::Error::WrongParticipant);
    CHECK(c == before);
    CHECK(c.place(c.token(), static_cast<st::Participant>(99), {1, 1}).error == mt::Error::InvalidParticipant);
    CHECK(c == before);
    for (const auto p : {st::Coordinate{-1, 1}, {15, 0}, {0, 15}, {std::numeric_limits<int>::max(), 0}, {7, 7}}) {
        const auto response = c.place(c.token(), current, p);
        CHECK(response.error == mt::Error::InvalidPlacement && response.decision.verdict == rules::Verdict::Invalid);
        CHECK(c == before); // entire controller, opening, event log, token and result
    }
    CHECK(c.start_compact("bad", "h8h8", st::Participant::A).error == mt::Error::InvalidOpening);
    CHECK(c == before);
    CHECK(c.start_compact("bad", "", st::Participant::A).error == mt::Error::EmptyOpening);
    CHECK(c == before);
    CHECK(c.start_compact("bad", "h8i8g9", static_cast<st::Participant>(99)).error == mt::Error::InvalidOpening);
    CHECK(c == before);
}
void win()
{
    auto c = controller(mt::DevelopmentPosition::Win, st::Participant::B);
    CHECK(c.game().history().size() == 7);
    CHECK(c.place(c.token(), st::Participant::A, {0, 6}).ok());
    const auto r = c.place(c.token(), st::Participant::B, {7, 4});
    CHECK(r.ok() && r.decision == rules::Decision{rules::Verdict::Win, rules::Reason::ExactFive});
    CHECK(c.game().result() == st::GameResult{st::Won{st::Participant::B}});
    CHECK(c.phase() == mt::Phase::Finished && c.end_reason() == mt::EndReason::Five);
    CHECK(!c.game().next_participant() && c.game().history().size() == 9);
    const auto finished = c;
    CHECK(c.place(c.token(), st::Participant::A, {1, 1}).error == mt::Error::NotPlaying);
    CHECK(c.resign(c.token(), st::Participant::A).error == mt::Error::NotPlaying);
    CHECK(c == finished);
}
void forbidden()
{
    auto c = controller(mt::DevelopmentPosition::Forbidden);
    const auto source = c.source_opening();
    const auto history = c.game();
    const auto response = c.place(c.token(), st::Participant::A, {7, 7});
    CHECK(response.ok() && response.decision == rules::Decision{rules::Verdict::Forbidden, rules::Reason::DoubleThree});
    CHECK(c.game().result() == st::GameResult{st::Won{st::Participant::B}});
    CHECK(c.end_reason() == mt::EndReason::Forbidden && c.phase() == mt::Phase::Finished);
    CHECK(c.game().history().size() == history.history().size());
    CHECK(std::equal(c.game().board().begin(), c.game().board().end(), history.board().begin()));
    CHECK(c.source_opening() == source);
    CHECK(c.events().back().position == st::Coordinate{7, 7});
    CHECK(c.events().back().ending == mt::EndReason::Forbidden);
    const auto ended = c;
    CHECK(c.place(c.token(), st::Participant::A, {6, 6}).error == mt::Error::NotPlaying && c == ended);
}
void draw()
{
    auto c = controller(mt::DevelopmentPosition::Draw);
    CHECK(c.game().history().size() == 199);
    std::optional<st::Coordinate> final;
    for (int r = 0; r < 15 && !final; ++r) {
        for (int col = 0; col < 15; ++col) {
            if ((col / 2 + r) % 2 == 1 && c.game().cell_at({r, col}) == st::Cell::Empty) { final = {r, col}; break; }
        }
    }
    CHECK(final);
    CHECK(c.place(c.token(), *c.game().next_participant(), *final).decision.verdict == rules::Verdict::Draw);
    CHECK(c.game().history().size() == 200 && c.game().result() == st::GameResult{st::Draw{}});
    CHECK(c.end_reason() == mt::EndReason::MoveLimit && !c.game().next_color());
    const auto ended = c;
    CHECK(c.place(c.token(), st::Participant::A, {14, 14}).error == mt::Error::NotPlaying && c == ended);
}
void resignation()
{
    auto c = controller();
    const auto source = c.source_opening();
    const auto before = c;
    CHECK(c.resign(c.token(), static_cast<st::Participant>(99)).error == mt::Error::InvalidParticipant && c == before);
    CHECK(c.resign(c.token(), st::Participant::A).ok()); // resignation does not require own turn
    CHECK(c.game().result() == st::GameResult{st::Won{st::Participant::B}});
    CHECK(c.end_reason() == mt::EndReason::Resigned && c.source_opening() == source);
}
void restart_stale()
{
    auto c = controller();
    const auto old = c.token();
    CHECK(c.place(old, *c.game().next_participant(), {8, 8}).ok());
    const auto after_move = c;
    CHECK(c.place(old, *c.game().next_participant(), {6, 6}).error == mt::Error::StaleInput && c == after_move);
    CHECK(c.restart(c.token()).ok());
    CHECK(c.token().session == old.session + 1 && c.token().revision == 0);
    CHECK(c.game() == c.source_opening().game() && c.events().size() == 1);
    CHECK(c.events()[0].kind == mt::EventKind::Restarted);
    const auto restarted = c;
    CHECK(c.place(old, st::Participant::B, {8, 8}).error == mt::Error::StaleInput);
    CHECK(c.resign(old, st::Participant::B).error == mt::Error::StaleInput);
    CHECK(c.restart(old).error == mt::Error::StaleInput);
    CHECK(c.close(old).error == mt::Error::StaleInput && c == restarted);
}
void close()
{
    auto c = controller();
    CHECK(c.close(c.token()).ok());
    CHECK(c.phase() == mt::Phase::Closed && c.game().result() == st::GameResult{st::Aborted{}});
    CHECK(c.end_reason() == mt::EndReason::Closed);
    const auto closed = c;
    CHECK(c.place(c.token(), st::Participant::B, {1, 1}).error == mt::Error::NotPlaying && c == closed);
    CHECK(c.restart(c.token()).ok() && c.phase() == mt::Phase::Playing);
    CHECK(c.resign(c.token(), st::Participant::B).ok());
    const auto result = c.game().result();
    CHECK(c.close(c.token()).ok() && c.game().result() == result); // closing preserves a completed result
}
void geometry()
{
    for (const unsigned dpi : {96U, 120U, 144U, 192U}) {
        for (const auto dimensions : {gui::Pixel{1100, 850}, {980, 700}, {1600, 1000}}) {
            const auto g = gui::board_geometry(dimensions.x * static_cast<int>(dpi) / 96,
                dimensions.y * static_cast<int>(dpi) / 96, dpi);
            CHECK(g.valid());
            for (std::size_t i = 0; i < st::board_area; ++i) {
                const auto p = *st::Coordinate::from_index(i);
                CHECK(g.hit(g.point(p)) == p);
            }
            CHECK(!g.hit({g.left - 1, g.top}));
            CHECK(!g.hit({g.left + 14 * g.step + 1, g.top}));
            CHECK(!g.hit({g.left + g.step / 2, g.top + g.step / 2}));
            CHECK(g.hit({g.left + g.step / 5, g.top}) == st::Coordinate{0, 0});
        }
    }
    CHECK(!gui::board_geometry(10, 10, 96).valid());
    CHECK(!gui::board_geometry(1100, 850, 0).valid());
    CHECK(!gui::board_geometry(1100, 850, 96).hit({std::numeric_limits<int>::max(), 0}));
    const auto old = gui::board_geometry(1100, 850, 96);
    const auto resized = gui::board_geometry(1400, 1000, 96);
    CHECK(old.point({7, 7}) != resized.point({7, 7}));
    CHECK(resized.hit(resized.point({7, 7})) == st::Coordinate{7, 7});
}
st::GameState dense_board()
{
    std::vector<st::Placement> placements;
    for (int r = 3; r <= 11; ++r) {
        for (int c = 3; c <= 11; ++c) {
            const int n = (r * 17 + c * 31) % 7;
            if (n < 5) { placements.push_back({{r, c}, n < 4 ? st::StoneColor::Black : st::StoneColor::White, {}}); }
        }
    }
    st::GameState game;
    CHECK(game.reset({st::Participant::A, placements[0].color}) == st::StateError::None);
    for (std::size_t i = 0; i < placements.size(); ++i) {
        CHECK(game.commit_placement(placements[i], i + 1 < placements.size()
            ? placements[i + 1].color : st::StoneColor::Black) == st::StateError::None);
    }
    return game;
}
void hints()
{
    const auto c = controller(mt::DevelopmentPosition::Forbidden);
    const auto before = c;
    const auto result = mt::compute_forbidden({c.token(), c.game()});
    CHECK(result.complete && !result.failed && result.forbidden[*st::Coordinate{7, 7}.index()]);
    CHECK(c == before);
    mt::HintCache cache;
    CHECK(cache.accept(result, c.token(), true));
    const auto accepted = cache;
    CHECK(!cache.accept(result, {c.token().session + 1, 0}, true) && cache == accepted);
    CHECK(!cache.accept(result, {c.token().session, 1}, true) && cache == accepted);
    CHECK(!cache.accept(result, c.token(), false) && cache == accepted);
    cache.clear();
    auto cancelled = mt::compute_forbidden({c.token(), c.game()}, [] { return true; });
    CHECK(!cancelled.complete && !cache.accept(cancelled, c.token(), true));
    cancelled.complete = true; cancelled.failed = true;
    CHECK(!cache.accept(cancelled, c.token(), true) && !cache.value());
}
void worker()
{
    mt::HintWorker worker;
    worker.submit({{1, 0}, dense_board()});
    auto c = controller(mt::DevelopmentPosition::Forbidden);
    const mt::Token latest{2, 4};
    worker.submit({latest, c.game()});
    std::optional<mt::HintResult> result;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!result && std::chrono::steady_clock::now() < deadline) {
        result = worker.take_result();
        if (!result) { std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
    }
    CHECK(result && result->token == latest && result->complete);
    CHECK(result->forbidden[*st::Coordinate{7, 7}.index()]);
    worker.submit({{3, 0}, dense_board()});
    worker.cancel();
    CHECK(!worker.take_result());
}
void benchmark()
{
    const auto basic = controller();
    const auto threat = controller(mt::DevelopmentPosition::Forbidden);
    const std::array inputs{basic.game(), threat.game(), dense_board()};
    const std::array names{"basic_3", "cross_threat_8", "dense_synthetic"};
    std::cout << "case,placements,samples,min_ms,mean_ms,max_ms\n" << std::fixed << std::setprecision(6);
    for (std::size_t i = 0; i < inputs.size(); ++i) {
        std::vector<double> times;
        for (int n = 0; n < 10; ++n) {
            const auto result = mt::compute_forbidden({{1, 0}, inputs[i]});
            CHECK(result.complete);
            times.push_back(result.milliseconds);
        }
        std::cout << names[i] << ',' << inputs[i].history().size() << ",10," << *std::min_element(times.begin(), times.end())
            << ',' << std::accumulate(times.begin(), times.end(), 0.0) / static_cast<double>(times.size())
            << ',' << *std::max_element(times.begin(), times.end()) << '\n';
    }
}
}
int main(int argc, char** argv)
{
    try {
        if (argc != 2) { return 2; }
        const std::string name = argv[1];
        if (name == "start_contract") { start_contract(); }
        else if (name == "turns") { turns(); }
        else if (name == "rejected") { rejected(); }
        else if (name == "win") { win(); }
        else if (name == "forbidden") { forbidden(); }
        else if (name == "draw") { draw(); }
        else if (name == "resignation") { resignation(); }
        else if (name == "restart_stale") { restart_stale(); }
        else if (name == "close") { close(); }
        else if (name == "geometry") { geometry(); }
        else if (name == "hints") { hints(); }
        else if (name == "worker") { worker(); }
        else if (name == "benchmark") { benchmark(); }
        else { return 2; }
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
