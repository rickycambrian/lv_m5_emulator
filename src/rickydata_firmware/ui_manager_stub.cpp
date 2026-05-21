#include "ui_manager.h"

#include "lvgl.h"
#include "screen_agents.h"
#include "screen_voice.h"
#include "esp_log.h"

static const char *TAG = "ui_manager_stub";

static screen_id_t s_current_screen = SCREEN_AGENTS;
static lv_obj_t *s_voice_screen = nullptr;
static lv_obj_t *s_agents_screen = nullptr;
static lv_obj_t *s_fallback_screen = nullptr;

extern "C" esp_err_t ui_manager_init(void)
{
    return ESP_OK;
}

static lv_obj_t *fallback_screen(void)
{
    if (s_fallback_screen != nullptr) {
        return s_fallback_screen;
    }

    s_fallback_screen = lv_obj_create(nullptr);
    lv_obj_t *label = lv_label_create(s_fallback_screen);
    lv_label_set_text(label, "Not available in emulator");
    lv_obj_center(label);
    return s_fallback_screen;
}

extern "C" lv_obj_t *ui_manager_get_screen(screen_id_t id)
{
    switch (id) {
    case SCREEN_VOICE:
        if (s_voice_screen == nullptr) {
            s_voice_screen = screen_voice_create();
        }
        return s_voice_screen;
    case SCREEN_AGENTS:
        if (s_agents_screen == nullptr) {
            s_agents_screen = screen_agents_create();
        }
        return s_agents_screen;
    default:
        return fallback_screen();
    }
}

extern "C" void ui_manager_show_screen(screen_id_t id)
{
    lv_obj_t *screen = ui_manager_get_screen(id);
    if (screen == nullptr) {
        ESP_LOGE(TAG, "Cannot show screen %d", static_cast<int>(id));
        return;
    }

    if (lv_scr_act() == nullptr) {
        lv_scr_load(screen);
    } else {
        lv_scr_load_anim(screen, LV_SCR_LOAD_ANIM_MOVE_LEFT, 160, 0, false);
    }
    s_current_screen = id;
}

extern "C" screen_id_t ui_manager_get_current_screen(void)
{
    return s_current_screen;
}

extern "C" bool ui_manager_lock(uint32_t timeout_ms)
{
    (void)timeout_ms;
    return true;
}

extern "C" void ui_manager_unlock(void)
{
}
