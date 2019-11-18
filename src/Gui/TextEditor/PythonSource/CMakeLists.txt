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

if(WIN32)
    add_definitions(-DFCGui) # For GUIExport to work
endif(WIN32)

# dependencies
list(APPEND Common_LIBS
    FreeCADBase FreeCADApp TextEditor)

# -------------------------------------------------------------------------------------

# The editor sources
SET(PythonSource_CPP_SRCS
    PythonSource.cpp
    PythonSourceFrames.cpp
    PythonSourceIdentifiers.cpp
    PythonSourceImports.cpp
    PythonSourceIndent.cpp
    PythonSourceListBase.cpp
    PythonSourceModule.cpp
    PythonSourceParameter.cpp
    PythonSourceRoot.cpp
)
SET(PythonSource_HPP_SRCS
    PythonSource.h
    PythonSourceFrames.h
    PythonSourceIdentifiers.h
    PythonSourceImports.h
    PythonSourceIndent.h
    PythonSourceListBase.h
    PythonSourceModule.h
    PythonSourceParameter.h
    PythonSourceRoot.h
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

# wrap it all up for lib TextEditor
SET(PythonSourceAll_SRCS ${PythonSource_SRCS}
                       ${PythonSource_Dlg_SRCS}
                       ${PythonSource_QRC_SRCS}
                       ${PythonSource_UIC_HDRS}
                       ${PythonSource_DbgTools_SRCS}
)
SOURCE_GROUP("PythonSource" FILES ${PythonSourceAll_SRCS})


# precompiled?
if (BUILD_USE_PCH)
    add_definitions(-D_PreComp_)
    ADD_MSVC_PRECOMPILED_HEADER(${LibName} PreCompiled.h PreCompiled.cpp TextEditor_CPP_SRCS)
endif(BUILD_USE_PCH)

#create library
ADD_LIBRARY(${LibName} STATIC ${PythonSourceAll_SRCS})
TARGET_LINK_LIBRARIES(${LibName} ${Common_LIBS} ${TextEditor})

# install to default target defined by
INSTALL(TARGETS ${LibName}
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)

# -------------------------------------------------------------------------------


