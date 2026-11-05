// Device mode: USBDeviceMSC exporting block drivers of TinyFATFS, TinyEXT4
// and a custom class, against a stub of Adafruit_USBD_MSC. The read/write
// plumbing (incl. the unaligned requests of the ESP32 core) is checked, and
// the FAT volume the PC would see is verified with fsck.vfat / mtools.
#include <USBDeviceMSC.h>

#include <vector>

#include "test_common.h"

/// Minimal custom driver: any class with this interface can be exported.
struct VectorDisk {
  explicit VectorDisk(uint32_t sectors) : data((size_t)sectors * 512, 0) {}
  int disk_initialize(uint8_t) { return 0; }
  int disk_status(uint8_t) { return 0; }
  int disk_read(uint8_t, uint8_t *buf, uint32_t lba, unsigned n) {
    memcpy(buf, &data[(size_t)lba * 512], (size_t)n * 512);
    return 0;
  }
  int disk_write(uint8_t, const uint8_t *buf, uint32_t lba, unsigned n) {
    memcpy(&data[(size_t)lba * 512], buf, (size_t)n * 512);
    return 0;
  }
  int disk_ioctl(uint8_t, int cmd, void *buf) {
    if (cmd == 1) { // GET_SECTOR_COUNT
      *(uint32_t *)buf = (uint32_t)(data.size() / 512);
      return 0;
    }
    return cmd == 0 ? 0 : 4; // CTRL_SYNC ok, others: RES_PARERR
  }
  std::vector<uint8_t> data;
};

/// Reads the whole exported drive the way the PC would
static std::string dumpVia(USBDeviceMSC &msc, TmpDir &d, uint32_t sectors) {
  std::string img = d / "export.img";
  FILE *f = fopen(img.c_str(), "wb");
  uint8_t buf[512];
  for (uint32_t s = 0; s < sectors; s++) {
    CHECK_EQ(msc._read(s, 0, buf, 512), 512);
    fwrite(buf, 512, 1, f);
  }
  fclose(f);
  return img;
}

/// First sector of partition 1 if the image has an MBR, else 0 (f_mkfs()
/// creates an MBR partitioned volume by default)
static uint32_t partitionStart(const std::string &img) {
  uint8_t mbr[512] = {0};
  FILE *f = fopen(img.c_str(), "rb");
  CHECK_EQ(fread(mbr, 512, 1, f), (size_t)1);
  fclose(f);
  bool is_mbr = mbr[510] == 0x55 && mbr[511] == 0xAA && mbr[0x1BE + 4] != 0 &&
                memcmp(mbr + 0x36, "FAT", 3) != 0 && memcmp(mbr + 0x52, "FAT", 3) != 0;
  return is_mbr ? (uint32_t)mbr[0x1C6] | (uint32_t)mbr[0x1C7] << 8 |
                      (uint32_t)mbr[0x1C8] << 16 | (uint32_t)mbr[0x1C9] << 24
                : 0;
}

static void test_fat_ramdisk_seen_by_pc() {
  TmpDir d;
  const uint32_t sectors = 256; // 128 KB
  fatfs::RamIO ram(sectors);
  {
    fatfs::SDClass sd;
    CHECK(sd.begin(ram)); // RamIO formats the volume on mount
    CHECK(writeFile(sd, "/hello.txt", "w", "Hello from the board!\n"));
    sd.end(); // the MCU lets go before the PC gets the drive
  }
  USBDeviceMSC msc;
  CHECK(msc.begin(ram));
  std::string img = dumpVia(msc, d, sectors);
  uint32_t start = partitionStart(img);
  std::string part = d / "volume.img";
  sh("dd if='" + img + "' of='" + part + "' bs=512 skip=" + std::to_string(start));
  CHECK_EQ(sh("fsck.vfat -n '" + part + "'"), 0);
  std::string at = "'" + img + "@@" + std::to_string(start * 512) + "'";
  CHECK_EQ(sh("mtype -i " + at + " ::hello.txt | grep -q 'Hello from the board'"), 0);

  // the PC writes a file; the MCU mounts the volume again and reads it
  std::string pc = d / "pc.txt";
  sh("printf 'written by the PC' > '" + pc + "'");
  CHECK_EQ(sh("mcopy -i " + at + " '" + pc + "' ::pc.txt"), 0);
  FILE *f = fopen(img.c_str(), "rb");
  uint8_t buf[512];
  for (uint32_t s = 0; s < sectors; s++) {
    CHECK_EQ(fread(buf, 512, 1, f), (size_t)1);
    CHECK_EQ(msc._write(s, 0, buf, 512), 512);
  }
  fclose(f);
  CHECK(msc.written());
  msc.end();
  fatfs::FatFs fat;
  fat.setDriver(ram);
  fatfs::FATFS vol;
  CHECK_EQ(fat.f_mount(&vol, "", 1), fatfs::FR_OK); // plain mount: no mkfs
  fatfs::FIL fil;
  CHECK_EQ(fat.f_open(&fil, "/pc.txt", FA_READ), fatfs::FR_OK);
  char text[32] = {0};
  UINT n = 0;
  fat.f_read(&fil, text, sizeof(text) - 1, &n);
  fat.f_close(&fil);
  CHECK_EQ(std::string(text), std::string("written by the PC"));
}

static void test_read_write_plumbing() {
  VectorDisk disk(64);
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
  CHECK_EQ(disk.data[3 * 512 + 449], out[449]);        // neighbours untouched
  CHECK_EQ(disk.data[4 * 512 + 37], (uint8_t)0xEE);   // last patched byte
  CHECK_EQ(disk.data[4 * 512 + 38], out[512 + 38]);

  // out of range requests are rejected, not passed to the driver
  CHECK_EQ(msc._read(64, 0, in, 512), -1);
  CHECK_EQ(msc._read(63, 0, in, 1024), -1);
  CHECK_EQ(msc._write(63, 300, patch, 300), -1);
  CHECK_EQ(msc._read(63, 0, in, 512), 512);

  // read-only export rejects writes
  msc.setWritable(false);
  CHECK_EQ(msc._write(3, 0, out, 512), -1);
  msc.end();
  CHECK_EQ(msc._read(0, 0, in, 512), -1); // not exported any more
}

static void test_ext4_ramio_export() {
  tinyext4::RamIO ram(128);
  USBDeviceMSC msc;
  CHECK(msc.begin(ram));
  uint8_t out[512], in[512];
  memset(out, 0x42, sizeof out);
  CHECK_EQ(msc._write(127, 0, out, 512), 512);
  CHECK_EQ(msc._read(127, 0, in, 512), 512);
  CHECK(memcmp(in, out, 512) == 0);
  CHECK_EQ(msc._read(128, 0, in, 512), -1);
  msc.end();
}

static void test_rejects_unusable_drivers() {
  VectorDisk empty(0);
  USBDeviceMSC msc;
  CHECK(!msc.begin(empty)); // no sectors
}

int main() {
  RUN(test_fat_ramdisk_seen_by_pc);
  RUN(test_read_write_plumbing);
  RUN(test_ext4_ramio_export);
  RUN(test_rejects_unusable_drivers);
  return finish();
}
