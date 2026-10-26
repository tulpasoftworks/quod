// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Kip DeCastro. See ../../LICENSE.
//
// The layout is in panel units (QUOD_PANEL_W x QUOD_PANEL_H, y down), scaled to the view. Mouse: press a button or a
// key; drag an encoder up / down (or scroll on it). Shift-click a button to latch it (click it again to let go).
// The buttons are named for what they do in Quod; the FM-1's ENV, LFO and ARP do nothing in it and aren't here.
// The FM-1's GLO is SHOWCASE: a click holds HOME + REC long enough to start it. The encoders are named
// for what each turns now (the planets, the FX page's, or the shifted ones).
#import "QuodPanelView.h"
#import <QuartzCore/QuartzCore.h>

static const char *const BTN_NAME[14] = {"OCT \u2212", "OCT +", "FX", "SHIFT", "", "", "SETTLE",
                                         "SHOWCASE", "HOME", "SAVE", "", "STEPS", "PLAY", "REC"};
enum { B_GLO = 7, B_HOME = 8, B_REC = 13 };
/* where each button sits: {row, column}, or {-1}: not on this panel (ENV, LFO, ARP). The first row plays the drone,
 * the second the sequencer, SAVE and the showcase. */
static const int BTN_POS[14][2] = {{0, 0}, {0, 1}, {0, 4}, {0, 2}, {-1}, {-1}, {0, 3},
                                   {1, 4}, {0, 5}, {1, 3}, {-1}, {1, 0}, {1, 1}, {1, 2}};
static const int ROW_N[2] = {6, 5};
static const int BLACK_KEY[12] = {0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0};   // from F

#define SCR_X 20.0
#define SCR_Y 20.0
#define SCR_S 480.0
#define ENC_Y 540.0                             // the encoders' top row; the second ENC_DY below it
#define ENC_DY 52.0
#define ENC_R 17.0
#define BTN_Y0 646.0
#define BTN_H 30.0
#define BTN_GAP 8.0
#define KEY_Y 740.0
#define KEY_H 120.0
#define KEY_BH 74.0

@implementation QuodPanelView {
    NSTimer *_timer;
    uint32_t _seq;
    uint64_t _leds;
    uint32_t _latched;                          // buttons Shift-clicked down
    int _downButton, _downKey, _dragEnc;
    CGFloat _dragAcc;
    CGFloat _scrollAcc[7];
    CFTimeInterval _showUntil;                  // SHOWCASE: the combo's buttons let go then
    uint8_t _rgba[QUOD_W * QUOD_H * 4];
    float _lastParam[64];
}

- (instancetype)initWithFrame:(NSRect)frame
{
    if (!(self = [super initWithFrame:frame])) return nil;
    _downButton = _downKey = _dragEnc = -1;
    for (int i = 0; i < 64; i++) _lastParam[i] = NAN;
    __weak QuodPanelView *weakSelf = self;
    _timer = [NSTimer timerWithTimeInterval:1.0 / 30.0 repeats:YES block:^(NSTimer *t) { [weakSelf tick]; }];
    [[NSRunLoop mainRunLoop] addTimer:_timer forMode:NSRunLoopCommonModes];
    return self;
}

- (void)viewWillMoveToWindow:(NSWindow *)w { if (!w) { [_timer invalidate]; _timer = nil; } }
- (BOOL)isFlipped { return YES; }
- (BOOL)acceptsFirstMouse:(NSEvent *)e { return YES; }

- (const quod_slot_t *)slot { return _audioUnit.slot; }

- (void)tick
{
    const quod_slot_t *s = self.slot;
    uint32_t seq;
    uint64_t leds;
    if (!s) return;
    if (_showUntil && CACurrentMediaTime() >= _showUntil) {
        const int combo[2] = {B_HOME, B_REC};
        for (int i = 0; i < 2; i++) s->button((uint32_t)combo[i], 0);
        _showUntil = 0;
    }
    s->screen(&seq);
    leds = s->leds();
    if (seq != _seq || leds != _leds) { _seq = seq; _leds = leds; [self setNeedsDisplay:YES]; }
    AUParameterTree *tree = _audioUnit.parameterTree;   // the panel turned a control: tell the host (automation)
    for (AUParameter *p in tree.allParameters) {
        float v = (float)s->param_get((uint32_t)p.address);
        if (p.address < 64 && v != _lastParam[p.address]) {
            if (!isnan(_lastParam[p.address])) [p setValue:v originator:nil];
            _lastParam[p.address] = v;
        }
    }
}

