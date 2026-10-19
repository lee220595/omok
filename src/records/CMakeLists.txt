# Reserved boundary: persistence and post-game replay.
add_library(omok_records INTERFACE)
target_link_libraries(omok_records INTERFACE omok_state)
