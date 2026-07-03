#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define _WIN32_WINNT 0x0A00
#define WINVER 0x0A00

#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <commctrl.h>
#include <commdlg.h>
#include <dwmapi.h>
#include <objbase.h>
#include <wincodec.h>
#include <gdiplus.h>

#include <string>
#include <vector>
#include <set>
#include <algorithm>
#include <cmath>

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif
#define DWMWCP_ROUND 2
#define PI 3.14159265358979323846f

#define ID_OPEN        1001
#define ID_SAVE        1002
#define ID_RENAME      1003
#define ID_ROTL        1004
#define ID_ROTR        1005
#define ID_CROP        1006
#define ID_ZIN         1007
#define ID_ZOUT        1008
#define ID_ZFIT        1009
#define ID_PREV        1010
#define ID_NEXT        1011
#define ID_EXIT        1012
#define ID_ABOUT       1013
#define ID_SHOWFOLDER  1014
#define ID_DEFAULT     1015
#define ID_SELALL      1016
#define ID_SELCLR      1017
#define ID_SETTINGS    1018
#define ID_UNDO        1019
#define ID_SCAN        1020
#define ID_CROPPERSP   1021
#define IDC_EDITNAME   2001
#define IDC_CHECKSEL   2002

#define IDC_HKBASE 3000
#define IDC_RESET  3050
#define IDC_DONE   3051
#define IDC_CANCEL 3052

#define IC_NONE 0
#define IC_PREV 1
#define IC_NEXT 2
#define IC_ROTL 3
#define IC_ROTR 4
#define IC_CROP 5
#define IC_ZOUT 6
#define IC_ZFIT 7
#define IC_ZIN  8

enum { ACT_RENAME, ACT_SELECT, ACT_SELECTALL, ACT_CLEARSEL, ACT_COPY, ACT_CUT,
       ACT_ROTATER, ACT_ROTATEL, ACT_PREV, ACT_NEXT, ACT_ZOOMIN, ACT_ZOOMOUT,
       ACT_ZOOMFIT, ACT_CROP, ACT_CROPPERSP, ACT_SCAN, ACT_UNDO, ACT_OPEN, ACT_SAVE, ACT_COUNT };

struct Hotkey { const WCHAR* name; UINT vk; bool ctrl; bool shift; bool alt; };

static const Hotkey g_defaultHotkeys[ACT_COUNT] = {
    { L"Rename",            VK_RETURN,     false, false, false },
    { L"Select / Deselect", 'S',           false, false, false },
    { L"Select All",        'A',           true,  false, false },
    { L"Clear Selection",   'D',           true,  false, false },
    { L"Copy",              'C',           true,  false, false },
    { L"Cut",               'X',           true,  false, false },
    { L"Rotate Right",      'R',           false, false, false },
    { L"Rotate Left",       'R',           false, true,  false },
    { L"Previous Image",    VK_LEFT,       false, false, false },
    { L"Next Image",        VK_RIGHT,      false, false, false },
    { L"Zoom In",           VK_OEM_PLUS,   true,  false, false },
    { L"Zoom Out",          VK_OEM_MINUS,  true,  false, false },
    { L"Fit to Window",     '0',           true,  false, false },
    { L"Crop (Rectangle)",  'C',           false, false, false },
    { L"Crop (Perspective)", 'C',          false, true,  false },
    { L"Scan (Document)",   'Z',           false, false, false },
    { L"Undo",              'Z',           true,  false, false },
    { L"Open",              'O',           true,  false, false },
    { L"Save",              'S',           true,  false, false },
};

static const WCHAR* const g_imageExts[] = {
    L".jpg", L".jpeg", L".jpe", L".jfif", L".png", L".gif", L".bmp", L".dib",
    L".tif", L".tiff", L".webp", L".svg", L".ico", L".heic", L".heif", L".wmf", L".emf"
};

static const Gdiplus::Color C_BG(255, 16, 16, 24);
static const Gdiplus::Color C_BAR(255, 26, 26, 38);
static const Gdiplus::Color C_BAR2(255, 32, 32, 48);
static const Gdiplus::Color C_BTN(255, 46, 46, 66);
static const Gdiplus::Color C_BTNHOV(255, 64, 64, 96);
static const Gdiplus::Color C_ACCENT(255, 90, 146, 255);
static const Gdiplus::Color C_ACCENT2(255, 116, 168, 255);
static const Gdiplus::Color C_TEXT(255, 238, 239, 247);
static const Gdiplus::Color C_DIM(255, 138, 140, 162);
static const Gdiplus::Color C_SEP(255, 52, 52, 74);

static HINSTANCE g_hInst = nullptr;
static HWND      g_hMain = nullptr;
static HWND      g_hEdit = nullptr;
static HWND      g_hCheck = nullptr;
static HFONT     g_hFont = nullptr;
static HFONT     g_hFontBold = nullptr;
static HFONT     g_hFontName = nullptr;
static HBRUSH    g_hbrChip = nullptr;
static HWND      g_btn[16] = {0};
static HWND      g_hoverBtn = nullptr;
static HMENU     g_hMenu = nullptr;

static Gdiplus::Bitmap* g_bmp = nullptr;
static std::vector<Gdiplus::Bitmap*> g_undoStack;

static std::wstring g_path;
static std::wstring g_dir;
static std::wstring g_ext;
static std::vector<std::wstring> g_files;
static int  g_index = -1;
static std::set<std::wstring> g_selected;

static bool g_editing  = false;
static bool g_cropping = false;
static bool g_dragging = false;
static bool g_dirty    = false;
static bool g_committing = false;
static double g_zoom   = 1.0;

static double g_cropCorners[4][2] = {{0,0},{0,0},{0,0},{0,0}};
static int g_dragCorner = -1;
static bool g_perspCrop = false;
static double g_rcX0 = 0, g_rcY0 = 0, g_rcX1 = 0, g_rcY1 = 0;

static bool g_scanMode = false;
static int g_scanLevel = 70;
static Gdiplus::Bitmap* g_scanBase = nullptr;
static std::vector<unsigned char> g_scanBaseBuf;
static std::vector<unsigned char> g_scanFullBuf;

static double g_ox = 0, g_oy = 0, g_scale = 1;
static HBITMAP g_dispCache = nullptr;
static int g_cacheW = 0, g_cacheH = 0;
static int g_canvasTop = 0, g_canvasLeft = 0, g_canvasW = 0, g_canvasH = 0;
static int g_tbh = 64;
static int g_statusH = 0, g_statusTop = 0;
static double g_dpi = 96.0;
static RECT g_chipRect = {0,0,0,0};

static std::wstring g_sLeft, g_sRight;
static std::wstring g_openOnStart;

static Hotkey g_hotkeys[ACT_COUNT];
static HWND g_hSettings = nullptr;
static Hotkey g_work[ACT_COUNT];
static int g_captureIdx = -1;
static HWND g_hkBtn[ACT_COUNT];
static HWND g_label[ACT_COUNT];

struct BtnDef { const WCHAR* text; int id; int w; bool primary; bool icon; int iconId; bool groupStart; };
static BtnDef g_btns[] = {
    { L"Open",   ID_OPEN,   82, false, false, IC_NONE, true  },
    { L"",       ID_PREV,   42, false, true,  IC_PREV, true  },
    { L"",       ID_NEXT,   42, false, true,  IC_NEXT, false },
    { L"",       ID_ROTL,   42, false, true,  IC_ROTL, true  },
    { L"",       ID_ROTR,   42, false, true,  IC_ROTR, false },
    { L"",       ID_CROP,   42, false, true,  IC_CROP, false },
    { L"",       ID_ZOUT,   42, false, true,  IC_ZOUT, true  },
    { L"",       ID_ZFIT,   46, false, true,  IC_ZFIT, false },
    { L"",       ID_ZIN,    42, false, true,  IC_ZIN,  false },
    { L"Scan",   ID_SCAN,   72, false, false, IC_NONE, true  },
    { L"Save",   ID_SAVE,   88, true,  false, IC_NONE, true  },
    { L"Folder", ID_SHOWFOLDER, 78, false, false, IC_NONE, false },
};
static const int g_nBtns = sizeof(g_btns) / sizeof(g_btns[0]);

static int DPI(int v) { return (int)(v * g_dpi / 96.0 + 0.5); }

static std::wstring Lower(const std::wstring& s) {
    std::wstring r = s;
    for (auto& c : r) c = (WCHAR)towlower(c);
    return r;
}
static std::wstring ExtOf(const std::wstring& path) {
    return Lower(PathFindExtensionW(path.c_str()));
}
static bool IsImageExt(const std::wstring& ext) {
    for (auto e : g_imageExts) if (ext == e) return true;
    return false;
}
static std::wstring NameOnly(const std::wstring& path) {
    LPCWSTR f = PathFindFileNameW(path.c_str());
    std::wstring n = f;
    LPCWSTR e = PathFindExtensionW(n.c_str());
    if (e && *e) n = n.substr(0, e - n.c_str());
    return n;
}
static std::wstring SanitizeName(const std::wstring& in) {
    std::wstring r;
    for (auto c : in) {
        if (c == L'<' || c == L'>' || c == L':' || c == L'"' || c == L'/' ||
            c == L'\\' || c == L'|' || c == L'?' || c == L'*') r += L'_';
        else r += c;
    }
    int n = (int)r.size();
    while (n > 0 && (r[n - 1] == L'.' || r[n - 1] == L' ')) n--;
    r.resize(n);
    return r;
}
static std::wstring ExePath() {
    WCHAR buf[MAX_PATH] = {0};
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    return buf;
}
static bool IsCurrentSelected() {
    return !g_path.empty() && g_selected.count(g_path) > 0;
}

static std::wstring vkName(UINT vk) {
    switch (vk) {
        case VK_RETURN: return L"Enter";
        case VK_LEFT: return L"Left";   case VK_RIGHT: return L"Right";
        case VK_UP: return L"Up";       case VK_DOWN: return L"Down";
        case VK_SPACE: return L"Space"; case VK_DELETE: return L"Delete";
        case VK_BACK: return L"Backspace"; case VK_TAB: return L"Tab";
        case VK_HOME: return L"Home";   case VK_END: return L"End";
        case VK_PRIOR: return L"PgUp";  case VK_NEXT: return L"PgDn";
        case VK_OEM_PLUS: return L"+";  case VK_OEM_MINUS: return L"-";
        case VK_OEM_COMMA: return L","; case VK_OEM_PERIOD: return L".";
    }
    if (vk >= '0' && vk <= '9') return std::wstring(1, (WCHAR)vk);
    if (vk >= 'A' && vk <= 'Z') return std::wstring(1, (WCHAR)vk);
    if (vk >= VK_F1 && vk <= VK_F24) return L"F" + std::to_wstring(vk - VK_F1 + 1);
    return L"Key";
}
static std::wstring KeyLabel(const Hotkey& h) {
    std::wstring s;
    if (h.ctrl) s += L"Ctrl+";
    if (h.shift) s += L"Shift+";
    if (h.alt) s += L"Alt+";
    s += vkName(h.vk);
    return s;
}
static int MatchAction(UINT vk, bool ctrl, bool shift, bool alt) {
    for (int i = 0; i < ACT_COUNT; i++)
        if (g_hotkeys[i].vk == vk && g_hotkeys[i].ctrl == ctrl && g_hotkeys[i].shift == shift && g_hotkeys[i].alt == alt)
            return i;
    return -1;
}

static int GetEncoderClsid(const WCHAR* mime, CLSID* pClsid) {
    UINT num = 0, size = 0;
    Gdiplus::GetImageEncodersSize(&num, &size);
    if (size == 0) return -1;
    Gdiplus::ImageCodecInfo* ci = (Gdiplus::ImageCodecInfo*)malloc(size);
    if (!ci) return -1;
    Gdiplus::GetImageEncoders(num, size, ci);
    int found = -1;
    for (UINT k = 0; k < num; k++) {
        if (wcscmp(ci[k].MimeType, mime) == 0) { found = (int)k; *pClsid = ci[k].Clsid; break; }
    }
    free(ci);
    return found;
}
static const WCHAR* MimeForExt(const std::wstring& ext, bool& canEncode) {
    canEncode = true;
    if (ext == L".jpg" || ext == L".jpeg" || ext == L".jpe" || ext == L".jfif") return L"image/jpeg";
    if (ext == L".png") return L"image/png";
    if (ext == L".gif") return L"image/gif";
    if (ext == L".bmp" || ext == L".dib") return L"image/bmp";
    if (ext == L".tif" || ext == L".tiff") return L"image/tiff";
    canEncode = false;
    return L"image/png";
}

static void UpdateStatus();
static void InvalidateChrome();
static void RebuildDisplayCache();
static void DoSave();
static void DoRotate(bool right);
static void DoCropApply();
static void ZoomBy(double f);
static void DoUndo();
static void EnterScanMode();
static void AdjustScan(int delta);
static void CommitScan();
static void CancelScan();
static void EnterCropRect();
static void EnterCropPersp();
static void DispatchAction(int act);
static void OpenSettings();
static void LoadHotkeys();
static void SaveHotkeys();
static void UpdateMenuHotkeys();

