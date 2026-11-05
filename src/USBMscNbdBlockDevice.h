#pragma once

/**
 * @file USBMscNbdBlockDevice.h
 * @brief Adapter that turns a connected TinyNBD client into a
 * USBMscBlockDevice, so a remote NBD export can be
 *  - mounted with USBMscFatFS / USBMscExt4FS / USBMscAutoFS (begin(dev)), or
 *  - exported to a PC as a USB drive with USBDeviceMSC::begin(dev).
 *
 * Needs the TinyNBD library (https://github.com/pschatzmann/arduino-nbd).
 * It is header-only and not included by any other header of this library,
 * so TinyNBD is only required in sketches that include this file.
 */

#include <NBD.h>

#include "USBMscBlockDevice.h"

/**
 * @brief USBMscBlockDevice on top of an nbd::NBDClient (512-byte blocks).
 *
 * The client must be connected before the device is used and must outlive
 * it. The adapter does not connect, reconnect or disconnect: if the
 * connection drops, every call fails until you call connect() on the
 * client again.
 *
 * @code
 * WiFiClient wifi;
 * nbd::NBDClient nbd_client(wifi);
 * USBMscNbdBlockDevice remote(nbd_client);
 * USBMscAutoFS<USBMscExt4FS> fs;
 *
 * nbd_client.connect("192.168.1.10", nbd::NBD_DEFAULT_PORT, "disk");
 * fs.begin(remote);
 * @endcode
 */
class USBMscNbdBlockDevice : public USBMscBlockDevice {
public:
  static constexpr uint16_t kBlockSize = 512;

  explicit USBMscNbdBlockDevice(nbd::NBDClient &client) : _client(client) {}

  /// Whole 512-byte blocks in the export (0 while not connected).
  uint32_t blockCount() override {
    if (!_client.isConnected()) {
      return 0;
    }
    uint64_t n = _client.size() / kBlockSize;
    return n > 0xFFFFFFFFULL ? 0xFFFFFFFFu : (uint32_t)n;
  }

  uint16_t blockSize() override { return kBlockSize; }

  bool readBlocks(uint32_t lba, void *buffer, uint32_t count) override {
    if (!inRange(lba, count)) {
      return false;
    }
    uint8_t *p = (uint8_t *)buffer;
    uint64_t offset = (uint64_t)lba * kBlockSize;
    size_t len = (size_t)count * kBlockSize;
    size_t max = chunk();
    while (len > 0) {
      size_t n = len < max ? len : max;
      if (!_client.read(offset, p, n)) {
        return false;
      }
      offset += n;
      p += n;
      len -= n;
    }
    return true;
  }

  /// Fails on a read-only export.
  bool writeBlocks(uint32_t lba, const void *buffer, uint32_t count) override {
    if (_client.isReadOnly() || !inRange(lba, count)) {
      return false;
    }
    const uint8_t *p = (const uint8_t *)buffer;
    uint64_t offset = (uint64_t)lba * kBlockSize;
    size_t len = (size_t)count * kBlockSize;
    size_t max = chunk();
    while (len > 0) {
      size_t n = len < max ? len : max;
      if (!_client.write(offset, p, n)) {
        return false;
      }
      offset += n;
      p += n;
      len -= n;
    }
    return true;
  }

  /// Commits pending writes on the server (no-op if it can't flush).
  void flush() override {
    if (_client.isConnected() && _client.canFlush()) {
      _client.flush();
    }
  }

  bool isReadOnly() { return _client.isReadOnly(); }

private:
  bool inRange(uint32_t lba, uint32_t count) {
    return _client.isConnected() &&
           (uint64_t)lba + count <= _client.size() / kBlockSize;
  }

  // Largest request the server accepts, a multiple of the block size.
  size_t chunk() {
    size_t max = _client.maxPayload();
    if (max < kBlockSize) {
      return max == 0 ? (size_t)0x7FFFFE00 : (size_t)kBlockSize;
    }
    return max - (max % kBlockSize);
  }

  nbd::NBDClient &_client;
};
