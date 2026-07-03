// IVPDocument.mm — implementation of the macOS document model.
#import "IVPDocument.h"
#import <ImageIO/ImageIO.h>
#import <CoreGraphics/CoreGraphics.h>
#include <cstring>

static NSArray<NSString *> *IVPImageExts() {
    static NSArray *e = @[@".jpg",@".jpeg",@".jpe",@".jfif",@".png",@".gif",@".bmp",@".dib",
                          @".tif",@".tiff",@".webp",@".heic",@".heif"];
    return e;
}
static BOOL IVPIsImageURL(NSURL *url) {
    NSString *ext = url.pathExtension.lowercaseString;
    return [IVPImageExts() containsObject:[@"." stringByAppendingString:ext]];
}

static CGColorSpaceRef IVPSRGB() { return CGColorSpaceCreateWithName(kCGColorSpaceSRGB); }

// CGImage -> BGRA ImageBuf (top-row-first), via a little-endian 32-bit context (=> BGRA).
static ivp::ImageBuf CGImageToImageBuf(CGImageRef img) {
    int w = (int)CGImageGetWidth(img), h = (int)CGImageGetHeight(img);
    ivp::ImageBuf b; if (w <= 0 || h <= 0) return b;
    b.alloc(w, h);
    CGColorSpaceRef cs = IVPSRGB();
    CGContextRef ctx = CGBitmapContextCreate(b.px.data(), w, h, 8, w * 4, cs,
                                             kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little);
    CGColorSpaceRelease(cs);
    if (!ctx) return b;
    CGContextTranslateCTM(ctx, 0, (CGFloat)h);
    CGContextScaleCTM(ctx, 1, -1);
    CGContextDrawImage(ctx, CGRectMake(0, 0, w, h), img);
    CGContextRelease(ctx);
    return b;
}
static CGImageRef ImageBufToCGImage(const ivp::ImageBuf &b) {
    if (!b.valid()) return NULL;
    CGColorSpaceRef cs = IVPSRGB();
    CGDataProviderRef prov = CGDataProviderCreateWithData(NULL, b.px.data(), b.px.size(), NULL);
    CGImageRef img = CGImageCreate(b.w, b.h, 8, 32, b.w * 4, cs,
                                   kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little,
                                   prov, NULL, false, kCGRenderingIntentDefault);
    CGDataProviderRelease(prov); CGColorSpaceRelease(cs);
    return img;
}
static ivp::ImageBuf LoadImageBufFromURL(NSURL *url) {
    ivp::ImageBuf empty;
    CGImageSourceRef src = CGImageSourceCreateWithURL((__bridge CFURLRef)url, NULL);
    if (!src) return empty;
    CGImageRef img = CGImageSourceCreateImageAtIndex(src, 0, NULL);
    CFRelease(src);
    if (!img) return empty;
    ivp::ImageBuf b = CGImageToImageBuf(img);
    CGImageRelease(img);
    return b;
}
// Composite a (possibly transparent, premultiplied) buffer over white for JPEG export.
static ivp::ImageBuf CompositeOverWhite(const ivp::ImageBuf &s) {
    ivp::ImageBuf o; if (!s.valid()) return o; o.alloc(s.w, s.h);
    for (size_t i = 0; i < (size_t)s.w * s.h; i++) {
        const uint8_t *p = s.px.data() + i * 4;
        uint8_t a = p[3];
        float af = a / 255.0f;
        uint8_t *q = o.px.data() + i * 4;
        q[2] = (uint8_t)(p[2] + (1 - af) * 255); // R (premultiplied + white*remaining)
        q[1] = (uint8_t)(p[1] + (1 - af) * 255); // G
        q[0] = (uint8_t)(p[0] + (1 - af) * 255); // B
        q[3] = 255;
    }
    return o;
}
static bool EncodeJPEGBytes(const ivp::ImageBuf &src, std::vector<uint8_t> &out) {
    ivp::ImageBuf w = CompositeOverWhite(src);
    CGImageRef img = ImageBufToCGImage(w);
    if (!img) return false;
    NSMutableData *md = [NSMutableData data];
    CGImageDestinationRef dst = CGImageDestinationCreateWithData((__bridge CFMutableDataRef)md,
                                                                  CFSTR("public.jpeg"), 1, NULL);
    if (!dst) { CGImageRelease(img); return false; }
    NSDictionary *props = @{ (__bridge NSString *)kCGImageDestinationLossyCompressionQuality: @0.9 };
    CGImageDestinationAddImage(dst, img, (__bridge CFDictionaryRef)props);
    bool ok = CGImageDestinationFinalize(dst);
    CFRelease(dst); CGImageRelease(img);
    if (!ok) return false;
    out.assign((const uint8_t *)md.bytes, (const uint8_t *)md.bytes + md.length);
    return true;
}

