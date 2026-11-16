#include "clover.h"
#include <stdatomic.h>

// ============================================================================
// Clover Cove — Cozy Acoustic Guitar + Marimba + Ocean Surf + Speech Synth
// 100% Zero-Asset Real-Time 48kHz Stereo Synthesizer
// ============================================================================

typedef struct {
    int   kind;
    float t;
    float dur;
    float pitch;
} CloverVoice;

static _Atomic int  g_tod_mode  = 0;
static _Atomic bool g_is_muted  = false;
static _Atomic int  g_sfx_ring[16];
static _Atomic int  g_sfx_write = 0;
static int          g_sfx_read  = 0;

static CloverVoice  g_voices[10];
static double       g_music_phase = 0.0;
static uint32_t     g_noise_rng   = 0xC107E995u;
static float        g_surf_lp_l   = 0.0f;
static float        g_surf_lp_r   = 0.0f;

void audio_init(void) {
    memset(g_voices, 0, sizeof(g_voices));
    g_music_phase = 0.0;
}

void clover_audio_set_state(int tod_preset, bool muted) {
    atomic_store(&g_tod_mode, tod_preset);
    atomic_store(&g_is_muted, muted);
}

static void push_voice_cmd(int encoded) {
    int w = atomic_load(&g_sfx_write);
    atomic_store(&g_sfx_ring[w & 15], encoded);
    atomic_store(&g_sfx_write, w + 1);
}

void clover_audio_sfx(int sfx_id) {
    push_voice_cmd(sfx_id & 0xFF);
}

void clover_audio_speech(int pitch_seed) {
    push_voice_cmd(0x100 | (pitch_seed & 0xFF));
}

static inline float midi_hz(float note) {
    return 440.0f * powf(2.0f, (note - 69.0f) * (1.0f / 12.0f));
}

static void spawn_sfx_voice(int cmd) {
    int slot = 0;
    float best_rem = 999.0f;
    for (int i = 0; i < 10; i++) {
        float rem = g_voices[i].dur - g_voices[i].t;
        if (rem <= 0.0f) { slot = i; break; }
        if (rem < best_rem) { best_rem = rem; slot = i; }
    }
    CloverVoice *v = &g_voices[slot];
    v->t = 0.0f;
    if (cmd & 0x100) {
        // Pitched formant speech blip
        int seed = cmd & 0xFF;
        v->kind  = 100;
        v->dur   = 0.072f;
        v->pitch = 370.0f + (float)((seed * 53) % 210);
        return;
    }
    v->kind = cmd;
    v->pitch = 1.0f;
    switch (cmd) {
        case CSFX_STEP:        v->dur = 0.055f; break;
        case CSFX_TOOL_SWAP:   v->dur = 0.11f;  break;
        case CSFX_SHAKE_TREE:  v->dur = 0.26f;  break;
        case CSFX_POP_PICKUP:  v->dur = 0.16f;  break;
        case CSFX_CAST_ROD:    v->dur = 0.24f;  break;
        case CSFX_NIBBLE:      v->dur = 0.14f;  break;
        case CSFX_SPLASH_BITE: v->dur = 0.36f;  break;
        case CSFX_FANFARE:     v->dur = 0.92f;  break;
        case CSFX_DIG:         v->dur = 0.20f;  break;
        case CSFX_ROCK_CHIME:  v->dur = 0.30f;  break;
        case CSFX_COINS_SELL:  v->dur = 0.48f;  break;
        case CSFX_NET_SWOOSH:  v->dur = 0.18f;  break;
        default:               v->dur = 0.14f;  break;
    }
}

