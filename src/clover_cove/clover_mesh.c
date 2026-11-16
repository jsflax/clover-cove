#include "clover.h"

// ============================================================================
// Analytic Island Heightfield & Walkability Queries
// ============================================================================
static inline float river_center_x(float wz) {
    return 3.4f + 0.85f * sinf((wz + 3.1f) * 0.30f);
}

static float raw_terrain_y(float wx, float wz) {
    // Oval emerald island with cozy coves
    float ang = atan2f(wz - 0.8f, wx);
    float cove_mod = 1.0f + 0.06f * cosf(ang * 3.0f) - 0.04f * sinf(ang * 2.0f);
    float r_isl = hypotf(wx * 0.86f, (wz - 0.8f) * 0.94f) / cove_mod;

    // Base meadow -> beach -> seabed profile
    float meadow_t = 1.0f - smoothstepf(10.8f, 13.0f, r_isl);
    float beach_t  = 1.0f - smoothstepf(12.6f, 16.0f, r_isl);
    float h = -1.65f + 1.92f * beach_t + 0.32f * meadow_t;

    // Subtle organic rolling turf undulation
    h += 0.045f * sinf(wx * 0.55f) * cosf(wz * 0.55f) * meadow_t;

    // Northern Second-Tier Clifftop Plateau (y = 2.42m) + Western Cobblestone Ramp
    float plat_x = 1.0f - smoothstepf(7.8f, 9.0f, fabsf(wx));
    float plat_z = (1.0f - smoothstepf(-3.8f, -2.7f, wz)) * smoothstepf(-11.2f, -9.8f, wz);
    float cliff_add = 1.82f * plat_x * plat_z;

    // Western cobblestone ramp smoothly bridging wz=-3.5 (top) to wz=0.5 (meadow)
    float ramp_x = smoothstepf(-7.1f, -6.5f, wx) * (1.0f - smoothstepf(-4.5f, -3.9f, wx));
    float ramp_z = 1.0f - clampf((wz + 3.5f) / 4.0f, 0.0f, 1.0f);
    float ramp_add = 1.82f * ramp_x * smoothstepf(0.0f, 1.0f, ramp_z) * smoothstepf(-10.5f, -9.2f, wz);
    h += (cliff_add > ramp_add) ? cliff_add : ramp_add;

    // Clifftop Pond Basin at (3.4, -6.8)
    float d_pond = hypotf(wx - 3.4f, wz + 6.8f);
    if (d_pond < 2.6f) {
        h -= 0.72f * (1.0f - smoothstepf(1.45f, 2.50f, d_pond));
    }
    // Waterfall spill notch at (3.4, -3.2)
    if (wz > -4.8f && wz < -2.4f) {
        float d_notch = fabsf(wx - 3.4f);
        if (d_notch < 1.3f) {
            h -= 0.30f * (1.0f - smoothstepf(0.6f, 1.25f, d_notch));
        }
    }

    // Winding River Channel from waterfall base (wz = -3.0) to southern sea (wz = 15.5)
    if (wz > -3.1f && wz < 16.0f) {
        float rx = river_center_x(wz);
        float dr = fabsf(wx - rx);
        float wz_in = smoothstepf(-3.1f, -2.3f, wz);
        if (dr < 2.1f) {
            h -= 0.92f * (1.0f - smoothstepf(1.05f, 2.05f, dr)) * wz_in;
        }
    }
    return h;
}

float clover_ground_y(float wx, float wz) {
    float h = raw_terrain_y(wx, wz);

    // Walkable Red Arched Wooden Bridge spanning river at wz in [2.1 .. 4.3]
    float br_cx = river_center_x(3.2f);
    if (wz >= 2.05f && wz <= 4.35f && fabsf(wx - br_cx) <= 2.35f) {
        float u = (wx - br_cx) / 2.35f;
        float bridge_y = 0.88f - 0.26f * u * u;
        if (bridge_y > h) h = bridge_y;
    }

    // Walkable Southern Wooden Fishing Pier extending out over the ocean!
    if (fabsf(wx) <= 1.12f && wz >= 12.6f && wz <= 18.8f) {
        float pier_y = 0.62f;
        if (pier_y > h) h = pier_y;
    }
    return h;
}

bool clover_is_water_at(float wx, float wz, float *out_water_y, bool *out_is_pond) {
    // Exclude bridge & pier decks
    float br_cx = river_center_x(3.2f);
    if (wz >= 2.05f && wz <= 4.35f && fabsf(wx - br_cx) <= 2.35f) return false;
    if (fabsf(wx) <= 1.15f && wz >= 12.6f && wz <= 18.8f) return false;

    float d_pond = hypotf(wx - 3.4f, wz + 6.8f);
    if (d_pond < 2.05f) {
        if (out_water_y) *out_water_y = 2.24f;
        if (out_is_pond) *out_is_pond = true;
        return true;
    }
    float th = raw_terrain_y(wx, wz);
    if (wz > -3.0f && wz < 13.8f && fabsf(wx - river_center_x(wz)) < 1.38f && th < 0.22f) {
        if (out_water_y) *out_water_y = 0.24f;
        if (out_is_pond) *out_is_pond = false;
        return true;
    }
    if (th < 0.02f) {
        if (out_water_y) *out_water_y = 0.0f;
        if (out_is_pond) *out_is_pond = false;
        return true;
    }
    return false;
}

bool clover_is_walkable(float wx, float wz) {
    if (fabsf(wx) > 18.0f || wz < -13.5f || wz > 19.0f) return false;
    if (clover_is_water_at(wx, wz, NULL, NULL)) return false;

    // Check steep cliff walls (except on bridge/pier/ramp)
    float h0 = clover_ground_y(wx, wz);
    if (h0 < 0.05f) return false;
    float hx = fabsf(clover_ground_y(wx + 0.35f, wz) - clover_ground_y(wx - 0.35f, wz));
    float hz = fabsf(clover_ground_y(wx, wz + 0.35f) - clover_ground_y(wx, wz - 0.35f));
    if (hx > 0.68f || hz > 0.68f) return false;

    // Building solid footprints: Cozy Player Cottage (-2.4, -6.8) & Cove Market Stall (-2.6, 0.2)
    if (fabsf(wx + 2.4f) < 1.95f && fabsf(wz + 6.8f) < 1.55f) return false;
    if (fabsf(wx + 2.6f) < 1.35f && fabsf(wz - 0.2f) < 0.95f) return false;

    return true;
}

