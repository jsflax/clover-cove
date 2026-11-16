#ifndef ENG96K_APP_H
#define ENG96K_APP_H

#include "eng_types.h"

// ============================================================================
// Unified Controller / Xbox Nano / MFi / Android Gamepad State
// Note: Controller vertical look (Invert-Y) defaults to ON.
// ============================================================================
enum {
    GP_BTN_A      = (1u << 0),  // Xbox A (South) — Primary Confirm / Interact / Use Tool
    GP_BTN_B      = (1u << 1),  // Xbox B (East)  — Cancel / Back / Sprint (hold)
    GP_BTN_X      = (1u << 2),  // Xbox X (West)  — Secondary Action / Place Decor
    GP_BTN_Y      = (1u << 3),  // Xbox Y (North) — Market Price Info / Toggle Free-Cam
    GP_BTN_LB     = (1u << 4),  // Left Bumper    — Cycle Tool Left
    GP_BTN_RB     = (1u << 5),  // Right Bumper   — Cycle Tool Right
    GP_BTN_VIEW   = (1u << 6),  // View / Select  — Toggle Free-Cam
    GP_BTN_MENU   = (1u << 7),  // Menu / Start   — Cycle Time-of-Day
    GP_BTN_L3     = (1u << 8),  // Left Stick Click
    GP_BTN_R3     = (1u << 9),  // Right Stick Click — Toggle Controller Invert-Y
    GP_BTN_DPAD_U = (1u << 10),
    GP_BTN_DPAD_D = (1u << 11),
    GP_BTN_DPAD_L = (1u << 12),
    GP_BTN_DPAD_R = (1u << 13),
};

typedef struct {
    bool connected;
    float lx, ly;       // Left stick  [-1..1], +x = Right, +y = Stick Up/Forward
    float rx, ry;       // Right stick [-1..1], +x = Right, +y = Physical Stick Up
    float lt, rt;       // Analog triggers [0..1]
    uint32_t held;      // Currently held button bitmask
    uint32_t pressed;   // Rising-edge buttons pressed THIS frame
} GamepadState;

typedef struct {
    v3 move_axes;       // Keyboard WASD (x = D-A, z = W-S) + QE/UO (y = E-Q)
    v3 look_axes;       // Keyboard IJKL / Arrows (x = L-J, y = I-K, z = Shift boost)
    GamepadState pad;   // Controller state
} EngInput;

typedef struct GameState GameState;

typedef struct {
    const char *window_title;
    const char *shot_names[5];
    int         shot_steps[5];
} AppDesc;

// Game-Implemented Contract
const AppDesc *app_get_desc(void);
size_t         app_state_size(void);

void app_init(GameState *g, bool zero_to_one_z);
void app_build_world(const GameState *g, WorldGeometry *out_geo);
void app_resize(GameState *g, float w, float h, float safe_top, float safe_bottom, float safe_left, float safe_right);
void app_update_and_build_packet(GameState *g, float dt, const EngInput *in, RenderPacket *pkt);

enum {
    TOUCH_DOWN = 0,
    TOUCH_MOVE = 1,
    TOUCH_UP   = 2,
};

void app_on_touch(GameState *g, int ptr_id, int phase, float sx, float sy);
void app_on_tap(GameState *g, float sx, float sy);
void app_on_drag(GameState *g, float dx, float dy, int finger_count, float start_x, float start_y);
void app_on_drag_end(GameState *g);
void app_on_pinch(GameState *g, float scale_delta);
void app_on_hover(GameState *g, float sx, float sy);
void app_on_key_press(GameState *g, int ch);
void app_set_preset_shot(GameState *g, int shot_id);

// Real-time 48kHz Stereo Synth Contract
void audio_init(void);
void audio_render_frames(float *out_lr, int frame_count, int sample_rate);

// Shared deadzone helper for platform hosts
static inline float eng_deadzone(float v, float dz) {
    float a = fabsf(v);
    if (a <= dz) return 0.0f;
    float s = (a - dz) / (1.0f - dz);
    return (v < 0.0f) ? -clampf(s, 0.0f, 1.0f) : clampf(s, 0.0f, 1.0f);
}

#endif // ENG96K_APP_H
