find_package(Threads REQUIRED)
add_library(omok_ai STATIC protocol.cpp engine.cpp cpu.cpp)
target_include_directories(omok_ai PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(omok_ai PUBLIC omok_match Threads::Threads)
target_compile_features(omok_ai PUBLIC cxx_std_20)
target_compile_definitions(omok_ai PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN
    WINVER=0x0A00 _WIN32_WINNT=0x0A00)
omok_compiler_options(omok_ai)
