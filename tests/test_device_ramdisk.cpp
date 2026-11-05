// Device-mode block layer: USBMscRamDisk (FAT format + addFile) and the
// read/write plumbing of USBDeviceMSC, checked with the real fsck/mtools.
#include <USBDeviceMSC.h>

#include <vector>

#include "test_common.h"

static std::string dump(TmpDir &d, USBMscRamDisk &disk, uint32_t blocks) {
  std::string img = d / "ram.img";
  FILE *f = fopen(img.c_str(), "wb");
  fwrite(disk.data(), 512, blocks, f);
  fclose(f);
  return img;
}

static void check_volume(uint32_t blocks) {
  TmpDir d;
  USBMscRamDisk disk(blocks);
  CHECK(disk.begin());
  std::vector<char> big(3000, 'A');
  CHECK(disk.addFile("HELLO.TXT", "hi there\n", 9));
  CHECK(disk.addFile("BIG.BIN", big.data(), big.size()));
  CHECK(disk.addFile("EMPTY", big.data(), 0));
  std::string img = dump(d, disk, blocks);
  CHECK_EQ(sh("fsck.vfat -n '" + img + "'"), 0);
  CHECK_EQ(sh("mtype -i '" + img + "' ::HELLO.TXT | grep -q 'hi there'"), 0);
  CHECK_EQ(sh("test $(mcopy -i '" + img + "' ::BIG.BIN - | wc -c) -eq 3000"), 0);
}

static void test_fat12_volume() { check_volume(512); }
static void test_small_volume() { check_volume(64); }
static void test_fat16_volume() { check_volume(8000); }

static void test_add_file_rejects_bad_input() {
  USBMscRamDisk disk(64);
  CHECK(disk.begin());
  CHECK(!disk.addFile("TOOLONGNAME.TXT", "x", 1));
  CHECK(!disk.addFile("A.TOOL", "x", 1));
  CHECK(!disk.addFile("", "x", 1));
  std::vector<char> huge(64 * 512, 'x');
  CHECK(!disk.addFile("HUGE.BIN", huge.data(), huge.size())); // does not fit
  CHECK(!USBMscRamDisk(8).begin());                           // too small to format
}

static void test_block_bounds() {
  USBMscRamDisk disk(64);
  CHECK(disk.begin(false));
  uint8_t b[1024] = {1};
  CHECK(disk.writeBlocks(62, b, 2));
  CHECK(!disk.writeBlocks(63, b, 2));
  CHECK(!disk.readBlocks(64, b, 1));
}

static void test_device_read_write() {
  USBMscRamDisk disk(64);
  CHECK(disk.begin(false));
  USBDeviceMSC msc;
  CHECK(msc.begin(disk));

  uint8_t out[1024], in[1024];
  for (int i = 0; i < 1024; i++) out[i] = (uint8_t)i;
  CHECK_EQ(msc._write(3, 0, out, 1024), 1024);
  CHECK(msc.written());
  memset(in, 0, sizeof in);
  CHECK_EQ(msc._read(3, 0, in, 1024), 1024);
  CHECK(memcmp(in, out, 1024) == 0);

  // unaligned (offset into a block, partial length), as the ESP32 core may do
  uint8_t patch[100];
  memset(patch, 0xEE, sizeof patch);
  CHECK_EQ(msc._write(3, 450, patch, 100), 100); // spans blocks 3 and 4
  CHECK_EQ(msc._read(3, 450, in, 100), 100);
  CHECK(memcmp(in, patch, 100) == 0);
  CHECK_EQ(disk.data()[3 * 512 + 449], out[449]); // neighbours untouched
  CHECK_EQ(disk.data()[4 * 512 + 37], (uint8_t)0xEE); // last patched byte
  CHECK_EQ(disk.data()[4 * 512 + 38], out[512 + 38]);

  // read-only export rejects writes
  msc.setWritable(false);
  CHECK_EQ(msc._write(3, 0, out, 512), -1);
  CHECK_EQ(msc._read(100, 0, in, 512), -1); // out of range
  msc.end();
}

int main() {
  RUN(test_fat12_volume);
  RUN(test_small_volume);
  RUN(test_fat16_volume);
  RUN(test_add_file_rejects_bad_input);
  RUN(test_block_bounds);
  RUN(test_device_read_write);
  return finish();
}
