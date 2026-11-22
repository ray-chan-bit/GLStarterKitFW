#pragma once

#include <cstdint>

// A position in microdegrees: 57.686900° is stored as 57686900.
// Integers avoid float rounding and format exactly as "57.686900".
struct Location {
    std::int32_t lat_e6;
    std::int32_t lon_e6;
};