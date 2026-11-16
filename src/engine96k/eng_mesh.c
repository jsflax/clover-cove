#include "eng_mesh.h"
#include <stdlib.h>

void mb_set_style(MeshBuilder *b, uint8_t r, uint8_t g, uint8_t bl, uint8_t mat, float rough, float metal, float outline) {
    b->col[0] = r; b->col[1] = g; b->col[2] = bl; b->col[3] = 255;
    b->cur_mat = mat;
    b->cur_rough = (uint8_t)(clampf(rough, 0.0f, 1.0f) * 255.0f);
    b->cur_metal = (uint8_t)(clampf(metal, 0.0f, 1.0f) * 255.0f);
    b->cur_outline = (uint8_t)(clampf(outline, 0.0f, 1.0f) * 255.0f);
}

void mb_set_alpha(MeshBuilder *b, uint8_t alpha) {
    b->col[3] = alpha;
}

void mb_set_bone(MeshBuilder *b, uint8_t bone0, uint8_t bone1, float w1) {
    uint8_t bw1 = (uint8_t)(clampf(w1, 0.0f, 1.0f) * 255.0f);
    b->cur_bone0 = bone0;
    b->cur_bone1 = bone1;
    b->cur_w0 = (uint8_t)(255u - bw1);
    b->cur_w1 = bw1;
}

uint32_t mb_vert(MeshBuilder *b, v3 p, v3 n, float u, float v_tc) {
    if (b->vc >= b->vcap) {
        b->vcap = b->vcap ? b->vcap * 2 : 16384;
        b->v = (Vertex *)realloc(b->v, b->vcap * sizeof(Vertex));
    }
    n = v3norm(n);
    Vertex *vt = &b->v[b->vc];
    vt->px = p.x; vt->py = p.y; vt->pz = p.z;
    vt->nx = n.x; vt->ny = n.y; vt->nz = n.z;
    vt->u = u; vt->v = v_tc;
    memcpy(vt->rgba, b->col, 4);
    vt->params[0] = b->cur_mat;
    vt->params[1] = b->cur_rough;
    vt->params[2] = b->cur_metal;
    vt->params[3] = b->cur_outline;
    vt->joints[0] = b->cur_bone0; vt->joints[1] = b->cur_bone1; vt->joints[2] = 0; vt->joints[3] = 0;
    vt->weights[0]= b->cur_w0;    vt->weights[1]= b->cur_w1;    vt->weights[2]= 0; vt->weights[3]= 0;
    return b->vc++;
}

void mb_tri(MeshBuilder *b, uint32_t i0, uint32_t i1, uint32_t i2) {
    if (b->ic + 3 > b->icap) {
        b->icap = b->icap ? b->icap * 2 : 49152;
        b->idx = (uint32_t *)realloc(b->idx, b->icap * sizeof(uint32_t));
    }
    b->idx[b->ic++] = i0;
    b->idx[b->ic++] = i1;
    b->idx[b->ic++] = i2;
}

void mb_quad(MeshBuilder *b, uint32_t i0, uint32_t i1, uint32_t i2, uint32_t i3) {
    mb_tri(b, i0, i1, i2);
    mb_tri(b, i0, i2, i3);
}

void mb_box(MeshBuilder *b, m4 xf, v3 half, float uv_scale) {
    static const int8_t faces[6][4][3] = {
        {{-1,-1, 1},{ 1,-1, 1},{ 1, 1, 1},{-1, 1, 1}},
        {{ 1,-1,-1},{-1,-1,-1},{-1, 1,-1},{ 1, 1,-1}},
        {{ 1,-1, 1},{ 1,-1,-1},{ 1, 1,-1},{ 1, 1, 1}},
        {{-1,-1,-1},{-1,-1, 1},{-1, 1, 1},{-1, 1,-1}},
        {{-1, 1, 1},{ 1, 1, 1},{ 1, 1,-1},{-1, 1,-1}},
        {{-1,-1,-1},{ 1,-1,-1},{ 1,-1, 1},{-1,-1, 1}},
    };
    static const float fn[6][3] = {
        {0,0,1},{0,0,-1},{1,0,0},{-1,0,0},{0,1,0},{0,-1,0}
    };
    for (int f = 0; f < 6; f++) {
        v3 wn = m4_mul_dir(xf, v3mk(fn[f][0], fn[f][1], fn[f][2]));
        uint32_t base = b->vc;
        for (int k = 0; k < 4; k++) {
            v3 lp = v3mk(faces[f][k][0] * half.x, faces[f][k][1] * half.y, faces[f][k][2] * half.z);
            v3 wp = m4_mul_pos(xf, lp);
            float u = ((k == 1 || k == 2) ? 1.0f : 0.0f) * uv_scale;
            float v = ((k >= 2) ? 1.0f : 0.0f) * uv_scale;
            mb_vert(b, wp, wn, u, v);
        }
        mb_quad(b, base, base + 1, base + 2, base + 3);
    }
}

