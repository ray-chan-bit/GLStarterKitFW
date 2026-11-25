# Embedded workshop

Minimal CMake/FreeRTOS firmware for the STM32F407 Discovery board
(STM32F407VGT6).

> **Smart City extension:** a smart waste bin publishes its GPS location over MQTT
> and is shown on OpenStreetMap. See [smartcity/README.md](smartcity/README.md).

## Prerequisites

- CMake 3.24 or newer
- A CMake build backend such as Ninja
- Arm GNU Toolchain (`arm-none-eabi-gcc`, `arm-none-eabi-g++`,
  `arm-none-eabi-ar`, `arm-none-eabi-ranlib`, `arm-none-eabi-objcopy`, and
  `arm-none-eabi-size` on PATH)
- Git and network access during the first configure, to fetch the pinned
  printf, FreeRTOS kernel, FreeRTOS-Plus-TCP, coreMQTT, and coreMQTT-Agent
  releases

## Build

Clone the repository with its STM32 dependencies:

```sh
git clone --recurse-submodules <repository-url>
```

For an existing clone, initialize them with:

```sh
git submodule update --init --recursive
```

```sh
cmake --preset debug
cmake --build --preset debug
```

The build creates these files in `build/debug/`:

- `embedded_workshop.elf`
- `embedded_workshop.hex`
- `embedded_workshop.bin`
- `embedded_workshop.map`

## Ethernet

The firmware uses these fixed IPv4 settings:

- Address: `192.168.137.42`
- Netmask: `255.255.255.0`
- Gateway and DNS: `192.168.137.1`
- TCP echo service: port `5000`

These match Windows Internet Connection Sharing: connect the board to a
Windows PC's Ethernet port and share the PC's internet connection to that
adapter, which then takes `192.168.137.1` and serves routing and DNS.

The locally administered MAC address is derived from the MCU's 96-bit unique
identifier. From a host on the `192.168.137.0/24` network, test ICMP and the
echo service with:

```sh
ping 192.168.137.42
printf 'hello\n' | nc 192.168.137.42 5000
```

The second command should print `hello` back to the terminal.

## MQTT

Once the link is up, the firmware connects to `test.mosquitto.org` on port
`1883` (plain TCP, MQTT 3.1.1, clean session) and publishes with QoS 0 and
without retain to these topics:

```text
SBC/<Device ID>/temperature
SBC/<Device ID>/humidity
SBC/<Device ID>/leds/green
```

`temperature` carries degrees Celsius with one decimal (for example `36.6`)
and `humidity` carries whole percent (for example `45`), both read from the
DHT11 every two seconds. A value is published for the first valid reading and
then whenever it moves at least 1.0 °C or 1 % from the last published one.
`leds/green` carries `1` or `0` and is published each time SW2 toggles the
green LED.

The Device ID is the MCU's 96-bit unique identifier as 24 uppercase hex
characters, highest-address word first. The MQTT client identifier is `sbc-`
followed by the last 16 of those characters.

The board's network must route to the internet on TCP port 1883. Watch the
messages from a host with:

```sh
mosquitto_sub -h test.mosquitto.org -t 'SBC/+/#' -v
```

While the link or broker is unavailable, each publisher retries its latest
value every second until it is sent; a newer value replaces an unsent one. The
client reconnects with a backoff of 1 s doubling to 30 s; a link that comes
back up triggers an immediate reconnect. The test broker is public, so anyone
can read or publish to these topics.

## Project layout

- `src/` and `include/` contain application code.
- `config/` contains the FreeRTOS application configuration.
- `platform/stm32f407/` contains board support and the linker script.
- `cmake/toolchains/` contains compiler-specific configuration.
- `third_party/` contains upstream HAL and CMSIS Git submodules, including the
  STM32F4 startup code.
- `THIRD_PARTY_NOTICES.md` records third-party licensing boundaries.

## Dependencies

### Git submodules under `third_party/`

- [STM32F4 HAL driver](https://github.com/STMicroelectronics/stm32f4xx-hal-driver.git),
  commit `1f6451c3e07728b4c830744de380e56bf5bc0026`
- [STM32F4 CMSIS Device](https://github.com/STMicroelectronics/cmsis-device-f4.git),
  commit `a833f4af71410f25b01468f976560d7ff63a2fc9`
- [CMSIS Core](https://github.com/STMicroelectronics/cmsis-core.git), commit
  `afc5ca6af0a49232fde7eb4548dd0962d119ce14`
- [LSM9DS1 driver](https://github.com/STMicroelectronics/lsm9ds1-pid.git),
  commit `fc35df5921119702e13bc28f9507a9902af696fc`

### Fetched by CMake at configure time

These are declared in `cmake/dependencies.cmake`:

- [printf](https://github.com/eyalroz/printf.git) v6.4.0, commit
  `92c735948e33633b141209240b3e414b86b869bf`
- [FreeRTOS kernel](https://github.com/FreeRTOS/FreeRTOS-Kernel.git) V11.3.1
  (heap_4)
- [FreeRTOS-Plus-TCP](https://github.com/FreeRTOS/FreeRTOS-Plus-TCP.git)
  V4.4.1, commit `c12361095aca68aeed858f45d14395fbffa92c0d`
- [coreMQTT](https://github.com/FreeRTOS/coreMQTT.git) v5.0.2, commit
  `04845c6a8e5f9cf2d232f1c6e80baeb81302e690`
- [coreMQTT-Agent](https://github.com/FreeRTOS/coreMQTT-Agent.git) v2.0.0,
  commit `2338d00db41196e4a1fce4bc3ab32f907e77f871`

### Checked in

- `third_party/svd/STM32F407.svd`, the register description used by the VS Code
  debug configurations in `.vscode/launch.json`.

### Debugging tools

Debug sessions use OpenOCD with the board's ST-LINK, driven from VS Code
through the launch configurations in `.vscode/launch.json`. OpenOCD is not
needed to build.

`THIRD_PARTY_NOTICES.md` records the licensing of these components.
