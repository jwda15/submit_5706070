/*
 * 과제 15: 퀵 정렬과 AVL 트리 정렬의 성능 비교
 * 빌드: cl /O2 /utf-8 hw15.c          (gcc: gcc -O2 -o hw15 hw15.c)
 * 실행: hw15
 *
 * 분석용 통계(정렬 단계 / 삽입·삭제 단계 분리, 트리 높이, 회전 횟수 등)까지 보려면:
 *   cl /O2 /utf-8 /DANALYSIS /Fe:hw15_analysis.exe hw15.c
 *   (기본 빌드의 출력은 과제 형식 그대로)
 *
 * rand()의 구현은 C 라이브러리마다 달라서, 같은 시드라도 Windows(MSVC)와
 * Linux(glibc)에서는 다른 데이터가 만들어진다. (README의 결과는 Windows, MSVC 기준)
 */
#ifndef ALGO_BODY   /* ============ 공통 부분 (한 번만 컴파일) ============ */

#ifndef _WIN32
#define _POSIX_C_SOURCE 199309L   /* gcc -std=c99 등에서도 clock_gettime을 쓰기 위함 */
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>              /* MSVC에는 clock_gettime이 없어서 QueryPerformanceCounter 사용 */
#endif

#define NUM_SIZES 6
#define REPEAT    10          /* 실행시간: 같은 조건으로 10회 측정 후 평균 */
#ifndef SEED
#define SEED      20261109u   /* 이전 과제와 같은 고정 시드 -> 결과 재현 가능. -DSEED=3u 처럼 바꿀 수 있음 */
#endif

/* =====================================================================
 * 측정 도구
 * ---------------------------------------------------------------------
 * 알고리즘 코드(파일 아래쪽 ALGO_BODY 부분)는 한 벌만 있고, 이 파일이 자기 자신을
 * 두 번 #include 해서 두 가지 버전으로 컴파일된다. (과제 14와 같은 방식)
 *   - 측정용 버전 (함수 이름 뒤에 _m, TRACK = 1): 비교/이동/메모리를 기록
 *   - 시간용 버전 (원래 이름,        TRACK = 0): 측정 코드가 컴파일 단계에서 완전히 빠짐
 * 실행시간은 측정 코드가 전혀 없는 시간용 버전으로 잰다.
 * ===================================================================== */
long long g_cmp  = 0;   /* 데이터 값 비교 횟수 */
long long g_move = 0;   /* 데이터 값 복사/대입 횟수 */

/*
 * 메모리: 동적 할당(배열, 노드)과 함수 스택 프레임을 시작할 때 더하고 끝날 때 뺀다.
 *         실행 중 가장 컸던 값(peak)이 최대 메모리 사용량이다.
 * 스택 프레임 = 매개변수 + 지역 변수 + 호출 비용(복귀 주소 + 이전 프레임 포인터)
 * (64비트 기준: int 4B, 포인터 8B, 호출 비용 16B)
 */
size_t g_mem_cur = 0, g_mem_peak = 0;

#define CALL_OVERHEAD (2 * sizeof(void *))

/* 배열 + 퀵 정렬 쪽 함수 프레임 */
#define FRAME_QUICK_SORT  (sizeof(int *) + sizeof(int)                         + CALL_OVERHEAD) /* a,n                       */
#define FRAME_QUICK_REC   (sizeof(int *) + 2 * sizeof(int) + sizeof(int)       + CALL_OVERHEAD) /* a,low,high | p            */
#define FRAME_PARTITION   (sizeof(int *) + 2 * sizeof(int) + 4 * sizeof(int)   + CALL_OVERHEAD) /* a,low,high | p,pivot,i,j  */
#define FRAME_MEDIAN3     (sizeof(int *) + 2 * sizeof(int) + sizeof(int)       + CALL_OVERHEAD) /* a,low,high | mid          */
#define FRAME_SWAP        (sizeof(int *) + 2 * sizeof(int) + sizeof(int)       + CALL_OVERHEAD) /* a,i,j | t                 */
#define FRAME_ARR_INSERT  (2 * sizeof(int *) + sizeof(int) + 2 * sizeof(int)   + CALL_OVERHEAD) /* a,size*,v | i,j           */
#define FRAME_ARR_DELETE  (2 * sizeof(int *) + sizeof(int) + 2 * sizeof(int)   + CALL_OVERHEAD) /* a,size*,v | i,j           */