void mb_sphere(MeshBuilder *b, v3 center, v3 radii, int rings, int segs, float u_base) {
    uint32_t base_v = b->vc;
    for (int r = 0; r <= rings; r++) {
        float vf = (float)r / (float)rings;
        float phi = vf * PI_F;
        float sp = sinf(phi), cp = cosf(phi);
        for (int s = 0; s <= segs; s++) {
            float uf = (float)s / (float)segs;
            float th = uf * TAU_F;
            v3 un = v3mk(sp * cosf(th), cp, sp * sinf(th));
            v3 wp = v3add(center, v3had(un, radii));
            v3 nrm = v3mk(un.x / (radii.x > 1e-4f ? radii.x : 1.0f),
                          un.y / (radii.y > 1e-4f ? radii.y : 1.0f),
                          un.z / (radii.z > 1e-4f ? radii.z : 1.0f));
            mb_vert(b, wp, nrm, u_base + uf, vf);
        }
    }
    int stride = segs + 1;
    for (int r = 0; r < rings; r++) {
        for (int s = 0; s < segs; s++) {
            uint32_t a = base_v + r * stride + s, c = a + stride;
            mb_quad(b, a, a + 1, c + 1, c);
        }
    }
}

void mb_lathe(MeshBuilder *b, m4 xf, const float *prof_yr, int stops, int segs,
              float height, float sx, float sz, float flute_amp, int flute_k, float u_base) {
    uint32_t ring_start = b->vc;
    for (int i = 0; i < stops; i++) {
        float y = prof_yr[i * 2 + 0] * height;
        float r = prof_yr[i * 2 + 1];
        float dy = (i + 1 < stops ? prof_yr[(i + 1) * 2 + 0] : prof_yr[i * 2 + 0]) -
                   (i > 0 ? prof_yr[(i - 1) * 2 + 0] : prof_yr[i * 2 + 0]);
        float dr = (i + 1 < stops ? prof_yr[(i + 1) * 2 + 1] : prof_yr[i * 2 + 1]) -
                   (i > 0 ? prof_yr[(i - 1) * 2 + 1] : prof_yr[i * 2 + 1]);
        float ny_local = -dr;
        float nr_local = dy * height;

        for (int s = 0; s <= segs; s++) {
            float frac = (float)s / (float)segs;
            float ang = frac * TAU_F;
            float fl = 1.0f + flute_amp * cosf(ang * (float)flute_k);
            float ca = cosf(ang), sa = sinf(ang);
            v3 lp = v3mk(ca * r * sx * fl, y, sa * r * sz * fl);
            v3 ln = v3mk(ca * nr_local / (sx > 0.01f ? sx : 1.0f), ny_local, sa * nr_local / (sz > 0.01f ? sz : 1.0f));
            mb_vert(b, m4_mul_pos(xf, lp), m4_mul_dir(xf, ln), u_base + frac, prof_yr[i * 2 + 0]);
        }
    }
    int stride = segs + 1;
    for (int i = 0; i < stops - 1; i++) {
        for (int s = 0; s < segs; s++) {
            uint32_t a = ring_start + i * stride + s;
            uint32_t c = a + stride;
            mb_quad(b, a, c, c + 1, a + 1);
        }
    }
}

