# RickyData Terminal Emulator Harness

This fork can run the M5Stack Core2 SDL/LVGL emulator with the RickyData terminal firmware UI screens.

## Run

```bash
./tools/run_rickydata_terminal_emulator.sh
```

Or run the bridge and emulator separately:

```bash
node tools/rickydata_terminal_bridge.mjs
pio run -e emulator_Core2
RICKYDATA_EMULATOR_SCREEN=voice ./.pio/build/emulator_Core2/program
```

The emulator expects `../rickydata_code` beside this checkout for the firmware UI sources and `$HOME/esp/esp-idf` for ESP-IDF's `cJSON`. The bridge loads the local RickyData SDK from `../rickydata_SDK` or from `RICKYDATA_SDK_PATH`, reads `~/.rickydata/credentials.json`, and uses the selected wallet token against `https://agents.rickydata.org`.

## Useful Environment

- `RICKYDATA_PROFILE`: RickyData credentials profile. Defaults to `default`.
- `RICKYDATA_BRIDGE_PORT`: local bridge port. Defaults to `8765`.
- `RICKYDATA_EMULATOR_SCREEN`: `agents` or `voice`.
- `RICKYDATA_EMULATOR_AUTO_CHAT=1`: start a live SDK chat smoke test automatically after launch.
- `RICKYDATA_EMULATOR_TRANSCRIPT`: text sent to the active agent when the emulated hold-to-talk flow releases.
- `RICKYDATA_MODEL`: optional model override passed through the SDK chat call.

## Coverage

This harness covers LVGL layout, touch/click callbacks, agent selection, RickyData auth/profile loading, SDK session creation, Gateway SSE streaming, and rendering live text/tool events.

It does not emulate the ESP32 hardware path: AXP192 PMIC, CH9102F UART, I2S microphone/speaker, SPI SD/display contention, ESP-IDF heap/TLS fragmentation, watchdog timing, or GPIO0 audio sharing.
