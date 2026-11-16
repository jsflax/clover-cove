#include "rhi_gles.h"
#include "../engine96k/eng_shaders.h"
#include <GLES3/gl3.h>
#include <android/log.h>
#include <stdlib.h>
#include <string.h>

#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "Eng96k", __VA_ARGS__)

struct GlesRHI {
    GLuint vao_empty, vao_world, vao_vfx, vao_ui;
    GLuint vbo, ibo, grass_vbo, vfx_vbo, ui_vbo;
    GLuint ubo_scene, ubo_draw;
    MeshSlice slices[MAX_MESH_SLICES];
    uint32_t grass_count;

    GLuint tex_proc[3];
    GLuint fbo_shadow, tex_shadow;

    int rt_w, rt_h, msaa_samples;
    GLuint fbo_msaa, rbo_color_msaa, rbo_depth_msaa;
    GLuint fbo_hdr, fbo_hdr_nodp, tex_hdr, tex_depth;
    GLuint fbo_bloom[3], tex_bloom[3];

    GLuint prog_sky, prog_shadow, prog_main, prog_outline, prog_grass, prog_water, prog_vfx, prog_bloom, prog_comp, prog_ui;
    GLint loc_bloom_par;
};

static GLuint compile_sh(GLenum type, const char *src) {
    GLuint s = glCreateShader(type);
    const char *srcs[2] = { k_glsl_preamble, src };
    if (src[0] == '#') glShaderSource(s, 1, &src, NULL);
    else               glShaderSource(s, 2, srcs, NULL);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(s, sizeof(log), NULL, log);
        LOGE("GLSL Compile Error: %s", log);
    }
    return s;
}

static GLuint link_prog(const char *vs_src, const char *fs_src) {
    GLuint vs = compile_sh(GL_VERTEX_SHADER, vs_src);
    GLuint fs = compile_sh(GL_FRAGMENT_SHADER, fs_src);
    GLuint p = glCreateProgram();
    glAttachShader(p, vs);
    glAttachShader(p, fs);
    glLinkProgram(p);
    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(p, sizeof(log), NULL, log);
        LOGE("GLSL Link Error: %s", log);
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    glUseProgram(p);
    GLuint sb = glGetUniformBlockIndex(p, "SceneBlock");
    if (sb != GL_INVALID_INDEX) glUniformBlockBinding(p, sb, 0);
    GLuint db = glGetUniformBlockIndex(p, "DrawBlock");
    if (db != GL_INVALID_INDEX) glUniformBlockBinding(p, db, 1);
    static const struct { const char *n; int u; } smps[] = {
        {"tex_marble", 0}, {"tex_grass", 1}, {"tex_fabric", 2}, {"tex_shadow", 3}, {"tex_depth", 4},
        {"u_src", 0}, {"hdr_tex", 0}, {"bloom1", 1}, {"bloom2", 2}, {"bloom3", 3}, {"tex_atlas", 0}
    };
    for (int i = 0; i < (int)(sizeof(smps) / sizeof(smps[0])); i++) {
        GLint loc = glGetUniformLocation(p, smps[i].n);
        if (loc >= 0) glUniform1i(loc, smps[i].u);
    }
    return p;
}

