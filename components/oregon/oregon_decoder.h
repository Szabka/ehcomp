#pragma once

#include <cstdint>
#include <cstring>

// Oregon V2/V3 pulse decoder.
// Pulse state machine from oregonStation.ino: MIT DecodeOOK (Jean-Claude Wippler, 2010)
// with the Oregon V2/V3 changes by Dominique Pierre. Value layout matches that sketch.

namespace oregon_decoder {

class DecodeOOK {
 public:
  enum State : uint8_t { UNKNOWN, T0, T1, T2, T3, OK, DONE };

  DecodeOOK() { reset(); }

  bool next_pulse(uint16_t width) {
    if (this->state_ != DONE) {
      switch (this->decode(width)) {
        case -1:
          this->reset();
          break;
        case 1:
          this->done();
          break;
        default:
          break;
      }
    }
    return this->state_ == DONE;
  }

  bool is_done() const { return this->state_ == DONE; }
  const uint8_t *data() const { return this->data_; }
  uint8_t size() const { return this->pos_; }

  void reset() {
    this->total_bits_ = 0;
    this->bits_ = 0;
    this->pos_ = 0;
    this->flip_ = 0;
    this->state_ = UNKNOWN;
    memset(this->data_, 0, sizeof(this->data_));
  }

 protected:
  virtual int8_t decode(uint16_t width) = 0;

  virtual void got_bit(uint8_t value) {
    this->total_bits_++;
    uint8_t *ptr = this->data_ + this->pos_;
    *ptr = (*ptr >> 1) | (value << 7);
    if (++this->bits_ >= 8) {
      this->bits_ = 0;
      if (++this->pos_ >= sizeof(this->data_)) {
        this->reset();
        return;
      }
    }
    this->state_ = OK;
  }

  void manchester(uint8_t value) {
    this->flip_ ^= value;
    this->got_bit(this->flip_);
  }

  void done() {
    while (this->bits_)
      this->got_bit(0);
    this->state_ = DONE;
  }

  uint8_t total_bits_{0};
  uint8_t bits_{0};
  uint8_t flip_{0};
  uint8_t state_{UNKNOWN};
  uint8_t pos_{0};
  uint8_t data_[16]{};
};

class OregonDecoderV2 : public DecodeOOK {
 public:
  // Keep every other Manchester bit. 160 raw bits -> 10 bytes.
  void got_bit(uint8_t value) override {
    if ((this->total_bits_ & 0x01) == 0) {
      this->data_[this->pos_] = (this->data_[this->pos_] >> 1) | (value ? 0x80 : 0);
    }
    this->total_bits_++;
    this->pos_ = this->total_bits_ >> 4;
    if (this->pos_ >= sizeof(this->data_)) {
      this->reset();
      return;
    }
    this->state_ = OK;
  }

 protected:
  int8_t decode(uint16_t width) override {
    // The last data pulse is often stretched by the gap that follows the frame.
    if (this->total_bits_ > 144 && width >= 1400)
      width = 1300;
    if (200 <= width && width < 1400) {
      uint8_t w = width >= 700;
      switch (this->state_) {
        case UNKNOWN:
          if (w != 0) {
            ++this->flip_;
          } else if (30 <= this->flip_) {
            // Short pulse after the preamble. The spec uses 32 ones; accept 30
            // because the receiver may have missed the start.
            this->flip_ = 0;
            this->state_ = T0;
          } else {
            return -1;
          }
          break;
        case OK:
          if (w == 0)
            this->state_ = T0;
          else
            this->manchester(1);
          break;
        case T0:
          if (w == 0)
            this->manchester(0);
          else
            return -1;
          break;
        default:
          break;
      }
    } else {
      return -1;
    }
    return this->total_bits_ == 160 ? 1 : 0;
  }
};

class OregonDecoderV3 : public DecodeOOK {
 public:
  void got_bit(uint8_t value) override {
    this->data_[this->pos_] = (this->data_[this->pos_] >> 1) | (value ? 0x80 : 0);
    this->total_bits_++;
    this->pos_ = this->total_bits_ >> 3;
    if (this->pos_ >= sizeof(this->data_)) {
      this->reset();
      return;
    }
    this->state_ = OK;
  }

 protected:
  int8_t decode(uint16_t width) override {
    if (200 <= width && width < 1200) {
      uint8_t w = width >= 700;
      switch (this->state_) {
        case UNKNOWN:
          if (w == 0) {
            ++this->flip_;
          } else if (32 <= this->flip_) {
            this->flip_ = 1;
            this->manchester(1);
          } else {
            return -1;
          }
          break;
        case OK:
          if (w == 0)
            this->state_ = T0;
          else
            this->manchester(1);
          break;
        case T0:
          if (w == 0)
            this->manchester(0);
          else
            return -1;
          break;
        default:
          break;
      }
    } else {
      return -1;
    }
    return this->total_bits_ == 80 ? 1 : 0;
  }
};

struct Reading {
  uint8_t channel;
  uint16_t type;
  bool has_temperature;
  bool has_humidity;
  float temperature;
  float humidity;
};

inline bool bcd_nibble(uint8_t nibble) { return nibble <= 9; }

// Temperature and humidity nibbles as parsed by oregonStation.ino.
inline Reading parse_v2(const uint8_t *data) {
  Reading reading{};
  reading.type = (uint16_t(data[0]) << 8) | data[1];
  reading.channel = data[2] >> 4;

  uint8_t tens = data[5] >> 4;
  uint8_t ones = data[5] & 0x0F;
  uint8_t tenths = data[4] >> 4;
  if (bcd_nibble(tens) && bcd_nibble(ones) && bcd_nibble(tenths)) {
    reading.has_temperature = true;
    reading.temperature = tens * 10.0f + ones + tenths * 0.1f;
    if ((data[6] & 0x0F) != 0)
      reading.temperature = -reading.temperature;
  }

  uint8_t hum_tens = data[7] & 0x0F;
  uint8_t hum_ones = data[6] >> 4;
  if (bcd_nibble(hum_tens) && bcd_nibble(hum_ones)) {
    reading.humidity = hum_tens * 10.0f + hum_ones;
    reading.has_humidity = reading.humidity <= 100.0f;
  }
  return reading;
}

}  // namespace oregon_decoder
