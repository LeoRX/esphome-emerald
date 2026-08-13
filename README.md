# ESPHome Emerald BLE

A maintained, narrow ESPHome external component for Emerald Energy Adviser BLE monitors.

## Status

**Development / not production-ready.** This repository is being rebuilt to remove an abandoned 2023 fork of ESPHome core BLE components. It will retain only the Emerald-specific protocol handling and target current upstream ESPHome BLE APIs.

The `components/emerald_ble` directory is deliberately **not published as deployable firmware code until the native port is complete and reviewed**. It contains no production release at this time.

## Design

The planned component will provide:

- native upstream `ble_client` and `esp32_ble_tracker`;
- Emerald passkey pairing through native `on_passkey_request`;
- UUID-based GATT characteristic discovery, never fixed handles;
- validated 30-second power notifications;
- battery telemetry;
- durable energy accounting in Home Assistant: an Integration helper sourced from power, then a daily Utility Meter.

It has **no RAM energy** or daily-energy sensors: device-RAM counters are not treated as authoritative lifetime or daily energy totals.

## Example configuration

[`examples/probe.example.yaml`](examples/probe.example.yaml) is a non-deployable outline. It deliberately references a future immutable release tag/commit instead of a mutable branch. Do not replace it with `main` on a live meter.

Use ESPHome release 2026.7 or later while this port is validated. The initial target is an `esp32dev` board using Arduino; no board, GPIO, or wiring assumptions are made beyond that.

## Security

This is a public project. Never commit `secrets.yaml`, live YAML, MAC addresses, IP addresses, API keys, OTA passwords, Wi-Fi credentials, pairing codes, packet captures containing identifying data, or build artefacts. The example uses `!secret` placeholders on purpose.

## Safe migration rule

Do **not** OTA this implementation onto the only production probe. Compile and test it on a spare ESP32 first, compare against the existing probe for at least 24 hours, then promote it only after verified BLE reconnect and measurement behaviour.

## Test

```sh
python3 -m unittest discover -s tests -v
```