// ---- layout (panel units)
- (CGFloat)scale { return MIN(self.bounds.size.width / QUOD_PANEL_W, self.bounds.size.height / QUOD_PANEL_H); }
- (NSPoint)panelPoint:(NSEvent *)e
{
    NSPoint p = [self convertPoint:e.locationInWindow fromView:nil];
    CGFloat k = self.scale;
    return NSMakePoint(p.x / k, p.y / k);
}
// the encoders as the FM-1 has them, in six columns:        SELECT  KNOB 1  KNOB 2  KNOB 3  KNOB 4
//                                                   PRESETS  ALGORITHM
static const int ENC_POS[7][2] = {{1, 0}, {1, 1}, {2, 0}, {3, 0}, {4, 0}, {5, 0}, {0, 1}};   // (column, row)
static NSPoint enc_centre(int e)
{
    return NSMakePoint(SCR_X + SCR_S * (ENC_POS[e][0] + 0.5) / 6.0, ENC_Y + ENC_POS[e][1] * ENC_DY);
}
static NSRect btn_rect(int b)
{
    int row = BTN_POS[b][0], col = BTN_POS[b][1];
    CGFloat w;
    if (row < 0) return NSZeroRect;
    w = (SCR_S - (ROW_N[row] - 1) * BTN_GAP) / ROW_N[row];
    return NSMakeRect(SCR_X + col * (w + BTN_GAP), BTN_Y0 + row * (BTN_H + BTN_GAP), w, BTN_H);
}
static int white_index(int k)                   // key k (0 = F3) -> its white key's place, or -1 for a black key
{
    int w = 0;
    if (BLACK_KEY[k % 12]) return -1;
    for (int i = 0; i < k; i++) w += !BLACK_KEY[i % 12];
    return w;
}
#define WHITE_W (SCR_S / 16.0)
static NSRect key_rect(int k)
{
    if (!BLACK_KEY[k % 12]) return NSMakeRect(SCR_X + white_index(k) * WHITE_W, KEY_Y, WHITE_W, KEY_H);
    return NSMakeRect(SCR_X + white_index(k - 1) * WHITE_W + WHITE_W * 0.68, KEY_Y, WHITE_W * 0.64, KEY_BH);
}
- (int)keyAt:(NSPoint)p
{
    for (int k = 0; k < 27; k++) if (BLACK_KEY[k % 12] && NSPointInRect(p, key_rect(k))) return k;
    for (int k = 0; k < 27; k++) if (!BLACK_KEY[k % 12] && NSPointInRect(p, key_rect(k))) return k;
    return -1;
}
- (int)buttonAt:(NSPoint)p
{
    for (int b = 0; b < 14; b++) if (BTN_POS[b][0] >= 0 && NSPointInRect(p, btn_rect(b))) return b;
    return -1;
}
- (int)encAt:(NSPoint)p
{
    for (int e = 0; e < 7; e++) {
        NSPoint c = enc_centre(e);
        if (hypot(p.x - c.x, p.y - c.y) <= ENC_R + 8) return e;
    }
    return -1;
}

// ---- drawing
static NSString *title(const char *s)           // "HEAVEN / HELL" -> "Heaven/Hell" (it fits under a knob)
{
    NSMutableString *t = [NSMutableString string];
    BOOL start = YES;
    for (; *s; s++) {
        if (*s == ' ' && (s[1] == '/' || (s[-1] == '/'))) continue;
        [t appendFormat:@"%c", start ? *s : (char)tolower(*s)];
        start = *s == ' ' || *s == '/';
    }
    return t;
}

static void label(NSString *s, NSRect r, NSColor *c, CGFloat size)
{
    NSMutableParagraphStyle *ps = [NSMutableParagraphStyle new];
    ps.alignment = NSTextAlignmentCenter;
    NSDictionary *a = @{NSFontAttributeName: [NSFont monospacedSystemFontOfSize:size weight:NSFontWeightMedium],
                        NSForegroundColorAttributeName: c, NSParagraphStyleAttributeName: ps};
    NSSize sz = [s sizeWithAttributes:a];
    [s drawInRect:NSMakeRect(r.origin.x, r.origin.y + (r.size.height - sz.height) / 2, r.size.width, sz.height)
        withAttributes:a];
}

