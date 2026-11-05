# Device mode quick start (be a USB stick)

```cpp
#include "USBDeviceMSC.h"

USBMscRamDisk disk(128);   // 64 KB RAM drive
USBDeviceMSC msc;

void setup() {
  disk.begin();                                  // allocate + format FAT
  disk.addFile("HELLO.TXT", "hello\r\n", 7);      // optional: preload a file
  msc.begin(disk);   // before Serial.begin() on RP2040; calls USB.begin() on ESP32
}

void loop() {}
```

Plug the board into a PC and a 64 KB drive appears. Or export an SD card
(any filesystem, the board just serves sectors):

```cpp
#include <SdFat.h>
#include "USBDeviceMSC.h"
#include "USBMscSdCard.h"

SdFat sd;
USBDeviceMSC msc;

void setup() {
  sd.cardBegin(SdSpiConfig(SS, SHARED_SPI, SD_SCK_MHZ(16)));  // not sd.begin()
  static USBMscSdCard card(sd.card());
  msc.begin(card);
}

void loop() {}
```

## What to know

- **Start order.** The MSC interface must be registered before the PC
  enumerates the board: call `msc.begin()` first in `setup()`, before
  `Serial.begin()` and any long initialisation. On ESP32-S2/S3 `begin()`
  starts the USB stack itself.
- **One writer at a time.** While the PC has the drive mounted, the sketch
  must not modify its files -- the PC caches, and both sides would corrupt the
  filesystem. Preload before `begin()`, or take turns with
  `msc.mediaPresent(false)` (PC "ejects") / `msc.mediaPresent(true)`.
- **Read-only export.** `msc.setWritable(false)` rejects PC writes.
- **Your own storage.** Subclass `USBMscBlockDevice` (`blockCount()`,
  `readBlocks()`, `writeBlocks()`) to export SPI flash, a file, or anything
  else; `USBMscRamDisk` and `USBMscSdCard` are two ready-made ones.
- **Filesystems.** The PC does the filesystem work, so FAT/exFAT works
  everywhere and ext2/3/4 on Linux. Windows offers to format an ext4 drive --
  decline (the ext4 example is read-only by default).
- **Notifications.** `msc.onWrite(cb)`, `msc.hostActive()` and
  `msc.written()` tell the sketch what the PC is doing.
- **Host and device together** need two USB controllers (e.g. native port as
  device plus a MAX3421E chip as host).

More: [Device API](DEVICE_API.md), [Boards](BOARDS.md#device-mode).