static void UpdateEditName() {
    if (g_path.empty()) { SetWindowTextW(g_hEdit, L"No image opened"); return; }
    SetWindowTextW(g_hEdit, NameOnly(g_path).c_str());
}
static void UpdateTitle() {
    std::wstring t = L"Image Viewer Pro";
    if (!g_path.empty()) { t += L"  \u2014  "; t += PathFindFileNameW(g_path.c_str()); }
    SetWindowTextW(g_hMain, t.c_str());
}
static void EnableButtons(bool on) {
    const int ids[] = { ID_SAVE, ID_ROTL, ID_ROTR, ID_CROP, ID_SCAN, ID_ZIN, ID_ZOUT, ID_ZFIT, ID_DEFAULT, ID_SELALL, ID_SELCLR, ID_UNDO };
    for (auto id : ids) {
        HWND h = GetDlgItem(g_hMain, id);
        if (h) EnableWindow(h, on);
    }
    HWND hp = GetDlgItem(g_hMain, ID_PREV);
    HWND hn = GetDlgItem(g_hMain, ID_NEXT);
    BOOL nav = (g_files.size() > 1) ? TRUE : FALSE;
    if (hp) EnableWindow(hp, nav && on);
    if (hn) EnableWindow(hn, nav && on);
}

static void RebuildFolderList() {
    g_files.clear();
    g_index = -1;
    if (g_dir.empty()) return;
    std::wstring spec = g_dir + L"\\*";
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(spec.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        std::wstring full = g_dir + L"\\" + fd.cFileName;
        if (IsImageExt(ExtOf(full))) g_files.push_back(full);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    std::sort(g_files.begin(), g_files.end(),
              [](const std::wstring& a, const std::wstring& b) {
                  return StrCmpLogicalW(a.c_str(), b.c_str()) < 0;
              });
    std::wstring cur = Lower(g_path);
    for (size_t i = 0; i < g_files.size(); i++)
        if (Lower(g_files[i]) == cur) { g_index = (int)i; break; }
    std::set<std::wstring> kept;
    for (const auto& p : g_selected) if (PathFileExistsW(p.c_str())) kept.insert(p);
    g_selected.swap(kept);
}

static void ClearUndo() {
    for (auto* b : g_undoStack) delete b;
    g_undoStack.clear();
}

static void CloseImage() {
    if (g_scanMode) {
        delete g_bmp; g_bmp = g_scanBase; g_scanBase = nullptr;
        g_scanBaseBuf.clear(); g_scanBaseBuf.shrink_to_fit();
        g_scanFullBuf.clear(); g_scanFullBuf.shrink_to_fit();
        g_scanMode = false;
    }
    delete g_bmp; g_bmp = nullptr;
    ClearUndo();
    g_path.clear(); g_dir.clear(); g_ext.clear();
    g_files.clear(); g_index = -1;
    g_dirty = false; g_cropping = false; g_perspCrop = false; g_zoom = 1.0;
    if (g_dispCache) { DeleteObject(g_dispCache); g_dispCache = nullptr; }
}

static Gdiplus::Bitmap* MakeOwnedCopy(Gdiplus::Bitmap* src) {
    if (!src) return nullptr;
    INT w = (INT)src->GetWidth(), h = (INT)src->GetHeight();
    if (w <= 0 || h <= 0) return nullptr;
    Gdiplus::Bitmap* dst = new Gdiplus::Bitmap(w, h, PixelFormat32bppARGB);
    Gdiplus::Rect r(0, 0, w, h);
    Gdiplus::BitmapData sd = {}, dd = {};
    if (src->LockBits(&r, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &sd) != Gdiplus::Ok) { delete dst; return nullptr; }
    if (dst->LockBits(&r, Gdiplus::ImageLockModeWrite, PixelFormat32bppARGB, &dd) != Gdiplus::Ok) { src->UnlockBits(&sd); delete dst; return nullptr; }
    INT bytes = w * 4;
    for (INT y = 0; y < h; y++)
        memcpy((BYTE*)dd.Scan0 + y * dd.Stride, (BYTE*)sd.Scan0 + y * sd.Stride, bytes);
    dst->UnlockBits(&dd);
    src->UnlockBits(&sd);
    return dst;
}

static Gdiplus::Bitmap* LoadViaWIC(const std::wstring& path) {
    IWICImagingFactory* factory = nullptr;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))) || !factory)
        return nullptr;
    IWICBitmapDecoder* decoder = nullptr;
    if (FAILED(factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ,
            WICDecodeMetadataCacheOnLoad, &decoder)) || !decoder) { factory->Release(); return nullptr; }
    IWICBitmapFrameDecode* frame = nullptr;
    if (FAILED(decoder->GetFrame(0, &frame)) || !frame) { decoder->Release(); factory->Release(); return nullptr; }

    IWICFormatConverter* conv = nullptr;
    if (FAILED(factory->CreateFormatConverter(&conv)) || !conv) { frame->Release(); decoder->Release(); factory->Release(); return nullptr; }
    if (FAILED(conv->Initialize(frame, GUID_WICPixelFormat32bppBGRA,
            WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom))) {
        conv->Release(); frame->Release(); decoder->Release(); factory->Release(); return nullptr;
    }
    UINT w = 0, h = 0;
    conv->GetSize(&w, &h);
    decoder->Release(); factory->Release();
    if (w == 0 || h == 0) { frame->Release(); conv->Release(); return nullptr; }

    UINT stride = w * 4;
    UINT size = stride * h;
    BYTE* buf = new (std::nothrow) BYTE[size];
    if (!buf) { frame->Release(); conv->Release(); return nullptr; }
    HRESULT hr = conv->CopyPixels(nullptr, stride, size, buf);
    frame->Release(); conv->Release();
    if (FAILED(hr)) { delete[] buf; return nullptr; }

    Gdiplus::Bitmap* src = new Gdiplus::Bitmap((INT)w, (INT)h, (INT)stride, PixelFormat32bppARGB, buf);
    Gdiplus::Bitmap* owned = MakeOwnedCopy(src);
    delete src;
    delete[] buf;
    return owned;
}

static bool LoadImageFromPath(const std::wstring& path) {
    if (g_scanMode) CancelScan();
    g_cropping = false; g_perspCrop = false;
    Gdiplus::Bitmap* b = LoadViaWIC(path);
    if (!b) {
        Gdiplus::Bitmap* tmp = Gdiplus::Bitmap::FromFile(path.c_str(), FALSE);
        if (tmp && tmp->GetLastStatus() == Gdiplus::Ok)
            b = MakeOwnedCopy(tmp);
        delete tmp;
    }
    if (!b) return false;

    delete g_bmp; g_bmp = b;
    ClearUndo();
    g_path = path;
    g_dir = path.substr(0, PathFindFileNameW(path.c_str()) - path.c_str());
    while (!g_dir.empty() && (g_dir.back() == L'\\' || g_dir.back() == L'/')) g_dir.pop_back();
    g_ext = ExtOf(path);
    g_dirty = false; g_cropping = false; g_dragging = false; g_dragCorner = -1;
    g_zoom = 1.0;
    g_editing = false;
    SendMessageW(g_hEdit, EM_SETREADONLY, TRUE, 0);

    RebuildFolderList();
    UpdateEditName();
    UpdateTitle();
    EnableButtons(true);
    if (g_hCheck) InvalidateRect(g_hCheck, nullptr, FALSE);
    RebuildDisplayCache();
    InvalidateRect(g_hMain, nullptr, FALSE);
    UpdateStatus();
    return true;
}

static void SwitchToImage(int idx) {
    int n = (int)g_files.size();
    if (n == 0) return;
    for (int k = 0; k < n; k++) {
        int j = ((idx + k) % n + n) % n;
        if (g_index >= 0 && j == g_index) continue;
        if (LoadImageFromPath(g_files[j])) return;
        if (g_index < 0) break;
    }
    g_sLeft = L"Could not open more images in this folder.";
    InvalidateChrome();
}

static void DoOpen() {
    WCHAR file[MAX_PATH] = {0};
    OPENFILENAMEW ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hMain;
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"All Images\0*.jpg;*.jpeg;*.jpe;*.jfif;*.png;*.gif;*.bmp;*.dib;*.tif;*.tiff;*.webp;*.svg;*.ico;*.heic;*.heif;*.wmf;*.emf\0All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    if (GetOpenFileNameW(&ofn)) LoadImageFromPath(file);
}

static void ZoomBy(double f) {
    if (!g_bmp) return;
    g_zoom *= f;
    if (g_zoom > 40) g_zoom = 40;
    if (g_zoom < 0.05) g_zoom = 0.05;
    RebuildDisplayCache();
    InvalidateRect(g_hMain, nullptr, FALSE);
    UpdateStatus();
}

static void RebuildDisplayCache() {
    if (g_dispCache) { DeleteObject(g_dispCache); g_dispCache = nullptr; }
    g_cacheW = 0; g_cacheH = 0;
    if (!g_bmp) return;
    int cw = g_canvasW, ch = g_canvasH;
    if (cw <= 0 || ch <= 0) return;
    INT iw = (INT)g_bmp->GetWidth(), ih = (INT)g_bmp->GetHeight();
    double fit = (std::min)((double)cw / iw, (double)ch / ih);
    if (fit <= 0) fit = 1;
    double scale = fit * g_zoom;
    int dw = (int)(iw * scale), dh = (int)(ih * scale);
    int ox = (cw - dw) / 2;
    int oy = (ch - dh) / 2;
    g_ox = (double)ox; g_oy = (double)(g_canvasTop + oy); g_scale = scale;
    Gdiplus::Bitmap cache(cw, ch, PixelFormat32bppARGB);
    Gdiplus::Graphics gc(&cache);
    gc.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
    Gdiplus::SolidBrush bgf(Gdiplus::Color(255, 16, 16, 24));
    gc.FillRectangle(&bgf, 0, 0, cw, ch);
    gc.DrawImage(g_bmp, Gdiplus::Rect(ox, oy, dw, dh), 0, 0, iw, ih, Gdiplus::UnitPixel);
    HBITMAP hb = nullptr;
    cache.GetHBITMAP(Gdiplus::Color(0, 0, 0, 0), &hb);
    g_dispCache = hb;
    g_cacheW = cw; g_cacheH = ch;
}

static void InitCropCorners() {
    if (!g_bmp) return;
    double W = (double)g_bmp->GetWidth(), H = (double)g_bmp->GetHeight();
    g_cropCorners[0][0] = 0; g_cropCorners[0][1] = 0;
    g_cropCorners[1][0] = W; g_cropCorners[1][1] = 0;
    g_cropCorners[2][0] = W; g_cropCorners[2][1] = H;
    g_cropCorners[3][0] = 0; g_cropCorners[3][1] = H;
}

