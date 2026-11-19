#pragma once
#include <Arduino.h>

int minDistance(const std::vector<int>& dist, const std::vector<bool>& routeChecked);
void dijkstra(const std::vector<std::vector<int>>& graph, int from);

//int minDistance(int dist[], bool routeChecked[], int num_nodes);
//void dijkstra(int graph[num_nodes][num_nodes], int from);