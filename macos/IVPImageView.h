// IVPImageView.h — custom view that draws the current image with zoom/pan.
#import <AppKit/AppKit.h>
@class IVPDocument;
@interface IVPImageView : NSView
@property (nonatomic, weak) IVPDocument *document;
- (void)fitToWindow;
- (double)zoomPercent;
@end
