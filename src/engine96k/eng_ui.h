#ifndef ENG96K_UI_H
#define ENG96K_UI_H

#include "eng_types.h"

static inline uint32_t rgba8(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    return ((uint32_t)r) | ((uint32_t)g << 8) | ((uint32_t)b << 16) | ((uint32_t)a << 24);
}

static inline bool ui_hit_rect(float x, float y, float x0, float y0, float x1, float y1) {
    return x >= x0 && x <= x1 && y >= y0 && y <= y1;
}

uint32_t ui_glyph_mask(char ch);
void     ui_push_panel(RenderPacket *pkt, float x0, float y0, float x1, float y1,
                       uint32_t bg, uint32_t border_col, float radius, float border_px);
void     ui_push_text(RenderPacket *pkt, float x, float y, float h_px, const char *str, uint32_t col, bool center_x);

#endif // ENG96K_UI_H
