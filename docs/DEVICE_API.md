# Device mode API (`USBDeviceMSC`)

In device mode the board is the USB stick: a PC plugged into the board's USB
port sees a drive whose sectors come from a `USBMscBlockDevice`. (For the
opposite direction, reading a stick, see [BLOCK_API.md](BLOCK_API.md).)

Select the device USB stack first: ESP32-S2/S3 *USB Mode > USB-OTG
(TinyUSB)*, RP2040 *USB Stack > Adafruit TinyUSB* -- see
[BOARDS.md](BOARDS.md#device-mode).

```cpp
#include "USBDeviceMSC.h"

USBMscRamDisk disk(128);   // 128 blocks * 512 bytes
USBDeviceMSC msc;

void setup() {
  disk.begin();                        // allocate + format FAT
  msc.setID("Arduino", "RAM Disk", "1.0");   // optional
  msc.begin(disk);
}
void loop() {}
```

## `USBDeviceMSC`

| Method | Purpose |
|---|---|
| `setID(vendor, product, revision)` | SCSI inquiry strings (max 8 / 16 / 4 chars). Call before `begin()`. |
| `begin(dev, startUsb = true)` | Register the MSC interface and export `dev`. |
| `end()` | Stop exporting; the PC sees the medium disappear. |
| `setWritable(bool)` | `false` makes the export read-only: PC writes fail. Default `true`. |
| `mediaPresent(bool)` | "Insert"/"eject" the medium. Use `false` while the sketch accesses the data itself. |
| `onWrite(cb)` | `void cb(uint32_t lba, uint32_t count)`, called from the USB task after the PC wrote blocks. Keep it short. |
| `hostActive()` | `true` once the PC has read or written. |
| `written()` / `clearWritten()` | Whether the PC has written since the last clear. |

**Start order.** `begin()` must run before the USB stack starts. On RP2040
(and other cores that start USB themselves) call it first in `setup()`,
before `Serial.begin()`. On ESP32-S2/S3 `begin()` calls `USB.begin()` for you;
pass `startUsb = false` if you combine MSC with other USB classes and call
`USB.begin()` yourself.

**One instance.** Only one `USBDeviceMSC` (a single logical unit) can be
active; a second `begin()` returns `false`.

## `USBMscBlockDevice`

Implement this to export your own storage:

```cpp
class MyFlash : public USBMscBlockDevice {
public:
  uint32_t blockCount() override;
  uint16_t blockSize() override { return 512; }       // default
  bool readBlocks(uint32_t lba, void *buf, uint32_t count) override;
  bool writeBlocks(uint32_t lba, const void *buf, uint32_t count) override;
  void flush() override {}                             // PC asked to flush
};
```

`readBlocks`/`writeBlocks` return `true` on success; a `false` makes the PC
see an I/O error. They run in the USB task, so don't block for long. On ESP32
the core may issue unaligned reads/writes; `USBDeviceMSC` turns those into
block operations for you.

## `USBMscRamDisk`

A RAM drive. `USBMscRamDisk disk(blocks, storage = nullptr)` mallocs
`blocks * 512` bytes, or uses the buffer you pass. Contents are lost on reset.

- `begin(formatFat = true)` -- allocates, and writes an empty FAT12 (small
  disks) or FAT16 volume. At least 64 blocks (32 KB) are required.
- `addFile("NAME.EXT", data, len)` -- adds a file to the root directory
  (8.3 names, upper case, files stored contiguously). Call it after
  `begin()` and before the PC gets the drive.
- `data()` -- direct access to the memory, e.g. to preload a prebuilt image
  (`begin(false)` then copy it in).

## `USBMscSdCard`

`#include "USBMscSdCard.h"` (needs SdFat) turns the board into a USB SD-card
reader. The whole card is exported, whatever filesystem is on it:

```cpp
SdFat sd;
USBDeviceMSC msc;

void setup() {
  sd.cardBegin(SdSpiConfig(SS, SHARED_SPI, SD_SCK_MHZ(16)));  // not sd.begin()
  static USBMscSdCard card(sd.card());
  msc.begin(card);
}
```

Use `cardBegin()`, not `begin()`: it initialises the card without requiring a
FAT filesystem, so ext4 cards work. See the examples
`USBDeviceMSC_FatSdCard` and `USBDeviceMSC_Ext4SdCard`.

## Filesystems in device mode

The PC does the filesystem work; the board only serves sectors. So any
filesystem the PC understands works: FAT/exFAT everywhere, ext2/3/4 on Linux
(Windows offers to *format* an ext4 drive -- decline; the ext4 example is
read-only by default for that reason).

The filesystem classes of host mode (`USBMscFatFS`, `USBMscExt4FS`, ...)
read from a plugged-in `USBHostMSC` drive and are not used here.

**Do not use the medium from the sketch while the PC has it mounted.** The PC
caches metadata, and two writers corrupt the filesystem. Either only touch
the data before `begin()`, or take turns:

```cpp
msc.mediaPresent(false);   // "eject": the PC unmounts
// ... sketch reads/writes the card or RAM disk ...
msc.mediaPresent(true);    // PC sees a new medium and remounts
```

## Host and device in one firmware

Both need a USB controller each. RP2040's native port is either host or
device (stack menu), ESP32-S2/S3 have one OTG controller. Boards with a
second controller (e.g. a MAX3421E host chip, see [MAX3421E.md](MAX3421E.md))
can run `USBHostMSC` and `USBDeviceMSC` together, e.g. as a USB-stick-to-PC
bridge.

## Limits

- Single logical unit, 512-byte (or `blockSize()`) blocks.
- No hot-swap detection of the PC side (`hostActive()` only says the PC has
  talked to the drive).
- Tested: desktop unit tests for the block layer (`device_ramdisk`) and
  compile tests on RP2040 and ESP32-S3. Enumeration and transfers on real
  hardware are not covered by automated tests.
