/*
 * 과제 13: 병합 정렬과 퀵 정렬의 성능 비교
 * 빌드: gcc -O2 -o hw13 hw13.c          (Visual Studio: cl /O2 hw13.c)
 * 실행: ./hw13
 *
 * - 데이터는 과제 12(hw12.c)와 같은 방법, 같은 시드, 같은 순서로 만든다.
 *   그래서 같은 컴파일 환경에서는 과제 12와 완전히 같은 데이터가 되고,
 *   퀵 정렬 세 가지의 비교 횟수도 과제 12의 결과와 같게 나온다.
 * - 퀵 정렬의 피벗 선택과 분할은 과제 12의 코드를 그대로 사용한다.
 *
 * rand()의 구현은 C 라이브러리마다 달라서, 같은 시드라도 Windows(MSVC, MinGW)와
 * Linux(glibc)에서는 다른 데이터가 만들어지고 결과도 달라진다.
 */
#ifndef _WIN32
#define _POSIX_C_SOURCE 199309L   /* gcc -std=c99 등에서도 clock_gettime을 쓰기 위함 */
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>              /* MSVC에는 clock_gettime이 없어서 QueryPerformanceCounter 사용 */
#endif

#define NUM_SIZES   6
#define NUM_ALGOS   4
#define REPEAT      100       /* 실행시간: 같은 조건으로 100회 측정 후 평균 */
#ifndef SEED
#define SEED        20261109u /* 과제 12와 같은 시드. -DSEED=3u 처럼 바꿀 수 있음 */
#endif

enum { ALGO_MERGE, ALGO_FIRST, ALGO_RANDOM, ALGO_MEDIAN3 };
enum { PIVOT_FIRST, PIVOT_RANDOM, PIVOT_MEDIAN3 };

/*
 * 비교 횟수 규칙
 * - 병합 정렬: 병합할 때 두 부분의 원소끼리 비교할 때마다 1회
 * - 퀵 정렬  : 분할에서 데이터 값과 피벗을 비교할 때마다 1회,
 *              Median-of-Three에서 중앙값을 정하려고 값끼리 비교할 때마다 1회
 * - 인덱스/종료 조건 검사, 무작위 위치 생성, 데이터 이동은 미포함
 */
long long g_cmp = 0;

/*
 * 데이터 이동 횟수 (README 분석용): 데이터 배열 또는 임시 배열에 원소 값을 쓴 횟수
 * - 병합 정렬: 원소를 임시 배열에 쓸 때 1회, 원래 배열로 되돌릴 때 1회
 * - 퀵 정렬  : swap 한 번에 배열 두 칸을 쓰므로 2회
 */
long long g_moves = 0;

/*
 * 메모리 측정
 * - 힙: 정렬 중에 malloc한 바이트 수 (병합 정렬의 임시 배열)
 * - 스택: 정렬 함수를 호출하기 직전의 스택 위치와, 정렬 중 가장 깊이 내려간 스택 위치의 차이.
 *   재귀 함수와 병합/분할 함수 안의 지역 변수 주소를 확인해서 가장 낮은 주소를 기록한다.
 *   (x86/x64는 스택이 낮은 주소 쪽으로 자란다)
 */
uintptr_t g_stack_base = 0, g_stack_low = 0;
long long g_heap_bytes = 0;
int g_depth = 0, g_max_depth = 0;

#define STACK_PROBE()                                          \
    do {                                                       \
        volatile char probe_;                                  \
        uintptr_t here_ = (uintptr_t)&probe_;                  \
        if (here_ < g_stack_low) g_stack_low = here_;          \
    } while (0)

void swap(int a[], int i, int j)
{
    int t = a[i];
    a[i] = a[j];
    a[j] = t;
    g_moves += 2;
}

/* ====================== 병합 정렬 ====================== */

