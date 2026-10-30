#import "settings.h"

#import "renderer.h"
#import "system_audio.h"
#import "thumbnails.h"

@implementation SettingsController {
    MilkRenderer* m_renderer;
    NSWindow* m_window;
    NSPopUpButton* m_device;
    NSTextField* m_summary;
}

- (instancetype)initWithRenderer:(MilkRenderer*)renderer {
    if ((self = [super init])) m_renderer = renderer;
    return self;
}

- (void)buildWindow {
    NSTextField* audioHeading = [NSTextField labelWithString:@"Audio device"];
    audioHeading.font = [NSFont boldSystemFontOfSize:NSFont.systemFontSize];
    NSTextField* audioNote = [NSTextField labelWithString:@"The output device whose sound drives the visuals."];
    audioNote.textColor = NSColor.secondaryLabelColor;
    m_device = [[NSPopUpButton alloc] initWithFrame:NSZeroRect pullsDown:NO];
    m_device.target = self;
    m_device.action = @selector(deviceChanged:);
    NSTextField* heading = [NSTextField labelWithString:@"Thumbnail cache"];
    heading.font = [NSFont boldSystemFontOfSize:NSFont.systemFontSize];
    m_summary = [NSTextField labelWithString:@""];
    m_summary.textColor = NSColor.secondaryLabelColor;
    NSButton* clear = [NSButton buttonWithTitle:@"Clear Thumbnail Cache" target:self action:@selector(clearCache:)];
    NSButton* open = [NSButton buttonWithTitle:@"Open Thumbnail Folder in Finder" target:self action:@selector(openFolder:)];
    NSStackView* buttons = [NSStackView stackViewWithViews:@[ clear, open ]];
    buttons.spacing = 8;
    NSStackView* stack = [NSStackView stackViewWithViews:@[ audioHeading, audioNote, m_device, heading, m_summary, buttons ]];
    stack.orientation = NSUserInterfaceLayoutOrientationVertical;
    stack.alignment = NSLayoutAttributeLeading;
    stack.spacing = 10;
    [stack setCustomSpacing:24 afterView:m_device];
    stack.edgeInsets = NSEdgeInsetsMake(20, 20, 20, 20);

    m_window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 200, 100)
                                           styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable
                                             backing:NSBackingStoreBuffered
                                               defer:NO];
    m_window.title = @"Settings";
    m_window.releasedWhenClosed = NO;
    m_window.contentView = stack;
    [m_window setContentSize:stack.fittingSize];
    [m_window center];
}

// Lists the devices now present, with the saved one selected. A saved device that is not there
// shows as "System Default", which is what the tap uses in that case.
- (void)reloadDevices {
    NSString* saved = [NSUserDefaults.standardUserDefaults stringForKey:MilkAudioDeviceKey];
    [m_device removeAllItems];
    [m_device addItemWithTitle:@"System Default"];
    m_device.lastItem.representedObject = @"";
    for (const Milk::OutputDevice& device : Milk::listOutputDevices()) {
        [m_device addItemWithTitle:@(device.name.c_str())];
        m_device.lastItem.representedObject = @(device.uid.c_str());
        if ([m_device.lastItem.representedObject isEqualToString:saved]) [m_device selectItem:m_device.lastItem];
    }
}

- (void)deviceChanged:(id)sender {
    [NSUserDefaults.standardUserDefaults setObject:m_device.selectedItem.representedObject forKey:MilkAudioDeviceKey];
    [m_renderer restartSystemAudio];
}

- (void)showWindow {
    if (m_window == nil) [self buildWindow];
    [self reloadDevices];
    [m_window setContentSize:m_window.contentView.fittingSize];
    m_summary.stringValue = [ThumbnailStore cacheSummary];
    [m_window makeKeyAndOrderFront:nil];
}

- (void)clearCache:(id)sender {
    [ThumbnailStore clearCache];
    m_summary.stringValue = [ThumbnailStore cacheSummary];
}

- (void)openFolder:(id)sender {
    [NSWorkspace.sharedWorkspace openURL:[NSURL fileURLWithPath:[ThumbnailStore cacheDirectory] isDirectory:YES]];
}

@end
