cmake_minimum_required(VERSION 3.28)

set(CMAKE_C_STANDARD 99)
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

    src/networking/client.c
    src/networking/server.c
    src/networking/socket.c

    src/scripting/scripting_api.c
    src/scripting/intermediate.c

    src/scripting/modules/modules.c
    src/scripting/modules/packets.c
    src/scripting/modules/players.c

    src/structures/result.c
    src/structures/hashtable.c

    src/util/crypto.c
    src/util/log.c
    src/util/stringext.c

    src/win32/fs.c
    src/win32/mutex.c
    src/win32/socket.c
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