// IVPDocument.h — data model / controller for the macOS port.
// Loads images via ImageIO, keeps a working BGRA buffer (ivp::ImageBuf) so the
// portable core (rotate/scan/warp/adjust/PDF) can operate on it, and exposes
// CGImage for display. Bridges to AppKit.
#import <AppKit/AppKit.h>
#include <CoreGraphics/CoreGraphics.h>
#include "ImageCore.h"   // expected on the include path (../core)
#include <vector>
#include <string>

@protocol IVPDocumentDelegate;
@interface IVPDocument : NSObject

@property (nonatomic, weak) id<IVPDocumentDelegate> delegate;
@property (nonatomic, strong) NSURL *currentURL;
@property (nonatomic, assign) CGImageRef displayImage; // current rendered image (read-only for views)
@property (nonatomic, readonly) BOOL hasImage;
@property (nonatomic, readonly) int imageW;
@property (nonatomic, readonly) int imageH;
@property (nonatomic, readonly) NSInteger selectionCount;
@property (nonatomic, readonly) NSUInteger folderCount;
@property (nonatomic, readonly) NSUInteger undoCount;
@property (nonatomic, readonly) NSUInteger redoCount;

- (void)openURL:(NSURL *)url;
- (void)goPrev;
- (void)goNext;
- (void)rotateRight;
- (void)rotateLeft;
- (void)undo;
- (void)redo;
- (void)toggleSelectCurrent;
- (void)selectAll;
- (void)clearSelection;
- (BOOL)isCurrentSelected;
- (void)deleteSelection;               // to Trash
- (BOOL)exportSelectionToPDF;          // prompts for save path
- (NSArray<NSString *> *)eventLog;     // timestamped action history
- (void)registerAsViewer;              // register with Launch Services + open Default Apps

// Adjust photo (brightness/contrast/… sliders)
@property (nonatomic, readonly) BOOL editing;
- (void)enterEdit;
- (void)applyEdit;
- (void)cancelEdit;
- (void)setAdjustValue:(int)index value:(int)v;
- (int)adjustValue:(int)index;
- (void)resetAdjust;

// Scan (document enhance)
@property (nonatomic, readonly) BOOL scanning;
@property (nonatomic, readonly) int scanLevel;
- (void)enterScan;
- (void)setScanLevel:(int)lvl;
- (void)commitScan;
- (void)cancelScan;

// Crop
@property (nonatomic, readonly) BOOL cropping;
@property (nonatomic, readonly) BOOL perspCrop;
- (void)startRectCrop;
- (void)startPerspCrop;
- (void)applyCrop;
- (void)cancelCrop;
// perspective corner access (image coords), settable by the view
- (double)cornerX:(int)i;
- (double)cornerY:(int)i;
- (void)setCornerX:(int)i x:(double)x y:(double)y;
- (double)rcX0; - (double)rcY0; - (double)rcX1; - (double)rcY1;
- (void)setRectCropX0:(double)x0 y0:(double)y0 x1:(double)x1 y1:(double)y1;
- (NSArray<NSURL *> *)selectedURLsSorted;
- (NSImage *)thumbnailForURL:(NSURL *)url maxSize:(CGFloat)s; // cached, for sidebar
@end

@protocol IVPDocumentDelegate <NSObject>
- (void)documentDidChange:(IVPDocument *)doc;          // image / history / status changed
- (void)documentSelectionDidChange:(IVPDocument *)doc; // sidebar / select button changed
@end
