// IVPImageView.mm — drawing + zoom/pan.
#import "IVPImageView.h"
#import "IVPDocument.h"

@interface IVPImageView ()
{
    double _zoom;        // 1.0 == fit-to-window
    double _panX, _panY; // in view points, relative to centered fit position
    BOOL   _dragging;
    NSPoint _dragStart;
    double _dragStartPanX, _dragStartPanY;
}
@end

@implementation IVPImageView

- (instancetype)initWithFrame:(NSRect)r {
    if ((self = [super initWithFrame:r])) {
        _zoom = 1.0; _panX = 0; _panY = 0;
    }
    return self;
}
- (BOOL)isFlipped { return NO; }
- (BOOL)acceptsFirstResponder { return YES; }
- (void)viewDidChangeEffectiveAppearance { [self setNeedsDisplay:YES]; }

- (void)drawRect:(NSRect)dirty {
    [NSColor colorWithSRGBRed:0.051 green:0.055 blue:0.071 alpha:1.0 setFill];
    NSRectFill(self.bounds);

    CGImageRef img = _document.displayImage;
    if (!img) return;
    int iw = _document.imageW, ih = _document.imageH;
    if (iw <= 0 || ih <= 0) return;
    CGFloat vw = NSWidth(self.bounds), vh = NSHeight(self.bounds);
    double fit = MIN(vw / (double)iw, vh / (double)ih); if (fit <= 0) fit = 1;
    double scale = fit * _zoom;
    double dw = iw * scale, dh = ih * scale;
    double baseX = (vw - dw) / 2.0 + _panX;
    double baseY = (vh - dh) / 2.0 + _panY;

    CGContextRef ctx = NSGraphicsContext.currentContext.CGContext;
    CGContextSaveGState(ctx);
    // Clip to this view so a zoomed image never leaks outside the canvas.
    CGContextClipToRect(ctx, NSRectToCGRect(self.bounds));
    CGRect dst = CGRectMake((CGFloat)baseX, (CGFloat)baseY, (CGFloat)dw, (CGFloat)dh);
    CGContextTranslateCTM(ctx, 0, CGRectGetMaxY(dst));
    CGContextScaleCTM(ctx, 1, -1); // our buffer is top-row-first; draw upright
    CGContextDrawImage(ctx, CGRectMake((CGFloat)baseX, 0, (CGFloat)dw, (CGFloat)dh), img);
    CGContextRestoreGState(ctx);
}

- (void)documentChanged { _zoom = 1.0; _panX = _panY = 0; [self setNeedsDisplay:YES]; }
- (void)fitToWindow { _zoom = 1.0; _panX = _panY = 0; [self setNeedsDisplay:YES]; }
- (double)zoomPercent { return _zoom * 100.0; }

- (double)maxPanX {
    int iw = _document.imageW, ih = _document.imageH; if (iw <= 0) return 0;
    CGFloat vw = NSWidth(self.bounds), vh = NSHeight(self.bounds);
    double fit = MIN(vw / (double)iw, vh / (double)ih);
    double dw = iw * fit * _zoom;
    return (dw > vw) ? (dw - vw) / 2.0 : 0.0;
}
- (double)maxPanY {
    int iw = _document.imageW, ih = _document.imageH; if (ih <= 0) return 0;
    CGFloat vw = NSWidth(self.bounds), vh = NSHeight(self.bounds);
    double fit = MIN(vw / (double)iw, vh / (double)ih);
    double dh = ih * fit * _zoom;
    return (dh > vh) ? (dh - vh) / 2.0 : 0.0;
}
- (void)clampPan {
    double mx = [self maxPanX], my = [self maxPanY];
    if (_panX >  mx) _panX =  mx; if (_panX < -mx) _panX = -mx;
    if (_panY >  my) _panY =  my; if (_panY < -my) _panY = -my;
}

- (void)mouseDown:(NSEvent *)e {
    if (!_document.hasImage || ([self maxPanX] == 0 && [self maxPanY] == 0)) return;
    _dragging = YES;
    _dragStart = [self convertPoint:e.locationInWindow fromView:nil];
    _dragStartPanX = _panX; _dragStartPanY = _panY;
    [[NSCursor closedHandCursor] push];
}
- (void)mouseDragged:(NSEvent *)e {
    if (!_dragging) return;
    NSPoint p = [self convertPoint:e.locationInWindow fromView:nil];
    _panX = _dragStartPanX + (p.x - _dragStart.x);
    _panY = _dragStartPanY + (p.y - _dragStart.y);
    [self clampPan];
    [self setNeedsDisplay:YES];
}
- (void)mouseUp:(NSEvent *)e { (void)e; if (_dragging) { _dragging = NO; [NSCursor pop]; } }

- (void)scrollWheel:(NSEvent *)e {
    if (!_document.hasImage) return;
    if (e.hasPreciseScrollingDeltas || e.modifierFlags & NSEventModifierFlagOption) {
        double d = e.scrollingDeltaY;
        if (d == 0) d = e.scrollingDeltaX;
        double f = d > 0 ? 1.1 : 1.0/1.1;
        [self zoomBy:f];
    } else {
        [super scrollWheel:e]; // let enclosing scrollview (if any) work
    }
}
- (void)magnifyWithEvent:(NSEvent *)e {
    if (!_document.hasImage) return;
    [self zoomBy:(1.0 + e.magnification)];
}
- (void)zoomBy:(double)f {
    double z = _zoom * f;
    if (z < 0.05) z = 0.05; if (z > 40) z = 40;
    _zoom = z;
    [self clampPan];
    [self setNeedsDisplay:YES];
    [[NSNotificationCenter defaultCenter] postNotificationName:@"IVPZoomChanged" object:self];
}

- (void)keyDown:(NSEvent *)e {
    NSString *s = e.charactersIgnoringModifiers;
    unichar c = [s characterAtIndex:0];
    if (c == '0' && (e.modifierFlags & NSEventModifierFlagCommand)) { [self fitToWindow]; return; }
    if (c == '+' || c == '=') { if (e.modifierFlags & NSEventModifierFlagCommand) { [self zoomBy:1.2]; return; } }
    if (c == '-' ) { if (e.modifierFlags & NSEventModifierFlagCommand) { [self zoomBy:1.0/1.2]; return; } }
    [super keyDown:e];
}
@end
