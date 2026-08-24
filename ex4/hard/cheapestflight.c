int findCheapestPrice(int n, int** flights, int flightsSize,
                      int* flightsColSize, int src, int dst, int k) {

    int dist[n];
    int temp[n];
    int i, j;

    // Initialize distances
    for (i = 0; i < n; i++) {
        dist[i] = 1000000;
    }

    dist[src] = 0;

    /*
       At most k stops means at most k+1 flights.
       Perform Bellman-Ford relaxation k+1 times.
    */
    for (i = 0; i <= k; i++) {

        // Copy current distances
        for (j = 0; j < n; j++) {
            temp[j] = dist[j];
        }

        // Relax every flight
        for (j = 0; j < flightsSize; j++) {

            int from = flights[j][0];
            int to = flights[j][1];
            int price = flights[j][2];

            if (dist[from] != 1000000 &&
                dist[from] + price < temp[to]) {

                temp[to] = dist[from] + price;
            }
        }

        // Update distances
        for (j = 0; j < n; j++) {
            dist[j] = temp[j];
        }
    }

    // Destination unreachable
    if (dist[dst] == 1000000)
        return -1;

    return dist[dst];
}
