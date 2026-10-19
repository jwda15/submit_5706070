/*
 * 과제 08: 최소 비용 신장 트리 알고리즘 비교 (Kruskal vs Prim)
 * 빌드: gcc -o hw08 hw08.c
 * 실행: ./hw08
 *
 *  - Kruskal : 인접 리스트에서 간선을 모아 힙 정렬한 뒤 Union-Find로 사이클을 검사한다.
 *  - Prim    : 인접 행렬과 dist[] 배열로 매번 가장 비용이 작은 정점을 고른다. O(V^2)
 *
 * 실행시간은 MST 계산 한 번이 수 마이크로초 수준이라 타이머 해상도보다 짧을 수 있으므로,
 * 같은 그래프에 대해 REPEAT_COUNT번 반복한 전체 시간을 재서 1회 평균(μs)으로 출력한다.
 * 그래프 생성과 결과 출력은 시간 측정 구간에 포함되지 않는다.
 */
#ifndef _WIN32
#define _POSIX_C_SOURCE 199309L   /* gcc -std=c99 등에서도 clock_gettime을 쓰기 위함 */
#endif

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#endif

#define VERTEX_COUNT 50
#define SPARSE_EDGES 100
#define DENSE_EDGES  600
#define MAX_WEIGHT   100     /* 가중치 1 ~ 100 */
#define REPEAT_COUNT 10000   /* 실행시간 측정 반복 횟수 */
#define INF          1000000000

/* ---------- 간선 (Kruskal의 간선 배열, MST 결과, 그래프 생성에 공통으로 사용) ---------- */
typedef struct {
    int u, v, weight;
} Edge;

/* ---------- 인접 리스트 ---------- */
typedef struct Node {
    int vertex;
    int weight;
    struct Node *link;
} Node;

typedef struct {
    int n;                       /* 정점 수 */
    int edges;                   /* 간선 수 */
    Node *head[VERTEX_COUNT];
} ListGraph;

/* ---------- 인접 행렬 (weight[u][v] = 0 이면 간선 없음) ---------- */
typedef struct {
    int n;
    int edges;
    int weight[VERTEX_COUNT][VERTEX_COUNT];
} MatrixGraph;

/* ---------- Kruskal 보조 자료구조 ---------- */
typedef struct {
    Edge *edges;                 /* 정렬할 간선 배열 (간선 수만큼 할당) */
    int edge_count;
    int parent[VERTEX_COUNT];    /* Union-Find 부모 */
    int rank[VERTEX_COUNT];      /* Union-Find 랭크 (트리 높이의 상한) */
} KruskalData;

/* ---------- Prim 보조 자료구조 ---------- */
typedef struct {
    int visited[VERTEX_COUNT];   /* MST에 포함되었는지 */
    int dist[VERTEX_COUNT];      /* MST까지의 최소 연결 비용 */
    int parent[VERTEX_COUNT];    /* dist[v]를 만든 MST 쪽 정점 */
} PrimData;

/* ---------- MST 결과 ---------- */
typedef struct {
    Edge edges[VERTEX_COUNT - 1];
    int count;
    int cost;
} MstResult;

/* ====================== 시간 측정 ====================== */

double now_us(void)
{
#ifdef _WIN32
    static LARGE_INTEGER freq;
    LARGE_INTEGER t;

    if (freq.QuadPart == 0)
        QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&t);
    return (double)t.QuadPart * 1000000.0 / (double)freq.QuadPart;
#else
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000.0 + ts.tv_nsec / 1000.0;
#endif
}

/* ====================== [1] 인접 리스트 ====================== */

void list_init(ListGraph *g)
{
    int i;

    g->n = VERTEX_COUNT;
    g->edges = 0;
    for (i = 0; i < VERTEX_COUNT; i++)
        g->head[i] = NULL;
}

/* u의 리스트 맨 앞에 (v, w)를 넣는다 */
void list_push(ListGraph *g, int u, int v, int w)
{
    Node *node = (Node *)malloc(sizeof(Node));

    if (node == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }
    node->vertex = v;
    node->weight = w;
    node->link = g->head[u];
    g->head[u] = node;
}

void list_insert_edge(ListGraph *g, int u, int v, int w)
{
    list_push(g, u, v, w);
    list_push(g, v, u, w);
    g->edges++;
}

void list_free(ListGraph *g)
{
    int i;
    Node *p, *next;

    for (i = 0; i < g->n; i++) {
        for (p = g->head[i]; p != NULL; p = next) {
            next = p->link;
            free(p);
        }
        g->head[i] = NULL;
    }
}

/* 관리 구조체 + 간선 노드 (무방향이므로 간선 하나당 노드 2개) */
long list_memory(ListGraph *g)
{
    return (long)sizeof(ListGraph) + 2L * g->edges * (long)sizeof(Node);
}

