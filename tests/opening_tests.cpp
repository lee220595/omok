#include "omok/opening/opening.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

using namespace omok::state;
using namespace omok::opening;
namespace rules = omok::rules;
namespace {
#define CHECK(...) do { if (!(__VA_ARGS__)) { throw std::runtime_error( \
    std::string(__func__) + ":" + std::to_string(__LINE__) + ": " #__VA_ARGS__); } } while (false)

const std::vector<Move> first{{1, {7, 7}, StoneColor::Black},
    {2, {7, 8}, StoneColor::White}, {3, {8, 6}, StoneColor::Black}};
const std::array<Move, 2> extra{{{4, {8, 8}, StoneColor::White}, {5, {6, 8}, StoneColor::Black}}};
const std::array<Move, 1> sixth{{{6, {8, 5}, StoneColor::White}}};
PreparedData data(std::vector<Move> moves = first) { return {"self-authored-v1", std::move(moves)}; }

Session after_three()
{
    Session session;
    session.begin_swap2();
    CHECK(session.place_first_three(Participant::A, first).ok());
    return session;
}
Session after_five()
{
    auto session = after_three();
    CHECK(session.choose(Participant::B, Choice::OfferTwo, extra).ok());
    return session;
}
void check_prefix(const Session& before, const Session& after)
{
    CHECK(after.game().history().size() >= before.game().history().size());
    CHECK(std::equal(before.game().history().begin(), before.game().history().end(), after.game().history().begin()));
    for (std::size_t i = 0; i < board_area; ++i) {
        if (before.game().board()[i] != Cell::Empty) { CHECK(after.game().board()[i] == before.game().board()[i]); }
    }
}
void loader(const std::string& path)
{
    PreparedData output = data();
    std::ifstream file(path);
    CHECK(file.good());
    std::string text;
    CHECK(static_cast<bool>(std::getline(file, text)));
    CHECK(load_compact("self-authored-v1", text, output).ok());
    CHECK(output == data());
    CHECK(load_compact("asymmetric", "A15o1b12", output).ok());
    CHECK(output.moves[0] == Move{1, {14, 0}, StoneColor::Black});
    CHECK(output.moves[1] == Move{2, {0, 14}, StoneColor::White});
    CHECK(output.moves[2] == Move{3, {11, 1}, StoneColor::Black});
    const auto before = output;
    for (const std::string_view bad : {"a", "1a", "a0", "a01", "a16", "p1", "a123", "h8i8h8", "h8 i8", "h8\n", "a-1", "a1!"}) {
        CHECK(!load_compact("invalid", bad, output).ok());
        CHECK(output == before);
    }
    CHECK(load_compact("", "a1", output).error == Error::MissingId);
    CHECK(output == before);
    std::string too_long;
    for (std::size_t i = 0; i < 201; ++i) {
        const auto p = *Coordinate::from_index(i);
        too_long += static_cast<char>('a' + p.column);
        too_long += std::to_string(p.row + 1);
    }
    CHECK(load_compact("long", too_long, output).error == Error::TooLong);
    CHECK(output == before);
    CHECK(load_compact("empty", "", output).ok());
    CHECK(output.moves.empty());
}
void prepared_restore()
{
    for (const auto owner : {Participant::A, Participant::B}) {
        for (std::size_t count : {0U, 1U, 2U, 3U}) {
            Session session;
            CHECK(session.restore_prepared(data({first.begin(), first.begin() + static_cast<std::ptrdiff_t>(count)}), owner).ok());
            CHECK(session.mode() == Mode::PreparedRenju && session.stage() == Stage::Complete);
            const auto h = session.handoff();
            CHECK(h.can_enter_main_game() && !h.input_actor && h.total_placements == count);
            CHECK(h.next_color == (count % 2 == 0 ? StoneColor::Black : StoneColor::White));
            CHECK(h.next_participant == session.game().colors().owner_of(*h.next_color));
            CHECK(session.game().colors().owner_of(StoneColor::Black) == owner);
            for (std::size_t i = 0; i < count; ++i) {
                CHECK(session.game().history()[i] == Placement{first[i].position, first[i].color, {}});
            }
        }
    }
}
void prepared_rejection()
{
    Session assigned;
    CHECK(assigned.restore_prepared(data(), Participant::B).ok());
    for (Session original : {assigned, after_five()}) {
        const auto before = original;
        auto bad = data();
        bad.id.clear();
        CHECK(original.restore_prepared(bad, Participant::A).error == Error::MissingId);
        CHECK(original == before);
        CHECK(original.restore_prepared(data(), static_cast<Participant>(99)).error == Error::InvalidParticipant);
        CHECK(original == before);
        for (int kind = 0; kind < 5; ++kind) {
            bad = data();
            if (kind == 0) { bad.moves[2].position = {-1, 2}; }
            if (kind == 1) { bad.moves[2].position = bad.moves[0].position; }
            if (kind == 2) { bad.moves[2].color = StoneColor::White; }
            if (kind == 3) { bad.moves[2].number = 2; }
            if (kind == 4) { bad.moves[2].color = static_cast<StoneColor>(99); }
            const auto status = original.restore_prepared(bad, Participant::A);
            CHECK(!status.ok() && status.move_number == bad.moves[2].number);
            CHECK(original == before); // includes colors, result, stage, id and events
        }
        bad = data(std::vector<Move>(201, first[0]));
        CHECK(original.restore_prepared(bad, Participant::A).error == Error::TooLong);
        CHECK(original == before);
    }
}
PreparedData black_terminal(bool forbidden = false)
{
    std::vector<Move> moves;
    const std::array<Coordinate, 5> black = forbidden
        ? std::array<Coordinate, 5>{{{7, 6}, {6, 7}, {7, 8}, {8, 7}, {7, 7}}}
        : std::array<Coordinate, 5>{{{7, 0}, {7, 1}, {7, 2}, {7, 3}, {7, 4}}};
    for (int n = 0; n < 9; ++n) {
        moves.push_back({static_cast<std::size_t>(n + 1), n % 2 == 0
            ? black[static_cast<std::size_t>(n / 2)] : Coordinate{0, n - 1},
            n % 2 == 0 ? StoneColor::Black : StoneColor::White});
    }
    return data(std::move(moves));
}
void prepared_terminal()
{
    Session session = after_five();
    const auto before = session;
    const auto status = session.restore_prepared(black_terminal(true), Participant::A);
    CHECK(status.error == Error::ForbiddenPlacement && status.move_number == 9);
    CHECK(status.reason == rules::Reason::DoubleThree && session == before);
    auto trailing = black_terminal();
    trailing.moves.push_back({10, {1, 1}, StoneColor::White});
    CHECK(session.restore_prepared(trailing, Participant::A).error == Error::AfterTerminal);
    CHECK(session == before);
    CHECK(session.restore_prepared(black_terminal(), Participant::B).ok());
    CHECK(session.game().result() == GameResult{Won{Participant::B}});
    CHECK(session.stage() == Stage::Finished && session.game().history().size() == 9);
    CHECK(session.handoff().continuation == Continuation::Terminal);
    CHECK(!session.handoff().can_enter_main_game() && !session.handoff().next_participant);
    CHECK(!session.game().next_color());
    CHECK(rules::evaluate(session.game(), {{1, 1}, StoneColor::White}).reason == rules::Reason::AlreadyFinished);
}
PreparedData long_prepared(std::size_t count, bool final_win = false)
{
    std::vector<Coordinate> black, white;
    const int last_row = final_win ? 14 : 15;
    for (int r = 0; r < last_row; ++r) {
        for (int c = 0; c < 15; ++c) {
            (((c / 2 + r) % 2 == 0) ? black : white).push_back({r, c});
        }
    }
    if (final_win) {
        white.resize(95);
        for (int c = 0; c < 5; ++c) { white.push_back({14, c}); }
    }
    std::vector<Move> moves;
    for (std::size_t i = 0; i < count; ++i) {
        const auto& points = i % 2 == 0 ? black : white;
        moves.push_back({i + 1, points.at(i / 2), i % 2 == 0 ? StoneColor::Black : StoneColor::White});
    }
    return data(std::move(moves));
}
void prepared_limit()
{
    Session session;
    for (std::size_t count : {198U, 199U, 200U}) {
        const auto input = long_prepared(count);
        CHECK(session.restore_prepared(input, Participant::A).ok());
        CHECK(session.game().history().size() == count);
        if (count == 200) {
            CHECK(session.stage() == Stage::Finished && session.game().result() == GameResult{Draw{}});
            CHECK(!session.handoff().can_enter_main_game() && !session.game().next_color());
        } else {
            CHECK(session.handoff().can_enter_main_game());
            const auto next = long_prepared(count + 1).moves.back();
            const auto decision = rules::evaluate(session.game(), {next.position, next.color});
            CHECK(decision.verdict == (count == 198 ? rules::Verdict::Legal : rules::Verdict::Draw));
        }
    }
    CHECK(session.restore_prepared(long_prepared(199, true), Participant::A).ok());
    CHECK(session.handoff().next_participant == Participant::B);
    CHECK(rules::evaluate(session.game(), {{14, 4}, StoneColor::White}).verdict == rules::Verdict::Win);
    CHECK(session.restore_prepared(long_prepared(200, true), Participant::A).ok());
    CHECK(session.game().result() == GameResult{Won{Participant::B}});
    CHECK(!session.handoff().can_enter_main_game());
    const auto terminal = session;
    CHECK(session.restore_prepared(long_prepared(201), Participant::B).error == Error::TooLong);
    CHECK(session == terminal);
}
void prepared_first_move()
{
    Session session;
    CHECK(session.restore_prepared(data(), Participant::B).ok());
    const auto snapshot = session;
    const auto h = session.handoff();
    CHECK(h.next_color == StoneColor::White && h.next_participant == Participant::A);
    const auto decision = rules::evaluate(session.game(), {{8, 8}, *h.next_color});
    CHECK(decision == rules::Decision{rules::Verdict::Legal, rules::Reason::None});
    CHECK(session == snapshot);
    // Exercise the documented handoff to a future single-writer controller using a copy.
    GameState next = session.game();
    CHECK(next.commit_placement({{8, 8}, *h.next_color, *h.next_participant}, StoneColor::Black) == StateError::None);
    CHECK(next.history().size() == 4 && next.next_participant() == Participant::B);
    CHECK(next.history().back().placed_by == Participant::A);
    CHECK(session == snapshot);
}
void swap2_path(bool offer_two, bool take_white)
{
    auto session = offer_two ? after_five() : after_three();
    const auto before = session;
    const auto actor = offer_two ? Participant::A : Participant::B;
    CHECK(session.handoff().input_actor == actor);
    CHECK(!session.game().colors().owner_of(StoneColor::Black));
    CHECK(!session.game().next_participant());
    const std::span<const Move> response = take_white
        ? (offer_two ? std::span<const Move>{sixth} : std::span<const Move>{extra}.first(1))
        : std::span<const Move>{};
    CHECK(session.choose(actor, take_white ? Choice::TakeWhite : Choice::TakeBlack, response).ok());
    check_prefix(before, session);
    const auto owner = take_white ? (actor == Participant::A ? Participant::B : Participant::A) : actor;
    CHECK(session.game().colors().owner_of(StoneColor::Black) == owner);
    CHECK(session.game().colors().owner_of(StoneColor::White) != owner);
    CHECK(session.game().history().size() == (offer_two ? 5U : 3U) + (take_white ? 1U : 0U));
    if (!take_white) { CHECK(session.game().history().size() == before.game().history().size()); }
    CHECK(session.stage() == Stage::Complete && session.mode() == Mode::Swap2Protocol);
    CHECK(session.handoff().continuation == Continuation::RuleContractRequired);
    CHECK(!session.handoff().can_enter_main_game() && !session.handoff().input_actor);
    CHECK(session.handoff().next_participant == (offer_two ? Participant::B : Participant::A));
    CHECK(session.handoff().next_color == (take_white ? StoneColor::Black : StoneColor::White));
    const auto complete = session;
    CHECK(session.choose(actor, Choice::TakeBlack).error == Error::WrongStage);
    CHECK(session == complete);
}
void swap2_rejections()
{
    for (Session session : {Session{}, after_three(), after_five()}) {
        const auto before = session;
        CHECK(session.place_first_three(Participant::A, first).error == Error::WrongStage);
        CHECK(session == before);
        CHECK(session.choose(static_cast<Participant>(99), Choice::TakeBlack).error == Error::InvalidParticipant);
        CHECK(session == before);
        if (session.stage() == Stage::Idle) {
            CHECK(session.choose(Participant::A, Choice::TakeBlack).error == Error::WrongStage);
        } else {
            const auto actor = *session.handoff().input_actor;
            const auto wrong = actor == Participant::A ? Participant::B : Participant::A;
            CHECK(session.choose(wrong, Choice::TakeBlack).error == Error::WrongActor);
            CHECK(session.choose(actor, static_cast<Choice>(99)).error == Error::InvalidChoice);
            CHECK(session.choose(actor, Choice::TakeWhite).error == Error::WrongCount);
            CHECK(session.choose(actor, Choice::TakeBlack, extra).error == Error::WrongCount);
            if (session.stage() == Stage::FinalChoice) {
                CHECK(session.choose(actor, Choice::OfferTwo, extra).error == Error::InvalidChoice);
            }
        }
        CHECK(session == before);
    }
    Session initial;
    initial.begin_swap2();
    const auto before = initial;
    CHECK(initial.place_first_three(Participant::B, first).error == Error::WrongActor);
    CHECK(initial.place_first_three(static_cast<Participant>(99), first).error == Error::InvalidParticipant);
    CHECK(initial.place_first_three(Participant::A, std::span<const Move>{first}.first(2)).error == Error::WrongCount);
    CHECK(initial.choose(Participant::A, Choice::TakeBlack).error == Error::WrongStage);
    CHECK(initial == before);
}
void swap2_atomic()
{
    Session initial;
    initial.begin_swap2();
    const auto before = initial;
    auto bad = first;
    bad[2].position = bad[0].position;
    CHECK(initial.place_first_three(Participant::A, bad).reason == rules::Reason::Occupied);
    CHECK(initial == before);
    for (int kind = 0; kind < 3; ++kind) {
        bad = first;
        if (kind == 0) { bad[1].color = StoneColor::Black; }
        if (kind == 1) { bad[1].number = 3; }
        if (kind == 2) { bad[1].position = {0, 15}; }
        CHECK(!initial.place_first_three(Participant::A, bad).ok());
        CHECK(initial == before);
    }
    for (int kind = 0; kind < 4; ++kind) {
        auto session = after_three();
        const auto snapshot = session;
        auto more = extra;
        if (kind == 0) { more[1].position = more[0].position; }
        if (kind == 1) { more[1].position = {15, 0}; }
        if (kind == 2) { more[1].color = StoneColor::White; }
        if (kind == 3) { more[1].number = 6; }
        CHECK(!session.choose(Participant::B, Choice::OfferTwo, more).ok());
        CHECK(session == snapshot); // first extra stone AND choice event rolled back
    }
    auto session = after_five();
    const auto snapshot = session;
    auto bad_sixth = sixth;
    bad_sixth[0].position = first[0].position;
    CHECK(!session.choose(Participant::A, Choice::TakeWhite, bad_sixth).ok());
    CHECK(session == snapshot);
}
void event_history()
{
    auto session = after_five();
    CHECK(session.game().history()[0].placed_by == Participant::A);
    CHECK(session.game().history()[1].placed_by == Participant::A); // A placed a white stone
    CHECK(session.game().history()[3].placed_by == Participant::B);
    const auto before = session;
    CHECK(session.choose(Participant::A, Choice::TakeWhite, sixth).ok());
    check_prefix(before, session);
    CHECK(session.game().history()[5].placed_by == Participant::A);
    const std::array types{EventType::Started, EventType::BatchPlaced, EventType::ChoiceMade,
        EventType::BatchPlaced, EventType::ChoiceMade, EventType::BatchPlaced,
        EventType::ColorsAssigned, EventType::Completed};
    CHECK(session.events().size() == types.size());
    for (std::size_t i = 0; i < types.size(); ++i) { CHECK(session.events()[i].type == types[i]); }
    CHECK(session.events()[2].actor == Participant::B && session.events()[2].choice == Choice::OfferTwo);
    CHECK(session.events()[4].actor == Participant::A && session.events()[4].choice == Choice::TakeWhite);
    CHECK(session.events()[6].black_owner == Participant::B && session.events()[6].total_placements == 6);
    CHECK(session.restore_prepared(data(), Participant::A).ok());
    CHECK(session.events().size() == 4 && session.data_id() == "self-authored-v1");
    CHECK(!session.events()[1].actor && session.events()[1].black_owner == Participant::A);
    CHECK(!session.events()[2].actor); // imported, no fabricated participant
}
void initialization()
{
    Session session;
    CHECK(session.handoff().continuation == Continuation::NotStarted);
    CHECK(session.events().empty() && session.data_id().empty());
    CHECK(session.restore_prepared(black_terminal(), Participant::A).ok());
    session.begin_swap2();
    CHECK(session.game() == GameState{});
    CHECK(session.stage() == Stage::FirstThree && session.handoff().input_actor == Participant::A);
    CHECK(session.events().size() == 1 && session.data_id().empty());
    CHECK(!session.handoff().can_enter_main_game() && !session.game().next_participant());
    CHECK(session.restore_prepared(data(), Participant::B).ok());
    const auto before = session;
    CHECK(session.choose(Participant::B, Choice::TakeBlack).error == Error::WrongStage);
    CHECK(session == before);
}
void snapshot(const std::string& id, const Session& session)
{
    const auto actor = [](std::optional<Participant> p) { return !p ? '-' : *p == Participant::A ? 'A' : 'B'; };
    const auto color = [](std::optional<StoneColor> c) { return !c ? '-' : *c == StoneColor::Black ? 'B' : 'W'; };
    std::cout << id << '|' << actor(session.game().colors().owner_of(StoneColor::Black)) << '|'
        << color(session.game().next_color()) << '|' << actor(session.game().next_participant()) << '|';
    for (const auto cell : session.game().board()) { std::cout << (cell == Cell::Empty ? '0' : cell == Cell::Black ? '1' : '2'); }
    std::cout << '|';
    bool separator = false;
    for (const auto& placement : session.game().history()) {
        if (separator) { std::cout << ';'; }
        separator = true;
        std::cout << placement.position.row << ',' << placement.position.column << ',' << color(placement.color)
            << ',' << actor(placement.placed_by);
    }
    std::cout << '\n';
}
void reference_snapshots()
{
    for (std::size_t count : {0U, 3U, 4U}) {
        auto moves = first;
        if (count == 0) { moves.clear(); }
        if (count == 4) { moves.push_back(extra[0]); }
        Session session;
        CHECK(session.restore_prepared(data(moves), Participant::B).ok());
        snapshot("prepared_" + std::to_string(count), session);
    }
    for (bool offer_two : {false, true}) {
        for (bool take_white : {false, true}) {
            auto session = offer_two ? after_five() : after_three();
            const auto actor = offer_two ? Participant::A : Participant::B;
            const auto moves = take_white ? (offer_two ? std::span<const Move>{sixth}
                : std::span<const Move>{extra}.first(1)) : std::span<const Move>{};
            CHECK(session.choose(actor, take_white ? Choice::TakeWhite : Choice::TakeBlack, moves).ok());
            snapshot(std::string(offer_two ? "two_" : "direct_") + (take_white ? "white" : "black"), session);
        }
    }
}
} // namespace

int main(int argc, char** argv)
{
    try {
        if (argc == 2 && std::string_view(argv[1]) == "--reference-snapshots") {
            reference_snapshots();
            return 0;
        }
        if (argc != 3) { return 2; }
        const std::string_view name = argv[1];
        if (name == "loader") { loader(argv[2]); }
        else if (name == "prepared_restore") { prepared_restore(); }
        else if (name == "prepared_rejection") { prepared_rejection(); }
        else if (name == "prepared_terminal") { prepared_terminal(); }
        else if (name == "prepared_limit") { prepared_limit(); }
        else if (name == "prepared_first_move") { prepared_first_move(); }
        else if (name == "swap2_take_black") { swap2_path(false, false); }
        else if (name == "swap2_take_white") { swap2_path(false, true); }
        else if (name == "swap2_two_take_black") { swap2_path(true, false); }
        else if (name == "swap2_two_take_white") { swap2_path(true, true); }
        else if (name == "swap2_rejections") { swap2_rejections(); }
        else if (name == "swap2_atomic") { swap2_atomic(); }
        else if (name == "event_history") { event_history(); }
        else if (name == "initialization") { initialization(); }
        else { return 2; }
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
