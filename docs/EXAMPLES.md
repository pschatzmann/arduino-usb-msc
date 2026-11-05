# Examples

| Example | What it shows |
|---|---|
| `USBHostMSC_Info` | Inquiry, capacity, hex dump of LBA 0 |
| `USBHostMSC_BlockRW` | Destructive raw block read/write round-trip |
| `USBMscFatFS_ListFiles` | Mount FAT/exFAT, recursively list files |
| `USBMscFS_ListFiles` | `USBMscAutoFS`: mount whichever of FAT/ext4 the drive is |
| `USBMscFS_Ext4ReadFile` | Mount ext2/3/4 read-only, read a file |
| `USBMscFS_Ext4WriteFile` | Mount ext4 read/write, append and read back |
| `USBMscFSPool_HubListFiles` | Multiple FAT drives behind a hub, mounted simultaneously |
| `USBHostMSC_MAX3421E_Info` | Same as `USBHostMSC_Info`, via an external MAX3421E host-shield chip |

`test_compile.sh` (repository root) test-compiles every example for
RP2040, ESP32-S3, STM32 and classic ESP32; `./test_compile.sh --list`
shows the matrix.
