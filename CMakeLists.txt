project (eli_fs_extra)

file(GLOB eli_fs_extra_sources ./src/**.c)
set(eli_fs_extra ${eli_fs_extra_sources})

add_library(eli_fs_extra ${eli_fs_extra})
target_link_libraries (eli_fs_extra)