/*
 * USBHostMSC_MAX3421E_Info
 *
 * Same as USBHostMSC_Info, but for boards with no USB peripheral of their
 * own (e.g. classic ESP32) that add USB host support via an external
 * MAX3421E host-shield chip wired over SPI. See docs/MAX3421E.md for
 * wiring, board coverage, and the CFG_TUH_MAX3421 build details.
 *
 * Board setup:
 *  - Classic ESP32 (no native USB): this is the only way to do USB host.
 *  - ESP32-S2/S3/P4 (native USB-OTG): only needed if you're deliberately
 *    using a MAX3421E chip instead of the native port -- add
 *    -DCFG_TUH_MAX3421=1 as a build flag. Otherwise use USBHostMSC_Info.
 *  - SAMD21/SAMD51/nRF52840: also supported by Adafruit_TinyUSB_Arduino's
 *    MAX3421E driver; adjust the pins below for your wiring.
 *
 * Wire the MAX3421E breakout's SPI (SCK/MOSI/MISO), CS and INT pins to
 * the board -- default below is CS=10, INT=9 (Adafruit's own default for
 * boards without a documented FeatherWing pinout); change to match your
 * wiring.
 */

#include "SPI.h"
#include "USBHostMSC.h"

// CS, INT -- change to match your wiring.
Adafruit_USBH_Host usbHost(&SPI, /*cs=*/10, /*intr=*/9);

USBHostMSC msc(usbHost);

void printInquiry(uint8_t dev_addr) {
  MSCInquiry inq;
  if (msc.inquiry(dev_addr, 0, inq)) {
    Serial.printf("  Vendor : %s\r\n", inq.vendor_id);
    Serial.printf("  Product: %s\r\n", inq.product_id);
    Serial.printf("  Rev    : %s\r\n", inq.product_rev);
  }
}

void onMount(uint8_t dev_addr) {
  Serial.printf("Mass storage device mounted: dev_addr=%u\r\n", dev_addr);
  Serial.printf("  LUNs       : %u\r\n", msc.maxLun(dev_addr));
  Serial.printf("  Block size : %u bytes\r\n", msc.blockSize(dev_addr));
  Serial.printf("  Block count: %u\r\n", msc.blockCount(dev_addr));
  Serial.printf("  Capacity   : %llu bytes\r\n",
                (unsigned long long)msc.capacity(dev_addr));
  printInquiry(dev_addr);
}

void onUnmount(uint8_t dev_addr) {
  Serial.printf("Mass storage device unmounted: dev_addr=%u\r\n", dev_addr);
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }
  Serial.println(
      "USBHostMSC_MAX3421E_Info: waiting for a USB drive...");

  msc.onMount(onMount);
  msc.onUnmount(onUnmount);
  msc.begin();
}

void loop() { msc.task(); }
