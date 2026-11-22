#pragma once

#include <array>

#include "location.hpp"
#include "mqtt_client.hpp"

extern "C" {
#include "FreeRTOS.h"
#include "queue.h"
}

namespace network {

// Publishes the Smart City object's identity and location as JSON to
// smartcity/objects/<object id>/location, retrying while the broker is
// unreachable until a newer location replaces it.
class LocationPublisher final {
public:
    explicit LocationPublisher(MqttClient& client);

    LocationPublisher(const LocationPublisher&) = delete;
    LocationPublisher& operator=(const LocationPublisher&) = delete;
    LocationPublisher(LocationPublisher&&) = delete;
    LocationPublisher& operator=(LocationPublisher&&) = delete;

    bool start();

    // Safe to call from any task. A newer location replaces one not yet sent.
    void on_location_changed(const Location& location);

private:
    static void task_entry(void* context);

    void run();

    MqttClient& client_;
    // "smartcity/objects/" + object id + "/location" + NUL.
    std::array<char, 64> topic_{};
    // Not a task notification: MqttClient::publish waits on this task's notification.
    QueueHandle_t changes_ = nullptr;
};

} // namespace network