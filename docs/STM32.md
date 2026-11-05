# STM32 setup

TinyUSB host mode is off by default in Adafruit_TinyUSB_Arduino for
STM32 (`CFG_TUH_ENABLED` hardcoded to 0 upstream, marked "disable for
now" -- untested/unsupported there). Getting it working needs two
things:

1. **A small patch to your Adafruit_TinyUSB_Arduino checkout.** See
   `extras/adafruit_tinyusb_stm32_host.patch` (apply with `git apply` or
   `patch -p1` from that library's root) -- it makes `CFG_TUH_ENABLED`
   opt-in via a build flag instead of hardcoded off, adds the
   `CFG_TUH_MSC` etc. config block, keeps the existing device-mode ISR
   out of the build when host mode claims the same OTG peripheral
   instead, and fixes a guard bug where its CDC-host wrapper compiled
   (and failed, since STM32's `HardwareSerial` has no default
   constructor) whenever MSC host was enabled, regardless of whether CDC
   host was actually wanted. None of this changes behavior for
   device-mode-only sketches -- everything is behind
   `#ifndef CFG_TUH_ENABLED` / `!CFG_TUH_ENABLED`, off unless you pass the
   flag below.
2. **Build with `-DCFG_TUH_ENABLED=1`**, e.g. with arduino-cli:
   ```
   arduino-cli compile --fqbn STMicroelectronics:stm32:GenF4:pnum=BLACKPILL_F411CE \
     --build-property "compiler.c.extra_flags=-DCFG_TUH_ENABLED=1" \
     --build-property "compiler.cpp.extra_flags=-DCFG_TUH_ENABLED=1" \
     <sketch>
   ```

`Serial` on this core is `#define`d to TinyUSB's own device-mode CDC-ACM
object -- touching it would try to claim the same OTG peripheral for
device role. Use a real hardware UART (`Serial1`) for debug output
instead, same as on RP2040.

## Coverage

Only "plain OTG_FS on PA11/PA12" parts are covered -- STM32F4, and
STM32F7 parts other than the FS-less H7-style ones (see
`src/USBHostMSC_stm32.cpp`). **Not covered yet:** STM32H7 parts without a
separate OTG_FS instance, STM32WBA, STM32U5 (different AF/clock-enable
macros and/or HS PHY bring-up needed).

**STM32WB (classic, e.g. WB55) cannot do USB host at all** -- it has no
OTG peripheral, only an older device-only "FSDEV" controller. This is a
hardware limitation, not something a software patch can fix.

## VBUS

VBUS is not driven for you. Driving 5V out to the attached USB device is
board-specific (some boards have a GPIO-controlled power switch IC;
simple boards like BlackPill-style ones typically don't, and just need
5V present on the connector by the board's own design). Add the GPIO
drive in your sketch's `setup()` before `msc.begin()` if your board needs
it.

## Filesystems

STM32duino has no `fs::FS`, so `USBMscFatFS` is implemented independently
here -- but on the *same* TinyFATFS library ESP32/RP2040 use (via
`src/fs/USBMscFatBackend.cpp`'s `fatfs::IO` adapter). It hands back
`USBMscFile` (a `Stream`-based file/directory handle shaped like STM32SD's
own `File`, but deliberately not named `File`/`SDClass` so it can coexist
with STM32SD -- e.g. an actual SD card -- in the same sketch) instead of
`fs::File`. `USBMscFatFS::fs()` returns `*this`, so `fatFs.fs().open(...)`
still works the same way it does on ESP32/RP2040. See
[FILESYSTEMS.md](FILESYSTEMS.md) for the full API and
`examples/USBHostMSC_FatFS_ListFiles` (works unchanged across all three
platforms via a small type alias).

`USBMscExt4FS`/`USBMscExt4FSReadOnly` work the same way:
`usbmscfs::Ext4Backend` (the TinyEXT4 wrapper) doesn't depend on `fs::FS`
at all, so it's unchanged from ESP32/RP2040 -- only the top-level class
needed a STM32 branch, reusing the same `USBMscFile`/self-referential-`fs()`
pattern as `USBMscFatFS`. See
[FILESYSTEMS.md](FILESYSTEMS.md#ext4-read-only-vs-readwrite) for the
read-only-vs-read/write guidance.

`USBMscFSPool` works on STM32 (it deduces whatever `fs()` returns).
`USBMscAutoFS` is **not** available on STM32 yet: it needs
`USBMscFatFS::fs()` and `USBMscExt4FSBase::fs()` to return the same type
so it can pick between them at runtime, and on STM32 today those are two
unrelated self-reference types (`USBMscFatFS&` vs `USBMscExt4FSBase&`).
That header compiles to nothing on this architecture.
