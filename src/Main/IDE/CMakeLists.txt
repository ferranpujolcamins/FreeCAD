
#set(CMAKE_POSITION_INDEPENDENT_CODE ON)


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


if(NOT BUILD_DYNAMIC_LINK_PYTHON)
    # executables have to be linked against python libraries,
    # because extension modules are not.
    list(APPEND FreeCAD_LIBS
        ${PYTHON_LIBRARIES}
    )
endif(NOT BUILD_DYNAMIC_LINK_PYTHON)

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
    FreeCADBase FreeCADApp FreeCADGui)

set(IDE_CPP_SRCS
    MainIde.cpp
    MainWindow.cpp
)

set(IDE_HPP_SRCS
    MainWindow.h
)

set(IDE_MOC_HDRS
    MainWindow.h
)

fc_wrap_cpp(IDE_MOC_SRCS ${IDE_MOC_HDRS})

set(IDE_SRCS
    ${IDE_CPP_SRCS}
    ${IDE_HPP_SRCS}
    ${IDE_MOC_HDRS}
)

source_group("IDE" FILES ${IDE_SRCS})

set(FREECAD_BUILD_IDE ON)

if (FREECAD_BUILD_IDE)

    set(IDE_RES_SRCS
        #../Icons/resource.qrc
        #Language/translation.qrc
    )

    if (BUILD_QT5)
        QT5_ADD_RESOURCES(IDE_QRC_SRCS ${IDE_RES_SRCS})
    else(BUILD_QT5)
        QT4_ADD_RESOURCES(IDE_QRC_SRCS ${IDE_RES_SRCS})
    endif(BUILD_QT5)

    list(APPEND IDE_SRCS ${IDE_QRC_SRCS})

    #create a separate executable
    add_executable(FreeCAD-IDE ${IDE_SRCS})
    target_link_libraries(FreeCAD-IDE ${Common_LIBS})

    SET_BIN_DIR(FreeCAD-IDE FreeCAD_IDE)

    set_property(TARGET FreeCAD-IDE PROPERTY POSITION_INDEPENDENT_CODE ON)
endif(FREECAD_BUILD_IDE)