/* ====================== [2] 인접 행렬 ====================== */

void matrix_init(MatrixGraph *g)
{
    int i, j;

    g->n = VERTEX_COUNT;
    g->edges = 0;
    for (i = 0; i < VERTEX_COUNT; i++)
        for (j = 0; j < VERTEX_COUNT; j++)
            g->weight[i][j] = 0;
}

void matrix_insert_edge(MatrixGraph *g, int u, int v, int w)
{
    g->weight[u][v] = w;
    g->weight[v][u] = w;
    g->edges++;
}

/* ====================== [3] 그래프 생성 ====================== */

/*
 * 가능한 모든 정점 쌍 (u < v) 1,225개를 섞어 앞에서 edge_count개를 고르고 가중치를 붙인다.
 * 자기 루프와 중복 간선이 생기지 않는다.
 * 연결 그래프가 아니면 처음부터 다시 만든다. 시도 횟수를 돌려준다.
 */
int generate_edges(Edge *out, int edge_count)
{
    int pair_total = VERTEX_COUNT * (VERTEX_COUNT - 1) / 2;
    Edge *pairs = (Edge *)malloc(sizeof(Edge) * pair_total);
    int i, j, k, attempts = 0;
    Edge tmp;

    /* 연결 여부 확인용 Union-Find (임시, 메모리 측정 제외) */
    int comp[VERTEX_COUNT], components, a, b;

    if (pairs == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }

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

        for (i = 0; i < edge_count; i++) {
            out[i] = pairs[i];
            out[i].weight = rand() % MAX_WEIGHT + 1;
        }

        /* 연결 그래프인지 확인: 간선으로 집합을 합쳐서 집합이 1개 남는지 본다 */
        for (i = 0; i < VERTEX_COUNT; i++)
            comp[i] = i;
        components = VERTEX_COUNT;
        for (i = 0; i < edge_count; i++) {
            a = out[i].u;
            b = out[i].v;
            while (comp[a] != a) a = comp[a];
            while (comp[b] != b) b = comp[b];
            if (a != b) {
                comp[a] = b;
                components--;
            }
        }
    } while (components != 1);

    free(pairs);
    return attempts;
}

/* ====================== [4] Kruskal ====================== */

/* 가중치 기준 최대 힙에서 i번 원소를 아래로 내린다 */
void sift_down(Edge *a, int i, int size)
{
    int child;
    Edge tmp;

    while ((child = 2 * i + 1) < size) {
        if (child + 1 < size && a[child + 1].weight > a[child].weight)
            child++;
        if (a[i].weight >= a[child].weight)
            break;
        tmp = a[i]; a[i] = a[child]; a[child] = tmp;
        i = child;
    }
}

/* 힙 정렬 (가중치 오름차순). 추가 배열이 필요 없다 */
void heap_sort(Edge *a, int size)
{
    int i;
    Edge tmp;

    for (i = size / 2 - 1; i >= 0; i--)
        sift_down(a, i, size);
    for (i = size - 1; i > 0; i--) {
        tmp = a[0]; a[0] = a[i]; a[i] = tmp;
        sift_down(a, 0, i);
    }
}

/* 경로 압축을 하는 find */
int uf_find(int *parent, int x)
{
    int root = x, next;

    while (parent[root] != root)
        root = parent[root];
    while (parent[x] != root) {
        next = parent[x];
        parent[x] = root;
        x = next;
    }
    return root;
}

/* 랭크 기준 union. 이미 같은 집합이면 0 (= 사이클) */
int uf_union(int *parent, int *rank, int a, int b)
{
    a = uf_find(parent, a);
    b = uf_find(parent, b);
    if (a == b)
        return 0;
    if (rank[a] < rank[b])
        parent[a] = b;
    else if (rank[a] > rank[b])
        parent[b] = a;
    else {
        parent[b] = a;
        rank[a]++;
    }
    return 1;
}

/*
 * 1) 인접 리스트에서 간선을 모은다 (u < v인 노드만 담아서 간선당 한 번)
 * 2) 가중치 순으로 정렬한다
 * 3) 작은 간선부터 사이클이 생기지 않으면 MST에 넣는다. V - 1개가 되면 멈춘다
 * 확인한 간선 수를 돌려준다.
 */
int kruskal(ListGraph *g, KruskalData *kd, MstResult *mst)
{
    int u, i, examined = 0;
    Node *p;
    Edge *e;

    kd->edge_count = 0;
    for (u = 0; u < g->n; u++)
        for (p = g->head[u]; p != NULL; p = p->link)
            if (u < p->vertex) {
                e = &kd->edges[kd->edge_count++];
                e->u = u;
                e->v = p->vertex;
                e->weight = p->weight;
            }

    heap_sort(kd->edges, kd->edge_count);

    for (i = 0; i < g->n; i++) {
        kd->parent[i] = i;
        kd->rank[i] = 0;
    }

    mst->count = 0;
    mst->cost = 0;
    for (i = 0; i < kd->edge_count && mst->count < g->n - 1; i++) {
        e = &kd->edges[i];
        examined++;
        if (uf_union(kd->parent, kd->rank, e->u, e->v)) {
            mst->edges[mst->count++] = *e;
            mst->cost += e->weight;
        }
    }
    return examined;
}

