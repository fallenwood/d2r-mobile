# Windows ARM64 platform

中文: [README.zh-CN.md](README.zh-CN.md)

**Native Windows ARM64 developer preview, not a fully accepted release.** The complete host loads genuine Windows-target modules, renders class selection and camp through Vulkan on a Qualcomm Adreno X1-85, and has completed an isolated test character's movement, item drag/drop, save/exit and reload across a process restart. The game-independent runtime can also be built and tested without game files. Audible playback, physical controllers, broader gameplay and long-run stability remain unaccepted. The initial class-selection run was approximately 5–6 FPS; performance is not release-ready.

## Requirements

- Windows 10/11 **ARM64**, not x64 or ARM64EC.
- [LLVM-MinGW](https://github.com/mstorsjo/llvm-mingw/releases), **UCRT** distribution with `aarch64-w64-mingw32-clang.exe` and `clang++.exe`. An ARM64-hosted distribution runs the compiler natively. MSVC is not supported by this build.
- CMake 3.22+, Ninja and Python 3.10+. Platform tests use only the Python standard library; Capstone, Unicorn and game files are not required.
- An NTFS build/test volume with alternate data streams and transactional NTFS available. The large-offset test briefly grows a file beyond 5 GiB; allow at least 6 GiB of free disk space.
- For the frontend build: official [Vulkan-Headers](https://github.com/KhronosGroup/Vulkan-Headers) and a **native ARM64** Vulkan loader/driver. No Vulkan SDK import library is assumed: CMake checks the installed loader's PE architecture and derives an ARM64 import library. Runtime-only builds need neither Vulkan headers nor a GPU driver.

## Build and test without game files

Run from the repository root in PowerShell. Keep build output outside the repository:

```powershell
.\dm\windows\build.ps1 `
  -Toolchain C:\Tools\llvm-mingw-ucrt-aarch64 `
  -VulkanHeaders C:\Tools\Vulkan-Headers `
  -PythonPath C:\Tools\Python313\python.exe `
  -BuildDir C:\dm-work\build\windows-arm64
```

Add `-CmakePath C:\Tools\CMake\bin\cmake.exe` and `-NinjaPath C:\Tools\ninja.exe` if they are not on `PATH`. Paths containing spaces must be quoted. The script does not install tools or modify the system `PATH`.

For just the runtime, shared-source compile checks and native tests, add `-RuntimeOnly`; omit `-VulkanHeaders`. Use a separate build directory when changing toolchains.

The equivalent CMake entry is:

```powershell
cmake -S .\dm\windows -B C:\dm-work\build\windows-arm64 -G Ninja `
  -DCMAKE_TOOLCHAIN_FILE=C:\src\d2r-mobile\dm\windows\toolchain.cmake `
  -DLLVM_MINGW=C:\Tools\llvm-mingw-ucrt-aarch64 `
  -DVULKAN_HEADERS=C:\Tools\Vulkan-Headers `
  -DDM_BUILD_HOST=OFF
cmake --build C:\dm-work\build\windows-arm64 --parallel 4
ctest --test-dir C:\dm-work\build\windows-arm64 --output-on-failure
```

Replace the example repository/tool paths with your own. `DM_BUILD_FRONTEND=OFF` removes the Vulkan dependency. `DM_CHECK_SHARED=OFF` disables the extra shared-source compile check; it does not change the runtime tests. Test failures are errors, not skipped successes on unsupported filesystems.

| Check | Actual coverage |
|---|---|
| `windows-runtime` | Native ARM64 execution of synthetic TLS rewrite blocks; TEB preservation and guest x18 gate write-back; TLS isolation; guest/host stack switching and pthread destructors; 5 GiB reservations, protection and recoverable write faults; COFF registration; UTF-8 filenames; positioned I/O beyond 4 GiB without moving the file cursor; aligned allocation; file/directory NTFS metadata; atomic directory exchange and preservation after failure; LP64 calendar layout and post-2038 dates |
| `windows-loader` | The existing synthetic ELF weak/strong import relocation test, using the real shared loader and import resolver |
| `windows-tools` | Platform TLS opcodes, allocated stack saves, refusal of unsupported stack-exclusive windows, Mach-O-to-COFF conversion, rejection of x64/ARM64EC dependencies, and the native loader's acceptance of Windows target 3 and rejection of other/missing targets |
| `windows-desktop`, `windows-touch-default` | Synthetic modules and overlay markers verify keyboard glyph selection, retention of the desktop layout after touch/controller-disconnect requests, contextual Return/Esc mappings, letterbox coordinates and unchanged non-desktop defaults |
| `windows-mouse` | Reuses shared desktop mouse tests: world picks/held steering, UI/item gestures, coalesced hover, capture-loss cancellation and same-poll short clicks |
| Build | Shared host/HLE/graphics sources, actual converted entry/NVN assembly, Windows entry points, and (unless disabled) the real Vulkan engine, replay adapter and Win32 frontend |

`diag.c`, `dm_bright.c` and `dm_vault_ui.c` are excluded from the game-independent shared-source check because they require generated game headers. An object/static-library build is not evidence that the complete executable links or the game starts.

## Platform contracts

- Windows owns **x18 = TEB**. Guest x18/TLS/TLR/gate values use actual `TlsAlloc` slots, not winpthreads internals. The direct slot offsets are validated on the loader and every guest worker. Insufficient direct slots or a layout mismatch aborts rather than reading another thread's state.
- `DM_TARGET=windows` emits **TARGET 3**. Reuse of Apple/Android/HarmonyOS rewrite outputs is refused. Unsupported stack rewrite forms fail explicitly; no below-SP fallback is permitted for Windows. Existing Apple/Android/HarmonyOS paths retain their previous target selection.
- The guest is LP64 and Windows is LLP64. The libc table adapts long conversions and the 56-byte guest `tm` layout; aligned allocations use matching Windows deallocators.
- Save commits require **transactional NTFS**. A single transaction exchanges committed/staged directories. No sequence of visible non-atomic renames is substituted when transactions fail. NTFS alternate streams preserve guest filename casing. Do not use exFAT, FAT, network shares or cloud-synced folders without independently proving the required semantics.
- `DM_ROMFS_OVERLAY` uses **`;`** between Windows paths, for example `C:\mods\first;D:\mods\second`, so drive letters are not split at `:`.
- The frontend uses a resizable Win32 window, an actual Win32 Vulkan surface/swapchain, letterboxed blits, XInput and native Unicode text entry. Closing requests a save before shared shutdown. Audio uses 48 kHz PCM16 WinMM; output failures are logged and follow the shared real-time silent-consumption policy.
- The Windows entry defaults to `DM_APP=1`: desktop/controller layouts with keyboard glyphs, changing to Xbox glyphs on controller input. Mouse touch events do not switch to the mobile layout, whose native button/bar drawing layer is absent on Windows.
- `mach_absolute_time` uses monotonic nanoseconds. Apple retired-instruction counting returns `ENOTSUP` with a diagnostic, not CPU cycles presented as instructions. `DM_SAMPLE` requires POSIX thread signals and fails explicitly on Windows.

Keyboard mapping: WASD = left stick; J/Space = A, K = B, U = X, I = Y; F/G = stick clicks; Q/E = shoulders; Z/C = triggers; `+` = Plus; Tab = Minus; arrows = D-pad. Return confirms, opens name entry or creates according to the current page. Esc goes back, or opens the function page in game with no panel open. Left mouse, drag and hover use the desktop gestures shared with macOS: frontend clicks, item operations, world picks/held steering, not raw Switch touchscreen injection. Presses in letterbox bars are ignored; focus/capture loss cancels a held gesture without invalidating an already released, queued click/drop. Opening/closing native text entry releases input state so dialog-consumed key-up events do not leave stuck keys. Right mouse = B, wheel = a right-stick pulse. XInput uses Xbox glyphs and dead zones. This is not a complete reproduction of PC-edition mouse gameplay.

## Prepare, build and launch with your own game files

Input must be the merged Switch **1.0.34.0 / 3.3.94012** update, containing `ExeFS\main`, `ExeFS\sdk` and `RomFS\data`. The preparation entry checks the executable hashes. It needs Python 3.10+ with `pillow numpy lz4 capstone unicorn`, .NET SDK 8+, LLVM-MinGW UCRT, and recent `spirv-opt.exe` (with `--trim-capabilities`), `spirv-val.exe` and `glslc.exe`. An x64 Python/tool process is acceptable for offline preparation: the script matches its decoder DLL to that Python, while the game host remains native ARM64. No tools are installed automatically.

```powershell
python .\dm\windows\prepare.py C:\games\Update C:\dm-work\windows `
  --toolchain C:\Tools\llvm-mingw-ucrt-aarch64 `
  --spirv-tools C:\Tools\VulkanSDK\Bin --jobs 4 --verify

.\dm\windows\build.ps1 `
  -Toolchain C:\Tools\llvm-mingw-ucrt-aarch64 `
  -VulkanHeaders C:\Tools\Vulkan-Headers `
  -BuildDir C:\dm-work\windows\build `
  -BuildHost -GeneratedHeaders C:\dm-work\windows\generated

.\dm\windows\run.ps1 -Root C:\dm-work\windows -GameDir C:\games\Update
```

Replace tool paths as needed; `python` must be the interpreter with those dependencies. Preparation regenerates the Windows target-3 modules, the three genuine headers, the desktop overlay/keyboard glyphs, 6,927 translated shaders and four source-built query kernels. It reuses the shared generators' asset cache, but does not skip module/shader regeneration merely because old output exists. .NET intermediates, decoder DLLs, assets and per-step logs (`prepare-logs`) all stay under the external root. No mobile native-art output is written into the checkout. `--verify` adds the full per-site equivalence and mutation checks; omit it only to skip that extra verification step.

`run.ps1 -CheckOnly` validates the executable's ARM64 PE architecture, Windows module targets and all 6,931 expected XGS1 bundles without launching. Normal launch sets process-local resource paths and restores the caller's environment on exit. Saves default to `<Root>\saves`; use `-SaveDir` for a separate smoke-test directory. Do not run two hosts against the same save directory. `-Executable` selects a different build. Close the running host before rebuilding its executable.

The genuine `main`/`sdk` outputs passed 25,084/11,975 sites respectively, at least two effective trials per site, with zero divergences or below-SP reads; each module detected all 300 deliberate mutations. This is not a Windows port of the complete `regress.sh`/`structck.py` suite; see [the Windows verifier scope](../verify/README.md#windows-arm64-standalone-equivalence).

The interactive native smoke check used an isolated level-1 offline standard Druid: mouse class selection, Return opening native name entry with Unicode text entered then cancelled, and Esc navigation; in camp, ground-click movement, opening panels, mouse tab navigation, moving the blue Town Portal scroll from the inventory's bottom-right to top-left, Save and Exit, and normal host shutdown. A fresh process loaded the character and entered camp with the scroll still at top-left, proving more than the default character-creation save. This covers basic input and the save cycle, not a full playthrough, combat, every item operation or performance acceptance. No game content, generated game headers, binaries or tool downloads are included here.
