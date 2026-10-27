// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Kip DeCastro. See ../../LICENSE.
#import <AVFoundation/AVFoundation.h>
#import "QuodAudioUnit.h"
#include "quod_resample.h"
#include <stdatomic.h>
#include <stdlib.h>

static atomic_bool slot_taken[QUOD_SLOTS];

typedef struct {                                // what the render block touches (no Objective-C in there)
    const quod_slot_t *slot;
    quod_rs_t *rs;
    float *scratch_l, *scratch_r;
    AUAudioFrameCount max_frames;
} render_ctx_t;

@implementation QuodAudioUnit {
    AUAudioUnitBus *_out;
    AUAudioUnitBusArray *_outs;
    AUParameterTree *_tree;
    int _slotIndex;
    render_ctx_t _ctx;
}

- (instancetype)initWithComponentDescription:(AudioComponentDescription)desc
                                     options:(AudioComponentInstantiationOptions)options
                                       error:(NSError **)error
{
    if (!(self = [super initWithComponentDescription:desc options:options error:error])) return nil;
    _slotIndex = -1;
    for (int i = 0; i < QUOD_SLOTS; i++) {
        bool no = false;
        if (atomic_compare_exchange_strong(&slot_taken[i], &no, true)) { _slotIndex = i; break; }
    }
    if (_slotIndex >= 0) {
        _slot = QUOD_SLOT[_slotIndex];
        _slot->init();
    }
    AVAudioFormat *fmt = [[AVAudioFormat alloc] initStandardFormatWithSampleRate:QUOD_FS channels:2];
    _out = [[AUAudioUnitBus alloc] initWithFormat:fmt error:error];
    if (!_out) return nil;
    _out.maximumChannelCount = 2;
    _outs = [[AUAudioUnitBusArray alloc] initWithAudioUnit:self busType:AUAudioUnitBusTypeOutput busses:@[_out]];
    [self buildParameters];
    self.maximumFramesToRender = 4096;
    return self;
}

- (void)dealloc
{
    free(_ctx.rs); free(_ctx.scratch_l); free(_ctx.scratch_r);
    if (_slotIndex >= 0) atomic_store(&slot_taken[_slotIndex], false);
}

static NSString *title_case(const char *s)       // "HEAVEN / HELL" -> "Heaven / Hell"
{
    NSMutableString *t = [NSMutableString string];
    BOOL start = YES;
    for (; *s; s++) {
        char c = *s;
        [t appendFormat:@"%c", start ? c : (char)tolower(c)];
        start = c == ' ' || c == '/';
    }
    return t;
}

- (void)buildParameters
{
    NSMutableArray<AUParameter *> *ps = [NSMutableArray array];
    const quod_slot_t *s = _slot ? _slot : QUOD_SLOT[0];
    for (uint32_t i = 0; i < s->param_count(); i++) {
        int32_t lo, hi, def;
        s->param_range(i, &lo, &hi, &def);
        AUParameter *p = [AUParameterTree createParameterWithIdentifier:[NSString stringWithFormat:@"p%u", i]
            name:title_case(s->param_name(i)) address:i min:lo max:hi unit:kAudioUnitParameterUnit_Generic
            unitName:nil flags:kAudioUnitParameterFlag_IsReadable | kAudioUnitParameterFlag_IsWritable
            valueStrings:nil dependentParameters:nil];
        p.value = def;
        [ps addObject:p];
    }
    _tree = [AUParameterTree createTreeWithChildren:ps];
    const quod_slot_t *slot = _slot;
    _tree.implementorValueObserver = ^(AUParameter *p, AUValue v) { if (slot) slot->param_set((uint32_t)p.address, (int32_t)lrintf(v)); };
    _tree.implementorValueProvider = ^AUValue(AUParameter *p) { return slot ? (AUValue)slot->param_get((uint32_t)p.address) : p.value; };
    _tree.implementorStringFromValueCallback = ^NSString *(AUParameter *p, const AUValue *v) {
        return [NSString stringWithFormat:@"%d", (int)lrintf(v ? *v : p.value)];
    };
}

- (AUParameterTree *)parameterTree { return _tree; }
- (AUAudioUnitBusArray *)outputBusses { return _outs; }
- (NSArray<NSNumber *> *)channelCapabilities { return @[@0, @2]; }
- (BOOL)providesUserPresets { return NO; }
- (BOOL)shouldChangeToFormat:(AVAudioFormat *)format forBus:(AUAudioUnitBus *)bus
{
    return format.channelCount == 2 && format.commonFormat == AVAudioPCMFormatFloat32 && !format.interleaved;
}
- (NSArray<NSString *> *)MIDIOutputNames { return @[]; }

