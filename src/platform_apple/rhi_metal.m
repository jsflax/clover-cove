#import "rhi_metal.h"
#include "../engine96k/eng_shaders.h"

@interface MetalRHIObj : NSObject {
@public
    id<MTLDevice> dev;
    id<MTLCommandQueue> queue;
    NSUInteger msaa_samples;

    id<MTLBuffer> vbo;
    id<MTLBuffer> ibo;
    id<MTLBuffer> grass_buf;
    id<MTLBuffer> vfx_buf;
    id<MTLBuffer> ui_buf;
    MeshSlice slices[MAX_MESH_SLICES];
    uint32_t grass_count;

    id<MTLTexture> tex_proc[3];
    id<MTLTexture> tex_shadow;

    uint32_t rt_w, rt_h;
    id<MTLTexture> tex_hdr_msaa;
    id<MTLTexture> tex_depth_msaa;
    id<MTLTexture> tex_hdr;
    id<MTLTexture> tex_depth;
    id<MTLTexture> tex_bloom[3];

    id<MTLSamplerState> smp_linear;
    id<MTLSamplerState> smp_clamp;
    id<MTLSamplerState> smp_shadow;
    id<MTLDepthStencilState> ds_read_write;
    id<MTLDepthStencilState> ds_read_only;
    id<MTLDepthStencilState> ds_none;

    id<MTLRenderPipelineState> pso_sky;
    id<MTLRenderPipelineState> pso_shadow;
    id<MTLRenderPipelineState> pso_main;
    id<MTLRenderPipelineState> pso_outline;
    id<MTLRenderPipelineState> pso_grass;
    id<MTLRenderPipelineState> pso_water;
    id<MTLRenderPipelineState> pso_vfx;
    id<MTLRenderPipelineState> pso_bloom;
    id<MTLRenderPipelineState> pso_composite;
    id<MTLRenderPipelineState> pso_ui;
}
@end
@implementation MetalRHIObj
@end

static MTLVertexDescriptor *make_world_vdesc(void) {
    MTLVertexDescriptor *vd = [MTLVertexDescriptor new];
    uint32_t b = 7;
    vd.attributes[0].format = MTLVertexFormatFloat3; vd.attributes[0].offset = 0;  vd.attributes[0].bufferIndex = b;
    vd.attributes[1].format = MTLVertexFormatFloat3; vd.attributes[1].offset = 12; vd.attributes[1].bufferIndex = b;
    vd.attributes[2].format = MTLVertexFormatFloat2; vd.attributes[2].offset = 24; vd.attributes[2].bufferIndex = b;
    vd.attributes[3].format = MTLVertexFormatUChar4; vd.attributes[3].offset = 32; vd.attributes[3].bufferIndex = b;
    vd.attributes[4].format = MTLVertexFormatUChar4; vd.attributes[4].offset = 36; vd.attributes[4].bufferIndex = b;
    vd.attributes[5].format = MTLVertexFormatUChar4; vd.attributes[5].offset = 40; vd.attributes[5].bufferIndex = b;
    vd.attributes[6].format = MTLVertexFormatUChar4; vd.attributes[6].offset = 44; vd.attributes[6].bufferIndex = b;
    vd.layouts[b].stride = sizeof(Vertex);
    return vd;
}

static MTLVertexDescriptor *make_vfx_vdesc(void) {
    MTLVertexDescriptor *vd = [MTLVertexDescriptor new];
    uint32_t b = 7;
    vd.attributes[0].format = MTLVertexFormatFloat3; vd.attributes[0].offset = 0;  vd.attributes[0].bufferIndex = b;
    vd.attributes[1].format = MTLVertexFormatFloat2; vd.attributes[1].offset = 12; vd.attributes[1].bufferIndex = b;
    vd.attributes[2].format = MTLVertexFormatUChar4; vd.attributes[2].offset = 20; vd.attributes[2].bufferIndex = b;
    vd.attributes[3].format = MTLVertexFormatFloat2; vd.attributes[3].offset = 24; vd.attributes[3].bufferIndex = b;
    vd.layouts[b].stride = sizeof(VfxVertex);
    return vd;
}

