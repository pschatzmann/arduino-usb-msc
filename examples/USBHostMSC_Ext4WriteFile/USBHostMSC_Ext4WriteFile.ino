/*
 * USBHostMSC_Ext4WriteFile
 *
 * DESTRUCTIVE TEST: mounts an ext2/3/4-formatted USB flash drive
 * read/write and writes a line to /usb-msc-test.txt (creating or
 * appending to it), then reads the file back and prints its contents.
 * Only run this against a drive you don't mind modifying.
 *
 * TinyEXT4 writes no journal: pulling the drive mid-write needs an
 * e2fsck on Linux, and ext2/ext3 (or a volume needing fsck) mount
 * read-only even here -- check readOnly() (see the wiki page FILESYSTEMS). If
 * you only need to read, prefer USBMscExt4FSReadOnly (see
 * USBHostMSC_Ext4ReadFile).
 *
 * Board setup: see USBHostMSC_Info example.
 */

#include "USBHostMSC.h"
#include "USBMscExt4FS.h"

#if defined(ARDUINO_ARCH_RP2040) || defined(ARDUINO_ARCH_STM32)
// RP2040: the native USB port is claimed for host duty.
// STM32: "Serial" is TinyUSB's own device-mode CDC-ACM object, which
// would conflict with claiming the same OTG peripheral for host mode.
#define SerialOut Serial1
#else
#define SerialOut Serial
#endif

// STM32 has no fs::File -- open() there returns a USBMscFile instead.
#if defined(ARDUINO_ARCH_STM32)
using UsbFsFile = USBMscFile;
#else
using UsbFsFile = File;
#endif

USBMscExt4FS extFs;
USBHostMSC msc(&extFs);

const char *kPath = "/usb-msc-test.txt";

void onMount(uint8_t dev_addr) {
  SerialOut.printf("Mass storage device mounted: dev_addr=%u\r\n", dev_addr);

  // By the time this callback runs, USBHostMSC has already called
  // extFs.begin(msc, dev_addr) internally.
  if (!extFs.mounted()) {
    SerialOut.println("Could not mount as ext2/3/4.");
    return;
  }

  SerialOut.printf("Mounted read/write. Appending a line to %s...\r\n", kPath);
  UsbFsFile wf = extFs.fs().open(kPath, "a");
  if (!wf) {
    SerialOut.println("  open for append failed");
    return;
  }
  wf.printf("hello from usb-msc at millis()=%lu\r\n", (unsigned long)millis());
  wf.close();

  SerialOut.println("Reading it back:");
  UsbFsFile rf = extFs.fs().open(kPath, "r");
  if (!rf) {
    SerialOut.println("  (could not reopen for reading)");
    return;
  }
  char buf[128];
  int n;
  while ((n = rf.read((uint8_t *)buf, sizeof(buf) - 1)) > 0) {
    buf[n] = 0;
    SerialOut.print(buf);
  }
  rf.close();
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
      "USBHostMSC_Ext4WriteFile: plug in a drive you can afford to modify.");

  msc.onMount(onMount);
  msc.onUnmount(onUnmount);
  msc.begin();
}

void loop() { msc.task(); }
