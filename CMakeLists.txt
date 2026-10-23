cmake_minimum_required(VERSION 3.24)
project(Omok VERSION 0.1.0 LANGUAGES CXX RC)

if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Omok requires a 64-bit Windows toolchain.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
include(CTest)

function(omok_compiler_options target)
    if(MSVC)
        target_compile_options(${target} PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:/W4;/permissive-;/utf-8>")
        set_property(TARGET ${target} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    elseif(MINGW)
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow
            -finput-charset=UTF-8 -fexec-charset=UTF-8)
    endif()
endfunction()

# State, rules and opening are implemented; match/AI/records reserve boundaries.
add_subdirectory(src/state)
add_subdirectory(src/rules)
add_subdirectory(src/opening)
add_subdirectory(src/match)
add_subdirectory(src/ai)
add_subdirectory(src/records)
add_subdirectory(src/gui)

if(MINGW)
    # windres reparses absolute -I arguments incorrectly for paths containing spaces.
    # Compile a self-contained resource with relative input/output names instead.
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/resources")
    configure_file(src/app/resources.rc resources/resources.rc COPYONLY)
    configure_file(src/app/app.manifest resources/app.manifest COPYONLY)
    set(manifest_object "${CMAKE_BINARY_DIR}/resources/resources.obj")
    add_custom_command(OUTPUT "${manifest_object}"
        COMMAND "${CMAKE_RC_COMPILER}" -O coff resources.rc -o resources.obj
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/resources"
        DEPENDS "${CMAKE_BINARY_DIR}/resources/resources.rc" "${CMAKE_BINARY_DIR}/resources/app.manifest"
        VERBATIM)
    set_source_files_properties("${manifest_object}" PROPERTIES GENERATED TRUE EXTERNAL_OBJECT TRUE)
    add_executable(omok WIN32 src/app/main.cpp "${manifest_object}")
else()
    add_executable(omok WIN32 src/app/main.cpp src/app/resources.rc)
endif()
target_include_directories(omok PRIVATE src/app)
target_link_libraries(omok PRIVATE omok_gui)

foreach(target IN ITEMS omok omok_gui)
    target_compile_definitions(${target} PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN
        WINVER=0x0A00 _WIN32_WINNT=0x0A00)
endforeach()
foreach(target IN ITEMS omok omok_gui omok_state)
    omok_compiler_options(${target})
endforeach()
if(MINGW)
    target_link_options(omok PRIVATE -municode -static -static-libgcc -static-libstdc++)
endif()
if(MSVC)
    # The resource file already contains the application manifest.
    target_link_options(omok PRIVATE /MANIFEST:NO)
endif()
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

install(TARGETS omok RUNTIME DESTINATION .)
install(FILES docs/runtime-readme.md DESTINATION . RENAME README.md)
install(FILES THIRD_PARTY_NOTICES.md DESTINATION .)
install(DIRECTORY licenses DESTINATION .)
set(CPACK_GENERATOR ZIP)
set(CPACK_PACKAGE_FILE_NAME "omok-${PROJECT_VERSION}-windows-x64")
include(CPack)
