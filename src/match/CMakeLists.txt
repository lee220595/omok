# Reserved boundary: input requests, clocks, completion, cancellation.
add_library(omok_match INTERFACE)
target_link_libraries(omok_match INTERFACE omok_state omok_rules omok_opening)
