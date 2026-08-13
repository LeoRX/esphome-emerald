#pragma once

#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"

#ifdef USE_ESP32

#include <esp_gattc_api.h>

namespace esphome::emerald_ble {

namespace espbt = esphome::esp32_ble_tracker;

static const espbt::ESPBTUUID EMERALD_SERVICE_TIME_UUID =
    espbt::ESPBTUUID::from_raw("00001910-0000-1000-8000-00805f9b34fb");
static const espbt::ESPBTUUID EMERALD_CHARACTERISTIC_TIME_READ_UUID =
    espbt::ESPBTUUID::from_raw("00002b10-0000-1000-8000-00805f9b34fb");
static const espbt::ESPBTUUID EMERALD_CHARACTERISTIC_TIME_WRITE_UUID =
    espbt::ESPBTUUID::from_raw("00002b11-0000-1000-8000-00805f9b34fb");

static const espbt::ESPBTUUID EMERALD_BATTERY_SERVICE_UUID = espbt::ESPBTUUID::from_uint16(0x180F);
static const espbt::ESPBTUUID EMERALD_BATTERY_CHARACTERISTIC_UUID = espbt::ESPBTUUID::from_uint16(0x2A19);

class Emerald : public ble_client::BLEClientNode, public Component {
 public:
  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;
  void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_battery(sensor::Sensor *battery) { this->battery_ = battery; }
  void set_power_sensor(sensor::Sensor *power_sensor) { this->power_sensor_ = power_sensor; }
  void set_pulses_per_kwh(float pulses_per_kwh) { this->pulses_per_kwh_ = pulses_per_kwh; }

 protected:
  void discover_characteristics_();
  void configure_after_auth_();
  void parse_battery_(const uint8_t *data, uint16_t length);
  void decode_power_frame_(const uint8_t *data, uint16_t length);

  sensor::Sensor *battery_{nullptr};
  sensor::Sensor *power_sensor_{nullptr};
  float pulses_per_kwh_{0.0f};
  uint16_t time_read_handle_{0};
  uint16_t time_write_handle_{0};
  uint16_t battery_handle_{0};
  bool characteristics_ready_{false};
  bool authenticated_{false};
  bool configured_{false};
  // Do not mark the node established until every native notify registration
  // requested by this component has completed successfully.
  uint8_t pending_notify_registrations_{0};
};

}  // namespace esphome::emerald_ble

#endif  // USE_ESP32
