/*
 * USBHostMSC_FatFS_ListFiles
 *
 * Mounts a FAT12/16/32 or exFAT formatted USB flash drive with TinyFATFS and
 * recursively lists its contents. Works the same on every supported board
 * (ESP32-S2/S3, RP2040, STM32): SDClass / File have the API of
 * the Arduino SD library.
 *
 * USBHostMscFatIO is the TinyFATFS driver for the drive; unplugging is
 * handled (open files fail, the next access after re-plugging mounts the
 * new drive). On ESP32 / RP2040 you can also get an fs::FS with
 * ArduinoFS -- see USBHostMSC_AutoFS_ListFiles.
 *
 * Board setup: see USBHostMSC_Info example.
 */
#include "USBHostMSC.h"
#include "USBHostMscFatIO.h"

#if defined(ARDUINO_ARCH_RP2040) || defined(ARDUINO_ARCH_STM32)
#define SerialOut Serial1
#else
#define SerialOut Serial
#endif

USBHostMSC msc;
USBHostMscFatIO usbIO(msc); // the first drive that is plugged in
SDClass usbFat;

void listDir(const char *dirname, uint8_t depth) {
  File dir = usbFat.open(dirname);
  if (!dir || !dir.isDirectory()) {
    return;
  }
  for (File entry = dir.openNextFile(); entry; entry = dir.openNextFile()) {
    for (uint8_t i = 0; i < depth; i++) {
      SerialOut.print("  ");
    }
    if (entry.isDirectory()) {
      SerialOut.printf("[%s]\r\n", entry.name());
      char child[256];
      snprintf(child, sizeof(child), "%s%s%s", dirname,
               strcmp(dirname, "/") == 0 ? "" : "/", entry.name());
      listDir(child, depth + 1);
    } else {
      SerialOut.printf("%s (%lu bytes)\r\n", entry.name(),
                       (unsigned long)entry.size());
    }
  }
}

void onMount(uint8_t dev_addr) {
  SerialOut.printf("Mass storage device mounted: dev_addr=%u\r\n", dev_addr);
  if (!usbFat.begin(usbIO)) {
    SerialOut.println("Could not mount as FAT/exFAT.");
    return;
  }
  SerialOut.println("Mounted. Contents:");
  listDir("/", 0);
}

void onUnmount(uint8_t dev_addr) {
  SerialOut.printf("Unmounted dev_addr=%u\r\n", dev_addr);
  usbFat.end();
}

void setup() {
  SerialOut.begin(115200);
  while (!SerialOut) {
    delay(10);
  }
  SerialOut.println("USBHostMSC_FatFS_ListFiles: waiting for a USB drive...");
  msc.onMount(onMount);
  msc.onUnmount(onUnmount);
  msc.begin();
}

void loop() { msc.task(); }
