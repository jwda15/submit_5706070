/*
 * 과제 12: 퀵 정렬의 피벗 선택에 따른 성능 비교 (+ 보너스: 재귀 vs 반복)
 * 빌드: gcc -O2 -o hw12 hw12.c          (Visual Studio: cl /O2 hw12.c)
 * 실행: ./hw12
 *
 * 분석용 통계까지 보려면: gcc -O2 -DANALYSIS -o hw12_analysis hw12.c && ./hw12_analysis
 *   (기본 빌드의 출력은 과제 형식 그대로이고, 비교 횟수는 두 빌드가 같음)
 *
 * rand()의 구현은 C 라이브러리마다 달라서, 같은 시드라도 Windows(MSVC, MinGW)와
 * Linux(glibc)에서는 다른 데이터가 만들어지고 비교 횟수도 달라진다.
 */
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

#define NUM_SIZES   6
#define BONUS_SIZES 7
#define REPEAT      100     /* 보너스 실행시간: 같은 조건으로 100회 측정 후 평균 */
#define MAX_N       20000   /* 실험하는 최대 데이터 크기 */
#ifndef SEED                    /* 고정 시드: 같은 C 라이브러리(rand 구현)에서는 실행할 때마다 같은 데이터 */
#define SEED        20261109u   /* 다른 데이터로 실험하려면 -DSEED=3u 처럼 빌드 옵션으로 바꿈 */
#endif

enum { PIVOT_FIRST, PIVOT_RANDOM, PIVOT_MEDIAN3 };

/*
 * 비교 횟수 규칙
 * - 분할에서 데이터 값과 피벗 값을 비교할 때마다 1회
 * - Median-of-Three 에서 중앙값을 정하려고 값끼리 비교할 때마다 1회
 * - 인덱스/종료 조건 검사, 무작위 위치 생성, 데이터 이동(swap)은 미포함
 */
long long g_cmp = 0;

#ifdef ANALYSIS
/* 분석용 통계 (비교 횟수에는 영향 없음) */
long long a_med_cmp = 0;    /* 중앙값 결정에 쓴 비교 횟수 */
long long a_parts = 0;      /* 분할 횟수 */
double    a_bigger = 0.0;   /* 분할 후 큰 쪽 구간이 차지하는 비율의 합 */
int       a_depth = 0, a_max_depth = 0;   /* 재귀 깊이 */
#endif

void swap(int a[], int i, int j)
{
    int t = a[i];
    a[i] = a[j];
    a[j] = t;
}

/* 첫 번째, 가운데, 마지막 원소 중 중앙값의 "위치"를 반환 */
int median_of_three(int a[], int low, int high)
{
    int mid = (low + high) / 2;
    int x = a[low], y = a[mid], z = a[high];

    g_cmp++;
    if (x < y) {
        g_cmp++;
        if (y < z) return mid;      /* x < y < z        -> y */
        g_cmp++;
        if (x < z) return high;     /* x < z <= y       -> z */
        return low;                 /* z <= x < y       -> x */
    } else {
        g_cmp++;
        if (x < z) return low;      /* y <= x < z       -> x */
        g_cmp++;
        if (y < z) return high;     /* y < z <= x       -> z */
        return mid;                 /* z <= y <= x      -> y */
    }
}

/* 피벗 선택: 이 부분만 방법별로 다름 */
int select_pivot(int a[], int low, int high, int method)
{
    switch (method) {
    case PIVOT_RANDOM:
        return low + rand() % (high - low + 1);   /* 분할마다 새로 선택 */
    case PIVOT_MEDIAN3:
#ifdef ANALYSIS
        {
            long long before = g_cmp;
            int idx = median_of_three(a, low, high);
            a_med_cmp += g_cmp - before;
            return idx;
        }
#else
        return median_of_three(a, low, high);
#endif
    case PIVOT_FIRST:
    default:
        return low;
    }
}

/*
 * 분할: 세 방법 모두 동일
 * 선택된 피벗을 구간 맨 앞으로 옮긴 뒤, 양쪽에서 좁혀 오며 교환
 * (피벗과 같은 값에서도 멈추므로 중복 값이 많아도 한쪽으로 쏠리지 않음)
 */
