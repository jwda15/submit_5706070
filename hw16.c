/*
 * 과제 16: 순차 탐색과 이진 탐색의 성능 비교
 *
 * 빌드: gcc -O2 -o hw16 hw16.c
 * 실행: ./hw16 [CSV파일] [검색할 이름 ...]
 *   ./hw16                                   기본 CSV(dataset_id_ascending_1.csv), 예시 이름 검색
 *   ./hw16 dataset_id_ascending_1.csv Ryan   지정한 이름(여러 개 가능) 검색
 *
 * 분석용 통계(첫 번째 원소 피벗과의 정렬 비용 비교, 이진 탐색 비교 내역)까지 보려면:
 *   gcc -O2 -DANALYSIS -o hw16_analysis hw16.c && ./hw16_analysis
 */
/* MinGW(Windows)에서도 %lld 가 올바르게 출력되도록 (다른 환경에서는 영향 없음) */
#define __USE_MINGW_ANSI_STDIO 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_CSV  "dataset_id_ascending (1).csv"
#define NAME_LEN     32
#define K_MAX        10000      /* 최대 검색 횟수 */
#define MISS_PERCENT 20         /* 검색 이름 중 존재하지 않는 이름 비율 (%) */
#define SEED         20261109UL /* 고정 시드: 검색 목록 재현 가능 */

typedef struct {
    int  id;
    char name[NAME_LEN];
    char gender;
    int  korean, english, math;
    int  total;                 /* 국어 + 영어 + 수학 */
} Student;

/* =====================================================================
 * 난수 생성기 (xorshift32)
 * rand() 는 OS/컴파일러마다 구현이 달라 검색 목록이 바뀌므로 직접 구현한다.
 * 32비트로 마스킹해서 unsigned long 이 32/64비트인 환경 모두 같은 수열을 낸다.
 * ===================================================================== */
unsigned long rng_state = SEED;

unsigned long rng_next(void)
{
    unsigned long x = rng_state;
    x ^= (x << 13) & 0xFFFFFFFFUL;
    x ^= x >> 17;
    x ^= (x << 5) & 0xFFFFFFFFUL;
    rng_state = x & 0xFFFFFFFFUL;
    return rng_state;
}

/* =====================================================================
 * 비교 횟수: 이름 비교(strcmp 호출) 1회 = 1
 * 모든 이름 비교는 이 함수를 거친다 -> 두 방법의 비교 기준이 같음 (strcmp 바이트 순서)
 * ===================================================================== */
long long g_cmp = 0;

int name_cmp(const char *a, const char *b)
{
    g_cmp++;
    return strcmp(a, b);
}

/* =====================================================================
 * CSV 읽기 (비교 횟수 제외)
 * 형식: ID,NAME,GENDER,KOREAN_GRADE,ENGLISH_GRADE,MATH_GRADE (첫 줄은 헤더)
 * ===================================================================== */
Student *load_csv(const char *path, int *count)
{
    FILE *fp = fopen(path, "r");
    char line[256];
    int cap = 1024, n = 0;
    Student *arr;

    if (fp == NULL) {
        fprintf(stderr, "Cannot open CSV file: %s\n", path);
        exit(1);
    }
    arr = (Student *)malloc(sizeof(Student) * cap);
    if (arr == NULL) { fprintf(stderr, "Memory allocation failed\n"); exit(1); }

    if (fgets(line, sizeof(line), fp) == NULL) {       /* 헤더 건너뛰기 */
        fprintf(stderr, "Empty CSV file\n");
        exit(1);
    }
    while (fgets(line, sizeof(line), fp) != NULL) {
        Student s;
        char g[8];
        if (sscanf(line, "%d,%31[^,],%7[^,],%d,%d,%d",
                   &s.id, s.name, g, &s.korean, &s.english, &s.math) != 6)
            continue;                                   /* 빈 줄 등은 무시 */
        s.gender = g[0];
        s.total = s.korean + s.english + s.math;        /* 총점 계산 */
        if (n == cap) {
            cap *= 2;
            arr = (Student *)realloc(arr, sizeof(Student) * cap);
            if (arr == NULL) { fprintf(stderr, "Memory allocation failed\n"); exit(1); }
        }
        arr[n++] = s;
    }
    fclose(fp);
    *count = n;
    return arr;
}

