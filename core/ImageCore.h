// ImageCore.h — portable image-processing core (C++17, no platform headers).
// Shared by the Windows (Win32/GDI+) and macOS (AppKit/CoreGraphics) builds.
//
// Buffers are BGRA, 8 bits/channel, 4 bytes/pixel, row-major (stride == w*4).
// All algorithms here are pure C++ and operate on raw BGRA buffers.
#pragma once

#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <cstdio>

namespace ivp {

struct ImageBuf {
    int w = 0, h = 0;
    std::vector<uint8_t> px; // BGRA, size = w*h*4
    bool valid() const { return w > 0 && h > 0 && px.size() == (size_t)w * h * 4; }
    uint8_t* data() { return px.data(); }
    const uint8_t* data() const { return px.data(); }
    void alloc(int W, int H) { w = W; h = H; px.assign((size_t)W * H * 4, 0); }
};

inline float clampf(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }
inline float sstepf(float e0, float e1, float x) {
    float t = (x - e0) / (e1 - e0); if (t < 0) t = 0; if (t > 1) t = 1; return t * t * (3 - 2 * t);
}

// 12-band color adjustment (same model as the Windows build).
// v[0..11]: Brightness Contrast Exposure Highlights Shadows Saturation
//           Vibrance Temperature Tint Gamma Sharpness Vignette (-100..100).
inline void AdjustBuf(const uint8_t* src, int W, int H, const int* v, uint8_t* dst) {
    float exposure = powf(2.0f, v[2] / 100.0f * 2.0f);
    float contrast = 1.0f + v[1] / 100.0f;
    float bright = v[0] / 100.0f * 0.5f;
    float gamma = powf(2.0f, -v[9] / 100.0f);
    float sat = 1.0f + v[5] / 100.0f;
    float vib = v[6] / 100.0f;
    float temp = v[7] / 100.0f * 0.18f;
    float tint = v[8] / 100.0f * 0.18f;
    float hl = v[3] / 100.0f * 0.5f, shw = v[4] / 100.0f * 0.5f;
    float vig = v[11] / 100.0f;
    float cx = (W - 1) * 0.5f, cy = (H - 1) * 0.5f;
    float maxd = sqrtf(cx * cx + cy * cy); if (maxd < 1) maxd = 1;
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            const uint8_t* p = src + ((size_t)y * W + x) * 4;
            float r = p[2] / 255.0f, g = p[1] / 255.0f, b = p[0] / 255.0f;
            r *= exposure; g *= exposure; b *= exposure;
            float lum = 0.299f * r + 0.587f * g + 0.114f * b;
            float hw = sstepf(0.5f, 1.0f, lum), swd = 1.0f - sstepf(0.0f, 0.5f, lum);
            r += hl * hw; g += hl * hw; b += hl * hw;
            r += shw * swd; g += shw * swd; b += shw * swd;
            r += bright; g += bright; b += bright;
            r = (r - 0.5f) * contrast + 0.5f; g = (g - 0.5f) * contrast + 0.5f; b = (b - 0.5f) * contrast + 0.5f;
            r = powf(clampf(r), gamma); g = powf(clampf(g), gamma); b = powf(clampf(b), gamma);
            r += temp; b -= temp; g -= tint;
            float lum2 = 0.299f * r + 0.587f * g + 0.114f * b;
            r = lum2 + (r - lum2) * sat; g = lum2 + (g - lum2) * sat; b = lum2 + (b - lum2) * sat;
            float mx = r; if (g > mx) mx = g; if (b > mx) mx = b; float mn = r; if (g < mn) mn = g; if (b < mn) mn = b;
            float amt = vib * (1.0f - (mx - mn));
            r = lum2 + (r - lum2) * (1.0f + amt); g = lum2 + (g - lum2) * (1.0f + amt); b = lum2 + (b - lum2) * (1.0f + amt);
            float dx = x - cx, dy = y - cy; float d = sqrtf(dx * dx + dy * dy) / maxd;
            float fc = 1.0f - vig * d * d * 1.2f;
            r *= fc; g *= fc; b *= fc;
            uint8_t* q = dst + ((size_t)y * W + x) * 4;
            q[2] = (uint8_t)(clampf(r) * 255.0f + 0.5f);
            q[1] = (uint8_t)(clampf(g) * 255.0f + 0.5f);
            q[0] = (uint8_t)(clampf(b) * 255.0f + 0.5f);
            q[3] = 255;
        }
    }
    if (v[10] != 0) {
        float s = v[10] / 100.0f * 1.2f;
        std::vector<uint8_t> tmp((size_t)W * H * 4);
        for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
            float rr = 0, gg = 0, bb = 0; int n = 0;
            for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
                int xx = x + dx, yy = y + dy; if (xx < 0 || yy < 0 || xx >= W || yy >= H) continue;
                const uint8_t* pp = dst + ((size_t)yy * W + xx) * 4;
                rr += pp[2]; gg += pp[1]; bb += pp[0]; n++;
            }
            uint8_t* o = tmp.data() + ((size_t)y * W + x) * 4;
            o[2] = (uint8_t)(rr / n); o[1] = (uint8_t)(gg / n); o[0] = (uint8_t)(bb / n); o[3] = 255;
        }
        for (size_t i = 0; i < (size_t)W * H; i++) {
            uint8_t* q = dst + i * 4; const uint8_t* t = tmp.data() + i * 4;
            for (int k = 0; k < 3; k++) {
                float c = q[k] / 255.0f + s * (q[k] / 255.0f - t[k] / 255.0f);
                if (c < 0) c = 0; if (c > 1) c = 1; q[k] = (uint8_t)(c * 255 + 0.5);
            }
        }
    }
}

