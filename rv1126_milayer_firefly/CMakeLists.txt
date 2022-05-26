cmake_minimum_required(VERSION 3.4.1)

set(CMAKE_C_COMPILER "/home/lyz/fouraibox/rv1126_rv1109_linux_release_20211022/prebuilts/gcc/linux-x86/arm/gcc-arm-8.3-2019.03-x86_64-arm-linux-gnueabihf/bin/arm-linux-gnueabihf-gcc")
set(CMAKE_CXX_COMPILER "/home/lyz/fouraibox/rv1126_rv1109_linux_release_20211022/prebuilts/gcc/linux-x86/arm/gcc-arm-8.3-2019.03-x86_64-arm-linux-gnueabihf/bin/arm-linux-gnueabihf-g++")

project(rv1126_milayer)

add_compile_options(-std=c++11 -Wall)
add_compile_options(-DLINUX)


if (CMAKE_C_COMPILER MATCHES "aarch64")
  set(LIB_ARCH lib64)
else()
  set(LIB_ARCH lib)
endif()

# rga
include_directories(${CMAKE_SOURCE_DIR}/3rdparty/rga/include)
set(RGA_SRC_PATH ${CMAKE_SOURCE_DIR}/3rdparty/rga/src)

# rkmedia
include_directories(${CMAKE_SOURCE_DIR}/3rdparty/easymedia)
include_directories(${CMAKE_SOURCE_DIR}/3rdparty/rkmedia)
set(RKMEDIA_LIB ${CMAKE_SOURCE_DIR}/${LIB_ARCH}/libeasymedia.so)

add_library(milayer SHARED
	${RGA_SRC_PATH}/RockchipRga.cpp
    	${RGA_SRC_PATH}/GrallocOps.cpp
    	${RGA_SRC_PATH}/NormalRga.cpp
    	${RGA_SRC_PATH}/NormalRgaApi.cpp
    	${RGA_SRC_PATH}/RgaUtils.cpp
	${RGA_SRC_PATH}/im2d.cpp
        src/milayer.cpp
        src/milog.cpp
        )

target_link_libraries(milayer
  ${RKMEDIA_LIB}
)

SET_TARGET_PROPERTIES(milayer PROPERTIES VERSION 1.0.0.5)
#set(CMAKE_INSTALL_PREFIX ${CMAKE_SOURCE_DIR})

install(TARGETS milayer DESTINATION ${CMAKE_SOURCE_DIR}/milayer_dev)
install(DIRECTORY ${LIB_ARCH}/ DESTINATION ${CMAKE_SOURCE_DIR}/milayer_dev)
install(FILES src/milayer.h  DESTINATION ${CMAKE_SOURCE_DIR}/milayer_dev)
