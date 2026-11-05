# One USB port: drive, power and Serial

In host mode the board's native USB port talks to the **drive**, so it can
no longer be used for the PC connection. That raises three questions:

1. How does the drive get its 5 V?
2. How is the board powered?
3. Where does `Serial` go (and how do I flash)?

> The wiring below is a guide, not a tested reference design. Check your
> board's schematic before connecting power, and never join two 5 V
> sources without a diode or a proper power-path chip.

## The easy way: a board with two USB connectors

Pick a board that separates the roles:

| Role | Connector |
|---|---|
| Power, flashing, `Serial` | the USB-to-UART port (e.g. "UART" on an ESP32-S3-DevKitC-1) |
| Drive | the native USB / OTG port (via a USB-OTG adapter or a Type-A socket) |

Other examples: Adafruit Feather RP2040 USB Host (Type-A host socket plus a
Type-C port for power and programming), ESP32-S3 boards with a Type-A host
socket. Two things to check:

- **Does 5 V reach the OTG connector's VBUS?** On some boards the
  second connector is not connected to 5 V by default (a solder jumper or
  pad may have to be bridged). With no 5 V on VBUS the drive stays dead and
  nothing enumerates.
- A flash drive draws roughly 100-500 mA. The supply behind the UART port
  must be able to deliver that on top of the board itself.

## Boards with a single USB port

Only the native USB port exists, and it belongs to the drive. Split the
jobs:

```
 5 V supply ──┬─────────────► board 5V (VIN) pin
 (phone       │
  charger,    ├─────────────► USB-A socket pin 1  (VBUS)
  power bank) │
 GND ─────────┼─────────────► board GND
              └─────────────► USB-A socket pin 4  (GND)
                             USB-A socket pin 2  (D-)  ◄── board USB D-
                             USB-A socket pin 3  (D+)  ◄── board USB D+

 USB-UART adapter (3.3 V logic): its RX ◄── board TX, its TX ──► board RX, GND common
```

- **Drive:** wire a USB-A receptacle (breakout board) to the chip's USB
  data pins and a **5 V** supply. D+/D- pins:
  - ESP32-S2/S3: D- = GPIO19, D+ = GPIO20
  - RP2040: the USB pads on the board (or use a board with a host socket)
  - STM32F4/F7: D- = PA11, D+ = PA12
- **Power:** feed 5 V into the board's 5V/VIN pin from a charger or power
  bank, and use the same supply for the socket's VBUS. Keep wires short
  and add a bulk capacitor (about 100 µF) near the socket so the drive's
  inrush doesn't brown out the board.
- **Do not also plug the board's own USB connector into a PC** while the
  socket is wired to the same pins: both would drive D+/D- and VBUS.
- **Alternative to a bare socket:** a USB-OTG "Y" cable or adapter with a
  separate power input. Plug its host end into the board's USB connector,
  the drive into its socket, and power into its power input. Whether the
  extra power reaches the board's 5 V depends on the board's connector
  wiring, so verify with a multimeter first.

## Using a powered OTG hub

A **self-powered** hub (one with its own 5 V input) plugged into the
board's USB port is the simplest way to get the drive's power right:

- The hub supplies VBUS and the drive's current, so the board's connector
  doesn't have to. This also helps boards whose OTG connector carries no
  5 V.
- You can plug in several drives; use `USBMscFSPool` to mount them all
  (see [MULTIPLE_DEVICES.md](MULTIPLE_DEVICES.md)).
- ESP32-S2/S3 and RP2040 host at full speed (12 Mbit/s). A plain USB 2.0
  hub works; a USB 3 hub should fall back to USB 2.

What a hub **does not** do:

- **It doesn't give you a PC connection.** A hub has one upstream port and
  that one belongs to the board; a PC is a second host and cannot sit
  downstream of it. On these chips the OTG controller also shares its USB
  PHY with the USB-serial/JTAG function (and, in "USB-OTG (TinyUSB)" mode,
  with the TinyUSB device stack), so the board can't be a host and a
  PC-visible device at the same time. `Serial` still has to go over a
  UART adapter or Wi-Fi (next section). A USB-to-serial dongle on the hub
  doesn't help either: its other side is UART pins, not your PC.
- **It doesn't reliably power the board.** The USB spec forbids a hub
  from feeding power upstream, though some cheap hubs do. Give the board
  its own 5 V input.

## Getting `Serial` back

`Serial` normally means the USB port, which is now busy. Use a hardware
UART instead and a USB-UART adapter (CP2102, CH340, FT232, ...) between
the UART pins and your PC:

- **ESP32-S2/S3:** set **Tools > USB Mode > USB-OTG (TinyUSB)** (required
  for the host stack) and leave **USB CDC On Boot > Disabled**. `Serial`
  is then UART0: TX = GPIO43, RX = GPIO44. Or open any UART explicitly:
  `Serial1.begin(115200, SERIAL_8N1, /*rx=*/44, /*tx=*/43);`
- **RP2040:** `Serial1` on GP0 (TX) / GP1 (RX); the examples already use
  `Serial1` there.
- **STM32:** `Serial1` on its UART pins; the examples already use it.

Connect adapter RX to board TX, adapter TX to board RX, and the grounds.
Keep the adapter at **3.3 V logic**. If the adapter powers the board too
(some have a 5 V pin), don't connect that pin as well as the separate
supply.

No UART adapter handy? Other ways to see output:

- Log over Wi-Fi (UDP/telnet) on ESP32.
- Write results to a file on the drive itself (the filesystem examples
  already show how) and read it on your PC afterwards.
- Blink an LED / drive a display for pass/fail.

## Flashing

- **ESP32-S3 over the native port:** with the drive unplugged, hold
  BOOT, tap RESET, release BOOT. The chip appears as a serial device and
  you can upload. Afterwards switch to the drive wiring. Host mode
  disables the USB serial/JTAG function, so repeat the BOOT+RESET steps for
  each re-flash.
- **ESP32-S3 over UART:** the adapter's DTR/RTS lines on EN and GPIO0
  give automatic reset into download mode; otherwise use the same
  BOOT+RESET dance. This avoids touching the drive wiring.
- **RP2040:** hold BOOTSEL while powering up and drag the UF2 (or let the
  IDE upload) -- the host port isn't involved.
- **STM32:** flash over the ST-Link/SWD header, or the UART bootloader.

## Checklist when nothing happens

1. Meter between the drive socket's VBUS and GND: about 5 V?
2. D+/D- not swapped, and the drive not behind a hub that needs more
   current than the supply gives.
3. Board configured per [BOARDS.md](BOARDS.md) (host mode, patches).
4. `Serial` really points at the UART you've wired up.
5. Try a different drive: some drives need more current than others, or
   take a long time to spin up.