// ============================================================================
// Sculpted Chibi Character Generator (Player + Mochi Cat + Puddles Frog + Biscuit Shiba)
// ============================================================================
static void build_chibi_character(MeshBuilder *b, int kind) {
    // kind: 0=Player, 1=Mochi (Calico Cat), 2=Puddles (Mint Frog), 3=Biscuit (Golden Shiba)
    uint8_t skin_r = 255, skin_g = 222, skin_b = 198;
    if (kind == 1) { skin_r = 252; skin_g = 244; skin_b = 230; } // Cream cat fur
    if (kind == 2) { skin_r = 122; skin_g = 224; skin_b = 156; } // Peppy mint frog
    if (kind == 3) { skin_r = 238; skin_g = 178; skin_b = 96;  } // Golden Shiba fur

    // Cute stubby legs & rounded shoes/paws
    static const float leg_prof[] = {0.0f, 0.058f, 0.5f, 0.064f, 1.0f, 0.072f};
    for (int side = 0; side < 2; side++) {
        float sx = (side == 0) ? 1.0f : -1.0f;
        uint8_t b_leg  = (side == 0) ? BONE_LEG_L  : BONE_LEG_R;
        uint8_t b_foot = (side == 0) ? BONE_FOOT_L : BONE_FOOT_R;

        mb_set_bone(b, b_foot, 0, 0.0f);
        if (kind == 0) {
            // Cozy cream & navy slip-on island sneakers
            mb_set_style(b, 44, 62, 118, MAT_PAINTED_TOY, 0.48f, 0.04f, 0.55f);
            mb_sphere(b, v3mk(sx * 0.10f, 0.064f, 0.024f), v3mk(0.066f, 0.056f, 0.092f), 8, 12, -2.0f);
            mb_set_style(b, 248, 245, 238, MAT_PAINTED_TOY, 0.42f, 0.0f, 0.40f);
            mb_sphere(b, v3mk(sx * 0.10f, 0.050f, 0.066f), v3mk(0.058f, 0.040f, 0.050f), 6, 10, -2.0f);
        } else {
            mb_set_style(b, skin_r, skin_g, skin_b, MAT_SKIN_FACE, 0.68f, 0.0f, 0.55f);
            mb_sphere(b, v3mk(sx * 0.10f, 0.06f, 0.02f), v3mk(0.064f, 0.055f, 0.082f), 8, 12, -2.0f);
        }

        mb_set_bone(b, b_leg, b_foot, 0.18f);
        if (kind == 0) {
            mb_set_style(b, 34, 52, 108, MAT_FABRIC_CAPE, 0.68f, 0.0f, 0.52f); // Deep navy indigo pants
        } else {
            mb_set_style(b, skin_r, skin_g, skin_b, MAT_SKIN_FACE, 0.68f, 0.0f, 0.50f);
        }
        mb_lathe(b, m4_translate(v3mk(sx * 0.098f, 0.11f, 0.0f)), leg_prof, 3, 10, 0.25f, 1.0f, 1.0f, 0.0f, 0, -2.0f);
    }

    // Plump Toy-Diorama Flared Chibi Tunic / Aloha Shirt (uv >= 5.0 triggers tropical print for heroine!)
    mb_set_bone(b, BONE_CHEST, BONE_PELVIS, 0.32f);
    static const float body_prof[] = {
        0.00f, 0.205f,
        0.18f, 0.214f,
        0.58f, 0.184f,
        0.88f, 0.146f,
        1.00f, 0.114f
    };
    mb_set_style(b, 248, 248, 250, MAT_FABRIC_CAPE, 0.58f, 0.04f, 0.60f);
    mb_lathe(b, m4_translate(v3mk(0.0f, 0.31f, 0.0f)), body_prof, 5, 16, 0.43f, 1.02f, 0.94f, 0.0f, 0, (kind == 0) ? 5.0f : 2.2f);

    // Cozy Aloha Collar & Buttons / Villager Neck Accents
    if (kind == 0) {
        // Sunny yellow camp/aloha V-collar + 3 cute wooden front buttons!
        mb_set_bone(b, BONE_CHEST, 0, 0.0f);
        mb_set_style(b, 252, 212, 46, MAT_PAINTED_TOY, 0.52f, 0.0f, 0.45f);
        for (int s = -1; s <= 1; s += 2) {
            float sx = (float)s;
            mb_box(b, m4_trs(v3mk(sx * 0.068f, 0.695f, 0.095f), v3mk(0.35f, sx * 0.32f, sx * -0.42f), v3mk(1, 1, 1)),
                   v3mk(0.052f, 0.016f, 0.048f), 1.0f);
        }
        mb_set_style(b, 96, 56, 34, MAT_PAINTED_TOY, 0.48f, 0.05f, 0.30f);
        mb_sphere(b, v3mk(0.0f, 0.60f, 0.162f), v3mk(0.017f, 0.017f, 0.012f), 5, 8, 0.0f);
        mb_sphere(b, v3mk(0.0f, 0.50f, 0.186f), v3mk(0.017f, 0.017f, 0.012f), 5, 8, 0.0f);
        mb_sphere(b, v3mk(0.0f, 0.40f, 0.204f), v3mk(0.017f, 0.017f, 0.012f), 5, 8, 0.0f);
    } else if (kind == 1) {
        // Mochi's cozy yellow ribbon bow at neck
        mb_set_style(b, 255, 208, 72, MAT_PAINTED_TOY, 0.45f, 0.05f, 0.45f);
        mb_sphere(b, v3mk(-0.055f, 0.70f, 0.135f), v3mk(0.052f, 0.036f, 0.028f), 6, 8, 0.0f);
        mb_sphere(b, v3mk( 0.055f, 0.70f, 0.135f), v3mk(0.052f, 0.036f, 0.028f), 6, 8, 0.0f);
    } else if (kind == 3) {
        // Biscuit's crisp white sailor kerchief
        mb_set_style(b, 250, 248, 245, MAT_PAINTED_TOY, 0.50f, 0.0f, 0.45f);
        static const float ker_p[] = {0.0f, 0.165f, 1.0f, 0.132f};
        mb_lathe(b, m4_translate(v3mk(0.0f, 0.66f, 0.0f)), ker_p, 2, 14, 0.075f, 1.08f, 1.05f, 0.0f, 0, 0.0f);
    }

    // Chibi Rounded Nubby Arms & Hands
    for (int side = 0; side < 2; side++) {
        float sx = (side == 0) ? 1.0f : -1.0f;
        uint8_t b_arm  = (side == 0) ? BONE_ARM_L  : BONE_ARM_R;
        uint8_t b_hand = (side == 0) ? BONE_HAND_L : BONE_HAND_R;
        v3 p_sh = v3mk(sx * 0.165f, 0.66f, 0.0f);
        v3 p_el = v3mk(sx * 0.255f, 0.53f, 0.02f);
        v3 p_wr = v3mk(sx * 0.305f, 0.43f, 0.05f);

        mb_set_bone(b, b_arm, BONE_CHEST, 0.15f);
        mb_set_style(b, 248, 248, 250, MAT_FABRIC_CAPE, 0.58f, 0.04f, 0.52f);
        mb_tube_bezier(b, p_sh, v3lerp(p_sh, p_el, 0.4f), v3lerp(p_sh, p_el, 0.8f), p_el, 0.062f, 0.056f, 1.0f, 4, 10, (kind == 0) ? 5.0f : 2.2f);

        mb_set_bone(b, b_hand, b_arm, 0.20f);
        mb_set_style(b, skin_r, skin_g, skin_b, MAT_SKIN_FACE, 0.65f, 0.0f, 0.52f);
        mb_tube_bezier(b, p_el, v3lerp(p_el, p_wr, 0.5f), p_wr, p_wr, 0.050f, 0.046f, 1.0f, 3, 8, -2.0f);
        mb_sphere(b, p_wr, v3mk(0.056f, 0.054f, 0.056f), 7, 10, -2.0f);
    }

    // Swaying Animal Tail (for Mochi Cat & Biscuit Shiba)
    if (kind == 1 || kind == 3) {
        mb_set_bone(b, BONE_TAIL_1, BONE_TAIL_2, 0.45f);
        if (kind == 1) {
            mb_set_style(b, 224, 146, 78, MAT_SKIN_FACE, 0.68f, 0.0f, 0.52f);
            mb_tube_bezier(b, v3mk(0, 0.38f, -0.14f), v3mk(0, 0.42f, -0.28f),
                           v3mk(0, 0.56f, -0.38f), v3mk(0, 0.68f, -0.34f), 0.048f, 0.042f, 1.0f, 6, 8, -2.0f);
        } else {
            mb_set_style(b, 248, 240, 224, MAT_SKIN_FACE, 0.68f, 0.0f, 0.52f);
            mb_sphere(b, v3mk(0.0f, 0.46f, -0.24f), v3mk(0.085f, 0.088f, 0.095f), 8, 10, -2.0f);
        }
    }

    // Big Adorable Chibi Head with Analytic SDF Face Mapping facing +Z!
    mb_set_bone(b, BONE_HEAD, BONE_CHEST, 0.05f);
    mb_set_style(b, skin_r, skin_g, skin_b, MAT_SKIN_FACE, 0.62f, 0.0f, 0.64f);
    {
        int rings = 20, segs = 28;
        uint32_t hv0 = b->vc;
        v3 hc = v3mk(0.0f, 0.965f, 0.015f);
        float rx = 0.258f, ry = 0.228f, rz = 0.242f;
        for (int r = 0; r <= rings; r++) {
            float vf = (float)r / (float)rings;
            float phi = vf * PI_F;
            float sp = sinf(phi), cp = cosf(phi);
            // Cute plump lower cheeks!
            float cheek = (cp < 0.1f && cp > -0.65f) ? 1.06f : 1.0f;
            for (int s = 0; s <= segs; s++) {
                float uf = (float)s / (float)segs;
                float th = uf * TAU_F - PI_F;
                float nx = sp * sinf(th), ny = cp, nz = sp * cosf(th);
                v3 p = v3mk(hc.x + nx * rx * cheek, hc.y + ny * ry, hc.z + nz * rz);
                float face_u = (nz > 0.25f) ? clampf(p.x / (rx * 1.02f) * 0.5f + 0.5f, 0.01f, 0.99f) : -2.0f;
                float face_v = (nz > 0.25f) ? clampf((p.y - (hc.y - ry * 0.94f)) / (ry * 1.88f), 0.01f, 0.99f) : 0.5f;
                mb_vert(b, p, v3mk(nx, ny, nz), face_u, face_v);
            }
        }
        int stride = segs + 1;
        for (int r = 0; r < rings; r++) {
            for (int s = 0; s < segs; s++) {
                uint32_t a = hv0 + r * stride + s, c = a + stride;
                mb_quad(b, a, c, c + 1, a + 1);
            }
        }
    }

    // Species Ears / Heroine's Sleek Short Chin-Length Dark-Brown Bob Hair!
    if (kind == 0) {
        // Sleek Chin-Length Bob with Straight-Across Blunt Bangs (no hat!)
        mb_set_style(b, 245, 245, 250, MAT_HAIR, 0.36f, 0.04f, 0.62f);
        enum { HRINGS = 18, HSEGS = 36 };
        v3 hp[HRINGS + 1][HSEGS + 1];
        for (int r = 0; r <= HRINGS; r++) {
            float vf = (float)r / (float)HRINGS;
            for (int s = 0; s <= HSEGS; s++) {
                float uf = (float)s / (float)HSEGS;
                float th = uf * TAU_F - PI_F;
                float abs_th = fabsf(th);
                float side_t = clampf((abs_th - 0.62f) / 0.17f, 0.0f, 1.0f);
                side_t = side_t * side_t * (3.0f - 2.0f * side_t);

                float bang_y = 1.018f - 0.008f * clampf(abs_th / 0.62f, 0.0f, 1.0f);
                float bob_y  = lerpf(0.698f, 0.736f, clampf((abs_th - 0.95f) / 1.9f, 0.0f, 1.0f));
                float hem_y  = lerpf(bang_y, bob_y, side_t);

                float y_top = 1.224f;
                float py, arc;
                if (vf <= 0.84f) {
                    float t = vf / 0.84f;
                    py  = y_top - (y_top - hem_y) * (1.0f - cosf(t * 0.5f * PI_F));
                    arc = sinf(t * (0.50f + 0.24f * side_t) * PI_F);
                } else {
                    float u_tk = (vf - 0.84f) / 0.16f;
                    py  = hem_y + 0.012f * u_tk;
                    arc = sinf((0.50f + 0.24f * side_t) * PI_F) * (1.0f - 0.18f * u_tk);
                }
                float hrx = 0.294f;
                float hrz = (cosf(th) >= 0.0f) ? 0.268f : 0.284f;
                hp[r][s] = v3mk(arc * hrx * sinf(th), py, 0.014f + arc * hrz * cosf(th));
            }
        }
        uint32_t bv0 = b->vc;
        for (int r = 0; r <= HRINGS; r++) {
            float vf = (float)r / (float)HRINGS;
            for (int s = 0; s <= HSEGS; s++) {
                float uf = (float)s / (float)HSEGS;
                v3 nrm;
                if (r == 0) {
                    nrm = v3mk(0.0f, 1.0f, 0.0f);
                } else {
                    int rp = r - 1;
                    int rn = (r < HRINGS) ? (r + 1) : r;
                    int sp_i = (s > 0) ? (s - 1) : (HSEGS - 1);
                    int sn_i = (s < HSEGS) ? (s + 1) : 1;
                    nrm = v3cross(v3sub(hp[rn][s], hp[rp][s]), v3sub(hp[r][sn_i], hp[r][sp_i]));
                }
                mb_vert(b, hp[r][s], nrm, uf, 1.0f - vf);
            }
        }
        int hstride = HSEGS + 1;
        for (int r = 0; r < HRINGS; r++) {
            for (int s = 0; s < HSEGS; s++) {
                uint32_t a = bv0 + r * hstride + s, c = a + hstride;
                mb_quad(b, a, c, c + 1, a + 1);
            }
        }
    } else if (kind == 1) {
        // Mochi Calico Cat Pointed Ears (left caramel, right chocolate, pink inner!)
        static const float ear_p[] = {0.0f, 0.095f, 0.6f, 0.062f, 1.0f, 0.010f};
        for (int s = -1; s <= 1; s += 2) {
            float sx = (float)s;
            if (s < 0) mb_set_style(b, 228, 142, 68, MAT_SKIN_FACE, 0.65f, 0.0f, 0.58f);
            else       mb_set_style(b, 96, 62, 48, MAT_SKIN_FACE, 0.65f, 0.0f, 0.58f);
            m4 exf = m4_trs(v3mk(sx * 0.165f, 1.13f, 0.01f), v3mk(0.08f, 0.0f, sx * -0.38f), v3mk(1.0f, 1.0f, 0.62f));
            mb_lathe(b, exf, ear_p, 3, 10, 0.175f, 1.0f, 1.0f, 0.0f, 0, -2.0f);
            mb_set_style(b, 255, 162, 178, MAT_PAINTED_TOY, 0.55f, 0.0f, 0.0f);
            mb_sphere(b, v3mk(sx * 0.175f, 1.18f, 0.048f), v3mk(0.038f, 0.052f, 0.018f), 5, 8, 0.0f);
        }
    } else if (kind == 2) {
        // Puddles Frog Cute Top Rounded Eye-Ridges + Pink Flower on Head!
        mb_set_style(b, skin_r, skin_g, skin_b, MAT_SKIN_FACE, 0.58f, 0.0f, 0.58f);
        for (int s = -1; s <= 1; s += 2) {
            mb_sphere(b, v3mk((float)s * 0.135f, 1.165f, 0.045f), v3mk(0.082f, 0.075f, 0.072f), 8, 12, -2.0f);
        }
        mb_set_style(b, 255, 118, 164, MAT_PAINTED_TOY, 0.45f, 0.0f, 0.45f);
        mb_sphere(b, v3mk(0.0f, 1.21f, 0.05f), v3mk(0.055f, 0.025f, 0.055f), 6, 10, 0.0f);
    } else if (kind == 3) {
        // Biscuit Shiba Perky Rounded-Triangle Dog Ears + Sailor Beret!
        static const float dear_p[] = {0.0f, 0.088f, 0.65f, 0.068f, 1.0f, 0.018f};
        mb_set_style(b, 218, 148, 68, MAT_SKIN_FACE, 0.65f, 0.0f, 0.58f);
        for (int s = -1; s <= 1; s += 2) {
            m4 exf = m4_trs(v3mk((float)s * 0.17f, 1.12f, 0.0f), v3mk(0.12f, 0.0f, (float)s * -0.42f), v3mk(1.0f, 1.0f, 0.68f));
            mb_lathe(b, exf, dear_p, 3, 10, 0.155f, 1.0f, 1.0f, 0.0f, 0, -2.0f);
        }
        mb_set_style(b, 248, 248, 252, MAT_PAINTED_TOY, 0.48f, 0.0f, 0.50f);
        mb_sphere(b, v3mk(0.0f, 1.19f, -0.02f), v3mk(0.125f, 0.048f, 0.125f), 7, 12, 0.0f);
        mb_set_style(b, 58, 124, 218, MAT_PAINTED_TOY, 0.45f, 0.0f, 0.40f);
        mb_sphere(b, v3mk(0.0f, 1.24f, -0.02f), v3mk(0.028f, 0.025f, 0.028f), 5, 8, 0.0f);
    }

    mb_set_bone(b, 0, 0, 0.0f);
}

