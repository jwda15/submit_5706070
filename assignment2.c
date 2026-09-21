/*
 * bintree.c  -  과제 02 : 이진트리 프로그램 구현
 *
 *   괄호 표기법으로 이진트리를 입력받아
 *     1. 배열 구현   (인덱스 1이 루트, 왼쪽 자식 2i, 오른쪽 자식 2i+1)
 *     2. 연결 구현   (노드 + left/right 포인터)
 *   두 방식으로 표현하고 출력/정보/형태판별/메모리비교/관계조회를 수행한다.
 *
 *   괄호 표기법 (이진트리)
 *     A(B,C)  : 왼쪽 B, 오른쪽 C
 *     A(B)    : 왼쪽 자식만   (A(B,) 도 허용)
 *     A(,C)   : 오른쪽 자식만
 *     단말 노드는 괄호 없이 이름만.  A() , A(,) 는 오류.
 *
 *   높이 : 루트의 레벨을 1로 둔다 (노드 1개 트리의 높이 = 1)
 *
 *   gcc -o bintree bintree.c
 */
#define _CRT_SECURE_NO_WARNINGS
/* 한글 깨짐 방지 : 이 파일은 UTF-8(BOM) 로 저장되어 있고,
 * MSVC 는 아래 pragma 로 실행 문자셋을 UTF-8 로 맞춘다 (/utf-8 옵션 없이도 동작) */
#ifdef _MSC_VER
#pragma execution_character_set("utf-8")
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#ifdef _WIN32
#include <windows.h>                   /* SetConsoleOutputCP */
#endif

#define MAXLEN   512
#define MAXLEVEL 27                     /* 노드 26개 편향 트리의 최대 높이 + 1 */

/* ================================================================== */
/*  공통 : 연결 노드 정의                                              */
/* ================================================================== */
typedef struct Node {
    char data;
    struct Node *left;
    struct Node *right;
} Node;

/* ================================================================== */
/*  공통 : 괄호 표기법 파서                                            */
/*    tree := NODE [ '(' [tree] [ ',' [tree] ] ')' ]                   */
/*    arr 가 NULL 이 아니면 배열에, buildLink 가 1 이면 연결 노드로 만든다 */
/* ================================================================== */
typedef struct {
    const char *s;
    int  pos;
    int  used[26];
    int  height;
    char *arr;
    int  buildLink;
    char err[160];
} Parser;

static int parseTree(Parser *p, int level, int idx, Node **out)
{
    char c = p->s[p->pos];
    Node *nd = NULL;
    int hasL = 0, hasR = 0;

    if (c < 'A' || c > 'Z') {
        if (c == '\0') sprintf(p->err, "입력이 중간에 끝났습니다. 노드가 와야 합니다.");
        else sprintf(p->err, "%d번째 문자 '%c' : 노드(영문 대문자)가 와야 합니다.", p->pos + 1, c);
        return 0;
    }
    if (p->used[c - 'A']) {
        sprintf(p->err, "%d번째 문자 : 노드 '%c' 가 중복되었습니다.", p->pos + 1, c);
        return 0;
    }
    p->used[c - 'A'] = 1;
    p->pos++;
    if (level > p->height) p->height = level;

    if (p->arr) p->arr[idx] = c;
    if (p->buildLink) {
        nd = (Node *)malloc(sizeof(Node));
        nd->data = c;
        nd->left = nd->right = NULL;
    }
    if (out) *out = nd;

    if (p->s[p->pos] != '(') return 1;          /* 단말 노드 */
    p->pos++;

    /* 왼쪽 자식 (비어 있을 수 있음) */
    if (p->s[p->pos] != ',' && p->s[p->pos] != ')') {
        if (!parseTree(p, level + 1, idx * 2, nd ? &nd->left : NULL)) return 0;
        hasL = 1;
    }
    /* 오른쪽 자식 */
    if (p->s[p->pos] == ',') {
        p->pos++;
        if (p->s[p->pos] != ')') {
            if (!parseTree(p, level + 1, idx * 2 + 1, nd ? &nd->right : NULL)) return 0;
            hasR = 1;
        }
    }
    if (p->s[p->pos] != ')') {
        if (p->s[p->pos] == ',')
            sprintf(p->err, "%d번째 문자 : 이진트리는 자식이 최대 2개입니다.", p->pos + 1);
        else if (p->s[p->pos] == '\0')
            sprintf(p->err, "괄호가 닫히지 않았습니다.");
        else
            sprintf(p->err, "%d번째 문자 '%c' : ',' 또는 ')' 가 와야 합니다.", p->pos + 1, p->s[p->pos]);
        return 0;
    }
    p->pos++;
    if (!hasL && !hasR) {
        sprintf(p->err, "%d번째 문자 : 자식이 없는 노드에 괄호가 있습니다.", p->pos);
        return 0;
    }
    return 1;
}

