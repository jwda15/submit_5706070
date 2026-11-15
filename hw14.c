/*
 * 과제 14: 힙 정렬, 퀵 정렬 및 병합 정렬의 성능 비교
 * 빌드: cl /O2 /utf-8 hw14.c          (gcc: gcc -O2 -o hw14 hw14.c)
 * 실행: hw14
 *
 * 분석용 통계(최대 재귀 깊이)까지 보려면:
 *   cl /O2 /utf-8 /DANALYSIS /Fe:hw14_analysis.exe hw14.c
 *   (기본 빌드의 출력은 과제 형식 그대로)
 *
 * - 데이터는 과제 12, 13(hw12.c, hw13.c)과 같은 방법, 같은 시드, 같은 순서로 만든다.
 *   그래서 같은 컴파일 환경에서는 이전 과제와 완전히 같은 데이터가 되고,
 *   병합 정렬과 퀵 정렬 세 가지의 비교 횟수도 과제 13의 결과와 같게 나온다.
 * - rand()의 구현은 C 라이브러리마다 달라서, 같은 시드라도 Windows(MSVC)와
 *   Linux(glibc)에서는 다른 데이터가 만들어진다. (README의 결과는 Windows, MSVC 기준)
 *
 * [파일 구성] 정렬 알고리즘 코드는 이 파일 맨 아래(SORT_BODY 부분)에 한 벌만 있고,
 * 이 파일이 자기 자신을 두 번 #include 해서 두 가지 버전으로 컴파일된다.
 *   - 측정용 버전 (함수 이름 뒤에 _m, TRACK = 1): 비교 횟수, 메모리, 재귀 깊이를 기록
 *   - 시간용 버전 (원래 이름,        TRACK = 0): 측정 코드가 컴파일 단계에서 완전히 빠짐
 * 실행시간은 측정 코드가 전혀 없는 시간용 버전으로 재므로, 측정 코드의 비용이
 * 알고리즘마다 다르게 섞여 들어가지 않는다. 두 버전의 알고리즘은 같은 소스라서 동작이 같다.
 */
#ifndef SORT_BODY   /* ============ 공통 부분 (한 번만 컴파일) ============ */

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
#define NUM_ALGOS 5
#define REPEAT    100         /* 실행시간: 같은 조건으로 100회 측정 후 평균 */
#ifndef SEED
#define SEED      20261109u   /* 과제 12, 13과 같은 시드 -> 같은 데이터, 결과 재현 가능 */
#endif

/* =====================================================================
 * 측정 도구 (측정용 버전, 즉 TRACK == 1 일 때만 동작)
 * ===================================================================== */

/* [1] 비교 횟수: 데이터 값끼리 비교하기 직전에 COUNT_CMP() 호출 */
long long g_cmp = 0;
#define COUNT_CMP() do { if (TRACK) g_cmp++; } while (0)

/*
 * [3] 메모리 사용량: 함수가 시작될 때 그 함수의 스택 프레임 크기만큼 더하고,
 *     끝날 때 뺀다. 동적 할당(임시 배열)도 같은 방식으로 더하고 뺀다.
 *     실행 중 가장 컸던 값(peak)이 "최대 추가 메모리 사용량"이다.
 *
 * 스택 프레임 크기 = 매개변수 + 지역 변수 + 호출 비용(복귀 주소 + 이전 프레임 포인터)
 * (64비트 기준: int 4B, 포인터 8B, 호출 비용 16B)
 */
size_t g_mem_cur = 0, g_mem_peak = 0;
int    g_depth = 0, g_max_depth = 0;    /* 재귀 깊이 (분석용) */

#define CALL_OVERHEAD (2 * sizeof(void *))

#define MEM_ADD(bytes) do { if (TRACK) { g_mem_cur += (bytes); \
        if (g_mem_cur > g_mem_peak) g_mem_peak = g_mem_cur; } } while (0)
#define MEM_SUB(bytes) do { if (TRACK) g_mem_cur -= (bytes); } while (0)

#define DEPTH_IN()  do { if (TRACK && ++g_depth > g_max_depth) g_max_depth = g_depth; } while (0)
#define DEPTH_OUT() do { if (TRACK) g_depth--; } while (0)

