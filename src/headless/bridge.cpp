#include "omok/match/controller.hpp"
#include <algorithm>
#include <cstdint>
#include <memory>

// ABI v1: opaque owned Controller; no C++ allocations or exceptions cross the ABI.
// Calls on a handle must be serialized. Buffers belong to the caller.
#define API extern "C" __declspec(dllexport)
using omok::match::Controller;
namespace s = omok::state;
API int omok_abi() noexcept { return 1; }
API void* omok_create() noexcept { try { return new Controller; } catch (...) { return nullptr; } }
API void omok_destroy(void* p) noexcept { delete static_cast<Controller*>(p); }
API void* omok_clone(void* p) noexcept {
    try { return p ? new Controller(*static_cast<Controller*>(p)) : nullptr; } catch (...) { return nullptr; }
}
API int omok_reset(void* p, const char* id, const char* compact, int owner) noexcept {
    if (!p || !id || !compact || owner < 0 || owner > 1) return -1;
    try {
        auto& c = *static_cast<Controller*>(p);
        auto r = *compact ? c.start_compact(id, compact, static_cast<s::Participant>(owner))
                          : c.start_direct(static_cast<s::Participant>(owner));
        return static_cast<int>(r.error);
    } catch (...) { return -2; }
}
API int omok_step(void* p, std::uint64_t session, std::uint64_t revision,
                  int actor, int action, int* decision) noexcept {
    if (!p || !decision || action < 0 || action >= 225 || actor < 0 || actor > 1) return -1;
    try {
        auto r = static_cast<Controller*>(p)->place({session, revision}, static_cast<s::Participant>(actor),
                                                  {action / 15, action % 15});
        decision[0] = static_cast<int>(r.decision.verdict);
        decision[1] = static_cast<int>(r.decision.reason);
        return static_cast<int>(r.error);
    } catch (...) { return -2; }
}
API int omok_close(void* p, std::uint64_t session, std::uint64_t revision) noexcept {
    if (!p) return -1;
    try { return static_cast<int>(static_cast<Controller*>(p)->close({session, revision}).error); }
    catch (...) { return -2; }
}
// snapshot: 16 metadata ints, 225 cells, then n*(action,color,actor) ints.
API int omok_snapshot(void* p, int* out, int capacity, std::uint64_t* token) noexcept {
    if (!p || !out || !token || capacity < 916) return -1;
    const auto& c = *static_cast<Controller*>(p); const auto& g = c.game();
    std::fill(out, out + capacity, 0);
    out[0] = static_cast<int>(c.phase()); out[1] = static_cast<int>(g.history().size());
    out[2] = g.next_color() ? static_cast<int>(*g.next_color()) : -1;
    out[3] = g.next_participant() ? static_cast<int>(*g.next_participant()) : -1;
    auto owner = g.colors().owner_of(s::StoneColor::Black);
    out[4] = owner ? static_cast<int>(*owner) : -1;
    out[5] = static_cast<int>(g.result().index()); out[6] = -1;
    if (auto w = std::get_if<s::Won>(&g.result())) out[6] = static_cast<int>(w->winner);
    out[7] = static_cast<int>(c.end_reason()); out[8] = static_cast<int>(c.rule_reason());
    out[9] = static_cast<int>(omok::rules::tournament_move_limit);
    out[10] = static_cast<int>(c.source_opening().game().history().size());
    out[11] = static_cast<int>(c.source_opening().mode());
    out[12] = static_cast<int>(c.events().size());
    for (int i = 0; i < 225; ++i) out[16+i] = static_cast<int>(g.board()[static_cast<std::size_t>(i)]);
    int i = 241;
    for (const auto& m : g.history()) {
        out[i++] = static_cast<int>(*m.position.index()); out[i++] = static_cast<int>(m.color);
        out[i++] = m.placed_by ? static_cast<int>(*m.placed_by) : -1;
    }
    token[0] = c.token().session; token[1] = c.token().revision;
    return 0;
}
API int omok_mask(void* p, unsigned char* out, int capacity) noexcept {
    if (!p || !out || capacity != 225) return -1;
    try {
        const auto& c = *static_cast<Controller*>(p); const auto& g = c.game();
        std::fill(out, out + capacity, 0);
        if (c.phase() != omok::match::Phase::Playing) return 0;
        for (int i = 0; i < 225; ++i) {
            auto d = omok::rules::evaluate(g, {{i/15,i%15}, *g.next_color()});
            out[i] = d.verdict == omok::rules::Verdict::Legal || d.verdict == omok::rules::Verdict::Win
                     || d.verdict == omok::rules::Verdict::Draw;
        }
        return 0;
    } catch (...) { return -2; }
}
