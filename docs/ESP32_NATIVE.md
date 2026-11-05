# ESP32-S2/S3/P4: native USB host

Chips with a native USB-OTG peripheral (DWC2) can be a USB host without
extra hardware. Which TinyUSB this library uses depends on the chip:

| Chip | TinyUSB | Needs |
|---|---|---|
| **ESP32-S2, ESP32-S3** | the TinyUSB **built into the arduino-esp32 core** (>= 3.3) | nothing: no extra library, no patch |
| **ESP32-P4** | the *Adafruit TinyUSB Library* | a small patch, see [below](#the-adafruit-path-esp32-p4-and-opt-in) |
| any ESP32 + MAX3421E chip | the *Adafruit TinyUSB Library* | see [MAX3421E.md](MAX3421E.md) |

## S2/S3: using the core's TinyUSB (default)

1. Install the **esp32 core 3.3 or newer** (3.3.12 was used for development).
2. Select **Tools > USB Mode > "USB-OTG (TinyUSB)"**.
3. Include `USBHostMSC.h` as usual. The library detects the core's
   `USBHost` class and starts it with `USBHost.begin()`.

Things that differ from the other boards:

- **`Serial` is UART0.** The native port is the host port, so
  "USB CDC On Boot" stays disabled and `Serial` goes to GPIO43 (TX) /
  GPIO44 (RX) -- connect a USB-UART adapter. See
  [SINGLE_PORT.md](SINGLE_PORT.md).
- **Mounting happens in `loop()`.** The core runs the TinyUSB host stack
  on its own FreeRTOS task. Blocking in that task's callbacks would stall
  the very task that completes the I/O, so the library only queues
  mount/unmount events there and `msc.task()` delivers them. That is where
  filesystems get mounted and your `onMount()`/`onUnmount()` callbacks
  run. **You must call `msc.task()` from `loop()`** (as in all examples).
- **Transfers go through a DMA-capable bounce buffer** (internal RAM,
  aligned), in chunks of `USB_HOST_MSC_MAX_BLOCKS_PER_XFER` blocks (default
  1, the same conservative value as the core's own `USBHostMSC`; some
  sticks wedge on longer transfers). Raise it with a build flag
  (`-DUSB_HOST_MSC_MAX_BLOCKS_PER_XFER=8`) if your drive copes and you want
  more speed. Writes pause `USB_HOST_MSC_WRITE_PACING_MS` (default 1 ms)
  after each chunk.
- A command that times out or whose device is unplugged abandons its
  bounce buffer (a small deliberate leak) because the controller might
  still be writing into it.
- A stale completion (from a command that timed out) is recognised by a
  tag and ignored, instead of satisfying the next command's wait.

### Name clash with the core's own `USBHostMSC`

The core also ships a `USBHostMSC` (a single-device block API) and
`USBMSCFS` (FAT on a mount point such as `/usb`) in its `USB` library.
Both are fine on their own, but this library's header has the same name
and the same class/object name, so **don't use both in one sketch**.
The core's `USBMSCFS` is a good choice when all you need is FAT on one
drive; this library adds ext2/3/4, several drives behind a hub, a
common API across RP2040/ESP32/STM32 and explicit partition handling.

### Not verified yet

The core path compiles and links for the ESP32-S3; it has not been run on
hardware yet. Please report problems, in particular drives that need
`USB_HOST_MSC_MAX_BLOCKS_PER_XFER=1` or that fail to enumerate.

## The Adafruit path: ESP32-P4 and opt-in

ESP32-P4 stays on the *Adafruit TinyUSB Library* (its internal RAM sits
behind a write-back cache and the bounce buffer here does not do cache
maintenance). You can also force this path on S2/S3 with
`-DUSBHOSTMSC_USE_ADAFRUIT_TINYUSB` (for example to use a MAX3421E host
shield there). In this mode use **Tools > USB Mode > "Hardware CDC and
JTAG"** so the native USB-OTG pins are free to act as a host port.

Adafruit_TinyUSB_Arduino's own `tusb_config_esp32.h` unconditionally set
`CFG_TUH_MAX3421 1` for **every** ESP32 variant, including S2/S3/P4 --
chips that have a native USB-OTG (DWC2) peripheral. A plain build
therefore silently linked and ran the MAX3421E host-shield driver instead
of the native OTG controller: no compile error, no runtime error either
since nothing calls `Adafruit_USBH_Host`'s SPI/pin setup. This was only
ever caught by disassembling the linked `hcd_init()` and confirming
which driver it actually calls into; compiling clean is not enough to
prove it is fixed.

Fix: **a small patch to your Adafruit_TinyUSB_Arduino checkout**, in
`extras/adafruit_tinyusb_esp32_native_host.patch` (apply with `git apply`
or `patch -p1` from that library's root). It makes `CFG_TUH_MAX3421`
chip-conditional:

- ESP32-S2/S3/P4 default to `CFG_TUH_MAX3421 0` -- the native DWC2 host.
- Every other ESP32 variant keeps `CFG_TUH_MAX3421 1` (see
  [MAX3421E.md](MAX3421E.md)).
- Either can still be overridden with `-DCFG_TUH_MAX3421=0/1`.

How it was verified: in an ESP32-S3 build on this path, disassembling the
linked `hcd_init()` showed `tuh_max3421_*` calls before the patch and
`dwc2_core_is_highspeed_phy` / `dwc2_core_init` (the native DWC2 driver)
after it. Classic ESP32 was rechecked to confirm it still defaults to
MAX3421E.
