#pragma once

#include <cstddef>

#include "location_publisher.hpp"
#include "stm32f407/keyboard.hpp"

// Simulates moving the Smart City object: each SW3 press sends the next
// preset location from Settings to the location publisher.
class LocationPresets final : public stm32f407::IButtonListener {
public:
    explicit LocationPresets(network::LocationPublisher& publisher);

    LocationPresets(const LocationPresets&) = delete;
    LocationPresets& operator=(const LocationPresets&) = delete;
    LocationPresets(LocationPresets&&) = delete;
    LocationPresets& operator=(LocationPresets&&) = delete;

    // Sends the current preset, for example once at start-up.
    void publish_current();
    void on_button_event(const stm32f407::ButtonEvent& event) override;

private:
    network::LocationPublisher& publisher_;
    std::size_t index_ = 0U;
};