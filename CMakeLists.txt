cmake_minimum_required(VERSION 3.10)

project(asio-ftp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED On)

# recursively find all .cpp files
file(GLOB_RECURSE src "src/*.cpp")

add_executable(asio-ftp main.cpp ${src})

# SQLite3 library
find_package(SQLite3 REQUIRED)
if (SQLite3_FOUND)
    include_directories(${SQLite3_INCLUDE_DIRS})
    target_link_libraries(asio-ftp sqlite3)
else()
    message(FATAL_ERROR "SQLite3 not found. Please install it using a package manager")
endif()

# get rid of warning
if(POLICY CMP0167)
  cmake_policy(SET CMP0167 NEW)
endif()

# Boost library (need Boost.Asio and Boost.JSON)
find_package(
    Boost 1.75.0 REQUIRED
    COMPONENTS json
)
if (Boost_FOUND)
    include_directories(${BOOST_INCLUDE_DIRS})
    target_link_libraries(asio-ftp ${BOOST_LIBRARIES})
    target_link_libraries(asio-ftp Boost::json)
else()
    message(FATAL_ERROR "Boost not found. Please install it using a package manager")
endif()

# OpenSSL (required by Boost.SSL)
find_package(OpenSSL REQUIRED)
if (OpenSSL_FOUND)
    include_directories(${OPENSSL_INCLUDE_DIRS})
    target_link_libraries(asio-ftp ${OPENSSL_LIBRARIES})
else()
    message(FATAL_ERROR "OpenSSL not found. Please install it using a package manager")
endif()

# Set sqlite3 to threadsafe
target_compile_options(asio-ftp PRIVATE "-DTHREADSAFE=1")