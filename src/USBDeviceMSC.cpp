#include "USBMsc_config.h"
#if USBDEVMSC_COMPILE
#include "USBDeviceMSC.h"

#include <stdlib.h>

// ----------------------------------------------------------- USBDeviceMSC

static USBDeviceMSC *s_instance = nullptr; // callbacks carry no context

USBDeviceMSC::USBDeviceMSC() {}
USBDeviceMSC::~USBDeviceMSC() { end(); }

void USBDeviceMSC::setID(const char *vendor, const char *product,
                         const char *revision) {
  strncpy(_vendor, vendor, 8);   _vendor[8] = 0;
  strncpy(_product, product, 16); _product[16] = 0;
  strncpy(_rev, revision, 4);    _rev[4] = 0;
}

// Sector range check: the PC must stay inside the exported capacity.
static bool inRange(uint32_t lba, uint32_t offset, uint32_t size,
                    uint32_t sectors, uint16_t bs) {
  uint64_t end = (uint64_t)lba * bs + offset + size;
  return end <= (uint64_t)sectors * bs;
}

int32_t USBDeviceMSC::_read(uint32_t lba, uint32_t offset, void *buf,
                            uint32_t size) {
  if (!_open) return -1;
  _active = true;
  const uint16_t bs = kSectorSize;
  if (!inRange(lba, offset, size, _disk.sectors, bs)) return -1;
  if (offset == 0 && size % bs == 0)
    return _readSectors(lba, buf, size / bs) ? (int32_t)size : -1;
  // Unaligned access (ESP32 core may split a block): bounce one block at a time.
  uint8_t *out = (uint8_t *)buf;
  uint32_t done = 0;
  uint8_t *tmp = (uint8_t *)malloc(bs);
  if (!tmp) return -1;
  while (done < size) {
    uint32_t pos = offset + done;
    if (!_readSectors(lba + pos / bs, tmp, 1)) { free(tmp); return -1; }
    uint32_t n = bs - pos % bs;
    if (n > size - done) n = size - done;
    memcpy(out + done, tmp + pos % bs, n);
    done += n;
  }
  free(tmp);
  return (int32_t)size;
}

int32_t USBDeviceMSC::_write(uint32_t lba, uint32_t offset, const uint8_t *buf,
                             uint32_t size) {
  if (!_open || !_writable) return -1;
  _active = true;
  const uint16_t bs = kSectorSize;
  if (!inRange(lba, offset, size, _disk.sectors, bs)) return -1;
  if (offset == 0 && size % bs == 0) {
    if (!_writeSectors(lba, buf, size / bs)) return -1;
  } else {
    uint8_t *tmp = (uint8_t *)malloc(bs);
    if (!tmp) return -1;
    uint32_t done = 0;
    while (done < size) {
      uint32_t pos = offset + done;
      uint32_t blk = lba + pos / bs;
      uint32_t n = bs - pos % bs;
      if (n > size - done) n = size - done;
      if (!_readSectors(blk, tmp, 1)) { free(tmp); return -1; }
      memcpy(tmp + pos % bs, buf + done, n);
      if (!_writeSectors(blk, tmp, 1)) { free(tmp); return -1; }
      done += n;
    }
    free(tmp);
  }
  _written = true;
  if (_onWrite) _onWrite(lba, (offset + size + bs - 1) / bs);
  return (int32_t)size;
}

void USBDeviceMSC::_flush() {
  if (_open) _disk.flush(_disk.ctx);
}

#if USBDEVMSC_ESP32_CORE

static int32_t cbRead(uint32_t lba, uint32_t offset, void *buf, uint32_t n) {
  return s_instance ? s_instance->_read(lba, offset, buf, n) : -1;
}
static int32_t cbWrite(uint32_t lba, uint32_t offset, uint8_t *buf,
                       uint32_t n) {
  return s_instance ? s_instance->_write(lba, offset, buf, n) : -1;
}
static bool cbStartStop(uint8_t, bool, bool load_eject) {
  if (s_instance && load_eject) s_instance->_flush();
  return true;
}

bool USBDeviceMSC::_begin(const Disk &disk, bool startUsb) {
  if (s_instance && s_instance != this) return false;
  s_instance = this;
  _disk = disk;
  _open = true;
  _msc.vendorID(_vendor);
  _msc.productID(_product);
  _msc.productRevision(_rev);
  _msc.onRead(cbRead);
  _msc.onWrite(cbWrite);
  _msc.onStartStop(cbStartStop);
  _msc.mediaPresent(true);
  if (!_msc.begin(_disk.sectors, kSectorSize)) return false;
  if (startUsb) USB.begin();
  return true;
}

void USBDeviceMSC::end() {
  if (s_instance != this) return;
  _msc.end();
  s_instance = nullptr;
  _open = false;
}

void USBDeviceMSC::mediaPresent(bool present) { _msc.mediaPresent(present); }

#else // Adafruit_USBD_MSC

static int32_t cbRead(uint32_t lba, void *buf, uint32_t n) {
  return s_instance ? s_instance->_read(lba, 0, buf, n) : -1;
}
static int32_t cbWrite(uint32_t lba, uint8_t *buf, uint32_t n) {
  return s_instance ? s_instance->_write(lba, 0, buf, n) : -1;
}
static void cbFlush() {
  if (s_instance) s_instance->_flush();
}

bool USBDeviceMSC::_begin(const Disk &disk, bool) {
  if (s_instance && s_instance != this) return false;
  s_instance = this;
  _disk = disk;
  _open = true;
  _msc.setID(_vendor, _product, _rev);
  _msc.setCapacity(_disk.sectors, kSectorSize);
  _msc.setReadWriteCallback(cbRead, cbWrite, cbFlush);
  _msc.setUnitReady(true);
  return _msc.begin();
}

void USBDeviceMSC::end() {
  if (s_instance != this) return;
  _msc.setUnitReady(false);
  s_instance = nullptr;
  _open = false;
}

void USBDeviceMSC::mediaPresent(bool present) { _msc.setUnitReady(present); }

#endif

#endif // USBDEVMSC_COMPILE
