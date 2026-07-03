// IVPRibbon.h — a Windows-style tabbed ribbon (Home/Image/View) for macOS.
#import <AppKit/AppKit.h>
@interface IVPRibbon : NSView
@property (nonatomic, weak) id target;
@property (nonatomic, readonly) CGFloat ribbonHeight;
@end
