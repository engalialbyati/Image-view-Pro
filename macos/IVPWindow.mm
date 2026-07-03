// IVPWindow.mm — assembles toolbar, sidebar (thumbnails), image view, status bar.
#import "IVPWindow.h"
#import "IVPDocument.h"
#import "IVPImageView.h"

static NSButton *IVPMakeBtn(SEL action, id target, NSString *symbol, NSString *tip) {
    NSButton *b = [NSButton buttonWithImage:[NSImage imageWithSystemSymbolName:symbol accessibilityDescription:tip]
                                     target:target action:action];
    b.bezelStyle = NSBezelStyleAccessoryBar;
    b.toolTip = tip;
    return b;
}

// ---- Sidebar: scrollable list of selected-image thumbnails ----
@interface IVPSidebar : NSView
@property (nonatomic, weak) IVPDocument *document;
@property (nonatomic, weak) NSWindow *hostWindow;
@property (nonatomic) CGFloat rowH;
- (void)reload;
@end
@implementation IVPSidebar {
    NSInteger _hover;
}
- (instancetype)initWithFrame:(NSRect)r {
    if ((self = [super initWithFrame:r])) { _rowH = 104; _hover = -1; }
    return self;
}
- (BOOL)isFlipped { return YES; }
- (void)reload { [self setNeedsDisplay:YES]; }
- (CGFloat)contentHeight { return MAX(0, (CGFloat)self.document.selectionCount * _rowH); }
- (void)drawRect:(NSRect)d {
    (void)d;
    [[NSColor colorWithSRGBRed:0.078 green:0.082 blue:0.106 alpha:1.0] setFill]; NSRectFill(self.bounds);
    NSInteger n = (NSInteger)self.document.selectionCount;
    NSArray<NSURL *> *items = [self.document selectedURLsSorted];
    CGFloat s = 76, pad = (_rowH - s) / 2;
    NSFont *nameFont = [NSFont systemFontOfSize:11];
    for (NSInteger i = 0; i < (NSInteger)items.count; i++) {
        CGFloat y = (CGFloat)i * _rowH;
        if (i == _hover) {
            [[NSColor colorWithWhite:1 alpha:0.05] setFill];
            NSRectFill(NSMakeRect(4, y, NSWidth(self.bounds) - 8, _rowH - 2));
        }
        CGFloat x = (NSWidth(self.bounds) - s) / 2;
        NSRect thumb = NSMakeRect(x, y + pad, s, s);
        [[NSColor colorWithWhite:1 alpha:0.06] setFill];
        NSBezierPath *bp = [NSBezierPath bezierPathWithRoundedRect:thumb xRadius:8 yRadius:8];
        [bp fill];
        NSImage *im = [self.document thumbnailForURL:items[i] maxSize:s];
        if (im) {
            NSSize is = im.size; double sc = MIN(s / is.width, s / is.height);
            NSSize ds = NSMakeSize(is.width * sc, is.height * sc);
            NSRect dr = NSMakeRect(thumb.origin.x + (s - ds.width) / 2, thumb.origin.y + (s - ds.height) / 2, ds.width, ds.height);
            [im drawInRect:dr fromRect:NSZeroRect operation:NSCompositingOperationSourceOver fraction:1 respectFlipped:YES hints:nil];
        }
        NSString *name = items[i].URLByDeletingPathExtension.lastPathComponent;
        NSDictionary *attr = @{NSFontAttributeName: nameFont,
                               NSForegroundColorAttributeName: (i == _hover ? [NSColor whiteColor] : [NSColor colorWithWhite:0.72 alpha:1])};
        [name drawInRect:NSMakeRect(8, y + pad + s + 2, NSWidth(self.bounds) - 16, 14)
            withAttributes:attr];
    }
    (void)n;
}
- (NSInteger)rowAtPoint:(NSPoint)p { NSInteger i = (NSInteger)(p.y / _rowH); NSArray *items = [self.document selectedURLsSorted]; return (i >= 0 && i < (NSInteger)items.count) ? i : -1; }
- (void)mouseMoved:(NSEvent *)e { NSPoint p = [self convertPoint:e.locationInWindow fromView:nil]; NSInteger h = [self rowAtPoint:p]; if (h != _hover) { _hover = h; [self setNeedsDisplay:YES]; } }
- (void)mouseDown:(NSEvent *)e {
    NSPoint p = [self convertPoint:e.locationInWindow fromView:nil];
    NSInteger i = [self rowAtPoint:p]; if (i < 0) return;
    NSArray<NSURL *> *items = [self.document selectedURLsSorted];
    [self.document openURL:items[i]];
}
- (void)mouseExited:(NSEvent *)e { (void)e; _hover = -1; [self setNeedsDisplay:YES]; }
- (void)updateTrackingAreas {
    [super updateTrackingAreas];
    for (NSTrackingArea *ta in self.trackingAreas) [self removeTrackingArea:ta];
    NSTrackingArea *ta = [[NSTrackingArea alloc] initWithRect:self.bounds options:(NSTrackingMouseMoved|NSTrackingMouseEnteredAndExited|NSTrackingActiveInKeyWindow) owner:self userInfo:nil];
    [self addTrackingArea:ta];
}
@end

