#import "preset_list.h"

#import "renderer.h"
#import "thumbnails.h"

namespace {
NSString* const COLUMN_NAME = @"name";
NSString* const CELL_ID = @"preset";
NSString* const TILE_ID = @"tile";
NSString* const MODE_KEY = @"PresetListMode";
enum ListMode : NSInteger { MODE_LIST = 0, MODE_GALLERY = 1 };
constexpr CGFloat TILE_WIDTH = 168;
constexpr CGFloat THUMB_SIZE = 152;
}

// One gallery tile: the preset's thumbnail, or a note while it renders, with its name under it.
// The playing preset's thumbnail has an accent border and a play mark.
@interface PresetTile : NSCollectionViewItem
@property (nonatomic, strong) NSImageView* thumbnail;
@property (nonatomic, strong) NSTextField* pending;
@property (nonatomic, strong) NSTextField* name;
@property (nonatomic, strong) NSImageView* playing;
- (void)showImage:(nullable NSImage*)image title:(NSString*)title tooltip:(NSString*)tooltip playing:(BOOL)playing;
@end

@implementation PresetTile
- (void)loadView {
    NSView* view = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, TILE_WIDTH, THUMB_SIZE + 44)];
    view.wantsLayer = YES;
    view.layer.cornerRadius = 8;
    _thumbnail = [NSImageView new];
    _thumbnail.imageScaling = NSImageScaleProportionallyUpOrDown;
    _thumbnail.wantsLayer = YES;
    _thumbnail.layer.cornerRadius = 6;
    _thumbnail.layer.masksToBounds = YES;
    _thumbnail.layer.backgroundColor = NSColor.blackColor.CGColor;
    _thumbnail.layer.borderColor = NSColor.controlAccentColor.CGColor;
    _pending = [NSTextField labelWithString:@"Rendering…"];
    _pending.font = [NSFont systemFontOfSize:11];
    _pending.textColor = NSColor.tertiaryLabelColor;
    _name = [NSTextField wrappingLabelWithString:@""];
    _name.font = [NSFont systemFontOfSize:11];
    _name.alignment = NSTextAlignmentCenter;
    _name.maximumNumberOfLines = 2;
    _name.lineBreakMode = NSLineBreakByTruncatingTail;
    _name.cell.truncatesLastVisibleLine = YES;
    NSImage* play = [NSImage imageWithSystemSymbolName:@"play.circle.fill" accessibilityDescription:@"Playing"];
    _playing = [NSImageView imageViewWithImage:play];
    _playing.contentTintColor = NSColor.controlAccentColor;
    _playing.symbolConfiguration = [NSImageSymbolConfiguration configurationWithPointSize:18 weight:NSFontWeightSemibold];
    for (NSView* v in @[ _thumbnail, _pending, _name, _playing ]) {
        v.translatesAutoresizingMaskIntoConstraints = NO;
        [view addSubview:v];
    }
    [NSLayoutConstraint activateConstraints:@[
        [_thumbnail.topAnchor constraintEqualToAnchor:view.topAnchor constant:6],
        [_thumbnail.centerXAnchor constraintEqualToAnchor:view.centerXAnchor],
        [_thumbnail.widthAnchor constraintEqualToConstant:THUMB_SIZE],
        [_thumbnail.heightAnchor constraintEqualToConstant:THUMB_SIZE],
        [_pending.centerXAnchor constraintEqualToAnchor:_thumbnail.centerXAnchor],
        [_pending.centerYAnchor constraintEqualToAnchor:_thumbnail.centerYAnchor],
        [_playing.trailingAnchor constraintEqualToAnchor:_thumbnail.trailingAnchor constant:-6],
        [_playing.bottomAnchor constraintEqualToAnchor:_thumbnail.bottomAnchor constant:-6],
        [_name.topAnchor constraintEqualToAnchor:_thumbnail.bottomAnchor constant:4],
        [_name.leadingAnchor constraintEqualToAnchor:view.leadingAnchor constant:6],
        [_name.trailingAnchor constraintEqualToAnchor:view.trailingAnchor constant:-6],
    ]];
    self.view = view;
}

