add_library(omok_gui STATIC main_window.cpp)
target_include_directories(omok_gui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(omok_gui PRIVATE user32 gdi32)
