#
# tiledb/api/c_api/query/CMakeLists.txt
#
# The MIT License
#
# Copyright (c) 2022 TileDB, Inc.
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

list(APPEND SOURCES
  query_api.cc
)
gather_sources(${SOURCES})

#
# Object library for other units to depend upon
#
add_library(capi_query OBJECT ${SOURCES})
target_link_libraries(capi_query PUBLIC export)
target_link_libraries(capi_query PUBLIC baseline $<TARGET_OBJECTS:baseline>)
target_link_libraries(capi_query PUBLIC constants $<TARGET_OBJECTS:constants>)

#
# Test-compile of object library ensures link-completeness
#
add_executable(compile_capi_query EXCLUDE_FROM_ALL)
target_sources(compile_capi_query PRIVATE test/compile_capi_query_main.cc)
target_link_libraries(compile_capi_query PRIVATE capi_query)
add_dependencies(all_link_complete compile_capi_query)

add_test_subdirectory()
