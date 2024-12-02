cmake_minimum_required(VERSION 3.28)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
project(
    dynamicserver
    VERSION 1.0
    LANGUAGES C
)

# Sources
add_executable(${PROJECT_NAME}
    ${PROJECT_NAME}.rc

    src/main.c

    src/api/modules/console.c
    src/api/modules/modules.c
    src/api/modules/packets.c
    src/api/modules/players.c
    src/api/modules/tables.c

    src/api/scripting_api.c
    src/api/intermediate.c

    src/data/crypto.c
    src/data/hashtable.c
    src/data/mutex.c
    src/data/result.c
    src/data/stringext.c

    src/net/client.c
    src/net/server.c
    src/net/socket.c

    src/io/console.c
    src/io/fs.c
)

# Copy to build directory
if (CMAKE_BUILD_TYPE STREQUAL "Release")
    add_custom_target(copy-ds ALL COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/.ds ${CMAKE_SOURCE_DIR}/out/.ds DEPENDS ${PROJECT_NAME})
    add_custom_target(copy-lua ALL COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/.luaconfig ${CMAKE_SOURCE_DIR}/out/.vscode DEPENDS ${PROJECT_NAME})
    add_custom_target(copy-exe ALL COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.exe ${CMAKE_SOURCE_DIR}/out/${PROJECT_NAME}.exe DEPENDS ${PROJECT_NAME})
endif()

# Libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/lib/lua5.4/lib/lua54${CMAKE_STATIC_LIBRARY_SUFFIX}
    ws2_32.lib
    bcrypt.lib
)
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE
    ${CMAKE_SOURCE_DIR}/lib/lua5.4/include
)

# Compile Options
target_compile_options(${PROJECT_NAME} PRIVATE
    -Wall
    -Wextra
)
add_definitions(-D_CRT_SECURE_NO_WARNINGS)