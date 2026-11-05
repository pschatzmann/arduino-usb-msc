// Minimal test helpers: temp dirs, mkfs-built disk images, Backend helpers.
#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <map>
#include <memory>
#include <string>

#include "USBMscPartition.h"
#include "fs/USBMscExt4Backend.h"
#include "fs/USBMscFatBackend.h"

//------------------------------------------------------------------ checks
inline int &failures() {
  static int n = 0;
  return n;
}
#define CHECK(cond)                                                            \
  do {                                                                         \
    if (!(cond)) {                                                             \
      printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                 \
      failures()++;                                                            \
    }                                                                          \
  } while (0)
#define CHECK_EQ(a, b)                                                         \
  do {                                                                         \
    auto _a = (a);                                                             \
    auto _b = (b);                                                             \
    if (!(_a == _b)) {                                                         \
      printf("  FAIL %s:%d: %s == %s\n", __FILE__, __LINE__, #a, #b);          \
      failures()++;                                                            \
    }                                                                          \
  } while (0)
#define RUN(fn)                                                                \
  do {                                                                         \
    int before = failures();                                                   \
    fn();                                                                      \
    printf("%s %s\n", failures() == before ? "ok  " : "FAIL", #fn);            \
  } while (0)
inline int finish() {
  printf("%s (%d failure%s)\n", failures() ? "FAILED" : "PASSED", failures(),
         failures() == 1 ? "" : "s");
  return failures() ? 1 : 0;
}

//------------------------------------------------------------------ shell
inline int sh(const std::string &cmd) {
  return system(("( " + cmd + " ) >/dev/null 2>&1").c_str());
}

struct TmpDir {
  std::string path;
  TmpDir() {
    char t[] = "/tmp/usbmsc_test_XXXXXX";
    path = mkdtemp(t);
  }
  ~TmpDir() { sh("rm -rf '" + path + "'"); }
  std::string operator/(const std::string &n) const { return path + "/" + n; }
};

//---------------------------------------------------------- disk images
enum class Layout { Raw, Mbr, Gpt };

/// Builds a disk image <dir>/<name>.img holding one filesystem of the given
/// kind ("fat32", "fat16", "exfat", "ext4", "ext2") in the given layout.
/// FAT32/FAT16/ext are pre-populated with /hello.txt ("Hello USB\n") and
/// /dir/sub.txt ("sub file content"); exFAT is left empty (no tool to
/// populate it without root) -- tests write their own files there.
inline std::string makeDisk(TmpDir &d, const std::string &kind, Layout layout,
                            const std::string &name = "disk") {
  std::string part = d / (name + ".part");
  std::string disk = d / (name + ".img");
  std::string content = d / (name + ".content");
  sh("mkdir -p '" + content + "/dir'");
  sh("printf 'Hello USB\\n' > '" + content + "/hello.txt'");
  sh("printf 'sub file content' > '" + content + "/dir/sub.txt'");

  // FAT32 needs >= 65525 clusters; at 512 B/cluster that is ~33 MB+
  const char *psize = kind == "fat32" ? "64M" : "32M";
  const char *dsize = kind == "fat32" ? "68M" : "36M";
  sh(std::string("truncate -s ") + psize + " '" + part + "'");
  int rc = 0;
  if (kind == "fat32") {
    rc = sh("mkfs.vfat -F 32 -s 1 '" + part + "'");
  } else if (kind == "fat16") {
    rc = sh("mkfs.vfat -F 16 '" + part + "'");
  } else if (kind == "exfat") {
    rc = sh("mkfs.exfat '" + part + "'");
  } else if (kind == "ext4") {
    rc = sh("mkfs.ext4 -q -F -d '" + content + "' '" + part + "'");
  } else if (kind == "ext2") {
    rc = sh("mkfs.ext2 -q -F -d '" + content + "' '" + part + "'");
  }
  if (rc != 0) {
    printf("mkfs %s failed\n", kind.c_str());
    exit(2);
  }
  if (kind == "fat32" || kind == "fat16") {
    sh("mmd -i '" + part + "' ::dir");
    sh("mcopy -i '" + part + "' '" + content + "/hello.txt' ::hello.txt");
    sh("mcopy -i '" + part + "' '" + content + "/dir/sub.txt' ::dir/sub.txt");
  }

  if (layout == Layout::Raw) {
    sh("mv '" + part + "' '" + disk + "'");
    return disk;
  }
  sh(std::string("truncate -s ") + dsize + " '" + disk + "'");
  bool ext = kind.rfind("ext", 0) == 0;
  if (layout == Layout::Mbr) {
    const char *type = ext ? "83" : (kind == "exfat" ? "7" : "c");
    rc = sh(std::string("echo 'start=2048, type=") + type + "' | sfdisk '" +
            disk + "'");
  } else {
    const char *type = ext ? "8300" : "0700";
    rc = sh(std::string("sgdisk -n 1:2048:0 -t 1:") + type + " '" + disk + "'");
  }
  if (rc != 0) {
    printf("partitioning failed\n");
    exit(2);
  }
  sh("dd if='" + part + "' of='" + disk + "' bs=512 seek=2048 conv=notrunc");
  sh("rm -f '" + part + "'");
  return disk;
}

/// Runs the filesystem checker for kind on the (whole-disk or partition at
/// 2048 sectors) image. Returns true if it reports no errors.
inline bool fsckOk(const std::string &disk, const std::string &kind,
                   bool partitioned) {
  std::string target = disk;
  std::string offset;
  if (partitioned) {
    // extract the partition into a scratch file for the checker
    target = disk + ".fsck";
    sh("dd if='" + disk + "' of='" + target + "' bs=512 skip=2048");
  }
  std::string cmd;
  if (kind == "ext4" || kind == "ext2") {
    cmd = "e2fsck -fn '" + target + "'";
  } else if (kind == "exfat") {
    cmd = "fsck.exfat -n '" + target + "'";
  } else {
    cmd = "fsck.vfat -n '" + target + "'";
  }
  bool ok = sh(cmd) == 0;
  if (partitioned) {
    sh("rm -f '" + target + "'");
  }
  return ok;
}

//------------------------------------------------------------- mounting
/// An image "plugged in" as dev 1, with a filesystem backend mounted on it.
struct Mounted {
  USBHostMSC msc;
  USBMscPartition part;
  std::unique_ptr<usbmscfs::Backend> be;
  static constexpr uint8_t kDev = 1;

  bool mountFat(const std::string &img) {
    if (!msc.attach(kDev, img.c_str())) {
      return false;
    }
    USBMscPartitionTable::bindFirstMatching(msc, kDev, isFatPartitionType,
                                            part);
    be.reset(new usbmscfs::FatBackend(part));
    return be->mount();
  }
  bool mountExt(const std::string &img, bool read_only) {
    if (!msc.attach(kDev, img.c_str())) {
      return false;
    }
    USBMscPartitionTable::bindFirstMatching(msc, kDev, isLinuxPartitionType,
                                            part);
    be.reset(new usbmscfs::Ext4Backend(part, read_only));
    return be->mount();
  }
  void unmount() { be.reset(); }
};

//------------------------------------------------------------- file helpers
inline bool writeFile(usbmscfs::Backend &be, const char *path,
                      const char *mode, const std::string &data) {
  usbmscfs::BackendFile *f = be.open(path, mode);
  if (!f) {
    return false;
  }
  size_t n = f->write((const uint8_t *)data.data(), data.size());
  f->close();
  delete f;
  return n == data.size();
}

inline bool readFile(usbmscfs::Backend &be, const char *path,
                     std::string &out) {
  usbmscfs::BackendFile *f = be.open(path, "r");
  if (!f) {
    return false;
  }
  out.clear();
  uint8_t buf[300];
  size_t n;
  while ((n = f->read(buf, sizeof(buf))) > 0) {
    out.append((const char *)buf, n);
  }
  f->close();
  delete f;
  return true;
}

struct Entry {
  bool is_dir;
  uint64_t size;
};
inline bool listDir(usbmscfs::Backend &be, const char *path,
                    std::map<std::string, Entry> &out) {
  usbmscfs::BackendFile *d = be.open(path, "d");
  if (!d) {
    return false;
  }
  out.clear();
  char name[256];
  bool is_dir;
  uint64_t size;
  while (d->nextEntry(name, sizeof(name), is_dir, size)) {
    out[name] = {is_dir, size};
  }
  d->close();
  delete d;
  return true;
}
