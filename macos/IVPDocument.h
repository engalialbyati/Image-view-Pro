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
- (NSArray<NSURL *> *)selectedURLsSorted;
- (NSImage *)thumbnailForURL:(NSURL *)url maxSize:(CGFloat)s; // cached, for sidebar
@end

@protocol IVPDocumentDelegate <NSObject>
- (void)documentDidChange:(IVPDocument *)doc;          // image / history / status changed
- (void)documentSelectionDidChange:(IVPDocument *)doc; // sidebar / select button changed
@end
