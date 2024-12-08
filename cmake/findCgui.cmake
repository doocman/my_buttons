
# Needed for it's "backport" library

fetchcontent_declare(
        cgui
        GIT_REPOSITORY https://github.com/doocman/component-gui.git
        GIT_TAG 0ad66f16e15eed8348885e06c9e0dcb6bfa11ee5 # main
)

FetchContent_GetProperties(cgui)

if (NOT cgui_POPULATED)
    FetchContent_Populate(cgui)
endif ()

add_library(mybdeps_cgui_headers INTERFACE)
add_library(mybdeps::cgui_headers ALIAS mybdeps_cgui_headers)
target_include_directories(mybdeps_cgui_headers INTERFACE "${cgui_SOURCE_DIR}/c++-src/inc")

