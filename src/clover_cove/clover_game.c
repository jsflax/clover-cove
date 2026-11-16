#include "clover.h"
#include <stdio.h>

static const AppDesc k_clover_desc = {
    .window_title = "Clover Cove — 96KB Cozy Island Life (Engine96k)",
    .shot_names = {
        "shot0_clover_overview.png",
        "shot1_villager_closeup.png",
        "shot2_waterfall_fishing.png",
        "shot3_market_plaza_chat.png",
        "shot4_starry_night_lanterns.png",
    },
    .shot_steps = {28, 28, 24, 28, 32},
};

const AppDesc *app_get_desc(void)   { return &k_clover_desc; }
size_t         app_state_size(void) { return sizeof(GameState); }

static const char *k_tool_names[TOOL_COUNT] = {"HAND", "ROD", "NET", "SHOVEL", "AXE"};
static const char *k_tod_names[4] = {"MORNING", "NOON", "SUNSET", "NIGHT"};

static void show_toast(GameState *g, const char *msg) {
    g->toast_msg = msg;
    g->toast_timer = 2.4f;
}

static void spawn_vfx(GameState *g, uint8_t kind, v3 pos, v3 vel, v3 axis, float life, float size, uint8_t r, uint8_t gr, uint8_t bl, uint8_t a) {
    int slot = -1;
    for (int i = 0; i < 256; i++) {
        if (!g->particles[i].active) { slot = i; break; }
    }
    if (slot < 0) slot = (int)(rng_next(&g->rng) & 255u);
    g->particles[slot].active = true;
    g->particles[slot].kind   = kind;
    g->particles[slot].pos    = pos;
    g->particles[slot].vel    = vel;
    g->particles[slot].axis   = axis;
    g->particles[slot].age    = 0.0f;
    g->particles[slot].life   = life;
    g->particles[slot].size   = size;
    g->particles[slot].rgba[0]= r;
    g->particles[slot].rgba[1]= gr;
    g->particles[slot].rgba[2]= bl;
    g->particles[slot].rgba[3]= a;
}

static void spawn_drop(GameState *g, uint8_t item_kind, v3 origin, v3 vel) {
    int slot = -1;
    for (int i = 0; i < MAX_DROPS; i++) {
        if (!g->drops[i].active) { slot = i; break; }
    }
    if (slot < 0) slot = 0;
    g->drops[slot].active    = true;
    g->drops[slot].item_kind = item_kind;
    g->drops[slot].pos       = origin;
    g->drops[slot].vel       = vel;
    g->drops[slot].yaw       = rng_frange(&g->rng, 0.0f, TAU_F);
    g->drops[slot].bob_phase = rng_frange(&g->rng, 0.0f, TAU_F);
}

static bool add_to_pockets(GameState *g, uint8_t item_kind) {
    if (item_kind == ITEM_COIN_BAG) {
        g->coins += 1000;
        show_toast(g, "+1000 COINS!");
        return true;
    }
    if (g->pocket_count >= MAX_POCKETS) {
        show_toast(g, "POCKETS FULL! SELL AT COVE MARKET!");
        return false;
    }
    g->pockets[g->pocket_count++] = item_kind;
    return true;
}

static int item_coin_value(uint8_t k) {
    switch (k) {
        case ITEM_APPLE:     return 250;
        case ITEM_ORANGE:    return 300;
        case ITEM_SEASHELL:  return 450;
        case ITEM_BUTTERFLY: return 1200;
        case ITEM_FISH:      return 1800;
        case ITEM_FOSSIL:    return 2500;
        default:             return 200;
    }
}

void app_init(GameState *g, bool zero_to_one_z) {
    memset(g, 0, sizeof(*g));
    g->initialized   = true;
    g->zero_to_one_z = zero_to_one_z;
    g->ctrl_invert_y = true; // Default: Inverted vertical controller look ON
    g->screen_w      = 1280.0f;
    g->screen_h      = 800.0f;
    g->ui_scale      = 1.0f;
    g->rng           = 0xC107E96Bu;

    g->player_pos      = v3mk(-0.6f, clover_ground_y(-0.6f, 3.4f), 3.4f);
    g->player_yaw      = 0.0f;
    g->left_stick_ptr  = -1;
    g->right_stick_ptr = -1;
    g->equipped_tool   = TOOL_HAND;
    g->coins           = 1500;
    g->tod_preset      = TOD_MORNING;
    g->tod_blend       = 0.0f;

    g->cam_focus     = g->player_pos;
    g->cam_yaw       = 0.0f;
    g->cam_pitch     = 0.56f; // Cozy ~32-deg diorama view angle
    g->cam_dist      = 10.8f;
    g->cur_fov       = 0.72f;

    // Populate Lush Fruit Trees & Beach Coconut Palms
    static const float tree_coords[MAX_TREES][3] = {
        {-7.6f, -8.2f, 0}, { 0.8f, -8.6f, 0}, { 6.6f, -7.8f, 0}, { 6.8f, -4.6f, 0}, // Clifftop orchard
        {-8.6f,  1.2f, 0}, {-7.2f,  5.4f, 0}, {-4.2f,  7.6f, 0}, { 8.5f,  0.3f, 0}, // Meadow apple/orange trees
        { 8.2f,  5.6f, 0}, { 1.4f,  7.8f, 0},
        {-9.8f,  9.4f, 1}, {-5.4f, 11.4f, 1}, { 7.2f, 10.8f, 1}, {10.4f,  7.6f, 1}, // Golden beach coconut palms!
    };
    g->tree_count = MAX_TREES;
    for (int i = 0; i < MAX_TREES; i++) {
        float tx = tree_coords[i][0], tz = tree_coords[i][1];
        bool palm = (tree_coords[i][2] > 0.5f);
        g->trees[i].active      = true;
        g->trees[i].is_palm     = palm;
        g->trees[i].pos         = v3mk(tx, clover_ground_y(tx, tz) - 0.04f, tz);
        g->trees[i].scale       = rng_frange(&g->rng, 0.92f, 1.08f);
        g->trees[i].yaw         = palm ? rng_frange(&g->rng, 0.0f, TAU_F) : rng_frange(&g->rng, -0.35f, 0.35f);
        g->trees[i].fruit_kind  = (i & 1) ? ITEM_ORANGE : ITEM_APPLE;
        g->trees[i].fruit_count = palm ? 2 : 3;
    }

    // Money Rocks
    static const float rock_xz[MAX_ROCKS][2] = {
        {-6.2f, -5.0f}, { 5.8f, -8.8f}, {-8.8f, -1.8f}, { 6.6f, 7.6f}, {-2.2f, 9.2f}
    };
    for (int i = 0; i < MAX_ROCKS; i++) {
        float rx = rock_xz[i][0], rz = rock_xz[i][1];
        g->rocks[i].pos       = v3mk(rx, clover_ground_y(rx, rz) - 0.04f, rz);
        g->rocks[i].scale     = rng_frange(&g->rng, 0.85f, 1.12f);
        g->rocks[i].yaw       = rng_frange(&g->rng, 0.0f, TAU_F);
        g->rocks[i].hits_left = 5;
    }

    // Star Fossil Dig Spots
    static const float dig_xz[MAX_DIG_SPOTS][2] = {
        { 0.4f, -5.6f}, {-5.6f, 3.2f}, { 6.2f, -1.6f}, { 1.8f, 9.8f}, {-7.4f, 7.4f}
    };
    for (int i = 0; i < MAX_DIG_SPOTS; i++) {
        float dx = dig_xz[i][0], dz = dig_xz[i][1];
        g->digs[i].active = true;
        g->digs[i].dug    = false;
        g->digs[i].pos    = v3mk(dx, clover_ground_y(dx, dz), dz);
    }

    // Beach Seashells ready to Beachcomb!
    static const float shell_xz[5][2] = {
        {-7.6f, 11.8f}, {-3.0f, 12.6f}, { 2.4f, 12.4f}, { 8.8f, 10.2f}, {-11.2f, 6.8f}
    };
    for (int i = 0; i < 5; i++) {
        float sx = shell_xz[i][0], sz = shell_xz[i][1];
        spawn_drop(g, ITEM_SEASHELL, v3mk(sx, clover_ground_y(sx, sz), sz), v3mk(0,0,0));
    }

    // Swimming Fish Shadows (Clifftop Pond, Winding River & Ocean Pier!)
    static const float fish_spots[MAX_FISH][4] = {
        { 3.4f, -6.8f, 1, 2}, // Clifftop Pond Koi
        { 3.8f, -0.6f, 0, 2}, // Waterfall Pool Koi
        { 4.1f,  6.4f, 0, 1}, // Lower River Snapper
        {-1.8f, 16.8f, 0, 0}, // Pier West Sea Bass
        { 2.1f, 17.6f, 0, 1}, // Pier East Red Snapper
        {-7.4f, 14.2f, 0, 0}, // Southwest Cove Sea Bass
        { 9.2f, 13.6f, 0, 1}, // Southeast Cove Red Snapper
    };
    for (int i = 0; i < MAX_FISH; i++) {
        float fx = fish_spots[i][0], fz = fish_spots[i][1];
        bool pond = (fish_spots[i][2] > 0.5f);
        float wy = pond ? 2.18f : (fz < 12.0f ? 0.18f : -0.08f);
        g->fish[i].active   = true;
        g->fish[i].home_pos = v3mk(fx, wy, fz);
        g->fish[i].pos      = g->fish[i].home_pos;
        g->fish[i].yaw      = rng_frange(&g->rng, 0.0f, TAU_F);
        g->fish[i].speed    = rng_frange(&g->rng, 0.45f, 0.85f);
        g->fish[i].species  = (uint8_t)fish_spots[i][3];
        g->fish[i].in_pond  = pond;
    }

    // Fluttering 3D Butterflies across Wildflower Meadows
    for (int i = 0; i < MAX_BUGS; i++) {
        float bx = rng_frange(&g->rng, -7.5f, 7.5f);
        float bz = rng_frange(&g->rng, -7.0f, 8.5f);
        float by = clover_ground_y(bx, bz) + 0.85f;
        g->bugs[i].active      = true;
        g->bugs[i].home_pos    = v3mk(bx, by, bz);
        g->bugs[i].pos         = g->bugs[i].home_pos;
        g->bugs[i].yaw         = rng_frange(&g->rng, 0.0f, TAU_F);
        g->bugs[i].flap_phase  = rng_frange(&g->rng, 0.0f, TAU_F);
        g->bugs[i].hue_variant = (uint8_t)(i % 3);
    }

    // Starter Cozy Island Lanterns, Campfire & Garden Bench!
    static const float init_decors[5][4] = {
        {-0.4f, -4.8f, 0, 0.0f},  // Cottage overlook lantern
        { 1.4f,  2.2f, 0, 0.0f},  // West bridge entrance lantern (beside north railing)
        {-3.6f,  6.2f, 1, 0.0f},  // Cozy meadow campfire!
        {-3.8f,  2.4f, 2, 1.57f}, // Plaza west-edge garden bench facing plaza!
        { 7.0f,  2.2f, 0, 0.0f},  // East bridge entrance lantern (matching west side!)
    };
    for (int i = 0; i < 5; i++) {
        float dx = init_decors[i][0], dz = init_decors[i][1];
        g->decors[i].active = true;
        g->decors[i].kind   = (uint8_t)init_decors[i][2];
        g->decors[i].pos    = v3mk(dx, clover_ground_y(dx, dz), dz);
        g->decors[i].yaw    = init_decors[i][3];
    }

    // 3 Beloved Island Animal Villagers!
    g->npcs[0] = (VillagerNpc){
        .name = "MOCHI", .catchphrase = "purr-fect",
        .mesh_id = MESH_CHAR_MOCHI, .eye_hue = 11.0f,
        .accent_r = 228, .accent_g = 142, .accent_b = 68,
        .cloth_r  = 248, .cloth_g  = 112, .cloth_b  = 128,
        .pos = v3mk(-1.8f, clover_ground_y(-1.8f, 2.4f), 2.4f),
        .home_pos = v3mk(-1.8f, 0.0f, 2.4f), .yaw = 0.25f, .pause_t = 2.0f,
    };
    g->npcs[1] = (VillagerNpc){
        .name = "PUDDLES", .catchphrase = "ribbit-yay",
        .mesh_id = MESH_CHAR_PUDDLES, .eye_hue = 12.0f,
        .accent_r = 122, .accent_g = 224, .accent_b = 156,
        .cloth_r  = 252, .cloth_g  = 208, .cloth_b  = 64,
        .pos = v3mk(5.4f, clover_ground_y(5.4f, 4.6f), 4.6f),
        .home_pos = v3mk(5.4f, 0.0f, 4.6f), .yaw = -0.55f, .pause_t = 3.2f,
    };
    g->npcs[2] = (VillagerNpc){
        .name = "BISCUIT", .catchphrase = "woof-aroo",
        .mesh_id = MESH_CHAR_BISCUIT, .eye_hue = 13.0f,
        .accent_r = 238, .accent_g = 178, .accent_b = 96,
        .cloth_r  = 64,  .cloth_g  = 148, .cloth_b  = 236,
        .pos = v3mk(-4.8f, clover_ground_y(-4.8f, -5.2f), -5.2f),
        .home_pos = v3mk(-4.8f, 0.0f, -5.2f), .yaw = 0.45f, .pause_t = 1.5f,
    };
    for (int i = 0; i < MAX_NPCS; i++) g->npcs[i].goal_pos = g->npcs[i].pos;
}

void app_resize(GameState *g, float w, float h, float safe_top, float safe_bottom, float safe_left, float safe_right) {
    g->screen_w = w > 1.0f ? w : 1.0f;
    g->screen_h = h > 1.0f ? h : 1.0f;
    g->safe_top = safe_top;
    g->safe_bottom = safe_bottom;
    g->safe_left = safe_left;
    g->safe_right = safe_right;
    float min_d = (w < h) ? w : h;
    g->ui_scale = clampf(min_d / 760.0f, 0.72f, 1.38f);
}

