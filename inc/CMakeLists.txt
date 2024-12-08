
set(CUR_TARGET myb_headers)
add_library(${CUR_TARGET} INTERFACE)
add_library(myb::${CUR_TARGET} ALIAS ${CUR_TARGET})
target_include_directories(${CUR_TARGET} INTERFACE "")
target_link_libraries(${CUR_TARGET} INTERFACE mybdeps::cgui_headers)
