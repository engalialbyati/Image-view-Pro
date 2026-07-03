# Image Viewer Pro

A **native Windows** image viewer written in C++ (Win32 API + GDI+ + WIC).
Compiles to a single, self-contained `.exe` with no runtime dependencies.

## Features

- **Open anything** — JPG, PNG, GIF, BMP, TIFF, WEBP, HEIC, AVIF, SVG, … (decoded with WIC, the same engine as Windows Photos)
- **Rename in place** — press `Enter`, type, `Enter` again (click away also commits)
- **Rotate** — `R` (right) / `Shift+R` (left); auto-saved
- **Crop**
  - `C` — draw a rectangle
  - `Shift+C` — auto edge-detect + draggable 4-corner perspective crop (keystone/tilt correction)
- **Scan** — `Z` enters scan mode (CamScanner-style document enhance); `Up`/`Down` sets intensity, `Enter` saves
- **Undo** — `Ctrl+Z` (reverts crop/scan/rotate while on the same image)
- **Select & organize** — `S` toggles selection, `Ctrl+A` all, `Ctrl+C`/`Ctrl+X` copy/cut files to paste anywhere
- **Browse a folder** — `Left`/`Right` to move between images
- **Configurable hotkeys** — *Edit → Set Hotkeys…* (persisted to `%APPDATA%\ImageViewerPro\settings.ini`)
- **Set as default viewer** + open files from Explorer / drag & drop
- Modern dark UI, DPI-aware, drag & drop, command-line open

## Build

Requires **MinGW-w64 GCC** (e.g. WinLibs UCRT). Static-linked, no redistributables needed.

```powershell
powershell -ExecutionPolicy Bypass -File build.ps1
```

Outputs `Image Viewer Pro.exe`.

## Default shortcuts

| Action | Key |
|---|---|
| Rename | `Enter` |
| Select / Deselect | `S` |
| Select All / Clear | `Ctrl+A` / `Ctrl+D` |
| Copy / Cut | `Ctrl+C` / `Ctrl+X` |
| Rotate Right / Left | `R` / `Shift+R` |
| Previous / Next | `Left` / `Right` |
| Zoom In / Out / Fit | `Ctrl++` / `Ctrl+-` / `Ctrl+0` |
| Crop (Rectangle) | `C` |
| Crop (Perspective / Auto) | `Shift+C` |
| Scan (Document) | `Z` |
| Undo | `Ctrl+Z` |
| Open / Save | `Ctrl+O` / `Ctrl+S` |

All shortcuts are customizable from **Edit → Set Hotkeys…**.

## Project layout

| File | Purpose |
|---|---|
| `main.cpp` | Application source |
| `app.rc` / `app.manifest` | Resources + manifest (visual styles, DPI) |
| `build.ps1` | MinGW build script |