// ---- Status bar ----
@interface IVPStatus : NSView
@property (nonatomic, copy) NSString *left;
@property (nonatomic, copy) NSString *right;
@end
@implementation IVPStatus
- (instancetype)initWithFrame:(NSRect)r { if ((self=[super initWithFrame:r])) { _left=@""; _right=@""; } return self; }
- (void)drawRect:(NSRect)d { (void)d;
    [[NSColor colorWithSRGBRed:0.063 green:0.067 blue:0.086 alpha:1.0] setFill]; NSRectFill(self.bounds);
    [[NSColor colorWithWhite:1 alpha:0.07] setFill]; NSRectFill(NSMakeRect(0, 0, NSWidth(self.bounds), 1));
    NSDictionary *a = @{NSFontAttributeName: [NSFont systemFontOfSize:11],
                        NSForegroundColorAttributeName: [NSColor colorWithWhite:0.72 alpha:1]};
    [_left drawInRect:NSMakeRect(12, 0, NSWidth(self.bounds) - 240, NSHeight(self.bounds)) withAttributes:a];
    NSMutableParagraphStyle *p = [NSMutableParagraphStyle new]; p.alignment = NSTextAlignmentRight;
    NSDictionary *a2 = @{NSFontAttributeName: [NSFont systemFontOfSize:11], NSParagraphStyleAttributeName: p,
                         NSForegroundColorAttributeName: [NSColor colorWithWhite:0.72 alpha:1]};
    [_right drawInRect:NSMakeRect(12, 0, NSWidth(self.bounds) - 24, NSHeight(self.bounds)) withAttributes:a2];
}
@end

// ---- Main window ----
@interface IVPWindow () <IVPDocumentDelegate, NSWindowDelegate>
@property (nonatomic, strong) IVPImageView *image;
@property (nonatomic, strong) IVPSidebar *sidebar;
@property (nonatomic, strong) NSScrollView *sidebarScroll;
@property (nonatomic, strong) IVPStatus *status;
@property (nonatomic, strong) NSView *toolbar;
@property (nonatomic, strong) NSButton *selectBtn;
@end
@implementation IVPWindow

