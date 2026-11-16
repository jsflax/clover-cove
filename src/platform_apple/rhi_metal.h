#ifndef ENG96K_RHI_METAL_H
#define ENG96K_RHI_METAL_H

#include "../engine96k/eng_types.h"
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

typedef struct MetalRHI MetalRHI;

MetalRHI *rhi_metal_create(id<MTLDevice> device, const WorldGeometry *geo, MTLPixelFormat dst_format);
void      rhi_metal_destroy(MetalRHI *rhi);
void      rhi_metal_render(MetalRHI *rhi, const RenderPacket *pkt, id<MTLTexture> dst_tex, id<CAMetalDrawable> opt_drawable);

#endif