- (void)setSelected:(BOOL)selected {
    [super setSelected:selected];
    self.view.layer.backgroundColor = selected ? [NSColor.selectedContentBackgroundColor colorWithAlphaComponent:0.35].CGColor : nil;
}

- (void)showImage:(nullable NSImage*)image title:(NSString*)title tooltip:(NSString*)tooltip playing:(BOOL)playing {
    self.thumbnail.image = image;
    self.pending.hidden = image != nil;
    self.name.stringValue = title;
    self.name.font = playing ? [NSFont systemFontOfSize:11 weight:NSFontWeightSemibold] : [NSFont systemFontOfSize:11];
    self.playing.hidden = !playing;
    self.thumbnail.layer.borderWidth = playing ? 3 : 0;
    self.view.toolTip = tooltip;
}
@end

// Double-clicking a tile, or pressing Return, plays it.
@interface PresetCollectionView : NSCollectionView
@property (nonatomic, copy) void (^onPlay)(void);
@end

@implementation PresetCollectionView
- (void)mouseDown:(NSEvent*)event {
    [super mouseDown:event];
    if (event.clickCount == 2 && self.onPlay) self.onPlay();
}
- (void)keyDown:(NSEvent*)event {
    if (event.keyCode == 36 || event.keyCode == 76) {
        if (self.onPlay) self.onPlay();
        return;
    }
    [super keyDown:event];
}
@end

// One row: a play mark when the preset is playing, its name, and its folder on the right.
@interface PresetCellView : NSTableCellView
@property (nonatomic, strong) NSImageView* playing;
@property (nonatomic, strong) NSTextField* name;
@property (nonatomic, strong) NSTextField* folder;
@end

@implementation PresetCellView
- (instancetype)initWithFrame:(NSRect)frame {
    if ((self = [super initWithFrame:frame])) {
        NSImage* play = [NSImage imageWithSystemSymbolName:@"play.fill" accessibilityDescription:@"Playing"];
        _playing = [NSImageView imageViewWithImage:play];
        _playing.contentTintColor = NSColor.controlAccentColor;
        _playing.symbolConfiguration = [NSImageSymbolConfiguration configurationWithPointSize:10 weight:NSFontWeightSemibold];
        _name = [NSTextField labelWithString:@""];
        _name.font = [NSFont systemFontOfSize:13];
        _name.lineBreakMode = NSLineBreakByTruncatingTail;
        _folder = [NSTextField labelWithString:@""];
        _folder.font = [NSFont systemFontOfSize:11];
        _folder.textColor = NSColor.secondaryLabelColor;
        _folder.lineBreakMode = NSLineBreakByTruncatingHead;
        for (NSView* v in @[ _playing, _name, _folder ]) {
            v.translatesAutoresizingMaskIntoConstraints = NO;
            [self addSubview:v];
        }
        [_name setContentCompressionResistancePriority:NSLayoutPriorityDefaultLow forOrientation:NSLayoutConstraintOrientationHorizontal];
        [_folder setContentHuggingPriority:NSLayoutPriorityRequired forOrientation:NSLayoutConstraintOrientationHorizontal];
        [_folder setContentCompressionResistancePriority:NSLayoutPriorityDefaultLow - 1 forOrientation:NSLayoutConstraintOrientationHorizontal];
        [NSLayoutConstraint activateConstraints:@[
            [_playing.leadingAnchor constraintEqualToAnchor:self.leadingAnchor constant:6],
            [_playing.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
            [_playing.widthAnchor constraintEqualToConstant:12],
            [_name.leadingAnchor constraintEqualToAnchor:_playing.trailingAnchor constant:6],
            [_name.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
            [_folder.leadingAnchor constraintGreaterThanOrEqualToAnchor:_name.trailingAnchor constant:8],
            [_folder.trailingAnchor constraintEqualToAnchor:self.trailingAnchor constant:-8],
            [_folder.firstBaselineAnchor constraintEqualToAnchor:_name.firstBaselineAnchor],
            [_folder.widthAnchor constraintLessThanOrEqualToAnchor:self.widthAnchor multiplier:0.4],
        ]];
        self.textField = _name;
    }
    return self;
}
@end

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
    NSScrollView* m_listScroll;
    PresetCollectionView* m_gallery;
    NSScrollView* m_galleryScroll;
    NSSegmentedControl* m_mode;
    ThumbnailStore* m_thumbnails;
    NSTextField* m_count;
    NSArray<NSNumber*>* m_visible;   // indices into the renderer's preset list that match the filter
    NSArray<NSString*>* m_titles;    // each preset's file name
    NSArray<NSString*>* m_folders;   // each preset's folder below the folder every preset shares
}

- (instancetype)initWithRenderer:(MilkRenderer*)renderer {
    if ((self = [super init])) {
        m_renderer = renderer;
        m_visible = @[];
        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(presetDidChange:)
                                                     name:MilkPresetDidChangeNotification
                                                   object:renderer];
        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(thumbnailDidRender:)
                                                     name:MilkThumbnailDidRenderNotification
                                                   object:nil];
    }
    return self;
}

