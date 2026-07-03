// IVPImageView.mm — drawing + zoom/pan + crop overlays (rect + perspective).
#import "IVPImageView.h"
#import "IVPDocument.h"

struct IVPGeom { double baseX, baseY, dw, dh, scale; int iw, ih; };

@interface IVPImageView ()
{
    double _zoom;
    double _panX, _panY;
    BOOL   _dragging;
    NSPoint _dragStart;
    double _dragStartPanX, _dragStartPanY;
    int    _cropDragCorner;   // -1 none, -2 drawing rect, 0..3 persp corner
}
@end

@implementation IVPImageView

- (instancetype)initWithFrame:(NSRect)r {
    if ((self = [super initWithFrame:r])) {
        _zoom = 1.0; _panX = 0; _panY = 0; _cropDragCorner = -99;
    }
    return self;
}
- (BOOL)isFlipped { return NO; }
- (BOOL)acceptsFirstResponder { return YES; }

- (IVPGeom)geom {
    IVPGeom g = {0,0,0,0,1,0,0};
    g.iw = _document.imageW; g.ih = _document.imageH;
    if (g.iw <= 0 || g.ih <= 0) return g;
    CGFloat vw = NSWidth(self.bounds), vh = NSHeight(self.bounds);
    double fit = MIN(vw / (double)g.iw, vh / (double)g.ih); if (fit <= 0) fit = 1;
    g.scale = fit * _zoom;
    g.dw = g.iw * g.scale; g.dh = g.ih * g.scale;
    g.baseX = (vw - g.dw) / 2.0 + _panX;
    g.baseY = (vh - g.dh) / 2.0 + _panY;
    return g;
}
- (NSPoint)imagePointForViewPoint:(NSPoint)p {
    IVPGeom g = [self geom];
    NSPoint ip = NSMakePoint(0, 0);
    ip.x = (p.x - g.baseX) / g.scale;
    ip.y = (g.baseY + g.dh - p.y) / g.scale;
    if (ip.x < 0) ip.x = 0; if (ip.y < 0) ip.y = 0;
    if (ip.x > g.iw) ip.x = g.iw; if (ip.y > g.ih) ip.y = g.ih;
    return ip;
}
- (NSRect)viewRectForImageRect:(double[4])r withGeom:(IVPGeom)g {
    // r = x0,y0,x1,y1 in image coords (y down from top)
    double vL = g.baseX + r[0] * g.scale, vR = g.baseX + r[1+1] * g.scale; // r[2]
    double vT = g.baseY + g.dh - r[1] * g.scale, vB = g.baseY + g.dh - r[3] * g.scale;
    return NSMakeRect(MIN(vL,vR), MIN(vT,vB), fabs(vR-vL), fabs(vT-vB));
}

- (void)drawRect:(NSRect)dirty {
    (void)dirty;
    [[NSColor colorWithSRGBRed:0.051 green:0.055 blue:0.071 alpha:1.0] setFill];
    NSRectFill(self.bounds);

    CGImageRef img = _document.displayImage;
    if (!img) return;
    IVPGeom g = [self geom];
    if (g.iw <= 0) return;

    CGContextRef ctx = [[NSGraphicsContext currentContext] CGContext];
    CGContextSaveGState(ctx);
    CGContextClipToRect(ctx, NSRectToCGRect(self.bounds));
    CGRect dst = CGRectMake((CGFloat)g.baseX, (CGFloat)g.baseY, (CGFloat)g.dw, (CGFloat)g.dh);
    CGContextTranslateCTM(ctx, 0, CGRectGetMaxY(dst));
    CGContextScaleCTM(ctx, 1, -1);
    CGContextDrawImage(ctx, CGRectMake((CGFloat)g.baseX, 0, (CGFloat)g.dw, (CGFloat)g.dh), img);
    CGContextRestoreGState(ctx);

    if (_document.cropping) [self drawCropOverlay:g ctx:ctx];
}