GlesRHI *rhi_gles_create(const WorldGeometry *geo) {
    GlesRHI *r = (GlesRHI *)calloc(1, sizeof(GlesRHI));
    memcpy(r->slices, geo->slices, sizeof(r->slices));
    r->grass_count = geo->grass_count;

    GLint max_s = 0;
    glGetIntegerv(GL_MAX_SAMPLES, &max_s);
    r->msaa_samples = (max_s >= 4) ? 4 : 1;

    r->prog_sky     = link_prog(k_gles_vs_fullscreen, k_gles_fs_sky);
    r->prog_shadow  = link_prog(k_gles_vs_shadow,     k_gles_fs_empty);
    r->prog_main    = link_prog(k_gles_vs_main,       k_gles_fs_main);
    r->prog_outline = link_prog(k_gles_vs_outline,    k_gles_fs_outline);
    r->prog_grass   = link_prog(k_gles_vs_grass,      k_gles_fs_grass);
    r->prog_water   = link_prog(k_gles_vs_water,      k_gles_fs_water);
    r->prog_vfx     = link_prog(k_gles_vs_vfx,        k_gles_fs_vfx);
    r->prog_bloom   = link_prog(k_gles_vs_fullscreen, k_gles_fs_bloom_down);
    r->prog_comp    = link_prog(k_gles_vs_fullscreen, k_gles_fs_composite);
    r->prog_ui      = link_prog(k_gles_vs_ui,         k_gles_fs_ui);
    r->loc_bloom_par = glGetUniformLocation(r->prog_bloom, "u_par");

    glGenBuffers(1, &r->ubo_scene);
    glBindBuffer(GL_UNIFORM_BUFFER, r->ubo_scene);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(SceneUniforms), NULL, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, r->ubo_scene);

    glGenBuffers(1, &r->ubo_draw);
    glBindBuffer(GL_UNIFORM_BUFFER, r->ubo_draw);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(DrawUniforms), NULL, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 1, r->ubo_draw);

    glGenVertexArrays(1, &r->vao_empty);

    glGenVertexArrays(1, &r->vao_world);
    glBindVertexArray(r->vao_world);
    glGenBuffers(1, &r->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, r->vbo);
    glBufferData(GL_ARRAY_BUFFER, geo->vert_count * sizeof(Vertex), geo->verts, GL_STATIC_DRAW);
    glGenBuffers(1, &r->ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, r->ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, geo->index_count * sizeof(uint32_t), geo->indices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(1); glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)12);
    glEnableVertexAttribArray(2); glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)24);
    glEnableVertexAttribArray(3); glVertexAttribIPointer(3, 4, GL_UNSIGNED_BYTE, sizeof(Vertex), (void*)32);
    glEnableVertexAttribArray(4); glVertexAttribIPointer(4, 4, GL_UNSIGNED_BYTE, sizeof(Vertex), (void*)36);
    glEnableVertexAttribArray(5); glVertexAttribIPointer(5, 4, GL_UNSIGNED_BYTE, sizeof(Vertex), (void*)40);
    glEnableVertexAttribArray(6); glVertexAttribIPointer(6, 4, GL_UNSIGNED_BYTE, sizeof(Vertex), (void*)44);

    if (geo->grass_count > 0) {
        glGenBuffers(1, &r->grass_vbo);
        glBindBuffer(GL_ARRAY_BUFFER, r->grass_vbo);
        glBufferData(GL_ARRAY_BUFFER, geo->grass_count * sizeof(GrassInstance), geo->grass, GL_STATIC_DRAW);
        glEnableVertexAttribArray(7); glVertexAttribPointer(7, 4, GL_FLOAT, GL_FALSE, sizeof(GrassInstance), (void*)0);
        glVertexAttribDivisor(7, 1);
        glEnableVertexAttribArray(8); glVertexAttribPointer(8, 4, GL_FLOAT, GL_FALSE, sizeof(GrassInstance), (void*)16);
        glVertexAttribDivisor(8, 1);
    }

    glGenVertexArrays(1, &r->vao_vfx);
    glBindVertexArray(r->vao_vfx);
    glGenBuffers(1, &r->vfx_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, r->vfx_vbo);
    glBufferData(GL_ARRAY_BUFFER, MAX_VFX_VERTS * sizeof(VfxVertex), NULL, GL_STREAM_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VfxVertex), (void*)0);
    glEnableVertexAttribArray(1); glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(VfxVertex), (void*)12);
    glEnableVertexAttribArray(2); glVertexAttribIPointer(2, 4, GL_UNSIGNED_BYTE, sizeof(VfxVertex), (void*)20);
    glEnableVertexAttribArray(3); glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(VfxVertex), (void*)24);

    glGenVertexArrays(1, &r->vao_ui);
    glBindVertexArray(r->vao_ui);
    glGenBuffers(1, &r->ui_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, r->ui_vbo);
    glBufferData(GL_ARRAY_BUFFER, MAX_UI_PRIMS * sizeof(UiPrim), NULL, GL_STREAM_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(UiPrim), (void*)0);  glVertexAttribDivisor(0, 1);
    glEnableVertexAttribArray(1); glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(UiPrim), (void*)16); glVertexAttribDivisor(1, 1);
    glEnableVertexAttribArray(2); glVertexAttribIPointer(2, 2, GL_UNSIGNED_INT, sizeof(UiPrim), (void*)32);   glVertexAttribDivisor(2, 1);
    glEnableVertexAttribArray(3); glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(UiPrim), (void*)40); glVertexAttribDivisor(3, 1);
    glEnableVertexAttribArray(4); glVertexAttribIPointer(4, 2, GL_UNSIGNED_INT, sizeof(UiPrim), (void*)48);   glVertexAttribDivisor(4, 1);
    glBindVertexArray(0);

    glGenTextures(1, &r->tex_shadow);
    glBindTexture(GL_TEXTURE_2D, r->tex_shadow);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_DEPTH_COMPONENT24, 2048, 2048);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    glGenFramebuffers(1, &r->fbo_shadow);
    glBindFramebuffer(GL_FRAMEBUFFER, r->fbo_shadow);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, r->tex_shadow, 0);
    GLenum none_buf = GL_NONE;
    glDrawBuffers(1, &none_buf);

    GLuint prog_gen = link_prog(k_gles_vs_fullscreen, k_gles_fs_gen_tex);
    GLint loc_kind = glGetUniformLocation(prog_gen, "u_kind");
    GLuint tmp_fbo = 0;
    glGenFramebuffers(1, &tmp_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, tmp_fbo);
    glViewport(0, 0, 512, 512);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glUseProgram(prog_gen);
    glBindVertexArray(r->vao_empty);

    glGenTextures(3, r->tex_proc);
    for (int k = 0; k < 3; k++) {
        glBindTexture(GL_TEXTURE_2D, r->tex_proc[k]);
        glTexStorage2D(GL_TEXTURE_2D, 9, GL_RGBA8, 512, 512);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, r->tex_proc[k], 0);
        glUniform1i(loc_kind, k);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glGenerateMipmap(GL_TEXTURE_2D);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &tmp_fbo);
    glDeleteProgram(prog_gen);
    __android_log_print(ANDROID_LOG_INFO, "Eng96k", "GLES 3.0 RHI initialized (%dx MSAA)", r->msaa_samples);
    return r;
}

