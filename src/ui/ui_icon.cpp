#include "hg/ui/ui_icon.h"

#include "hg/log.h"
#include "hg/ui/theme.h"

typedef struct {
    lv_obj_t obj;
    const hg_ui_icon_asset_t *asset;
    lv_obj_t *foreground;
    lv_obj_t *background;
    lv_color_t color;
    lv_opa_t opacity;
} ui_icon_t;

const lv_obj_class_t hg_ui_icon_class = {
    .base_class = &lv_obj_class,
    .instance_size = sizeof(ui_icon_t),
};

static ui_icon_t *instance(lv_obj_t *icon) {
    LV_ASSERT_OBJ(icon, &hg_ui_icon_class);
    return reinterpret_cast<ui_icon_t *>(icon);
}

static const ui_icon_t *instance(const lv_obj_t *icon) {
    LV_ASSERT_OBJ(icon, &hg_ui_icon_class);
    return reinterpret_cast<const ui_icon_t *>(icon);
}

static const hg_ui_icon_asset_t *find_asset(hg_ui_icon_id_t id, uint16_t size) {
    for (unsigned i = 0; i < hg_ui_icon_asset_count; ++i) {
        const hg_ui_icon_asset_t *asset = &hg_ui_icon_assets[i];
        if (asset->id == id && asset->size == size) {
            return asset;
        }
    }
    HG_ERROR("ui-icon", "Missing icon %d at %u px\n", static_cast<int>(id), size);
    return nullptr;
}

static void apply_opacity(lv_obj_t *icon) {
    const ui_icon_t *value = instance(icon);
    lv_obj_set_style_img_opa(value->foreground, value->opacity, 0);
#if HG_UI_ICON_DUOTONE
    lv_obj_set_style_img_opa(value->background,
        (static_cast<unsigned>(value->opacity) * value->asset->background_opacity + 127) / 255, 0);
#endif
}

lv_obj_t *hg_ui_icon_create(lv_obj_t *parent, hg_ui_icon_id_t id, uint16_t size) {
    if (!find_asset(id, size)) {
        return nullptr;
    }
    lv_obj_t *icon = lv_obj_class_create_obj(&hg_ui_icon_class, parent);
    lv_obj_class_init_obj(icon);
    lv_obj_remove_style_all(icon);
    lv_obj_clear_flag(icon, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    ui_icon_t *value = instance(icon);
    value->color = hg_theme_fg();
    value->opacity = LV_OPA_COVER;
    for (unsigned i = 0; i < (HG_UI_ICON_DUOTONE ? 2U : 1U); ++i) {
        lv_obj_t *image = lv_img_create(icon);
        lv_obj_remove_style_all(image);
        lv_obj_clear_flag(image, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_img_recolor_opa(image, LV_OPA_COVER, 0);
        lv_obj_set_style_img_recolor(image, value->color, 0);
        if (i == 0 && HG_UI_ICON_DUOTONE) {
            value->background = image;
        } else {
            value->foreground = image;
        }
    }
    hg_ui_icon_set(icon, id, size);
    return icon;
}

void hg_ui_icon_set(lv_obj_t *icon, hg_ui_icon_id_t id, uint16_t size) {
    ui_icon_t *value = instance(icon);
    const hg_ui_icon_asset_t *asset = find_asset(id, size);
    if (!asset || asset == value->asset) {
        return;
    }
    value->asset = asset;
    lv_obj_set_size(icon, size, size);
    lv_img_set_src(value->foreground, asset->foreground);
#if HG_UI_ICON_DUOTONE
    lv_obj_t *background = value->background;
    if (asset->background) {
        lv_img_set_src(background, asset->background);
        lv_obj_clear_flag(background, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(background, LV_OBJ_FLAG_HIDDEN);
    }
#endif
    apply_opacity(icon);
}

void hg_ui_icon_set_color(lv_obj_t *icon, lv_color_t color) {
    ui_icon_t *value = instance(icon);
    if (lv_color_to32(value->color) == lv_color_to32(color)) {
        return;
    }
    value->color = color;
    for (unsigned i = 0; i < lv_obj_get_child_cnt(icon); ++i) {
        lv_obj_set_style_img_recolor(lv_obj_get_child(icon, i), color, 0);
    }
}

void hg_ui_icon_set_opacity(lv_obj_t *icon, lv_opa_t opacity) {
    ui_icon_t *value = instance(icon);
    if (value->opacity != opacity) {
        value->opacity = opacity;
        apply_opacity(icon);
    }
}

const hg_ui_icon_asset_t *hg_ui_icon_get_asset(const lv_obj_t *icon) {
    return instance(icon)->asset;
}

lv_color_t hg_ui_icon_get_color(const lv_obj_t *icon) {
    return instance(icon)->color;
}

lv_opa_t hg_ui_icon_get_opacity(const lv_obj_t *icon) {
    return instance(icon)->opacity;
}
