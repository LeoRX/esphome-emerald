#include "emerald_ble.h"

#include "esphome/core/log.h"

#ifdef USE_ESP32

#include <cstring>

namespace esphome::emerald_ble {

static const char *const TAG = "emerald_ble";
static constexpr uint8_t SET_AUTO_UPLOAD_COMMAND[] = {0x00, 0x01, 0x02, 0x0B, 0x01, 0x01};
static constexpr uint8_t POWER_FRAME_HEADER[] = {0x00, 0x01, 0x02, 0x0A, 0x06};
static constexpr uint16_t POWER_FRAME_LENGTH = 11;
static constexpr uint16_t POWER_FRAME_PULSES_OFFSET = 9;
static constexpr float POWER_INTERVAL_SECONDS = 30.0f;

void Emerald::dump_config() {
  ESP_LOGCONFIG(TAG, "Emerald BLE:");
  LOG_SENSOR("  ", "Battery", this->battery_);
  LOG_SENSOR("  ", "Power", this->power_sensor_);
  ESP_LOGCONFIG(TAG, "  Pulses per kWh: %.1f", this->pulses_per_kwh_);
}

void Emerald::discover_characteristics_() {
  this->characteristics_ready_ = false;
  this->time_read_handle_ = 0;
  this->time_write_handle_ = 0;
  this->battery_handle_ = 0;

  auto *time_read = this->parent()->get_characteristic(EMERALD_SERVICE_TIME_UUID,
                                                        EMERALD_CHARACTERISTIC_TIME_READ_UUID);
  auto *time_write = this->parent()->get_characteristic(EMERALD_SERVICE_TIME_UUID,
                                                         EMERALD_CHARACTERISTIC_TIME_WRITE_UUID);
  auto *battery = this->parent()->get_characteristic(EMERALD_BATTERY_SERVICE_UUID,
                                                      EMERALD_BATTERY_CHARACTERISTIC_UUID);
  // Power is the sole outage-critical characteristic. Emerald units may omit or
  // restrict the standard Battery Service, so battery must never block power setup.
  if (time_read == nullptr || time_write == nullptr) {
    ESP_LOGW(TAG, "Required Emerald power GATT characteristics were not found");
    return;
  }

  this->time_read_handle_ = time_read->handle;
  this->time_write_handle_ = time_write->handle;
  if (battery != nullptr) {
    this->battery_handle_ = battery->handle;
  } else {
    ESP_LOGW(TAG, "Emerald battery characteristic unavailable; continuing with power only");
  }
  this->characteristics_ready_ = true;
  ESP_LOGI(TAG, "Emerald power GATT discovered; battery %s", battery == nullptr ? "unavailable" : "available");
}

void Emerald::configure_after_auth_() {
  // Existing ESPHome BLE bonds do not necessarily emit a fresh AUTH_CMPL event
  // after a firmware update. Service discovery proves the client is connected;
  // the following GATT operations will still fail safely if the peer requires
  // authentication that has not completed.
  if (!this->characteristics_ready_ || this->configured_)
    return;

  this->pending_notify_registrations_ = 1;
  // ESPHome 2026.7.4's BLEClient wrapper does not expose a notification-registration helper.
  // Use the stable ESP-IDF operation directly; REG_FOR_NOTIFY events below keep this component's
  // own subscription lifecycle explicit before it reports itself established.
  const auto notify_status = esp_ble_gattc_register_for_notify(
      this->parent()->get_gattc_if(), this->parent()->get_remote_bda(), this->time_read_handle_);
  if (notify_status != ESP_OK) {
    this->pending_notify_registrations_ = 0;
    ESP_LOGW(TAG, "Unable to register for Emerald power notifications: %d", notify_status);
    return;
  }
  ESP_LOGI(TAG, "Emerald power notification registration requested");

  const auto write_status = esp_ble_gattc_write_char(
      this->parent()->get_gattc_if(), this->parent()->get_conn_id(), this->time_write_handle_,
      sizeof(SET_AUTO_UPLOAD_COMMAND), const_cast<uint8_t *>(SET_AUTO_UPLOAD_COMMAND), ESP_GATT_WRITE_TYPE_NO_RSP,
      ESP_GATT_AUTH_REQ_NONE);
  if (write_status != ESP_OK) {
    ESP_LOGW(TAG, "Unable to enable Emerald automatic uploads: %d", write_status);
    return;
  }
  ESP_LOGI(TAG, "Emerald automatic-upload command queued");

  if (this->battery_handle_ != 0) {
    const auto read_status = esp_ble_gattc_read_char(this->parent()->get_gattc_if(), this->parent()->get_conn_id(),
                                                     this->battery_handle_, ESP_GATT_AUTH_REQ_NONE);
    if (read_status != ESP_OK)
      ESP_LOGW(TAG, "Unable to request optional Emerald battery level: %d", read_status);
  }

  this->configured_ = true;
}

void Emerald::parse_battery_(const uint8_t *data, uint16_t length) {
  if (length != 1) {
    ESP_LOGW(TAG, "Ignoring invalid Emerald battery payload length %u", length);
    return;
  }
  if (this->battery_ != nullptr)
    this->battery_->publish_state(data[0]);
}

void Emerald::decode_power_frame_(const uint8_t *data, uint16_t length) {
  if (length != POWER_FRAME_LENGTH) {
    ESP_LOGW(TAG, "Ignoring Emerald power frame with length %u", length);
    return;
  }
  if (memcmp(data, POWER_FRAME_HEADER, sizeof(POWER_FRAME_HEADER)) != 0) {
    ESP_LOGW(TAG, "Ignoring Emerald frame with unknown header");
    return;
  }
  if (this->pulses_per_kwh_ <= 0.0f) {
    ESP_LOGW(TAG, "Ignoring Emerald power frame: pulses_per_kwh is not configured");
    return;
  }

  const uint16_t pulses = (static_cast<uint16_t>(data[POWER_FRAME_PULSES_OFFSET]) << 8) |
                          static_cast<uint16_t>(data[POWER_FRAME_PULSES_OFFSET + 1]);
  const float watts = pulses * POWER_INTERVAL_SECONDS * 1000.0f / this->pulses_per_kwh_;
  ESP_LOGD(TAG, "Emerald power: %u pulses / 30 s = %.1f W", pulses, watts);
  if (this->power_sensor_ != nullptr)
    this->power_sensor_->publish_state(watts);
}

void Emerald::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                  esp_ble_gattc_cb_param_t *param) {
  switch (event) {
    case ESP_GATTC_SEARCH_CMPL_EVT:
      this->discover_characteristics_();
      this->configure_after_auth_();
      break;
    case ESP_GATTC_READ_CHAR_EVT:
      if (param->read.conn_id != this->parent()->get_conn_id() || param->read.status != ESP_GATT_OK)
        break;
      if (param->read.handle == this->battery_handle_)
        this->parse_battery_(param->read.value, param->read.value_len);
      break;
    case ESP_GATTC_NOTIFY_EVT:
      if (param->notify.conn_id != this->parent()->get_conn_id())
        break;
      if (param->notify.handle == this->time_read_handle_)
        this->decode_power_frame_(param->notify.value, param->notify.value_len);
      else if (param->notify.handle == this->battery_handle_)
        this->parse_battery_(param->notify.value, param->notify.value_len);
      break;
    case ESP_GATTC_REG_FOR_NOTIFY_EVT:
      if (param->reg_for_notify.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "Emerald notify registration failed for handle %u: %d", param->reg_for_notify.handle,
                 param->reg_for_notify.status);
        break;
      }
      if (param->reg_for_notify.handle == this->time_read_handle_ && this->pending_notify_registrations_ > 0) {
        this->pending_notify_registrations_--;
        if (this->pending_notify_registrations_ == 0) {
          this->node_state = espbt::ClientState::ESTABLISHED;
          ESP_LOGI(TAG, "Emerald power notifications established");
        }
      }
      break;
    case ESP_GATTC_DISCONNECT_EVT:
      this->characteristics_ready_ = false;
      this->authenticated_ = false;
      this->configured_ = false;
      this->pending_notify_registrations_ = 0;
      this->time_read_handle_ = 0;
      this->time_write_handle_ = 0;
      this->battery_handle_ = 0;
      break;
    default:
      break;
  }
}

void Emerald::gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
  if (event != ESP_GAP_BLE_AUTH_CMPL_EVT || !this->parent()->check_addr(param->ble_security.auth_cmpl.bd_addr))
    return;

  this->authenticated_ = param->ble_security.auth_cmpl.success;
  if (!this->authenticated_) {
    ESP_LOGW(TAG, "Emerald BLE authentication failed");
    return;
  }
  this->configure_after_auth_();
}

}  // namespace esphome::emerald_ble

#endif  // USE_ESP32
