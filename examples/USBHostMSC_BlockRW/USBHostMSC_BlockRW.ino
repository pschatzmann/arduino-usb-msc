/*
 * USBHostMSC_BlockRW
 *
 * DESTRUCTIVE TEST: writes a known pattern to one block near the very end
 * of the drive and reads it back to verify raw block read10/write10 work.
 * The last block is used specifically to avoid corrupting a filesystem
 * that starts at LBA 0 -- but on some drives the backup GPT header lives
 * there. Only run this against a drive you don't mind reformatting.
 *
 * Board setup: see USBHostMSC_Info example.
 */

#include "USBHostMSC.h"
#include <string.h>

#if defined(ARDUINO_ARCH_RP2040) || defined(ARDUINO_ARCH_STM32)
// RP2040: the native USB port is claimed for host duty.
// STM32: "Serial" is TinyUSB's own device-mode CDC-ACM object, which
// would conflict with claiming the same OTG peripheral for host mode.
#define SerialOut Serial1
#else
#define SerialOut Serial
#endif

USBHostMSC msc;
bool test_done = false;

void runTest(uint8_t dev_addr) {
  uint32_t block_size = msc.blockSize(dev_addr);
  uint32_t block_count = msc.blockCount(dev_addr);
  if (block_size == 0 || block_size > 512 || block_count == 0) {
    SerialOut.println("Unsupported block size/count, aborting test.");
    return;
  }

  uint32_t test_lba = block_count - 1;
  uint8_t write_buf[512];
  uint8_t read_buf[512];
  for (uint32_t i = 0; i < block_size; i++) {
    write_buf[i] = (uint8_t)(i ^ 0xA5);
  }

  SerialOut.printf("Writing test pattern to LBA %lu...\r\n", (unsigned long)test_lba);
  if (!msc.writeBlocks(dev_addr, test_lba, write_buf, 1)) {
    SerialOut.println("write10 failed");
    return;
  }

  SerialOut.println("Reading it back...");
  if (!msc.readBlocks(dev_addr, test_lba, read_buf, 1)) {
    SerialOut.println("read10 failed");
    return;
  }

  bool ok = memcmp(write_buf, read_buf, block_size) == 0;
  SerialOut.println(ok ? "PASS: read-back matches" : "FAIL: read-back mismatch");
}

void onMount(uint8_t dev_addr) {
  SerialOut.printf("Mounted dev_addr=%u\r\n", dev_addr);
  runTest(dev_addr);
  test_done = true;
}

void onUnmount(uint8_t dev_addr) {
  SerialOut.printf("Unmounted dev_addr=%u\r\n", dev_addr);
  test_done = false;
}

void setup() {
  SerialOut.begin(115200);
  while (!SerialOut) {
    delay(10);
  }
  SerialOut.println("USBHostMSC_BlockRW: plug in a drive you can afford to overwrite.");

  msc.onMount(onMount);
  msc.onUnmount(onUnmount);
  msc.begin();
}

void loop() { msc.task(); }
