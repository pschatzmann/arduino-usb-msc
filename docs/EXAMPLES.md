# Examples

Examples are named by direction: `USBHostMSC_*` read a USB stick plugged into
the board, `USBDeviceMSC_*` make the board a USB stick for a PC. Host and
device examples need different USB configurations on RP2040 (see
[BOARDS.md](BOARDS.md)).

## Host mode (`USBHostMSC_*`)

| Example | What it shows |
|---|---|
| `USBHostMSC_Info` | Inquiry, capacity, hex dump of LBA 0 |
| `USBHostMSC_BlockRW` | Destructive raw block read/write round-trip |
| `USBHostMSC_FatFS_ListFiles` | Mount FAT/exFAT, recursively list files |
| `USBHostMSC_AutoFS_ListFiles` | `USBMscAutoFS`: mount whichever of FAT/ext4 the drive is |
| `USBHostMSC_Ext4ReadFile` | Mount ext2/3/4 read-only, read a file |
| `USBHostMSC_Ext4WriteFile` | Mount ext4 read/write, append and read back |
| `USBHostMSC_FSPool_HubListFiles` | Multiple FAT drives behind a hub, mounted simultaneously |
| `USBHostMSC_MAX3421E_Info` | Same as `USBHostMSC_Info`, via an external MAX3421E host-shield chip |

## Device mode (`USBDeviceMSC_*`)

| Example | What it shows |
|---|---|
| `USBDeviceMSC_RamDisk` | 64 KB FAT RAM drive with a preloaded file (`addFile()`) |
| `USBDeviceMSC_FatSdCard` | USB card reader for a FAT/exFAT SD card |
| `USBDeviceMSC_Ext4SdCard` | USB card reader for an ext2/3/4 SD card (read-only by default) |

The SD card examples use the SdFat library (bundled with the RP2040 core).

## Compile tests

`test_compile.sh` (repository root) test-compiles every example against the
boards it applies to: the host examples for RP2040 (host stack), ESP32-S3,
STM32 and classic ESP32; the device examples for RP2040 (device stack) and
ESP32-S3. `./test_compile.sh --list` shows the matrix.
