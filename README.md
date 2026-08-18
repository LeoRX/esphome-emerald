# ESPHome Emerald BLE

A maintained ESPHome external component for Emerald Energy Adviser BLE monitors. It uses stock ESPHome BLE primitives and only implements the Emerald-specific protocol.

## What it does

- discovers GATT characteristics by UUID rather than fixed handles;
- authenticates through ESPHome's native BLE passkey callback;
- publishes validated instantaneous **power** readings and optional battery telemetry;
- deliberately does **not** publish a device-RAM lifetime or daily-energy counter.

That last point matters: the ESP32 is a live measurement transport, not the accounting system. Home Assistant owns durable energy accounting.

## Status and supported baseline

`v0.1.0` is the first pinned public release for the native component. It has protocol-vector tests and has been validated against current ESPHome BLE APIs. Use **ESPHome 2026.7 or newer**.

This repository contains no production secrets, MAC addresses, credentials, packet captures, firmware images, or deployment YAML.

## ESPHome setup

Copy [`examples/probe.example.yaml`](examples/probe.example.yaml) **outside this repository** and create a local `secrets.yaml`. Keep both private.

Pin the component to a release tag (or a full reviewed commit) rather than `main`:

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/LeoRX/esphome-emerald
      ref: v0.1.0
      path: components
    components: [emerald_ble]
```

The component needs stock ESPHome BLE support plus one `ble_client` with `auto_connect: true` and an `on_passkey_request` handler. The non-deployable example shows the full shape.

### Safe deployment

1. Make a private backup of your existing ESPHome YAML and, where possible, a serial flash backup.
2. Compile before upload; do not let a live deployment pull an unreviewed mutable branch.
3. OTA normally preserves ESP32 NVS/BLE bonds. Do **not** erase flash merely to update the component.
4. After upload, verify that power changes over time—not just that one old value remains displayed.
5. Keep a serial recovery path for a sole production probe.

## Home Assistant setup: durable grid-import accounting

The `power` entity is watts and **must not** be used directly in Energy Dashboard. Create an Integration helper from it:

- Source: your Emerald power sensor
- Method: `left`
- Unit prefix: `k`
- Unit time: `h`
- Maximum sub-interval: `30 seconds`

This creates a cumulative `kWh` entity with `device_class: energy` and an eligible energy state class. Use it as the canonical source for all accounting.

### Tariff meters

Create one multi-tariff Utility Meter sourced from the cumulative HA energy entity:

- Reset cycle: **Daily**
- Reset offset: **0 days**
- Supported tariffs: `free`, `offpeak`, `peak`
- Net consumption: **off**
- Delta values: **off**
- Periodically resetting: **off** (the HA energy source is monotonic)
- Sensor always available: **off**

Use the generated tariff sensors as **Grid consumption** sources in Energy Dashboard. Keep your tariff-switching automation pointed at the generated `select` entity. Do not add solar/export sources unless the physical meter truly measures them.

### Availability and limitations

Use a sustained-outage alert (ten minutes is sensible) for the Emerald power entity. HA's Integration helper survives ESP32 resets, but it cannot reconstruct energy during intervals where the meter, BLE connection, ESP32, or HA provides no power samples. Accounting resumes when telemetry returns.

## Updating or migrating

If replacing a previous device-side kWh sensor, recreate or repoint tariff Utility Meters to the HA-owned cumulative source. Preserve tariff names and the generated entity IDs where possible. Do not feed a Utility Meter from an old volatile ESP32 total.

## Development

Run the protocol tests locally:

```sh
python3 -m unittest discover -s tests -v
```

GitHub Actions runs the same test vector suite on push and pull requests.

## Security

Never commit live YAML, `secrets.yaml`, MAC/IP addresses, API keys, OTA passwords, Wi-Fi credentials, pairing codes, or build artefacts. See [SECURITY.md](SECURITY.md).
