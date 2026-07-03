// IVPRibbon.mm — tab strip + grouped large buttons.
#import "IVPRibbon.h"

@interface IVPRibbon ()
@property (nonatomic, strong) NSMutableArray<NSDictionary *> *tabs;
@property (nonatomic, strong) NSMutableDictionary<NSString *, NSButton *> *buttonForAction;
@property (nonatomic) NSInteger activeTab;
@property (nonatomic) CGFloat tabStripH;
@end

@implementation IVPRibbon

+ (NSButton *)mkButton:(SEL)act symbol:(NSString *)sym title:(NSString *)t target:(id)target {
    NSButton *b = [NSButton buttonWithTitle:t image:[NSImage imageWithSystemSymbolName:sym accessibilityDescription:t] target:target action:act];
    b.bezelStyle = NSBezelStyleAccessoryBar;
    b.imagePosition = NSImageAbove;
    b.font = [NSFont systemFontOfSize:10];
    b.bordered = NO;
    b.imageScaling = NSImageScaleProportionallyDown;
    return b;
}

- (instancetype)initWithFrame:(NSRect)r {
    if ((self = [super initWithFrame:r])) {
        _tabStripH = 28;
        _activeTab = 0;
        _buttonForAction = [NSMutableDictionary dictionary];
        [self buildModel];
    }
    return self;
}
- (BOOL)isFlipped { return YES; }
- (CGFloat)ribbonHeight { return _tabStripH + 74; }

- (void)buildModel {
    NSDictionary* (^mk)(NSString *, NSString *, NSString *) = ^NSDictionary*(NSString *selName, NSString *symbol, NSString *title) {
        return @{@"action": NSStringFromSelector(NSSelectorFromString(selName)), @"symbol": symbol, @"title": title};
    };
    NSArray *home = @[
        @{@"name": @"File",      @"items": @[ mk(@"doOpen:", @"folder", @"Open") ]},
        @{@"name": @"Browse",    @"items": @[ mk(@"doPrev:", @"chevron.left", @"Previous"), mk(@"doNext:", @"chevron.right", @"Next") ]},
        @{@"name": @"History",   @"items": @[ mk(@"doUndo:", @"arrow.uturn.backward", @"Undo"), mk(@"doRedo:", @"arrow.uturn.forward", @"Redo") ]},
        @{@"name": @"Select",    @"items": @[ mk(@"doSelect:", @"checkmark.circle", @"Select"), mk(@"doSelectAll:", @"checkmark.rectangle", @"Select All"), mk(@"doClearSel:", @"xmark.circle", @"Clear") ]},
        @{@"name": @"Manage",    @"items": @[ mk(@"doPDF:", @"doc.richtext", @"PDF"), mk(@"doDelete:", @"trash", @"Delete") ]},
    ];
    NSArray *image = @[
        @{@"name": @"Rotate",  @"items": @[ mk(@"doRotL:", @"rotate.left", @"Rotate Left"), mk(@"doRotR:", @"rotate.right", @"Rotate Right") ]},
        @{@"name": @"Crop",    @"items": @[ mk(@"doCropRect:", @"crop", @"Rectangle"), mk(@"doCropPersp:", @"viewfinder", @"Perspective") ]},
        @{@"name": @"Enhance", @"items": @[ mk(@"doEdit:", @"slider.horizontal.3", @"Adjust"), mk(@"doScan:", @"doc.viewfinder", @"Scan") ]},
    ];
    NSArray *view = @[
        @{@"name": @"Zoom",   @"items": @[ mk(@"doZoomIn:", @"plus.magnifyingglass", @"Zoom In"), mk(@"doZoomOut:", @"minus.magnifyingglass", @"Zoom Out"), mk(@"doFit:", @"arrow.up.left.and.arrow.down.right", @"Fit") ]},
        @{@"name": @"Browse", @"items": @[ mk(@"doPrev:", @"chevron.left", @"Previous"), mk(@"doNext:", @"chevron.right", @"Next") ]},
    ];
    _tabs = [@[@{@"name": @"Home", @"groups": home}, @{@"name": @"Image", @"groups": image}, @{@"name": @"View", @"groups": view}] mutableCopy];
}

- (NSButton *)buttonForAction:(NSString *)actStr {
    NSButton *b = _buttonForAction[actStr];
    if (b) return b;
    // find action details across tabs
    for (NSDictionary *tab in _tabs) for (NSDictionary *grp in tab[@"groups"]) for (NSDictionary *cmd in grp[@"items"]) {
        if ([cmd[@"action"] isEqualToString:actStr]) {
            SEL a = NSSelectorFromString(actStr);
            b = [IVPRibbon mkButton:a symbol:cmd[@"symbol"] title:cmd[@"title"] target:_target];
            _buttonForAction[actStr] = b;
            [self addSubview:b];
            return b;
        }
    }
    return nil;
}