// ============================================================================
// Cozy Architecture Helpers (Player Cottage, Cove Market Stall, Red Bridge, Pier)
// ============================================================================
static void build_player_cottage(MeshBuilder *b, v3 origin) {
    // Stone foundation
    mb_set_style(b, 168, 160, 152, MAT_MARBLE_GOLD, 0.55f, 0.05f, 0.35f);
    mb_box(b, m4_translate(v3add(origin, v3mk(0, 0.18f, 0))), v3mk(1.95f, 0.20f, 1.55f), 1.5f);
    // Front wooden porch step
    mb_set_style(b, 168, 118, 76, MAT_WOOD_BARK, 0.62f, 0.02f, 0.35f);
    mb_box(b, m4_translate(v3add(origin, v3mk(0, 0.12f, 1.78f))), v3mk(0.75f, 0.12f, 0.32f), 1.0f);

    // Warm cream stucco walls
    mb_set_style(b, 252, 242, 222, MAT_PAINTED_TOY, 0.68f, 0.0f, 0.42f);
    mb_box(b, m4_translate(v3add(origin, v3mk(0, 1.18f, 0))), v3mk(1.78f, 0.85f, 1.38f), 1.0f);

    // Cozy exposed timber corner beams
    mb_set_style(b, 128, 86, 56, MAT_WOOD_BARK, 0.72f, 0.0f, 0.38f);
    for (int sx = -1; sx <= 1; sx += 2) {
        for (int sz = -1; sz <= 1; sz += 2) {
            mb_box(b, m4_translate(v3add(origin, v3mk((float)sx * 1.76f, 1.18f, (float)sz * 1.36f))),
                   v3mk(0.09f, 0.86f, 0.09f), 1.0f);
        }
    }

    // Berry-Terracotta Pitched Gable Roof (2 angled shingle slopes + gable triangle fill)
    uint32_t rv_roof = b->vc;
    mb_set_style(b, 218, 74, 68, MAT_PAINTED_TOY, 0.56f, 0.02f, 0.45f);
    mb_box(b, m4_trs(v3add(origin, v3mk(0.0f, 2.38f,  0.80f)), v3mk( 0.58f, 0, 0), v3mk(1,1,1)), v3mk(2.08f, 0.11f, 1.12f), 2.0f);
    mb_box(b, m4_trs(v3add(origin, v3mk(0.0f, 2.38f, -0.80f)), v3mk(-0.58f, 0, 0), v3mk(1,1,1)), v3mk(2.08f, 0.11f, 1.12f), 2.0f);
    for (uint32_t k = rv_roof; k < b->vc; k++) b->v[k].u += 8.0f;
    // Ridge beam
    mb_set_style(b, 242, 198, 96, MAT_PAINTED_TOY, 0.45f, 0.10f, 0.40f);
    mb_box(b, m4_translate(v3add(origin, v3mk(0.0f, 2.96f, 0.0f))), v3mk(2.12f, 0.09f, 0.12f), 1.0f);
    // Gable end triangles
    mb_set_style(b, 250, 238, 216, MAT_PAINTED_TOY, 0.68f, 0.0f, 0.35f);
    for (int s = -1; s <= 1; s += 2) {
        float gx = origin.x + (float)s * 1.76f;
        uint32_t gv = b->vc;
        v3 n = v3mk((float)s, 0, 0);
        mb_vert(b, v3mk(gx, origin.y + 1.98f, origin.z - 1.42f), n, 0, 0);
        mb_vert(b, v3mk(gx, origin.y + 1.98f, origin.z + 1.42f), n, 1, 0);
        mb_vert(b, v3mk(gx, origin.y + 2.92f, origin.z),         n, 0.5f, 1);
        if (s > 0) mb_tri(b, gv, gv + 2, gv + 1);
        else       mb_tri(b, gv, gv + 1, gv + 2);
    }

    // Stone Chimney on Right Side (smoke puffs spawn above origin + (1.45, 3.45, -0.45)!)
    mb_set_style(b, 148, 138, 132, MAT_MARBLE_GOLD, 0.65f, 0.05f, 0.42f);
    mb_box(b, m4_translate(v3add(origin, v3mk(1.45f, 2.55f, -0.45f))), v3mk(0.28f, 0.85f, 0.28f), 1.0f);
    mb_box(b, m4_translate(v3add(origin, v3mk(1.45f, 3.42f, -0.45f))), v3mk(0.34f, 0.08f, 0.34f), 1.0f);

    // Arched Teal Wooden Front Door + Golden Wreath
    mb_set_style(b, 64, 158, 148, MAT_PAINTED_TOY, 0.48f, 0.05f, 0.45f);
    mb_box(b, m4_translate(v3add(origin, v3mk(0.0f, 0.92f, 1.40f))), v3mk(0.38f, 0.62f, 0.06f), 1.0f);
    mb_set_style(b, 248, 206, 86, MAT_ARMOR_METAL, 0.25f, 0.90f, 0.35f);
    mb_sphere(b, v3add(origin, v3mk(0.0f, 1.12f, 1.47f)), v3mk(0.14f, 0.14f, 0.04f), 6, 10, 0.0f);

    // Warm Glowing Cottage Windows (Front Left/Right + Side Windows!)
    for (int s = -1; s <= 1; s += 2) {
        mb_set_style(b, 128, 86, 56, MAT_WOOD_BARK, 0.65f, 0.0f, 0.40f);
        mb_box(b, m4_translate(v3add(origin, v3mk((float)s * 1.05f, 1.18f, 1.39f))), v3mk(0.34f, 0.34f, 0.05f), 1.0f);
        mb_set_style(b, 255, 214, 124, MAT_CRYSTAL_GLOW, 0.10f, 0.0f, 0.0f);
        mb_box(b, m4_translate(v3add(origin, v3mk((float)s * 1.05f, 1.18f, 1.42f))), v3mk(0.26f, 0.26f, 0.04f), 1.0f);
    }

    // Cute Red Mailbox out front!
    mb_set_style(b, 138, 96, 62, MAT_WOOD_BARK, 0.68f, 0.0f, 0.40f);
    mb_box(b, m4_translate(v3add(origin, v3mk(1.45f, 0.45f, 2.05f))), v3mk(0.06f, 0.45f, 0.06f), 1.0f);
    mb_set_style(b, 228, 68, 68, MAT_PAINTED_TOY, 0.42f, 0.08f, 0.48f);
    mb_box(b, m4_translate(v3add(origin, v3mk(1.45f, 0.96f, 2.05f))), v3mk(0.16f, 0.14f, 0.22f), 1.0f);
}