// ============================================================================
// Context-Smart Island Action Trigger (Xbox 'A' / Space / HUD 'ACT' / Auto-Tap)
// ============================================================================
static void open_dialog(GameState *g, int npc_idx, const char *speaker, const char *l1, const char *l2) {
    g->dialog_open = true;
    g->dialog_npc_idx = npc_idx;
    g->dialog_speaker = speaker;
    g->dialog_line1 = l1;
    g->dialog_line2 = l2;
    g->dialog_char_revealed = 0;
    g->dialog_char_t = 0.0f;
    clover_audio_sfx(CSFX_UI_CHIME);
}

static void start_showoff(GameState *g, uint8_t item_kind, const char *title, const char *pun) {
    g->player_anim   = CANIM_SHOWOFF;
    g->player_anim_t = 0.0f;
    g->action_lock_t = 2.6f;
    g->showoff_timer = 2.8f;
    g->showoff_item  = item_kind;
    g->showoff_title = title;
    g->showoff_pun   = pun;
    clover_audio_sfx(CSFX_FANFARE);
    for (int i = 0; i < 14; i++) {
        float a = (float)i * (TAU_F / 14.0f);
        spawn_vfx(g, 2, v3add(g->player_pos, v3mk(0, 1.35f, 0)),
                  v3mk(cosf(a) * 2.2f, rng_frange(&g->rng, 1.2f, 2.8f), sinf(a) * 2.2f),
                  v3mk(0,1,0), 0.75f, 0.16f, 255, 224, 96, 255);
    }
}

static void perform_primary_action(GameState *g) {
    if (g->dialog_open) {
        g->dialog_open = false;
        clover_audio_sfx(CSFX_UI_CHIME);
        return;
    }
    if (g->showoff_timer > 0.0f) {
        g->showoff_timer = 0.0f;
        g->action_lock_t = 0.0f;
        g->player_anim   = CANIM_IDLE;
        return;
    }

    // 1. Active Fishing Reel-In!
    if (g->fish_state != FISH_IDLE) {
        if (g->fish_state == FISH_BITE) {
            uint8_t sp = (g->hooked_fish_idx >= 0) ? g->fish[g->hooked_fish_idx].species : 0;
            if (g->hooked_fish_idx >= 0) {
                // Respawn fish subtly back at home after catch
                g->fish[g->hooked_fish_idx].pos = g->fish[g->hooked_fish_idx].home_pos;
            }
            g->fish_state = FISH_IDLE;
            add_to_pockets(g, ITEM_FISH);
            if (sp == 2)      start_showoff(g, ITEM_FISH, "I CAUGHT A GOLDEN KOI!", "DON'T BE COY, IT'S PURE JOY!");
            else if (sp == 1) start_showoff(g, ITEM_FISH, "I CAUGHT A RED SNAPPER!", "LOOKING PRETTY DAPPER!");
            else              start_showoff(g, ITEM_FISH, "I CAUGHT A SEA BASS!", "BASS-ICALLY AN ISLAND LEGEND!");
        } else {
            g->fish_state = FISH_IDLE;
            g->player_anim = CANIM_IDLE;
            clover_audio_sfx(CSFX_CAST_ROD);
            show_toast(g, "REELED IN TOO SOON!");
        }
        return;
    }

    if (g->action_lock_t > 0.05f) return;

    v3 fwd = v3mk(sinf(g->player_yaw), 0.0f, cosf(g->player_yaw));
    v3 probe = v3add(g->player_pos, v3mul(fwd, 1.05f));

    // 2. Pick up nearby Ground Drop (Fruit, Seashell, Fossil, Coin Bag)
    int best_drop = -1; float best_dd = 1.55f;
    for (int i = 0; i < MAX_DROPS; i++) {
        if (!g->drops[i].active) continue;
        float d = v3len(v3sub(g->drops[i].pos, g->player_pos));
        if (d < best_dd) { best_dd = d; best_drop = i; }
    }
    if (best_drop >= 0) {
        uint8_t ik = g->drops[best_drop].item_kind;
        if (add_to_pockets(g, ik)) {
            g->drops[best_drop].active = false;
            g->player_anim = CANIM_SHAKE;
            g->player_anim_t = 0.0f;
            g->action_lock_t = 0.28f;
            clover_audio_sfx(ik == ITEM_COIN_BAG ? CSFX_ROCK_CHIME : CSFX_POP_PICKUP);
            if (ik == ITEM_FOSSIL) show_toast(g, "POCKETED ANCIENT FOSSIL!");
            else if (ik == ITEM_SEASHELL) show_toast(g, "PICKED UP CORAL SEASHELL!");
            else if (ik != ITEM_COIN_BAG) show_toast(g, "PICKED UP FRESH ISLAND FRUIT!");
        }
        return;
    }

    // 3. Cove Striped Market Stall — Sell Pocket Items for Coins!
    v3 stall_front = v3mk(-2.6f, clover_ground_y(-2.6f, 1.5f), 1.5f);
    if (v3len(v3sub(g->player_pos, stall_front)) < 2.2f) {
        if (g->pocket_count > 0) {
            int earned = 0;
            for (int i = 0; i < g->pocket_count; i++) earned += item_coin_value(g->pockets[i]);
            g->pocket_count = 0;
            g->coins += earned;
            clover_audio_sfx(CSFX_COINS_SELL);
            static char sell_buf[64];
            snprintf(sell_buf, sizeof(sell_buf), "SOLD ALL GOODS FOR +%d COINS! CHEERS!", earned);
            open_dialog(g, -1, "COVE MARKET", sell_buf, "THANK YOU FOR SUPPORTING CLOVER COVE!");
        } else {
            open_dialog(g, -1, "COVE MARKET",
                        "BRING FRUIT, SHELLS, BUGS, FISH OR FOSSILS!",
                        "WE PAY TOP COINS ALL DAY AND NIGHT!");
        }
        return;
    }

    // 4. Chat with Friendly Animal Villager!
    for (int i = 0; i < MAX_NPCS; i++) {
        float d = v3len(v3sub(g->npcs[i].pos, g->player_pos));
        if (d < 2.15f) {
            v3 to_p = v3sub(g->player_pos, g->npcs[i].pos);
            g->npcs[i].yaw = atan2f(to_p.x, to_p.z);
            g->npcs[i].anim = CANIM_WAVE;
            g->npcs[i].pause_t = 5.0f;
            g->player_yaw = atan2f(-to_p.x, -to_p.z);
            if (i == 0) {
                open_dialog(g, 0, "MOCHI",
                            (g->tod_preset == TOD_NIGHT) ? "THE STARS OVER THE COVE ARE SO SPARKLY, PURR-FECT!"
                                                         : "HI NEIGHBOR! TRY SHAKING APPLE TREES OR FISHING!",
                            "PRESS LB/RB FOR TOOLS OR X TO PLACE COZY LANTERNS!");
            } else if (i == 1) {
                open_dialog(g, 1, "PUDDLES",
                            "THE WATERFALL RIVER FEELS SO COOL TODAY, RIBBIT-YAY!",
                            "CAST YOUR ROD BY THE PIER WHEN BIG SHADOWS SWIM BY!");
            } else {
                open_dialog(g, 2, "BISCUIT",
                            "I SMELLED BURIED FOSSILS NEAR STAR CRACKS, WOOF-AROO!",
                            "EQUIP YOUR SHOVEL AND HIT TREASURE ROCKS FOR COINS!");
            }
            return;
        }
    }

    // 5. Context-Smart Bug Net Swing (if NET equipped OR Butterfly right in front!)
    int near_bug = -1; float best_bd = 2.25f;
    for (int i = 0; i < MAX_BUGS; i++) {
        if (!g->bugs[i].active) continue;
        float d = hypotf(g->bugs[i].pos.x - probe.x, g->bugs[i].pos.z - probe.z);
        if (d < best_bd) { best_bd = d; near_bug = i; }
    }
    if (g->equipped_tool == TOOL_NET || (near_bug >= 0 && best_bd < 1.65f)) {
        g->equipped_tool = TOOL_NET;
        g->player_anim   = CANIM_SWING;
        g->player_anim_t = 0.0f;
        g->action_lock_t = 0.48f;
        clover_audio_sfx(CSFX_NET_SWOOSH);
        if (near_bug >= 0) {
            uint8_t hv = g->bugs[near_bug].hue_variant;
            g->bugs[near_bug].active = false;
            g->bugs[near_bug].respawn_t = 12.0f;
            add_to_pockets(g, ITEM_BUTTERFLY);
            if (hv == 1)      start_showoff(g, ITEM_BUTTERFLY, "CAUGHT AN EMPEROR BUTTERFLY!", "LONG MAY ITS AZURE WINGS REIGN!");
            else if (hv == 2) start_showoff(g, ITEM_BUTTERFLY, "CAUGHT A BLOSSOM BUTTERFLY!", "PRETTY IN PINK CLOVER PETALS!");
            else              start_showoff(g, ITEM_BUTTERFLY, "CAUGHT A TIGER BUTTERFLY!", "EARNED MY STRIPES ON THIS ONE!");
        }
        return;
    }

    // 6. Context-Smart Shovel Dig / Money Rock Strike!
    int near_dig = -1;
    for (int i = 0; i < MAX_DIG_SPOTS; i++) {
        if (g->digs[i].active && !g->digs[i].dug && v3len(v3sub(g->digs[i].pos, probe)) < 1.55f) {
            near_dig = i; break;
        }
    }
    int near_rock = -1;
    for (int i = 0; i < MAX_ROCKS; i++) {
        if (v3len(v3sub(g->rocks[i].pos, probe)) < 1.65f) { near_rock = i; break; }
    }
    if (near_dig >= 0 || (near_rock >= 0 && (g->equipped_tool == TOOL_SHOVEL || g->equipped_tool == TOOL_AXE || g->equipped_tool == TOOL_HAND))) {
        if (near_dig >= 0) {
            g->equipped_tool = TOOL_SHOVEL;
            g->player_anim = CANIM_SWING; g->player_anim_t = 0.0f; g->action_lock_t = 0.48f;
            g->digs[near_dig].dug = true;
            g->digs[near_dig].reset_timer = 22.0f;
            clover_audio_sfx(CSFX_DIG);
            uint8_t prize = (rng_f01(&g->rng) < 0.65f) ? ITEM_FOSSIL : ITEM_COIN_BAG;
            spawn_drop(g, prize, v3add(g->digs[near_dig].pos, v3mk(0, 0.25f, 0)),
                       v3mk(rng_frange(&g->rng, -1.2f, 1.2f), 4.2f, rng_frange(&g->rng, 0.8f, 1.8f)));
            for (int k = 0; k < 8; k++) {
                spawn_vfx(g, 2, g->digs[near_dig].pos, v3mk(rng_frange(&g->rng,-1.8f,1.8f), 2.4f, rng_frange(&g->rng,-1.8f,1.8f)),
                          v3mk(0,1,0), 0.55f, 0.14f, 158, 112, 72, 240);
            }
            show_toast(g, prize == ITEM_FOSSIL ? "DUG UP AN ANCIENT FOSSIL!" : "DUG UP A 1000-COIN BAG!");
            return;
        }
        if (near_rock >= 0) {
            if (g->equipped_tool != TOOL_AXE) g->equipped_tool = TOOL_SHOVEL;
            g->player_anim = CANIM_SWING; g->player_anim_t = 0.0f; g->action_lock_t = 0.42f;
            g->rocks[near_rock].hit_shake = 0.35f;
            if (g->rocks[near_rock].hits_left > 0) {
                g->rocks[near_rock].hits_left--;
                g->rocks[near_rock].reset_timer = 18.0f;
                clover_audio_sfx(CSFX_ROCK_CHIME);
                float a = rng_frange(&g->rng, 0.0f, TAU_F);
                spawn_drop(g, ITEM_COIN_BAG, v3add(g->rocks[near_rock].pos, v3mk(0, 0.55f, 0)),
                           v3mk(cosf(a) * 2.4f, 4.4f, sinf(a) * 2.4f));
                show_toast(g, "CLANG! TREASURE ROCK DROPPED COINS!");
            } else {
                clover_audio_sfx(CSFX_DIG);
                show_toast(g, "TREASURE ROCK RESTING FOR A BIT!");
            }
            return;
        }
    }

    // 7. Shake Nearby Fruit / Coconut Tree!
    for (int i = 0; i < g->tree_count; i++) {
        if (!g->trees[i].active) continue;
        if (v3len(v3sub(g->trees[i].pos, probe)) < 1.85f) {
            g->player_anim = (g->equipped_tool == TOOL_AXE) ? CANIM_SWING : CANIM_SHAKE;
            g->player_anim_t = 0.0f;
            g->action_lock_t = 0.45f;
            g->trees[i].shake_timer = 0.55f;
            clover_audio_sfx(CSFX_SHAKE_TREE);
            for (int k = 0; k < 10; k++) {
                spawn_vfx(g, 2, v3add(g->trees[i].pos, v3mk(rng_frange(&g->rng,-0.9f,0.9f), 2.1f, rng_frange(&g->rng,-0.9f,0.9f))),
                          v3mk(rng_frange(&g->rng,-1.2f,1.2f), -1.4f, rng_frange(&g->rng,-1.2f,1.2f)),
                          v3mk(0,1,0), 0.9f, 0.14f, 108, 214, 88, 255);
            }
            if (g->trees[i].fruit_count > 0) {
                int cnt = g->trees[i].fruit_count;
                g->trees[i].fruit_count = 0;
                g->trees[i].regrow_timer = 20.0f;
                for (int f = 0; f < cnt; f++) {
                    float ang = (float)f * (TAU_F / (float)cnt) + 0.5f;
                    spawn_drop(g, g->trees[i].fruit_kind,
                               v3add(g->trees[i].pos, v3mk(cosf(ang) * 0.7f, 1.85f, sinf(ang) * 0.7f)),
                               v3mk(cosf(ang) * 1.9f, 2.8f, sinf(ang) * 1.9f));
                }
                show_toast(g, "FRESH FRUIT DROPPED FROM THE TREE!");
            } else {
                show_toast(g, "FRUIT IS STILL GROWING BACK!");
            }
            return;
        }
    }

    // 8. Cast Fishing Rod into Ocean, River, or Clifftop Pond!
    for (int step = 0; step < 3; step++) {
        float dist = 2.4f + (float)step * 0.95f;
        v3 cast_pt = v3add(g->player_pos, v3mul(fwd, dist));
        float wat_y = 0.0f; bool is_pond = false;
        if (clover_is_water_at(cast_pt.x, cast_pt.z, &wat_y, &is_pond)) {
            g->equipped_tool   = TOOL_ROD;
            g->fish_state      = FISH_CASTING;
            g->bobber_start    = v3add(g->player_pos, v3mk(0, 0.95f, 0));
            g->bobber_pos      = v3mk(cast_pt.x, wat_y, cast_pt.z);
            g->fish_timer      = 0.28f;
            g->nibbles_left    = 1 + (int)(rng_next(&g->rng) % 2u);
            g->hooked_fish_idx = -1;
            g->player_anim     = CANIM_FISH_WAIT;
            g->player_anim_t   = 0.0f;
            clover_audio_sfx(CSFX_CAST_ROD);

            // Attract nearest fish in same body of water!
            float best_fd = 9.5f;
            for (int i = 0; i < MAX_FISH; i++) {
                if (!g->fish[i].active || g->fish[i].in_pond != is_pond) continue;
                float fd = hypotf(g->fish[i].pos.x - cast_pt.x, g->fish[i].pos.z - cast_pt.z);
                if (fd < best_fd) { best_fd = fd; g->hooked_fish_idx = i; }
            }
            show_toast(g, "BOBBER CAST! WAIT FOR BIG SPLASH!");
            return;
        }
    }

    // Fallback tool swing (non-blocking wave while moving!)
    if (g->equipped_tool == TOOL_SHOVEL || g->equipped_tool == TOOL_NET || g->equipped_tool == TOOL_AXE) {
        g->player_anim = CANIM_SWING;
        g->player_anim_t = 0.0f;
        g->action_lock_t = 0.26f;
        clover_audio_sfx(CSFX_NET_SWOOSH);
    } else {
        g->player_anim = CANIM_WAVE;
        g->player_anim_t = 0.0f;
        if (g->player_vel_mag < 0.05f) g->action_lock_t = 0.45f;
        clover_audio_sfx(CSFX_UI_CHIME);
    }
}

