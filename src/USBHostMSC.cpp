#include "USBMsc_config.h"
#if USBHOSTMSC_COMPILE
#include "USBHostMSC.h"

#include <string.h>

#if USBHOSTMSC_ESP32_CORE
#include "esp_heap_caps.h"
#endif

#if defined(ARDUINO_ARCH_STM32)
// USBHostMSC_stm32.cpp: RCC/GPIO/NVIC bring-up TinyUSB's dwc2 driver
// expects to already be done before tusb_init() runs.
void usbhostmsc_stm32_hw_init(void);
#endif

USBHostMSC *USBHostMSC::_instance = nullptr;

USBHostMSC::USBHostMSC(USBMscFSBackend *fsBackend)
    : _on_mount(nullptr), _on_unmount(nullptr), _fs_backend(fsBackend),
      _busy(false), _io_ok(false) {
  for (uint8_t i = 0; i < kMaxDevices; i++) {
    _mounted[i] = false;
#if USBHOSTMSC_ESP32_CORE
    _present[i] = false;
#endif
  }
  _instance = this;
}

#if defined(CFG_TUH_MAX3421) && CFG_TUH_MAX3421
USBHostMSC::USBHostMSC(Adafruit_USBH_Host &host, USBMscFSBackend *fsBackend)
    : _on_mount(nullptr), _on_unmount(nullptr), _fs_backend(fsBackend),
      _busy(false), _io_ok(false), _max3421_host(&host) {
  for (uint8_t i = 0; i < kMaxDevices; i++) {
    _mounted[i] = false;
  }
  _instance = this;
}
#endif

bool USBHostMSC::begin(uint8_t rhport) {
  _instance = this;

#if USBHOSTMSC_ESP32_CORE
  (void)rhport; // the core's USBHost picks the root hub port
  // Everything TinyUSB's worker task touches must exist before it starts.
  if (!_events) {
    _events = xQueueCreate(16, sizeof(Event));
  }
  if (!_done_sem) {
    _done_sem = xSemaphoreCreateBinary();
  }
  if (!_io_mutex) {
    _io_mutex = xSemaphoreCreateRecursiveMutex();
  }
  if (!_events || !_done_sem || !_io_mutex) {
    return false;
  }
  return USBHost.begin();
#else

#if defined(ARDUINO_ARCH_STM32)
  usbhostmsc_stm32_hw_init();
#endif

#if defined(CFG_TUH_MAX3421) && CFG_TUH_MAX3421
  // Adafruit_USBH_Host::begin() does the SPI/CS/INT bring-up a MAX3421E
  // chip needs before it can call tusb_init() itself; skip our own call
  // below in that case.
  if (_max3421_host) {
    return _max3421_host->begin(rhport);
  }
#endif

  const tusb_rhport_init_t host_init = {
      .role = TUSB_ROLE_HOST,
      .speed = TUH_OPT_HIGH_SPEED ? TUSB_SPEED_HIGH : TUSB_SPEED_FULL,
  };
  return tusb_init(rhport, &host_init);
#endif
}

void USBHostMSC::task() {
#if USBHOSTMSC_ESP32_CORE
  USBHost.task();
  // Mount/unmount events are queued by TinyUSB's worker task; handle them
  // here, in loop() context. (Doing blocking filesystem I/O inside the
  // worker's callback would stall the very task that completes the I/O.)
  Event ev;
  while (_events && xQueueReceive(_events, &ev, 0) == pdTRUE) {
    if (ev.mount) {
      if (_present[ev.dev_addr]) { // still plugged in?
        _handleMount(ev.dev_addr);
      }
    } else {
      _handleUnmount(ev.dev_addr);
    }
  }
#else
  tuh_task();
#endif
}

bool USBHostMSC::isMounted(uint8_t dev_addr) const {
  return dev_addr < kMaxDevices && _mounted[dev_addr];
}

uint8_t USBHostMSC::mountedDeviceCount() const {
  uint8_t count = 0;
  for (uint8_t i = 0; i < kMaxDevices; i++) {
    if (_mounted[i]) {
      count++;
    }
  }
  return count;
}

uint8_t USBHostMSC::deviceAddress(uint8_t index) const {
  uint8_t count = 0;
  for (uint8_t i = 0; i < kMaxDevices; i++) {
    if (_mounted[i]) {
      if (count == index) {
        return i;
      }
      count++;
    }
  }
  return 0;
}

