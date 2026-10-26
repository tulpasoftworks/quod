// SPDX-License-Identifier: MIT
// Quod, the app: carries the Audio Unit (for Logic, GarageBand, ...) and plays it on its own, from the Mac's MIDI
// sources to its speakers. Copyright (c) 2026 Kip DeCastro. See ../../LICENSE.
#import <Cocoa/Cocoa.h>
#import <AVFoundation/AVFoundation.h>
#import <CoreAudioKit/CoreAudioKit.h>
#import <CoreMIDI/CoreMIDI.h>

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