- (void)dealloc { [[NSNotificationCenter defaultCenter] removeObserver:self]; }

// Splits each name into its file name and its folder, leaving out the folders every preset shares.
- (void)buildLabels {
    NSArray<NSString*>* names = m_renderer.presetNames;
    NSArray<NSString*>* shared = names.count > 0 ? [names[0].stringByDeletingLastPathComponent pathComponents] : @[];
    for (NSString* name in names) {
        NSArray<NSString*>* parts = name.stringByDeletingLastPathComponent.pathComponents;
        NSUInteger common = 0;
        while (common < shared.count && common < parts.count && [shared[common] isEqualToString:parts[common]]) ++common;
        shared = [shared subarrayWithRange:NSMakeRange(0, common)];
    }
    NSMutableArray<NSString*>* titles = [NSMutableArray arrayWithCapacity:names.count];
    NSMutableArray<NSString*>* folders = [NSMutableArray arrayWithCapacity:names.count];
    for (NSString* name in names) {
        NSArray<NSString*>* parts = name.stringByDeletingLastPathComponent.pathComponents;
        NSArray<NSString*>* rest = parts.count > shared.count ? [parts subarrayWithRange:NSMakeRange(shared.count, parts.count - shared.count)] : @[];
        [titles addObject:name.lastPathComponent];
        [folders addObject:[rest componentsJoinedByString:@"/"]];
    }
    m_titles = titles;
    m_folders = folders;
}

