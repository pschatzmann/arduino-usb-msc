/*
 * USBHostMSC_Ext4ReadFile
 *
 * Mounts an ext2/3/4-formatted USB flash drive read-only and prints the
 * contents of /hello.txt (create that file from a Linux machine first).
 *
 * Referencing USBMscExt4FSReadOnly is what pulls the ext4 backend
 * (TinyEXT4, MIT) into your firmware; there is no separate build flag.
 *
 * Board setup: see USBHostMSC_Info example.
 */

#include "USBHostMSC.h"
#include "USBMscExt4FSReadOnly.h"

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

USBMscExt4FSReadOnly extFs;
USBHostMSC msc(&extFs);

void onMount(uint8_t dev_addr) {
  SerialOut.printf("Mass storage device mounted: dev_addr=%u\r\n", dev_addr);

  // By the time this callback runs, USBHostMSC has already called
  // extFs.begin(msc, dev_addr) internally.
  if (!extFs.mounted()) {
    SerialOut.println("Could not mount as ext2/3/4.");
    return;
  }

  SerialOut.println("Mounted. Reading /hello.txt:");
  UsbFsFile f = extFs.fs().open("/hello.txt", "r");
  if (!f) {
    SerialOut.println("  (could not open /hello.txt)");
    return;
  }

  char buf[128];
  int n;
  while ((n = f.read((uint8_t *)buf, sizeof(buf) - 1)) > 0) {
    buf[n] = 0;
    SerialOut.print(buf);
  }
  f.close();
  SerialOut.println();
}

void onUnmount(uint8_t dev_addr) {
  SerialOut.printf("Unmounted dev_addr=%u\r\n", dev_addr);
}

void setup() {
  SerialOut.begin(115200);
  while (!SerialOut) {
    delay(10);
  }
  SerialOut.println("USBHostMSC_Ext4ReadFile: waiting for a USB drive...");

  msc.onMount(onMount);
  msc.onUnmount(onUnmount);
  msc.begin();
}

void loop() { msc.task(); }
