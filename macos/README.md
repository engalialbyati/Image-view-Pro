# Image Viewer Pro — macOS port

A native macOS build of Image Viewer Pro, written in Objective-C++ on AppKit /
CoreGraphics / ImageIO, and sharing the same portable C++ image-processing core
(`../core/ImageCore.h`) as the Windows build.

> The Windows build remains the fully-featured reference. This macOS port
> currently implements: open, folder navigation, rotate, zoom/pan, selection
> with a thumbnail sidebar, move-to-Trash, multi-image **Export as PDF**, and
> undo/redo. Perspective crop, document-scan, and the adjustments panel exist in
> the core and are pending UI wiring.

## Build

Requires **macOS 11+** and the **Xcode Command Line Tools** (`xcode-select --install`).

```sh
cd macos
make
open build/ImageViewerPro.app
```

The Makefile produces `build/ImageViewerPro.app` (a normal `.app` bundle).

## Run

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