/* 함수별 스택 프레임 크기: (매개변수) + (지역 변수) + 호출 비용 */
#define FRAME_HEAP_SORT   (sizeof(int *) + sizeof(int)       + 2 * sizeof(int) + CALL_OVERHEAD) /* a,n | i,tmp            */
#define FRAME_SIFT_DOWN   (sizeof(int *) + 2 * sizeof(int)   + 3 * sizeof(int) + CALL_OVERHEAD) /* a,root,size | parent,child,tmp */
#define FRAME_MERGE_SORT  (sizeof(int *) + sizeof(int)       + sizeof(int *)   + CALL_OVERHEAD) /* a,n | temp             */
#define FRAME_MERGE_REC   (2 * sizeof(int *) + 2 * sizeof(int) + sizeof(int)   + CALL_OVERHEAD) /* a,temp,left,right | mid */
#define FRAME_MERGE       (2 * sizeof(int *) + 3 * sizeof(int) + 3 * sizeof(int) + CALL_OVERHEAD) /* a,temp,left,mid,right | i,j,k */
#define FRAME_QUICK_SORT  (sizeof(int *) + 2 * sizeof(int)   + 0               + CALL_OVERHEAD) /* a,n,method             */
#define FRAME_QUICK_REC   (sizeof(int *) + 3 * sizeof(int)   + sizeof(int)     + CALL_OVERHEAD) /* a,low,high,method | p  */
#define FRAME_PARTITION   (sizeof(int *) + 3 * sizeof(int)   + 4 * sizeof(int) + CALL_OVERHEAD) /* a,low,high,method | p,pivot,i,j */
#define FRAME_SEL_PIVOT   (sizeof(int *) + 3 * sizeof(int)   + 0               + CALL_OVERHEAD) /* a,low,high,method      */
#define FRAME_MEDIAN3     (sizeof(int *) + 2 * sizeof(int)   + 4 * sizeof(int) + CALL_OVERHEAD) /* a,low,high | mid,x,y,z */
#define FRAME_SWAP        (sizeof(int *) + 2 * sizeof(int)   + sizeof(int)     + CALL_OVERHEAD) /* a,i,j | t              */

enum { PIVOT_FIRST, PIVOT_RANDOM, PIVOT_MEDIAN3 };

/* ---- 정렬 알고리즘을 두 버전으로 컴파일 ---- */
#define SORT_BODY

#define TRACK 1               /* 측정용 버전: heap_sort_m, merge_sort_m, ... */
#define F(name) name##_m
#include __FILE__
#undef TRACK
#undef F

#define TRACK 0               /* 시간용 버전: heap_sort, merge_sort, ... (측정 코드 없음) */
#define F(name) name
#include __FILE__
#undef TRACK
#undef F

#undef SORT_BODY

/* =====================================================================
 * 공통
 * ===================================================================== */
int is_sorted(const int a[], int n)
{
    int i;
    for (i = 1; i < n; i++)
        if (a[i - 1] > a[i])
            return 0;
    return 1;
}

/*
 * 0 ~ 2^30 - 1 범위의 난수 (과제 11, 12, 13과 같은 방법).
 * MSVC의 rand()는 0 ~ 32767만 만들어서 15비트씩 두 번 뽑아 이어 붙인다.
 */
int random_value(void)
{
    unsigned int hi = (unsigned int)rand() & 0x7FFF;
    unsigned int lo = (unsigned int)rand() & 0x7FFF;
    return (int)((hi << 15) | lo);
}

/* 밀리초 단위 현재 시각 (과제 13과 같은 방법) */
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

typedef void (*sort_func)(int a[], int n);

