cmake_minimum_required(VERSION 3.16)
project(standx_client)

set(CMAKE_CXX_STANDARD 17)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBSODIUM REQUIRED libsodium)
pkg_check_modules(LIBCURL REQUIRED libcurl)

include_directories(${LIBSODIUM_INCLUDE_DIRS} ${LIBCURL_INCLUDE_DIRS})
link_directories(${LIBSODIUM_LIBRARY_DIRS} ${LIBCURL_LIBRARY_DIRS})

find_package(PkgConfig REQUIRED)
pkg_check_modules(SECP256K1 QUIET libsecp256k1)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/deps ${LIBSODIUM_INCLUDE_DIRS} ${LIBCURL_INCLUDE_DIRS} ${SECP256K1_INCLUDE_DIRS})
link_directories(${LIBSODIUM_LIBRARY_DIRS} ${LIBCURL_LIBRARY_DIRS} ${SECP256K1_LIBRARY_DIRS})

add_executable(standx_client src/main.cpp deps/tiny_keccak.c)
target_link_libraries(standx_client ${LIBSODIUM_LIBRARIES} ${LIBCURL_LIBRARIES} ${SECP256K1_LIBRARIES})