static void AutoDetectCorners() {
    if (!g_bmp) { InitCropCorners(); return; }
    INT W = (INT)g_bmp->GetWidth(), H = (INT)g_bmp->GetHeight();
    if (W < 8 || H < 8) { InitCropCorners(); return; }
    Gdiplus::Rect rect(0, 0, W, H);
    Gdiplus::BitmapData sd = {};
    if (g_bmp->LockBits(&rect, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &sd) != Gdiplus::Ok) { InitCropCorners(); return; }
    std::vector<int> gray((size_t)W * H);
    for (INT y = 0; y < H; y++) {
        const unsigned char* r = (const unsigned char*)sd.Scan0 + (size_t)y * sd.Stride;
        for (INT x = 0; x < W; x++) gray[(size_t)y * W + x] = (int)(r[x * 4 + 2] * 0.299f + r[x * 4 + 1] * 0.587f + r[x * 4 + 0] * 0.114f);
    }
    g_bmp->UnlockBits(&sd);

    std::vector<int> border;
    int step = (int)((std::min)(W, H) / 64) + 1;
    for (INT x = 0; x < W; x += step) { border.push_back(gray[x]); border.push_back(gray[(size_t)(H - 1) * W + x]); }
    for (INT y = 0; y < H; y += step) { border.push_back(gray[(size_t)y * W]); border.push_back(gray[(size_t)y * W + (W - 1)]); }
    std::sort(border.begin(), border.end());
    int bg = border[border.size() / 2];

    double f = 240.0 / (std::max)(W, H); if (f > 1.0) f = 1.0;
    int sw = (std::max)(1, (int)(W * f + 0.5)), sh = (std::max)(1, (int)(H * f + 0.5));
    std::vector<int> sm((size_t)sw * sh, 0), cnt((size_t)sw * sh, 0);
    for (INT y = 0; y < H; y++) { int sy = (int)(y * f); if (sy >= sh) sy = sh - 1; for (INT x = 0; x < W; x++) { int sx = (int)(x * f); if (sx >= sw) sx = sw - 1; size_t o = (size_t)sy * sw + sx; sm[o] += gray[(size_t)y * W + x]; cnt[o]++; } }
    for (size_t i = 0; i < sm.size(); i++) if (cnt[i]) sm[i] /= cnt[i];

    std::vector<int> mg((size_t)sw * sh, 0);
    double sum = 0; long n = 0;
    for (int y = 1; y < sh - 1; y++) {
        for (int x = 1; x < sw - 1; x++) {
            int gx = sm[(size_t)(y - 1) * sw + (x + 1)] + 2 * sm[(size_t)y * sw + (x + 1)] + sm[(size_t)(y + 1) * sw + (x + 1)]
                   - sm[(size_t)(y - 1) * sw + (x - 1)] - 2 * sm[(size_t)y * sw + (x - 1)] - sm[(size_t)(y + 1) * sw + (x - 1)];
            int gy = sm[(size_t)(y + 1) * sw + (x - 1)] + 2 * sm[(size_t)(y + 1) * sw + x] + sm[(size_t)(y + 1) * sw + (x + 1)]
                   - sm[(size_t)(y - 1) * sw + (x - 1)] - 2 * sm[(size_t)(y - 1) * sw + x] - sm[(size_t)(y - 1) * sw + (x + 1)];
            int m = (int)sqrt((double)gx * gx + (double)gy * gy);
            mg[(size_t)y * sw + x] = m; sum += m; n++;
        }
    }
    double mean = n ? sum / n : 0;
    double sq = 0; for (size_t i = 0; i < mg.size(); i++) { double d = mg[i] - mean; sq += d * d; }
    double sdv = n ? sqrt(sq / n) : 0;
    double Tg = mean + 1.3 * sdv; if (Tg < 18) Tg = 18;

    double tl = 1e18, tr = -1e18, br = -1e18, bl = -1e18;
    double tlX = 0, tlY = 0, trX = (double)sw, trY = 0, brX = (double)sw, brY = (double)sh, blX = 0, blY = (double)sh;
    bool any = false;
    for (int y = 1; y < sh - 1; y++) {
        for (int x = 1; x < sw - 1; x++) {
            size_t i = (size_t)y * sw + x;
            int df = sm[i] - bg; if (df < 0) df = -df;
            bool edge = mg[i] > Tg;
            bool fore = df > 30;
            if (!edge && !fore) continue;
            any = true;
            double X = x, Y = y;
            double a = X + Y, b = X - Y, c = Y - X;
            if (a < tl) { tl = a; tlX = X; tlY = Y; }
            if (b > tr) { tr = b; trX = X; trY = Y; }
            if (a > br) { br = a; brX = X; brY = Y; }
            if (c > bl) { bl = c; blX = X; blY = Y; }
        }
    }
    if (!any) { InitCropCorners(); return; }

    double qArea = 0.5 * std::fabs((tlX * trY - trX * tlY) + (trX * brY - brX * trY) + (brX * blY - blX * brY) + (blX * tlY - tlX * blY));
    if (qArea > 0.92 * (sw * sh)) { InitCropCorners(); return; }

    g_cropCorners[0][0] = f > 0 ? tlX / f : tlX; g_cropCorners[0][1] = f > 0 ? tlY / f : tlY;
    g_cropCorners[1][0] = f > 0 ? trX / f : trX; g_cropCorners[1][1] = f > 0 ? trY / f : trY;
    g_cropCorners[2][0] = f > 0 ? brX / f : brX; g_cropCorners[2][1] = f > 0 ? brY / f : brY;
    g_cropCorners[3][0] = f > 0 ? blX / f : blX; g_cropCorners[3][1] = f > 0 ? blY / f : blY;
    for (int i = 0; i < 4; i++) {
        if (g_cropCorners[i][0] < 0) g_cropCorners[i][0] = 0;
        if (g_cropCorners[i][1] < 0) g_cropCorners[i][1] = 0;
        if (g_cropCorners[i][0] > W) g_cropCorners[i][0] = W;
        if (g_cropCorners[i][1] > H) g_cropCorners[i][1] = H;
    }
}

static void EnterCropRect() {
    if (!g_bmp) return;
    g_cropping = true; g_perspCrop = false; g_dragging = false; g_dragCorner = -1;
    g_rcX0 = g_rcY0 = g_rcX1 = g_rcY1 = 0;
    SetFocus(g_hMain);
    InvalidateRect(g_hMain, nullptr, FALSE);
    UpdateStatus();
}
static void EnterCropPersp() {
    if (!g_bmp) return;
    g_cropping = true; g_perspCrop = true; g_dragging = false; g_dragCorner = -1;
    AutoDetectCorners();
    SetFocus(g_hMain);
    InvalidateRect(g_hMain, nullptr, FALSE);
    UpdateStatus();
}

static bool SolveHomography(const double srcQ[4][2], const double dstR[4][2], double h[8]) {
    double A[8][9] = {{0}};
    for (int i = 0; i < 4; i++) {
        double xd = dstR[i][0], yd = dstR[i][1];
        double xs = srcQ[i][0], ys = srcQ[i][1];
        A[2*i][0]=xd; A[2*i][1]=yd; A[2*i][2]=1; A[2*i][6]=-xd*xs; A[2*i][7]=-yd*xs; A[2*i][8]=xs;
        A[2*i+1][3]=xd; A[2*i+1][4]=yd; A[2*i+1][5]=1; A[2*i+1][6]=-xd*ys; A[2*i+1][7]=-yd*ys; A[2*i+1][8]=ys;
    }
    for (int col = 0; col < 8; col++) {
        int piv = col;
        for (int r = col + 1; r < 8; r++) if (fabs(A[r][col]) > fabs(A[piv][col])) piv = r;
        if (fabs(A[piv][col]) < 1e-12) return false;
        if (piv != col) for (int c = 0; c < 9; c++) { double t = A[piv][c]; A[piv][c] = A[col][c]; A[col][c] = t; }
        double dv = A[col][col];
        for (int c = 0; c < 9; c++) A[col][c] /= dv;
        for (int r = 0; r < 8; r++) if (r != col) { double f = A[r][col]; for (int c = 0; c < 9; c++) A[r][c] -= f * A[col][c]; }
    }
    for (int i = 0; i < 8; i++) h[i] = A[i][8];
    return true;
}

static Gdiplus::Bitmap* WarpPerspective(Gdiplus::Bitmap* src, const double quad[4][2]) {
    INT W = (INT)src->GetWidth(), H = (INT)src->GetHeight();
    auto dist = [](double x1, double y1, double x2, double y2){ return sqrt((x2-x1)*(x2-x1)+(y2-y1)*(y2-y1)); };
    double top = dist(quad[0][0], quad[0][1], quad[1][0], quad[1][1]);
    double bot = dist(quad[3][0], quad[3][1], quad[2][0], quad[2][1]);
    double lf  = dist(quad[0][0], quad[0][1], quad[3][0], quad[3][1]);
    double rt  = dist(quad[1][0], quad[1][1], quad[2][0], quad[2][1]);
    int outW = (int)((top + bot) / 2 + 0.5);
    int outH = (int)((lf + rt) / 2 + 0.5);
    if (outW < 2 || outH < 2) return nullptr;
    double dstR[4][2] = {{0,0},{(double)outW,0},{(double)outW,(double)outH},{0,(double)outH}};
    double h[8];
    if (!SolveHomography(quad, dstR, h)) return nullptr;

    Gdiplus::Rect srect(0, 0, W, H);
    Gdiplus::BitmapData sd = {};
    if (src->LockBits(&srect, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &sd) != Gdiplus::Ok) return nullptr;
    const unsigned char* sp = (const unsigned char*)sd.Scan0;
    int sstride = sd.Stride;

    Gdiplus::Bitmap* nb = new Gdiplus::Bitmap(outW, outH, PixelFormat32bppARGB);
    Gdiplus::Rect drect(0, 0, outW, outH);
    Gdiplus::BitmapData dd = {};
    nb->LockBits(&drect, Gdiplus::ImageLockModeWrite, PixelFormat32bppARGB, &dd);
    unsigned char* dp = (unsigned char*)dd.Scan0;
    int dstride = dd.Stride;
    for (int y = 0; y < outH; y++) {
        unsigned char* drow = dp + (size_t)y * dstride;
        for (int x = 0; x < outW; x++) {
            double den = h[6] * x + h[7] * y + 1.0;
            double sx = (h[0] * x + h[1] * y + h[2]) / den;
            double sy = (h[3] * x + h[4] * y + h[5]) / den;
            unsigned char r, gg, b;
            if (sx < 0 || sy < 0 || sx > W - 1 || sy > H - 1) {
                r = gg = b = 255;
            } else {
                int x0 = (int)sx, y0 = (int)sy;
                double fx = sx - x0, fy = sy - y0;
                int x1 = x0 + 1; if (x1 > W - 1) x1 = W - 1;
                int y1 = y0 + 1; if (y1 > H - 1) y1 = H - 1;
                const unsigned char* p00 = sp + (size_t)y0 * sstride + x0 * 4;
                const unsigned char* p10 = sp + (size_t)y0 * sstride + x1 * 4;
                const unsigned char* p01 = sp + (size_t)y1 * sstride + x0 * 4;
                const unsigned char* p11 = sp + (size_t)y1 * sstride + x1 * 4;
                double w00 = (1 - fx) * (1 - fy), w10 = fx * (1 - fy), w01 = (1 - fx) * fy, w11 = fx * fy;
                b  = (unsigned char)(p00[0]*w00 + p10[0]*w10 + p01[0]*w01 + p11[0]*w11 + 0.5);
                gg = (unsigned char)(p00[1]*w00 + p10[1]*w10 + p01[1]*w01 + p11[1]*w11 + 0.5);
                r  = (unsigned char)(p00[2]*w00 + p10[2]*w10 + p01[2]*w01 + p11[2]*w11 + 0.5);
            }
            drow[x*4+0] = b; drow[x*4+1] = gg; drow[x*4+2] = r; drow[x*4+3] = 255;
        }
    }
    nb->UnlockBits(&dd);
    src->UnlockBits(&sd);
    return nb;
}

static void DrawRectOverlay(Gdiplus::Graphics* g) {
    if (!g_bmp) return;
    double minx = (std::min)(g_rcX0, g_rcX1), maxx = (std::max)(g_rcX0, g_rcX1);
    double miny = (std::min)(g_rcY0, g_rcY1), maxy = (std::max)(g_rcY0, g_rcY1);
    int sx = (int)(g_ox + minx * g_scale), sy = (int)(g_oy + miny * g_scale);
    int sw = (int)((maxx - minx) * g_scale), sh = (int)((maxy - miny) * g_scale);
    if (sw < 1 || sh < 1) return;
    Gdiplus::SolidBrush dim(Gdiplus::Color(150, 0, 0, 0));
    g->FillRectangle(&dim, 0, g_canvasTop, g_canvasW, sy - g_canvasTop);
    g->FillRectangle(&dim, 0, sy, sx, sh);
    g->FillRectangle(&dim, sx + sw, sy, g_canvasW - (sx + sw), sh);
    g->FillRectangle(&dim, 0, sy + sh, g_canvasW, g_canvasH - ((sy + sh) - g_canvasTop));
    Gdiplus::Pen pen(Gdiplus::Color(255, 90, 146, 255), 1.6f);
    pen.SetDashStyle(Gdiplus::DashStyleDash);
    g->DrawRectangle(&pen, sx, sy, sw, sh);
}

static void DrawCropOverlay(Gdiplus::Graphics* g) {
    if (!g_bmp) return;
    Gdiplus::PointF p[4];
    for (int i = 0; i < 4; i++)
        p[i] = Gdiplus::PointF((float)(g_ox + g_cropCorners[i][0] * g_scale), (float)(g_oy + g_cropCorners[i][1] * g_scale));
    Gdiplus::GraphicsPath quad;
    quad.AddPolygon(p, 4);
    Gdiplus::Region outside(Gdiplus::Rect(0, g_canvasTop, g_canvasW, g_canvasH));
    outside.Exclude(&quad);
    Gdiplus::SolidBrush dim(Gdiplus::Color(165, 0, 0, 0));
    g->FillRegion(&dim, &outside);
    Gdiplus::Pen line(Gdiplus::Color(255, 90, 146, 255), 2.0f);
    g->DrawPath(&line, &quad);
    float r = (float)DPI(7);
    for (int i = 0; i < 4; i++) {
        Gdiplus::SolidBrush hb(Gdiplus::Color(255, 255, 255, 255));
        g->FillEllipse(&hb, p[i].X - r, p[i].Y - r, r * 2, r * 2);
        Gdiplus::Pen hp(Gdiplus::Color(255, 90, 146, 255), 2.0f);
        g->DrawEllipse(&hp, p[i].X - r, p[i].Y - r, r * 2, r * 2);
    }
}

static void DoRotate(bool right) {
    if (!g_bmp) return;
    Gdiplus::RotateFlipType t = right ? Gdiplus::Rotate90FlipNone : Gdiplus::Rotate270FlipNone;
    if (g_bmp->RotateFlip(t) != Gdiplus::Ok) return;
    g_cropping = false; g_zoom = 1.0; g_dirty = true;
    RebuildDisplayCache();
    DoSave();
    InvalidateRect(g_hMain, nullptr, FALSE);
    UpdateStatus();
}