int main(void)
{
    int sizes[NUM_SIZES] = {1000, 2000, 4000, 8000, 16000, 20000};
    const char *names[NUM_ALGOS] = {
        "Heap Sort", "Merge Sort", "Quick (First)", "Quick (Random)", "Quick (Median3)"
    };
    /* 측정용 버전 (비교 횟수, 메모리, 재귀 깊이 기록) */
    sort_func measured[NUM_ALGOS] = {
        heap_sort_m, merge_sort_m, quick_first_m, quick_random_m, quick_median3_m
    };
    /* 시간용 버전 (측정 코드 없음) */
    sort_func timed[NUM_ALGOS] = {
        heap_sort, merge_sort, quick_first, quick_random, quick_median3
    };
    int *data[NUM_SIZES];
    int *work;
    long long cmp[NUM_SIZES][NUM_ALGOS];
    double    ms[NUM_SIZES][NUM_ALGOS];
    size_t    mem[NUM_SIZES][NUM_ALGOS];
    int       depth[NUM_SIZES][NUM_ALGOS];
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
     * [1단계] 데이터 생성 + 측정 실행 (비교 횟수, 최대 추가 메모리, 정렬 확인)
     * 과제 12, 13과 같은 순서로 rand()를 사용해야 같은 데이터가 나온다.
     * 그래서 크기마다 데이터를 만든 직후 다섯 알고리즘을 한 번씩 정렬한다.
     * rand()를 쓰는 것은 Quick (Random)뿐이고, First -> Random -> Median3 순서가
     * 과제 12, 13과 같다. (힙 정렬, 병합 정렬은 rand()를 쓰지 않아 순서에 영향이 없다)
     */
    for (s = 0; s < NUM_SIZES; s++) {
        n = sizes[s];
        data[s] = (int *)malloc(sizeof(int) * n);
        if (data[s] == NULL) {
            fprintf(stderr, "Memory allocation failed\n");
            return 1;
        }
        for (i = 0; i < n; i++)                      /* 원본 데이터 생성 (중복 허용) */
            data[s][i] = random_value();

        for (k = 0; k < NUM_ALGOS; k++) {
            memcpy(work, data[s], sizeof(int) * n);  /* 같은 원본의 복사본 */
            g_cmp = 0;
            g_mem_cur = g_mem_peak = 0;
            g_depth = g_max_depth = 0;
            measured[k](work, n);
            cmp[s][k]   = g_cmp;
            mem[s][k]   = g_mem_peak;
            depth[s][k] = g_max_depth;
            ms[s][k]    = 0.0;
            if (!is_sorted(work, n)) all_sorted = 0;
        }
    }

    /*
     * [2단계] 실행시간 측정 (측정 코드가 없는 시간용 버전 사용)
     * 크기마다 다섯 알고리즘을 번갈아 REPEAT번씩 정렬하고 평균을 낸다.
     * 번갈아 실행하므로 측정 순서(CPU 클럭 변화, 캐시 상태 등)의 영향이 알고리즘마다 고르게 퍼진다.
     * 배열 복사와 정렬 확인은 시간 측정 구간 밖에서 한다.
     */
    for (s = 0; s < NUM_SIZES; s++) {
        n = sizes[s];
        for (r = 0; r < REPEAT; r++) {
            for (k = 0; k < NUM_ALGOS; k++) {
                memcpy(work, data[s], sizeof(int) * n);   /* 복사: 측정 제외 */
                t0 = now_ms();
                timed[k](work, n);
                ms[s][k] += now_ms() - t0;
                if (!is_sorted(work, n)) all_sorted = 0;  /* 확인: 측정 제외 */
            }
        }
        for (k = 0; k < NUM_ALGOS; k++)
            ms[s][k] /= REPEAT;
    }

    /* ---- 결과 출력 ---- */
    for (s = 0; s < NUM_SIZES; s++) {
        printf("Data Size : %d\n\n", sizes[s]);
        printf("Algorithm       Comparisons    Time(ms)    Memory(Bytes)\n");
        printf("--------------------------------------------------------\n");
        for (k = 0; k < NUM_ALGOS; k++)
            printf("%-16s%-15lld%-12.4f%zu\n", names[k], cmp[s][k], ms[s][k], mem[s][k]);
        printf("\n");

#ifdef ANALYSIS
        for (k = 0; k < NUM_ALGOS; k++)
            printf("  [analysis] %-16s max recursion depth %3d | extra memory %zu B\n",
                   names[k], depth[s][k], mem[s][k]);
        printf("\n");
#endif
    }
#ifndef ANALYSIS
    (void)depth;
#endif

    printf("Sort check : %s\n",
           all_sorted ? "All results are sorted in ascending order"
                      : "ERROR - unsorted result found");

    for (s = 0; s < NUM_SIZES; s++)
        free(data[s]);
    free(work);
    return 0;
}

#else   /* ============ SORT_BODY: 정렬 알고리즘 (두 버전으로 컴파일됨) ============ */

