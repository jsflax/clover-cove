#include "../engine96k/eng_app.h"
#include "../engine96k/eng_mesh.h"
#include "rhi_gles.h"

#include <android/native_activity.h>
#include <android/native_window.h>
#include <android/input.h>
#include <android/keycodes.h>
#include <android/log.h>
#include <aaudio/AAudio.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

typedef struct {
    pthread_mutex_t mtx;
    pthread_t thread;
    bool running;
    bool paused;

    ANativeWindow *pending_win;
    ANativeWindow *active_win;
    AInputQueue   *pending_iq;
    AInputQueue   *active_iq;

    EGLDisplay dpy;
    EGLConfig  cfg;
    EGLContext ctx;
    EGLSurface surf;

    AAudioStream *audio_stream;
    GameState *game;
    RenderPacket *pkt;
    WorldGeometry geo;
    GlesRHI *rhi;

    // Multitouch gesture state
    float down_x, down_y;
    float last_x, last_y;
    float last_pinch_dist;
    bool moved_far;
    int prev_ptrs;

    // Gamepad / Xbox Bluetooth state
    GamepadState pad_live;
    uint32_t key_held_mask;
    uint32_t hat_held_mask;
    uint32_t prev_pad_held;
} AndroidApp;

static double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static aaudio_data_callback_result_t aaudio_cb(AAudioStream *stream, void *user, void *audioData, int32_t numFrames) {
    (void)user;
    int32_t sr = AAudioStream_getSampleRate(stream);
    audio_render_frames((float *)audioData, (int)numFrames, sr > 0 ? sr : 48000);
    return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

static void init_aaudio(AndroidApp *app) {
    AAudioStreamBuilder *b = NULL;
    if (AAudio_createStreamBuilder(&b) != AAUDIO_OK) return;
    AAudioStreamBuilder_setFormat(b, AAUDIO_FORMAT_PCM_FLOAT);
    AAudioStreamBuilder_setChannelCount(b, 2);
    AAudioStreamBuilder_setSampleRate(b, 48000);
    AAudioStreamBuilder_setPerformanceMode(b, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
    AAudioStreamBuilder_setDataCallback(b, aaudio_cb, app);
    if (AAudioStreamBuilder_openStream(b, &app->audio_stream) == AAUDIO_OK) {
        AAudioStream_requestStart(app->audio_stream);
    }
    AAudioStreamBuilder_delete(b);
}

static void init_egl_context(AndroidApp *app) {
    app->dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(app->dpy, NULL, NULL);
    const EGLint attribs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,
        EGL_NONE
    };
    EGLint num = 0;
    eglChooseConfig(app->dpy, attribs, &app->cfg, 1, &num);
    const EGLint ctx_attr[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
    app->ctx = eglCreateContext(app->dpy, app->cfg, EGL_NO_CONTEXT, ctx_attr);
}

static uint32_t map_android_btn(int32_t kc) {
    switch (kc) {
        case AKEYCODE_BUTTON_A:
        case AKEYCODE_DPAD_CENTER:   return GP_BTN_A;
        case AKEYCODE_BUTTON_B:      return GP_BTN_B;
        case AKEYCODE_BUTTON_X:      return GP_BTN_X;
        case AKEYCODE_BUTTON_Y:      return GP_BTN_Y;
        case AKEYCODE_BUTTON_L1:     return GP_BTN_LB;
        case AKEYCODE_BUTTON_R1:     return GP_BTN_RB;
        case AKEYCODE_BUTTON_SELECT: return GP_BTN_VIEW;
        case AKEYCODE_BUTTON_START:
        case AKEYCODE_MENU:          return GP_BTN_MENU;
        case AKEYCODE_BUTTON_THUMBL: return GP_BTN_L3;
        case AKEYCODE_BUTTON_THUMBR: return GP_BTN_R3;
        case AKEYCODE_DPAD_UP:       return GP_BTN_DPAD_U;
        case AKEYCODE_DPAD_DOWN:     return GP_BTN_DPAD_D;
        case AKEYCODE_DPAD_LEFT:     return GP_BTN_DPAD_L;
        case AKEYCODE_DPAD_RIGHT:    return GP_BTN_DPAD_R;
        default:                     return 0;
    }
}

static void handle_input(AndroidApp *app) {
    if (!app->active_iq) return;
    AInputEvent *ev = NULL;
    while (AInputQueue_getEvent(app->active_iq, &ev) >= 0) {
        if (AInputQueue_preDispatchEvent(app->active_iq, ev)) continue;
        int handled = 0;
        int32_t etype = AInputEvent_getType(ev);
        int32_t src   = AInputEvent_getSource(ev);

        if (etype == AINPUT_EVENT_TYPE_KEY) {
            int32_t kc  = AKeyEvent_getKeyCode(ev);
            int32_t act = AKeyEvent_getAction(ev);
            uint32_t bit = map_android_btn(kc);
            if (bit != 0) {
                app->pad_live.connected = true;
                if (act == AKEY_EVENT_ACTION_DOWN) app->key_held_mask |= bit;
                else if (act == AKEY_EVENT_ACTION_UP) app->key_held_mask &= ~bit;
                handled = 1;
            }
        } else if (etype == AINPUT_EVENT_TYPE_MOTION) {
            if ((src & AINPUT_SOURCE_CLASS_JOYSTICK) != 0) {
                app->pad_live.connected = true;
                float lx = eng_deadzone(AMotionEvent_getAxisValue(ev, AMOTION_EVENT_AXIS_X, 0), 0.14f);
                float ly = eng_deadzone(-AMotionEvent_getAxisValue(ev, AMOTION_EVENT_AXIS_Y, 0), 0.14f);
                float rx_z = AMotionEvent_getAxisValue(ev, AMOTION_EVENT_AXIS_Z, 0);
                float rx_r = AMotionEvent_getAxisValue(ev, AMOTION_EVENT_AXIS_RX, 0);
                float ry_z = -AMotionEvent_getAxisValue(ev, AMOTION_EVENT_AXIS_RZ, 0);
                float ry_r = -AMotionEvent_getAxisValue(ev, AMOTION_EVENT_AXIS_RY, 0);
                float rx = eng_deadzone(fabsf(rx_z) >= fabsf(rx_r) ? rx_z : rx_r, 0.14f);
                float ry = eng_deadzone(fabsf(ry_z) >= fabsf(ry_r) ? ry_z : ry_r, 0.14f);
                float lt = fmaxf(AMotionEvent_getAxisValue(ev, AMOTION_EVENT_AXIS_LTRIGGER, 0),
                                 AMotionEvent_getAxisValue(ev, AMOTION_EVENT_AXIS_BRAKE, 0));
                float rt = fmaxf(AMotionEvent_getAxisValue(ev, AMOTION_EVENT_AXIS_RTRIGGER, 0),
                                 AMotionEvent_getAxisValue(ev, AMOTION_EVENT_AXIS_GAS, 0));
                float hx = AMotionEvent_getAxisValue(ev, AMOTION_EVENT_AXIS_HAT_X, 0);
                float hy = AMotionEvent_getAxisValue(ev, AMOTION_EVENT_AXIS_HAT_Y, 0);
                app->pad_live.lx = lx; app->pad_live.ly = ly;
                app->pad_live.rx = rx; app->pad_live.ry = ry;
                app->pad_live.lt = clampf(lt, 0.0f, 1.0f);
                app->pad_live.rt = clampf(rt, 0.0f, 1.0f);
                uint32_t hm = 0;
                if (hx < -0.5f) hm |= GP_BTN_DPAD_L;
                if (hx >  0.5f) hm |= GP_BTN_DPAD_R;
                if (hy < -0.5f) hm |= GP_BTN_DPAD_U;
                if (hy >  0.5f) hm |= GP_BTN_DPAD_D;
                app->hat_held_mask = hm;
                handled = 1;
            } else {
                int32_t full_act = AMotionEvent_getAction(ev);
                int32_t act  = full_act & AMOTION_EVENT_ACTION_MASK;
                int32_t pidx = (full_act & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
                int ptrs = (int)AMotionEvent_getPointerCount(ev);

                if (act == AMOTION_EVENT_ACTION_DOWN || act == AMOTION_EVENT_ACTION_POINTER_DOWN) {
                    if (pidx >= 0 && pidx < ptrs) {
                        int pid = AMotionEvent_getPointerId(ev, (size_t)pidx) & 7;
                        app_on_touch(app->game, pid, TOUCH_DOWN, AMotionEvent_getX(ev, (size_t)pidx), AMotionEvent_getY(ev, (size_t)pidx));
                    }
                } else if (act == AMOTION_EVENT_ACTION_MOVE) {
                    for (int i = 0; i < ptrs; i++) {
                        int pid = AMotionEvent_getPointerId(ev, (size_t)i) & 7;
                        app_on_touch(app->game, pid, TOUCH_MOVE, AMotionEvent_getX(ev, (size_t)i), AMotionEvent_getY(ev, (size_t)i));
                    }
                } else if (act == AMOTION_EVENT_ACTION_POINTER_UP) {
                    if (pidx >= 0 && pidx < ptrs) {
                        int pid = AMotionEvent_getPointerId(ev, (size_t)pidx) & 7;
                        app_on_touch(app->game, pid, TOUCH_UP, AMotionEvent_getX(ev, (size_t)pidx), AMotionEvent_getY(ev, (size_t)pidx));
                    }
                } else if (act == AMOTION_EVENT_ACTION_UP || act == AMOTION_EVENT_ACTION_CANCEL) {
                    for (int i = 0; i < ptrs; i++) {
                        int pid = AMotionEvent_getPointerId(ev, (size_t)i) & 7;
                        app_on_touch(app->game, pid, TOUCH_UP, AMotionEvent_getX(ev, (size_t)i), AMotionEvent_getY(ev, (size_t)i));
                    }
                }
                handled = 1;
            }
        }
        AInputQueue_finishEvent(app->active_iq, ev, handled);
    }
}

static void *render_thread_main(void *arg) {
    AndroidApp *app = (AndroidApp *)arg;
    init_egl_context(app);
    audio_init();
    init_aaudio(app);

    app->game = (GameState *)calloc(1, app_state_size());
    app->pkt  = (RenderPacket *)calloc(1, sizeof(RenderPacket));
    app_init(app->game, false); // OpenGL ES [-1..1] NDC Z
    app_build_world(app->game, &app->geo);

    double prev_t = now_sec();
    while (app->running) {
        pthread_mutex_lock(&app->mtx);
        if (app->pending_iq != app->active_iq) app->active_iq = app->pending_iq;
        if (app->pending_win != app->active_win) {
            if (app->surf != EGL_NO_SURFACE) {
                eglMakeCurrent(app->dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
                eglDestroySurface(app->dpy, app->surf);
                app->surf = EGL_NO_SURFACE;
            }
            app->active_win = app->pending_win;
            if (app->active_win) {
                EGLint fmt = 0;
                eglGetConfigAttrib(app->dpy, app->cfg, EGL_NATIVE_VISUAL_ID, &fmt);
                ANativeWindow_setBuffersGeometry(app->active_win, 0, 0, fmt);
                app->surf = eglCreateWindowSurface(app->dpy, app->cfg, app->active_win, NULL);
                eglMakeCurrent(app->dpy, app->surf, app->surf, app->ctx);
                if (!app->rhi) app->rhi = rhi_gles_create(&app->geo);
            }
        }
        bool paused = app->paused;
        pthread_mutex_unlock(&app->mtx);

        handle_input(app);

        if (app->surf != EGL_NO_SURFACE && !paused && app->rhi) {
            EGLint w = 0, h = 0;
            eglQuerySurface(app->dpy, app->surf, EGL_WIDTH, &w);
            eglQuerySurface(app->dpy, app->surf, EGL_HEIGHT, &h);
            if (w > 32 && h > 32) {
                app_resize(app->game, (float)w, (float)h, 0.0f, 0.0f, 36.0f, 36.0f);
                double t = now_sec();
                float dt = (float)(t - prev_t);
                prev_t = t;

                EngInput in = {0};
                in.look_axes.z = 1.0f;
                app->pad_live.held = app->key_held_mask | app->hat_held_mask;
                app->pad_live.pressed = app->pad_live.held & ~app->prev_pad_held;
                app->prev_pad_held = app->pad_live.held;
                in.pad = app->pad_live;

                app_update_and_build_packet(app->game, dt, &in, app->pkt);
                rhi_gles_render(app->rhi, app->pkt, w, h);
                eglSwapBuffers(app->dpy, app->surf);
            }
        } else {
            prev_t = now_sec();
            usleep(16000);
        }
    }

    if (app->audio_stream) {
        AAudioStream_requestStop(app->audio_stream);
        AAudioStream_close(app->audio_stream);
    }
    if (app->rhi) rhi_gles_destroy(app->rhi);
    world_free_geo(&app->geo);
    free(app->pkt);
    free(app->game);
    return NULL;
}

static void on_win_created(ANativeActivity *a, ANativeWindow *w) {
    AndroidApp *app = (AndroidApp *)a->instance;
    pthread_mutex_lock(&app->mtx); app->pending_win = w; pthread_mutex_unlock(&app->mtx);
}
static void on_win_destroyed(ANativeActivity *a, ANativeWindow *w) {
    (void)w;
    AndroidApp *app = (AndroidApp *)a->instance;
    pthread_mutex_lock(&app->mtx); app->pending_win = NULL; pthread_mutex_unlock(&app->mtx);
}
static void on_iq_created(ANativeActivity *a, AInputQueue *q) {
    AndroidApp *app = (AndroidApp *)a->instance;
    pthread_mutex_lock(&app->mtx); app->pending_iq = q; pthread_mutex_unlock(&app->mtx);
}
static void on_iq_destroyed(ANativeActivity *a, AInputQueue *q) {
    (void)q;
    AndroidApp *app = (AndroidApp *)a->instance;
    pthread_mutex_lock(&app->mtx); app->pending_iq = NULL; pthread_mutex_unlock(&app->mtx);
}
static void on_pause(ANativeActivity *a) {
    AndroidApp *app = (AndroidApp *)a->instance;
    pthread_mutex_lock(&app->mtx); app->paused = true; pthread_mutex_unlock(&app->mtx);
    if (app->audio_stream) AAudioStream_requestPause(app->audio_stream);
}
static void on_resume(ANativeActivity *a) {
    AndroidApp *app = (AndroidApp *)a->instance;
    pthread_mutex_lock(&app->mtx); app->paused = false; pthread_mutex_unlock(&app->mtx);
    if (app->audio_stream) AAudioStream_requestStart(app->audio_stream);
}
static void on_destroy(ANativeActivity *a) {
    AndroidApp *app = (AndroidApp *)a->instance;
    app->running = false;
    pthread_join(app->thread, NULL);
    pthread_mutex_destroy(&app->mtx);
    free(app);
}

__attribute__((visibility("default")))
void ANativeActivity_onCreate(ANativeActivity *activity, void *savedState, size_t savedStateSize) {
    (void)savedState; (void)savedStateSize;
    AndroidApp *app = (AndroidApp *)calloc(1, sizeof(AndroidApp));
    pthread_mutex_init(&app->mtx, NULL);
    app->running = true;
    app->surf = EGL_NO_SURFACE;
    activity->instance = app;

    activity->callbacks->onNativeWindowCreated   = on_win_created;
    activity->callbacks->onNativeWindowDestroyed = on_win_destroyed;
    activity->callbacks->onInputQueueCreated     = on_iq_created;
    activity->callbacks->onInputQueueDestroyed   = on_iq_destroyed;
    activity->callbacks->onPause                 = on_pause;
    activity->callbacks->onResume                = on_resume;
    activity->callbacks->onDestroy               = on_destroy;

    pthread_create(&app->thread, NULL, render_thread_main, app);
}
