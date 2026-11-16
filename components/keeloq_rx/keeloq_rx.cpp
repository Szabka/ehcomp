#include "keeloq_rx.h"

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <cinttypes>
#include <cstdio>

namespace esphome {
namespace keeloq_rx {

static const char *const TAG = "keeloq_rx";

static uint32_t pulse_width(int32_t pulse) {
  return pulse < 0 ? static_cast<uint32_t>(-pulse) : static_cast<uint32_t>(pulse);
}

static bool near_us(uint32_t value, uint32_t target) {
  uint32_t delta = target / 3;
  if (delta < 80)
    delta = 80;
  return value + delta >= target && target + delta >= value;
}

static bool bit_at(const uint8_t *bits, uint8_t index) { return (bits[index / 8] >> (index % 8)) & 1; }

// HCS101 is the fixed-code encoder. Its 66-bit word carries a fixed 00, a fixed
// trailing 1, and the same function nibble twice. Hopping parts do not.
static bool is_hcs101(const uint8_t *bits, uint8_t nbits) {
  if (nbits != 66 || bit_at(bits, 10) || bit_at(bits, 11) || !bit_at(bits, 65))
    return false;
  uint8_t first = 0;
  uint8_t second = 0;
  for (uint8_t b = 0; b < 4; b++) {
    if (bit_at(bits, 12 + b))
      first |= 1 << b;
    if (bit_at(bits, 60 + b))
      second |= 1 << b;
  }
  return first == second;
}

// Normal transmissions share a code word per family. HCS200, HCS300 and HCS301
// cannot be told apart from one button press.
static const char *chip_type(const uint8_t *bits, uint8_t nbits) {
  if (is_hcs101(bits, nbits))
    return "HCS101";
  switch (nbits) {
    case 66:
      return "HCS200/HCS300/HCS301";
    case 67:
      return "HCS360/HCS361";
    case 69:
      return "HCS362/HCS365/HCS370";
    default:
      return nullptr;
  }
}

static void log_pulses(const remote_base::RawTimings &raw, size_t begin, size_t end) {
  char line[160];
  size_t pos = 0;
  for (size_t i = begin; i < end; i++) {
    int wrote = snprintf(line + pos, sizeof(line) - pos, "%" PRId32 " ", raw[i]);
    if (wrote < 0)
      return;
    if (static_cast<size_t>(wrote) >= sizeof(line) - pos) {
      line[pos] = '\0';
      ESP_LOGD(TAG, "segment %s", line);
      pos = 0;
      wrote = snprintf(line, sizeof(line), "%" PRId32 " ", raw[i]);
      if (wrote < 0)
        return;
    }
    pos += static_cast<size_t>(wrote);
  }
  if (pos > 0) {
    ESP_LOGD(TAG, "segment %s", line);
  }
}

void KeeloqRx::dump_config() { ESP_LOGCONFIG(TAG, "KeeLoq"); }

bool KeeloqRx::on_receive(remote_base::RemoteReceiveData data) {
  const remote_base::RawTimings &raw = data.get_raw_data();
  const size_t count = raw.size();
  bool found = false;

  for (size_t i = 0; i + 1 < count; i++) {
    if (raw[i] >= 0)
      continue;
    const uint32_t header = pulse_width(raw[i]);
    const uint32_t next = pulse_width(raw[i + 1]);
    // Header is 10 TE. The pulse before it is a 1 TE preamble mark, so 6x
    // still separates it from a bit space. The pulse after it is the first
    // data mark, which is 2 TE when that bit is 0, so the ratio is only ~5.
    if (header < 1200 || header > 4800 || next == 0 || header < next * 4)
      continue;
    if (i > 0) {
      const uint32_t prev = pulse_width(raw[i - 1]);
      if (prev == 0 || header < prev * 6)
        continue;
    }

    const uint32_t te_header = header / 10;
    uint8_t bits[12] = {};
    uint8_t nbits = 0;
    uint32_t te_sum = 0;
    uint8_t te_samples = 0;
    size_t j = i + 1;

    while (j + 1 < count && nbits < 96) {
      if (raw[j] <= 0 || raw[j + 1] >= 0)
        break;
      const uint32_t mark = pulse_width(raw[j]);
      const uint32_t space = pulse_width(raw[j + 1]);
      const bool long_space = space > te_header * 5;
      const bool bit1 = near_us(mark, te_header) && (long_space || near_us(space, te_header * 2));
      const bool bit0 = near_us(mark, te_header * 2) && (long_space || near_us(space, te_header));
      if (!bit1 && !bit0)
        break;
      if (bit1)
        bits[nbits / 8] |= 1 << (nbits % 8);
      nbits++;
      if (!long_space) {
        te_sum += (mark + space) / 3;
        te_samples++;
      }
      j += 2;
      if (long_space)
        break;
    }

    if (nbits < 60)
      continue;

    size_t begin = i;
    uint8_t preamble = 0;
    while (begin > 0 && preamble < 40) {
      if (!near_us(pulse_width(raw[begin - 1]), te_header))
        break;
      begin--;
      preamble++;
    }

    const uint32_t te = te_samples > 0 ? te_sum / te_samples : te_header;
    const char *chip = chip_type(bits, nbits);
    if (chip != nullptr) {
      ESP_LOGD(TAG, "KeeLoq %s: %u bits, TE=%" PRIu32 " us, header=%" PRIu32 " us, preamble=%u pulses", chip,
               nbits, te, header, preamble);
    } else {
      ESP_LOGD(TAG, "KeeLoq unknown: %u bits, TE=%" PRIu32 " us, header=%" PRIu32 " us, preamble=%u pulses", nbits,
               te, header, preamble);
    }

    char bitstr[97];
    for (uint8_t b = 0; b < nbits; b++)
      bitstr[b] = bit_at(bits, b) ? '1' : '0';
    bitstr[nbits] = '\0';
    ESP_LOGD(TAG, "bits %s", bitstr);

    std::string hex = format_hex_pretty(bits, (nbits + 7) / 8, ' ', false);
    ESP_LOGD(TAG, "code %s", hex.c_str());

    if (is_hcs101(bits, nbits)) {
      uint16_t ser3 = 0;
      uint16_t counter = 0;
      uint32_t ser1 = 0;
      uint8_t function = 0;
      for (uint8_t b = 0; b < 10; b++)
        if (bit_at(bits, b))
          ser3 |= 1U << b;
      for (uint8_t b = 0; b < 4; b++)
        if (bit_at(bits, 12 + b))
          function |= 1U << b;
      for (uint8_t b = 0; b < 16; b++)
        if (bit_at(bits, 16 + b))
          counter |= 1U << b;
      for (uint8_t b = 0; b < 28; b++)
        if (bit_at(bits, 32 + b))
          ser1 |= 1UL << b;
      ESP_LOGD(TAG,
               "ser3=0x%03X func=0x%X (S3=%u S0=%u S1=%u S2=%u) counter=0x%04X ser1=0x%07" PRIx32 " vlow=%u",
               ser3, function, function & 1, (function >> 1) & 1, (function >> 2) & 1, (function >> 3) & 1, counter,
               ser1, bit_at(bits, 64) ? 1 : 0);
    } else if (nbits >= 66) {
      uint32_t hop = 0;
      uint32_t serial = 0;
      uint8_t button = 0;
      for (uint8_t b = 0; b < 32; b++)
        if (bit_at(bits, b))
          hop |= 1UL << b;
      for (uint8_t b = 0; b < 28; b++)
        if (bit_at(bits, 32 + b))
          serial |= 1UL << b;
      for (uint8_t b = 0; b < 4; b++)
        if (bit_at(bits, 60 + b))
          button |= 1 << b;
      const unsigned vlow = nbits > 64 && bit_at(bits, 64) ? 1 : 0;
      const unsigned repeat = nbits > 65 && bit_at(bits, 65) ? 1 : 0;
      ESP_LOGD(TAG,
               "hop=0x%08" PRIx32 " serial=0x%07" PRIx32
               " btn=0x%X (S2=%u S1=%u S0=%u S3=%u) vlow=%u repeat=%u",
               hop, serial, button, button & 1, (button >> 1) & 1, (button >> 2) & 1, (button >> 3) & 1, vlow,
               repeat);
      if (nbits > 66) {
        char extra[40];
        size_t pos = 0;
        for (uint8_t b = 66; b < nbits && pos + 2 < sizeof(extra); b++)
          extra[pos++] = bit_at(bits, b) ? '1' : '0';
        extra[pos] = '\0';
        ESP_LOGD(TAG, "tail %s", extra);
      }
    }

    log_pulses(raw, begin, j);
    found = true;
    if (j > 0)
      i = j - 1;
  }

  return found;
}

}  // namespace keeloq_rx
}  // namespace esphome