static void place_cozy_decor(GameState *g) {
    v3 fwd = v3mk(sinf(g->player_yaw), 0.0f, cosf(g->player_yaw));
    v3 dp  = v3add(g->player_pos, v3mul(fwd, 1.35f));
    if (!clover_is_walkable(dp.x, dp.z)) {
        show_toast(g, "CAN'T PLACE DECOR ON WATER/CLIFF!");
        return;
    }
    // Pick Up if decor already right in front!
    for (int i = 0; i < MAX_DECORS; i++) {
        if (g->decors[i].active && v3len(v3sub(g->decors[i].pos, dp)) < 0.95f) {
            g->decors[i].active = false;
            clover_audio_sfx(CSFX_POP_PICKUP);
            show_toast(g, "STORED ISLAND DECOR!");
            return;
        }
    }
    int slot = -1;
    for (int i = 0; i < MAX_DECORS; i++) if (!g->decors[i].active) { slot = i; break; }
    if (slot < 0) slot = 5 + (g->next_decor_kind % (MAX_DECORS - 5));

    uint8_t k = g->next_decor_kind % 3;
    g->next_decor_kind++;
    g->decors[slot].active = true;
    g->decors[slot].kind   = k;
    g->decors[slot].pos    = v3mk(dp.x, clover_ground_y(dp.x, dp.z), dp.z);
    g->decors[slot].yaw    = g->player_yaw + PI_F;
    clover_audio_sfx(CSFX_POP_PICKUP);
    show_toast(g, k == 0 ? "PLACED WARM GARDEN LANTERN!" : (k == 1 ? "LIT A COZY CAMPFIRE!" : "PLACED COZY GARDEN BENCH!"));
}

static void cycle_time_of_day(GameState *g) {
    g->tod_preset = (g->tod_preset + 1) & 3;
    clover_audio_sfx(CSFX_UI_CHIME);
    static const char *tod_toasts[4] = {
        "TIME: SUNLIT ISLAND MORNING",
        "TIME: BRIGHT TROPICAL NOON",
        "TIME: GOLDEN COVE SUNSET",
        "TIME: STARRY LANTERN NIGHT",
    };
    show_toast(g, tod_toasts[g->tod_preset]);
}

// ============================================================================
// Curved-World Screen Ray -> Island Terrain Intersection (for Tap-to-Walk!)
// ============================================================================
static bool screen_ray_to_island(const GameState *g, float sx, float sy, v3 *out_wp) {
    float ndc_x = (sx / g->screen_w) * 2.0f - 1.0f;
    float ndc_y = 1.0f - (sy / g->screen_h) * 2.0f;
    float aspect = g->screen_w / g->screen_h;
    m4 view = m4_look_at(g->cur_eye, g->cur_target, v3mk(0, 1, 0));
    m4 proj = m4_persp(g->cur_fov, aspect, 0.25f, 140.0f, g->zero_to_one_z);
    m4 inv_vp = m4_invert(m4_mul(proj, view));

    v4 p_near = m4_mul_v4(inv_vp, v4mk(ndc_x, ndc_y, g->zero_to_one_z ? 0.0f : -1.0f, 1.0f));
    v4 p_far  = m4_mul_v4(inv_vp, v4mk(ndc_x, ndc_y, 1.0f, 1.0f));
    v3 ro = v3mul(v3mk(p_near.x, p_near.y, p_near.z), 1.0f / p_near.w);
    v3 p1 = v3mul(v3mk(p_far.x,  p_far.y,  p_far.z),  1.0f / p_far.w);
    v3 rd = v3norm(v3sub(p1, ro));

    float curve_k = g->free_cam ? 0.0f : WORLD_CURVE_K;
    for (int i = 0; i < 96; i++) {
        float t = 1.5f + (float)i * 0.45f;
        v3 p = v3add(ro, v3mul(rd, t));
        float dx = p.x - g->cam_focus.x, dz = p.z - g->cam_focus.z;
        float curved_ground = clover_ground_y(p.x, p.z) - curve_k * (dz * dz + 0.28f * dx * dx);
        if (p.y <= curved_ground) {
            *out_wp = v3mk(p.x, clover_ground_y(p.x, p.z), p.z);
            return true;
        }
    }
    return false;
}

// ============================================================================
// Touch / Mouse / Keyboard Event Handlers (True Independent Multi-Thumb!)
// ============================================================================
static float clover_top_y(const GameState *g) {
    float s = g->ui_scale;
    return fmaxf(g->safe_top, 20.0f * s) + 8.0f * s;
}

static void clover_inv_y_rect(const GameState *g, float *x0, float *y0, float *x1, float *y1) {
    float s = g->ui_scale;
    float top_y = clover_top_y(g);
    float r_edge = g->screen_w - g->safe_right - 14.0f * s;
    if (g->screen_w >= 860.0f * s) {
        float rx = r_edge - (76.0f + 108.0f + 118.0f) * s;
        *x0 = rx - 94.0f * s; *y0 = top_y;
        *x1 = rx;             *y1 = top_y + 40.0f * s;
    } else {
        *x0 = r_edge - 94.0f * s; *y0 = top_y + 46.0f * s;
        *x1 = r_edge;             *y1 = top_y + 86.0f * s;
    }
}

static bool clover_try_hud_tap(GameState *g, float sx, float sy) {
    float s = g->ui_scale;
    float top_y = clover_top_y(g);

    // Top-Right Utility Pills: [INV-Y / NORM-Y] [TIME] [FREE CAM] [SND] (full 48pt+ iOS touch targets!)
    float iy0, iy1, ix0, ix1;
    clover_inv_y_rect(g, &ix0, &iy0, &ix1, &iy1);
    if (ui_hit_rect(sx, sy, ix0 - 8.0f * s, iy0 - 20.0f * s, ix1 + 6.0f * s, iy1 + 22.0f * s)) {
        g->ctrl_invert_y = !g->ctrl_invert_y;
        clover_audio_sfx(CSFX_UI_CHIME);
        show_toast(g, g->ctrl_invert_y ? "RIGHT THUMB Y-AXIS: INVERTED" : "RIGHT THUMB Y-AXIS: NORMAL");
        return true;
    }

    float rx = g->screen_w - g->safe_right - 14.0f * s;
    if (ui_hit_rect(sx, sy, rx - 70.0f * s, top_y - 20.0f * s, rx + 8.0f * s, top_y + 62.0f * s)) {
        g->audio_muted = !g->audio_muted;
        show_toast(g, g->audio_muted ? "AUDIO MUTED" : "COZY ISLAND AUDIO ON");
        return true;
    }
    rx -= 76.0f * s;
    if (ui_hit_rect(sx, sy, rx - 102.0f * s, top_y - 20.0f * s, rx + 4.0f * s, top_y + 62.0f * s)) {
        g->free_cam = !g->free_cam;
        if (g->free_cam) {
            g->free_pos = g->cur_eye;
            v3 d = v3norm(v3sub(g->cur_target, g->cur_eye));
            g->free_yaw = atan2f(d.x, d.z);
            g->free_pitch = asinf(clampf(d.y, -0.95f, 0.95f));
            show_toast(g, "6-DOF FREE-CAM ON");
        } else {
            show_toast(g, "COZY ISLAND CAM ON");
        }
        return true;
    }
    rx -= 108.0f * s;
    if (ui_hit_rect(sx, sy, rx - 112.0f * s, top_y - 20.0f * s, rx + 4.0f * s, top_y + 62.0f * s)) {
        cycle_time_of_day(g);
        return true;
    }

    // Bottom HUD: 5-Tool Quick Bar + [DECOR] + Big Leaf [ACT] Button!
    float bot_y = g->screen_h - g->safe_bottom - 14.0f * s;
    float act_r = 42.0f * s;
    float act_cx = g->screen_w - g->safe_right - 18.0f * s - act_r;
    float act_cy = bot_y - act_r;
    if (hypotf(sx - act_cx, sy - act_cy) <= act_r * 1.28f) {
        perform_primary_action(g);
        return true;
    }
    // [DECOR] pill left of ACT button
    if (ui_hit_rect(sx, sy, act_cx - act_r - 108.0f * s, bot_y - 50.0f * s, act_cx - act_r - 4.0f * s, bot_y + 4.0f * s)) {
        place_cozy_decor(g);
        return true;
    }

    // 5-Tool Quick Bar
    float bar_w = 340.0f * s;
    float bar_x0 = g->safe_left + 16.0f * s;
    if (g->screen_w > 740.0f * s) bar_x0 = (g->screen_w - bar_w) * 0.5f - 40.0f * s;
    if (ui_hit_rect(sx, sy, bar_x0 - 6.0f * s, bot_y - 52.0f * s, bar_x0 + bar_w + 6.0f * s, bot_y + 6.0f * s)) {
        int ti = (int)((sx - bar_x0) / (bar_w / 5.0f));
        ti = ti < 0 ? 0 : (ti >= TOOL_COUNT ? TOOL_COUNT - 1 : ti);
        g->equipped_tool = (uint8_t)ti;
        g->fish_state = FISH_IDLE;
        clover_audio_sfx(CSFX_TOOL_SWAP);
        return true;
    }

    if (g->dialog_open || g->showoff_timer > 0.0f || g->fish_state != FISH_IDLE) {
        perform_primary_action(g);
        return true;
    }
    return false;
}

static void clover_tap_island_world(GameState *g, float sx, float sy) {
    v3 hit;
    if (!g->free_cam && screen_ray_to_island(g, sx, sy, &hit)) {
        v3 diff = v3sub(hit, g->player_pos);
        float d2d = hypotf(diff.x, diff.z);
        if (d2d < 1.85f) {
            if (d2d > 0.15f) g->player_yaw = atan2f(diff.x, diff.z);
            perform_primary_action(g);
        } else {
            g->walk_target = hit;
            g->has_walk_target = true;
            g->auto_act_on_arrive = true;
            clover_audio_sfx(CSFX_STEP);
        }
    }
}

void app_on_tap(GameState *g, float sx, float sy) {
    if (clover_try_hud_tap(g, sx, sy)) return;
    clover_tap_island_world(g, sx, sy);
}

