#include "rf_capture.h"

#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <cinttypes>
#include <cstdio>
#include <cstdlib>

namespace esphome::rf_capture {

static const char *const TAG = "rf_capture";

static void IRAM_ATTR HOT write_value(RfCaptureStore *arg, uint32_t delta, bool level) {
  int32_t multiplier = ((int32_t) level << 1) - 1;
  uint32_t buffer_write = arg->buffer_write;
  arg->buffer[buffer_write++] = (int32_t) delta * multiplier;
  if (buffer_write >= arg->buffer_entries) {
    buffer_write = 0;
  }

  // The next write would lap the reader. Keep the ring intact and stop capturing
  // until loop() has logged this sequence.
  if (buffer_write == arg->buffer_read) {
    if (!arg->overflow) {
      uint32_t start = arg->buffer_start;
      uint32_t len = buffer_write >= start ? buffer_write - start : arg->buffer_entries - start + buffer_write;
      if (len == 0)
        len = arg->buffer_entries;
      arg->overflow_start = start;
      arg->overflow_count = len;
      arg->overflow = true;
      arg->frozen = true;
    }
    buffer_write = arg->buffer_start;
  }

  if (delta >= arg->idle_us) {
    if (arg->buffer_write == arg->buffer_start) {
      buffer_write = arg->buffer_start;
    } else {
      arg->buffer_start = buffer_write;
    }
  }
  arg->buffer_write = buffer_write;
}

static void IRAM_ATTR HOT commit_value(RfCaptureStore *arg, uint32_t micros, bool level) {
  if (level != arg->commit_level) {
    write_value(arg, micros - arg->commit_micros, level);
    arg->commit_micros = micros;
    arg->commit_level = level;
  }
}

void IRAM_ATTR HOT RfCaptureStore::gpio_intr(RfCaptureStore *arg) {
  if (arg->frozen)
    return;

  const bool curr_level = !arg->pin.digital_read();
  const uint32_t curr_micros = micros();
  const bool prev_level = arg->prev_level;
  const uint32_t prev_micros = arg->prev_micros;

  if (curr_micros - prev_micros >= arg->filter_us && prev_level != curr_level) {
    commit_value(arg, prev_micros, prev_level);
  }
  arg->prev_micros = curr_micros;
  arg->prev_level = curr_level;
}

void RfCapture::setup() {
  this->pin_->setup();
  this->store_.idle_us = this->idle_us_;
  this->store_.filter_us = this->filter_us_;
  this->store_.pin = this->pin_->to_isr();
  this->store_.buffer_entries = (this->buffer_size_ + sizeof(int32_t) - 1) / sizeof(int32_t);
  this->store_.buffer = new int32_t[this->store_.buffer_entries];
  this->store_.prev_micros = micros();
  this->store_.commit_micros = this->store_.prev_micros;
  this->store_.prev_level = this->pin_->digital_read();
  this->store_.commit_level = this->store_.prev_level;
  this->pin_->attach_interrupt(RfCaptureStore::gpio_intr, &this->store_, gpio::INTERRUPT_ANY_EDGE);
  this->high_freq_.start();
}

void RfCapture::dump_config() {
  ESP_LOGCONFIG(TAG,
                "RF capture:\n"
                "  Buffer Size: %" PRIu32 " bytes (%" PRIu32 " pulses)\n"
                "  Tolerance: %" PRIu32 "%s\n"
                "  Filter out pulses shorter than: %" PRIu32 " us\n"
                "  Signal is done after %" PRIu32 " us of no changes",
                this->buffer_size_, this->store_.buffer_entries, this->tolerance_,
                (this->tolerance_mode_ == remote_base::TOLERANCE_MODE_TIME) ? LOG_STR_LITERAL(" us")
                                                                            : LOG_STR_LITERAL("%"),
                this->filter_us_, this->idle_us_);
  LOG_PIN("  Pin: ", this->pin_);
}

void RfCapture::log_overflow_() {
  const uint32_t start = this->store_.overflow_start;
  const uint32_t count = this->store_.overflow_count;
  const uint32_t entries = this->store_.buffer_entries;
  if (count == 0 || entries == 0) {
    ESP_LOGW(TAG, "Buffer overflow: empty capture");
    return;
  }

  uint32_t min_us = UINT32_MAX;
  uint32_t max_us = 0;
  uint32_t below_200 = 0;
  uint32_t symbol = 0;
  uint32_t at_least_1500 = 0;
  uint32_t idx = start;
  for (uint32_t i = 0; i < count; i++) {
    int32_t value = this->store_.buffer[idx];
    uint32_t width = value < 0 ? static_cast<uint32_t>(-value) : static_cast<uint32_t>(value);
    if (width < min_us)
      min_us = width;
    if (width > max_us)
      max_us = width;
    if (width < 200)
      below_200++;
    else if (width < 1500)
      symbol++;
    else
      at_least_1500++;
    if (++idx >= entries)
      idx = 0;
  }

  ESP_LOGW(TAG,
           "Buffer overflow: %" PRIu32 " pulses, min %" PRIu32 " us, max %" PRIu32
           " us, <200us: %" PRIu32 ", 200-1499us: %" PRIu32 ", >=1500us: %" PRIu32,
           count, min_us, max_us, below_200, symbol, at_least_1500);

  char line[160];
  size_t pos = 0;
  idx = start;
  for (uint32_t i = 0; i < count; i++) {
    int32_t value = this->store_.buffer[idx];
    if (++idx >= entries)
      idx = 0;

    int wrote = snprintf(line + pos, sizeof(line) - pos, "%" PRId32 " ", value);
    if (wrote < 0)
      break;
    if (static_cast<size_t>(wrote) >= sizeof(line) - pos) {
      line[pos] = '\0';
      ESP_LOGW(TAG, "%s", line);
      yield();
      pos = 0;
      wrote = snprintf(line, sizeof(line), "%" PRId32 " ", value);
      if (wrote < 0)
        break;
    }
    pos += static_cast<size_t>(wrote);
  }
  if (pos > 0) {
    ESP_LOGW(TAG, "%s", line);
  }
}

void RfCapture::loop() {
  auto &s = this->store_;
  const bool was_frozen = s.frozen;
  if (s.overflow) {
    this->log_overflow_();
    s.overflow = false;
  }

  uint32_t last_index = s.buffer_start;
  if (last_index == s.buffer_read) {
    InterruptLock lock;
    if (!s.frozen && s.buffer_read == s.buffer_start && s.buffer_write != s.buffer_start &&
        micros() - s.prev_micros >= this->idle_us_) {
      commit_value(&s, s.prev_micros, s.prev_level);
      write_value(&s, s.idle_us, !s.commit_level);
      last_index = s.buffer_start;
    }
  }
  if (last_index != s.buffer_read) {
    uint32_t temp_read = s.buffer_read;
    uint32_t reserve_size = 0;
    while (temp_read != last_index && (uint32_t) std::abs(s.buffer[temp_read]) < this->idle_us_) {
      reserve_size++;
      temp_read++;
      if (temp_read >= s.buffer_entries) {
        temp_read = 0;
      }
    }
    this->temp_.clear();
    this->temp_.reserve(reserve_size + 1);

    for (uint32_t i = 0; i < reserve_size + 1; i++) {
      this->temp_.push_back((int32_t) s.buffer[s.buffer_read++]);
      if (s.buffer_read >= s.buffer_entries) {
        s.buffer_read = 0;
      }
    }
    this->call_listeners_dumpers_();
  }

  if (was_frozen) {
    InterruptLock lock;
    s.prev_micros = micros();
    s.commit_micros = s.prev_micros;
    s.frozen = false;
  }
}

}  // namespace esphome::rf_capture