void audio_render_frames(float *out_lr, int frame_count, int sample_rate) {
    if (atomic_load(&g_is_muted) || sample_rate <= 0) {
        memset(out_lr, 0, (size_t)frame_count * 2 * sizeof(float));
        return;
    }

    int w = atomic_load(&g_sfx_write);
    while (g_sfx_read < w) {
        spawn_sfx_voice(atomic_load(&g_sfx_ring[g_sfx_read & 15]));
        g_sfx_read++;
    }

    int tod = atomic_load(&g_tod_mode);
    float dt = 1.0f / (float)sample_rate;

    // Warm Major-7th / Lydian Island Chord Progression (Fmaj9 -> G6 -> Em7 -> Am7)
    static const int8_t chords[4][4] = {
        {53, 60, 64, 69}, // Fmaj9
        {55, 62, 64, 71}, // G6
        {52, 59, 64, 67}, // Em7
        {57, 64, 67, 71}, // Am7
    };
    static const int8_t marimba_mel[16] = {
        72, -1, 76, 79,  76, -1, 74, -1,
        79, -1, 81, 79,  76, -1, 72, 74,
    };

    // Tempo gently slows down from peppy Morning/Noon to dreamy Starry Night!
    double step_hz = (tod == TOD_NIGHT) ? 2.55 : (tod == TOD_SUNSET ? 3.15 : 3.85);

    for (int f = 0; f < frame_count; f++) {
        double prev_p = g_music_phase;
        g_music_phase += step_hz * (double)dt;
        int step_idx = ((int)g_music_phase) & 31;
        double frac  = g_music_phase - floor(g_music_phase);
        int bar      = (step_idx >> 3) & 3;
        (void)prev_p;

        float n_raw = rng_f01(&g_noise_rng) * 2.0f - 1.0f;

        // 1. Gentle Rolling Ocean Surf Wash (stereo decorrelated low-pass breeze)
        float surf_swell = 0.42f + 0.58f * (0.5f + 0.5f * sinf((float)g_music_phase * 0.24f));
        g_surf_lp_l += 0.018f * (n_raw - g_surf_lp_l);
        float n_raw_r = rng_f01(&g_noise_rng) * 2.0f - 1.0f;
        g_surf_lp_r += 0.016f * (n_raw_r - g_surf_lp_r);
        float mix_l = g_surf_lp_l * surf_swell * 0.055f;
        float mix_r = g_surf_lp_r * surf_swell * 0.055f;

        // 2. Warm Nylon-String Acoustic Guitar Pluck Arpeggio
        int arp_slot = step_idx & 3;
        float arp_hz = midi_hz((float)chords[bar][arp_slot]);
        float g_env  = expf(-(float)frac * 5.2f) * smoothstepf(0.0f, 0.015f, (float)frac);
        float g_t    = (float)(frac / step_hz);
        float guitar = (sinf(TAU_F * arp_hz * g_t) * 0.68f
                      + sinf(TAU_F * arp_hz * 2.0f * g_t) * 0.24f * expf(-(float)frac * 8.0f)
                      + sinf(TAU_F * arp_hz * 3.0f * g_t) * 0.08f * expf(-(float)frac * 12.0f)) * g_env * 0.085f;
        mix_l += guitar * 0.90f;
        mix_r += guitar * 1.10f;

        // 3. Wooden Marimba / Celeste Chime Lead (Celeste sparkle at Night!)
        int mel_note = marimba_mel[step_idx & 15];
        if (mel_note > 0 && (step_idx < 16 || (step_idx & 1) == 0)) {
            float m_hz  = midi_hz((float)(tod == TOD_NIGHT ? mel_note + 12 : mel_note));
            float m_env = expf(-(float)frac * (tod == TOD_NIGHT ? 4.2f : 8.5f)) * smoothstepf(0.0f, 0.008f, (float)frac);
            float overtone = (tod == TOD_NIGHT) ? 2.0f : 3.98f; // Classic wooden marimba 4th harmonic vs celeste!
            float m_sig = (sinf(TAU_F * m_hz * g_t) + 0.22f * sinf(TAU_F * m_hz * overtone * g_t) * expf(-(float)frac * 14.0f)) * m_env * 0.072f;
            mix_l += m_sig * 1.08f;
            mix_r += m_sig * 0.92f;
        }

        // 4. Active Gameplay SFX + Formant Speech Voices
        for (int i = 0; i < 10; i++) {
            CloverVoice *v = &g_voices[i];
            if (v->t >= v->dur) continue;
            float t = v->t;
            float u = t / v->dur;
            v->t += dt;
            float s = 0.0f;

            switch (v->kind) {
                case 100: { // Cute formant speech blip
                    float f0 = v->pitch * (1.0f + 0.14f * sinf(u * PI_F));
                    float env = sinf(u * PI_F);
                    s = (sinf(TAU_F * f0 * t) * 0.55f + sinf(TAU_F * f0 * 2.1f * t) * 0.32f + sinf(TAU_F * f0 * 3.2f * t) * 0.13f) * env * 0.14f;
                } break;
                case CSFX_STEP:
                    s = (n_raw * 0.45f + sinf(TAU_F * 155.0f * t) * 0.55f) * (1.0f - u) * 0.10f;
                    break;
                case CSFX_TOOL_SWAP:
                    s = sinf(TAU_F * lerpf(520.0f, 880.0f, u) * t) * (1.0f - u) * 0.14f;
                    break;
                case CSFX_SHAKE_TREE:
                    s = n_raw * sinf(u * PI_F * 3.0f) * (1.0f - u) * 0.16f;
                    break;
                case CSFX_POP_PICKUP:
                    s = sinf(TAU_F * lerpf(380.0f, 980.0f, u * u) * t) * sinf(u * PI_F) * 0.20f;
                    break;
                case CSFX_CAST_ROD:
                    s = (n_raw * 0.35f + sinf(TAU_F * lerpf(780.0f, 260.0f, u) * t) * 0.65f) * (1.0f - u) * 0.15f;
                    break;
                case CSFX_NIBBLE:
                    s = sinf(TAU_F * lerpf(420.0f, 240.0f, u) * t) * expf(-u * 5.0f) * 0.18f;
                    break;
                case CSFX_SPLASH_BITE:
                    s = (n_raw * 0.72f + sinf(TAU_F * lerpf(320.0f, 110.0f, u) * t) * 0.48f) * expf(-u * 3.4f) * 0.28f;
                    break;
                case CSFX_FANFARE: {
                    static const float fn[4] = {523.25f, 659.25f, 783.99f, 1046.50f};
                    int ni = (int)(u * 4.0f); if (ni > 3) ni = 3;
                    float sub = u * 4.0f - (float)ni;
                    s = (sinf(TAU_F * fn[ni] * t) + 0.35f * sinf(TAU_F * fn[ni] * 2.0f * t)) * expf(-sub * 2.4f) * 0.20f;
                } break;
                case CSFX_DIG:
                    s = (n_raw * 0.65f + sinf(TAU_F * 135.0f * t) * 0.45f) * expf(-u * 5.5f) * 0.20f;
                    break;
                case CSFX_ROCK_CHIME:
                    s = (sinf(TAU_F * 1318.5f * t) + 0.5f * sinf(TAU_F * 2637.0f * t)) * expf(-u * 6.0f) * 0.22f;
                    break;
                case CSFX_COINS_SELL: {
                    float hz = (u < 0.35f) ? 987.77f : 1318.51f;
                    s = (sinf(TAU_F * hz * t) + 0.4f * sinf(TAU_F * hz * 2.0f * t)) * expf(-fmodf(u, 0.35f) * 8.0f) * 0.22f;
                } break;
                case CSFX_NET_SWOOSH:
                    s = n_raw * sinf(u * PI_F) * 0.20f;
                    break;
                default:
                    s = sinf(TAU_F * 784.0f * t) * (1.0f - u) * 0.14f;
                    break;
            }
            mix_l += s;
            mix_r += s;
        }

        out_lr[f * 2 + 0] = clampf(mix_l, -0.95f, 0.95f);
        out_lr[f * 2 + 1] = clampf(mix_r, -0.95f, 0.95f);
    }
}
