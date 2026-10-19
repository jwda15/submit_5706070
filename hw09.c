/*
 * 과제 09: 모든 정점 쌍의 최단경로 탐색
 * 빌드: gcc -o hw09 hw09.c
 * 실행: ./hw09
 *
 *  - 그래프는 인접 행렬로 표현한다. (weight[u][v] = 0 이면 간선 없음)
 *  - 각 정점을 시작 정점으로 Dijkstra를 10번 수행한다.
 *    dist[s][v] : s에서 v까지의 최단거리
 *    prev[s][v] : s에서 v까지의 최단경로에서 v 바로 앞의 정점 (경로 복원용)
 *  - 결과 검증용으로 Floyd-Warshall을 따로 수행해서 최단거리가 같은지 비교한다.
 */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define VERTEX_COUNT 10
#define EDGE_COUNT   20
#define MAX_WEIGHT   100     /* 가중치 1 ~ 100 */
#define INF          1000000000

typedef struct {
    int u, v, weight;
} Edge;

int weight[VERTEX_COUNT][VERTEX_COUNT];      /* 인접 행렬 */
Edge edges[EDGE_COUNT];                      /* 생성된 간선 (출력용, u < v) */

int dist[VERTEX_COUNT][VERTEX_COUNT];        /* Dijkstra 결과 */
int prev[VERTEX_COUNT][VERTEX_COUNT];
int fw_dist[VERTEX_COUNT][VERTEX_COUNT];     /* Floyd-Warshall 결과 (검증용) */

/* ====================== [1] 그래프 생성 ====================== */

/* 행렬을 DFS로 탐색해서 모든 정점에 도달하면 1 */
int is_connected(void)
{
    int visited[VERTEX_COUNT] = {0};
    int stack[VERTEX_COUNT], top = 0, count = 0, u, v;

    stack[top++] = 0;
    visited[0] = 1;
    while (top > 0) {
        u = stack[--top];
        count++;
        for (v = 0; v < VERTEX_COUNT; v++)
            if (weight[u][v] != 0 && !visited[v]) {
                visited[v] = 1;
                stack[top++] = v;
            }
    }
    return count == VERTEX_COUNT;
}

/* 간선을 (u, v) 순으로 정렬한다 (출력을 보기 쉽게 하기 위함) */
int edge_cmp(const void *a, const void *b)
{
    const Edge *x = (const Edge *)a, *y = (const Edge *)b;

    if (x->u != y->u)
        return x->u - y->u;
    return x->v - y->v;
}

/*
 * 가능한 정점 쌍 (u < v) 45개를 섞어 앞에서 20개를 고르고 가중치를 붙인다.
 * u < v인 쌍만 만들므로 자기 루프가 없고, 각 쌍이 한 번씩만 있으므로 중복 간선도 없다.
 * 연결 그래프가 아니면 다시 만든다. 시도 횟수를 돌려준다.
 */
int generate_graph(void)
{
    Edge pairs[VERTEX_COUNT * (VERTEX_COUNT - 1) / 2], tmp;
    int pair_total = VERTEX_COUNT * (VERTEX_COUNT - 1) / 2;
    int i, j, k, attempts = 0;

    do {
        attempts++;
        k = 0;
        for (i = 0; i < VERTEX_COUNT; i++)
            for (j = i + 1; j < VERTEX_COUNT; j++) {
                pairs[k].u = i;
                pairs[k].v = j;
                k++;
            }

        /* Fisher-Yates 셔플 */
        for (i = pair_total - 1; i > 0; i--) {
            j = rand() % (i + 1);
            tmp = pairs[i]; pairs[i] = pairs[j]; pairs[j] = tmp;
        }

        for (i = 0; i < VERTEX_COUNT; i++)
            for (j = 0; j < VERTEX_COUNT; j++)
                weight[i][j] = 0;

        for (i = 0; i < EDGE_COUNT; i++) {
            edges[i] = pairs[i];
            edges[i].weight = rand() % MAX_WEIGHT + 1;
            weight[edges[i].u][edges[i].v] = edges[i].weight;
            weight[edges[i].v][edges[i].u] = edges[i].weight;
        }
    } while (!is_connected());

    qsort(edges, EDGE_COUNT, sizeof(Edge), edge_cmp);
    return attempts;
}

