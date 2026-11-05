#include "USBMsc_config.h"
#if USBDEVMSC_COMPILE
#include "USBDeviceMSC.h"

#include <stdlib.h>

// ---------------------------------------------------------------- RAM disk

USBMscRamDisk::USBMscRamDisk(uint32_t blocks, uint8_t *storage)
    : _blocks(blocks), _data(storage), _owned(false) {}

USBMscRamDisk::~USBMscRamDisk() {
  if (_owned) free(_data);
}

bool USBMscRamDisk::begin(bool formatFat) {
  if (!_data) {
    _data = (uint8_t *)calloc(_blocks, 512);
    _owned = _data != nullptr;
  }
  if (!_data) return false;
  return formatFat ? format() : true;
}

bool USBMscRamDisk::readBlocks(uint32_t lba, void *buffer, uint32_t count) {
  if (!_data || lba + count > _blocks) return false;
  memcpy(buffer, _data + (size_t)lba * 512, (size_t)count * 512);
  return true;
}

bool USBMscRamDisk::writeBlocks(uint32_t lba, const void *buffer,
                                uint32_t count) {
  if (!_data || lba + count > _blocks) return false;
  memcpy(_data + (size_t)lba * 512, buffer, (size_t)count * 512);
  return true;
}

static void put16(uint8_t *p, uint16_t v) { p[0] = v; p[1] = v >> 8; }
static void put32(uint8_t *p, uint32_t v) {
  p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24;
}

bool USBMscRamDisk::format(const char *label) {
  if (!_data || _blocks < 64) return false;
  memset(_data, 0, (size_t)_blocks * 512);

  // Choose cluster size so the cluster count stays below 65525 (FAT16)
  // and, for tiny disks, below 4085 (FAT12).
  const uint16_t rootEntries = 64; // 4 sectors
  const uint8_t reserved = 1, fats = 2;
  uint8_t spc = 1;
  while (_blocks / spc > 65000 && spc < 64) spc <<= 1;
  uint32_t clusters = _blocks / spc;
  bool fat12 = clusters < 4085;
  uint32_t fatSectors = fat12 ? ((clusters + 2) * 3 / 2 + 511) / 512
                              : ((clusters + 2) * 2 + 511) / 512;
  uint32_t rootSectors = rootEntries * 32 / 512;
  uint32_t overhead = reserved + fats * fatSectors + rootSectors;
  if (_blocks <= overhead + spc) return false;
  clusters = (_blocks - overhead) / spc;
  fat12 = clusters < 4085; // recheck after overhead

  uint8_t *b = _data;
  b[0] = 0xEB; b[1] = 0x3C; b[2] = 0x90;
  memcpy(b + 3, "MSDOS5.0", 8);
  put16(b + 11, 512);
  b[13] = spc;
  put16(b + 14, reserved);
  b[16] = fats;
  put16(b + 17, rootEntries);
  if (_blocks < 65536) put16(b + 19, (uint16_t)_blocks);
  else put32(b + 32, _blocks);
  b[21] = 0xF8;
  put16(b + 22, (uint16_t)fatSectors);
  put16(b + 24, 63);
  put16(b + 26, 255);
  b[36] = 0x80;
  b[38] = 0x29;
  put32(b + 39, 0x20240101);
  memset(b + 43, ' ', 11);
  memcpy(b + 43, label, strnlen(label, 11));
  memcpy(b + 54, fat12 ? "FAT12   " : "FAT16   ", 8);
  b[510] = 0x55; b[511] = 0xAA;

  for (uint8_t f = 0; f < fats; f++) {
    uint8_t *fat = _data + (size_t)(reserved + f * fatSectors) * 512;
    fat[0] = 0xF8; fat[1] = 0xFF; fat[2] = 0xFF;
    if (!fat12) fat[3] = 0xFF;
  }
  _spc = spc;
  _fat12 = fat12;
  _fatSectors = fatSectors;
  _rootStart = reserved + fats * fatSectors;
  _dataStart = _rootStart + rootSectors;
  _clusters = clusters;
  _rootEntries = rootEntries;
  _nextCluster = 2;
  // volume label entry in the root directory (fsck wants one when the boot
  // sector names a label)
  uint8_t *e = _data + (size_t)_rootStart * 512;
  memset(e, ' ', 11);
  memcpy(e, label, strnlen(label, 11));
  e[11] = 0x08;
  put16(e + 24, 0x0021);
  _rootUsed = 1;
  return true;
}

void USBMscRamDisk::setFat(uint32_t c, uint32_t v) {
  for (uint8_t f = 0; f < 2; f++) {
    uint8_t *fat = _data + (size_t)(1 + f * _fatSectors) * 512;
    if (_fat12) {
      uint32_t off = c + c / 2;
      if (c & 1) {
        fat[off] = (fat[off] & 0x0F) | ((v & 0x0F) << 4);
        fat[off + 1] = (v >> 4) & 0xFF;
      } else {
        fat[off] = v & 0xFF;
        fat[off + 1] = (fat[off + 1] & 0xF0) | ((v >> 8) & 0x0F);
      }
    } else {
      put16(fat + c * 2, (uint16_t)v);
    }
  }
}

