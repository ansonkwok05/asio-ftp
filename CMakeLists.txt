cmake_minimum_required(VERSION 3.30)

project(asio-ftp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED On)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(CheckIPOSupported)
check_ipo_supported(RESULT iposupported OUTPUT error)

# recursively find all .cpp files
file(GLOB_RECURSE src "src/*.cpp")

add_executable(asio-ftp main.cpp ${src})

# SQLite3 library
find_package(SQLite3 REQUIRED)
if (SQLite3_FOUND)
    message(STATUS "SQLite3 found")
    include_directories(${SQLite3_INCLUDE_DIRS})
    target_link_libraries(asio-ftp sqlite3)
else()
    message(FATAL_ERROR "SQLite3 not found")
endif()

# Boost library (need Boost.Asio and Boost.JSON)
find_package(
    Boost 1.78.0 REQUIRED
    COMPONENTS json
)
if (Boost_FOUND)
    message(STATUS "Boost found")
    include_directories(${BOOST_INCLUDE_DIRS})
    target_link_libraries(asio-ftp ${BOOST_LIBRARIES})
    target_link_libraries(asio-ftp Boost::json)
else()
    message(FATAL_ERROR "Boost not found")
endif()

# OpenSSL (required by Boost.SSL)
find_package(OpenSSL REQUIRED)
if (OpenSSL_FOUND)
    message(STATUS "OpenSSL found")
    include_directories(${OPENSSL_INCLUDE_DIRS})
    target_link_libraries(asio-ftp ${OPENSSL_LIBRARIES})
else()
    message(FATAL_ERROR "OpenSSL not found")
endif()

# enable link time optimization for release build
if(CMAKE_BUILD_TYPE STREQUAL "Release" AND iposupported)
    message(STATUS "IPO / LTO enabled")
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
    set_property(TARGET asio-ftp PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()
