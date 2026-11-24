#include "nmea_tcp_receiver.hpp"

#include <array>
#include <cstdint>

#include <printf/printf.h>

extern "C" {
#include "task.h"
}

namespace {

constexpr std::uint16_t nmea_port = 5001U;
constexpr configSTACK_DEPTH_TYPE receiver_stack_depth = 512U;
constexpr UBaseType_t receiver_task_priority = tskIDLE_PRIORITY + 1U;
// Longer than the simulator's one-second interval, so a quiet second is not a disconnect.
constexpr TickType_t socket_timeout = pdMS_TO_TICKS(5000U);
// Limits traffic to the public broker; a GPS module sends a fix every second.
constexpr TickType_t publish_period = pdMS_TO_TICKS(2000U);
// NMEA needs only a small receive buffer; this keeps the FreeRTOS heap free for MQTT.
constexpr std::uint32_t receive_buffer_size = 1024U;
constexpr EventBits_t link_up_bit = 1U << 0U;

} // namespace

namespace network {

NmeaTcpReceiver::NmeaTcpReceiver(LocationPublisher& publisher)
    : publisher_(publisher)
{
}

bool NmeaTcpReceiver::start()
{
    configASSERT(!started_);

    link_state_events_ = xEventGroupCreate();
    if (link_state_events_ == nullptr) {
        return false;
    }

    if (xTaskCreate(task_entry,
                    "nmea",
                    receiver_stack_depth,
                    this,
                    receiver_task_priority,
                    nullptr) != pdPASS) {
        vEventGroupDelete(link_state_events_);
        link_state_events_ = nullptr;
        return false;
    }

    started_ = true;
    return true;
}

void NmeaTcpReceiver::on_link_state_changed(LinkState state)
{
    configASSERT(started_);

    if (state == LinkState::UP) {
        xEventGroupSetBits(link_state_events_, link_up_bit);
    } else {
        xEventGroupClearBits(link_state_events_, link_up_bit);
    }
}

void NmeaTcpReceiver::task_entry(void* context)
{
    static_cast<NmeaTcpReceiver*>(context)->run();
}

void NmeaTcpReceiver::run()
{
    for (;;) {
        xEventGroupWaitBits(
            link_state_events_, link_up_bit, pdFALSE, pdTRUE, portMAX_DELAY);

        const Socket_t server = create_server_socket();
        if (server == FREERTOS_INVALID_SOCKET) {
            vTaskDelay(pdMS_TO_TICKS(1000U));
            continue;
        }

        while (link_is_up()) {
            freertos_sockaddr client_address{};
            socklen_t client_address_length = sizeof(client_address);
            const Socket_t client = FreeRTOS_accept(
                server, &client_address, &client_address_length);
            if (client == FREERTOS_INVALID_SOCKET) {
                continue;
            }

            printf("GPS simulator connected\n");
            serve_client(client);
            FreeRTOS_closesocket(client);
            printf("GPS simulator disconnected\n");
        }

        FreeRTOS_closesocket(server);
    }
}

bool NmeaTcpReceiver::link_is_up() const
{
    return (xEventGroupGetBits(link_state_events_) & link_up_bit) != 0U;
}

Socket_t NmeaTcpReceiver::create_server_socket() const
{
    const Socket_t server = FreeRTOS_socket(
        FREERTOS_AF_INET, FREERTOS_SOCK_STREAM, FREERTOS_IPPROTO_TCP);
    if (server == FREERTOS_INVALID_SOCKET) {
        return FREERTOS_INVALID_SOCKET;
    }

    (void)FreeRTOS_setsockopt(
        server, 0, FREERTOS_SO_RCVTIMEO, &socket_timeout, sizeof(socket_timeout));
    // Accepted sockets inherit this smaller buffer from the listening socket.
    (void)FreeRTOS_setsockopt(
        server, 0, FREERTOS_SO_RCVBUF, &receive_buffer_size, sizeof(receive_buffer_size));

    freertos_sockaddr address{};
    address.sin_port = FreeRTOS_htons(nmea_port);
    if (FreeRTOS_bind(server, &address, sizeof(address)) != 0 ||
        FreeRTOS_listen(server, 1) != 0) {
        FreeRTOS_closesocket(server);
        return FREERTOS_INVALID_SOCKET;
    }

    return server;
}

void NmeaTcpReceiver::serve_client(Socket_t client)
{
    std::array<char, 128> buffer{};
    (void)FreeRTOS_setsockopt(
        client, 0, FREERTOS_SO_RCVTIMEO, &socket_timeout, sizeof(socket_timeout));

    parser_ = NmeaParser{};  // Each connection starts with an empty line buffer.

    while (link_is_up()) {
        const BaseType_t received = FreeRTOS_recv(client, buffer.data(), buffer.size(), 0);
        if (received > 0) {
            for (BaseType_t i = 0; i < received; ++i) {
                Location location{};
                handle(parser_.feed(buffer[static_cast<std::size_t>(i)], location), location);
            }
        } else if (received != -pdFREERTOS_ERRNO_EWOULDBLOCK) {
            break;  // The simulator closed the connection, or it timed out.
        }
    }
}

void NmeaTcpReceiver::handle(NmeaParser::Result result, const Location& location)
{
    switch (result) {
    case NmeaParser::Result::LOCATION: {
        const TickType_t now = xTaskGetTickCount();
        if (!has_published_ || now - last_publish_ >= publish_period) {
            publisher_.on_location_changed(location);
            last_publish_ = now;
            has_published_ = true;
        }
        break;
    }
    case NmeaParser::Result::NO_FIX:
        printf("GPS: no fix, location not published\n");
        break;
    case NmeaParser::Result::INVALID:
        printf("GPS: invalid NMEA sentence dropped\n");
        break;
    case NmeaParser::Result::NONE:
        break;
    }
}

} // namespace network