@interface IVPDocument ()
{
    ivp::ImageBuf _work;
    CGImageRef _display;
    std::vector<ivp::ImageBuf> _undo, _redo;
    std::vector<std::string> _undoLabel;
    BOOL _editing; ivp::ImageBuf _editBase, _editSmall; int _adjVals[12];
    BOOL _scanning; ivp::ImageBuf _scanBase, _scanFull; int _scanLevel;
    BOOL _cropping, _perspCrop; double _corners[4][2]; double _rcX0,_rcY0,_rcX1,_rcY1;
}
@property (nonatomic, strong) NSMutableArray<NSURL *> *files;
@property (nonatomic) NSInteger index;
@property (nonatomic, strong) NSMutableSet<NSURL *> *selection;
@property (nonatomic, strong) NSCache<NSURL *, NSImage *> *thumbCache;
@end

@implementation IVPDocument

- (instancetype)init {
    if ((self = [super init])) {
        _files = [NSMutableArray array];
        _selection = [NSMutableSet set];
        _thumbCache = [NSCache new];
        _thumbCache.countLimit = 400;
        _index = -1;
        _editing = NO; _scanning = NO; _cropping = NO; _perspCrop = NO; _scanLevel = 70;
        for (int i = 0; i < 12; i++) _adjVals[i] = 0;
        for (int i = 0; i < 4; i++) { _corners[i][0] = 0; _corners[i][1] = 0; }
    }
    return self;
}
- (void)dealloc { if (_display) CGImageRelease(_display); }

- (BOOL)hasImage { return _work.valid(); }
- (int)imageW { return _work.w; }
- (int)imageH { return _work.h; }
- (NSUInteger)folderCount { return _files.count; }
- (NSInteger)selectionCount { return (NSInteger)_selection.count; }
- (NSUInteger)undoCount { return _undo.size(); }
- (NSUInteger)redoCount { return _redo.size(); }
- (CGImageRef)displayImage { return _display; }

- (BOOL)isCurrentSelected { return _currentURL && [_selection containsObject:_currentURL]; }

- (void)refreshDisplay {
    if (_display) { CGImageRelease(_display); _display = NULL; }
    if (_work.valid()) _display = ImageBufToCGImage(_work);
    [_delegate documentDidChange:self];
}

- (void)rebuildFolderList {
    [_files removeAllObjects];
    if (!_currentURL) { _index = -1; return; }
    NSURL *dir = [_currentURL URLByDeletingLastPathComponent];
    NSFileManager *fm = [NSFileManager defaultManager];
    NSArray *names = [fm contentsOfDirectoryAtPath:dir.path error:nil];
    NSMutableArray *tmp = [NSMutableArray array];
    for (NSString *n in names) {
        NSURL *u = [dir URLByAppendingPathComponent:n];
        NSNumber *isDir = nil;
        if ([u getResourceValue:&isDir forKey:NSURLIsDirectoryKey error:nil] && isDir.boolValue) continue;
        if (IVPIsImageURL(u)) [tmp addObject:u];
    }
    [tmp sortUsingComparator:^NSComparisonResult(NSURL *a, NSURL *b) {
        return [a.lastPathComponent localizedStandardCompare:b.lastPathComponent];
    }];
    [_files addObjectsFromArray:tmp];
    _index = (NSInteger)[_files indexOfObject:_currentURL];
    if (_index == NSNotFound) _index = -1;
    // prune selection to existing files
    NSMutableArray *keep = [NSMutableArray array];
    for (NSURL *u in _selection) if ([_files containsObject:u]) [keep addObject:u];
    [_selection removeAllObjects];
    for (NSURL *u in keep) [_selection addObject:u];
}

