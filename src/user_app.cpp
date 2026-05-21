#include "lvgl.h"
#include "lvgl_port_m5stack.hpp"
#include "screen_agents.h"
#include "screen_voice.h"
#include "ui_manager.h"
#include "ui_theme.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef USE_EEZ_STUDIO
#include "ui/ui.h"

// EEZ Studio specific actions
/*
extern "C" void action_example_for_eez(lv_event_t* e)
{
    // Example action for EEZ Studio
    // You can add your own logic here
    // The following code demonstrates how to change the UI
    lv_obj_t *label = lv_label_create(lv_scr_act());
    lv_label_set_text(label, "Hello EEZ Studio!");
}
*/
#endif


void user_app(void)
{
#ifndef USE_EEZ_STUDIO
    if (lvgl_port_lock()) {
        ui_theme_apply();
        ui_manager_get_screen(SCREEN_VOICE);
        ui_manager_get_screen(SCREEN_AGENTS);
        lvgl_port_unlock();
    }

    static char active_agent_id[80] = "erc8004-expert";
    static char active_agent_name[80] = "ERC-8004 Expert";

    static const char *agents_json =
        "["
        "{\"id\":\"cambrian-beta-lending-private-de6ec3\",\"name\":\"Cambrian Beta Lending\",\"description\":\"Private lending agent\"},"
        "{\"id\":\"erc8004-expert\",\"name\":\"ERC-8004 Expert\",\"description\":\"Protocol and agent wallet help\"},"
        "{\"id\":\"rickydatascience-copilot\",\"name\":\"RickyDataScience Copilot\",\"description\":\"RickyData development copilot\"},"
        "{\"id\":\"geo-expert\",\"name\":\"Geo Expert\",\"description\":\"Geospatial analysis assistant\"},"
        "{\"id\":\"rickydata-security-expert\",\"name\":\"RickyData Security\",\"description\":\"Security review assistant\"}"
        "]";

    auto set_agent_name = [](const char *agent_id) {
        if (strcmp(agent_id, "cambrian-beta-lending-private-de6ec3") == 0) {
            snprintf(active_agent_name, sizeof(active_agent_name), "%s", "Cambrian Lending");
        } else if (strcmp(agent_id, "erc8004-expert") == 0) {
            snprintf(active_agent_name, sizeof(active_agent_name), "%s", "ERC-8004 Expert");
        } else if (strcmp(agent_id, "rickydatascience-copilot") == 0) {
            snprintf(active_agent_name, sizeof(active_agent_name), "%s", "RickyData Copilot");
        } else if (strcmp(agent_id, "geo-expert") == 0) {
            snprintf(active_agent_name, sizeof(active_agent_name), "%s", "Geo Expert");
        } else if (strcmp(agent_id, "rickydata-security-expert") == 0) {
            snprintf(active_agent_name, sizeof(active_agent_name), "%s", "Security Expert");
        } else {
            snprintf(active_agent_name, sizeof(active_agent_name), "%s", agent_id);
        }
    };

    screen_agents_set_callback(
        [](const char *agent_id, void *ctx) {
            (void)ctx;
            snprintf(active_agent_id, sizeof(active_agent_id), "%s", agent_id);
            if (strcmp(agent_id, "cambrian-beta-lending-private-de6ec3") == 0) {
                snprintf(active_agent_name, sizeof(active_agent_name), "%s", "Cambrian Lending");
            } else if (strcmp(agent_id, "erc8004-expert") == 0) {
                snprintf(active_agent_name, sizeof(active_agent_name), "%s", "ERC-8004 Expert");
            } else if (strcmp(agent_id, "rickydatascience-copilot") == 0) {
                snprintf(active_agent_name, sizeof(active_agent_name), "%s", "RickyData Copilot");
            } else if (strcmp(agent_id, "geo-expert") == 0) {
                snprintf(active_agent_name, sizeof(active_agent_name), "%s", "Geo Expert");
            } else if (strcmp(agent_id, "rickydata-security-expert") == 0) {
                snprintf(active_agent_name, sizeof(active_agent_name), "%s", "Security Expert");
            } else {
                snprintf(active_agent_name, sizeof(active_agent_name), "%s", agent_id);
            }

            screen_voice_clear_text();
            screen_voice_set_status(active_agent_name, 92, true);
            screen_voice_append_text("Agent selected.\n");
            screen_voice_append_text(active_agent_id);
            screen_voice_append_text("\n\nHold to talk.\n");
            ui_manager_show_screen(SCREEN_VOICE);
        },
        nullptr);

    screen_voice_set_callback(
        [](int action, void *ctx) {
            (void)ctx;
            if (action == VOICE_ACTION_START_RECORD) {
                screen_voice_clear_text();
                screen_voice_set_button_state(VOICE_BTN_RECORDING);
                screen_voice_append_text("Recording from Core2 microphone...\n");
                screen_voice_append_text("(emulator harness: audio capture is stubbed)\n");
            } else if (action == VOICE_ACTION_STOP_RECORD) {
                screen_voice_set_button_state(VOICE_BTN_PROCESSING);
                screen_voice_append_text("\nTranscript: \"das das\"\n");
                screen_voice_append_text("POST agents.rickydata.org/agents/");
                screen_voice_append_text(active_agent_id);
                screen_voice_append_text("/chat\n\n");
                screen_voice_append_text("Streaming response:\n");
                screen_voice_append_text("This Mac emulator is rendering the firmware LVGL UI. The live Gateway, STT, and TTS path still needs the real Core2 or a separate host network shim.\n");
                screen_voice_set_button_state(VOICE_BTN_IDLE);
            } else if (action == VOICE_ACTION_STOP_PLAYBACK) {
                screen_voice_append_text("\nPlayback stopped.\n");
                screen_voice_set_button_state(VOICE_BTN_IDLE);
            } else if (action == VOICE_ACTION_RETRY) {
                screen_voice_set_button_state(VOICE_BTN_IDLE);
            }
        },
        nullptr);

    set_agent_name(active_agent_id);
    screen_voice_set_status(active_agent_name, 92, true);
    screen_voice_append_text("Agent selected.\n");
    screen_voice_append_text(active_agent_id);
    screen_voice_append_text("\n\nHold to talk.\n");
    screen_agents_set_selected(active_agent_id);
    screen_agents_set_list(agents_json);

    const char *screen = getenv("RICKYDATA_EMULATOR_SCREEN");
    if (screen != nullptr && strcmp(screen, "voice") == 0) {
        ui_manager_show_screen(SCREEN_VOICE);
    } else {
        ui_manager_show_screen(SCREEN_AGENTS);
    }
#else
    // Or you can initialize the UI for EEZ Studio if you are using it
    if (lvgl_port_lock()) {
        ui_init();
        lvgl_port_unlock();
    }
#endif
}
