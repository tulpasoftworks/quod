// SPDX-License-Identifier: MIT
// A film of the Audio Unit, offline: QuodAudioUnit's own render block at a host's rate and its panel (QuodPanelView)
// drawn every frame, as a WAV and PNGs; SHOWCASE pressed at 5 s. Copyright (c) 2026 Kip DeCastro. See ../../LICENSE.
//
//   cd apple && clang -fobjc-arc -O2 -IEngine -IExtension -I../src -I../gen Tools/quod_film.m Extension/QuodAudioUnit.m \
//       Extension/QuodPanelView.m Engine/quod_slot[0-7].c Engine/quod_slots.c Engine/quod_resample.c \
//       -framework Cocoa -framework AVFoundation -framework AudioToolbox -framework QuartzCore -o build/quod_film
//   build/quod_film OUTDIR [SECONDS] [RATE]     -> OUTDIR/quod.wav, OUTDIR/f00000.png ... (30 a second)
#import <Cocoa/Cocoa.h>
#import <AVFoundation/AVFoundation.h>
#import "QuodAudioUnit.h"
#import "QuodPanelView.h"

@interface QuodPanelView (Film)
- (void)tick;
@end

static void put32(FILE *f, uint32_t v) { fwrite(&v, 4, 1, f); }
static void put16(FILE *f, uint16_t v) { fwrite(&v, 2, 1, f); }

int main(int argc, const char *argv[])
{
    @autoreleasepool {
        if (argc < 2) { fprintf(stderr, "quod_film OUTDIR [SECONDS] [RATE]\n"); return 1; }
        NSString *dir = [NSString stringWithUTF8String:argv[1]];
        double seconds = argc > 2 ? atof(argv[2]) : 104, rate = argc > 3 ? atof(argv[3]) : 48000;
        const int fps = 30, combo[2] = {8, 13};         // HOME REC: what SHOWCASE presses
        NSError *error = nil;
        [NSApplication sharedApplication];
        [[NSFileManager defaultManager] createDirectoryAtPath:dir withIntermediateDirectories:YES attributes:nil error:nil];

        AudioComponentDescription desc = {kAudioUnitType_MusicDevice, 'Quod', 'Tlpa', 0, 0};
        QuodAudioUnit *au = [[QuodAudioUnit alloc] initWithComponentDescription:desc error:&error];
        AVAudioFormat *fmt = [[AVAudioFormat alloc] initStandardFormatWithSampleRate:rate channels:2];
        if (!au || ![au.outputBusses[0] setFormat:fmt error:&error] || ![au allocateRenderResourcesAndReturnError:&error]) {
            fprintf(stderr, "the Audio Unit: %s\n", error.localizedDescription.UTF8String);
            return 1;
        }
        AURenderBlock render = au.renderBlock;
        const quod_slot_t *slot = au.slot;

        QuodPanelView *view = [[QuodPanelView alloc] initWithFrame:NSMakeRect(0, 0, 2 * QUOD_PANEL_W, 2 * QUOD_PANEL_H)];
        view.audioUnit = au;

        uint32_t total = (uint32_t)(seconds * rate), done = 0, frame = 0;
        float *l = calloc(4096, sizeof(float)), *r = calloc(4096, sizeof(float));
        NSString *wavPath = [dir stringByAppendingPathComponent:@"quod.wav"];
        FILE *wav = fopen(wavPath.fileSystemRepresentation, "wb");
        fwrite("RIFF", 1, 4, wav); put32(wav, 36 + total * 4); fwrite("WAVEfmt ", 1, 8, wav);
        put32(wav, 16); put16(wav, 1); put16(wav, 2); put32(wav, (uint32_t)rate); put32(wav, (uint32_t)rate * 4);
        put16(wav, 4); put16(wav, 16); fwrite("data", 1, 4, wav); put32(wav, total * 4);
        AudioTimeStamp ts = {0};
        ts.mFlags = kAudioTimeStampSampleTimeValid;
        int pressed = 0;
        while (done < total) {
            uint32_t next = (uint32_t)((uint64_t)(frame + 1) * (uint64_t)rate / fps), n = next - done;
            double t = (double)done / rate;
            if (!pressed && t >= 5.0) { for (int i = 0; i < 2; i++) slot->button((uint32_t)combo[i], 1); pressed = 1; }
            if (pressed == 1 && t >= 6.2) { for (int i = 0; i < 2; i++) slot->button((uint32_t)combo[i], 0); pressed = 2; }
            if (n > total - done) n = total - done;
            for (uint32_t o = 0; o < n;) {      // the host's way: blocks of up to 512
                uint32_t k = n - o > 512 ? 512 : n - o;
                AudioBufferList *abl = alloca(sizeof(AudioBufferList) + sizeof(AudioBuffer));
                AudioUnitRenderActionFlags flags = 0;
                abl->mNumberBuffers = 2;
                abl->mBuffers[0] = (AudioBuffer){1, k * 4, l};
                abl->mBuffers[1] = (AudioBuffer){1, k * 4, r};
                ts.mSampleTime = done + o;
                render(&flags, &ts, k, 0, abl, nil);
                for (uint32_t i = 0; i < k; i++) {
                    float a = l[i] * 32767.0f, b = r[i] * 32767.0f;
                    put16(wav, (uint16_t)(int16_t)(a > 32767 ? 32767 : a < -32767 ? -32767 : a));
                    put16(wav, (uint16_t)(int16_t)(b > 32767 ? 32767 : b < -32767 ? -32767 : b));
                }
                o += k;
            }
            done += n;
            @autoreleasepool {                  // the panel as it is now
                [view tick];
                NSBitmapImageRep *rep = [view bitmapImageRepForCachingDisplayInRect:view.bounds];
                [view cacheDisplayInRect:view.bounds toBitmapImageRep:rep];
                NSData *png = [rep representationUsingType:NSBitmapImageFileTypePNG properties:@{}];
                [png writeToFile:[dir stringByAppendingPathComponent:[NSString stringWithFormat:@"f%05u.png", frame]] atomically:NO];
            }
            frame++;
        }
        fclose(wav);
        printf("%u frames, %.1f s at %.0f Hz -> %s\n", frame, seconds, rate, dir.UTF8String);
    }
    return 0;
}
