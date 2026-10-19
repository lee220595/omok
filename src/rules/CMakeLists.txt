# Reserved boundary: legality, renju adjudication, wins, draws.
add_library(omok_rules INTERFACE)
target_link_libraries(omok_rules INTERFACE omok_state)
