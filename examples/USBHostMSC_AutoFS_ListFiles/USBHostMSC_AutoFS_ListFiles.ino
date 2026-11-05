/*
 * USBHostMSC_AutoFS_ListFiles
 *
 * Mounts a USB flash drive's filesystem (tries FAT first, then ext2/3/4 --
 * see USBMscAutoFS) and recursively lists its contents.
 *
 * Passing &mscFs to the USBHostMSC constructor makes it mount/unmount
 * automatically as devices come and go, before onMount()/onUnmount() run.
 * There's no separate build flag for backend selection: this example
 * pulls in both the FAT and ext4 backends because it
 * references USBMscAutoFS. Use USBMscFatFS or USBMscExt4FS directly (see
 * USBHostMSC_Ext4ReadFile) to keep the other one out of your firmware.
 *
 * Board setup: see USBHostMSC_Info example.
 */

#include "USBHostMSC.h"
#include "USBMscAutoFS.h"

#if defined(ARDUINO_ARCH_RP2040) || defined(ARDUINO_ARCH_STM32)
// RP2040: the native USB port is claimed for host duty.
// STM32: "Serial" is TinyUSB's own device-mode CDC-ACM object, which
// would conflict with claiming the same OTG peripheral for host mode.
#define SerialOut Serial1
#else
#define SerialOut Serial
#endif

USBMscAutoFS<> mscFs; // FAT (r/w) + ext4 (read-only); USBMscAutoFS<USBMscExt4FS> for ext4 r/w
USBHostMSC msc(&mscFs);

void listDir(fs::FS &fs, const char *dirname, uint8_t depth) {
  File dir = fs.open(dirname, "r");
  if (!dir || !dir.isDirectory()) {
    return;
  }

  File entry = dir.openNextFile();
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
  // mscFs.begin(msc, dev_addr) internally.
  if (!mscFs.isFat() && !mscFs.isExt4()) {
    SerialOut.println("Could not detect/mount a supported filesystem.");
    return;
  }

  SerialOut.printf("Mounted as %s. Contents:\r\n", mscFs.isFat() ? "FAT" : "ext2/3/4");
  listDir(mscFs.fs(), "/", 0);
}

void onUnmount(uint8_t dev_addr) {
  SerialOut.printf("Unmounted dev_addr=%u\r\n", dev_addr);
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