- (void)loadWorkFromURL:(NSURL *)url {
    ivp::ImageBuf b = LoadImageBufFromURL(url);
    _work = b;
    _undo.clear(); _redo.clear(); _undoLabel.clear();
    [self refreshDisplay];
}

- (void)openURL:(NSURL *)url {
    _currentURL = url;
    [self rebuildFolderList];
    [self loadWorkFromURL:url];
}

- (void)switchTo:(NSInteger)i {
    if (_files.count == 0) return;
    NSInteger n = (NSInteger)_files.count;
    NSInteger j = ((i % n) + n) % n;
    NSURL *u = _files[j];
    _currentURL = u;
    _index = j;
    [self loadWorkFromURL:u];
}
- (void)goPrev { if (_files.count > 1) [self switchTo:_index - 1]; }
- (void)goNext { if (_files.count > 1) [self switchTo:_index + 1]; }

- (void)pushHistory:(const ivp::ImageBuf&)snap label:(const char*)lbl {
    _undo.push_back(snap);
    _undoLabel.push_back(lbl ? std::string(lbl) : "");
    _redo.clear();
}
- (void)rotateRight {
    if (!_work.valid()) return;
    [self pushHistory:_work label:"Rotate right"];
    _work = ivp::Rotate90(_work, true);
    [self refreshDisplay];
}
- (void)rotateLeft {
    if (!_work.valid()) return;
    [self pushHistory:_work label:"Rotate left"];
    _work = ivp::Rotate90(_work, false);
    [self refreshDisplay];
}
- (void)undo {
    if (_undo.empty()) return;
    _redo.push_back(_work);
    ivp::ImageBuf e = _undo.back(); _undo.pop_back(); _undoLabel.pop_back();
    _work = e;
    [self refreshDisplay];
}
- (void)redo {
    if (_redo.empty()) return;
    _undo.push_back(_work); _undoLabel.push_back("redo");
    ivp::ImageBuf e = _redo.back(); _redo.pop_back();
    _work = e;
    [self refreshDisplay];
}

- (void)toggleSelectCurrent {
    if (!_currentURL) return;
    if ([_selection containsObject:_currentURL]) [_selection removeObject:_currentURL];
    else [_selection addObject:_currentURL];
    [_delegate documentSelectionDidChange:self];
}
- (void)selectAll {
    for (NSURL *u in _files) [_selection addObject:u];
    [_delegate documentSelectionDidChange:self];
}
- (void)clearSelection {
    [_selection removeAllObjects];
    [_delegate documentSelectionDidChange:self];
}

- (NSArray<NSURL *> *)selectedURLsSorted {
    NSArray *a = [_selection allObjects];
    return [a sortedArrayUsingComparator:^NSComparisonResult(NSURL *x, NSURL *y) {
        return [x.path localizedStandardCompare:y.path];
    }];
}

