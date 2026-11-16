#include "clover.h"

static const v3 k_chibi_pivots[MAX_BONES] = {
    [BONE_ROOT]   = { 0.00f, 0.00f,  0.00f},
    [BONE_PELVIS] = { 0.00f, 0.34f,  0.00f},
    [BONE_SPINE]  = { 0.00f, 0.46f,  0.00f},
    [BONE_CHEST]  = { 0.00f, 0.62f,  0.00f},
    [BONE_HEAD]   = { 0.00f, 0.76f,  0.00f},
    [BONE_ARM_L]  = { 0.19f, 0.66f,  0.00f},
    [BONE_HAND_L] = { 0.28f, 0.48f,  0.03f},
    [BONE_ARM_R]  = {-0.19f, 0.66f,  0.00f},
    [BONE_HAND_R] = {-0.28f, 0.48f,  0.03f},
    [BONE_LEG_L]  = { 0.095f,0.34f,  0.00f},
    [BONE_FOOT_L] = { 0.100f,0.12f,  0.02f},
    [BONE_LEG_R]  = {-0.095f,0.34f,  0.00f},
    [BONE_FOOT_R] = {-0.100f,0.12f,  0.02f},
    [BONE_TAIL_1] = { 0.00f, 0.38f, -0.15f},
    [BONE_TAIL_2] = { 0.00f, 0.46f, -0.30f},
    [BONE_WEAPON] = {-0.31f, 0.46f,  0.06f},
};

static const int8_t k_chibi_parent[MAX_BONES] = {
    [BONE_ROOT]   = -1,
    [BONE_PELVIS] = BONE_ROOT,
    [BONE_SPINE]  = BONE_PELVIS,
    [BONE_CHEST]  = BONE_SPINE,
    [BONE_HEAD]   = BONE_CHEST,
    [BONE_ARM_L]  = BONE_CHEST,
    [BONE_HAND_L] = BONE_ARM_L,
    [BONE_ARM_R]  = BONE_CHEST,
    [BONE_HAND_R] = BONE_ARM_R,
    [BONE_LEG_L]  = BONE_PELVIS,
    [BONE_FOOT_L] = BONE_LEG_L,
    [BONE_LEG_R]  = BONE_PELVIS,
    [BONE_FOOT_R] = BONE_LEG_R,
    [BONE_TAIL_1] = BONE_PELVIS,
    [BONE_TAIL_2] = BONE_TAIL_1,
    [BONE_WEAPON] = BONE_HAND_R,
};

