#pragma once

// See USBMscExt4FSBase.h -- ESP32, RP2040 and STM32.
#if defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_ARCH_RP2040) ||            \
    defined(ARDUINO_ARCH_STM32)

#include "USBMscExt4FSBase.h"

/**
 * @brief Mounts an ext2/3/4 partition (or whole, unpartitioned disk)
 * read/write, using TinyEXT4. fs::File on ESP32/RP2040, USBMscFile on
 * STM32 (see USBMscExt4FSBase.h).
 *
 * TinyEXT4 writes no journal, and mounts ext2/ext3 volumes (and ext4
 * volumes needing fsck or using unwritable features) read-only even when
 * asked for read/write -- check readOnly() after mounting. If you only
 * need to read, USBMscExt4FSReadOnly is the lower-risk choice.
 */
class USBMscExt4FS : public USBMscExt4FSBase {
public:
  USBMscExt4FS() : USBMscExt4FSBase(/* read_only= */ false) {}
};

#endif // ARDUINO_ARCH_ESP32 || ARDUINO_ARCH_RP2040 || ARDUINO_ARCH_STM32
