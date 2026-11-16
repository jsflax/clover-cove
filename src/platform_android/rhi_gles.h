#ifndef ENG96K_RHI_GLES_H
#define ENG96K_RHI_GLES_H

#include "../engine96k/eng_types.h"

typedef struct GlesRHI GlesRHI;

GlesRHI *rhi_gles_create(const WorldGeometry *geo);
void     rhi_gles_destroy(GlesRHI *rhi);
void     rhi_gles_render(GlesRHI *rhi, const RenderPacket *pkt, int width, int height);

#endif
