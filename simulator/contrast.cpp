// Browser-only contrast audit support: lists visible text/graphics with their declared colours so
// the JS analyzer can compare them against the rendered framebuffer (WCAG 2.2 AA).
#include "hg/ui/fonts.h"
#include "hg/ui/theme.h"
#include "hg/ui/ui_icon.h"
#include "hg/ui/weather_format.h"

#include <emscripten.h>
#include <lvgl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

namespace {

constexpr size_t kJsonSize = 32768;
constexpr int kMaxNodes = 512;

struct node_t {
    lv_obj_t *obj;
    lv_area_t area;
    bool opaque;
    lv_coord_t radius;
};

char json[kJsonSize];
size_t used = 0;
node_t nodes[kMaxNodes];
int node_count = 0;

void append(const char *format, ...) __attribute__((format(printf, 1, 2)));
void append(const char *format, ...) {
    if (used >= kJsonSize) return;
    va_list args;
    va_start(args, format);
    int written = vsnprintf(json + used, kJsonSize - used, format, args);
    va_end(args);
    used = written < 0 ? kJsonSize : LV_MIN(kJsonSize, used + (size_t)written);
}

void append_string(const char *text) {
    append("\"");
    for (int i = 0; text && text[i] && i < 48; ++i) {
        unsigned char c = (unsigned char)text[i];
        if (c == '"' || c == '\\') append("\\%c", c);
        else if (c < 0x20) append(" ");
        else append("%c", c);
    }
    append("\"");
}

uint32_t rgb(lv_color_t color) { return lv_color_to32(color) & 0xFFFFFF; }

bool visible_area(lv_obj_t *obj, lv_area_t *out) {
    lv_area_t area;
    lv_obj_get_coords(obj, &area);
    for (lv_obj_t *p = obj; p; p = lv_obj_get_parent(p)) {
        if (lv_obj_has_flag(p, LV_OBJ_FLAG_HIDDEN)) return false;
        if (p != obj) {
            lv_area_t parent;
            lv_obj_get_coords(p, &parent);
            if (!_lv_area_intersect(&area, &area, &parent)) return false;
        }
    }
    lv_area_t screen = {0, 0, (lv_coord_t)(LV_HOR_RES - 1), (lv_coord_t)(LV_VER_RES - 1)};
    if (!_lv_area_intersect(&area, &area, &screen)) return false;
    *out = area;
    return true;
}

uint32_t inherited_opa(lv_obj_t *obj) {
    return lv_obj_get_style_opa_recursive(obj, LV_PART_MAIN);
}

int font_px(const lv_font_t *font) {
    if (font == &inter_14) return 14;
    if (font == &inter_28) return 28;
    if (font == &inter_48) return 48;
    if (font == &inter_96) return 96;
    return font ? font->line_height * 5 / 6 : 0;
}

void collect(lv_obj_t *obj) {
    lv_area_t area;
    if (!visible_area(obj, &area)) return;
    if (node_count < kMaxNodes) {
        bool opaque = lv_obj_get_style_bg_opa(obj, LV_PART_MAIN) >= LV_OPA_COVER &&
                      inherited_opa(obj) >= LV_OPA_COVER;
        nodes[node_count++] = {obj, area, opaque, lv_obj_get_style_radius(obj, LV_PART_MAIN)};
    }
    if (lv_obj_check_type(obj, &hg_ui_icon_class)) return;
    for (uint32_t i = 0; i < lv_obj_get_child_cnt(obj); ++i) collect(lv_obj_get_child(obj, i));
}

bool occluded(int index) {
    for (int j = index + 1; j < node_count; ++j) {
        if (nodes[j].opaque &&
            _lv_area_is_in(&nodes[index].area, &nodes[j].area, nodes[j].radius)) return true;
    }
    return false;
}

void emit(const char *kind, lv_obj_t *obj, const lv_area_t *area, int px, bool has_color,
          lv_color_t color, uint32_t opa, const char *text) {
    append("%s{\"kind\":\"%s\",\"x\":%d,\"y\":%d,\"w\":%d,\"h\":%d,\"px\":%d,", used > 1 ? "," : "",
           kind, area->x1, area->y1, lv_area_get_width(area), lv_area_get_height(area), px);
    if (has_color) append("\"color\":%u,", (unsigned)rgb(color));
    lv_area_t full;
    lv_obj_get_coords(obj, &full);
    bool clipped = full.x1 != area->x1 || full.y1 != area->y1 || full.x2 != area->x2 ||
                   full.y2 != area->y2;
    append("\"clipped\":%s,\"opa\":%u,\"text\":", clipped ? "true" : "false", (unsigned)opa);
    append_string(text);
    append("}");
}

void emit_target(int index) {
    lv_obj_t *obj = nodes[index].obj;
    const lv_area_t *area = &nodes[index].area;
    uint32_t opa = inherited_opa(obj);
    if (lv_obj_check_type(obj, &lv_label_class)) {
        const char *text = lv_label_get_text(obj);
        bool blank = true;
        for (const char *c = text; c && *c; ++c) blank = blank && (*c == ' ' || *c == '\n');
        if (blank) return;
        opa = opa * lv_obj_get_style_text_opa(obj, LV_PART_MAIN) / LV_OPA_COVER;
        emit("text", obj, area, font_px(lv_obj_get_style_text_font(obj, LV_PART_MAIN)), true,
             lv_obj_get_style_text_color(obj, LV_PART_MAIN), opa, text);
    } else if (lv_obj_check_type(obj, &hg_ui_icon_class)) {
        emit("icon", obj, area, lv_area_get_height(area), true, hg_ui_icon_get_color(obj),
             opa * hg_ui_icon_get_opacity(obj) / LV_OPA_COVER, "ui-icon");
    } else if (lv_obj_check_type(obj, &lv_img_class)) {
        opa = opa * lv_obj_get_style_img_opa(obj, LV_PART_MAIN) / LV_OPA_COVER;
        bool recolor = lv_obj_get_style_img_recolor_opa(obj, LV_PART_MAIN) >= LV_OPA_COVER;
        emit("image", obj, area, lv_area_get_height(area), recolor,
             lv_obj_get_style_img_recolor(obj, LV_PART_MAIN), opa, "image");
    } else if (lv_obj_check_type(obj, &lv_line_class)) {
        opa = opa * lv_obj_get_style_line_opa(obj, LV_PART_MAIN) / LV_OPA_COVER;
        emit("line", obj, area, lv_area_get_height(area), true,
             lv_obj_get_style_line_color(obj, LV_PART_MAIN), opa, "line");
    }
}

}  // namespace

