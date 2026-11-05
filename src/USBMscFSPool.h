#pragma once

#include "USBHostMSC.h"
#include "USBMscFSBackend.h"

/**
 * @brief Mounts up to MaxDevices FSType filesystems simultaneously, one
 * per USB Mass Storage device address -- e.g. several flash drives
 * behind a hub. Works on ESP32, RP2040 and STM32.
 *
 * FSType is any class with the same shape as USBMscFatFS/USBMscExt4FS/
 * USBMscExt4FSReadOnly/USBMscAutoFS: `bool begin(USBHostMSC&, uint8_t)`,
 * `void end(uint8_t)`, `bool mounted()`, and a `fs()` accessor. fs()'s
 * return type isn't hardcoded here -- it's deduced from whatever FSType's
 * own fs() returns, which is fs::FS& on ESP32/RP2040 but a self-reference
 * (e.g. USBMscFatFS&) on STM32 (see USBMscFatFS.h/USBMscExt4FSBase.h).
 * That's what lets this same template work unchanged on all three
 * platforms, unlike USBMscAutoFS (not yet ported to STM32 -- see
 * the wiki page STM32 -- because *that* class needs USBMscFatFS::fs() and
 * USBMscExt4FSBase::fs() to return the *same* type so it can pick between
 * them at runtime; USBMscFSPool never mixes two different FSTypes in one
 * instance, so it never faces that problem).
 *
 * ```cpp
 * USBMscFSPool<USBMscFatFS, 4> pool;   // up to 4 FAT drives at once
 * USBHostMSC msc(&pool);
 *
 * void onMount(uint8_t dev_addr) {
 *   if (pool.mounted(dev_addr)) {
 *     File f = pool.fs(dev_addr).open("/hello.txt", "r"); // USBMscFile on STM32
 *     ...
 *   }
 * }
 * ```
 *
 * Storage for all MaxDevices FSType instances is a fixed-size member
 * array -- no heap allocation, but every slot's RAM/flash cost (FatFs
 * buffers, TinyEXT4 caches, ...) is paid MaxDevices times over, whether or
 * not that many devices are ever actually plugged in. Size MaxDevices to
 * how many devices you actually expect (a typical hub has 4 ports), not
 * to CFG_TUH_DEVICE_MAX (and, if FSType is USBMscFatFS, not beyond
 * TinyFATFS's own FF_VOLUMES=4 -- see that library's src/ff/ffconf.h).
 */
template <typename FSType, uint8_t MaxDevices>
class USBMscFSPool : public USBMscFSBackend {
public:
  bool begin(USBHostMSC &msc, uint8_t dev_addr) override {
    Slot *slot = find(dev_addr);
    if (!slot) {
      slot = findFree();
    }
    if (!slot) {
      return false; // all MaxDevices slots already in use
    }
    if (!slot->fs.begin(msc, dev_addr)) {
      return false;
    }
    slot->dev_addr = dev_addr;
    slot->used = true;
    return true;
  }

  void end(uint8_t dev_addr) override {
    Slot *slot = find(dev_addr);
    if (!slot) {
      return;
    }
    slot->fs.end(dev_addr);
    slot->used = false;
    slot->dev_addr = 0;
  }

  /// True if dev_addr currently has a mounted filesystem in this pool.
  bool mounted(uint8_t dev_addr) const { return find(dev_addr) != nullptr; }

  /// The FSType instance mounted for dev_addr, or nullptr if there is none.
  /// Prefer this over fs() when the device might have just been unplugged.
  FSType *get(uint8_t dev_addr) {
    Slot *slot = find(dev_addr);
    return slot ? &slot->fs : nullptr;
  }

  /// The filesystem mounted for dev_addr. PRECONDITION: mounted(dev_addr)
  /// is true -- there is no "null reference", so calling this for an
  /// unmounted device is undefined behaviour. Use get() for a checked
  /// lookup.
  /// Return type is deduced from FSType::fs() -- see the class comment.
  /// decltype(auto), not auto: plain auto would strip the reference and
  /// return a (sliced, for polymorphic FSTypes) copy instead.
  decltype(auto) fs(uint8_t dev_addr) {
    Slot *slot = find(dev_addr);
    return slot->fs.fs();
  }

  /// Number of currently-occupied slots.
  uint8_t mountedCount() const {
    uint8_t n = 0;
    for (uint8_t i = 0; i < MaxDevices; i++) {
      if (_slots[i].used) {
        n++;
      }
    }
    return n;
  }

  /// dev_addr of the index-th occupied slot (0-based), or 0 if out of range.
  uint8_t deviceAddress(uint8_t index) const {
    uint8_t n = 0;
    for (uint8_t i = 0; i < MaxDevices; i++) {
      if (_slots[i].used) {
        if (n == index) {
          return _slots[i].dev_addr;
        }
        n++;
      }
    }
    return 0;
  }

private:
  struct Slot {
    FSType fs;
    uint8_t dev_addr = 0;
    bool used = false;
  };

  Slot *find(uint8_t dev_addr) {
    for (uint8_t i = 0; i < MaxDevices; i++) {
      if (_slots[i].used && _slots[i].dev_addr == dev_addr) {
        return &_slots[i];
      }
    }
    return nullptr;
  }
  const Slot *find(uint8_t dev_addr) const {
    for (uint8_t i = 0; i < MaxDevices; i++) {
      if (_slots[i].used && _slots[i].dev_addr == dev_addr) {
        return &_slots[i];
      }
    }
    return nullptr;
  }
  Slot *findFree() {
    for (uint8_t i = 0; i < MaxDevices; i++) {
      if (!_slots[i].used) {
        return &_slots[i];
      }
    }
    return nullptr;
  }

  Slot _slots[MaxDevices];
};
