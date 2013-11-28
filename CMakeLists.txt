project(vfs.rtv)

cmake_minimum_required(VERSION 2.6)

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR})

enable_language(CXX)

find_package(xbmc REQUIRED)
find_package(xbmcplatform REQUIRED)
find_package(TinyXML REQUIRED)

include_directories(${XBMC_INCLUDE_DIR}
                    ${XBMCPLATFORM_INCLUDE_DIRS}
                    ${TINYXML_INCLUDE_DIRS}
                    ${PROJECT_SOURCE_DIR}/lib/libRTV)

set(RTV_SOURCES src/RTVFile.cpp)
add_subdirectory(lib/libRTV)

set(DEPLIBS ${TINYXML_LIBRARIES} rtv)

add_definitions(-D_LARGEFILE64_SOURCE
                -D_FILE_OFFSET_BITS=64
                -D__STDC_FORMAT_MACROS)

build_addon(vfs.rtv RTV DEPLIBS)
