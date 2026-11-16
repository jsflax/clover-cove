#import <UIKit/UIKit.h>
#import <Metal/Metal.h>
#import <QuartzCore/QuartzCore.h>
#import <AVFoundation/AVFoundation.h>
#import <GameController/GameController.h>
#include "../engine96k/eng_app.h"
#include "rhi_metal.h"

static GamepadState poll_ios_gamepad(uint32_t *prev_held) {
    GamepadState gs = {0};
    for (GCController *c in [GCController controllers]) {
        GCExtendedGamepad *gp = c.extendedGamepad;
        if (gp) {
            gs.connected = true;
            float lx = eng_deadzone(gp.leftThumbstick.xAxis.value, 0.14f);
            float ly = eng_deadzone(gp.leftThumbstick.yAxis.value, 0.14f);
            float rx = eng_deadzone(gp.rightThumbstick.xAxis.value, 0.14f);
            float ry = eng_deadzone(gp.rightThumbstick.yAxis.value, 0.14f);
            if (fabsf(lx) > fabsf(gs.lx)) gs.lx = lx;
            if (fabsf(ly) > fabsf(gs.ly)) gs.ly = ly;
            if (fabsf(rx) > fabsf(gs.rx)) gs.rx = rx;
            if (fabsf(ry) > fabsf(gs.ry)) gs.ry = ry;
            if (gp.leftTrigger.value > gs.lt)  gs.lt = gp.leftTrigger.value;
            if (gp.rightTrigger.value > gs.rt) gs.rt = gp.rightTrigger.value;
            uint32_t h = 0;
            if (gp.buttonA.pressed) h |= GP_BTN_A;
            if (gp.buttonB.pressed) h |= GP_BTN_B;
            if (gp.buttonX.pressed) h |= GP_BTN_X;
            if (gp.buttonY.pressed) h |= GP_BTN_Y;
            if (gp.leftShoulder.pressed)  h |= GP_BTN_LB;
            if (gp.rightShoulder.pressed) h |= GP_BTN_RB;
            if (gp.buttonOptions && gp.buttonOptions.pressed) h |= GP_BTN_VIEW;
            if (gp.buttonMenu.pressed)    h |= GP_BTN_MENU;
            if (gp.leftThumbstickButton && gp.leftThumbstickButton.pressed)   h |= GP_BTN_L3;
            if (gp.rightThumbstickButton && gp.rightThumbstickButton.pressed) h |= GP_BTN_R3;
            if (gp.dpad.up.pressed)    h |= GP_BTN_DPAD_U;
            if (gp.dpad.down.pressed)  h |= GP_BTN_DPAD_D;
            if (gp.dpad.left.pressed)  h |= GP_BTN_DPAD_L;
            if (gp.dpad.right.pressed) h |= GP_BTN_DPAD_R;
            gs.held |= h;
        }
    }
    gs.pressed = gs.held & ~(*prev_held);
    *prev_held = gs.held;
    return gs;
}

@interface ClipMetalView : UIView
@end
@implementation ClipMetalView
+ (Class)layerClass { return [CAMetalLayer class]; }
@end

@interface ClipViewController : GCEventViewController {
    CAMetalLayer *_metalLayer;
    id<MTLDevice> _dev;
    MetalRHI *_rhi;
    GameState *_game;
    RenderPacket *_pkt;
    WorldGeometry _geo;
    CADisplayLink *_link;
    double _lastTime;
    UITouch * __unsafe_unretained _touchSlots[8];
    uint32_t _prevPadHeld;
    AVAudioEngine *_audioEngine;
    AVAudioSourceNode *_sourceNode;
}
@end

@implementation ClipViewController

- (UIInterfaceOrientationMask)supportedInterfaceOrientations {
    return UIInterfaceOrientationMaskLandscape;
}
- (BOOL)prefersStatusBarHidden { return YES; }
- (UIRectEdge)preferredScreenEdgesDeferringSystemGestures { return UIRectEdgeBottom; }

- (void)loadView {
    ClipMetalView *v = [[ClipMetalView alloc] initWithFrame:CGRectMake(0, 0, 844, 390)];
    v.multipleTouchEnabled = YES;
    v.contentScaleFactor = 3.0;
    v.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    self.view = v;
}

