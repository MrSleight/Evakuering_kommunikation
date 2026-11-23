# ESP32 Mesh Network

A wireless mesh network built using ESP32 development boards, PlatformIO, Arduino and PainlessMesh. The system supports communication between multiple ESP32 nodes over multiple mesh hops and forwards telemetry to a Python server through a USB-connected gateway.

The project is designed to keep the ESP32 mesh operational independently of the Python server connection.

## Features

- Automatic mesh formation using PainlessMesh.
- Communication between ESP32 nodes across multiple mesh hops.
- Gateway discovery through periodic mesh announcements.
- Routing of telemetry from mesh nodes to a USB-connected gateway.
- Forwarding of telemetry to a Python server over USB serial.
- JSON-based communication between ESP32 firmware and Python.
- USB heartbeat for detecting Python server connection and disconnection.
- Sensor message handling with pending-message tracking and delivery acknowledgements.
- Local sensor value updates with revision tracking.
- Dijkstra routing module merged into the project for shortest-path calculations.
- Flash optimization using compiler and linker settings.
- Mesh operation continues when the Python server is unavailable.

## Hardware

- ESP32 development boards (tested with ESP32 Dev Module).
- USB cables for programming and connecting the gateway to a computer.
- At least two ESP32 boards; three are recommended for testing multi-hop communication.

## Software requirements

Install the following:

