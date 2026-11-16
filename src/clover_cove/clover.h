#ifndef CLOVER_COVE_H
#define CLOVER_COVE_H

#include "../engine96k/eng_app.h"
#include "../engine96k/eng_mesh.h"
#include "../engine96k/eng_ui.h"

#define CLOVER_GRASS_COUNT  5400
#define MAX_TREES           14
#define MAX_DROPS           28
#define MAX_ROCKS           5
#define MAX_DIG_SPOTS       5
#define MAX_FISH            7
#define MAX_BUGS            8
#define MAX_DECORS          12
#define MAX_NPCS            3
#define MAX_POCKETS         12
#define WORLD_CURVE_K       0.0115f

enum {
    MESH_CHAR_PLAYER = ENG_MESH_GAME_START,
    MESH_CHAR_MOCHI,
    MESH_CHAR_PUDDLES,
    MESH_CHAR_BISCUIT,
    MESH_SHADOW_BLOB,
    MESH_TREE_OAK,
    MESH_TREE_PALM,
    MESH_FRUIT_APPLE,
    MESH_FRUIT_ORANGE,
    MESH_ROCK_NODE,
    MESH_DIG_CRACK,
    MESH_DIG_HOLE,
    MESH_FOSSIL,
    MESH_COIN_BAG,
    MESH_SEASHELL,
    MESH_FISH_SHADOW,
    MESH_CAUGHT_FISH,
    MESH_BUTTERFLY,
    MESH_BOBBER,
    MESH_TOOL_ROD,
    MESH_TOOL_NET,
    MESH_TOOL_SHOVEL,
    MESH_TOOL_AXE,
    MESH_DECOR_LANTERN,
    MESH_DECOR_CAMPFIRE,
    MESH_DECOR_BENCH,
};

enum {
    BONE_ROOT   = 0,
    BONE_PELVIS = 1,
    BONE_SPINE  = 2,
    BONE_CHEST  = 3,
    BONE_HEAD   = 4,
    BONE_ARM_L  = 5,
    BONE_HAND_L = 6,
    BONE_ARM_R  = 7,
    BONE_HAND_R = 8,
    BONE_LEG_L  = 9,
    BONE_FOOT_L = 10,
    BONE_LEG_R  = 11,
    BONE_FOOT_R = 12,
    BONE_TAIL_1 = 13,
    BONE_TAIL_2 = 14,
    BONE_WEAPON = 15,
};

enum {
    CANIM_IDLE     = 0,
    CANIM_WALK     = 1,
    CANIM_RUN      = 2,
    CANIM_SWING    = 3, // Net / Shovel / Axe swing
    CANIM_SHAKE    = 4, // Shake tree / pick up
    CANIM_FISH_WAIT= 5, // Holding rod waiting for bite
    CANIM_SHOWOFF  = 6, // Celebratory "I caught a...!" hold-up pose
    CANIM_WAVE     = 7, // Happy villager greeting wave
};

enum {
    TOOL_HAND   = 0,
    TOOL_ROD    = 1,
    TOOL_NET    = 2,
    TOOL_SHOVEL = 3,
    TOOL_AXE    = 4,
    TOOL_COUNT  = 5,
};

enum {
    ITEM_NONE      = 0,
    ITEM_APPLE     = 1,
    ITEM_ORANGE    = 2,
    ITEM_SEASHELL  = 3,
    ITEM_FOSSIL    = 4,
    ITEM_COIN_BAG  = 5,
    ITEM_FISH      = 6,
    ITEM_BUTTERFLY = 7,
};

enum {
    TOD_MORNING = 0,
    TOD_NOON    = 1,
    TOD_SUNSET  = 2,
    TOD_NIGHT   = 3,
};

enum {
    FISH_IDLE     = 0,
    FISH_CASTING  = 1,
    FISH_WAITING  = 2,
    FISH_NIBBLE   = 3,
    FISH_BITE     = 4, // SPLASH! Press A now!
};

typedef struct {
    uint8_t anim;
    float   anim_time;
    float   move_speed; // 0..1 normalized
    float   tail_phase;
    bool    has_tail;
} ChibiAnim;

typedef struct {
    bool    active;
    bool    is_palm;
    v3      pos;
    float   scale;
    float   yaw;
    uint8_t fruit_kind;  // ITEM_APPLE or ITEM_ORANGE
    uint8_t fruit_count; // 0..3 hanging fruits
    float   regrow_timer;
    float   shake_timer;
} IslandTree;

typedef struct {
    bool    active;
    uint8_t item_kind;
    v3      pos;
    v3      vel;
    float   yaw;
    float   bob_phase;
} GroundDrop;

typedef struct {
    v3      pos;
    float   scale;
    float   yaw;
    uint8_t hits_left;
    float   hit_shake;
    float   reset_timer;
} IslandRock;

typedef struct {
    bool    active;
    bool    dug;
    v3      pos;
    float   reset_timer;
} DigSpot;

typedef struct {
    bool    active;
    v3      pos;
    v3      home_pos;
    float   yaw;
    float   speed;
    float   wander_t;
    uint8_t species; // 0=Sea Bass, 1=Red Snapper, 2=Koi
    bool    in_pond;
} SwimmingFish;

typedef struct {
    bool    active;
    v3      pos;
    v3      home_pos;
    float   yaw;
    float   flap_phase;
    uint8_t hue_variant; // 0=Monarch Gold, 1=Emperor Azure, 2=Blossom Pink
    float   respawn_t;
} IslandBug;

typedef struct {
    bool    active;
    uint8_t kind; // 0=Lantern, 1=Campfire, 2=Garden Bench
    v3      pos;
    float   yaw;
} PlacedDecor;

