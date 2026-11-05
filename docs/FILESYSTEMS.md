# Filesystems: USBMscFatFS / USBMscExt4FS / USBMscExt4FSReadOnly / USBMscAutoFS

`USBMscFatFS` and `USBMscExt4FS`/`USBMscExt4FSReadOnly` all work on
**ESP32, RP2040, and STM32** (F4/F7 OTG_FS parts -- see
[STM32.md](STM32.md)). `USBMscFSPool` works on all three too.
`USBMscAutoFS` is **ESP32 and RP2040 only** for now (see below).

On ESP32/RP2040 these build on the core's `fs::FS`/`File`. STM32duino has
no such abstraction, so there each class is a self-contained
implementation returning `USBMscFile` (a `Stream`-based handle shaped like
`fs::File`/STM32SD's own `File`, but not named `File` to avoid colliding
with STM32SD if a sketch also uses an actual SD card) instead of
`fs::File`, and `fs()` returns a reference to itself instead of a
separate `fs::FS`. The method names and call shape
(`fatFs.fs().open(path, mode)`) are the same either way -- see
`examples/USBMscFatFS_ListFiles`, `examples/USBMscFS_Ext4ReadFile` and
`examples/USBMscFS_Ext4WriteFile`, which all build unchanged on all
three platforms via a small per-platform type alias.

`USBMscAutoFS` isn't ported to STM32 yet: it needs `USBMscFatFS::fs()`
and `USBMscExt4FSBase::fs()` to return the same type so it can pick
between them at runtime, and on STM32 today they return two unrelated
self-reference types (`USBMscFatFS&` vs `USBMscExt4FSBase&`) rather than
a common `fs::FS&`. `USBMscFSPool` doesn't hit this problem -- it only
ever wraps one `FSType` per instance, never picks between two, so its
`fs()` can just deduce whatever that single type returns (`decltype(auto)`
-- see `USBMscFSPool.h`), which works out to the same type on every
platform.

## exFAT

`USBMscFatFS` also mounts exFAT drives, on all three platforms -- FatFs
handles FAT12/16/32 and exFAT transparently through the same
`f_open()`/`f_read()`/`f_mount()` calls the backend already uses, so no
code differs, only a config flag (`FF_FS_EXFAT`, on in TinyFATFS's
`src/ff/ffconf.h`, which every platform shares). Enabling
it costs a modest amount of extra RAM per mounted instance (an internal
directory-entry scratch buffer FatFs needs for exFAT's larger directory
records; roughly 600 bytes) and a few KB of flash for the exFAT code
paths -- paid whether or not a given sketch ever actually mounts an
exFAT drive, since it's all compiled into the same functions as the
FAT12/16/32 code, not something the linker can strip independently.

Each filesystem backend is its own class, implementing `USBMscFSBackend`.
Hand one to `USBHostMSC`'s constructor and it gets mounted/unmounted
automatically as devices come and go, before your `onMount()`/`onUnmount()`
callbacks run:

```cpp
#include "USBHostMSC.h"
#include "USBMscFatFS.h"

USBMscFatFS fatFs;
USBHostMSC msc(&fatFs);   // auto-mounts fatFs on device mount

void onMount(uint8_t dev_addr) {
  if (!fatFs.mounted()) {
    Serial.println("no FAT filesystem found");
    return;
  }
  File f = fatFs.fs().open("/hello.txt", "r");
  // ... use f like any other Arduino File
}

void setup() {
  Serial.begin(115200);
  msc.onMount(onMount);
  msc.begin();
}

void loop() { msc.task(); }
```

