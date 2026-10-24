#pragma once

#include "omok/rules/renju.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace omok::opening {

struct Move {
    std::size_t number; // 1-based, including externally prepared stones
    state::Coordinate position;
    state::StoneColor color;
    bool operator==(const Move&) const = default;
};
struct PreparedData {
    std::string id; // caller's source/version identifier, not proof of official provenance
    std::vector<Move> moves;
    bool operator==(const PreparedData&) const = default;
};
enum class Error {
    None, MissingId, MalformedData, TooLong, InvalidParticipant,
    WrongNumber, WrongColor, InvalidPlacement, ForbiddenPlacement, AfterTerminal,
    WrongStage, WrongActor, WrongCount, InvalidChoice, StorageFailure
};
struct Status {
    Error error{Error::None};
    std::size_t move_number{}; // offending 1-based move, 0 for non-placement errors
    rules::Reason reason{rules::Reason::None};
    [[nodiscard]] bool ok() const noexcept { return error == Error::None; }
    bool operator==(const Status&) const = default;
};

// GomocupJudge client compact position, e.g. h8i8g9. Letter = x/column,
// number = y/row + 1. Strict 15x15 subset; NOT a SWAP2BOARD string parser.
// Leaves output entirely unchanged on failure; does not adjudicate the game.
[[nodiscard]] Status load_compact(std::string_view id, std::string_view text, PreparedData& output);

enum class Mode { None, PreparedRenju, Swap2Protocol, DirectRenju };
enum class Stage { Idle, FirstThree, SecondChoice, FinalChoice, Complete, Finished };
// TakeWhite includes the next white placement (4th or 6th), per SWAP2BOARD.
enum class Choice { TakeBlack, TakeWhite, OfferTwo };
enum class Continuation { OpeningInput, RenjuReady, RuleContractRequired, Terminal, NotStarted };
enum class EventType { Started, BatchPlaced, ChoiceMade, ColorsAssigned, Completed };
struct Event {
    EventType type;
    std::optional<state::Participant> actor;
    std::optional<Choice> choice;
    std::optional<state::Participant> black_owner;
    std::size_t total_placements;
    bool operator==(const Event&) const = default;
};
struct Handoff {
    Mode mode;
    Stage stage;
    Continuation continuation;
    std::size_t total_placements;
    std::optional<state::Participant> input_actor; // opening actor, distinct from stone owner
    std::optional<state::StoneColor> next_color;
    std::optional<state::Participant> next_participant;
    [[nodiscard]] bool can_enter_main_game() const noexcept
    {
        return continuation == Continuation::RenjuReady;
    }
    bool operator==(const Handoff&) const = default;
};

class Session {
public:
    [[nodiscard]] const state::GameState& game() const noexcept { return game_; }
    [[nodiscard]] Mode mode() const noexcept { return mode_; }
    [[nodiscard]] Stage stage() const noexcept { return stage_; }
    [[nodiscard]] const std::string& data_id() const noexcept { return data_id_; }
    [[nodiscard]] std::span<const Event> events() const noexcept { return events_; }
    [[nodiscard]] Handoff handoff() const noexcept;

    // Explicit new-opening initialization. Success replaces the entire old session;
    // failure (including allocation exceptions) preserves it. Does not append games.
    [[nodiscard]] Status restore_prepared(const PreparedData& data, state::Participant black_owner);
    // Local empty-board start with Renju board adjudication; not a tournament opening.
    [[nodiscard]] Status begin_direct_renju(state::Participant black_owner);
    void begin_swap2(); // fresh structural protocol session, first proposer A

    // All responses are atomic batches. Sequence numbers/colors are explicit inputs.
    [[nodiscard]] Status place_first_three(state::Participant actor, std::span<const Move> moves);
    [[nodiscard]] Status choose(state::Participant actor, Choice choice, std::span<const Move> moves = {});

    bool operator==(const Session&) const = default;

private:
    [[nodiscard]] Status append_structural(state::Participant actor, std::span<const Move> moves);
    void record(EventType type, std::optional<state::Participant> actor = {},
        std::optional<Choice> choice = {}, std::optional<state::Participant> black_owner = {});
    state::GameState game_;
    Mode mode_{Mode::None};
    Stage stage_{Stage::Idle};
    std::string data_id_;
    std::vector<Event> events_;
};

} // namespace omok::opening