- (void)buildWindow {
    [self buildLabels];
    m_panel = [[NSPanel alloc] initWithContentRect:NSMakeRect(0, 0, 600, 680)
                                         styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable |
                                                   NSWindowStyleMaskUtilityWindow
                                           backing:NSBackingStoreBuffered
                                             defer:NO];
    m_panel.title = @"Presets";
    m_panel.releasedWhenClosed = NO;
    m_panel.hidesOnDeactivate = NO;
    m_panel.minSize = NSMakeSize(320, 240);
    NSView* content = m_panel.contentView;

    m_search = [[NSSearchField alloc] initWithFrame:NSMakeRect(10, 646, 500, 24)];
    m_search.placeholderString = @"Filter presets";
    m_search.sendsSearchStringImmediately = YES;
    m_search.delegate = self;
    m_search.target = self;
    m_search.action = @selector(filterChanged:);
    m_search.autoresizingMask = NSViewWidthSizable | NSViewMinYMargin;
    [content addSubview:m_search];

    NSImage* listIcon = [NSImage imageWithSystemSymbolName:@"list.bullet" accessibilityDescription:@"List"];
    NSImage* galleryIcon = [NSImage imageWithSystemSymbolName:@"square.grid.2x2" accessibilityDescription:@"Gallery"];
    m_mode = [NSSegmentedControl segmentedControlWithImages:@[ listIcon, galleryIcon ]
                                               trackingMode:NSSegmentSwitchTrackingSelectOne
                                                     target:self
                                                     action:@selector(modeChanged:)];
    m_mode.frame = NSMakeRect(518, 646, 72, 24);
    m_mode.autoresizingMask = NSViewMinXMargin | NSViewMinYMargin;
    [m_mode setToolTip:@"List" forSegment:MODE_LIST];
    [m_mode setToolTip:@"Gallery" forSegment:MODE_GALLERY];
    [content addSubview:m_mode];

    m_count = [NSTextField labelWithString:@""];
    m_count.frame = NSMakeRect(10, 8, 580, 17);
    m_count.font = [NSFont systemFontOfSize:11];
    m_count.textColor = NSColor.secondaryLabelColor;
    m_count.autoresizingMask = NSViewWidthSizable | NSViewMaxYMargin;
    [content addSubview:m_count];

    NSScrollView* scroll = [[NSScrollView alloc] initWithFrame:NSMakeRect(0, 30, 600, 608)];
    m_listScroll = scroll;
    scroll.hasVerticalScroller = YES;
    scroll.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    scroll.drawsBackground = NO;
    m_table = [[PresetTableView alloc] initWithFrame:scroll.bounds];
    NSTableColumn* name = [[NSTableColumn alloc] initWithIdentifier:COLUMN_NAME];
    name.title = @"Preset";
    name.resizingMask = NSTableColumnAutoresizingMask;
    name.width = scroll.contentSize.width;
    [m_table addTableColumn:name];
    m_table.headerView = nil;
    m_table.style = NSTableViewStyleInset;
    m_table.rowHeight = 24;
    m_table.intercellSpacing = NSMakeSize(0, 2);
    m_table.columnAutoresizingStyle = NSTableViewUniformColumnAutoresizingStyle;
    m_table.usesAlternatingRowBackgroundColors = NO;
    m_table.allowsMultipleSelection = NO;
    m_table.dataSource = self;
    m_table.delegate = self;
    m_table.target = self;
    m_table.doubleAction = @selector(playSelected:);
    __weak PresetListController* weakSelf = self;
    m_table.onReturn = ^{ [weakSelf playSelected:nil]; };
    scroll.documentView = m_table;
    [m_table sizeLastColumnToFit];
    [content addSubview:scroll];

    m_galleryScroll = [[NSScrollView alloc] initWithFrame:scroll.frame];
    m_galleryScroll.hasVerticalScroller = YES;
    m_galleryScroll.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    m_galleryScroll.drawsBackground = NO;
    NSCollectionViewFlowLayout* layout = [NSCollectionViewFlowLayout new];
    layout.itemSize = NSMakeSize(TILE_WIDTH, THUMB_SIZE + 44);
    layout.minimumInteritemSpacing = 6;
    layout.minimumLineSpacing = 8;
    layout.sectionInset = NSEdgeInsetsMake(8, 10, 8, 10);
    m_gallery = [[PresetCollectionView alloc] initWithFrame:m_galleryScroll.bounds];
    m_gallery.collectionViewLayout = layout;
    m_gallery.backgroundColors = @[ NSColor.clearColor ];
    m_gallery.selectable = YES;
    m_gallery.allowsMultipleSelection = NO;
    m_gallery.dataSource = self;
    m_gallery.delegate = self;
    [m_gallery registerClass:PresetTile.class forItemWithIdentifier:TILE_ID];
    m_gallery.onPlay = ^{ [weakSelf playSelected:nil]; };
    m_galleryScroll.documentView = m_gallery;
    [content addSubview:m_galleryScroll];

    [self setMode:static_cast<ListMode>([NSUserDefaults.standardUserDefaults integerForKey:MODE_KEY])];
}

