cmake_minimum_required (VERSION 3.1)
cmake_policy(VERSION 3.1)
project (N1QL_to_JSON)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

set(LITECORE ../vendor/couchbase-lite-core/)
set(FLEECE   ${LITECORE}/vendor/fleece)

set(SRC "n1ql.cc" "n1ql_to_json.cc")


add_library(n1ql_to_json_static STATIC ${SRC})

target_include_directories( n1ql_to_json_static PRIVATE
    ${PROJECT_SOURCE_DIR}
    ${LITECORE}vendor/fleece/Fleece/Support/
    ${LITECORE}vendor/fleece/API/
)

target_compile_definitions(n1ql_to_json_static PRIVATE -DCMAKE)


add_library(n1ql_to_json SHARED empty.cpp)
target_link_libraries(n1ql_to_json PRIVATE 
    n1ql_to_json_static
    Fleece
    FleeceBase
)

if(APPLE)
    set_target_properties(
        n1ql_to_json PROPERTIES LINK_FLAGS
        "-exported_symbols_list ${PROJECT_SOURCE_DIR}/../Xcode/n1ql_to_json.exp"
    )
endif()