- (void)drawScreen:(CGContextRef)g
{
    const quod_slot_t *s = self.slot;
    uint32_t seq;
    if (!s) return;
    const uint16_t *fb = s->screen(&seq);
    for (int i = 0; i < QUOD_W * QUOD_H; i++) {
        uint16_t v = (uint16_t)(fb[i] >> 8 | fb[i] << 8);   // (stored byte-swapped, as the FM-1's panel takes it)
        _rgba[4 * i] = (uint8_t)((v >> 11) * 255 / 31);
        _rgba[4 * i + 1] = (uint8_t)((v >> 5 & 63) * 255 / 63);
        _rgba[4 * i + 2] = (uint8_t)((v & 31) * 255 / 31);
        _rgba[4 * i + 3] = 255;
    }
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    CGContextRef bc = CGBitmapContextCreate(_rgba, QUOD_W, QUOD_H, 8, QUOD_W * 4, cs,
                                            (CGBitmapInfo)kCGImageAlphaNoneSkipLast);
    CGImageRef img = CGBitmapContextCreateImage(bc);
    CGContextSaveGState(g);
    CGContextSetInterpolationQuality(g, kCGInterpolationNone);
    CGContextTranslateCTM(g, SCR_X, SCR_Y + SCR_S);   // (the view is flipped; the image is not)
    CGContextScaleCTM(g, 1, -1);
    CGContextDrawImage(g, CGRectMake(0, 0, SCR_S, SCR_S), img);
    CGContextRestoreGState(g);
    CGImageRelease(img);
    CGContextRelease(bc);
    CGColorSpaceRelease(cs);
}

- (void)drawRect:(NSRect)dirty
{
    CGContextRef g = NSGraphicsContext.currentContext.CGContext;
    CGFloat k = self.scale;
    [[NSColor colorWithWhite:0.07 alpha:1] setFill];
    NSRectFill(self.bounds);
    CGContextSaveGState(g);
    CGContextScaleCTM(g, k, k);
    [[NSColor blackColor] setFill];
    NSRectFill(NSMakeRect(SCR_X - 4, SCR_Y - 4, SCR_S + 8, SCR_S + 8));
    [self drawScreen:g];
    if (!self.slot) label(@"EVERY QUOD SLOT IS IN USE", NSMakeRect(SCR_X, SCR_Y + 220, SCR_S, 40), NSColor.grayColor, 14);

    for (int e = 0; e < 7; e++) {               // the encoders
        NSPoint c = enc_centre(e);
        NSBezierPath *o = [NSBezierPath bezierPathWithOvalInRect:NSMakeRect(c.x - ENC_R, c.y - ENC_R, 2 * ENC_R, 2 * ENC_R)];
        [[NSColor colorWithWhite:e == _dragEnc ? 0.32 : 0.2 alpha:1] setFill];
        [o fill];
        [[NSColor colorWithWhite:0.45 alpha:1] setStroke];
        o.lineWidth = 1.5;
        [o stroke];
        int lock = 0;                           // its dial, as the screen draws it: 270 degrees from the bottom left,
        int32_t lv = self.slot ? self.slot->enc_level((uint32_t)e, &lock) : -1;   // lit as far as it is (gold: a lock)
        if (lv >= 0) {
            NSColor *on = lock ? [NSColor colorWithRed:0.95 green:0.75 blue:0.25 alpha:1]
                               : [NSColor colorWithWhite:0.85 alpha:1];
            NSBezierPath *lit = [NSBezierPath bezierPath], *rest = [NSBezierPath bezierPath];
            CGFloat at = -135.0 + 270.0 * lv / 32767.0, r = ENC_R - 4;
            for (int n = 0; n <= 48; n++) {     // the whole arc, dim; lit up to the dial's place
                CGFloat d = -135.0 + n * 270.0 / 48.0;
                NSPoint q = NSMakePoint(c.x + r * sin(d * M_PI / 180.0), c.y - r * cos(d * M_PI / 180.0));
                if (n) [rest lineToPoint:q]; else [rest moveToPoint:q];
                if (d <= at) { if (n) [lit lineToPoint:q]; else [lit moveToPoint:q]; }
            }
            rest.lineWidth = lit.lineWidth = 2;
            [[NSColor colorWithWhite:0.32 alpha:1] setStroke]; [rest stroke];
            [on setStroke]; [lit stroke];
            [[NSColor whiteColor] setFill];   // the pointer
            CGFloat px = c.x + r * sin(at * M_PI / 180.0), py = c.y - r * cos(at * M_PI / 180.0);
            NSRectFill(NSMakeRect(px - 1.5, py - 1.5, 3, 3));
        }
        const char *name = self.slot ? self.slot->enc_name((uint32_t)e) : "";
        label(title(name), NSMakeRect(c.x - 36, c.y - ENC_R - 15, 72, 12),
              [NSColor colorWithWhite:*name ? 0.65 : 0.3 alpha:1], 9);
    }
    for (int b = 0; b < 14; b++) {              // the buttons, lit by Quod's LEDs
        NSRect r = btn_rect(b);
        BOOL lit = (_leds >> b) & 1, held = b == _downButton || ((_latched >> b) & 1) || (b == B_GLO && _showUntil);
        NSBezierPath *p;
        if (BTN_POS[b][0] < 0) continue;
        p = [NSBezierPath bezierPathWithRoundedRect:r xRadius:5 yRadius:5];
        [(held ? [NSColor colorWithWhite:0.42 alpha:1] : [NSColor colorWithWhite:0.17 alpha:1]) setFill];
        [p fill];
        if (lit) {
            [[NSColor colorWithRed:0.95 green:0.35 blue:0.25 alpha:1] setStroke];
            p.lineWidth = 2;
            [p stroke];
        }
        label([NSString stringWithUTF8String:BTN_NAME[b]], r, lit ? NSColor.whiteColor : [NSColor colorWithWhite:0.75 alpha:1],
              11);
    }
    for (int pass = 0; pass < 2; pass++)        // the keys: the white ones, then the black over them
        for (int key = 0; key < 27; key++) {
            BOOL black = BLACK_KEY[key % 12];
            if (black != pass) continue;
            NSRect r = key_rect(key);
            BOOL down = key == _downKey, lit = (_leds >> (14 + key)) & 1;
            NSColor *c = black ? [NSColor colorWithWhite:down ? 0.35 : 0.1 alpha:1]
                               : [NSColor colorWithWhite:down ? 0.72 : 0.9 alpha:1];
            [c setFill];
            NSRectFill(NSInsetRect(r, 0.5, 0));
            if (lit) {
                [[NSColor colorWithRed:0.95 green:0.35 blue:0.25 alpha:1] setFill];
                NSRectFill(NSMakeRect(r.origin.x + r.size.width / 2 - 4, r.origin.y + r.size.height - 14, 8, 8));
            }
        }
    CGContextRestoreGState(g);
}

