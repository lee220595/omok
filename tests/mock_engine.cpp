// Fixed-response protocol fixture, not a playing AI.
#include <windows.h>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
int wmain(int argc, wchar_t** argv)
{
    if (argc < 1) { return 2; }
    const auto mode = std::filesystem::path(argv[0]).stem().wstring();
    std::string line;
    while (std::getline(std::cin, line)) {
        if (!line.empty() && line.back() == '\r') { line.pop_back(); }
        if (line == "ABOUT") {
            if (mode.find(L"bootexit") != std::wstring::npos) { return 17; }
            std::cout << "name=\"Mock\", version=\"fixture\"" << std::endl;
        }
        else if (line == "START 15") { std::cout << (mode.find(L"startfail") != std::wstring::npos ? "ERROR unsupported" : "OK") << std::endl; }
        else if (line == "DONE") {
            if (mode.find(L"crashafter") != std::wstring::npos) { std::cout << "8,8" << std::endl; return 17; }
            if (mode.find(L"exit") != std::wstring::npos) { return 17; }
            if (mode.find(L"hang") != std::wstring::npos) { std::this_thread::sleep_for(std::chrono::seconds(30)); }
            else if (mode.find(L"timeout") != std::wstring::npos) { continue; } // accepts END
            else if (mode.find(L"bad") != std::wstring::npos) { std::cout << "8,8junk" << std::endl; }
            else if (mode.find(L"long") != std::wstring::npos) { std::cout << std::string(20000, 'x') << std::endl; }
            else {
                std::cerr << "fixture stderr\n";
                std::cout << "MESSAGE diagnostic\nDEBUG trace\n8" << std::flush;
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                std::cout << ",8\r" << std::flush;
            }
        }
        else if (line == "END") { return 0; }
    }
    return 0;
}