static id<MTLRenderPipelineState> build_pso(id<MTLDevice> dev, id<MTLLibrary> lib,
                                            NSString *vs, NSString *fs,
                                            MTLVertexDescriptor *vd,
                                            MTLPixelFormat color_fmt, MTLPixelFormat depth_fmt,
                                            int blend_mode, NSUInteger samples) {
    MTLRenderPipelineDescriptor *pd = [MTLRenderPipelineDescriptor new];
    pd.vertexFunction = [lib newFunctionWithName:vs];
    if (fs) pd.fragmentFunction = [lib newFunctionWithName:fs];
    if (vd) pd.vertexDescriptor = vd;
    pd.rasterSampleCount = samples;
    pd.depthAttachmentPixelFormat = depth_fmt;
    if (color_fmt != MTLPixelFormatInvalid) {
        MTLRenderPipelineColorAttachmentDescriptor *ca = pd.colorAttachments[0];
        ca.pixelFormat = color_fmt;
        if (blend_mode > 0) {
            ca.blendingEnabled = YES;
            ca.sourceRGBBlendFactor = (blend_mode == 2) ? MTLBlendFactorOne : MTLBlendFactorSourceAlpha;
            ca.destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
            ca.sourceAlphaBlendFactor = MTLBlendFactorOne;
            ca.destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
        }
    }
    NSError *err = nil;
    id<MTLRenderPipelineState> pso = [dev newRenderPipelineStateWithDescriptor:pd error:&err];
    if (!pso) NSLog(@"PSO Error (%@/%@): %@", vs, fs, err);
    return pso;
}

