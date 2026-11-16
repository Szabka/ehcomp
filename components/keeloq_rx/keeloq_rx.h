#pragma once

#include "esphome/components/remote_base/remote_base.h"
#include "esphome/core/component.h"

namespace esphome {
namespace keeloq_rx {

class KeeloqRx : public Component, public remote_base::RemoteReceiverListener {
 public:
  void dump_config() override;
  bool on_receive(remote_base::RemoteReceiveData data) override;
};

}  // namespace keeloq_rx
}  // namespace esphome
