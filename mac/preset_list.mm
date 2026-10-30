#import "preset_list.h"

#import "renderer.h"

namespace {
NSString* const COLUMN_PLAYING = @"playing";
NSString* const COLUMN_NAME = @"name";
}

// Return and Enter play the selected row, which NSTableView does not do by itself.
@interface PresetTableView : NSTableView
@property (nonatomic, copy) void (^onReturn)(void);
@end

@implementation PresetTableView
- (void)keyDown:(NSEvent*)event {
    if (event.keyCode == 36 || event.keyCode == 76) {
        if (self.onReturn) self.onReturn();
        return;
    }
    [super keyDown:event];
}
@end

@implementation PresetListController {
    MilkRenderer* m_renderer;
    NSPanel* m_panel;
    NSSearchField* m_search;
    PresetTableView* m_table;
    NSTextField* m_count;
    NSArray<NSNumber*>* m_visible;   // indices into the renderer's preset list that match the filter
}

- (instancetype)initWithRenderer:(MilkRenderer*)renderer {
    if ((self = [super init])) {
        m_renderer = renderer;
        m_visible = @[];
        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(presetDidChange:)
                                                     name:MilkPresetDidChangeNotification
                                                   object:renderer];
    }
    return self;
}

- (void)dealloc { [[NSNotificationCenter defaultCenter] removeObserver:self]; }

- (void)buildWindow {
    m_panel = [[NSPanel alloc] initWithContentRect:NSMakeRect(0, 0, 560, 680)
                                         styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable |
                                                   NSWindowStyleMaskUtilityWindow
                                           backing:NSBackingStoreBuffered
                                             defer:NO];
    m_panel.title = @"Presets";
    m_panel.releasedWhenClosed = NO;
    m_panel.hidesOnDeactivate = NO;
    m_panel.minSize = NSMakeSize(320, 240);
    NSView* content = m_panel.contentView;

    m_search = [[NSSearchField alloc] initWithFrame:NSMakeRect(10, 646, 540, 24)];
    m_search.placeholderString = @"Filter presets";
    m_search.sendsSearchStringImmediately = YES;
    m_search.delegate = self;
    m_search.target = self;
    m_search.action = @selector(filterChanged:);
    m_search.autoresizingMask = NSViewWidthSizable | NSViewMinYMargin;
    [content addSubview:m_search];

    m_count = [NSTextField labelWithString:@""];
    m_count.frame = NSMakeRect(10, 8, 540, 17);
    m_count.font = [NSFont systemFontOfSize:11];
    m_count.textColor = NSColor.secondaryLabelColor;
    m_count.autoresizingMask = NSViewWidthSizable | NSViewMaxYMargin;
    [content addSubview:m_count];

    NSScrollView* scroll = [[NSScrollView alloc] initWithFrame:NSMakeRect(0, 30, 560, 608)];
    scroll.hasVerticalScroller = YES;
    scroll.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    m_table = [[PresetTableView alloc] initWithFrame:scroll.bounds];
    NSTableColumn* playing = [[NSTableColumn alloc] initWithIdentifier:COLUMN_PLAYING];
    playing.width = 22;
    playing.minWidth = 22;
    playing.maxWidth = 22;
    NSTableColumn* name = [[NSTableColumn alloc] initWithIdentifier:COLUMN_NAME];
    name.title = @"Preset";
    name.resizingMask = NSTableColumnAutoresizingMask;
    [m_table addTableColumn:playing];
    [m_table addTableColumn:name];
    m_table.headerView = nil;
    m_table.columnAutoresizingStyle = NSTableViewLastColumnOnlyAutoresizingStyle;
    m_table.usesAlternatingRowBackgroundColors = YES;
    m_table.allowsMultipleSelection = NO;
    m_table.dataSource = self;
    m_table.delegate = self;
    m_table.target = self;
    m_table.doubleAction = @selector(playSelected:);
    __weak PresetListController* weakSelf = self;
    m_table.onReturn = ^{ [weakSelf playSelected:nil]; };
    scroll.documentView = m_table;
    [content addSubview:scroll];
}