/* AVL 트리 쪽 함수 프레임 */
#define FRAME_AVL_INSERT  (sizeof(void *) + sizeof(int)                        + CALL_OVERHEAD) /* t,v                       */
#define FRAME_AVL_DELETE  (sizeof(void *) + sizeof(int) + sizeof(int *) + 2 * sizeof(void *) + CALL_OVERHEAD) /* t,v,done | child,s */
#define FRAME_NEW_NODE    (sizeof(int) + sizeof(void *)                        + CALL_OVERHEAD) /* v | node                  */
#define FRAME_REMOVE_MIN  (sizeof(void *) + sizeof(void *)                     + CALL_OVERHEAD) /* t | r                     */
#define FRAME_REBALANCE   (sizeof(void *) + sizeof(int)                        + CALL_OVERHEAD) /* t | bf                    */
#define FRAME_ROTATE      (sizeof(void *) + sizeof(void *)                     + CALL_OVERHEAD) /* x | y                     */

#ifdef ANALYSIS
long long a_rotations = 0;   /* AVL 회전 횟수 */
#endif

/* =====================================================================
 * [2] AVL 트리 (중복 값은 노드의 count 로 저장)
 * ===================================================================== */
typedef struct Node {
    int key;                /* 데이터 값 */
    int count;              /* 같은 값의 개수 */
    int height;             /* 높이 (잎 노드 = 1) */
    struct Node *left, *right;
} Node;

/* 높이/균형 관련 작은 계산은 매크로로 처리 (함수 호출 없음, 데이터 비교 아님) */
#define H(t)       ((t) ? (t)->height : 0)
#define MAXI(a, b) ((a) > (b) ? (a) : (b))
#define UPDATE_HEIGHT(t) ((t)->height = MAXI(H((t)->left), H((t)->right)) + 1)
#define BALANCE(t) (H((t)->left) - H((t)->right))

void avl_free(Node *t)
{
    if (t == NULL) return;
    avl_free(t->left);
    avl_free(t->right);
    free(t);
}

/* ===== 검증용 (측정 제외) ===== */

/* 중위 순회로 out[] 에 오름차순으로 펼침 (count 만큼 반복) */
void inorder(const Node *t, int out[], int *k)
{
    int c;
    if (t == NULL) return;
    inorder(t->left, out, k);
    for (c = 0; c < t->count; c++)
        out[(*k)++] = t->key;
    inorder(t->right, out, k);
}

/* AVL 속성(모든 노드의 균형 인수 -1~1, 높이 정확) 확인. 반환: 높이, 실패 시 -1 */
int check_avl(const Node *t)
{
    int lh, rh;
    if (t == NULL) return 0;
    lh = check_avl(t->left);
    rh = check_avl(t->right);
    if (lh < 0 || rh < 0 || lh - rh > 1 || rh - lh > 1 || t->height != MAXI(lh, rh) + 1)
        return -1;
    return MAXI(lh, rh) + 1;
}

int count_nodes(const Node *t)
{
    return t ? 1 + count_nodes(t->left) + count_nodes(t->right) : 0;
}

/* =====================================================================
 * 실험
 * ===================================================================== */
enum { OP_INSERT, OP_DELETE };

typedef struct {
    long long cmp, move;
    size_t mem;
    double ms;
    int final_count;
#ifdef ANALYSIS
    long long cmp_build, move_build;   /* 정렬(구축) 단계 */
    int inserts, deletes, skipped;
#endif
} Result;

double now_ms(void)
{
#ifdef _WIN32
    static LARGE_INTEGER freq;
    LARGE_INTEGER t;

    if (freq.QuadPart == 0)
        QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&t);
    return (double)t.QuadPart * 1000.0 / (double)freq.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
#endif
}

/*
 * 0 ~ 2^30 - 1 범위의 난수 (이전 과제 11 ~ 14와 같은 방법).
 * MSVC의 rand()는 0 ~ 32767만 만들어서 15비트씩 두 번 뽑아 이어 붙인다.
 */
int random_value(void)
{
    unsigned int hi = (unsigned int)rand() & 0x7FFF;
    unsigned int lo = (unsigned int)rand() & 0x7FFF;
    return (int)((hi << 15) | lo);
}