// ---- the mouse
- (void)mouseDown:(NSEvent *)e
{
    const quod_slot_t *s = self.slot;
    NSPoint p = [self panelPoint:e];
    int b, key, enc;
    if (!s) return;
    if ((b = [self buttonAt:p]) == B_GLO) {                    // SHOWCASE: the combo, held 1.2 s
        const int combo[2] = {B_HOME, B_REC};
        for (int i = 0; i < 2; i++) s->button((uint32_t)combo[i], 1);
        _showUntil = CACurrentMediaTime() + 1.2;
    } else if (b >= 0) {
        if (e.modifierFlags & NSEventModifierFlagShift) {   // latch: held until clicked again
            _latched ^= 1u << b;
            s->button((uint32_t)b, (_latched >> b) & 1);
        } else if ((_latched >> b) & 1) {
            _latched &= ~(1u << b);
            s->button((uint32_t)b, 0);
        } else {
            _downButton = b;
            s->button((uint32_t)b, 1);
        }
    } else if ((key = [self keyAt:p]) >= 0) {
        _downKey = key;
        s->key((uint32_t)key, 1);
    } else if ((enc = [self encAt:p]) >= 0) {
        _dragEnc = enc;
        _dragAcc = 0;
    }
    [self setNeedsDisplay:YES];
}

- (void)mouseDragged:(NSEvent *)e
{
    const quod_slot_t *s = self.slot;
    if (!s) return;
    if (_dragEnc >= 0) {                        // up = clockwise, a detent every 6 points
        _dragAcc -= e.deltaY;
        while (_dragAcc >= 6) { s->enc((uint32_t)_dragEnc, 1); _dragAcc -= 6; }
        while (_dragAcc <= -6) { s->enc((uint32_t)_dragEnc, -1); _dragAcc += 6; }
    } else if (_downKey >= 0) {                 // a glissando across the keys
        int key = [self keyAt:[self panelPoint:e]];
        if (key >= 0 && key != _downKey) {
            s->key((uint32_t)_downKey, 0);
            _downKey = key;
            s->key((uint32_t)key, 1);
            [self setNeedsDisplay:YES];
        }
    }
}

- (void)mouseUp:(NSEvent *)e
{
    const quod_slot_t *s = self.slot;
    if (s && _downButton >= 0) s->button((uint32_t)_downButton, 0);
    if (s && _downKey >= 0) s->key((uint32_t)_downKey, 0);
    _downButton = _downKey = _dragEnc = -1;
    [self setNeedsDisplay:YES];
}

- (void)scrollWheel:(NSEvent *)e
{
    const quod_slot_t *s = self.slot;
    int enc = [self encAt:[self panelPoint:e]];
    if (!s || enc < 0) return;
    _scrollAcc[enc] += e.hasPreciseScrollingDeltas ? e.scrollingDeltaY / 6.0 : e.scrollingDeltaY;
    while (_scrollAcc[enc] >= 1) { s->enc((uint32_t)enc, 1); _scrollAcc[enc] -= 1; }
    while (_scrollAcc[enc] <= -1) { s->enc((uint32_t)enc, -1); _scrollAcc[enc] += 1; }
}

@end
