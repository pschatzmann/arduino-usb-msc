// Hot plug: drives pulled while files are open, replaced by the same or by
// another drive at the same USB address, and I/O errors in the middle of a
// write. Meaningful under AddressSanitizer (the default).
#include "test_common.h"

//------------------------------------------------------------------ FAT
static void test_fat_unplug_and_replug_same_drive() {
  TmpDir d;
  std::string img = makeDisk(d, "fat32", Layout::Mbr);
  Drive drive;
  CHECK(drive.plug(img));
  USBHostMscFatIO io(drive.msc);
  fatfs::SDClass sd;
  CHECK(sd.begin(io));
  fatfs::File wf = sd.open("/kept.txt", fatMode("w"));
  CHECK((bool)wf);
  CHECK_EQ(wf.write((const uint8_t *)"persist me", 10), (size_t)10);
  wf.flush();

  drive.unplug();
  CHECK_EQ(wf.write((const uint8_t *)"x", 1), (size_t)0);
  CHECK(!sd.exists("/hello.txt"));
  std::string s;
  CHECK(!readFile(sd, "/hello.txt", s));

  // plugged in again: FatFs remounts by itself, the old file stays invalid
  CHECK(drive.plug(img));
  CHECK_EQ(wf.write((const uint8_t *)"x", 1), (size_t)0);
  CHECK(readFile(sd, "/kept.txt", s));
  CHECK_EQ(s, std::string("persist me"));
  CHECK_EQ(wf.write((const uint8_t *)"x", 1), (size_t)0);
  wf.close();
  CHECK(writeFile(sd, "/after.txt", "w", "new mount"));
  sd.end();
  CHECK(fsckOk(img, "fat32", true));
}

static void test_fat_replaced_by_other_drive() {
  TmpDir d;
  std::string a = makeDisk(d, "fat32", Layout::Mbr, "a");
  std::string b = makeDisk(d, "fat16", Layout::Raw, "b");
  Drive drive;
  CHECK(drive.plug(a));
  USBHostMscFatIO io(drive.msc);
  fatfs::SDClass sd;
  CHECK(sd.begin(io));
  fatfs::File wf = sd.open("/from_a.txt", fatMode("w"));
  CHECK((bool)wf);
  std::string data(5000, 'a'); // more than a sector: dirty FIL buffer
  CHECK_EQ(wf.write((const uint8_t *)data.data(), data.size()), data.size());

  drive.unplug();
  CHECK(drive.plug(b)); // another drive, same address
  unsigned writes = drive.msc.writes;
  CHECK_EQ(wf.write((const uint8_t *)"x", 1), (size_t)0);
  wf.flush();
  wf.close(); // must not flush drive a's state onto drive b
  CHECK_EQ(drive.msc.writes, writes);

  // the next access mounts drive b
  std::string s;
  CHECK(readFile(sd, "/hello.txt", s));
  CHECK(!sd.exists("/from_a.txt"));
  sd.end();
  CHECK(fsckOk(b, "fat16", false));
}

static void test_fat_io_errors() {
  TmpDir d;
  std::string img = makeDisk(d, "fat32", Layout::Raw);
  Drive drive;
  CHECK(drive.plug(img));
  USBHostMscFatIO io(drive.msc);
  fatfs::SDClass sd;
  CHECK(sd.begin(io));
  drive.msc.failAfter(3); // the drive starts failing in the middle of a write
  (void)writeFile(sd, "/err.bin", "w", std::string(100000, 'z'));
  drive.msc.failAfter(-1);
  sd.end();
}

//----------------------------------------------------------------- ext4
static void test_ext4_unplug_and_replug_same_drive() {
  TmpDir d;
  std::string img = makeDisk(d, "ext4", Layout::Raw);
  Drive drive;
  CHECK(drive.plug(img));
  USBHostMscExt4IO io(drive.msc);
  tinyext4::Ext4FS fs;
  CHECK(fs.begin(io));
  tinyext4::File wf = fs.open("/kept.txt", "w");
  CHECK((bool)wf);
  CHECK_EQ(wf.write((const uint8_t *)"persist me", 10), (size_t)10);
  wf.flush();
  fs.sync();

  drive.unplug();
  CHECK_EQ(wf.write((const uint8_t *)"x", 1), (size_t)0);
  std::string s;
  CHECK(!readFile(fs, "/dir/sub.txt", s)); // not in the cache: needs the drive
  fs.end(); // must not hang or crash on a dead drive

  CHECK(drive.plug(img));
  CHECK(fs.begin(io)); // ext4 needs an explicit remount
  CHECK(!wf);          // file of the old mount
  CHECK_EQ(wf.write((const uint8_t *)"x", 1), (size_t)0);
  wf.close();
  CHECK(readFile(fs, "/kept.txt", s));
  CHECK_EQ(s, std::string("persist me"));
  fs.end();
  CHECK(fsckOk(img, "ext4", false));
}