/* =====================================================================
 * 알고리즘을 두 버전으로 컴파일
 * ===================================================================== */
#define ALGO_BODY

#define TRACK 1               /* 측정용 버전: run_array_m, run_avl_m, ... */
#define F(name) name##_m
#include __FILE__
#undef TRACK
#undef F

#define TRACK 0               /* 시간용 버전: run_array, run_avl, ... (측정 코드 없음) */
#define F(name) name
#include __FILE__
#undef TRACK
#undef F

#undef ALGO_BODY

void fmt_cell(char *buf, size_t len, const char *fmt_num, double v, const char *unit)
{
    char num[32];
    snprintf(num, sizeof(num), fmt_num, v);
    snprintf(buf, len, "%s%s", num, unit);
}

int main(void)
{
    int sizes[NUM_SIZES] = {1000, 2000, 4000, 8000, 16000, 20000};
    int *data[NUM_SIZES], *op[NUM_SIZES], *val[NUM_SIZES];
    int s, i, r, n, ops;
    int all_ok = 1;

    /*
     * 난수 생성 (측정 제외): 모든 데이터와 연산 순서를 실험 전에 미리 만든다.
     * - 데이터: 이전 과제와 같은 random_value()로 만들되 % n -> 값 범위 0 ~ n-1
     *          (중복 허용. 범위가 2^30이면 삭제할 값이 데이터에 있을 확률이 거의 0이라
     *           삭제가 일어나지 않으므로, 실제로 삭제가 일어나도록 범위를 n으로 맞춤)
     * - 연산: 임의의 정수를 생성해 짝수면 삽입, 홀수면 삭제
     * - 값  : 연산 종류와 별도로 새 난수를 생성 (같은 범위 0 ~ n-1)
     * 두 자료구조는 같은 배열(op, val)을 그대로 사용 -> 같은 연산 순서와 같은 값
     */
    srand(SEED);
    for (s = 0; s < NUM_SIZES; s++) {
        n = sizes[s];
        ops = n / 5;                                 /* n 의 20% */
        data[s] = (int *)malloc(sizeof(int) * n);
        op[s]   = (int *)malloc(sizeof(int) * ops);
        val[s]  = (int *)malloc(sizeof(int) * ops);
        if (!data[s] || !op[s] || !val[s]) {
            fprintf(stderr, "Memory allocation failed\n");
            return 1;
        }
        for (i = 0; i < n; i++)
            data[s][i] = random_value() % n;
        for (i = 0; i < ops; i++) {
            op[s][i]  = (random_value() % 2 == 0) ? OP_INSERT : OP_DELETE;
            val[s][i] = random_value() % n;
        }
    }

    for (s = 0; s < NUM_SIZES; s++) {
        Result qa, av;
        int qsize, asize, k, ok;
        int *arr, *flat;
        Node *root;
        double total, t0;
        char c1[96], c2[96];

        n = sizes[s];
        ops = n / 5;

        /* ---------- (1) 측정 실행: 비교, 이동, 메모리 ---------- */
        g_cmp = g_move = 0; g_mem_cur = g_mem_peak = 0;
        arr = run_array_m(data[s], n, op[s], val[s], ops, &qsize, &qa);
        qa.cmp = g_cmp; qa.move = g_move; qa.mem = g_mem_peak; qa.final_count = qsize;

        g_cmp = g_move = 0; g_mem_cur = g_mem_peak = 0;
#ifdef ANALYSIS
        a_rotations = 0;
#endif
        root = run_avl_m(data[s], n, op[s], val[s], ops, &asize, &av);
        av.cmp = g_cmp; av.move = g_move; av.mem = g_mem_peak; av.final_count = asize;

        /* ---------- 검증 (측정 제외) ----------
         * 배열이 오름차순인지, AVL 속성이 유지되는지,
         * AVL 중위 순회 결과가 배열과 원소 하나하나까지 같은지 확인 */
        ok = (qsize == asize) && (check_avl(root) >= 0);
        for (i = 1; ok && i < qsize; i++)
            if (arr[i - 1] > arr[i]) ok = 0;
        flat = (int *)malloc(sizeof(int) * (asize > 0 ? asize : 1));
        k = 0;
        inorder(root, flat, &k);
        if (k != asize) ok = 0;
        for (i = 0; ok && i < qsize; i++)
            if (arr[i] != flat[i]) ok = 0;
        if (!ok) all_ok = 0;

#ifdef ANALYSIS
        {
            int nodes = count_nodes(root), height = check_avl(root);
#endif
        free(flat);
        free(arr);
#ifdef ANALYSIS
            printf("  [analysis] n=%d ops=%d | insert %d, delete %d, skipped %d\n",
                   n, ops, qa.inserts, qa.deletes, qa.skipped);
            printf("  [analysis] Array build(load+sort): cmp %lld, move %lld | ops: cmp %lld, move %lld\n",
                   qa.cmp_build, qa.move_build, qa.cmp - qa.cmp_build, qa.move - qa.move_build);
            printf("  [analysis] AVL   build(insert n) : cmp %lld, move %lld | ops: cmp %lld, move %lld\n",
                   av.cmp_build, av.move_build, av.cmp - av.cmp_build, av.move - av.move_build);
            printf("  [analysis] AVL final nodes %d (distinct values), height %d, rotations %lld\n",
                   nodes, height, a_rotations);
        }
#endif
        avl_free(root);

        /* ---------- (2) 시간 실행: 측정 코드가 없는 시간용 버전으로 REPEAT 회 ----------
         * 측정 구간: 자료구조 생성(배열 할당/적재, 노드 삽입)부터 모든 삽입·삭제 완료까지
         * 해제(free)는 측정 구간 밖 */
        total = 0.0;
        for (r = 0; r < REPEAT; r++) {
            t0 = now_ms();
            arr = run_array(data[s], n, op[s], val[s], ops, &qsize, &qa);
            total += now_ms() - t0;
            free(arr);
        }
        qa.ms = total / REPEAT;

        total = 0.0;
        for (r = 0; r < REPEAT; r++) {
            t0 = now_ms();
            root = run_avl(data[s], n, op[s], val[s], ops, &asize, &av);
            total += now_ms() - t0;
            avl_free(root);
        }
        av.ms = total / REPEAT;

        /* ---------- 출력 ---------- */
        printf("Data Size : %d\n", n);
        printf("Operations: %d\n\n", ops);
        printf("                  Quick Sort      AVL Tree\n");
        printf("------------------------------------------------\n");
        fmt_cell(c1, sizeof(c1), "%.4f", qa.ms, " ms");
        fmt_cell(c2, sizeof(c2), "%.4f", av.ms, " ms");
        printf("Total Time        %-16s%s\n", c1, c2);
        printf("Comparisons       %-16lld%lld\n", qa.cmp, av.cmp);
        printf("Data Movements    %-16lld%lld\n", qa.move, av.move);
        fmt_cell(c1, sizeof(c1), "%.0f", (double)qa.mem, " Bytes");
        fmt_cell(c2, sizeof(c2), "%.0f", (double)av.mem, " Bytes");
        printf("Memory Usage      %-16s%s\n\n", c1, c2);
        printf("Final Data Count  %-16d%d\n", qa.final_count, av.final_count);
        printf("Data Check        %s\n\n",
               ok ? "Same data (sorted array == AVL inorder)" : "MISMATCH");
    }

    printf("All checks : %s\n", all_ok ? "PASS" : "FAIL");

    for (s = 0; s < NUM_SIZES; s++) {
        free(data[s]); free(op[s]); free(val[s]);
    }
    return 0;
}

