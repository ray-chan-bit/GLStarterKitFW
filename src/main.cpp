extern "C" {
#include "FreeRTOS.h"
}

#include <printf/printf.h>

#include "stm32f407/board.hpp"
#include "stm32f407/console.hpp"
#include "stm32f407/device_id.hpp"
#include "stm32f407/dht11.hpp"
#include "stm32f407/keyboard.hpp"
#include "stm32f407/wh1602.hpp"

#include "ethernet.hpp"
#include "humidity_publisher.hpp"
#include "leds_controller.hpp"

#include "location_presets.hpp"
#include "location_publisher.hpp"

#include "mqtt_client.hpp"
#include "mqtt_client_id.hpp"
#include "settings.hpp"
#include "tcp_echo_server.hpp"
#include "temperature_publisher.hpp"
#include "ui.hpp"

namespace {

network::Ethernet ethernet{Settings::ethernet_config};
network::TcpEchoServer echo_server;
stm32f407::Keyboard keyboard;

const stm32f407::DeviceId device_id = stm32f407::read_device_id();
const MqttClientId mqtt_client_id = make_mqtt_client_id(device_id);
network::MqttClient mqtt_client{Settings::mqtt_broker, mqtt_client_id.data()};
network::TemperaturePublisher temperature_publisher{mqtt_client, device_id};
network::HumidityPublisher humidity_publisher{mqtt_client, device_id};
LedsController leds_controller{mqtt_client, device_id};

network::LocationPublisher location_publisher{mqtt_client};
LocationPresets location_presets{location_publisher};


stm32f407::Dht11 climate_sensor;

stm32f407::Wh1602 display;
ui::Ui user_interface{display, Settings::ethernet_config.ip};

} // namespace

int main()
{
    if (!stm32f407::initialize_system_clock()) {
        stm32f407::halt();
    }

    stm32f407::initialize_board();

    if (!stm32f407::start_console()) {
        stm32f407::halt();
    }
    
    printf("hello %d\n", 1);

    // The UI's task must exist before any of its sources can report to it.
    if (!user_interface.start() 
        || !keyboard.add_listener(user_interface)) {
        stm32f407::halt();
    }

     if (!leds_controller.start() 
        || !keyboard.add_listener(leds_controller)){
        stm32f407::halt();
    }

    if (!keyboard.add_listener(location_presets)) {
        stm32f407::halt();
    }
    keyboard.start();

    if (!ethernet.start() 
        || !echo_server.start() 
        || !ethernet.add_link_state_client(echo_server)
        || !ethernet.add_link_state_client(user_interface)) {
        stm32f407::halt();
    }

    if (!mqtt_client.add_broker_connection_listener(user_interface)
        || !mqtt_client.start() 
        || !ethernet.add_link_state_client(mqtt_client)
        || !temperature_publisher.start()
        || !humidity_publisher.start()
        || !location_publisher.start()) {
        stm32f407::halt();
    }

    // The bin reports its first preset as soon as the broker is reachable.
    location_presets.publish_current();

    if (!climate_sensor.add_humidity_listener( humidity_publisher,Settings::humidity_reporting_threshold) 
        || !climate_sensor.add_temperature_listener(temperature_publisher, Settings::temperature_reporting_threshold)
        || !climate_sensor.add_humidity_listener(user_interface, Settings::ui_humidity_threshold) 
        || !climate_sensor.add_temperature_listener(user_interface, Settings::ui_temperature_threshold)) {
        stm32f407::halt();
    }

    climate_sensor.start();

     printf("Starting FreeRTOS scheduller\n");

    vTaskStartScheduler();
    stm32f407::halt();
}
