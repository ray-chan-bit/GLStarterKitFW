#include "location_publisher.hpp"

#include <cstddef>
#include <cstdint>

#include <printf/printf.h>

#include "settings.hpp"

extern "C" {
#include "task.h"
}

namespace {

constexpr configSTACK_DEPTH_TYPE publisher_stack_depth = 512U;
constexpr UBaseType_t publisher_task_priority = tskIDLE_PRIORITY + 1U;
constexpr TickType_t retry_period = pdMS_TO_TICKS(1000U);

// Formats microdegrees as decimal degrees: 57686900 -> "57.686900", -1234567 -> "-1.234567".
void format_degrees(char* out, std::size_t size, std::int32_t micro)
{
    const char* sign = micro < 0 ? "-" : "";
    const std::uint32_t magnitude = micro < 0
        ? static_cast<std::uint32_t>(-static_cast<std::int64_t>(micro))
        : static_cast<std::uint32_t>(micro);
    (void)snprintf(out, size, "%s%lu.%06lu", sign,
                   static_cast<unsigned long>(magnitude / 1000000U),
                   static_cast<unsigned long>(magnitude % 1000000U));
}

// Builds {"id":...,"type":...,"name":...,"latitude":...,"longitude":...}.
std::array<char, 192> make_payload(const Location& location)
{
    std::array<char, 24> latitude{};
    std::array<char, 24> longitude{};
    format_degrees(latitude.data(), latitude.size(), location.lat_e6);
    format_degrees(longitude.data(), longitude.size(), location.lon_e6);

    std::array<char, 192> payload{};
    (void)snprintf(payload.data(), payload.size(),
                   "{\"id\":\"%s\",\"type\":\"%s\",\"name\":\"%s\","
                   "\"latitude\":%s,\"longitude\":%s}",
                   Settings::object_id, Settings::object_type, Settings::object_name,
                   latitude.data(), longitude.data());
    return payload;
}

} // namespace

namespace network {

LocationPublisher::LocationPublisher(MqttClient& client)
    : client_(client)
{
    (void)snprintf(topic_.data(), topic_.size(),
                   "smartcity/objects/%s/location", Settings::object_id);
}

bool LocationPublisher::start()
{
    changes_ = xQueueCreate(1U, sizeof(Location));
    if (changes_ == nullptr) {
        return false;
    }

    return xTaskCreate(task_entry,
                       "location",
                       publisher_stack_depth,
                       this,
                       publisher_task_priority,
                       nullptr) == pdPASS;
}

void LocationPublisher::on_location_changed(const Location& location)
{
    (void)xQueueOverwrite(changes_, &location);
}

void LocationPublisher::task_entry(void* context)
{
    static_cast<LocationPublisher*>(context)->run();
}

void LocationPublisher::run()
{
    Location location{};
    bool pending = false;
    for (;;) {
        // Times out only while a location is still pending, to retry it.
        (void)xQueueReceive(changes_, &location, pending ? retry_period : portMAX_DELAY);
        pending = !client_.publish(topic_.data(), make_payload(location).data());
        if (!pending) {
            printf("Location published: %s\n", make_payload(location).data());
        }
    }
}

} // namespace network