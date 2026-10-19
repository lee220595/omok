add_library(omok_state STATIC game_state.cpp)
target_include_directories(omok_state PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(omok_state PUBLIC cxx_std_20)
