/*
 * 과제 06: 순차 탐색, 이진 탐색 트리와 AVL 트리의 성능 비교
 * 빌드: gcc -o hw06 hw06.c
 * 실행: ./hw06
 *
 * 숫자 비교 횟수를 세는 기준
 *  - 배열 : 새 값(또는 탐색 키)을 배열 원소 하나와 비교할 때마다 1회
 *  - BST  : 새 값(또는 탐색 키)을 노드 하나의 값과 비교할 때마다 1회
 *  - AVL  : BST 와 동일. 높이 계산, balance factor 확인, 회전은 세지 않음
 *  (한 노드에서 "같다 / 작다 / 크다" 를 가리는 것을 비교 1회로 본다)
 */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define DATA_COUNT   100    /* 난수 생성(삽입 시도) 횟수 */
#define SEARCH_COUNT 50     /* 탐색 대상 개수 */
#define MAX_VALUE    1000   /* 0 ~ 1000 */

/* ---------- 이진 탐색 트리 노드 ---------- */
typedef struct BstNode {
    int data;
    struct BstNode *left;
    struct BstNode *right;
} BstNode;

/* ---------- AVL 트리 노드 ---------- */
typedef struct AvlNode {
    int data;
    int height;             /* 이 노드를 루트로 하는 서브트리의 높이 (노드 1개 = 1) */
    struct AvlNode *left;
    struct AvlNode *right;
} AvlNode;

/* AVL 회전 횟수 (비교 횟수와는 별개로, 균형 유지에 든 작업량을 보기 위한 값) */
int ll_count = 0, rr_count = 0, lr_count = 0, rl_count = 0;

