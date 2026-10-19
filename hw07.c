/*
 * 과제 07: 인접 행렬과 인접 리스트의 성능 비교
 * 빌드: gcc -o hw07 hw07.c
 * 실행: ./hw07
 *
 * 비교 횟수를 세는 기준 (그래프 자료구조의 원소를 한 번 확인할 때마다 1회)
 *  - 인접 행렬 : matrix[u][v] 칸 하나를 확인할 때마다 1회
 *  - 인접 리스트 : 리스트 노드 하나의 정점 번호를 확인할 때마다 1회
 *
 * 연산별 기준
 *  - 간선 삽입 : 중복 간선인지 확인하는 비용
 *                행렬은 matrix[u][v] 1회, 리스트는 u의 리스트 전체를 확인
 *                (새 노드는 리스트 맨 앞에 넣으므로 삽입 위치를 찾는 비교는 없음)
 *  - 간선 삭제 : 삭제할 위치를 찾는 비용
 *                행렬은 matrix[u][v] 1회 확인 후 두 칸을 0으로 만듦
 *                리스트는 u의 리스트에서 v를, v의 리스트에서 u를 찾아야 하므로 두 탐색의 합
 *  - 연결 확인 : 행렬은 matrix[u][v] 1회, 리스트는 u의 리스트에서 v를 찾을 때까지
 *  - 인접 정점 : 행렬은 matrix[u][0] ~ matrix[u][99] 100회, 리스트는 u의 노드 수만큼
 *
 * 난수 생성용 임시 배열, 실험용 정점 쌍 선택에 쓰는 확인은 비교 횟수와 메모리에 넣지 않는다.
 */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define VERTEX_COUNT 100
#define SPARSE_EDGES 100
#define DENSE_EDGES  4000
#define TRIAL_COUNT  1000   /* 평균을 구하기 위한 추가 반복 횟수 */

/* ---------- 인접 행렬 ---------- */
typedef struct {
    int n;                                  /* 정점 수 */
    int edges;                              /* 간선 수 */
    int matrix[VERTEX_COUNT][VERTEX_COUNT];
} MatrixGraph;

/* ---------- 인접 리스트 ---------- */
typedef struct Node {
    int vertex;
    struct Node *link;
} Node;

typedef struct {
    int n;                                  /* 정점 수 */
    int edges;                              /* 간선 수 */
    int node_count;                         /* 할당된 노드 수 (= 간선 수 * 2) */
    Node *head[VERTEX_COUNT];
} ListGraph;

/* 정수를 4,523 처럼 천 단위 쉼표가 들어간 문자열로 바꾼다 (n >= 0) */
char *comma(long n, char *buf)
{
    char digits[24];
    int len = 0, i, j = 0;

    do {
        digits[len++] = (char)('0' + n % 10);
        n /= 10;
    } while (n > 0);

    for (i = len - 1; i >= 0; i--) {
        buf[j++] = digits[i];
        if (i > 0 && i % 3 == 0)
            buf[j++] = ',';
    }
    buf[j] = '\0';
    return buf;
}

/* ====================== [1] 인접 행렬 ====================== */

void matrix_init(MatrixGraph *g)
{
    int i, j;

    g->n = VERTEX_COUNT;
    g->edges = 0;
    for (i = 0; i < VERTEX_COUNT; i++)
        for (j = 0; j < VERTEX_COUNT; j++)
            g->matrix[i][j] = 0;
}

/* 간선 삽입. 이미 있으면 0, 넣었으면 1 */
int matrix_insert(MatrixGraph *g, int u, int v, int *cmp)
{
    (*cmp)++;
    if (g->matrix[u][v] == 1)
        return 0;
    g->matrix[u][v] = 1;
    g->matrix[v][u] = 1;
    g->edges++;
    return 1;
}

/* 간선 삭제. 없으면 0, 지웠으면 1 */
int matrix_delete(MatrixGraph *g, int u, int v, int *cmp)
{
    (*cmp)++;
    if (g->matrix[u][v] == 0)
        return 0;
    g->matrix[u][v] = 0;
    g->matrix[v][u] = 0;
    g->edges--;
    return 1;
}

int matrix_connected(MatrixGraph *g, int u, int v, int *cmp)
{
    (*cmp)++;
    return g->matrix[u][v] == 1;
}

