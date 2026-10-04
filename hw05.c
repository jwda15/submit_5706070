/*
 * 과제 05: 순차 탐색과 이진 탐색 트리 탐색의 비교
 * 빌드: gcc -o hw05 hw05.c
 * 실행: ./hw05
 */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define DATA_COUNT   100    /* 저장할 정수 개수 */
#define SEARCH_COUNT 50     /* 탐색 대상 개수 */
#define MAX_VALUE    1000   /* 0 ~ 1000 */

/* ---------- 이진 탐색 트리 노드 ---------- */
typedef struct Node {
    int data;
    struct Node *left;
    struct Node *right;
} Node;

Node *create_node(int data)
{
    Node *n = (Node *)malloc(sizeof(Node));
    if (n == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }
    n->data = data;
    n->left = NULL;
    n->right = NULL;
    return n;
}

/*
 * BST 삽입. 기존 노드 하나와 비교할 때마다 *cmp 를 1 증가시킨다.
 * (중복 값은 들어오지 않는다고 가정 - main 에서 이미 걸러냄)
 */
Node *bst_insert(Node *root, int key, int *cmp)
{
    Node *new_node = create_node(key);
    Node *cur = root;
    Node *parent = NULL;

    if (root == NULL)           /* 첫 노드: 비교 없음 */
        return new_node;

    while (cur != NULL) {
        parent = cur;
        (*cmp)++;               /* key 와 cur->data 비교 1회 */
        if (key < cur->data)
            cur = cur->left;
        else
            cur = cur->right;
    }

    if (key < parent->data)
        parent->left = new_node;
    else
        parent->right = new_node;

    return root;
}

/* BST 탐색. 방문한 노드 하나당 비교 1회. 찾으면 1, 못 찾으면 0 */
int bst_search(Node *root, int key, int *cmp)
{
    Node *cur = root;
    *cmp = 0;

    while (cur != NULL) {
        (*cmp)++;
        if (key == cur->data)
            return 1;
        else if (key < cur->data)
            cur = cur->left;
        else
            cur = cur->right;
    }
    return 0;
}

/* 순차 탐색. 배열 원소 하나당 비교 1회. 찾으면 1, 못 찾으면 0 */
int seq_search(int arr[], int n, int key, int *cmp)
{
    int i;
    *cmp = 0;

    for (i = 0; i < n; i++) {
        (*cmp)++;
        if (arr[i] == key)
            return 1;
    }
    return 0;
}

/* 트리 높이 (노드 1개짜리 트리 = 1) */
int bst_height(Node *root)
{
    int lh, rh;
    if (root == NULL)
        return 0;
    lh = bst_height(root->left);
    rh = bst_height(root->right);
    return (lh > rh ? lh : rh) + 1;
}

void bst_free(Node *root)
{
    if (root == NULL)
        return;
    bst_free(root->left);
    bst_free(root->right);
    free(root);
}

int main(void)
{
    int arr[DATA_COUNT];
    int keys[SEARCH_COUNT];
    int used[MAX_VALUE + 1] = {0};  /* 중복 검사용: used[v] == 1 이면 이미 나온 값 */
    Node *root = NULL;

    int build_cmp = 0;              /* BST 생성 비교 횟수 */
    int seq_total = 0, bst_total = 0;
    int found_count = 0;
    int i;

    srand((unsigned int)time(NULL));

    /* ---------- 1. 서로 다른 정수 100개 생성 -> 배열 + BST ---------- */
    i = 0;
    while (i < DATA_COUNT) {
        int v = rand() % (MAX_VALUE + 1);
        if (used[v])                /* 중복이면 버리고 다시 생성 */
            continue;
        used[v] = 1;
        arr[i] = v;                             /* 발생 순서 그대로 저장 */
        root = bst_insert(root, v, &build_cmp); /* 같은 순서로 BST 삽입 */
        i++;
    }

    printf("===== Generated %d Integers (in generation order) =====\n", DATA_COUNT);
    for (i = 0; i < DATA_COUNT; i++) {
        printf("%5d", arr[i]);
        if ((i + 1) % 10 == 0)
            printf("\n");
    }

    printf("\n===== BST Construction =====\n");
    printf("BST build comparisons : %d\n", build_cmp);
    printf("BST height            : %d\n", bst_height(root));

    /* ---------- 2. 탐색 대상 50개 생성 (중복/존재 여부 무관) ---------- */
    for (i = 0; i < SEARCH_COUNT; i++)
        keys[i] = rand() % (MAX_VALUE + 1);

    printf("\n===== %d Search Keys =====\n", SEARCH_COUNT);
    for (i = 0; i < SEARCH_COUNT; i++) {
        printf("%5d", keys[i]);
        if ((i + 1) % 10 == 0)
            printf("\n");
    }

    /* ---------- 3. 각 탐색 대상에 대해 두 탐색 수행 ---------- */
    printf("\n===== Search Results =====\n");
    printf("%4s  %6s  %-9s  %10s  %10s\n",
           "No", "Key", "Result", "Sequential", "BST");
    printf("----------------------------------------------\n");

    for (i = 0; i < SEARCH_COUNT; i++) {
        int seq_cmp, bst_cmp;
        int seq_found = seq_search(arr, DATA_COUNT, keys[i], &seq_cmp);
        int bst_found = bst_search(root, keys[i], &bst_cmp);

        if (seq_found != bst_found)     /* 두 결과는 항상 같아야 함 */
            printf("  (Warning: search results do not match)\n");

        printf("%4d  %6d  %-9s  %10d  %10d\n",
               i + 1, keys[i], seq_found ? "Found" : "Not Found",
               seq_cmp, bst_cmp);

        seq_total += seq_cmp;
        bst_total += bst_cmp;
        if (seq_found)
            found_count++;
    }

    /* ---------- 4. 통계 ---------- */
    printf("\n===== Statistics =====\n");
    printf("Number of searches : %d (Found %d / Not Found %d)\n",
           SEARCH_COUNT, found_count, SEARCH_COUNT - found_count);

    printf("\nSequential Search\n");
    printf("Total comparisons   : %d\n", seq_total);
    printf("Average comparisons : %.2f\n", (double)seq_total / SEARCH_COUNT);

    printf("\nBST Search\n");
    printf("Total comparisons   : %d\n", bst_total);
    printf("Average comparisons : %.2f\n", (double)bst_total / SEARCH_COUNT);

    printf("\nIncluding BST Build Cost\n");
    printf("BST build comparisons        : %d\n", build_cmp);
    printf("BST build + search (total)   : %d\n", build_cmp + bst_total);
    printf("Sequential search (total)    : %d\n", seq_total);

    bst_free(root);
    return 0;
}
