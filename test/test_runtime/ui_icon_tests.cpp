#include <climits>
#include <unity.h>

#include "hg/services/wifi_signal.h"
#include "hg/ui/theme.h"
#include "hg/ui/ui_icon.h"

void test_wifi_signal_boundaries(void) {
    const int values[] = {INT_MIN, -86, -85, -84, -76, -75, -74, -66, -65, -64, -55, INT_MAX};
    const hg_wifi_bars_t expected[] = {
        HG_WIFI_BARS_NONE, HG_WIFI_BARS_NONE, HG_WIFI_BARS_LOW, HG_WIFI_BARS_LOW,
        HG_WIFI_BARS_LOW, HG_WIFI_BARS_MEDIUM, HG_WIFI_BARS_MEDIUM, HG_WIFI_BARS_MEDIUM,
        HG_WIFI_BARS_HIGH, HG_WIFI_BARS_HIGH, HG_WIFI_BARS_HIGH, HG_WIFI_BARS_HIGH,
    };
    for (unsigned i = 0; i < sizeof(values) / sizeof(values[0]); ++i) {
        TEST_ASSERT_EQUAL(expected[i], hg_wifi_bars_from_rssi(values[i]));
    }
}

void test_ui_icon_assets_and_layers(void) {
    lv_obj_t *icon = hg_ui_icon_create(lv_layer_top(), HG_UI_ICON_ALARM, 48);
    TEST_ASSERT_NOT_NULL(icon);
    TEST_ASSERT_FALSE(lv_obj_has_flag(icon, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    TEST_ASSERT_EQUAL(HG_UI_ICON_DUOTONE ? 2 : 1, lv_obj_get_child_cnt(icon));
    for (unsigned i = 0; i < hg_ui_icon_asset_count; ++i) {
        const hg_ui_icon_asset_t *asset = &hg_ui_icon_assets[i];
        TEST_ASSERT_EQUAL(LV_IMG_CF_ALPHA_8BIT, asset->foreground->header.cf);
        TEST_ASSERT_EQUAL(asset->size * asset->size, asset->foreground->data_size);
        unsigned coverage = 0;
        for (unsigned j = 0; j < asset->foreground->data_size; ++j) {
            coverage += asset->foreground->data[j];
        }
        TEST_ASSERT_GREATER_THAN(0, coverage);
        hg_ui_icon_set(icon, asset->id, asset->size);
        TEST_ASSERT_TRUE(asset == hg_ui_icon_get_asset(icon));
        hg_ui_icon_set_color(icon, hg_theme_alarm());
        lv_obj_update_layout(icon);
        TEST_ASSERT_EQUAL(asset->size, lv_obj_get_width(icon));
        TEST_ASSERT_EQUAL(asset->size, lv_obj_get_height(icon));
        const lv_opa_t opacities[] = {LV_OPA_COVER, LV_OPA_60, LV_OPA_TRANSP, LV_OPA_COVER};
        for (lv_opa_t opacity : opacities) {
            hg_ui_icon_set_opacity(icon, opacity);
            TEST_ASSERT_EQUAL(opacity, hg_ui_icon_get_opacity(icon));
            lv_obj_t *foreground = lv_obj_get_child(icon, -1);
            TEST_ASSERT_TRUE(lv_img_get_src(foreground) == asset->foreground);
            TEST_ASSERT_EQUAL(opacity, lv_obj_get_style_img_opa(foreground, 0));
            TEST_ASSERT_EQUAL_HEX32(lv_color_to32(hg_theme_alarm()),
                                   lv_color_to32(lv_obj_get_style_img_recolor(foreground, 0)));
#if HG_UI_ICON_DUOTONE
            lv_obj_t *background = lv_obj_get_child(icon, 0);
            TEST_ASSERT_EQUAL(!asset->background, lv_obj_has_flag(background, LV_OBJ_FLAG_HIDDEN));
            if (asset->background) {
                TEST_ASSERT_EQUAL(51, asset->background_opacity);
                TEST_ASSERT_TRUE(lv_img_get_src(background) == asset->background);
                TEST_ASSERT_EQUAL((opacity * 51U + 127) / 255, lv_obj_get_style_img_opa(background, 0));
                TEST_ASSERT_EQUAL_HEX32(lv_color_to32(hg_theme_alarm()),
                                       lv_color_to32(lv_obj_get_style_img_recolor(background, 0)));
                TEST_ASSERT_EQUAL(asset->size * asset->size, asset->background->data_size);
            }
#else
            TEST_ASSERT_NULL(asset->background);
#endif
        }
    }
    lv_obj_del(icon);
}
