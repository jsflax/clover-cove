#include "eng_types.h"

m4 m4_ident(void) {
    m4 r = {{0}};
    r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
    return r;
}

m4 m4_mul(m4 a, m4 b) {
    m4 r;
    for (int c = 0; c < 4; c++) {
        for (int row = 0; row < 4; row++) {
            r.m[c * 4 + row] =
                a.m[0 * 4 + row] * b.m[c * 4 + 0] +
                a.m[1 * 4 + row] * b.m[c * 4 + 1] +
                a.m[2 * 4 + row] * b.m[c * 4 + 2] +
                a.m[3 * 4 + row] * b.m[c * 4 + 3];
        }
    }
    return r;
}

m4 m4_translate(v3 t) {
    m4 r = m4_ident();
    r.m[12] = t.x; r.m[13] = t.y; r.m[14] = t.z;
    return r;
}

m4 m4_scale(v3 s) {
    m4 r = {{0}};
    r.m[0] = s.x; r.m[5] = s.y; r.m[10] = s.z; r.m[15] = 1.0f;
    return r;
}

m4 m4_rot_x(float rad) {
    m4 r = m4_ident();
    float c = cosf(rad), s = sinf(rad);
    r.m[5] = c;  r.m[9]  = -s;
    r.m[6] = s;  r.m[10] =  c;
    return r;
}

m4 m4_rot_y(float rad) {
    m4 r = m4_ident();
    float c = cosf(rad), s = sinf(rad);
    r.m[0] =  c; r.m[8]  = s;
    r.m[2] = -s; r.m[10] = c;
    return r;
}

m4 m4_rot_z(float rad) {
    m4 r = m4_ident();
    float c = cosf(rad), s = sinf(rad);
    r.m[0] = c; r.m[4] = -s;
    r.m[1] = s; r.m[5] =  c;
    return r;
}

m4 m4_euler_zyx(v3 rot) {
    return m4_mul(m4_rot_y(rot.y), m4_mul(m4_rot_x(rot.x), m4_rot_z(rot.z)));
}

m4 m4_trs(v3 t, v3 rot_xyz, v3 s) {
    return m4_mul(m4_translate(t), m4_mul(m4_euler_zyx(rot_xyz), m4_scale(s)));
}

m4 m4_look_at(v3 eye, v3 center, v3 up) {
    v3 f = v3norm(v3sub(center, eye));
    v3 s = v3norm(v3cross(f, up));
    v3 u = v3cross(s, f);
    m4 r = m4_ident();
    r.m[0] = s.x;  r.m[4] = s.y;  r.m[8]  = s.z;
    r.m[1] = u.x;  r.m[5] = u.y;  r.m[9]  = u.z;
    r.m[2] = -f.x; r.m[6] = -f.y; r.m[10] = -f.z;
    r.m[12] = -v3dot(s, eye);
    r.m[13] = -v3dot(u, eye);
    r.m[14] =  v3dot(f, eye);
    return r;
}

m4 m4_persp(float fovy_rad, float aspect, float zn, float zf, bool zero_to_one_z) {
    float f = 1.0f / tanf(fovy_rad * 0.5f);
    m4 r = {{0}};
    r.m[0] = f / (aspect > 0.01f ? aspect : 1.0f);
    r.m[5] = f;
    r.m[11] = -1.0f;
    if (zero_to_one_z) {
        r.m[10] = zf / (zn - zf);
        r.m[14] = (zn * zf) / (zn - zf);
    } else {
        r.m[10] = (zf + zn) / (zn - zf);
        r.m[14] = (2.0f * zf * zn) / (zn - zf);
    }
    return r;
}

m4 m4_ortho(float l, float r_val, float b, float t, float zn, float zf, bool zero_to_one_z) {
    m4 m = m4_ident();
    m.m[0] = 2.0f / (r_val - l);
    m.m[5] = 2.0f / (t - b);
    m.m[12] = -(r_val + l) / (r_val - l);
    m.m[13] = -(t + b) / (t - b);
    if (zero_to_one_z) {
        m.m[10] = -1.0f / (zf - zn);
        m.m[14] = -zn / (zf - zn);
    } else {
        m.m[10] = -2.0f / (zf - zn);
        m.m[14] = -(zf + zn) / (zf - zn);
    }
    return m;
}

v4 m4_mul_v4(m4 m, v4 v) {
    return (v4){
        m.m[0]*v.x + m.m[4]*v.y + m.m[8]*v.z  + m.m[12]*v.w,
        m.m[1]*v.x + m.m[5]*v.y + m.m[9]*v.z  + m.m[13]*v.w,
        m.m[2]*v.x + m.m[6]*v.y + m.m[10]*v.z + m.m[14]*v.w,
        m.m[3]*v.x + m.m[7]*v.y + m.m[11]*v.z + m.m[15]*v.w
    };
}

