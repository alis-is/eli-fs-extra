cmake_minimum_required (VERSION 2.6)
project (eli_fs_extra)

file(GLOB eli_fs_extra_sources ./src/**.c)
SET(eli_fs_extra ${eli_fs_extra_sources})

ADD_LIBRARY (eli_fs_extra ${eli_fs_extra})
TARGET_LINK_LIBRARIES (eli_fs_extra)