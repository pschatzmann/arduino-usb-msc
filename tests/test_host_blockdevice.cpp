#include "test_common.h"

// Mounting through a plain USBMscBlockDevice (RAM disk, SD, NBD export, ...)
// instead of a USBHostMSC device. The block device here is backed by a disk
// image file, like the USBHostMSC test stub, but only goes through the
// USBMscBlockDevice interface.
class FileBlockDevice : public USBMscBlockDevice {
public:
  explicit FileBlockDevice(const std::string &path) {
    _f = fopen(path.c_str(), "r+b");
    if (_f) {
      fseek(_f, 0, SEEK_END);
      _blocks = (uint32_t)(ftell(_f) / 512);
    }
  }
  ~FileBlockDevice() override {
    if (_f) fclose(_f);
  }
  bool ok() const { return _f != nullptr; }
  uint32_t blockCount() override { return _blocks; }
  bool readBlocks(uint32_t lba, void *buf, uint32_t n) override {
    if (!_f || (uint64_t)lba + n > _blocks) return false;
    fseek(_f, (long)lba * 512, SEEK_SET);
    return fread(buf, 512, n, _f) == n;
  }
  bool writeBlocks(uint32_t lba, const void *buf, uint32_t n) override {
    if (!_f || (uint64_t)lba + n > _blocks) return false;
    fseek(_f, (long)lba * 512, SEEK_SET);
    bool r = fwrite(buf, 512, n, _f) == n;
    fflush(_f);
    return r;
  }
  void flush() override { flushes++; }
  unsigned flushes = 0;

private:
  FILE *_f = nullptr;
  uint32_t _blocks = 0;
};

static void test_read_mbr_from_block_device() {
  TmpDir d;
  FileBlockDevice dev(makeDisk(d, "fat32", Layout::Mbr));
  CHECK(dev.ok());
  USBMscPartitionEntry e[4];
  CHECK_EQ((int)USBMscPartitionTable::read(dev, e), 1);
  CHECK_EQ((int)e[0].type, 0x0C);
  CHECK_EQ(e[0].lba_start, 2048u);
}

static void test_fat_partitioned() {
  TmpDir d;
  FileBlockDevice dev(makeDisk(d, "fat32", Layout::Mbr));
  USBMscPartition part;
  USBMscPartitionTable::bindFirstMatching(dev, isFatPartitionType, part);
  CHECK_EQ(part.blockSize(), 512u);
  CHECK(part.blockCount() < dev.blockCount()); // partition, not whole disk
  CHECK_EQ(part.deviceAddress(), 0);

  usbmscfs::FatBackend be(part);
  CHECK(be.mount());
  std::string s;
  CHECK(readFile(be, "/hello.txt", s));
  CHECK_EQ(s, std::string("Hello USB\n"));
  CHECK(writeFile(be, "/new.txt", "w", "via block device"));
  CHECK(readFile(be, "/new.txt", s));
  CHECK_EQ(s, std::string("via block device"));
}

static void test_fat_raw_and_persisted() {
  TmpDir d;
  std::string img = makeDisk(d, "fat16", Layout::Raw);
  {
    FileBlockDevice dev(img);
    USBMscPartition part;
    USBMscPartitionTable::bindFirstMatching(dev, isFatPartitionType, part);
    usbmscfs::FatBackend be(part);
    CHECK(be.mount());
    CHECK(writeFile(be, "/p.txt", "w", "persist"));
  }
  CHECK(fsckOk(img, "fat16", false));
  FileBlockDevice dev(img);
  USBMscPartition part;
  USBMscPartitionTable::bindFirstMatching(dev, isFatPartitionType, part);
  usbmscfs::FatBackend be(part);
  CHECK(be.mount());
  std::string s;
  CHECK(readFile(be, "/p.txt", s));
  CHECK_EQ(s, std::string("persist"));
}

static void test_ext4_partitioned() {
  TmpDir d;
  std::string img = makeDisk(d, "ext4", Layout::Mbr);
  {
    FileBlockDevice dev(img);
    USBMscPartition part;
    USBMscPartitionTable::bindFirstMatching(dev, isLinuxPartitionType, part);
    usbmscfs::Ext4Backend be(part, /*read_only=*/false);
    CHECK(be.mount());
    std::string s;
    CHECK(readFile(be, "/hello.txt", s));
    CHECK_EQ(s, std::string("Hello USB\n"));
    CHECK(writeFile(be, "/new.txt", "w", "ext4 via block device"));
  }
  CHECK(fsckOk(img, "ext4", true));
}

static void test_out_of_range_and_clamp() {
  TmpDir d;
  FileBlockDevice dev(makeDisk(d, "fat16", Layout::Raw));
  USBMscPartition part;
  part.begin(dev);
  uint8_t b[512];
  CHECK(part.readBlocks(0, b, 1));
  CHECK(!part.readBlocks(part.blockCount(), b, 1));
  CHECK(!part.writeBlocks(part.blockCount() - 1, b, 2));

  // A partition entry reaching past the end of the device is clamped.
  USBMscPartitionEntry e = {0x0C, false, dev.blockCount() - 10, 1000};
  USBMscPartition tail;
  tail.begin(dev, e);
  CHECK_EQ(tail.blockCount(), 10u);
  USBMscPartitionEntry beyond = {0x0C, false, dev.blockCount() + 5, 10};
  tail.begin(dev, beyond);
  CHECK_EQ(tail.blockCount(), 0u);
}

static void test_unbound_partition_fails() {
  USBMscPartition part;
  uint8_t b[512];
  CHECK(!part.readBlocks(0, b, 1));
  CHECK_EQ(part.blockSize(), 0u);
}

int main() {
  RUN(test_read_mbr_from_block_device);
  RUN(test_fat_partitioned);
  RUN(test_fat_raw_and_persisted);
  RUN(test_ext4_partitioned);
  RUN(test_out_of_range_and_clamp);
  RUN(test_unbound_partition_fails);
  return finish();
}
