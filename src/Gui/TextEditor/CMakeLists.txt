SET(LibName TextEditor)
INCLUDE_DIRECTORIES(
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PYTHON_INCLUDE_DIRS}
)
FILE(GLOB CMakeCache RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_BINARY_DIR}/CMakeCache.txt)

IF(CMAKE_BUILD_TYPE)
    add_definitions(-DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}")
ENDIF(CMAKE_BUILD_TYPE)

if(WIN32)
    add_definitions(-DFCGui) # For GUIExport to work
endif(WIN32)

if (BUILD_QT5)
    include_directories(
        ${Qt5Core_INCLUDE_DIRS}
        ${Qt5Widgets_INCLUDE_DIRS}
        ${Qt5PrintSupport_INCLUDE_DIRS}
        ${Qt5UiTools_INCLUDE_DIRS}
    )
    list(APPEND TextEditor_LIBS
        FreeCADBase
        ${Qt5Core_LIBRARIES}
        ${Qt5Widgets_LIBRARIES}
        ${Qt5PrintSupport_LIBRARIES}
        ${Qt5UiTools_LIBRARIES}
    )
else()
    include_directories(
        ${QT_INCLUDE_DIR}
    )
    list(APPEND TextEditor_LIBS
        FreeCADBase
        ${QT_LIBRARIES}
        ${QT_QTUITOOLS_LIBRARY}
    )
endif()

# The editor sources
SET(TextEditor_CPP_SRCS
    EditorView.cpp
    TextDocumentEditorView.cpp
    PythonCode.cpp
    PythonEditor.cpp
    PythonJedi.cpp
    PythonSyntaxHighlighter.cpp
    SyntaxHighlighter.cpp
    TextEditor.cpp
)
SET(TextEditor_HPP_SRCS
    EditorView.h
    TextDocumentEditorView.h
    PythonCode.h
    PythonEditor.h
    PythonJedi.h
    PythonSyntaxHighlighter.h
    SyntaxHighlighter.h
    TextEditor.h
)
SET(TextEditor_MOC_HDRS
    EditorView.h
    TextDocumentEditorView.h
    PythonCode.h
    PythonEditor.h
    PythonJedi.h
    PythonSyntaxHighlighter.h
    SyntaxHighlighter.h
    TextEditor.h
)

# for debugging/dumping syntax parser and AST code
if(BUILD_PYTHON_DEBUGTOOLS)
    SET(TextEditor_CPP_SRCS
        ${TextEditor_CPP_SRCS}
        PythonCodeDebugTools.cpp)
    SET(TextEditor_HPP_SRCS
        ${TextEditor_HPP_SRCS}
        PythonCodeDebugTools.h)
    add_definitions(-DBUILD_PYTHON_DEBUGTOOLS=1)
endif()
SET(TextEditor_SRCS
    ${TextEditor_CPP_SRCS}
    ${TextEditor_HPP_SRCS}
)
SOURCE_GROUP("TextEditor" FILES ${TextEditor_SRCS})

# run moc
fc_wrap_cpp(TextEditor_MOC_SRCS ${TextEditor_MOC_HDRS})

SET(TextEditorAll_SRCS ${TextEditor_SRCS} ${TextEditor_MOC_SRCS})

# precompiled?
if (BUILD_USE_PCH)
    add_definitions(-D_PreComp_)
    ADD_MSVC_PRECOMPILED_HEADER(FreeCADGui PreCompiled.h PreCompiled.cpp FreeCADGui_CPP_SRCS)
endif(BUILD_USE_PCH)

#create library
ADD_LIBRARY(${LibName} STATIC ${TextEditorAll_SRCS})
TARGET_LINK_LIBRARIES(${LibName} ${TextEditor_LIBS})

# install to default target defined by
INSTALL(TARGETS ${LibName}
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)
