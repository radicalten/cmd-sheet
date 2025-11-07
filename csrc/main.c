/*
 * mini_sheet.c ― a 100 % self-contained, tiny spreadsheet.
 *
 * 26 columns (A–Z) × 10 rows (1–10)
 * Supports numeric constants and formulas with + - * / and parentheses.
 *
 * Build:   gcc mini_sheet.c -o mini_sheet
 * Run :   ./mini_sheet
 *
 * Public-domain / CC0 – do whatever you like.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_ROWS 10
#define MAX_COLS 26
#define MAX_EXPR 256

typedef struct {
    char   expr[MAX_EXPR];  /* original text ("" ==> empty)              */
    double val;             /* cached numeric value                      */
    int    is_formula;      /* 0 = plain number, 1 = expression          */
    int    evaluating;      /* recursion flag for cycle detection        */
} Cell;

static Cell sheet[MAX_ROWS][MAX_COLS];

/* Forward declarations for the recursive-descent parser */
static double parse_expr (const char **p, int *err);
static double eval_cell  (int row, int col, int *err);

/* ────────────────────────────────────────────────────────── UTILITIES ─── */

static void ltrim(char *s)
{
    while (isspace((unsigned char)*s)) memmove(s, s+1, strlen(s));
}

static void rtrim(char *s)
{
    size_t n = strlen(s);
    while (n && isspace((unsigned char)s[n-1])) s[--n] = '\0';
}

static void trim(char *s) { rtrim(s); ltrim(s); }

static int parse_cell_name(const char *s, int *row, int *col)
{
    /* Accept  A1 .. Z10  (case-insensitive) */
    if (!isalpha((unsigned char)s[0])) return 0;
    *col = toupper((unsigned char)s[0]) - 'A';
    if (*col < 0 || *col >= MAX_COLS)  return 0;

    char *endptr;
    long r = strtol(s+1, &endptr, 10);
    if (r < 1 || r > MAX_ROWS || endptr == s+1) return 0;

    if (*endptr) return 0; /* trailing junk */
    *row = (int)r - 1;
    return 1;
}

/* ───────────────────────────────────── PARSER: expr / term / factor ─── */

static void skip_ws(const char **p)
{
    while (isspace((unsigned char)**p)) ++*p;
}

static double parse_factor(const char **p, int *err)
{
    skip_ws(p);
    if (*err) return 0.0;

    /* unary +/- */
    if (**p == '+' || **p == '-') {
        int sign = (**p == '-') ? -1 : 1;
        ++*p;
        return sign * parse_factor(p, err);
    }

    /* parenthesized expression */
    if (**p == '(') {
        ++*p;
        double v = parse_expr(p, err);
        skip_ws(p);
        if (**p != ')') { *err = 1; return 0.0; }
        ++*p;
        return v;
    }

    /* number OR cell reference */
    if (isalpha((unsigned char)**p)) {
        char buf[16] = {0};
        int i=0;
        while (isalnum((unsigned char)**p) && i < 15) buf[i++] = *(*p)++;
        buf[i] = '\0';
        int row,col;
        if (!parse_cell_name(buf, &row, &col)) { *err = 1; return 0.0; }
        return eval_cell(row, col, err);
    }

    /* numeric constant */
    if (isdigit((unsigned char)**p) || **p=='.') {
        char *endptr;
        double v = strtod(*p, &endptr);
        *p = endptr;
        return v;
    }

    *err = 1;
    return 0.0;
}

static double parse_term(const char **p, int *err)
{
    double v = parse_factor(p, err);
    while (!*err) {
        skip_ws(p);
        if (**p == '*' || **p == '/') {
            char op = *(*p)++;
            double rhs = parse_factor(p, err);
            if (*err) return 0.0;
            v = (op == '*') ? v * rhs : v / rhs;
        } else {
            break;
        }
    }
    return v;
}

static double parse_expr(const char **p, int *err)
{
    double v = parse_term(p, err);
    while (!*err) {
        skip_ws(p);
        if (**p == '+' || **p == '-') {
            char op = *(*p)++;
            double rhs = parse_term(p, err);
            if (*err) return 0.0;
            v = (op == '+') ? v + rhs : v - rhs;
        } else {
            break;
        }
    }
    return v;
}