static void DoCropApply() {
    if (!g_bmp || !g_cropping) { g_cropping = false; InvalidateRect(g_hMain, nullptr, FALSE); UpdateStatus(); return; }
    if (g_perspCrop) {
        double quad[4][2];
        for (int i = 0; i < 4; i++) { quad[i][0] = g_cropCorners[i][0]; quad[i][1] = g_cropCorners[i][1]; }
        Gdiplus::Bitmap* nb = WarpPerspective(g_bmp, quad);
        if (nb) { g_undoStack.push_back(g_bmp); g_bmp = nb; g_dirty = true; }
    } else {
        double minx = (std::min)(g_rcX0, g_rcX1), maxx = (std::max)(g_rcX0, g_rcX1);
        double miny = (std::min)(g_rcY0, g_rcY1), maxy = (std::max)(g_rcY0, g_rcY1);
        INT W = (INT)g_bmp->GetWidth(), H = (INT)g_bmp->GetHeight();
        INT x = (INT)minx, y = (INT)miny, w = (INT)(maxx - minx), h = (INT)(maxy - miny);
        if (x < 0) x = 0; if (y < 0) y = 0;
        if (x + w > W) w = W - x; if (y + h > H) h = H - y;
        if (w > 1 && h > 1) {
            Gdiplus::Bitmap* nb = g_bmp->Clone(x, y, w, h, g_bmp->GetPixelFormat());
            if (nb) { g_undoStack.push_back(g_bmp); g_bmp = nb; g_dirty = true; }
        }
    }
    g_cropping = false; g_perspCrop = false; g_dragCorner = -1; g_zoom = 1.0;
    RebuildDisplayCache();
    DoSave();
    InvalidateRect(g_hMain, nullptr, FALSE);
    UpdateStatus();
}

static void DoUndo() {
    if (g_undoStack.empty()) { g_sLeft = L"Nothing to undo"; InvalidateChrome(); return; }
    delete g_bmp;
    g_bmp = g_undoStack.back();
    g_undoStack.pop_back();
    g_dirty = true; g_zoom = 1.0; g_cropping = false;
    RebuildDisplayCache();
    DoSave();
    InvalidateRect(g_hMain, nullptr, FALSE);
    UpdateStatus();
}

static void ComputeScanFull(INT W, INT H, const std::vector<unsigned char>& baseBuf, std::vector<unsigned char>& fullBuf) {
    fullBuf.assign((size_t)W * H * 4, 255);
    std::vector<unsigned char> gray((size_t)W * H);
    for (size_t i = 0; i < (size_t)W * H; i++) {
        const unsigned char* p = &baseBuf[i * 4];
        gray[i] = (unsigned char)(p[2] * 0.299f + p[1] * 0.587f + p[0] * 0.114f);
    }
    const int S = 16;
    int bw = W / S > 0 ? W / S : 1;
    int bh = H / S > 0 ? H / S : 1;
    std::vector<float> bg((size_t)bw * bh);
    for (int by = 0; by < bh; by++) {
        for (int bx = 0; bx < bw; bx++) {
            int x0 = bx * S, y0 = by * S;
            int x1 = (std::min)(x0 + S, (int)W), y1 = (std::min)(y0 + S, (int)H);
            long sum = 0, cnt = 0;
            for (int yy = y0; yy < y1; yy++)
                for (int xx = x0; xx < x1; xx++) { sum += gray[(size_t)yy * W + xx]; cnt++; }
            bg[(size_t)by * bw + bx] = cnt ? (float)sum / cnt : 0.0f;
        }
    }
    for (INT y = 0; y < H; y++) {
        for (INT x = 0; x < W; x++) {
            float g = gray[(size_t)y * W + x];
            float fx = (float)x / S - 0.5f; if (fx < 0) fx = 0; if (fx > bw - 1) fx = (float)bw - 1;
            float fy = (float)y / S - 0.5f; if (fy < 0) fy = 0; if (fy > bh - 1) fy = (float)bh - 1;
            int x0i = (int)floor(fx), y0i = (int)floor(fy);
            int x1i = (std::min)(x0i + 1, bw - 1), y1i = (std::min)(y0i + 1, bh - 1);
            float tx = fx - x0i, ty = fy - y0i;
            float b = bg[(size_t)y0i * bw + x0i] * (1 - tx) * (1 - ty) + bg[(size_t)y0i * bw + x1i] * tx * (1 - ty)
                    + bg[(size_t)y1i * bw + x0i] * (1 - tx) * ty + bg[(size_t)y1i * bw + x1i] * tx * ty;
            if (b < 1.0f) b = 1.0f;
            float t = g * 255.0f / b;
            if (t < 0) t = 0; if (t > 255) t = 255;
            t = (t - 128.0f) * 1.45f + 128.0f;
            if (t < 0) t = 0; if (t > 255) t = 255;
            t = powf(t / 255.0f, 0.82f) * 255.0f;
            if (t > 236) t = 255;
            unsigned char v = (unsigned char)t;
            size_t o = ((size_t)y * W + x) * 4;
            fullBuf[o] = v; fullBuf[o + 1] = v; fullBuf[o + 2] = v; fullBuf[o + 3] = 255;
        }
    }
}

static Gdiplus::Bitmap* MakeScanPreview(INT W, INT H, double k) {
    Gdiplus::Bitmap* nb = new Gdiplus::Bitmap(W, H, PixelFormat32bppARGB);
    Gdiplus::Rect rect(0, 0, W, H);
    Gdiplus::BitmapData dd = {};
    nb->LockBits(&rect, Gdiplus::ImageLockModeWrite, PixelFormat32bppARGB, &dd);
    unsigned char* dst = (unsigned char*)dd.Scan0;
    for (INT y = 0; y < H; y++) {
        unsigned char* row = dst + (size_t)y * dd.Stride;
        for (INT x = 0; x < W; x++) {
            size_t o = ((size_t)y * W + x) * 4;
            double b0 = g_scanBaseBuf[o], b1 = g_scanBaseBuf[o + 1], b2 = g_scanBaseBuf[o + 2];
            double f0 = g_scanFullBuf[o], f1 = g_scanFullBuf[o + 1], f2 = g_scanFullBuf[o + 2];
            row[x * 4 + 0] = (unsigned char)(b0 + (f0 - b0) * k + 0.5);
            row[x * 4 + 1] = (unsigned char)(b1 + (f1 - b1) * k + 0.5);
            row[x * 4 + 2] = (unsigned char)(b2 + (f2 - b2) * k + 0.5);
            row[x * 4 + 3] = 255;
        }
    }
    nb->UnlockBits(&dd);
    return nb;
}

static void EnterScanMode() {
    if (!g_bmp || g_scanMode) return;
    INT W = (INT)g_bmp->GetWidth(), H = (INT)g_bmp->GetHeight();
    if (W <= 0 || H <= 0) return;
    Gdiplus::Rect rect(0, 0, W, H);
    Gdiplus::BitmapData sd = {};
    if (g_bmp->LockBits(&rect, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &sd) != Gdiplus::Ok) return;
    g_scanBaseBuf.assign((size_t)W * H * 4, 0);
    for (INT y = 0; y < H; y++) memcpy(&g_scanBaseBuf[(size_t)y * W * 4], (BYTE*)sd.Scan0 + (size_t)y * sd.Stride, (size_t)W * 4);
    g_bmp->UnlockBits(&sd);

    ComputeScanFull(W, H, g_scanBaseBuf, g_scanFullBuf);
    g_scanBase = g_bmp;
    g_scanMode = true;
    g_scanLevel = 70;
    g_bmp = MakeScanPreview(W, H, g_scanLevel / 100.0);
    RebuildDisplayCache();
    InvalidateRect(g_hMain, nullptr, FALSE);
    UpdateStatus();
}
static void AdjustScan(int delta) {
    if (!g_scanMode || !g_scanBase) return;
    g_scanLevel += delta;
    if (g_scanLevel < 0) g_scanLevel = 0;
    if (g_scanLevel > 100) g_scanLevel = 100;
    INT W = (INT)g_scanBase->GetWidth(), H = (INT)g_scanBase->GetHeight();
    Gdiplus::Bitmap* pv = MakeScanPreview(W, H, g_scanLevel / 100.0);
    delete g_bmp;
    g_bmp = pv;
    RebuildDisplayCache();
    InvalidateRect(g_hMain, nullptr, FALSE);
    UpdateStatus();
}
static void CommitScan() {
    if (!g_scanMode) return;
    g_undoStack.push_back(g_scanBase);
    g_scanBase = nullptr;
    g_scanBaseBuf.clear();
    g_scanBaseBuf.shrink_to_fit();
    g_scanFullBuf.clear();
    g_scanFullBuf.shrink_to_fit();
    g_scanMode = false;
    g_dirty = true;
    DoSave();
    RebuildDisplayCache();
    InvalidateRect(g_hMain, nullptr, FALSE);
    UpdateStatus();
}
static void CancelScan() {
    if (!g_scanMode) return;
    delete g_bmp;
    g_bmp = g_scanBase;
    g_scanBase = nullptr;
    g_scanBaseBuf.clear();
    g_scanBaseBuf.shrink_to_fit();
    g_scanFullBuf.clear();
    g_scanFullBuf.shrink_to_fit();
    g_scanMode = false;
    RebuildDisplayCache();
    InvalidateRect(g_hMain, nullptr, FALSE);
    UpdateStatus();
}

static void DoSave() {
    if (!g_bmp) return;
    bool canEncode = true;
    const WCHAR* mime = MimeForExt(g_ext, canEncode);
    CLSID clsid;
    std::wstring savePath = g_path;
    std::wstring saveExt = g_ext;
    if (!canEncode) {
        saveExt = L".png"; mime = L"image/png";
        savePath = g_dir + L"\\" + NameOnly(g_path) + L".png";
    }
    if (GetEncoderClsid(mime, &clsid) < 0) {
        MessageBoxW(g_hMain, L"No encoder available for this format.", L"Save", MB_OK | MB_ICONWARNING);
        return;
    }
    Gdiplus::EncoderParameters ep;
    ULONG q = 95;
    Gdiplus::EncoderParameters* pep = nullptr;
    if (mime == L"image/jpeg") {
        ep.Count = 1;
        ep.Parameter[0].Guid = Gdiplus::EncoderQuality;
        ep.Parameter[0].NumberOfValues = 1;
        ep.Parameter[0].Type = Gdiplus::EncoderParameterValueTypeLong;
        ep.Parameter[0].Value = &q;
        pep = &ep;
    }
    if (g_bmp->Save(savePath.c_str(), &clsid, pep) != Gdiplus::Ok) {
        MessageBoxW(g_hMain, L"Failed to save the image.", L"Save", MB_OK | MB_ICONERROR);
        return;
    }
    if (!canEncode) {
        g_path = savePath; g_ext = saveExt;
        UpdateEditName(); UpdateTitle(); RebuildFolderList();
    }
    g_dirty = false;
    UpdateStatus();
}

static void StartEdit() {
    if (g_path.empty()) return;
    g_editing = true;
    SendMessageW(g_hEdit, EM_SETREADONLY, FALSE, 0);
    SetFocus(g_hEdit);
    SendMessageW(g_hEdit, EM_SETSEL, 0, -1);
    InvalidateChrome();
    UpdateStatus();
}
static void CommitEdit() {
    g_editing = false;
    SendMessageW(g_hEdit, EM_SETREADONLY, TRUE, 0);
    WCHAR buf[MAX_PATH] = {0};
    GetWindowTextW(g_hEdit, buf, MAX_PATH);
    std::wstring nn = SanitizeName(buf);
    if (nn.empty() || nn == NameOnly(g_path)) { UpdateEditName(); UpdateStatus(); return; }
    std::wstring target = g_dir + L"\\" + nn + g_ext;
    if (Lower(target) != Lower(g_path) && PathFileExistsW(target.c_str())) {
        MessageBoxW(g_hMain, L"A file with that name already exists.", L"Rename", MB_OK | MB_ICONWARNING);
        UpdateEditName(); UpdateStatus(); return;
    }
    if (MoveFileW(g_path.c_str(), target.c_str())) {
        if (g_selected.count(g_path)) { g_selected.erase(g_path); g_selected.insert(target); }
        g_path = target;
        RebuildFolderList();
        UpdateEditName(); UpdateTitle();
    } else {
        MessageBoxW(g_hMain, L"Could not rename the file.", L"Rename", MB_OK | MB_ICONERROR);
        UpdateEditName();
    }
    UpdateStatus();
}
static void CancelEdit() {
    g_editing = false;
    SendMessageW(g_hEdit, EM_SETREADONLY, TRUE, 0);
    UpdateEditName();
    UpdateStatus();
}