- (void)drawCropOverlay:(IVPGeom)g ctx:(CGContextRef)ctx {
    // dim whole canvas
    CGContextSaveGState(ctx);
    CGContextSetRGBFillColor(ctx, 0, 0, 0, 0.55);
    CGContextFillRect(ctx, NSRectToCGRect(self.bounds));

    if (_document.perspCrop) {
        // brighten the quad by re-drawing the image clipped to the polygon
        NSPoint p[4];
        for (int i = 0; i < 4; i++) {
            double ix = [_document cornerX:i], iy = [_document cornerY:i];
            p[i] = NSMakePoint(g.baseX + ix * g.scale, g.baseY + g.dh - iy * g.scale);
        }
        CGContextSaveGState(ctx);
        CGContextBeginPath(ctx);
        CGContextMoveToPoint(ctx, p[0].x, p[0].y);
        for (int i = 1; i < 4; i++) CGContextAddLineToPoint(ctx, p[i].x, p[i].y);
        CGContextClosePath(ctx); CGContextClip(ctx);
        CGContextTranslateCTM(ctx, 0, g.baseY + g.dh);
        CGContextScaleCTM(ctx, 1, -1);
        CGContextDrawImage(ctx, CGRectMake((CGFloat)g.baseX, 0, (CGFloat)g.dw, (CGFloat)g.dh), _document.displayImage);
        CGContextRestoreGState(ctx);
        // outline
        CGContextBeginPath(ctx);
        CGContextMoveToPoint(ctx, p[0].x, p[0].y);
        for (int i = 1; i < 4; i++) CGContextAddLineToPoint(ctx, p[i].x, p[i].y);
        CGContextClosePath(ctx);
        CGContextSetRGBStrokeColor(ctx, 0.29, 0.57, 1.0, 1.0);
        CGContextSetLineWidth(ctx, 2);
        CGContextStrokePath(ctx);
        CGFloat r = 7;
        for (int i = 0; i < 4; i++) {
            CGContextSetRGBFillColor(ctx, 1, 1, 1, 1);
            CGContextFillEllipseInRect(ctx, CGRectMake(p[i].x - r, p[i].y - r, 2*r, 2*r));
            CGContextSetRGBStrokeColor(ctx, 0.29, 0.57, 1.0, 1.0);
            CGContextSetLineWidth(ctx, 2);
            CGContextStrokeEllipseInRect(ctx, CGRectMake(p[i].x - r, p[i].y - r, 2*r, 2*r));
        }
    } else {
        double r[4] = {[_document rcX0], [_document rcY0], [_document rcX1], [_document rcY1]};
        if (r[0] != r[2] && r[1] != r[3]) {
            NSRect rr = [self viewRectForImageRect:r withGeom:g];
            CGContextSaveGState(ctx);
            CGContextClipToRect(ctx, NSRectToCGRect(rr));
            CGContextTranslateCTM(ctx, 0, g.baseY + g.dh);
            CGContextScaleCTM(ctx, 1, -1);
            CGContextDrawImage(ctx, CGRectMake((CGFloat)g.baseX, 0, (CGFloat)g.dw, (CGFloat)g.dh), _document.displayImage);
            CGContextRestoreGState(ctx);
            CGContextStrokeRectWithColor(ctx, NSRectToCGRect(rr), [NSColor systemBlueColor].CGColor);
            CGContextSetLineWidth(ctx, 2); CGContextStrokeRect(ctx, NSRectToCGRect(rr));
        }
    }
    CGContextRestoreGState(ctx);
}

- (void)documentChanged { _zoom = 1.0; _panX = _panY = 0; _cropDragCorner = -99; [self setNeedsDisplay:YES]; }
- (void)fitToWindow { _zoom = 1.0; _panX = _panY = 0; [self setNeedsDisplay:YES]; }
- (double)zoomPercent { return _zoom * 100.0; }

- (double)maxPanX { IVPGeom g=[self geom]; return (g.dw > NSWidth(self.bounds)) ? (g.dw - NSWidth(self.bounds))/2.0 : 0; }
- (double)maxPanY { IVPGeom g=[self geom]; return (g.dh > NSHeight(self.bounds)) ? (g.dh - NSHeight(self.bounds))/2.0 : 0; }
- (void)clampPan {
    double mx=[self maxPanX], my=[self maxPanY];
    if (_panX> mx)_panX= mx; if(_panX<-mx)_panX=-mx;
    if (_panY> my)_panY= my; if(_panY<-my)_panY=-my;
}

