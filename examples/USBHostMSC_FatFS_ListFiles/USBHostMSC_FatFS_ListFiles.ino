/*
 * USBHostMSC_FatFS_ListFiles
 *
 * Mounts a FAT-formatted USB flash drive and recursively lists its
 * contents. Unlike USBHostMSC_AutoFS_ListFiles (which uses USBMscAutoFS and so
 * pulls in both backends), this sketch only ever references USBMscFatFS,
 * so the ext4 backend never gets linked into the firmware image.
 *
 * Board setup: see USBHostMSC_Info example.
 */

#include "USBHostMSC.h"
#include "USBMscFatFS.h"

#if defined(ARDUINO_ARCH_RP2040) || defined(ARDUINO_ARCH_STM32)
// RP2040: the native USB port is claimed for host duty.
// STM32: "Serial" is TinyUSB's own device-mode CDC-ACM object, which
// would conflict with claiming the same OTG peripheral for host mode.
#define SerialOut Serial1
#else
#define SerialOut Serial
#endif

// STM32 has no fs::FS/File -- USBMscFatFS::fs() there returns *this
// (USBMscFatFS&) instead of fs::FS&, and open() returns a USBMscFile
// instead of a fs::File. These aliases let this one sketch build for all
// three platforms unchanged.
#if defined(ARDUINO_ARCH_STM32)
using UsbFsType = USBMscFatFS;
using UsbFsFile = USBMscFile;
#else
using UsbFsType = fs::FS;
using UsbFsFile = File;
#endif

USBMscFatFS fatFs;
USBHostMSC msc(&fatFs);

void listDir(UsbFsType &fs, const char *dirname, uint8_t depth) {
  UsbFsFile dir = fs.open(dirname, "r");
  if (!dir || !dir.isDirectory()) {
    return;
  }

  UsbFsFile entry = dir.openNextFile();
  while (entry) {
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
    entry = dir.openNextFile();
  }
}

void onMount(uint8_t dev_addr) {
  SerialOut.printf("Mass storage device mounted: dev_addr=%u\r\n", dev_addr);

  if (!fatFs.mounted()) {
    SerialOut.println("Could not mount as FAT.");
    return;
  }

  SerialOut.println("Mounted. Contents:");
  listDir(fatFs.fs(), "/", 0);
}

void onUnmount(uint8_t dev_addr) {
  SerialOut.printf("Unmounted dev_addr=%u\r\n", dev_addr);
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
