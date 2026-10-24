#include "omok/ai/engine.hpp"
#include <charconv>
#include <stdexcept>
namespace omok::ai {
Line parse_line(std::string_view text)
{
    const auto begin = text.find_first_not_of(" \t\r\n");
    if (begin == std::string_view::npos) { return {LineKind::Empty, {}}; }
    text = text.substr(begin, text.find_last_not_of(" \t\r\n") - begin + 1);
    const auto prefix = [&](std::string_view key) { return text == key || (text.starts_with(key) && text.size() > key.size() && text[key.size()] == ' '); };
    if (prefix("MESSAGE") || prefix("DEBUG")) { return {LineKind::Diagnostic, {}}; }
    if (prefix("OK")) { return {LineKind::Ok, {}}; }
    if (prefix("ERROR") || prefix("UNKNOWN")) { return {LineKind::Error, {}}; }
    if (text.starts_with("name=")) { return {LineKind::About, {}}; }
    const auto comma = text.find(',');
    if (comma == std::string_view::npos) { return {LineKind::Invalid, {}}; }
    int x{}, y{};
    const auto a = std::from_chars(text.data(), text.data() + comma, x);
    const auto b = std::from_chars(text.data() + comma + 1, text.data() + text.size(), y);
    if (a.ec != std::errc{} || a.ptr != text.data() + comma || b.ec != std::errc{} || b.ptr != text.data() + text.size()) {
        return {LineKind::Invalid, {}};
    }
    return {LineKind::Move, state::Coordinate{y, x}};
}
std::string board_command(const state::GameState& game, state::Participant actor)
{
    if (!game.next_participant() || game.next_participant() != actor || !game.colors().color_of(actor)) {
        throw std::invalid_argument("BOARD requires the assigned next participant");
    }
    std::string command = "BOARD\r\n";
    for (const auto& placement : game.history()) {
        const auto relative = game.colors().relative_to(placement.color, actor);
        if (!relative) { throw std::invalid_argument("unassigned BOARD color"); }
        command += std::to_string(placement.position.column) + ',' + std::to_string(placement.position.row) + ','
            + (*relative == state::RelativeStone::Own ? '1' : '2') + "\r\n";
    }
    return command + "DONE\r\n";
}
std::string move_command(const state::GameState& game, state::Participant actor)
{
    const auto board = board_command(game, actor); // validate actor/color even on an empty board
    return game.history().empty() ? "BEGIN\r\n" : board;
}
match::Response apply_result(match::Controller& controller, const Result& result)
{
    if (result.token != controller.token() || result.outcome == Outcome::Cancelled) { return {match::Error::StaleInput}; }
    using Failure = match::EngineFailure;
    if (result.outcome == Outcome::Move && result.position) {
        const auto response = controller.place(result.token, result.actor, *result.position);
        if (response.error != match::Error::InvalidPlacement) { return response; }
        return controller.engine_failure(result.token, result.actor, Failure::IllegalMove);
    }
    const auto failure = result.outcome == Outcome::StartFailure ? Failure::Start
        : result.outcome == Outcome::Timeout ? Failure::Timeout
        : result.outcome == Outcome::Exited ? Failure::Exited : Failure::BadOutput;
    return controller.engine_failure(result.token, result.actor, failure);
}
}