static void ensure_gles_rts(GlesRHI *r, int w, int h) {
    if (r->rt_w == w && r->rt_h == h && r->fbo_hdr) return;
    r->rt_w = w; r->rt_h = h;

    if (r->fbo_hdr) {
        if (r->fbo_msaa) {
            glDeleteFramebuffers(1, &r->fbo_msaa);
            glDeleteRenderbuffers(1, &r->rbo_color_msaa);
            glDeleteRenderbuffers(1, &r->rbo_depth_msaa);
        }
        glDeleteFramebuffers(1, &r->fbo_hdr);
        glDeleteFramebuffers(1, &r->fbo_hdr_nodp);
        glDeleteTextures(1, &r->tex_hdr);
        glDeleteTextures(1, &r->tex_depth);
        glDeleteFramebuffers(3, r->fbo_bloom);
        glDeleteTextures(3, r->tex_bloom);
    }

    glGenTextures(1, &r->tex_hdr);
    glBindTexture(GL_TEXTURE_2D, r->tex_hdr);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA16F, w, h);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenTextures(1, &r->tex_depth);
    glBindTexture(GL_TEXTURE_2D, r->tex_depth);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_DEPTH_COMPONENT24, w, h);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenFramebuffers(1, &r->fbo_hdr);
    glBindFramebuffer(GL_FRAMEBUFFER, r->fbo_hdr);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, r->tex_hdr, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, r->tex_depth, 0);

    glGenFramebuffers(1, &r->fbo_hdr_nodp);
    glBindFramebuffer(GL_FRAMEBUFFER, r->fbo_hdr_nodp);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, r->tex_hdr, 0);

    if (r->msaa_samples > 1) {
        glGenRenderbuffers(1, &r->rbo_color_msaa);
        glBindRenderbuffer(GL_RENDERBUFFER, r->rbo_color_msaa);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, r->msaa_samples, GL_RGBA16F, w, h);

        glGenRenderbuffers(1, &r->rbo_depth_msaa);
        glBindRenderbuffer(GL_RENDERBUFFER, r->rbo_depth_msaa);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, r->msaa_samples, GL_DEPTH_COMPONENT24, w, h);

        glGenFramebuffers(1, &r->fbo_msaa);
        glBindFramebuffer(GL_FRAMEBUFFER, r->fbo_msaa);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, r->rbo_color_msaa);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, r->rbo_depth_msaa);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            glDeleteFramebuffers(1, &r->fbo_msaa);
            r->fbo_msaa = 0;
            r->msaa_samples = 1;
        }
    }

    glGenTextures(3, r->tex_bloom);
    glGenFramebuffers(3, r->fbo_bloom);
    for (int i = 0; i < 3; i++) {
        int bw = (w >> (i + 1)) > 1 ? (w >> (i + 1)) : 1;
        int bh = (h >> (i + 1)) > 1 ? (h >> (i + 1)) : 1;
        glBindTexture(GL_TEXTURE_2D, r->tex_bloom[i]);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA16F, bw, bh);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindFramebuffer(GL_FRAMEBUFFER, r->fbo_bloom[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, r->tex_bloom[i], 0);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

static void draw_slice_gl(const GlesRHI *r, uint32_t mesh_id) {
    if (mesh_id >= MAX_MESH_SLICES) return;
    MeshSlice s = r->slices[mesh_id];
    if (s.index_count == 0) return;
    glDrawElements(GL_TRIANGLES, (GLsizei)s.index_count, GL_UNSIGNED_INT, (void*)(uintptr_t)(s.first_index * sizeof(uint32_t)));
}

static void upload_draw_u(const GlesRHI *r, const DrawUniforms *du) {
    glBindBuffer(GL_UNIFORM_BUFFER, r->ubo_draw);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(DrawUniforms), du);
}

