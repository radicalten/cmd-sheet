/*
 *  TinySheet – single-file terminal spreadsheet
 *  gcc -std=c99 -Wall -Wextra -o tinysheet tinysheet.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define COLS 26
#define ROWS 100
#define BUF  64

typedef struct {
    char formula[BUF];   /* raw user text, e.g. "=A1+B2" */
    int  value;          /* last computed value */
    int  dirty;          /* 1 = needs recalc */
} Cell;

static Cell sheet[COLS][ROWS];
static int curx = 0, cury = 0;   /* 0-based */
static int quit = 0;

/* ---------- screen helpers ---------- */
void movecursor(int y, int x) { printf("\033[%d;%dH", y, x); }
void clearscreen(void)        { printf("\033[2J"); }
void showcursor(void)         { printf("\033[?25h"); }
void hidecursor(void)         { printf("\033[?25l"); }

/* ---------- expression evaluator ---------- */
static int eval(const char *expr, int *ok);

static int parseatom(const char *s, int *pos, int *ok) {
    int val = 0;
    while (isspace(s[*pos])) (*pos)++;
    if (s[*pos]=='=') (*pos)++; /* skip leading = if present */
    while (isspace(s[*pos])) (*pos)++;

    if (s[*pos]>='A' && s[*pos]<='Z') {            /* cell reference */
        int c = s[*pos]-'A'; (*pos)++;
        int r = 0;
        while (isdigit(s[*pos])) { r = r*10 + (s[*pos]-'0'); (*pos)++; }
        if (r<1 || r>ROWS) { *ok=0; return 0; }
        return sheet[c][r-1].value;
    } else if (isdigit(s[*pos])) {                 /* number */
        while (isdigit(s[*pos])) { val = val*10 + (s[*pos]-'0'); (*pos)++; }
        return val;
    } else {
        *ok = 0; return 0;
    }
}

static int parseexpr(const char *s, int *pos, int *ok) {
    int left = parseatom(s, pos, ok);
    if (!*ok) return 0;
    while (s[*pos]) {
        while (isspace(s[*pos])) (*pos)++;
        char op = s[*pos];
        if (op!='+' && op!='-' && op!='*' && op!='/') break;
        (*pos)++;
        int right = parseatom(s, pos, ok);
        if (!*ok) return 0;
        if (op=='+') left += right;
        else if (op=='-') left -= right;
        else if (op=='*') left *= right;
        else if (op=='/') { if (right==0) { *ok=0; return 0; } left /= right; }
    }
    return left;
}

static int eval(const char *expr, int *ok) {
    *ok = 1;
    int pos = 0;
    return parseexpr(expr, &pos, ok);
}

/* ---------- recalc whole sheet ---------- */
static void recalc(void) {
    for (int y = 0; y < ROWS; y++) {
        for (int x = 0; x < COLS; x++) {
            if (sheet[x][y].formula[0]=='=') {
                int ok = 1;
                sheet[x][y].value = eval(sheet[x][y].formula, &ok);
                sheet[x][y].dirty = 0;
                if (!ok) sheet[x][y].value = 0;
            }
        }
    }
}

/* ---------- draw ---------- */
static void draw(void) {
    hidecursor();
    movecursor(1,1);
    printf("\033[0m"); /* reset */
    /* header */
    printf("     ");
    for (int x = 0; x < 10; x++) printf("%c       ", 'A'+x);
    printf("\n");
    /* rows */
    for (int y = (cury<5?0:cury-5); y < (cury<5?20:cury+15) && y<ROWS; y++) {
        printf("%3d  ", y+1);
        for (int x = 0; x < 10; x++) {
            Cell *c = &sheet[x][y];
            if (c->formula[0])
                printf("%7d ", c->value);
            else
                printf("%7s ", "");
        }
        printf("\n");
    }
    /* status line */
    movecursor(22,1);
    printf("Cell %c%d: %s", 'A'+curx, cury+1, sheet[curx][cury].formula);
    printf("\nArrow/hjkl move  =const  +-*//  c=clear  r=recalc  q=quit");
    movecursor(23,1);
    fflush(stdout);
    showcursor();
}

/* ---------- input helpers ---------- */
static int getkey(void) {
    int ch = getchar();
    if (ch==27) {
        if (getchar()=='[') {
            ch = getchar();
            if (ch=='A') return 'k'; /* up */
            if (ch=='B') return 'j'; /* down */
            if (ch=='C') return 'l'; /* right */
            if (ch=='D') return 'h'; /* left */
        }
        return 0;
    }
    return ch;
}

static void prompt(const char *msg, char *buf, int len) {
    movecursor(24,1);
    printf("\033[0K%s", msg);
    fflush(stdout);
    fgets(buf, len, stdin);
    buf[strcspn(buf,"\r\n")]=0;
}

/* ---------- main ---------- */
int main(void) {
    system("stty -icanon -echo");
    clearscreen();
    while (!quit) {
        draw();
        int k = getkey();
        if (k=='q') quit=1;
        else if (k=='h' && curx>0) curx--;
        else if (k=='l' && curx<COLS-1) curx++;
        else if (k=='j' && cury<ROWS-1) cury++;
        else if (k=='k' && cury>0) cury--;
        else if (k=='c') { sheet[curx][cury].formula[0]=0; sheet[curx][cury].value=0; }
        else if (k=='r') recalc();
        else if (k=='=' || k=='+' || k=='-' || k=='*' || k=='/') {
            char buf[BUF];
            if (k=='=')
                prompt("Enter value or expression: ", buf, BUF);
            else {
                char operand[BUF];
                prompt("Enter operand: ", operand, BUF);
                snprintf(buf, BUF, "=%c%d%c%s", 'A'+curx, cury+1, k, operand);
            }
            strcpy(sheet[curx][cury].formula, buf);
            recalc();
        }
    }
    clearscreen();
    movecursor(1,1);
    system("stty icanon echo");
    return 0;
}