void app_on_touch(GameState *g, int ptr_id, int phase, float sx, float sy) {
    if (ptr_id < 0 || ptr_id >= 8) return;
    float s = g->ui_scale;
    float max_r = 56.0f * s;

    if (phase == TOUCH_DOWN) {
        g->ptrs[ptr_id].down  = true;
        g->ptrs[ptr_id].moved = false;
        g->ptrs[ptr_id].sx0   = sx;
        g->ptrs[ptr_id].sy0   = sy;
        g->ptrs[ptr_id].sx    = sx;
        g->ptrs[ptr_id].sy    = sy;

        // Instant zero-latency HUD button activation (never steals left or right stick!)
        if (clover_try_hud_tap(g, sx, sy)) {
            g->ptrs[ptr_id].role = 1; // HUD button
            return;
        }

        if (!g->free_cam && sx < g->screen_w * 0.46f && sy > clover_top_y(g) + 52.0f * s && g->left_stick_ptr < 0) {
            g->ptrs[ptr_id].role = 2; // Left Movement Thumbstick
            g->left_stick_ptr = ptr_id;
            g->touch_stick_ox = sx;
            g->touch_stick_oy = sy;
            g->touch_stick_dx = 0.0f;
            g->touch_stick_dy = 0.0f;
        } else if (g->right_stick_ptr < 0) {
            g->ptrs[ptr_id].role = 3; // Right Camera Thumbstick
            g->right_stick_ptr = ptr_id;
            g->right_stick_ox = sx;
            g->right_stick_oy = sy;
            g->right_stick_dx = 0.0f;
            g->right_stick_dy = 0.0f;
        } else {
            g->ptrs[ptr_id].role = 4; // Second camera finger for pinch zoom
            g->pinch_prev_dist = 0.0f;
        }
    } else if (phase == TOUCH_MOVE) {
        if (!g->ptrs[ptr_id].down) return;
        float prev_x = g->ptrs[ptr_id].sx, prev_y = g->ptrs[ptr_id].sy;
        g->ptrs[ptr_id].sx = sx;
        g->ptrs[ptr_id].sy = sy;

        uint8_t role = g->ptrs[ptr_id].role;
        if (role == 1) return;

        float dist0 = hypotf(sx - g->ptrs[ptr_id].sx0, sy - g->ptrs[ptr_id].sy0);
        if (dist0 > 9.0f * s) g->ptrs[ptr_id].moved = true;

        if (role == 2 && ptr_id == g->left_stick_ptr) {
            if (g->ptrs[ptr_id].moved) {
                g->touch_stick_active = true;
                g->touch_stick_dx = clampf(sx - g->touch_stick_ox, -max_r, max_r);
                g->touch_stick_dy = clampf(sy - g->touch_stick_oy, -max_r, max_r);
                g->has_walk_target = false;
            }
            return;
        }

        // Check if 2 non-HUD, non-left-stick fingers are pinching on right/center
        int pA = -1, pB = -1;
        for (int i = 0; i < 8; i++) {
            if (g->ptrs[i].down && (g->ptrs[i].role == 3 || g->ptrs[i].role == 4)) {
                if (pA < 0) pA = i; else if (pB < 0) pB = i;
            }
        }
        if (pA >= 0 && pB >= 0) {
            g->right_stick_active = false;
            g->ptrs[pA].moved = g->ptrs[pB].moved = true;
            float d = hypotf(g->ptrs[pB].sx - g->ptrs[pA].sx, g->ptrs[pB].sy - g->ptrs[pA].sy);
            if (g->pinch_prev_dist > 10.0f) {
                app_on_pinch(g, (d - g->pinch_prev_dist) / (260.0f * s));
            }
            g->pinch_prev_dist = d;
            return;
        }

        if (role == 3 && ptr_id == g->right_stick_ptr && g->ptrs[ptr_id].moved) {
            g->right_stick_active = true;
            g->right_stick_dx = clampf(sx - g->right_stick_ox, -max_r, max_r);
            g->right_stick_dy = clampf(sy - g->right_stick_oy, -max_r, max_r);
            // Crisp direct 1:1 vertical/horizontal swipe response + virtual stick hold!
            float dx = (sx - prev_x) / s;
            float dy = (sy - prev_y) / s;
            float y_inv = g->ctrl_invert_y ? 1.0f : -1.0f;
            if (g->free_cam) {
                g->free_yaw -= dx * 0.0042f;
                g->free_pitch = clampf(g->free_pitch + dy * y_inv * 0.0042f, -1.38f, 1.38f);
            } else {
                g->cam_yaw -= dx * 0.0045f;
                g->cam_pitch = clampf(g->cam_pitch - dy * y_inv * 0.0042f, 0.12f, 1.22f);
            }
        }
    } else if (phase == TOUCH_UP) {
        if (!g->ptrs[ptr_id].down) return;
        uint8_t role = g->ptrs[ptr_id].role;
        bool moved = g->ptrs[ptr_id].moved;
        g->ptrs[ptr_id].down = false;
        g->ptrs[ptr_id].role = 0;
        g->pinch_prev_dist   = 0.0f;

        if (role == 2 && ptr_id == g->left_stick_ptr) {
            g->left_stick_ptr     = -1;
            g->touch_stick_active = false;
            g->touch_stick_dx     = 0.0f;
            g->touch_stick_dy     = 0.0f;
            if (!moved) clover_tap_island_world(g, sx, sy);
        } else if (role == 3 && ptr_id == g->right_stick_ptr) {
            g->right_stick_ptr    = -1;
            g->right_stick_active = false;
            g->right_stick_dx    = 0.0f;
            g->right_stick_dy    = 0.0f;
            if (!moved) clover_tap_island_world(g, sx, sy);
        }
    }
}

void app_on_drag(GameState *g, float dx, float dy, int finger_count, float start_x, float start_y) {
    g->drag_occurred = true;
    // Left-half 1-finger drag on touch/trackpad = Smooth Analog Virtual Thumbstick!
    if (!g->free_cam && finger_count == 1 && start_x > 0.5f && start_x < g->screen_w * 0.45f && start_y > g->screen_h * 0.25f) {
        if (!g->touch_stick_active) {
            g->touch_stick_active = true;
            g->touch_stick_ox = start_x;
            g->touch_stick_oy = start_y;
            g->touch_stick_dx = 0.0f;
            g->touch_stick_dy = 0.0f;
        }
        float max_r = 56.0f * g->ui_scale;
        g->touch_stick_dx = clampf(g->touch_stick_dx + dx, -max_r, max_r);
        g->touch_stick_dy = clampf(g->touch_stick_dy + dy, -max_r, max_r);
        g->has_walk_target = false;
        return;
    }

    // Right-half or 2-finger drag = Smooth Camera Orbit & Tilt!
    float y_inv = g->ctrl_invert_y ? 1.0f : -1.0f;
    if (g->free_cam) {
        g->free_yaw   -= dx * 0.0045f;
        g->free_pitch  = clampf(g->free_pitch + dy * y_inv * 0.0045f, -1.35f, 1.35f);
    } else {
        g->cam_yaw   -= dx * 0.0052f;
        g->cam_pitch  = clampf(g->cam_pitch - dy * y_inv * 0.0042f, 0.12f, 1.22f);
    }
}

void app_on_drag_end(GameState *g) {
    g->touch_stick_active = false;
    g->touch_stick_dx = 0.0f;
    g->touch_stick_dy = 0.0f;
    g->drag_occurred  = false;
}

void app_on_pinch(GameState *g, float scale_delta) {
    if (g->free_cam) {
        v3 fwd = v3mk(cosf(g->free_pitch) * sinf(g->free_yaw), sinf(g->free_pitch), cosf(g->free_pitch) * cosf(g->free_yaw));
        g->free_pos = v3add(g->free_pos, v3mul(fwd, scale_delta * 6.0f));
    } else {
        g->cam_dist = clampf(g->cam_dist - scale_delta * 6.5f, 4.8f, 22.0f);
    }
}

void app_on_hover(GameState *g, float sx, float sy) {
    (void)g; (void)sx; (void)sy;
}

void app_on_key_press(GameState *g, int ch) {
    if (ch >= '1' && ch <= '5') {
        g->equipped_tool = (uint8_t)(ch - '1');
        g->fish_state = FISH_IDLE;
        clover_audio_sfx(CSFX_TOOL_SWAP);
    } else if (ch == ' ' || ch == '\r' || ch == '\n') {
        perform_primary_action(g);
    } else if (ch == 'c' || ch == 'C') {
        place_cozy_decor(g);
    } else if (ch == 't' || ch == 'T') {
        cycle_time_of_day(g);
    } else if (ch == 'f' || ch == 'F') {
        g->free_cam = !g->free_cam;
        if (g->free_cam) {
            g->free_pos = g->cur_eye;
            v3 d = v3norm(v3sub(g->cur_target, g->cur_eye));
            g->free_yaw = atan2f(d.x, d.z);
            g->free_pitch = asinf(clampf(d.y, -0.95f, 0.95f));
        }
        show_toast(g, g->free_cam ? "6-DOF FREE-CAM ON" : "COZY ISLAND CAM ON");
    } else if (ch == 'v' || ch == 'V') {
        g->ctrl_invert_y = !g->ctrl_invert_y;
        show_toast(g, g->ctrl_invert_y ? "CAMERA INVERT-Y: ON" : "CAMERA INVERT-Y: OFF");
    } else if (ch == 'm' || ch == 'M') {
        g->audio_muted = !g->audio_muted;
        show_toast(g, g->audio_muted ? "AUDIO MUTED" : "COZY ISLAND AUDIO ON");
    } else if (ch == 'r' || ch == 'R') {
        bool inv = g->ctrl_invert_y, mute = g->audio_muted;
        app_init(g, g->zero_to_one_z);
        g->ctrl_invert_y = inv; g->audio_muted = mute;
        show_toast(g, "ISLAND RESET!");
    }
}

// ============================================================================
// 5 Showcase --capture Presets
// ============================================================================
void app_set_preset_shot(GameState *g, int shot_id) {
    g->dialog_open = false;
    g->showoff_timer = 0.0f;
    g->free_cam = false;
    g->tod_preset = TOD_MORNING;
    g->tod_blend  = 0.0f;

    if (shot_id == 0) {
        // Shot 0: Hero Island Diorama Overview (Curved Horizon, Cottage, Waterfall, Bridge, Plaza & Surf!)
        g->player_pos = v3mk(-0.5f, clover_ground_y(-0.5f, 3.2f), 3.2f);
        g->player_yaw = 0.15f;
        g->player_anim = CANIM_WAVE;
        g->action_lock_t = 5.0f;
        g->cam_focus = v3mk(-0.4f, 1.35f, 0.6f);
        g->cam_yaw = -0.14f;
        g->cam_pitch = 0.44f;
        g->cam_dist = 19.2f;
    } else if (shot_id == 1) {
        // Shot 1: Close-Up Chibi Portrait — Player & Mochi the Calico Cat Waving Full-Body on Sunlit Plaza!
        g->player_pos = v3mk(-0.85f, clover_ground_y(-0.85f, 2.5f), 2.5f);
        g->player_yaw = -0.16f;
        g->player_anim = CANIM_WAVE;
        g->action_lock_t = 5.0f;
        g->npcs[0].pos = v3mk(-2.05f, clover_ground_y(-2.05f, 2.5f), 2.5f);
        g->npcs[0].yaw = 0.20f;
        g->npcs[0].anim = CANIM_WAVE;
        g->npcs[0].pause_t = 5.0f;
        g->free_cam = true;
        g->free_pos = v3mk(-1.45f, 1.48f, 5.25f);
        g->free_yaw = 3.14159f;
        g->free_pitch = -0.09f;
    } else if (shot_id == 2) {
        // Shot 2: Waterfall Fishing Triumph — Proud Chibi Hold-Up Pose + Pun Banner!
        g->tod_preset = TOD_NOON; g->tod_blend = 1.0f;
        g->equipped_tool = TOOL_ROD;
        g->player_pos = v3mk(1.55f, clover_ground_y(1.55f, 0.35f), 0.35f);
        g->player_yaw = 0.18f;
        start_showoff(g, ITEM_FISH, "I CAUGHT A GOLDEN KOI!", "DON'T BE COY, IT'S PURE JOY!");
        g->free_cam = true;
        g->free_pos = v3mk(2.45f, 1.95f, 4.45f);
        g->free_yaw = 2.96f;
        g->free_pitch = -0.11f;
    } else if (shot_id == 3) {
        // Shot 3: Golden Sunset Chat at Cove Striped Market Stall!
        g->tod_preset = TOD_SUNSET; g->tod_blend = 2.0f;
        g->player_pos = v3mk(-1.2f, clover_ground_y(-1.2f, 2.1f), 2.1f);
        g->player_yaw = -0.45f;
        g->npcs[0].pos = v3mk(-2.35f, clover_ground_y(-2.35f, 2.0f), 2.0f);
        g->npcs[0].yaw = 0.45f;
        g->npcs[0].anim = CANIM_WAVE;
        g->npcs[0].pause_t = 5.0f;
        open_dialog(g, 0, "MOCHI",
                    "GOLDEN SUNSET AT COVE MARKET IS SO COZY, PURR-FECT!",
                    "LET'S SELL OUR APPLES AND SHELLS FOR SHINY COINS!");
        g->dialog_char_revealed = 120;
        g->free_cam = true;
        g->free_pos = v3mk(-1.1f, 2.45f, 6.2f);
        g->free_yaw = 2.98f;
        g->free_pitch = -0.18f;
    } else {
        // Shot 4: Magical Starry Night with Twinkling Stars, Glowing Cottage, Lanterns & Campfire!
        g->tod_preset = TOD_NIGHT; g->tod_blend = 3.0f;
        g->player_pos = v3mk(-1.15f, clover_ground_y(-1.15f, 3.0f), 3.0f);
        g->player_yaw = -0.18f;
        g->player_anim = CANIM_WAVE;
        g->action_lock_t = 5.0f;
        g->free_cam = true;
        g->free_pos = v3mk(-1.45f, 3.35f, 9.85f);
        g->free_yaw = 3.10f;
        g->free_pitch = -0.14f;
    }
}

// ============================================================================
// Main Frame Simulation + GPU RenderPacket Builder
// ============================================================================
static void push_draw(RenderPacket *pkt, uint32_t mesh_id, uint32_t flags, const DrawUniforms *u) {
    if (pkt->draw_count >= MAX_DYNAMIC_DRAWS) return;
    pkt->draws[pkt->draw_count++] = (DynamicDraw){mesh_id, flags, *u};
}

