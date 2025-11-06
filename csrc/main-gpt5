/*
  minisheet.c - minimal terminal spreadsheet (single-file, no deps)

  Features:
    - Arrow keys / hjkl to move
    - Enter / 'e' to edit a cell (text or formula)
    - Formulas: begin with '=', support numbers, + - * /, parentheses, cell refs (A1, AB12)
    - Text: anything not starting with '='; numbers without '=' are treated as numbers
    - c to clear, s to save, o to open, q to quit
    - Simple save format: "A1<TAB><raw cell content>" per line
    - Viewport scrolling
    - Linux/macOS terminals (ANSI + termios). No external libs.

  Compile:
    cc -std=c99 -O2 -Wall -Wextra -o minisheet minisheet.c
*/

#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <signal.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <sys/ioctl.h>

/* --- Config --- */
#define MAX_ROWS 200
#define MAX_COLS 50
#define CELL_MAX 128
#define DEFAULT_SHEET_ROWS 100
#define DEFAULT_SHEET_COLS 26
#define CELL_WIDTH 11 /* characters per column including padding */

/* --- Globals (sheet) --- */
static char cells[MAX_ROWS][MAX_COLS][CELL_MAX]; /* raw content per cell */
static int sheet_rows = DEFAULT_SHEET_ROWS;
static int sheet_cols = DEFAULT_SHEET_COLS;

/* --- Globals (UI/State) --- */
static int cur_r = 0, cur_c = 0;     /* cursor cell */
static int top_r = 0, left_c = 0;    /* viewport top-left */
static char status[256] = {0};
static struct termios orig_termios;
static bool raw_enabled = false;

/* --- Utils --- */
static void die(const char *fmt, ...) {
    if (raw_enabled) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
    }
    va_list ap; va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    exit(1);
}

static void disable_raw_mode(void) {
    if (raw_enabled) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
        raw_enabled = false;
    }
}

static void enable_raw_mode(void) {
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) die("tcgetattr failed");
    atexit(disable_raw_mode);

    struct termios raw = orig_termios;
    raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr failed");
    raw_enabled = true;
}

static int get_window_size(int *rows, int *cols) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1) {
        return -1;
    }
    if (ws.ws_col == 0) return -1;
    *rows = ws.ws_row;
    *cols = ws.ws_col;
    return 0;
}

static void set_status(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    vsnprintf(status, sizeof(status), fmt, ap);
    va_end(ap);
}

/* --- Terminal drawing helpers --- */
static void term_clear(void) { write(STDOUT_FILENO, "\x1b[2J", 4); }
static void term_home(void)  { write(STDOUT_FILENO, "\x1b[H", 3); }
static void term_hide_cursor(void){ write(STDOUT_FILENO, "\x1b[?25l", 6); }
static void term_show_cursor(void){ write(STDOUT_FILENO, "\x1b[?25h", 6); }
static void term_move(int r, int c) {
    char buf[32];
    int n = snprintf(buf, sizeof(buf), "\x1b[%d;%dH", r, c);
    write(STDOUT_FILENO, buf, n);
}

static void term_invert_on(void) { write(STDOUT_FILENO, "\x1b[7m", 4); }
static void term_invert_off(void){ write(STDOUT_FILENO, "\x1b[0m", 4); }

/* --- Keys --- */
enum Keys {
    KEY_NULL = 0,
    KEY_CTRL_C = 3,
    KEY_CTRL_D = 4,
    KEY_CTRL_L = 12,
    KEY_ENTER = 13,
    KEY_ESC = 27,
    KEY_TAB = 9,
    KEY_BACKSPACE = 127,

    KEY_ARROW_LEFT = 1000,
    KEY_ARROW_RIGHT,
    KEY_ARROW_UP,
    KEY_ARROW_DOWN,
    KEY_DELETE,
    KEY_HOME,
    KEY_END,
    KEY_PAGE_UP,
    KEY_PAGE_DOWN
};