static void build_cove_market_stall(MeshBuilder *b, v3 origin) {
    // Cozy wooden counter & corner posts
    mb_set_style(b, 168, 116, 72, MAT_WOOD_BARK, 0.62f, 0.02f, 0.42f);
    mb_box(b, m4_translate(v3add(origin, v3mk(0, 0.48f, 0.28f))), v3mk(1.32f, 0.48f, 0.42f), 1.0f);
    mb_set_style(b, 244, 224, 184, MAT_PAINTED_TOY, 0.52f, 0.0f, 0.38f);
    mb_box(b, m4_translate(v3add(origin, v3mk(0, 0.98f, 0.28f))), v3mk(1.40f, 0.06f, 0.50f), 1.0f);

    mb_set_style(b, 132, 88, 54, MAT_WOOD_BARK, 0.68f, 0.0f, 0.40f);
    for (int sx = -1; sx <= 1; sx += 2) {
        for (int sz = -1; sz <= 1; sz += 2) {
            mb_box(b, m4_translate(v3add(origin, v3mk((float)sx * 1.24f, 1.15f, (float)sz * 0.62f))),
                   v3mk(0.065f, 1.15f, 0.065f), 1.0f);
        }
    }

    // Iconic Striped Emerald-and-Cream Awning Canopy (5 alternating stripes!)
    for (int i = 0; i < 5; i++) {
        float x_off = ((float)i - 2.0f) * 0.56f;
        if (i & 1) mb_set_style(b, 250, 244, 228, MAT_PAINTED_TOY, 0.52f, 0.0f, 0.42f);
        else       mb_set_style(b, 64, 182, 112, MAT_PAINTED_TOY, 0.52f, 0.0f, 0.42f);
        mb_box(b, m4_trs(v3add(origin, v3mk(x_off, 2.28f, 0.08f)), v3mk(0.24f, 0, 0), v3mk(1,1,1)),
               v3mk(0.285f, 0.07f, 0.92f), 1.0f);
    }

    // Displayed Coin Pouch, Fruit Crate & Glowing Market Lantern on Counter
    mb_set_style(b, 248, 204, 72, MAT_ARMOR_METAL, 0.32f, 0.85f, 0.42f);
    mb_sphere(b, v3add(origin, v3mk(-0.68f, 1.16f, 0.28f)), v3mk(0.16f, 0.18f, 0.16f), 7, 10, 0.0f);
    mb_set_style(b, 232, 64, 62, MAT_PAINTED_TOY, 0.28f, 0.05f, 0.42f);
    mb_sphere(b, v3add(origin, v3mk( 0.10f, 1.14f, 0.28f)), v3mk(0.13f, 0.13f, 0.13f), 7, 10, 0.0f);
    mb_set_style(b, 255, 218, 128, MAT_CRYSTAL_GLOW, 0.10f, 0.0f, 0.0f);
    mb_sphere(b, v3add(origin, v3mk( 0.88f, 1.22f, 0.28f)), v3mk(0.14f, 0.18f, 0.14f), 7, 10, 0.0f);
}