static int runParser(Parser *p, const char *s, char *arr, int buildLink, Node **root)
{
    memset(p, 0, sizeof(*p));
    p->s = s;
    p->arr = arr;
    p->buildLink = buildLink;
    if (!parseTree(p, 1, 1, root)) return 0;
    if (p->s[p->pos] != '\0') {
        sprintf(p->err, "%d번째 문자 '%c' : 트리가 끝난 뒤에 문자가 남아 있습니다.",
                p->pos + 1, p->s[p->pos]);
        return 0;
    }
    return 1;
}

/* 공백 제거 + 대문자 변환 */
static void normalize(char *s)
{
    int i, j = 0;
    for (i = 0; s[i]; i++)
        if (!isspace((unsigned char)s[i]))
            s[j++] = (char)toupper((unsigned char)s[i]);
    s[j] = '\0';
}

/* ================================================================== */
/*  1. 배열 구현                                                        */
/*     a[1..size-1] 사용, a[i]==0 이면 빈 자리.  size = 2^height        */
/* ================================================================== */
typedef struct {
    char *a;
    int  size;
} ArrTree;

static int A_has(const ArrTree *t, int i)
{
    return i >= 1 && i < t->size && t->a[i] != 0;
}

static int levelOf(int i)
{
    int lv = 0;
    while (i) { lv++; i >>= 1; }
    return lv;
}

/* 한쪽 자식만 있으면 빈 쪽을 (null) 로 찍어서 왼쪽/오른쪽을 구분한다 */
static void A_printRec(const ArrTree *t, int i, char *prefix, int pipe)
{
    int ch[2] = { 2 * i, 2 * i + 1 };
    int k;
    size_t len;

    if (!A_has(t, ch[0]) && !A_has(t, ch[1])) return;

    for (k = 0; k < 2; k++) {
        printf("%s+---", prefix);
        if (!A_has(t, ch[k])) { printf("(null)\n"); continue; }
        printf("%c\n", t->a[ch[k]]);
        len = strlen(prefix);
        strcat(prefix, (pipe && k == 0) ? "|   " : "    ");
        A_printRec(t, ch[k], prefix, pipe);
        prefix[len] = '\0';
    }
}

static void A_print(const ArrTree *t, int pipe)
{
    char prefix[4 * MAXLEVEL + 1] = "";
    printf("%c\n", t->a[1]);
    A_printRec(t, 1, prefix, pipe);
}

static void A_info(const ArrTree *t, int *n, int *leaf, int *h, int *deg)
{
    int i, c, lv;
    *n = *leaf = *h = *deg = 0;
    for (i = 1; i < t->size; i++) {
        if (!t->a[i]) continue;
        (*n)++;
        c = A_has(t, 2 * i) + A_has(t, 2 * i + 1);
        if (c == 0) (*leaf)++;
        if (c > *deg) *deg = c;
        lv = levelOf(i);
        if (lv > *h) *h = lv;
    }
}

/* 완전 : 1..n 번 자리가 빈칸 없이 모두 차 있어야 한다 */
static int A_isComplete(const ArrTree *t, int n)
{
    int i;
    for (i = 1; i <= n; i++)
        if (!t->a[i]) return 0;
    return 1;
}

/* 포화 : 1..2^h-1 전부 참 = n == 2^h - 1 */
static int A_isFull(int n, int h)
{
    return n == (1 << h) - 1;
}

/* 편향 : 0 아님 / 1 왼쪽 편향 / 2 오른쪽 편향 (노드 2개 이상일 때만) */
static int A_skew(const ArrTree *t, int n)
{
    int i, allL = 1, allR = 1;
    if (n < 2) return 0;
    for (i = 1; i < t->size; i++) {
        if (!t->a[i]) continue;
        if (A_has(t, 2 * i + 1)) allL = 0;
        if (A_has(t, 2 * i))     allR = 0;
    }
    return allL ? 1 : (allR ? 2 : 0);
}