static int read_key(void) {
    char c;
    ssize_t n = read(STDIN_FILENO, &c, 1);
    if (n <= 0) return KEY_NULL;

    if (c == '\x1b') {
        char seq[3];
        if (read(STDIN_FILENO, &seq[0], 1) != 1) return KEY_ESC;
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return KEY_ESC;

        if (seq[0] == '[') {
            if (seq[1] >= '0' && seq[1] <= '9') {
                if (read(STDIN_FILENO, &seq[2], 1) != 1) return KEY_ESC;
                if (seq[2] == '~') {
                    switch (seq[1]) {
                        case '1': return KEY_HOME;
                        case '3': return KEY_DELETE;
                        case '4': return KEY_END;
                        case '5': return KEY_PAGE_UP;
                        case '6': return KEY_PAGE_DOWN;
                        case '7': return KEY_HOME;
                        case '8': return KEY_END;
                    }
                }
            } else {
                switch (seq[1]) {
                    case 'A': return KEY_ARROW_UP;
                    case 'B': return KEY_ARROW_DOWN;
                    case 'C': return KEY_ARROW_RIGHT;
                    case 'D': return KEY_ARROW_LEFT;
                    case 'H': return KEY_HOME;
                    case 'F': return KEY_END;
                }
            }
        } else if (seq[0] == 'O') {
            switch (seq[1]) {
                case 'H': return KEY_HOME;
                case 'F': return KEY_END;
            }
        }
        return KEY_ESC;
    }
    if (c == '\r') return KEY_ENTER;
    if (c == 127) return KEY_BACKSPACE;
    return (unsigned char)c;
}

/* --- Column name helper (0->A, 25->Z, 26->AA, etc.) --- */
static void col_to_name(int col, char *out, size_t outlen) {
    char tmp[16];
    int i = 0;
    int n = col;
    if (n < 0) { snprintf(out, outlen, "?"); return; }
    do {
        int rem = n % 26;
        tmp[i++] = (char)('A' + rem);
        n = n / 26 - 1;
    } while (n >= 0 && i < (int)sizeof(tmp));
    // reverse
    int j = 0;
    for (; j < i && j + 1 < (int)outlen; j++) {
        out[j] = tmp[i - 1 - j];
    }
    out[j] = '\0';
}

/* --- Parse a cell id like "AB12" into 0-based (row,col). Return 1 ok, 0 fail --- */
static int parse_cell_id(const char *s, int *row, int *col) {
    int i = 0;
    if (!isalpha((unsigned char)s[i])) return 0;
    long colnum = 0;
    while (isalpha((unsigned char)s[i])) {
        colnum = colnum * 26 + (toupper((unsigned char)s[i]) - 'A' + 1);
        i++;
    }
    if (!isdigit((unsigned char)s[i])) return 0;
    long rownum = 0;
    while (isdigit((unsigned char)s[i])) {
        rownum = rownum * 10 + (s[i] - '0');
        i++;
    }
    if (s[i] != '\0') return 0;
    colnum -= 1; rownum -= 1;
    if (rownum < 0 || colnum < 0) return 0;
    *row = (int)rownum;
    *col = (int)colnum;
    return 1;
}

/* --- Expression parser --- */
typedef struct Parser {
    const char *s;
    size_t pos;
    int err;
    int (*visiting)[MAX_COLS]; /* pointer to visiting matrix */
} Parser;

static void p_skip_spaces(Parser *p) {
    while (isspace((unsigned char)p->s[p->pos])) p->pos++;
}

static int p_peek(Parser *p) {
    return (unsigned char)p->s[p->pos];
}

static int p_match(Parser *p, char ch) {
    p_skip_spaces(p);
    if (p->s[p->pos] == ch) { p->pos++; return 1; }
    return 0;
}

/* Forward decl */
static double eval_cell_value(int r, int c, int visiting[MAX_ROWS][MAX_COLS], int *err);

