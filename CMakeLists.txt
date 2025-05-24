# CMakeLists.txt
cmake_minimum_required(VERSION 3.14)
project(async_chat
        VERSION 1.0
        LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Boost 1.66 REQUIRED COMPONENTS system)
find_package(SqliteOrm REQUIRED)

# Клиент
add_executable(client
    client.cpp
)
target_link_libraries(client
    PRIVATE
        Boost::system
        sqlite_orm::sqlite_orm
        pthread
)
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(client PRIVATE -Wall -Wextra -pedantic)
endif()

# Сервер
add_executable(server
    server.cpp
)
target_link_libraries(server
    PRIVATE
        Boost::system
        sqlite_orm::sqlite_orm
        pthread
)
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(server PRIVATE -Wall -Wextra -pedantic)
endif()
