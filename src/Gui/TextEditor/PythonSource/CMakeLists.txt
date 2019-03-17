
SET(LibName PythonSource)
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

# qt specific
if (BUILD_QT5)
    include_directories(
        ${Qt5Core_INCLUDE_DIRS}
        ${Qt5Widgets_INCLUDE_DIRS}
        ${Qt5PrintSupport_INCLUDE_DIRS}
        ${Qt5UiTools_INCLUDE_DIRS}
    )
    list(APPEND Common_LIBS
        ${Qt5Core_LIBRARIES}
        ${Qt5Widgets_LIBRARIES}
        ${Qt5PrintSupport_LIBRARIES}
        ${Qt5UiTools_LIBRARIES}
    )
else()
    include_directories(
        ${QT_INCLUDE_DIR}
    )
    list(APPEND Common_LIBS
        ${QT_LIBRARIES}
        ${QT_QTUITOOLS_LIBRARY}
    )
endif()

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
SET(PythonSource_MOC_HDRS
    PythonSourceRoot.h
)
# run moc
fc_wrap_cpp(PythonSource_MOC_SRCS ${PythonSource_MOC_HDRS})

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

if (BUILD_QT5)
    QT5_ADD_RESOURCES(PythonSource_QRC_SRCS ${PythonSource_RES_SRCS})
    QT5_WRAP_UI(PythonSource_UIC_HDRS ${PythonSource_Dlg_UIC_SRCS})
else(BUILD_QT5)
    QT4_ADD_RESOURCES(PythonSource_QRC_SRCS ${PythonSource_RES_SRCS})
    QT4_WRAP_UI(PythonSource_UIC_HDRS ${PythonSource_Dlg_UIC_SRCS})
endif(BUILD_QT5)
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


