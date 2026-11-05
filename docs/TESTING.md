# Testing

Host mode and device mode are both tested automatically, in two ways:
desktop unit tests (CMake/ctest, also run by `.github/workflows/tests.yml`)
and firmware compile tests (`test_compile.sh`). Test names say which side
they cover: `host_*` / `USBHostMSC_*` and `device_*` / `USBDeviceMSC_*`.

## Desktop unit tests (Linux/macOS, CMake)

### Host mode (`host_*`)

The hardware-independent part of the host side -- `USBMscPartition` and the
FAT / ext4 backends -- is built against a stub of `USBHostMSC` that reads
and writes a disk-image file (`tests/stub/USBHostMSC_host_test.h`; the
real header is swapped out by `-DUSBHOSTMSC_HOST_TEST`). The images are
created with the real `mkfs` tools, and `fsck` is run on them afterwards.

```bash
cmake -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Needs `cmake`, a C++17 compiler and these tools on `PATH`: `mkfs.vfat` and
`fsck.vfat` (dosfstools), `mkfs.exfat` and `fsck.exfat` (exfatprogs),
`mkfs.ext2/4` and `e2fsck` (e2fsprogs, >= 1.43), `mcopy`/`mmd` (mtools),
`sfdisk` (util-linux) and `sgdisk` (gdisk). The tests build with
AddressSanitizer and UBSan by default (`-DUSBHOSTMSC_SANITIZE=OFF` to
disable).

TinyFATFS and TinyEXT4 are taken from a sibling checkout
(`../TinyFATFS`, `../TinyEXT4`) if present, otherwise downloaded; point
CMake at others with `-DTINYFATFS_DIR=...` / `-DTINYEXT4_DIR=...`.

| Suite | Covers |
|---|---|
| `host_partition` | MBR parsing, partition-type tables (incl. exFAT `0x07`), bounds checking, clamping of bogus partition entries |
| `host_fat` | FAT16/FAT32/exFAT, raw and MBR layouts: reading, listing, `w`/`a`/`w+`/`r+`/`a+`, mkdir/rename/remove/rmdir, multi-cluster files, `fsck` clean, remount; rejects non-FAT disks |
| `host_ext4` | ext4 on raw, MBR and GPT disks; real file sizes in listings; read-only request; ext2 forced read-only; `e2fsck` clean; rejects non-ext disks |
| `host_lifetime` | Backend destroyed / device unplugged / I/O errors while files are open (use-after-free and flush-on-teardown), under ASan |

### Device mode (`device_*`)

`USBDeviceMSC.cpp` is compiled against a stub of `Adafruit_USBD_MSC`
(`tests/stub/Adafruit_TinyUSB.h`), so the block layer runs on the desktop.

| Suite | Covers |
|---|---|
| `device_ramdisk` | `USBMscRamDisk` FAT12/FAT16 formatting and `addFile()`, checked with the real `fsck.vfat` and `mtools`; bounds checking; `USBDeviceMSC` read/write plumbing incl. unaligned access (as the ESP32 core issues), read-only export |

### What the desktop tests cannot cover

TinyUSB enumeration and real transfers (host and device), `USBHostMSC`'s
blocking/timeout logic, `USBMscSdCard` on real cards, DMA alignment, the ESP32/RP2040 `fs::FS` adapters, the STM32
`USBMscFile`, and board bring-up. Those need hardware (or at least the
Arduino cores).

## Firmware compile tests

`test_compile.sh` builds every example with `arduino-cli`; see the header of
that script. Host examples (`USBHostMSC_*`) build for RP2040 (host stack),
ESP32-S3 (USB-OTG mode, core TinyUSB), STM32 and classic ESP32 (needs the
Adafruit TinyUSB Library). Device examples (`USBDeviceMSC_*`) build for
RP2040 (`usbstack=tinyusb`, the device stack) and ESP32-S3. The two modes
need different RP2040 USB stacks, hence the separate `rp2040` and
`rp2040dev` rows.