/* =====================================================================
 * [1] 힙 정렬 (최대 힙, 입력 배열 자체를 힙으로 사용)
 * ---------------------------------------------------------------------
 * 배열 인덱스 0 이 루트, i 의 자식은 2i+1, 2i+2
 * ===================================================================== */

/* root 에서 시작해 아래로 내려가며 최대 힙 속성 복구 (반복문 사용, 재귀 없음) */
static void F(sift_down)(int a[], int root, int size)
{
    int parent = root, child, tmp = a[root];
    MEM_ADD(FRAME_SIFT_DOWN);

    while ((child = 2 * parent + 1) < size) {       /* 인덱스 검사: 미포함 */
        if (child + 1 < size) {                      /* 오른쪽 자식이 있으면 */
            COUNT_CMP();                             /* 두 자식 중 큰 쪽 고르기 */
            if (a[child + 1] > a[child])
                child++;
        }
        COUNT_CMP();                                 /* 내려가는 값과 큰 자식 비교 */
        if (tmp >= a[child])
            break;
        a[parent] = a[child];                        /* 큰 자식을 위로 올림 */
        parent = child;
    }
    a[parent] = tmp;

    MEM_SUB(FRAME_SIFT_DOWN);
}

void F(heap_sort)(int a[], int n)
{
    int i, tmp;
    MEM_ADD(FRAME_HEAP_SORT);

    /* 1) 입력 배열을 최대 힙으로 구성: 마지막 내부 노드부터 루트까지 sift_down */
    for (i = n / 2 - 1; i >= 0; i--)
        F(sift_down)(a, i, n);

    /* 2) 루트(최댓값)를 힙의 마지막 원소와 교환 -> 힙 크기 1 감소 -> 힙 속성 복구 */
    for (i = n - 1; i > 0; i--) {
        tmp = a[0];
        a[0] = a[i];
        a[i] = tmp;
        F(sift_down)(a, 0, i);
    }

    MEM_SUB(FRAME_HEAP_SORT);
}

/* =====================================================================
 * [2] 병합 정렬 (과제 13과 같은 하향식 재귀, 크기 n 인 임시 배열 하나 사용)
 * ===================================================================== */

/* a[left..mid] 와 a[mid+1..right] (각각 정렬됨) 를 병합 */
static void F(merge)(int a[], int temp[], int left, int mid, int right)
{
    int i = left, j = mid + 1, k = left;
    MEM_ADD(FRAME_MERGE);

    while (i <= mid && j <= right) {                 /* 인덱스 검사: 미포함 */
        COUNT_CMP();                                 /* 두 부분의 맨 앞 값 비교 */
        if (a[i] <= a[j])                            /* 같으면 왼쪽 먼저: 안정 정렬 */
            temp[k++] = a[i++];
        else
            temp[k++] = a[j++];
    }
    while (i <= mid)   temp[k++] = a[i++];           /* 남은 쪽은 비교 없이 복사 */
    while (j <= right) temp[k++] = a[j++];

    for (k = left; k <= right; k++)                  /* 원래 배열로 되돌림 */
        a[k] = temp[k];

    MEM_SUB(FRAME_MERGE);
}

static void F(merge_sort_rec)(int a[], int temp[], int left, int right)
{
    int mid;
    MEM_ADD(FRAME_MERGE_REC);
    DEPTH_IN();

    if (left < right) {
        mid = (left + right) / 2;
        F(merge_sort_rec)(a, temp, left, mid);       /* 분할 */
        F(merge_sort_rec)(a, temp, mid + 1, right);
        F(merge)(a, temp, left, mid, right);         /* 병합 */
    }

    DEPTH_OUT();
    MEM_SUB(FRAME_MERGE_REC);
}

void F(merge_sort)(int a[], int n)
{
    int *temp;
    MEM_ADD(FRAME_MERGE_SORT);

    temp = (int *)malloc(sizeof(int) * n);           /* 임시 배열: 추가 메모리 */
    if (temp == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }
    MEM_ADD(sizeof(int) * n);

    F(merge_sort_rec)(a, temp, 0, n - 1);

    free(temp);
    MEM_SUB(sizeof(int) * n);
    MEM_SUB(FRAME_MERGE_SORT);
}

/* =====================================================================
 * [3] 퀵 정렬 (과제 12, 13 과 같은 분할 알고리즘, 피벗 선택만 다름)
 * ===================================================================== */

