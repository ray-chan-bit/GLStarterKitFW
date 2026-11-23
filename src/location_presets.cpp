#include "location_presets.hpp"

#include "settings.hpp"

LocationPresets::LocationPresets(network::LocationPublisher& publisher)
    : publisher_(publisher)
{
}

void LocationPresets::publish_current()
{
    publisher_.on_location_changed(Settings::preset_locations[index_]);
}

void LocationPresets::on_button_event(const stm32f407::ButtonEvent& event)
{
    if (event.button == stm32f407::Button::SW4 &&
        event.type == stm32f407::ButtonEventType::PRESSED) {
        index_ = (index_ + 1U) % Settings::preset_locations.size();
        publish_current();
    }
}