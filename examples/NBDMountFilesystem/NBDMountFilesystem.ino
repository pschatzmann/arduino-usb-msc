/*
 * NBDMountFilesystem
 *
 * Mounts a disk that lives on another machine -- a Linux `nbd-server`, qemu,
 * nbdkit or another microcontroller running TinyNBD -- as a FAT or ext2/3/4
 * filesystem and prints /hello.txt from it.
 *
 * The export must already contain a filesystem (optionally inside an MBR
 * partition table). Use a read-only export or a scratch export if you add
 * writes: never let two clients write to the same export.
 *
 * Needs the TinyNBD library (https://github.com/pschatzmann/arduino-nbd).
 * Like the USB host examples, this sketch is built for a board/USB setting
 * where the host stack is available (see USBHostMSC_Info); no USB device
 * has to be plugged in.
 *
 * See the NBD page in the wiki:
 * https://github.com/pschatzmann/arduino-usb-msc/wiki/NBD
 */

#include <WiFi.h>

#include "USBMscAutoFS.h"
#include "USBMscExt4FS.h"
#include "USBMscNbdBlockDevice.h" // includes TinyNBD's NBD.h

const char *ssid = "your-ssid";
const char *password = "your-password";
const char *server_host = "192.168.1.10";
const char *export_name = "disk";

WiFiClient wifi;
nbd::NBDClient nbd_client(wifi);
USBMscNbdBlockDevice remote(nbd_client);
USBMscAutoFS<USBMscExt4FS> autoFs; // FAT or ext4, whichever is on the export

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  if (!nbd_client.connect(server_host, nbd::NBD_DEFAULT_PORT, export_name)) {
    Serial.println("NBD connect failed");
    return;
  }
  Serial.printf("Export: %llu bytes%s\n",
                (unsigned long long)nbd_client.size(),
                nbd_client.isReadOnly() ? " (read-only)" : "");

  if (!autoFs.begin(remote)) {
    Serial.println("No FAT or ext2/3/4 filesystem found on the export");
    return;
  }
  Serial.println(autoFs.isFat() ? "Mounted FAT" : "Mounted ext4");

  File f = autoFs.fs().open("/hello.txt", "r");
  if (!f) {
    Serial.println("Could not open /hello.txt");
    return;
  }
  while (f.available()) {
    Serial.write(f.read());
  }
  f.close();
}

void loop() {}