int partition(int a[], int low, int high, int method)
{
    int p = select_pivot(a, low, high, method);
    int pivot, i, j;

    swap(a, low, p);                /* 데이터 이동: 비교 횟수 미포함 */
    pivot = a[low];
    i = low;
    j = high + 1;

    while (1) {
        /* 왼쪽에서 피벗 이상인 값 찾기 */
        while (1) {
            i++;
            if (i > high) break;            /* 인덱스 검사: 미포함 */
            g_cmp++;                         /* a[i] 와 피벗 비교 1회 */
            if (!(a[i] < pivot)) break;
        }
        /* 오른쪽에서 피벗 이하인 값 찾기 (a[low] == pivot 이라 low 에서 반드시 멈춤) */
        while (1) {
            j--;
            g_cmp++;                         /* a[j] 와 피벗 비교 1회 */
            if (!(a[j] > pivot)) break;
        }
        if (i >= j) break;                   /* 인덱스 비교: 미포함 */
        swap(a, i, j);
    }
    swap(a, low, j);                         /* 피벗을 제자리로 */
#ifdef ANALYSIS
    {
        int left = j - low, right = high - j;
        a_parts++;
        a_bigger += (double)(left > right ? left : right) / (high - low + 1);
    }
#endif
    return j;
}

/* 재귀적 퀵 정렬 */
void quick_sort_recursive(int a[], int low, int high, int method)
{
    int p;
    if (low < high) {
#ifdef ANALYSIS
        if (++a_depth > a_max_depth) a_max_depth = a_depth;
#endif
        p = partition(a, low, high, method);
        quick_sort_recursive(a, low, p - 1, method);
        quick_sort_recursive(a, p + 1, high, method);
#ifdef ANALYSIS
        a_depth--;
#endif
    }
}

/*
 * 반복적 퀵 정렬 (스택 사용)
 * - 스택에는 아직 정렬하지 않은 구간 (low, high) 쌍을 저장
 * - 오른쪽 구간을 먼저 push 해서 왼쪽 구간이 먼저 처리되도록 함
 *   -> 재귀 버전과 같은 순서로 같은 분할이 일어남 (비교 횟수도 동일)
 * - 스택은 미리 잡아 둔 고정 크기 배열을 사용 (malloc 비용이 실행시간에 섞이지 않게)
 *   구간 하나를 꺼내면 최대 2개를 넣으므로, 쌓이는 구간 수는 n 개를 넘지 않음
 */
static int g_stack[2 * (MAX_N + 2)];

void quick_sort_iterative(int a[], int n, int method)
{
    int top = -1;
    int low, high, p;

    g_stack[++top] = 0;
    g_stack[++top] = n - 1;

    while (top >= 0) {
        high = g_stack[top--];          /* pop */
        low  = g_stack[top--];
        if (low < high) {
            p = partition(a, low, high, method);
            g_stack[++top] = p + 1;     /* 오른쪽 구간 push */
            g_stack[++top] = high;
            g_stack[++top] = low;       /* 왼쪽 구간 push (먼저 꺼내짐) */
            g_stack[++top] = p - 1;
        }
    }
}

/* 오름차순 정렬 확인 (측정에 포함하지 않음) */
int is_sorted(const int a[], int n)
{
    int i;
    for (i = 1; i < n; i++)
        if (a[i - 1] > a[i])
            return 0;
    return 1;
}

/*
 * 0 ~ 2^30 - 1 범위의 난수 (이전 쉘 정렬 과제 hw11과 같은 방법).
 * MSVC의 rand()는 0 ~ 32767만 만들어서 15비트씩 두 번 뽑아 이어 붙인다.
 * (RAND_MAX가 더 큰 환경에서도 같은 범위가 된다)
 */
int random_value(void)
{
    unsigned int hi = (unsigned int)rand() & 0x7FFF;
    unsigned int lo = (unsigned int)rand() & 0x7FFF;
    return (int)((hi << 15) | lo);
}

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

