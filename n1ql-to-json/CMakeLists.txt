cmake_minimum_required (VERSION 3.1)
cmake_policy(VERSION 3.1)
project (N1QL_to_JSON)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

set(LITECORE ../vendor/couchbase-lite-core/)
set(FLEECE   ${LITECORE}/vendor/fleece)

set(
    SRC 
    n1ql.cc
    n1ql_to_json.cc
)

add_library(n1ql_to_json_obj OBJECT ${SRC})
add_library(n1ql_to_json SHARED $<TARGET_OBJECTS:n1ql_to_json_obj>)
add_library(n1ql_to_json_static STATIC $<TARGET_OBJECTS:n1ql_to_json_obj>)

target_compile_definitions(n1ql_to_json_obj PRIVATE -DCMAKE)
target_include_directories(
    n1ql_to_json_obj PRIVATE
    ${LITECORE}/C/include
    ${LITECORE}/LiteCore/Support
    ${FLEECE}/API
    ${FLEECE}/Fleece/Support
)
if(WIN32)
    target_include_directories(
        n1ql_to_json_obj PUBLIC
        ${LITECORE}/MSVC
    )

    set_target_properties(
        n1ql_to_json PROPERTIES LINK_FLAGS
        "/def:${PROJECT_SOURCE_DIR}/../cblite/cblite.def"
    )
endif()

target_link_libraries(
    n1ql_to_json PUBLIC 
    FleeceStatic
)

if(APPLE)
    set_target_properties(
        n1ql_to_json PROPERTIES LINK_FLAGS
        "-exported_symbols_list ${PROJECT_SOURCE_DIR}/../Xcode/n1ql_to_json.exp"
    )
endif()
