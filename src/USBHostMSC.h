/*
 * usb-msc
 *
 * Thin Arduino wrapper around TinyUSB's host-mode Mass Storage Class
 * driver (tuh_msc_*), giving blocking, SCSI-block-level access to a USB
 * flash drive attached to the board's USB host port.
 *
 * Built on TinyUSB: the copy that is built into the arduino-esp32 core
 * (ESP32-S2/S3), the one bundled with the RP2040 core, or otherwise the
 * "Adafruit TinyUSB Library". USB Host mode must be selected for the
 * target board:
 *   - RP2040 (earlephilhower core): Tools > USB Stack >
 *     "Adafruit TinyUSB Host (native)"
 *   - ESP32-S2/S3 (arduino-esp32 core >= 3.3): uses the TinyUSB that is
 *     built into the core -- nothing else to install. Tools > USB Mode >
 *     "USB-OTG (TinyUSB)". Serial is then UART0 (see docs/SINGLE_PORT.md).
 *   - STM32 (STM32duino core, F4/F7 OTG_FS parts only -- see
 *     USBHostMSC_stm32.cpp): build with -DCFG_TUH_ENABLED=1, and apply the
 *     small Adafruit_TinyUSB_Arduino patch described in
 *     docs/STM32.md (host mode is off by default there and needs two local
 *     changes to that library to enable safely)
 *   - ESP32-P4 (write-back cache) and anything built with
 *     -DUSBHOSTMSC_USE_ADAFRUIT_TINYUSB use the "Adafruit TinyUSB Library"
 *     instead, which needs a small patch for its native host controller --
 *     see docs/ESP32_NATIVE.md.
 *   - MCUs with no USB peripheral at all (classic ESP32, ESP32-C3/C6/H2,
 *     SAMD, nRF52, ...) can still act as a USB host via an external
 *     MAX3421E host-shield chip wired over SPI -- see docs/MAX3421E.md.
 */
#pragma once

// Which TinyUSB stack do we sit on?
//  - ESP32-S2/S3 (arduino-esp32 >= 3.3): the TinyUSB host stack that is
//    already compiled into the core, started with USBHost.begin().
//  - everything else (RP2040 core, STM32, MAX3421E boards, ESP32-P4, or
//    -DUSBHOSTMSC_USE_ADAFRUIT_TINYUSB): the "Adafruit TinyUSB Library".
// ESP32-P4 stays on the Adafruit path: its internal RAM sits behind a
// write-back data cache, so DMA buffers need cache maintenance that is not
// implemented here. (On S2/S3 the internal-RAM bounce buffer is uncached.)
#if defined(ARDUINO_ARCH_ESP32) && !defined(USBHOSTMSC_USE_ADAFRUIT_TINYUSB)
#include "sdkconfig.h"
#include "soc/soc_caps.h"
#if SOC_USB_OTG_SUPPORTED && !CONFIG_IDF_TARGET_ESP32P4 &&                     \
    __has_include(<USBHost.h>)
#define USBHOSTMSC_ESP32_CORE 1
#endif
#endif
#ifndef USBHOSTMSC_ESP32_CORE
#define USBHOSTMSC_ESP32_CORE 0
#endif

#if USBHOSTMSC_ESP32_CORE
#include <Arduino.h>
#include <USBHost.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#if defined(ARDUINO_USB_MODE) && ARDUINO_USB_MODE == 1
#warning \
    "usb-msc: with Tools > USB Mode > 'Hardware CDC and JTAG' the core " \
    "does not run TinyUSB, so USB host will not work. Select 'USB-OTG (TinyUSB)'."
#endif
#else
#if !__has_include(<Adafruit_TinyUSB.h>)
#error \
    "usb-msc needs the 'Adafruit TinyUSB Library' on this board " \
    "(install it from the Library Manager). It is only NOT needed on " \
    "ESP32-S2/S3 (arduino-esp32 core TinyUSB) and RP2040 (bundled with core)."
#endif
#include <Adafruit_TinyUSB.h>
#endif

#include "USBMscFSBackend.h"

// RP2040 (earlephilhower core) only builds the TinyUSB host driver when the
// "Adafruit TinyUSB Host (native)" USB Stack menu entry is selected; that
// menu entry is what defines USE_TINYUSB_HOST. ESP32 has no such menu (host
// mode there is gated purely by CFG_TUH_ENABLED/CFG_TUH_MSC, checked below).
#if defined(ARDUINO_ARCH_RP2040) && !defined(USE_TINYUSB_HOST)
#error \
    "usb-msc requires USB Host mode. On RP2040 select " \
    "Tools > USB Stack > \"Adafruit TinyUSB Host (native)\"."