static void A_dump(const ArrTree *t)
{
    int i;
    if (t->size > 64) {
        printf("(배열 크기 %d 칸 - 너무 커서 내용 출력 생략)\n", t->size - 1);
        return;
    }
    printf("배열 내용 : ");
    for (i = 1; i < t->size; i++)
        printf("[%d]%c ", i, t->a[i] ? t->a[i] : '-');
    printf("\n");
}

/* ================================================================== */
/*  2. 연결 구현                                                        */
/* ================================================================== */
static void L_printRec(const Node *nd, char *prefix, int pipe)
{
    const Node *ch[2];
    int k;
    size_t len;

    if (!nd->left && !nd->right) return;
    ch[0] = nd->left;
    ch[1] = nd->right;

    for (k = 0; k < 2; k++) {
        printf("%s+---", prefix);
        if (!ch[k]) { printf("(null)\n"); continue; }
        printf("%c\n", ch[k]->data);
        len = strlen(prefix);
        strcat(prefix, (pipe && k == 0) ? "|   " : "    ");
        L_printRec(ch[k], prefix, pipe);
        prefix[len] = '\0';
    }
}

static void L_print(const Node *root, int pipe)
{
    char prefix[4 * MAXLEVEL + 1] = "";
    printf("%c\n", root->data);
    L_printRec(root, prefix, pipe);
}

static int L_count(const Node *nd)
{
    if (!nd) return 0;
    return 1 + L_count(nd->left) + L_count(nd->right);
}

static int L_leaf(const Node *nd)
{
    if (!nd) return 0;
    if (!nd->left && !nd->right) return 1;
    return L_leaf(nd->left) + L_leaf(nd->right);
}

static int L_height(const Node *nd)
{
    int hl, hr;
    if (!nd) return 0;
    hl = L_height(nd->left);
    hr = L_height(nd->right);
    return 1 + (hl > hr ? hl : hr);
}

static int L_degree(const Node *nd)
{
    int d, dl, dr;
    if (!nd) return 0;
    d  = (nd->left != NULL) + (nd->right != NULL);
    dl = L_degree(nd->left);
    dr = L_degree(nd->right);
    if (dl > d) d = dl;
    if (dr > d) d = dr;
    return d;
}

/* 완전 : 각 노드에 배열식 번호를 매겼을 때 모든 번호가 n 이하여야 한다 */
static int L_isCompleteRec(const Node *nd, int idx, int n)
{
    if (!nd) return 1;
    if (idx > n) return 0;
    return L_isCompleteRec(nd->left, 2 * idx, n) &&
           L_isCompleteRec(nd->right, 2 * idx + 1, n);
}

static int L_isComplete(const Node *root, int n)
{
    return L_isCompleteRec(root, 1, n);
}

static int L_isFull(int n, int h)
{
    return n == (1 << h) - 1;
}

static void L_skewRec(const Node *nd, int *allL, int *allR)
{
    if (!nd) return;
    if (nd->right) *allL = 0;
    if (nd->left)  *allR = 0;
    L_skewRec(nd->left, allL, allR);
    L_skewRec(nd->right, allL, allR);
}

static int L_skew(const Node *root, int n)
{
    int allL = 1, allR = 1;
    if (n < 2) return 0;
    L_skewRec(root, &allL, &allR);
    return allL ? 1 : (allR ? 2 : 0);
}

static void L_free(Node *nd)
{
    if (!nd) return;
    L_free(nd->left);
    L_free(nd->right);
    free(nd);
}

/* ================================================================== */
/*  공통 출력                                                           */
/* ================================================================== */
static const char *YN(int b) { return b ? "예" : "아니오"; }

static const char *skewName(int s)
{
    return s == 1 ? "예 (왼쪽 편향)" : (s == 2 ? "예 (오른쪽 편향)" : "아니오");
}

static void printInfo(int n, int leaf, int h, int deg)
{
    printf("1. 전체 노드의 수   : %d\n", n);
    printf("2. 단말 노드의 수   : %d\n", leaf);
    printf("3. 비단말 노드의 수 : %d\n", n - leaf);
    printf("4. 트리의 높이      : %d\n", h);
    printf("5. 트리의 차수      : %d\n", deg);
}