// Solve a homography mapping dstR (axis-aligned rect) <- srcQ (quad).
inline bool SolveHomography(const double srcQ[4][2], const double dstR[4][2], double h[8]) {
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

// Perspective (keystone) warp: extract the quad region from src into an upright ImageBuf.
inline ImageBuf WarpPerspective(const ImageBuf& src, const double quad[4][2]) {
    int W = src.w, H = src.h;
    auto dist = [](double x1, double y1, double x2, double y2){ return sqrt((x2-x1)*(x2-x1)+(y2-y1)*(y2-y1)); };
    double top = dist(quad[0][0], quad[0][1], quad[1][0], quad[1][1]);
    double bot = dist(quad[3][0], quad[3][1], quad[2][0], quad[2][1]);
    double lf  = dist(quad[0][0], quad[0][1], quad[3][0], quad[3][1]);
    double rt  = dist(quad[1][0], quad[1][1], quad[2][0], quad[2][1]);
    int outW = (int)((top + bot) / 2 + 0.5);
    int outH = (int)((lf + rt) / 2 + 0.5);
    if (outW < 2 || outH < 2 || !src.valid()) return {};
    double dstR[4][2] = {{0,0},{(double)outW,0},{(double)outW,(double)outH},{0,(double)outH}};
    double h[8];
    if (!SolveHomography(quad, dstR, h)) return {};
    ImageBuf out; out.alloc(outW, outH);
    const uint8_t* sp = src.data();
    for (int y = 0; y < outH; y++) {
        uint8_t* drow = out.data() + (size_t)y * outW * 4;
        for (int x = 0; x < outW; x++) {
            double den = h[6] * x + h[7] * y + 1.0;
            double sx = (h[0] * x + h[1] * y + h[2]) / den;
            double sy = (h[3] * x + h[4] * y + h[5]) / den;
            uint8_t r, g, b;
            if (sx < 0 || sy < 0 || sx > W - 1 || sy > H - 1) {
                r = g = b = 255;
            } else {
                int x0 = (int)sx, y0 = (int)sy;
                double fx = sx - x0, fy = sy - y0;
                int x1 = x0 + 1; if (x1 > W - 1) x1 = W - 1;
                int y1 = y0 + 1; if (y1 > H - 1) y1 = H - 1;
                const uint8_t* p00 = sp + (size_t)y0 * W * 4 + x0 * 4;
                const uint8_t* p10 = sp + (size_t)y0 * W * 4 + x1 * 4;
                const uint8_t* p01 = sp + (size_t)y0 * 1 * W * 4; (void)p01;
                const uint8_t* p11 = sp + (size_t)y1 * 1 * W * 4; (void)p11;
                const uint8_t* q01 = sp + (size_t)y1 * W * 4 + x0 * 4;
                const uint8_t* q11 = sp + (size_t)y1 * W * 4 + x1 * 4;
                double w00 = (1 - fx) * (1 - fy), w10 = fx * (1 - fy), w01 = (1 - fx) * fy, w11 = fx * fy;
                b  = (uint8_t)(p00[0]*w00 + p10[0]*w10 + q01[0]*w01 + q11[0]*w11 + 0.5);
                g  = (uint8_t)(p00[1]*w00 + p10[1]*w10 + q01[1]*w01 + q11[1]*w11 + 0.5);
                r  = (uint8_t)(p00[2]*w00 + p10[2]*w10 + q01[2]*w01 + q11[2]*w11 + 0.5);
            }
            drow[x*4+0] = b; drow[x*4+1] = g; drow[x*4+2] = r; drow[x*4+3] = 255;
        }
    }
    return out;
}

// Auto-detect a document quad via edge/foreground analysis. Returns false if nothing found
// (caller should fall back to full frame). outCorners are in image pixels.
inline bool AutoDetectCorners(const ImageBuf& src, double outCorners[4][2]) {
    int W = src.w, H = src.h;
    if (W < 8 || H < 8 || !src.valid()) return false;
    std::vector<int> gray((size_t)W * H);
    const uint8_t* s = src.data();
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            const uint8_t* p = s + ((size_t)y * W + x) * 4;
            gray[(size_t)y * W + x] = (int)(p[2] * 0.299f + p[1] * 0.587f + p[0] * 0.114f);
        }
    }
    std::vector<int> border;
    int step = (int)((std::min)(W, H) / 64) + 1;
    for (int x = 0; x < W; x += step) { border.push_back(gray[x]); border.push_back(gray[(size_t)(H-1)*W + x]); }
    for (int y = 0; y < H; y += step) { border.push_back(gray[(size_t)y * W]); border.push_back(gray[(size_t)y * W + (W-1)]); }
    std::sort(border.begin(), border.end());
    int bg = border[border.size() / 2];

    double f = 240.0 / (std::max)(W, H); if (f > 1.0) f = 1.0;
    int sw = (std::max)(1, (int)(W * f + 0.5)), sh = (std::max)(1, (int)(H * f + 0.5));
    std::vector<int> sm((size_t)sw * sh, 0), cnt((size_t)sw * sh, 0);
    for (int y = 0; y < H; y++) { int sy = (int)(y * f); if (sy >= sh) sy = sh - 1;
        for (int x = 0; x < W; x++) { int sx = (int)(x * f); if (sx >= sw) sx = sw - 1;
            size_t o = (size_t)sy * sw + sx; sm[o] += gray[(size_t)y * W + x]; cnt[o]++; } }
    for (size_t i = 0; i < sm.size(); i++) if (cnt[i]) sm[i] /= cnt[i];

    std::vector<int> mg((size_t)sw * sh, 0);
    double sum = 0; long n = 0;
    for (int y = 1; y < sh - 1; y++) for (int x = 1; x < sw - 1; x++) {
        int gx = sm[(size_t)(y-1)*sw + (x+1)] + 2*sm[(size_t)y*sw + (x+1)] + sm[(size_t)(y+1)*sw + (x+1)]
               - sm[(size_t)(y-1)*sw + (x-1)] - 2*sm[(size_t)y*sw + (x-1)] - sm[(size_t)(y+1)*sw + (x-1)];
        int gy = sm[(size_t)(y+1)*sw + (x-1)] + 2*sm[(size_t)(y+1)*sw + x] + sm[(size_t)(y+1)*sw + (x+1)]
               - sm[(size_t)(y-1)*sw + (x-1)] - 2*sm[(size_t)(y-1)*sw + x] - sm[(size_t)(y-1)*sw + (x+1)];
        int m = (int)sqrt((double)gx*gx + (double)gy*gy);
        mg[(size_t)y*sw + x] = m; sum += m; n++;
    }
    double mean = n ? sum / n : 0;
    double sq = 0; for (size_t i = 0; i < mg.size(); i++) { double d = mg[i] - mean; sq += d * d; }
    double sdv = n ? sqrt(sq / n) : 0;
    double Tg = mean + 1.3 * sdv; if (Tg < 18) Tg = 18;

    double tl = 1e18, tr = -1e18, br = -1e18, bl = -1e18;
    double tlX=0,tlY=0,trX=(double)sw,trY=0,brX=(double)sw,brY=(double)sh,blX=0,blY=(double)sh;
    bool any = false;
    for (int y = 1; y < sh - 1; y++) for (int x = 1; x < sw - 1; x++) {
        size_t i = (size_t)y * sw + x;
        int df = sm[i] - bg; if (df < 0) df = -df;
        bool edge = mg[i] > Tg; bool fore = df > 30;
        if (!edge && !fore) continue;
        any = true;
        double X = x, Y = y, a = X + Y, b = X - Y, c = Y - X;
        if (a < tl) { tl = a; tlX = X; tlY = Y; }
        if (b > tr) { tr = b; trX = X; trY = Y; }
        if (a > br) { br = a; brX = X; brY = Y; }
        if (c > bl) { bl = c; blX = X; blY = Y; }
    }
    if (!any) return false;
    double qArea = 0.5 * std::fabs((tlX*trY - trX*tlY) + (trX*brY - brX*trY) + (brX*blY - blX*brY) + (blX*tlY - tlX*blY));
    if (qArea > 0.92 * (sw * sh)) return false;
    outCorners[0][0] = f>0?tlX/f:tlX; outCorners[0][1] = f>0?tlY/f:tlY;
    outCorners[1][0] = f>0?trX/f:trX; outCorners[1][1] = f>0?trY/f:trY;
    outCorners[2][0] = f>0?brX/f:brX; outCorners[2][1] = f>0?brY/f:brY;
    outCorners[3][0] = f>0?blX/f:blX; outCorners[3][1] = f>0?blY/f:blY;
    for (int i = 0; i < 4; i++) {
        if (outCorners[i][0] < 0) outCorners[i][0] = 0;
        if (outCorners[i][1] < 0) outCorners[i][1] = 0;
        if (outCorners[i][0] > W) outCorners[i][0] = W;
        if (outCorners[i][1] > H) outCorners[i][1] = H;
    }
    return true;
}

