#pragma once
#include "omok/match/controller.hpp"
#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <functional>
#include <mutex>
#include <thread>

namespace omok::ai {
struct CpuFeatures {
    std::string brand;
    bool sse41{}, avx2{}, fma{}, bmi2{}, avx512{}, avxvnni{};
};
[[nodiscard]] CpuFeatures cpu_features();
// Conservative AVX2 selection; no AVX512/VNNI executable is selected automatically.
[[nodiscard]] std::filesystem::path select_rapfi(const std::filesystem::path& directory);
[[nodiscard]] std::string validate_rapfi_installation(const std::filesystem::path& executable);
enum class LineKind { Empty, Diagnostic, Ok, About, Move, Error, Invalid };
struct Line { LineKind kind; std::optional<state::Coordinate> position; };
[[nodiscard]] Line parse_line(std::string_view text);
[[nodiscard]] std::string board_command(const state::GameState& game, state::Participant actor);
// BEGIN for the first black move, ordered BOARD for every later snapshot.
[[nodiscard]] std::string move_command(const state::GameState& game, state::Participant actor);
struct Limits { int turn_ms{30000}; int remaining_ms{180000}; int initialization_ms{10000}; };
struct Request {
    match::Token token;
    state::GameState game;
    state::Participant actor;
    std::filesystem::path executable;
    std::filesystem::path log_directory;
    Limits limits;
};
enum class Outcome { Move, StartFailure, Timeout, InvalidOutput, Exited, Cancelled };
struct Result {
    match::Token token;
    state::Participant actor;
    Outcome outcome{Outcome::StartFailure};
    std::optional<state::Coordinate> position;
    std::string about, detail, transcript;
    std::filesystem::path log_path;
    double elapsed_ms{};
    unsigned long process_id{}, exit_code{};
    bool process_reaped{}, forced_cleanup{};
};
// Fresh process per request: START + BEGIN/ordered BOARD, then END and reap.
// Initial integration trades TT reuse for isolation and full-state restoration.
[[nodiscard]] Result request_move(const Request& request, const std::function<bool()>& cancelled = {});
[[nodiscard]] match::Response apply_result(match::Controller& controller, const Result& result);
class EngineWorker {
public:
    EngineWorker();
    ~EngineWorker();
    void submit(Request request);
    void cancel();
    [[nodiscard]] std::optional<Result> take_result();
private:
    void run();
    std::mutex mutex_;
    std::condition_variable ready_;
    std::optional<Request> pending_;
    std::optional<Result> result_;
    std::atomic<std::uint64_t> generation_{};
    bool stopping_{};
    std::thread thread_;
};
}
