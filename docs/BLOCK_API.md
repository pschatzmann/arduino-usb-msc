# Block-device API (`USBHostMSC`)

```cpp
#include "USBHostMSC.h"

USBHostMSC msc;

void onMount(uint8_t dev_addr) {
  Serial.printf("Drive mounted: %llu bytes\r\n",
                 (unsigned long long)msc.capacity(dev_addr));
}

void setup() {
  Serial.begin(115200);
  msc.onMount(onMount);
  msc.begin();
}

void loop() {
  msc.task();
}
```

## Methods

- `bool begin(uint8_t rhport = USB_HOST_MSC_RHPORT)`
- `void task()` -- call from `loop()`
- `bool isMounted(dev_addr)`, `mountedDeviceCount()`, `deviceAddress(index)`
- `blockCount()`, `blockSize()`, `capacity()`, `maxLun()`
- `bool ready(dev_addr)` -- readiness is tracked per device by TinyUSB
- `bool inquiry(dev_addr, lun, MSCInquiry&)`
- `bool readBlocks(dev_addr, lba, buffer, block_count, lun = 0)`
- `bool writeBlocks(dev_addr, lba, buffer, block_count, lun = 0)`
- `onMount(cb)`, `onUnmount(cb)`

## Blocking behaviour

`readBlocks()`, `writeBlocks()` and `inquiry()` block until the command
finishes. They return `false` if the device is unplugged mid-command or
the command takes longer than `USB_HOST_MSC_IO_TIMEOUT_MS` (default
10000 ms). A completion that arrives after such a timeout is recognised
by a per-command tag and ignored.

## Which thread runs what

- **RP2040, STM32, MAX3421E (Adafruit TinyUSB):** you drive the stack by
  calling `msc.task()` from `loop()`. Mount/unmount callbacks are invoked
  from inside `tuh_task()`; blocking calls pump the stack themselves.
- **ESP32-S2/S3 (core TinyUSB):** the core runs the stack on its own
  FreeRTOS task. TinyUSB's callbacks only queue events; `msc.task()`
  (called from `loop()`) delivers them, so **filesystem mounting and your
  `onMount()`/`onUnmount()` callbacks run in `loop()` context**. Blocking
  calls are serialised by a mutex and sleep on a semaphore instead of
  pumping. See [ESP32_NATIVE.md](ESP32_NATIVE.md).

In both cases, call `msc.task()` regularly -- on ESP32 nothing mounts
without it.

## Build-time options

- `USB_HOST_MSC_RHPORT` -- root hub port passed to `begin()` by default
  (ignored on the ESP32 core path)
- `USB_HOST_MSC_IO_TIMEOUT_MS` -- per-command timeout (see above)
- `USBHOSTMSC_USE_ADAFRUIT_TINYUSB` -- ESP32 only: use the Adafruit
  TinyUSB Library instead of the core's TinyUSB (always the case on
  ESP32-P4)
- `USB_HOST_MSC_MAX_BLOCKS_PER_XFER` (default 1) and
  `USB_HOST_MSC_WRITE_PACING_MS` (default 1) -- ESP32 core path only:
  chunk size per SCSI command and the pause after each written chunk

## Multiple devices

`USBHostMSC` handles several mass-storage devices behind a hub; every call
is keyed by `dev_addr`. To mount several *filesystems* at once see
[MULTIPLE_DEVICES.md](MULTIPLE_DEVICES.md).
