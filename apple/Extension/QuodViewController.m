// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Kip DeCastro. See ../../LICENSE.
#import "QuodViewController.h"
#import "QuodAudioUnit.h"
#import "QuodPanelView.h"

@implementation QuodViewController {
    QuodAudioUnit *_au;
    QuodPanelView *_panel;
}

- (void)loadView
{
    _panel = [[QuodPanelView alloc] initWithFrame:NSMakeRect(0, 0, QUOD_PANEL_W, QUOD_PANEL_H)];
    _panel.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    _panel.audioUnit = _au;
    self.view = _panel;
    self.preferredContentSize = NSMakeSize(QUOD_PANEL_W, QUOD_PANEL_H);
}

- (AUAudioUnit *)createAudioUnitWithComponentDescription:(AudioComponentDescription)desc error:(NSError **)error
{
    _au = [[QuodAudioUnit alloc] initWithComponentDescription:desc error:error];
    dispatch_async(dispatch_get_main_queue(), ^{ self->_panel.audioUnit = self->_au; });
    return _au;
}

@end
