# Host mode quick start (read a USB stick)

```cpp
#include "USBHostMSC.h"
#include "USBMscFatFS.h"

USBMscFatFS fatFs;
USBHostMSC msc(&fatFs);   // auto-mounts fatFs when a drive is plugged in

void onMount(uint8_t dev_addr) {
  if (fatFs.mounted()) {
    File f = fatFs.fs().open("/hello.txt", "r");
    while (f.available()) Serial.write(f.read());   // Serial1 on RP2040/STM32
  }
}

void setup() {
  Serial.begin(115200);   // Serial1 on RP2040/STM32
  msc.onMount(onMount);
  msc.begin();
}

void loop() { msc.task(); }
```

More: [Block API](BLOCK_API.md), [Filesystems](FILESYSTEMS.md), [Hubs](MULTIPLE_DEVICES.md), [Boards](BOARDS.md).
