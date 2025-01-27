#
# tiledb/sm/vfs/CMakeLists.txt
#
# The MIT License
#
# Copyright (c) 2021 TileDB, Inc.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#

include(common NO_POLICY_SCOPE)
include(object_library)

#
# `vfs` object library
#
commence(object_library vfs)
    this_target_sources(
        vfs.cc vfs_file_handle.cc mem_filesystem.cc path_win.cc posix.cc win.cc uri.cc)
    this_target_object_libraries(
        baseline buffer cancelable_tasks config math stats stringx thread_pool)
    if(WIN32)
        if(MSVC)
            find_library(SHLWAPI_LIBRARY shlwapi)
            message(STATUS "Found Win32 lib shlwapi: ${SHLWAPI_LIBRARY}")
            this_target_link_libraries(${SHLWAPI_LIBRARY})
        else()
            message(STATUS "Linking to Win32 lib shlwapi")
            this_target_link_libraries(-lshlwapi)
        endif()
    endif()
conclude(object_library)

add_test_subdirectory()
