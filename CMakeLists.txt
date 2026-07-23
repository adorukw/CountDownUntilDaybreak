cmake_minimum_required(VERSION 3.10)

project(CDUD VERSION 1.0.0 LANGUAGES C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wextra -g)

file(GLOB_RECURSE SRC CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/src/*.c)
add_executable(CDUD ${SRC})

target_include_directories(CDUD PRIVATE ${PROJECT_SOURCE_DIR}/include)

add_custom_target(copy_compile_db ALL
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_BINARY_DIR}/compile_commands.json
        ${PROJECT_SOURCE_DIR}/compile_commands.json
    COMMENT "复制 compile_commands.json 到项目根目录"
)
