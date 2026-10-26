// SPDX-License-Identifier: MIT
// The FM-1's panel on a Mac: its screen, the seven encoders, the fourteen buttons, the 27 keys.
// Copyright (c) 2026 Kip DeCastro. See ../../LICENSE.
#import <Cocoa/Cocoa.h>
#import "QuodAudioUnit.h"

#define QUOD_PANEL_W 520.0
#define QUOD_PANEL_H 880.0

@interface QuodPanelView : NSView
@property (nonatomic, strong) QuodAudioUnit *audioUnit;
@end
