/*
 * USBHostMSC_FSPool_HubListFiles
 *
 * Plug a USB hub into the host port, then plug up to 4 FAT-formatted
 * flash drives into the hub. Each one gets mounted into its own slot and
 * listed independently -- unlike USBMscFatFS (a single instance), which
 * would tear down and remount over itself for every device that mounts.
 *
 * A hub adds power draw on top of the drives themselves; whether your
 * board's host port can supply enough current for a bus-powered hub plus
 * multiple drives is a hardware question this library can't answer for
 * you -- a self-powered hub (its own power supply) sidesteps it.
 *
 * Board setup: see USBHostMSC_Info example.
 */

#include "USBHostMSC.h"
#include "USBMscFSPool.h"
#include "USBMscFatFS.h"

#if defined(ARDUINO_ARCH_RP2040) || defined(ARDUINO_ARCH_STM32)
// RP2040: the native USB port is claimed for host duty.
// STM32: "Serial" is TinyUSB's own device-mode CDC-ACM object, which
// would conflict with claiming the same OTG peripheral for host mode.
#define SerialOut Serial1
#else
#define SerialOut Serial
#endif

// STM32 has no fs::FS/File -- USBMscFatFS::fs() there returns
// USBMscFatFS& (itself) instead of fs::FS&, and open() returns a
// USBMscFile instead of a fs::File. These aliases let this one sketch
// build for all three platforms unchanged.
#if defined(ARDUINO_ARCH_STM32)
using UsbFsType = USBMscFatFS;
using UsbFsFile = USBMscFile;
#else
using UsbFsType = fs::FS;
using UsbFsFile = File;
#endif

// Up to 4 simultaneously mounted FAT drives (a typical hub has 4 ports).
USBMscFSPool<USBMscFatFS, 4> pool;
USBHostMSC msc(&pool);

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

  // By the time this callback runs, USBHostMSC has already called
  // pool.begin(msc, dev_addr) internally.
  if (!pool.mounted(dev_addr)) {
    SerialOut.printf("  Could not mount as FAT, or all %u pool slots are full.\r\n",
                      (unsigned)pool.mountedCount());
    return;
  }

  SerialOut.printf("  Mounted (%u/%u slots in use). Contents:\r\n",
                    pool.mountedCount(), 4);
  listDir(pool.fs(dev_addr), "/", 1);
}

void onUnmount(uint8_t dev_addr) {
  SerialOut.printf("Unmounted dev_addr=%u\r\n", dev_addr);
}

void setup() {
  SerialOut.begin(115200);
  while (!SerialOut) {
    delay(10);
  }
  SerialOut.println(
      "USBHostMSC_FSPool_HubListFiles: plug in a hub, then plug in some drives...");

  msc.onMount(onMount);
  msc.onUnmount(onUnmount);
  msc.begin();
}

void loop() { msc.task(); }
