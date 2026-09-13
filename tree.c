#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <ctype.h> 

#define MAXLEN 1024

/* ---------------- 1) 입력 검증 ---------------- */
int validate(const char* s)
{
    int depth = 0, i;
    int used[26] = { 0 };
    char prev = 0;                 /* 0 = 문자열 시작 */

    if (s[0] == '\0') return 0;

    for (i = 0; s[i]; i++) {
        char c = s[i];
        if (c >= 'A' && c <= 'Z') {
            /* 노드는 맨 앞 / '(' 뒤 / ',' 뒤에만 올 수 있다 */
            if (!(prev == 0 || prev == '(' || prev == ',')) return 0;
            if (used[c - 'A']) return 0;          /* 같은 노드 이름 중복 */
            used[c - 'A'] = 1;
        }
        else if (c == '(') {
            if (!(prev >= 'A' && prev <= 'Z')) return 0;   /* '(' 앞은 노드 */
            depth++;
        }
        else if (c == ')') {
            if (!((prev >= 'A' && prev <= 'Z') || prev == ')')) return 0;
            if (--depth < 0) return 0;            /* 괄호 불균형 */
        }
        else if (c == ',') {
            if (!((prev >= 'A' && prev <= 'Z') || prev == ')')) return 0;
            if (depth == 0) return 0;             /* 루트는 하나뿐 */
        }
        else {
            return 0;                             /* 허용되지 않는 문자 */
        }
        prev = c;
    }
    if (depth != 0) return 0;
    if (!((prev >= 'A' && prev <= 'Z') || prev == ')')) return 0;
    return 1;
}

/* i번째 노드의 서브트리가 끝난 뒤 ','가 오는가 = 다음 형제가 있는가 */
int has_next_sibling(const char* s, int i)
{
    int j = i + 1, d = 0;
    if (s[j] == '(') {
        do {
            if (s[j] == '(') d++;
            else if (s[j] == ')') d--;
            j++;
        } while (d > 0);
    }
    return s[j] == ',';
}

int main(void)
{
    char s[MAXLEN];
    int i;

    /* 스택들 */
    char node_stack[MAXLEN];  int n_top = 0;   /* 현재 열려 있는(부모) 노드들 */
    int  cnt_stack[MAXLEN];   int c_top = 0;   /* 각 부모의 자식 수 카운터 */
    int  more[MAXLEN];                          /* 각 레벨에 다음 형제가 있는지 */

    int total = 0, leaf = 0, height = 0, degree = 0;
    char parentC = 0;                           /* 0 = 아직 못 찾음 */
    int  foundC = 0, rootIsC = 0;
    char childC[32]; int nchildC = 0;
    int depth;

    printf("트리의 괄호 표기법 입력: ");
    if (scanf("%1000s", s) != 1) {
        printf("입력 오류\n");
        return 1;
    }

    for (i = 0; s[i]; i++)
        s[i] = (char)toupper((unsigned char)s[i]);

    if (!validate(s)) {
        printf("오류: 올바른 트리의 괄호 표기법이 아닙니다.\n");
        return 1;
    }

    /* ---------------- 2) 한 번 스캔하며 정보 수집 ---------------- */
    for (i = 0; s[i]; i++) {
        char c = s[i];

        if (c >= 'A' && c <= 'Z') {
            total++;
            if (s[i + 1] != '(') leaf++;        /* 뒤에 '('가 없으면 단말 노드 */

            depth = n_top;                      /* 루트 depth = 0 */
            if (depth + 1 > height) height = depth + 1;

            if (n_top > 0) {
                cnt_stack[c_top - 1]++;         /* 부모의 자식 수 +1 */
                if (node_stack[n_top - 1] == 'C')   /* C의 자식 */
                    childC[nchildC++] = c;
                if (c == 'C') { foundC = 1; parentC = node_stack[n_top - 1]; }
            }
            else {
                if (c == 'C') { foundC = 1; rootIsC = 1; }
            }
        }
        else if (c == '(') {
            node_stack[n_top++] = s[i - 1];     /* 방금 읽은 노드가 부모가 됨 */
            cnt_stack[c_top++] = 0;
        }
        else if (c == ')') {
            if (cnt_stack[c_top - 1] > degree)  /* 차수 최댓값 갱신 */
                degree = cnt_stack[c_top - 1];
            c_top--;
            n_top--;
        }
    }

    /* ---------------- 3) 결과 출력 ---------------- */
    printf("\n입력된 트리 : %s\n\n", s);
    printf("전체 노드 수   : %d\n", total);
    printf("단말 노드 수   : %d\n", leaf);
    printf("비단말 노드 수 : %d\n", total - leaf);
    printf("트리의 높이    : %d\n", height);
    printf("트리의 차수    : %d\n", degree);

    if (!foundC)        printf("노드 C의 부모  : 노드 C가 없습니다\n");
    else if (rootIsC)   printf("노드 C의 부모  : 없음 (루트)\n");
    else                printf("노드 C의 부모  : %c\n", parentC);

    printf("노드 C의 자식  : ");
    if (!foundC)        printf("노드 C가 없습니다\n");
    else if (nchildC == 0) printf("없음 (단말 노드)\n");
    else {
        for (i = 0; i < nchildC; i++)
            printf("%c%s", childC[i], (i == nchildC - 1) ? "\n" : ", ");
    }

    /* ---------------- 4) 계층 구조 출력 (기본) ---------------- */
    printf("\n[트리 구조]\n");
    depth = 0;
    for (i = 0; s[i]; i++) {
        char c = s[i];
        if (c >= 'A' && c <= 'Z') {
            int k;
            if (depth == 0) {
                printf("%c\n", c);
            }
            else {
                for (k = 1; k < depth; k++)
                    printf("    ");          /* 공백 4칸만 */
                printf("+---%c\n", c);
            }
        }
        else if (c == '(') depth++;
        else if (c == ')')   depth--;
    }

    /* ---------------- 5) 계층 구조 출력 (연결선 포함) ---------------- */
    printf("\n[트리 구조 - 연결선]\n");
    depth = 0;
    for (i = 0; s[i]; i++) {
        char c = s[i];
        if (c >= 'A' && c <= 'Z') {
            int k;
            more[depth] = has_next_sibling(s, i);   /* 내 레벨 형제 유무 기록 */
            if (depth == 0) {
                printf("%c\n", c);
            }
            else {
                for (k = 1; k < depth; k++)
                    printf(more[k] ? "|   " : "    ");
                printf("+---%c\n", c);
            }
        }
        else if (c == '(') depth++;
        else if (c == ')')   depth--;
    }

    return 0;
}
