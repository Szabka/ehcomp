#include "oregon.h"
#include "oregon_decoder.h"

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace oregon {

static const char *const TAG = "oregon";

void Oregon::dump_config() {
  ESP_LOGCONFIG(TAG, "Oregon Scientific");
  for (const auto &target : this->targets_) {
    if (target.temperature != nullptr) {
      LOG_SENSOR("  ", "Temperature", target.temperature);
    }
    if (target.humidity != nullptr) {
      LOG_SENSOR("  ", "Humidity", target.humidity);
    }
    if (target.channel == 0 && !target.match_type) {
      ESP_LOGCONFIG(TAG, "    Matches any sensor");
    } else {
      if (target.channel != 0) {
        ESP_LOGCONFIG(TAG, "    Channel: %u", target.channel);
      }
      if (target.match_type) {
        ESP_LOGCONFIG(TAG, "    Type: 0x%04X", target.type);
      }
    }
  }
}

void Oregon::add_sensor(uint8_t channel, uint16_t type, bool match_type, sensor::Sensor *temperature,
                        sensor::Sensor *humidity) {
  this->targets_.push_back(Target{channel, type, match_type, temperature, humidity});
}

void Oregon::publish_(const uint8_t *bytes, uint8_t version) {
  if (version != 2)
    return;
  oregon_decoder::Reading reading = oregon_decoder::parse_v2(bytes);
  if (!reading.has_temperature && !reading.has_humidity)
    return;

  for (const auto &target : this->targets_) {
    if (target.channel != 0 && target.channel != reading.channel)
      continue;
    if (target.match_type && target.type != reading.type)
      continue;
    if (reading.has_temperature && target.temperature != nullptr)
      target.temperature->publish_state(reading.temperature);
    if (reading.has_humidity && target.humidity != nullptr)
      target.humidity->publish_state(reading.humidity);
  }
}

bool Oregon::on_receive(remote_base::RemoteReceiveData data) {
  oregon_decoder::OregonDecoderV2 v2;
  oregon_decoder::OregonDecoderV3 v3;
  bool v2_done = false;
  bool v3_done = false;

  for (int32_t timing : data.get_raw_data()) {
    uint32_t width = timing < 0 ? static_cast<uint32_t>(-timing) : static_cast<uint32_t>(timing);
    if (width == 0)
      continue;
    if (width > 65535)
      width = 65535;
    auto pulse = static_cast<uint16_t>(width);
    if (!v2_done)
      v2_done = v2.next_pulse(pulse);
    if (!v3_done)
      v3_done = v3.next_pulse(pulse);
  }

  if (!v2_done && !v3_done)
    return false;

  if (v2_done) {
    const uint8_t *bytes = v2.data();
    oregon_decoder::Reading reading = oregon_decoder::parse_v2(bytes);
    std::string hex = format_hex_pretty(bytes, v2.size(), ' ', false);
    if (reading.has_temperature && reading.has_humidity) {
      ESP_LOGD(TAG, "Received Oregon v2 %s ch=%u type=0x%04X temp=%.1f C humidity=%.0f%%", hex.c_str(),
               reading.channel, reading.type, reading.temperature, reading.humidity);
    } else if (reading.has_temperature) {
      ESP_LOGD(TAG, "Received Oregon v2 %s ch=%u type=0x%04X temp=%.1f C", hex.c_str(), reading.channel, reading.type,
               reading.temperature);
    } else {
      ESP_LOGD(TAG, "Received Oregon v2 %s", hex.c_str());
    }
    this->publish_(bytes, 2);
  }
  if (v3_done && !v2_done) {
    ESP_LOGD(TAG, "Received Oregon v3 %s", format_hex_pretty(v3.data(), v3.size(), ' ', false).c_str());
  }
  return true;
}

}  // namespace esphome::oregon
}  // namespace esphome