v3 m4_mul_pos(m4 m, v3 p) {
    v4 r = m4_mul_v4(m, (v4){p.x, p.y, p.z, 1.0f});
    return (v3){r.x, r.y, r.z};
}

v3 m4_mul_dir(m4 m, v3 d) {
    v4 r = m4_mul_v4(m, (v4){d.x, d.y, d.z, 0.0f});
    return (v3){r.x, r.y, r.z};
}

m4 m4_invert(m4 src) {
    float *m = src.m;
    m4 inv;
    float *o = inv.m;
    o[0] = m[5]*m[10]*m[15] - m[5]*m[11]*m[14] - m[9]*m[6]*m[15] + m[9]*m[7]*m[14] + m[13]*m[6]*m[11] - m[13]*m[7]*m[10];
    o[4] = -m[4]*m[10]*m[15] + m[4]*m[11]*m[14] + m[8]*m[6]*m[15] - m[8]*m[7]*m[14] - m[12]*m[6]*m[11] + m[12]*m[7]*m[10];
    o[8] = m[4]*m[9]*m[15] - m[4]*m[11]*m[13] - m[8]*m[5]*m[15] + m[8]*m[7]*m[13] + m[12]*m[5]*m[11] - m[12]*m[7]*m[9];
    o[12] = -m[4]*m[9]*m[14] + m[4]*m[10]*m[13] + m[8]*m[5]*m[14] - m[8]*m[6]*m[13] - m[12]*m[5]*m[10] + m[12]*m[6]*m[9];
    o[1] = -m[1]*m[10]*m[15] + m[1]*m[11]*m[14] + m[9]*m[2]*m[15] - m[9]*m[3]*m[14] - m[13]*m[2]*m[11] + m[13]*m[3]*m[10];
    o[5] = m[0]*m[10]*m[15] - m[0]*m[11]*m[14] - m[8]*m[2]*m[15] + m[8]*m[3]*m[14] + m[12]*m[2]*m[11] - m[12]*m[3]*m[10];
    o[9] = -m[0]*m[9]*m[15] + m[0]*m[11]*m[13] + m[8]*m[1]*m[15] - m[8]*m[3]*m[13] - m[12]*m[1]*m[11] + m[12]*m[3]*m[9];
    o[13] = m[0]*m[9]*m[14] - m[0]*m[10]*m[13] - m[8]*m[1]*m[14] + m[8]*m[2]*m[13] + m[12]*m[1]*m[10] - m[12]*m[2]*m[9];
    o[2] = m[1]*m[6]*m[15] - m[1]*m[7]*m[14] - m[5]*m[2]*m[15] + m[5]*m[3]*m[14] + m[13]*m[2]*m[7] - m[13]*m[3]*m[6];
    o[6] = -m[0]*m[6]*m[15] + m[0]*m[7]*m[14] + m[4]*m[2]*m[15] - m[4]*m[3]*m[14] - m[12]*m[2]*m[7] + m[12]*m[3]*m[6];
    o[10] = m[0]*m[5]*m[15] - m[0]*m[7]*m[13] - m[4]*m[1]*m[15] + m[4]*m[3]*m[13] + m[12]*m[1]*m[7] - m[12]*m[3]*m[5];
    o[14] = -m[0]*m[5]*m[14] + m[0]*m[6]*m[13] + m[4]*m[1]*m[14] - m[4]*m[2]*m[13] - m[12]*m[1]*m[6] + m[12]*m[2]*m[5];
    o[3] = -m[1]*m[6]*m[11] + m[1]*m[7]*m[10] + m[5]*m[2]*m[11] - m[5]*m[3]*m[10] - m[9]*m[2]*m[7] + m[9]*m[3]*m[6];
    o[7] = m[0]*m[6]*m[11] - m[0]*m[7]*m[10] - m[4]*m[2]*m[11] + m[4]*m[3]*m[10] + m[8]*m[2]*m[7] - m[8]*m[3]*m[6];
    o[11] = -m[0]*m[5]*m[11] + m[0]*m[7]*m[9] + m[4]*m[1]*m[11] - m[4]*m[3]*m[9] - m[8]*m[1]*m[7] + m[8]*m[3]*m[5];
    o[15] = m[0]*m[5]*m[10] - m[0]*m[6]*m[9] - m[4]*m[1]*m[10] + m[4]*m[2]*m[9] + m[8]*m[1]*m[6] - m[8]*m[2]*m[5];
    float det = m[0]*o[0] + m[1]*o[4] + m[2]*o[8] + m[3]*o[12];
    if (fabsf(det) < 1e-8f) return m4_ident();
    det = 1.0f / det;
    for (int i = 0; i < 16; i++) o[i] *= det;
    return inv;
}
