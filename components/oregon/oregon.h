#pragma once

#include "esphome/components/remote_base/remote_base.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"

#include <vector>

namespace esphome {
namespace oregon {

class Oregon : public Component, public remote_base::RemoteReceiverListener {
 public:
  void dump_config() override;
  bool on_receive(remote_base::RemoteReceiveData data) override;

  void add_sensor(uint8_t channel, uint16_t type, bool match_type, sensor::Sensor *temperature,
                  sensor::Sensor *humidity);

 protected:
  struct Target {
    uint8_t channel;
    uint16_t type;
    bool match_type;
    sensor::Sensor *temperature;
    sensor::Sensor *humidity;
  };

  void publish_(const uint8_t *bytes, uint8_t version);

  std::vector<Target> targets_{};
};

}  // namespace oregon
}  // namespace esphome