// ---- the project's state: the controls, the steps, their notes and locks
- (NSDictionary<NSString *, id> *)fullState
{
    NSMutableDictionary *d = [[super fullState] mutableCopy] ?: [NSMutableDictionary dictionary];
    if (_slot) {
        uint8_t buf[1024];
        uint32_t n = _slot->state_get(buf, sizeof buf);
        if (n) d[@"quod"] = [NSData dataWithBytes:buf length:n];
        n = _slot->backup_get(buf, sizeof buf);   // (the same as an FM-1 backup, a .quodstate file: the app's
        if (n) d[@"quodBackup"] = [NSData dataWithBytes:buf length:n];   // Open / Save State)
    }
    return d;
}

- (void)setFullState:(NSDictionary<NSString *, id> *)fullState
{
    [super setFullState:fullState];
    NSData *q = fullState[@"quod"], *b = fullState[@"quodBackup"];
    if (_slot && [q isKindOfClass:[NSData class]]) _slot->state_set(q.bytes, (uint32_t)q.length);
    else if (_slot && [b isKindOfClass:[NSData class]]) _slot->backup_set(b.bytes, (uint32_t)b.length);
}

// ---- rendering
- (BOOL)allocateRenderResourcesAndReturnError:(NSError **)error
{
    if (![super allocateRenderResourcesAndReturnError:error]) return NO;
    if (_out.format.channelCount != 2) {
        if (error) *error = [NSError errorWithDomain:NSOSStatusErrorDomain code:kAudioUnitErr_FormatNotSupported userInfo:nil];
        return NO;
    }
    free(_ctx.rs); free(_ctx.scratch_l); free(_ctx.scratch_r);
    _ctx.slot = _slot;
    _ctx.max_frames = self.maximumFramesToRender;
    _ctx.rs = calloc(1, sizeof *_ctx.rs);
    _ctx.scratch_l = calloc(_ctx.max_frames, sizeof(float));
    _ctx.scratch_r = calloc(_ctx.max_frames, sizeof(float));
    if (_slot) quod_rs_init(_ctx.rs, _slot, _out.format.sampleRate);
    return YES;
}

- (void)deallocateRenderResources
{
    [super deallocateRenderResources];
}

static void handle_event(const render_ctx_t *c, const AURenderEvent *e)
{
    switch (e->head.eventType) {
    case AURenderEventParameter:
    case AURenderEventParameterRamp:
        c->slot->param_set((uint32_t)e->parameter.parameterAddress, (int32_t)lrintf(e->parameter.value));
        break;
    case AURenderEventMIDI:
        if (e->MIDI.length >= 2)
            c->slot->midi(e->MIDI.data[0], e->MIDI.data[1], e->MIDI.length >= 3 ? e->MIDI.data[2] : 0);
        break;
    default:
        break;
    }
}

- (AUInternalRenderBlock)internalRenderBlock
{
    render_ctx_t *c = &_ctx;
    return ^AUAudioUnitStatus(AudioUnitRenderActionFlags *flags, const AudioTimeStamp *ts, AVAudioFrameCount frames,
                              NSInteger bus, AudioBufferList *out, const AURenderEvent *events,
                              AURenderPullInputBlock pull) {
        (void)flags; (void)ts; (void)bus; (void)pull;
        float *l, *r;
        if (frames > c->max_frames) return kAudioUnitErr_TooManyFramesToProcess;
        if (out->mNumberBuffers < 2) return kAudioUnitErr_FormatNotSupported;
        if (!out->mBuffers[0].mData) out->mBuffers[0].mData = c->scratch_l;   // (the host asks us for buffers)
        if (!out->mBuffers[1].mData) out->mBuffers[1].mData = c->scratch_r;
        l = out->mBuffers[0].mData;
        r = out->mBuffers[1].mData;
        out->mBuffers[0].mDataByteSize = out->mBuffers[1].mDataByteSize = frames * sizeof(float);
        if (!c->slot) {
            memset(l, 0, frames * sizeof(float)); memset(r, 0, frames * sizeof(float));
            return noErr;
        }
        for (const AURenderEvent *e = events; e; e = e->head.next) handle_event(c, e);   // (a block's events at
                                                                                        // its start: Quod's own
                                                                                        // time is a millisecond)
        quod_rs_render(c->rs, l, r, frames);
        return noErr;
    };
}

@end
