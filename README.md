# MyArk

A Windows kernel (KMDF/WDM) driver paired with a Qt 6 GUI for enumerating and
inspecting kernel objects, callbacks, and internal data structures.

> **For research and educational use only.** The driver performs undocumented
> kernel operations (pattern scanning, object table walking, arbitrary kernel
> memory reads). Loading it on a system you do not own or are not authorized to
> test may violate policy or law.

## Features

Kernel enumeration (R0) with a user-mode viewer (R3):

- Object / process / thread / image notify routines
- `ObRegisterCallbacks` registrations
- Registry (`CmRegisterCallback`) callbacks
- Timers (DPC and IO)
- SSDT and Shadow SSDT
- IDT (per CPU)
- System threads and worker threads (ExPool / IoPool)
- Minifilters and their callbacks
- Kernel object types (+ per-type `_OBJECT_TYPE_INITIALIZER` procedures)
- WFP filters and callouts
- Loaded drivers, per-driver device list and IRP dispatch table
- Unloaded drivers (`MmUnloadedDrivers`, `PiDDBCacheTable`, CI hash buckets)
- BugCheck callbacks, last-chance shutdown, notify shutdown callbacks
- Kernel memory read + Capstone-based disassembly

The GUI refreshes on tab switch, supports right-click refresh / open-in-explorer /
disassemble, and every table supports header sorting and Ctrl+C copy.

## Layout

```
myarkproject/
├── KMDF Driver1/         # Kernel driver (WDM, x64)
├── MyArkQtVer/           # Qt 6 GUI (x64)
├── third_party/capstone/ # Capstone 5 headers + prebuilt lib/dll
├── myarkproject.sln      # Combined solution (driver + GUI)
├── KMDF Driver1.sln
└── MyArkQtVer.slnx
```

## Requirements

- Windows 10/11 x64
- Visual Studio 2022/2026 with the Windows Driver Kit (WDK)
- Qt 6.11.1 (msvc2022_64)

## Build

Open `myarkproject.sln` in Visual Studio and build both projects (x64), or:

```
msbuild myarkproject.sln /p:Configuration=Release /p:Platform=x64
```

The driver is test-signed (`SignMode=TestSign`); load it with test signing
enabled. The GUI copies `capstone.dll` next to the executable after build.

## Protocol

R0 and R3 share `protocol.h` (keep the two copies in sync). IOCTL codes use
`CTL_CODE(FILE_DEVICE_UNKNOWN=0x22, ...)` with `METHOD_BUFFERED`. The response
header carries `Status`, `Count`, `TotalSize`, and `ProtocolVersion` (currently 3);
the GUI refuses data whose version does not match.

## License

MIT — see [LICENSE](LICENSE).
