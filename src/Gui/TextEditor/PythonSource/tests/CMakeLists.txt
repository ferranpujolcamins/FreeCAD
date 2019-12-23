cmake_minimum_required(VERSION 3.5)
project(testPythonSource LANGUAGES CXX)
#set (CMAKE_CXX_STANDARD 11)
add_definitions(-DGTEST_LANGUAGE_CXX11)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

set(CMAKE_CXX_FLAGS "-Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

SET(TestSRCS
    tst_PythonToken.cpp
    tst_PythonLexer.cpp
)

find_package(Threads REQUIRED)
find_package(GTest)

if (${GTEST_FOUND})
    set(GOOGLETEST_DIR ${GTEST_ROOT})
elseif ($ENV{GOOGLETEST_DIR})
    set(GOOGLETEST_DIR $ENV{GOOGLETEST_DIR})
else ()
    message(WARNING "Using googletest src dir specified at Qt Creator wizard")
    set(GOOGLETEST_DIR "/usr/src/googletest")
endif ()

if (EXISTS ${GOOGLETEST_DIR})
    set(GTestSrc ${GOOGLETEST_DIR}/googletest)
    set(GMockSrc ${GOOGLETEST_DIR}/googlemock)
else ()
    message( FATAL_ERROR "No googletest src dir found - set GOOGLETEST_DIR to enable!")
endif ()

include_directories(${GTestSrc}
                    ${GTestSrc}/include
                    ${GMockSrc}
                    ${GMockSrc}/include
                    ../)

add_executable(testPythonSource main.cpp ${TestSRCS}
               ${GTestSrc}/src/gtest-all.cc
               ${GMockSrc}/src/gmock-all.cc)
add_test(testPythonSource COMMAND testPythonSource)
target_link_libraries(testPythonSource PRIVATE Threads::Threads PythonSource)

# copy the test scripts to build dir
add_custom_target(copy-test-scripts ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/testscripts
    ${CMAKE_CURRENT_BINARY_DIR}/testscripts
)