static void ToggleSelectCurrent() {
    if (g_path.empty()) return;
    if (g_selected.count(g_path)) g_selected.erase(g_path);
    else g_selected.insert(g_path);
    if (g_hCheck) InvalidateRect(g_hCheck, nullptr, FALSE);
    InvalidateChrome();
    UpdateStatus();
}
static void SelectAll() {
    if (g_files.empty()) return;
    for (const auto& f : g_files) g_selected.insert(f);
    if (g_hCheck) InvalidateRect(g_hCheck, nullptr, FALSE);
    InvalidateChrome();
    UpdateStatus();
}
static void ClearSelection() {
    g_selected.clear();
    if (g_hCheck) InvalidateRect(g_hCheck, nullptr, FALSE);
    InvalidateChrome();
    UpdateStatus();
}
static void CopySelectionToClipboard(bool cut) {
    if (g_selected.empty()) return;
    std::wstring paths;
    for (const auto& p : g_selected) { paths += p; paths.push_back(0); }
    paths.push_back(0);
    size_t bytes = sizeof(DROPFILES) + paths.size() * sizeof(WCHAR);
    HGLOBAL hDrop = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (!hDrop) return;
    DROPFILES* df = (DROPFILES*)GlobalLock(hDrop);
    ZeroMemory(df, sizeof(DROPFILES));
    df->pFiles = sizeof(DROPFILES);
    df->fWide = TRUE;
    memcpy((BYTE*)df + sizeof(DROPFILES), paths.data(), paths.size() * sizeof(WCHAR));
    GlobalUnlock(hDrop);

    UINT fmtEff = RegisterClipboardFormatW(L"Preferred DropEffect");
    DWORD effect = cut ? DROPEFFECT_MOVE : DROPEFFECT_COPY;
    HGLOBAL hEff = GlobalAlloc(GMEM_MOVEABLE, sizeof(DWORD));
    *(DWORD*)GlobalLock(hEff) = effect;
    GlobalUnlock(hEff);

    bool ok = false;
    if (OpenClipboard(g_hMain)) {
        EmptyClipboard();
        ok = (SetClipboardData(CF_HDROP, hDrop) != nullptr);
        SetClipboardData(fmtEff, hEff);
        CloseClipboard();
    }
    if (!ok) { GlobalFree(hDrop); GlobalFree(hEff); }
    else {
        g_sLeft = cut ? (L"Cut " + std::to_wstring(g_selected.size()) + L" file(s) \u2014 paste in any folder")
                      : (L"Copied " + std::to_wstring(g_selected.size()) + L" file(s) \u2014 paste in any folder");
        InvalidateChrome();
    }
}

static bool RegSetString(HKEY root, const std::wstring& sub, const WCHAR* val, const std::wstring& data) {
    HKEY k; LONG r = RegCreateKeyExW(root, sub.c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &k, nullptr);
    if (r != ERROR_SUCCESS) return false;
    r = RegSetValueExW(k, val, 0, REG_SZ, (const BYTE*)data.c_str(), (DWORD)((data.size() + 1) * sizeof(WCHAR)));
    RegCloseKey(k);
    return r == ERROR_SUCCESS;
}
static bool RegSetNone(HKEY root, const std::wstring& sub, const WCHAR* val) {
    HKEY k; LONG r = RegCreateKeyExW(root, sub.c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &k, nullptr);
    if (r != ERROR_SUCCESS) return false;
    r = RegSetValueExW(k, val, 0, REG_NONE, nullptr, 0);
    RegCloseKey(k);
    return r == ERROR_SUCCESS;
}
static void RegisterAssociations() {
    std::wstring exe = ExePath();
    std::wstring cmd = L"\"" + exe + L"\" \"%1\"";
    RegSetString(HKEY_CURRENT_USER, L"Software\\Classes\\ImageViewerPro.Image", nullptr, L"Image Viewer Pro Image");
    RegSetString(HKEY_CURRENT_USER, L"Software\\Classes\\ImageViewerPro.Image\\DefaultIcon", nullptr, exe);
    RegSetString(HKEY_CURRENT_USER, L"Software\\Classes\\ImageViewerPro.Image\\shell\\open\\command", nullptr, cmd);
    for (auto e : g_imageExts)
        RegSetNone(HKEY_CURRENT_USER, std::wstring(L"Software\\Classes\\") + e + L"\\OpenWithProgids", L"ImageViewerPro.Image");
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
}
static void DoSetDefault() {
    RegisterAssociations();
    ShellExecuteW(nullptr, L"open", L"ms-settings:defaultapps", nullptr, nullptr, SW_SHOWNORMAL);
    g_sLeft = L"Registered \u2014 choose Image Viewer Pro in Default Apps.";
    InvalidateChrome();
}

static void DispatchAction(int act) {
    switch (act) {
        case ACT_RENAME:    if (g_editing) CommitEdit(); else StartEdit(); break;
        case ACT_SELECT:    ToggleSelectCurrent(); break;
        case ACT_SELECTALL: SelectAll(); break;
        case ACT_CLEARSEL:  ClearSelection(); break;
        case ACT_COPY:      CopySelectionToClipboard(false); break;
        case ACT_CUT:       CopySelectionToClipboard(true); break;
        case ACT_ROTATER:   DoRotate(true); break;
        case ACT_ROTATEL:   DoRotate(false); break;
        case ACT_PREV:      SwitchToImage(g_index - 1); break;
        case ACT_NEXT:      SwitchToImage(g_index + 1); break;
        case ACT_ZOOMIN:    ZoomBy(1.2); break;
        case ACT_ZOOMOUT:   ZoomBy(1.0 / 1.2); break;
        case ACT_ZOOMFIT:   if (g_bmp) { g_zoom = 1.0; RebuildDisplayCache(); InvalidateRect(g_hMain, nullptr, FALSE); UpdateStatus(); } break;
        case ACT_CROP:      EnterCropRect(); break;
        case ACT_CROPPERSP: EnterCropPersp(); break;
        case ACT_SCAN:      EnterScanMode(); break;
        case ACT_UNDO:      DoUndo(); break;
        case ACT_OPEN:      DoOpen(); break;
        case ACT_SAVE:      DoSave(); break;
    }
}

static std::wstring SettingsPath() {
    WCHAR appdata[MAX_PATH] = {0};
    if (!ExpandEnvironmentStringsW(L"%APPDATA%", appdata, MAX_PATH)) appdata[0] = 0;
    std::wstring dir = std::wstring(appdata) + L"\\ImageViewerPro";
    CreateDirectoryW(dir.c_str(), nullptr);
    return dir + L"\\settings.ini";
}
static void LoadHotkeys() {
    for (int i = 0; i < ACT_COUNT; i++) g_hotkeys[i] = g_defaultHotkeys[i];
    std::wstring p = SettingsPath();
    for (int i = 0; i < ACT_COUNT; i++) {
        WCHAR def[64];
        swprintf(def, 64, L"%u,%d,%d,%d", g_defaultHotkeys[i].vk, g_defaultHotkeys[i].ctrl ? 1 : 0, g_defaultHotkeys[i].shift ? 1 : 0, g_defaultHotkeys[i].alt ? 1 : 0);
        WCHAR out[64] = {0};
        GetPrivateProfileStringW(L"Hotkeys", g_defaultHotkeys[i].name, def, out, 64, p.c_str());
        unsigned vk = 0; int c = 0, s = 0, a = 0;
        if (swscanf(out, L"%u,%d,%d,%d", &vk, &c, &s, &a) == 4 && vk != 0) {
            g_hotkeys[i].vk = vk; g_hotkeys[i].ctrl = c != 0; g_hotkeys[i].shift = s != 0; g_hotkeys[i].alt = a != 0;
        }
    }
}
static void SaveHotkeys() {
    std::wstring p = SettingsPath();
    for (int i = 0; i < ACT_COUNT; i++) {
        WCHAR val[64];
        swprintf(val, 64, L"%u,%d,%d,%d", g_hotkeys[i].vk, g_hotkeys[i].ctrl ? 1 : 0, g_hotkeys[i].shift ? 1 : 0, g_hotkeys[i].alt ? 1 : 0);
        WritePrivateProfileStringW(L"Hotkeys", g_hotkeys[i].name, val, p.c_str());
    }
}
struct MMap { int id; int action; const WCHAR* base; };
static void UpdateMenuHotkeys() {
    if (!g_hMenu) return;
    static const MMap map[] = {
        { ID_OPEN,   ACT_OPEN,    L"&Open Image..." },
        { ID_SAVE,   ACT_SAVE,    L"&Save" },
        { ID_RENAME, ACT_RENAME,  L"&Rename" },
        { ID_UNDO,   ACT_UNDO,    L"&Undo Edit" },
        { ID_SELALL, ACT_SELECTALL, L"Select &All" },
        { ID_SELCLR, ACT_CLEARSEL, L"&Clear Selection" },
        { ID_ROTL,   ACT_ROTATEL, L"Rotate &Left" },
        { ID_ROTR,   ACT_ROTATER, L"Rotate &Right" },
        { ID_SCAN,   ACT_SCAN,    L"&Scan Document" },
        { ID_CROP,      ACT_CROP,      L"Crop (Rectangle)" },
        { ID_CROPPERSP, ACT_CROPPERSP, L"Crop (Perspective / Auto)" },
        { ID_ZIN,    ACT_ZOOMIN,  L"Zoom &In" },
        { ID_ZOUT,   ACT_ZOOMOUT, L"Zoom &Out" },
        { ID_ZFIT,   ACT_ZOOMFIT, L"&Fit" },
        { ID_PREV,   ACT_PREV,    L"&Previous Image" },
        { ID_NEXT,   ACT_NEXT,    L"&Next Image" },
    };
    for (auto& e : map) {
        std::wstring text = e.base;
        text += L"\t";
        text += KeyLabel(g_hotkeys[e.action]);
        MENUITEMINFOW mii = {};
        mii.cbSize = sizeof(mii);
        mii.fMask = MIIM_STRING;
        mii.dwTypeData = (WCHAR*)text.c_str();
        SetMenuItemInfoW(g_hMenu, e.id, FALSE, &mii);
    }
}

static void UpdateStatus() {
    if (g_scanMode) g_sLeft = L"Scan " + std::to_wstring(g_scanLevel) + L"%  \u2014  Up/Down intensity, Enter to save, Esc to cancel";
    else if (g_editing) g_sLeft = L"Renaming \u2014 Enter to save, Esc to cancel";
    else if (g_cropping) g_sLeft = g_perspCrop ? L"Crop \u2014 drag corners to adjust, Enter to apply, Esc to cancel"
                                              : L"Crop \u2014 drag to draw a rectangle, Enter to apply, Esc to cancel";
    else if (g_bmp) g_sLeft = L"Edit > Set Hotkeys to customize keys";
    else g_sLeft = L"Ready";
    g_sRight.clear();
    if (g_bmp) {
        if (!g_selected.empty()) g_sRight += L"\u25CF " + std::to_wstring(g_selected.size()) + L" selected    ";
        g_sRight += std::to_wstring(g_bmp->GetWidth()) + L" \u00D7 " + std::to_wstring(g_bmp->GetHeight());
        int zp = (int)(g_zoom * 100 + 0.5);
        g_sRight += L"    " + std::to_wstring(zp) + L"%";
        if (g_dirty) g_sRight += L"  \u2022";
    }
    InvalidateChrome();
}

static void InvalidateChrome() {
    if (!g_hMain) return;
    RECT rc; GetClientRect(g_hMain, &rc);
    RECT topRc = { 0, 0, rc.right, g_tbh };
    RECT stRc = { 0, g_statusTop, rc.right, rc.bottom };
    InvalidateRect(g_hMain, &topRc, FALSE);
    InvalidateRect(g_hMain, &stRc, FALSE);
}

static void Layout() {
    RECT rc; GetClientRect(g_hMain, &rc);
    g_statusH = DPI(30);
    g_statusTop = rc.bottom - g_statusH;

    int btnH = DPI(40);
    int y = (g_tbh - btnH) / 2;
    int x = rc.right - DPI(10);
    int leftmost = rc.right;
    for (int i = g_nBtns - 1; i >= 0; i--) {
        if (g_btns[i].groupStart) x -= DPI(14); else x -= DPI(6);
        int w = DPI(g_btns[i].w);
        x -= w;
        if (g_btn[i]) MoveWindow(g_btn[i], x, y, w, btnH, TRUE);
        leftmost = x;
    }

    int chkW = DPI(104);
    int chkX = DPI(16);
    if (g_hCheck) MoveWindow(g_hCheck, chkX, (g_tbh - btnH) / 2, chkW, btnH, TRUE);

    int chipX = chkX + chkW + DPI(12);
    int chipW = leftmost - DPI(16) - chipX;
    if (chipW < DPI(220)) chipW = DPI(220);
    int chipH = DPI(40);
    int chipY = (g_tbh - chipH) / 2;
    g_chipRect = { chipX, chipY, chipX + chipW, chipY + chipH };
    int padX = DPI(14), padY = DPI(7);
    MoveWindow(g_hEdit, chipX + padX, chipY + padY, chipW - 2 * padX, chipH - 2 * padY, TRUE);

    g_canvasTop = g_tbh;
    g_canvasLeft = 0;
    g_canvasW = rc.right;
    g_canvasH = g_statusTop - g_tbh;
}

static void MakeRoundPath(Gdiplus::GraphicsPath& path, int x, int y, int w, int h, int r) {
    if (r < 1) r = 1;
    if (r > w / 2) r = w / 2;
    if (r > h / 2) r = h / 2;
    Gdiplus::Rect rt(x, y, w, h);
    path.AddArc(rt.X, rt.Y, r * 2, r * 2, 180, 90);
    path.AddArc(rt.GetRight() - r * 2, rt.Y, r * 2, r * 2, 270, 90);
    path.AddArc(rt.GetRight() - r * 2, rt.GetBottom() - r * 2, r * 2, r * 2, 0, 90);
    path.AddArc(rt.X, rt.GetBottom() - r * 2, r * 2, r * 2, 90, 90);
    path.CloseFigure();
}

