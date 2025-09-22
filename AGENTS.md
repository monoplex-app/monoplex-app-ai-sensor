# Repository Guidelines

## Project Structure & Modules
Source lives in `src/`, organised by feature (`wifi_handler.*`, `mqtt_handler.*`, `camera_handler.*`, etc.) with shared declarations in `globals.h` and configuration in `config.*`. Hardware assets (root CA, device credentials) sit in `data/` for SPIFFS deployment; keep replacements out of version control. Header-only helpers belong in `include/`, while reusable libraries should go under `lib/`. Adjust board- and build-specific options only through `platformio.ini` and `partitions.csv`.

## Build, Flash & Monitor
Typical workflow uses PlatformIO from the project root. Run `pio run` to build the default `esp32-s3-devkitc-1` environment. Flash the firmware with `pio run -t upload`, and view serial logs at `115200` via `pio device monitor`. Use `pio run -t clean` before release builds to ensure a fresh artifact. When provisioning, sync new filesystem payloads with `pio run -t uploadfs` once certificates are staged in `data/`.

## Coding Style & Naming
Follow the existing Arduino-flavoured C++ style: 4-space indentation, braces on the same line as declarations, and `lowerCamelCase` for functions and variables (`initPins`, `deviceUid`). Constants and macros stay `UPPER_SNAKE_CASE`. Keep Serial output concise and bilingual comments only where required for downstream teams. Prefer splitting larger features into paired `.cpp/.h` modules inside `src/` to preserve testability.

## Testing Guidelines
Unit and component tests belong under `test/` using PlatformIO's Unity harness. Name suites after the feature under test (e.g. `test/test_mqtt_handler/`). Run them with `pio test -e esp32-s3-devkitc-1`, and document any hardware dependencies in the suite README. For sensor or camera flows, add sanity checks that can execute headlessly, and capture manual verification steps in PR notes if hardware-in-the-loop is unavoidable.

## Commit & PR Expectations
Match the current history by using Conventional Commit prefixes (`feat:`, `fix:`, `chore:`) and writing imperative, scope-aware summaries. Each PR should explain behavioural changes, list affected modules, and link to Jira tickets or GitHub issues. Include firmware size deltas and attach relevant serial logs or screenshots when touching BLE, Wi-Fi, or camera flows. Request at least one reviewer familiar with the target subsystem before merging.