#endif

#if !(CFG_TUH_ENABLED && CFG_TUH_MSC)
#if defined(ARDUINO_ARCH_STM32)
#error \
    "usb-msc requires CFG_TUH_ENABLED and CFG_TUH_MSC to be set to 1. " \
    "On STM32 this needs a build flag (-DCFG_TUH_ENABLED=1) plus a small " \
    "patch to Adafruit_TinyUSB_Arduino -- see docs/STM32.md."
#elif USBHOSTMSC_ESP32_CORE
#error \
    "usb-msc requires TinyUSB host mode. On ESP32-S2/S3 select " \
    "Tools > USB Mode > \"USB-OTG (TinyUSB)\"."
#else
#error \
    "usb-msc requires CFG_TUH_ENABLED and CFG_TUH_MSC to be set to 1 " \
    "(Adafruit TinyUSB Library with host mode enabled -- see " \
    "docs/BOARDS.md and docs/ESP32_NATIVE.md)."
#endif
#endif

// Root hub port used to bring up the host controller. Native (non-MAX3421)
// ports only expose one usable value; override with a build flag if needed.
// MAX3421E boards use rhport 1 by convention (see Adafruit's own examples
// and docs/MAX3421E.md) -- rhport 0 there is reserved for a chip's own
// native USB peripheral, if it has one and it's used for device mode.
#ifndef USB_HOST_MSC_RHPORT
#if defined(BOARD_TUH_RHPORT)
#define USB_HOST_MSC_RHPORT BOARD_TUH_RHPORT
#elif defined(CFG_TUH_MAX3421) && CFG_TUH_MAX3421
#define USB_HOST_MSC_RHPORT 1
#else
#define USB_HOST_MSC_RHPORT 0
#endif
#endif

// Upper bound for one blocking SCSI command (inquiry/read/write). If the
// drive is unplugged or stops answering mid-command, the call returns
// false after this long instead of spinning forever. Override with a build
// flag for very slow media.
#ifndef USB_HOST_MSC_IO_TIMEOUT_MS
#define USB_HOST_MSC_IO_TIMEOUT_MS 10000
#endif

#if USBHOSTMSC_ESP32_CORE
// ESP32 core path: SCSI transfers go through a DMA-capable bounce buffer in
// chunks of at most this many blocks. 1 is the same conservative value the
// core's own USBHostMSC uses (some controllers/sticks wedge on longer
// transfers); raise it with a build flag if your drive copes, for speed.
#ifndef USB_HOST_MSC_MAX_BLOCKS_PER_XFER
#define USB_HOST_MSC_MAX_BLOCKS_PER_XFER 1
#endif
// Pause after each written chunk (ms); some sticks wedge when hammered.
#ifndef USB_HOST_MSC_WRITE_PACING_MS
#define USB_HOST_MSC_WRITE_PACING_MS 1
#endif
#endif

/// SCSI Inquiry response, trimmed to plain C-strings for convenience.
struct MSCInquiry {
  char vendor_id[9];   // 8 chars + NUL
  char product_id[17]; // 16 chars + NUL
  char product_rev[5]; // 4 chars + NUL
};

/**
 * @brief Blocking wrapper around TinyUSB's USB Host Mass Storage (tuh_msc_*)
 * class driver. One instance manages the single on-board host controller;
 * multiple mass storage devices (e.g. behind a hub) are addressed by their
 * TinyUSB device address.
 */
class USBHostMSC {
public:
  using MountCallback = void (*)(uint8_t dev_addr);

  /// fsBackend, if given (e.g. a USBMscFatFS, USBMscExt4FS or
  /// USBMscAutoFS), is auto-mounted/unmounted as devices come and go,
  /// before onMount()/onUnmount() callbacks run.
  explicit USBHostMSC(USBMscFSBackend *fsBackend = nullptr);

#if defined(CFG_TUH_MAX3421) && CFG_TUH_MAX3421
  /// For MAX3421E (external host-shield chip) boards -- e.g. classic
  /// ESP32, which has no USB peripheral of its own -- construct the
  /// Adafruit_USBH_Host with your SPI bus + CS + INT pins and pass it
  /// here instead of calling the plain begin(). See docs/MAX3421E.md.
  explicit USBHostMSC(Adafruit_USBH_Host &host,
                       USBMscFSBackend *fsBackend = nullptr);
#endif

