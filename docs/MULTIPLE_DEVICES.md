# Hubs and multiple devices

`USBHostMSC` itself is hub-ready: TinyUSB's hub class driver is enabled
in the config on all three ports, and every mount/unmount callback and
every block-level call (`readBlocks()`, `writeBlocks()`, ...) is already
keyed by `dev_addr`, so multiple mass-storage devices behind a hub each
enumerate with their own address and work independently -- no extra
setup needed if you're using `USBHostMSC` directly.

`USBMscFatFS`/`USBMscExt4FS`/`USBMscExt4FSReadOnly`/`USBMscAutoFS` (see
[FILESYSTEMS.md](FILESYSTEMS.md)) are each a **single instance** holding
at most one mounted `fs::FS`, though. Hand one to `USBHostMSC`'s
constructor and plug in a hub with two flash drives, and the second
device's mount event tears down and replaces the first drive's mounted
filesystem, rather than mounting both at once.

For multiple simultaneous filesystems, use `USBMscFSPool<FSType,
MaxDevices>` instead -- it holds `MaxDevices` instances of `FSType`
(any of the classes above) in a fixed-size array (no heap), routing
`begin`/`end` by `dev_addr`:

```cpp
#include "USBHostMSC.h"
#include "USBMscFSPool.h"
#include "USBMscFatFS.h"

USBMscFSPool<USBMscFatFS, 4> pool;   // up to 4 FAT drives at once
USBHostMSC msc(&pool);

void onMount(uint8_t dev_addr) {
  if (pool.mounted(dev_addr)) {
    File f = pool.fs(dev_addr).open("/hello.txt", "r");
    // ...
  }
}

void setup() {
  msc.onMount(onMount);
  msc.begin();
}

void loop() { msc.task(); }
```

See `examples/USBHostMSC_FSPool_HubListFiles`.

Size `MaxDevices` to how many devices you actually expect (a typical hub
has 4 ports) -- every slot's RAM/flash cost (FatFs buffers, TinyEXT4
caches, ...) is paid `MaxDevices` times over, whether or not that many
devices are ever plugged in. If you expect more devices than that, or
more than one hub, also check `CFG_TUH_HUB` / `CFG_TUH_DEVICE_MAX` in the
active `tusb_config_*.h` for your platform -- they cap how many devices
TinyUSB itself will track, independent of `MaxDevices` here.

FAT is limited to 4 simultaneous volumes by FatFs' `FF_VOLUMES` (set in
TinyFATFS's `ffconf.h`); each mounted ext4 volume costs TinyEXT4's small
sector cache (a few KB). A pool larger than the FatFs limit will fail to
mount the extra FAT drives.

A hub also adds power draw on top of the drives themselves. A
self-powered hub (its own power supply) sidesteps the question; whether
your board's host port can supply enough current for a bus-powered hub
plus multiple drives is a hardware question this library can't answer
for you.

## API

### USBMscFSPool<FSType, MaxDevices>

- `bool begin(USBHostMSC &msc, uint8_t dev_addr)` -- normally called for
  you via the `USBHostMSC` constructor
- `void end(uint8_t dev_addr)`
- `bool mounted(uint8_t dev_addr)`
- `fs::FS &fs(uint8_t dev_addr)` -- **only** call when `mounted(dev_addr)`
  is true (undefined behaviour otherwise)
- `FSType *get(uint8_t dev_addr)` -- checked lookup; `nullptr` if not mounted
- `uint8_t mountedCount()`
- `uint8_t deviceAddress(uint8_t index)` -- `dev_addr` of the index-th
  occupied slot (0-based), or 0 if out of range