// ============================================================================
// Build Complete Clover Cove Static Diorama + Dynamic Game Slices
// ============================================================================
void app_build_world(const GameState *g, WorldGeometry *out_geo) {
    (void)g;
    memset(out_geo, 0, sizeof(*out_geo));
    MeshBuilder b = {0};

    // ------------------------------------------------------------------------
    // SLICE 0: ENG_MESH_ENV_MAIN — Two-Tier Island, Coral Beach, Plaza, Cottage, Stall, Bridge & Pier
    // ------------------------------------------------------------------------
    uint32_t env_i0 = b.ic;
    {
        const int nx = 96, nz = 96;
        const float min_x = -20.0f, max_x = 20.0f;
        const float min_z = -16.0f, max_z = 20.0f;
        uint32_t tv0 = b.vc;

        for (int iz = 0; iz <= nz; iz++) {
            float wz = lerpf(min_z, max_z, (float)iz / (float)nz);
            for (int ix = 0; ix <= nx; ix++) {
                float wx = lerpf(min_x, max_x, (float)ix / (float)nx);
                float wy = raw_terrain_y(wx, wz);

                float eps = 0.24f;
                float hL = raw_terrain_y(wx - eps, wz), hR = raw_terrain_y(wx + eps, wz);
                float hD = raw_terrain_y(wx, wz - eps), hU = raw_terrain_y(wx, wz + eps);
                v3 nrm = v3norm(v3mk(hL - hR, 2.0f * eps, hD - hU));

                // Unified MAT_CLIFF_TURF across terrain: plaza, ramp, coral sand & turf blend smoothly per-pixel in fs_main!
                mb_set_style(&b, 106, 188, 84, MAT_CLIFF_TURF, 0.74f, 0.0f, 0.0f);
                mb_vert(&b, v3mk(wx, wy, wz), nrm, wx * 0.25f, wz * 0.25f);
            }
        }
        int stride = nx + 1;
        for (int iz = 0; iz < nz; iz++) {
            for (int ix = 0; ix < nx; ix++) {
                uint32_t a = tv0 + iz * stride + ix, c = a + stride;
                mb_quad(&b, a, c, c + 1, a + 1);
            }
        }

        // Cozy Player Cottage on Clifftop Overlook (-2.4, y, -6.8)
        float cot_y = raw_terrain_y(-2.4f, -6.8f);
        build_player_cottage(&b, v3mk(-2.4f, cot_y - 0.04f, -6.8f));

        // Cove Striped Market Stall on Town Plaza (-2.6, y, 0.2)
        float stall_y = raw_terrain_y(-2.6f, 0.2f);
        build_cove_market_stall(&b, v3mk(-2.6f, stall_y - 0.02f, 0.2f));

        // Festive Town Plaza Lantern Garland Arch!
        mb_set_style(&b, 132, 88, 54, MAT_WOOD_BARK, 0.68f, 0.0f, 0.40f);
        mb_box(&b, m4_translate(v3mk(-3.9f, stall_y + 1.35f, 3.8f)), v3mk(0.08f, 1.35f, 0.08f), 1.0f);
        mb_box(&b, m4_translate(v3mk( 0.7f, stall_y + 1.35f, 3.8f)), v3mk(0.08f, 1.35f, 0.08f), 1.0f);
        mb_tube_bezier(&b, v3mk(-3.9f, stall_y + 2.65f, 3.8f), v3mk(-2.4f, stall_y + 2.15f, 3.8f),
                       v3mk(-0.8f, stall_y + 2.15f, 3.8f), v3mk( 0.7f, stall_y + 2.65f, 3.8f), 0.022f, 0.022f, 1.0f, 8, 6, 0.0f);
        for (int l = 0; l < 5; l++) {
            float lt = ((float)l + 0.5f) / 5.0f;
            float lx = lerpf(-3.9f, 0.7f, lt);
            float ly = stall_y + 2.65f - 0.38f * sinf(lt * PI_F);
            mb_set_style(&b, 255, 212, 118, MAT_CRYSTAL_GLOW, 0.1f, 0.0f, 0.0f);
            mb_sphere(&b, v3mk(lx, ly - 0.08f, 3.8f), v3mk(0.10f, 0.12f, 0.10f), 6, 8, 0.0f);
        }

        // Red-Lacquered Arched Wooden Bridge over the River!
        float br_cx = river_center_x(3.2f);
        for (int s = 0; s < 9; s++) {
            float u = ((float)s - 4.0f) / 4.2f;
            float px = br_cx + u * 2.25f;
            float py = 0.86f - 0.26f * u * u;
            mb_set_style(&b, 178, 124, 78, MAT_WOOD_BARK, 0.62f, 0.02f, 0.38f);
            mb_box(&b, m4_translate(v3mk(px, py - 0.06f, 3.2f)), v3mk(0.24f, 0.07f, 1.12f), 1.0f);
        }
        mb_set_style(&b, 218, 58, 56, MAT_PAINTED_TOY, 0.42f, 0.05f, 0.48f);
        for (int side = -1; side <= 1; side += 2) {
            float rz = 3.2f + (float)side * 1.05f;
            mb_tube_bezier(&b, v3mk(br_cx - 2.25f, 1.08f, rz), v3mk(br_cx - 0.8f, 1.42f, rz),
                           v3mk(br_cx + 0.8f,  1.42f, rz), v3mk(br_cx + 2.25f, 1.08f, rz), 0.055f, 0.055f, 1.0f, 8, 8, 0.0f);
            for (int p = -2; p <= 2; p++) {
                float u = (float)p * 0.48f;
                float px = br_cx + u * 2.25f;
                float py = 0.86f - 0.26f * u * u;
                mb_box(&b, m4_translate(v3mk(px, py + 0.22f, rz)), v3mk(0.065f, 0.26f, 0.065f), 1.0f);
            }
        }

        // Southern Wooden Fishing Pier with Timber Pilings & Tip Lantern!
        for (int p = 0; p < 12; p++) {
            float pz = 12.8f + (float)p * 0.50f;
            mb_set_style(&b, 168, 118, 74, MAT_WOOD_BARK, 0.65f, 0.02f, 0.38f);
            mb_box(&b, m4_translate(v3mk(0.0f, 0.58f, pz)), v3mk(1.10f, 0.06f, 0.22f), 1.0f);
            if ((p % 3) == 0) {
                for (int sx = -1; sx <= 1; sx += 2) {
                    mb_set_style(&b, 118, 82, 52, MAT_WOOD_BARK, 0.78f, 0.0f, 0.38f);
                    mb_box(&b, m4_translate(v3mk((float)sx * 0.98f, 0.15f, pz)), v3mk(0.11f, 0.85f, 0.11f), 1.0f);
                }
            }
        }
        mb_set_style(&b, 118, 82, 52, MAT_WOOD_BARK, 0.72f, 0.0f, 0.42f);
        mb_box(&b, m4_translate(v3mk(0.95f, 1.15f, 18.4f)), v3mk(0.07f, 0.55f, 0.07f), 1.0f);
        mb_set_style(&b, 255, 216, 122, MAT_CRYSTAL_GLOW, 0.1f, 0.0f, 0.0f);
        mb_sphere(&b, v3mk(0.95f, 1.72f, 18.4f), v3mk(0.13f, 0.16f, 0.13f), 7, 10, 0.0f);
    }
    out_geo->slices[ENG_MESH_ENV_MAIN] = (MeshSlice){env_i0, b.ic - env_i0, 0};

    // ------------------------------------------------------------------------
    // SLICE 1: ENG_MESH_WATER — Stylized Surf Ocean + Clifftop Pond + Waterfall + River
    // ------------------------------------------------------------------------
    uint32_t wat_i0 = b.ic;
    {
        // Ocean Plane with Shoreline Proximity encoded in v.col.r (0..255) for Stylized Surf!
        const int wnx = 80, wnz = 80;
        uint32_t wv0 = b.vc;
        for (int iz = 0; iz <= wnz; iz++) {
            float wz = lerpf(-38.0f, 38.0f, (float)iz / (float)wnz);
            for (int ix = 0; ix <= wnx; ix++) {
                float wx = lerpf(-38.0f, 38.0f, (float)ix / (float)wnx);
                float th = raw_terrain_y(wx, wz);
                float sh = smoothstepf(-1.25f, 0.06f, th);
                uint8_t sh_byte = (uint8_t)(clampf(sh, 0.0f, 1.0f) * 255.0f);
                mb_set_style(&b, sh_byte, 200, 220, 0, 0.1f, 0.0f, 0.0f);
                mb_vert(&b, v3mk(wx, 0.0f, wz), v3mk(0, 1, 0), wx * 0.15f, wz * 0.15f);
            }
        }
        int wstride = wnx + 1;
        for (int iz = 0; iz < wnz; iz++) {
            for (int ix = 0; ix < wnx; ix++) {
                uint32_t a = wv0 + iz * wstride + ix, c2 = a + wstride;
                mb_quad(&b, a, c2, c2 + 1, a + 1);
            }
        }

        // Clifftop Pond Surface at (3.4, 2.24, -6.8)
        uint32_t pv0 = b.vc;
        mb_set_style(&b, 140, 210, 230, 0, 0.1f, 0.0f, 0.0f);
        mb_vert(&b, v3mk(3.4f, 2.24f, -6.8f), v3mk(0, 1, 0), 0.5f, 0.5f);
        for (int s = 0; s <= 20; s++) {
            float ang = ((float)s / 20.0f) * TAU_F;
            mb_set_style(&b, 220, 210, 230, 0, 0.1f, 0.0f, 0.0f);
            mb_vert(&b, v3mk(3.4f + cosf(ang) * 2.25f, 2.24f, -6.8f + sinf(ang) * 2.25f), v3mk(0, 1, 0), cosf(ang), sinf(ang));
        }
        for (int s = 0; s < 20; s++) {
            mb_tri(&b, pv0, pv0 + 1 + ((s + 1) % 21), pv0 + 1 + s);
        }

        // Winding River Ribbon from Waterfall Base to Sea
        uint32_t rv0 = b.vc;
        for (int i = 0; i <= 28; i++) {
            float wz = lerpf(-3.2f, 14.5f, (float)i / 28.0f);
            float rx = river_center_x(wz);
            mb_set_style(&b, 215, 210, 230, 0, 0.1f, 0.0f, 0.0f);
            mb_vert(&b, v3mk(rx - 1.55f, 0.24f, wz), v3mk(0, 1, 0), 0.0f, (float)i * 0.3f);
            mb_set_style(&b, 90, 210, 230, 0, 0.1f, 0.0f, 0.0f);
            mb_vert(&b, v3mk(rx,         0.24f, wz), v3mk(0, 1, 0), 0.5f, (float)i * 0.3f);
            mb_set_style(&b, 215, 210, 230, 0, 0.1f, 0.0f, 0.0f);
            mb_vert(&b, v3mk(rx + 1.55f, 0.24f, wz), v3mk(0, 1, 0), 1.0f, (float)i * 0.3f);
        }
        for (int i = 0; i < 28; i++) {
            uint32_t b0 = rv0 + i * 3, b1 = b0 + 3;
            mb_quad(&b, b0, b1, b1 + 1, b0 + 1);
            mb_quad(&b, b0 + 1, b1 + 1, b1 + 2, b0 + 2);
        }

        // Cascading Frothy Waterfall Sheet (uv.x >= 9.5 triggers animated waterfall shader!)
        uint32_t fv0 = b.vc;
        mb_set_style(&b, 255, 255, 255, 0, 0.1f, 0.0f, 0.0f);
        for (int iy = 0; iy <= 8; iy++) {
            float t = (float)iy / 8.0f;
            float wy = lerpf(2.26f, 0.20f, t);
            float wz = -3.35f + 0.52f * sinf(t * PI_F * 0.5f);
            mb_vert(&b, v3mk(3.4f - 0.92f, wy, wz), v3mk(0, 0.25f, 0.97f), 10.0f, t * 2.2f);
            mb_vert(&b, v3mk(3.4f + 0.92f, wy, wz), v3mk(0, 0.25f, 0.97f), 10.8f, t * 2.2f);
        }
        for (int iy = 0; iy < 8; iy++) {
            uint32_t a = fv0 + iy * 2;
            mb_quad(&b, a, a + 2, a + 3, a + 1);
        }
    }
    out_geo->slices[ENG_MESH_WATER] = (MeshSlice){wat_i0, b.ic - wat_i0, 0};

    // ------------------------------------------------------------------------
    // SLICE 2: ENG_MESH_GRASS_BLADE + 5,400 Swaying Grass & Wildflower Instances!
    // ------------------------------------------------------------------------
    uint32_t gr_i0 = b.ic;
    mb_grass_tuft(&b);
    out_geo->slices[ENG_MESH_GRASS_BLADE] = (MeshSlice){gr_i0, b.ic - gr_i0, 0};

    uint32_t gseed = 0x51A7F00Du;
    uint32_t gc = 0;
    for (int att = 0; att < 18000 && gc < CLOVER_GRASS_COUNT; att++) {
        float wx = rng_frange(&gseed, -13.5f, 13.5f);
        float wz = rng_frange(&gseed, -11.5f, 13.0f);
        if (!clover_is_walkable(wx, wz)) continue;
        // Skip brick plaza, ramp, bridge & pier
        if (wx > -4.3f && wx < 1.1f && wz > -0.9f && wz < 4.5f) continue;
        if (wx > -6.9f && wx < -4.1f && wz > -3.7f && wz < 0.5f) continue;
        if (wz > 1.9f && wz < 4.5f && fabsf(wx - river_center_x(3.2f)) < 2.5f) continue;
        if (wz > 12.2f && fabsf(wx) < 1.5f) continue;
        float wy = raw_terrain_y(wx, wz);
        if (wy < 0.42f) continue; // Keep coral sand beach clean!

        float fl_roll = rng_f01(&gseed);
        float fl_type = 0.0f;
        if (fl_roll > 0.90f) fl_type = 3.0f;      // Pink Cosmos
        else if (fl_roll > 0.82f) fl_type = 2.0f; // Sky-Blue Hyacinth
        else if (fl_roll > 0.73f) fl_type = 1.0f; // Golden Tulip

        out_geo->grass[gc].pos_scale = v4mk(wx, wy - 0.02f, wz, rng_frange(&gseed, 0.68f, 1.25f));
        out_geo->grass[gc].rot_color = v4mk(rng_frange(&gseed, 0.0f, TAU_F),
                                            rng_frange(&gseed, 0.0f, TAU_F),
                                            fl_type,
                                            rng_frange(&gseed, 0.88f, 1.18f));
        gc++;
    }
    out_geo->grass_count = gc;

    // ------------------------------------------------------------------------
    // SLICES 3..6: 4 Chibi Rigged Characters!
    // ------------------------------------------------------------------------
    for (int k = 0; k < 4; k++) {
        uint32_t ci0 = b.ic;
        build_chibi_character(&b, k);
        out_geo->slices[MESH_CHAR_PLAYER + k] = (MeshSlice){ci0, b.ic - ci0, 0};
    }

    // SLICE: Contact Shadow Blob
    {
        uint32_t si0 = b.ic, sv0 = b.vc;
        mb_set_style(&b, 18, 22, 32, MAT_PAINTED_TOY, 1.0f, 0.0f, 0.0f);
        mb_vert(&b, v3mk(0, 0.02f, 0), v3mk(0, 1, 0), 0.5f, 0.5f);
        for (int s = 0; s <= 14; s++) {
            float a = ((float)s / 14.0f) * TAU_F;
            mb_vert(&b, v3mk(cosf(a) * 0.42f, 0.02f, sinf(a) * 0.42f), v3mk(0, 1, 0), 0.5f, 0.5f);
        }
        for (int s = 0; s < 14; s++) mb_tri(&b, sv0, sv0 + 1 + ((s + 1) % 15), sv0 + 1 + s);
        out_geo->slices[MESH_SHADOW_BLOB] = (MeshSlice){si0, b.ic - si0, 0};
    }

    // SLICE: Cozy Hardwood Fruit Tree (Single Watertight 3-Tier Puffball Crown!)
    {
        uint32_t ti0 = b.ic;
        static const float trk_p[] = {0.0f, 0.34f, 0.25f, 0.26f, 0.75f, 0.20f, 1.0f, 0.15f};
        mb_set_style(&b, 142, 92, 58, MAT_WOOD_BARK, 0.75f, 0.0f, 0.42f);
        mb_lathe(&b, m4_ident(), trk_p, 4, 12, 1.65f, 1.0f, 1.0f, 0.04f, 5, 0.0f);

        static const float oak_lobes[10][4] = {
            { 0.00f, 2.42f,  0.00f, 1.08f},
            {-0.36f, 2.28f,  0.28f, 0.88f},
            { 0.36f, 2.28f,  0.28f, 0.88f},
            { 0.00f, 2.26f, -0.38f, 0.90f},
            {-0.58f, 1.88f,  0.16f, 0.88f},
            {-0.46f, 1.84f,  0.44f, 0.76f},
            {-0.48f, 1.90f, -0.24f, 0.78f},
            { 0.58f, 1.88f,  0.14f, 0.88f},
            { 0.46f, 1.84f,  0.42f, 0.76f},
            { 0.48f, 1.90f, -0.24f, 0.78f},
        };
        v3 pivot = v3mk(0.0f, 2.02f, 0.08f);
        mb_set_style(&b, 92, 186, 72, MAT_FOLIAGE, 0.72f, 0.0f, 0.26f);
        int rings = 22, segs = 34;
        uint32_t cv0 = b.vc;
        for (int r = 0; r <= rings; r++) {
            float vf = (float)r / (float)rings, phi = vf * PI_F;
            float sp = sinf(phi), cp = cosf(phi);
            for (int s = 0; s <= segs; s++) {
                float uf = (float)s / (float)segs, th = uf * TAU_F;
                v3 d = v3norm(v3mk(sp * cosf(th), cp * 0.90f, sp * sinf(th)));
                float best_t = 0.75f;
                int best_l = 0;
                for (int l = 0; l < 10; l++) {
                    v3 lo = v3sub(v3mk(oak_lobes[l][0], oak_lobes[l][1], oak_lobes[l][2]), pivot);
                    float lr = oak_lobes[l][3];
                    float bd = v3dot(lo, d);
                    float disc = bd * bd - (v3dot(lo, lo) - lr * lr);
                    if (disc > 0.0f) {
                        float tl = bd + sqrtf(disc);
                        if (tl > best_t) { best_t = tl; best_l = l; }
                    }
                }
                float bump = 1.0f + 0.032f * sinf(th * 5.0f + phi * 4.0f) + 0.022f * cosf(th * 7.0f - phi * 3.0f);
                v3 wp = v3add(pivot, v3mul(d, best_t * bump));
                v3 lc = v3mk(oak_lobes[best_l][0], oak_lobes[best_l][1], oak_lobes[best_l][2]);
                v3 lobe_n = v3norm(v3sub(wp, lc));
                v3 dome_n = v3norm(v3sub(wp, v3mk(0.0f, 1.35f, 0.06f)));
                v3 uni_n  = v3norm(v3lerp(lobe_n, dome_n, 0.70f));
                float ht = clampf((wp.y - 1.18f) / 2.25f, 0.0f, 1.0f);
                float sh = 0.64f + 0.44f * smoothstepf(0.0f, 1.0f, ht);
                b.col[0] = (uint8_t)clampf(lerpf(52.0f, 126.0f, ht) * sh, 20.0f, 255.0f);
                b.col[1] = (uint8_t)clampf(lerpf(148.0f, 214.0f, ht) * sh, 40.0f, 255.0f);
                b.col[2] = (uint8_t)clampf(lerpf(80.0f, 66.0f, ht) * sh, 20.0f, 255.0f);
                mb_vert(&b, wp, uni_n, uf, vf);
            }
        }
        int cstride = segs + 1;
        for (int r = 0; r < rings; r++) {
            for (int s = 0; s < segs; s++) {
                uint32_t a = cv0 + r * cstride + s, c2 = a + cstride;
                mb_quad(&b, a, a + 1, c2 + 1, c2);
            }
        }
        out_geo->slices[MESH_TREE_OAK] = (MeshSlice){ti0, b.ic - ti0, 0};
    }

    // SLICE: Beach Coconut Palm Tree!
    {
        uint32_t pi0 = b.ic;
        mb_set_style(&b, 168, 122, 78, MAT_WOOD_BARK, 0.76f, 0.0f, 0.45f);
        mb_tube_bezier(&b, v3mk(0, 0, 0), v3mk(0.18f, 0.9f, 0.25f),
                       v3mk(0.28f, 1.8f, 0.42f), v3mk(0.22f, 2.55f, 0.48f), 0.25f, 0.13f, 1.0f, 8, 9, 0.0f);
        v3 crown = v3mk(0.22f, 2.55f, 0.48f);
        mb_set_style(&b, 86, 196, 78, MAT_FOLIAGE, 0.58f, 0.0f, 0.38f);
        for (int f = 0; f < 6; f++) {
            float ang = (float)f * (TAU_F / 6.0f) + 0.3f;
            float ca = cosf(ang), sa = sinf(ang);
            mb_tube_bezier(&b, crown,
                           v3add(crown, v3mk(ca * 0.55f, 0.35f, sa * 0.55f)),
                           v3add(crown, v3mk(ca * 1.15f, 0.05f, sa * 1.15f)),
                           v3add(crown, v3mk(ca * 1.55f, -0.55f, sa * 1.55f)),
                           0.18f, 0.03f, 0.25f, 6, 6, 0.0f);
        }
        mb_set_style(&b, 122, 82, 48, MAT_PAINTED_TOY, 0.65f, 0.0f, 0.40f);
        mb_sphere(&b, v3add(crown, v3mk( 0.16f, -0.16f, 0.12f)), v3mk(0.16f, 0.18f, 0.16f), 6, 8, 0.0f);
        mb_sphere(&b, v3add(crown, v3mk(-0.16f, -0.16f, 0.10f)), v3mk(0.16f, 0.18f, 0.16f), 6, 8, 0.0f);
        out_geo->slices[MESH_TREE_PALM] = (MeshSlice){pi0, b.ic - pi0, 0};
    }

    // SLICE: Ruby Apple & Sunny Orange Fruits
    for (int fk = 0; fk < 2; fk++) {
        uint32_t fi0 = b.ic;
        if (fk == 0) mb_set_style(&b, 236, 52, 56, MAT_PAINTED_TOY, 0.24f, 0.05f, 0.52f);
        else         mb_set_style(&b, 252, 148, 42, MAT_PAINTED_TOY, 0.30f, 0.05f, 0.52f);
        mb_sphere(&b, v3mk(0, 0.18f, 0), v3mk(0.18f, 0.175f, 0.18f), 8, 12, 0.0f);
        mb_set_style(&b, 84, 192, 74, MAT_PAINTED_TOY, 0.45f, 0.0f, 0.35f);
        mb_sphere(&b, v3mk(0.06f, 0.36f, 0), v3mk(0.08f, 0.03f, 0.045f), 5, 8, 0.0f);
        out_geo->slices[fk == 0 ? MESH_FRUIT_APPLE : MESH_FRUIT_ORANGE] = (MeshSlice){fi0, b.ic - fi0, 0};
    }

    // SLICE: Cozy Money Rock Boulder
    {
        uint32_t ri0 = b.ic;
        mb_set_style(&b, 156, 162, 174, MAT_MARBLE_GOLD, 0.68f, 0.08f, 0.48f);
        mb_sphere(&b, v3mk(0.0f, 0.32f, 0.0f), v3mk(0.62f, 0.46f, 0.56f), 6, 9, 0.0f);
        mb_sphere(&b, v3mk(0.28f, 0.22f, 0.22f), v3mk(0.36f, 0.28f, 0.34f), 5, 7, 0.0f);
        out_geo->slices[MESH_ROCK_NODE] = (MeshSlice){ri0, b.ic - ri0, 0};
    }

    // SLICE: Star Dig Crack & Dug Hole
    {
        uint32_t di0 = b.ic;
        mb_set_style(&b, 76, 48, 32, MAT_PAINTED_TOY, 0.9f, 0.0f, 0.0f);
        for (int k = 0; k < 3; k++) {
            mb_box(&b, m4_trs(v3mk(0, 0.03f, 0), v3mk(0, (float)k * 1.047f, 0), v3mk(1,1,1)),
                   v3mk(0.34f, 0.015f, 0.045f), 1.0f);
        }
        out_geo->slices[MESH_DIG_CRACK] = (MeshSlice){di0, b.ic - di0, 0};

        uint32_t hi0 = b.ic;
        static const float hole_p[] = {0.0f, 0.32f, 0.5f, 0.44f, 1.0f, 0.34f};
        mb_set_style(&b, 132, 88, 54, MAT_WOOD_BARK, 0.88f, 0.0f, 0.35f);
        mb_lathe(&b, m4_ident(), hole_p, 3, 12, 0.14f, 1.0f, 1.0f, 0.0f, 0, 0.0f);
        mb_set_style(&b, 38, 24, 18, MAT_PAINTED_TOY, 0.95f, 0.0f, 0.0f);
        mb_sphere(&b, v3mk(0, 0.04f, 0), v3mk(0.31f, 0.05f, 0.31f), 5, 10, 0.0f);
        out_geo->slices[MESH_DIG_HOLE] = (MeshSlice){hi0, b.ic - hi0, 0};
    }

    // SLICE: Ammonite Spiral Fossil, Golden Coin Pouch & Pink Beach Seashell
    {
        uint32_t fsi0 = b.ic;
        mb_set_style(&b, 74, 104, 168, MAT_PAINTED_TOY, 0.38f, 0.12f, 0.52f);
        mb_sphere(&b, v3mk(0, 0.22f, 0), v3mk(0.24f, 0.22f, 0.12f), 8, 12, 0.0f);
        mb_set_style(&b, 238, 226, 198, MAT_PAINTED_TOY, 0.45f, 0.05f, 0.35f);
        mb_sphere(&b, v3mk(0.02f, 0.22f, 0.08f), v3mk(0.14f, 0.13f, 0.07f), 6, 10, 0.0f);
        out_geo->slices[MESH_FOSSIL] = (MeshSlice){fsi0, b.ic - fsi0, 0};

        uint32_t bi0 = b.ic;
        mb_set_style(&b, 250, 204, 64, MAT_ARMOR_METAL, 0.28f, 0.88f, 0.52f);
        mb_sphere(&b, v3mk(0, 0.20f, 0), v3mk(0.20f, 0.21f, 0.20f), 8, 12, 0.0f);
        mb_set_style(&b, 228, 56, 56, MAT_PAINTED_TOY, 0.45f, 0.0f, 0.35f);
        mb_box(&b, m4_translate(v3mk(0, 0.38f, 0)), v3mk(0.08f, 0.04f, 0.08f), 1.0f);
        out_geo->slices[MESH_COIN_BAG] = (MeshSlice){bi0, b.ic - bi0, 0};

        uint32_t shi0 = b.ic;
        static const float sh_p[] = {0.0f, 0.06f, 0.5f, 0.22f, 1.0f, 0.03f};
        mb_set_style(&b, 255, 158, 176, MAT_PAINTED_TOY, 0.32f, 0.08f, 0.48f);
        mb_lathe(&b, m4_trs(v3mk(0, 0.10f, 0), v3mk(1.35f, 0, 0), v3mk(1,1,1)), sh_p, 3, 12, 0.32f, 1.0f, 0.55f, 0.12f, 7, 0.0f);
        out_geo->slices[MESH_SEASHELL] = (MeshSlice){shi0, b.ic - shi0, 0};
    }

    // SLICE: Underwater Fish Shadow, Caught Vibrant Fish, Fluttering Butterfly & Red/White Bobber
    {
        uint32_t fsh0 = b.ic;
        mb_set_style(&b, 14, 38, 56, MAT_PAINTED_TOY, 0.8f, 0.0f, 0.0f);
        mb_sphere(&b, v3mk(0, 0.0f, 0.05f), v3mk(0.16f, 0.04f, 0.38f), 6, 10, 0.0f);
        mb_box(&b, m4_trs(v3mk(0, 0.0f, -0.36f), v3mk(0, 0.78f, 0), v3mk(1,1,1)), v3mk(0.14f, 0.02f, 0.14f), 1.0f);
        out_geo->slices[MESH_FISH_SHADOW] = (MeshSlice){fsh0, b.ic - fsh0, 0};

        uint32_t cf0 = b.ic;
        mb_set_style(&b, 76, 212, 164, MAT_ARMOR_METAL, 0.22f, 0.65f, 0.55f);
        mb_sphere(&b, v3mk(0, 0.0f, 0), v3mk(0.16f, 0.24f, 0.42f), 8, 12, 0.0f);
        mb_set_style(&b, 248, 112, 92, MAT_PAINTED_TOY, 0.35f, 0.1f, 0.45f);
        mb_box(&b, m4_trs(v3mk(0, 0.0f, -0.44f), v3mk(0.78f, 0, 0), v3mk(1,1,1)), v3mk(0.03f, 0.18f, 0.18f), 1.0f);
        out_geo->slices[MESH_CAUGHT_FISH] = (MeshSlice){cf0, b.ic - cf0, 0};

        uint32_t bfi0 = b.ic;
        mb_set_style(&b, 250, 248, 250, MAT_FABRIC_CAPE, 0.35f, 0.0f, 0.42f);
        mb_box(&b, m4_trs(v3mk(-0.14f, 0, 0), v3mk(0, 0,  0.38f), v3mk(1,1,1)), v3mk(0.14f, 0.015f, 0.16f), 2.2f);
        mb_box(&b, m4_trs(v3mk( 0.14f, 0, 0), v3mk(0, 0, -0.38f), v3mk(1,1,1)), v3mk(0.14f, 0.015f, 0.16f), 2.2f);
        out_geo->slices[MESH_BUTTERFLY] = (MeshSlice){bfi0, b.ic - bfi0, 0};

        uint32_t bbi0 = b.ic;
        mb_set_style(&b, 238, 52, 52, MAT_PAINTED_TOY, 0.25f, 0.05f, 0.50f);
        mb_sphere(&b, v3mk(0, 0.08f, 0), v3mk(0.11f, 0.11f, 0.11f), 7, 10, 0.0f);
        mb_set_style(&b, 250, 250, 252, MAT_PAINTED_TOY, 0.25f, 0.05f, 0.50f);
        mb_sphere(&b, v3mk(0, -0.02f, 0), v3mk(0.105f, 0.08f, 0.105f), 6, 10, 0.0f);
        out_geo->slices[MESH_BOBBER] = (MeshSlice){bbi0, b.ic - bbi0, 0};
    }

    // SLICES: 4 Held Chibi Tools (built in local space around origin for BONE_WEAPON attachment!)
    {
        // Fishing Rod
        uint32_t r0 = b.ic;
        mb_set_style(&b, 164, 112, 66, MAT_WOOD_BARK, 0.55f, 0.05f, 0.45f);
        mb_tube_bezier(&b, v3mk(0,0,-0.15f), v3mk(0,0.22f,0.35f), v3mk(0,0.48f,0.85f), v3mk(0,0.68f,1.35f), 0.028f, 0.010f, 1.0f, 6, 6, 0.0f);
        mb_set_style(&b, 238, 194, 78, MAT_ARMOR_METAL, 0.25f, 0.90f, 0.40f);
        mb_sphere(&b, v3mk(0.04f, 0.06f, 0.12f), v3mk(0.05f, 0.05f, 0.05f), 5, 8, 0.0f);
        out_geo->slices[MESH_TOOL_ROD] = (MeshSlice){r0, b.ic - r0, 0};

        // Bug Net
        uint32_t n0 = b.ic;
        mb_set_style(&b, 164, 112, 66, MAT_WOOD_BARK, 0.58f, 0.05f, 0.45f);
        mb_tube_bezier(&b, v3mk(0,0,-0.15f), v3mk(0,0.12f,0.25f), v3mk(0,0.24f,0.55f), v3mk(0,0.35f,0.82f), 0.026f, 0.022f, 1.0f, 4, 6, 0.0f);
        mb_set_style(&b, 246, 248, 252, MAT_PAINTED_TOY, 0.55f, 0.0f, 0.45f);
        static const float net_p[] = {0.0f, 0.04f, 0.4f, 0.20f, 1.0f, 0.26f};
        mb_lathe(&b, m4_translate(v3mk(0, 0.12f, 1.06f)), net_p, 3, 12, 0.34f, 1.0f, 1.0f, 0.0f, 0, 0.0f);
        out_geo->slices[MESH_TOOL_NET] = (MeshSlice){n0, b.ic - n0, 0};

        // Shovel
        uint32_t s0 = b.ic;
        mb_set_style(&b, 168, 116, 68, MAT_WOOD_BARK, 0.58f, 0.05f, 0.45f);
        mb_tube_bezier(&b, v3mk(0,0,-0.18f), v3mk(0,0.08f,0.20f), v3mk(0,0.16f,0.50f), v3mk(0,0.24f,0.78f), 0.026f, 0.024f, 1.0f, 4, 6, 0.0f);
        mb_set_style(&b, 224, 232, 244, MAT_ARMOR_METAL, 0.22f, 0.92f, 0.50f);
        mb_box(&b, m4_trs(v3mk(0, 0.28f, 0.94f), v3mk(-0.25f, 0, 0), v3mk(1,1,1)), v3mk(0.13f, 0.025f, 0.18f), 1.0f);
        out_geo->slices[MESH_TOOL_SHOVEL] = (MeshSlice){s0, b.ic - s0, 0};

        // Axe
        uint32_t a0 = b.ic;
        mb_set_style(&b, 168, 116, 68, MAT_WOOD_BARK, 0.58f, 0.05f, 0.45f);
        mb_tube_bezier(&b, v3mk(0,0,-0.15f), v3mk(0,0.10f,0.20f), v3mk(0,0.20f,0.48f), v3mk(0,0.30f,0.74f), 0.028f, 0.026f, 1.0f, 4, 6, 0.0f);
        mb_set_style(&b, 228, 58, 58, MAT_PAINTED_TOY, 0.35f, 0.2f, 0.48f);
        mb_box(&b, m4_translate(v3mk(0, 0.24f, 0.66f)), v3mk(0.04f, 0.14f, 0.11f), 1.0f);
        mb_set_style(&b, 232, 238, 248, MAT_ARMOR_METAL, 0.20f, 0.94f, 0.50f);
        mb_box(&b, m4_translate(v3mk(0, 0.16f, 0.68f)), v3mk(0.025f, 0.10f, 0.13f), 1.0f);
        out_geo->slices[MESH_TOOL_AXE] = (MeshSlice){a0, b.ic - a0, 0};
    }

    // SLICES: 3 Placeable Cozy Island Decorations (Streetlamp Lantern, Campfire, Garden Bench!)
    {
        uint32_t dl0 = b.ic;
        mb_set_style(&b, 52, 62, 68, MAT_ARMOR_METAL, 0.42f, 0.75f, 0.45f);
        static const float post_p[] = {0.0f, 0.18f, 0.12f, 0.09f, 0.92f, 0.06f, 1.0f, 0.14f};
        mb_lathe(&b, m4_ident(), post_p, 4, 10, 1.95f, 1.0f, 1.0f, 0.0f, 0, 0.0f);
        mb_set_style(&b, 255, 218, 126, MAT_CRYSTAL_GLOW, 0.10f, 0.0f, 0.0f);
        mb_sphere(&b, v3mk(0, 2.12f, 0), v3mk(0.18f, 0.22f, 0.18f), 8, 12, 0.0f);
        mb_set_style(&b, 52, 62, 68, MAT_ARMOR_METAL, 0.42f, 0.75f, 0.45f);
        static const float cap_p[] = {0.0f, 0.26f, 1.0f, 0.02f};
        mb_lathe(&b, m4_translate(v3mk(0, 2.30f, 0)), cap_p, 2, 10, 0.18f, 1.0f, 1.0f, 0.0f, 0, 0.0f);
        out_geo->slices[MESH_DECOR_LANTERN] = (MeshSlice){dl0, b.ic - dl0, 0};

        uint32_t dc0 = b.ic;
        mb_set_style(&b, 142, 138, 134, MAT_MARBLE_GOLD, 0.75f, 0.0f, 0.40f);
        for (int s = 0; s < 7; s++) {
            float a = (float)s * (TAU_F / 7.0f);
            mb_sphere(&b, v3mk(cosf(a) * 0.42f, 0.10f, sinf(a) * 0.42f), v3mk(0.13f, 0.10f, 0.13f), 5, 7, 0.0f);
        }
        mb_set_style(&b, 124, 78, 46, MAT_WOOD_BARK, 0.80f, 0.0f, 0.40f);
        mb_box(&b, m4_trs(v3mk(0, 0.14f, 0), v3mk(0, 0.5f, 0.25f), v3mk(1,1,1)), v3mk(0.36f, 0.06f, 0.07f), 1.0f);
        mb_box(&b, m4_trs(v3mk(0, 0.14f, 0), v3mk(0, -0.9f, -0.25f), v3mk(1,1,1)), v3mk(0.36f, 0.06f, 0.07f), 1.0f);
        mb_set_style(&b, 255, 164, 58, MAT_CRYSTAL_GLOW, 0.1f, 0.0f, 0.0f);
        mb_sphere(&b, v3mk(0, 0.26f, 0), v3mk(0.20f, 0.26f, 0.20f), 6, 8, 0.0f);
        out_geo->slices[MESH_DECOR_CAMPFIRE] = (MeshSlice){dc0, b.ic - dc0, 0};

        uint32_t db0 = b.ic;
        mb_set_style(&b, 78, 168, 128, MAT_PAINTED_TOY, 0.48f, 0.05f, 0.48f);
        mb_box(&b, m4_translate(v3mk(0, 0.40f, 0)), v3mk(0.68f, 0.05f, 0.24f), 1.0f);
        mb_box(&b, m4_trs(v3mk(0, 0.68f, -0.20f), v3mk(-0.18f, 0, 0), v3mk(1,1,1)), v3mk(0.68f, 0.22f, 0.045f), 1.0f);
        mb_set_style(&b, 58, 66, 72, MAT_ARMOR_METAL, 0.45f, 0.75f, 0.42f);
        for (int sx = -1; sx <= 1; sx += 2) {
            mb_box(&b, m4_translate(v3mk((float)sx * 0.56f, 0.22f, 0)), v3mk(0.05f, 0.22f, 0.22f), 1.0f);
        }
        out_geo->slices[MESH_DECOR_BENCH] = (MeshSlice){db0, b.ic - db0, 0};
    }

    mb_finish_into(&b, out_geo);
}
