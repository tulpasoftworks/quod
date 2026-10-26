// SPDX-License-Identifier: MIT
// Quod as an Audio Unit (an instrument: MIDI in, stereo out).
// Copyright (c) 2026 Kip DeCastro. See ../../LICENSE.
#import <AudioToolbox/AudioToolbox.h>
#include "quod_engine.h"

@interface QuodAudioUnit : AUAudioUnit
@property (nonatomic, readonly) const quod_slot_t *slot;   // NULL: every slot taken (the AU then makes silence)
@end
