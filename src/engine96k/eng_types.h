#ifndef ENG96K_TYPES_H
#define ENG96K_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define TAU_F 6.28318530717958647692f
#define PI_F  3.14159265358979323846f

typedef struct { float x, y; } v2;
typedef struct { float x, y, z; } v3;
typedef struct { float x, y, z, w; } v4;
typedef struct { float m[16]; } m4; // Column-major (OpenGL / Metal standard)

static inline v2 v2mk(float x, float y) { return (v2){x, y}; }
static inline v3 v3mk(float x, float y, float z) { return (v3){x, y, z}; }
static inline v4 v4mk(float x, float y, float z, float w) { return (v4){x, y, z, w}; }

static inline v3 v3add(v3 a, v3 b) { return (v3){a.x + b.x, a.y + b.y, a.z + b.z}; }
static inline v3 v3sub(v3 a, v3 b) { return (v3){a.x - b.x, a.y - b.y, a.z - b.z}; }
static inline v3 v3mul(v3 a, float s) { return (v3){a.x * s, a.y * s, a.z * s}; }
static inline v3 v3had(v3 a, v3 b) { return (v3){a.x * b.x, a.y * b.y, a.z * b.z}; }
static inline float v3dot(v3 a, v3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static inline v3 v3cross(v3 a, v3 b) {
    return (v3){a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
static inline float v3len(v3 a) { return sqrtf(v3dot(a, a)); }
static inline v3 v3norm(v3 a) {
    float l = v3len(a);
    return l > 1e-6f ? v3mul(a, 1.0f / l) : (v3){0.0f, 1.0f, 0.0f};
}
static inline v3 v3lerp(v3 a, v3 b, float t) {
    return (v3){a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t};
}
static inline float clampf(float x, float lo, float hi) {
    return x < lo ? lo : (x > hi ? hi : x);
}
static inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
static inline float smoothstepf(float e0, float e1, float x) {
    float t = clampf((x - e0) / (e1 - e0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

// Deterministic PCG-style RNG
static inline uint32_t rng_next(uint32_t *state) {
    uint32_t old = *state;
    *state = old * 747796405u + 2891336453u;
    uint32_t word = ((old >> ((old >> 28u) + 4u)) ^ old) * 277803737u;
    return (word >> 22u) ^ word;
}
static inline float rng_f01(uint32_t *state) {
    return (float)(rng_next(state) & 0x00FFFFFFu) * (1.0f / 16777216.0f);
}
static inline float rng_frange(uint32_t *state, float lo, float hi) {
    return lo + (hi - lo) * rng_f01(state);
}

// Matrix math (implemented in eng_math.c)
m4 m4_ident(void);
m4 m4_mul(m4 a, m4 b);
m4 m4_translate(v3 t);
m4 m4_scale(v3 s);
m4 m4_rot_x(float rad);
m4 m4_rot_y(float rad);
m4 m4_rot_z(float rad);
m4 m4_euler_zyx(v3 rot);
m4 m4_trs(v3 t, v3 rot_xyz, v3 s);
m4 m4_look_at(v3 eye, v3 center, v3 up);
m4 m4_persp(float fovy_rad, float aspect, float zn, float zf, bool zero_to_one_z);
m4 m4_ortho(float l, float r, float b, float t, float zn, float zf, bool zero_to_one_z);
m4 m4_invert(m4 m);
v3 m4_mul_pos(m4 m, v3 p);
v3 m4_mul_dir(m4 m, v3 d);
v4 m4_mul_v4(m4 m, v4 v);

// Stylized PBR / Cel / Toy-Diorama Material IDs
enum {
    MAT_MARBLE_GOLD = 0,   // Veined cobblestone plaza + stone relief
    MAT_CLIFF_TURF  = 1,   // Sedimentary cliff + lush painted emerald grass
    MAT_FOLIAGE     = 2,   // Stylized tree canopy + wind sway + sun translucency
    MAT_WOOD_BARK   = 3,   // Tree trunk, bridge timber, cottage beams
    MAT_SKIN_FACE   = 4,   // Character skin/fur + infinite-res analytic SDF eyes/expression
    MAT_HAIR        = 5,   // Sculpted hair + Kajiya-Kay anisotropic highlight ring
    MAT_FABRIC_CAPE = 6,   // Patterned cloth/awning (uv.x >= 1.95 tints by du.tint_outline.rgb)
    MAT_ARMOR_METAL = 7,   // Polished metal / tool steel / gold coin
    MAT_CRYSTAL_GLOW= 8,   // Warm cottage window / glowing lantern bulb
    MAT_SAND_SHELL  = 9,   // Sparkling warm coral beach sand + wet shoreline darkening
    MAT_PAINTED_TOY = 10,  // Tactile matte-satin painted wood, roof shingles, glossy fruit & props
};

// Compact 48-byte universal vertex format used for environment AND skinned characters
typedef struct {
    float px, py, pz;       // 12B: Position
    float nx, ny, nz;       // 12B: Normal
    float u, v;             // 8B:  UV
    uint8_t rgba[4];        // 4B:  Base tint color (sRGB-ish linearizable)
    uint8_t params[4];      // 4B:  [0]=mat_id, [1]=roughness*255, [2]=metallic*255, [3]=outline_width*255
    uint8_t joints[4];      // 4B:  Bone indices (0..15; 0 = identity/rigid)
    uint8_t weights[4];     // 4B:  Bone weights normalized 0..255 (255,0,0,0 for rigid)
} Vertex;

// Static / Skinned Mesh slice inside the global unified Vertex/Index buffer
typedef struct {
    uint32_t first_index;
    uint32_t index_count;
    uint32_t base_vertex;
} MeshSlice;

// Reserved engine mesh slots (0..2 are rendered by RHI passes; 3..MAX_MESH_SLICES-1 are game slices)
enum {
    ENG_MESH_ENV_MAIN    = 0, // Static world terrain & architecture
    ENG_MESH_WATER       = 1, // Translucent stream / river / ocean / waterfall
    ENG_MESH_GRASS_BLADE = 2, // Hardware-instanced grass / wildflower clump
    ENG_MESH_GAME_START  = 3,
};

#define MAX_MESH_SLICES     48
#define MAX_BONES           16
#define MAX_GRASS_INSTANCES 5600
#define MAX_DYNAMIC_DRAWS   224
#define MAX_VFX_VERTS       4096
#define MAX_UI_PRIMS        2048

// VFX Quad / Ribbon Particle emitted per frame into dynamic HDR VFX buffer
typedef struct {
    float px, py, pz;
    float u, v;
    uint8_t rgba[4];
    float kind;    // 0=slash arc, 1=ripple ring, 2=spark/petal/leaf/smoke, 3=beam, 4=contact shadow
    float param;   // phase / glow boost
} VfxVertex;

// Crisp Analytic SDF/Vector 2D UI Quad (exact 64-byte alignment matching Metal & GLSL)
typedef struct {
    float x0, y0, x1, y1;   // 16B: Screen pixels (top-left origin)
    float u0, v0, u1, v1;   // 16B: UV or glyph coords
    uint32_t col0;          // 4B:  RGBA8 packed primary color
    uint32_t col1;          // 4B:  RGBA8 packed secondary / border color
    float radius;           // 4B:  Corner radius in px
    float border;           // 4B:  Border thickness in px
    uint32_t kind;          // 4B:  0=panel, 1=glyph_stroke
    uint32_t glyph_bits;    // 4B:  Packed 5x6 vector segment mask
    uint32_t pad0, pad1;    // 8B:  Pad to exact 64-byte stride
} UiPrim;

// 16-byte aligned GPU Scene Uniform Buffer (shared identically across Metal & GLES std140)
typedef struct {
    m4 view;
    m4 proj;
    m4 view_proj;
    m4 inv_view_proj;
    m4 light_view_proj;
    v4 cam_pos_time;        // xyz = cam_pos, w = time_sec
    v4 sun_dir_intensity;   // xyz = normalized vector TOWARD sun, w = sun intensity
    v4 sun_color_fog;       // rgb = sun linear color, w = fog density
    v4 sky_zenith_pad;      // rgb = sky zenith, w = world_curve (0=flat, >0=rolling-horizon world curve)
    v4 sky_horizon_pad;     // rgb = sky horizon, w = sky_mode (2.0 = day .. 3.0 = starry night)
    v4 grid_params;         // reserved / alignment
    v4 hover_cursor;        // xy = target ground ring xz, z = ring active flag
    v4 screen_params;       // x = width, y = height, z = 1/width, w = 1/height
    v4 sun_screen_pos;      // xy = sun UV on screen, z = sun_visible_weight, w = bloom_strength
    // [0..7] = warm point lights (xyz=pos, w=range), [8..9] = water ripples, [40] = fog_tint, [41] = curve_origin_xz + focus/closeup
    v4 tile_overlay[42];
} SceneUniforms;

// 16-byte aligned Per-Draw Uniform Buffer (supports both rigid props & 16-bone skinned characters)
typedef struct {
    m4 model;
    v4 tint_outline;        // rgb = cloth/accent tint, w = outline_scale
    v4 hair_color;          // rgb = hair / accent color, w = unused
    v4 custom_params;       // x = hurt_flash, y = face_style, z = anim_phase, w = is_skinned
    m4 bones[MAX_BONES];    // Skinning palette (bone_world_from_bind)
} DrawUniforms;

// Instanced grass / blossom blade data (32 bytes = 2x vec4)
typedef struct {
    v4 pos_scale;           // xyz = world root pos, w = blade height scale
    v4 rot_color;           // x = yaw, y = wind_phase, z = flower_type (0=grass, 1=gold/tulip, 2=azure/cosmos, 3=pink), w = shade
} GrassInstance;

// Procedural Geometry Output Container generated once at boot (< 15 ms)
typedef struct {
    Vertex *verts;
    uint32_t vert_count;
    uint32_t vert_cap;
    uint32_t *indices;
    uint32_t index_count;
    uint32_t index_cap;
    MeshSlice slices[MAX_MESH_SLICES];

    GrassInstance grass[MAX_GRASS_INSTANCES];
    uint32_t grass_count;
} WorldGeometry;

enum {
    DRAW_FLAG_SHADOW  = (1u << 0),
    DRAW_FLAG_OUTLINE = (1u << 1),
    DRAW_FLAGS_DEFAULT = (DRAW_FLAG_SHADOW | DRAW_FLAG_OUTLINE),
};

typedef struct {
    uint32_t mesh_id;
    uint32_t flags;
    DrawUniforms u;
} DynamicDraw;

// Dynamic Per-Frame Render Packet passed from C game to Metal / GLES backend
typedef struct {
    SceneUniforms scene_u;

    uint32_t draw_count;
    DynamicDraw draws[MAX_DYNAMIC_DRAWS];

    VfxVertex vfx_verts[MAX_VFX_VERTS];
    uint32_t vfx_vert_count;

    UiPrim ui_prims[MAX_UI_PRIMS];
    uint32_t ui_prim_count;
} RenderPacket;

#endif // ENG96K_TYPES_H
