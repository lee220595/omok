#include "omok/ai/engine.hpp"
#include <iostream>
int wmain(int argc, wchar_t** argv)
{
    const auto cpu = omok::ai::cpu_features();
    std::cout << "CPU=" << cpu.brand << " SSE41=" << cpu.sse41 << " AVX2=" << cpu.avx2 << " FMA=" << cpu.fma
        << " BMI2=" << cpu.bmi2 << " AVX512=" << cpu.avx512 << " AVXVNNI=" << cpu.avxvnni << '\n';
    if (argc < 2) { return 0; }
    const std::filesystem::path exe = std::filesystem::absolute(argv[1]);
    const auto error = omok::ai::validate_rapfi_installation(exe);
    if (!error.empty()) { std::cerr << error << '\n'; return 1; }
    const int turn = argc >= 3 ? std::stoi(argv[2]) : 1000;
    for (int n = 0; n < 5; ++n) {
        omok::match::Controller c;
        const auto owner = (n == 2 || n == 4) ? omok::state::Participant::B : omok::state::Participant::A;
        if (n >= 3) {
            if (!c.start_direct(owner).ok()) return 1;
            if (n == 4 && !c.place(c.token(), owner, {7, 7}).ok()) return 1;
        } else if (!c.start_compact("dev-basic-v1", "h8i8g9", owner).ok()) return 1;
        if (n == 1 && !c.place(c.token(), *c.game().next_participant(), {8, 8}).ok()) return 1;
        const auto actor = *c.game().next_participant();
        const auto before = c;
        const auto result = omok::ai::request_move({c.token(), c.game(), actor, exe,
            std::filesystem::absolute(L"build/rapfi-logs"), {turn, 180000, 10000}});
        std::cout << result.about << "\ncase=" << n << " outcome=" << static_cast<int>(result.outcome)
            << " elapsed_ms=" << result.elapsed_ms << " pid=" << result.process_id << " reaped=" << result.process_reaped
            << " forced=" << result.forced_cleanup << " detail=" << result.detail;
        if (result.position) std::cout << " row=" << result.position->row << " column=" << result.position->column;
        std::cout << '\n';
        if (!(c == before) || result.outcome != omok::ai::Outcome::Move || !result.process_reaped
            || !omok::ai::apply_result(c, result).ok() || c.game().history().size() != before.game().history().size() + 1) return 1;
    }
    return 0;
}
