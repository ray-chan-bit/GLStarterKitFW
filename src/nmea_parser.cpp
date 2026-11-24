#include "nmea_parser.hpp"

#include <cstdint>

namespace {

constexpr std::size_t max_fields = 20U;

struct Field {
    const char* text;
    std::size_t length;
};

bool is_digit(char c)
{
    return c >= '0' && c <= '9';
}

int hex_value(char c)
{
    if (is_digit(c)) {
        return c - '0';
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    return -1;
}

// Converts "ddmm.mmmm" (latitude) or "dddmm.mmmm" (longitude) plus its
// hemisphere letter into microdegrees, using integers only.
bool parse_coordinate(const Field& value, const Field& hemisphere,
                      std::int32_t max_degrees, char positive, char negative,
                      std::int32_t& micro_degrees)
{
    // Whole part: degrees followed by two digits of minutes.
    std::size_t i = 0U;
    std::int32_t whole = 0;
    while (i < value.length && is_digit(value.text[i])) {
        whole = whole * 10 + (value.text[i] - '0');
        ++i;
        if (whole > 18059) {  // More than 180 degrees 59 minutes.
            return false;
        }
    }
    if (i < 3U) {  // Needs at least one degree digit and two minute digits.
        return false;
    }

    // Fraction of a minute, kept to 5 digits: minutes are then in 1e-5 units.
    std::int32_t fraction = 0;
    std::size_t fraction_digits = 0U;
    if (i < value.length) {
        if (value.text[i] != '.') {
            return false;
        }
        ++i;
        for (; i < value.length; ++i) {
            if (!is_digit(value.text[i])) {
                return false;
            }
            if (fraction_digits < 5U) {
                fraction = fraction * 10 + (value.text[i] - '0');
                ++fraction_digits;
            }
        }
    }
    for (; fraction_digits < 5U; ++fraction_digits) {
        fraction *= 10;
    }

    const std::int32_t degrees = whole / 100;
    const std::int32_t minutes = whole % 100;
    if (minutes >= 60 || degrees > max_degrees) {
        return false;
    }

    // 1 minute = 1/60 degree, so 1e-5 minute = 1e6 / 6e6 microdegree = 1/6.
    const std::int64_t minutes_e5 = static_cast<std::int64_t>(minutes) * 100000 + fraction;
    std::int64_t result = static_cast<std::int64_t>(degrees) * 1000000 + (minutes_e5 + 3) / 6;
    if (result > static_cast<std::int64_t>(max_degrees) * 1000000) {
        return false;
    }

    if (hemisphere.length != 1U) {
        return false;
    }
    if (hemisphere.text[0] == negative) {
        result = -result;
    } else if (hemisphere.text[0] != positive) {
        return false;
    }

    micro_degrees = static_cast<std::int32_t>(result);
    return true;
}

} // namespace

NmeaParser::Result NmeaParser::feed(char c, Location& location)
{
    if (c == '$') {  // A new sentence always starts here, even after garbage.
        length_ = 0U;
        overflow_ = false;
    }

    if (c == '\r' || c == '\n') {
        if (length_ == 0U) {
            return Result::NONE;  // The LF after a CR, or an empty line.
        }
        const bool too_long = overflow_;
        const Result result = too_long ? Result::INVALID : parse_line(location);
        length_ = 0U;
        overflow_ = false;
        return result;
    }

    if (length_ < line_.size()) {
        line_[length_++] = c;
    } else {
        overflow_ = true;
    }
    return Result::NONE;
}

NmeaParser::Result NmeaParser::parse_line(Location& location) const
{
    // Shape: $<body>*<two hex digits>
    if (length_ < 4U || line_[0] != '$' || line_[length_ - 3U] != '*') {
        return Result::INVALID;
    }
    const std::size_t body_end = length_ - 3U;

    std::uint8_t checksum = 0U;
    for (std::size_t i = 1U; i < body_end; ++i) {
        checksum = static_cast<std::uint8_t>(checksum ^ static_cast<std::uint8_t>(line_[i]));
    }
    const int high = hex_value(line_[length_ - 2U]);
    const int low = hex_value(line_[length_ - 1U]);
    if (high < 0 || low < 0 || checksum != static_cast<std::uint8_t>(high * 16 + low)) {
        return Result::INVALID;
    }

    // Split the body into comma-separated fields.
    std::array<Field, max_fields> fields{};
    std::size_t count = 0U;
    std::size_t start = 1U;
    for (std::size_t i = 1U; i <= body_end; ++i) {
        if (i == body_end || line_[i] == ',') {
            if (count == max_fields) {
                break;
            }
            fields[count++] = Field{&line_[start], i - start};
            start = i + 1U;
        }
    }

    // Field 0 is the talker and type, e.g. "GPRMC". Ignore all but RMC.
    const Field& type = fields[0];
    if (type.length != 5U || type.text[2] != 'R' || type.text[3] != 'M' || type.text[4] != 'C') {
        return Result::NONE;
    }
    if (count < 7U) {
        return Result::INVALID;
    }

    // Field 2 is the status: A = valid fix, V = no fix.
    if (fields[2].length != 1U) {
        return Result::INVALID;
    }
    if (fields[2].text[0] == 'V') {
        return Result::NO_FIX;
    }
    if (fields[2].text[0] != 'A') {
        return Result::INVALID;
    }

    Location parsed{};
    if (!parse_coordinate(fields[3], fields[4], 90, 'N', 'S', parsed.lat_e6) ||
        !parse_coordinate(fields[5], fields[6], 180, 'E', 'W', parsed.lon_e6)) {
        return Result::INVALID;
    }
    if (parsed.lat_e6 == 0 && parsed.lon_e6 == 0) {
        return Result::INVALID;  // "Null island": a typical default, not a real fix.
    }

    location = parsed;
    return Result::LOCATION;
}