void mb_tube_bezier(MeshBuilder *b, v3 p0, v3 p1, v3 p2, v3 p3,
                    float r0, float r1, float flat_ratio, int steps, int sides, float u_base) {
    uint32_t start_v = b->vc;
    v3 prev_up = v3mk(0.0f, 1.0f, 0.0f);
    for (int i = 0; i <= steps; i++) {
        float t = (float)i / (float)steps;
        float it = 1.0f - t;
        v3 c = v3add(v3add(v3mul(p0, it*it*it), v3mul(p1, 3.0f*it*it*t)),
                     v3add(v3mul(p2, 3.0f*it*t*t), v3mul(p3, t*t*t)));
        v3 tanv = v3add(v3add(v3mul(v3sub(p1, p0), 3.0f*it*it), v3mul(v3sub(p2, p1), 6.0f*it*t)),
                        v3mul(v3sub(p3, p2), 3.0f*t*t));
        tanv = v3norm(tanv);
        if (fabsf(v3dot(tanv, prev_up)) > 0.94f) prev_up = v3mk(0.0f, 0.0f, 1.0f);
        v3 side = v3norm(v3cross(prev_up, tanv));
        v3 up = v3cross(tanv, side);
        prev_up = up;

        float r = lerpf(r0, r1, powf(t, 0.85f));
        for (int s = 0; s <= sides; s++) {
            float frac = (float)s / (float)sides;
            float ang = frac * TAU_F;
            float ca = cosf(ang), sa = sinf(ang);
            v3 off = v3add(v3mul(side, ca * r), v3mul(up, sa * r * flat_ratio));
            v3 nrm = v3norm(v3add(v3mul(side, ca), v3mul(up, sa / (flat_ratio > 0.1f ? flat_ratio : 1.0f))));
            mb_vert(b, v3add(c, off), nrm, u_base + frac, t);
        }
    }
    int stride = sides + 1;
    for (int i = 0; i < steps; i++) {
        for (int s = 0; s < sides; s++) {
            uint32_t a = start_v + i * stride + s;
            uint32_t c = a + stride;
            mb_quad(b, a, a + 1, c + 1, c);
        }
    }
}

