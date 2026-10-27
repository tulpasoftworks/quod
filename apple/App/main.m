// SPDX-License-Identifier: MIT
// Quod, the app: carries the Audio Unit (for Logic, GarageBand, ...) and plays it on its own, from the Mac's MIDI
// sources to its speakers. Copyright (c) 2026 Kip DeCastro. See ../../LICENSE.
#import <Cocoa/Cocoa.h>
#import <AVFoundation/AVFoundation.h>
#import <CoreAudioKit/CoreAudioKit.h>
#import <CoreMIDI/CoreMIDI.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

static const AudioComponentDescription QUOD = {kAudioUnitType_MusicDevice, 'Quod', 'Tlpa', 0, 0};

@interface AppDelegate : NSObject <NSApplicationDelegate>
@end

@implementation AppDelegate {
    NSWindow *_window;
    AVAudioEngine *_engine;
    AVAudioUnit *_unit;
    MIDIClientRef _client;
    MIDIPortRef _in;
}

- (void)applicationDidFinishLaunching:(NSNotification *)n
{
    _window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 520, 880)
                                          styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                                                    NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable
                                            backing:NSBackingStoreBuffered defer:NO];
    [self menus];
    _window.title = @"Quod";
    _window.contentAspectRatio = NSMakeSize(520, 880);
    [_window center];
    [_window makeKeyAndOrderFront:nil];
    [AVAudioUnit instantiateWithComponentDescription:QUOD options:kAudioComponentInstantiation_LoadOutOfProcess
                                   completionHandler:^(AVAudioUnit *unit, NSError *error) {
        dispatch_async(dispatch_get_main_queue(), ^{
            if (!unit) { [self fail:error]; return; }
            [self play:unit];
        });
    }];
}

- (void)fail:(NSError *)error
{
    NSAlert *a = [NSAlert new];
    a.messageText = @"Quod's Audio Unit didn't load";
    a.informativeText = error.localizedDescription ?: @"The extension may not be registered yet: quit and open the app again.";
    [a runModal];
}

- (void)play:(AVAudioUnit *)unit
{
    NSError *error = nil;
    _unit = unit;
    _engine = [AVAudioEngine new];
    [_engine attachNode:unit];
    [_engine connect:unit to:_engine.mainMixerNode format:nil];
    if (![_engine startAndReturnError:&error]) { [self fail:error]; return; }
    [unit.AUAudioUnit requestViewControllerWithCompletionHandler:^(AUViewControllerBase *vc) {
        dispatch_async(dispatch_get_main_queue(), ^{
            if (!vc) return;
            vc.view.frame = self->_window.contentView.bounds;
            vc.view.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
            [self->_window.contentView addSubview:vc.view];
            self->_window.contentViewController = vc;
            [self->_window setContentSize:NSMakeSize(520, 880)];
        });
    }];
    [self listen];
}

- (void)listen                                  // every MIDI source, now and as they come, into the unit
{
    AUScheduleMIDIEventBlock send = _unit.AUAudioUnit.scheduleMIDIEventBlock;
    if (!send) return;
    MIDIClientCreateWithBlock(CFSTR("Quod"), &_client, ^(const MIDINotification *note) {
        if (note->messageID == kMIDIMsgSetupChanged) [self connectSources];
    });
    MIDIInputPortCreateWithProtocol(_client, CFSTR("in"), kMIDIProtocol_1_0, &_in, ^(const MIDIEventList *list, void *ref) {
        const MIDIEventPacket *p = list->packet;
        for (UInt32 i = 0; i < list->numPackets; i++, p = MIDIEventPacketNext(p))
            for (UInt32 w = 0; w < p->wordCount; w++) {
                UInt32 u = p->words[w];
                if (u >> 28 == 2) {             // a MIDI 1.0 channel voice message
                    uint8_t m[3] = {(uint8_t)(u >> 16), (uint8_t)(u >> 8 & 127), (uint8_t)(u & 127)};
                    send(AUEventSampleTimeImmediate, 0, 3, m);
                }
            }
    });
    [self connectSources];
}

- (void)connectSources
{
    for (ItemCount i = 0; i < MIDIGetNumberOfSources(); i++) MIDIPortConnectSource(_in, MIDIGetSource(i), NULL);
}

// ---- File: a .quodstate file (an FM-1 backup: the FM-1's, the browser's and the plug-in's are the same)
- (void)menus
{
    NSMenu *bar = [NSMenu new], *app = [NSMenu new], *file = [[NSMenu alloc] initWithTitle:@"File"];
    [app addItemWithTitle:@"About Quod" action:@selector(orderFrontStandardAboutPanel:) keyEquivalent:@""];
    [app addItem:[NSMenuItem separatorItem]];
    [app addItemWithTitle:@"Quit Quod" action:@selector(terminate:) keyEquivalent:@"q"];
    [file addItemWithTitle:@"Open State\u2026" action:@selector(openState:) keyEquivalent:@"o"];
    [file addItemWithTitle:@"Save State\u2026" action:@selector(saveState:) keyEquivalent:@"s"];
    [[bar addItemWithTitle:@"" action:nil keyEquivalent:@""] setSubmenu:app];
    [[bar addItemWithTitle:@"File" action:nil keyEquivalent:@""] setSubmenu:file];
    NSApp.mainMenu = bar;
}

- (void)say:(NSString *)what
{
    NSAlert *a = [NSAlert new];
    a.messageText = what;
    [a beginSheetModalForWindow:_window completionHandler:nil];
}

- (void)saveState:(id)sender
{
    NSData *b = _unit.AUAudioUnit.fullState[@"quodBackup"];
    if (![b isKindOfClass:[NSData class]]) return;
    NSSavePanel *p = [NSSavePanel savePanel];
    p.nameFieldStringValue = @"quod.quodstate";
    p.allowedContentTypes = @[[UTType typeWithFilenameExtension:@"quodstate"] ?: UTTypeData];
    [p beginSheetModalForWindow:_window completionHandler:^(NSModalResponse r) {
        if (r == NSModalResponseOK && ![b writeToURL:p.URL atomically:YES]) [self say:@"The state wasn't saved."];
    }];
}

- (void)openState:(id)sender
{
    if (!_unit) return;
    NSOpenPanel *p = [NSOpenPanel openPanel];
    p.allowedContentTypes = @[[UTType typeWithFilenameExtension:@"quodstate"] ?: UTTypeData];
    [p beginSheetModalForWindow:_window completionHandler:^(NSModalResponse r) {
        if (r != NSModalResponseOK) return;
        NSData *b = [NSData dataWithContentsOfURL:p.URL];
        NSMutableDictionary *d = [self->_unit.AUAudioUnit.fullState mutableCopy];
        if (!b || !d) return;
        [d removeObjectForKey:@"quod"];     // (the unit takes a backup when there is no state of its own)
        d[@"quodBackup"] = b;
        self->_unit.AUAudioUnit.fullState = d;
        if (![self->_unit.AUAudioUnit.fullState[@"quodBackup"] isEqual:b])
            [self say:@"That isn't a Quod state file of this version, or it is damaged."];
    }];
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)a { return YES; }

@end

int main(int argc, const char *argv[])
{
    @autoreleasepool {
        NSApplication *app = [NSApplication sharedApplication];
        AppDelegate *d = [AppDelegate new];
        app.delegate = d;
        [app setActivationPolicy:NSApplicationActivationPolicyRegular];
        [app activateIgnoringOtherApps:YES];
        [app run];
    }
    return 0;
}
