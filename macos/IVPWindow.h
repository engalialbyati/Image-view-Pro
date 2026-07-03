// IVPWindow.h
#import <AppKit/AppKit.h>
@class IVPDocument;
@interface IVPWindow : NSWindow
@property (nonatomic, strong) IVPDocument *document;
- (void)openDocument;
@end
