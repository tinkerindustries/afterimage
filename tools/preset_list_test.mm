// Checks the renderer's preset list and the preset list window's filtering and selection without
// showing anything. Exits non-zero on a failure.

#import "preset_list.h"
#import "renderer.h"

#import <Cocoa/Cocoa.h>
#import <MetalKit/MetalKit.h>

static int failures = 0;

static void expect(BOOL ok, NSString* what) {
    if (!ok) {
        ++failures;
        NSLog(@"FAIL %@", what);
    }
}

int main() {
    @autoreleasepool {
        [NSApplication sharedApplication];
        MTKView* view = [[MTKView alloc] initWithFrame:NSMakeRect(0, 0, 320, 240) device:MTLCreateSystemDefaultDevice()];
        view.colorPixelFormat = MTLPixelFormatBGRA8Unorm;
        NSString* error = nil;
        MilkRenderer* renderer = [[MilkRenderer alloc] initWithView:view error:&error];
        expect(renderer != nil, [@"renderer starts: " stringByAppendingString:error ?: @""]);
        if (renderer == nil) return 1;

        NSArray<NSString*>* names = renderer.presetNames;
        expect(names.count > 100, @"the renderer lists the presets");
        expect(![names.firstObject hasSuffix:@".milk"], @"names have no extension");
        expect(renderer.currentPresetIndex == -1, @"no preset is playing at the start");

        __block int changes = 0;
        id observer = [[NSNotificationCenter defaultCenter] addObserverForName:MilkPresetDidChangeNotification
                                                                        object:renderer
                                                                         queue:nil
                                                                    usingBlock:^(NSNotification*) { ++changes; }];
        [renderer selectPresetAtIndex:5];
        expect(renderer.currentPresetIndex == 5, @"selecting an index plays it");
        expect(changes == 1, @"a selection posts one change notification");
        expect([renderer.status containsString:names[5].lastPathComponent], @"the status names the preset");
        [renderer nextPreset];
        expect(renderer.currentPresetIndex == 6 && changes == 2, @"next preset moves on and notifies");
        [renderer selectPresetAtIndex:-1];
        [renderer selectPresetAtIndex:(NSInteger)names.count];
        expect(renderer.currentPresetIndex == 6 && changes == 2, @"an out-of-range index does nothing");
        [[NSNotificationCenter defaultCenter] removeObserver:observer];

        PresetListController* list = [[PresetListController alloc] initWithRenderer:renderer];
        [list showWindow];
        NSArray* visible = [list valueForKey:@"m_visible"];
        expect(visible.count == names.count, @"the list shows every preset without a filter");

        NSSearchField* search = [list valueForKey:@"m_search"];
        NSString* word = [names[10] componentsSeparatedByString:@" "].lastObject;
        search.stringValue = word;
        [list performSelector:@selector(filterChanged:) withObject:search];
        visible = [list valueForKey:@"m_visible"];
        BOOL allMatch = visible.count > 0;
        for (NSNumber* index in visible) {
            allMatch = allMatch && [names[index.integerValue] rangeOfString:word options:NSCaseInsensitiveSearch].location != NSNotFound;
        }
        expect(allMatch && visible.count < names.count, @"the filter keeps only matching presets");
        expect([visible containsObject:@10], @"the filter keeps the preset it was built from");

        NSTableView* table = [list valueForKey:@"m_table"];
        const NSInteger row = [visible indexOfObject:@10];
        [table selectRowIndexes:[NSIndexSet indexSetWithIndex:row] byExtendingSelection:NO];
        [list performSelector:@selector(playSelected:) withObject:nil];
        expect(renderer.currentPresetIndex == 10, @"playing the selected row plays that preset");

        search.stringValue = @"";
        [list performSelector:@selector(filterChanged:) withObject:search];
        visible = [list valueForKey:@"m_visible"];
        expect(visible.count == names.count, @"clearing the filter shows everything again");
        NSLog(@"%d failures", failures);
    }
    return failures == 0 ? 0 : 1;
}