static void DrawArrowHead(Gdiplus::Graphics* g, float tipX, float tipY, float dirX, float dirY, float len, const Gdiplus::Color& c, float pw) {
    float ang = atan2f(dirY, dirX);
    float back = 150.f * PI / 180.f;
    Gdiplus::Pen p(c, pw);
    p.SetStartCap(Gdiplus::LineCapRound); p.SetEndCap(Gdiplus::LineCapRound);
    g->DrawLine(&p, tipX, tipY, tipX + cosf(ang + back) * len, tipY + sinf(ang + back) * len);
    g->DrawLine(&p, tipX, tipY, tipX + cosf(ang - back) * len, tipY + sinf(ang - back) * len);
}
static void DrawCircularArrow(Gdiplus::Graphics* g, float cx, float cy, float R, float startDeg, float sweepDeg, bool clockwise, const Gdiplus::Color& c, float pw) {
    Gdiplus::Pen pen(c, pw);
    pen.SetStartCap(Gdiplus::LineCapRound); pen.SetEndCap(Gdiplus::LineCapRound);
    Gdiplus::RectF r(cx - R, cy - R, 2 * R, 2 * R);
    g->DrawArc(&pen, r, startDeg, sweepDeg);
    float endDeg = startDeg + sweepDeg;
    float er = endDeg * PI / 180.f;
    float ex = cx + R * cosf(er), ey = cy + R * sinf(er);
    float dirX, dirY;
    if (clockwise) { dirX = -sinf(er); dirY = cosf(er); }
    else { dirX = sinf(er); dirY = -cosf(er); }
    DrawArrowHead(g, ex, ey, dirX, dirY, R * 0.55f, c, pw);
}

static void DrawIcon(Gdiplus::Graphics* g, int iconId, float cx, float cy, float s, const Gdiplus::Color& c) {
    float pw = s * 0.12f;
    Gdiplus::Pen p(c, pw);
    p.SetStartCap(Gdiplus::LineCapRound); p.SetEndCap(Gdiplus::LineCapRound);
    if (iconId == IC_PREV || iconId == IC_NEXT) {
        float d = (iconId == IC_PREV) ? -1.f : 1.f;
        float ax = cx + d * s * 0.18f;
        g->DrawLine(&p, ax, cy - s * 0.22f, cx - d * s * 0.05f, cy);
        g->DrawLine(&p, cx - d * s * 0.05f, cy, ax, cy + s * 0.22f);
    } else if (iconId == IC_ROTL || iconId == IC_ROTR) {
        float R = s * 0.40f;
        if (iconId == IC_ROTR) DrawCircularArrow(g, cx, cy + s * 0.02f, R, 135, 270, true, c, pw);
        else DrawCircularArrow(g, cx, cy + s * 0.02f, R, 45, -270, false, c, pw);
    } else if (iconId == IC_CROP) {
        float h = s * 0.42f, arm = s * 0.26f;
        g->DrawLine(&p, cx - h, cy - h + arm, cx - h, cy - h);
        g->DrawLine(&p, cx - h, cy - h, cx - h + arm, cy - h);
        g->DrawLine(&p, cx + h - arm, cy - h, cx + h, cy - h);
        g->DrawLine(&p, cx + h, cy - h, cx + h, cy - h + arm);
        g->DrawLine(&p, cx + h, cy + h - arm, cx + h, cy + h);
        g->DrawLine(&p, cx + h, cy + h, cx + h - arm, cy + h);
        g->DrawLine(&p, cx - h + arm, cy + h, cx - h, cy + h);
        g->DrawLine(&p, cx - h, cy + h, cx - h, cy + h - arm);
    } else if (iconId == IC_ZIN || iconId == IC_ZOUT || iconId == IC_ZFIT) {
        if (iconId == IC_ZFIT) {
            float o = s * 0.40f, i = s * 0.17f;
            g->DrawRectangle(&p, cx - o, cy - o, 2 * o, 2 * o);
            g->DrawRectangle(&p, cx - i, cy - i, 2 * i, 2 * i);
        } else {
            float mcx = cx - s * 0.12f, mcy = cy - s * 0.12f, mr = s * 0.30f;
            g->DrawEllipse(&p, mcx - mr, mcy - mr, 2 * mr, 2 * mr);
            g->DrawLine(&p, mcx + mr * 0.72f, mcy + mr * 0.72f, cx + s * 0.36f, cy + s * 0.36f);
            if (iconId == IC_ZIN) {
                g->DrawLine(&p, mcx - mr * 0.5f, mcy, mcx + mr * 0.5f, mcy);
                g->DrawLine(&p, mcx, mcy - mr * 0.5f, mcx, mcy + mr * 0.5f);
            } else {
                g->DrawLine(&p, mcx - mr * 0.5f, mcy, mcx + mr * 0.5f, mcy);
            }
        }
    }
}

static void DrawButton(DRAWITEMSTRUCT* dis) {
    RECT rr = dis->rcItem;
    int w = rr.right - rr.left, h = rr.bottom - rr.top;
    HDC hdc = dis->hDC;
    Gdiplus::Graphics g(hdc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);

    bool disabled = (dis->itemState & ODS_DISABLED) != 0;
    bool pressed = (dis->itemState & ODS_SELECTED) != 0;
    bool hover = (g_hoverBtn == dis->hwndItem) && !disabled;
    const BtnDef* def = nullptr;
    for (int i = 0; i < g_nBtns; i++) if (g_btns[i].id == (int)dis->CtlID) { def = &g_btns[i]; break; }

    Gdiplus::Color fill = C_BTN;
    if (disabled) fill = Gdiplus::Color(255, 36, 36, 52);
    else if (def && def->primary) fill = pressed ? C_ACCENT : (hover ? C_ACCENT2 : C_ACCENT);
    else if (pressed) fill = C_ACCENT;
    else if (hover) fill = C_BTNHOV;

    int radius = DPI(9);
    Gdiplus::GraphicsPath path;
    MakeRoundPath(path, DPI(1), DPI(1), w - DPI(2), h - DPI(2), radius);
    Gdiplus::SolidBrush br(fill);
    g.FillPath(&br, &path);

    if (def && def->icon) {
        Gdiplus::Color ic;
        if (disabled) ic = C_DIM;
        else if (pressed) ic = Gdiplus::Color(255, 255, 255, 255);
        else if (hover) ic = C_ACCENT2;
        else ic = C_TEXT;
        DrawIcon(&g, def->iconId, (float)w / 2, (float)h / 2, (float)DPI(18), ic);
    } else {
        WCHAR text[64] = {0};
        GetWindowTextW(dis->hwndItem, text, 64);
        Gdiplus::Color tc = disabled ? C_DIM : (((def && def->primary) || pressed) ? Gdiplus::Color(255, 255, 255, 255) : C_TEXT);
        Gdiplus::Font font(hdc, (def && def->primary) ? g_hFontBold : g_hFont);
        Gdiplus::SolidBrush tb(tc);
        Gdiplus::StringFormat sf;
        sf.SetAlignment(Gdiplus::StringAlignmentCenter);
        sf.SetLineAlignment(Gdiplus::StringAlignmentCenter);
        g.DrawString(text, -1, &font, Gdiplus::RectF(0, 0, (Gdiplus::REAL)w, (Gdiplus::REAL)h), &sf, &tb);
    }
}

static void DrawCheckbox(DRAWITEMSTRUCT* dis) {
    RECT rr = dis->rcItem;
    int w = rr.right - rr.left, h = rr.bottom - rr.top;
    HDC hdc = dis->hDC;
    Gdiplus::Graphics g(hdc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);

    bool sel = IsCurrentSelected();
    bool hover = (g_hoverBtn == dis->hwndItem);
    int box = DPI(20);
    int px = DPI(2);
    int py = (h - box) / 2;

    Gdiplus::Color boxFill = sel ? C_ACCENT : (hover ? C_BTNHOV : C_BTN);
    Gdiplus::GraphicsPath bp;
    MakeRoundPath(bp, px, py, box, box, DPI(5));
    Gdiplus::SolidBrush bb(boxFill);
    g.FillPath(&bb, &bp);
    Gdiplus::Pen outline(sel ? C_ACCENT : Gdiplus::Color(255, 96, 98, 126), 1);
    g.DrawPath(&outline, &bp);
    if (sel) {
        Gdiplus::Pen cp(Gdiplus::Color(255, 255, 255, 255), 2.6f);
        cp.SetStartCap(Gdiplus::LineCapRound); cp.SetEndCap(Gdiplus::LineCapRound);
        float bx = (float)px, by = (float)py;
        g.DrawLine(&cp, bx + box * 0.24f, by + box * 0.52f, bx + box * 0.44f, by + box * 0.72f);
        g.DrawLine(&cp, bx + box * 0.44f, by + box * 0.72f, bx + box * 0.78f, by + box * 0.26f);
    }
    Gdiplus::Font font(hdc, g_hFont);
    Gdiplus::SolidBrush tb(C_TEXT);
    Gdiplus::StringFormat sf;
    sf.SetLineAlignment(Gdiplus::StringAlignmentCenter);
    g.DrawString(L"Select", -1, &font,
        Gdiplus::RectF((Gdiplus::REAL)(px + box + DPI(8)), 0,
                       (Gdiplus::REAL)(w - px - box - DPI(8)), (Gdiplus::REAL)h), &sf, &tb);
}

static void Paint(HDC hdc) {
    RECT crc; GetClientRect(g_hMain, &crc);
    int cw = g_canvasW, ch = g_canvasH;
    int top = g_canvasTop;

    HDC mem = CreateCompatibleDC(hdc);
    HBITMAP mbmp = CreateCompatibleBitmap(hdc, crc.right, crc.bottom);
    HBITMAP old = (HBITMAP)SelectObject(mem, mbmp);

    Gdiplus::Graphics g(mem);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
    g.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);

    Gdiplus::LinearGradientBrush barBrush(Gdiplus::Rect(0, 0, crc.right, g_tbh), C_BAR2, C_BAR, 90.f);
    g.FillRectangle(&barBrush, 0, 0, crc.right, g_tbh);
    Gdiplus::Pen sep(C_SEP, 1);
    g.DrawLine(&sep, 0, g_tbh - 1, crc.right, g_tbh - 1);

    int cxs = g_chipRect.left, cys = g_chipRect.top, cxw = g_chipRect.right - g_chipRect.left, cxh = g_chipRect.bottom - g_chipRect.top;
    Gdiplus::GraphicsPath chipPath;
    MakeRoundPath(chipPath, cxs, cys, cxw, cxh, DPI(12));
    Gdiplus::SolidBrush chipBg(Gdiplus::Color(255, 38, 38, 56));
    g.FillPath(&chipBg, &chipPath);
    if (g_editing) {
        Gdiplus::Pen ring(C_ACCENT, 1.6f);
        g.DrawPath(&ring, &chipPath);
    }

    Gdiplus::SolidBrush bg(C_BG);
    g.FillRectangle(&bg, 0, top, cw, ch);

    if (g_bmp && g_dispCache) {
        HDC cdc = CreateCompatibleDC(mem);
        HBITMAP oldC = (HBITMAP)SelectObject(cdc, g_dispCache);
        BitBlt(mem, 0, g_canvasTop, g_cacheW, g_cacheH, cdc, 0, 0, SRCCOPY);
        SelectObject(cdc, oldC);
        DeleteDC(cdc);

        if (g_cropping) { if (g_perspCrop) DrawCropOverlay(&g); else DrawRectOverlay(&g); }

        if (IsCurrentSelected()) {
            Gdiplus::GraphicsPath bp;
            int bw = DPI(120), bh = DPI(32);
            MakeRoundPath(bp, DPI(16), top + DPI(16), bw, bh, DPI(16));
            Gdiplus::SolidBrush sbb(Gdiplus::Color(225, 90, 146, 255));
            g.FillPath(&sbb, &bp);
            Gdiplus::Pen cp(Gdiplus::Color(255, 255, 255, 255), 2.4f);
            cp.SetStartCap(Gdiplus::LineCapRound); cp.SetEndCap(Gdiplus::LineCapRound);
            float bx = (float)DPI(16) + bw * 0.12f, by = (float)(top + DPI(16)) + bh * 0.5f;
            g.DrawLine(&cp, bx, by + 2, bx + 6, by + 8);
            g.DrawLine(&cp, bx + 6, by + 8, bx + 16, by - 5);
            Gdiplus::Font font(mem, g_hFont);
            Gdiplus::SolidBrush tb(Gdiplus::Color(255, 255, 255, 255));
            Gdiplus::StringFormat sf;
            sf.SetLineAlignment(Gdiplus::StringAlignmentCenter);
            g.DrawString(L"Selected", -1, &font,
                Gdiplus::RectF((Gdiplus::REAL)(DPI(16) + 24), (Gdiplus::REAL)(top + DPI(16)),
                               (Gdiplus::REAL)(bw - 24), (Gdiplus::REAL)bh), &sf, &tb);
        }
    } else {
        Gdiplus::SolidBrush fg(Gdiplus::Color(255, 150, 152, 172));
        Gdiplus::Font font(L"Segoe UI", 20, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
        Gdiplus::Font font2(L"Segoe UI", 13, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
        const WCHAR* m1 = L"Open an image to begin";
        const WCHAR* m2 = L"File > Open (Ctrl+O)   or   drag & drop a file here";
        Gdiplus::StringFormat sf; sf.SetAlignment(Gdiplus::StringAlignmentCenter);
        g.DrawString(m1, -1, &font, Gdiplus::RectF(0, (Gdiplus::REAL)(top + ch / 2 - 40), (Gdiplus::REAL)cw, 36), &sf, &fg);
        Gdiplus::SolidBrush fg2(Gdiplus::Color(255, 100, 102, 122));
        g.DrawString(m2, -1, &font2, Gdiplus::RectF(0, (Gdiplus::REAL)(top + ch / 2 + 4), (Gdiplus::REAL)cw, 24), &sf, &fg2);
    }

    Gdiplus::SolidBrush sbg(C_BAR);
    g.FillRectangle(&sbg, 0, g_statusTop, crc.right, g_statusH);
    g.DrawLine(&sep, 0, g_statusTop, crc.right, g_statusTop);
    {
        Gdiplus::Font sf(mem, g_hFont);
        Gdiplus::SolidBrush stl(C_DIM);
        Gdiplus::StringFormat sfl;
        sfl.SetLineAlignment(Gdiplus::StringAlignmentCenter);
        g.DrawString(g_sLeft.c_str(), -1, &sf, Gdiplus::RectF((Gdiplus::REAL)DPI(16), (Gdiplus::REAL)g_statusTop, (Gdiplus::REAL)(crc.right - DPI(300)), (Gdiplus::REAL)g_statusH), &sfl, &stl);
        if (!g_sRight.empty()) {
            Gdiplus::StringFormat sfr;
            sfr.SetAlignment(Gdiplus::StringAlignmentFar);
            sfr.SetLineAlignment(Gdiplus::StringAlignmentCenter);
            g.DrawString(g_sRight.c_str(), -1, &sf, Gdiplus::RectF((Gdiplus::REAL)DPI(16), (Gdiplus::REAL)g_statusTop, (Gdiplus::REAL)(crc.right - DPI(16)), (Gdiplus::REAL)g_statusH), &sfr, &stl);
        }
    }

    BitBlt(hdc, 0, 0, crc.right, crc.bottom, mem, 0, 0, SRCCOPY);
    SelectObject(mem, old);
    DeleteObject(mbmp);
    DeleteDC(mem);
}

static void ClientToImage(POINT p, float& ix, float& iy) {
    ix = (float)((p.x - g_ox) / g_scale);
    iy = (float)((p.y - g_oy) / g_scale);
    if (g_bmp) {
        if (ix < 0) ix = 0; if (iy < 0) iy = 0;
        if (ix > (float)g_bmp->GetWidth())  ix = (float)g_bmp->GetWidth();
        if (iy > (float)g_bmp->GetHeight()) iy = (float)g_bmp->GetHeight();
    }
}

static LRESULT CALLBACK BtnSubProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR) {
    switch (msg) {
        case WM_MOUSEMOVE:
            if (g_hoverBtn != hWnd) { g_hoverBtn = hWnd; InvalidateRect(hWnd, nullptr, FALSE); }
            {
                TRACKMOUSEEVENT tme = { sizeof(tme) };
                tme.dwFlags = TME_LEAVE;
                tme.hwndTrack = hWnd;
                TrackMouseEvent(&tme);
            }
            break;
        case WM_MOUSELEAVE:
            if (g_hoverBtn == hWnd) { g_hoverBtn = nullptr; InvalidateRect(hWnd, nullptr, FALSE); }
            break;
    }
    return DefSubclassProc(hWnd, msg, wp, lp);
}