void clover_compute_bones(const ChibiAnim *ca, float time_sec, m4 out_bones[MAX_BONES]) {
    v3 rot[MAX_BONES] = {{0}};
    v3 pos_off[MAX_BONES] = {{0}};

    float ph = time_sec * 2.6f + ca->tail_phase;
    float t = ca->anim_time;

    // Cozy bouncy chibi idle breathing & gentle head tilt
    float breath = sinf(ph) * 0.018f;
    pos_off[BONE_PELVIS].y = breath;
    rot[BONE_HEAD].z = sinf(ph * 0.55f) * 0.045f;
    rot[BONE_ARM_L].z =  0.22f + breath * 1.4f;
    rot[BONE_ARM_R].z = -0.22f - breath * 1.4f;

    if (ca->anim == CANIM_WALK || ca->anim == CANIM_RUN) {
        bool run = (ca->anim == CANIM_RUN);
        float rate = run ? 14.5f : 9.2f;
        float amp  = (run ? 0.88f : 0.54f) * clampf(ca->move_speed, 0.25f, 1.0f);
        float cyc  = time_sec * rate + ca->tail_phase;
        float s = sinf(cyc), c = cosf(cyc);

        // Peppy chibi waddle & bounce
        pos_off[BONE_PELVIS].y = fabsf(c) * (run ? 0.085f : 0.045f);
        rot[BONE_PELVIS].z = s * 0.09f;
        rot[BONE_SPINE].x  = run ? 0.20f : 0.08f;
        rot[BONE_HEAD].x   = run ? -0.14f : -0.04f;
        rot[BONE_HEAD].z   = -s * 0.07f;

        rot[BONE_LEG_L].x  =  s * amp;
        rot[BONE_FOOT_L].x = clampf(-c * 0.45f, 0.0f, 0.7f);
        rot[BONE_LEG_R].x  = -s * amp;
        rot[BONE_FOOT_R].x = clampf( c * 0.45f, 0.0f, 0.7f);

        rot[BONE_ARM_L] = v3mk(-s * amp * 0.92f, 0.0f, run ? 0.42f : 0.24f);
        rot[BONE_ARM_R] = v3mk( s * amp * 0.92f, 0.0f, run ? -0.42f : -0.24f);
    } else if (ca->anim == CANIM_SWING) {
        // Windup -> Crisp overhead Net / Shovel / Axe chop!
        float u = clampf(t / 0.48f, 0.0f, 1.0f);
        if (u < 0.36f) {
            float k = smoothstepf(0.0f, 0.36f, u);
            pos_off[BONE_PELVIS].y = 0.06f * k;
            rot[BONE_SPINE].x = -0.24f * k;
            rot[BONE_ARM_R]   = v3mk(-2.25f * k, -0.20f * k, -0.15f);
            rot[BONE_ARM_L]   = v3mk(-1.35f * k,  0.30f * k,  0.15f);
        } else {
            float k = smoothstepf(0.36f, 0.68f, u);
            float settle = 1.0f - smoothstepf(0.75f, 1.0f, u);
            pos_off[BONE_PELVIS].y = -0.05f * sinf(k * PI_F);
            rot[BONE_SPINE].x = lerpf(-0.24f, 0.34f, k) * settle;
            rot[BONE_ARM_R]   = v3mk(lerpf(-2.25f, -0.35f, k) * settle, 0.15f * settle, -0.20f);
            rot[BONE_WEAPON].x= 0.85f * k * settle;
        }
    } else if (ca->anim == CANIM_SHAKE) {
        // Rapid two-handed tree shake / pickup!
        float w = sinf(t * 36.0f);
        pos_off[BONE_PELVIS].z = 0.05f;
        rot[BONE_SPINE].x = 0.15f;
        rot[BONE_CHEST].y = w * 0.22f;
        rot[BONE_HEAD].z  = w * 0.12f;
        rot[BONE_ARM_L]   = v3mk(-1.25f,  0.22f + w * 0.18f,  0.18f);
        rot[BONE_ARM_R]   = v3mk(-1.25f, -0.22f + w * 0.18f, -0.18f);
    } else if (ca->anim == CANIM_FISH_WAIT) {
        // Patient two-hand rod hold leaning slightly toward water
        float bob = sinf(time_sec * 3.0f) * 0.04f;
        rot[BONE_SPINE].x  = 0.10f;
        rot[BONE_ARM_R]    = v3mk(-0.92f + bob, 0.18f, -0.12f);
        rot[BONE_ARM_L]    = v3mk(-0.82f + bob, -0.32f, 0.14f);
        rot[BONE_WEAPON].x = 0.35f;
    } else if (ca->anim == CANIM_SHOWOFF) {
        // Iconic "TA-DA! I caught a...!" chibi jump & proud hold-up pose!
        float pop = (t < 0.28f) ? sinf((t / 0.28f) * PI_F) * 0.22f : sinf(time_sec * 6.0f) * 0.025f;
        pos_off[BONE_PELVIS].y = pop;
        rot[BONE_HEAD].z  = -0.10f + sinf(time_sec * 4.5f) * 0.05f;
        rot[BONE_ARM_R]   = v3mk(-2.45f, 0.18f, -0.28f);
        rot[BONE_ARM_L]   = v3mk(-0.45f, 0.0f,   0.58f);
        rot[BONE_LEG_L].z =  0.14f;
        rot[BONE_LEG_R].z = -0.14f;
    } else if (ca->anim == CANIM_WAVE) {
        // Enthusiastic villager wave!
        float w = sinf(time_sec * 12.0f);
        pos_off[BONE_PELVIS].y = fabsf(sinf(time_sec * 6.0f)) * 0.04f;
        rot[BONE_HEAD].z = 0.12f + w * 0.06f;
        rot[BONE_ARM_R]  = v3mk(-2.35f, 0.0f, -0.38f + w * 0.34f);
        rot[BONE_ARM_L]  = v3mk(0.10f, 0.0f, 0.30f);
    }

    if (ca->has_tail) {
        float wag = sinf(time_sec * (ca->anim == CANIM_WAVE ? 9.5f : 4.2f) + ca->tail_phase);
        rot[BONE_TAIL_1] = v3mk(0.28f, wag * 0.42f, 0.0f);
        rot[BONE_TAIL_2] = v3mk(0.35f, wag * 0.55f, 0.0f);
    }

    m4 world_pose[MAX_BONES];
    for (int b = 0; b < MAX_BONES; b++) {
        int p = k_chibi_parent[b];
        v3 rel_pivot = (p < 0) ? k_chibi_pivots[b] : v3sub(k_chibi_pivots[b], k_chibi_pivots[p]);
        m4 local = m4_mul(m4_translate(v3add(rel_pivot, pos_off[b])), m4_euler_zyx(rot[b]));
        world_pose[b] = (p < 0) ? local : m4_mul(world_pose[p], local);
        out_bones[b] = m4_mul(world_pose[b], m4_translate(v3mul(k_chibi_pivots[b], -1.0f)));
    }
}