- (ListMode)mode { return m_mode.selectedSegment == MODE_GALLERY ? MODE_GALLERY : MODE_LIST; }

// Shows the list or the gallery. The gallery starts the thumbnails rendering the first time.
- (void)setMode:(ListMode)mode {
    m_mode.selectedSegment = mode;
    m_listScroll.hidden = mode != MODE_LIST;
    m_galleryScroll.hidden = mode != MODE_GALLERY;
    [NSUserDefaults.standardUserDefaults setInteger:mode forKey:MODE_KEY];
    if (mode == MODE_GALLERY) {
        if (m_thumbnails == nil) {
            m_thumbnails = [[ThumbnailStore alloc] initWithPresetPaths:m_renderer.presetPaths];
            [m_thumbnails start];
        }
        [m_gallery reloadData];
    }
    [self updateCount];
    [self scrollToPlaying];
    [m_panel makeFirstResponder:(mode == MODE_GALLERY ? static_cast<NSView*>(m_gallery) : m_table)];
}

- (void)modeChanged:(id)sender { [self setMode:[self mode]]; }

- (void)showWindow {
    if (m_panel == nil) {
        [self buildWindow];
        [m_panel center];
        [self applyFilter];
    }
    [m_panel makeKeyAndOrderFront:nil];
    [self scrollToPlaying];
    [m_panel makeFirstResponder:(m_galleryScroll.hidden ? static_cast<NSView*>(m_table) : m_gallery)];
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
    if (!m_galleryScroll.hidden) [m_gallery reloadData];
    [self updateCount];
}

- (void)updateCount {
    NSString* text = [m_search.stringValue stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceCharacterSet];
    const unsigned long total = m_renderer.presetNames.count;
    NSString* count = text.length == 0 ? [NSString stringWithFormat:@"%lu presets", total]
                                       : [NSString stringWithFormat:@"%lu of %lu presets", (unsigned long)m_visible.count, total];
    if (m_thumbnails && m_thumbnails.readyCount < m_thumbnails.totalCount) {
        count = [count stringByAppendingFormat:@" · rendering thumbnails, %ld of %ld done", (long)m_thumbnails.readyCount,
                                               (long)m_thumbnails.totalCount];
    }
    m_count.stringValue = count;
}

- (void)thumbnailDidRender:(NSNotification*)note {
    if (note.object != m_thumbnails) return;
    [self updateCount];
    if (m_galleryScroll.hidden) return;
    const NSInteger row = [self rowForPresetIndex:[note.userInfo[@"index"] integerValue]];
    if (row >= 0) [m_gallery reloadItemsAtIndexPaths:[NSSet setWithObject:[NSIndexPath indexPathForItem:row inSection:0]]];
}

- (NSInteger)rowForPresetIndex:(NSInteger)index {
    for (NSUInteger row = 0; row < m_visible.count; ++row) {
        if (m_visible[row].integerValue == index) return static_cast<NSInteger>(row);
    }
    return -1;
}

- (void)selectRow:(NSInteger)row {
    [m_table selectRowIndexes:[NSIndexSet indexSetWithIndex:row] byExtendingSelection:NO];
    if (m_galleryScroll.hidden) return;
    NSIndexPath* path = [NSIndexPath indexPathForItem:row inSection:0];
    m_gallery.selectionIndexPaths = [NSSet setWithObject:path];
}

