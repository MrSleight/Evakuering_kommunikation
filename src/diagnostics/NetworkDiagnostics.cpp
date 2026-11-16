
#include <Arduino.h>
#include "NetworkDiagnostics.h"

void printNetworkStatus(
    painlessMesh& mesh,
    bool gatewayAvailable,
    uint32_t gatewayNodeId
) {
    auto nodes = mesh.getNodeList();

    Serial.println();
    Serial.println("========== MESH STATUS ==========");

    Serial.printf("Own Node ID: %u\n", mesh.getNodeId());

    Serial.printf("Connected nodes: %d\n",
                  (int)nodes.size());

    for (uint32_t nodeId : nodes) {
        Serial.printf("  Node ID: %u\n", nodeId);
    }

    Serial.println("Network topology:");
    Serial.println(mesh.subConnectionJson());

    Serial.printf("Gateway available: %s\n",
                  gatewayAvailable ? "YES" : "NO");

    if (gatewayAvailable) {
        Serial.printf("Gateway Node ID: %u\n", gatewayNodeId);
    }

    Serial.println("=================================");
}
