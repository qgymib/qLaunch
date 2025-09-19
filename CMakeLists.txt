cmake_minimum_required(VERSION 3.10)
project(qLaunch)

###############################################################################
# Support functions
###############################################################################
function(setup_target_wall name)
    if (CMAKE_C_COMPILER_ID STREQUAL "MSVC")
        target_compile_options(${name} PRIVATE /W4 /WX)
    else ()
        target_compile_options(${name} PRIVATE -Wall -Wextra -Werror)
    endif ()
endfunction()

###############################################################################
# Setup library and executable file
###############################################################################

add_executable(${PROJECT_NAME} WIN32
    "src/list.c"
    "src/main.c"
)
target_compile_options(${PROJECT_NAME} PRIVATE -DUNICODE -DWINVER=0x0601)
target_link_libraries(${PROJECT_NAME} PRIVATE Comctl32)
setup_target_wall(${PROJECT_NAME})

if (MINGW)
    target_link_options(${PROJECT_NAME} PRIVATE -municode)
elseif(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /utf-8)
endif ()

###############################################################################
# third_party dependency
###############################################################################

# inih
target_sources(${PROJECT_NAME} PRIVATE
    third_party/inih/ini.h
    third_party/inih/ini.c
)
target_include_directories(${PROJECT_NAME} PRIVATE third_party/inih)