static void F(swap)(int a[], int i, int j)
{
    int t;
    MEM_ADD(FRAME_SWAP);
    t = a[i];
    a[i] = a[j];
    a[j] = t;
    MEM_SUB(FRAME_SWAP);
}

/* 첫 번째, 가운데, 마지막 원소 중 중앙값의 위치 반환 (값 비교 2~3회 포함) */
static int F(median_of_three)(int a[], int low, int high)
{
    int mid = (low + high) / 2;
    int x = a[low], y = a[mid], z = a[high];
    int result;
    MEM_ADD(FRAME_MEDIAN3);

    COUNT_CMP();
    if (x < y) {
        COUNT_CMP();
        if (y < z)      result = mid;    /* x < y < z   -> y */
        else {
            COUNT_CMP();
            result = (x < z) ? high      /* x < z <= y  -> z */
                             : low;      /* z <= x < y  -> x */
        }
    } else {
        COUNT_CMP();
        if (x < z)      result = low;    /* y <= x < z  -> x */
        else {
            COUNT_CMP();
            result = (y < z) ? high      /* y < z <= x  -> z */
                             : mid;      /* z <= y <= x -> y */
        }
    }

    MEM_SUB(FRAME_MEDIAN3);
    return result;
}

/* 피벗 선택: 세 방법에서 다른 부분은 이것뿐 */
static int F(select_pivot)(int a[], int low, int high, int method)
{
    int result;
    MEM_ADD(FRAME_SEL_PIVOT);

    if (method == PIVOT_RANDOM)
        result = low + rand() % (high - low + 1);    /* 분할마다 새로 선택, 비교 아님 */
    else if (method == PIVOT_MEDIAN3)
        result = F(median_of_three)(a, low, high);
    else
        result = low;                                /* PIVOT_FIRST */

    MEM_SUB(FRAME_SEL_PIVOT);
    return result;
}

/* 분할: 선택된 피벗을 맨 앞으로 옮긴 뒤 양쪽에서 좁혀 오며 교환 */
static int F(partition)(int a[], int low, int high, int method)
{
    int p, pivot, i, j;
    MEM_ADD(FRAME_PARTITION);

    p = F(select_pivot)(a, low, high, method);
    F(swap)(a, low, p);
    pivot = a[low];
    i = low;
    j = high + 1;

    while (1) {
        while (1) {                                  /* 왼쪽에서 피벗 이상인 값 찾기 */
            i++;
            if (i > high) break;                     /* 인덱스 검사: 미포함 */
            COUNT_CMP();
            if (!(a[i] < pivot)) break;
        }
        while (1) {                                  /* 오른쪽에서 피벗 이하인 값 찾기 */
            j--;                                     /* a[low] == pivot 이라 low 에서 반드시 멈춤 */
            COUNT_CMP();
            if (!(a[j] > pivot)) break;
        }
        if (i >= j) break;                           /* 인덱스 비교: 미포함 */
        F(swap)(a, i, j);
    }
    F(swap)(a, low, j);                              /* 피벗을 제자리로 */

    MEM_SUB(FRAME_PARTITION);
    return j;
}

static void F(quick_sort_rec)(int a[], int low, int high, int method)
{
    int p;
    MEM_ADD(FRAME_QUICK_REC);
    DEPTH_IN();

    if (low < high) {
        p = F(partition)(a, low, high, method);
        F(quick_sort_rec)(a, low, p - 1, method);
        F(quick_sort_rec)(a, p + 1, high, method);
    }

    DEPTH_OUT();
    MEM_SUB(FRAME_QUICK_REC);
}

static void F(quick_sort)(int a[], int n, int method)
{
    MEM_ADD(FRAME_QUICK_SORT);
    F(quick_sort_rec)(a, 0, n - 1, method);
    MEM_SUB(FRAME_QUICK_SORT);
}

void F(quick_first)(int a[], int n)   { F(quick_sort)(a, n, PIVOT_FIRST); }
void F(quick_random)(int a[], int n)  { F(quick_sort)(a, n, PIVOT_RANDOM); }
void F(quick_median3)(int a[], int n) { F(quick_sort)(a, n, PIVOT_MEDIAN3); }

#endif  /* SORT_BODY */
