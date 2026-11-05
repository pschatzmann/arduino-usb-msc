#include "test_common.h"

static void put32(uint8_t *p, uint32_t v) {
  for (int i = 0; i < 4; i++) p[i] = (uint8_t)(v >> (8 * i));
}

static std::string craftMbr(TmpDir &d, uint32_t lba, uint32_t count) {
  std::string img = d / "crafted.img";
  sh("truncate -s 8M '" + img + "'"); // 16384 sectors
  uint8_t mbr[512] = {0};
  uint8_t *e = mbr + 446;
  e[4] = 0x0C;
  put32(e + 8, lba);
  put32(e + 12, count);
  mbr[510] = 0x55;
  mbr[511] = 0xAA;
  FILE *f = fopen(img.c_str(), "r+b");
  fwrite(mbr, 1, 512, f);
  fclose(f);
  return img;
}

static void test_type_predicates() {
  for (uint8_t t : {0x01, 0x04, 0x06, 0x07, 0x0B, 0x0C, 0x0E}) {
    CHECK(isFatPartitionType(t));
  }
  CHECK(!isFatPartitionType(0x83));
  CHECK(!isFatPartitionType(0x00));
  CHECK(isLinuxPartitionType(0x83));
  CHECK(!isLinuxPartitionType(0x0C));
}

static void test_read_mbr() {
  TmpDir d;
  std::string img = makeDisk(d, "fat32", Layout::Mbr);
  USBHostMSC msc;
  CHECK(msc.attach(1, img.c_str()));
  USBMscPartitionEntry e[4];
  CHECK_EQ((int)USBMscPartitionTable::read(msc, 1, e), 1);
  CHECK_EQ((int)e[0].type, 0x0C);
  CHECK_EQ(e[0].lba_start, 2048u);
}

static void test_exfat_type_07_binds_partition() {
  TmpDir d;
  std::string img = makeDisk(d, "exfat", Layout::Mbr);
  USBHostMSC msc;
  CHECK(msc.attach(1, img.c_str()));
  USBMscPartition part;
  USBMscPartitionTable::bindFirstMatching(msc, 1, isFatPartitionType, part);
  CHECK_EQ(part.blockCount(), msc.blockCount(1) - 2048);
}

static void test_bounds_checked() {
  TmpDir d;
  std::string img = makeDisk(d, "fat32", Layout::Mbr);
  USBHostMSC msc;
  CHECK(msc.attach(1, img.c_str()));
  USBMscPartition part;
  USBMscPartitionTable::bindFirstMatching(msc, 1, isFatPartitionType, part);
  uint32_t n = part.blockCount();
  uint8_t buf[1024];
  CHECK(part.readBlocks(0, buf, 1));
  CHECK(part.readBlocks(n - 1, buf, 1));
  CHECK(!part.readBlocks(n, buf, 1));
  CHECK(!part.readBlocks(n - 1, buf, 2));
  CHECK(!part.readBlocks(0xFFFFFFFFu, buf, 1)); // lba + count must not wrap
  CHECK(!part.writeBlocks(n, buf, 1));
  CHECK(!part.writeBlocks(n - 1, buf, 2));
}

static void test_oversized_entry_is_clamped() {
  TmpDir d;
  std::string img = craftMbr(d, 2048, 0x00FFFFFF);
  USBHostMSC msc;
  CHECK(msc.attach(1, img.c_str()));
  USBMscPartition part;
  USBMscPartitionTable::bindFirstMatching(msc, 1, isFatPartitionType, part);
  CHECK_EQ(part.blockCount(), 16384u - 2048u);
}

static void test_entry_past_end_of_disk() {
  TmpDir d;
  std::string img = craftMbr(d, 0x10000000, 100);
  USBHostMSC msc;
  CHECK(msc.attach(1, img.c_str()));
  USBMscPartition part;
  USBMscPartitionTable::bindFirstMatching(msc, 1, isFatPartitionType, part);
  CHECK_EQ(part.blockCount(), 0u);
  uint8_t buf[512];
  CHECK(!part.readBlocks(0, buf, 1));
}

int main() {
  RUN(test_type_predicates);
  RUN(test_read_mbr);
  RUN(test_exfat_type_07_binds_partition);
  RUN(test_bounds_checked);
  RUN(test_oversized_entry_is_clamped);
  RUN(test_entry_past_end_of_disk);
  return finish();
}