#pragma mark Adjust photo
- (BOOL)editing { return _editing; }
- (int)adjCount { return 12; }
- (int)adjustValue:(int)i { return (i>=0 && i<12) ? _adjVals[i] : 0; }
- (void)enterEdit {
    if (_editing || !_work.valid()) return;
    _editBase = _work;
    double f = 700.0 / (MAX(_editBase.w, _editBase.h)); if (f > 1) f = 1;
    _editSW = MAX(1, (int)(_editBase.w * f + 0.5));
    _editSH = MAX(1, (int)(_editBase.h * f + 0.5));
    _editSmall.alloc(_editSW, _editSH);
    // box-average downscale
    for (int y = 0; y < _editSH; y++) for (int x = 0; x < _editSW; x++) {
        int sr0 = (int)(y / f), sr1 = (int)((y+1)/f), sc0 = (int)(x/f), sc1 = (int)((x+1)/f);
        if (sr1<=sr0) sr1=sr0+1; if (sc1<=sc0) sc1=sc0+1;
        long rr=0,gg=0,bb=0,n=0;
        for (int yy=sr0; yy<sr1 && yy<_editBase.h; yy++) for (int xx=sc0; xx<sc1 && xx<_editBase.w; xx++) {
            const uint8_t *p = _editBase.px.data() + ((size_t)yy*_editBase.w + xx)*4;
            rr+=p[2]; gg+=p[1]; bb+=p[0]; n++;
        }
        if (!n) n=1;
        uint8_t *q = _editSmall.px.data() + ((size_t)y*_editSW + x)*4;
        q[2]=(uint8_t)(rr/n); q[1]=(uint8_t)(gg/n); q[0]=(uint8_t)(bb/n); q[3]=255;
    }
    for (int i=0;i<12;i++) _adjVals[i]=0;
    _editing = YES;
    [self recomputeEdit];
}
- (void)recomputeEdit {
    if (!_editing) return;
    ivp::ImageBuf out; out.alloc(_editSW, _editSH);
    ivp::AdjustBuf(_editSmall.data(), _editSW, _editSH, _adjVals, out.data());
    _work = out;
    if (_display) { CGImageRelease(_display); _display = NULL; }
    _display = ImageBufToCGImage(_work);
    [_delegate documentDidChange:self];
}
- (void)setAdjustValue:(int)i value:(int)v { if (i>=0 && i<12) { _adjVals[i]=v; [self recomputeEdit]; } }
- (void)resetAdjust { for (int i=0;i<12;i++) _adjVals[i]=0; [self recomputeEdit]; }
- (void)applyEdit {
    if (!_editing) return;
    int W=_editBase.w, H=_editBase.h;
    ivp::ImageBuf out; out.alloc(W, H);
    ivp::AdjustBuf(_editBase.data(), W, H, _adjVals, out.data());
    [self pushHistory:_editBase label:"Adjust photo"];
    _work = out;
    _editBase = ivp::ImageBuf(); _editSmall = ivp::ImageBuf();
    _editing = NO;
    [self refreshDisplay];
}
- (void)cancelEdit {
    if (!_editing) return;
    _work = _editBase; _editBase = ivp::ImageBuf(); _editSmall = ivp::ImageBuf();
    _editing = NO;
    [self refreshDisplay];
}

#pragma mark Scan document
- (BOOL)scanning { return _scanning; }
- (int)scanLevel { return _scanLevel; }
static ivp::ImageBuf BlendScan(const ivp::ImageBuf &b, const ivp::ImageBuf &f, double k) {
    ivp::ImageBuf o; if (!b.valid()) return o; o.alloc(b.w, b.h);
    int n = b.w * b.h;
    for (int i = 0; i < n; i++) {
        const uint8_t *p = b.px.data() + i*4, *q = f.px.data() + i*4;
        uint8_t *r = o.px.data() + i*4;
        for (int c = 0; c < 3; c++) r[c] = (uint8_t)(p[c] + (q[c]-p[c])*k + 0.5);
        r[3] = 255;
    }
    return o;
}
- (void)enterScan {
    if (_scanning || !_work.valid()) return;
    _scanBase = _work;
    _scanFull = ivp::ScanDocument(_scanBase);
    _scanLevel = 70; _scanning = YES;
    _work = BlendScan(_scanBase, _scanFull, _scanLevel/100.0);
    [self refreshDisplay];
}
- (void)setScanLevel:(int)lvl {
    if (!_scanning) return;
    if (lvl<0) lvl=0; if (lvl>100) lvl=100; _scanLevel = lvl;
    _work = BlendScan(_scanBase, _scanFull, _scanLevel/100.0);
    [self refreshDisplay];
}
- (void)commitScan {
    if (!_scanning) return;
    [self pushHistory:_scanBase label:"Scan"];
    _scanBase = ivp::ImageBuf(); _scanFull = ivp::ImageBuf(); _scanning = NO;
    [self refreshDisplay];
}
- (void)cancelScan {
    if (!_scanning) return;
    _work = _scanBase; _scanBase = ivp::ImageBuf(); _scanFull = ivp::ImageBuf(); _scanning = NO;
    [self refreshDisplay];
}