int main(void)
{
    int sizes[NUM_SIZES] = {1000, 2000, 4000, 8000, 16000, 20000};
    int bonus_sizes[BONUS_SIZES] = {1000, 2000, 4000, 8000, 10000, 16000, 20000};
    const char *labels[3] = {
        "Quick Sort (First)    ",
        "Quick Sort (Random)   ",
        "Quick Sort (Median-3) "
    };
    int s, m, r, i, n;
    int all_sorted = 1;
    int *original, *work;
    long long counts[3];

    srand(SEED);

    /* ================= 피벗 선택 방법별 비교 횟수 ================= */
    for (s = 0; s < NUM_SIZES; s++) {
        n = sizes[s];
        original = (int *)malloc(sizeof(int) * n);
        work     = (int *)malloc(sizeof(int) * n);
        if (original == NULL || work == NULL) {
            fprintf(stderr, "Memory allocation failed\n");
            return 1;
        }

        /* 원본 데이터 생성 (중복 허용, 측정 제외) */
        for (i = 0; i < n; i++)
            original[i] = random_value();

        for (m = 0; m < 3; m++) {
            memcpy(work, original, sizeof(int) * n);   /* 같은 원본의 복사본 */
            g_cmp = 0;
#ifdef ANALYSIS
            a_med_cmp = 0; a_parts = 0; a_bigger = 0.0; a_depth = 0; a_max_depth = 0;
#endif
            quick_sort_recursive(work, 0, n - 1, m);
            counts[m] = g_cmp;
            if (!is_sorted(work, n)) all_sorted = 0;
#ifdef ANALYSIS
            printf("  [analysis] %s| partitions %6lld | avg bigger part %.3f | "
                   "max depth %3d | median-select cmp %6lld\n",
                   labels[m], a_parts, a_bigger / a_parts, a_max_depth, a_med_cmp);
#endif
        }

        printf("Data Size : %d\n\n", n);
        for (m = 0; m < 3; m++)
            printf("%s: %lld comparisons\n", labels[m], counts[m]);
        printf("\n");

        free(original);
        free(work);
    }

    printf("Sort check : %s\n",
           all_sorted ? "All results are sorted in ascending order"
                      : "ERROR - unsorted result found");

    /* ================= [보너스] 재귀 vs 반복 실행시간 ================= */
    printf("\n========== [Bonus] Recursive vs Iterative ==========\n");
    printf("(average of %d runs, sorting time only)\n\n", REPEAT);

    for (s = 0; s < BONUS_SIZES; s++) {
        double t_rec = 0.0, t_itr = 0.0, t0;
        long long cmp_rec = 0, cmp_itr = 0;
        int ok = 1;

        n = bonus_sizes[s];
        original = (int *)malloc(sizeof(int) * n);
        work     = (int *)malloc(sizeof(int) * n);
        if (original == NULL || work == NULL) {
            fprintf(stderr, "Memory allocation failed\n");
            return 1;
        }
        for (i = 0; i < n; i++)
            original[i] = random_value();

        for (r = 0; r < REPEAT; r++) {
            /* 재귀 */
            memcpy(work, original, sizeof(int) * n);   /* 복사: 측정 제외 */
            g_cmp = 0;
            t0 = now_ms();
            quick_sort_recursive(work, 0, n - 1, PIVOT_MEDIAN3);
            t_rec += now_ms() - t0;
            cmp_rec = g_cmp;
            if (!is_sorted(work, n)) ok = 0;

            /* 반복 */
            memcpy(work, original, sizeof(int) * n);
            g_cmp = 0;
            t0 = now_ms();
            quick_sort_iterative(work, n, PIVOT_MEDIAN3);
            t_itr += now_ms() - t0;
            cmp_itr = g_cmp;
            if (!is_sorted(work, n)) ok = 0;
        }

        printf("Data Size : %d\n", n);
        printf("Pivot     : Median-of-Three\n\n");
        printf("Recursive Quick Sort : %.4f ms\n", t_rec / REPEAT);
        printf("Iterative Quick Sort : %.4f ms\n", t_itr / REPEAT);
        printf("(comparisons: recursive %lld / iterative %lld, sorted: %s)\n\n",
               cmp_rec, cmp_itr, ok ? "Yes" : "No");

        free(original);
        free(work);
    }

    return 0;
}
