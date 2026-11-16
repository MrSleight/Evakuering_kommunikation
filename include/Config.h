
#pragma once

// Mesh configuration
#define MESH_PREFIX     "ESP32_MESH"
#define MESH_PASSWORD   "mesh_password_123"
#define MESH_PORT       5555
#define MESH_CHANNEL    6

// Gateway configuration
#define HOST_TIMEOUT          10000
#define GATEWAY_ANNOUNCE_MS    3000
#define GATEWAY_TIMEOUT       10000

// Intervals
#define BROADCAST_INTERVAL         5000
#define STATUS_INTERVAL           15000
#define SERVER_TELEMETRY_INTERVAL 10000