`begin()` reads the MBR (if any), picks the first partition of the
matching type (or treats the whole disk as one partition if there's no
MBR), checks the filesystem signature, and mounts it. `USBMscExt4FS` and
`USBMscExt4FSReadOnly` are the same shape, for ext2/3/4 -- see
[Ext4 read-only vs read/write](#ext4-read-only-vs-readwrite) below for
which to use.

`USBMscAutoFS<ExtType = USBMscExt4FSReadOnly>` composes `USBMscFatFS` +
`ExtType`, tries FAT then ext4, and mounts whichever one the drive is
actually formatted as:

```cpp
USBMscAutoFS<> mscFs;                // FAT (r/w) + ext4 (read-only)
USBMscAutoFS<USBMscExt4FS> mscFsRW;  // FAT (r/w) + ext4 (r/w)
```

`isFat()`/`isExt4()` report which one mounted. The template defaults to
the read-only ext4 class for the same reason `USBMscAutoFS` doesn't just
always use `USBMscExt4FS`: auto-mounting whatever drive happens to be
plugged in shouldn't risk writing to it unless you say so by naming
`USBMscExt4FS` explicitly. See `examples/USBMscFS_ListFiles`,
`examples/USBMscFatFS_ListFiles`, `examples/USBMscFS_Ext4ReadFile` and
`examples/USBMscFS_Ext4WriteFile`.

**Partition tables:** `USBMscFatFS` supports MBR-partitioned and
unpartitioned (superfloppy) media only. GPT disks (typically >2 TB, or many
macOS-formatted drives) will not mount as FAT -- reformat with an MBR
table. The ext4 classes additionally find ext partitions on **GPT** disks
(TinyEXT4 scans MBR and GPT itself). exFAT drives formatted on
Windows/macOS (MBR type `0x07`) are recognised.

## Unplugging while files are open

Pulling a drive tears the filesystem down immediately. Any `File`
objects you still hold are force-closed (reads return 0, writes fail,
`openNextFile()` returns an empty handle) and are safe to destroy, but
unflushed data is lost -- call `flush()`/`close()` before the drive can
be removed. Blocking block I/O in flight when the drive disappears
returns `false` rather than hanging; every command is also bounded by
`USB_HOST_MSC_IO_TIMEOUT_MS` (default 10000, override with a build flag).

## Backend selection: it's just which class you reference

There is **no build flag and no config header.** Which backend(s) end up
in your firmware is decided purely by which of these classes your sketch
actually instantiates; the linker discards the rest.

| Backend | Class | Library | License | Read/Write |
|---|---|---|---|---|
| FAT12/16/32, exFAT | `USBMscFatFS` | [TinyFATFS](https://github.com/pschatzmann/TinyFATFS) (ChaN FatFs) | MIT wrapper + permissive FatFs license | read/write |
| ext2/3/4 | `USBMscExt4FSReadOnly` | [TinyEXT4](https://github.com/pschatzmann/TinyEXT4) | MIT | read-only |
| ext2/3/4 | `USBMscExt4FS` | [TinyEXT4](https://github.com/pschatzmann/TinyEXT4) | MIT | read/write (ext4 only, see below) |

Both libraries are header-only and are installed as ordinary Library
Manager dependencies (`depends=` in `library.properties`). Each is
included from exactly one translation unit of this library
(`src/fs/USBMscFatBackend.cpp` / `src/fs/USBMscExt4Backend.cpp`), so
neither leaks into your sketch's namespace. Everything in the dependency
chain is permissively licensed (MIT; FatFs's own BSD-1-Clause-style
license needs no attribution for binary-only redistribution), so
linking either filesystem does not place your firmware under any copyleft
terms.

## Ext4 read-only vs read/write

`USBMscExt4FS` and `USBMscExt4FSReadOnly` are thin subclasses of the same
`USBMscExt4FSBase`; the only difference is the `readOnly` flag passed to
TinyEXT4's `Ext4FS::begin()`.

TinyEXT4 **does not write a journal**. Consequences:

- A volume that has a pending journal recovery / was not cleanly
  unmounted is mounted **read-only**, whatever you asked for. Run
  `e2fsck` on a Linux machine to fix it.
- **ext2 and ext3** volumes are always mounted read-only.
- ext4 volumes using features TinyEXT4 can't write (inline data,
  bigalloc, encryption, casefold, quota) are also mounted read-only.
- If the drive is pulled (or power is lost) in the middle of a write, the
  volume needs `e2fsck` the next time it is plugged into Linux.

After mounting, `readOnly()` on the filesystem object tells you what you
actually got -- check it before relying on writes:

```cpp
if (extFs.mounted() && extFs.readOnly()) {
  Serial.println("mounted read-only (ext2/3, needs fsck, or unsupported feature)");
}
```

Writes are flushed when you `flush()`/`close()` a file (and on `mkdir`,
`remove`, `rename`), so close files before the drive can be removed.
Prefer `USBMscExt4FSReadOnly` whenever you only need to read: a bug in
your sketch then structurally can't corrupt the drive.

Files created or modified on ext4 get a modification time of 0 on
Arduino unless the platform's `time()` is set (TinyEXT4's default).

## API

### USBMscFatFS / USBMscExt4FS / USBMscExt4FSReadOnly

- `bool begin(USBHostMSC &msc, uint8_t dev_addr)` -- normally called for
  you via the `USBHostMSC` constructor; call directly only if you're not
  using that auto-mount hook
- `bool mounted()`
- `bool readOnly()` -- (ext4 classes) true if the volume ended up mounted read-only
- `fs::FS &fs()` -- the mounted filesystem, valid once `mounted()`. On
  STM32, this returns a self-reference (`USBMscFatFS&` or
  `USBMscExt4FSBase&`) instead, since there's no separate
  `fs::FS`-equivalent type -- `open()`/`exists()`/`remove()`/`rename()`/
  `mkdir()`/`rmdir()` are directly on the class there, and `open()`
  returns a `USBMscFile` instead of a `fs::File`.
- `void end()`

### USBMscAutoFS<ExtType>

Same as above, plus:

- `bool isFat() const`, `bool isExt4() const`
