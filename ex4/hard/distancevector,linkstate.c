#include <stdio.h>
#define MAX 10
#define INF 999
int n;
int cost[MAX][MAX];
int dist[MAX][MAX];
int nexthop[MAX][MAX];
int visited[MAX];
int changed;
int edgeU[MAX * MAX], edgeV[MAX * MAX], edgeW[MAX * MAX], edgeCount;

void resetTables(void) {
    int i, j;
    for (i = 0; i < n; i++) {
        for (j = 0; j < n; j++) {
            dist[i][j] = cost[i][j];
            if (i == j)
                nexthop[i][j] = i;
            else if (cost[i][j] != INF)
                nexthop[i][j] = j;
            else
                nexthop[i][j] = -1;
        }
    }
}

void printAllTables(const char *label) {
    int i, j;
    printf("\n================= %s ROUTING TABLES =================\n", label);
    for (i = 0; i < n; i++) {
        printf("\n--- Routing table for Node %d ---\n", i);
        printf("Destination\tCost\tNext Hop\n");
        for (j = 0; j < n; j++) {
            if (i == j) continue;
            printf("%d\t\t", j);
            if (dist[i][j] >= INF)
                printf("INF\t-\n");
            else
                printf("%d\t%d\n", dist[i][j], nexthop[i][j]);
        }
    }
}
void displayPath(int src, int dest) {
    int cur;

    if (src < 0 || src >= n || dest < 0 || dest >= n) {
        printf("Invalid node number(s): %d, %d (valid range 0-%d).\n", src, dest, n - 1);
        return;
    }
    if (src == dest) {
        printf("Path: %d (source = destination)\n", src);
        return;
    }
    if (dist[src][dest] >= INF || nexthop[src][dest] == -1) {
        printf("No path exists from %d to %d.\n", src, dest);
        return;
    }

    printf("Path: %d", src);
    cur = src;
    while (cur != dest) {
        cur = nexthop[cur][dest];
        printf(" -> %d", cur);
    }
    printf("\nTotal Cost = %d\n", dist[src][dest]);
}

void displayAllPaths(void) {
    int i, j;
    printf("\n----------- Complete Shortest Paths (all node pairs) -----------\n");
    for (i = 0; i < n; i++) {
        for (j = 0; j < n; j++) {
            if (i == j) continue;
            displayPath(i, j);
        }
    }
}

void showPathMenu(void) {
    int src, dest, more;

    displayAllPaths();

    printf("\nLook up a specific path too? (1-Yes / 0-No): ");
    if (scanf("%d", &more) != 1) return;

    while (more == 1) {
        printf("Enter source and destination node to view path: ");
        if (scanf("%d %d", &src, &dest) != 2) return;
        displayPath(src, dest);

        printf("View another path? (1-Yes / 0-No): ");
        if (scanf("%d", &more) != 1) return;
    }
}
int askEdgeChange(void) {
    int change, i, j, newcost;

    printf("\nDo you want to change any edge cost? (1-Yes / 0-No): ");
    if (scanf("%d", &change) != 1) return 0;
    if (change == 1) {
        printf("Enter i j newcost (use %d for no link): ", INF);
        if (scanf("%d %d %d", &i, &j, &newcost) != 3) return 0;
        if (i < 0 || i >= n || j < 0 || j >= n || i == j) {
            printf("Invalid edge (%d,%d): node numbers must be 0-%d and i != j.\n", i, j, n - 1);
            return 0;
        }
        cost[i][j] = newcost;
        cost[j][i] = newcost;
        printf("Edge (%d,%d) updated to %d. Recalculating...\n", i, j, newcost);
        return 1;
    }
    return 0;
}
int dv_mindist(int d[], int n, int exclude) {
    int w, min = INF;
    for (w = 0; w < n; w++) {
        if (w != exclude && d[w] < min)
            min = d[w];
    }
    return min;
}

