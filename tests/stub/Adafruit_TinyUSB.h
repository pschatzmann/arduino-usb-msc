// Desktop stub of the device-mode MSC class, just enough for USBDeviceMSC.cpp.
#pragma once
#include <stdint.h>
#define CFG_TUD_ENABLED 1
#define CFG_TUD_MSC 1
struct Adafruit_USBD_MSC {
  void setID(const char *, const char *, const char *) {}
  void setCapacity(uint32_t, uint16_t) {}
  void setReadWriteCallback(int32_t (*)(uint32_t, void *, uint32_t),
                            int32_t (*)(uint32_t, uint8_t *, uint32_t),
                            void (*)()) {}
  void setUnitReady(bool) {}
  bool begin() { return true; }
};
