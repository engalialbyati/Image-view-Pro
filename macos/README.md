# Image Viewer Pro — macOS port

A native macOS build of Image Viewer Pro, written in Objective-C++ on AppKit /
CoreGraphics / ImageIO, and sharing the same portable C++ image-processing core
(`../core/ImageCore.h`) as the Windows build.

> The Windows build remains the fully-featured reference. The macOS port shares
> the same portable C++ core and now matches the main image-editing features:
> open, folder navigation, rotate, **Adjust photo** (sliders), **Scan document**,
> **Crop** (rectangle + perspective), zoom/pan, selection with a thumbnail
> sidebar, move-to-Trash, multi-image **Export as PDF**, and undo/redo.
> The chrome is a native Mac toolbar rather than a Windows-style ribbon, and
> hotkey settings / event log / set-as-default are not yet ported.

## Install (first time on macOS)

You need **macOS 11 (Big Sur) or newer**.

### Option A — Download the prebuilt app

1. Go to the latest release:
   https://github.com/engalialbyati/Image-view-Pro/releases/latest
2. Under **Assets**, download **`ImageViewerPro-macOS-<version>.zip`**.
3. Unzip it (double-click). You get **`ImageViewerPro.app`**.
4. Drag `ImageViewerPro.app` into **`/Applications`** (or wherever you like).

> The app is **not code-signed or notarized** (it's built from source, not an
> Apple Developer ID build). The first time you open it, macOS Gatekeeper will
> stop it. Open it with one of these:

**Easy way (Finder):**
1. In Finder, locate `ImageViewerPro.app`.
2. **Right-click** (or Control-click) it → choose **Open**.
3. In the “cannot be opened” dialog, click **Open** again. You only have to do this once.

**Terminal way (removes the quarantine flag for everyone on this Mac):**
```sh
xattr -dr com.apple.quarantine /Applications/ImageViewerPro.app
```
Then double-click the app normally.

### Option B — Build from source

Requires the **Xcode Command Line Tools**:
```sh
xcode-select --install   # if you don't have them yet
```
Then:
```sh
git clone https://github.com/engalialbyati/Image-view-Pro.git
cd Image-view-Pro/macos
make
open build/ImageViewerPro.app
```
A locally-built copy is already trusted by Gatekeeper, so no “Open Anyway” step
is needed.

### Verifying it runs

Launch it → **File → Open…** (⌘O), or drag an image onto the window. Use
**← / →** to move through the folder, **R / ⇧R** to rotate, scroll/pinch to
zoom, and click-drag to pan when zoomed in.

## Continuous builds

Every push that changes `core/` or `macos/` triggers a fresh macOS build on
GitHub Actions, and the `.app` is uploaded as a workflow artifact:
https://github.com/engalialbyati/Image-view-Pro/actions
(download the latest “Build (macOS)” run → “ImageViewerPro-macOS”). Useful if you
want a bleeding-edge build between releases.

---

## Shortcuts

- **File → Open…** (⌘O) or **drag an image onto the window**.
- **← / →** previous / next image in the folder.
- **R** / **⇧R** rotate.
- **⌘+ / ⌘− / ⌘0** zoom in / out / fit. Scroll or pinch to zoom; click-drag to pan.
- **S** toggle selection, **⌘A** select all, **⌘D** clear.
- Toolbar **PDF** exports the selected images to a multi-page PDF; **Trash** moves
  the selected images to the Trash.

## Project layout

| File | Purpose |
|---|---|
| `main.mm` | App entry, menu bar, window creation |
| `IVPWindow.h/.mm` | Window: toolbar, selection sidebar, image view, status bar |
| `IVPImageView.h/.mm` | Image display with zoom/pan (clipped to the canvas) |
| `IVPDocument.h/.mm` | Model: ImageIO loading, folder list, selection, history, rotate, Trash, PDF export |
| `Info.plist` | Bundle metadata + image document types |
| `Makefile` | `make` → `.app` |
| `../core/ImageCore.h` | Shared portable core (adjust / warp / scan / corners / rotate / PDF) |

## Notes

- CI: `.github/workflows/build-macos.yml` builds the app on every push that
  touches `core/` or `macos/` and uploads the `.app` as an artifact — so you get
  a build even without a local Mac.
- The app is not code-signed/notarized. To run a locally-built copy on Apple
  Silicon you may need to right-click → **Open** the first time, or run:
  `xattr -dr com.apple.quarantine build/ImageViewerPro.app`.