MetalRHI *rhi_metal_create(id<MTLDevice> device, const WorldGeometry *geo, MTLPixelFormat dst_format) {
    MetalRHIObj *rhi = [MetalRHIObj new];
    rhi->dev = device;
    rhi->queue = [device newCommandQueue];

    BOOL can_depth_resolve = NO;
    if (@available(macOS 11.0, iOS 14.0, *)) {
        can_depth_resolve = [device supportsFamily:MTLGPUFamilyApple3] || [device supportsFamily:MTLGPUFamilyMac2];
    }
    rhi->msaa_samples = (can_depth_resolve && [device supportsTextureSampleCount:4]) ? 4 : 1;

    NSError *err = nil;
    id<MTLLibrary> lib = [device newLibraryWithSource:[NSString stringWithUTF8String:k_metal_shader_src]
                                              options:nil error:&err];
    if (!lib) {
        NSLog(@"Metal Shader Compile Error: %@", err);
        return NULL;
    }

    rhi->vbo = [device newBufferWithBytes:geo->verts length:geo->vert_count * sizeof(Vertex) options:MTLResourceStorageModeShared];
    rhi->ibo = [device newBufferWithBytes:geo->indices length:geo->index_count * sizeof(uint32_t) options:MTLResourceStorageModeShared];
    if (geo->grass_count > 0) {
        rhi->grass_buf = [device newBufferWithBytes:geo->grass length:geo->grass_count * sizeof(GrassInstance) options:MTLResourceStorageModeShared];
    }
    rhi->vfx_buf = [device newBufferWithLength:MAX_VFX_VERTS * sizeof(VfxVertex) options:MTLResourceStorageModeShared];
    rhi->ui_buf  = [device newBufferWithLength:MAX_UI_PRIMS * sizeof(UiPrim) options:MTLResourceStorageModeShared];
    memcpy(rhi->slices, geo->slices, sizeof(rhi->slices));
    rhi->grass_count = geo->grass_count;

    MTLSamplerDescriptor *sd = [MTLSamplerDescriptor new];
    sd.minFilter = MTLSamplerMinMagFilterLinear;
    sd.magFilter = MTLSamplerMinMagFilterLinear;
    sd.mipFilter = MTLSamplerMipFilterLinear;
    sd.maxAnisotropy = 8;
    sd.sAddressMode = MTLSamplerAddressModeRepeat;
    sd.tAddressMode = MTLSamplerAddressModeRepeat;
    rhi->smp_linear = [device newSamplerStateWithDescriptor:sd];

    sd.mipFilter = MTLSamplerMipFilterNotMipmapped;
    sd.maxAnisotropy = 1;
    sd.sAddressMode = MTLSamplerAddressModeClampToEdge;
    sd.tAddressMode = MTLSamplerAddressModeClampToEdge;
    rhi->smp_clamp = [device newSamplerStateWithDescriptor:sd];

    sd.compareFunction = MTLCompareFunctionLessEqual;
    rhi->smp_shadow = [device newSamplerStateWithDescriptor:sd];

    MTLDepthStencilDescriptor *dd = [MTLDepthStencilDescriptor new];
    dd.depthCompareFunction = MTLCompareFunctionLessEqual;
    dd.depthWriteEnabled = YES;
    rhi->ds_read_write = [device newDepthStencilStateWithDescriptor:dd];
    dd.depthWriteEnabled = NO;
    rhi->ds_read_only = [device newDepthStencilStateWithDescriptor:dd];
    dd.depthCompareFunction = MTLCompareFunctionAlways;
    rhi->ds_none = [device newDepthStencilStateWithDescriptor:dd];

    MTLTextureDescriptor *shd = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float
                                                                                   width:2048 height:2048 mipmapped:NO];
    shd.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
    shd.storageMode = MTLStorageModePrivate;
    rhi->tex_shadow = [device newTextureWithDescriptor:shd];

    MTLVertexDescriptor *vd_world = make_world_vdesc();
    MTLVertexDescriptor *vd_vfx   = make_vfx_vdesc();
    NSUInteger ms = rhi->msaa_samples;
    rhi->pso_sky       = build_pso(device, lib, @"vs_fullscreen", @"fs_sky",        nil,      MTLPixelFormatRGBA16Float, MTLPixelFormatDepth32Float, 0, ms);
    rhi->pso_shadow    = build_pso(device, lib, @"vs_shadow",     nil,              vd_world, MTLPixelFormatInvalid,     MTLPixelFormatDepth32Float, 0, 1);
    rhi->pso_main      = build_pso(device, lib, @"vs_main",       @"fs_main",       vd_world, MTLPixelFormatRGBA16Float, MTLPixelFormatDepth32Float, 0, ms);
    rhi->pso_outline   = build_pso(device, lib, @"vs_outline",    @"fs_outline",    vd_world, MTLPixelFormatRGBA16Float, MTLPixelFormatDepth32Float, 0, ms);
    rhi->pso_grass     = build_pso(device, lib, @"vs_grass",      @"fs_grass",      vd_world, MTLPixelFormatRGBA16Float, MTLPixelFormatDepth32Float, 0, ms);
    rhi->pso_water     = build_pso(device, lib, @"vs_water",      @"fs_water",      vd_world, MTLPixelFormatRGBA16Float, MTLPixelFormatInvalid,      1, 1);
    rhi->pso_vfx       = build_pso(device, lib, @"vs_vfx",        @"fs_vfx",        vd_vfx,   MTLPixelFormatRGBA16Float, MTLPixelFormatInvalid,      2, 1);
    rhi->pso_bloom     = build_pso(device, lib, @"vs_fullscreen", @"fs_bloom_down", nil,      MTLPixelFormatRGBA16Float, MTLPixelFormatInvalid,      0, 1);
    rhi->pso_composite = build_pso(device, lib, @"vs_fullscreen", @"fs_composite",  nil,      dst_format,                MTLPixelFormatInvalid,      0, 1);
    rhi->pso_ui        = build_pso(device, lib, @"vs_ui",         @"fs_ui",         nil,      dst_format,                MTLPixelFormatInvalid,      1, 1);

    // Synthesize 3 512x512 Mipmapped Procedural PBR Textures on GPU (< 1 ms!)
    id<MTLRenderPipelineState> pso_gen = build_pso(device, lib, @"vs_fullscreen", @"fs_gen_tex", nil, MTLPixelFormatRGBA8Unorm, MTLPixelFormatInvalid, 0, 1);
    id<MTLCommandBuffer> cb = [rhi->queue commandBuffer];
    for (int k = 0; k < 3; k++) {
        MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                                                      width:512 height:512 mipmapped:YES];
        td.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
        td.storageMode = MTLStorageModePrivate;
        rhi->tex_proc[k] = [device newTextureWithDescriptor:td];

        MTLRenderPassDescriptor *rp = [MTLRenderPassDescriptor renderPassDescriptor];
        rp.colorAttachments[0].texture = rhi->tex_proc[k];
        rp.colorAttachments[0].loadAction = MTLLoadActionDontCare;
        rp.colorAttachments[0].storeAction = MTLStoreActionStore;
        id<MTLRenderCommandEncoder> enc = [cb renderCommandEncoderWithDescriptor:rp];
        [enc setRenderPipelineState:pso_gen];
        [enc setFragmentBytes:&k length:sizeof(int) atIndex:0];
        [enc drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
        [enc endEncoding];
    }
    id<MTLBlitCommandEncoder> blit = [cb blitCommandEncoder];
    for (int k = 0; k < 3; k++) {
        [blit generateMipmapsForTexture:rhi->tex_proc[k]];
    }
    [blit endEncoding];
    [cb commit];
    [cb waitUntilCompleted];

    return (MetalRHI *)CFBridgingRetain(rhi);
}

