#include "lvgl.h"
#include "lvgl_port_m5stack.hpp"
#include "screen_agents.h"
#include "screen_voice.h"
#include "ui_manager.h"
#include "ui_theme.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef USE_EEZ_STUDIO
#include <atomic>
#include <chrono>
#include <string>
#include <thread>

#if !defined(ARDUINO) && __has_include(<arpa/inet.h>) && __has_include(<sys/socket.h>) && __has_include(<unistd.h>)
#define RICKYDATA_HOST_BRIDGE 1
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#else
#define RICKYDATA_HOST_BRIDGE 0
#endif
#endif

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

#ifndef USE_EEZ_STUDIO
namespace {

static char s_active_agent_id[80] = "erc8004-expert";
static char s_active_agent_name[80] = "ERC-8004 Expert";
static std::atomic<bool> s_bridge_request_running{false};

static const char *k_agents_json =
    "["
    "{\"id\":\"cambrian-beta-lending-private-de6ec3\",\"name\":\"Cambrian Beta Lending\",\"description\":\"Private lending agent\"},"
    "{\"id\":\"erc8004-expert\",\"name\":\"ERC-8004 Expert\",\"description\":\"Protocol and agent wallet help\"},"
    "{\"id\":\"rickydatascience-copilot\",\"name\":\"RickyDataScience Copilot\",\"description\":\"RickyData development copilot\"},"
    "{\"id\":\"geo-expert\",\"name\":\"Geo Expert\",\"description\":\"Geospatial analysis assistant\"},"
    "{\"id\":\"rickydata-security-expert\",\"name\":\"RickyData Security\",\"description\":\"Security review assistant\"}"
    "]";

static void set_agent_name(const char *agent_id)
{
    if (strcmp(agent_id, "cambrian-beta-lending-private-de6ec3") == 0) {
        snprintf(s_active_agent_name, sizeof(s_active_agent_name), "%s", "Cambrian Lending");
    } else if (strcmp(agent_id, "erc8004-expert") == 0) {
        snprintf(s_active_agent_name, sizeof(s_active_agent_name), "%s", "ERC-8004 Expert");
    } else if (strcmp(agent_id, "rickydatascience-copilot") == 0) {
        snprintf(s_active_agent_name, sizeof(s_active_agent_name), "%s", "RickyData Copilot");
    } else if (strcmp(agent_id, "geo-expert") == 0) {
        snprintf(s_active_agent_name, sizeof(s_active_agent_name), "%s", "Geo Expert");
    } else if (strcmp(agent_id, "rickydata-security-expert") == 0) {
        snprintf(s_active_agent_name, sizeof(s_active_agent_name), "%s", "Security Expert");
    } else {
        snprintf(s_active_agent_name, sizeof(s_active_agent_name), "%s", agent_id);
    }
}

static void append_text_locked(const std::string &text)
{
    if (text.empty()) return;
    if (lvgl_port_lock()) {
        screen_voice_append_text(text.c_str());
        lvgl_port_unlock();
    }
}

static void set_button_state_locked(voice_btn_state_t state)
{
    if (lvgl_port_lock()) {
        screen_voice_set_button_state(state);
        lvgl_port_unlock();
    }
}

static std::string getenv_or(const char *name, const char *fallback)
{
    const char *value = getenv(name);
    return value && value[0] ? value : fallback;
}

static std::string json_escape(const std::string &input)
{
    std::string out;
    out.reserve(input.size() + 8);
    for (char c : input) {
        switch (c) {
        case '\\': out += "\\\\"; break;
        case '"':  out += "\\\""; break;
        case '\n': out += "\\n";  break;
        case '\r': out += "\\r";  break;
        case '\t': out += "\\t";  break;
        default:   out += c;      break;
        }
    }
    return out;
}

static std::string base64_decode(const std::string &input)
{
    static const signed char table[256] = {
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,
        52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,
        -1,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,
        15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
        -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
        41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
    };

    std::string out;
    int value = 0;
    int bits = -8;
    for (unsigned char c : input) {
        if (c == '=') break;
        signed char decoded = table[c];
        if (decoded == -1) continue;
        value = (value << 6) + decoded;
        bits += 6;
        if (bits >= 0) {
            out.push_back(static_cast<char>((value >> bits) & 0xFF));
            bits -= 8;
        }
    }
    return out;
}

#if RICKYDATA_HOST_BRIDGE
static bool send_all(int fd, const std::string &data)
{
    const char *ptr = data.c_str();
    size_t remaining = data.size();
    while (remaining > 0) {
        ssize_t sent = send(fd, ptr, remaining, 0);
        if (sent <= 0) return false;
        ptr += sent;
        remaining -= static_cast<size_t>(sent);
    }
    return true;
}

static void handle_sse_line(const std::string &raw_line, std::string &event_name)
{
    std::string line = raw_line;
    if (!line.empty() && line.back() == '\r') line.pop_back();

    if (line.rfind("event: ", 0) == 0) {
        event_name = line.substr(7);
        return;
    }
    if (line.rfind("data: ", 0) != 0) return;

    const std::string payload = base64_decode(line.substr(6));
    if (payload.empty()) return;

    if (event_name == "text") {
        append_text_locked(payload);
    } else if (event_name == "tool_call") {
        append_text_locked("\nUsing: " + payload + "\n");
    } else if (event_name == "tool_result") {
        append_text_locked("\nTool result: " + payload + "\n");
    } else if (event_name == "done") {
        append_text_locked("\n" + payload + "\n");
    } else if (event_name == "error") {
        append_text_locked("\nError: " + payload + "\n");
    } else {
        append_text_locked(payload + "\n");
    }
}

static void process_sse_bytes(std::string &line_buffer, const std::string &bytes,
                              std::string &event_name)
{
    line_buffer += bytes;
    size_t newline = std::string::npos;
    while ((newline = line_buffer.find('\n')) != std::string::npos) {
        std::string line = line_buffer.substr(0, newline);
        line_buffer.erase(0, newline + 1);
        handle_sse_line(line, event_name);
    }
}

static bool post_bridge_chat(const std::string &agent_id, const std::string &message)
{
    const std::string host = getenv_or("RICKYDATA_BRIDGE_HOST", "127.0.0.1");
    const int port = atoi(getenv_or("RICKYDATA_BRIDGE_PORT", "8765").c_str());

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return false;

    sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));
    if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
        close(fd);
        return false;
    }
    if (connect(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0) {
        close(fd);
        return false;
    }

    std::string body = "{\"agentId\":\"" + json_escape(agent_id) +
                       "\",\"message\":\"" + json_escape(message) + "\"}";
    std::string request =
        "POST /chat HTTP/1.1\r\n"
        "Host: " + host + ":" + std::to_string(port) + "\r\n"
        "Content-Type: application/json\r\n"
        "Accept: text/event-stream\r\n"
        "Connection: close\r\n"
        "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" +
        body;

    if (!send_all(fd, request)) {
        close(fd);
        return false;
    }

    bool headers_done = false;
    bool http_ok = false;
    std::string pending;
    std::string line_buffer;
    std::string event_name = "status";
    char buf[1024];
    while (true) {
        ssize_t n = recv(fd, buf, sizeof(buf), 0);
        if (n <= 0) break;
        std::string chunk(buf, static_cast<size_t>(n));

        if (!headers_done) {
            pending += chunk;
            size_t pos = pending.find("\r\n\r\n");
            if (pos == std::string::npos) continue;
            std::string headers = pending.substr(0, pos);
            http_ok = headers.find("HTTP/1.1 200") != std::string::npos ||
                      headers.find("HTTP/1.0 200") != std::string::npos;
            headers_done = true;
            process_sse_bytes(line_buffer, pending.substr(pos + 4), event_name);
            pending.clear();
        } else {
            process_sse_bytes(line_buffer, chunk, event_name);
        }
    }

    close(fd);
    return headers_done && http_ok;
}
#endif

