# Reserved boundary: external processes and Gomocup transport, not AI search.
add_library(omok_ai INTERFACE)
target_link_libraries(omok_ai INTERFACE omok_state)