/* u의 인접 정점을 출력한다 (print가 0이면 비교 횟수만 센다) */
int matrix_adjacent(MatrixGraph *g, int u, int *cmp, int print)
{
    int v, count = 0;

    for (v = 0; v < g->n; v++) {
        (*cmp)++;
        if (g->matrix[u][v] == 1) {
            if (print)
                printf(" %d", v);
            count++;
        }
    }
    return count;
}

long matrix_memory(MatrixGraph *g)
{
    (void)g;
    return (long)sizeof(MatrixGraph);
}

/* ====================== [2] 인접 리스트 ====================== */

void list_init(ListGraph *g)
{
    int i;

    g->n = VERTEX_COUNT;
    g->edges = 0;
    g->node_count = 0;
    for (i = 0; i < VERTEX_COUNT; i++)
        g->head[i] = NULL;
}

/* u의 리스트 맨 앞에 v를 넣는다 */
void list_push(ListGraph *g, int u, int v)
{
    Node *node = (Node *)malloc(sizeof(Node));

    if (node == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }
    node->vertex = v;
    node->link = g->head[u];
    g->head[u] = node;
    g->node_count++;
}

/* u의 리스트에서 v를 찾을 때까지 노드를 차례로 확인한다. 찾으면 1 */
int list_find(ListGraph *g, int u, int v, int *cmp)
{
    Node *p;

    for (p = g->head[u]; p != NULL; p = p->link) {
        (*cmp)++;
        if (p->vertex == v)
            return 1;
    }
    return 0;
}

/* u의 리스트에서 v 노드를 찾아 지운다. 지웠으면 1 */
int list_remove(ListGraph *g, int u, int v, int *cmp)
{
    Node *prev = NULL, *p;

    for (p = g->head[u]; p != NULL; prev = p, p = p->link) {
        (*cmp)++;
        if (p->vertex == v) {
            if (prev == NULL)
                g->head[u] = p->link;
            else
                prev->link = p->link;
            free(p);
            g->node_count--;
            return 1;
        }
    }
    return 0;
}

/* 간선 삽입. u의 리스트에서 중복을 확인한 뒤 양쪽 리스트 맨 앞에 넣는다 */
int list_insert(ListGraph *g, int u, int v, int *cmp)
{
    if (list_find(g, u, v, cmp))
        return 0;
    list_push(g, u, v);
    list_push(g, v, u);
    g->edges++;
    return 1;
}

/* 간선 삭제. u의 리스트에서 v를, v의 리스트에서 u를 찾아 지운다 */
int list_delete(ListGraph *g, int u, int v, int *cmp)
{
    if (!list_remove(g, u, v, cmp))
        return 0;
    list_remove(g, v, u, cmp);
    g->edges--;
    return 1;
}

int list_connected(ListGraph *g, int u, int v, int *cmp)
{
    return list_find(g, u, v, cmp);
}

int list_adjacent(ListGraph *g, int u, int *cmp, int print)
{
    Node *p;
    int count = 0;

    for (p = g->head[u]; p != NULL; p = p->link) {
        (*cmp)++;
        if (print)
            printf(" %d", p->vertex);
        count++;
    }
    return count;
}

/* 관리 구조체(정점 수, 간선 수, 헤드 포인터 배열) + 간선 노드 */
long list_memory(ListGraph *g)
{
    return (long)sizeof(ListGraph) + (long)g->node_count * (long)sizeof(Node);
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
    g->node_count = 0;
    g->edges = 0;
}

/* ====================== [3] 그래프 생성과 실험 ====================== */

/*
 * 가능한 모든 정점 쌍 (u < v) 4,950개를 배열에 넣고 섞은 뒤 앞에서 edge_count개를 고른다.
 * 자기 루프와 중복 간선이 생기지 않는다.
 * 같은 순서로 행렬과 리스트에 넣으므로 두 표현은 같은 그래프가 된다.
 */