/* =====================================================================
 * [1] 순차 탐색: 정렬하지 않은 원본 배열을 처음부터 끝까지 비교
 *     동명이인을 모두 찾아야 하므로 일치해도 멈추지 않고 끝까지 확인한다.
 *     결과(일치하는 학생의 인덱스)는 out[] 에 저장, 개수 반환
 * ===================================================================== */
int sequential_search(const Student a[], int n, const char *key, int out[])
{
    int i, found = 0;
    for (i = 0; i < n; i++)
        if (name_cmp(key, a[i].name) == 0)
            out[found++] = i;
    return found;
}

/* =====================================================================
 * [2] 퀵 정렬 (이름 오름차순, Median-of-Three 피벗, 이전 과제와 같은 분할)
 * ===================================================================== */
enum { PIVOT_FIRST, PIVOT_MEDIAN3 };

void swap_student(Student a[], int i, int j)
{
    Student t = a[i];
    a[i] = a[j];
    a[j] = t;
}

/* 첫 번째, 가운데, 마지막 이름 중 중앙값의 위치 (이름 비교 2~3회) */
int median_of_three(const Student a[], int low, int high)
{
    int mid = (low + high) / 2;
    if (name_cmp(a[low].name, a[mid].name) < 0) {
        if (name_cmp(a[mid].name, a[high].name) < 0) return mid;
        return (name_cmp(a[low].name, a[high].name) < 0) ? high : low;
    } else {
        if (name_cmp(a[low].name, a[high].name) < 0) return low;
        return (name_cmp(a[mid].name, a[high].name) < 0) ? high : mid;
    }
}

int partition(Student a[], int low, int high, int method)
{
    int p = (method == PIVOT_MEDIAN3) ? median_of_three(a, low, high) : low;
    int i = low, j = high + 1;
    char pivot[NAME_LEN];

    swap_student(a, low, p);
    strcpy(pivot, a[low].name);

    while (1) {
        while (1) {                                     /* 왼쪽에서 피벗 이상인 이름 찾기 */
            i++;
            if (i > high) break;                        /* 인덱스 검사: 비교 아님 */
            if (!(name_cmp(a[i].name, pivot) < 0)) break;
        }
        while (1) {                                     /* 오른쪽에서 피벗 이하인 이름 찾기 */
            j--;
            if (!(name_cmp(a[j].name, pivot) > 0)) break;
        }
        if (i >= j) break;
        swap_student(a, i, j);
    }
    swap_student(a, low, j);
    return j;
}

void quick_sort(Student a[], int low, int high, int method)
{
    int p;
    if (low < high) {
        p = partition(a, low, high, method);
        quick_sort(a, low, p - 1, method);
        quick_sort(a, p + 1, high, method);
    }
}

/* =====================================================================
 * 이진 탐색: 정렬된 배열에서 이름 하나를 찾은 뒤,
 *           그 양옆으로 같은 이름(동명이인)이 계속되는지 확인 (이 비교도 모두 포함)
 *           일치하는 구간 [first, last] 를 돌려주고, 개수 반환
 * ===================================================================== */
#ifdef ANALYSIS
long long a_bin_main = 0, a_bin_dup = 0;    /* 이진 탐색 본 비교 / 동명이인 확인 비교 */
#endif