static int parse_cell_ref(Parser *p, int *r, int *c) {
    size_t start = p->pos;
    long colnum = 0;
    int count_letters = 0;
    while (isalpha((unsigned char)p->s[p->pos])) {
        colnum = colnum * 26 + (toupper((unsigned char)p->s[p->pos]) - 'A' + 1);
        p->pos++; count_letters++;
    }
    if (count_letters == 0) { p->pos = start; return 0; }
    if (!isdigit((unsigned char)p->s[p->pos])) { p->pos = start; return 0; }
    long rownum = 0;
    while (isdigit((unsigned char)p->s[p->pos])) {
        rownum = rownum * 10 + (p->s[p->pos] - '0');
        p->pos++;
    }
    colnum -= 1; rownum -= 1;
    if (rownum < 0 || colnum < 0) { p->err = 1; return 0; }
    *r = (int)rownum; *c = (int)colnum;
    return 1;
}

static double parse_expr(Parser *p); /* forward */

static double parse_factor(Parser *p) {
    p_skip_spaces(p);
    int ch = p_peek(p);

    if (ch == '+' || ch == '-') {
        int sign = (ch == '-') ? -1 : 1;
        p->pos++;
        double v = parse_factor(p);
        return sign * v;
    }

    if (p_match(p, '(')) {
        double v = parse_expr(p);
        if (!p_match(p, ')')) { p->err = 1; }
        return v;
    }

    /* Try cell reference */
    size_t save = p->pos;
    int r, c;
    if (parse_cell_ref(p, &r, &c)) {
        if (r >= sheet_rows || c >= sheet_cols) {
            p->err = 1; return 0.0;
        }
        int err = 0;
        double v = eval_cell_value(r, c, p->visiting, &err);
        if (err) p->err = 1;
        return v;
    } else {
        p->pos = save;
    }

    /* Try number */
    char *end = NULL;
    double v = strtod(p->s + p->pos, &end);
    if (end == p->s + p->pos) { p->err = 1; return 0.0; }
    p->pos = (size_t)(end - p->s);
    return v;
}

static double parse_term(Parser *p) {
    double v = parse_factor(p);
    for (;;) {
        p_skip_spaces(p);
        int ch = p_peek(p);
        if (ch == '*' || ch == '/') {
            p->pos++;
            double rhs = parse_factor(p);
            if (ch == '*') v *= rhs;
            else {
                if (rhs == 0.0) { p->err = 1; return 0.0; }
                v /= rhs;
            }
        } else break;
    }
    return v;
}

static double parse_expr(Parser *p) {
    double v = parse_term(p);
    for (;;) {
        p_skip_spaces(p);
        int ch = p_peek(p);
        if (ch == '+' || ch == '-') {
            p->pos++;
            double rhs = parse_term(p);
            if (ch == '+') v += rhs;
            else v -= rhs;
        } else break;
    }
    return v;
}

/* Evaluate a cell's value (for formulas or numeric). visiting matrix detects cycles. */
static double eval_cell_value(int r, int c, int visiting[MAX_ROWS][MAX_COLS], int *err) {
    if (r < 0 || r >= sheet_rows || c < 0 || c >= sheet_cols) { *err = 1; return 0.0; }

    if (visiting[r][c]) { *err = 1; return 0.0; } /* cycle */
    visiting[r][c] = 1;

    const char *raw = cells[r][c];
    double result = 0.0;
    *err = 0;

    if (raw[0] == '\0') {
        result = 0.0;
    } else if (raw[0] == '=') {
        Parser p = {0};
        p.s = raw + 1;
        p.pos = 0;
        p.err = 0;
        p.visiting = visiting;
        result = parse_expr(&p);
        p_skip_spaces(&p);
        if (p.s[p.pos] != '\0') p.err = 1;
        if (p.err) { *err = 1; result = 0.0; }
    } else {
        /* Try to parse as number; if not, treat as 0 for arithmetic */
        char *end = NULL;
        double v = strtod(raw, &end);
        while (isspace((unsigned char)*end)) end++;
        if (*raw != '\0' && *end == '\0') {
            result = v;
        } else {
            result = 0.0; /* text evaluates as 0 when referenced */
        }
    }

    visiting[r][c] = 0;
    return result;
}

