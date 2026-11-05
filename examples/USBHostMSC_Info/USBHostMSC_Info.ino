/*
 * USBHostMSC_Info
 *
 * Waits for a USB flash drive to be plugged into the board's USB host
 * port, prints its SCSI Inquiry info and capacity, then hex-dumps the
 * first block (LBA 0, typically the MBR/partition table).
 *
 * Board setup:
 *  - RP2040:   Tools > USB Stack > "Adafruit TinyUSB Host (native)"
 *              Wire the USB connector's D+/D- (and 5V/GND) directly to the
 *              RP2040 (e.g. an "RP2040 USB Host" board, or bit-bang wiring
 *              on a plain Pico). Serial output goes over Serial1 since the
 *              native USB port is now the host port.
 *  - ESP32-S2/S3: Tools > USB Mode > "USB-OTG (TinyUSB)" (arduino-esp32
 *              core >= 3.3; the core's own TinyUSB is used, no extra
 *              library). The native USB port is now the host port, so
 *              Serial is UART0 (TX=GPIO43, RX=GPIO44): connect a USB-UART
 *              adapter -- see docs/SINGLE_PORT.md.
 *  - STM32:    (F4/F7 OTG_FS parts, e.g. BlackPill F411CE) Requires a
 *              small patch to Adafruit_TinyUSB_Arduino and building with
 *              -DCFG_TUH_ENABLED=1 -- see docs/STM32.md. Serial
 *              output goes over Serial1, same reason as RP2040.
 */

#include "USBHostMSC.h"

#if defined(ARDUINO_ARCH_RP2040) || defined(ARDUINO_ARCH_STM32)
// RP2040: the native USB port is claimed for host duty.
// STM32: "Serial" is TinyUSB's own device-mode CDC-ACM object, which
// would conflict with claiming the same OTG peripheral for host mode.
#define SerialOut Serial1
#else
#define SerialOut Serial
#endif

USBHostMSC msc;

void printInquiry(uint8_t dev_addr) {
  MSCInquiry inq;
  if (msc.inquiry(dev_addr, 0, inq)) {
    SerialOut.printf("  Vendor : %s\r\n", inq.vendor_id);
    SerialOut.printf("  Product: %s\r\n", inq.product_id);
    SerialOut.printf("  Rev    : %s\r\n", inq.product_rev);
  }
}

void printBlockZero(uint8_t dev_addr) {
  uint32_t block_size = msc.blockSize(dev_addr);
  if (block_size == 0 || block_size > 4096) {
    return;
  }

  uint8_t buf[512];
  if (block_size > sizeof(buf)) {
    return;
  }

  if (!msc.readBlocks(dev_addr, 0, buf, 1)) {
    SerialOut.println("  read10(LBA 0) failed");
    return;
  }

  SerialOut.println("  LBA 0 (first 64 bytes):");
  for (uint16_t i = 0; i < 64 && i < block_size; i++) {
    if (i % 16 == 0) {
      SerialOut.printf("\r\n  %04x: ", i);
    }
    SerialOut.printf("%02x ", buf[i]);
  }
  SerialOut.println();
}

void onMount(uint8_t dev_addr) {
  SerialOut.printf("Mass storage device mounted: dev_addr=%u\r\n", dev_addr);
  SerialOut.printf("  LUNs       : %u\r\n", msc.maxLun(dev_addr));
  SerialOut.printf("  Block size : %u bytes\r\n", msc.blockSize(dev_addr));
  SerialOut.printf("  Block count: %u\r\n", msc.blockCount(dev_addr));
  SerialOut.printf("  Capacity   : %llu bytes\r\n",
                    (unsigned long long)msc.capacity(dev_addr));
  printInquiry(dev_addr);
  printBlockZero(dev_addr);
}

void onUnmount(uint8_t dev_addr) {
  SerialOut.printf("Mass storage device unmounted: dev_addr=%u\r\n", dev_addr);
}

void setup() {
  SerialOut.begin(115200);
  while (!SerialOut) {
    delay(10);
  }
  SerialOut.println("USBHostMSC_Info: waiting for a USB drive...");

  msc.onMount(onMount);
  msc.onUnmount(onUnmount);
  msc.begin();
}

void loop() { msc.task(); }
