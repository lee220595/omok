add_executable(omok_state_tests state_tests.cpp)
target_link_libraries(omok_state_tests PRIVATE omok_state)
omok_compiler_options(omok_state_tests)
if(MINGW)
    target_link_options(omok_state_tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()

foreach(case IN ITEMS initial_state coordinates boundaries rejected_requests color_assignment
        relative_stones recoloring history explicit_turn terminal_results rejected_results reset no_adjudication)
    add_test(NAME state.${case} COMMAND omok_state_tests ${case})
    set_tests_properties(state.${case} PROPERTIES TIMEOUT 10 LABELS state)
endforeach()
