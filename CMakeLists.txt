cmake_minimum_required(VERSION 3.15)
project(StarByte VERSION 0.8.1 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

if(MSVC)
    add_compile_options(/W4 /utf-8)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)
endif()

add_executable(starbyte
    src/main.c
    src/lexer.c
    src/parser.c
    src/ast.c
    src/value.c
    src/interpreter.c
    src/codegen.c
)

target_include_directories(starbyte PRIVATE src)
target_compile_definitions(starbyte PRIVATE
    STARBYTE_VERSION="${PROJECT_VERSION}"
)

if(NOT WIN32)
    target_link_libraries(starbyte PRIVATE m)
endif()

if(WIN32)
    target_compile_definitions(starbyte PRIVATE STARBYTE_WINDOWS=1)
else()
    target_compile_definitions(starbyte PRIVATE STARBYTE_LINUX=1)
endif()

install(TARGETS starbyte RUNTIME DESTINATION bin)
