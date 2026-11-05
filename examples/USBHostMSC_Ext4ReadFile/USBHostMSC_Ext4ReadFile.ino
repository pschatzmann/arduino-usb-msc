/*
 * USBHostMSC_Ext4ReadFile
 *
 * Mounts an ext2/3/4-formatted USB flash drive read-only with TinyEXT4 and
 * prints the contents of /hello.txt (create that file from a Linux machine
 * first). The drive may be unpartitioned, or MBR or GPT partitioned.
 *
 * USBHostMscExt4IO is the TinyEXT4 driver for the drive. After the drive was
 * unplugged every access fails until Ext4FS::begin() is called again, so
 * nothing cached from one drive is ever written to another one.
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
USBHostMscExt4IO usbIO(msc); // the first drive that is plugged in
tinyext4::Ext4FS usbExt4;

void onMount(uint8_t dev_addr) {
  SerialOut.printf("Mass storage device mounted: dev_addr=%u\r\n", dev_addr);
  if (!usbExt4.begin(usbIO, /*readOnly=*/true)) {
    SerialOut.printf("Could not mount as ext2/3/4: %s\r\n",
                     usbExt4.lastErrorText());
    return;
  }
  SerialOut.println("Mounted. Reading /hello.txt:");
  tinyext4::File f = usbExt4.open("/hello.txt", "r");
  if (!f) {
    SerialOut.println("  (could not open /hello.txt)");
    return;
  }
  char buf[128];
  size_t n;
  while ((n = f.read((uint8_t *)buf, sizeof(buf) - 1)) > 0) {
    buf[n] = 0;
    SerialOut.print(buf);
  }
  f.close();
  SerialOut.println();
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
  SerialOut.println("USBHostMSC_Ext4ReadFile: waiting for a USB drive...");
  msc.onMount(onMount);
  msc.onUnmount(onUnmount);
  msc.begin();
}

void loop() { msc.task(); }