static void run_bridge_chat_async(const std::string &message)
{
    if (s_bridge_request_running.exchange(true)) {
        screen_voice_append_text("\nBridge request already running.\n");
        return;
    }

    const std::string agent_id = s_active_agent_id;
    std::thread([agent_id, message]() {
#if RICKYDATA_HOST_BRIDGE
        append_text_locked("SDK bridge: connecting...\n\n");
        bool ok = post_bridge_chat(agent_id, message);
        if (!ok) {
            append_text_locked("SDK bridge unavailable on 127.0.0.1:8765.\n");
            append_text_locked("Start it with: node tools/rickydata_terminal_bridge.mjs\n");
        }
#else
        (void)agent_id;
        append_text_locked("SDK bridge is only enabled for the native SDL emulator build.\n");
#endif
        set_button_state_locked(VOICE_BTN_IDLE);
        s_bridge_request_running = false;
    }).detach();
}

static void start_emulated_voice_turn(const char *message)
{
    screen_voice_set_button_state(VOICE_BTN_PROCESSING);
    screen_voice_append_text("\nTranscript: \"");
    screen_voice_append_text(message);
    screen_voice_append_text("\"\n");
    screen_voice_append_text("POST agents.rickydata.org/agents/");
    screen_voice_append_text(s_active_agent_id);
    screen_voice_append_text("/chat\n\n");
    run_bridge_chat_async(message);
}

