// PC unit test for NmeaParser. Build and run from the repository root:
//   g++ -std=c++17 -Wall -Wextra -Iinclude tests/nmea_parser_test.cpp src/nmea_parser.cpp -o nmea_test && ./nmea_test
#include <cstdio>

#include "nmea_parser.hpp"

namespace {

int failures = 0;

void check(const char* input, NmeaParser::Result expected, long lat = 0, long lon = 0)
{
    NmeaParser parser;
    Location location{};
    NmeaParser::Result last = NmeaParser::Result::NONE;
    for (const char* c = input; *c != '\0'; ++c) {
        const NmeaParser::Result result = parser.feed(*c, location);
        if (result != NmeaParser::Result::NONE) {
            last = result;
        }
    }
    const bool ok = last == expected &&
        (expected != NmeaParser::Result::LOCATION ||
         (location.lat_e6 == lat && location.lon_e6 == lon));
    if (!ok) {
        ++failures;
    }
    std::printf("%s  %s", ok ? "PASS" : "FAIL", input);
}

} // namespace

int main()
{
    using R = NmeaParser::Result;
    // Valid fix at Slottsskogen.
    check("$GPRMC,123519.00,A,5741.2140,N,01156.5560,E,0.0,0.0,231126,,,A*53\r\n",
          R::LOCATION, 57686900, 11942600);
    // Status V: no fix.
    check("$GPRMC,123519.00,V,5741.2140,N,01156.5560,E,0.0,0.0,231126,,,A*44\r\n", R::NO_FIX);
    // Wrong checksum.
    check("$GPRMC,123519.00,A,5741.2140,N,01156.5560,E,0.0,0.0,231126,,,A*00\r\n", R::INVALID);
    // South and west give negative values.
    check("$GPRMC,123519.00,A,4807.038,S,01131.000,W,022.4,084.4,230394,003.1,W*4B\r\n",
          R::LOCATION, -48117300, -11516667);
    // Other sentence types are ignored.
    check("$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n", R::NONE);
    // Latitude 95 degrees is out of range.
    check("$GPRMC,123519.00,A,9500.0000,N,01156.5560,E,0.0,0.0,231126,,,A*5F\r\n", R::INVALID);
    // Empty fields.
    check("$GPRMC,,A,,,,,,,,,,A*4B\r\n", R::INVALID);
    // Garbage before the sentence is skipped.
    check("garbage$GPRMC,123519.00,A,5741.2140,N,01156.5560,E,0.0,0.0,231126,,,A*53\n",
          R::LOCATION, 57686900, 11942600);

    std::printf("%s\n", failures == 0 ? "All tests passed." : "SOME TESTS FAILED.");
    return failures == 0 ? 0 : 1;
}