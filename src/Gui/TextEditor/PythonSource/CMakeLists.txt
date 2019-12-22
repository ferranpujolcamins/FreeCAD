cmake_minimum_required(VERSION 3.3 FATAL_ERROR)
SET(LibName PythonSource)

SET(CMAKE_POSITION_INDEPENDENT_CODE ON)

INCLUDE_DIRECTORIES(
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
    ${PYTHON_INCLUDE_DIRS}
)
FILE(GLOB CMakeCache RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_BINARY_DIR}/CMakeCache.txt)

IF(CMAKE_BUILD_TYPE)
    add_definitions(-DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}")
ENDIF(CMAKE_BUILD_TYPE)

# dependencies
list(APPEND Common_LIBS ${PYTHON_LIBRARIES})

# -------------------------------------------------------------------------------------

# The python analyzer sources
SET(PythonSource_CPP_SRCS
    PythonLexer.cpp
    PythonSource.cpp
    PythonSourceFrames.cpp
    PythonSourceIdentifiers.cpp
    PythonSourceImports.cpp
    PythonSourceIndent.cpp
    PythonSourceItemBase.cpp
    PythonSourceListBase.cpp
    PythonSourceModule.cpp
    PythonSourceParameter.cpp
    PythonSourceParser.cpp
    PythonSourceRoot.cpp
    PythonSourcePrimitiveTypes.cpp
    PythonToken.cpp
)
SET(PythonSource_HPP_SRCS
    PythonLexer.h
    PythonSource.h
    PythonSourceFrames.h
    PythonSourceIdentifiers.h
    PythonSourceImports.h
    PythonSourceIndent.h
    PythonSourceItemBase.h
    PythonSourceListBase.h
    PythonSourceModule.h
    PythonSourceParameter.h
    PythonSourceParser.h
    PythonSourceRoot.h
    PythonSourcePrimitiveTypes.h
    PythonToken.h

)

SET(PythonSource_SRCS
    ${PythonSource_CPP_SRCS}
    ${PythonSource_HPP_SRCS}
    ${PythonSource_MOC_HDRS}
)

# for debugging/dumping syntax parser and AST code
if(BUILD_PYTHON_DEBUGTOOLS)
    SET(PythonSource_DbgTools_CPP_SRCS
        PythonSourceDebugTools.cpp)
    SET(PythonSource_DbgTools_HPP_SRCS
        PythonSourceDebugTools.h)
    add_definitions(-DBUILD_PYTHON_DEBUGTOOLS=1)
    SET(PythonSource_SRCS
        ${PythonSource_SRCS}
        ${PythonSource_DbgTools_CPP_SRCS}
        ${PythonSource_DbgTools_HPP_SRCS})
endif()
SOURCE_GROUP("PythonSource" FILES ${PythonSource_SRCS})

SET(PythonSource_Dlg_SRCS
    ${PythonSource_Dlg_CPP_SRCS}
    ${PythonSource_Dlg_HPP_SRCS}
    ${PythonSource_Dlg_MOC_SRCS}
)

# wrap it all up for lib PythonSource
SET(PythonSourceAll_SRCS ${PythonSource_SRCS}
                       ${PythonSource_DbgTools_SRCS}
)
SOURCE_GROUP("PythonSource" FILES ${PythonSourceAll_SRCS})


#create library
ADD_LIBRARY(${LibName} SHARED ${PythonSourceAll_SRCS})
TARGET_LINK_LIBRARIES(${LibName} ${Common_LIBS} ${TextEditor})

# install to default target defined by
INSTALL(TARGETS ${LibName}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# -------------------------------------------------------------------------------

if(BUILD_TEST)
    enable_testing()
    include(GoogleTest)
    add_subdirectory(tests)
endif(BUILD_TEST)