- (void)showWindow {
    if (m_panel == nil) {
        [self buildWindow];
        [m_panel center];
        [self applyFilter];
    }
    [m_panel makeKeyAndOrderFront:nil];
    [self scrollToPlaying];
    [m_panel makeFirstResponder:m_table];
}

// Rebuilds the visible rows from the filter text.
- (void)applyFilter {
    NSString* text = [m_search.stringValue stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceCharacterSet];
    NSArray<NSString*>* names = m_renderer.presetNames;
    NSMutableArray<NSNumber*>* visible = [NSMutableArray arrayWithCapacity:names.count];
    for (NSUInteger i = 0; i < names.count; ++i) {
        if (text.length == 0 || [names[i] rangeOfString:text options:NSCaseInsensitiveSearch].location != NSNotFound) {
            [visible addObject:@(i)];
        }
    }
    m_visible = visible;
    [m_table reloadData];
    m_count.stringValue = text.length == 0 ? [NSString stringWithFormat:@"%lu presets", (unsigned long)names.count]
                                           : [NSString stringWithFormat:@"%lu of %lu presets", (unsigned long)visible.count,
                                                                        (unsigned long)names.count];
}

- (NSInteger)rowForPresetIndex:(NSInteger)index {
    for (NSUInteger row = 0; row < m_visible.count; ++row) {
        if (m_visible[row].integerValue == index) return static_cast<NSInteger>(row);
    }
    return -1;
}

- (void)scrollToPlaying {
    const NSInteger row = [self rowForPresetIndex:m_renderer.currentPresetIndex];
    if (row < 0) return;
    [m_table selectRowIndexes:[NSIndexSet indexSetWithIndex:row] byExtendingSelection:NO];
    [m_table scrollRowToVisible:row];
}

- (void)filterChanged:(id)sender {
    [self applyFilter];
    if (m_visible.count > 0) [m_table selectRowIndexes:[NSIndexSet indexSetWithIndex:0] byExtendingSelection:NO];
}

- (void)playSelected:(id)sender {
    const NSInteger row = m_table.selectedRow >= 0 ? m_table.selectedRow : m_table.clickedRow;
    if (row < 0 || row >= static_cast<NSInteger>(m_visible.count)) return;
    [m_renderer selectPresetAtIndex:m_visible[row].integerValue];
}

- (void)presetDidChange:(NSNotification*)note {
    if (m_panel == nil) return;
    [m_table reloadData];
    if (m_panel.visible) [self scrollToPlaying];
}

// Down arrow and Return in the filter field move to the list and play the first match.
- (BOOL)control:(NSControl*)control textView:(NSTextView*)textView doCommandBySelector:(SEL)selector {
    if (selector == @selector(moveDown:)) {
        [m_panel makeFirstResponder:m_table];
        return YES;
    }
    if (selector == @selector(insertNewline:)) {
        if (m_visible.count > 0) {
            if (m_table.selectedRow < 0) [m_table selectRowIndexes:[NSIndexSet indexSetWithIndex:0] byExtendingSelection:NO];
            [self playSelected:nil];
            [m_panel makeFirstResponder:m_table];
        }
        return YES;
    }
    return NO;
}

- (NSInteger)numberOfRowsInTableView:(NSTableView*)tableView { return static_cast<NSInteger>(m_visible.count); }

- (id)tableView:(NSTableView*)tableView objectValueForTableColumn:(NSTableColumn*)column row:(NSInteger)row {
    const NSInteger index = m_visible[row].integerValue;
    if ([column.identifier isEqualToString:COLUMN_PLAYING]) return index == m_renderer.currentPresetIndex ? @"▶" : @"";
    return m_renderer.presetNames[index];
}

@end