void rhi_gles_render(GlesRHI *r, const RenderPacket *pkt, int w, int h) {
    if (!r || w < 16 || h < 16) return;
    ensure_gles_rts(r, w, h);

    glBindBuffer(GL_UNIFORM_BUFFER, r->ubo_scene);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(SceneUniforms), &pkt->scene_u);

    DrawUniforms env_du;
    memset(&env_du, 0, sizeof(env_du));
    env_du.model = m4_ident();
    env_du.tint_outline = v4mk(1.0f, 1.0f, 1.0f, 0.85f);

    glDisable(GL_CULL_FACE);

    // PASS 1: 2048x2048 Directional Sun Shadow Map
    glBindFramebuffer(GL_FRAMEBUFFER, r->fbo_shadow);
    glViewport(0, 0, 2048, 2048);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LEQUAL);
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.6f, 1.6f);

    glUseProgram(r->prog_shadow);
    glBindVertexArray(r->vao_world);
    upload_draw_u(r, &env_du);
    draw_slice_gl(r, ENG_MESH_ENV_MAIN);
    for (uint32_t i = 0; i < pkt->draw_count; i++) {
        if (!(pkt->draws[i].flags & DRAW_FLAG_SHADOW)) continue;
        upload_draw_u(r, &pkt->draws[i].u);
        draw_slice_gl(r, pkt->draws[i].mesh_id);
    }
    glDisable(GL_POLYGON_OFFSET_FILL);

    // PASS 2A: 4x MSAA Opaque HDR Scene + Outlines + Grass -> Resolve to tex_hdr & tex_depth
    glBindFramebuffer(GL_FRAMEBUFFER, r->fbo_msaa ? r->fbo_msaa : r->fbo_hdr);
    glViewport(0, 0, w, h);
    glDepthMask(GL_TRUE);
    glClear(GL_DEPTH_BUFFER_BIT);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glUseProgram(r->prog_sky);
    glBindVertexArray(r->vao_empty);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glUseProgram(r->prog_main);
    glBindVertexArray(r->vao_world);
    for (int k = 0; k < 3; k++) {
        glActiveTexture(GL_TEXTURE0 + k);
        glBindTexture(GL_TEXTURE_2D, r->tex_proc[k]);
    }
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, r->tex_shadow);

    upload_draw_u(r, &env_du);
    draw_slice_gl(r, ENG_MESH_ENV_MAIN);
    for (uint32_t i = 0; i < pkt->draw_count; i++) {
        upload_draw_u(r, &pkt->draws[i].u);
        draw_slice_gl(r, pkt->draws[i].mesh_id);
    }

    glDepthMask(GL_FALSE);
    glUseProgram(r->prog_outline);
    upload_draw_u(r, &env_du);
    draw_slice_gl(r, ENG_MESH_ENV_MAIN);
    for (uint32_t i = 0; i < pkt->draw_count; i++) {
        if (!(pkt->draws[i].flags & DRAW_FLAG_OUTLINE)) continue;
        upload_draw_u(r, &pkt->draws[i].u);
        draw_slice_gl(r, pkt->draws[i].mesh_id);
    }

    MeshSlice gs = r->slices[ENG_MESH_GRASS_BLADE];
    if (r->grass_count > 0 && gs.index_count > 0) {
        glDepthMask(GL_TRUE);
        glUseProgram(r->prog_grass);
        glDrawElementsInstanced(GL_TRIANGLES, (GLsizei)gs.index_count, GL_UNSIGNED_INT,
                                (void*)(uintptr_t)(gs.first_index * sizeof(uint32_t)), (GLsizei)r->grass_count);
    }

    if (r->fbo_msaa) {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, r->fbo_msaa);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, r->fbo_hdr);
        glBlitFramebuffer(0, 0, w, h, 0, 0, w, h, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);
    }

    // PASS 2B: Depth-Aware Water + Soft Contact Shadows & Soft HDR VFX Particles
    if (r->slices[ENG_MESH_WATER].index_count > 0 || pkt->vfx_vert_count > 0) {
        glBindFramebuffer(GL_FRAMEBUFFER, r->fbo_hdr_nodp);
        glViewport(0, 0, w, h);
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_2D, r->tex_depth);

        if (r->slices[ENG_MESH_WATER].index_count > 0) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glUseProgram(r->prog_water);
            glBindVertexArray(r->vao_world);
            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, r->tex_proc[2]);
            draw_slice_gl(r, ENG_MESH_WATER);
        }

        if (pkt->vfx_vert_count > 0) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
            glUseProgram(r->prog_vfx);
            glBindVertexArray(r->vao_vfx);
            glBindBuffer(GL_ARRAY_BUFFER, r->vfx_vbo);
            glBufferSubData(GL_ARRAY_BUFFER, 0, pkt->vfx_vert_count * sizeof(VfxVertex), pkt->vfx_verts);
            glDrawArrays(GL_TRIANGLES, 0, (GLsizei)pkt->vfx_vert_count);
        }
    }

    // PASS 3: 3-Mip Karis Dual-Kawase HDR Bloom Pyramid
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glUseProgram(r->prog_bloom);
    glBindVertexArray(r->vao_empty);
    GLuint srcs[3] = {r->tex_hdr, r->tex_bloom[0], r->tex_bloom[1]};
    for (int step = 0; step < 3; step++) {
        int sw = (step == 0) ? w : ((w >> step) > 1 ? (w >> step) : 1);
        int sh = (step == 0) ? h : ((h >> step) > 1 ? (h >> step) : 1);
        int dw = (w >> (step + 1)) > 1 ? (w >> (step + 1)) : 1;
        int dh = (h >> (step + 1)) > 1 ? (h >> (step + 1)) : 1;
        float rad = (step == 0) ? 1.25f : (step == 1 ? 1.85f : 2.35f);
        glBindFramebuffer(GL_FRAMEBUFFER, r->fbo_bloom[step]);
        glViewport(0, 0, dw, dh);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, srcs[step]);
        glUniform4f(r->loc_bloom_par, rad / (float)sw, rad / (float)sh, step == 0 ? 1.0f : 0.0f, 0.0f);
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }

    // PASS 4: Final Composite (SSAO + Tilt-Shift Bokeh + God-Rays + Bloom + AgX) + Vector/SDF HUD
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, w, h);
    glUseProgram(r->prog_comp);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, r->tex_hdr);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, r->tex_bloom[0]);
    glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, r->tex_bloom[1]);
    glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, r->tex_bloom[2]);
    glActiveTexture(GL_TEXTURE4); glBindTexture(GL_TEXTURE_2D, r->tex_depth);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    if (pkt->ui_prim_count > 0) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glUseProgram(r->prog_ui);
        glBindVertexArray(r->vao_ui);
        glBindBuffer(GL_ARRAY_BUFFER, r->ui_vbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0, pkt->ui_prim_count * sizeof(UiPrim), pkt->ui_prims);
        glDrawArraysInstanced(GL_TRIANGLES, 0, 6, (GLsizei)pkt->ui_prim_count);
    }
}

void rhi_gles_destroy(GlesRHI *r) {
    free(r);
}
