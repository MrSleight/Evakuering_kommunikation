
#include <Arduino.h>
#include <ArduinoJson.h>
#include <cstring>
#include "mesh/MeshManager.h"
#include "diagnostics/NetworkDiagnostics.h"
#include "gateway/SerialManager.h"
#include "gateway/GatewayManager.h"
#include <limits.h>


#include "Config.h"

#define EXIT_DISTANCE_CHECK 2000

//GLobals
uint32_t lastBroadcast = 0;
uint32_t lastStatus = 0;
uint32_t lastServerTelemetry = 0;

uint32_t lastExitCheck = 0;

bool blocked = false;
const uint8_t this_node = 1;
int exitRight = 0;
int exitLeft = 2;
const int num_nodes = 3;

//dijkstra map:
int dijkstraMap[num_nodes][num_nodes] = { 
                  {0,2,1},
                  {2,0,3},
                  {1,3,0}};


int minDistance(int dist[], bool routeChecked[])
{
    int min = INT_MAX, min_index;

    for (int i = 0; i < num_nodes; i++){
        if (routeChecked[i] == false && dist[i] <= min){
            min = dist[i], min_index = i;
        }
    }

    return min_index;
}


void dijkstra(int graph[num_nodes][num_nodes], int from){
    int dist[num_nodes];
    bool routeChecked[num_nodes];

    for (size_t i = 0; i < num_nodes; i++)
    {
        dist[i] = INT_MAX;
        routeChecked[i] = false;
    }

    dist[from] = 0;
    
    for (int i = 0; i < num_nodes; i++)
    {
        int u = minDistance(dist, routeChecked); //returns the lightest index
        routeChecked[u] = true;

        if (routeChecked[exitRight] && routeChecked[exitLeft])
        {
            break;
        }
        
        for (int j = 0; j < num_nodes; j++)
        {
            if (!routeChecked[j] && graph[u][j] && dist[u] != INT_MAX && (dist[u] + graph[u][j] < dist[j])) {
                dist[j] = dist[u] + graph[u][j];
            }
            
        }
        
    }
    
    if (dist[exitRight] < dist[exitLeft]){
        printf("Right is closer than Left==============================\n");
    }else if (dist[exitRight] > dist[exitLeft]){
        printf("Left is closer than Right==============================\n");
    }
}

// MESSAGE CALLBACK ==========================================

void receivedCallback(uint32_t from, String &msg) {

    JsonDocument doc;

    DeserializationError error = deserializeJson(doc, msg);

    if (error) {
        Serial.printf("[RECEIVED] From %u: %s\n",
                      from, msg.c_str());
        return;
    }

    const char* type = doc["type"] | "";

    if (strcmp(type, "gateway_announce") == 0 ||
        strcmp(type, "server_data") == 0) {

        handleGatewayMessage(from, doc);
        return;
    }

    Serial.println();
    Serial.println("[RECEIVED MESSAGE]");
    Serial.printf("Sender: %u\n", from);
    Serial.printf("Message: %s\n", msg.c_str());
    Serial.println();
}

// CONNECTION CALLBACKS==========================================


void newConnectionCallback(uint32_t nodeId) {
    Serial.printf("[NEW NODE CONNECTED] Node ID: %u\n", nodeId);
    printNetworkStatus(
    mesh,
    isGatewayAvailable(),
    getGatewayNodeId()
    );}

void changedConnectionCallback() {
    Serial.println("[MESH TOPOLOGY CHANGED]");
    printNetworkStatus(
    mesh,
    isGatewayAvailable(),
    getGatewayNodeId()
    );}

void nodeTimeAdjustedCallback(int32_t offset) {
    Serial.printf("[TIME SYNC] Offset: %ld us\n", (long)offset);
}

// SETUP==========================================

void setup() {
    beginSerialManager();
    delay(1000);

    Serial.println();
    Serial.println("=================================");
    Serial.println("       ESP32 MESH NETWORK");
    Serial.println("=================================");

    mesh.setDebugMsgTypes(
        ERROR |
        STARTUP |
        CONNECTION |
        COMMUNICATION
    );

    mesh.onReceive(&receivedCallback);
    mesh.onNewConnection(&newConnectionCallback);
    mesh.onChangedConnections(&changedConnectionCallback);
    mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);

    beginMesh();
    beginGatewayManager();

    Serial.printf("[MESH STARTED] Node ID: %u\n",
                  mesh.getNodeId());
}

// MAIN LOOP==========================================


void loop() {

    updateMesh();
    processSerialInput();
    updateSerialConnection();

    uint32_t now = millis();

    updateGatewayManager();

    if(now - lastExitCheck >= EXIT_DISTANCE_CHECK){
        lastExitCheck = now;
        dijkstra(dijkstraMap, this_node);
    }

    // Mesh broadcast
    if (now - lastBroadcast >= BROADCAST_INTERVAL) {

        lastBroadcast = now;

        String message = "Hello from node ";
        message += String(mesh.getNodeId());

        mesh.sendBroadcast(message);

        Serial.printf("[MESH BROADCAST] %s\n", message.c_str());
    }

    // Periodic telemetry to Python
    if (now - lastServerTelemetry >= SERVER_TELEMETRY_INTERVAL) {

        lastServerTelemetry = now;

        String payload = "Telemetry from node ";
        payload += String(mesh.getNodeId());

        sendToServer(payload);
    }

    // Network diagnostics
    if (now - lastStatus >= STATUS_INTERVAL) {

        lastStatus = now;

        printNetworkStatus(
            mesh,
            isGatewayAvailable(),
            getGatewayNodeId()
        );
    }
}

