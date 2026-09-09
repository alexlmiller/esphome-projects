#include "../components/eco_flow_erv/protocol.h"
#include <cassert>
#include <iostream>

using namespace esphome::eco_flow_erv;

int main() {
  const Frame off{0x05, 0, 0x05};
  const Frame supply[]{{0x05, 0x19, 0x1E}, {0x05, 0x1A, 0x1F}, {0x05, 0x1B, 0x20}};
  const Frame exhaust[]{{0x05, 0x09, 0x0E}, {0x05, 0x0A, 0x0F}, {0x05, 0x0B, 0x10}};
  // Only speed 1 below is captured; speed 2/3 are explicitly inferred fixtures.
  const Frame phase_b[]{{0x05, 0x0D, 0x12}, {0x05, 0x0E, 0x13}, {0x05, 0x0F, 0x14}};
  for (uint8_t speed = 1; speed <= 3; ++speed) {
    assert(encode(true, speed, Mode::SUPPLY) == supply[speed - 1]);
    assert(encode(true, speed, Mode::EXHAUST) == exhaust[speed - 1]);
    assert(encode(true, speed, Mode::RECOVERY) == exhaust[speed - 1]);
    assert(encode(true, speed, Mode::RECOVERY, true) == phase_b[speed - 1]);
    for (auto mode : {Mode::SUPPLY, Mode::EXHAUST, Mode::RECOVERY})
      for (bool phase : {false, true}) assert(encode(false, speed, mode, phase) == off);
  }
  assert(encode(true, 0, Mode::SUPPLY) == off);
  assert(encode(true, 4, Mode::EXHAUST) == off);
  assert(encode(true, 1, static_cast<Mode>(255)) == off);

  Parser parser;
  Frame received{};
  // Noise, partial packets, bad checksum, then OFF with two 05 bytes.
  int count = 0;
  for (uint8_t byte : {0xFF, 0x05, 0x19, 0x00, 0x05, 0x00, 0x05})
    if (parser.push(byte, received)) { ++count; assert(received == off); }
  assert(count == 1);
  assert(parser.bad_checksum_candidates() == 1);
  for (const auto &frame : supply) {
    assert(!parser.push(frame[0], received));
    assert(!parser.push(frame[1], received));
    assert(parser.push(frame[2], received));
    assert(received == frame);
  }
  parser.push(0x05, received);
  parser.reset();
  assert(!parser.push(0x19, received));
  assert(!parser.push(0x1E, received));
  assert(parser.bad_checksum_candidates() == 1);  // Resync retains diagnostics.
  Parser repeated_off;
  for (unsigned i = 0; i < 100; ++i) {
    assert(!repeated_off.push(0x05, received));
    assert(!repeated_off.push(0x00, received));
    assert(repeated_off.push(0x05, received));
  }
  assert(repeated_off.bad_checksum_candidates() == 0);
  assert(known_state(0x0E));
  assert(!known_state(0x08));  // Do not invent passive mode.
  assert(!known_state(0x18));

  Controller c;
  c.set_power(true);
  assert(!c.power());
  assert(!c.next_frame(0, received));  // No boot transmission.
  c.set_mode(Mode::EXHAUST);
  c.set_speed(3);
  c.set_enabled(true);
  assert(!c.power());
  assert(c.next_frame(0, received) && received == off);
  c.set_power(true);
  assert(!c.next_frame(96, received));
  assert(c.next_frame(97, received) && received == exhaust[2]);
  c.set_enabled(true);  // Idempotent re-arm must not interrupt operation.
  assert(c.power());
  c.set_power(false);
  c.set_mode(Mode::SUPPLY);
  c.set_speed(2);
  assert(c.next_frame(194, received) && received == off);
  c.set_power(true);
  assert(c.next_frame(291, received) && received == supply[1]);
  c.set_speed(0);
  assert(c.speed() == 2);
  assert(c.next_frame(10000, received));
  assert(!c.next_frame(10000, received));  // No stale-frame catch-up burst.
  c.set_enabled(false);
  assert(!c.power() && !c.next_frame(20000, received));
  c.set_enabled(true);
  assert(c.next_frame(20001, received) && received == off);

  Controller recovery;
  recovery.set_enabled(true);
  recovery.set_mode(Mode::RECOVERY);
  recovery.set_power(true);
  for (uint32_t packet = 0; packet < 2100; ++packet) {
    assert(recovery.next_frame(packet * 97, received));
    assert(received == ((packet / 700) % 2 ? phase_b[0] : exhaust[0]));
  }
  // Our policy: speed-only changes preserve phase; OFF resets to phase A.
  assert(recovery.phase_b());
  recovery.set_speed(3);
  assert(recovery.next_frame(2100 * 97, received) && received == phase_b[2]);
  recovery.set_power(false);
  assert(recovery.next_frame(2101 * 97, received) && received == off);
  recovery.set_power(true);
  assert(recovery.next_frame(2102 * 97, received) && received == exhaust[2]);
  recovery.set_mode(Mode::SUPPLY);
  assert(!recovery.phase_b());

  Controller wrap;
  wrap.set_enabled(true);
  assert(wrap.next_frame(UINT32_MAX - 40, received));
  assert(!wrap.next_frame(55, received));
  assert(wrap.next_frame(56, received));

  Controller rapid;
  rapid.set_enabled(true);
  assert(rapid.next_frame(0, received));
  rapid.set_power(true);
  rapid.set_enabled(false);
  rapid.set_enabled(true);
  assert(!rapid.next_frame(1, received));
  assert(rapid.next_frame(97, received) && received == off);
  std::cout << "PASS: captured matrix, inferred fixtures, parser, arming, power independence, recovery, timing and wrap\n";
}
