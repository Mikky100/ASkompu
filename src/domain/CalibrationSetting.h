#pragma once

#include <cstdint>

namespace domain {
namespace calibration {

bool isValid(uint32_t millimetersPerPulse);
uint32_t validatedOrDefault(uint32_t millimetersPerPulse);
uint32_t increment(uint32_t millimetersPerPulse);
uint32_t decrement(uint32_t millimetersPerPulse);

}  // namespace calibration
}  // namespace domain
