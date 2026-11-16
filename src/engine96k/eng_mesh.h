#ifndef ENG96K_MESH_H
#define ENG96K_MESH_H

#include "eng_types.h"

typedef struct {
    Vertex *v;
    uint32_t vc, vcap;
    uint32_t *idx;
    uint32_t ic, icap;
    uint8_t cur_mat, cur_rough, cur_metal, cur_outline;
    uint8_t cur_bone0, cur_bone1, cur_w0, cur_w1;
    uint8_t col[4];
} MeshBuilder;

void     mb_set_style(MeshBuilder *b, uint8_t r, uint8_t g, uint8_t bl, uint8_t mat, float rough, float metal, float outline);
void     mb_set_alpha(MeshBuilder *b, uint8_t alpha);
void     mb_set_bone(MeshBuilder *b, uint8_t bone0, uint8_t bone1, float w1);
uint32_t mb_vert(MeshBuilder *b, v3 p, v3 n, float u, float v_tc);
void     mb_tri(MeshBuilder *b, uint32_t i0, uint32_t i1, uint32_t i2);
void     mb_quad(MeshBuilder *b, uint32_t i0, uint32_t i1, uint32_t i2, uint32_t i3);

void mb_box(MeshBuilder *b, m4 xf, v3 half, float uv_scale);
void mb_sphere(MeshBuilder *b, v3 center, v3 radii, int rings, int segs, float u_base);
void mb_lathe(MeshBuilder *b, m4 xf, const float *prof_yr, int stops, int segs,
              float height, float sx, float sz, float flute_amp, int flute_k, float u_base);
void mb_tube_bezier(MeshBuilder *b, v3 p0, v3 p1, v3 p2, v3 p3,
                    float r0, float r1, float flat_ratio, int steps, int sides, float u_base);
void mb_fluffy_canopy(MeshBuilder *b, v3 center, float base_rad, uint32_t seed);
void mb_grass_tuft(MeshBuilder *b);

void mb_finish_into(MeshBuilder *b, WorldGeometry *out_geo);
void world_free_geo(WorldGeometry *geo);

#endif // ENG96K_MESH_H