/* Format cell display into out buffer of width w (<= CELL_WIDTH). Highlight decisions done by caller. */
static void format_cell_display(int r, int c, char *out, int w) {
    if (w < 1) { out[0] = '\0'; return; }
    const char *raw = cells[r][c];
    char buf[64];

    if (raw[0] == '\0') {
        /* empty */
        memset(out, ' ', w);
        out[w-1] = ' ';
        out[w] = '\0';
        return;
    }

    if (raw[0] == '=') {
        int visiting[MAX_ROWS][MAX_COLS] = {{0}};
        int err = 0;
        double v = eval_cell_value(r, c, visiting, &err);
        if (err) {
            snprintf(buf, sizeof(buf), "#ERR");
        } else {
            /* choose compact format within width-1 (leave one space) */
            snprintf(buf, sizeof(buf), "%.8g", v);
        }
        size_t len = strlen(buf);
        if ((int)len > w - 1) len = w - 1;
        /* right align numeric */
        int pad = (w - (int)len);
        if (pad < 0) pad = 0;
        memset(out, ' ', w);
        memcpy(out + pad, buf, len);
        out[w] = '\0';
        return;
    }

    /* Not formula: decide if numeric or text for alignment */
    char *end = NULL;
    double v = strtod(raw, &end);
    while (isspace((unsigned char)*end)) end++;
    if (*raw != '\0' && *end == '\0') {
        /* numeric raw */
        snprintf(buf, sizeof(buf), "%.8g", v);
        size_t len = strlen(buf);
        if ((int)len > w - 1) len = w - 1;
        int pad = (w - (int)len);
        memset(out, ' ', w);
        memcpy(out + pad, buf, len);
        out[w] = '\0';
    } else {
        /* text, left-align */
        size_t len = strlen(raw);
        if ((int)len > w - 1) len = w - 1;
        memset(out, ' ', w);
        memcpy(out, raw, len);
        out[w] = '\0';
    }
}

/* --- Prompt line editor (simple) --- */
static int prompt_input(const char *label, char *buf, size_t buflen) {
    /* Returns 1 if entered (buf filled), 0 if cancelled (ESC) */
    size_t len = strlen(buf);
    size_t cur = len;
    int win_r, win_c;
    get_window_size(&win_r, &win_c);
    if (buflen == 0) return 0;

    for (;;) {
        /* Draw prompt on last line */
        term_move(win_r, 1);
        term_invert_on();
        char left[256];
        snprintf(left, sizeof(left), " %s", label);
        int ll = (int)strlen(left);
        write(STDOUT_FILENO, left, ll);

        /* Render buffer, clamp to screen width */
        int room = win_c - ll - 2;
        if (room < 0) room = 0;
        char tmp[1024];
        size_t vis_len = len;
        if ((int)vis_len > room) vis_len = room;
        memcpy(tmp, buf, vis_len);
        tmp[vis_len] = '\0';
        write(STDOUT_FILENO, tmp, vis_len);

        /* Clear rest of line */
        if (ll + (int)vis_len < win_c) {
            char spaces[256];
            int to_clear = win_c - ll - (int)vis_len;
            if (to_clear > (int)sizeof(spaces)) to_clear = (int)sizeof(spaces);
            memset(spaces, ' ', to_clear);
            write(STDOUT_FILENO, spaces, to_clear);
        }

        /* Move cursor to caret position */
        int caret_col = ll + 1 + (int)cur;
        if (caret_col > win_c) caret_col = win_c;
        term_move(win_r, caret_col);
        term_invert_off();
        /* Read key */
        int k = read_key();
        if (k == KEY_ENTER) {
            return 1;
        } else if (k == KEY_ESC) {
            return 0;
        } else if (k == KEY_BACKSPACE || k == 8) {
            if (cur > 0) {
                memmove(buf + cur - 1, buf + cur, len - cur + 1);
                cur--; len--;
            }
        } else if (k == KEY_DELETE) {
            if (cur < len) {
                memmove(buf + cur, buf + cur + 1, len - cur);
                len--;
            }
        } else if (k == KEY_ARROW_LEFT) {
            if (cur > 0) cur--;
        } else if (k == KEY_ARROW_RIGHT) {
            if (cur < len) cur++;
        } else if (k == KEY_HOME) {
            cur = 0;
        } else if (k == KEY_END) {
            cur = len;
        } else if (k == 21) { /* Ctrl+U clear */
            buf[0] = '\0'; len = 0; cur = 0;
        } else if (k >= 32 && k < 127) {
            if (len + 1 < buflen) {
                memmove(buf + cur + 1, buf + cur, len - cur + 1);
                buf[cur] = (char)k;
                cur++; len++;
            }
        }
    }
}