- (void)setup {
    self.contentView.wantsLayer = YES;
    self.contentView.layer.backgroundColor = [[NSColor windowBackgroundColor] CGColor];
    [self setOpaque:NO];
    self.backgroundColor = [NSColor colorWithSRGBRed:0.05 green:0.054 blue:0.071 alpha:1];

    _document = [IVPDocument new];
    _document.delegate = self;

    _image = [[IVPImageView alloc] initWithFrame:self.contentView.bounds];
    _image.document = _document;
    _image.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;

    _sidebar = [[IVPSidebar alloc] initWithFrame:NSMakeRect(0, 0, 220, 1000)];
    _sidebar.document = _document;
    _sidebar.hostWindow = self;
    _sidebarScroll = [[NSScrollView alloc] initWithFrame:NSMakeRect(0, 0, 220, 400)];
    _sidebarScroll.documentView = _sidebar;
    _sidebarScroll.drawsBackground = NO;
    _sidebarScroll.autohidesScrollers = YES;

    _status = [[IVPStatus alloc] initWithFrame:NSMakeRect(0, 0, 100, 24)];

    _toolbar = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 100, 52)];

    NSButton *openB   = IVPMakeBtn(@selector(doOpen:),    self, @"folder",            @"Open");
    NSButton *prevB   = IVPMakeBtn(@selector(doPrev:),    self, @"chevron.left",      @"Previous");
    NSButton *nextB   = IVPMakeBtn(@selector(doNext:),    self, @"chevron.right",     @"Next");
    NSButton *rotL    = IVPMakeBtn(@selector(doRotL:),    self, @"rotate.left",       @"Rotate Left");
    NSButton *rotR    = IVPMakeBtn(@selector(doRotR:),    self, @"rotate.right",      @"Rotate Right");
    NSButton *undoB   = IVPMakeBtn(@selector(doUndo:),    self, @"arrow.uturn.backward", @"Undo");
    NSButton *redoB   = IVPMakeBtn(@selector(doRedo:),    self, @"arrow.uturn.forward",  @"Redo");
    NSButton *fitB    = IVPMakeBtn(@selector(doFit:),     self, @"arrow.up.left.and.arrow.down.right", @"Fit");
    _selectBtn        = IVPMakeBtn(@selector(doSelect:),  self, @"checkmark.circle",  @"Select / Deselect");
    NSButton *pdfB    = IVPMakeBtn(@selector(doPDF:),     self, @"doc.richtext",      @"Export selected as PDF");
    NSButton *delB    = IVPMakeBtn(@selector(doDelete:),  self, @"trash",             @"Delete (Trash)");
    NSArray *btns = @[openB, prevB, nextB, rotL, rotR, undoB, redoB, fitB, _selectBtn, pdfB, delB];
    for (NSButton *b in btns) [b setButtonType:NSButtonTypeMomentaryChange];
    [_toolbar addSubview:openB];
    [self layoutToolbar:btns];

    [self.contentView addSubview:_toolbar];
    [self.contentView addSubview:_sidebarScroll];
    [self.contentView addSubview:_image];
    [self.contentView addSubview:_status];
    [self setDelegate:self];
    [self layout];

    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(zoomChanged) name:@"IVPZoomChanged" object:_image];
    [self updateStatus];
}

- (void)layoutToolbar:(NSArray *)btns {
    CGFloat x = 12, y = 8, gap = 6;
    for (NSButton *b in btns) {
        NSSize s = [b fittingSize]; if (s.width < 30) s.width = 30;
        b.frame = NSMakeRect(x, y, MAX(s.width, 36), 36);
        x += MAX(s.width, 36) + gap;
    }
}

- (void)layout {
    NSRect b = self.contentView.bounds;
    CGFloat tb = 52, st = 24, side = (_sidebarScroll.hidden ? 0 : 220);
    _toolbar.frame = NSMakeRect(0, NSHeight(b) - tb, NSWidth(b), tb);
    _status.frame  = NSMakeRect(0, 0, NSWidth(b), st);
    _sidebarScroll.frame = NSMakeRect(0, st, side, NSHeight(b) - tb - st);
    _image.frame = NSMakeRect(side, st, NSWidth(b) - side, NSHeight(b) - tb - st);
    CGFloat h = MAX(0, (CGFloat)_document.selectionCount * _sidebar.rowH);
    NSRect sf = _sidebar.bounds; sf.size = NSMakeSize(220, h); _sidebar.frame = sf;
}
- (void)windowDidResize:(NSNotification *)n { (void)n; [self layout]; }