#pragma mark Crop
- (BOOL)cropping { return _cropping; }
- (BOOL)perspCrop { return _perspCrop; }
- (double)cornerX:(int)i { return (i>=0&&i<4)?_corners[i][0]:0; }
- (double)cornerY:(int)i { return (i>=0&&i<4)?_corners[i][1]:0; }
- (void)setCornerX:(int)i x:(double)x y:(double)y { if (i>=0&&i<4){ _corners[i][0]=x; _corners[i][1]=y; } }
- (void)setRectCropX0:(double)x0 y0:(double)y0 x1:(double)x1 y1:(double)y1 { _rcX0=x0;_rcY0=y0;_rcX1=x1;_rcY1=y1; }
- (double)rcX0 { return _rcX0; } - (double)rcY0 { return _rcY0; }
- (double)rcX1 { return _rcX1; } - (double)rcY1 { return _rcY1; }
- (void)startRectCrop {
    if (!_work.valid()) return;
    _cropping = YES; _perspCrop = NO; _rcX0=_rcY0=_rcX1=_rcY1=0;
    [_delegate documentDidChange:self];
}
- (void)startPerspCrop {
    if (!_work.valid()) return;
    _cropping = YES; _perspCrop = YES;
    double q[4][2] = {{0,0},{(double)_work.w,0},{(double)_work.w,(double)_work.h},{0,(double)_work.h}};
    ivp::AutoDetectCorners(_work, q); // auto-detect; falls back to full frame
    for (int i=0;i<4;i++){ _corners[i][0]=q[i][0]; _corners[i][1]=q[i][1]; }
    [_delegate documentDidChange:self];
}
- (void)applyCrop {
    if (!_cropping) return;
    if (_perspCrop) {
        double q[4][2]; for (int i=0;i<4;i++){ q[i][0]=_corners[i][0]; q[i][1]=_corners[i][1]; }
        ivp::ImageBuf nb = ivp::WarpPerspective(_work, q);
        if (nb.valid()) { [self pushHistory:_work label:"Perspective crop"]; _work = nb; }
    } else {
        double minX=MIN(_rcX0,_rcX1), maxX=MAX(_rcX0,_rcX1), minY=MIN(_rcY0,_rcY1), maxY=MAX(_rcY0,_rcY1);
        int x=(int)minX, y=(int)minY, w=(int)(maxX-minX), h=(int)(maxY-minY);
        if (x<0)x=0; if(y<0)y=0; if(x+w>_work.w)w=_work.w-x; if(y+h>_work.h)h=_work.h-y;
        if (w>1 && h>1) {
            ivp::ImageBuf nb; nb.alloc(w,h);
            for (int yy=0; yy<h; yy++) memcpy(nb.px.data()+(size_t)yy*w*4, _work.px.data()+((size_t)(y+yy)*_work.w+x)*4, (size_t)w*4);
            [self pushHistory:_work label:"Crop"]; _work = nb;
        }
    }
    _cropping = NO; _perspCrop = NO;
    [self refreshDisplay];
}
- (void)cancelCrop { _cropping = NO; _perspCrop = NO; [_delegate documentDidChange:self]; }

