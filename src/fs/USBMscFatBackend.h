#pragma once

#include "../USBMscPartition.h"
#include "USBMscFsBackend.h"

namespace usbmscfs {

/**
 * @brief FAT12/16/32/exFAT filesystem backend built on TinyFATFS.
 *
 * Kept as an opaque pimpl so that <fatfs.h> (a header-only library) is
 * only ever #include'd from USBMscFatBackend.cpp -- including it from more
 * than one translation unit would cause duplicate-symbol link errors.
 */
class FatBackend : public Backend {
public:
  explicit FatBackend(USBMscPartition &part);
  ~FatBackend() override;

  bool mount() override;
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