extern "C" {
EMSCRIPTEN_KEEPALIVE const char *sim_contrast_targets(void) {
    node_count = 0;
    collect(lv_scr_act());
    collect(lv_layer_top());
    collect(lv_layer_sys());
    used = 0;
    append("[");
    for (int i = 0; i < node_count; ++i) {
        if (lv_obj_check_type(nodes[i].obj, &lv_obj_class) || occluded(i)) continue;
        if (inherited_opa(nodes[i].obj) == LV_OPA_TRANSP) continue;
        emit_target(i);
    }
    append("]");
    return json;
}

EMSCRIPTEN_KEEPALIVE const char *sim_theme_tokens(int night) {
    bool previous = hg_theme_nightlight();
    hg_theme_set_nightlight(night != 0);
    static char tokens[512];
    snprintf(tokens, sizeof(tokens),
             "{\"bg\":%u,\"fg\":%u,\"alarm\":%u,\"muted\":%u,\"dim\":%u,\"rain\":%u,"
             "\"aqi\":[%u,%u,%u,%u,%u,%u]}",
             (unsigned)rgb(hg_theme_bg()), (unsigned)rgb(hg_theme_fg()),
             (unsigned)rgb(hg_theme_alarm()), (unsigned)rgb(hg_theme_muted()),
             (unsigned)rgb(hg_theme_dim()), (unsigned)rgb(hg_wx_rain_color()),
             (unsigned)rgb(hg_wx_aqi_color(0)), (unsigned)rgb(hg_wx_aqi_color(1)),
             (unsigned)rgb(hg_wx_aqi_color(2)), (unsigned)rgb(hg_wx_aqi_color(3)),
             (unsigned)rgb(hg_wx_aqi_color(4)), (unsigned)rgb(hg_wx_aqi_color(5)));
    hg_theme_set_nightlight(previous);
    return tokens;
}
}