1. [Visual Studio Code](https://code.visualstudio.com/)
2. [PlatformIO IDE extension](https://platformio.org/install/ide?install=vscode)
3. [Python 3](https://www.python.org/downloads/)

PlatformIO installs the ESP32 platform, compiler toolchain and libraries specified in `platformio.ini`.

The Python server requires PySerial:

```bash
py -m pip install pyserial
```

## Project structure

The project is organized into separate modules for mesh communication, gateway management, messaging, diagnostics and routing.

```text
Painfull_project/
│
├── include/
│   └── Config.h
│
├── src/
│   ├── main.cpp
│   │
│   ├── mesh/
│   │   ├── MeshManager.h
│   │   └── MeshManager.cpp
│   │
│   ├── gateway/
│   │   ├── SerialManager.h
│   │   ├── SerialManager.cpp
│   │   ├── GatewayManager.h
│   │   └── GatewayManager.cpp
│   │
│   ├── messaging/
│   │   ├── MessageManager.h
│   │   └── MessageManager.cpp
│   │
│   ├── dijkstra/
│   │   ├── DijkstraManager.h
│   │   └── DijkstraManager.cpp
│   │
│   └── diagnostics/
│       ├── NetworkDiagnostics.h
│       └── NetworkDiagnostics.cpp
│
├── python_server.py
├── platformio.ini
└── README.md
```

The exact source tree may vary as modules are developed.

## Installation

1. Clone or download this repository.
2. Open the project folder in Visual Studio Code.
3. Open the PlatformIO terminal.
4. Build the firmware:

```bash
pio run
```

5. Connect an ESP32 and upload the firmware:

```bash
pio run -t upload
```

Repeat the upload for each ESP32 node.

All nodes must use compatible firmware and mesh configuration.

## Running the Python server

1. Connect the gateway ESP32 to your computer using USB.
2. Close the PlatformIO Serial Monitor so the serial port is available.
3. Open `python_server.py`.
4. Set the correct COM port for your computer.
5. Start the server:

```bash
py python_server.py
```

The Python server listens for JSON messages forwarded by the gateway.

The ESP32 gateway communicates with the Python application over USB serial at 115200 baud.

The Python application sends periodic heartbeat messages to indicate that the server is connected.

## Configuration

Mesh settings and timing intervals are defined in:

```text
include/Config.h
```

The PlatformIO environment, framework and library dependencies are defined in:

```text
platformio.ini
```

All ESP32 nodes must use compatible mesh settings, including the same mesh name, password and Wi-Fi channel.

## Architecture

### MeshManager

Responsible for initializing and updating the PainlessMesh network.

### GatewayManager

Handles gateway announcements, gateway discovery and telemetry routing.

Nodes identify an available USB-connected gateway through periodic mesh announcements. Messages intended for the Python server are forwarded to the gateway.

The current architecture supports one active USB gateway.

### SerialManager

Handles USB serial communication between the ESP32 gateway and the Python server.

Responsibilities include:
- Sending JSON messages to the Python application.
- Receiving serial input.
- Processing server heartbeat messages.
- Tracking the host connection state.

### MessageManager

Manages sensor values and outgoing sensor messages.

The current implementation includes:
- Sensor A and Sensor B value tracking.
- Pending-message handling.
- Revision tracking to identify updated sensor values.
- Delivery acknowledgement callbacks for reliable sensor transmission.

Sensor updates are queued when necessary and processed by the message manager.

### DijkstraManager

Contains the Dijkstra shortest-path algorithm.

The module is intended to calculate shortest paths through a weighted network graph. It is designed to support future routing decisions based on network topology and blocked connections.

The current implementation is still under development and requires validation before it can be relied upon for operational route selection.

### NetworkDiagnostics

Contains network diagnostic functionality for inspecting mesh topology and gateway status.

### Python server

Receives telemetry forwarded by the USB-connected gateway.

The Python application is separate from the ESP32 mesh and is not required for normal ESP32-to-ESP32 communication.

## Communication architecture

The general telemetry flow is:

```text
ESP32 Node A
     |
     | Mesh communication
     |
ESP32 Node B
     |
     | Mesh communication
     |
USB-connected Gateway
     |
     | USB Serial / JSON
     |
Python Server
```

Messages can travel across multiple mesh hops before reaching the gateway.

The gateway forwards received telemetry to the Python application using JSON messages.

## Message handling

The firmware currently supports sensor message handling for two sensor types.

### SENSOR_A

Represents the first sensor value.

The message manager tracks changes and queues updated values for transmission.

### SENSOR_B

Represents a separate sensor value and uses the same general message-handling mechanism as SENSOR_A.

### Delivery acknowledgements

Sensor messages use PainlessMesh delivery acknowledgements when transmitted to another mesh node.

For messages originating directly at the USB-connected gateway, the local callback confirms handoff to serial output. It does not confirm that the Python application has received or processed the message.

### BLOCKAGE

Blockage information is intended to represent unavailable paths or connections in the mesh network.

The planned implementation will distribute blockage information between nodes and make it available to the routing algorithm.

Reliable blockage propagation, acknowledgement handling and persistent storage remain under development.

## Routing algorithm

The project includes a Dijkstra implementation in the `routing/` directory.

Dijkstra's algorithm is intended to calculate the shortest available route through a weighted graph.

The planned routing process is:

1. Represent the mesh topology as a weighted graph.
2. Identify the source node.
3. Calculate shortest-path distances.
4. Account for blocked connections by excluding unavailable edges.
5. Select the most suitable route toward the destination.

The current implementation compares distances to two exit nodes, referred to as `exit_right` and `exit_left`.

Further development is required before the algorithm can be considered complete. This includes correcting the node-count handling, validating graph and node indices, handling unreachable destinations and returning a usable route rather than only printing the selected direction.

## Flash optimization

The PlatformIO build configuration uses compiler and linker optimizations to reduce firmware size.

Current optimization flags include:

```ini
build_unflags =
    -fno-lto

build_flags =
    -Os
    -flto
    -ffunction-sections
    -fdata-sections
    -Wl,--gc-sections
```

These settings enable size optimization, link-time optimization and removal of unused functions and data sections.

The optimization reduced the measured firmware size from approximately 1,246,735 bytes to 1,190,171 bytes.

## Current limitations

- Only one active USB gateway is supported.
- The Python server currently receives telemetry; application commands from Python into the mesh are not yet implemented as a complete feature.
- Gateway-local delivery acknowledgement confirms serial handoff, not Python application receipt.
- Sensor message delivery acknowledgements do not provide end-to-end confirmation from the Python application.
- Dijkstra routing is merged but still requires implementation fixes and validation.
- Blockage propagation, reliable blockage delivery and persistent blockage storage are still under development.
- The routing algorithm is not yet fully integrated with dynamic blockage information.

## Planned development

### Routing

- Complete and validate the Dijkstra implementation.
- Integrate routing decisions with mesh topology information.
- Incorporate blockage information into route calculations.
- Support reliable route selection when network connections become unavailable.

### Message handling

- Add reliable blockage message propagation.
- Implement acknowledgements and retransmissions for blockage messages.
- Add persistent storage for blockage information.
- Introduce message prioritization.
- Improve end-to-end delivery confirmation.

### Gateway and server

- Implement application commands from Python into the mesh.
- Improve gateway selection and recovery.
- Add server-side message handling and validation.

### Performance

- Investigate more compact message formats.
- Reduce unnecessary mesh traffic.
- Continue optimizing firmware size and memory usage.

## Development notes

The project is divided into modules to keep mesh communication, gateway management, serial communication, messaging and routing independent.

Changes to the architecture or source structure should be reflected in this README.

Documentation should distinguish between implemented, tested and planned functionality. A merged module should not be considered operational until its implementation has been validated.