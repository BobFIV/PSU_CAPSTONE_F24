# nRF9161 DK

This repository contains the firmware source code for an advanced bike-sharing IoT application running on a Nordic Semiconductor nRF9161 DK.

## Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Technologies Used](#technologies-used)
- [File Structure](#file-structure)
- [Setup](#setup)
- [Configuration](#configuration)

## Overview

This repository contains the source code for an IoT application running on a Nordic Semiconductor nRF9160 DK. The application integrates various technologies such as LTE, GNSS, DECT-NR, I2C, and CoAP to monitor and transmit data related to bike status, battery levels, and mesh connectivity. It uses Zephyr RTOS and Nordic SDK for real-time operations and device management. The application handles button events, controls LEDs, and communicates with a remote server using the oneM2M protocol over CoAP.

## Features

- **Button Handling and LED Control:** Lock status responding and indicator
- **GNSS Data Acquisition**: Receiving accurate PVT data from GPS transmissions
- **Environmental Sensing**: Acceleration and temperature data acquisition
- **Battery monitoring**: Reading and reporting device battery level
- **CoAP Communication**: Sending and receiving data to/from an oneM2M ACME server
- **LTE Connectivity**: Connecting to LTE network for direct to server communication
- **DECT-NR**: Communication between devices in an adaptive and dynamic mesh network
- **Periodic Data Transmission**: Sending bike data to server for real-time monitoring over web
- **Multithreading**: Concurrent threads between separate application modules for high efficiency

## Technologies Used

- **Zephyr RTOS**: Real-time operating system for embedded devices.
- **nRF Connect SDK**: Libraries and tools for Nordic Semiconductor devices.
- **LTE**: Cellular connectivity using LTE.
- **GNSS**: Global Navigation Satellite System for location tracking.
- **DECT-NR**: Digital Enhanced Cordless Telecommunications for mesh communication.
- **CoAP**: Constrained Application Protocol for communication with servers.
- **I2C**: Inter-Integrated Circuit for communication with sensors.
- **Visual Studio Code**: Code editor for development and debugging.

## File Structure

Source code is split into separate modular files based on the system(s) it interacts with primarily. A header file exists for each corresponding source file, which handles imports, macros, data type declarations, and function prototypes.

There are 2 root directories containing the separate firmware used by each device in each device pair. One nRF9161-DK board utilizes **LTE** to connect with the cellular network, the other utilizes **DECT-NR** to connect with other devices.

```
nrf9161dk/               # LTE enabled device firmware
│
├── boards/              # Board specific device tree overlays
│   └── nrf9161dk_nrf9161_ns.overlay
│
├── src/
│   ├── battery.c/       # Battery level reporting using ADC
│   ├── battery.h/       # Battery measurement parameters
│   ├── coap_onem2m.c/   # oneM2M based CoAP communication
│   ├── coap_onem2m.h/
│   ├── gnss.c/          # GNSS initialization and event handling
│   ├── gnss.h/
│   ├── i2c.h/           # Sensor reading over I2C
│   ├── i2c.h/
│   ├── lte.h/           # LTE modem configuration and event handling
│   ├── lte.h/
│   ├── uart.h/          # Intra-device communication over UART
│   ├── uart.h/
│   ├── main.c/          # Main application logic and event handling
│   └── main.h/
│
├── CMakeLists.txt
├── Kconfig
├── prj.conf
├── sample.yaml
└── README.md

nrf9161dk-dectnr/        # DECT-NR enabled device firmware
│
├── boards/
│   └── nrf9161dk_nrf9161_ns.overlay
│
├── src/
│   ├── dect-nr.c/
│   ├── dect-nr.h/
│   ├── mesh-root.c/
│   ├── mesh-root.h/
│   ├── mesh.c/
│   ├── mesh.h/
│   ├── uart.c/
│   ├── uart.h/
│   ├── main.c/
│   └── main.h/
│
├── CMakeLists.txt
├── Kconfig
├── prj.conf
├── sample.yaml
└── README.md
```

## Setup

### Prerequisites

- **nRF Command Line Tools**
- **Visual Studio Code**
- **nRF Connect Extension Pack** for VS Code
- **J-Link**

### Installation


## Configuration



