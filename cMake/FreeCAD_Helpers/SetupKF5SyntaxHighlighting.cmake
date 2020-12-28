macro(SetupKF5SyntaxHighlighting)
# -------------------------------- KD5 Syntaxhighlighting --------------------------------

# switches (optional if you need to point to a non system build ie. debug built lib):
#   -DKF5SYNTAX_INCLUDE_DIRS=/path/to/header/files/:/may/be/multiple/paths/
#   -DKF5SYNTAX_LIB=/path/to/lib/libkf5syntaxhighlighting.so

# exports:
#   KF5SYNTAX_INCLUDE_DIRS
#   KF5SYNTAX_LIBDIR
#   KF5SYNTAX_LIBNAME

# optional for TextEditor in Gui
if(BUILD_GUI)
    if (KF5SYNTAX_INCLUDE_DIRS OR KF5SYNTAX_LIB)
        #user supplied paths to our lib
        # usefull when you need to link against another version than systems , or a debug built one
        set(KF5SyntaxHighlighting_FOUND TRUE) # default to found

        # split to a list ie separated by ;
        string(REPLACE ":" ";" _DIRS_LIST ${KF5SYNTAX_INCLUDE_DIRS})
        foreach(_DIR ${_DIRS_LIST})
            if(NOT EXISTS ${_DIR})
                message(WARNING "** User supplied KF5SYNTAX_INCLUDE_DIRS="
                       "${_DIR} does not exist\n")
                unset(KF5SyntaxHighlighting_FOUND)
            endif()
        endforeach()
        unset(_DIRS_LIST)

        if(NOT EXISTS ${KF5SYNTAX_LIB})
            message(WARNING "** User supplied KF5SYNTAX_LIB="
               "${KF5SYNTAX_LIB} does not exist")
           unset(KF5SyntaxHighlighting_FOUND)
        else()
            get_filename_component(KF5SYNTAX_LIBDIR ${KF5SYNTAX_LIB} DIRECTORY)
            get_filename_component(KF5SYNTAX_LIBNAME ${KF5SYNTAX_LIB} NAME_WE)
            string(REGEX REPLACE "^lib" "" KF5SYNTAX_LIBNAME ${KF5SYNTAX_LIBNAME})
            #message(FATAL_ERROR "name:${KF5SYNTAX_LIBNAME}")
        endif()

        if (KF5SyntaxHighlighting_FOUND)
            set(KF5SyntaxHighlighting_VERSION "unknown (lib found)")
            string(REPLACE ":" ";" KF5SYNTAX_INCLUDE_DIRS ${KF5SYNTAX_INCLUDE_DIRS})
        else()
            message("=================\n"
                "KF5SyntaxHighlighting not found using supplied paths.\n"
                "Generic Syntax highlighting vill be disabled.\n"
                " please adjust your paths!\n"
                "=================\n")
            set(KF5SyntaxHighlighting_VERSION "not found")
        endif()
    else(KF5SYNTAX_INCLUDE_DIRS OR KF5SYNTAX_LIB)
        find_package(KF5SyntaxHighlighting)

        if(KF5SyntaxHighlighting_FOUND)
            get_target_property(KF5SYNTAX_INCLUDE_DIRS KF5::SyntaxHighlighting INTERFACE_INCLUDE_DIRECTORIES)
            get_target_property(KF5SYNTAX_LIB KF5::SyntaxHighlighting LOCATION)
            #message(FATAL_ERROR "libpath: ${KF5SYNTAX_LIB}")
            get_filename_component(KF5SYNTAX_LIBDIR ${KF5SYNTAX_LIB} DIRECTORY)
            get_filename_component(KF5SYNTAX_LIBNAME ${KF5SYNTAX_LIB} NAME_WE)
            string(REGEX REPLACE "^lib" "" KF5SYNTAX_LIBNAME ${KF5SYNTAX_LIBNAME})
            #message(FATAL_ERROR "inc ${KF5SYNTAX_INCLUDE_DIRS}")
            #message(FATAL_ERROR "lib ${KF5SYNTAX_LIBDIR}")
            #message(FATAL_ERROR "name:${KF5SYNTAX_LIBNAME}")
        else(KF5SyntaxHighlighting_FOUND)
            message("=================\n"
                "KF5SyntaxHighlighting not found.\n"
                "Generic Syntax highlighting vill be disabled.\n"
                " libkf5syntaxhighlighting-dev in ubuntu\n"
                "=================\n")
            set(KF5SyntaxHighlighting_VERSION "not found")
        endif(KF5SyntaxHighlighting_FOUND)
    endif(KF5SYNTAX_INCLUDE_DIRS OR KF5SYNTAX_LIB)
endif(BUILD_GUI)

endmacro(SetupKF5SyntaxHighlighting)
