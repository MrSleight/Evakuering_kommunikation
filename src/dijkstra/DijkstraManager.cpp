#include <Arduino.h>
#include <limits.h>
#include "DijkstraManager.h"

const int num_nodes = 0;
int exit_right = 0;
int exit_left = 0;

int minDistance(const std::vector<int>& dist, const std::vector<bool>& routeChecked){
    int min = INT_MAX, min_index;

    for (int i = 0; i < num_nodes; i++){
        if (routeChecked[i] == false && dist[i] <= min){
            min = dist[i], min_index = i;
        }
    }

    return min_index;
}

void dijkstra(const std::vector<std::vector<int>>& graph, int from){
    int num_nodes = graph.size();
    std::vector<int> dist(num_nodes, INT_MAX);
    std::vector<bool> routeChecked(num_nodes, false);

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

        if (routeChecked[exit_right] && routeChecked[exit_left])
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

    if (dist[exit_right] < dist[exit_left]){
        printf("Right is closer than Left==============================\n");
    }else if (dist[exit_right] > dist[exit_left]){
        printf("Left is closer than Right==============================\n");
    }
}