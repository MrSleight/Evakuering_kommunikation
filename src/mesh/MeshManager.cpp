
#include <Arduino.h>
#include "MeshManager.h"
#include "Config.h"

Scheduler userScheduler;
painlessMesh mesh;

void beginMesh() {
    mesh.init(
        MESH_PREFIX,
        MESH_PASSWORD,
        &userScheduler,
        MESH_PORT,
        WIFI_AP_STA,
        MESH_CHANNEL
    );
}

void updateMesh() {
    mesh.update();
}