static void ensure_render_targets(MetalRHIObj *rhi, uint32_t w, uint32_t h) {
    if (rhi->rt_w == w && rhi->rt_h == h && rhi->tex_hdr) return;
    rhi->rt_w = w; rhi->rt_h = h;

    MTLTextureDescriptor *hd = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA16Float width:w height:h mipmapped:NO];
    hd.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
    hd.storageMode = MTLStorageModePrivate;
    rhi->tex_hdr = [rhi->dev newTextureWithDescriptor:hd];

    MTLTextureDescriptor *dp = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float width:w height:h mipmapped:NO];
    dp.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
    dp.storageMode = MTLStorageModePrivate;
    rhi->tex_depth = [rhi->dev newTextureWithDescriptor:dp];

    if (rhi->msaa_samples > 1) {
        MTLTextureDescriptor *mc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA16Float width:w height:h mipmapped:NO];
        mc.textureType = MTLTextureType2DMultisample;
        mc.sampleCount = rhi->msaa_samples;
        mc.usage = MTLTextureUsageRenderTarget;
        mc.storageMode = MTLStorageModePrivate;
        rhi->tex_hdr_msaa = [rhi->dev newTextureWithDescriptor:mc];

        MTLTextureDescriptor *md = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float width:w height:h mipmapped:NO];
        md.textureType = MTLTextureType2DMultisample;
        md.sampleCount = rhi->msaa_samples;
        md.usage = MTLTextureUsageRenderTarget;
        md.storageMode = MTLStorageModePrivate;
        rhi->tex_depth_msaa = [rhi->dev newTextureWithDescriptor:md];
    }

    for (int i = 0; i < 3; i++) {
        uint32_t bw = (w >> (i + 1)) > 1 ? (w >> (i + 1)) : 1;
        uint32_t bh = (h >> (i + 1)) > 1 ? (h >> (i + 1)) : 1;
        hd.width = bw; hd.height = bh;
        rhi->tex_bloom[i] = [rhi->dev newTextureWithDescriptor:hd];
    }
}

static void draw_slice(id<MTLRenderCommandEncoder> enc, MetalRHIObj *rhi, uint32_t mesh_id) {
    if (mesh_id >= MAX_MESH_SLICES) return;
    MeshSlice s = rhi->slices[mesh_id];
    if (s.index_count == 0) return;
    [enc drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                    indexCount:s.index_count
                     indexType:MTLIndexTypeUInt32
                   indexBuffer:rhi->ibo
             indexBufferOffset:s.first_index * sizeof(uint32_t)];
}

