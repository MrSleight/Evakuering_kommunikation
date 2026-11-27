#include <Arduino.h>
#include <limits.h>
#include <vector>

#include "../messaging/MessageManager.h"
#include "../Display/DisplayManager.h"
#include "DijkstraManager.h"

const std::vector<uint32_t> nodeIds = {
    4294836393,
    4294868513,
    4294827913
    // 1231725625,
};

std::vector<std::vector<int>> dijkstraMap = {
    {0, 2, 1},
    {2, 0, 3},
    {1, 3, 0}
};

uint32_t this_node = 4294827913;
uint32_t exitRight = 4294836393;
uint32_t exitLeft  = 4294868513;

static int findNodeIndex(
    const std::vector<uint32_t>& nodeIds,
    uint32_t nodeId
) {
    for (size_t i = 0; i < nodeIds.size(); i++) {
        if (nodeIds[i] == nodeId) {
            return static_cast<int>(i);
        }
    }

    return -1;
}

static int minDistance(
    const std::vector<int>& dist,
    const std::vector<bool>& checked
) {
    int min = INT_MAX;
    int minIndex = -1;

    for (size_t i = 0; i < dist.size(); i++) {
        if (!checked[i] && dist[i] < min) {
            min = dist[i];
            minIndex = static_cast<int>(i);
        }
    }

    return minIndex;
}

RoutingResult dijkstra(
    const std::vector<std::vector<int>>& graph,
    const std::vector<uint32_t>& nodeIds,
    const std::vector<bool>& blocked,
    uint32_t from,
    uint32_t exitRight,
    uint32_t exitLeft
) {
    RoutingResult result = {false, 0, INT_MAX};

    const int numNodes = static_cast<int>(nodeIds.size());

    if (numNodes == 0 ||
        graph.size() != nodeIds.size() ||
        blocked.size() != nodeIds.size()) {
        return result;
    }

    for (const auto& row : graph) {
        if (row.size() != nodeIds.size()) {
            return result;
        }
    }

    const int startIndex = findNodeIndex(nodeIds, from);
    const int rightIndex = findNodeIndex(nodeIds, exitRight);
    const int leftIndex = findNodeIndex(nodeIds, exitLeft);

    if (startIndex < 0 || rightIndex < 0 || leftIndex < 0) {
        return result;
    }

    std::vector<int> dist(numNodes, INT_MAX);
    std::vector<bool> checked(numNodes, false);

    dist[startIndex] = 0;

    for (int i = 0; i < numNodes; i++) {
        int u = minDistance(dist, checked);

        if (u == -1) {
            break;
        }

        checked[u] = true;

        if (checked[rightIndex] && checked[leftIndex]) {
            break;
        }

        for (int j = 0; j < numNodes; j++) {
            if (checked[j] || blocked[j]) {
                continue;
            }

            const int weight = graph[u][j];

            if (weight <= 0 || dist[u] == INT_MAX) {
                continue;
            }

            if (dist[u] > INT_MAX - weight) {
                continue;
            }

            const int newCost = dist[u] + weight;

            if (newCost < dist[j]) {
                dist[j] = newCost;
            }
        }
    }

    if (dist[rightIndex] == INT_MAX &&
        dist[leftIndex] == INT_MAX) {
        return result;
    }

    if (dist[rightIndex] <= dist[leftIndex]) {
        result.selectedExit = exitRight;
        result.totalCost = dist[rightIndex];
    } else {
        result.selectedExit = exitLeft;
        result.totalCost = dist[leftIndex];
    }

    result.valid = true;
    return result;
}

RoutingResult calculateRoute(uint32_t from) {
    std::vector<bool> blockedNodes;

    for (uint32_t id : nodeIds) {
        blockedNodes.push_back(isNodeBlocked(id));
    }

    Serial.println("[ROUTING] Blockage status:");

    for (size_t i = 0; i < nodeIds.size(); i++) {
        Serial.printf(
            "[ROUTING] Node %u: %s\n",
            nodeIds[i],
            blockedNodes[i] ? "BLOCKED" : "AVAILABLE"
        );
    }

    RoutingResult result = dijkstra(
        dijkstraMap,
        nodeIds,
        blockedNodes,
        from,
        exitRight,
        exitLeft
    );

    // Update display direction when a valid route is found.
    if (result.valid) {
        const bool directionRight = (result.selectedExit == exitRight);
        displaySetDirection(directionRight);

        Serial.printf(
            "[DISPLAY] Direction set to: %s\n",
            directionRight ? "RIGHT" : "LEFT"
        );
    }

    return result;
}