# WDD – Windows `dd` Clone

**WDD** is a lightweight, high-performance Windows-native clone of the classic Unix `dd` utility, written in C. It supports:

- Copying files and raw disks.
- Custom block sizes (`bs`), skip/seek, count limits.
- Sparse files and `conv` options (`sync`, `noerror`, `notrunc`, `sparse`).
- Direct I/O for high-speed transfers.
- Unicode paths on Windows.
- Optional progress/status reporting.

---

## Features

- ✅ Native Windows API (`ReadFile` / `WriteFile`) for speed.
- ✅ Supports raw device access (`\\.\PHYSICALDRIVE#`), USB drives, and standard files.
- ✅ Handles large files efficiently.
- ✅ Fully supports `--help` and `--version`.
- ✅ Sparse file support for optimized space usage.
- ✅ Lightweight single executable with optional InnoSetup installer.

---

## Installation

Download the latest `wdd` release from [Releases](https://github.com/yourusername/wdd/releases).

**Option 1: Portable**

- Extract `wdd.exe` to any folder.
- Run directly from a terminal.

**Option 2: Installer**

- Run `wdd_setup.exe` created via InnoSetup.
- Follow the wizard to install in `C:\Program Files\WDD` (default).
- Add the installation folder to your `PATH` if desired.

> ⚠️ **Important:** To access raw disks (e.g., `\\.\PHYSICALDRIVE#`), run as Administrator.

---

## Usage

### Basic File Copy

```powershell
wdd if=input.file of=output.file bs=1M
