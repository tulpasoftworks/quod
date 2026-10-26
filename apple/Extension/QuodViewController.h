// SPDX-License-Identifier: MIT
// The extension's principal class: makes the Audio Unit and shows its panel. Copyright (c) 2026 Kip DeCastro.
#import <CoreAudioKit/CoreAudioKit.h>

@interface QuodViewController : AUViewController <AUAudioUnitFactory>
@end
