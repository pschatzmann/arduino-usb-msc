# Host mode: TinyFATFS / TinyEXT4 through USBHostMscFatIO / USBHostMscExt4IO
# against an image-file stub of USBHostMSC (see tests/stub/USBHostMSC_host_test.h).
foreach(t fat ext4 lifetime)
  add_executable(test_host_${t} test_host_${t}.cpp)
  target_link_libraries(test_host_${t} PRIVATE usbmsc_test)
  target_compile_options(test_host_${t} PRIVATE -Wall -Wextra)
  add_test(NAME host_${t} COMMAND test_host_${t})
endforeach()

# Device mode: USBDeviceMSC exporting block drivers, against a stub of
# Adafruit_USBD_MSC (tests/stub/Adafruit_TinyUSB.h).
add_executable(test_device test_device.cpp
               ${CMAKE_SOURCE_DIR}/src/USBDeviceMSC.cpp)
target_link_libraries(test_device PRIVATE usbmsc_test)
target_compile_options(test_device PRIVATE -Wall -Wextra)
add_test(NAME device COMMAND test_device)
