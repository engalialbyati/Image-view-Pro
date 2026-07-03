// main.mm — application entry, menu bar, and window creation.
#import <AppKit/AppKit.h>
#import "IVPWindow.h"
#import "IVPDocument.h"
#import "IVPImageView.h"

@interface IVPAppDelegate : NSObject <NSApplicationDelegate>
@property (nonatomic, strong) IVPWindow *window;
@end

@implementation IVPAppDelegate
- (void)applicationDidFinishLaunching:(NSNotification *)n {
    (void)n;
    NSRect f = NSMakeRect(0, 0, 1200, 760);
    NSUInteger style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                       NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable |
                       NSWindowStyleMaskFullScreen;
    self.window = [[IVPWindow alloc] initWithContentRect:f styleMask:style
                                          backing:NSBackingStoreBuffered defer:NO];
    [self.window setTitle:@"Image Viewer Pro"];
    [self.window center];
    [self.window registerForDraggedTypes:@[NSPasteboardTypeFileURL]];
    [self.window setup];
    [self.window makeKeyAndOrderFront:nil];
    // process a file passed on the command line, if any
    NSArray *args = [[NSProcessInfo processInfo] arguments];
    if (args.count > 1) {
        NSURL *u = [NSURL fileURLWithPath:args[1]];
        if (u) [self.window.document openURL:u];
    }
}

- (void)application:(NSApplication *)sender openFiles:(NSArray<NSString *> *)filenames {
    (void)sender;
    if (filenames.count) [self.window.document openURL:[NSURL fileURLWithPath:filenames[0]]];
}

- (void)buildMenu {
    NSMenu *main = [NSApp mainMenu];
    // App menu
    NSMenuItem *appItem = [main addItemWithTitle:@"" action:nil keyEquivalent:@""];
    NSMenu *appMenu = [NSMenu new];
    [appMenu addItemWithTitle:@"About Image Viewer Pro" action:@selector(orderFrontStandardAboutPanel:) keyEquivalent:@""];
    [appMenu addItem:NSMenuItem.separatorItem];
    [appMenu addItemWithTitle:@"Hide Image Viewer Pro" action:@selector(hide:) keyEquivalent:@"h"];
    [appMenu addItemWithTitle:@"Quit Image Viewer Pro" action:@selector(terminate:) keyEquivalent:@"q"];
    appItem.submenu = appMenu;

    NSMenuItem *fileItem = [main addItemWithTitle:@"File" action:nil keyEquivalent:@""];
    NSMenu *fileMenu = [NSMenu new];
    [fileMenu addItemWithTitle:@"Open…" action:@selector(doOpen:) keyEquivalent:@"o"].target = self.window;
    [fileMenu addItem:NSMenuItem.separatorItem];
    [fileMenu addItemWithTitle:@"Export Selected as PDF…" action:@selector(doPDF:) keyEquivalent:@""].target = self.window;
    [fileMenu addItemWithTitle:@"Move Selected to Trash" action:@selector(doDelete:) keyEquivalent:@""].target = self.window;
    [fileMenu addItem:NSMenuItem.separatorItem];
    [fileMenu addItemWithTitle:@"Close Window" action:@selector(performClose:) keyEquivalent:@"w"];
    fileItem.submenu = fileMenu;

    NSMenuItem *editItem = [main addItemWithTitle:@"Edit" action:nil keyEquivalent:@""];
    NSMenu *editMenu = [NSMenu new];
    [editMenu addItemWithTitle:@"Undo" action:@selector(doUndo:) keyEquivalent:@"z"].target = self.window;
    [editMenu addItemWithTitle:@"Redo" action:@selector(doRedo:) keyEquivalent:@"y"].target = self.window;
    [editMenu addItem:NSMenuItem.separatorItem];
    [editMenu addItemWithTitle:@"Select / Deselect" action:@selector(doSelect:) keyEquivalent:@"s"].target = self.window;
    [editMenu addItemWithTitle:@"Select All" action:@selector(doSelectAll:) keyEquivalent:@"a"].target = self.window;
    [editMenu addItemWithTitle:@"Clear Selection" action:@selector(doClearSel:) keyEquivalent:@"d"].target = self.window;
    editItem.submenu = editMenu;

    NSMenuItem *imgItem = [main addItemWithTitle:@"Image" action:nil keyEquivalent:@""];
    NSMenu *imgMenu = [NSMenu new];
    [imgMenu addItemWithTitle:@"Rotate Left" action:@selector(doRotL:) keyEquivalent:@"r"].target = self.window;
    [imgMenu addItemWithTitle:@"Rotate Right" action:@selector(doRotR:) keyEquivalent:@"R"].target = self.window;
    [imgMenu addItemWithTitle:@"Adjust Photo…" action:@selector(doEdit:) keyEquivalent:@"e"].target = self.window;
    [imgMenu addItemWithTitle:@"Scan Document" action:@selector(doScan:) keyEquivalent:@"z"].target = self.window;
    [imgMenu addItem:NSMenuItem.separatorItem];
    [imgMenu addItemWithTitle:@"Crop (Rectangle)" action:@selector(doCropRect:) keyEquivalent:@"c"].target = self.window;
    [imgMenu addItemWithTitle:@"Crop (Perspective / Auto)" action:@selector(doCropPersp:) keyEquivalent:@"C"].target = self.window;
    [imgMenu addItem:NSMenuItem.separatorItem];
    [imgMenu addItemWithTitle:@"Previous" action:@selector(doPrev:) keyEquivalent:@""].target = self.window;
    [imgMenu addItemWithTitle:@"Next" action:@selector(doNext:) keyEquivalent:@""].target = self.window;
    imgItem.submenu = imgMenu;

    NSMenuItem *viewItem = [main addItemWithTitle:@"View" action:nil keyEquivalent:@""];
    NSMenu *viewMenu = [NSMenu new];
    [viewMenu addItemWithTitle:@"Fit to Window" action:@selector(doFit:) keyEquivalent:@"0"].target = self.window;
    [viewMenu addItemWithTitle:@"Zoom In" action:@selector(doZoomIn:) keyEquivalent:@"+"].target = self.window;
    [viewMenu addItemWithTitle:@"Zoom Out" action:@selector(doZoomOut:) keyEquivalent:@"-"].target = self.window;
    viewItem.submenu = viewMenu;
}
- (void)selectAllDoc:(id)s { (void)s; [self.window.document selectAll]; }
- (void)clearSel:(id)s { (void)s; [self.window.document clearSelection]; }
- (void)zoomIn:(id)s { (void)s; [self.window.image zoomBy:1.2]; }
- (void)zoomOut:(id)s { (void)s; [self.window.image zoomBy:1.0/1.2]; }
@end

int main(int argc, const char *argv[]) {
    (void)argc; (void)argv;
    @autoreleasepool {
        NSApplication *app = [NSApplication sharedApplication];
        [app setActivationPolicy:NSApplicationActivationPolicyRegular];
        IVPAppDelegate *dg = [IVPAppDelegate new];
        app.delegate = dg;
        [app finishLaunching];
        [dg buildMenu];
        [app activateIgnoringOtherApps:YES];
        [app run];
    }
    return 0;
}