static void CreateButtons() {
    for (int i = 0; i < g_nBtns; i++) {
        g_btn[i] = CreateWindowW(L"BUTTON", g_btns[i].text,
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_CLIPSIBLINGS,
            0, 0, 10, 10, g_hMain, (HMENU)(LONG_PTR)g_btns[i].id, g_hInst, nullptr);
        SetWindowSubclass(g_btn[i], BtnSubProc, 0, 0);
    }
    g_hCheck = CreateWindowW(L"BUTTON", L"Select",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_CLIPSIBLINGS,
        0, 0, 10, 10, g_hMain, (HMENU)IDC_CHECKSEL, g_hInst, nullptr);
    SetWindowSubclass(g_hCheck, BtnSubProc, 0, 0);
}

static void OnCommand(WPARAM wp, LPARAM lp) {
    int code = HIWORD(wp);
    int id = LOWORD(wp);
    if (id == IDC_EDITNAME && code == EN_KILLFOCUS) {
        if (g_editing && !g_committing) { g_committing = true; CommitEdit(); g_committing = false; }
        return;
    }
    bool fromMenu = (lp == 0);
    bool fromBtn = ((code == BN_CLICKED) && (lp != 0));
    if (!fromMenu && !fromBtn) return;
    switch (id) {
        case IDC_CHECKSEL: ToggleSelectCurrent(); break;
        case ID_OPEN:  DoOpen(); break;
        case ID_SAVE:  DoSave(); break;
        case ID_ROTL:  DoRotate(false); break;
        case ID_ROTR:  DoRotate(true); break;
        case ID_SCAN:  EnterScanMode(); break;
        case ID_UNDO:  DoUndo(); break;
        case ID_SETTINGS: OpenSettings(); break;
        case ID_CROP:      EnterCropRect(); break;
        case ID_CROPPERSP: EnterCropPersp(); break;
        case ID_ZIN:   ZoomBy(1.2); break;
        case ID_ZOUT:  ZoomBy(1.0 / 1.2); break;
        case ID_ZFIT:  if (g_bmp) { g_zoom = 1.0; RebuildDisplayCache(); InvalidateRect(g_hMain, nullptr, FALSE); UpdateStatus(); } break;
        case ID_PREV:  SwitchToImage(g_index - 1); break;
        case ID_NEXT:  SwitchToImage(g_index + 1); break;
        case ID_SELALL: SelectAll(); break;
        case ID_SELCLR: ClearSelection(); break;
        case ID_DEFAULT: DoSetDefault(); break;
        case ID_SHOWFOLDER: if (!g_path.empty()) ShellExecuteW(g_hMain, L"open", g_dir.c_str(), nullptr, nullptr, SW_SHOWNORMAL); break;
        case ID_EXIT:  PostMessageW(g_hMain, WM_CLOSE, 0, 0); break;
        case ID_RENAME: StartEdit(); break;
        case ID_ABOUT: MessageBoxW(g_hMain,
            L"Image Viewer Pro\n\nOpen, rename, rotate, crop, scan, undo and copy/cut images.\n\nEdit > Set Hotkeys to customize all shortcuts.",
            L"About", MB_OK | MB_ICONINFORMATION); break;
    }
}