/* ====================== [5] Prim ====================== */

/*
 * start에서 시작한다. 매번 dist[]가 가장 작은 미방문 정점을 골라 MST에 넣고,
 * 그 정점의 행렬 행을 보고 다른 정점들의 dist[]를 갱신한다.
 */
void prim(MatrixGraph *g, PrimData *pd, int start, MstResult *mst)
{
    int i, v, u, w, min;

    for (v = 0; v < g->n; v++) {
        pd->visited[v] = 0;
        pd->dist[v] = INF;
        pd->parent[v] = -1;
    }
    pd->dist[start] = 0;

    mst->count = 0;
    mst->cost = 0;
    for (i = 0; i < g->n; i++) {
        u = -1;
        min = INF;
        for (v = 0; v < g->n; v++)
            if (!pd->visited[v] && pd->dist[v] < min) {
                min = pd->dist[v];
                u = v;
            }
        if (u == -1)             /* 연결 그래프가 아니면 여기서 끝남 */
            break;

        pd->visited[u] = 1;
        if (pd->parent[u] != -1) {
            mst->edges[mst->count].u = pd->parent[u];
            mst->edges[mst->count].v = u;
            mst->edges[mst->count].weight = pd->dist[u];
            mst->count++;
            mst->cost += pd->dist[u];
        }

        for (v = 0; v < g->n; v++) {
            w = g->weight[u][v];
            if (w != 0 && !pd->visited[v] && w < pd->dist[v]) {
                pd->dist[v] = w;
                pd->parent[v] = u;
            }
        }
    }
}

/* ====================== [6] 실험과 출력 ====================== */

void print_mst_edges(MstResult *mst)
{
    int i;

    printf("MST edge list (u-v:weight)\n");
    for (i = 0; i < mst->count; i++) {
        printf("  %2d-%-2d:%3d", mst->edges[i].u, mst->edges[i].v, mst->edges[i].weight);
        if (i % 7 == 6 || i == mst->count - 1)
            printf("\n");
    }
}

void print_case(int no, const char *graph, const char *repr, const char *algo,
                int edges, MstResult *mst, long memory, double time_us)
{
    printf("Case %d : %s Graph - Adjacency %s - %s\n", no, graph, repr, algo);
    printf("Vertices      : %d\n", VERTEX_COUNT);
    printf("Edges         : %d\n", edges);
    printf("MST Edges     : %d\n", mst->count);
    printf("MST Cost      : %d\n", mst->cost);
    printf("Memory        : %ld Bytes\n", memory);
    printf("Execution Time: %.3f us (average of %d runs)\n", time_us, REPEAT_COUNT);
}

typedef struct {
    MstResult kruskal_mst, prim_mst;
    long list_mem, kruskal_aux_mem, matrix_mem, prim_aux_mem, mst_mem;
    double kruskal_us, prim_us;
    int examined, attempts, start;
} Summary;

void run_graph(const char *name, int edge_count, Summary *s)
{
    Edge *generated = (Edge *)malloc(sizeof(Edge) * edge_count);
    ListGraph lg;
    MatrixGraph *mg = (MatrixGraph *)malloc(sizeof(MatrixGraph));
    KruskalData kd;
    PrimData pd;
    double t0, t1;
    long checksum = 0;
    int i;

    if (generated == NULL || mg == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }

    /* 같은 간선 목록으로 인접 리스트와 인접 행렬을 만든다 */
    s->attempts = generate_edges(generated, edge_count);
    list_init(&lg);
    matrix_init(mg);
    for (i = 0; i < edge_count; i++) {
        list_insert_edge(&lg, generated[i].u, generated[i].v, generated[i].weight);
        matrix_insert_edge(mg, generated[i].u, generated[i].v, generated[i].weight);
    }
    free(generated);   /* 생성용 임시 배열은 여기서 해제 (메모리 측정 제외) */

    kd.edges = (Edge *)malloc(sizeof(Edge) * edge_count);
    if (kd.edges == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }
    s->start = rand() % VERTEX_COUNT;

    /* ---- Kruskal 시간 측정 ---- */
    t0 = now_us();
    for (i = 0; i < REPEAT_COUNT; i++) {
        s->examined = kruskal(&lg, &kd, &s->kruskal_mst);
        checksum += s->kruskal_mst.cost;
    }
    t1 = now_us();
    s->kruskal_us = (t1 - t0) / REPEAT_COUNT;

    /* ---- Prim 시간 측정 ---- */
    t0 = now_us();
    for (i = 0; i < REPEAT_COUNT; i++) {
        prim(mg, &pd, s->start, &s->prim_mst);
        checksum += s->prim_mst.cost;
    }
    t1 = now_us();
    s->prim_us = (t1 - t0) / REPEAT_COUNT;

    /* 반복 계산이 최적화로 사라지지 않도록 결과를 사용한다 */
    if (checksum != (long)REPEAT_COUNT * (s->kruskal_mst.cost + s->prim_mst.cost))
        printf("Warning: MST cost changed between runs (%s)\n", name);

    /* ---- 메모리 ---- */
    s->list_mem = list_memory(&lg);
    s->kruskal_aux_mem = (long)sizeof(KruskalData) + (long)edge_count * (long)sizeof(Edge);
    s->matrix_mem = (long)sizeof(MatrixGraph);
    s->prim_aux_mem = (long)sizeof(PrimData);
    s->mst_mem = (long)sizeof(MstResult);

    free(kd.edges);
    list_free(&lg);
    free(mg);
}

