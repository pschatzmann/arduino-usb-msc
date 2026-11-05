# Licensing and dependencies

usb-host-msc's own code (`USBHostMSC`, `USBMscPartition`, the filesystem
backend abstraction, the STM32 port) is [MIT](../LICENSE).

Filesystem support comes from two header-only libraries, installed as
normal Library Manager dependencies (`depends=` in `library.properties`):

- **[TinyFATFS](https://github.com/pschatzmann/TinyFATFS)** (FAT12/16/32,
  exFAT) -- MIT wrapper around ChaN's **FatFs**, which has its own
  permissive BSD-1-Clause-style license: source redistribution must keep
  the copyright notice, binary-only redistribution (compiled firmware)
  needs no attribution.
- **[TinyEXT4](https://github.com/pschatzmann/TinyEXT4)** (ext2/3/4) -- MIT.

Only the classes your sketch references are linked into the firmware.
Nothing in the dependency chain is copyleft.
