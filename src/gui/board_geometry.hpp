#pragma once
#include "omok/state/game_state.hpp"
namespace omok::gui {
struct Pixel { int x; int y; bool operator==(const Pixel&) const = default; };
struct BoardGeometry {
    int left{};
    int top{};
    int step{};
    [[nodiscard]] bool valid() const noexcept { return step >= 8; }
    [[nodiscard]] Pixel point(state::Coordinate p) const noexcept;
    [[nodiscard]] std::optional<state::Coordinate> hit(Pixel pixel) const noexcept;
};
[[nodiscard]] BoardGeometry board_geometry(int width, int height, unsigned dpi) noexcept;
}