void app_update_and_build_packet(GameState *g, float dt, const EngInput *in, RenderPacket *pkt) {
    dt = clampf(dt, 0.001f, 0.05f);
    g->time_sec += dt;
    clover_audio_set_state(g->tod_preset, g->audio_muted);

    const GamepadState pad = in->pad;
    uint32_t pr = pad.pressed;

    // 1. Xbox Nano / Gamepad Rising-Edge Shortcuts!
    if (pr & GP_BTN_R3) {
        g->ctrl_invert_y = !g->ctrl_invert_y;
        show_toast(g, g->ctrl_invert_y ? "CAMERA INVERT-Y: ON" : "CAMERA INVERT-Y: OFF");
    }
    if ((pr & GP_BTN_VIEW) || (pr & GP_BTN_Y)) {
        app_on_key_press(g, 'f');
    }
    if ((pr & GP_BTN_MENU) || (pr & GP_BTN_DPAD_U)) {
        cycle_time_of_day(g);
    }
    if ((pr & GP_BTN_LB) || (pr & GP_BTN_DPAD_L)) {
        g->equipped_tool = (uint8_t)((g->equipped_tool + TOOL_COUNT - 1) % TOOL_COUNT);
        g->fish_state = FISH_IDLE;
        clover_audio_sfx(CSFX_TOOL_SWAP);
    }
    if ((pr & GP_BTN_RB) || (pr & GP_BTN_DPAD_R)) {
        g->equipped_tool = (uint8_t)((g->equipped_tool + 1) % TOOL_COUNT);
        g->fish_state = FISH_IDLE;
        clover_audio_sfx(CSFX_TOOL_SWAP);
    }
    if (pr & GP_BTN_A) {
        perform_primary_action(g);
    }
    if (pr & GP_BTN_X) {
        place_cozy_decor(g);
    }
    if ((pr & GP_BTN_B) && (g->dialog_open || g->showoff_timer > 0.0f || g->fish_state != FISH_IDLE)) {
        perform_primary_action(g);
    }

    // Smooth Time-of-Day Blend
    float target_tod = (float)g->tod_preset;
    g->tod_blend += (target_tod - g->tod_blend) * clampf(dt * 2.8f, 0.0f, 1.0f);
    if (g->toast_timer > 0.0f) g->toast_timer -= dt;
    if (g->showoff_timer > 0.0f) g->showoff_timer -= dt;
    if (g->action_lock_t > 0.0f) {
        g->action_lock_t -= dt;
        g->player_anim_t += dt;
        if (g->action_lock_t <= 0.0f && g->fish_state == FISH_IDLE) {
            g->player_anim = CANIM_IDLE;
        }
    }

    // Reveal Dialogue Text + Pitched Formant Speech Blips
    if (g->dialog_open) {
        g->dialog_char_t += dt;
        while (g->dialog_char_t >= 0.032f && g->dialog_char_revealed < 140) {
            g->dialog_char_t -= 0.032f;
            g->dialog_char_revealed++;
            int l1_len = (int)strlen(g->dialog_line1);
            int total_len = l1_len + (int)strlen(g->dialog_line2);
            if (g->dialog_char_revealed <= total_len && (g->dialog_char_revealed & 1) == 1) {
                clover_audio_speech(g->dialog_char_revealed + (g->dialog_npc_idx + 2) * 19);
            }
        }
    }

    // 2. Camera Orbit / Tilt / Zoom (with ctrl_invert_y = TRUE by default, plus Right Virtual Thumbstick!)
    float pad_y_sign = g->ctrl_invert_y ? -1.0f : 1.0f;
    float zoom_trig  = (pad.lt - pad.rt) * 9.0f * dt;
    float rstick_x = 0.0f, rstick_y = 0.0f;
    if (g->right_stick_active) {
        float max_r = 56.0f * g->ui_scale;
        rstick_x = g->right_stick_dx / max_r;
        rstick_y = -g->right_stick_dy / max_r; // +1 = Thumb Pushed Up, identical to physical controller!
    }

    if (g->free_cam) {
        float boost = (in->look_axes.z > 0.5f || (pad.held & GP_BTN_B)) ? 2.2f : 1.0f;
        g->free_yaw   -= (in->look_axes.x + pad.rx + rstick_x) * 1.9f * dt;
        g->free_pitch  = clampf(g->free_pitch + (in->look_axes.y + (pad.ry + rstick_y) * pad_y_sign) * 1.6f * dt, -1.42f, 1.42f);
        v3 fwd   = v3mk(cosf(g->free_pitch) * sinf(g->free_yaw), sinf(g->free_pitch), cosf(g->free_pitch) * cosf(g->free_yaw));
        v3 right = v3norm(v3cross(fwd, v3mk(0, 1, 0)));
        float mx = in->move_axes.x + pad.lx;
        float mz = in->move_axes.z + pad.ly;
        float my = in->move_axes.y + (pad.rt - pad.lt);
        g->free_pos = v3add(g->free_pos, v3mul(fwd,   mz * 9.5f * boost * dt));
        g->free_pos = v3add(g->free_pos, v3mul(right, mx * 9.5f * boost * dt));
        g->free_pos.y += my * 7.5f * boost * dt;

        g->cur_eye    = g->free_pos;
        g->cur_target = v3add(g->free_pos, fwd);
    } else {
        g->cam_yaw   -= (in->look_axes.x + pad.rx + rstick_x) * 2.05f * dt;
        g->cam_pitch  = clampf(g->cam_pitch - (in->look_axes.y + (pad.ry + rstick_y) * pad_y_sign) * 1.65f * dt, 0.12f, 1.22f);
        g->cam_dist   = clampf(g->cam_dist + zoom_trig - in->move_axes.y * 7.0f * dt, 4.8f, 22.0f);

        // Player Movement (Left Stick / WASD / Touch Thumbstick / Tap-to-Walk)
        bool can_move = (g->action_lock_t <= 0.01f && !g->dialog_open && g->fish_state == FISH_IDLE);
        float in_x = in->move_axes.x + pad.lx;
        float in_y = in->move_axes.z + pad.ly;
        if (g->touch_stick_active) {
            float max_r = 56.0f * g->ui_scale;
            in_x += g->touch_stick_dx / max_r;
            in_y -= g->touch_stick_dy / max_r;
        }
        float in_len = hypotf(in_x, in_y);
        if (in_len > 0.08f) g->has_walk_target = false;

        v3 wish_world = v3mk(0, 0, 0);
        if (can_move && in_len > 0.08f) {
            if (in_len > 1.0f) { in_x /= in_len; in_y /= in_len; in_len = 1.0f; }
            // Camera-relative planar basis
            v3 cam_fwd   = v3mk(-sinf(g->cam_yaw), 0.0f, -cosf(g->cam_yaw));
            v3 cam_right = v3mk( cosf(g->cam_yaw), 0.0f, -sinf(g->cam_yaw));
            wish_world   = v3add(v3mul(cam_right, in_x), v3mul(cam_fwd, in_y));
        } else if (can_move && g->has_walk_target) {
            v3 diff = v3sub(g->walk_target, g->player_pos);
            float d = hypotf(diff.x, diff.z);
            if (d < 0.42f) {
                g->has_walk_target = false;
                if (g->auto_act_on_arrive) {
                    g->auto_act_on_arrive = false;
                    perform_primary_action(g);
                }
            } else {
                wish_world = v3mk(diff.x / d, 0.0f, diff.z / d);
                in_len = clampf(d * 0.8f, 0.45f, 1.0f);
            }
        }

        float wlen = hypotf(wish_world.x, wish_world.z);
        if (can_move && wlen > 0.05f) {
            bool sprint = (in->look_axes.z > 0.5f) || ((pad.held & (GP_BTN_B | GP_BTN_L3)) != 0) || (in_len > 0.92f && g->touch_stick_active);
            g->sprinting = sprint;
            float spd = (sprint ? 5.2f : 3.4f) * clampf(in_len, 0.25f, 1.0f);
            v3 step = v3mul(wish_world, (spd * dt) / wlen);

            // Smooth sliding collision against water/cliffs/buildings
            float nx = g->player_pos.x + step.x;
            float nz = g->player_pos.z + step.z;
            if (clover_is_walkable(nx, nz)) {
                g->player_pos.x = nx; g->player_pos.z = nz;
            } else if (clover_is_walkable(nx, g->player_pos.z)) {
                g->player_pos.x = nx;
            } else if (clover_is_walkable(g->player_pos.x, nz)) {
                g->player_pos.z = nz;
            } else {
                g->has_walk_target = false;
            }
            g->player_pos.y = clover_ground_y(g->player_pos.x, g->player_pos.z);

            float target_yaw = atan2f(wish_world.x, wish_world.z);
            float dyaw = target_yaw - g->player_yaw;
            while (dyaw >  PI_F) dyaw -= TAU_F;
            while (dyaw < -PI_F) dyaw += TAU_F;
            g->player_yaw += dyaw * clampf(dt * 14.0f, 0.0f, 1.0f);

            g->player_vel_mag = clampf(in_len, 0.3f, 1.0f);
            g->player_anim    = sprint ? CANIM_RUN : CANIM_WALK;

            g->step_dust_cd -= dt;
            if (g->step_dust_cd <= 0.0f) {
                g->step_dust_cd = sprint ? 0.20f : 0.34f;
                clover_audio_sfx(CSFX_STEP);
                if (sprint) {
                    spawn_vfx(g, 2, v3add(g->player_pos, v3mk(0, 0.12f, 0)),
                              v3mk(rng_frange(&g->rng,-0.4f,0.4f), 0.65f, rng_frange(&g->rng,-0.4f,0.4f)),
                              v3mk(0,1,0), 0.42f, 0.18f, 245, 240, 228, 190);
                }
            }
        } else if (can_move) {
            g->player_vel_mag = 0.0f;
            g->player_anim    = CANIM_IDLE;
        }

        v3 ideal_focus = v3add(g->player_pos, v3mk(0.0f, 0.72f, 0.0f));
        g->cam_focus = v3lerp(g->cam_focus, ideal_focus, clampf(dt * 7.5f, 0.0f, 1.0f));
        float cp = cosf(g->cam_pitch), sp = sinf(g->cam_pitch);
        v3 offset = v3mk(sinf(g->cam_yaw) * cp * g->cam_dist, sp * g->cam_dist, cosf(g->cam_yaw) * cp * g->cam_dist);
        g->cur_eye    = v3add(g->cam_focus, offset);
        float min_eye_y = clover_ground_y(g->cur_eye.x, g->cur_eye.z) + 0.42f;
        if (g->cur_eye.y < min_eye_y) g->cur_eye.y = min_eye_y;
        g->cur_target = g->cam_focus;
    }

    // 3. Update Living Island World (Fishing, Drops, Trees, Villagers, Fish, Butterflies, VFX!)
    if (g->fish_state != FISH_IDLE) {
        g->fish_timer -= dt;
        if (g->fish_state == FISH_CASTING && g->fish_timer <= 0.0f) {
            g->fish_state = FISH_WAITING;
            g->fish_timer = rng_frange(&g->rng, 1.1f, 1.9f);
            spawn_vfx(g, 1, v3add(g->bobber_pos, v3mk(0, 0.04f, 0)), v3mk(0,0,0), v3mk(0,1,0), 0.65f, 0.55f, 220, 248, 255, 220);
        } else if (g->fish_state == FISH_WAITING && g->fish_timer <= 0.0f) {
            if (g->nibbles_left > 0) {
                g->nibbles_left--;
                g->fish_state = FISH_NIBBLE;
                g->fish_timer = 0.45f;
                clover_audio_sfx(CSFX_NIBBLE);
                spawn_vfx(g, 1, v3add(g->bobber_pos, v3mk(0, 0.04f, 0)), v3mk(0,0,0), v3mk(0,1,0), 0.55f, 0.48f, 220, 248, 255, 200);
            } else {
                g->fish_state = FISH_BITE;
                g->fish_timer = 1.35f; // Generous 1.35s reaction window!
                clover_audio_sfx(CSFX_SPLASH_BITE);
                show_toast(g, "! SPLASH ! PRESS A / ACT NOW!");
                for (int k = 0; k < 10; k++) {
                    float a = (float)k * (TAU_F / 10.0f);
                    spawn_vfx(g, 2, v3add(g->bobber_pos, v3mk(0, 0.1f, 0)),
                              v3mk(cosf(a)*1.8f, 2.8f, sinf(a)*1.8f), v3mk(0,1,0), 0.55f, 0.15f, 210, 248, 255, 255);
                }
            }
        } else if (g->fish_state == FISH_NIBBLE && g->fish_timer <= 0.0f) {
            g->fish_state = FISH_WAITING;
            g->fish_timer = rng_frange(&g->rng, 0.75f, 1.35f);
        } else if (g->fish_state == FISH_BITE && g->fish_timer <= 0.0f) {
            g->fish_state = FISH_IDLE;
            g->player_anim = CANIM_IDLE;
            show_toast(g, "OH NO, THE FISH GOT AWAY!");
        }
    }

    // Bouncy Ground Drop Physics
    for (int i = 0; i < MAX_DROPS; i++) {
        if (!g->drops[i].active) continue;
        GroundDrop *d = &g->drops[i];
        d->yaw += dt * 1.2f;
        if (v3len(d->vel) > 0.02f) {
            d->vel.y -= 13.5f * dt;
            d->pos = v3add(d->pos, v3mul(d->vel, dt));
            float gy = clover_ground_y(d->pos.x, d->pos.z);
            if (d->pos.y <= gy) {
                d->pos.y = gy;
                if (fabsf(d->vel.y) > 1.2f) {
                    d->vel.y = -d->vel.y * 0.42f;
                    d->vel.x *= 0.55f; d->vel.z *= 0.55f;
                } else {
                    d->vel = v3mk(0, 0, 0);
                }
            }
        }
    }

    // Trees, Rocks, Dig Spots regrow timers
    for (int i = 0; i < g->tree_count; i++) {
        if (g->trees[i].shake_timer > 0.0f) g->trees[i].shake_timer -= dt;
        if (g->trees[i].fruit_count == 0) {
            g->trees[i].regrow_timer -= dt;
            if (g->trees[i].regrow_timer <= 0.0f) g->trees[i].fruit_count = g->trees[i].is_palm ? 2 : 3;
        }
    }
    for (int i = 0; i < MAX_ROCKS; i++) {
        if (g->rocks[i].hit_shake > 0.0f) g->rocks[i].hit_shake -= dt;
        if (g->rocks[i].hits_left < 5) {
            g->rocks[i].reset_timer -= dt;
            if (g->rocks[i].reset_timer <= 0.0f) g->rocks[i].hits_left = 5;
        }
    }
    for (int i = 0; i < MAX_DIG_SPOTS; i++) {
        if (g->digs[i].dug) {
            g->digs[i].reset_timer -= dt;
            if (g->digs[i].reset_timer <= 0.0f) g->digs[i].dug = false;
        }
    }

    // Swimming Fish AI
    for (int i = 0; i < MAX_FISH; i++) {
        if (!g->fish[i].active) continue;
        SwimmingFish *f = &g->fish[i];
        if (g->fish_state != FISH_IDLE && g->hooked_fish_idx == i) {
            v3 to_b = v3sub(v3mk(g->bobber_pos.x, f->pos.y, g->bobber_pos.z), f->pos);
            float d = hypotf(to_b.x, to_b.z);
            if (d > 0.35f) {
                f->yaw = atan2f(to_b.x, to_b.z);
                f->pos.x += (to_b.x / d) * 1.6f * dt;
                f->pos.z += (to_b.z / d) * 1.6f * dt;
            }
        } else {
            f->wander_t += dt * f->speed;
            v3 target = v3add(f->home_pos, v3mk(sinf(f->wander_t + (float)i) * 1.15f, 0.0f, cosf(f->wander_t * 0.8f) * 1.15f));
            v3 dv = v3sub(target, f->pos);
            if (hypotf(dv.x, dv.z) > 0.02f) f->yaw = atan2f(dv.x, dv.z);
            f->pos = v3lerp(f->pos, target, clampf(dt * 2.0f, 0.0f, 1.0f));
        }
    }

    // Fluttering Butterflies
    for (int i = 0; i < MAX_BUGS; i++) {
        IslandBug *bg = &g->bugs[i];
        if (!bg->active) {
            bg->respawn_t -= dt;
            if (bg->respawn_t <= 0.0f) bg->active = true;
            continue;
        }
        bg->flap_phase += dt * 16.0f;
        float bt = g->time_sec * 0.85f + (float)i * 2.1f;
        v3 tp = v3add(bg->home_pos, v3mk(sinf(bt) * 1.8f, 0.25f * sinf(bt * 2.7f), cosf(bt * 0.75f) * 1.8f));
        v3 dv = v3sub(tp, bg->pos);
        if (hypotf(dv.x, dv.z) > 0.01f) bg->yaw = atan2f(dv.x, dv.z);
        bg->pos = v3lerp(bg->pos, tp, clampf(dt * 2.5f, 0.0f, 1.0f));
    }

    // Wandering Friendly Villagers
    for (int i = 0; i < MAX_NPCS; i++) {
        VillagerNpc *n = &g->npcs[i];
        if (g->dialog_open && g->dialog_npc_idx == i) continue;
        if (n->pause_t > 0.0f) {
            n->pause_t -= dt;
            if (n->pause_t <= 0.0f) {
                n->anim = CANIM_IDLE;
                float ox = rng_frange(&g->rng, -2.4f, 2.4f);
                float oz = rng_frange(&g->rng, -2.4f, 2.4f);
                float gx = n->home_pos.x + ox, gz = n->home_pos.z + oz;
                if (clover_is_walkable(gx, gz)) n->goal_pos = v3mk(gx, clover_ground_y(gx, gz), gz);
            }
        } else {
            v3 dv = v3sub(n->goal_pos, n->pos);
            float d = hypotf(dv.x, dv.z);
            if (d < 0.25f) {
                n->anim = CANIM_IDLE;
                n->pause_t = rng_frange(&g->rng, 2.2f, 4.8f);
            } else {
                float nx = n->pos.x + (dv.x / d) * 1.35f * dt;
                float nz = n->pos.z + (dv.z / d) * 1.35f * dt;
                if (clover_is_walkable(nx, nz)) {
                    n->pos.x = nx; n->pos.z = nz;
                    n->pos.y = clover_ground_y(nx, nz);
                    n->yaw   = atan2f(dv.x, dv.z);
                    n->anim  = CANIM_WALK;
                } else {
                    n->pause_t = 1.5f;
                    n->anim = CANIM_IDLE;
                }
            }
        }
    }

    // Continuous Ambient Diorama VFX: Chimney Smoke + Waterfall Mist + Gentle Sea Breeze + Pollen/Fireflies!
    if (rng_f01(&g->rng) < dt * 3.5f) {
        v3 ch_p = v3mk(-0.95f, clover_ground_y(-2.4f, -6.8f) + 3.55f, -7.25f);
        spawn_vfx(g, 2, ch_p, v3mk(0.35f, 0.95f, 0.15f), v3mk(0,1,0), 2.2f, 0.28f, 245, 242, 238, 130);
    }
    if (rng_f01(&g->rng) < dt * 8.5f) {
        spawn_vfx(g, 2, v3mk(3.4f + rng_frange(&g->rng, -0.85f, 0.85f), 0.32f, -2.85f),
                  v3mk(rng_frange(&g->rng, -0.5f, 0.5f), rng_frange(&g->rng, 0.8f, 1.6f), rng_frange(&g->rng, 0.2f, 0.9f)),
                  v3mk(0,1,0), 0.75f, 0.26f, 225, 250, 255, 165);
    }
    if (rng_f01(&g->rng) < dt * 0.85f) {
        v3 bp = v3add(g->cam_focus, v3mk(rng_frange(&g->rng, -11.0f, -4.0f), rng_frange(&g->rng, 5.8f, 8.2f), rng_frange(&g->rng, -7.0f, 7.0f)));
        spawn_vfx(g, 3, bp, v3mk(3.6f, 0.14f * sinf(g->time_sec), 0.50f), v3mk(1, 0, 0.15f), 1.4f, 0.55f, 240, 250, 255, 34);
    }
    if (rng_f01(&g->rng) < dt * 5.5f) {
        // Warm golden pollen motes by day -> twinkling chartreuse-amber fireflies at sunset & night!
        bool is_eve = (g->tod_blend > 1.65f);
        float fx = rng_frange(&g->rng, -9.5f, 9.5f), fz = rng_frange(&g->rng, -8.0f, 10.5f);
        float fy = clover_ground_y(fx, fz) + rng_frange(&g->rng, 0.35f, 1.65f);
        spawn_vfx(g, 2, v3mk(fx, fy, fz),
                  v3mk(rng_frange(&g->rng, -0.28f, 0.28f), rng_frange(&g->rng, 0.08f, 0.32f), rng_frange(&g->rng, -0.28f, 0.28f)),
                  v3mk(0, 1, 0), 2.4f, is_eve ? 0.11f : 0.075f,
                  is_eve ? 215 : 255, is_eve ? 255 : 246, is_eve ? 95 : 195, is_eve ? 235 : 135);
    }

    // ========================================================================
    // 4. Populate RenderPacket (SceneUniforms + DynamicDraws + VFX + Crisp Leaf UI)
    // ========================================================================
    pkt->draw_count    = 0;
    pkt->vfx_vert_count= 0;
    pkt->ui_prim_count = 0;

    SceneUniforms *su = &pkt->scene_u;
    memset(su, 0, sizeof(*su));

    float aspect = g->screen_w / g->screen_h;
    su->view          = m4_look_at(g->cur_eye, g->cur_target, v3mk(0, 1, 0));
    su->proj          = m4_persp(g->cur_fov, aspect, 0.25f, 140.0f, g->zero_to_one_z);
    su->view_proj     = m4_mul(su->proj, su->view);
    su->inv_view_proj = m4_invert(su->view_proj);

    // Dynamic Time-of-Day Atmosphere (Morning -> Noon -> Sunset -> Starry Night)
    static const v3 k_sun_dirs[4] = {
        { 0.52f, 0.64f, 0.56f}, // Morning
        { 0.18f, 0.88f, 0.44f}, // Bright Noon
        {-0.68f, 0.28f, 0.68f}, // Golden Sunset
        {-0.38f, 0.62f,-0.68f}, // Crisp Moonlit Night
    };
    static const v3 k_sun_cols[4] = {
        {1.05f, 0.95f, 0.80f},
        {1.08f, 1.02f, 0.92f},
        {1.32f, 0.68f, 0.36f},
        {0.34f, 0.46f, 0.82f},
    };
    static const float k_sun_int[4] = {2.15f, 2.35f, 1.95f, 0.72f};
    static const v3 k_sky_zen[4] = {
        {0.08f, 0.42f, 0.88f},
        {0.06f, 0.38f, 0.92f},
        {0.18f, 0.18f, 0.52f},
        {0.015f,0.035f,0.11f},
    };
    static const v3 k_sky_hor[4] = {
        {0.58f, 0.86f, 0.98f},
        {0.48f, 0.84f, 0.98f},
        {1.18f, 0.54f, 0.34f},
        {0.04f, 0.09f, 0.22f},
    };
    static const float k_sky_mode[4] = {2.00f, 2.00f, 2.42f, 3.00f};

    int t0 = (int)floorf(clampf(g->tod_blend, 0.0f, 3.0f));
    int t1 = (t0 < 3) ? (t0 + 1) : 3;
    float tf = clampf(g->tod_blend - (float)t0, 0.0f, 1.0f);

    v3 sun_d   = v3norm(v3lerp(k_sun_dirs[t0], k_sun_dirs[t1], tf));
    v3 sun_c   = v3lerp(k_sun_cols[t0], k_sun_cols[t1], tf);
    float sun_i= lerpf(k_sun_int[t0], k_sun_int[t1], tf);
    v3 sky_z   = v3lerp(k_sky_zen[t0], k_sky_zen[t1], tf);
    v3 sky_h   = v3lerp(k_sky_hor[t0], k_sky_hor[t1], tf);
    float smode= lerpf(k_sky_mode[t0], k_sky_mode[t1], tf);

    bool in_closeup = (g->dialog_open || g->showoff_timer > 0.0f) && !g->free_cam;
    float sh_ext = in_closeup ? 8.5f : 15.0f;
    m4 lview = m4_look_at(v3add(g->cam_focus, v3mul(sun_d, 28.0f)), g->cam_focus, v3mk(0, 1, 0));
    m4 lproj = m4_ortho(-sh_ext, sh_ext, -sh_ext, sh_ext, 1.0f, 62.0f, g->zero_to_one_z);
    su->light_view_proj   = m4_mul(lproj, lview);
    su->cam_pos_time      = v4mk(g->cur_eye.x, g->cur_eye.y, g->cur_eye.z, g->time_sec);
    su->sun_dir_intensity = v4mk(sun_d.x, sun_d.y, sun_d.z, sun_i);
    su->sun_color_fog     = v4mk(sun_c.x, sun_c.y, sun_c.z, 0.013f);
    su->sky_zenith_pad    = v4mk(sky_z.x, sky_z.y, sky_z.z, g->free_cam ? 0.0f : WORLD_CURVE_K);
    su->sky_horizon_pad   = v4mk(sky_h.x, sky_h.y, sky_h.z, smode);
    su->screen_params     = v4mk(g->screen_w, g->screen_h, 1.0f / g->screen_w, 1.0f / g->screen_h);

    v4 sun_clip = m4_mul_v4(su->view_proj, v4mk(g->cur_eye.x + sun_d.x * 80.0f, g->cur_eye.y + sun_d.y * 80.0f, g->cur_eye.z + sun_d.z * 80.0f, 1.0f));
    float sun_vis = (sun_clip.w > 0.1f && smode < 2.7f) ? 0.65f : 0.0f;
    su->sun_screen_pos = v4mk(sun_clip.w > 0.1f ? (sun_clip.x / sun_clip.w * 0.5f + 0.5f) : 0.5f,
                              sun_clip.w > 0.1f ? (sun_clip.y / sun_clip.w * 0.5f + 0.5f) : 0.8f,
                              sun_vis, (smode > 2.5f) ? 0.62f : 0.36f);

    // Warm Island Point Lights (Cottage, Stall, Garland Arch, Pier Lantern + Placed Lanterns/Campfires!)
    float warm_boost = smoothstepf(1.5f, 2.8f, smode) * 1.35f + 0.22f;
    su->tile_overlay[0] = v4mk(-2.4f, clover_ground_y(-2.4f, -6.8f) + 1.2f, -5.2f, 1.4f * warm_boost);
    su->tile_overlay[1] = v4mk(-1.8f, clover_ground_y(-2.6f,  0.2f) + 1.4f,  0.6f, 1.25f * warm_boost);
    su->tile_overlay[2] = v4mk(-1.6f, clover_ground_y(-2.6f,  0.2f) + 2.3f,  3.8f, 1.15f * warm_boost);
    su->tile_overlay[3] = v4mk( 0.95f, 1.72f, 18.4f, 1.35f * warm_boost);
    int li = 4;
    for (int i = 0; i < MAX_DECORS && li < 8; i++) {
        if (!g->decors[i].active || g->decors[i].kind == 2) continue;
        float ly = (g->decors[i].kind == 0) ? 1.9f : 0.45f;
        float lw = (g->decors[i].kind == 0 ? 1.45f : 1.75f * (0.9f + 0.1f * sinf(g->time_sec * 9.0f + (float)i))) * warm_boost;
        su->tile_overlay[li++] = v4mk(g->decors[i].pos.x, g->decors[i].pos.y + ly, g->decors[i].pos.z, lw);
    }
    // Dynamic Water Ring-Ripple Sources (Bobber + Waterfall Plunge Pool!)
    if (g->fish_state != FISH_IDLE) {
        su->tile_overlay[8] = v4mk(g->bobber_pos.x, g->bobber_pos.y, g->bobber_pos.z, (g->fish_state == FISH_BITE) ? 1.8f : 1.0f);
    }
    su->tile_overlay[9]  = v4mk(3.4f, 0.24f, -2.85f, 1.0f);
    su->tile_overlay[40] = v4mk(sky_h.x * 0.85f, sky_h.y * 0.85f, sky_h.z * 0.90f, 1.0f);
    su->tile_overlay[41] = v4mk(g->cam_focus.x, g->cam_focus.z, v3len(v3sub(g->cur_eye, g->cur_target)), in_closeup ? 1.0f : 0.0f);

    if (g->has_walk_target) {
        su->hover_cursor = v4mk(g->walk_target.x, g->walk_target.z, 1.0f, 0.0f);
    }

    // Helper template for rigid props + soft ground-hugging contact shadow emitter
    DrawUniforms prop_u;
    memset(&prop_u, 0, sizeof(prop_u));
    prop_u.tint_outline = v4mk(1.0f, 1.0f, 1.0f, 0.85f);

    #define EMIT_CONTACT_OVAL(cx_, cz_, rad_, al_) do { \
        if (pkt->vfx_vert_count + 6 <= MAX_VFX_VERTS) { \
            float x0_ = (cx_) - (rad_), x1_ = (cx_) + (rad_); \
            float z0_ = (cz_) - (rad_), z1_ = (cz_) + (rad_); \
            float y00_ = clover_ground_y(x0_, z0_) + 0.022f, y10_ = clover_ground_y(x1_, z0_) + 0.022f; \
            float y11_ = clover_ground_y(x1_, z1_) + 0.022f, y01_ = clover_ground_y(x0_, z1_) + 0.022f; \
            uint8_t ca_ = (uint8_t)(al_); \
            VfxVertex *sv_ = &pkt->vfx_verts[pkt->vfx_vert_count]; \
            sv_[0] = (VfxVertex){x0_, y00_, z0_, 0, 0, {12, 18, 36, ca_}, 4.0f, 0.0f}; \
            sv_[1] = (VfxVertex){x1_, y10_, z0_, 1, 0, {12, 18, 36, ca_}, 4.0f, 0.0f}; \
            sv_[2] = (VfxVertex){x1_, y11_, z1_, 1, 1, {12, 18, 36, ca_}, 4.0f, 0.0f}; \
            sv_[3] = sv_[0]; sv_[4] = sv_[2]; \
            sv_[5] = (VfxVertex){x0_, y01_, z1_, 0, 1, {12, 18, 36, ca_}, 4.0f, 0.0f}; \
            pkt->vfx_vert_count += 6; \
        } \
    } while (0)

    // A. Draw Player Character + Soft Contact Shadow + Equipped Tool / Proud Show-Off Item!
    {
        DrawUniforms cu;
        memset(&cu, 0, sizeof(cu));
        cu.model = m4_mul(m4_translate(g->player_pos), m4_rot_y(g->player_yaw));
        cu.tint_outline  = v4mk(0.99f, 0.82f, 0.16f, 1.0f); // Sunny yellow tropical aloha shirt!
        cu.hair_color    = v4mk(0.24f, 0.15f, 0.12f, 1.0f); // Deep rich espresso-chestnut short bob!
        cu.custom_params = v4mk(0.0f, 10.0f, g->time_sec, 1.0f);

        ChibiAnim ca = {
            .anim = g->player_anim,
            .anim_time = g->player_anim_t,
            .move_speed = g->player_vel_mag,
            .tail_phase = 0.0f,
            .has_tail = false,
        };
        clover_compute_bones(&ca, g->time_sec, cu.bones);
        push_draw(pkt, MESH_CHAR_PLAYER, DRAW_FLAGS_DEFAULT, &cu);

        float hop = clampf(cu.bones[BONE_PELVIS].m[13], 0.0f, 0.4f);
        EMIT_CONTACT_OVAL(g->player_pos.x, g->player_pos.z, 0.38f + hop * 0.28f, 168.0f / (1.0f + hop * 3.2f));

        if (g->showoff_timer > 0.0f) {
            uint32_t smesh = (g->showoff_item == ITEM_FISH) ? MESH_CAUGHT_FISH
                           : (g->showoff_item == ITEM_BUTTERFLY ? MESH_BUTTERFLY : MESH_FOSSIL);
            v3 hold_off = v3mk(-sinf(g->player_yaw - 0.8f) * 0.34f,
                               1.26f + 0.04f * sinf(g->time_sec * 5.0f),
                               -cosf(g->player_yaw - 0.8f) * 0.34f);
            prop_u.model = m4_trs(v3add(g->player_pos, hold_off),
                                  v3mk(0.18f, g->player_yaw + 1.35f, 0.0f), v3mk(0.62f, 0.62f, 0.62f));
            prop_u.tint_outline = v4mk(1.0f, 0.68f, 0.18f, 0.9f);
            push_draw(pkt, smesh, DRAW_FLAGS_DEFAULT, &prop_u);
            prop_u.tint_outline = v4mk(1, 1, 1, 0.85f);
        } else if (g->equipped_tool != TOOL_HAND) {
            static const uint32_t tmeshes[TOOL_COUNT] = {0, MESH_TOOL_ROD, MESH_TOOL_NET, MESH_TOOL_SHOVEL, MESH_TOOL_AXE};
            m4 hand_world = m4_mul(cu.model, m4_mul(cu.bones[BONE_WEAPON], m4_translate(v3mk(-0.31f, 0.46f, 0.06f))));
            prop_u.model = hand_world;
            push_draw(pkt, tmeshes[g->equipped_tool], DRAW_FLAGS_DEFAULT, &prop_u);
        }
    }

    // B. Draw 3 Wandering Animal Villagers!
    for (int i = 0; i < MAX_NPCS; i++) {
        const VillagerNpc *n = &g->npcs[i];
        DrawUniforms nu;
        memset(&nu, 0, sizeof(nu));
        nu.model = m4_mul(m4_translate(n->pos), m4_rot_y(n->yaw));
        nu.tint_outline  = v4mk(n->cloth_r / 255.0f, n->cloth_g / 255.0f, n->cloth_b / 255.0f, 1.0f);
        nu.hair_color    = v4mk(n->accent_r / 255.0f, n->accent_g / 255.0f, n->accent_b / 255.0f, 1.0f);
        nu.custom_params = v4mk(0.0f, n->eye_hue, g->time_sec, 1.0f);

        ChibiAnim ca = {
            .anim = n->anim,
            .anim_time = n->anim_t,
            .move_speed = (n->anim == CANIM_WALK) ? 0.65f : 0.0f,
            .tail_phase = (float)(i + 1) * 1.9f,
            .has_tail = (i == 0 || i == 2),
        };
        clover_compute_bones(&ca, g->time_sec, nu.bones);
        push_draw(pkt, n->mesh_id, DRAW_FLAGS_DEFAULT, &nu);

        float nhop = clampf(nu.bones[BONE_PELVIS].m[13], 0.0f, 0.4f);
        EMIT_CONTACT_OVAL(n->pos.x, n->pos.z, 0.37f + nhop * 0.26f, 164.0f / (1.0f + nhop * 3.2f));
    }

    // C. Draw Trees & Hanging 3D Fruits
    for (int i = 0; i < g->tree_count; i++) {
        const IslandTree *t = &g->trees[i];
        if (!t->active) continue;
        float wob = (t->shake_timer > 0.0f) ? sinf(t->shake_timer * 42.0f) * 0.14f * (t->shake_timer / 0.55f) : 0.0f;
        prop_u.model = m4_trs(t->pos, v3mk(0.0f, t->yaw, wob), v3mk(t->scale, t->scale, t->scale));
        push_draw(pkt, t->is_palm ? MESH_TREE_PALM : MESH_TREE_OAK, DRAW_FLAGS_DEFAULT, &prop_u);
        EMIT_CONTACT_OVAL(t->pos.x, t->pos.z, (t->is_palm ? 0.52f : 0.72f) * t->scale, 86.0f);

        if (!t->is_palm && t->fruit_count > 0) {
            static const v3 f_offs[3] = {{-0.72f, 1.66f, 0.88f}, {0.72f, 1.68f, 0.86f}, {0.0f, 2.22f, 1.06f}};
            uint32_t fm = (t->fruit_kind == ITEM_ORANGE) ? MESH_FRUIT_ORANGE : MESH_FRUIT_APPLE;
            for (int f = 0; f < t->fruit_count && f < 3; f++) {
                v3 fp = m4_mul_pos(prop_u.model, f_offs[f]);
                DrawUniforms fu = prop_u;
                fu.model = m4_translate(fp);
                push_draw(pkt, fm, DRAW_FLAG_OUTLINE, &fu);
            }
        }
    }

    // D. Draw Money Rocks, Dig Spots, Ground Drops, Fish, Butterflies, Bobber & Placed Decors
    for (int i = 0; i < MAX_ROCKS; i++) {
        float shk = (g->rocks[i].hit_shake > 0.0f) ? sinf(g->rocks[i].hit_shake * 50.0f) * 0.08f : 0.0f;
        prop_u.model = m4_trs(v3add(g->rocks[i].pos, v3mk(shk, 0, 0)), v3mk(0, g->rocks[i].yaw, 0),
                              v3mk(g->rocks[i].scale, g->rocks[i].scale, g->rocks[i].scale));
        push_draw(pkt, MESH_ROCK_NODE, DRAW_FLAGS_DEFAULT, &prop_u);
        EMIT_CONTACT_OVAL(g->rocks[i].pos.x, g->rocks[i].pos.z, 0.68f * g->rocks[i].scale, 94.0f);
    }
    #undef EMIT_CONTACT_OVAL
    for (int i = 0; i < MAX_DIG_SPOTS; i++) {
        if (!g->digs[i].active) continue;
        prop_u.model = m4_translate(g->digs[i].pos);
        push_draw(pkt, g->digs[i].dug ? MESH_DIG_HOLE : MESH_DIG_CRACK, 0, &prop_u);
    }
    for (int i = 0; i < MAX_DROPS; i++) {
        if (!g->drops[i].active) continue;
        const GroundDrop *d = &g->drops[i];
        uint32_t dm = MESH_FRUIT_APPLE;
        if (d->item_kind == ITEM_ORANGE)    dm = MESH_FRUIT_ORANGE;
        if (d->item_kind == ITEM_SEASHELL)  dm = MESH_SEASHELL;
        if (d->item_kind == ITEM_FOSSIL)    dm = MESH_FOSSIL;
        if (d->item_kind == ITEM_COIN_BAG)  dm = MESH_COIN_BAG;
        float bob = 0.04f * sinf(g->time_sec * 3.2f + d->bob_phase);
        prop_u.model = m4_trs(v3add(d->pos, v3mk(0, bob, 0)), v3mk(0, d->yaw, 0), v3mk(1, 1, 1));
        push_draw(pkt, dm, DRAW_FLAGS_DEFAULT, &prop_u);
    }
    for (int i = 0; i < MAX_FISH; i++) {
        if (!g->fish[i].active) continue;
        float wig = sinf(g->time_sec * 8.5f + (float)i) * 0.16f;
        prop_u.model = m4_trs(g->fish[i].pos, v3mk(0, g->fish[i].yaw + wig, 0), v3mk(1.05f, 1.0f, 1.15f));
        prop_u.tint_outline = v4mk(1, 1, 1, 0.0f);
        push_draw(pkt, MESH_FISH_SHADOW, 0, &prop_u);
        prop_u.tint_outline = v4mk(1, 1, 1, 0.85f);
    }
    for (int i = 0; i < MAX_BUGS; i++) {
        if (!g->bugs[i].active) continue;
        const IslandBug *bg = &g->bugs[i];
        float flap = 0.55f + 0.45f * fabsf(sinf(bg->flap_phase));
        DrawUniforms bu = prop_u;
        bu.model = m4_trs(bg->pos, v3mk(0.25f, bg->yaw, 0.0f), v3mk(flap, 1.0f, 1.0f));
        if (bg->hue_variant == 1)      bu.tint_outline = v4mk(0.22f, 0.72f, 1.00f, 0.75f);
        else if (bg->hue_variant == 2) bu.tint_outline = v4mk(1.00f, 0.48f, 0.74f, 0.75f);
        else                           bu.tint_outline = v4mk(1.00f, 0.76f, 0.18f, 0.75f);
        push_draw(pkt, MESH_BUTTERFLY, DRAW_FLAG_OUTLINE, &bu);
    }
    if (g->fish_state != FISH_IDLE) {
        float dip = (g->fish_state == FISH_BITE) ? -0.22f : (g->fish_state == FISH_NIBBLE ? -0.08f : 0.03f * sinf(g->time_sec * 4.5f));
        v3 bp = (g->fish_state == FISH_CASTING) ? v3lerp(g->bobber_pos, g->bobber_start, g->fish_timer / 0.28f)
                                                : v3add(g->bobber_pos, v3mk(0, dip, 0));
        prop_u.model = m4_translate(bp);
        push_draw(pkt, MESH_BOBBER, DRAW_FLAG_OUTLINE, &prop_u);
        // Fishing line beam VFX from rod tip to bobber!
        v3 rod_tip = v3add(g->player_pos, v3mk(sinf(g->player_yaw) * 1.15f, 1.25f, cosf(g->player_yaw) * 1.15f));
        if (pkt->vfx_vert_count + 6 <= MAX_VFX_VERTS) {
            v3 side = v3mk(0.018f, 0.018f, 0.0f);
            VfxVertex *vv = &pkt->vfx_verts[pkt->vfx_vert_count];
            uint8_t lc[4] = {245, 250, 255, 210};
            vv[0] = (VfxVertex){rod_tip.x - side.x, rod_tip.y, rod_tip.z, 0, 0, {lc[0],lc[1],lc[2],lc[3]}, 3, 0};
            vv[1] = (VfxVertex){rod_tip.x + side.x, rod_tip.y, rod_tip.z, 1, 0, {lc[0],lc[1],lc[2],lc[3]}, 3, 0};
            vv[2] = (VfxVertex){bp.x + side.x, bp.y + 0.08f, bp.z,        1, 1, {lc[0],lc[1],lc[2],lc[3]}, 3, 0};
            vv[3] = vv[0]; vv[4] = vv[2];
            vv[5] = (VfxVertex){bp.x - side.x, bp.y + 0.08f, bp.z,        0, 1, {lc[0],lc[1],lc[2],lc[3]}, 3, 0};
            pkt->vfx_vert_count += 6;
        }
    }
    for (int i = 0; i < MAX_DECORS; i++) {
        if (!g->decors[i].active) continue;
        static const uint32_t dmeshes[3] = {MESH_DECOR_LANTERN, MESH_DECOR_CAMPFIRE, MESH_DECOR_BENCH};
        prop_u.model = m4_trs(g->decors[i].pos, v3mk(0, g->decors[i].yaw, 0), v3mk(1, 1, 1));
        prop_u.custom_params = v4mk(0, 0, 0.5f + 0.5f * sinf(g->time_sec * 6.5f + (float)i), 0);
        push_draw(pkt, dmeshes[g->decors[i].kind % 3], DRAW_FLAGS_DEFAULT, &prop_u);
        prop_u.custom_params = v4mk(0, 0, 0, 0);
        // Crackling Campfire Ember Sparks!
        if (g->decors[i].kind == 1 && rng_f01(&g->rng) < dt * 7.0f) {
            spawn_vfx(g, 2, v3add(g->decors[i].pos, v3mk(rng_frange(&g->rng,-0.15f,0.15f), 0.35f, rng_frange(&g->rng,-0.15f,0.15f))),
                      v3mk(rng_frange(&g->rng,-0.3f,0.3f), rng_frange(&g->rng,1.1f,2.0f), rng_frange(&g->rng,-0.3f,0.3f)),
                      v3mk(0,1,0), 0.65f, 0.12f, 255, 178, 64, 245);
        }
    }

    // E. Emit Active VFX Particles into Dynamic Buffer
    v3 cam_r = v3mk(su->view.m[0], su->view.m[4], su->view.m[8]);
    v3 cam_u = v3mk(su->view.m[1], su->view.m[5], su->view.m[9]);
    for (int i = 0; i < 256; i++) {
        if (!g->particles[i].active) continue;
        g->particles[i].age += dt;
        if (g->particles[i].age >= g->particles[i].life) { g->particles[i].active = false; continue; }
        if (pkt->vfx_vert_count + 6 > MAX_VFX_VERTS) continue;

        g->particles[i].pos = v3add(g->particles[i].pos, v3mul(g->particles[i].vel, dt));
        float u = g->particles[i].age / g->particles[i].life;
        uint8_t alpha = (uint8_t)((float)g->particles[i].rgba[3] * sinf(u * PI_F));
        float sz = g->particles[i].size * (g->particles[i].kind == 1 ? (0.4f + 1.1f * u) : 1.0f);

        v3 ax1 = cam_r, ax2 = cam_u;
        if (g->particles[i].kind == 1) { ax1 = v3mk(1, 0, 0); ax2 = v3mk(0, 0, 1); }
        if (g->particles[i].kind == 3) { ax1 = v3mul(v3norm(g->particles[i].vel), 2.4f); ax2 = v3mk(0, 0.18f, 0); }

        v3 c = g->particles[i].pos;
        v3 p0 = v3add(c, v3add(v3mul(ax1, -sz), v3mul(ax2, -sz)));
        v3 p1 = v3add(c, v3add(v3mul(ax1,  sz), v3mul(ax2, -sz)));
        v3 p2 = v3add(c, v3add(v3mul(ax1,  sz), v3mul(ax2,  sz)));
        v3 p3 = v3add(c, v3add(v3mul(ax1, -sz), v3mul(ax2,  sz)));
        uint8_t *rc = g->particles[i].rgba;
        VfxVertex *vv = &pkt->vfx_verts[pkt->vfx_vert_count];
        vv[0] = (VfxVertex){p0.x,p0.y,p0.z, 0,0, {rc[0],rc[1],rc[2],alpha}, g->particles[i].kind, u};
        vv[1] = (VfxVertex){p1.x,p1.y,p1.z, 1,0, {rc[0],rc[1],rc[2],alpha}, g->particles[i].kind, u};
        vv[2] = (VfxVertex){p2.x,p2.y,p2.z, 1,1, {rc[0],rc[1],rc[2],alpha}, g->particles[i].kind, u};
        vv[3] = vv[0]; vv[4] = vv[2];
        vv[5] = (VfxVertex){p3.x,p3.y,p3.z, 0,1, {rc[0],rc[1],rc[2],alpha}, g->particles[i].kind, u};
        pkt->vfx_vert_count += 6;
    }

    // ========================================================================
    // 5. Tactile Cozy Rounded Cream & Leaf-Green Vector HUD
    // ========================================================================
    float s = g->ui_scale;
    float top_y = clover_top_y(g);
    uint32_t cream_bg  = rgba8(252, 246, 228, 236);
    uint32_t wood_bdr  = rgba8(198, 158, 106, 255);
    uint32_t brown_txt = rgba8(84,  54,  34,  255);
    uint32_t leaf_bg   = rgba8(72,  188, 108, 245);
    uint32_t leaf_bdr  = rgba8(245, 252, 235, 255);
    uint32_t gold_bg   = rgba8(248, 198, 64,  245);

    // Top-Left: Island Badge + Coin Counter + Pockets Count!
    float lx = g->safe_left + 14.0f * s;
    ui_push_panel(pkt, lx, top_y, lx + 444.0f * s, top_y + 42.0f * s, cream_bg, wood_bdr, 20.0f * s, 2.5f * s);
    char hud_top[64];
    snprintf(hud_top, sizeof(hud_top), "CLOVER COVE  %d COINS  BAG %d/%d", g->coins, g->pocket_count, MAX_POCKETS);
    ui_push_text(pkt, lx + 16.0f * s, top_y + 13.0f * s, 14.5f * s, hud_top, brown_txt, false);

    // Top-Right Pills: [INV-Y / NORM-Y]  [MORNING/NOON/SUNSET/NIGHT]  [FREE CAM]  [SND]
    float rx = g->screen_w - g->safe_right - 14.0f * s;
    ui_push_panel(pkt, rx - 68.0f * s, top_y, rx, top_y + 40.0f * s,
                  g->audio_muted ? rgba8(180,90,90,230) : cream_bg, wood_bdr, 20.0f * s, 2.0f * s);
    ui_push_text(pkt, rx - 34.0f * s, top_y + 13.0f * s, 14.0f * s, g->audio_muted ? "MUTE" : "SND", brown_txt, true);
    rx -= 76.0f * s;

    ui_push_panel(pkt, rx - 100.0f * s, top_y, rx, top_y + 40.0f * s,
                  g->free_cam ? gold_bg : cream_bg, wood_bdr, 20.0f * s, 2.0f * s);
    ui_push_text(pkt, rx - 50.0f * s, top_y + 13.0f * s, 14.0f * s, g->free_cam ? "ISLAND" : "FREECAM", brown_txt, true);
    rx -= 108.0f * s;

    ui_push_panel(pkt, rx - 110.0f * s, top_y, rx, top_y + 40.0f * s, leaf_bg, leaf_bdr, 20.0f * s, 2.0f * s);
    ui_push_text(pkt, rx - 55.0f * s, top_y + 13.0f * s, 14.0f * s, k_tod_names[g->tod_preset], rgba8(255,255,255,255), true);

    {
        float ix0, iy0, ix1, iy1;
        clover_inv_y_rect(g, &ix0, &iy0, &ix1, &iy1);
        uint32_t inv_bg  = g->ctrl_invert_y ? gold_bg : rgba8(92, 192, 248, 245);
        uint32_t inv_bdr = g->ctrl_invert_y ? wood_bdr : leaf_bdr;
        uint32_t inv_txt = g->ctrl_invert_y ? brown_txt : rgba8(255, 255, 255, 255);
        ui_push_panel(pkt, ix0, iy0, ix1, iy1, inv_bg, inv_bdr, 20.0f * s, 2.2f * s);
        ui_push_text(pkt, (ix0 + ix1) * 0.5f, iy0 + 13.0f * s, 14.0f * s, g->ctrl_invert_y ? "INV-Y" : "NORM-Y", inv_txt, true);
    }

    // Virtual Left & Right Thumbstick Indicators while Touch Dragging in Parallel!
    if (g->touch_stick_active) {
        float r0 = 44.0f * s, r1 = 20.0f * s;
        ui_push_panel(pkt, g->touch_stick_ox - r0, g->touch_stick_oy - r0, g->touch_stick_ox + r0, g->touch_stick_oy + r0,
                      rgba8(255,255,255,70), rgba8(255,255,255,160), r0, 2.0f * s);
        float px = g->touch_stick_ox + g->touch_stick_dx, py = g->touch_stick_oy + g->touch_stick_dy;
        ui_push_panel(pkt, px - r1, py - r1, px + r1, py + r1, leaf_bg, leaf_bdr, r1, 2.0f * s);
    }
    if (g->right_stick_active) {
        float r0 = 44.0f * s, r1 = 20.0f * s;
        ui_push_panel(pkt, g->right_stick_ox - r0, g->right_stick_oy - r0, g->right_stick_ox + r0, g->right_stick_oy + r0,
                      rgba8(255,255,255,70), rgba8(255,255,255,160), r0, 2.0f * s);
        float px = g->right_stick_ox + g->right_stick_dx, py = g->right_stick_oy + g->right_stick_dy;
        ui_push_panel(pkt, px - r1, py - r1, px + r1, py + r1, gold_bg, wood_bdr, r1, 2.0f * s);
    }

    // Celebratory Catch Show-Off Banner!
    if (g->showoff_timer > 0.0f && g->showoff_title) {
        float bw = 460.0f * s, bh = 78.0f * s;
        float bx0 = (g->screen_w - bw) * 0.5f, by0 = g->screen_h * 0.70f;
        ui_push_panel(pkt, bx0, by0, bx0 + bw, by0 + bh, cream_bg, gold_bg, 24.0f * s, 4.0f * s);
        ui_push_text(pkt, g->screen_w * 0.5f, by0 + 16.0f * s, 18.0f * s, g->showoff_title, rgba8(46, 138, 78, 255), true);
        ui_push_text(pkt, g->screen_w * 0.5f, by0 + 44.0f * s, 14.0f * s, g->showoff_pun, brown_txt, true);
    }

    // Cozy Villager Speech Bubble
    if (g->dialog_open && g->dialog_line1) {
        float dw = clampf(540.0f * s, 280.0f, g->screen_w - 32.0f * s);
        float dh = 92.0f * s;
        float dx0 = (g->screen_w - dw) * 0.5f;
        float dy0 = g->screen_h - g->safe_bottom - dh - 72.0f * s;
        ui_push_panel(pkt, dx0, dy0, dx0 + dw, dy0 + dh, cream_bg, wood_bdr, 26.0f * s, 3.0f * s);
        // Cute speaker name pill overlapping top-left edge!
        ui_push_panel(pkt, dx0 + 18.0f * s, dy0 - 15.0f * s, dx0 + 148.0f * s, dy0 + 15.0f * s,
                      rgba8(244, 126, 98, 255), leaf_bdr, 15.0f * s, 2.0f * s);
        ui_push_text(pkt, dx0 + 83.0f * s, dy0 - 7.0f * s, 14.0f * s, g->dialog_speaker, rgba8(255,255,255,255), true);

        char b1[96] = {0}, b2[96] = {0};
        int l1 = (int)strlen(g->dialog_line1);
        int n1 = g->dialog_char_revealed < l1 ? g->dialog_char_revealed : l1;
        if (n1 > 94) n1 = 94;
        memcpy(b1, g->dialog_line1, (size_t)n1);
        if (g->dialog_char_revealed > l1 && g->dialog_line2) {
            int n2 = g->dialog_char_revealed - l1;
            int l2 = (int)strlen(g->dialog_line2);
            if (n2 > l2) n2 = l2;
            if (n2 > 94) n2 = 94;
            memcpy(b2, g->dialog_line2, (size_t)n2);
        }
        ui_push_text(pkt, dx0 + 22.0f * s, dy0 + 24.0f * s, 14.5f * s, b1, brown_txt, false);
        if (b2[0]) ui_push_text(pkt, dx0 + 22.0f * s, dy0 + 52.0f * s, 14.5f * s, b2, brown_txt, false);
    }

    // Fishing Bite Alert Pill!
    if (g->fish_state == FISH_BITE) {
        float aw = 260.0f * s;
        ui_push_panel(pkt, (g->screen_w - aw) * 0.5f, top_y + 52.0f * s, (g->screen_w + aw) * 0.5f, top_y + 90.0f * s,
                      gold_bg, leaf_bdr, 19.0f * s, 3.0f * s);
        ui_push_text(pkt, g->screen_w * 0.5f, top_y + 63.0f * s, 16.0f * s, "! FISH ON! PRESS A / ACT !", brown_txt, true);
    } else if (g->toast_timer > 0.0f && g->toast_msg) {
        float tw = 340.0f * s;
        ui_push_panel(pkt, (g->screen_w - tw) * 0.5f, top_y + 50.0f * s, (g->screen_w + tw) * 0.5f, top_y + 82.0f * s,
                      cream_bg, wood_bdr, 16.0f * s, 2.0f * s);
        ui_push_text(pkt, g->screen_w * 0.5f, top_y + 59.0f * s, 13.5f * s, g->toast_msg, brown_txt, true);
    }

    // Bottom 5-Tool Quick Bar + [DECOR] + Big Tactile [ACT] Button!
    float bot_y = g->screen_h - g->safe_bottom - 14.0f * s;
    float bar_w = 340.0f * s;
    float bar_x0 = g->safe_left + 16.0f * s;
    if (g->screen_w > 740.0f * s) bar_x0 = (g->screen_w - bar_w) * 0.5f - 40.0f * s;

    ui_push_panel(pkt, bar_x0 - 4.0f * s, bot_y - 46.0f * s, bar_x0 + bar_w + 4.0f * s, bot_y,
                  cream_bg, wood_bdr, 22.0f * s, 2.5f * s);
    float slot_w = bar_w / 5.0f;
    for (int t = 0; t < TOOL_COUNT; t++) {
        float sx0 = bar_x0 + (float)t * slot_w + 3.0f * s;
        bool sel = (g->equipped_tool == t);
        if (sel) {
            ui_push_panel(pkt, sx0, bot_y - 41.0f * s, sx0 + slot_w - 6.0f * s, bot_y - 5.0f * s,
                          leaf_bg, leaf_bdr, 17.0f * s, 2.0f * s);
        }
        ui_push_text(pkt, sx0 + (slot_w - 6.0f * s) * 0.5f, bot_y - 29.0f * s, 12.5f * s,
                     k_tool_names[t], sel ? rgba8(255,255,255,255) : brown_txt, true);
    }

    float act_r  = 40.0f * s;
    float act_cx = g->screen_w - g->safe_right - 18.0f * s - act_r;
    float act_cy = bot_y - act_r;
    ui_push_panel(pkt, act_cx - act_r - 104.0f * s, bot_y - 44.0f * s, act_cx - act_r - 8.0f * s, bot_y - 4.0f * s,
                  gold_bg, leaf_bdr, 20.0f * s, 2.5f * s);
    ui_push_text(pkt, act_cx - act_r - 56.0f * s, bot_y - 30.0f * s, 12.5f * s, "X DECOR", brown_txt, true);

    ui_push_panel(pkt, act_cx - act_r, act_cy - act_r, act_cx + act_r, act_cy + act_r,
                  leaf_bg, leaf_bdr, act_r, 3.5f * s);
    ui_push_text(pkt, act_cx, act_cy - 8.0f * s, 16.5f * s, pad.connected ? "A ACT" : "ACT", rgba8(255,255,255,255), true);
}