void rhi_metal_render(MetalRHI *raw_rhi, const RenderPacket *pkt, id<MTLTexture> dst_tex, id<CAMetalDrawable> opt_drawable) {
    if (!raw_rhi || !dst_tex) return;
    MetalRHIObj *rhi = (__bridge MetalRHIObj *)raw_rhi;
    uint32_t w = (uint32_t)dst_tex.width, h = (uint32_t)dst_tex.height;
    ensure_render_targets(rhi, w, h);

    id<MTLCommandBuffer> cb = [rhi->queue commandBuffer];

    DrawUniforms env_du;
    memset(&env_du, 0, sizeof(env_du));
    env_du.model = m4_ident();
    env_du.tint_outline = v4mk(1.0f, 1.0f, 1.0f, 0.85f);

    // PASS 1: 2048x2048 Directional Sun Shadow Map
    {
        MTLRenderPassDescriptor *rp = [MTLRenderPassDescriptor renderPassDescriptor];
        rp.depthAttachment.texture = rhi->tex_shadow;
        rp.depthAttachment.loadAction = MTLLoadActionClear;
        rp.depthAttachment.storeAction = MTLStoreActionStore;
        rp.depthAttachment.clearDepth = 1.0;

        id<MTLRenderCommandEncoder> enc = [cb renderCommandEncoderWithDescriptor:rp];
        [enc setRenderPipelineState:rhi->pso_shadow];
        [enc setDepthStencilState:rhi->ds_read_write];
        [enc setDepthBias:0.0012f slopeScale:1.6f clamp:0.015f];
        [enc setVertexBuffer:rhi->vbo offset:0 atIndex:7];
        [enc setVertexBytes:&pkt->scene_u length:sizeof(SceneUniforms) atIndex:0];

        [enc setVertexBytes:&env_du length:sizeof(DrawUniforms) atIndex:1];
        draw_slice(enc, rhi, ENG_MESH_ENV_MAIN);

        for (uint32_t i = 0; i < pkt->draw_count; i++) {
            if (!(pkt->draws[i].flags & DRAW_FLAG_SHADOW)) continue;
            [enc setVertexBytes:&pkt->draws[i].u length:sizeof(DrawUniforms) atIndex:1];
            draw_slice(enc, rhi, pkt->draws[i].mesh_id);
        }
        [enc endEncoding];
    }

    // PASS 2A: 4x MSAA Opaque HDR Scene + Outlines + Grass -> Resolve Color & Depth
    {
        MTLRenderPassDescriptor *rp = [MTLRenderPassDescriptor renderPassDescriptor];
        if (rhi->msaa_samples > 1) {
            rp.colorAttachments[0].texture = rhi->tex_hdr_msaa;
            rp.colorAttachments[0].resolveTexture = rhi->tex_hdr;
            rp.colorAttachments[0].loadAction = MTLLoadActionDontCare;
            rp.colorAttachments[0].storeAction = MTLStoreActionMultisampleResolve;
            rp.depthAttachment.texture = rhi->tex_depth_msaa;
            rp.depthAttachment.resolveTexture = rhi->tex_depth;
            rp.depthAttachment.loadAction = MTLLoadActionClear;
            rp.depthAttachment.storeAction = MTLStoreActionMultisampleResolve;
        } else {
            rp.colorAttachments[0].texture = rhi->tex_hdr;
            rp.colorAttachments[0].loadAction = MTLLoadActionDontCare;
            rp.colorAttachments[0].storeAction = MTLStoreActionStore;
            rp.depthAttachment.texture = rhi->tex_depth;
            rp.depthAttachment.loadAction = MTLLoadActionClear;
            rp.depthAttachment.storeAction = MTLStoreActionStore;
        }
        rp.depthAttachment.clearDepth = 1.0;

        id<MTLRenderCommandEncoder> enc = [cb renderCommandEncoderWithDescriptor:rp];

        // 2a. Sky Dome
        [enc setRenderPipelineState:rhi->pso_sky];
        [enc setDepthStencilState:rhi->ds_none];
        [enc setFragmentBytes:&pkt->scene_u length:sizeof(SceneUniforms) atIndex:0];
        [enc drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];

        // 2b. World & Dynamic Props / Skinned Characters
        [enc setRenderPipelineState:rhi->pso_main];
        [enc setDepthStencilState:rhi->ds_read_write];
        [enc setVertexBuffer:rhi->vbo offset:0 atIndex:7];
        [enc setVertexBytes:&pkt->scene_u length:sizeof(SceneUniforms) atIndex:0];
        [enc setFragmentBytes:&pkt->scene_u length:sizeof(SceneUniforms) atIndex:0];
        [enc setFragmentTexture:rhi->tex_proc[0] atIndex:0];
        [enc setFragmentTexture:rhi->tex_proc[1] atIndex:1];
        [enc setFragmentTexture:rhi->tex_proc[2] atIndex:2];
        [enc setFragmentTexture:rhi->tex_shadow  atIndex:3];
        [enc setFragmentSamplerState:rhi->smp_linear atIndex:0];
        [enc setFragmentSamplerState:rhi->smp_shadow atIndex:1];

        [enc setVertexBytes:&env_du length:sizeof(DrawUniforms) atIndex:1];
        [enc setFragmentBytes:&env_du length:sizeof(DrawUniforms) atIndex:1];
        draw_slice(enc, rhi, ENG_MESH_ENV_MAIN);

        for (uint32_t i = 0; i < pkt->draw_count; i++) {
            [enc setVertexBytes:&pkt->draws[i].u length:sizeof(DrawUniforms) atIndex:1];
            [enc setFragmentBytes:&pkt->draws[i].u length:sizeof(DrawUniforms) atIndex:1];
            draw_slice(enc, rhi, pkt->draws[i].mesh_id);
        }

        // 2c. Stylized Ink Outlines
        [enc setRenderPipelineState:rhi->pso_outline];
        [enc setDepthStencilState:rhi->ds_read_only];
        [enc setVertexBytes:&env_du length:sizeof(DrawUniforms) atIndex:1];
        draw_slice(enc, rhi, ENG_MESH_ENV_MAIN);
        for (uint32_t i = 0; i < pkt->draw_count; i++) {
            if (!(pkt->draws[i].flags & DRAW_FLAG_OUTLINE)) continue;
            [enc setVertexBytes:&pkt->draws[i].u length:sizeof(DrawUniforms) atIndex:1];
            draw_slice(enc, rhi, pkt->draws[i].mesh_id);
        }

        // 2d. Hardware-Instanced Grass & Wildflowers
        MeshSlice gs = rhi->slices[ENG_MESH_GRASS_BLADE];
        if (rhi->grass_count > 0 && gs.index_count > 0 && rhi->grass_buf) {
            [enc setRenderPipelineState:rhi->pso_grass];
            [enc setDepthStencilState:rhi->ds_read_write];
            [enc setVertexBuffer:rhi->grass_buf offset:0 atIndex:1];
            [enc setFragmentTexture:rhi->tex_shadow atIndex:0];
            [enc setFragmentSamplerState:rhi->smp_shadow atIndex:0];
            [enc drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                            indexCount:gs.index_count
                             indexType:MTLIndexTypeUInt32
                           indexBuffer:rhi->ibo
                     indexBufferOffset:gs.first_index * sizeof(uint32_t)
                         instanceCount:rhi->grass_count];
        }
        [enc endEncoding];
    }

    // PASS 2B: Depth-Aware Water + Soft Contact Shadows & Soft HDR VFX Particles
    if (rhi->slices[ENG_MESH_WATER].index_count > 0 || pkt->vfx_vert_count > 0) {
        MTLRenderPassDescriptor *rp = [MTLRenderPassDescriptor renderPassDescriptor];
        rp.colorAttachments[0].texture = rhi->tex_hdr;
        rp.colorAttachments[0].loadAction = MTLLoadActionLoad;
        rp.colorAttachments[0].storeAction = MTLStoreActionStore;

        id<MTLRenderCommandEncoder> enc = [cb renderCommandEncoderWithDescriptor:rp];
        [enc setVertexBytes:&pkt->scene_u length:sizeof(SceneUniforms) atIndex:0];
        [enc setFragmentBytes:&pkt->scene_u length:sizeof(SceneUniforms) atIndex:0];

        if (rhi->slices[ENG_MESH_WATER].index_count > 0) {
            [enc setRenderPipelineState:rhi->pso_water];
            [enc setVertexBuffer:rhi->vbo offset:0 atIndex:7];
            [enc setFragmentTexture:rhi->tex_proc[2] atIndex:0];
            [enc setFragmentTexture:rhi->tex_depth   atIndex:1];
            [enc setFragmentSamplerState:rhi->smp_linear atIndex:0];
            draw_slice(enc, rhi, ENG_MESH_WATER);
        }

        if (pkt->vfx_vert_count > 0) {
            [enc setRenderPipelineState:rhi->pso_vfx];
            memcpy(rhi->vfx_buf.contents, pkt->vfx_verts, pkt->vfx_vert_count * sizeof(VfxVertex));
            [enc setVertexBuffer:rhi->vfx_buf offset:0 atIndex:7];
            [enc setFragmentTexture:rhi->tex_depth atIndex:0];
            [enc drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:pkt->vfx_vert_count];
        }
        [enc endEncoding];
    }

    // PASS 3: 3-Mip Karis Dual-Kawase HDR Bloom Pyramid
    id<MTLTexture> b_srcs[3] = {rhi->tex_hdr, rhi->tex_bloom[0], rhi->tex_bloom[1]};
    for (int step = 0; step < 3; step++) {
        MTLRenderPassDescriptor *rp = [MTLRenderPassDescriptor renderPassDescriptor];
        rp.colorAttachments[0].texture = rhi->tex_bloom[step];
        rp.colorAttachments[0].loadAction = MTLLoadActionDontCare;
        rp.colorAttachments[0].storeAction = MTLStoreActionStore;
        id<MTLRenderCommandEncoder> enc = [cb renderCommandEncoderWithDescriptor:rp];
        [enc setRenderPipelineState:rhi->pso_bloom];
        [enc setFragmentTexture:b_srcs[step] atIndex:0];
        [enc setFragmentSamplerState:rhi->smp_clamp atIndex:0];
        float rad = (step == 0) ? 1.25f : (step == 1 ? 1.85f : 2.35f);
        v4 par = v4mk(rad / (float)b_srcs[step].width,
                      rad / (float)b_srcs[step].height,
                      step == 0 ? 1.0f : 0.0f, 0.0f);
        [enc setFragmentBytes:&par length:sizeof(v4) atIndex:0];
        [enc drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
        [enc endEncoding];
    }

    // PASS 4: Composite (SSAO + Tilt-Shift Bokeh + God-Rays + Bloom + AgX) + Vector/SDF HUD
    {
        MTLRenderPassDescriptor *rp = [MTLRenderPassDescriptor renderPassDescriptor];
        rp.colorAttachments[0].texture = dst_tex;
        rp.colorAttachments[0].loadAction = MTLLoadActionDontCare;
        rp.colorAttachments[0].storeAction = MTLStoreActionStore;

        id<MTLRenderCommandEncoder> enc = [cb renderCommandEncoderWithDescriptor:rp];
        [enc setRenderPipelineState:rhi->pso_composite];
        [enc setFragmentBytes:&pkt->scene_u length:sizeof(SceneUniforms) atIndex:0];
        [enc setFragmentTexture:rhi->tex_hdr      atIndex:0];
        [enc setFragmentTexture:rhi->tex_bloom[0] atIndex:1];
        [enc setFragmentTexture:rhi->tex_bloom[1] atIndex:2];
        [enc setFragmentTexture:rhi->tex_bloom[2] atIndex:3];
        [enc setFragmentTexture:rhi->tex_depth    atIndex:4];
        [enc setFragmentSamplerState:rhi->smp_clamp atIndex:0];
        [enc drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];

        if (pkt->ui_prim_count > 0) {
            [enc setRenderPipelineState:rhi->pso_ui];
            [enc setVertexBytes:&pkt->scene_u length:sizeof(SceneUniforms) atIndex:0];
            memcpy(rhi->ui_buf.contents, pkt->ui_prims, pkt->ui_prim_count * sizeof(UiPrim));
            [enc setVertexBuffer:rhi->ui_buf offset:0 atIndex:1];
            [enc drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:6 instanceCount:pkt->ui_prim_count];
        }
        [enc endEncoding];
    }

    if (opt_drawable) [cb presentDrawable:opt_drawable];
    [cb commit];
    if (!opt_drawable) [cb waitUntilCompleted];
}

void rhi_metal_destroy(MetalRHI *raw_rhi) {
    if (raw_rhi) CFBridgingRelease(raw_rhi);
}
