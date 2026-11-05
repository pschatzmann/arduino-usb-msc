// ext2/3/4 on a USB drive: USBHostMscExt4IO + tinyext4::Ext4FS, whole disk,
// MBR and GPT (partition lookup is done by TinyEXT4).
#include "test_common.h"

static void exercise_writes(tinyext4::Ext4FS &fs) {
  std::string s;
  CHECK(writeFile(fs, "/new.txt", "w", "abc"));
  CHECK(writeFile(fs, "/new.txt", "a", "def"));
  CHECK(readFile(fs, "/new.txt", s));
  CHECK_EQ(s, std::string("abcdef"));

  {
    tinyext4::File f = fs.open("/rw.txt", "w+");
    CHECK((bool)f);
    CHECK_EQ(f.write((const uint8_t *)"xyz", 3), (size_t)3);
    CHECK(f.seek(0));
    uint8_t b[8] = {0};
    CHECK_EQ(f.read(b, sizeof(b)), (size_t)3);
    f.close();
  }
  {
    tinyext4::File f = fs.open("/rw.txt", "r+");
    CHECK((bool)f);
    CHECK(f.seek(1));
    CHECK_EQ(f.write((const uint8_t *)"Q", 1), (size_t)1);
    f.close();
  }
  CHECK(readFile(fs, "/rw.txt", s));
  CHECK_EQ(s, std::string("xQz"));

  CHECK(fs.mkdir("/d2"));
  CHECK(fs.open("/d2", "r").isDirectory());
  CHECK(writeFile(fs, "/d2/f.txt", "w", "inside"));
  CHECK(!fs.rmdir("/d2"));
  CHECK(fs.rename("/d2/f.txt", "/d2/g.txt"));
  CHECK(!fs.exists("/d2/f.txt"));
  CHECK(fs.remove("/d2/g.txt"));
  CHECK(fs.rmdir("/d2"));
  CHECK(fs.remove("/rw.txt"));
  CHECK(!fs.exists("/nope"));
  CHECK(!fs.open("/nope", "r"));

  std::string big(300 * 1024 + 77, 0);
  for (size_t i = 0; i < big.size(); i++) big[i] = (char)(i * 17 + (i >> 9));
  CHECK(writeFile(fs, "/big.bin", "w", big));
  CHECK(readFile(fs, "/big.bin", s));
  CHECK(s == big);
}

static void check_prepopulated(tinyext4::Ext4FS &fs) {
  std::string s;
  CHECK(readFile(fs, "/hello.txt", s));
  CHECK_EQ(s, std::string("Hello USB\n"));
  std::map<std::string, Entry> m;
  CHECK(listDir(fs, "/", m));
  CHECK(m.count("hello.txt") && !m["hello.txt"].is_dir);
  CHECK_EQ(m["hello.txt"].size, (uint64_t)10);
  CHECK(m.count("dir") && m["dir"].is_dir);
  CHECK(!m.count(".") && !m.count(".."));
  CHECK(listDir(fs, "/dir", m));
  CHECK(m.count("sub.txt") && m["sub.txt"].size == 16);
}

static void rw_case(Layout layout) {
  TmpDir d;
  std::string img = makeDisk(d, "ext4", layout);
  {
    Drive drive;
    CHECK(drive.plug(img));
    USBHostMscExt4IO io(drive.msc);
    tinyext4::Ext4FS fs;
    CHECK(fs.begin(io));
    CHECK(!fs.isReadOnly());
    check_prepopulated(fs);
    exercise_writes(fs);
    fs.end();
  }
  CHECK(fsckOk(img, "ext4", layout != Layout::Raw));
  Drive drive;
  CHECK(drive.plug(img));
  USBHostMscExt4IO io(drive.msc, Drive::kDev);
  tinyext4::Ext4FS fs;
  CHECK(fs.begin(io, true));
  std::string s;
  CHECK(readFile(fs, "/new.txt", s));
  CHECK_EQ(s, std::string("abcdef"));
}

static void test_ext4_raw() { rw_case(Layout::Raw); }
static void test_ext4_mbr() { rw_case(Layout::Mbr); }
static void test_ext4_gpt() { rw_case(Layout::Gpt); }

static void test_read_only_request() {
  TmpDir d;
  std::string img = makeDisk(d, "ext4", Layout::Raw);
  Drive drive;
  CHECK(drive.plug(img));
  USBHostMscExt4IO io(drive.msc);
  tinyext4::Ext4FS fs;
  CHECK(fs.begin(io, true));
  CHECK(fs.isReadOnly());
  check_prepopulated(fs);
  unsigned writes = drive.msc.writes;
  CHECK(!writeFile(fs, "/x.txt", "w", "no"));
  CHECK(!writeFile(fs, "/hello.txt", "a", "no"));
  CHECK(!fs.mkdir("/nd"));
  CHECK(!fs.remove("/hello.txt"));
  CHECK(!fs.rename("/hello.txt", "/h2.txt"));
  fs.end();
  CHECK_EQ(drive.msc.writes, writes);
}

static void test_ext2_is_read_only_even_if_rw_requested() {
  TmpDir d;
  std::string img = makeDisk(d, "ext2", Layout::Mbr);
  Drive drive;
  CHECK(drive.plug(img));
  USBHostMscExt4IO io(drive.msc);
  tinyext4::Ext4FS fs;
  CHECK(fs.begin(io, false));
  CHECK(fs.isReadOnly());
  check_prepopulated(fs);
  CHECK(!writeFile(fs, "/x.txt", "w", "no"));
}

static void test_rejects_fat() {
  TmpDir d;
  std::string img = makeDisk(d, "fat32", Layout::Mbr);
  Drive drive;
  CHECK(drive.plug(img));
  USBHostMscExt4IO io(drive.msc);
  tinyext4::Ext4FS fs;
  CHECK(!fs.begin(io, false));
}

int main() {
  RUN(test_ext4_raw);
  RUN(test_ext4_mbr);
  RUN(test_ext4_gpt);
  RUN(test_read_only_request);
  RUN(test_ext2_is_read_only_even_if_rw_requested);
  RUN(test_rejects_fat);
  return finish();
}
