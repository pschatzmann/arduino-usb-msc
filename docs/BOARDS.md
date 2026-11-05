# Supported targets and board configuration

This library has two modes: **host** (`USBHostMSC`: the board reads a USB
stick) and **device** (`USBDeviceMSC`: the board is a USB stick). They use
the board's USB controller in opposite roles, so a board picks one per
firmware, unless it has a second controller. The tables and sections below
are about **host mode**; device mode is covered in
[Device mode](#device-mode) at the end.

Which TinyUSB is used: ESP32-S2/S3 -> the one built into the arduino-esp32
core; RP2040 -> the one bundled with the RP2040 core; everything else
(STM32, MAX3421E boards, ESP32-P4) -> the *Adafruit TinyUSB Library* from
the Library Manager.

| Board                   | Core                  | Host controller                  |
|-------------------------|-----------------------|-----------------------------------|
| RP2040                  | earlephilhower `rp2040:rp2040` | native dual-role USB controller |
| ESP32-S2/S3             | `esp32:esp32` >= 3.3  | native USB-OTG (DWC2) controller, using the core's built-in TinyUSB -- nothing extra to install, see [ESP32_NATIVE.md](ESP32_NATIVE.md) |
| ESP32-P4                | `esp32:esp32`         | native USB-OTG, via the Adafruit TinyUSB Library + a small patch (not yet verified), see [ESP32_NATIVE.md](ESP32_NATIVE.md) |
| STM32F4 / STM32F7 (OTG_FS parts) | STM32duino `STMicroelectronics:stm32` | native OTG_FS (DWC2) controller, PA11/PA12 |
| Any board with no native USB (classic ESP32, ESP32-C3/C6/H2, SAMD, nRF52, ...) | matching Arduino core | external MAX3421E host-shield chip over SPI, see [MAX3421E.md](MAX3421E.md) |

**STM32WB (classic, e.g. WB55) cannot do USB host at all** -- hardware
limitation, see [STM32.md](STM32.md). **ESP8266 cannot do USB host
either** -- no TinyUSB port exists for that architecture, MAX3421E
included, see [MAX3421E.md](MAX3421E.md#supported-boards).

## Board configuration

- **RP2040**: Tools > USB Stack > **"Adafruit TinyUSB Host (native)"**.
  Wire the USB connector's D+/D-/5V/GND to the RP2040 (many boards, e.g.
  "Adafruit Feather RP2040 USB Host", do this on-board). Since the native
  USB port becomes the host port, use `Serial1` for debug output.

- **ESP32-S2/S3**: arduino-esp32 **3.3 or newer**, Tools > USB Mode >
  **"USB-OTG (TinyUSB)"**. The TinyUSB host stack built into the core is
  used, so no extra library and no patch are needed. The native USB port
  becomes the host port, so `Serial` is UART0 (GPIO43 TX / GPIO44 RX) --
  see **[SINGLE_PORT.md](SINGLE_PORT.md)** for power and Serial wiring and
  **[ESP32_NATIVE.md](ESP32_NATIVE.md)** for details.

- **ESP32-P4**: still on the Adafruit TinyUSB Library, which needs a small
  patch -- see **[ESP32_NATIVE.md](ESP32_NATIVE.md)**.

- **STM32**: needs the *Adafruit TinyUSB Library* with a small patch to
  your checkout plus a build flag -- see **[STM32.md](STM32.md)** for the full
  setup, board coverage, and VBUS notes.

- **Classic ESP32 / other boards with no native USB**: install the
  *Adafruit TinyUSB Library* and add an external
  MAX3421E host-shield chip over SPI -- see **[MAX3421E.md](MAX3421E.md)**
  for wiring, board coverage, and API differences (a small constructor
  change).

Boards with a single USB port: see [SINGLE_PORT.md](SINGLE_PORT.md) for
powering the board, feeding the drive and keeping `Serial`.

The patches themselves live in [../extras/](../extras/).

## Device mode

`USBDeviceMSC` needs the USB **device** stack and no patches:

| Board | Core | Setting | Uses |
|---|---|---|---|
| ESP32-S2/S3 | `esp32:esp32` >= 3.x | Tools > USB Mode > **"USB-OTG (TinyUSB)"** | the core's `USBMSC` (nothing to install) |
| RP2040 | earlephilhower `rp2040:rp2040` | Tools > USB Stack > **"Adafruit TinyUSB"** (not "...Host (native)") | `Adafruit_USBD_MSC` |
| other Adafruit TinyUSB boards (SAMD21/51, nRF52, STM32, ...) | their usual core | USB Stack: TinyUSB | `Adafruit_USBD_MSC`; compiles, not tested here |

Notes:

- **Host and device at the same time** only works with two controllers: e.g.
  the native port as device plus a MAX3421E chip ([MAX3421E.md](MAX3421E.md))
  as host. On RP2040 the "Adafruit TinyUSB Host (native)" stack has no
  device side; the library's host code is compiled out when the device
  stack is selected and vice versa (see `src/USBMsc_config.h`; define
  `USBHOSTMSC_DISABLE` / `USBDEVMSC_DISABLE` to force one side off on
  boards where this cannot be detected).
- **ESP32-S2/S3** use one USB-OTG controller for either role; the mode is
  chosen by the sketch, `USBDeviceMSC::begin()` starts `USB`.
- With device mode the native USB port is the connection to the PC, so
  `Serial` over USB (`CDC`) and flashing work as usual, unlike in host
  mode ([SINGLE_PORT.md](SINGLE_PORT.md) is host-only).
- Details of the API: [DEVICE_API.md](DEVICE_API.md).