/* --- Save/Load --- */
static void coord_to_name(int r, int c, char *out, size_t outlen) {
    char col[16]; col_to_name(c, col, sizeof(col));
    snprintf(out, outlen, "%s%d", col, r + 1);
}

static int save_file(const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    for (int r = 0; r < sheet_rows; r++) {
        for (int c = 0; c < sheet_cols; c++) {
            if (cells[r][c][0] != '\0') {
                char name[32];
                coord_to_name(r, c, name, sizeof(name));
                fprintf(f, "%s\t%s\n", name, cells[r][c]);
            }
        }
    }
    fclose(f);
    return 0;
}

static int load_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    /* Clear existing */
    for (int r = 0; r < sheet_rows; r++)
        for (int c = 0; c < sheet_cols; c++)
            cells[r][c][0] = '\0';

    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        size_t n = strlen(line);
        while (n && (line[n-1] == '\n' || line[n-1] == '\r')) { line[--n] = 0; }
        char *tab = strchr(line, '\t');
        if (!tab) continue;
        *tab = '\0';
        const char *id = line;
        const char *val = tab + 1;
        int r, c;
        if (!parse_cell_id(id, &r, &c)) continue;
        if (r >= 0 && r < sheet_rows && c >= 0 && c < sheet_cols) {
            strncpy(cells[r][c], val, CELL_MAX - 1);
            cells[r][c][CELL_MAX - 1] = '\0';
        }
    }
    fclose(f);
    return 0;
}

/* --- Drawing UI --- */
static int digits_for(int x) {
    int d = 1; int n = (x <= 0) ? 1 : x;
    while (n >= 10) { n /= 10; d++; }
    return d;
}

static void ensure_visible(void) {
    int scr_r, scr_c;
    if (get_window_size(&scr_r, &scr_c) == -1) { scr_r = 24; scr_c = 80; }
    int header_rows = 2; /* title + column header */
    int status_rows = 1;
    int grid_rows_avail = scr_r - header_rows - status_rows;
    if (grid_rows_avail < 1) grid_rows_avail = 1;
    int row_digits = digits_for(sheet_rows);
    int left_pad = row_digits + 2; /* row numbers + space */

    int grid_cols_avail = (scr_c - left_pad) / CELL_WIDTH;
    if (grid_cols_avail < 1) grid_cols_avail = 1;

    if (cur_r < top_r) top_r = cur_r;
    if (cur_r >= top_r + grid_rows_avail) top_r = cur_r - grid_rows_avail + 1;
    if (top_r < 0) top_r = 0;
    if (top_r > sheet_rows - 1) top_r = sheet_rows - 1;

    if (cur_c < left_c) left_c = cur_c;
    if (cur_c >= left_c + grid_cols_avail) left_c = cur_c - grid_cols_avail + 1;
    if (left_c < 0) left_c = 0;
    if (left_c > sheet_cols - 1) left_c = sheet_cols - 1;
}

