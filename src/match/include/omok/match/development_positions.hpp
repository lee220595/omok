#pragma once
#include <string>
namespace omok::match {
// Self-authored development fixtures, never official tournament openings.
enum class DevelopmentPosition { Basic, Win, Forbidden, Draw, Finished };
struct DevelopmentData { std::string id; std::string compact; };
[[nodiscard]] DevelopmentData development_data(DevelopmentPosition position);
}
