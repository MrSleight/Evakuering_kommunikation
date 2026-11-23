
#pragma once

#include <Arduino.h>
#include <vector>

struct RoutingResult {
    bool valid;
    uint32_t selectedExit;
    int totalCost;
};

RoutingResult dijkstra(
    const std::vector<std::vector<int>>& graph,
    const std::vector<uint32_t>& nodeIds,
    const std::vector<bool>& blocked,
    uint32_t from,
    uint32_t exitRight,
    uint32_t exitLeft
);

RoutingResult calculateRoute(uint32_t from);