#include "omok/ai/engine.hpp"
#include <cstring>
#include <fstream>
#include <sstream>
#if defined(_MSC_VER)
#include <intrin.h>
#else
#include <cpuid.h>
#endif
namespace omok::ai {
namespace {
void cpuid(unsigned leaf, unsigned sub, unsigned (&v)[4])
{
#if defined(_MSC_VER)
    int raw[4]; __cpuidex(raw, static_cast<int>(leaf), static_cast<int>(sub));
    std::memcpy(v, raw, sizeof(v));
#else
    __cpuid_count(leaf, sub, v[0], v[1], v[2], v[3]);
#endif
}
unsigned long long xcr0()
{
#if defined(_MSC_VER)
    return _xgetbv(0);
#else
    unsigned low, high;
    __asm__ volatile("xgetbv" : "=a"(low), "=d"(high) : "c"(0));
    return (static_cast<unsigned long long>(high) << 32) | low;
#endif
}
}
CpuFeatures cpu_features()
{
    CpuFeatures f;
    unsigned v[4]{}; cpuid(0, 0, v); const auto maximum = v[0];
    cpuid(1, 0, v);
    f.sse41 = (v[2] & (1U << 19)) && (v[2] & (1U << 9)) && (v[3] & (1U << 26));
    const bool avx = (v[2] & (1U << 28)) && (v[2] & (1U << 27)) && ((xcr0() & 6) == 6);
    f.fma = avx && (v[2] & (1U << 12));
    if (maximum >= 7) {
        cpuid(7, 0, v); const auto subleaves = v[0];
        f.avx2 = avx && (v[1] & (1U << 5)); f.bmi2 = (v[1] & (1U << 8)) != 0;
        f.avx512 = avx && ((xcr0() & 0xe6) == 0xe6) && (v[1] & (1U << 16));
        if (subleaves >= 1) { cpuid(7, 1, v); f.avxvnni = avx && (v[0] & (1U << 4)); }
    }
    cpuid(0x80000000, 0, v);
    if (v[0] >= 0x80000004) {
        char brand[49]{};
        for (unsigned n = 0; n < 3; ++n) { cpuid(0x80000002 + n, 0, v); std::memcpy(brand + n * 16, v, 16); }
        f.brand = brand;
    }
    return f;
}
std::filesystem::path select_rapfi(const std::filesystem::path& directory)
{
    const auto f = cpu_features();
    if (!f.sse41) { return {}; }
    const auto name = f.avx2 && f.fma && f.bmi2 ? L"pbrain-rapfi-windows-avx2.exe" : L"pbrain-rapfi-windows-sse.exe";
    const auto path = directory / name;
    return std::filesystem::is_regular_file(path) ? std::filesystem::absolute(path) : std::filesystem::path{};
}
std::string validate_rapfi_installation(const std::filesystem::path& executable)
{
    if (!std::filesystem::is_regular_file(executable)) { return "engine executable is missing"; }
    const auto f = cpu_features(); const auto name = executable.filename().wstring();
    if (name.find(L"avx512") != std::wstring::npos || name.find(L"avxvnni") != std::wstring::npos) {
        return "select the verified AVX2 or SSE Windows executable";
    }
    if (!f.sse41 || (name.find(L"avx2") != std::wstring::npos && (!f.avx2 || !f.fma || !f.bmi2))) {
        return "CPU/OS does not support the executable instruction set";
    }
    const auto dir = executable.parent_path();
    for (const auto file : {L"config.toml", L"model210901.bin", L"mix9svqrenju_bs15_black.bin.lz4", L"mix9svqrenju_bs15_white.bin.lz4"}) {
        if (!std::filesystem::is_regular_file(dir / file)) { return "required Rapfi config/model file is missing"; }
    }
    std::ifstream input(dir / "config.toml"); std::string line; bool coordinates = false;
    while (std::getline(input, line)) {
        line = line.substr(0, line.find('#'));
        const auto equal = line.find('='); if (equal == std::string::npos) { continue; }
        std::string key, value; std::istringstream(line.substr(0, equal)) >> key;
        std::istringstream(line.substr(equal + 1)) >> value;
        if (key == "coord_conversion_mode") { coordinates = value == "\"none\""; }
    }
    return coordinates ? "" : "config requires coord_conversion_mode = none";
}
}