static void draw_ui(void) {
    int scr_r, scr_c;
    if (get_window_size(&scr_r, &scr_c) == -1) { scr_r = 24; scr_c = 80; }

    term_hide_cursor();
    term_clear();
    term_home();

    /* Title bar */
    term_invert_on();
    char title[256];
    char cur_name[32]; coord_to_name(cur_r, cur_c, cur_name, sizeof(cur_name));
    snprintf(title, sizeof(title),
             " MiniSheet  | Arrows/HJKL move  Enter/E edit  C clear  S save  O open  Q quit  | %s ",
             cur_name);
    int tlen = (int)strlen(title);
    if (tlen > scr_c) tlen = scr_c;
    write(STDOUT_FILENO, title, tlen);
    if (tlen < scr_c) {
        char spaces[256];
        int rem = scr_c - tlen;
        if (rem > (int)sizeof(spaces)) rem = (int)sizeof(spaces);
        memset(spaces, ' ', rem);
        write(STDOUT_FILENO, spaces, rem);
    }
    term_invert_off();

    /* Column headers */
    int row_digits = digits_for(sheet_rows);
    int left_pad = row_digits + 2;
    char spaces[256];
    if (left_pad > 0) {
        int n = left_pad; if (n > (int)sizeof(spaces)) n = (int)sizeof(spaces);
        memset(spaces, ' ', n);
        write(STDOUT_FILENO, spaces, n);
    }
    int grid_cols_avail = (scr_c - left_pad) / CELL_WIDTH;
    if (grid_cols_avail < 1) grid_cols_avail = 1;
    for (int c = 0; c < grid_cols_avail && (left_c + c) < sheet_cols; c++) {
        char name[16]; col_to_name(left_c + c, name, sizeof(name));
        char hdr[CELL_WIDTH + 1];
        memset(hdr, ' ', CELL_WIDTH);
        size_t n = strlen(name);
        if (n > CELL_WIDTH - 1) n = CELL_WIDTH - 1;
        /* center-ish */
        int start = (CELL_WIDTH - (int)n) / 2;
        if (start < 0) start = 0;
        memcpy(hdr + start, name, n);
        hdr[CELL_WIDTH] = '\0';
        write(STDOUT_FILENO, hdr, CELL_WIDTH);
    }
    write(STDOUT_FILENO, "\n", 1);

    /* Grid body */
    int header_rows = 2;
    int status_rows = 1;
    int grid_rows_avail = scr_r - header_rows - status_rows;
    if (grid_rows_avail < 1) grid_rows_avail = 1;

    for (int r = 0; r < grid_rows_avail && (top_r + r) < sheet_rows; r++) {
        int rn = top_r + r + 1;
        char rn_buf[16];
        snprintf(rn_buf, sizeof(rn_buf), "%*d ", row_digits, rn);
        write(STDOUT_FILENO, rn_buf, (int)strlen(rn_buf));
        write(STDOUT_FILENO, " ", 1);

        for (int c = 0; c < grid_cols_avail && (left_c + c) < sheet_cols; c++) {
            int col_index = left_c + c;
            char disp[CELL_WIDTH + 1] = {0};
            format_cell_display(top_r + r, col_index, disp, CELL_WIDTH - 1);
            /* one space padding on right */
            disp[CELL_WIDTH - 1] = ' ';
            disp[CELL_WIDTH] = '\0';

            if ((top_r + r) == cur_r && col_index == cur_c) {
                term_invert_on();
                write(STDOUT_FILENO, disp, CELL_WIDTH);
                term_invert_off();
            } else {
                write(STDOUT_FILENO, disp, CELL_WIDTH);
            }
        }
        write(STDOUT_FILENO, "\n", 1);
    }

    /* Status line */
    term_invert_on();
    char raw[CELL_MAX];
    strncpy(raw, cells[cur_r][cur_c], CELL_MAX - 1);
    raw[CELL_MAX - 1] = '\0';
    char stat[512];
    snprintf(stat, sizeof(stat), " %s = %s", cur_name, raw[0] ? raw : "(empty)");
    int slen = (int)strlen(stat);
    if (slen > scr_c) slen = scr_c;
    write(STDOUT_FILENO, stat, slen);
    if (slen < scr_c) {
        int rem = scr_c - slen;
        int n = rem; if (n > (int)sizeof(spaces)) n = (int)sizeof(spaces);
        memset(spaces, ' ', n);
        write(STDOUT_FILENO, spaces, n);
    }
    term_invert_off();

    term_show_cursor();
}