static void test_ext4_replaced_by_other_drive() {
  TmpDir d;
  std::string a = makeDisk(d, "ext4", Layout::Raw, "a");
  std::string b = makeDisk(d, "ext4", Layout::Mbr, "b");
  Drive drive;
  CHECK(drive.plug(a));
  USBHostMscExt4IO io(drive.msc);
  tinyext4::Ext4FS fs;
  CHECK(fs.begin(io));
  tinyext4::File wf = fs.open("/from_a.txt", "w");
  CHECK((bool)wf);
  CHECK_EQ(wf.write((const uint8_t *)"dirty", 5), (size_t)5);

  drive.unplug();
  CHECK(drive.plug(b)); // same address, but not the drive fs has cached
  unsigned writes = drive.msc.writes;
  (void)wf.write((const uint8_t *)"x", 1);
  wf.close(); // would write a's inode and bitmaps onto b
  fs.end();   // would write a's superblock onto b
  CHECK_EQ(drive.msc.writes, writes);

  CHECK(fs.begin(io)); // now b is mounted
  CHECK(!fs.exists("/from_a.txt"));
  CHECK(writeFile(fs, "/on_b.txt", "w", "b"));
  fs.end();
  CHECK(fsckOk(b, "ext4", true));
}

static void test_ext4_io_errors() {
  TmpDir d;
  std::string img = makeDisk(d, "ext4", Layout::Raw);
  Drive drive;
  CHECK(drive.plug(img));
  USBHostMscExt4IO io(drive.msc);
  tinyext4::Ext4FS fs;
  CHECK(fs.begin(io));
  drive.msc.failAfter(3);
  (void)writeFile(fs, "/err.bin", "w", std::string(100000, 'z'));
  drive.msc.failAfter(-1);
  fs.end();
}

//----------------------------------------------------------- USBHostMscDisk
static void test_disk_large_transfers_are_split() {
  TmpDir d;
  std::string img = makeDisk(d, "fat16", Layout::Raw); // 32 MB
  Drive drive;
  CHECK(drive.plug(img));
  USBHostMscDisk disk(drive.msc);
  CHECK(disk.init());
  const uint32_t n = 0x9000; // more than one 16-bit SCSI block count
  std::vector<uint8_t> buf((size_t)n * 512, 0x5A);
  CHECK(disk.write(16, buf.data(), n));
  CHECK(drive.msc.max_count <= 0x8000);
  std::vector<uint8_t> back((size_t)n * 512, 0);
  CHECK(disk.read(16, back.data(), n));
  CHECK(back == buf);
  CHECK(!disk.read(disk.sectorCount() - 1, back.data(), 2)); // past the end
  CHECK(disk.read(disk.sectorCount() - 1, back.data(), 1));
}

static void test_disk_rejects_other_block_sizes() {
  TmpDir d;
  std::string img = makeDisk(d, "fat16", Layout::Raw);
  USBHostMSC msc;
  CHECK(msc.attach(1, img.c_str(), 4096));
  USBHostMscDisk disk(msc);
  CHECK(!disk.init());
  CHECK(!disk.ready());
  uint8_t b[512];
  CHECK(!disk.read(0, b, 1));
}

int main() {
  RUN(test_fat_unplug_and_replug_same_drive);
  RUN(test_fat_replaced_by_other_drive);
  RUN(test_fat_io_errors);
  RUN(test_ext4_unplug_and_replug_same_drive);
  RUN(test_ext4_replaced_by_other_drive);
  RUN(test_ext4_io_errors);
  RUN(test_disk_large_transfers_are_split);
  RUN(test_disk_rejects_other_block_sizes);
  return finish();
}