typedef struct {
    const char *name;
    const char *catchphrase;
    uint32_t    mesh_id;
    float       eye_hue;
    uint8_t     accent_r, accent_g, accent_b;
    uint8_t     cloth_r, cloth_g, cloth_b;
    v3          pos;
    v3          home_pos;
    v3          goal_pos;
    float       yaw;
    float       pause_t;
    uint8_t     anim;
    float       anim_t;
} VillagerNpc;

struct GameState {
    bool     initialized;
    bool     zero_to_one_z;
    bool     ctrl_invert_y;   // Defaults to TRUE (ON) per user preference!
    bool     audio_muted;
    float    screen_w, screen_h;
    float    ui_scale;
    float    safe_top, safe_bottom, safe_left, safe_right;
    float    time_sec;
    uint32_t rng;

    // Player Villager
    v3       player_pos;
    float    player_yaw;
    float    player_vel_mag;
    bool     sprinting;
    uint8_t  player_anim;
    float    player_anim_t;
    float    action_lock_t;
    float    step_dust_cd;
    uint8_t  equipped_tool;
    uint8_t  next_decor_kind;

    // Tap-to-walk / independent dual-thumbstick multitouch state
    bool     has_walk_target;
    bool     auto_act_on_arrive;
    v3       walk_target;
    bool     touch_stick_active;
    float    touch_stick_ox, touch_stick_oy;
    float    touch_stick_dx, touch_stick_dy;
    bool     right_stick_active;
    float    right_stick_ox, right_stick_oy;
    float    right_stick_dx, right_stick_dy;
    int      left_stick_ptr;
    int      right_stick_ptr;
    struct {
        bool    down;
        bool    moved;
        uint8_t role; // 0=none, 1=hud_btn, 2=left_stick, 3=right_stick, 4=extra_cam
        float   sx0, sy0;
        float   sx, sy;
    } ptrs[8];
    float    pinch_prev_dist;
    bool     drag_occurred;

    // Economy & Pockets
    int      coins;
    uint8_t  pockets[MAX_POCKETS];
    int      pocket_count;

    // Fishing State Machine
    uint8_t  fish_state;
    v3       bobber_pos;
    v3       bobber_start;
    float    fish_timer;
    int      hooked_fish_idx;
    int      nibbles_left;

    // Show-off Catch Banner & Villager Dialogue Bubble
    float       showoff_timer;
    uint8_t     showoff_item;
    const char *showoff_title;
    const char *showoff_pun;

    bool        dialog_open;
    int         dialog_npc_idx; // -1 = Cove Market Stall
    const char *dialog_speaker;
    const char *dialog_line1;
    const char *dialog_line2;
    int         dialog_char_revealed;
    float       dialog_char_t;

    // HUD Toast
    float       toast_timer;
    const char *toast_msg;

    // Time of Day (0=Morning, 1=Noon, 2=Sunset, 3=Starry Night)
    uint8_t  tod_preset;
    float    tod_blend;       // Smoothly interpolated 0..3

    // Living Island Entities
    IslandTree   trees[MAX_TREES];
    int          tree_count;
    GroundDrop   drops[MAX_DROPS];
    IslandRock   rocks[MAX_ROCKS];
    DigSpot      digs[MAX_DIG_SPOTS];
    SwimmingFish fish[MAX_FISH];
    IslandBug    bugs[MAX_BUGS];
    PlacedDecor  decors[MAX_DECORS];
    VillagerNpc  npcs[MAX_NPCS];

    // Context Target Highlight
    bool        has_context_hint;
    v3          context_pos;
    const char *context_label;

    // Camera (Cozy Diorama Follow-Orbit + 6-DOF Free-Cam)
    bool     free_cam;
    v3       cam_focus;
    float    cam_yaw;
    float    cam_pitch;
    float    cam_dist;
    v3       free_pos;
    float    free_yaw;
    float    free_pitch;
    v3       cur_eye;
    v3       cur_target;
    float    cur_fov;

    // VFX Particles (leaf bursts, water splashes, chimney smoke, sea-breeze wisps, sparkles)
    struct {
        bool    active;
        uint8_t kind;
        v3      pos;
        v3      vel;
        v3      axis;
        float   age, life;
        float   size;
        uint8_t rgba[4];
    } particles[256];
};

// Shared Island Terrain Queries (clover_mesh.c)
float clover_ground_y(float wx, float wz);
bool  clover_is_water_at(float wx, float wz, float *out_water_y, bool *out_is_pond);
bool  clover_is_walkable(float wx, float wz);

// Chibi 16-Bone Rig Evaluator (clover_anim.c)
void clover_compute_bones(const ChibiAnim *ca, float time_sec, m4 out_bones[MAX_BONES]);

// Cozy Acoustic Guitar + Marimba + Ocean Surf + Formant Speech Synth (clover_audio.c)
void clover_audio_sfx(int sfx_id);
void clover_audio_speech(int pitch_seed);
void clover_audio_set_state(int tod_preset, bool muted);

enum {
    CSFX_STEP       = 0,
    CSFX_TOOL_SWAP  = 1,
    CSFX_SHAKE_TREE = 2,
    CSFX_POP_PICKUP = 3,
    CSFX_CAST_ROD   = 4,
    CSFX_NIBBLE     = 5,
    CSFX_SPLASH_BITE= 6,
    CSFX_FANFARE    = 7,
    CSFX_DIG        = 8,
    CSFX_ROCK_CHIME = 9,
    CSFX_COINS_SELL = 10,
    CSFX_NET_SWOOSH = 11,
    CSFX_UI_CHIME   = 12,
};

#endif // CLOVER_COVE_H
