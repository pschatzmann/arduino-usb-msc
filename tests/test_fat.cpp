#include "test_common.h"

// Operations every writable backend must get right.
static void exercise_writes(usbmscfs::Backend &be) {
  std::string s;

  CHECK(writeFile(be, "/new.txt", "w", "abc"));
  CHECK(readFile(be, "/new.txt", s));
  CHECK_EQ(s, std::string("abc"));

  CHECK(writeFile(be, "/new.txt", "a", "def"));
  CHECK(readFile(be, "/new.txt", s));
  CHECK_EQ(s, std::string("abcdef"));

  // "w+" must be readable as well as writable
  usbmscfs::BackendFile *f = be.open("/rw.txt", "w+");
  CHECK(f != nullptr);
  if (f) {
    CHECK_EQ(f->write((const uint8_t *)"xyz", 3), (size_t)3);
    CHECK(f->seek(0));
    uint8_t b[8] = {0};
    CHECK_EQ(f->read(b, sizeof(b)), (size_t)3);
    CHECK(memcmp(b, "xyz", 3) == 0);
    f->close();
    delete f;
  }

  // "r+" overwrites in place
  f = be.open("/rw.txt", "r+");
  CHECK(f != nullptr);
  if (f) {
    CHECK(f->seek(1));
    CHECK_EQ(f->write((const uint8_t *)"Q", 1), (size_t)1);
    f->close();
    delete f;
  }
  CHECK(readFile(be, "/rw.txt", s));
  CHECK_EQ(s, std::string("xQz"));

  // "a+" can read too
  f = be.open("/rw.txt", "a+");
  CHECK(f != nullptr);
  if (f) {
    CHECK_EQ(f->write((const uint8_t *)"!", 1), (size_t)1);
    CHECK(f->seek(0));
    uint8_t b[8] = {0};
    CHECK_EQ(f->read(b, sizeof(b)), (size_t)4);
    f->close();
    delete f;
  }

  // directories
  CHECK(be.mkdir("/d2"));
  CHECK(be.isDirectory("/d2"));
  CHECK(!be.isDirectory("/new.txt"));
  CHECK(writeFile(be, "/d2/f.txt", "w", "inside"));
  CHECK(!be.rmdir("/d2")); // not empty
  CHECK(be.rename("/d2/f.txt", "/d2/g.txt"));
  CHECK(!be.exists("/d2/f.txt"));
  CHECK(be.exists("/d2/g.txt"));
  CHECK(be.remove("/d2/g.txt"));
  CHECK(be.rmdir("/d2"));
  CHECK(!be.exists("/d2"));
  CHECK(be.remove("/rw.txt"));
  CHECK(!be.exists("/rw.txt"));
  CHECK(be.exists("/"));
  CHECK(be.isDirectory("/"));
  CHECK(!be.exists("/nope"));
  CHECK(be.open("/nope", "r") == nullptr);

  // a file spanning many clusters / extents
  std::string big(200 * 1024 + 123, 0);
  for (size_t i = 0; i < big.size(); i++) big[i] = (char)(i * 31 + (i >> 8));
  CHECK(writeFile(be, "/big.bin", "w", big));
  CHECK(readFile(be, "/big.bin", s));
  CHECK(s == big);
  usbmscfs::BackendFile *bf = be.open("/big.bin", "r");
  CHECK(bf && bf->size() == big.size());
  if (bf) {
    CHECK(bf->seek(150000));
    uint8_t one;
    CHECK_EQ(bf->read(&one, 1), (size_t)1);
    CHECK_EQ((char)one, big[150000]);
    bf->close();
    delete bf;
  }
}

static void check_prepopulated(usbmscfs::Backend &be) {
  std::string s;
  CHECK(readFile(be, "/hello.txt", s));
  CHECK_EQ(s, std::string("Hello USB\n"));

  std::map<std::string, Entry> m;
  CHECK(listDir(be, "/", m));
  CHECK(m.count("hello.txt") && !m["hello.txt"].is_dir &&
        m["hello.txt"].size == 10);
  CHECK(m.count("dir") && m["dir"].is_dir);

  CHECK(listDir(be, "/dir", m));
  CHECK(m.count("sub.txt") && m["sub.txt"].size == 16);

  CHECK(readFile(be, "/dir/sub.txt", s));
  CHECK_EQ(s, std::string("sub file content"));
}

static void run_case(const char *kind, Layout layout, bool populated) {
  TmpDir d;
  std::string img = makeDisk(d, kind, layout);
  {
    Mounted m;
    CHECK(m.mountFat(img));
    if (!m.be) return;
    if (populated) check_prepopulated(*m.be);
    exercise_writes(*m.be);
    m.unmount();
  }
  CHECK(fsckOk(img, kind, layout != Layout::Raw));

  // everything written must survive a remount
  Mounted m2;
  CHECK(m2.mountFat(img));
  if (m2.be) {
    std::string s;
    CHECK(readFile(*m2.be, "/new.txt", s));
    CHECK_EQ(s, std::string("abcdef"));
  }
}

static void test_fat32_raw() { run_case("fat32", Layout::Raw, true); }
static void test_fat32_mbr() { run_case("fat32", Layout::Mbr, true); }
static void test_fat16_mbr() { run_case("fat16", Layout::Mbr, true); }
static void test_exfat_raw() { run_case("exfat", Layout::Raw, false); }
static void test_exfat_mbr() { run_case("exfat", Layout::Mbr, false); }

static void test_rejects_ext4() {
  TmpDir d;
  std::string img = makeDisk(d, "ext4", Layout::Raw);
  Mounted m;
  CHECK(!m.mountFat(img));
}

int main() {
  RUN(test_fat32_raw);
  RUN(test_fat32_mbr);
  RUN(test_fat16_mbr);
  RUN(test_exfat_raw);
  RUN(test_exfat_mbr); // MBR type 0x07
  RUN(test_rejects_ext4);
  return finish();
}