- (void)mouseDown:(NSEvent *)e {
    NSPoint p = [self convertPoint:e.locationInWindow fromView:nil];
    if (_document.cropping) {
        if (_document.perspCrop) {
            IVPGeom g = [self geom]; int best = -1; double bestD = 16;
            for (int i = 0; i < 4; i++) {
                double cx = g.baseX + [_document cornerX:i]*g.scale, cy = g.baseY + g.dh - [_document cornerY:i]*g.scale;
                double d = hypot(cx - p.x, cy - p.y);
                if (d < bestD) { bestD = d; best = i; }
            }
            _cropDragCorner = (best >= 0) ? best : -1;
        } else {
            NSPoint ip = [self imagePointForViewPoint:p];
            [_document setRectCropX0:ip.x y0:ip.y x1:ip.x y1:ip.y];
            _cropDragCorner = -2;
        }
        [self setNeedsDisplay:YES];
        return;
    }
    if (!_document.hasImage || ([self maxPanX] == 0 && [self maxPanY] == 0)) return;
    _dragging = YES; _dragStart = p; _dragStartPanX = _panX; _dragStartPanY = _panY;
    [[NSCursor closedHandCursor] push];
}
- (void)mouseDragged:(NSEvent *)e {
    NSPoint p = [self convertPoint:e.locationInWindow fromView:nil];
    if (_document.cropping) {
        NSPoint ip = [self imagePointForViewPoint:p];
        if (_cropDragCorner == -2) {
            [_document setRectCropX0:[_document rcX0] y0:[_document rcY0] x1:ip.x y1:ip.y];
        } else if (_cropDragCorner >= 0) {
            [_document setCornerX:_cropDragCorner x:ip.x y:ip.y];
        }
        [self setNeedsDisplay:YES];
        return;
    }
    if (!_dragging) return;
    _panX = _dragStartPanX + (p.x - _dragStart.x);
    _panY = _dragStartPanY + (p.y - _dragStart.y);
    [self clampPan]; [self setNeedsDisplay:YES];
}
- (void)mouseUp:(NSEvent *)e { (void)e; if (_dragging) { _dragging = NO; [NSCursor pop]; } _cropDragCorner = -1; }

- (void)scrollWheel:(NSEvent *)e {
    if (!_document.hasImage) return;
    if (e.hasPreciseScrollingDeltas || (e.modifierFlags & NSEventModifierFlagOption)) {
        double d = e.scrollingDeltaY; if (d == 0) d = e.scrollingDeltaX;
        [self zoomBy:d > 0 ? 1.1 : 1.0/1.1];
    } else { [super scrollWheel:e]; }
}
- (void)magnifyWithEvent:(NSEvent *)e { if (_document.hasImage) [self zoomBy:(1.0 + e.magnification)]; }
- (void)zoomBy:(double)f {
    double z=_zoom*f; if(z<0.05)z=0.05; if(z>40)z=40; _zoom=z; [self clampPan]; [self setNeedsDisplay:YES];
    [[NSNotificationCenter defaultCenter] postNotificationName:@"IVPZoomChanged" object:self];
}

- (void)keyDown:(NSEvent *)e {
    unichar c = [e.charactersIgnoringModifiers characterAtIndex:0];
    if (_document.cropping) {
        if (c == NSCarriageReturnCharacter) { [_document applyCrop]; return; }
        if (c == 27) { [_document cancelCrop]; return; } // Esc
    } else if (_document.editing) {
        if (c == NSCarriageReturnCharacter) { [_document applyEdit]; return; }
        if (c == 27) { [_document cancelEdit]; return; }
    } else if (_document.scanning) {
        if (c == NSUpArrowFunctionKey)   { [_document setScanLevel:_document.scanLevel + 5]; return; }
        if (c == NSDownArrowFunctionKey) { [_document setScanLevel:_document.scanLevel - 5]; return; }
        if (c == NSCarriageReturnCharacter) { [_document commitScan]; return; }
        if (c == 27) { [_document cancelScan]; return; }
    }
    if (c == '0' && (e.modifierFlags & NSEventModifierFlagCommand)) { [self fitToWindow]; return; }
    [super keyDown:e];
}
@end