- (NSImage *)thumbnailForURL:(NSURL *)url maxSize:(CGFloat)s {
    NSImage *cached = [_thumbCache objectForKey:url];
    if (cached) return cached;
    CGImageSourceRef src = CGImageSourceCreateWithURL((__bridge CFURLRef)url, NULL);
    if (!src) return nil;
    NSDictionary *opts = @{
        (__bridge NSString *)kCGImageSourceThumbnailMaxPixelSize: @(s),
        (__bridge NSString *)kCGImageSourceCreateThumbnailFromImageAlways: @YES,
        (__bridge NSString *)kCGImageSourceCreateThumbnailWithTransform: @YES
    };
    CGImageRef tg = CGImageSourceCreateThumbnailAtIndex(src, 0, (__bridge CFDictionaryRef)opts);
    CFRelease(src);
    if (!tg) return nil;
    NSImage *im = [[NSImage alloc] initWithCGImage:tg size:NSMakeSize((CGFloat)CGImageGetWidth(tg), (CGFloat)CGImageGetHeight(tg))];
    CGImageRelease(tg);
    [_thumbCache setObject:im forKey:url];
    return im;
}

- (void)deleteSelection {
    NSArray<NSURL *> *toDelete = [self selectedURLsSorted];
    if (toDelete.count == 0) return;
    NSAlert *alert = [NSAlert new];
    alert.messageText = [NSString stringWithFormat:@"Move %lu selected image%@ to Trash?",
                         (unsigned long)toDelete.count, toDelete.count == 1 ? @"" : @"s"];
    alert.informativeText = @"You can restore them from the Trash.";
    [alert addButtonWithTitle:@"Move to Trash"];
    [alert addButtonWithTitle:@"Cancel"];
    [alert beginSheetModalForWindow:[NSApp keyWindow] completionHandler:^(NSModalResponse r) {
        if (r != NSAlertFirstButtonReturn) return;
        dispatch_async(dispatch_get_main_queue(), ^{
            NSError *err = nil;
            [[NSWorkspace sharedWorkspace] recycleURLs:toDelete completionHandler:nil];
            (void)err;
            NSMutableSet *gone = [NSMutableSet setWithArray:toDelete];
            // remove still-existing selections, keep the rest
            NSMutableArray *keep = [NSMutableArray array];
            for (NSURL *u in self->_selection) if (![gone containsObject:u]) [keep addObject:u];
            [self->_selection removeAllObjects];
            for (NSURL *u in keep) [self->_selection addObject:u];
            [self rebuildFolderList];
            if ([gone containsObject:self->_currentURL]) {
                if (self->_files.count > 0) [self switchTo:MAX(0, self->_index)];
                else { self->_currentURL = nil; self->_work = ivp::ImageBuf(); [self refreshDisplay]; }
            }
            [self->_delegate documentSelectionDidChange:self];
            [self->_delegate documentDidChange:self];
        });
    }];
}

- (BOOL)exportSelectionToPDF {
    NSArray<NSURL *> *list = [self selectedURLsSorted];
    if (list.count == 0 && _currentURL) list = @[_currentURL];
    if (list.count == 0) {
        NSBeep(); return NO;
    }
    NSSavePanel *sp = [NSSavePanel savePanel];
    sp.allowedFileTypes = @[@"pdf"];
    sp.nameFieldStringValue = @"images.pdf";
    if ([sp runModal] != NSModalResponseOK) return NO;
    const char *outc = [sp.URL.path UTF8String];
    std::vector<ivp::PdfPage> pages;
    int failed = 0;
    for (NSURL *u in list) {
        ivp::ImageBuf b = LoadImageBufFromURL(u);
        if (!b.valid()) { failed++; continue; }
        ivp::PdfPage pg; pg.W = b.w; pg.H = b.h;
        if (!EncodeJPEGBytes(b, pg.bytes)) { failed++; continue; }
        pages.push_back(pg);
    }
    bool ok = !pages.empty() && ivp::WritePDFFile(outc, pages);
    if (!ok) {
        NSAlert *a = [NSAlert new]; a.messageText = @"PDF export failed."; [a runModal];
        return NO;
    }
    NSAlert *a = [NSAlert new];
    a.messageText = [NSString stringWithFormat:@"Exported %lu page%@ to PDF.",
                     (unsigned long)pages.size(), pages.size() == 1 ? @"" : @"s"];
    if (failed > 0) a.informativeText = [NSString stringWithFormat:@"%d image(s) could not be read.", failed];
    [a runModal];
    return YES;
}

@end
