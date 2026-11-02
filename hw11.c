/*
 * 과제 11: 삽입 정렬과 쉘 정렬의 성능 비교
 * 빌드: gcc -O2 -o hw11 hw11.c
 * 실행: ./hw11
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define NUM_SIZES 6
#define MAX_PASSES 32   /* 간격 수열의 최대 길이 (n = 20000이면 기본 간격 15단계) */

/*
 * 비교 횟수 규칙
 * - 데이터 값끼리 비교(a[j] > key)할 때마다 결과와 관계없이 1회
 * - 인덱스/종료 조건(j >= 0, j >= gap 등)은 세지 않음
 */

/* 쉘 정렬이 사용한 간격과 간격별 비교 횟수 (README 분석용 기록, 비교 횟수에는 영향 없음) */
typedef struct {
    int count;
    int gap[MAX_PASSES];
    long long cmp[MAX_PASSES];
} PassLog;

/* 간격 gap 으로 삽입 정렬 수행 (gap = 1 이면 일반 삽입 정렬) */
void gap_insertion_sort(int a[], int n, int gap, long long *cmp)
{
    int i, j, key;

    for (i = gap; i < n; i++) {
        key = a[i];
        j = i - gap;
        while (j >= 0) {             /* 인덱스 검사: 비교 횟수에 미포함 */
            (*cmp)++;                /* 데이터 값 비교 1회 */
            if (a[j] > key) {
                a[j + gap] = a[j];
                j -= gap;
            } else {
                break;
            }
        }
        a[j + gap] = key;
    }
}

/* 간격 하나로 정렬하고 그 단계의 비교 횟수를 기록한다 */
void shell_pass(int a[], int n, int gap, long long *cmp, PassLog *log)
{
    long long before = *cmp;

    gap_insertion_sort(a, n, gap, cmp);
    log->gap[log->count] = gap;
    log->cmp[log->count] = *cmp - before;
    log->count++;
}

/* [1] 삽입 정렬 */
long long insertion_sort(int a[], int n)
{
    long long cmp = 0;
    gap_insertion_sort(a, n, 1, &cmp);
    return cmp;
}

/* [2] 쉘 정렬 - 기본 간격: n/2, n/4, ..., 1 */
long long shell_sort_original(int a[], int n, PassLog *log)
{
    long long cmp = 0;
    int gap;

    log->count = 0;
    for (gap = n / 2; gap >= 1; gap /= 2)
        shell_pass(a, n, gap, &cmp, log);
    return cmp;
}

/* [3] 쉘 정렬 - Knuth 수열: 1, 4, 13, 40, ... (데이터 크기보다 작은 가장 큰 간격부터) */
long long shell_sort_knuth(int a[], int n, PassLog *log)
{
    long long cmp = 0;
    int gap = 1;

    while (3 * gap + 1 < n)          /* n 보다 작은 가장 큰 h 찾기 */
        gap = 3 * gap + 1;

    log->count = 0;
    for (; gap >= 1; gap = (gap - 1) / 3)   /* 역순: h(k) = (h(k+1) - 1) / 3 */
        shell_pass(a, n, gap, &cmp, log);
    return cmp;
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
 * 0 ~ 2^30 - 1 범위의 난수.
 * MSVC의 rand()는 0 ~ 32767만 만들어서 n = 20000이면 같은 값이 많이 생기므로
 * 15비트씩 두 번 뽑아 이어 붙인다. (RAND_MAX가 더 큰 환경에서도 같은 범위가 된다)
 */
int random_value(void)
{
    unsigned int hi = (unsigned int)rand() & 0x7FFF;
    unsigned int lo = (unsigned int)rand() & 0x7FFF;
    return (int)((hi << 15) | lo);
}

void print_gaps(const char *name, PassLog *log)
{
    int i;

    printf("  %-9s (%2d passes):", name, log->count);
    for (i = 0; i < log->count; i++)
        printf(" %d", log->gap[i]);
    printf("\n");
}

void print_pass_detail(const char *name, PassLog *log, long long total)
{
    int i;

    printf("Shell Sort (%s), n = 20000\n", name);
    printf("  Gap     Comparisons   Share\n");
    for (i = 0; i < log->count; i++)
        printf("  %5d  %11lld  %5.1f%%\n", log->gap[i], log->cmp[i],
               100.0 * (double)log->cmp[i] / (double)total);
    printf("  Total  %11lld\n\n", total);
}

int main(void)
{
    int sizes[NUM_SIZES] = {1000, 2000, 4000, 8000, 16000, 20000};
    int s, i, n;
    int all_sorted = 1;
    int *original, *work;
    long long c_ins, c_org, c_knu;
    long long last_org = 0, last_knu = 0;
    PassLog org_log[NUM_SIZES], knu_log[NUM_SIZES];

    srand((unsigned int)time(NULL));

    for (s = 0; s < NUM_SIZES; s++) {
        n = sizes[s];
        original = (int *)malloc(sizeof(int) * n);
        work     = (int *)malloc(sizeof(int) * n);
        if (original == NULL || work == NULL) {
            fprintf(stderr, "Memory allocation failed\n");
            return 1;
        }

        /* 원본 데이터 생성 (측정 제외) */
        for (i = 0; i < n; i++)
            original[i] = random_value();

        /* 세 정렬 모두 같은 원본의 복사본 사용 (복사는 측정 제외) */
        memcpy(work, original, sizeof(int) * n);
        c_ins = insertion_sort(work, n);
        if (!is_sorted(work, n)) all_sorted = 0;

        memcpy(work, original, sizeof(int) * n);
        c_org = shell_sort_original(work, n, &org_log[s]);
        if (!is_sorted(work, n)) all_sorted = 0;

        memcpy(work, original, sizeof(int) * n);
        c_knu = shell_sort_knuth(work, n, &knu_log[s]);
        if (!is_sorted(work, n)) all_sorted = 0;

        printf("Data Size : %d\n\n", n);
        printf("Insertion Sort       : %lld comparisons\n", c_ins);
        printf("Shell Sort (Original): %lld comparisons\n", c_org);
        printf("Shell Sort (Knuth)   : %lld comparisons\n\n", c_knu);

        last_org = c_org;
        last_knu = c_knu;
        free(original);
        free(work);
    }

    printf("Sort check : %s\n\n",
           all_sorted ? "All results are sorted in ascending order"
                      : "ERROR - unsorted result found");

    /* ---- 이하 README 분석용 추가 정보 ---- */
    printf("Gap sequences\n");
    for (s = 0; s < NUM_SIZES; s++) {
        printf("n = %d\n", sizes[s]);
        print_gaps("Original", &org_log[s]);
        print_gaps("Knuth", &knu_log[s]);
    }
    printf("\n");

    print_pass_detail("Original", &org_log[NUM_SIZES - 1], last_org);
    print_pass_detail("Knuth", &knu_log[NUM_SIZES - 1], last_knu);

    return 0;
}