/* 정수를 4,523 처럼 천 단위 쉼표가 들어간 문자열로 바꾼다 (n >= 0) */
char *comma(int n, char *buf)
{
    char digits[16];
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

/* ====================== [1] 배열 ====================== */

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

/*
 * 배열 삽입. 처음부터 끝까지 순차 탐색으로 중복을 확인하고,
 * 없으면 맨 뒤에 추가한다. 비교 횟수는 *cmp 에 누적한다.
 * 삽입했으면 1, 중복이라 삽입하지 않았으면 0
 */
int array_insert(int arr[], int *n, int key, int *cmp)
{
    int c;
    int found = seq_search(arr, *n, key, &c);

    *cmp += c;
    if (found)
        return 0;

    arr[*n] = key;
    (*n)++;
    return 1;
}

/* ====================== [2] 이진 탐색 트리 ====================== */

BstNode *bst_create_node(int data)
{
    BstNode *n = (BstNode *)malloc(sizeof(BstNode));
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
 * BST 삽입. 노드 하나와 비교할 때마다 *cmp 를 1 증가시킨다.
 * 같은 값을 만나면 중복이므로 삽입하지 않는다.
 * 삽입했으면 *inserted = 1, 중복이면 0. 반환값은 트리의 루트.
 */
BstNode *bst_insert(BstNode *root, int key, int *cmp, int *inserted)
{
    BstNode *cur = root;
    BstNode *parent = NULL;
    BstNode *new_node;

    while (cur != NULL) {
        (*cmp)++;               /* key 와 cur->data 비교 1회 */
        if (key == cur->data) {
            *inserted = 0;      /* 중복 */
            return root;
        }
        parent = cur;
        if (key < cur->data)
            cur = cur->left;
        else
            cur = cur->right;
    }

    new_node = bst_create_node(key);
    *inserted = 1;

    if (parent == NULL)         /* 빈 트리: 비교 없이 루트가 됨 */
        return new_node;

    if (key < parent->data)
        parent->left = new_node;
    else
        parent->right = new_node;
    return root;
}

/* BST 탐색. 방문한 노드 하나당 비교 1회. 찾으면 1, 못 찾으면 0 */
int bst_search(BstNode *root, int key, int *cmp)
{
    BstNode *cur = root;
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

/* 트리 높이: 빈 트리 0, 루트만 있으면 1 */
int bst_height(BstNode *root)
{
    int lh, rh;
    if (root == NULL)
        return 0;
    lh = bst_height(root->left);
    rh = bst_height(root->right);
    return (lh > rh ? lh : rh) + 1;
}

void bst_free(BstNode *root)
{
    if (root == NULL)
        return;
    bst_free(root->left);
    bst_free(root->right);
    free(root);
}

/* ====================== [3] AVL 트리 ====================== */

AvlNode *avl_create_node(int data)
{
    AvlNode *n = (AvlNode *)malloc(sizeof(AvlNode));
    if (n == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }
    n->data = data;
    n->height = 1;
    n->left = NULL;
    n->right = NULL;
    return n;
}

/* 노드에 저장해 둔 높이를 읽는다 (빈 트리 = 0) */
int avl_height(AvlNode *n)
{
    return (n == NULL) ? 0 : n->height;
}

/* 자식들의 높이로 자기 높이를 다시 계산한다 */
void avl_update_height(AvlNode *n)
{
    int lh = avl_height(n->left);
    int rh = avl_height(n->right);
    n->height = (lh > rh ? lh : rh) + 1;
}

/* balance factor = 왼쪽 서브트리 높이 - 오른쪽 서브트리 높이 */
int avl_balance(AvlNode *n)
{
    return (n == NULL) ? 0 : avl_height(n->left) - avl_height(n->right);
}

/*
 * 오른쪽 회전 (LL 상황에서 사용)
 *
 *        y              x
 *       / \            / \
 *      x   C   ->     A   y
 *     / \                / \
 *    A   B              B   C
 */
AvlNode *rotate_right(AvlNode *y)
{
    AvlNode *x = y->left;
    AvlNode *b = x->right;

    x->right = y;
    y->left = b;

    avl_update_height(y);       /* 아래로 내려간 y 부터 갱신 */
    avl_update_height(x);
    return x;                   /* 새 서브트리 루트 */
}

/*
 * 왼쪽 회전 (RR 상황에서 사용)
 *
 *      x                  y
 *     / \                / \
 *    A   y       ->     x   C
 *       / \            / \
 *      B   C          A   B
 */
AvlNode *rotate_left(AvlNode *x)
{
    AvlNode *y = x->right;
    AvlNode *b = y->left;

    y->left = x;
    x->right = b;

    avl_update_height(x);
    avl_update_height(y);
    return y;
}

/*
 * node 의 균형이 깨졌으면 회전으로 복구하고, 서브트리의 새 루트를 돌려준다.
 * 어떤 회전을 할지는 balance factor 만 보고 정하므로 숫자 비교는 발생하지 않는다.
 */
AvlNode *avl_rebalance(AvlNode *node)
{
    int bf;

    avl_update_height(node);
    bf = avl_balance(node);

    if (bf > 1) {                           /* 왼쪽이 2 이상 높음 */
        if (avl_balance(node->left) >= 0) { /* LL: 왼쪽 자식의 왼쪽에 삽입됨 */
            ll_count++;
            return rotate_right(node);
        }
        lr_count++;                         /* LR: 왼쪽 자식의 오른쪽에 삽입됨 */
        node->left = rotate_left(node->left);
        return rotate_right(node);
    }

    if (bf < -1) {                          /* 오른쪽이 2 이상 높음 */
        if (avl_balance(node->right) <= 0) {/* RR: 오른쪽 자식의 오른쪽에 삽입됨 */
            rr_count++;
            return rotate_left(node);
        }
        rl_count++;                         /* RL: 오른쪽 자식의 왼쪽에 삽입됨 */
        node->right = rotate_right(node->right);
        return rotate_left(node);
    }

    return node;                            /* 균형 유지 중: 그대로 */
}

/*
 * AVL 삽입 (재귀). 삽입 위치를 찾는 방법은 BST 와 같고,
 * 노드 하나와 비교할 때마다 *cmp 를 1 증가시킨다.
 * 삽입 후 재귀에서 돌아오는 길에 조상 노드들의 균형을 복구한다.
 * 삽입했으면 *inserted = 1, 중복이면 0. 반환값은 서브트리의 (새) 루트.
 */
AvlNode *avl_insert(AvlNode *node, int key, int *cmp, int *inserted)
{
    if (node == NULL) {         /* 삽입 위치 도착 */
        *inserted = 1;
        return avl_create_node(key);
    }

    (*cmp)++;                   /* key 와 node->data 비교 1회 */
    if (key == node->data) {
        *inserted = 0;          /* 중복 */
        return node;
    }

    if (key < node->data)
        node->left = avl_insert(node->left, key, cmp, inserted);
    else
        node->right = avl_insert(node->right, key, cmp, inserted);

    if (!*inserted)             /* 삽입이 없었으면 트리 모양도 그대로 */
        return node;

    return avl_rebalance(node);
}

/* AVL 탐색. 방법은 BST 탐색과 동일. 찾으면 1, 못 찾으면 0 */
int avl_search(AvlNode *root, int key, int *cmp)
{
    AvlNode *cur = root;
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

void avl_free(AvlNode *root)
{
    if (root == NULL)
        return;
    avl_free(root->left);
    avl_free(root->right);
    free(root);
}

/* ====================== 출력 보조 ====================== */

void print_numbers(int a[], int n)
{
    int i;
    for (i = 0; i < n; i++) {
        printf("%5d", a[i]);
        if ((i + 1) % 10 == 0 || i == n - 1)
            printf("\n");
    }
}

int main(void)
{
    int gen[DATA_COUNT];            /* 생성된 난수 100개 (출력용) */
    int arr[DATA_COUNT];            /* [1] 정수 배열 */
    int arr_len = 0;
    BstNode *bst_root = NULL;       /* [2] 이진 탐색 트리 */
    AvlNode *avl_root = NULL;       /* [3] AVL 트리 */
    int bst_count = 0, avl_count = 0;

    int arr_build = 0, bst_build = 0, avl_build = 0;    /* 생성 비교 횟수 */
    int seq_total = 0, bst_total = 0, avl_total = 0;    /* 탐색 비교 횟수 */

    int keys[SEARCH_COUNT];
    int seq_cmp[SEARCH_COUNT], bst_cmp[SEARCH_COUNT], avl_cmp[SEARCH_COUNT];
    int found[SEARCH_COUNT];
    int found_count = 0;
    int bst_h, avl_h;
    char b1[32], b2[32], b3[32];
    int i;

    srand((unsigned int)time(NULL));

    /* ---------- 1. 난수 100개 생성 -> 같은 순서로 배열, BST, AVL 에 삽입 ---------- */
    for (i = 0; i < DATA_COUNT; i++) {
        int v = rand() % (MAX_VALUE + 1);
        int ins;

        gen[i] = v;

        array_insert(arr, &arr_len, v, &arr_build);

        bst_root = bst_insert(bst_root, v, &bst_build, &ins);
        bst_count += ins;

        avl_root = avl_insert(avl_root, v, &avl_build, &ins);
        avl_count += ins;
    }

    bst_h = bst_height(bst_root);
    avl_h = avl_height(avl_root);

    printf("===== Generated %d Integers (in generation order) =====\n", DATA_COUNT);
    print_numbers(gen, DATA_COUNT);

    printf("\n===== Stored Values in Array (first-occurrence order) =====\n");
    print_numbers(arr, arr_len);

    if (arr_len != bst_count || arr_len != avl_count)   /* 셋은 항상 같아야 함 */
        printf("(Warning: stored counts do not match: Array %d, BST %d, AVL %d)\n",
               arr_len, bst_count, avl_count);

    printf("\nGenerated values  : %d\n", DATA_COUNT);
    printf("Stored values     : %d\n", arr_len);
    printf("Duplicates skipped: %d\n", DATA_COUNT - arr_len);

    printf("\nConstruction\n");
    printf("Array comparisons : %5s\n", comma(arr_build, b1));
    printf("BST comparisons   : %5s\n", comma(bst_build, b2));
    printf("AVL comparisons   : %5s\n", comma(avl_build, b3));
    printf("AVL rotations     : %5d (LL %d, RR %d, LR %d, RL %d)\n",
           ll_count + rr_count + lr_count + rl_count,
           ll_count, rr_count, lr_count, rl_count);

    printf("\nStructure\n");
    printf("Array length : %d\n", arr_len);
    printf("BST height   : %d\n", bst_h);
    printf("AVL height   : %d\n", avl_h);

    /* ---------- 2. 탐색 대상 50개 생성 (존재 여부 무관) ---------- */
    for (i = 0; i < SEARCH_COUNT; i++)
        keys[i] = rand() % (MAX_VALUE + 1);

    printf("\n===== %d Search Keys =====\n", SEARCH_COUNT);
    print_numbers(keys, SEARCH_COUNT);

    /* ---------- 3. 각 탐색 대상에 대해 세 가지 탐색 수행 ---------- */
    printf("\n===== Search Details =====\n");
    for (i = 0; i < SEARCH_COUNT; i++) {
        int seq_found = seq_search(arr, arr_len, keys[i], &seq_cmp[i]);
        int bst_found = bst_search(bst_root, keys[i], &bst_cmp[i]);
        int avl_found = avl_search(avl_root, keys[i], &avl_cmp[i]);

        printf("\nSearch Key : %d\n", keys[i]);

        printf("\nSequential Search\n");
        printf("Result      : %s\n", seq_found ? "Found" : "Not Found");
        printf("Comparisons : %d\n", seq_cmp[i]);

        printf("\nBST Search\n");
        printf("Result      : %s\n", bst_found ? "Found" : "Not Found");
        printf("Comparisons : %d\n", bst_cmp[i]);

        printf("\nAVL Search\n");
        printf("Result      : %s\n", avl_found ? "Found" : "Not Found");
        printf("Comparisons : %d\n", avl_cmp[i]);

        if (seq_found != bst_found || seq_found != avl_found)   /* 항상 같아야 함 */
            printf("(Warning: search results do not match)\n");

        found[i] = seq_found;
        found_count += seq_found;
        seq_total += seq_cmp[i];
        bst_total += bst_cmp[i];
        avl_total += avl_cmp[i];
    }

    /* ---------- 4. 탐색 결과 요약표 ---------- */
    printf("\n===== Search Summary Table =====\n");
    printf("%4s  %6s  %-9s  %10s  %5s  %5s\n",
           "No", "Key", "Result", "Sequential", "BST", "AVL");
    printf("-----------------------------------------------------\n");
    for (i = 0; i < SEARCH_COUNT; i++)
        printf("%4d  %6d  %-9s  %10d  %5d  %5d\n",
               i + 1, keys[i], found[i] ? "Found" : "Not Found",
               seq_cmp[i], bst_cmp[i], avl_cmp[i]);

    /* ---------- 5. 전체 통계 ---------- */
    printf("\n===== Summary =====\n");
    printf("Stored values : %d\n", arr_len);

    printf("\nConstruction\n");
    printf("Array comparisons : %5s\n", comma(arr_build, b1));
    printf("BST comparisons   : %5s\n", comma(bst_build, b2));
    printf("AVL comparisons   : %5s\n", comma(avl_build, b3));

    printf("\nStructure\n");
    printf("Array length : %d\n", arr_len);
    printf("BST height   : %d\n", bst_h);
    printf("AVL height   : %d\n", avl_h);

    printf("\nSearches : %d (Found %d / Not Found %d)\n",
           SEARCH_COUNT, found_count, SEARCH_COUNT - found_count);

    printf("\nSequential Search\n");
    printf("Total comparisons   : %s\n", comma(seq_total, b1));
    printf("Average comparisons : %.2f\n", (double)seq_total / SEARCH_COUNT);

    printf("\nBST Search\n");
    printf("Total comparisons   : %s\n", comma(bst_total, b1));
    printf("Average comparisons : %.2f\n", (double)bst_total / SEARCH_COUNT);

    printf("\nAVL Search\n");
    printf("Total comparisons   : %s\n", comma(avl_total, b1));
    printf("Average comparisons : %.2f\n", (double)avl_total / SEARCH_COUNT);

    bst_free(bst_root);
    avl_free(avl_root);
    return 0;
}