bool USBMscRamDisk::addFile(const char *name, const void *data, uint32_t len) {
  if (!_data || !_spc || _rootUsed >= _rootEntries) return false;
  // 8.3 name -> padded 11 bytes
  uint8_t n83[11];
  memset(n83, ' ', 11);
  const char *dot = strchr(name, '.');
  size_t base = dot ? (size_t)(dot - name) : strlen(name);
  size_t ext = dot ? strlen(dot + 1) : 0;
  if (base == 0 || base > 8 || ext > 3) return false;
  memcpy(n83, name, base);
  if (ext) memcpy(n83 + 8, dot + 1, ext);

  const uint32_t csize = (uint32_t)_spc * 512;
  const uint32_t need = (len + csize - 1) / csize;
  if (_nextCluster - 2 + need > _clusters) return false;
  const uint32_t first = need ? _nextCluster : 0;
  for (uint32_t i = 0; i < need; i++) {
    uint32_t c = _nextCluster + i;
    setFat(c, i + 1 == need ? (_fat12 ? 0xFFF : 0xFFFF) : c + 1);
  }
  if (need) {
    memcpy(_data + ((size_t)_dataStart + (size_t)(first - 2) * _spc) * 512, data,
           len);
    _nextCluster += need;
  }
  uint8_t *e = _data + (size_t)_rootStart * 512 + (size_t)_rootUsed * 32;
  memcpy(e, n83, 11);
  e[11] = 0x20; // archive
  put16(e + 24, 0x0021); // 1980-01-01
  put16(e + 26, (uint16_t)first);
  put32(e + 28, len);
  _rootUsed++;
  return true;
}

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

int32_t USBDeviceMSC::_read(uint32_t lba, uint32_t offset, void *buf,
                            uint32_t size) {
  if (!_dev) return -1;
  _active = true;
  const uint16_t bs = _dev->blockSize();
  if (offset == 0 && size % bs == 0)
    return _dev->readBlocks(lba, buf, size / bs) ? (int32_t)size : -1;
  // Unaligned access (ESP32 core may split a block): bounce one block at a time.
  uint8_t *out = (uint8_t *)buf;
  uint32_t done = 0;
  uint8_t *tmp = (uint8_t *)malloc(bs);
  if (!tmp) return -1;
  while (done < size) {
    uint32_t pos = offset + done;
    if (!_dev->readBlocks(lba + pos / bs, tmp, 1)) { free(tmp); return -1; }
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
  if (!_dev || !_writable) return -1;
  _active = true;
  const uint16_t bs = _dev->blockSize();
  if (offset == 0 && size % bs == 0) {
    if (!_dev->writeBlocks(lba, buf, size / bs)) return -1;
  } else {
    uint8_t *tmp = (uint8_t *)malloc(bs);
    if (!tmp) return -1;
    uint32_t done = 0;
    while (done < size) {
      uint32_t pos = offset + done;
      uint32_t blk = lba + pos / bs;
      uint32_t n = bs - pos % bs;
      if (n > size - done) n = size - done;
      if (!_dev->readBlocks(blk, tmp, 1)) { free(tmp); return -1; }
      memcpy(tmp + pos % bs, buf + done, n);
      if (!_dev->writeBlocks(blk, tmp, 1)) { free(tmp); return -1; }
      done += n;
    }
    free(tmp);
  }
  _written = true;
  if (_onWrite) _onWrite(lba, (offset + size + bs - 1) / bs);
  return (int32_t)size;
}

void USBDeviceMSC::_flush() {
  if (_dev) _dev->flush();
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

bool USBDeviceMSC::begin(USBMscBlockDevice &dev, bool startUsb) {
  if (s_instance && s_instance != this) return false;
  s_instance = this;
  _dev = &dev;
  _msc.vendorID(_vendor);
  _msc.productID(_product);
  _msc.productRevision(_rev);
  _msc.onRead(cbRead);
  _msc.onWrite(cbWrite);
  _msc.onStartStop(cbStartStop);
  _msc.mediaPresent(true);
  if (!_msc.begin(dev.blockCount(), dev.blockSize())) return false;
  if (startUsb) USB.begin();
  return true;
}

void USBDeviceMSC::end() {
  if (s_instance != this) return;
  _msc.end();
  s_instance = nullptr;
  _dev = nullptr;
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

bool USBDeviceMSC::begin(USBMscBlockDevice &dev, bool) {
  if (s_instance && s_instance != this) return false;
  s_instance = this;
  _dev = &dev;
  _msc.setID(_vendor, _product, _rev);
  _msc.setCapacity(dev.blockCount(), dev.blockSize());
  _msc.setReadWriteCallback(cbRead, cbWrite, cbFlush);
  _msc.setUnitReady(true);
  return _msc.begin();
}

void USBDeviceMSC::end() {
  if (s_instance != this) return;
  _msc.setUnitReady(false);
  s_instance = nullptr;
  _dev = nullptr;
}

void USBDeviceMSC::mediaPresent(bool present) { _msc.setUnitReady(present); }

#endif

#endif // USBDEVMSC_COMPILE