/* --- Editing --- */
static void edit_current_cell(void) {
    char label[64];
    char name[32]; coord_to_name(cur_r, cur_c, name, sizeof(name));
    snprintf(label, sizeof(label), "Edit %s: ", name);

    char buf[CELL_MAX];
    strncpy(buf, cells[cur_r][cur_c], CELL_MAX - 1);
    buf[CELL_MAX - 1] = '\0';
    int ok = prompt_input(label, buf, sizeof(buf));
    if (ok) {
        strncpy(cells[cur_r][cur_c], buf, CELL_MAX - 1);
        cells[cur_r][cur_c][CELL_MAX - 1] = '\0';
        set_status("Set %s", name);
    } else {
        set_status("Edit cancelled");
    }
}

/* --- Input loop and main --- */
static void on_sigint(int sig) {
    (void)sig;
    disable_raw_mode();
    _exit(1);
}

int main(void) {
    signal(SIGINT, on_sigint);
    enable_raw_mode();

    set_status("Welcome to MiniSheet. Press 'q' to quit.");
    for (;;) {
        ensure_visible();
        draw_ui();
        int key = read_key();
        if (key == 0) continue;

        if (key == 'q' || key == 'Q') {
            term_move(999, 1); /* move cursor down to avoid overwriting */
            break;
        }

        switch (key) {
            case KEY_ARROW_LEFT: case 'h': if (cur_c > 0) cur_c--; break;
            case KEY_ARROW_RIGHT: case 'l': if (cur_c + 1 < sheet_cols) cur_c++; break;
            case KEY_ARROW_UP: case 'k': if (cur_r > 0) cur_r--; break;
            case KEY_ARROW_DOWN: case 'j': if (cur_r + 1 < sheet_rows) cur_r++; break;
            case KEY_HOME: cur_c = 0; break;
            case KEY_END: cur_c = sheet_cols - 1; break;
            case KEY_PAGE_UP: cur_r = (cur_r >= 10) ? cur_r - 10 : 0; break;
            case KEY_PAGE_DOWN: cur_r = (cur_r + 10 < sheet_rows) ? cur_r + 10 : sheet_rows - 1; break;
            case KEY_TAB: if (cur_c + 1 < sheet_cols) cur_c++; break;

            case KEY_ENTER:
            case 'e':
            case 'E':
                edit_current_cell();
                break;

            case 'c':
            case 'C':
                cells[cur_r][cur_c][0] = '\0';
                set_status("Cleared cell");
                break;

            case 's':
            case 'S': {
                char path[256] = "sheet.txt";
                if (prompt_input("Save as: ", path, sizeof(path))) {
                    if (save_file(path) == 0) set_status("Saved to %s", path);
                    else set_status("Save failed: %s", strerror(errno));
                } else {
                    set_status("Save cancelled");
                }
            } break;

            case 'o':
            case 'O': {
                char path[256] = "sheet.txt";
                if (prompt_input("Open file: ", path, sizeof(path))) {
                    if (load_file(path) == 0) set_status("Loaded %s", path);
                    else set_status("Open failed: %s", strerror(errno));
                } else {
                    set_status("Open cancelled");
                }
            } break;

            default:
                break;
        }
    }

    disable_raw_mode();
    printf("\nBye!\n");
    return 0;
}
