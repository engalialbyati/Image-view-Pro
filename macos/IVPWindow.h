// IVPWindow.h
#import <AppKit/AppKit.h>
@class IVPDocument;
@class IVPImageView;
@interface IVPWindow : NSWindow
@property (nonatomic, strong) IVPDocument *document;
@property (nonatomic, readonly) IVPImageView *image;
- (void)setup;
- (void)openDocument;
@end