/* ====================== [2] Dijkstra ====================== */

/*
 * start에서 다른 모든 정점까지의 최단거리와 직전 정점을 구한다.
 * 매번 확정되지 않은 정점 중 거리가 가장 작은 정점을 골라 확정하고,
 * 그 정점을 거쳐 가는 것이 더 짧으면 이웃의 거리와 직전 정점을 갱신한다.
 */
void dijkstra(int start)
{
    int found[VERTEX_COUNT];
    int *d = dist[start], *p = prev[start];
    int i, u, v, min;

    for (v = 0; v < VERTEX_COUNT; v++) {
        d[v] = INF;
        p[v] = -1;
        found[v] = 0;
    }
    d[start] = 0;

    for (i = 0; i < VERTEX_COUNT; i++) {
        u = -1;
        min = INF;
        for (v = 0; v < VERTEX_COUNT; v++)
            if (!found[v] && d[v] < min) {
                min = d[v];
                u = v;
            }
        if (u == -1)
            break;
        found[u] = 1;

        for (v = 0; v < VERTEX_COUNT; v++)
            if (weight[u][v] != 0 && !found[v] && d[u] + weight[u][v] < d[v]) {
                d[v] = d[u] + weight[u][v];
                p[v] = u;
            }
    }
}

/* prev를 거꾸로 따라가서 s -> t 경로를 path[]에 담는다. 정점 수를 돌려준다 */
int build_path(int s, int t, int *path)
{
    int stack[VERTEX_COUNT], n = 0, len = 0, v;

    for (v = t; v != -1; v = prev[s][v])
        stack[n++] = v;
    while (n > 0)
        path[len++] = stack[--n];
    return len;
}

/* ====================== [3] Floyd-Warshall (검증용) ====================== */

void floyd_warshall(void)
{
    int i, j, k;

    for (i = 0; i < VERTEX_COUNT; i++)
        for (j = 0; j < VERTEX_COUNT; j++) {
            if (i == j)
                fw_dist[i][j] = 0;
            else if (weight[i][j] != 0)
                fw_dist[i][j] = weight[i][j];
            else
                fw_dist[i][j] = INF;
        }

    for (k = 0; k < VERTEX_COUNT; k++)
        for (i = 0; i < VERTEX_COUNT; i++)
            for (j = 0; j < VERTEX_COUNT; j++)
                if (fw_dist[i][k] != INF && fw_dist[k][j] != INF &&
                    fw_dist[i][k] + fw_dist[k][j] < fw_dist[i][j])
                    fw_dist[i][j] = fw_dist[i][k] + fw_dist[k][j];
}

/* 간선 수가 가장 적은 경로의 간선 수 (BFS). 최단경로와 비교하기 위해 사용 */
int min_hops(int s, int t)
{
    int hop[VERTEX_COUNT], queue[VERTEX_COUNT], front = 0, rear = 0, u, v;

    for (v = 0; v < VERTEX_COUNT; v++)
        hop[v] = -1;
    hop[s] = 0;
    queue[rear++] = s;
    while (front < rear) {
        u = queue[front++];
        for (v = 0; v < VERTEX_COUNT; v++)
            if (weight[u][v] != 0 && hop[v] == -1) {
                hop[v] = hop[u] + 1;
                queue[rear++] = v;
            }
    }
    return hop[t];
}

/* ====================== [4] 출력 ====================== */

