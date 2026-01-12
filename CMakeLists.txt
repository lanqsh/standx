cmake_minimum_required(VERSION 3.10)
project(standx_client)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")

set(CMAKE_VERBOSE_MAKEFILE OFF)

find_package(OpenSSL REQUIRED)
find_package(CURL REQUIRED)
find_package(Poco REQUIRED COMPONENTS Foundation Util Net Data)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBSODIUM REQUIRED libsodium)
pkg_check_modules(SECP256K1 QUIET libsecp256k1)

if(SECP256K1_FOUND)
  message(STATUS "pkg-config found libsecp256k1: ${SECP256K1_LIBRARIES}")
  set(SECP256K1_INCLUDE_DIRS ${SECP256K1_INCLUDE_DIRS})
  set(SECP256K1_LIBRARY_DIRS ${SECP256K1_LIBRARY_DIRS})
  set(SECP256K1_LIBRARIES ${SECP256K1_LIBRARIES})
else()
  message(STATUS "pkg-config did not find libsecp256k1, trying common locations (/usr/local, /usr)")
  find_path(SECP256K1_INCLUDE_DIRS NAMES secp256k1.h PATHS /usr/local/include /usr/include)
  find_library(SECP256K1_LIBRARIES NAMES secp256k1 PATHS /usr/local/lib /usr/lib)
  if(SECP256K1_INCLUDE_DIRS AND SECP256K1_LIBRARIES)
    message(STATUS "Found libsecp256k1 include: ${SECP256K1_INCLUDE_DIRS}, lib: ${SECP256K1_LIBRARIES}")
  else()
    message(WARNING "libsecp256k1 not found via pkg-config or common locations.")
  endif()
endif()

include_directories(
  ${CURL_INCLUDE_DIRS}
  ${OPENSSL_INCLUDE_DIRS}
  ${LIBSODIUM_INCLUDE_DIRS}
  ${SECP256K1_INCLUDE_DIRS}
)

link_directories(
  ${LIBSODIUM_LIBRARY_DIRS}
  ${SECP256K1_LIBRARY_DIRS}
)

file(GLOB SOURCES "src/*.cpp")

add_executable(standx_client
  ${SOURCES}
)

target_link_libraries(standx_client
  PRIVATE
  ${CURL_LIBRARIES}
  ${OPENSSL_LIBRARIES}
  ${LIBSODIUM_LIBRARIES}
  ${SECP256K1_LIBRARIES}
  Poco::Foundation
  Poco::Util
  Poco::Net
  Poco::Data
)