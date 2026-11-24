#pragma once

#include "ethernet.hpp"
#include "location_publisher.hpp"
#include "nmea_parser.hpp"

extern "C" {
#include "FreeRTOS.h"
#include "FreeRTOS_Sockets.h"
#include "event_groups.h"
}

namespace network {

// Acts like a GPS module connected over the network: listens on TCP port 5001
// for NMEA sentences (for example from gps_sim.py) and hands each valid fix
// to the location publisher, at most once every two seconds.
class NmeaTcpReceiver final : public ILinkStateClient {
public:
    explicit NmeaTcpReceiver(LocationPublisher& publisher);

    NmeaTcpReceiver(const NmeaTcpReceiver&) = delete;
    NmeaTcpReceiver& operator=(const NmeaTcpReceiver&) = delete;
    NmeaTcpReceiver(NmeaTcpReceiver&&) = delete;
    NmeaTcpReceiver& operator=(NmeaTcpReceiver&&) = delete;

    bool start();
    void on_link_state_changed(LinkState state) override;

private:
    static void task_entry(void* context);

    void run();
    bool link_is_up() const;
    Socket_t create_server_socket() const;
    void serve_client(Socket_t client);
    void handle(NmeaParser::Result result, const Location& location);

    LocationPublisher& publisher_;
    NmeaParser parser_;
    EventGroupHandle_t link_state_events_ = nullptr;
    TickType_t last_publish_ = 0U;
    bool has_published_ = false;
    bool started_ = false;
};

} // namespace network