/*
 * USBHostMSC_Hub_ListFiles
 *
 * Plug a USB hub into the host port, then plug up to 4 FAT-formatted flash
 * drives into the hub. Each one is mounted into its own slot -- a
 * USBHostMscFatIO bound to the drive's USB address plus its own
 * SDClass -- and listed independently.
 *
 * A hub adds power draw on top of the drives themselves; whether your
 * board's host port can supply enough current for a bus-powered hub plus
 * multiple drives is a hardware question -- a self-powered hub (its own
 * power supply) sidesteps it.
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

struct Slot {
  uint8_t dev_addr = 0; // 0 = free
  USBHostMscFatIO io;
  SDClass fat;
};
const int kSlots = 4;
Slot slots[kSlots];

void listDir(SDClass &fat, const char *dirname, uint8_t depth) {
  File dir = fat.open(dirname);
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
      listDir(fat, child, depth + 1);
    } else {
      SerialOut.printf("%s (%lu bytes)\r\n", entry.name(),
                       (unsigned long)entry.size());
    }
  }
}

int usedSlots() {
  int n = 0;
  for (Slot &s : slots) {
    if (s.dev_addr != 0) n++;
  }
  return n;
}

void onMount(uint8_t dev_addr) {
  SerialOut.printf("Mass storage device mounted: dev_addr=%u\r\n", dev_addr);
  for (Slot &s : slots) {
    if (s.dev_addr != 0) continue;
    s.io.begin(msc, dev_addr); // this drive only
    if (!s.fat.begin(s.io)) {
      SerialOut.println("  Could not mount as FAT/exFAT.");
      return;
    }
    s.dev_addr = dev_addr;
    SerialOut.printf("  Mounted (%d/%d slots in use). Contents:\r\n",
                     usedSlots(), kSlots);
    listDir(s.fat, "/", 1);
    return;
  }
  SerialOut.println("  All slots are in use.");
}

void onUnmount(uint8_t dev_addr) {
  SerialOut.printf("Unmounted dev_addr=%u\r\n", dev_addr);
  for (Slot &s : slots) {
    if (s.dev_addr == dev_addr) {
      s.fat.end();
      s.dev_addr = 0;
    }
  }
}

void setup() {
  SerialOut.begin(115200);
  while (!SerialOut) {
    delay(10);
  }
  SerialOut.println(
      "USBHostMSC_Hub_ListFiles: plug in a hub, then plug in some drives...");
  msc.onMount(onMount);
  msc.onUnmount(onUnmount);
  msc.begin();
}

void loop() { msc.task(); }
