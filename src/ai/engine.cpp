#include "omok/ai/engine.hpp"
#include <windows.h>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <stdexcept>
#include <utility>
#include <vector>
namespace omok::ai {
namespace {
using Clock = std::chrono::steady_clock;
constexpr std::size_t log_limit = 256 * 1024;
struct Failure { Outcome outcome; std::string message; };
struct Handle {
    HANDLE value{};
    Handle() = default;
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    ~Handle() { reset(); }
    void reset(HANDLE next = nullptr) { if (value && value != INVALID_HANDLE_VALUE) { CloseHandle(value); } value = next; }
};
void check(BOOL ok, const char* step)
{
    if (!ok) { throw Failure{Outcome::StartFailure, std::string(step) + " Windows error=" + std::to_string(GetLastError())}; }
}
void log(Result& result, const std::string& text)
{
    if (result.transcript.size() + text.size() > log_limit) { throw Failure{Outcome::InvalidOutput, "engine transcript exceeded 256 KiB"}; }
    result.transcript += text;
}
class Process {
public:
    Handle process, job, input, output, errors;
    DWORD id{};
    ~Process() { stop(); }
    void open(const std::filesystem::path& executable)
    {
        SECURITY_ATTRIBUTES attributes{sizeof(attributes), nullptr, TRUE};
        Handle child_in, child_out, child_err;
        check(CreatePipe(&child_in.value, &input.value, &attributes, 65536), "stdin pipe");
        check(CreatePipe(&output.value, &child_out.value, &attributes, 65536), "stdout pipe");
        check(CreatePipe(&errors.value, &child_err.value, &attributes, 65536), "stderr pipe");
        check(SetHandleInformation(input.value, HANDLE_FLAG_INHERIT, 0), "stdin inheritance");
        check(SetHandleInformation(output.value, HANDLE_FLAG_INHERIT, 0), "stdout inheritance");
        check(SetHandleInformation(errors.value, HANDLE_FLAG_INHERIT, 0), "stderr inheritance");
        job.reset(CreateJobObjectW(nullptr, nullptr)); check(job.value != nullptr, "job");
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limit{};
        limit.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE | JOB_OBJECT_LIMIT_PROCESS_MEMORY;
        limit.ProcessMemoryLimit = 1024ULL * 1024 * 1024;
        check(SetInformationJobObject(job.value, JobObjectExtendedLimitInformation, &limit, sizeof(limit)), "job limits");
        SIZE_T bytes = 0;
        InitializeProcThreadAttributeList(nullptr, 1, 0, &bytes);
        std::vector<unsigned char> storage(bytes);
        auto* list = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage.data());
        check(InitializeProcThreadAttributeList(list, 1, 0, &bytes), "handle list");
        struct Guard { LPPROC_THREAD_ATTRIBUTE_LIST list; ~Guard() { DeleteProcThreadAttributeList(list); } } guard{list};
        HANDLE inherited[]{child_in.value, child_out.value, child_err.value};
        check(UpdateProcThreadAttribute(list, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherited, sizeof(inherited), nullptr, nullptr), "restricted handles");
        STARTUPINFOEXW startup{}; startup.StartupInfo.cb = sizeof(startup); startup.lpAttributeList = list;
        startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
        startup.StartupInfo.hStdInput = child_in.value; startup.StartupInfo.hStdOutput = child_out.value; startup.StartupInfo.hStdError = child_err.value;
        const auto full = std::filesystem::absolute(executable);
        auto command = L"\"" + full.wstring() + L"\"";
        PROCESS_INFORMATION info{};
        check(CreateProcessW(full.c_str(), command.data(), nullptr, nullptr, TRUE,
            CREATE_NO_WINDOW | CREATE_SUSPENDED | EXTENDED_STARTUPINFO_PRESENT, nullptr,
            full.parent_path().c_str(), &startup.StartupInfo, &info), "CreateProcess");
        process.reset(info.hProcess); Handle main_thread; main_thread.reset(info.hThread); id = info.dwProcessId;
        check(AssignProcessToJobObject(job.value, process.value), "assign job");
        DWORD_PTR allowed{}, system{};
        check(GetProcessAffinityMask(GetCurrentProcess(), &allowed, &system), "query CPU affinity");
        const auto one_core = allowed & (~allowed + 1);
        check(SetProcessAffinityMask(process.value, one_core), "one-core affinity");
        check(ResumeThread(main_thread.value) != static_cast<DWORD>(-1), "resume engine");
    }
    void send(std::string_view command)
    {
        DWORD written{};
        if (!WriteFile(input.value, command.data(), static_cast<DWORD>(command.size()), &written, nullptr) || written != command.size()) {
            throw Failure{Outcome::Exited, "engine stdin closed"};
        }
    }
    bool read(HANDLE pipe, std::string& buffer)
    {
        DWORD available{};
        if (!PeekNamedPipe(pipe, nullptr, 0, nullptr, &available, nullptr) || !available) { return false; }
        char chunk[4096]; DWORD count{};
        if (!ReadFile(pipe, chunk, std::min<DWORD>(available, sizeof(chunk)), &count, nullptr)) { return false; }
        buffer.append(chunk, count); return count != 0;
    }
    void stop() noexcept
    {
        if (!process.value || reaped) { return; }
        if (WaitForSingleObject(process.value, 0) == WAIT_TIMEOUT) {
            DWORD written{}; const char end[] = "END\r\n";
            if (input.value) { WriteFile(input.value, end, sizeof(end) - 1, &written, nullptr); }
            if (WaitForSingleObject(process.value, 1000) == WAIT_TIMEOUT) {
                forced = true;
                // Only the process/job created by this adapter is eligible for forced cleanup.
                TerminateProcess(process.value, 1);
            }
        }
        reaped = WaitForSingleObject(process.value, 1000) == WAIT_OBJECT_0;
        GetExitCodeProcess(process.value, &exit_code);
    }
    bool reaped{}, forced{}; DWORD exit_code{};
};
std::string next_line(Process& process, Result& result, std::string& buffer, Clock::time_point deadline,
    const std::function<bool()>& cancelled, Outcome timeout_outcome)
{
    for (;;) {
        if (cancelled && cancelled()) { throw Failure{Outcome::Cancelled, "request cancelled"}; }
        if (Clock::now() >= deadline) { throw Failure{timeout_outcome, "response deadline exceeded"}; }
        std::string stderr_text;
        if (process.read(process.errors.value, stderr_text)) { log(result, "! " + stderr_text); }
        const auto newline = buffer.find_first_of("\r\n");
        if (newline != std::string::npos) {
            if (newline > 16384) { throw Failure{Outcome::InvalidOutput, "engine line exceeded 16 KiB"}; }
            auto line = buffer.substr(0, newline); buffer.erase(0, newline + 1);
            log(result, "< " + line + '\n'); return line;
        }
        if (buffer.size() > 16384) { throw Failure{Outcome::InvalidOutput, "engine line exceeded 16 KiB"}; }
        if (process.read(process.output.value, buffer)) { continue; }
        if (WaitForSingleObject(process.process.value, 0) == WAIT_OBJECT_0) { throw Failure{Outcome::Exited, "engine exited before a complete response"}; }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
}
}
Result request_move(const Request& request, const std::function<bool()>& cancelled)
{
    Result result{}; result.token = request.token; result.actor = request.actor;
    const auto started = Clock::now(); Process process; bool initialized = false;
    try {
        const auto board = move_command(request.game, request.actor);
        if (cancelled && cancelled()) { throw Failure{Outcome::Cancelled, "superseded before start"}; }
        if (request.limits.turn_ms <= 0 || request.limits.remaining_ms <= 0 || request.limits.initialization_ms <= 0) {
            throw Failure{Outcome::Timeout, "no engine time remaining"};
        }
        process.open(request.executable); result.process_id = process.id;
        const auto send = [&](const std::string& text) { log(result, "> " + text); process.send(text); };
        std::string buffer;
        const auto initialization_deadline = started + std::chrono::milliseconds(std::min(request.limits.initialization_ms, request.limits.remaining_ms));
        const auto wait_for = [&](LineKind wanted, Clock::time_point deadline, Outcome timeout) {
            for (;;) {
                const auto text = next_line(process, result, buffer, deadline, cancelled, timeout);
                const auto parsed = parse_line(text);
                if (parsed.kind == LineKind::Empty || parsed.kind == LineKind::Diagnostic) { continue; }
                if (parsed.kind == LineKind::Error) { throw Failure{Outcome::StartFailure, text}; }
                if (parsed.kind != wanted) { throw Failure{initialized ? Outcome::InvalidOutput : Outcome::StartFailure, "unexpected engine response: " + text}; }
                if (wanted == LineKind::About) { result.about = text; }
                if (wanted == LineKind::Move) { result.position = parsed.position; }
                return;
            }
        };
        send("ABOUT\r\n"); wait_for(LineKind::About, initialization_deadline, Outcome::StartFailure);
        send("START 15\r\n"); wait_for(LineKind::Ok, initialization_deadline, Outcome::StartFailure);
        initialized = true;
        const int elapsed = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - started).count());
        const int remaining = request.limits.remaining_ms - elapsed;
        const int turn = std::min(request.limits.turn_ms, remaining);
        if (turn <= 0) { throw Failure{Outcome::Timeout, "engine match time exhausted during initialization"}; }
        const auto deadline = Clock::now() + std::chrono::milliseconds(turn);
        // Rapfi's supplied config reserves zero bytes outside TT. Advertise 512 MiB
        // to leave model/runtime headroom under the hard 1 GiB process limit.
        send("INFO rule 4\r\nINFO max_memory 536870912\r\nINFO thread_num 1\r\nINFO pondering 0\r\nINFO game_type 0\r\nABOUT\r\n");
        wait_for(LineKind::About, deadline, Outcome::Timeout); // serial settings fence
        const auto now = Clock::now();
        const auto available_turn = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count();
        const auto available_match = request.limits.remaining_ms - std::chrono::duration_cast<std::chrono::milliseconds>(now - started).count();
        if (available_turn <= 0 || available_match <= 0) { throw Failure{Outcome::Timeout, "time exhausted during engine settings"}; }
        const auto budget = std::min(available_turn, available_match);
        // NNUE files are loaded lazily on BEGIN/BOARD, outside Rapfi's own search timer.
        // Keep the manager deadline strict and reserve setup/IPC headroom instead.
        const auto search_budget = std::max<decltype(budget)>(1, budget - std::min<decltype(budget)>(1000, budget / 2));
        send("INFO timeout_turn " + std::to_string(search_budget)
            + "\r\nINFO timeout_match 180000\r\nINFO time_left " + std::to_string(available_match) + "\r\n");
        send(board); wait_for(LineKind::Move, deadline, Outcome::Timeout);
        result.outcome = Outcome::Move;
    } catch (const Failure& failure) {
        result.outcome = !initialized && failure.outcome == Outcome::Exited ? Outcome::StartFailure : failure.outcome;
        result.detail = failure.message;
    }
      catch (const std::exception& error) { result.outcome = Outcome::StartFailure; result.detail = error.what(); }
    process.stop();
    result.process_id = process.id; result.process_reaped = process.reaped;
    result.exit_code = process.exit_code; result.forced_cleanup = process.forced;
    if (result.outcome == Outcome::Move && process.exit_code != 0 && !process.forced) {
        result.outcome = Outcome::Exited; result.position.reset(); result.detail = "engine terminated abnormally after response";
    }
    result.elapsed_ms = std::chrono::duration<double, std::milli>(Clock::now() - started).count();
    if (!request.log_directory.empty()) {
        try {
            std::filesystem::create_directories(request.log_directory);
            result.log_path = request.log_directory / ("engine-" + std::to_string(request.token.session) + "-"
                + std::to_string(request.token.revision) + "-" + std::to_string(process.id) + "-"
                + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + ".log");
            std::ofstream file(result.log_path, std::ios::binary);
            file << result.transcript << "# outcome=" << static_cast<int>(result.outcome) << " pid=" << process.id
                << " elapsed_ms=" << result.elapsed_ms << " reaped=" << result.process_reaped << " forced=" << result.forced_cleanup
                << " exit=" << result.exit_code << " detail=" << result.detail << '\n';
            if (!file) { result.detail += " (log write failed)"; }
        } catch (...) { result.detail += " (log directory unavailable)"; }
    }
    return result;
}
EngineWorker::EngineWorker() : thread_([this] { run(); }) {}
EngineWorker::~EngineWorker()
{
    { std::lock_guard lock(mutex_); stopping_ = true; ++generation_; pending_.reset(); }
    ready_.notify_one(); thread_.join();
}
void EngineWorker::submit(Request request)
{
    { std::lock_guard lock(mutex_); ++generation_; pending_ = std::move(request); result_.reset(); }
    ready_.notify_one();
}
void EngineWorker::cancel()
{
    std::lock_guard lock(mutex_); ++generation_; pending_.reset(); result_.reset();
}
std::optional<Result> EngineWorker::take_result()
{
    std::lock_guard lock(mutex_); auto result = std::move(result_); result_.reset(); return result;
}
void EngineWorker::run()
{
    for (;;) {
        std::unique_lock lock(mutex_); ready_.wait(lock, [this] { return stopping_ || pending_; });
        if (stopping_) { return; }
        const auto request = std::move(*pending_); pending_.reset(); const auto generation = generation_.load(); lock.unlock();
        auto result = request_move(request, [this, generation] { return generation_.load() != generation; });
        lock.lock(); if (!stopping_ && generation_.load() == generation) { result_ = std::move(result); }
    }
}
}
