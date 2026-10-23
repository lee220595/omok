#pragma once
#include "omok/opening/opening.hpp"
#include <cstdint>

namespace omok::match {
struct Token {
    std::uint64_t session{};
    std::uint64_t revision{};
    bool operator==(const Token&) const = default;
};
enum class Phase { NotStarted, Playing, Finished, Closed };
enum class EndReason { None, Five, MoveLimit, BoardFull, Forbidden, Resigned, Closed, OpeningResult };
enum class Error { None, NotReady, EmptyOpening, InvalidOpening, StaleInput, NotPlaying,
    InvalidParticipant, WrongParticipant, InvalidPlacement, StorageFailure, VersionExhausted };
struct Response {
    Error error{Error::None};
    rules::Decision decision{};
    opening::Status opening_status{};
    [[nodiscard]] bool ok() const noexcept { return error == Error::None; }
};
enum class EventKind { Started, Restarted, Placement, Ended };
struct Event {
    EventKind kind;
    Token token;
    std::optional<state::Participant> actor;
    std::optional<state::Coordinate> position;
    rules::Decision decision;
    EndReason ending;
    bool operator==(const Event&) const = default;
};
class Controller {
public:
    [[nodiscard]] const state::GameState& game() const noexcept { return game_; }
    [[nodiscard]] const opening::Session& source_opening() const noexcept { return opening_; }
    [[nodiscard]] Token token() const noexcept { return token_; }
    [[nodiscard]] Phase phase() const noexcept { return phase_; }
    [[nodiscard]] EndReason end_reason() const noexcept { return end_reason_; }
    [[nodiscard]] rules::Reason rule_reason() const noexcept { return rule_reason_; }
    [[nodiscard]] std::span<const Event> events() const noexcept { return events_; }
    [[nodiscard]] Response start(const opening::Session& source);
    [[nodiscard]] Response start_compact(std::string_view id, std::string_view text, state::Participant black_owner);
    [[nodiscard]] Response restart(Token expected);
    [[nodiscard]] Response place(Token expected, state::Participant actor, state::Coordinate position);
    [[nodiscard]] Response resign(Token expected, state::Participant actor);
    [[nodiscard]] Response close(Token expected);
    bool operator==(const Controller&) const = default;
private:
    [[nodiscard]] Error input_error(Token expected) const noexcept;
    opening::Session opening_;
    state::GameState game_;
    Token token_;
    Phase phase_{Phase::NotStarted};
    EndReason end_reason_{EndReason::None};
    rules::Reason rule_reason_{rules::Reason::None};
    std::vector<Event> events_; // current session only; file/archive support is separate
};
} // namespace omok::match