int binary_search(const Student a[], int n, const char *key, int *first, int *last)
{
    int low = 0, high = n - 1, mid, c, L, R;
#ifdef ANALYSIS
    long long before = g_cmp;
#endif

    while (low <= high) {
        mid = (low + high) / 2;
        c = name_cmp(key, a[mid].name);
        if (c == 0) {
#ifdef ANALYSIS
            a_bin_main += g_cmp - before;
            before = g_cmp;
#endif
            L = R = mid;
            while (L - 1 >= 0 && name_cmp(key, a[L - 1].name) == 0) L--;   /* 왼쪽 동명이인 */
            while (R + 1 < n  && name_cmp(key, a[R + 1].name) == 0) R++;   /* 오른쪽 동명이인 */
#ifdef ANALYSIS
            a_bin_dup += g_cmp - before;
#endif
            *first = L;
            *last = R;
            return R - L + 1;
        }
        if (c < 0) high = mid - 1;
        else       low = mid + 1;
    }
#ifdef ANALYSIS
    a_bin_main += g_cmp - before;
#endif
    return 0;
}

/* =====================================================================
 * 검증·출력용 (비교 횟수 제외)
 * ===================================================================== */
int cmp_int(const void *x, const void *y)
{
    return *(const int *)x - *(const int *)y;
}

/* 두 방법의 결과가 같은 학생 집합인지 학번으로 확인 */
int same_result(const Student orig[], const int seq_idx[], int seq_cnt,
                const Student sorted[], int first, int bin_cnt)
{
    int *a, *b, i, ok = 1;
    if (seq_cnt != bin_cnt) return 0;
    if (seq_cnt == 0) return 1;
    a = (int *)malloc(sizeof(int) * seq_cnt);
    b = (int *)malloc(sizeof(int) * seq_cnt);
    for (i = 0; i < seq_cnt; i++) {
        a[i] = orig[seq_idx[i]].id;
        b[i] = sorted[first + i].id;
    }
    qsort(a, seq_cnt, sizeof(int), cmp_int);
    qsort(b, seq_cnt, sizeof(int), cmp_int);
    for (i = 0; i < seq_cnt; i++)
        if (a[i] != b[i]) { ok = 0; break; }
    free(a); free(b);
    return ok;
}

void print_student(const Student *s)
{
    printf("ID      : %d\n", s->id);
    printf("Name    : %s\n", s->name);
    printf("Korean  : %d\n", s->korean);
    printf("English : %d\n", s->english);
    printf("Math    : %d\n", s->math);
    printf("Total   : %d\n", s->total);
}

/* 이름 하나를 두 방법으로 검색해서 결과 출력 (출력용 검색은 실험 비교 횟수와 별개) */
void search_and_print(const Student orig[], const Student sorted[], int n, const char *name)
{
    int *idx = (int *)malloc(sizeof(int) * n);
    int seq_cnt, bin_cnt, first = 0, last = -1, i;

    seq_cnt = sequential_search(orig, n, name, idx);
    bin_cnt = binary_search(sorted, n, name, &first, &last);

    printf("Search Result\n");
    if (seq_cnt == 0) {
        printf("Name    : %s\n", name);
        printf("No student found\n");
    } else {
        for (i = 0; i < seq_cnt; i++) {                 /* 동명이인 모두 출력 (학번 순) */
            if (seq_cnt > 1) printf("[%d/%d]\n", i + 1, seq_cnt);
            print_student(&orig[idx[i]]);
            if (i < seq_cnt - 1) printf("\n");
        }
    }
    printf("(Sequential: %d found, Quick + Binary: %d found -> %s)\n\n",
           seq_cnt, bin_cnt,
           same_result(orig, idx, seq_cnt, sorted, first, bin_cnt) ? "same result" : "MISMATCH");
    free(idx);
}

/* 정렬된 배열에 이름이 있는지 (검색 목록 생성용, 비교 횟수 제외) */
int exists_name(const Student sorted[], int n, const char *name)
{
    int low = 0, high = n - 1, mid, c;
    while (low <= high) {
        mid = (low + high) / 2;
        c = strcmp(name, sorted[mid].name);
        if (c == 0) return 1;
        if (c < 0) high = mid - 1; else low = mid + 1;
    }
    return 0;
}