- (void)drawRect:(NSRect)d {
    (void)d;
    [[NSColor colorWithSRGBRed:0.098 green:0.102 blue:0.125 alpha:1.0] setFill]; NSRectFill(self.bounds); // tab strip bg
    // ribbon content area background
    NSRect contentArea = NSMakeRect(0, _tabStripH, NSWidth(self.bounds), NSHeight(self.bounds) - _tabStripH);
    [[NSColor colorWithSRGBRed:0.137 green:0.141 blue:0.180 alpha:1.0] setFill]; NSRectFill(contentArea);

    // tabs
    NSDictionary *tabFont = @{NSFontAttributeName: [NSFont boldSystemFontOfSize:12]};
    CGFloat x = 0;
    for (NSInteger i = 0; i < (NSInteger)_tabs.count; i++) {
        NSString *name = _tabs[i][@"name"];
        NSSize s = [name sizeWithAttributes:tabFont];
        CGFloat w = MAX(70, s.width + 28);
        NSRect tr = NSMakeRect(x, 0, w, _tabStripH);
        if (i == _activeTab) {
            [[NSColor colorWithSRGBRed:0.137 green:0.141 blue:0.180 alpha:1.0] setFill]; NSRectFill(tr);
            [[NSColor systemBlueColor] setFill];
            NSRectFill(NSMakeRect(tr.origin.x, tr.origin.y + tr.size.height - 3, tr.size.width, 3));
        }
        NSMutableParagraphStyle *p = [NSMutableParagraphStyle new]; p.alignment = NSTextAlignmentCenter;
        [name drawInRect:tr withAttributes:@{NSFontAttributeName: [NSFont boldSystemFontOfSize:12],
            NSForegroundColorAttributeName: (i == _activeTab) ? [NSColor whiteColor] : [NSColor colorWithWhite:0.62 alpha:1],
            NSParagraphStyleAttributeName: p}];
        x += w;
    }
    [[NSColor colorWithWhite:1 alpha:0.08] setFill]; NSRectFill(NSMakeRect(0, _tabStripH, NSWidth(self.bounds), 1));
}

- (void)mouseDown:(NSEvent *)e {
    NSPoint p = [self convertPoint:e.locationInWindow fromView:nil];
    if (p.y >= _tabStripH) { [super mouseDown:e]; return; }
    NSDictionary *tabFont = @{NSFontAttributeName: [NSFont boldSystemFontOfSize:12]};
    CGFloat x = 0;
    for (NSInteger i = 0; i < (NSInteger)_tabs.count; i++) {
        NSString *name = _tabs[i][@"name"];
        NSSize s = [name sizeWithAttributes:tabFont];
        CGFloat w = MAX(70, s.width + 28);
        if (p.x >= x && p.x < x + w) { if (_activeTab != i) { _activeTab = i; [self setNeedsDisplay:YES]; [self layout]; } return; }
        x += w;
    }
}

- (void)layout {
    CGFloat btnW = 60, btnH = 56, gap = 4, groupGap = 14;
    CGFloat y0 = _tabStripH + 4;
    CGFloat x = groupGap;
    NSDictionary *grpFont = @{NSFontAttributeName: [NSFont systemFontOfSize:10]};
    // hide all buttons first
    for (NSButton *b in _buttonForAction.allValues) b.hidden = YES;
    NSArray *groups = _tabs[_activeTab][@"groups"];
    for (NSInteger gi = 0; gi < (NSInteger)groups.count; gi++) {
        NSDictionary *grp = groups[gi];
        NSArray *items = grp[@"items"];
        CGFloat groupLeft = x;
        for (NSDictionary *cmd in items) {
            NSButton *b = [self buttonForAction:cmd[@"action"]];
            b.hidden = NO;
            b.frame = NSMakeRect(x, y0, btnW, btnH);
            x += btnW + gap;
        }
        CGFloat groupRight = x - gap;
        NSString *gname = grp[@"name"];
        NSSize gs = [gname sizeWithAttributes:grpFont];
        NSTextField *gl = [self labelForGroup:gi];
        gl.stringValue = gname;
        [gl sizeToFit];
        gl.frame = NSMakeRect((groupLeft + groupRight)/2 - gs.width/2, y0 + btnH + 2, gs.width, 12);
        x = groupRight + groupGap;
    }
}
- (NSTextField *)labelForGroup:(NSInteger)i {
    static NSMutableDictionary *labels;
    static dispatch_once_t once;
    dispatch_once(&once, ^{ labels = [NSMutableDictionary dictionary]; });
    NSNumber *k = @(i);
    NSTextField *l = labels[k];
    if (!l) {
        l = [NSTextField labelWithString:@""];
        l.alignment = NSTextAlignmentCenter;
        l.textColor = [NSColor colorWithWhite:0.6 alpha:1];
        l.font = [NSFont systemFontOfSize:10];
        [self addSubview:l];
        labels[k] = l;
    }
    return l;
}

- (void)viewWillDraw { [self layout]; [super viewWillDraw]; }
@end
