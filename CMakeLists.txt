cmake_minimum_required(VERSION 3.16)
project(standx_client)

set(CMAKE_CXX_STANDARD 17)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBSODIUM REQUIRED libsodium)
pkg_check_modules(LIBCURL REQUIRED libcurl)
find_package(OpenSSL REQUIRED)

include_directories(${LIBSODIUM_INCLUDE_DIRS} ${LIBCURL_INCLUDE_DIRS})
link_directories(${LIBSODIUM_LIBRARY_DIRS} ${LIBCURL_LIBRARY_DIRS})

find_package(PkgConfig REQUIRED)
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
		message(WARNING "libsecp256k1 not found via pkg-config or common locations. You may need to set PKG_CONFIG_PATH or install libsecp256k1.")
	endif()
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/deps ${LIBSODIUM_INCLUDE_DIRS} ${LIBCURL_INCLUDE_DIRS} ${SECP256K1_INCLUDE_DIRS})
link_directories(${LIBSODIUM_LIBRARY_DIRS} ${LIBCURL_LIBRARY_DIRS} ${SECP256K1_LIBRARY_DIRS})

add_executable(standx_client
    src/main.cpp
    src/crypto_utils.cpp
    src/http_client.cpp
    src/auth.cpp
    src/standx_client.cpp
    deps/tiny_keccak.c)
target_link_libraries(standx_client ${LIBSODIUM_LIBRARIES} ${LIBCURL_LIBRARIES} ${SECP256K1_LIBRARIES} OpenSSL::Crypto)
