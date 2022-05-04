cmake_minimum_required(VERSION 3.22)
project(nntp)

find_package(Poco CONFIG REQUIRED Crypto Net NetSSLWin Util XML)

if(NOT TARGET Poco::Net)
	message(FATAL_ERROR "No Poco::Net target")
endif()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

add_subdirectory(nntp)
add_subdirectory(samples)
