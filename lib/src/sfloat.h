#pragma once

#include <array>
#include <cstdint>

std::array<uint8_t, 5> encodeIEEE11073(float temp);
float decodeIEEE11073(const uint8_t *data, uint16_t len);