#else   /* ============ ALGO_BODY: 알고리즘 (두 버전으로 컴파일됨) ============ */

#undef COUNT_CMP
#undef COUNT_MOVE
#undef MEM_ADD
#undef MEM_SUB
#undef COUNT_ROT
#if TRACK
#define COUNT_CMP()  (g_cmp++)
#define COUNT_MOVE() (g_move++)
#define MEM_ADD(b)   do { g_mem_cur += (b); if (g_mem_cur > g_mem_peak) g_mem_peak = g_mem_cur; } while (0)
#define MEM_SUB(b)   (g_mem_cur -= (b))
#ifdef ANALYSIS
#define COUNT_ROT()  (a_rotations++)
#else
#define COUNT_ROT()  ((void)0)
#endif
#else                   /* 시간용 버전: 측정 코드 없음 */
#define COUNT_CMP()  ((void)0)
#define COUNT_MOVE() ((void)0)
#define MEM_ADD(b)   ((void)0)
#define MEM_SUB(b)   ((void)0)
#define COUNT_ROT()  ((void)0)
#endif

/* =====================================================================
 * [1] 배열 + 퀵 정렬 (Median-of-Three 피벗, 과제 12/14 와 같은 분할)
 * ===================================================================== */
void F(swap)(int a[], int i, int j)
{
    int t;
    MEM_ADD(FRAME_SWAP);
    t = a[i];    COUNT_MOVE();
    a[i] = a[j]; COUNT_MOVE();
    a[j] = t;    COUNT_MOVE();
    MEM_SUB(FRAME_SWAP);
}

