#include "HelloDemoView.hpp"

#import <Cocoa/Cocoa.h>

@interface MosaicHelloDemoAppDelegate : NSObject <NSApplicationDelegate>
@property(nonatomic, strong) NSWindow * window;
@end

@interface MosaicHelloDemoApplication : NSApplication
@end

@implementation MosaicHelloDemoApplication
//////////////////////////////////////////////////////////////////////////
- (void)sendEvent:(NSEvent *)event
{
    if(event.type == NSEventTypeScrollWheel)
    {
        NSWindow * targetWindow = event.window;

        if(targetWindow == nil && event.windowNumber != 0)
        {
            targetWindow = [self windowWithWindowNumber:event.windowNumber];
        }

        if(targetWindow == nil)
        {
            targetWindow = self.keyWindow;
        }

        MosaicHelloDemoView * view = [targetWindow.contentView isKindOfClass:MosaicHelloDemoView.class] ? static_cast<MosaicHelloDemoView *>(targetWindow.contentView) : nil;

        if(view != nil && [view handleApplicationScrollWheelEvent:event])
        {
            return;
        }
    }

    [super sendEvent:event];
}

@end

@implementation MosaicHelloDemoAppDelegate
//////////////////////////////////////////////////////////////////////////
- (void)applicationDidFinishLaunching:(NSNotification *)notification
{
    (void)notification;
    // The large host viewport lets the demo use stable first-use positions and sizes for all
    // floating windows without constraining them to a smaller native window.
    NSRect frame = NSMakeRect(0, 0, 1200, 800);
    self.window = [[NSWindow alloc] initWithContentRect:frame styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable backing:NSBackingStoreBuffered defer:NO];
    self.window.title = NSProcessInfo.processInfo.processName;
    self.window.minSize = NSMakeSize(800, 600);
    self.window.contentView = [[MosaicHelloDemoView alloc] initWithFrame:frame];
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
        MosaicHelloDemoApplication * application = MosaicHelloDemoApplication.sharedApplication;
        application.activationPolicy = NSApplicationActivationPolicyRegular;

        NSMenu * menuBar = [[NSMenu alloc] init];
        NSMenuItem * applicationItem = [[NSMenuItem alloc] init];
        [menuBar addItem:applicationItem];
        NSMenu * applicationMenu = [[NSMenu alloc] init];
        NSString * quitTitle = [@"Quit " stringByAppendingString:NSProcessInfo.processInfo.processName];
        [applicationMenu addItemWithTitle:quitTitle action:@selector(terminate:) keyEquivalent:@"q"];
        applicationItem.submenu = applicationMenu;
        application.mainMenu = menuBar;

        MosaicHelloDemoAppDelegate * delegate = [[MosaicHelloDemoAppDelegate alloc] init];
        application.delegate = delegate;
        [application run];
    }

    return 0;
}