void mb_fluffy_canopy(MeshBuilder *b, v3 center, float base_rad, uint32_t seed) {
    static const float lobe_offs[9][4] = {
        { 0.00f,  0.34f,  0.00f, 0.88f},
        {-0.46f,  0.02f,  0.32f, 0.78f},
        { 0.48f,  0.04f,  0.28f, 0.78f},
        { 0.04f,  0.06f, -0.48f, 0.78f},
        {-0.42f,  0.16f, -0.34f, 0.72f},
        { 0.42f,  0.16f, -0.32f, 0.72f},
        { 0.00f, -0.12f,  0.44f, 0.74f},
        {-0.26f,  0.28f,  0.18f, 0.68f},
        { 0.28f,  0.26f, -0.14f, 0.68f},
    };
    v3 lobes[9];
    float rads[9];
    for (int l = 0; l < 9; l++) {
        float jx = rng_frange(&seed, -0.05f, 0.05f);
        float jz = rng_frange(&seed, -0.05f, 0.05f);
        lobes[l] = v3mk((lobe_offs[l][0] + jx) * base_rad, lobe_offs[l][1] * base_rad, (lobe_offs[l][2] + jz) * base_rad);
        rads[l]  = lobe_offs[l][3] * base_rad;
    }
    mb_set_style(b, 92, 186, 72, MAT_FOLIAGE, 0.72f, 0.0f, 0.26f);
    v3 dome_pivot = v3sub(center, v3mk(0.0f, base_rad * 0.55f, 0.0f));
    int rings = 20, segs = 32;
    uint32_t base_v = b->vc;
    for (int r = 0; r <= rings; r++) {
        float v_f = (float)r / (float)rings;
        float phi = v_f * PI_F;
        float sp = sinf(phi), cp = cosf(phi);
        for (int s = 0; s <= segs; s++) {
            float u_f = (float)s / (float)segs;
            float th = u_f * TAU_F;
            v3 d = v3norm(v3mk(sp * cosf(th), cp * 0.88f, sp * sinf(th)));
            float best_t = base_rad * 0.65f;
            int best_l = 0;
            for (int l = 0; l < 9; l++) {
                float bd = v3dot(lobes[l], d);
                float disc = bd * bd - (v3dot(lobes[l], lobes[l]) - rads[l] * rads[l]);
                if (disc > 0.0f) {
                    float tl = bd + sqrtf(disc);
                    if (tl > best_t) { best_t = tl; best_l = l; }
                }
            }
            float bump = 1.0f + 0.035f * sinf(th * 5.0f + phi * 4.0f) + 0.025f * cosf(th * 7.0f - phi * 3.0f);
            v3 rel = v3mul(d, best_t * bump);
            v3 wp = v3add(center, rel);
            float h_t = clampf((wp.y - (center.y - base_rad * 0.82f)) / (base_rad * 1.95f), 0.0f, 1.0f);
            float shade = 0.64f + 0.44f * smoothstepf(0.0f, 1.0f, h_t);
            b->col[0] = (uint8_t)clampf(lerpf(52.0f, 126.0f, h_t) * shade, 20.0f, 255.0f);
            b->col[1] = (uint8_t)clampf(lerpf(146.0f, 214.0f, h_t) * shade, 40.0f, 255.0f);
            b->col[2] = (uint8_t)clampf(lerpf(80.0f, 66.0f, h_t) * shade, 20.0f, 255.0f);
            v3 lobe_n = v3norm(v3sub(rel, lobes[best_l]));
            v3 dome_n = v3norm(v3sub(wp, dome_pivot));
            v3 crown_n = v3norm(v3lerp(lobe_n, dome_n, 0.72f));
            mb_vert(b, wp, crown_n, u_f, v_f);
        }
    }
    int stride = segs + 1;
    for (int r = 0; r < rings; r++) {
        for (int s = 0; s < segs; s++) {
            uint32_t a = base_v + r * stride + s;
            uint32_t c = a + stride;
            mb_quad(b, a, a + 1, c + 1, c);
        }
    }
}

void mb_grass_tuft(MeshBuilder *b) {
    mb_set_style(b, 120, 195, 88, MAT_FOLIAGE, 0.65f, 0.0f, 0.0f);
    for (int bl = 0; bl < 3; bl++) {
        float ang = (float)bl * (TAU_F / 3.0f);
        float ca = cosf(ang), sa = sinf(ang);
        uint32_t gv = b->vc;
        mb_vert(b, v3mk(-0.045f * sa, 0.00f,  0.045f * ca), v3mk(0, 1, 0), 0.0f, 0.0f);
        mb_vert(b, v3mk( 0.045f * sa, 0.00f, -0.045f * ca), v3mk(0, 1, 0), 1.0f, 0.0f);
        mb_vert(b, v3mk(0.06f*ca - 0.028f*sa, 0.24f, 0.06f*sa + 0.028f*ca), v3mk(0, 1, 0), 0.0f, 0.5f);
        mb_vert(b, v3mk(0.06f*ca + 0.028f*sa, 0.24f, 0.06f*sa - 0.028f*ca), v3mk(0, 1, 0), 1.0f, 0.5f);
        mb_vert(b, v3mk(0.16f*ca, 0.46f, 0.16f*sa), v3mk(0, 1, 0), 0.5f, 1.0f);
        mb_quad(b, gv, gv + 1, gv + 3, gv + 2);
        mb_tri(b, gv + 2, gv + 3, gv + 4);
        mb_quad(b, gv + 1, gv, gv + 2, gv + 3);
        mb_tri(b, gv + 3, gv + 2, gv + 4);
    }
}

void mb_finish_into(MeshBuilder *b, WorldGeometry *out_geo) {
    out_geo->verts = b->v;
    out_geo->vert_count = b->vc;
    out_geo->vert_cap = b->vcap;
    out_geo->indices = b->idx;
    out_geo->index_count = b->ic;
    out_geo->index_cap = b->icap;
}

void world_free_geo(WorldGeometry *geo) {
    free(geo->verts);
    free(geo->indices);
    memset(geo, 0, sizeof(*geo));
}