- (void)viewDidLayoutSubviews {
    [super viewDidLayoutSubviews];
    UIScreen *scr = self.view.window.windowScene.screen;
    if (scr && scr.nativeScale >= 1.0) {
        self.view.contentScaleFactor = scr.nativeScale;
    }
}

- (void)viewDidLoad {
    [super viewDidLoad];
    self.controllerUserInteractionEnabled = NO;

    _dev = MTLCreateSystemDefaultDevice();
    _metalLayer = (CAMetalLayer *)self.view.layer;
    _metalLayer.device = _dev;
    _metalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    _metalLayer.opaque = YES;

    _game = (GameState *)calloc(1, app_state_size());
    _pkt  = (RenderPacket *)calloc(1, sizeof(RenderPacket));
    audio_init();
    app_init(_game, true);
    app_build_world(_game, &_geo);
    _rhi = rhi_metal_create(_dev, &_geo, MTLPixelFormatBGRA8Unorm);

    [self setupOrRestartAudio];

    NSNotificationCenter *nc = [NSNotificationCenter defaultCenter];
    [nc addObserver:self selector:@selector(onAudioRouteOrActive) name:UIApplicationDidBecomeActiveNotification object:nil];
    [nc addObserver:self selector:@selector(clearAllTouches) name:UIApplicationWillResignActiveNotification object:nil];
    [nc addObserver:self selector:@selector(onAudioRouteOrActive) name:AVAudioEngineConfigurationChangeNotification object:nil];
    [nc addObserver:self selector:@selector(onAudioRouteOrActive) name:AVAudioSessionInterruptionNotification object:nil];

    _lastTime = CACurrentMediaTime();
    _link = [CADisplayLink displayLinkWithTarget:self selector:@selector(onFrame:)];
    [_link addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSRunLoopCommonModes];
}

- (void)setupOrRestartAudio {
    AVAudioSession *sess = [AVAudioSession sharedInstance];
    [sess setCategory:AVAudioSessionCategoryPlayback withOptions:AVAudioSessionCategoryOptionMixWithOthers error:nil];
    [sess setActive:YES error:nil];

    if (_audioEngine) {
        [_audioEngine stop];
    }
    _audioEngine = [AVAudioEngine new];
    double sr = [_audioEngine.outputNode inputFormatForBus:0].sampleRate;
    if (sr < 8000.0) sr = sess.sampleRate > 8000.0 ? sess.sampleRate : 48000.0;
    int render_sr = (int)(sr + 0.5);

    AVAudioFormat *fmt = [[AVAudioFormat alloc] initStandardFormatWithSampleRate:sr channels:2];
    _sourceNode = [[AVAudioSourceNode alloc] initWithFormat:fmt renderBlock:^OSStatus(BOOL *isSilence, const AudioTimeStamp *ts, AVAudioFrameCount fc, AudioBufferList *abl) {
        (void)ts;
        *isSilence = NO;
        static float buf[4096 * 2];
        uint32_t n = fc > 4096 ? 4096 : fc;
        audio_render_frames(buf, (int)n, render_sr);
        if (abl->mNumberBuffers >= 2) {
            float *outL = (float *)abl->mBuffers[0].mData;
            float *outR = (float *)abl->mBuffers[1].mData;
            for (uint32_t i = 0; i < n; i++) { outL[i] = buf[i * 2]; outR[i] = buf[i * 2 + 1]; }
        } else if (abl->mNumberBuffers == 1) {
            memcpy(abl->mBuffers[0].mData, buf, n * 2 * sizeof(float));
        }
        return noErr;
    }];
    [_audioEngine attachNode:_sourceNode];
    [_audioEngine connect:_sourceNode to:_audioEngine.mainMixerNode format:fmt];
    [_audioEngine prepare];
    [_audioEngine startAndReturnError:nil];
}

- (void)onAudioRouteOrActive {
    if (!_audioEngine || !_audioEngine.isRunning) [self setupOrRestartAudio];
}

- (void)clearAllTouches {
    for (int i = 0; i < 8; i++) {
        if (_touchSlots[i] != nil) {
            _touchSlots[i] = nil;
            app_on_touch(_game, i, TOUCH_UP, 0.0f, 0.0f);
        }
    }
}

- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    (void)event;
    if (!_audioEngine || !_audioEngine.isRunning) [self setupOrRestartAudio];
    CGFloat sc = self.view.contentScaleFactor;
    for (UITouch *t in touches) {
        int slot = -1;
        for (int i = 0; i < 8; i++) {
            if (_touchSlots[i] == t) { slot = i; break; }
        }
        if (slot < 0) {
            for (int i = 0; i < 8; i++) {
                if (_touchSlots[i] == nil) { _touchSlots[i] = t; slot = i; break; }
            }
        }
        if (slot >= 0) {
            CGPoint p = [t locationInView:self.view];
            app_on_touch(_game, slot, TOUCH_DOWN, (float)(p.x * sc), (float)(p.y * sc));
        }
    }
}

- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    (void)event;
    CGFloat sc = self.view.contentScaleFactor;
    for (UITouch *t in touches) {
        for (int i = 0; i < 8; i++) {
            if (_touchSlots[i] == t) {
                CGPoint p = [t locationInView:self.view];
                app_on_touch(_game, i, TOUCH_MOVE, (float)(p.x * sc), (float)(p.y * sc));
                break;
            }
        }
    }
}

- (void)endTouches:(NSSet<UITouch *> *)touches {
    CGFloat sc = self.view.contentScaleFactor;
    for (UITouch *t in touches) {
        for (int i = 0; i < 8; i++) {
            if (_touchSlots[i] == t) {
                CGPoint p = [t locationInView:self.view];
                _touchSlots[i] = nil;
                app_on_touch(_game, i, TOUCH_UP, (float)(p.x * sc), (float)(p.y * sc));
                break;
            }
        }
    }
}

- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    (void)event;
    [self endTouches:touches];
}

- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    (void)event;
    [self endTouches:touches];
}

- (void)onFrame:(CADisplayLink *)link {
    CGFloat sc = self.view.contentScaleFactor;
    CGSize b = self.view.bounds.size;
    CGSize dsz = CGSizeMake(b.width * sc, b.height * sc);
    if (dsz.width < 64 || dsz.height < 64) return;
    _metalLayer.drawableSize = dsz;
    UIEdgeInsets sa = self.view.safeAreaInsets;
    app_resize(_game, (float)dsz.width, (float)dsz.height,
               (float)(sa.top * sc), (float)(sa.bottom * sc), (float)(sa.left * sc), (float)(sa.right * sc));

    double now = link.timestamp;
    float dt = (float)(now - _lastTime);
    _lastTime = now;

    id<CAMetalDrawable> dr = [_metalLayer nextDrawable];
    if (!dr) return;
    EngInput in = {0};
    in.look_axes.z = 1.0f;
    in.pad = poll_ios_gamepad(&_prevPadHeld);
    app_update_and_build_packet(_game, dt, &in, _pkt);
    rhi_metal_render(_rhi, _pkt, dr.texture, dr);
}
@end

@interface ClipSceneDelegate : UIResponder <UIWindowSceneDelegate>
@property (strong, nonatomic) UIWindow *window;
@end

@implementation ClipSceneDelegate
- (void)scene:(UIScene *)scene willConnectToSession:(UISceneSession *)session options:(UISceneConnectionOptions *)opts {
    (void)session; (void)opts;
    if (![scene isKindOfClass:[UIWindowScene class]]) return;
    self.window = [[UIWindow alloc] initWithWindowScene:(UIWindowScene *)scene];
    self.window.rootViewController = [ClipViewController new];
    [self.window makeKeyAndVisible];
}
@end

@interface ClipAppDelegate : UIResponder <UIApplicationDelegate>
@property (strong, nonatomic) UIWindow *window;
@end

@implementation ClipAppDelegate
- (BOOL)application:(UIApplication *)app didFinishLaunchingWithOptions:(NSDictionary *)opts {
    (void)app; (void)opts;
    return YES;
}
@end

int main(int argc, char *argv[]) {
    @autoreleasepool {
        return UIApplicationMain(argc, argv, nil, NSStringFromClass([ClipAppDelegate class]));
    }
}