/* ────────────────────────────────── CELL EVALUATION (with caching) ─── */

static double eval_cell(int row, int col, int *err)
{
    if (row < 0 || row >= MAX_ROWS || col < 0 || col >= MAX_COLS) {
        *err = 1; return 0.0;
    }

    Cell *c = &sheet[row][col];

    if (!c->expr[0]) return 0.0;          /* blank cell == 0 */
    if (c->evaluating) {                  /* cycle! */
        fprintf(stderr, "Error: circular reference at %c%d\n", col+'A', row+1);
        *err = 1; return 0.0;
    }

    if (!c->is_formula) {                 /* plain number */
        return c->val;
    }

    c->evaluating = 1;
    const char *p = c->expr;
    int local_err = 0;
    double v = parse_expr(&p, &local_err);
    skip_ws(&p);
    if (*p) local_err = 1;                /* leftover junk */

    if (local_err) {
        fprintf(stderr, "Error: bad formula in %c%d: %s\n",
                col+'A', row+1, c->expr);
        *err = 1;
        v = 0.0;
    } else {
        c->val = v;                       /* cache result */
    }
    c->evaluating = 0;
    return v;
}

/* ───────────────────────────────────────────── display & helpers ─── */

static void print_sheet(void)
{
    printf("\n        ");
    for (int c=0; c<MAX_COLS; ++c) printf(" %6c", 'A'+c);
    printf("\n");

    for (int r=0; r<MAX_ROWS; ++r)
    {
        printf("Row %2d ", r+1);
        for (int c=0; c<MAX_COLS; ++c)
        {
            int err=0;
            double v = eval_cell(r,c,&err);
            if (err) printf("  #### ");
            else if (!sheet[r][c].expr[0]) printf("       ");
            else                           printf(" %6.2f", v);
        }
        printf("\n");
    }
    printf("\n");
}

static void print_help(void)
{
    puts("\nMini-sheet commands:");
    puts("  A1 = 5            (set constant)");
    puts("  B2 = A1 * 2 + 7   (set formula)");
    puts("  print             (show grid)");
    puts("  help              (this help)");
    puts("  quit / exit       (leave)");
}

/* ───────────────────────────────────────────── main loop ─── */

int main(void)
{
    char line[512];

    puts("Mini-sheet 0.1  –  type 'help' for instructions.");

    while (1)
    {
        fputs("> ", stdout);
        if (!fgets(line, sizeof line, stdin)) break;
        trim(line);
        if (!*line) continue;

        /* quit? */
        if (!strcasecmp(line, "quit") || !strcasecmp(line,"exit")) break;

        /* help? */
        if (!strcasecmp(line, "help")) { print_help(); continue; }

        /* print? */
        if (!strcasecmp(line, "print")) { print_sheet(); continue; }

        /* assignment? look for '=' */
        char *eq = strchr(line, '=');
        if (eq)
        {
            *eq = '\0';
            char lhs[32], rhs[MAX_EXPR];
            strncpy(lhs, line, sizeof lhs-1);
            lhs[sizeof lhs-1] = '\0';
            strncpy(rhs, eq+1, sizeof rhs-1);
            rhs[sizeof rhs-1] = '\0';
            trim(lhs); trim(rhs);

            int row,col;
            if (!parse_cell_name(lhs, &row, &col)) {
                puts("Syntax:  <Cell> = <expr>   (Cell is A1..Z10)");
                continue;
            }
            Cell *c = &sheet[row][col];
            strncpy(c->expr, rhs, MAX_EXPR-1);
            c->expr[MAX_EXPR-1]='\0';

            /* decide if it's a plain number */
            char *endptr; double v = strtod(rhs,&endptr);
            trim(endptr);
            if (*rhs && !*endptr) {           /* pure numeric */
                c->is_formula = 0;
                c->val = v;
            } else {
                c->is_formula = 1;
            }

            int err=0; (void)eval_cell(row,col,&err);
            if (!err)
                printf("%s = %g\n", lhs, c->val);
            continue;
        }

        puts("Unknown command. Type 'help'.");
    }

    puts("Bye.");
    return 0;
}
