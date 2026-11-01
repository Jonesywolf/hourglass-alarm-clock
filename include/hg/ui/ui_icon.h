#ifndef HG_UI_UI_ICON_H
#define HG_UI_UI_ICON_H

#include "hg/ui/ui_icon_assets.h"

extern const lv_obj_class_t hg_ui_icon_class;

lv_obj_t *hg_ui_icon_create(lv_obj_t *parent, hg_ui_icon_id_t id, uint16_t size);
void hg_ui_icon_set(lv_obj_t *icon, hg_ui_icon_id_t id, uint16_t size);
void hg_ui_icon_set_color(lv_obj_t *icon, lv_color_t color);
void hg_ui_icon_set_opacity(lv_obj_t *icon, lv_opa_t opacity);
const hg_ui_icon_asset_t *hg_ui_icon_get_asset(const lv_obj_t *icon);
lv_color_t hg_ui_icon_get_color(const lv_obj_t *icon);
lv_opa_t hg_ui_icon_get_opacity(const lv_obj_t *icon);

#endif
