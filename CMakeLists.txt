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
    src/scripting/modules/net.c

    src/structures/result.c
    src/structures/hashtable.c

    src/util/crypto.c
    src/util/log.c
    src/util/stringext.c
    src/util/fs.c
)

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