void generate_graph(MatrixGraph *mg, ListGraph *lg, int edge_count)
{
    int pair_total = VERTEX_COUNT * (VERTEX_COUNT - 1) / 2;
    int *pu = (int *)malloc(sizeof(int) * pair_total);
    int *pv = (int *)malloc(sizeof(int) * pair_total);
    int i, j, k = 0, tmp;

    if (pu == NULL || pv == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }
    for (i = 0; i < VERTEX_COUNT; i++)
        for (j = i + 1; j < VERTEX_COUNT; j++) {
            pu[k] = i;
            pv[k] = j;
            k++;
        }

    /* Fisher-Yates 셔플 (쌍은 4,950개라 RAND_MAX의 최솟값 32,767보다 작다) */
    for (i = pair_total - 1; i > 0; i--) {
        j = rand() % (i + 1);
        tmp = pu[i]; pu[i] = pu[j]; pu[j] = tmp;
        tmp = pv[i]; pv[i] = pv[j]; pv[j] = tmp;
    }

    matrix_init(mg);
    list_init(lg);
    for (i = 0; i < edge_count; i++) {
        /* 생성 과정은 측정 대상이 아니므로 비교 횟수를 세지 않고 바로 넣는다 */
        mg->matrix[pu[i]][pv[i]] = 1;
        mg->matrix[pv[i]][pu[i]] = 1;
        mg->edges++;
        list_push(lg, pu[i], pv[i]);
        list_push(lg, pv[i], pu[i]);
        lg->edges++;
    }

    free(pu);
    free(pv);
}

/* 실험할 정점 쌍을 고를 때 쓰는 함수. 행렬을 보고 고르며 비교 횟수에는 넣지 않는다 */
void random_pair(int *u, int *v)
{
    *u = rand() % VERTEX_COUNT;
    do {
        *v = rand() % VERTEX_COUNT;
    } while (*v == *u);
}

void pick_absent_edge(MatrixGraph *mg, int *u, int *v)
{
    do {
        random_pair(u, v);
    } while (mg->matrix[*u][*v] == 1);
}

void pick_present_edge(MatrixGraph *mg, int *u, int *v)
{
    do {
        random_pair(u, v);
    } while (mg->matrix[*u][*v] == 0);
}

typedef struct {
    long memory;
    int insert_cmp;
    int delete_cmp;
    int connect_cmp;
    int adjacent_cmp;
} Result;

void print_case(int no, const char *graph, const char *repr, Result *r)
{
    char buf[32];

    printf("Case %d : %s Graph - Adjacency %s\n", no, graph, repr);
    printf("Memory                         : %s Bytes\n", comma(r->memory, buf));
    printf("Edge insertion comparisons     : %d\n", r->insert_cmp);
    printf("Edge deletion comparisons      : %d\n", r->delete_cmp);
    printf("Connectivity check comparisons : %d\n", r->connect_cmp);
    printf("Adjacent vertices comparisons  : %d\n", r->adjacent_cmp);
    printf("\n");
}

/*
 * 한 그래프(희소 또는 밀집)에 대해 행렬과 리스트 두 경우를 측정한다.
 * 모든 연산은 같은 정점 쌍(같은 정점)을 대상으로 한다.
 */