- (void)scrollToPlaying {
    const NSInteger row = [self rowForPresetIndex:m_renderer.currentPresetIndex];
    if (row < 0) return;
    [self selectRow:row];
    [m_table scrollRowToVisible:row];
    if (!m_galleryScroll.hidden) {
        [m_gallery scrollToItemsAtIndexPaths:[NSSet setWithObject:[NSIndexPath indexPathForItem:row inSection:0]]
                              scrollPosition:NSCollectionViewScrollPositionCenteredVertically];
    }
}

- (void)filterChanged:(id)sender {
    [self applyFilter];
    if (m_visible.count > 0) [self selectRow:0];
}

- (void)playSelected:(id)sender {
    NSInteger row = m_table.selectedRow >= 0 ? m_table.selectedRow : m_table.clickedRow;
    if (!m_galleryScroll.hidden) {
        NSIndexPath* path = m_gallery.selectionIndexPaths.anyObject;
        row = path ? static_cast<NSInteger>(path.item) : -1;
    }
    if (row < 0 || row >= static_cast<NSInteger>(m_visible.count)) return;
    [m_renderer selectPresetAtIndex:m_visible[row].integerValue];
}

- (void)presetDidChange:(NSNotification*)note {
    if (m_panel == nil) return;
    [m_table reloadData];
    if (!m_galleryScroll.hidden) [m_gallery reloadData];
    if (m_panel.visible) [self scrollToPlaying];
}

// Down arrow and Return in the filter field move to the list and play the first match.
- (BOOL)control:(NSControl*)control textView:(NSTextView*)textView doCommandBySelector:(SEL)selector {
    if (selector == @selector(moveDown:)) {
        [m_panel makeFirstResponder:(m_galleryScroll.hidden ? static_cast<NSView*>(m_table) : m_gallery)];
        return YES;
    }
    if (selector == @selector(insertNewline:)) {
        if (m_visible.count > 0) {
            const bool gallery = !m_galleryScroll.hidden;
            if ((gallery ? m_gallery.selectionIndexPaths.count == 0 : m_table.selectedRow < 0)) [self selectRow:0];
            [self playSelected:nil];
            [m_panel makeFirstResponder:(gallery ? static_cast<NSView*>(m_gallery) : m_table)];
        }
        return YES;
    }
    return NO;
}

- (NSInteger)numberOfRowsInTableView:(NSTableView*)tableView { return static_cast<NSInteger>(m_visible.count); }

- (NSView*)tableView:(NSTableView*)tableView viewForTableColumn:(NSTableColumn*)column row:(NSInteger)row {
    PresetCellView* cell = [tableView makeViewWithIdentifier:CELL_ID owner:self];
    if (cell == nil) {
        cell = [[PresetCellView alloc] initWithFrame:NSMakeRect(0, 0, column.width, tableView.rowHeight)];
        cell.identifier = CELL_ID;
    }
    const NSInteger index = m_visible[row].integerValue;
    const BOOL playing = index == m_renderer.currentPresetIndex;
    cell.playing.hidden = !playing;
    cell.name.stringValue = m_titles[index];
    cell.name.font = playing ? [NSFont systemFontOfSize:13 weight:NSFontWeightSemibold] : [NSFont systemFontOfSize:13];
    cell.folder.stringValue = m_folders[index];
    cell.toolTip = m_renderer.presetNames[index];
    return cell;
}

- (NSInteger)collectionView:(NSCollectionView*)collectionView numberOfItemsInSection:(NSInteger)section {
    return static_cast<NSInteger>(m_visible.count);
}

- (NSCollectionViewItem*)collectionView:(NSCollectionView*)collectionView itemForRepresentedObjectAtIndexPath:(NSIndexPath*)path {
    PresetTile* tile = [collectionView makeItemWithIdentifier:TILE_ID forIndexPath:path];
    const NSInteger index = m_visible[path.item].integerValue;
    [tile showImage:[m_thumbnails imageAtIndex:index]
              title:m_titles[index]
            tooltip:m_renderer.presetNames[index]
            playing:index == m_renderer.currentPresetIndex];
    return tile;
}

@end
