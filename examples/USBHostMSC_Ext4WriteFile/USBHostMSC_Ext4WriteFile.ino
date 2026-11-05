/*
 * USBHostMSC_Ext4WriteFile
 *
 * DESTRUCTIVE TEST: mounts an ext4-formatted USB flash drive read/write
 * with TinyEXT4 and appends a line to /usb-msc-test.txt, then reads the
 * file back and prints its contents. Only run this against a drive you
 * don't mind modifying.
 *
 * TinyEXT4 writes no journal: pulling the drive mid-write needs an e2fsck
 * on Linux. ext2/ext3, a volume that needs fsck, or one using features
 * TinyEXT4 can't write, is mounted read-only even here -- check
 * isReadOnly(). If you only need to read, see USBHostMSC_Ext4ReadFile.
 *
 * Board setup: see USBHostMSC_Info example.
 */
#include "USBHostMSC.h"
#include "USBHostMscExt4IO.h"

#if defined(ARDUINO_ARCH_RP2040) || defined(ARDUINO_ARCH_STM32)
#define SerialOut Serial1
#else
#define SerialOut Serial
#endif

USBHostMSC msc;
USBHostMscExt4IO usbIO(msc);
tinyext4::Ext4FS usbExt4;

const char *kPath = "/usb-msc-test.txt";

void onMount(uint8_t dev_addr) {
  SerialOut.printf("Mass storage device mounted: dev_addr=%u\r\n", dev_addr);
  if (!usbExt4.begin(usbIO)) {
    SerialOut.printf("Could not mount as ext2/3/4: %s\r\n",
                     usbExt4.lastErrorText());
    return;
  }
  if (usbExt4.isReadOnly()) {
    SerialOut.println("Mounted read-only (ext2/3 or needs fsck): not writing.");
    return;
  }
  SerialOut.printf("Mounted read/write. Appending a line to %s...\r\n", kPath);
  tinyext4::File wf = usbExt4.open(kPath, "a");
  if (!wf) {
    SerialOut.println("  open for append failed");
    return;
  }
  wf.printf("hello from usb-msc at millis()=%lu\r\n", (unsigned long)millis());
  wf.close();   // writes the inode
  usbExt4.sync(); // and the metadata: safe to pull the drive now

  SerialOut.println("Reading it back:");
  tinyext4::File rf = usbExt4.open(kPath, "r");
  if (!rf) {
    SerialOut.println("  (could not reopen for reading)");
    return;
  }
  char buf[128];
  size_t n;
  while ((n = rf.read((uint8_t *)buf, sizeof(buf) - 1)) > 0) {
    buf[n] = 0;
    SerialOut.print(buf);
  }
  rf.close();
}

void onUnmount(uint8_t dev_addr) {
  SerialOut.printf("Unmounted dev_addr=%u\r\n", dev_addr);
  usbExt4.end();
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