bool USBHostMSC::ready(uint8_t dev_addr, uint8_t lun) const {
  (void)lun;
  return tuh_msc_ready(dev_addr);
}

uint8_t USBHostMSC::maxLun(uint8_t dev_addr) const {
  return tuh_msc_get_maxlun(dev_addr);
}

uint32_t USBHostMSC::blockCount(uint8_t dev_addr, uint8_t lun) const {
  return tuh_msc_get_block_count(dev_addr, lun);
}

uint32_t USBHostMSC::blockSize(uint8_t dev_addr, uint8_t lun) const {
  return tuh_msc_get_block_size(dev_addr, lun);
}

uint64_t USBHostMSC::capacity(uint8_t dev_addr, uint8_t lun) const {
  return (uint64_t)blockCount(dev_addr, lun) * blockSize(dev_addr, lun);
}

//--------------------------------------------------------------------+
// Blocking SCSI plumbing
//--------------------------------------------------------------------+

bool USBHostMSC::_devPresent(uint8_t dev_addr) const {
  if (dev_addr >= kMaxDevices) {
    return false;
  }
#if USBHOSTMSC_ESP32_CORE
  return _present[dev_addr]; // updated at once by TinyUSB's own task
#else
  return _mounted[dev_addr]; // cleared by the umount callback inside tuh_task()
#endif
}

uintptr_t USBHostMSC::_newTag() {
  if (++_tag_seq == 0) {
    _tag_seq = 1; // 0 means "not waiting for anything"
  }
  _tag = _tag_seq;
  return _tag;
}

bool USBHostMSC::_waitForIo(uint8_t dev_addr) {
  const uint32_t start = millis();
  while (_busy) {
#if USBHOSTMSC_ESP32_CORE
    if (USBHost.tuhBackgroundActive()) {
      // The worker task drives the stack; sleep until it signals completion.
      xSemaphoreTake(_done_sem, pdMS_TO_TICKS(5));
    } else {
      tuh_task_ext(0, false); // no worker: pump without blocking
      yield();
    }
#else
    if (tuh_task_event_ready()) {
      tuh_task();
    }
    yield(); // keep the watchdog and background tasks fed
#endif
    // Unplugged mid-command: TinyUSB never delivers a completion for it.
    if (!_devPresent(dev_addr)) {
      _busy = false;
      _tag = 0; // drop any late completion
      return false;
    }
    if ((uint32_t)(millis() - start) > USB_HOST_MSC_IO_TIMEOUT_MS) {
      _busy = false;
      _tag = 0;
      return false;
    }
  }
  return _io_ok;
}

// Trampoline shared by every blocking SCSI call: TinyUSB invokes this from
// its own task context once the command's CSW has been received.
static bool msc_complete_cb(uint8_t dev_addr,
                             const tuh_msc_complete_data_t *cb_data) {
  if (USBHostMSC::_instance) {
    USBHostMSC::_instance->_handleIoComplete(dev_addr, cb_data);
  }
  return true;
}

bool USBHostMSC::_handleIoComplete(uint8_t dev_addr,
                                    const tuh_msc_complete_data_t *cb_data) {
  (void)dev_addr;
  // A command we gave up on can still complete later; it must not satisfy
  // the wait of whatever command came after it.
  if (cb_data->user_arg == 0 || cb_data->user_arg != _tag) {
    return true;
  }
  _io_ok = cb_data->csw->status == MSC_CSW_STATUS_PASSED;
  _busy = false;
#if USBHOSTMSC_ESP32_CORE
  xSemaphoreGive(_done_sem);
#endif
  return true;
}

bool USBHostMSC::_scsi(ScsiOp op, uint8_t dev_addr, uint8_t lun, void *buf,
                       uint32_t lba, uint16_t count) {
#if USBHOSTMSC_ESP32_CORE
  while (xSemaphoreTake(_done_sem, 0) == pdTRUE) {
  } // drop a stale wake-up
#endif
  _busy = true;
  _io_ok = false;
  const uintptr_t tag = _newTag();
  bool submitted = false;
  switch (op) {
  case ScsiOp::Inquiry:
    submitted = tuh_msc_inquiry(dev_addr, lun, (scsi_inquiry_resp_t *)buf,
                                msc_complete_cb, tag);
    break;
  case ScsiOp::Read:
    submitted = tuh_msc_read10(dev_addr, lun, buf, lba, count, msc_complete_cb,
                               tag);
    break;
  case ScsiOp::Write:
    submitted = tuh_msc_write10(dev_addr, lun, buf, lba, count,
                                msc_complete_cb, tag);
    break;
  }
  if (!submitted) {
    _busy = false;
    _tag = 0;
    return false;
  }
  return _waitForIo(dev_addr);
}

