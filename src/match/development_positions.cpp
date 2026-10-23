#include "omok/match/development_positions.hpp"
#include "omok/state/game_state.hpp"
#include <vector>

namespace omok::match {
DevelopmentData development_data(DevelopmentPosition position)
{
    switch (position) {
    case DevelopmentPosition::Basic: return {"dev-basic-v1", "h8i8g9"};
    case DevelopmentPosition::Win: return {"dev-win-v1", "a8a1b8c1c8e1d8"};
    case DevelopmentPosition::Forbidden: return {"dev-forbidden-v1", "g8a1h7c1i8e1h9g1"};
    case DevelopmentPosition::Finished: return {"dev-finished-v1", "a8a1b8c1c8e1d8g1e8"};
    case DevelopmentPosition::Draw: break;
    }
    std::vector<state::Coordinate> black, white;
    for (int r = 0; r < 15; ++r) {
        for (int c = 0; c < 15; ++c) { (((c / 2 + r) % 2 == 0) ? black : white).push_back({r, c}); }
    }
    std::string text;
    for (std::size_t i = 0; i < 199; ++i) {
        const auto p = (i % 2 == 0 ? black : white)[i / 2];
        text += static_cast<char>('a' + p.column);
        text += std::to_string(p.row + 1);
    }
    return {"dev-draw-199-v1", std::move(text)};
}
}
