// Unplug / teardown while files are open: the use-after-free fixed in the
// Backend file registry. Meaningful under AddressSanitizer (the default).
#include "test_common.h"

enum class Kind { Fat, Ext4 };

static bool mount(Mounted &m, Kind k, const std::string &img) {
  return k == Kind::Fat ? m.mountFat(img) : m.mountExt(img, false);
}
static const char *kindName(Kind k) { return k == Kind::Fat ? "fat32" : "ext4"; }

static void open_files_survive_backend(Kind k) {
  TmpDir d;
  std::string img = makeDisk(d, kindName(k), Layout::Mbr);
  usbmscfs::BackendFile *wf, *rf, *df;
  {
    Mounted m;
    CHECK(mount(m, k, img));
    if (!m.be) return;
    wf = m.be->open("/pending.txt", "w");
    rf = m.be->open("/hello.txt", "r");
    df = m.be->open("/", "d");
    CHECK(wf && rf && df);
    if (!(wf && rf && df)) return;
    CHECK_EQ(wf->write((const uint8_t *)"persist me", 10), (size_t)10);
    // no close(): the backend goes away first ("drive unplugged")
    auto alive = m.be->aliveToken();
    CHECK(*alive);
    m.unmount();
    CHECK(!*alive);
  }
  // the orphaned handles must be inert and safe to use and destroy
  uint8_t b[16];
  char name[64];
  bool is_dir;
  uint64_t sz;
  CHECK_EQ(rf->read(b, sizeof(b)), (size_t)0);
  CHECK_EQ(wf->write((const uint8_t *)"x", 1), (size_t)0);
  CHECK(!df->nextEntry(name, sizeof(name), is_dir, sz));
  CHECK(!rf->seek(0));
  df->rewindDir();
  wf->flush();
  wf->close();
  rf->close();
  df->close();
  delete wf;
  delete rf;
  delete df;

  // ...and the data written before the teardown reached the disk
  Mounted m2;
  CHECK(mount(m2, k, img));
  if (m2.be) {
    std::string s;
    CHECK(readFile(*m2.be, "/pending.txt", s));
    CHECK_EQ(s, std::string("persist me"));
  }
}
static void test_fat_open_files_survive_backend() { open_files_survive_backend(Kind::Fat); }
static void test_ext4_open_files_survive_backend() { open_files_survive_backend(Kind::Ext4); }

static void unplug_mid_use(Kind k) {
  TmpDir d;
  std::string img = makeDisk(d, kindName(k), Layout::Raw);
  Mounted m;
  CHECK(mount(m, k, img));
  if (!m.be) return;
  usbmscfs::BackendFile *rf = m.be->open("/hello.txt", "r");
  CHECK(rf != nullptr);
  m.msc.detach(); // transfers now fail, like a pulled drive
  uint8_t b[16];
  if (rf) {
    CHECK_EQ(rf->read(b, sizeof(b)), (size_t)0);
    rf->close();
    delete rf;
  }
  CHECK(!writeFile(*m.be, "/after.txt", "w", "x"));
  m.unmount(); // must not hang or crash flushing to a dead device
}
static void test_fat_unplug_mid_use() { unplug_mid_use(Kind::Fat); }
static void test_ext4_unplug_mid_use() { unplug_mid_use(Kind::Ext4); }

static void io_errors_dont_crash(Kind k) {
  TmpDir d;
  std::string img = makeDisk(d, kindName(k), Layout::Raw);
  Mounted m;
  CHECK(mount(m, k, img));
  if (!m.be) return;
  m.msc.failAfter(3); // the device starts failing in the middle of a write
  std::string big(100000, 'z');
  (void)writeFile(*m.be, "/err.bin", "w", big);
  m.msc.failAfter(-1);
  m.unmount();
}
static void test_fat_io_errors() { io_errors_dont_crash(Kind::Fat); }
static void test_ext4_io_errors() { io_errors_dont_crash(Kind::Ext4); }

int main() {
  RUN(test_fat_open_files_survive_backend);
  RUN(test_ext4_open_files_survive_backend);
  RUN(test_fat_unplug_mid_use);
  RUN(test_ext4_unplug_mid_use);
  RUN(test_fat_io_errors);
  RUN(test_ext4_io_errors);
  return finish();
}
