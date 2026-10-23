#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <variant>

namespace omok::state {

inline constexpr int board_size = 15;
inline constexpr std::size_t board_area = 225;

// Origin is the top-left intersection: row grows down, column grows right.
// Raw input may be invalid; no indexing is permitted until it is checked.
struct Coordinate {
    int row{};
    int column{};

    [[nodiscard]] constexpr bool is_on_board() const noexcept
    {
        return row >= 0 && row < board_size && column >= 0 && column < board_size;
    }
    [[nodiscard]] constexpr std::optional<std::size_t> index() const noexcept
    {
        if (!is_on_board()) {
            return std::nullopt;
        }
        return static_cast<std::size_t>(row * board_size + column);
    }
    [[nodiscard]] static constexpr std::optional<Coordinate> from_index(std::size_t index) noexcept
    {
        if (index >= board_area) {
            return std::nullopt;
        }
        return Coordinate{static_cast<int>(index / board_size), static_cast<int>(index % board_size)};
    }
    bool operator==(const Coordinate&) const = default;
};

enum class StoneColor { Black, White };
enum class Cell { Empty, Black, White };
enum class Participant { A, B };
// A semantic view only: these enumerators are NOT Gomocup wire values.
enum class RelativeStone { Own, Opponent };

class ColorAssignment {
public:
    // Unassigned until an opening/controller explicitly supplies an owner.
    ColorAssignment() = default;
    [[nodiscard]] std::optional<Participant> owner_of(StoneColor color) const noexcept;
    [[nodiscard]] std::optional<StoneColor> color_of(Participant participant) const noexcept;
    [[nodiscard]] std::optional<RelativeStone> relative_to(
        StoneColor stone, Participant observer) const noexcept;
    bool operator==(const ColorAssignment&) const = default;

private:
    friend class GameState;
    std::optional<Participant> black_owner_;
};

struct InProgress { bool operator==(const InProgress&) const = default; };
struct Won {
    Participant winner;
    bool operator==(const Won&) const = default;
};
struct Draw { bool operator==(const Draw&) const = default; };
struct Aborted { bool operator==(const Aborted&) const = default; };
using GameResult = std::variant<InProgress, Won, Draw, Aborted>;

struct Placement {
    Coordinate position;
    StoneColor color;
    // Actual actor, not the eventual owner. nullopt = externally prepared stone.
    std::optional<Participant> placed_by;
    bool operator==(const Placement&) const = default;
};

struct InitialState {
    std::optional<Participant> black_owner;
    StoneColor next_color{StoneColor::Black};
};

enum class StateError {
    None,
    OutOfBounds,
    Occupied,
    InvalidColor,
    InvalidParticipant,
    UnexpectedColor,
    AlreadyFinished,
    InvalidResult
};

class GameState {
public:
    GameState() = default;

    [[nodiscard]] std::optional<Cell> cell_at(Coordinate position) const noexcept;
    [[nodiscard]] std::span<const Cell, board_area> board() const noexcept { return board_; }
    // Index + 1 is the placement number. Read-only view, not a snapshot; copy to retain it.
    [[nodiscard]] std::span<const Placement> history() const noexcept
    {
        return {history_.data(), history_size_};
    }
    [[nodiscard]] const ColorAssignment& colors() const noexcept { return colors_; }
    [[nodiscard]] std::optional<StoneColor> next_color() const noexcept { return next_color_; }
    [[nodiscard]] std::optional<Participant> next_participant() const noexcept;
    [[nodiscard]] const GameResult& result() const noexcept { return result_; }

    // Storage transaction, NOT move authorization or rule adjudication.
    // Controller supplies the next color; there is no automatic alternation.
    [[nodiscard]] StateError commit_placement(Placement placement, StoneColor next_color) noexcept;
    [[nodiscard]] StateError assign_colors(Participant black_owner) noexcept;
    // Records an externally adjudicated terminal result; never detects wins/draws.
    [[nodiscard]] StateError finish(GameResult result) noexcept;
    // Explicit full reset. Invalid configuration preserves the entire old state.
    [[nodiscard]] StateError reset(InitialState initial = {}) noexcept;

    bool operator==(const GameState&) const = default;

private:
    std::array<Cell, board_area> board_{};
    std::array<Placement, board_area> history_{};
    std::size_t history_size_{};
    ColorAssignment colors_;
    std::optional<StoneColor> next_color_{StoneColor::Black};
    GameResult result_{InProgress{}};
};

} // namespace omok::state
