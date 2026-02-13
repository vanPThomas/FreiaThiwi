cmake_minimum_required(VERSION 3.16)
project(FreiaThiwi VERSION 0.5.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Version macro for source code
set(PROJECT_VERSION_MAJOR 0)
set(PROJECT_VERSION_MINOR 5)
set(PROJECT_VERSION_PATCH 0)
set(PROJECT_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")
add_compile_definitions(PROJECT_VERSION="${PROJECT_VERSION}")

# Main executable
add_executable(freia-thiwi
    src/main.cpp
    src/FreiaEncryption.cpp
    src/server.cpp
    src/AccountDatabase.cpp
)

target_include_directories(freia-thiwi PRIVATE include)

# Dependencies
find_package(OpenSSL REQUIRED)
find_package(SQLite3 REQUIRED)

target_link_libraries(freia-thiwi
    PRIVATE
        OpenSSL::SSL
        OpenSSL::Crypto
        SQLite::SQLite3
)

# Output directory
set_target_properties(freia-thiwi PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
)

target_compile_options(freia-thiwi PRIVATE
    -Wall -Wextra -Wpedantic -Werror=return-type
)

# Fancy startup message
message(STATUS "
𐍆𐍂𐌴𐌹𐌰 𐌸𐌹𐍅𐌹 v${PROJECT_VERSION} - The maid is ready.
Three passwords. Zero trust. Runs on a €35 computer.
Dictator-proof. Raid-proof. Future-proof.
")