  /// Bring up the TinyUSB host controller. Call once from setup().
  /// (rhport is ignored on the ESP32 core path, where USBHost.begin() picks
  /// the port.)
  bool begin(uint8_t rhport = USB_HOST_MSC_RHPORT);

  /// Pump the TinyUSB host stack. Call as often as possible from loop().
  /// On ESP32 the stack runs on its own task; task() then delivers pending
  /// mount/unmount events -- filesystem mounting and your onMount() /
  /// onUnmount() callbacks run from here, in loop() context.
  void task();

  /// True once dev_addr has enumerated a Mass Storage interface.
  bool isMounted(uint8_t dev_addr) const;

  /// Number of currently mounted Mass Storage devices.
  uint8_t mountedDeviceCount() const;

  /// dev_addr of the index-th mounted device (0-based), or 0 if out of range.
  uint8_t deviceAddress(uint8_t index) const;

  /// True if the device finished enumeration and isn't busy transferring.
  /// TinyUSB tracks readiness per device, not per LUN, so `lun` is accepted
  /// for API symmetry but ignored.
  bool ready(uint8_t dev_addr, uint8_t lun = 0) const;

  uint8_t maxLun(uint8_t dev_addr) const;
  uint32_t blockCount(uint8_t dev_addr, uint8_t lun = 0) const;
  uint32_t blockSize(uint8_t dev_addr, uint8_t lun = 0) const;
  uint64_t capacity(uint8_t dev_addr, uint8_t lun = 0) const;

  /// Blocking SCSI Inquiry; fills vendor/product/revision strings.
  bool inquiry(uint8_t dev_addr, uint8_t lun, MSCInquiry &out);

  /// Blocking read of block_count blocks starting at lba into buffer.
  /// buffer must be at least block_count * blockSize() bytes.
  bool readBlocks(uint8_t dev_addr, uint32_t lba, void *buffer,
                   uint16_t block_count, uint8_t lun = 0);

  /// Blocking write of block_count blocks starting at lba from buffer.
  bool writeBlocks(uint8_t dev_addr, uint32_t lba, const void *buffer,
                    uint16_t block_count, uint8_t lun = 0);

  /// Invoked (from task()) when a Mass Storage device mounts.
  void onMount(MountCallback cb);
  /// Invoked (from task()) when a Mass Storage device unmounts.
  void onUnmount(MountCallback cb);

  //------------- internal use only -------------//
  void _handleMount(uint8_t dev_addr);
  void _handleUnmount(uint8_t dev_addr);
  bool _handleIoComplete(uint8_t dev_addr,
                          const tuh_msc_complete_data_t *cb_data);
#if USBHOSTMSC_ESP32_CORE
  /// Called from TinyUSB's worker task: queues the event for task().
  void _postEvent(uint8_t dev_addr, bool mount);
#endif

  static USBHostMSC *_instance;

private:
  static constexpr uint8_t kMaxDevices = CFG_TUH_DEVICE_MAX + 1;

  bool _mounted[kMaxDevices];
  MountCallback _on_mount;
  MountCallback _on_unmount;
  USBMscFSBackend *_fs_backend;

  volatile bool _busy;
  volatile bool _io_ok;
  volatile uintptr_t _tag = 0; // completion we wait for; 0 = none (stale ones are dropped)
  uintptr_t _tag_seq = 0;

#if defined(CFG_TUH_MAX3421) && CFG_TUH_MAX3421
  Adafruit_USBH_Host *_max3421_host = nullptr;
#endif

  enum class ScsiOp : uint8_t { Inquiry, Read, Write };

  bool _devPresent(uint8_t dev_addr) const;
  uintptr_t _newTag();
  bool _scsi(ScsiOp op, uint8_t dev_addr, uint8_t lun, void *buf,
             uint32_t lba, uint16_t count);
  bool _waitForIo(uint8_t dev_addr);

#if USBHOSTMSC_ESP32_CORE
  struct Event {
    uint8_t dev_addr;
    bool mount;
  };
  bool _xfer(bool is_write, uint8_t dev_addr, uint8_t lun, uint32_t lba,
             void *buffer, uint32_t blocks);
  uint8_t *_bounceBuffer(size_t bytes);
  void _abandonBounce();

  QueueHandle_t _events = nullptr;
  SemaphoreHandle_t _done_sem = nullptr;
  SemaphoreHandle_t _io_mutex = nullptr;
  uint8_t *_bounce = nullptr;
  size_t _bounce_size = 0;
  volatile bool _present[kMaxDevices]; // device is on the bus (set by TinyUSB's task)
#endif
};
