# WDD – Windows `dd` Clone

**WDD** is a lightweight, fast, and native Windows implementation of the classic Unix `dd` utility.
It’s written in C, supports Unicode paths, and provides powerful low‑level block copying and disk imaging features.

---

## Features

* ✅ Native Windows API (`ReadFile` / `WriteFile`) for performance
* ✅ Unicode path support (wide character)
* ✅ Raw device access (`\\.\PHYSICALDRIVE#`) for imaging disks and USB drives
* ✅ Sparse file support (`conv=sparse`)
* ✅ Standard `dd`‑style options: `bs`, `count`, `skip`, `seek`, `conv`, `iflag`, `oflag`, `status`
* ✅ `--help` and `--version` support
* ✅ Optionally packaged via an installer (InnoSetup)

---

## Downloads

Get the latest installers and binaries from the **Releases** section:

[GitHub Releases](https://github.com/Noyb747/WDD/releases)

Each release contains:

* `wdd_setup.exe` – Windows installer

---

## Installation

Run `wdd_setup.exe` and follow the wizard to install WDD system‑wide (e.g., `C:\Program Files\WDD`).

> ⚠️ To access raw physical devices like `\\.\PHYSICALDRIVE0`, you **must run your terminal as Administrator**.

---

## Usage Examples

### Copy a File

```powershell
wdd if=input.file of=output.file bs=1M
```

### Block Size, Skip, and Seek

```powershell
wdd if=source.img of=target.img bs=4M skip=1 seek=1
```

### Show Progress

```powershell
wdd if=input of=output bs=1M status=progress
```

### Direct I/O (Fast)

```powershell
wdd if=input of=output bs=4M iflag=direct oflag=direct
```

### Sparse Output

```powershell
wdd if=input of=output conv=sparse
```

### Help / Version

```powershell
wdd --help
wdd --version
```

---

## Safety & Best Practices

⚠️ **WDD operates at a low level.** Mistargeted `of=` can overwrite entire disks without confirmation. Carefully verify your device paths before running.

* Always run **as Administrator** for raw disk access.
* Disconnect drives you do not intend to touch to avoid accidents.
* Use large block sizes (`bs=4M` or higher) for better performance on large copies.

---

## Build Instructions

To build `wdd` yourself using MinGW:

```bash
gcc wdd.c -o wdd.exe -std=c11 -O2 -Wall -Wextra -Wpedantic \
  -DWIN32_LEAN_AND_MEAN -municode -static-libgcc -s
```

* `-municode` enables Unicode entry point (`wmain`).
* `-static-libgcc` produce a standalone exe.

Or just use the `compile.bat`.

---

## License

WDD is released under the **MIT License**.

---

## Contributing

Contributions are welcome!

* ⭐ Star the project
* 🐛 Report issues
* 🧠 Suggest enhancements
* 💻 Submit pull requests