//--------------------------------------------------------------------+
// Public blocking API
//--------------------------------------------------------------------+

#if USBHOSTMSC_ESP32_CORE

// DMA-capable, aligned, internal-RAM buffer. The caller's buffer may live in
// PSRAM or be unaligned, neither of which the DWC2 DMA can use, so every
// transfer is bounced through this one.
uint8_t *USBHostMSC::_bounceBuffer(size_t bytes) {
  if (_bounce && _bounce_size >= bytes) {
    return _bounce;
  }
  if (_bounce) {
    heap_caps_free(_bounce);
    _bounce = nullptr;
    _bounce_size = 0;
  }
  _bounce = (uint8_t *)heap_caps_aligned_alloc(
      32, bytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  if (_bounce) {
    _bounce_size = bytes;
  }
  return _bounce;
}

// After a timeout or unplug the controller may still be writing into the
// buffer, so it must never be reused or freed: deliberately leak it.
void USBHostMSC::_abandonBounce() {
  _bounce = nullptr;
  _bounce_size = 0;
}

bool USBHostMSC::_xfer(bool is_write, uint8_t dev_addr, uint8_t lun,
                       uint32_t lba, void *buffer, uint32_t blocks) {
  if (!buffer || blocks == 0 || !_devPresent(dev_addr) || !_io_mutex) {
    return false;
  }
  const uint32_t bsize = tuh_msc_get_block_size(dev_addr, lun);
  if (bsize == 0) {
    return false;
  }
  if (xSemaphoreTakeRecursive(_io_mutex, pdMS_TO_TICKS(60000)) != pdTRUE) {
    return false;
  }

  uint8_t *user = (uint8_t *)buffer;
  bool ok = true;
  while (blocks > 0 && ok) {
    uint32_t chunk = blocks;
    if (chunk > USB_HOST_MSC_MAX_BLOCKS_PER_XFER) {
      chunk = USB_HOST_MSC_MAX_BLOCKS_PER_XFER;
    }
    const size_t nbytes = (size_t)chunk * bsize;
    uint8_t *dma = _bounceBuffer(nbytes);
    if (!dma) {
      ok = false;
      break;
    }
    if (is_write) {
      memcpy(dma, user, nbytes);
    }

    // The previous command's status stage may still be in flight.
    const uint32_t start = millis();
    while (!tuh_msc_ready(dev_addr) && _devPresent(dev_addr) &&
           (millis() - start) < 3000) {
      if (!USBHost.tuhBackgroundActive()) {
        tuh_task_ext(0, false);
      }
      vTaskDelay(1);
    }

    if (!_scsi(is_write ? ScsiOp::Write : ScsiOp::Read, dev_addr, lun, dma, lba,
               (uint16_t)chunk)) {
      _abandonBounce();
      ok = false;
      break;
    }
    if (!is_write) {
      memcpy(user, dma, nbytes);
    } else if (USB_HOST_MSC_WRITE_PACING_MS > 0) {
      delay(USB_HOST_MSC_WRITE_PACING_MS);
    }
    user += nbytes;
    lba += chunk;
    blocks -= chunk;
  }
  xSemaphoreGiveRecursive(_io_mutex);
  return ok;
}

bool USBHostMSC::inquiry(uint8_t dev_addr, uint8_t lun, MSCInquiry &out) {
  if (!_devPresent(dev_addr) || !_io_mutex ||
      xSemaphoreTakeRecursive(_io_mutex, pdMS_TO_TICKS(60000)) != pdTRUE) {
    return false;
  }
  bool ok = false;
  uint8_t *dma = _bounceBuffer(512);
  if (dma) {
    ok = _scsi(ScsiOp::Inquiry, dev_addr, lun, dma, 0, 0);
    if (ok) {
      scsi_inquiry_resp_t resp;
      memcpy(&resp, dma, sizeof(resp));
      memcpy(out.vendor_id, resp.vendor_id, sizeof(resp.vendor_id));
      out.vendor_id[sizeof(resp.vendor_id)] = '\0';
      memcpy(out.product_id, resp.product_id, sizeof(resp.product_id));
      out.product_id[sizeof(resp.product_id)] = '\0';
      memcpy(out.product_rev, resp.product_rev, sizeof(resp.product_rev));
      out.product_rev[sizeof(resp.product_rev)] = '\0';
    } else {
      _abandonBounce();
    }
  }
  xSemaphoreGiveRecursive(_io_mutex);
  return ok;
}

bool USBHostMSC::readBlocks(uint8_t dev_addr, uint32_t lba, void *buffer,
                             uint16_t block_count, uint8_t lun) {
  return _xfer(false, dev_addr, lun, lba, buffer, block_count);
}

bool USBHostMSC::writeBlocks(uint8_t dev_addr, uint32_t lba,
                              const void *buffer, uint16_t block_count,
                              uint8_t lun) {
  return _xfer(true, dev_addr, lun, lba, (void *)buffer, block_count);
}

#else // Adafruit TinyUSB path: the caller's buffer goes straight to TinyUSB

bool USBHostMSC::inquiry(uint8_t dev_addr, uint8_t lun, MSCInquiry &out) {
  scsi_inquiry_resp_t resp;
  if (!_scsi(ScsiOp::Inquiry, dev_addr, lun, &resp, 0, 0)) {
    return false;
  }
  memcpy(out.vendor_id, resp.vendor_id, sizeof(resp.vendor_id));
  out.vendor_id[sizeof(resp.vendor_id)] = '\0';
  memcpy(out.product_id, resp.product_id, sizeof(resp.product_id));
  out.product_id[sizeof(resp.product_id)] = '\0';
  memcpy(out.product_rev, resp.product_rev, sizeof(resp.product_rev));
  out.product_rev[sizeof(resp.product_rev)] = '\0';
  return true;
}

bool USBHostMSC::readBlocks(uint8_t dev_addr, uint32_t lba, void *buffer,
                             uint16_t block_count, uint8_t lun) {
  return _scsi(ScsiOp::Read, dev_addr, lun, buffer, lba, block_count);
}

bool USBHostMSC::writeBlocks(uint8_t dev_addr, uint32_t lba,
                              const void *buffer, uint16_t block_count,
                              uint8_t lun) {
  return _scsi(ScsiOp::Write, dev_addr, lun, (void *)buffer, lba, block_count);
}

#endif

void USBHostMSC::onMount(MountCallback cb) { _on_mount = cb; }
void USBHostMSC::onUnmount(MountCallback cb) { _on_unmount = cb; }

void USBHostMSC::_handleMount(uint8_t dev_addr) {
  if (dev_addr < kMaxDevices) {
    _mounted[dev_addr] = true;
  }
  if (_fs_backend) {
    _fs_backend->begin(*this, dev_addr);
  }
  if (_on_mount) {
    _on_mount(dev_addr);
  }
}

void USBHostMSC::_handleUnmount(uint8_t dev_addr) {
  if (_fs_backend) {
    _fs_backend->end(dev_addr);
  }
  if (dev_addr < kMaxDevices) {
    _mounted[dev_addr] = false;
  }
  if (_on_unmount) {
    _on_unmount(dev_addr);
  }
}

#if USBHOSTMSC_ESP32_CORE
void USBHostMSC::_postEvent(uint8_t dev_addr, bool mount) {
  if (dev_addr >= kMaxDevices) {
    return;
  }
  _present[dev_addr] = mount; // visible at once to a wait that must abort
  if (_events) {
    Event ev = {dev_addr, mount};
    xQueueSend(_events, &ev, 0);
  }
}
#endif

//--------------------------------------------------------------------+
// TinyUSB host MSC callbacks (weak in the core stack, strong here)
//--------------------------------------------------------------------+
extern "C" {

void tuh_msc_mount_cb(uint8_t dev_addr) {
  if (!USBHostMSC::_instance) {
    return;
  }
#if USBHOSTMSC_ESP32_CORE
  // Runs on TinyUSB's worker task: just queue it for task().
  USBHostMSC::_instance->_postEvent(dev_addr, true);
#else
  USBHostMSC::_instance->_handleMount(dev_addr);
#endif
}

void tuh_msc_umount_cb(uint8_t dev_addr) {
  if (!USBHostMSC::_instance) {
    return;
  }
#if USBHOSTMSC_ESP32_CORE
  USBHostMSC::_instance->_postEvent(dev_addr, false);
#else
  USBHostMSC::_instance->_handleUnmount(dev_addr);
#endif
}

} // extern "C"

#endif // USBHOSTMSC_COMPILE
