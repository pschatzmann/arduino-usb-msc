// FAT12/16/32 and exFAT on a USB drive: USBHostMscFatIO + fatfs::SDClass,
// whole disk and MBR partitioned (partition lookup is done by FatFs).
#include "test_common.h"

static void exercise_writes(fatfs::SDClass &sd) {
  std::string s;

  CHECK(writeFile(sd, "/new.txt", "w", "abc"));
  CHECK(readFile(sd, "/new.txt", s));
  CHECK_EQ(s, std::string("abc"));

  CHECK(writeFile(sd, "/new.txt", "a", "def"));
  CHECK(readFile(sd, "/new.txt", s));
  CHECK_EQ(s, std::string("abcdef"));

  // "w+" must be readable as well as writable
  {
    fatfs::File f = sd.open("/rw.txt", fatMode("w+"));
    CHECK((bool)f);
    CHECK_EQ(f.write((const uint8_t *)"xyz", 3), (size_t)3);
    CHECK(f.seek(0));
    uint8_t b[8] = {0};
    CHECK_EQ(f.readBytes(b, sizeof(b)), (size_t)3);
    CHECK(memcmp(b, "xyz", 3) == 0);
    f.close();
  }
  // "r+" overwrites in place
  {
    fatfs::File f = sd.open("/rw.txt", fatMode("r+"));
    CHECK((bool)f);
    CHECK(f.seek(1));
    CHECK_EQ(f.write((const uint8_t *)"Q", 1), (size_t)1);
    f.close();
  }
  CHECK(readFile(sd, "/rw.txt", s));
  CHECK_EQ(s, std::string("xQz"));

  // directories
  fatfs::FatFs &fat = *sd.getFatFs();
  CHECK(sd.mkdir("/d2"));
  CHECK(sd.open("/d2").isDirectory());
  CHECK(writeFile(sd, "/d2/f.txt", "w", "inside"));
  CHECK(!sd.rmdir("/d2")); // not empty
  CHECK_EQ(fat.f_rename("/d2/f.txt", "/d2/g.txt"), fatfs::FR_OK);
  CHECK(!sd.exists("/d2/f.txt"));
  CHECK(sd.exists("/d2/g.txt"));
  CHECK(sd.remove("/d2/g.txt"));
  CHECK(sd.rmdir("/d2"));
  CHECK(!sd.exists("/d2"));
  CHECK(sd.remove("/rw.txt"));
  CHECK(!sd.exists("/rw.txt"));
  CHECK(!sd.exists("/nope"));
  CHECK(!sd.open("/nope"));

  // a file spanning many clusters
  std::string big(200 * 1024 + 123, 0);
  for (size_t i = 0; i < big.size(); i++) big[i] = (char)(i * 31 + (i >> 8));
  CHECK(writeFile(sd, "/big.bin", "w", big));
  CHECK(readFile(sd, "/big.bin", s));
  CHECK(s == big);
  {
    fatfs::File f = sd.open("/big.bin");
    CHECK(f && f.size() == big.size());
    CHECK(f.seek(150000));
    CHECK_EQ((char)f.read(), big[150000]);
    f.close();
  }
}

static void check_prepopulated(fatfs::SDClass &sd) {
  std::string s;
  CHECK(readFile(sd, "/hello.txt", s));
  CHECK_EQ(s, std::string("Hello USB\n"));

  std::map<std::string, Entry> m;
  CHECK(listDir(sd, "/", m));
  CHECK(m.count("hello.txt") && !m["hello.txt"].is_dir &&
        m["hello.txt"].size == 10);
  CHECK(m.count("dir") && m["dir"].is_dir);

  CHECK(listDir(sd, "/dir", m));
  CHECK(m.count("sub.txt") && m["sub.txt"].size == 16);

  CHECK(readFile(sd, "/dir/sub.txt", s));
  CHECK_EQ(s, std::string("sub file content"));
}

static void run_case(const char *kind, Layout layout, bool populated) {
  TmpDir d;
  std::string img = makeDisk(d, kind, layout);
  {
    Drive drive;
    CHECK(drive.plug(img));
    USBHostMscFatIO io(drive.msc); // kAnyDevice
    fatfs::SDClass sd;
    CHECK(sd.begin(io));
    CHECK_EQ(io.disk().deviceAddress(), Drive::kDev);
    if (populated) check_prepopulated(sd);
    exercise_writes(sd);
    sd.end();
  }
  CHECK(fsckOk(img, kind, layout != Layout::Raw));

  // everything written must survive a remount
  Drive drive;
  CHECK(drive.plug(img));
  USBHostMscFatIO io(drive.msc, Drive::kDev); // explicit address
  fatfs::SDClass sd;
  CHECK(sd.begin(io));
  std::string s;
  CHECK(readFile(sd, "/new.txt", s));
  CHECK_EQ(s, std::string("abcdef"));
}

static void test_fat32_raw() { run_case("fat32", Layout::Raw, true); }
static void test_fat32_mbr() { run_case("fat32", Layout::Mbr, true); }
static void test_fat16_mbr() { run_case("fat16", Layout::Mbr, true); }
static void test_exfat_raw() { run_case("exfat", Layout::Raw, false); }
static void test_exfat_mbr() { run_case("exfat", Layout::Mbr, false); }

static void test_rejects_ext4() {
  TmpDir d;
  std::string img = makeDisk(d, "ext4", Layout::Raw);
  Drive drive;
  CHECK(drive.plug(img));
  USBHostMscFatIO io(drive.msc);
  fatfs::SDClass sd;
  CHECK(!sd.begin(io)); // mounted immediately, not lazily
}

static void test_no_drive() {
  USBHostMSC msc;
  USBHostMscFatIO io(msc);
  fatfs::SDClass sd;
  CHECK(!sd.begin(io));
  USBHostMscFatIO unbound; // begin() never called
  CHECK(!sd.begin(unbound));
}

int main() {
  RUN(test_fat32_raw);
  RUN(test_fat32_mbr);
  RUN(test_fat16_mbr);
  RUN(test_exfat_raw);
  RUN(test_exfat_mbr); // MBR type 0x07
  RUN(test_rejects_ext4);
  RUN(test_no_drive);
  return finish();
}