/* a[low..mid]와 a[mid+1..high]를 tmp에 병합한 뒤 a로 되돌린다 */
void merge(int a[], int tmp[], int low, int mid, int high)
{
    int i = low, j = mid + 1, k = low;

    STACK_PROBE();
    while (i <= mid && j <= high) {      /* 인덱스 검사: 미포함 */
        g_cmp++;                         /* 두 부분의 원소 비교 1회 */
        if (a[i] <= a[j])                /* 같으면 왼쪽 먼저 -> 안정 정렬 */
            tmp[k++] = a[i++];
        else
            tmp[k++] = a[j++];
    }
    while (i <= mid)                     /* 남은 원소는 비교 없이 복사 */
        tmp[k++] = a[i++];
    while (j <= high)
        tmp[k++] = a[j++];

    memcpy(a + low, tmp + low, sizeof(int) * (high - low + 1));
    g_moves += 2LL * (high - low + 1);   /* 임시 배열에 쓰기 + 되돌리기 */
}

void merge_sort_rec(int a[], int tmp[], int low, int high)
{
    int mid;

    STACK_PROBE();
    if (low < high) {
        if (++g_depth > g_max_depth) g_max_depth = g_depth;
        mid = (low + high) / 2;
        merge_sort_rec(a, tmp, low, mid);
        merge_sort_rec(a, tmp, mid + 1, high);
        merge(a, tmp, low, mid, high);
        g_depth--;
    }
}

/* 임시 배열을 한 번만 할당해서 모든 병합에서 같이 쓴다 (할당/해제도 정렬 시간에 포함) */
void merge_sort(int a[], int n)
{
    int *tmp = (int *)malloc(sizeof(int) * n);

    if (tmp == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }
    g_heap_bytes += (long long)sizeof(int) * n;
    merge_sort_rec(a, tmp, 0, n - 1);
    free(tmp);
}

/* ====================== 퀵 정렬 (과제 12와 동일) ====================== */

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
        return median_of_three(a, low, high);
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

    STACK_PROBE();
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
    return j;
}

/* 재귀적 퀵 정렬 */
void quick_sort_rec(int a[], int low, int high, int method)
{
    int p;

    STACK_PROBE();
    if (low < high) {
        if (++g_depth > g_max_depth) g_max_depth = g_depth;
        p = partition(a, low, high, method);
        quick_sort_rec(a, low, p - 1, method);
        quick_sort_rec(a, p + 1, high, method);
        g_depth--;
    }
}

/* ====================== 공통 ====================== */

/* 알고리즘 번호로 정렬을 수행한다 */
void run_sort(int algo, int a[], int n)
{
    switch (algo) {
    case ALGO_MERGE:   merge_sort(a, n);                         break;
    case ALGO_FIRST:   quick_sort_rec(a, 0, n - 1, PIVOT_FIRST);   break;
    case ALGO_RANDOM:  quick_sort_rec(a, 0, n - 1, PIVOT_RANDOM);  break;
    case ALGO_MEDIAN3: quick_sort_rec(a, 0, n - 1, PIVOT_MEDIAN3); break;
    }
}

