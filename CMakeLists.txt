cmake_minimum_required(VERSION 3.25)
project(math-hunter LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

option(MATHHUNTER_ENABLE_SANITIZERS "Build with -fsanitize=address,undefined,leak" OFF)
option(MATHHUNTER_ENABLE_TESTS "Build test executables" ON)
option(MATHHUNTER_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

add_library(mathhunter_warnings INTERFACE)
target_compile_options(mathhunter_warnings INTERFACE
    -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
    -Wnon-virtual-dtor -Woverloaded-virtual -Wnull-dereference -Wdouble-promotion
)
if(MATHHUNTER_WARNINGS_AS_ERRORS)
    target_compile_options(mathhunter_warnings INTERFACE -Werror)
endif()

add_library(mathhunter_sanitizers INTERFACE)
if(MATHHUNTER_ENABLE_SANITIZERS)
    target_compile_options(mathhunter_sanitizers INTERFACE
        -fsanitize=address,undefined,leak
        -fno-sanitize-recover=all
        -fno-omit-frame-pointer
    )
    target_link_options(mathhunter_sanitizers INTERFACE
        -fsanitize=address,undefined,leak
    )
endif()

find_path(GMP_INCLUDE_DIR NAMES gmp.h)
find_library(GMP_LIBRARY NAMES gmp)
find_library(GMPXX_LIBRARY NAMES gmpxx)
if(NOT GMP_INCLUDE_DIR OR NOT GMP_LIBRARY OR NOT GMPXX_LIBRARY)
    message(FATAL_ERROR "GMP (libgmp-dev) with C++ bindings (gmpxx) is required")
endif()
add_library(gmp_deps INTERFACE)
target_include_directories(gmp_deps INTERFACE ${GMP_INCLUDE_DIR})
target_link_libraries(gmp_deps INTERFACE ${GMPXX_LIBRARY} ${GMP_LIBRARY})

find_package(OpenSSL REQUIRED COMPONENTS Crypto)
find_package(nlohmann_json 3.9 REQUIRED)

file(GLOB_RECURSE MATHHUNTER_CORE_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/core/*.cpp
     ${CMAKE_SOURCE_DIR}/src/arithmetic/*.cpp ${CMAKE_SOURCE_DIR}/src/certificate/*.cpp
     ${CMAKE_SOURCE_DIR}/src/sat/*.cpp ${CMAKE_SOURCE_DIR}/src/conjectures/*.cpp
     ${CMAKE_SOURCE_DIR}/src/verifier/*.cpp ${CMAKE_SOURCE_DIR}/src/framework/*.cpp)

add_library(mathhunter_core STATIC ${MATHHUNTER_CORE_SOURCES})
target_include_directories(mathhunter_core PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(mathhunter_core PUBLIC gmp_deps OpenSSL::Crypto nlohmann_json::nlohmann_json
                       mathhunter_warnings mathhunter_sanitizers)

# mathhunter_core relies on link-time module self-registration: every
# src/conjectures/*_registration.cpp translation unit contributes a
# [[gnu::constructor]] function that registers its module with
# framework::registry(), but nothing else in the program references
# symbols inside those translation units. A normal static-library link
# only pulls in .o members that resolve an outstanding symbol reference,
# which would silently drop registration entirely. --whole-archive (via
# CMake's LINK_LIBRARY:WHOLE_ARCHIVE genex) forces every member of
# mathhunter_core into the final binary so registration always runs.
add_executable(math-hunter src/cli/hunter_main.cpp src/cli/metadata_loader.cpp)
target_link_libraries(math-hunter PRIVATE $<LINK_LIBRARY:WHOLE_ARCHIVE,mathhunter_core>)

add_executable(math-verifier src/cli/verifier_main.cpp)
target_link_libraries(math-verifier PRIVATE $<LINK_LIBRARY:WHOLE_ARCHIVE,mathhunter_core>)

set_target_properties(math-hunter math-verifier
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if(MATHHUNTER_ENABLE_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