/* 첫 번째, 가운데, 마지막 원소 중 중앙값의 위치 반환 (값 비교 2~3회) */
int F(median_of_three)(int a[], int low, int high)
{
    int mid = (low + high) / 2;
    int result;
    MEM_ADD(FRAME_MEDIAN3);

    COUNT_CMP();
    if (a[low] < a[mid]) {
        COUNT_CMP();
        if (a[mid] < a[high]) result = mid;
        else { COUNT_CMP(); result = (a[low] < a[high]) ? high : low; }
    } else {
        COUNT_CMP();
        if (a[low] < a[high]) result = low;
        else { COUNT_CMP(); result = (a[mid] < a[high]) ? high : mid; }
    }

    MEM_SUB(FRAME_MEDIAN3);
    return result;
}

int F(partition)(int a[], int low, int high)
{
    int p, pivot, i, j;
    MEM_ADD(FRAME_PARTITION);

    p = F(median_of_three)(a, low, high);
    F(swap)(a, low, p);                                 /* 피벗을 맨 앞으로 */
    pivot = a[low]; COUNT_MOVE();                    /* 피벗 값 복사 */
    i = low;
    j = high + 1;

    while (1) {
        while (1) {
            i++;
            if (i > high) break;                     /* 인덱스 검사: 미포함 */
            COUNT_CMP();
            if (!(a[i] < pivot)) break;
        }
        while (1) {
            j--;
            COUNT_CMP();
            if (!(a[j] > pivot)) break;
        }
        if (i >= j) break;
        F(swap)(a, i, j);
    }
    F(swap)(a, low, j);                                 /* 피벗을 제자리로 */

    MEM_SUB(FRAME_PARTITION);
    return j;
}

void F(quick_sort_rec)(int a[], int low, int high)
{
    int p;
    MEM_ADD(FRAME_QUICK_REC);
    if (low < high) {
        p = F(partition)(a, low, high);
        F(quick_sort_rec)(a, low, p - 1);
        F(quick_sort_rec)(a, p + 1, high);
    }
    MEM_SUB(FRAME_QUICK_REC);
}

void F(quick_sort)(int a[], int n)
{
    MEM_ADD(FRAME_QUICK_SORT);
    F(quick_sort_rec)(a, 0, n - 1);
    MEM_SUB(FRAME_QUICK_SORT);
}

/* 정렬된 배열에 삽입: 삽입 위치를 앞에서부터 선형 탐색 -> 뒤 데이터를 한 칸씩 뒤로 이동 */
void F(array_insert)(int a[], int *size, int v)
{
    int i, j;
    MEM_ADD(FRAME_ARR_INSERT);

    for (i = 0; i < *size; i++) {                    /* v 보다 큰 첫 위치 (같은 값 뒤에 삽입) */
        COUNT_CMP();
        if (a[i] > v) break;
    }
    for (j = *size; j > i; j--) {                    /* 뒤로 이동 */
        a[j] = a[j - 1]; COUNT_MOVE();
    }
    a[i] = v; COUNT_MOVE();
    (*size)++;

    MEM_SUB(FRAME_ARR_INSERT);
}

/* 정렬된 배열에서 삭제: v 를 앞에서부터 선형 탐색 -> 하나 제거하고 뒤 데이터를 앞으로 이동
 * 반환값: 1 = 삭제함, 0 = 값이 없어 삭제하지 않음 */