void run_experiment(const char *name, int edge_count, int case_no)
{
    MatrixGraph *mg = (MatrixGraph *)malloc(sizeof(MatrixGraph));
    ListGraph lg;
    Result mr = {0}, lr = {0};
    int iu, iv, du, dv, cu, cv, au;
    int connected, degree;
    long t;
    long sum_m[4] = {0}, sum_l[4] = {0};
    int c;

    if (mg == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }
    generate_graph(mg, &lg, edge_count);

    printf("============================================================\n");
    printf("%s Graph\n", name);
    printf("Vertices : %d\n", VERTEX_COUNT);
    printf("Edges    : %d\n", mg->edges);
    printf("============================================================\n\n");

    /* 메모리는 그래프를 만든 직후에 측정 */
    mr.memory = matrix_memory(mg);
    lr.memory = list_memory(&lg);

    /* 간선 삽입: 그래프에 없는 정점 쌍 */
    pick_absent_edge(mg, &iu, &iv);
    matrix_insert(mg, iu, iv, &mr.insert_cmp);
    list_insert(&lg, iu, iv, &lr.insert_cmp);

    /* 간선 삭제: 현재 그래프에 있는 간선 (방금 넣은 간선 제외) */
    do {
        pick_present_edge(mg, &du, &dv);
    } while ((du == iu && dv == iv) || (du == iv && dv == iu));
    matrix_delete(mg, du, dv, &mr.delete_cmp);
    list_delete(&lg, du, dv, &lr.delete_cmp);

    /* 연결 여부 확인: 임의의 두 정점 */
    random_pair(&cu, &cv);
    connected = matrix_connected(mg, cu, cv, &mr.connect_cmp);
    list_connected(&lg, cu, cv, &lr.connect_cmp);

    /* 인접 정점: 임의의 정점 하나 */
    au = rand() % VERTEX_COUNT;

    printf("Selected operations (same for matrix and list)\n");
    printf("Insert edge        : (%d, %d)\n", iu, iv);
    printf("Delete edge        : (%d, %d)\n", du, dv);
    printf("Connectivity check : (%d, %d) -> %s\n", cu, cv,
           connected ? "Connected" : "Not connected");
    printf("Adjacent vertices  : vertex %d\n\n", au);

    printf("[Matrix] Adjacent vertices of %d :", au);
    degree = matrix_adjacent(mg, au, &mr.adjacent_cmp, 1);
    printf("\n         (%d vertices)\n", degree);
    printf("[List]   Adjacent vertices of %d :", au);
    degree = list_adjacent(&lg, au, &lr.adjacent_cmp, 1);
    printf("\n         (%d vertices)\n\n", degree);

    print_case(case_no, name, "Matrix", &mr);
    print_case(case_no + 1, name, "List", &lr);

    /*
     * 한 번의 측정은 고른 정점에 따라 값이 크게 달라지므로
     * 같은 방식으로 TRIAL_COUNT번 반복한 평균도 함께 출력한다.
     * 매번 간선 하나를 넣고 하나를 지우므로 간선 수는 유지된다.
     */
    for (t = 0; t < TRIAL_COUNT; t++) {
        pick_absent_edge(mg, &iu, &iv);
        c = 0; matrix_insert(mg, iu, iv, &c); sum_m[0] += c;
        c = 0; list_insert(&lg, iu, iv, &c);  sum_l[0] += c;

        pick_present_edge(mg, &du, &dv);
        c = 0; matrix_delete(mg, du, dv, &c); sum_m[1] += c;
        c = 0; list_delete(&lg, du, dv, &c);  sum_l[1] += c;

        random_pair(&cu, &cv);
        c = 0; matrix_connected(mg, cu, cv, &c); sum_m[2] += c;
        c = 0; list_connected(&lg, cu, cv, &c);  sum_l[2] += c;

        au = rand() % VERTEX_COUNT;
        c = 0; matrix_adjacent(mg, au, &c, 0); sum_m[3] += c;
        c = 0; list_adjacent(&lg, au, &c, 0);  sum_l[3] += c;
    }

    printf("Average over %d random operations (%s Graph)\n", TRIAL_COUNT, name);
    printf("                                 Matrix      List\n");
    printf("Edge insertion comparisons     : %6.2f  %8.2f\n",
           (double)sum_m[0] / TRIAL_COUNT, (double)sum_l[0] / TRIAL_COUNT);
    printf("Edge deletion comparisons      : %6.2f  %8.2f\n",
           (double)sum_m[1] / TRIAL_COUNT, (double)sum_l[1] / TRIAL_COUNT);
    printf("Connectivity check comparisons : %6.2f  %8.2f\n",
           (double)sum_m[2] / TRIAL_COUNT, (double)sum_l[2] / TRIAL_COUNT);
    printf("Adjacent vertices comparisons  : %6.2f  %8.2f\n\n",
           (double)sum_m[3] / TRIAL_COUNT, (double)sum_l[3] / TRIAL_COUNT);

    list_free(&lg);
    free(mg);
}

int main(void)
{
    srand((unsigned)time(NULL));

    printf("sizeof(int) = %d, sizeof(Node *) = %d, sizeof(Node) = %d\n",
           (int)sizeof(int), (int)sizeof(Node *), (int)sizeof(Node));
    printf("sizeof(MatrixGraph) = %d, sizeof(ListGraph) = %d\n\n",
           (int)sizeof(MatrixGraph), (int)sizeof(ListGraph));

    run_experiment("Sparse", SPARSE_EDGES, 1);
    run_experiment("Dense", DENSE_EDGES, 3);

    return 0;
}