static void select_agent(const char *agent_id)
{
    snprintf(s_active_agent_id, sizeof(s_active_agent_id), "%s", agent_id);
    set_agent_name(agent_id);
    screen_voice_clear_text();
    screen_voice_set_status(s_active_agent_name, 92, true);
    screen_voice_append_text("Agent selected.\n");
    screen_voice_append_text(s_active_agent_id);
    screen_voice_append_text("\n\nHold to talk.\n");
    ui_manager_show_screen(SCREEN_VOICE);
}

} // namespace
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

    screen_agents_set_callback(
        [](const char *agent_id, void *ctx) {
            (void)ctx;
            select_agent(agent_id);
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
                const std::string transcript = getenv_or(
                    "RICKYDATA_EMULATOR_TRANSCRIPT",
                    "Say one short sentence confirming live RickyData SDK bridge connectivity.");
                start_emulated_voice_turn(transcript.c_str());
            } else if (action == VOICE_ACTION_STOP_PLAYBACK) {
                screen_voice_append_text("\nPlayback stopped.\n");
                screen_voice_set_button_state(VOICE_BTN_IDLE);
            } else if (action == VOICE_ACTION_RETRY) {
                screen_voice_set_button_state(VOICE_BTN_IDLE);
            }
        },
        nullptr);

    set_agent_name(s_active_agent_id);
    screen_voice_set_status(s_active_agent_name, 92, true);
    screen_voice_append_text("Agent selected.\n");
    screen_voice_append_text(s_active_agent_id);
    screen_voice_append_text("\n\nHold to talk.\n");
    screen_agents_set_selected(s_active_agent_id);
    screen_agents_set_list(k_agents_json);

    const char *screen = getenv("RICKYDATA_EMULATOR_SCREEN");
    const bool auto_chat = getenv("RICKYDATA_EMULATOR_AUTO_CHAT") != nullptr;
    if ((screen != nullptr && strcmp(screen, "voice") == 0) || auto_chat) {
        ui_manager_show_screen(SCREEN_VOICE);
    } else {
        ui_manager_show_screen(SCREEN_AGENTS);
    }

    if (auto_chat) {
        std::thread([]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            const std::string transcript = getenv_or(
                "RICKYDATA_EMULATOR_TRANSCRIPT",
                "Say one short sentence confirming live RickyData SDK bridge connectivity.");
            if (lvgl_port_lock()) {
                screen_voice_clear_text();
                screen_voice_set_status(s_active_agent_name, 92, true);
                lvgl_port_unlock();
            }
            append_text_locked("Auto SDK bridge smoke test.\n");
            if (lvgl_port_lock()) {
                start_emulated_voice_turn(transcript.c_str());
                lvgl_port_unlock();
            }
        }).detach();
    }
#else
    // Or you can initialize the UI for EEZ Studio if you are using it
    if (lvgl_port_lock()) {
        ui_init();
        lvgl_port_unlock();
    }
#endif
}
