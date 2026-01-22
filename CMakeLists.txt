cmake_minimum_required(VERSION 3.16)
project(FreiaThiwi VERSION 0.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- version -------------------------------------------------
set(PROJECT_VERSION_MAJOR 0)
set(PROJECT_VERSION_MINOR 3)
set(PROJECT_VERSION_PATCH 0)
set(PROJECT_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")

# make the macro visible to the source
add_compile_definitions(PROJECT_VERSION="${PROJECT_VERSION}")
# -------------------------------------------------------------

# 𐍆𐍂𐌴𐌹𐌰 𐌸𐌹𐍅𐌹 - Free Servant
add_executable(freia-thiwi
    src/main.cpp
    src/FreiaEncryption.cpp
    src/server.cpp
)

target_include_directories(freia-thiwi PRIVATE include)

# OpenSSL (required)
find_package(OpenSSL REQUIRED)
target_link_libraries(freia-thiwi OpenSSL::SSL OpenSSL::Crypto)

# Copy binary to bin/ after build
set_target_properties(freia-thiwi PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin
)

# Raspberry Pi? Just works.
# Japan? USA? Just works.
# Police raid? Binary is 300 KB. Burn the Pi.
message("
𐍆𐍂𐌴𐌹𐌰 𐌸𐌹𐍅𐌹 v${PROJECT_VERSION} - The maid is ready.
Three passwords. Zero trust. Runs on a €35 computer.
Dictator-proof. Raid-proof. Future-proof.
")