int main(void)
{
    Summary sp, de;

    srand((unsigned)time(NULL));

    run_graph("Sparse", SPARSE_EDGES, &sp);
    run_graph("Dense", DENSE_EDGES, &de);

    printf("Graph generation\n");
    printf("Sparse Graph : %d vertices, %d edges, connected (attempts: %d)\n",
           VERTEX_COUNT, SPARSE_EDGES, sp.attempts);
    printf("Dense Graph  : %d vertices, %d edges, connected (attempts: %d)\n",
           VERTEX_COUNT, DENSE_EDGES, de.attempts);
    printf("Edge weights : 1 ~ %d\n\n", MAX_WEIGHT);

    print_case(1, "Sparse", "List", "Kruskal", SPARSE_EDGES, &sp.kruskal_mst,
               sp.list_mem + sp.kruskal_aux_mem + sp.mst_mem, sp.kruskal_us);
    printf("Sorted edges examined: %d / %d\n", sp.examined, SPARSE_EDGES);
    print_mst_edges(&sp.kruskal_mst);
    printf("\n");

    print_case(2, "Dense", "List", "Kruskal", DENSE_EDGES, &de.kruskal_mst,
               de.list_mem + de.kruskal_aux_mem + de.mst_mem, de.kruskal_us);
    printf("Sorted edges examined: %d / %d\n", de.examined, DENSE_EDGES);
    print_mst_edges(&de.kruskal_mst);
    printf("\n");

    print_case(3, "Sparse", "Matrix", "Prim", SPARSE_EDGES, &sp.prim_mst,
               sp.matrix_mem + sp.prim_aux_mem + sp.mst_mem, sp.prim_us);
    printf("Start vertex: %d\n", sp.start);
    print_mst_edges(&sp.prim_mst);
    printf("\n");

    print_case(4, "Dense", "Matrix", "Prim", DENSE_EDGES, &de.prim_mst,
               de.matrix_mem + de.prim_aux_mem + de.mst_mem, de.prim_us);
    printf("Start vertex: %d\n", de.start);
    print_mst_edges(&de.prim_mst);
    printf("\n");

    printf("MST cost check\n");
    printf("Sparse Graph : Kruskal %d, Prim %d -> %s\n", sp.kruskal_mst.cost, sp.prim_mst.cost,
           sp.kruskal_mst.cost == sp.prim_mst.cost ? "Same" : "Different");
    printf("Dense Graph  : Kruskal %d, Prim %d -> %s\n\n", de.kruskal_mst.cost, de.prim_mst.cost,
           de.kruskal_mst.cost == de.prim_mst.cost ? "Same" : "Different");

    printf("Memory breakdown (Bytes)\n");
    printf("sizeof(Node) = %d, sizeof(Edge) = %d\n", (int)sizeof(Node), (int)sizeof(Edge));
    printf("                         Sparse     Dense\n");
    printf("Adjacency list         : %6ld    %6ld\n", sp.list_mem, de.list_mem);
    printf("Kruskal edges + UF     : %6ld    %6ld\n", sp.kruskal_aux_mem, de.kruskal_aux_mem);
    printf("MST result             : %6ld    %6ld\n", sp.mst_mem, de.mst_mem);
    printf("Adjacency matrix       : %6ld    %6ld\n", sp.matrix_mem, de.matrix_mem);
    printf("Prim arrays            : %6ld    %6ld\n", sp.prim_aux_mem, de.prim_aux_mem);
    printf("MST result             : %6ld    %6ld\n", sp.mst_mem, de.mst_mem);

    return 0;
}