int main(void)
{
    int attempts, s, t, i, len, sum;
    int path[VERTEX_COUNT];
    int pair_count = 0, sum_ok = 0, fw_ok = 0;
    int direct_edge_not_used = 0, more_edges_than_min = 0;
    int self_loops = 0, duplicates = 0, edge_cells = 0;
    int degree[VERTEX_COUNT] = {0};

    srand((unsigned)time(NULL));
    attempts = generate_graph();

    /* ---- 그래프 출력 ---- */
    printf("Graph\n");
    printf("Vertices : %d\n", VERTEX_COUNT);
    printf("Edges    : %d\n\n", EDGE_COUNT);

    printf("Edges\n");
    for (i = 0; i < EDGE_COUNT; i++) {
        printf("(%d, %d) Weight: %d\n", edges[i].u, edges[i].v, edges[i].weight);
        degree[edges[i].u]++;
        degree[edges[i].v]++;
    }
    printf("\nDegree   :");
    for (i = 0; i < VERTEX_COUNT; i++)
        printf(" %d(%d)", i, degree[i]);
    printf("\n\n");

    /* ---- 최단경로 계산 ---- */
    for (s = 0; s < VERTEX_COUNT; s++)
        dijkstra(s);
    floyd_warshall();

    /* ---- 최단경로 출력 ---- */
    printf("Shortest Paths\n\n");
    for (s = 0; s < VERTEX_COUNT; s++)
        for (t = s + 1; t < VERTEX_COUNT; t++) {
            len = build_path(s, t, path);

            printf("%d -> %d\n", s, t);
            printf("Path     : ");
            for (i = 0; i < len; i++)
                printf(i == 0 ? "%d" : " -> %d", path[i]);
            printf("\n");

            /* 경로의 간선 가중치를 하나씩 더해서 최단거리와 같은지 확인 */
            printf("Weights  : ");
            sum = 0;
            for (i = 0; i + 1 < len; i++) {
                printf(i == 0 ? "%d" : " + %d", weight[path[i]][path[i + 1]]);
                sum += weight[path[i]][path[i + 1]];
            }
            printf(" = %d\n", sum);
            printf("Distance : %d\n", dist[s][t]);

            if (len - 1 > min_hops(s, t)) {
                printf("Note     : uses %d edges, fewest possible is %d\n",
                       len - 1, min_hops(s, t));
                more_edges_than_min++;
            }
            if (weight[s][t] != 0 && len > 2) {
                printf("Note     : direct edge (%d, %d) weight %d is longer\n",
                       s, t, weight[s][t]);
                direct_edge_not_used++;
            }
            printf("\n");

            pair_count++;
            if (sum == dist[s][t])
                sum_ok++;
            if (fw_dist[s][t] == dist[s][t])
                fw_ok++;
        }

    printf("Total vertex pairs : %d\n\n", pair_count);

    /* ---- 확인 사항 ---- */
    for (s = 0; s < VERTEX_COUNT; s++) {
        if (weight[s][s] != 0)
            self_loops++;
        for (t = 0; t < VERTEX_COUNT; t++)
            if (weight[s][t] != 0)
                edge_cells++;
    }
    for (i = 0; i + 1 < EDGE_COUNT; i++)
        if (edges[i].u == edges[i + 1].u && edges[i].v == edges[i + 1].v)
            duplicates++;

    printf("Checks\n");
    printf("Vertices                          : %d\n", VERTEX_COUNT);
    printf("Edges (matrix cells / 2)          : %d\n", edge_cells / 2);
    printf("Self-loops                        : %d\n", self_loops);
    printf("Duplicate edges                   : %d\n", duplicates);
    printf("Connected                         : %s (generation attempts: %d)\n",
           is_connected() ? "Yes" : "No", attempts);
    printf("Vertex pairs printed              : %d / %d\n", pair_count,
           VERTEX_COUNT * (VERTEX_COUNT - 1) / 2);
    printf("Path weight sum == Distance       : %d / %d\n", sum_ok, pair_count);
    printf("Dijkstra == Floyd-Warshall        : %d / %d\n", fw_ok, pair_count);
    printf("Paths with more edges than fewest : %d\n", more_edges_than_min);
    printf("Direct edge not shortest          : %d\n", direct_edge_not_used);

    return 0;
}
