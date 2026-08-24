int networkDelayTime(int** times, int timesSize, int* timesColSize,
                     int n, int k) {

    int graph[101][101];
    int dist[101];
    int visited[101];

    int i, j;

    // Initialize graph
    for (i = 1; i <= n; i++) {
        for (j = 1; j <= n; j++) {
            graph[i][j] = 1000000;
        }
    }

    // Store directed edges
    for (i = 0; i < timesSize; i++) {
        int u = times[i][0];
        int v = times[i][1];
        int w = times[i][2];

        graph[u][v] = w;
    }

    // Initialize distances
    for (i = 1; i <= n; i++) {
        dist[i] = 1000000;
        visited[i] = 0;
    }

    dist[k] = 0;

    // Dijkstra
    for (i = 1; i <= n; i++) {

        int u = -1;
        int min = 1000000;

        // Find unvisited node with minimum distance
        for (j = 1; j <= n; j++) {
            if (!visited[j] && dist[j] < min) {
                min = dist[j];
                u = j;
            }
        }

        if (u == -1)
            break;

        visited[u] = 1;

        // Relax all neighbours
        for (j = 1; j <= n; j++) {
            if (!visited[j] &&
                graph[u][j] != 1000000 &&
                dist[u] + graph[u][j] < dist[j]) {

                dist[j] = dist[u] + graph[u][j];
            }
        }
    }

    // Find maximum shortest distance
    int answer = 0;

    for (i = 1; i <= n; i++) {

        if (dist[i] == 1000000)
            return -1;

        if (dist[i] > answer)
            answer = dist[i];
    }

    return answer;
}