// CamScanner-style flat-field "scan" of the source. Output is grayscale-white BGRA.
inline ImageBuf ScanDocument(const ImageBuf& src) {
    int W = src.w, H = src.h;
    if (!src.valid()) return {};
    std::vector<uint8_t> gray((size_t)W * H);
    const uint8_t* s = src.data();
    for (size_t i = 0; i < (size_t)W * H; i++) {
        const uint8_t* p = s + i * 4;
        gray[i] = (uint8_t)(p[2] * 0.299f + p[1] * 0.587f + p[0] * 0.114f);
    }
    const int S = 16;
    int bw = W / S > 0 ? W / S : 1;
    int bh = H / S > 0 ? H / S : 1;
    std::vector<float> bg((size_t)bw * bh);
    for (int by = 0; by < bh; by++) for (int bx = 0; bx < bw; bx++) {
        int x0 = bx*S, y0 = by*S, x1 = (std::min)(x0+S, W), y1 = (std::min)(y0+S, H);
        long sum = 0, cnt = 0;
        for (int yy = y0; yy < y1; yy++) for (int xx = x0; xx < x1; xx++) { sum += gray[(size_t)yy*W + xx]; cnt++; }
        bg[(size_t)by*bw + bx] = cnt ? (float)sum/cnt : 0.0f;
    }
    ImageBuf out; out.alloc(W, H);
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
        float g = gray[(size_t)y*W + x];
        float fx = (float)x/S - 0.5f; if (fx < 0) fx = 0; if (fx > bw-1) fx = (float)bw-1;
        float fy = (float)y/S - 0.5f; if (fy < 0) fy = 0; if (fy > bh-1) fy = (float)bh-1;
        int x0i=(int)floor(fx), y0i=(int)floor(fy);
        int x1i=(std::min)(x0i+1, bw-1), y1i=(std::min)(y0i+1, bh-1);
        float tx=fx-x0i, ty=fy-y0i;
        float b = bg[(size_t)y0i*bw + x0i]*(1-tx)*(1-ty) + bg[(size_t)y0i*bw + x1i]*tx*(1-ty)
                + bg[(size_t)y1i*bw + x0i]*(1-tx)*ty     + bg[(size_t)y1i*bw + x1i]*tx*ty;
        if (b < 1.0f) b = 1.0f;
        float t = g * 255.0f / b; if (t<0)t=0; if(t>255)t=255;
        t = (t - 128.0f) * 1.45f + 128.0f; if (t<0)t=0; if(t>255)t=255;
        t = powf(t/255.0f, 0.82f) * 255.0f; if (t>236) t=255;
        uint8_t v = (uint8_t)t;
        uint8_t* o = out.data() + ((size_t)y*W + x)*4;
        o[0]=v; o[1]=v; o[2]=v; o[3]=255;
    }
    return out;
}

