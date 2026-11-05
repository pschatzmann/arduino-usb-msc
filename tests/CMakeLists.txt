# Host mode: filesystem layer + partition parsing against an image-file stub
# of USBHostMSC (see tests/stub/USBHostMSC_host_test.h).
foreach(t partition fat ext4 lifetime)
  add_executable(test_host_${t} test_host_${t}.cpp)
  target_link_libraries(test_host_${t} PRIVATE usbmsc_fs)
  target_compile_options(test_host_${t} PRIVATE -Wall -Wextra)
  add_test(NAME host_${t} COMMAND test_host_${t})
endforeach()

# Device mode: USBMscRamDisk + USBDeviceMSC against a stub of Adafruit_USBD_MSC
# (tests/stub/Adafruit_TinyUSB.h).
add_executable(test_device_ramdisk test_device_ramdisk.cpp
               ${CMAKE_SOURCE_DIR}/src/USBDeviceMSC.cpp)
target_include_directories(test_device_ramdisk PRIVATE
  ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/stub)
target_link_libraries(test_device_ramdisk PRIVATE usbmsc_fs)
target_compile_options(test_device_ramdisk PRIVATE -Wall -Wextra)
add_test(NAME device_ramdisk COMMAND test_device_ramdisk)
