#pragma once

#include "../USBMscPartition.h"
#include "USBMscFsBackend.h"

namespace usbmscfs {

/**
 * @brief ext2/ext3/ext4 filesystem backend built on TinyEXT4 (MIT), a
 * header-only ext implementation without journaling.
 *
 * Kept as an opaque pimpl so that TinyEXT4's headers are only ever
 * #include'd from USBMscExt4Backend.cpp.
 *
 * ext4 (extents + filetype) mounts read/write unless the caller asked for
 * read-only; ext2/ext3, filesystems with a pending journal recovery, and
 * filesystems using features TinyEXT4 can't write are mounted read-only
 * (see isReadOnly()).
 */
class Ext4Backend : public Backend {
public:
  /// read_only=true mounts read-only (USBMscExt4FSReadOnly); false asks
  /// for a read/write mount (USBMscExt4FS), which TinyEXT4 may still
  /// downgrade -- check isReadOnly() after mount().
  Ext4Backend(USBMscPartition &part, bool read_only);
  ~Ext4Backend() override;

  bool mount() override;
  bool isReadOnly() const override;
  BackendFile *open(const char *path, const char *mode) override;
  bool exists(const char *path) override;
  bool isDirectory(const char *path) override;
  bool remove(const char *path) override;
  bool rename(const char *path_from, const char *path_to) override;
  bool mkdir(const char *path) override;
  bool rmdir(const char *path) override;

private:
  struct Impl;
  Impl *_impl;
};

} // namespace usbmscfs
