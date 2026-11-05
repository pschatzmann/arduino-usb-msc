#pragma once

// See USBMscExt4FSBase.h -- ESP32, RP2040 and STM32.
#if defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_ARCH_RP2040) ||            \
    defined(ARDUINO_ARCH_STM32)

#include "USBMscExt4FSBase.h"

/**
 * @brief Mounts an ext2/3/4 partition (or whole, unpartitioned disk)
 * read-only, using TinyEXT4. fs::File on ESP32/RP2040, USBMscFile on
 * STM32 (see USBMscExt4FSBase.h).
 *
 * Mounts read-only -- write()/mkdir()/remove()/etc. on the result simply
 * fail, rather than risking a write to media
 * the caller only meant to read. Use USBMscExt4FS instead if you need to
 * write.
 */
class USBMscExt4FSReadOnly : public USBMscExt4FSBase {
public:
  USBMscExt4FSReadOnly() : USBMscExt4FSBase(/* read_only= */ true) {}
};

#endif // ARDUINO_ARCH_ESP32 || ARDUINO_ARCH_RP2040 || ARDUINO_ARCH_STM32