void dv_linkstate(int d[], int n, int node) {
    int j, k, cand[MAX], best;

    for (j = 0; j < n; j++) {
        if (j == node) continue;

        for (k = 0; k < n; k++) {
            if (k == node) { cand[k] = INF; continue; }
            if (cost[node][k] == INF || dist[k][j] == INF)
                cand[k] = INF;
            else
                cand[k] = cost[node][k] + dist[k][j];
        }

        best = dv_mindist(cand, n, node);
        if (best < d[j]) {
            d[j] = best;
            for (k = 0; k < n; k++) {
                if (k != node && cand[k] == best) {
                    nexthop[node][j] = k;
                    break;
                }
            }
            changed = 1;
        }
    }
}

void runDistanceVector(void) {
    int i, iteration = 0;

    resetTables();
    printAllTables("INITIAL (Distance Vector)");

    do {
        changed = 0;
        iteration++;
        for (i = 0; i < n; i++)
            dv_linkstate(dist[i], n, i);
    } while (changed);

    printf("\nDistance Vector converged after %d iteration(s).\n", iteration);
    printAllTables("FINAL (Distance Vector)");
}

int ls_mindist(int d[], int n, int src) {
    int i, min = INF, minIndex = -1;
    (void) src;
    for (i = 0; i < n; i++) {
        if (!visited[i] && d[i] < min) {
            min = d[i];
            minIndex = i;
        }
    }
    return minIndex;
}

void ls_linkstate(int d[], int n, int src) {
    int i, j, u, count, hop;
    int pred[MAX];

    for (i = 0; i < n; i++) {
        visited[i] = 0;
        d[i] = cost[src][i];
        pred[i] = (i != src && cost[src][i] != INF) ? src : -1;
    }
    d[src] = 0;
    visited[src] = 1;
    pred[src] = src;

    for (count = 1; count < n; count++) {
        u = ls_mindist(d, n, src);
        if (u == -1) break;
        visited[u] = 1;

        for (j = 0; j < n; j++) {
            if (!visited[j] && cost[u][j] != INF &&
                d[u] + cost[u][j] < d[j]) {
                d[j] = d[u] + cost[u][j];
                pred[j] = u;
            }
        }
    }

    for (j = 0; j < n; j++) {
        if (j == src) { nexthop[src][j] = src; continue; }
        if (d[j] >= INF || pred[j] == -1) { nexthop[src][j] = -1; continue; }
        hop = j;
        while (pred[hop] != src)
            hop = pred[hop];
        nexthop[src][j] = hop;
    }
}

void runLinkState(void) {
    int i;

    resetTables();
    printAllTables("INITIAL (Link State)");

    for (i = 0; i < n; i++)
        ls_linkstate(dist[i], n, i);

    printAllTables("FINAL (Link State)");
}
int main(void) {
    int i, j, choice;

    printf("===== Routing Algorithms: Distance Vector, Link State, Bellman-Ford =====\n");
    printf("Enter number of nodes (max %d): ", MAX);
    if (scanf("%d", &n) != 1 || n <= 0 || n > MAX) {
        printf("Invalid number of nodes. Must be between 1 and %d.\n", MAX);
        return 1;
    }

    printf("Enter cost matrix (%d = no direct link, 0 for diagonal).\n", INF);
    printf("Enter all %d values row by row, separated by spaces or newlines:\n", n * n);
    for (i = 0; i < n; i++) {
        for (j = 0; j < n; j++) {
            if (scanf("%d", &cost[i][j]) != 1) {
                printf("Invalid input while reading cost matrix. Exiting.\n");
                return 1;
            }
        }
    }

    do {
        printf("\n--------------- MENU ---------------\n");
        printf("1. Distance Vector Routing\n");
        printf("2. Link State Routing\n");
        printf("3. Exit\n");
        printf("Enter your choice: ");
        if (scanf("%d", &choice) != 1) {
            printf("Invalid input. Exiting.\n");
            break;
        }

        switch (choice) {
            case 1:
                do {
                    runDistanceVector();
                    showPathMenu();
                } while (askEdgeChange());
                break;
            case 2:
                do {
                    runLinkState();
                    showPathMenu();
                } while (askEdgeChange());
                break;
            case 3:
                printf("Exiting.\n");
                break;
            default:
                printf("Invalid choice. Please enter 1-5.\n");
        }
    } while (choice != 3);

    return 0;
}