static void printShape(int comp, int full, int skew)
{
    printf("완전 이진트리 : %s\n", YN(comp));
    printf("포화 이진트리 : %s\n", YN(full));
    printf("편향 이진트리 : %s\n", skewName(skew));
}

/* ================================================================== */
/*  3-[1] 메모리 사용량 비교                                            */
/*    배열 : 2^h 칸 * sizeof(char)   (0번 칸 포함 실제 할당량)            */
/*    연결 : n개 * sizeof(Node)      (malloc 헤더 오버헤드 제외)          */
/* ================================================================== */
/* UTF-8 에서 한글 한 글자는 3바이트라 "%-12s" 로는 칸이 맞지 않는다.
   화면에 보이는 폭(한글 2칸, 영문 1칸)을 직접 세어 공백을 채운다. */
static void printPadded(const char *s, int width)
{
    int i, w = 0;
    for (i = 0; s[i]; i++) {
        unsigned char c = (unsigned char)s[i];
        if ((c & 0xC0) != 0x80)                 /* 이어지는 바이트는 세지 않음 */
            w += (c < 0x80) ? 1 : 2;
    }
    printf("%s", s);
    for (i = w; i < width; i++) putchar(' ');
}

static void memRow(const char *label, const char *expr)
{
    char buf[MAXLEN];
    Parser p;
    Node *root = NULL;
    int n, h, slots;
    size_t arrBytes, linkBytes;

    strcpy(buf, expr);
    normalize(buf);
    if (!runParser(&p, buf, NULL, 1, &root)) {
        printPadded(label, 10);
        printf(" 파싱 오류: %s\n", p.err);
        return;
    }
    n = L_count(root);
    h = p.height;
    slots = (1 << h) - 1;
    arrBytes  = (size_t)(1 << h) * sizeof(char);
    linkBytes = (size_t)n * sizeof(Node);

    printPadded(label, 12);
    printf("  n=%-3d h=%-3d | 배열: %8d칸 %10zu B (사용률 %6.2f%%) | 연결: %6zu B\n",
           n, h, slots, arrBytes, 100.0 * n / slots, linkBytes);
    printf("             입력: %s\n", buf);
    L_free(root);
}

/* ================================================================== */
/*  3-[2] 부모 / 자식 / 형제 조회                                       */
/* ================================================================== */
#define NM(c) ((c) ? (c) : '-')

static void A_relation(const ArrTree *t, char x)
{
    long ops = 0;
    int i, found = 0;

    for (i = 1; i < t->size; i++) {
        ops++;
        if (t->a[i] == x) { found = 1; break; }
    }
    printf("[배열] ");
    if (!found) { printf("노드 %c 없음 (비교 %ld회)\n", x, ops); return; }

    printf("인덱스 %d, 위치 탐색 비교 %ld회, 이후 관계 계산은 인덱스 연산 O(1)\n", i, ops);
    printf("  부모        : %c\n", NM(i > 1 ? t->a[i / 2] : 0));
    printf("  왼쪽 자식   : %c\n", NM(A_has(t, 2 * i) ? t->a[2 * i] : 0));
    printf("  오른쪽 자식 : %c\n", NM(A_has(t, 2 * i + 1) ? t->a[2 * i + 1] : 0));
    printf("  형제        : %c\n", NM(i > 1 && A_has(t, i ^ 1) ? t->a[i ^ 1] : 0));
}

/* 부모 포인터가 없으므로 루트부터 내려가며 찾고, 찾는 김에 부모를 기억한다 */
static Node *L_find(Node *nd, char x, Node *par, Node **parOut, long *ops)
{
    Node *r;
    if (!nd) return NULL;
    (*ops)++;
    if (nd->data == x) { *parOut = par; return nd; }
    r = L_find(nd->left, x, nd, parOut, ops);
    if (r) return r;
    return L_find(nd->right, x, nd, parOut, ops);
}

static void L_relation(Node *root, char x)
{
    long ops = 0;
    Node *par = NULL, *nd, *sib = NULL;

    nd = L_find(root, x, NULL, &par, &ops);
    printf("[연결] ");
    if (!nd) { printf("노드 %c 없음 (방문 %ld회)\n", x, ops); return; }

    if (par) sib = (par->left == nd) ? par->right : par->left;
    printf("탐색 방문 %ld회 (부모/형제는 탐색 중 기억한 부모로 계산)\n", ops);
    printf("  부모        : %c\n", NM(par ? par->data : 0));
    printf("  왼쪽 자식   : %c\n", NM(nd->left ? nd->left->data : 0));
    printf("  오른쪽 자식 : %c\n", NM(nd->right ? nd->right->data : 0));
    printf("  형제        : %c\n", NM(sib ? sib->data : 0));
}