static HMENU BuildMenu() {
    HMENU mb = CreateMenu();
    HMENU mFile = CreatePopupMenu();
    AppendMenuW(mFile, MF_STRING, ID_OPEN, L"&Open Image...");
    AppendMenuW(mFile, MF_STRING, ID_SAVE, L"&Save");
    AppendMenuW(mFile, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(mFile, MF_STRING, ID_DEFAULT, L"Set as &Default Image Viewer");
    AppendMenuW(mFile, MF_STRING, ID_SHOWFOLDER, L"Show in &Folder");
    AppendMenuW(mFile, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(mFile, MF_STRING, ID_EXIT, L"E&xit");
    AppendMenuW(mb, MF_POPUP, (UINT_PTR)mFile, L"&File");

    HMENU mEdit = CreatePopupMenu();
    AppendMenuW(mEdit, MF_STRING, ID_RENAME, L"&Rename");
    AppendMenuW(mEdit, MF_STRING, ID_UNDO, L"&Undo Edit");
    AppendMenuW(mEdit, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(mEdit, MF_STRING, ID_SELALL, L"Select &All");
    AppendMenuW(mEdit, MF_STRING, ID_SELCLR, L"&Clear Selection");
    AppendMenuW(mEdit, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(mEdit, MF_STRING, ID_SETTINGS, L"Set &Hotkeys...");
    AppendMenuW(mb, MF_POPUP, (UINT_PTR)mEdit, L"&Edit");

    HMENU mImg = CreatePopupMenu();
    AppendMenuW(mImg, MF_STRING, ID_ROTL, L"Rotate &Left");
    AppendMenuW(mImg, MF_STRING, ID_ROTR, L"Rotate &Right");
    AppendMenuW(mImg, MF_STRING, ID_SCAN, L"&Scan Document");
    AppendMenuW(mImg, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(mImg, MF_STRING, ID_CROP, L"Crop (Rectangle)");
    AppendMenuW(mImg, MF_STRING, ID_CROPPERSP, L"Crop (Perspective / Auto)");
    AppendMenuW(mb, MF_POPUP, (UINT_PTR)mImg, L"&Image");

    HMENU mView = CreatePopupMenu();
    AppendMenuW(mView, MF_STRING, ID_ZIN, L"Zoom &In");
    AppendMenuW(mView, MF_STRING, ID_ZOUT, L"Zoom &Out");
    AppendMenuW(mView, MF_STRING, ID_ZFIT, L"&Fit");
    AppendMenuW(mView, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(mView, MF_STRING, ID_PREV, L"&Previous Image");
    AppendMenuW(mView, MF_STRING, ID_NEXT, L"&Next Image");
    AppendMenuW(mb, MF_POPUP, (UINT_PTR)mView, L"&View");

    HMENU mHelp = CreatePopupMenu();
    AppendMenuW(mHelp, MF_STRING, ID_ABOUT, L"&About");
    AppendMenuW(mb, MF_POPUP, (UINT_PTR)mHelp, L"&Help");
    return mb;
}

static void ApplyDarkTitlebar(HWND hWnd) {
    BOOL dark = TRUE;
    DwmSetWindowAttribute(hWnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
    int pref = DWMWCP_ROUND;
    DwmSetWindowAttribute(hWnd, DWMWA_WINDOW_CORNER_PREFERENCE, &pref, sizeof(pref));
}

static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE: {
            g_hMain = hWnd;
            HDC sdc = GetDC(nullptr);
            g_dpi = (double)GetDeviceCaps(sdc, LOGPIXELSY);
            if (g_dpi <= 0) g_dpi = 96.0;
            ReleaseDC(nullptr, sdc);
            g_tbh = DPI(64);
            ApplyDarkTitlebar(hWnd);

            g_hFont = CreateFontW(-DPI(10), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_GUI_FONT | FF_SWISS, L"Segoe UI");
            g_hFontBold = CreateFontW(-DPI(10), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_GUI_FONT | FF_SWISS, L"Segoe UI");
            g_hFontName = CreateFontW(-DPI(15), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_GUI_FONT | FF_SWISS, L"Segoe UI");
            g_hbrChip = CreateSolidBrush(RGB(38, 38, 56));

            g_hEdit = CreateWindowExW(0, L"EDIT", L"No image opened",
                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_READONLY,
                0, 0, 200, 30, hWnd, (HMENU)IDC_EDITNAME, g_hInst, nullptr);
            SendMessageW(g_hEdit, WM_SETFONT, (WPARAM)g_hFontName, TRUE);
            SendMessageW(g_hEdit, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELONG(DPI(2), DPI(2)));

            CreateButtons();
            DragAcceptFiles(hWnd, TRUE);
            EnableButtons(false);
            UpdateStatus();
            return 0;
        }
        case WM_SIZE:
            if (wp != SIZE_MINIMIZED) { Layout(); RebuildDisplayCache(); InvalidateRect(hWnd, nullptr, FALSE); }
            return 0;
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps; HDC hdc = BeginPaint(hWnd, &ps);
            Paint(hdc);
            EndPaint(hWnd, &ps);
            return 0;
        }
        case WM_DRAWITEM: {
            DRAWITEMSTRUCT* dis = (DRAWITEMSTRUCT*)lp;
            if (dis->CtlType == ODT_BUTTON) {
                if (dis->CtlID == IDC_CHECKSEL) DrawCheckbox(dis);
                else DrawButton(dis);
            }
            return TRUE;
        }
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORSTATIC:
            if ((HWND)lp == g_hEdit) {
                HDC hdc = (HDC)wp;
                SetTextColor(hdc, RGB(238, 239, 247));
                SetBkColor(hdc, RGB(38, 38, 56));
                return (LRESULT)g_hbrChip;
            }
            break;
        case WM_LBUTTONDOWN: {
            if (g_cropping && g_bmp) {
                POINT p = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
                double W = (double)g_bmp->GetWidth(), H = (double)g_bmp->GetHeight();
                double ix = (p.x - g_ox) / g_scale; double iy = (p.y - g_oy) / g_scale;
                if (ix < 0) ix = 0; if (iy < 0) iy = 0;
                if (ix > W) ix = W; if (iy > H) iy = H;
                if (g_perspCrop) {
                    int best = -1; double bestD = (double)DPI(20);
                    for (int i = 0; i < 4; i++) {
                        double sx = g_ox + g_cropCorners[i][0] * g_scale;
                        double sy = g_oy + g_cropCorners[i][1] * g_scale;
                        double d = sqrt((sx - p.x) * (sx - p.x) + (sy - p.y) * (sy - p.y));
                        if (d < bestD) { bestD = d; best = i; }
                    }
                    if (best >= 0) { g_dragCorner = best; g_dragging = true; SetCapture(hWnd); }
                } else {
                    g_rcX0 = g_rcX1 = ix; g_rcY0 = g_rcY1 = iy;
                    g_dragging = true; SetCapture(hWnd);
                }
            }
            return 0;
        }
        case WM_MOUSEMOVE: {
            if (g_dragging && g_cropping && g_bmp) {
                POINT p = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
                double W = (double)g_bmp->GetWidth(), H = (double)g_bmp->GetHeight();
                double ix = (p.x - g_ox) / g_scale; double iy = (p.y - g_oy) / g_scale;
                if (ix < 0) ix = 0; if (iy < 0) iy = 0;
                if (ix > W) ix = W; if (iy > H) iy = H;
                if (g_perspCrop && g_dragCorner >= 0) {
                    g_cropCorners[g_dragCorner][0] = ix;
                    g_cropCorners[g_dragCorner][1] = iy;
                } else if (!g_perspCrop) {
                    g_rcX1 = ix; g_rcY1 = iy;
                }
                InvalidateRect(hWnd, nullptr, FALSE);
            }
            return 0;
        }
        case WM_LBUTTONUP: {
            if (g_dragging) { g_dragging = false; g_dragCorner = -1; ReleaseCapture(); InvalidateRect(hWnd, nullptr, FALSE); }
            return 0;
        }
        case WM_MOUSEWHEEL: {
            if (g_bmp && GET_WHEEL_DELTA_WPARAM(wp) != 0) {
                int d = GET_WHEEL_DELTA_WPARAM(wp);
                if (d > 0) g_zoom *= 1.15; else g_zoom /= 1.15;
                if (g_zoom > 40) g_zoom = 40; if (g_zoom < 0.05) g_zoom = 0.05;
                InvalidateRect(hWnd, nullptr, FALSE); UpdateStatus();
            }
            return 0;
        }
        case WM_DROPFILES: {
            HDROP drop = (HDROP)wp;
            WCHAR f[MAX_PATH] = {0};
            if (DragQueryFileW(drop, 0, f, MAX_PATH)) LoadImageFromPath(f);
            DragFinish(drop);
            return 0;
        }
        case WM_COMMAND:
            OnCommand(wp, lp);
            return 0;
        case WM_GETMINMAXINFO: {
            MINMAXINFO* m = (MINMAXINFO*)lp;
            m->ptMinTrackSize.x = DPI(820);
            m->ptMinTrackSize.y = DPI(540);
            return 0;
        }
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hWnd, msg, wp, lp);
}

static void UpdateHKButton(int idx) {
    if (!g_hkBtn[idx]) return;
    if (g_captureIdx == idx) SetWindowTextW(g_hkBtn[idx], L"Press a key\u2026  (Esc to cancel)");
    else SetWindowTextW(g_hkBtn[idx], KeyLabel(g_work[idx]).c_str());
}
static void LayoutSettings(HWND h) {
    RECT rc; GetClientRect(h, &rc);
    int pad = DPI(16), rowH = DPI(28), lblW = DPI(200), btnW = DPI(180), gap = DPI(10);
    int y = DPI(44);
    for (int i = 0; i < ACT_COUNT; i++) {
        if (g_label[i]) MoveWindow(g_label[i], pad, y + DPI(4), lblW, DPI(22), TRUE);
        if (g_hkBtn[i]) MoveWindow(g_hkBtn[i], pad + lblW + gap, y, btnW, DPI(28), TRUE);
        y += rowH;
    }
    int by = y + DPI(12);
    int bw = DPI(110);
    HWND r = GetDlgItem(h, IDC_RESET), cn = GetDlgItem(h, IDC_CANCEL), dn = GetDlgItem(h, IDC_DONE);
    if (r) MoveWindow(r, pad, by, bw, DPI(30), TRUE);
    if (dn) MoveWindow(dn, rc.right - pad - bw, by, bw, DPI(30), TRUE);
    if (cn) MoveWindow(cn, rc.right - pad - bw - gap - bw, by, bw, DPI(30), TRUE);
}
static LRESULT CALLBACK HKSubProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR ref) {
    int idx = (int)ref;
    if ((msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN) && g_captureIdx == idx) {
        UINT vk = (UINT)wp;
        if (vk == VK_ESCAPE) { g_captureIdx = -1; UpdateHKButton(idx); return 0; }
        if (vk == VK_SHIFT || vk == VK_CONTROL || vk == VK_MENU) return 0;
        bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        bool alt = (GetKeyState(VK_MENU) & 0x8000) != 0;
        int conf = -1;
        for (int k = 0; k < ACT_COUNT; k++)
            if (k != idx && g_work[k].vk == vk && g_work[k].ctrl == ctrl && g_work[k].shift == shift && g_work[k].alt == alt) { conf = k; break; }
        if (conf >= 0) {
            std::wstring m = L"That key is already used by: "; m += g_work[conf].name;
            MessageBoxW(g_hSettings, m.c_str(), L"Hotkey in use", MB_OK | MB_ICONWARNING);
        } else {
            g_work[idx].vk = vk; g_work[idx].ctrl = ctrl; g_work[idx].shift = shift; g_work[idx].alt = alt;
        }
        g_captureIdx = -1;
        UpdateHKButton(idx);
        return 0;
    }
    return DefSubclassProc(hWnd, msg, wp, lp);
}
static LRESULT CALLBACK SettingsWndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE: {
            HWND hint = CreateWindowW(L"STATIC", L"Click a key box, then press the new key combination.",
                WS_CHILD | WS_VISIBLE, DPI(16), DPI(14), DPI(400), DPI(20), hWnd, (HMENU)1, g_hInst, nullptr);
            SendMessageW(hint, WM_SETFONT, (WPARAM)g_hFont, TRUE);
            for (int i = 0; i < ACT_COUNT; i++) {
                g_label[i] = CreateWindowW(L"STATIC", g_work[i].name, WS_CHILD | WS_VISIBLE, 0, 0, 10, 10, hWnd, (HMENU)(INT_PTR)(3100 + i), g_hInst, nullptr);
                SendMessageW(g_label[i], WM_SETFONT, (WPARAM)g_hFont, TRUE);
                g_hkBtn[i] = CreateWindowW(L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 10, 10, hWnd, (HMENU)(INT_PTR)(IDC_HKBASE + i), g_hInst, nullptr);
                SendMessageW(g_hkBtn[i], WM_SETFONT, (WPARAM)g_hFont, TRUE);
                SetWindowSubclass(g_hkBtn[i], HKSubProc, 0, (DWORD_PTR)(INT_PTR)i);
                UpdateHKButton(i);
            }
            HWND rb = CreateWindowW(L"BUTTON", L"Reset Defaults", WS_CHILD | WS_VISIBLE, 0, 0, 10, 10, hWnd, (HMENU)IDC_RESET, g_hInst, nullptr);
            HWND db = CreateWindowW(L"BUTTON", L"Done", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 0, 0, 10, 10, hWnd, (HMENU)IDC_DONE, g_hInst, nullptr);
            HWND cb = CreateWindowW(L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE, 0, 0, 10, 10, hWnd, (HMENU)IDC_CANCEL, g_hInst, nullptr);
            SendMessageW(rb, WM_SETFONT, (WPARAM)g_hFont, TRUE);
            SendMessageW(db, WM_SETFONT, (WPARAM)g_hFont, TRUE);
            SendMessageW(cb, WM_SETFONT, (WPARAM)g_hFont, TRUE);
            return 0;
        }
        case WM_SIZE: LayoutSettings(hWnd); return 0;
        case WM_COMMAND: {
            int id = LOWORD(wp); int code = HIWORD(wp);
            if (code == BN_CLICKED) {
                if (id >= IDC_HKBASE && id < IDC_HKBASE + ACT_COUNT) {
                    g_captureIdx = id - IDC_HKBASE;
                    UpdateHKButton(g_captureIdx);
                    SetFocus(g_hkBtn[g_captureIdx]);
                } else if (id == IDC_RESET) {
                    for (int i = 0; i < ACT_COUNT; i++) g_work[i] = g_defaultHotkeys[i];
                    for (int i = 0; i < ACT_COUNT; i++) UpdateHKButton(i);
                } else if (id == IDC_DONE) {
                    for (int i = 0; i < ACT_COUNT; i++) g_hotkeys[i] = g_work[i];
                    SaveHotkeys();
                    UpdateMenuHotkeys();
                    PostMessageW(hWnd, WM_CLOSE, 0, 0);
                } else if (id == IDC_CANCEL) {
                    PostMessageW(hWnd, WM_CLOSE, 0, 0);
                }
            }
            return 0;
        }
        case WM_CLOSE:
            EnableWindow(g_hMain, TRUE);
            SetForegroundWindow(g_hMain);
            g_hSettings = nullptr;
            DestroyWindow(hWnd);
            return 0;
    }
    return DefWindowProcW(hWnd, msg, wp, lp);
}
static void OpenSettings() {
    if (g_hSettings) { SetFocus(g_hSettings); return; }
    for (int i = 0; i < ACT_COUNT; i++) g_work[i] = g_hotkeys[i];
    g_captureIdx = -1;
    static bool reg = false;
    if (!reg) {
        WNDCLASSEXW wc = { sizeof(wc) };
        wc.lpfnWndProc = SettingsWndProc;
        wc.hInstance = g_hInst;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = L"IVPSettings";
        RegisterClassExW(&wc);
        reg = true;
    }
    int wW = DPI(440);
    int wH = DPI(44) + ACT_COUNT * DPI(28) + DPI(70);
    g_hSettings = CreateWindowExW(0, L"IVPSettings", L"Hotkey Settings",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, CW_USEDEFAULT, CW_USEDEFAULT, wW, wH,
        g_hMain, nullptr, g_hInst, nullptr);
    EnableWindow(g_hMain, FALSE);
    ShowWindow(g_hSettings, SW_SHOW);
    UpdateWindow(g_hSettings);
}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow) {
    g_hInst = hInstance;

    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_BAR_CLASSES | ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    ULONG_PTR gdiToken = 0;
    Gdiplus::GdiplusStartupInput si;
    Gdiplus::GdiplusStartup(&gdiToken, &si, nullptr);

    LoadHotkeys();

    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv) {
        for (int i = 1; i < argc; i++) {
            std::wstring a = argv[i];
            if (!a.empty() && a[0] != L'/') { g_openOnStart = a; break; }
        }
        LocalFree(argv);
    }

    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = L"ImageViewerProMain";
    wc.hIcon = (HICON)LoadImageW(hInstance, MAKEINTRESOURCEW(1), IMAGE_ICON,
        GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR);
    wc.hIconSm = (HICON)LoadImageW(hInstance, MAKEINTRESOURCEW(1), IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
    RegisterClassExW(&wc);

    g_hMenu = BuildMenu();
    UpdateMenuHotkeys();

    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    int ww = sw * 72 / 100, wh = sh * 82 / 100;

    g_hMain = CreateWindowExW(0, wc.lpszClassName, L"Image Viewer Pro",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        (sw - ww) / 2, (sh - wh) / 2, ww, wh,
        nullptr, g_hMenu, hInstance, nullptr);

    ShowWindow(g_hMain, nCmdShow);
    UpdateWindow(g_hMain);

    if (!g_openOnStart.empty()) LoadImageFromPath(g_openOnStart);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        if (msg.message == WM_KEYDOWN && g_hSettings == nullptr) {
            bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            bool alt = (GetKeyState(VK_MENU) & 0x8000) != 0;
            UINT vk = (UINT)msg.wParam;
            if (g_scanMode) {
                if (vk == VK_UP) { AdjustScan(5); continue; }
                if (vk == VK_DOWN) { AdjustScan(-5); continue; }
                if (vk == VK_RETURN) { CommitScan(); continue; }
                if (vk == VK_ESCAPE) { CancelScan(); continue; }
                continue;
            }
            if (vk == VK_ESCAPE) {
                if (g_cropping) { g_cropping = false; g_perspCrop = false; InvalidateRect(g_hMain, nullptr, FALSE); UpdateStatus(); continue; }
                if (g_editing) { CancelEdit(); continue; }
            } else if (g_cropping) {
                int act = MatchAction(vk, ctrl, shift, alt);
                if (act == ACT_CROP || act == ACT_CROPPERSP) { g_cropping = false; g_perspCrop = false; InvalidateRect(g_hMain, nullptr, FALSE); UpdateStatus(); continue; }
                if (vk == VK_RETURN) { DoCropApply(); continue; }
            } else if (g_editing) {
                if (g_hotkeys[ACT_RENAME].vk == vk && g_hotkeys[ACT_RENAME].ctrl == ctrl && g_hotkeys[ACT_RENAME].shift == shift && g_hotkeys[ACT_RENAME].alt == alt) { CommitEdit(); continue; }
            } else {
                int act = MatchAction(vk, ctrl, shift, alt);
                if (act >= 0) { DispatchAction(act); continue; }
            }
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    CloseImage();
    if (g_hFont) DeleteObject(g_hFont);
    if (g_hFontBold) DeleteObject(g_hFontBold);
    if (g_hFontName) DeleteObject(g_hFontName);
    if (g_hbrChip) DeleteObject(g_hbrChip);
    CoUninitialize();
    Gdiplus::GdiplusShutdown(gdiToken);
    return (int)msg.wParam;
}
