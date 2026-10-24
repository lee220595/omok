#include "omok/ai/engine.hpp"
#include "omok/match/development_positions.hpp"
#include <windows.h>
#include <chrono>
#include <iostream>
#include <stdexcept>
using namespace omok;
namespace {
#define CHECK(...) do { if (!(__VA_ARGS__)) throw std::runtime_error(std::string(__func__) + ":" + std::to_string(__LINE__) + ": " #__VA_ARGS__); } while(false)
match::Controller game(state::Participant black = state::Participant::A)
{
    match::Controller c; CHECK(c.start_compact("ai-test", "h8i8g9", black).ok()); return c;
}
ai::Request request(const match::Controller& c, const std::filesystem::path& mock, const std::wstring& mode)
{
    return {c.token(), c.game(), *c.game().next_participant(), mock.parent_path() / (L"mock-" + mode + L".exe"), {}, {250, 10000, 2000}};
}
void reaped(const ai::Result& r)
{
    CHECK(r.process_id != 0 && r.process_reaped);
    const auto handle = OpenProcess(SYNCHRONIZE, FALSE, r.process_id);
    if (handle) { CHECK(WaitForSingleObject(handle, 0) == WAIT_OBJECT_0); CloseHandle(handle); }
}
void protocol()
{
    for (const auto owner : {state::Participant::A, state::Participant::B}) {
        auto c = game(owner); const auto before = c;
        const auto actor = *c.game().next_participant();
        CHECK(ai::board_command(c.game(), actor) == "BOARD\r\n7,7,2\r\n8,7,1\r\n6,8,2\r\nDONE\r\n");
        CHECK(c.place(c.token(), actor, {8, 8}).ok());
        CHECK(ai::board_command(c.game(), owner) == "BOARD\r\n7,7,1\r\n8,7,2\r\n6,8,1\r\n8,8,2\r\nDONE\r\n");
        CHECK(before.game().history()[0].placed_by == std::nullopt);
    }
    CHECK(ai::parse_line(" 8,6\r").position == state::Coordinate{6, 8});
    CHECK(ai::parse_line("MESSAGE 8,6").kind == ai::LineKind::Diagnostic);
    for (auto text : {"SWAP", "8,6 9,7", "8,6junk", "99999999999999,1", "1,", "OKAY"}) {
        CHECK(ai::parse_line(text).kind == ai::LineKind::Invalid);
    }
    CHECK(ai::parse_line("-1,15").position == state::Coordinate{15, -1}); // no clamping
    bool threw = false; try { static_cast<void>(ai::board_command(game().game(), state::Participant::A)); } catch (...) { threw = true; }
    CHECK(threw);
}
void normal(const std::filesystem::path& mock)
{
    auto c = game(); const auto before = c; const auto r = ai::request_move(request(c, mock, L"normal"));
    CHECK(r.outcome == ai::Outcome::Move && r.position == state::Coordinate{8, 8}); reaped(r);
    CHECK(r.exit_code == 0 && !r.forced_cleanup && c == before);
    CHECK(r.transcript.find("fixture stderr") != std::string::npos);
    CHECK(ai::apply_result(c, r).ok()); CHECK(c.game().history().size() == 4);
    CHECK(c.game().history().back().placed_by == state::Participant::B);
    const auto after = c; CHECK(ai::apply_result(c, r).error == match::Error::StaleInput && c == after);
}
void first_move(const std::filesystem::path& mock)
{
    for (const auto black : {state::Participant::A, state::Participant::B}) {
        match::Controller c; CHECK(c.start_direct(black).ok());
        const auto before = c;
        CHECK(ai::move_command(c.game(), black) == "BEGIN\r\n");
        const auto white = black == state::Participant::A ? state::Participant::B : state::Participant::A;
        bool threw = false;
        try { static_cast<void>(ai::move_command(c.game(), white)); } catch (const std::invalid_argument&) { threw = true; }
        CHECK(threw && c == before);
        const auto r = ai::request_move(request(c, mock, L"normal")); reaped(r);
        CHECK(r.outcome == ai::Outcome::Move && c == before);
        CHECK(r.transcript.find("> BEGIN\r\n") != std::string::npos && r.transcript.find("> BOARD") == std::string::npos);
        CHECK(ai::apply_result(c, r).ok() && c.game().history().size() == 1);
        CHECK(c.game().history().back().color == state::StoneColor::Black && c.game().history().back().placed_by == black);
        CHECK(c.game().next_participant() == white);
        CHECK(c.restart(c.token()).ok() && c.game().history().empty());
        const auto restarted = c;
        CHECK(ai::apply_result(c, r).error == match::Error::StaleInput && c == restarted);
        CHECK(c.place(c.token(), black, {7, 7}).ok());
        CHECK(ai::move_command(c.game(), white) == "BOARD\r\n7,7,2\r\nDONE\r\n");
        const auto response = ai::request_move(request(c, mock, L"normal")); reaped(response);
        CHECK(response.outcome == ai::Outcome::Move && ai::apply_result(c, response).ok());
        CHECK(c.game().history().size() == 2 && c.game().history().back().color == state::StoneColor::White);
        CHECK(c.game().next_participant() == black);
    }
}
void errors(const std::filesystem::path& mock)
{
    for (const auto mode : {L"bad", L"long", L"exit", L"crashafter", L"timeout", L"hang", L"startfail", L"bootexit"}) {
        auto c = game(); const auto r = ai::request_move(request(c, mock, mode)); reaped(r);
        if (std::wstring(mode) == L"startfail" || std::wstring(mode) == L"bootexit") { CHECK(r.outcome == ai::Outcome::StartFailure); }
        else if (std::wstring(mode) == L"exit" || std::wstring(mode) == L"crashafter") { CHECK(r.outcome == ai::Outcome::Exited); }
        else if (std::wstring(mode) == L"timeout" || std::wstring(mode) == L"hang") { CHECK(r.outcome == ai::Outcome::Timeout); }
        else { CHECK(r.outcome == ai::Outcome::InvalidOutput); }
        if (std::wstring(mode) == L"hang") { CHECK(r.forced_cleanup); }
        CHECK(ai::apply_result(c, r).ok() && c.phase() == match::Phase::Finished);
        if (r.outcome == ai::Outcome::StartFailure) { CHECK(c.game().result() == state::GameResult{state::Aborted{}}); }
        else { CHECK(c.game().result() == state::GameResult{state::Won{state::Participant::A}}); }
        CHECK(c.game().history().size() == 3);
    }
    auto c = game(); auto missing = request(c, mock, L"missing");
    CHECK(ai::request_move(missing).outcome == ai::Outcome::StartFailure);
}
void stale()
{
    auto c = game(); ai::Result r{}; r.token = c.token(); r.actor = state::Participant::B;
    r.outcome = ai::Outcome::Move; r.position = {7, 7};
    CHECK(ai::apply_result(c, r).ok()); CHECK(c.end_reason() == match::EndReason::EngineIllegalMove);
    CHECK(c.restart(c.token()).ok()); const auto before = c;
    CHECK(ai::apply_result(c, r).error == match::Error::StaleInput && c == before);
    r.token = c.token(); r.outcome = ai::Outcome::Cancelled;
    CHECK(!ai::apply_result(c, r).ok() && c == before);
    r.outcome = ai::Outcome::Exited; r.actor = state::Participant::A;
    CHECK(ai::apply_result(c, r).error == match::Error::WrongParticipant && c == before);
    CHECK(c.close(c.token()).ok()); r.token = c.token(); r.actor = state::Participant::B;
    const auto closed = c; CHECK(!ai::apply_result(c, r).ok() && c == closed);
    match::Controller forbidden;
    CHECK(forbidden.start_compact("foul", "g8a1h7c1i8e1h9g1", state::Participant::A).ok());
    r.token = forbidden.token(); r.actor = state::Participant::A; r.outcome = ai::Outcome::Move; r.position = {7, 7};
    CHECK(ai::apply_result(forbidden, r).ok());
    CHECK(forbidden.end_reason() == match::EndReason::Forbidden && forbidden.game().history().size() == 8);
}
void worker(const std::filesystem::path& mock)
{
    auto c = game(); ai::EngineWorker worker;
    worker.submit(request(c, mock, L"timeout"));
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    CHECK(c.restart(c.token()).ok()); worker.cancel(); worker.submit(request(c, mock, L"normal"));
    std::optional<ai::Result> r;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!r && std::chrono::steady_clock::now() < deadline) {
        r = worker.take_result(); if (!r) std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    CHECK(r && r->token == c.token() && r->outcome == ai::Outcome::Move); reaped(*r);
    CHECK(ai::apply_result(c, *r).ok());
    worker.submit(request(game(), mock, L"timeout")); // destruction while queued/running
}
}
int wmain(int argc, wchar_t** argv)
{
    try {
        if (argc != 3) { return 2; }
        const std::wstring name = argv[1]; const std::filesystem::path mock = argv[2];
        if (name == L"protocol") protocol(); else if (name == L"normal") normal(mock);
        else if (name == L"errors") errors(mock); else if (name == L"stale") stale();
        else if (name == L"worker") worker(mock);
        else if (name == L"first_move") first_move(mock); else return 2;
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
