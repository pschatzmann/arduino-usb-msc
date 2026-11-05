cmake_minimum_required(VERSION 3.16)

# Desktop (Linux/macOS) unit tests for the filesystem layer of usb-msc.
#
#   cmake -B build && cmake --build build && ctest --test-dir build --output-on-failure
#
# This does NOT build the Arduino firmware (that is done with arduino-cli,
# see test_compile.sh). It runs TinyFATFS and TinyEXT4 through the USB IO
# adapters against an image-file stub of USBHostMSC, so they can be
# exercised with real mkfs-generated disk images, under AddressSanitizer. Needs: dosfstools, exfatprogs,
# e2fsprogs, mtools, gdisk (sgdisk), util-linux (sfdisk).
project(usb_msc_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(USBHOSTMSC_SANITIZE "Build with address + undefined behaviour sanitizers" ON)
set(TINYFATFS_DIR "" CACHE PATH "TinyFATFS checkout (default: sibling dir, else downloaded)")
set(TINYEXT4_DIR  "" CACHE PATH "TinyEXT4 checkout (default: sibling dir, else downloaded)")

if(USBHOSTMSC_SANITIZE)
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()

# --- header-only dependencies: use a local checkout if there is one ----
include(FetchContent)
if(TINYFATFS_DIR)
  set(TINYFATFS_SRC "${TINYFATFS_DIR}")
endif()
if(TINYEXT4_DIR)
  set(TINYEXT4_SRC "${TINYEXT4_DIR}")
endif()
if(NOT TINYFATFS_SRC)
  if(EXISTS "${CMAKE_SOURCE_DIR}/../TinyFATFS/src")
    set(TINYFATFS_SRC "${CMAKE_SOURCE_DIR}/../TinyFATFS")
  else()
    FetchContent_Declare(tinyfatfs GIT_REPOSITORY https://github.com/pschatzmann/TinyFATFS.git
                         GIT_TAG main SOURCE_SUBDIR _none_)
    FetchContent_MakeAvailable(tinyfatfs)
    set(TINYFATFS_SRC "${tinyfatfs_SOURCE_DIR}")
  endif()
endif()
if(NOT TINYEXT4_SRC)
  if(EXISTS "${CMAKE_SOURCE_DIR}/../TinyEXT4/src")
    set(TINYEXT4_SRC "${CMAKE_SOURCE_DIR}/../TinyEXT4")
  else()
    FetchContent_Declare(tinyext4 GIT_REPOSITORY https://github.com/pschatzmann/TinyEXT4.git
                         GIT_TAG main SOURCE_SUBDIR _none_)
    FetchContent_MakeAvailable(tinyext4)
    set(TINYEXT4_SRC "${tinyext4_SOURCE_DIR}")
  endif()
endif()
message(STATUS "TinyFATFS: ${TINYFATFS_SRC}")
message(STATUS "TinyEXT4:  ${TINYEXT4_SRC}")

# --- the code under test ----------------------------------------------
# Header only: the IO adapters (USBHostMscDisk / USBHostMscFatIO /
# USBHostMscExt4IO) against an image-file stub of USBHostMSC, and
# USBDeviceMSC.cpp against a stub of Adafruit_USBD_MSC.
add_library(usbmsc_test INTERFACE)
target_compile_definitions(usbmsc_test INTERFACE USBHOSTMSC_HOST_TEST
  # TinyFATFS's desktop Arduino emulation would otherwise define main()
  # calling setup()/loop()
  FATFS_NO_MAIN)
target_include_directories(usbmsc_test INTERFACE
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/tests/stub)
# SYSTEM: don't show the (header-only) dependencies' own warnings
target_include_directories(usbmsc_test SYSTEM INTERFACE
  ${TINYFATFS_SRC}/src
  ${TINYEXT4_SRC}/src)

enable_testing()
add_subdirectory(tests)