/* ================================================================== */
/*  main                                                                */
/* ================================================================== */
int main(void)
{
    char line[MAXLEN];
    Parser p;
    ArrTree at;
    Node *root = NULL;
    int n, leaf, h, deg;

#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);        /* 콘솔을 UTF-8 로 : 한글 출력 */
#endif

    printf("이진트리를 괄호 표기법으로 입력하세요 (예: A(B(D,E),C(,F))) : ");
    if (!fgets(line, sizeof(line), stdin)) return 1;
    normalize(line);

    /* 1차 : 문법 검사 + 높이 계산 */
    if (!runParser(&p, line, NULL, 0, NULL)) {
        printf("오류: %s\n", p.err);
        return 1;
    }

    /* 2차 : 배열 구성 (높이 h 이면 인덱스 1..2^h-1 필요) */
    at.size = 1 << p.height;
    at.a = (char *)calloc(at.size, sizeof(char));
    if (!at.a) { printf("오류: 배열 메모리 할당 실패\n"); return 1; }
    runParser(&p, line, at.a, 0, NULL);

    /* 3차 : 연결 구조 구성 */
    runParser(&p, line, NULL, 1, &root);

    /* ---------------- 1. 배열 구현 ---------------- */
    printf("\n==================== 1. 배열 구현 ====================\n");
    A_dump(&at);
    printf("\n[1] 이진트리 출력\n");
    A_print(&at, 0);
    printf("\n('|' 사용)\n");
    A_print(&at, 1);

    printf("\n[2] 트리 정보\n");
    A_info(&at, &n, &leaf, &h, &deg);
    printInfo(n, leaf, h, deg);

    printf("\n[3] 형태 판별\n");
    printShape(A_isComplete(&at, n), A_isFull(n, h), A_skew(&at, n));

    /* ---------------- 2. 연결 구현 ---------------- */
    printf("\n==================== 2. 연결 구현 ====================\n");
    printf("[1] 이진트리 출력\n");
    L_print(root, 0);
    printf("\n('|' 사용)\n");
    L_print(root, 1);

    printf("\n[2] 트리 정보\n");
    n = L_count(root);
    leaf = L_leaf(root);
    h = L_height(root);
    deg = L_degree(root);
    printInfo(n, leaf, h, deg);

    printf("\n[3] 형태 판별\n");
    printShape(L_isComplete(root, n), L_isFull(n, h), L_skew(root, n));

    /* ---------------- 3-[1] 메모리 ---------------- */
    printf("\n==================== 3-[1] 메모리 사용량 ====================\n");
    printf("sizeof(char) = %zu B, sizeof(Node) = %zu B, sizeof(Node*) = %zu B\n",
           sizeof(char), sizeof(Node), sizeof(Node *));
    memRow("입력 트리",   line);
    memRow("일반",       "A(B(D,E(H)),C(,F(G)))");
    memRow("완전",       "A(B(D(H,I),E(J)),C(F,G))");
    memRow("포화",       "A(B(D(H,I),E(J,K)),C(F(L,M),G(N,O)))");
    memRow("왼쪽편향",   "A(B(C(D(E(F(G(H(I(J)))))))))");
    memRow("오른쪽편향", "A(,B(,C(,D(,E(,F(,G(,H(,I(,J)))))))))");

    /* ---------------- 3-[2] 관계 조회 ---------------- */
    printf("\n==================== 3-[2] 부모/자식/형제 조회 ====================\n");
    printf("('-' 는 없음)\n");
    for (;;) {
        char q[MAXLEN];
        printf("\n조회할 노드 (종료: 엔터) : ");
        if (!fgets(q, sizeof(q), stdin)) break;
        normalize(q);
        if (q[0] == '\0' || q[0] == '0') break;
        if (q[0] < 'A' || q[0] > 'Z') { printf("영문자 한 글자를 입력하세요.\n"); continue; }
        A_relation(&at, q[0]);
        L_relation(root, q[0]);
    }

    free(at.a);
    L_free(root);
    return 0;
}