/* 측정 값을 초기화하고 정렬한다. 스택 기준점은 정렬 함수를 부르기 직전 위치 */
void measured_sort(int algo, int a[], int n)
{
    volatile char base;

    g_cmp = 0;
    g_moves = 0;
    g_heap_bytes = 0;
    g_depth = 0;
    g_max_depth = 0;
    g_stack_base = (uintptr_t)&base;
    g_stack_low = g_stack_base;
    run_sort(algo, a, n);
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
 * 0 ~ 2^30 - 1 범위의 난수 (과제 11, 12와 같은 방법).
 * MSVC의 rand()는 0 ~ 32767만 만들어서 15비트씩 두 번 뽑아 이어 붙인다.
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

typedef struct {
    long long cmp;          /* 비교 횟수 */
    long long moves;        /* 데이터 이동 횟수 */
    double time_ms;         /* REPEAT회 평균 실행시간 */
    long long heap;         /* 추가 힙 메모리 */
    long long stack;        /* 측정한 스택 사용량 */
    int depth;              /* 최대 재귀 깊이 */
} Result;

int main(void)
{
    int sizes[NUM_SIZES] = {1000, 2000, 4000, 8000, 16000, 20000};
    const char *names[NUM_ALGOS] = {
        "Merge Sort", "Quick (First)", "Quick (Random)", "Quick (Median3)"
    };
    int *original[NUM_SIZES];
    int *work;
    Result res[NUM_SIZES][NUM_ALGOS];
    int s, k, r, i, n;
    int all_sorted = 1;
    double t0;

    work = (int *)malloc(sizeof(int) * sizes[NUM_SIZES - 1]);
    if (work == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        return 1;
    }

    srand(SEED);

    /*
     * [1단계] 데이터 생성 + 비교 횟수, 메모리 측정
     * 과제 12와 같은 순서로 rand()를 사용해야 같은 데이터가 나오므로,
     * 크기마다 데이터를 만든 직후 First -> Random -> Median3 순서로 한 번씩 정렬한다.
     * (병합 정렬은 rand()를 쓰지 않아서 순서에 영향이 없다)
     */
    for (s = 0; s < NUM_SIZES; s++) {
        n = sizes[s];
        original[s] = (int *)malloc(sizeof(int) * n);
        if (original[s] == NULL) {
            fprintf(stderr, "Memory allocation failed\n");
            return 1;
        }
        for (i = 0; i < n; i++)              /* 원본 데이터 생성 (중복 허용) */
            original[s][i] = random_value();

        for (k = 0; k < NUM_ALGOS; k++) {
            memcpy(work, original[s], sizeof(int) * n);   /* 같은 원본의 복사본 */
            measured_sort(k, work, n);
            if (!is_sorted(work, n)) all_sorted = 0;

            res[s][k].cmp = g_cmp;
            res[s][k].moves = g_moves;
            res[s][k].heap = g_heap_bytes;
            res[s][k].stack = (long long)(g_stack_base - g_stack_low);
            res[s][k].depth = g_max_depth;
            res[s][k].time_ms = 0.0;
        }
    }

    /*
     * [2단계] 실행시간 측정
     * 크기마다 네 알고리즘을 번갈아 REPEAT번씩 정렬하고 평균을 낸다.
     * 배열 복사와 정렬 확인은 시간 측정 구간 밖에서 한다.
     */
    for (s = 0; s < NUM_SIZES; s++) {
        n = sizes[s];
        for (r = 0; r < REPEAT; r++) {
            for (k = 0; k < NUM_ALGOS; k++) {
                memcpy(work, original[s], sizeof(int) * n);
                t0 = now_ms();
                run_sort(k, work, n);
                res[s][k].time_ms += now_ms() - t0;
                if (!is_sorted(work, n)) all_sorted = 0;
            }
        }
        for (k = 0; k < NUM_ALGOS; k++)
            res[s][k].time_ms /= REPEAT;
    }

    /* ---- 결과 출력 ---- */
    for (s = 0; s < NUM_SIZES; s++) {
        printf("Data Size : %d\n\n", sizes[s]);
        printf("Algorithm       Comparisons    Time(ms)    Memory(Bytes)\n");
        printf("--------------------------------------------------------\n");
        for (k = 0; k < NUM_ALGOS; k++)
            printf("%-16s%-15lld%-12.4f%lld\n", names[k], res[s][k].cmp,
                   res[s][k].time_ms, res[s][k].heap + res[s][k].stack);
        printf("\n");
    }

    printf("Sort check : %s\n\n",
           all_sorted ? "All results are sorted in ascending order"
                      : "ERROR - unsorted result found");

    /* ---- 메모리 내역 (README 분석용) ---- */
    printf("Details (Memory = Heap + Stack)\n");
    printf("Time: average of %d runs / Comparisons, Memory, Moves: first run\n\n", REPEAT);
    printf("Size    Algorithm        Heap(Bytes)  Stack(Bytes)  Max depth  Moves\n");
    printf("--------------------------------------------------------------------\n");
    for (s = 0; s < NUM_SIZES; s++) {
        for (k = 0; k < NUM_ALGOS; k++)
            printf("%-8d%-17s%-13lld%-14lld%-11d%lld\n", sizes[s], names[k],
                   res[s][k].heap, res[s][k].stack, res[s][k].depth, res[s][k].moves);
    }

    for (s = 0; s < NUM_SIZES; s++)
        free(original[s]);
    free(work);
    return 0;
}
