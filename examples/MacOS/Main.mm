#include "MosaicView.hpp"

#import <Cocoa/Cocoa.h>

@interface MosaicAppDelegate : NSObject <NSApplicationDelegate>
@property(nonatomic, strong) NSWindow * window;
@end

@implementation MosaicAppDelegate
//////////////////////////////////////////////////////////////////////////
- (void)applicationDidFinishLaunching:(NSNotification *)notification
{
    (void)notification;
    NSRect frame = NSMakeRect(0, 0, 1440, 860);
    self.window = [[NSWindow alloc] initWithContentRect:frame styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable backing:NSBackingStoreBuffered defer:NO];
    self.window.title = @"Mosaic Fake Editor — Cocoa + Metal Example";
    self.window.minSize = NSMakeSize(1000, 650);
    self.window.contentView = [[MosaicView alloc] initWithFrame:frame];
    self.window.acceptsMouseMovedEvents = YES;
    [self.window center];
    [self.window makeKeyAndOrderFront:nil];
    [self.window makeFirstResponder:self.window.contentView];
    [NSApp activateIgnoringOtherApps:YES];
}
//////////////////////////////////////////////////////////////////////////
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender
{
    (void)sender;

    return YES;
}

@end
//////////////////////////////////////////////////////////////////////////
int main(int argc, const char * argv[])
{
    (void)argc;
    (void)argv;
    @autoreleasepool
    {
        NSApplication * application = NSApplication.sharedApplication;
        application.activationPolicy = NSApplicationActivationPolicyRegular;

        NSMenu * menuBar = [[NSMenu alloc] init];
        NSMenuItem * applicationItem = [[NSMenuItem alloc] init];
        [menuBar addItem:applicationItem];
        NSMenu * applicationMenu = [[NSMenu alloc] init];
        [applicationMenu addItemWithTitle:@"Quit Mosaic Fake Editor" action:@selector(terminate:) keyEquivalent:@"q"];
        applicationItem.submenu = applicationMenu;
        application.mainMenu = menuBar;

        MosaicAppDelegate * delegate = [[MosaicAppDelegate alloc] init];
        application.delegate = delegate;
        [application run];
    }

    return 0;
}
