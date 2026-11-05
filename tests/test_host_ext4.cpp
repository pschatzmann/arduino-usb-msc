#include "test_common.h"

// (same write-path checks as test_fat.cpp, against the ext4 backend)
static void exercise_writes(usbmscfs::Backend &be) {
  std::string s;
  CHECK(writeFile(be, "/new.txt", "w", "abc"));
  CHECK(writeFile(be, "/new.txt", "a", "def"));
  CHECK(readFile(be, "/new.txt", s));
  CHECK_EQ(s, std::string("abcdef"));

  usbmscfs::BackendFile *f = be.open("/rw.txt", "w+");
  CHECK(f != nullptr);
  if (f) {
    CHECK_EQ(f->write((const uint8_t *)"xyz", 3), (size_t)3);
    CHECK(f->seek(0));
    uint8_t b[8] = {0};
    CHECK_EQ(f->read(b, sizeof(b)), (size_t)3);
    f->close();
    delete f;
  }
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

  CHECK(be.mkdir("/d2"));
  CHECK(be.isDirectory("/d2"));
  CHECK(writeFile(be, "/d2/f.txt", "w", "inside"));
  CHECK(!be.rmdir("/d2"));
  CHECK(be.rename("/d2/f.txt", "/d2/g.txt"));
  CHECK(!be.exists("/d2/f.txt"));
  CHECK(be.remove("/d2/g.txt"));
  CHECK(be.rmdir("/d2"));
  CHECK(be.remove("/rw.txt"));
  CHECK(!be.exists("/nope"));
  CHECK(be.open("/nope", "r") == nullptr);

  std::string big(300 * 1024 + 77, 0);
  for (size_t i = 0; i < big.size(); i++) big[i] = (char)(i * 17 + (i >> 9));
  CHECK(writeFile(be, "/big.bin", "w", big));
  CHECK(readFile(be, "/big.bin", s));
  CHECK(s == big);
}

static void check_prepopulated(usbmscfs::Backend &be) {
  std::string s;
  CHECK(readFile(be, "/hello.txt", s));
  CHECK_EQ(s, std::string("Hello USB\n"));
  std::map<std::string, Entry> m;
  CHECK(listDir(be, "/", m));
  CHECK(m.count("hello.txt") && !m["hello.txt"].is_dir);
  CHECK_EQ(m["hello.txt"].size, (uint64_t)10); // real size, not 0
  CHECK(m.count("dir") && m["dir"].is_dir);
  CHECK(!m.count(".") && !m.count(".."));
  CHECK(listDir(be, "/dir", m));
  CHECK(m.count("sub.txt") && m["sub.txt"].size == 16);
  CHECK(be.isDirectory("/dir"));
  CHECK(be.exists("/"));
}

static void rw_case(Layout layout) {
  TmpDir d;
  std::string img = makeDisk(d, "ext4", layout);
  {
    Mounted m;
    CHECK(m.mountExt(img, false));
    if (!m.be) return;
    CHECK(!m.be->isReadOnly());
    check_prepopulated(*m.be);
    exercise_writes(*m.be);
    m.unmount();
  }
  CHECK(fsckOk(img, "ext4", layout != Layout::Raw));
  Mounted m2;
  CHECK(m2.mountExt(img, true));
  if (m2.be) {
    std::string s;
    CHECK(readFile(*m2.be, "/new.txt", s));
    CHECK_EQ(s, std::string("abcdef"));
  }
}

static void test_ext4_raw() { rw_case(Layout::Raw); }
static void test_ext4_mbr() { rw_case(Layout::Mbr); }
static void test_ext4_gpt() { rw_case(Layout::Gpt); } // found by TinyEXT4

static void test_read_only_request() {
  TmpDir d;
  std::string img = makeDisk(d, "ext4", Layout::Raw);
  Mounted m;
  CHECK(m.mountExt(img, true));
  if (!m.be) return;
  CHECK(m.be->isReadOnly());
  check_prepopulated(*m.be);
  CHECK(!writeFile(*m.be, "/x.txt", "w", "no"));
  CHECK(!writeFile(*m.be, "/hello.txt", "a", "no"));
  CHECK(!m.be->mkdir("/nd"));
  CHECK(!m.be->remove("/hello.txt"));
  CHECK(!m.be->rename("/hello.txt", "/h2.txt"));
  std::string s;
  CHECK(readFile(*m.be, "/hello.txt", s));
  CHECK_EQ(s, std::string("Hello USB\n"));
}

static void test_ext2_is_read_only_even_if_rw_requested() {
  TmpDir d;
  std::string img = makeDisk(d, "ext2", Layout::Mbr);
  Mounted m;
  CHECK(m.mountExt(img, false));
  if (!m.be) return;
  CHECK(m.be->isReadOnly());
  check_prepopulated(*m.be);
  CHECK(!writeFile(*m.be, "/x.txt", "w", "no"));
}

static void test_rejects_fat() {
  TmpDir d;
  std::string img = makeDisk(d, "fat32", Layout::Mbr);
  Mounted m;
  CHECK(!m.mountExt(img, false));
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