// Rotate an ImageBuf 90 degrees (clockwise if right else counter-clockwise).
inline ImageBuf Rotate90(const ImageBuf& src, bool right) {
    if (!src.valid()) return {};
    int W = src.w, H = src.h;
    ImageBuf out;
    out.alloc(H, W);
    const uint8_t* s = src.data();
    uint8_t* d = out.data();
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
        int nx, ny;
        if (right) { nx = H - 1 - y; ny = x; }       // -> (outW=H, outH=W)
        else       { nx = y; ny = W - 1 - x; }
        const uint8_t* p = s + ((size_t)y*W + x)*4;
        uint8_t* q = d + ((size_t)ny*out.w + nx)*4;
        q[0]=p[0]; q[1]=p[1]; q[2]=p[2]; q[3]=p[3];
    }
    return out;
}

struct PdfPage { int W = 0, H = 0; std::vector<uint8_t> bytes; }; // JPEG bytes

// Write a multi-page PDF from pre-encoded JPEG pages (portable file I/O).
inline bool WritePDFFile(const std::string& path, const std::vector<PdfPage>& pages) {
    std::string pdf; pdf.reserve(1 << 16);
    pdf += "%PDF-1.4\n%\xE2\xE3\xCF\xD3\n";
    std::vector<long long> off;
    int totalObjs = 2 + (int)pages.size() * 3;
    off.resize(totalObjs + 1, 0);
    auto beginObj = [&](int n) { off[n] = (long long)pdf.size(); pdf += std::to_string(n) + " 0 obj\n"; };
    auto wr = [&](const std::string& s) { pdf += s; };

    beginObj(1); wr("<< /Type /Catalog /Pages 2 0 R >>\nendobj\n");
    beginObj(2);
    std::string kids;
    for (size_t i = 0; i < pages.size(); i++) kids += std::to_string(3 + (int)i * 3) + " 0 R ";
    wr(std::string("<< /Type /Pages /Kids [ ") + kids + std::string("] /Count ") + std::to_string(pages.size()) + " >>\nendobj\n");

    const double PW = 612.0, PH = 792.0, M = 24.0;
    for (size_t i = 0; i < pages.size(); i++) {
        const PdfPage& pg = pages[i];
        bool landscape = pg.W > 0 && pg.H > 0 && (double)pg.W / (double)pg.H > (PW / PH);
        double pw = landscape ? PH : PW, ph = landscape ? PW : PH;
        double availW = pw - 2*M, availH = ph - 2*M;
        double sc = (std::min)(availW / pg.W, availH / pg.H);
        double dw = pg.W * sc, dh = pg.H * sc, x = (pw - dw)/2.0, y = (ph - dh)/2.0;
        int pageObj = 3 + (int)i * 3, imgObj = pageObj + 1, contObj = pageObj + 2;
        char buf[512];
        beginObj(pageObj);
        snprintf(buf, sizeof(buf),
            "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 %.2f %.2f] "
            "/Resources << /XObject << /Im0 %d 0 R >> /ProcSet [/PDF /ImageC] >> "
            "/Contents %d 0 R >>\nendobj\n", pw, ph, imgObj, contObj);
        wr(buf);
        beginObj(imgObj);
        snprintf(buf, sizeof(buf),
            "<< /Type /XObject /Subtype /Image /Width %d /Height %d "
            "/ColorSpace /DeviceRGB /BitsPerComponent 8 /Filter /DCTDecode /Length %zu >>\nstream\n",
            pg.W, pg.H, pg.bytes.size());
        wr(buf);
        pdf.append((const char*)pg.bytes.data(), pg.bytes.size());
        wr("\nendstream\nendobj\n");
        beginObj(contObj);
        snprintf(buf, sizeof(buf), "q\n%.2f 0 0 %.2f %.2f %.2f cm\n/Im0 Do\nQ\n", dw, dh, x, y);
        std::string cs = buf;
        snprintf(buf, sizeof(buf), "<< /Length %zu >>\nstream\n", cs.size());
        wr(buf); wr(cs); wr("endstream\nendobj\n");
    }
    long long xrefPos = (long long)pdf.size();
    char hb[40];
    snprintf(hb, sizeof(hb), "xref\n0 %d\n", totalObjs + 1); wr(hb);
    wr("0000000000 65535 f \n");
    for (int n = 1; n <= totalObjs; n++) { snprintf(hb, sizeof(hb), "%010lld 00000 n \n", off[n]); wr(hb); }
    snprintf(hb, sizeof(hb), "trailer\n<< /Size %d /Root 1 0 R >>\nstartxref\n%lld\n%%%%EOF\n", totalObjs + 1, xrefPos);
    wr(hb);

    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f.write(pdf.data(), (std::streamsize)pdf.size());
    return f.good();
}

} // namespace ivp