int F(array_delete)(int a[], int *size, int v)
{
    int i, j, found = 0;
    MEM_ADD(FRAME_ARR_DELETE);

    for (i = 0; i < *size; i++) {                    /* v 이상인 첫 위치 (정렬돼 있으니 넘어가면 중단) */
        COUNT_CMP();
        if (a[i] >= v) break;
    }
    if (i < *size) {
        COUNT_CMP();
        if (a[i] == v) found = 1;
    }
    if (found) {
        for (j = i; j < *size - 1; j++) {            /* 앞으로 이동 */
            a[j] = a[j + 1]; COUNT_MOVE();
        }
        (*size)--;
    }

    MEM_SUB(FRAME_ARR_DELETE);
    return found;
}

Node *F(new_node)(int v)
{
    Node *node;
    MEM_ADD(FRAME_NEW_NODE);

    node = (Node *)malloc(sizeof(Node));
    if (node == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }
    MEM_ADD(sizeof(Node));                           /* 노드 메모리 */
    node->key = v; COUNT_MOVE();                     /* 데이터 값 대입 */
    node->count = 1;
    node->height = 1;
    node->left = node->right = NULL;

    MEM_SUB(FRAME_NEW_NODE);
    return node;
}

/* 회전: 포인터 연결만 바뀜 -> 데이터 이동에 포함하지 않음 */
Node *F(rotate_right)(Node *x)
{
    Node *y;
    MEM_ADD(FRAME_ROTATE);
    COUNT_ROT();
    y = x->left;
    x->left = y->right;
    y->right = x;
    UPDATE_HEIGHT(x);
    UPDATE_HEIGHT(y);
    MEM_SUB(FRAME_ROTATE);
    return y;
}

Node *F(rotate_left)(Node *x)
{
    Node *y;
    MEM_ADD(FRAME_ROTATE);
    COUNT_ROT();
    y = x->right;
    x->right = y->left;
    y->left = x;
    UPDATE_HEIGHT(x);
    UPDATE_HEIGHT(y);
    MEM_SUB(FRAME_ROTATE);
    return y;
}

/* 높이 갱신 후 균형 인수가 ±2 이면 회전 (균형 인수 비교는 제어 비교 -> 미포함) */
Node *F(rebalance)(Node *t)
{
    int bf;
    MEM_ADD(FRAME_REBALANCE);

    UPDATE_HEIGHT(t);
    bf = BALANCE(t);
    if (bf > 1) {                                    /* 왼쪽이 무거움 */
        if (BALANCE(t->left) < 0)                    /* LR */
            t->left = F(rotate_left)(t->left);
        t = F(rotate_right)(t);                         /* LL */
    } else if (bf < -1) {                            /* 오른쪽이 무거움 */
        if (BALANCE(t->right) > 0)                   /* RL */
            t->right = F(rotate_right)(t->right);
        t = F(rotate_left)(t);                          /* RR */
    }

    MEM_SUB(FRAME_REBALANCE);
    return t;
}

Node *F(avl_insert)(Node *t, int v)
{
    MEM_ADD(FRAME_AVL_INSERT);

    if (t == NULL) {
        t = F(new_node)(v);
        MEM_SUB(FRAME_AVL_INSERT);
        return t;
    }
    COUNT_CMP();
    if (v < t->key) {
        t->left = F(avl_insert)(t->left, v);
    } else {
        COUNT_CMP();
        if (v > t->key) {
            t->right = F(avl_insert)(t->right, v);
        } else {
            t->count++;                              /* 중복: 개수만 증가 (구조 변화 없음) */
            MEM_SUB(FRAME_AVL_INSERT);
            return t;
        }
    }
    t = F(rebalance)(t);

    MEM_SUB(FRAME_AVL_INSERT);
    return t;
}

/* 서브트리의 최솟값 노드를 떼어냄 (왼쪽 끝으로 이동만 하므로 데이터 비교 없음) */
Node *F(remove_min)(Node *t)
{
    Node *r;
    MEM_ADD(FRAME_REMOVE_MIN);

    if (t->left == NULL) {
        r = t->right;
        free(t);
        MEM_SUB(sizeof(Node));
        MEM_SUB(FRAME_REMOVE_MIN);
        return r;
    }
    t->left = F(remove_min)(t->left);
    t = F(rebalance)(t);

    MEM_SUB(FRAME_REMOVE_MIN);
    return t;
}