/* =====================================================================
 * main
 * ===================================================================== */
int main(int argc, char *argv[])
{
    /* 표에 출력할 K: 역전 구간(작은 K)은 촘촘하게, 이후는 크게 늘림 */
    static const int K_LIST[] = {1, 5, 10, 15, 16, 17, 18, 19, 20, 25, 30, 50,
                                 100, 200, 500, 1000, 2000, 5000, 10000};
    const int K_LIST_N = sizeof(K_LIST) / sizeof(K_LIST[0]);

    const char *csv = DEFAULT_CSV;
    int name_arg = 1;
    Student *orig, *sorted;
    int n, i, k;
    char (*keys)[NAME_LEN];
    long long sort_cmp, seq_total, bin_total;
    long long *seq_at, *bin_at;          /* K 회 검색까지의 누적 비교 횟수 */
    int *idx, first, last, seq_cnt, bin_cnt;
    int all_same = 1, hit = 0, breakeven = -1;

    if (argc >= 2 && strstr(argv[1], ".csv") != NULL) {
        csv = argv[1];
        name_arg = 2;
    }

    /* ---------- 데이터 입력 (비교 횟수 제외) ---------- */
    orig = load_csv(csv, &n);
    printf("Loaded %d students from %s\n\n", n, csv);

    /* ---------- 퀵 정렬: 원본의 복사본을 이름 오름차순으로 ---------- */
    sorted = (Student *)malloc(sizeof(Student) * n);
    memcpy(sorted, orig, sizeof(Student) * n);
    g_cmp = 0;
    quick_sort(sorted, 0, n - 1, PIVOT_MEDIAN3);
    sort_cmp = g_cmp;
    for (i = 1; i < n; i++)
        if (strcmp(sorted[i - 1].name, sorted[i].name) > 0) {
            fprintf(stderr, "Sort check failed\n");
            return 1;
        }

    /* ---------- 검색 이름 목록 생성 (K_MAX 개, 비교 횟수 제외) ----------
     * - 80%: 실제 학생 이름 (학생을 무작위로 골라 그 이름 사용 -> 흔한 이름일수록 자주 검색됨)
     * - 20%: 존재하지 않는 이름 (대문자 + 소문자 4~7자를 무작위 생성, 실제로 없는지 확인)
     * K 가 커질 때는 이 목록의 앞에서부터 K 개를 사용 -> 기존 목록에 새 검색이 추가되는 방식 */
    rng_state = SEED;
    keys = malloc(sizeof(*keys) * K_MAX);
    for (k = 0; k < K_MAX; k++) {
        if (rng_next() % 100 < MISS_PERCENT) {
            do {
                int len = 5 + rng_next() % 4, c;
                keys[k][0] = 'A' + rng_next() % 26;
                for (c = 1; c < len; c++)
                    keys[k][c] = 'a' + rng_next() % 26;
                keys[k][len] = '\0';
            } while (exists_name(sorted, n, keys[k]));
        } else {
            strcpy(keys[k], orig[rng_next() % n].name);
        }
    }

    /* ---------- 검색 결과 출력 ---------- */
    if (argc > name_arg) {
        for (i = name_arg; i < argc; i++)               /* 명령줄로 받은 이름 */
            search_and_print(orig, sorted, n, argv[i]);
    } else {
        /* 예시: 동명이인이 있는 이름, 한 명뿐인 이름, 없는 이름 */
        search_and_print(orig, sorted, n, "Ryan");
        search_and_print(orig, sorted, n, "Zzyzx");
        search_and_print(orig, sorted, n, "Hong Gildong");
    }

    /* ---------- K 에 따른 실험 ----------
     * 검색을 1번부터 K_MAX 번까지 차례로 수행하며 누적 비교 횟수를 기록한다.
     * K 번째 값 = 앞에서부터 K 개 검색 이름을 썼을 때의 합 (목록에 새 검색을 추가하는 방식) */
    seq_at = (long long *)malloc(sizeof(long long) * (K_MAX + 1));
    bin_at = (long long *)malloc(sizeof(long long) * (K_MAX + 1));
    idx = (int *)malloc(sizeof(int) * n);
    seq_total = 0;
    bin_total = 0;
    seq_at[0] = 0;
    bin_at[0] = 0;
#ifdef ANALYSIS
    a_bin_main = a_bin_dup = 0;
#endif

    for (k = 0; k < K_MAX; k++) {
        g_cmp = 0;
        seq_cnt = sequential_search(orig, n, keys[k], idx);
        seq_total += g_cmp;

        g_cmp = 0;
        bin_cnt = binary_search(sorted, n, keys[k], &first, &last);
        bin_total += g_cmp;

        if (!same_result(orig, idx, seq_cnt, sorted, first, bin_cnt)) all_same = 0;
        if (seq_cnt > 0) hit++;

        seq_at[k + 1] = seq_total;
        bin_at[k + 1] = bin_total;
    }

    /* 정렬 비용을 포함한 이진 탐색이 처음으로 순차 탐색보다 적어지는 K */
    for (k = 1; k <= K_MAX; k++)
        if (sort_cmp + bin_at[k] < seq_at[k]) { breakeven = k; break; }

    printf("Comparison Results\n");
    printf("(Quick Sort comparisons: %lld, included once in every Quick + Binary total)\n\n",
           sort_cmp);
    printf("K      Sequential     Quick + Binary\n");
    printf("--------------------------------------\n");
    for (i = 0; i < K_LIST_N; i++) {
        k = K_LIST[i];
        printf("%-7d%-15lld%lld\n", k, seq_at[k], sort_cmp + bin_at[k]);
    }
    printf("\n");
    if (breakeven > 0)
        printf("Break-even : Quick + Binary becomes smaller from K = %d "
               "(K = %d: %lld vs %lld, K = %d: %lld vs %lld)\n",
               breakeven,
               breakeven - 1, seq_at[breakeven - 1], sort_cmp + bin_at[breakeven - 1],
               breakeven, seq_at[breakeven], sort_cmp + bin_at[breakeven]);
    else
        printf("Break-even : not reached up to K = %d\n", K_MAX);
    printf("Search keys: %d (existing %d, not existing %d)\n", K_MAX, hit, K_MAX - hit);
    printf("Result check : %s\n",
           all_same ? "Sequential and Quick + Binary results are identical for all searches"
                    : "MISMATCH FOUND");

#ifdef ANALYSIS
    {
        Student *tmp = (Student *)malloc(sizeof(Student) * n);
        long long first_cmp;
        int distinct = 1;
        memcpy(tmp, orig, sizeof(Student) * n);
        g_cmp = 0;
        quick_sort(tmp, 0, n - 1, PIVOT_FIRST);
        first_cmp = g_cmp;
        for (i = 1; i < n; i++)
            if (strcmp(sorted[i - 1].name, sorted[i].name) != 0) distinct++;
        printf("\n  [analysis] students %d, distinct names %d\n", n, distinct);
        printf("  [analysis] quick sort comparisons: Median-of-Three %lld, First element %lld\n",
               sort_cmp, first_cmp);
        printf("  [analysis] binary search over %d searches: main %lld, duplicate check %lld\n",
               K_MAX, a_bin_main, a_bin_dup);
        printf("  [analysis] per search: sequential %.1f, binary %.2f\n",
               (double)seq_total / K_MAX, (double)bin_total / K_MAX);
        free(tmp);
    }
#endif

    free(orig); free(sorted); free(keys); free(seq_at); free(bin_at); free(idx);
    return 0;
}