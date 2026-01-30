cmake_minimum_required(VERSION 3.10)
project(tmlanguage2vimsyntax)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Oniguruma library
find_package(PkgConfig REQUIRED)
pkg_check_modules(ONIGURAMA REQUIRED oniguruma)

# Add JSON library (nlohmann/json)
include(FetchContent)
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.2
)
FetchContent_MakeAvailable(nlohmann_json)

# Executable
add_executable(tmlanguage2vimsyntax
    main.cxx
    tmlanguage2vimsyntax.cxx
)

# Include directories
target_include_directories(tmlanguage2vimsyntax PRIVATE 
    ${ONIGURAMA_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(tmlanguage2vimsyntax
    ${ONIGURAMA_LIBRARIES}
    nlohmann_json::nlohmann_json
)

# Compiler flags
target_compile_options(tmlanguage2vimsyntax PRIVATE ${ONIGURAMA_CFLAGS_OTHER})