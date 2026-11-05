/*
 * USBHostMSC_AutoFS_ListFiles
 *
 * Mounts a USB flash drive whatever it is formatted with -- FAT/exFAT
 * (TinyFATFS) is tried first, then ext2/3/4 (TinyEXT4, read-only) -- and
 * lists its contents through the standard Arduino fs::FS API, so the same
 * listDir() works for both. fs::FS is what other libraries take (e.g.
 * WebServer::serveStatic()).
 *
 * fatfs::ArduinoFS and tinyext4::ArduinoFS turn a mounted volume into an
 * fs::FS; they exist on the ESP32 and RP2040 cores. This sketch pulls both
 * filesystems into the firmware; use USBHostMSC_FatFS_ListFiles or
 * USBHostMSC_Ext4ReadFile if you only need one.
 *
 * Board setup: see USBHostMSC_Info example. ESP32 / RP2040 only.
 */
#include "USBHostMSC.h"
#include "USBHostMscExt4IO.h"
#include "USBHostMscFatIO.h"
#include "fatfs-arduino-fs.h"
#include "tinyext4/ArduinoFS.h"

#if defined(ARDUINO_ARCH_RP2040)
#define SerialOut Serial1
#else
#define SerialOut Serial
#endif

USBHostMSC msc;

USBHostMscFatIO fatIO(msc);
fatfs::SDClass usbFat;
fatfs::ArduinoFS fatFS(usbFat);

USBHostMscExt4IO extIO(msc);
tinyext4::Ext4FS usbExt4;
tinyext4::ArduinoFS extFS(usbExt4);

fs::FS *mounted = nullptr;

void listDir(fs::FS &fs, const char *dirname, uint8_t depth) {
  fs::File dir = fs.open(dirname, "r");
  if (!dir || !dir.isDirectory()) {
    return;
  }
  for (fs::File entry = dir.openNextFile(); entry; entry = dir.openNextFile()) {
    for (uint8_t i = 0; i < depth; i++) {
      SerialOut.print("  ");
    }
    if (entry.isDirectory()) {
      SerialOut.printf("[%s]\r\n", entry.name());
      char child[256];
      snprintf(child, sizeof(child), "%s%s%s", dirname,
               strcmp(dirname, "/") == 0 ? "" : "/", entry.name());
      listDir(fs, child, depth + 1);
    } else {
      SerialOut.printf("%s (%u bytes)\r\n", entry.name(), (unsigned)entry.size());
    }
  }
}

void onMount(uint8_t dev_addr) {
  SerialOut.printf("Mass storage device mounted: dev_addr=%u\r\n", dev_addr);
  if (usbFat.begin(fatIO)) {
    SerialOut.println("Mounted as FAT/exFAT. Contents:");
    mounted = &fatFS;
  } else if (usbExt4.begin(extIO, /*readOnly=*/true)) {
    SerialOut.println("Mounted as ext2/3/4 (read-only). Contents:");
    mounted = &extFS;
  } else {
    SerialOut.println("Could not detect/mount a supported filesystem.");
    return;
  }
  listDir(*mounted, "/", 0);
}

void onUnmount(uint8_t dev_addr) {
  SerialOut.printf("Unmounted dev_addr=%u\r\n", dev_addr);
  usbFat.end();
  usbExt4.end();
  mounted = nullptr;
}

void setup() {
  SerialOut.begin(115200);
  while (!SerialOut) {
    delay(10);
  }
  SerialOut.println("USBHostMSC_AutoFS_ListFiles: waiting for a USB drive...");
  msc.onMount(onMount);
  msc.onUnmount(onUnmount);
  msc.begin();
}

void loop() { msc.task(); }
