# Standalone CMake project for the DSP unit tests.
# These tests don't link against JUCE -- they only exercise the
# JUCE-independent DSP modules. You can either:
#
#   (a) run tests/run_all.sh  -- a shell driver that compiles each test
#                                with g++ and runs it
#
#   (b) configure this directory as its own CMake project:
#         cd ds4-native/tests
#         cmake -B build
#         cmake --build build
#         ctest --test-dir build --output-on-failure

cmake_minimum_required(VERSION 3.22)
project(ds4_dsp_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_testing()

set(SRC ../Source)

function(add_dsp_test name srcs)
    add_executable(${name} ${srcs})
    target_include_directories(${name} PRIVATE ${SRC})
    if (MSVC)
        target_compile_options(${name} PRIVATE /W3)
    else()
        target_compile_options(${name} PRIVATE -Wall -Wextra -Wno-unused-parameter)
    endif()
    add_test(NAME ${name} COMMAND ${name})
endfunction()

add_dsp_test(test_TriangleCoreVCO "test_TriangleCoreVCO.cpp;${SRC}/TriangleCoreVCO.cpp")
add_dsp_test(test_ExpEnvelope     "test_ExpEnvelope.cpp;${SRC}/ExpEnvelope.cpp")
add_dsp_test(test_OTAVCA          "test_OTAVCA.cpp;${SRC}/OTAVCA.cpp")
add_dsp_test(test_LFOSchmitt      "test_LFOSchmitt.cpp;${SRC}/LFOSchmitt.cpp")
add_dsp_test(test_PiezoTrigger    "test_PiezoTrigger.cpp;${SRC}/PiezoTrigger.cpp")
add_dsp_test(test_MoogLadder      "test_MoogLadder.cpp")
add_dsp_test(test_stress
    "test_stress.cpp;${SRC}/TriangleCoreVCO.cpp;${SRC}/NoiseVoice.cpp;${SRC}/ExpEnvelope.cpp;${SRC}/OTAVCA.cpp;${SRC}/LFOSchmitt.cpp;${SRC}/PiezoTrigger.cpp")
