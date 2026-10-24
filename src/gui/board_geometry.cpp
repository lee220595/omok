#include "board_geometry.hpp"
#include <algorithm>
#include <cmath>

namespace omok::gui {
Pixel BoardGeometry::point(state::Coordinate p) const noexcept
{
    return {left + p.column * step, top + p.row * step};
}
std::optional<state::Coordinate> BoardGeometry::hit(Pixel p) const noexcept
{
    if (!valid() || p.x < left || p.y < top || static_cast<double>(p.x) > left + 14.0 * step
        || static_cast<double>(p.y) > top + 14.0 * step) { return {}; }
    const state::Coordinate c{static_cast<int>(std::lround((static_cast<double>(p.y) - top) / step)),
        static_cast<int>(std::lround((static_cast<double>(p.x) - left) / step))};
    const auto center = point(c);
    const auto dx = static_cast<double>(p.x) - center.x;
    const auto dy = static_cast<double>(p.y) - center.y;
    if (!c.is_on_board() || dx * dx + dy * dy > static_cast<double>(step) * step * 0.35 * 0.35) { return {}; }
    return c;
}
BoardGeometry board_geometry(int width, int height, unsigned dpi) noexcept
{
    if (dpi == 0 || dpi > 960 || width <= 0 || height <= 0) { return {}; }
    const double scale = dpi / 96.0;
    const int top = static_cast<int>(240 * scale);
    const int available_w = std::max(0, width - static_cast<int>(360 * scale));
    const int available_h = std::max(0, height - top - static_cast<int>(40 * scale));
    const int step = std::min(available_w, available_h) / 14;
    if (step < 8) { return {}; }
    return {static_cast<int>(40 * scale) + (available_w - 14 * step) / 2,
        top + (available_h - 14 * step) / 2, step};
}
}
