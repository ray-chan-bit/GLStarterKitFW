#pragma once

#include <array>
#include <cstddef>

#include "location.hpp"

// Turns a stream of NMEA 0183 characters into locations. Only RMC sentences
// ($GPRMC, $GNRMC, ...) are used; every line must carry a valid checksum.
// Has no FreeRTOS or network code, so it can be tested on a PC.
class NmeaParser {
public:
    enum class Result {
        NONE,      // No complete line yet, or a sentence type we ignore.
        LOCATION,  // A valid RMC line with a fix: the location was written.
        NO_FIX,    // A valid RMC line whose status is V (receiver has no fix).
        INVALID,   // A broken line: bad checksum, missing fields, bad values.
    };

    // Feeds one received character. When it completes a line, parses it.
    Result feed(char c, Location& location);

private:
    Result parse_line(Location& location) const;

    // NMEA limits a sentence to 82 characters including "$" and CR LF.
    std::array<char, 82> line_{};
    std::size_t length_ = 0U;
    bool overflow_ = false;
};