/* v 를 하나 삭제. 찾으면 *done = 1, 없으면 그대로 */
Node *F(avl_delete)(Node *t, int v, int *done)
{
    Node *child, *s;
    MEM_ADD(FRAME_AVL_DELETE);

    if (t == NULL) {                                 /* 값 없음 */
        MEM_SUB(FRAME_AVL_DELETE);
        return NULL;
    }
    COUNT_CMP();
    if (v < t->key) {
        t->left = F(avl_delete)(t->left, v, done);
    } else {
        COUNT_CMP();
        if (v > t->key) {
            t->right = F(avl_delete)(t->right, v, done);
        } else {
            *done = 1;
            if (t->count > 1) {                      /* 중복이 남아 있으면 개수만 감소 */
                t->count--;
                MEM_SUB(FRAME_AVL_DELETE);
                return t;
            }
            if (t->left == NULL || t->right == NULL) {   /* 자식 0~1개: 자식으로 대체 */
                child = t->left ? t->left : t->right;
                free(t);
                MEM_SUB(sizeof(Node));
                MEM_SUB(FRAME_AVL_DELETE);
                return child;
            }
            /* 자식 2개: 오른쪽 서브트리의 최솟값(후속자)을 가져오고 후속자 노드 제거 */
            s = t->right;
            while (s->left != NULL)
                s = s->left;
            t->key = s->key; COUNT_MOVE();           /* 데이터 값 복사 */
            t->count = s->count;
            t->right = F(remove_min)(t->right);
        }
    }
    t = F(rebalance)(t);

    MEM_SUB(FRAME_AVL_DELETE);
    return t;
}

/*
 * 배열 방법 전체 과정: 배열 할당 -> 원본 적재 -> 퀵 정렬 -> 삽입/삭제 반복
 * 배열 용량은 모든 연산이 삽입일 경우까지 고려해 n + ops 로 한 번에 잡는다.
 * 반환된 배열(정렬 결과)은 호출한 쪽에서 검증 후 해제한다.
 */
int *F(run_array)(const int src[], int n, const int op[], const int val[], int ops,
               int *out_size, Result *res)
{
    int cap = n + ops, size = n, i;
    int *a = (int *)malloc(sizeof(int) * cap);
    if (a == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }
    MEM_ADD(sizeof(int) * cap);

    for (i = 0; i < n; i++) {                        /* 원본을 배열에 적재 */
        a[i] = src[i]; COUNT_MOVE();
    }
    F(quick_sort)(a, n);
#ifdef ANALYSIS
    res->cmp_build = g_cmp; res->move_build = g_move;
    res->inserts = res->deletes = res->skipped = 0;
#endif

    for (i = 0; i < ops; i++) {
        if (op[i] == OP_INSERT) {
            F(array_insert)(a, &size, val[i]);
#ifdef ANALYSIS
            res->inserts++;
#endif
        } else {
#ifdef ANALYSIS
            if (F(array_delete)(a, &size, val[i])) res->deletes++; else res->skipped++;
#else
            F(array_delete)(a, &size, val[i]);
#endif
        }
    }
    *out_size = size;
    (void)res;
    return a;
}

/* AVL 방법 전체 과정: 원본을 하나씩 삽입(정렬 상태 유지) -> 삽입/삭제 반복 */
Node *F(run_avl)(const int src[], int n, const int op[], const int val[], int ops,
              int *out_size, Result *res)
{
    Node *root = NULL;
    int size = n, i, done;

    for (i = 0; i < n; i++)
        root = F(avl_insert)(root, src[i]);
#ifdef ANALYSIS
    res->cmp_build = g_cmp; res->move_build = g_move;
    res->inserts = res->deletes = res->skipped = 0;
#endif

    for (i = 0; i < ops; i++) {
        if (op[i] == OP_INSERT) {
            root = F(avl_insert)(root, val[i]);
            size++;
#ifdef ANALYSIS
            res->inserts++;
#endif
        } else {
            done = 0;
            root = F(avl_delete)(root, val[i], &done);
            if (done) size--;
#ifdef ANALYSIS
            if (done) res->deletes++; else res->skipped++;
#endif
        }
    }
    *out_size = size;
    (void)res;
    return root;
}

#endif  /* ALGO_BODY */
