# Testing

There are two kinds of tests.

## Desktop unit tests (Linux/macOS, CMake)

The hardware-independent part of the library -- `USBMscPartition` and the
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
| `partition` | MBR parsing, partition-type tables (incl. exFAT `0x07`), bounds checking, clamping of bogus partition entries |
| `fat` | FAT16/FAT32/exFAT, raw and MBR layouts: reading, listing, `w`/`a`/`w+`/`r+`/`a+`, mkdir/rename/remove/rmdir, multi-cluster files, `fsck` clean, remount; rejects non-FAT disks |
| `ext4` | ext4 on raw, MBR and GPT disks; real file sizes in listings; read-only request; ext2 forced read-only; `e2fsck` clean; rejects non-ext disks |
| `lifetime` | Backend destroyed / device unplugged / I/O errors while files are open (use-after-free and flush-on-teardown), under ASan |

### What the desktop tests cannot cover

TinyUSB enumeration and real transfers, `USBHostMSC`'s blocking/timeout
logic, DMA alignment, the ESP32/RP2040 `fs::FS` adapters, the STM32
`USBMscFile`, and board bring-up. Those need hardware (or at least the
Arduino cores).

## Firmware compile tests

`test_compile.sh` builds every example for RP2040, ESP32-S3 (USB-OTG mode,
core TinyUSB), STM32 and classic ESP32 (needs the Adafruit TinyUSB
Library) with `arduino-cli`; see the header of that script.
