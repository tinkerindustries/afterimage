#import "preset_list.h"
#import "renderer.h"

#import <Cocoa/Cocoa.h>
#import <MetalKit/MetalKit.h>

@interface MilkView : MTKView
@property (nonatomic, strong) MilkRenderer* renderer;
@end

@implementation MilkView

- (BOOL)acceptsFirstResponder { return YES; }

- (void)keyDown:(NSEvent*)event {
    NSString* key = event.charactersIgnoringModifiers.lowercaseString;
    if (key.length == 0) return;
    const unichar c = [key characterAtIndex:0];
    if (c >= '1' && c <= '5') [self.renderer setMotion:c - '1'];
    else if (c == 'n') [self.renderer nextPreset];
    else if (c == 'p') [self.renderer previousPreset];
    else if (c == 'w') [self.renderer nextWaveMode];
    else if (c == 'c') [self.renderer toggleColourCycle];
    else if (c == 'r') [self.renderer toggleWrap];
    else if (c == 'a') [self.renderer toggleAudioSource];
    else if (c == 'l') [self.renderer togglePresetLock];
    else if (c == 'h') [self.renderer toggleHardCuts];
    else if (c == ' ') [self.renderer clear];
    else if (c == 'f') [self.window toggleFullScreen:nil];
    else if (c == 27) {
        if (self.window.styleMask & NSWindowStyleMaskFullScreen) [self.window toggleFullScreen:nil];
    } else {
        [super keyDown:event];
        return;
    }
    self.window.title = self.renderer.status;
}

@end

@interface AppDelegate : NSObject <NSApplicationDelegate>
@property (nonatomic, strong) NSWindow* window;
@property (nonatomic, strong) MilkView* view;
@property (nonatomic, strong) PresetListController* presetList;
@end

@implementation AppDelegate

- (void)applicationDidFinishLaunching:(NSNotification*)notification {
    NSMenu* menuBar = [NSMenu new];
    NSMenuItem* appItem = [NSMenuItem new];
    [menuBar addItem:appItem];
    NSMenu* appMenu = [NSMenu new];
    [appMenu addItemWithTitle:@"Toggle Full Screen" action:@selector(toggleFullScreen:) keyEquivalent:@"f"].keyEquivalentModifierMask =
        NSEventModifierFlagCommand | NSEventModifierFlagControl;
    [appMenu addItem:[NSMenuItem separatorItem]];
    [appMenu addItemWithTitle:@"Quit Milk" action:@selector(terminate:) keyEquivalent:@"q"];
    appItem.submenu = appMenu;
    NSMenuItem* presetsItem = [NSMenuItem new];
    [menuBar addItem:presetsItem];
    NSMenu* presetsMenu = [[NSMenu alloc] initWithTitle:@"Presets"];
    [presetsMenu addItemWithTitle:@"Preset List\u2026" action:@selector(showPresetList:) keyEquivalent:@"l"];
    [presetsMenu addItemWithTitle:@"Next Preset" action:@selector(nextPreset:) keyEquivalent:@"]"];
    [presetsMenu addItemWithTitle:@"Previous Preset" action:@selector(previousPreset:) keyEquivalent:@"["];
    presetsItem.submenu = presetsMenu;
    NSApp.mainMenu = menuBar;

    const NSRect frame = NSMakeRect(0, 0, 1280, 720);
    self.window = [[NSWindow alloc] initWithContentRect:frame
                                              styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                                                        NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable
                                                backing:NSBackingStoreBuffered
                                                  defer:NO];
    self.window.collectionBehavior = NSWindowCollectionBehaviorFullScreenPrimary;

    self.view = [[MilkView alloc] initWithFrame:frame device:MTLCreateSystemDefaultDevice()];
    self.view.colorPixelFormat = MTLPixelFormatBGRA8Unorm;
    self.view.preferredFramesPerSecond = 120;
    NSString* error = nil;
    self.view.renderer = [[MilkRenderer alloc] initWithView:self.view error:&error];
    if (self.view.renderer == nil) {
        NSAlert* alert = [NSAlert new];
        alert.messageText = @"Milk could not start";
        alert.informativeText = error ?: @"unknown error";
        [alert runModal];
        [NSApp terminate:nil];
        return;
    }
    self.view.delegate = self.view.renderer;
    self.presetList = [[PresetListController alloc] initWithRenderer:self.view.renderer];
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(presetDidChange:)
                                                 name:MilkPresetDidChangeNotification
                                               object:self.view.renderer];

    self.window.contentView = self.view;
    self.window.title = self.view.renderer.status;
    [self.window center];
    [self.window makeKeyAndOrderFront:nil];
    [self.window makeFirstResponder:self.view];
    [NSApp activate];
}

- (void)showPresetList:(id)sender { [self.presetList showWindow]; }

- (void)nextPreset:(id)sender { [self.view.renderer nextPreset]; }

- (void)previousPreset:(id)sender { [self.view.renderer previousPreset]; }

// The window title names the preset, so it follows every change, whichever way it was made.
- (void)presetDidChange:(NSNotification*)note { self.window.title = self.view.renderer.status; }

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)app { return YES; }

@end

int main(int, const char**) {
    @autoreleasepool {
        NSApplication* app = [NSApplication sharedApplication];
        app.activationPolicy = NSApplicationActivationPolicyRegular;
        AppDelegate* delegate = [AppDelegate new];
        app.delegate = delegate;
        [app run];
    }
    return 0;
}