#pragma mark Actions
- (void)doOpen:(id)s { (void)s; [self openDocument]; }
- (void)doPrev:(id)s { (void)s; [_document goPrev]; }
- (void)doNext:(id)s { (void)s; [_document goNext]; }
- (void)doRotL:(id)s { (void)s; [_document rotateLeft]; }
- (void)doRotR:(id)s { (void)s; [_document rotateRight]; }
- (void)doUndo:(id)s { (void)s; [_document undo]; }
- (void)doRedo:(id)s { (void)s; [_document redo]; }
- (void)doFit:(id)s  { (void)s; [_image fitToWindow]; [self updateStatus]; }
- (void)doSelect:(id)s { (void)s; [_document toggleSelectCurrent]; }
- (void)doPDF:(id)s  { (void)s; [_document exportSelectionToPDF]; }
- (void)doDelete:(id)s { (void)s; [_document deleteSelection]; }

- (void)openDocument {
    NSOpenPanel *p = [NSOpenPanel openPanel];
    p.allowedFileTypes = @[@"jpg",@"jpeg",@"png",@"gif",@"bmp",@"tif",@"tiff",@"webp",@"heic",@"heif"];
    p.allowsMultipleSelection = NO;
    if ([p runModal] == NSModalResponseOK) { [_document openURL:p.URL]; }
}

#pragma mark Document delegate
- (void)documentDidChange:(IVPDocument *)doc {
    (void)doc;
    [_image documentChanged];
    [self updateStatus];
}
- (void)documentSelectionDidChange:(IVPDocument *)doc {
    (void)doc;
    [_sidebar reload];
    [self layout];
    [self updateSelectButton];
    [self updateStatus];
}
- (void)zoomChanged { [self updateStatus]; }

- (void)keyDown:(NSEvent *)e {
    NSString *s = e.charactersIgnoringModifiers;
    if (s.length == 1) {
        unichar c = [s characterAtIndex:0];
        if (c == NSLeftArrowFunctionKey)  { [_document goPrev]; return; }
        if (c == NSRightArrowFunctionKey) { [_document goNext]; return; }
    }
    [super keyDown:e];
}

- (void)updateSelectButton {
    if (_document.isCurrentSelected) _selectBtn.contentTintColor = [NSColor systemBlueColor];
    else _selectBtn.contentTintColor = [NSColor labelColor];
}

- (void)updateStatus {
    NSString *left = @"Ready — File > Open, or drag an image into the window";
    if (_document.hasImage) {
        if (_document.isCurrentSelected) left = @"Selected";
        else left = @"Open";
    }
    NSString *right = @"";
    if (_document.hasImage) {
        right = [NSString stringWithFormat:@"%d × %d    %.0f%%",
                 _document.imageW, _document.imageH, _image.zoomPercent];
        if (_document.selectionCount > 0)
            right = [NSString stringWithFormat:@"● %lu selected    %@", (unsigned long)_document.selectionCount, right];
    }
    _status.left = left; _status.right = right;
    [_status setNeedsDisplay:YES];
}

- (BOOL)performDragOperation:(id<NSDraggingInfo>)sender {
    NSPasteboard *pb = [sender draggingPasteboard];
    NSString *path = [pb.stringForType:NSFileURLType] ?: [pb.stringForType:@"public.file-url"];
    if (path) { [_document openURL:[NSURL URLWithString:path]]; return YES; }
    NSURL *u = [NSURL URLFromPasteboard:pb];
    if (u) { [_document openURL:u]; return YES; }
    return NO;
}
- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)sender { (void)sender; return NSDragOperationCopy; }

@end
