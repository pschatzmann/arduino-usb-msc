#pragma once

#include <stdint.h>

class USBHostMSC;

/**
 * @brief Interface implemented by filesystem-mounting classes (USBMscFatFS,
 * USBMscExt4FS, USBMscAutoFS) so one of them can be handed to USBHostMSC's
 * constructor and get mounted/unmounted automatically as devices come and
 * go.
 *
 * There is no compile-time backend-selection flag anywhere in this
 * library: which filesystem backends end up in your firmware image is decided purely by which
 * of these classes your sketch actually instantiates and references --
 * the linker discards the rest.
 */
class USBMscFSBackend {
public:
  virtual ~USBMscFSBackend() {}

  /// Called when a Mass Storage device mounts. Attempts to find/mount
  /// this backend's filesystem on it; returns false (leaving the backend
  /// unmounted) if not found or the wrong type.
  virtual bool begin(USBHostMSC &msc, uint8_t dev_addr) = 0;

  /// Called when dev_addr unmounts. Implementations that only ever track
  /// a single device (USBMscFatFS, USBMscExt4FS, USBMscAutoFS) may ignore
  /// dev_addr; USBMscFSPool uses it to tear down the right slot.
  virtual void end(uint8_t dev_addr) = 0;
};
