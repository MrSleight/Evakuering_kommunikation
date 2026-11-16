# ESP32 Mesh Network

A wireless mesh network built using ESP32 boards, PlatformIO, Arduino and PainlessMesh. The project supports communication between multiple ESP32 nodes and forwarding telemetry to a Python server through one USB-connected gateway.

## Features

- Automatic mesh formation between ESP32 nodes.
- Communication between nodes across multiple mesh hops.
- Gateway discovery through mesh announcements.
- Forwarding telemetry from mesh nodes to a Python server.
- USB heartbeat to detect Python server connection and disconnection.
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

PlatformIO automatically installs the ESP32 platform, compiler toolchain and libraries specified in `platformio.ini`.

The Python server PySerial, install it with:

```bash
py -m pip install pyserial
```

## Project structure

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
│   └── diagnostics/
│       ├── NetworkDiagnostics.h
│       └── NetworkDiagnostics.cpp
│
├── python_server.py
├── platformio.ini
└── README.md
```

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

## Python dependencies

The Python server requires PySerial for USB serial communication.

Install it using:

```bash
py -m pip install pyserial
```

## Running the Python server

1. Connect the gateway ESP32 to your computer using USB.
2. Close the PlatformIO Serial Monitor.
3. Open `python_server.py`.
4. Set the correct COM port for your computer.
5. Start the server:

```bash
py python_server.py
```

The Python server should now receive telemetry forwarded by the gateway.

## Configuration

Mesh settings and timing intervals are defined in:

```text
include/Config.h
```

The PlatformIO environment and library dependencies are defined in:

```text
platformio.ini
```

All ESP32 nodes must use compatible mesh settings, including the same mesh name, password and Wi-Fi channel.

## Architecture

- **MeshManager:** Starts and updates the PainlessMesh network.
- **SerialManager:** Handles USB communication, JSON input and Python heartbeat.
- **NetworkDiagnostics:** Displays mesh topology and gateway status.
- **Gateway logic:** Discovers the active USB gateway and forwards telemetry to the Python server.
- **Python server:** Receives telemetry from the gateway.

The Python server is not required for normal ESP32-to-ESP32 mesh communication.

## Current limitations

- Only one USB gateway is supported.
- The Python server currently receives data; it does not send application commands into the mesh.
- Reliable message delivery with acknowledgements, retries and persistent queues is not yet implemented.
- Routing algorithms and blockage handling are planned for future development.

## Planned development

The following features are planned but are not yet implemented.

### Message handling

The system is planned to support three message types:

- **BLOCKAGE:** High-priority messages reporting blocked paths or connections. Blockage information should be distributed to relevant nodes and remain stored until explicitly cleared. Reliable delivery, acknowledgements and retransmissions are required.

- **SENSOR_A:** Sensor messages where only the latest value matters. New values replace previous values.

- **SENSOR_B:** Uses the same latest-value principle as SENSOR_A but represents a separate sensor type.

### Routing algorithm

Each ESP32 node is intended to maintain relevant blockage information locally and calculate routes independently using Dijkstra's algorithm.

The algorithm should select the shortest available route while avoiding blocked paths.

### Future improvements

- Reliable message delivery.
- Message acknowledgements and retransmissions.
- Persistent storage of blockage information.
- Message prioritization.
- Binary message formats to reduce network traffic.

## Development notes

Whenever a code change affects the project structure, installation process, architecture or functionality, update this README accordingly.

Keep the documentation synchronized with the actual implementation. Clearly distinguish implemented features from planned features.