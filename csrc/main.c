// minisheet.c - a tiny terminal spreadsheet (single-file, no deps)
// Build: cc -std=c99 -O2 -Wall -Wextra -o minisheet minisheet.c
// Runs on POSIX terminals (Linux/macOS). Windows not supported.
// Features:
// - 26 columns (A-Z), 100 rows
// - Arrow key navigation, edit values/formulas, save/load
// - Formulas start with '=' and support + - * /, parentheses, and cell refs (A1..Z100)
// - Errors: PARSE, BADREF, DIV/0, CYCLE (propagated)
// - Simple file format: lines "row col escaped_text\n" (0-based row/col)
//   Escapes: \n, \t, \\

// Disclaimer: this is educational/minimal, not bulletproof. Keep backups!

#define _XOPEN_SOURCE 600
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#define MAX_ROWS 100
#define MAX_COLS 26
#define CELL_TEXT_LEN 128

typedef struct {
    char text[CELL_TEXT_LEN]; // empty => empty cell; starts with '=' => formula
    double value;             // last computed numeric value
    int evaluated;            // 1 if value/err computed for this frame
    int evaluating;           // 1 if computing (cycle detection)
    int err;                  // error code for last eval
} Cell;

enum {
    ERR_NONE = 0,
    ERR_PARSE = 1,
    ERR_BADREF = 2,
    ERR_DIV0 = 3,
    ERR_CYCLE = 4,
    ERR_DEP = 5
};

static const char* err_name(int e) {
    switch (e) {
        case ERR_NONE: return "";
        case ERR_PARSE: return "PARSE";
        case ERR_BADREF: return "BADREF";
        case ERR_DIV0: return "DIV/0";
        case ERR_CYCLE: return "CYCLE";
        case ERR_DEP: return "DEPERR";
    }
    return "ERR";
}

static Cell sheet[MAX_ROWS][MAX_COLS];

static int cur_row = 0, cur_col = 0;
static int row_off = 0, col_off = 0;
static int screen_rows = 24, screen_cols = 80;
static char status_msg[256] = "";
static char last_file[256] = "";

static struct termios orig_termios;

static void die_cleanup(void) {
    // Restore terminal
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
    // Show cursor
    write(STDOUT_FILENO, "\x1b[?25h\x1b[0m", 9);
}

static void die(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    char buf[1024];
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    die_cleanup();
    write(STDERR_FILENO, buf, strlen(buf));
    write(STDERR_FILENO, "\n", 1);
    exit(1);
}

static void enable_raw_mode(void) {
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) die("tcgetattr: %s", strerror(errno));
    atexit(die_cleanup);
    struct termios raw = orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |=  (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr: %s", strerror(errno));
}

static void get_window_size(void) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        screen_rows = 24;
        screen_cols = 80;
    } else {
        screen_rows = ws.ws_row;
        screen_cols = ws.ws_col;
    }
}

static void handle_sigwinch(int sig) {
    (void)sig;
    get_window_size();
}

enum {
    KEY_NULL = 0,
    KEY_CTRL_C = 3,
    KEY_CTRL_G = 7,
    KEY_BACKSPACE = 127,
    KEY_ENTER = 1000,
    KEY_ARROW_LEFT,
    KEY_ARROW_RIGHT,
    KEY_ARROW_UP,
    KEY_ARROW_DOWN,
    KEY_DEL,
    KEY_HOME,
    KEY_END,
    KEY_PAGE_UP,
    KEY_PAGE_DOWN,
    KEY_SHIFT_TAB
};

static int read_key(void) {
    char c;
    ssize_t nread = read(STDIN_FILENO, &c, 1);
    if (nread != 1) return KEY_NULL;
    if (c == '\r' || c == '\n') return KEY_ENTER;
    if (c == '\t') return '\t';
    if (c == 0x1b) {
        char seq[6] = {0};
        // Try read up to 5 bytes of escape sequence
        int i = 0;
        ioctl(STDIN_FILENO, FIONREAD, &i);
        if (i == 0) return 0x1b;
        int r = read(STDIN_FILENO, seq, sizeof(seq)-1);
        if (r <= 0) return 0x1b;
        // CSI sequences
        if (seq[0] == '[') {
            if (seq[1] >= '0' && seq[1] <= '9') {
                if (seq[2] == '~') {
                    switch (seq[1]) {
                        case '1': return KEY_HOME;
                        case '3': return KEY_DEL;
                        case '4': return KEY_END;
                        case '5': return KEY_PAGE_UP;
                        case '6': return KEY_PAGE_DOWN;
                        case '7': return KEY_HOME;
                        case '8': return KEY_END;
                    }
                } else if (seq[2] == ';' && seq[4] == '~') {
                    // e.g., 1;2Z for shift-tab in some terms
                    if (seq[1] == '1' && seq[3] == '2' && seq[4] == 'Z') return KEY_SHIFT_TAB;
                }
            } else {
                switch (seq[1]) {
                    case 'A': return KEY_ARROW_UP;
                    case 'B': return KEY_ARROW_DOWN;
                    case 'C': return KEY_ARROW_RIGHT;
                    case 'D': return KEY_ARROW_LEFT;
                    case 'H': return KEY_HOME;
                    case 'F': return KEY_END;
                    case 'Z': return KEY_SHIFT_TAB; // shift-tab in some terms
                }
            }
        } else if (seq[0] == 'O') {
            switch (seq[1]) {
                case 'H': return KEY_HOME;
                case 'F': return KEY_END;
            }
        }
        return KEY_NULL;
    }
    if (c == 127) return KEY_BACKSPACE;
    return (unsigned char)c;
}

static inline char col_name(int c) { return (char)('A' + c); }

static void cell_label(int r, int c, char out[8]) {
    // single-letter columns A-Z only
    snprintf(out, 8, "%c%d", col_name(c), r + 1);
}

static bool parse_cell_label(const char* s, int* out_r, int* out_c) {
    if (!s || !isalpha((unsigned char)*s)) return false;
    char ch = toupper((unsigned char)*s++);
    if (ch < 'A' || ch > 'Z') return false;
    int c = ch - 'A';
    if (!isdigit((unsigned char)*s)) return false;
    long row = strtol(s, NULL, 10);
    if (row < 1 || row > MAX_ROWS) return false;
    *out_c = c;
    *out_r = (int)row - 1;
    return true;
}

static void clear_eval_flags(void) {
    for (int r = 0; r < MAX_ROWS; ++r)
        for (int c = 0; c < MAX_COLS; ++c) {
            sheet[r][c].evaluated = 0;
            sheet[r][c].evaluating = 0;
            sheet[r][c].err = 0;
        }
}

typedef struct {
    const char* s;
} Parser;

static void p_skip(Parser* p) {
    while (isspace((unsigned char)*p->s)) p->s++;
}

static double eval_cell(int r, int c); // forward

static double parse_expr(Parser* p, int* err); // forward

static bool parse_cellref(Parser* p, int* rr, int* cc) {
    p_skip(p);
    const char* start = p->s;
    if (!isalpha((unsigned char)*p->s)) return false;
    char ch = toupper((unsigned char)*p->s++);
    if (ch < 'A' || ch > 'Z') { p->s = start; return false; }
    int col = ch - 'A';
    if (!isdigit((unsigned char)*p->s)) { p->s = start; return false; }
    long row = strtol(p->s, (char**)&p->s, 10);
    if (row < 1 || row > MAX_ROWS) { p->s = start; return false; }
    *rr = (int)row - 1;
    *cc = col;
    return true;
}

static double parse_factor(Parser* p, int* err) {
    if (*err) return 0.0;
    p_skip(p);
    const char* s = p->s;
    if (*s == '(') {
        p->s++;
        double v = parse_expr(p, err);
        p_skip(p);
        if (*p->s != ')') { *err = ERR_PARSE; return 0.0; }
        p->s++;
        return v;
    }
    if (*s == '+') { p->s++; return parse_factor(p, err); }
    if (*s == '-') { p->s++; return -parse_factor(p, err); }
    int rr, cc;
    if (parse_cellref(p, &rr, &cc)) {
        if (rr < 0 || rr >= MAX_ROWS || cc < 0 || cc >= MAX_COLS) {
            *err = ERR_BADREF; return 0.0;
        }
        double v = eval_cell(rr, cc);
        if (sheet[rr][cc].err != ERR_NONE) *err = sheet[rr][cc].err;
        return v;
    }
    // number
    char* end = NULL;
    double v = strtod(p->s, &end);
    if (end == p->s) { *err = ERR_PARSE; return 0.0; }
    p->s = end;
    return v;
}

static double parse_term(Parser* p, int* err) {
    double v = parse_factor(p, err);
    for (;;) {
        p_skip(p);
        if (*err) return 0.0;
        if (*p->s == '*') {
            p->s++;
            double r = parse_factor(p, err);
            v *= r;
        } else if (*p->s == '/') {
            p->s++;
            double r = parse_factor(p, err);
            if (r == 0.0) { *err = ERR_DIV0; return 0.0; }
            v /= r;
        } else break;
    }
    return v;
}

static double parse_expr(Parser* p, int* err) {
    double v = parse_term(p, err);
    for (;;) {
        p_skip(p);
        if (*err) return 0.0;
        if (*p->s == '+') {
            p->s++;
            double r = parse_term(p, err);
            v += r;
        } else if (*p->s == '-') {
            p->s++;
            double r = parse_term(p, err);
            v -= r;
        } else break;
    }
    return v;
}

static double eval_cell(int r, int c) {
    Cell* cell = &sheet[r][c];
    if (cell->evaluated) return cell->value;
    if (cell->evaluating) { cell->err = ERR_CYCLE; cell->value = 0.0; cell->evaluated = 1; return 0.0; }
    cell->evaluating = 1;
    cell->err = ERR_NONE;
    if (cell->text[0] == '=') {
        Parser p = { .s = cell->text + 1 };
        double v = parse_expr(&p, &cell->err);
        p_skip(&p);
        if (*p.s != '\0' && cell->err == ERR_NONE) cell->err = ERR_PARSE;
        if (cell->err != ERR_NONE) v = 0.0;
        cell->value = v;
    } else if (cell->text[0] == '\0') {
        cell->value = 0.0;
    } else {
        char* end = NULL;
        double v = strtod(cell->text, &end);
        if (end && *end == '\0') {
            cell->value = v;
        } else {
            // Treat plain text as 0 for formula math
            cell->value = 0.0;
        }
    }
    cell->evaluated = 1;
    cell->evaluating = 0;
    return cell->value;
}

static void statusf(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(status_msg, sizeof(status_msg), fmt, ap);
    va_end(ap);
}

// tiny helper to clamp
static int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

static void draw_screen(void) {
    get_window_size();
    int left_w = 5;         // width for row numbers (e.g., " 100 ")
    int cell_w = 10;        // cell width
    int top_h = 1;          // header row
    int bot_h = 1;          // status bar
    int grid_h = screen_rows - top_h - bot_h;
    if (grid_h < 1) grid_h = 1;
    if (screen_cols < left_w + 3) left_w = screen_cols > 3 ? screen_cols - 3 : screen_cols;

    int avail_cols = screen_cols - left_w;
    int vis_cols = avail_cols > 0 ? (avail_cols / cell_w) : 0;
    if (vis_cols < 1) vis_cols = 1;
    if (vis_cols > MAX_COLS) vis_cols = MAX_COLS;

    int vis_rows = grid_h;
    if (vis_rows > MAX_ROWS) vis_rows = MAX_ROWS;

    // keep cursor cell in view
    if (cur_row < row_off) row_off = cur_row;
    if (cur_col < col_off) col_off = cur_col;
    if (cur_row >= row_off + vis_rows) row_off = cur_row - vis_rows + 1;
    if (cur_col >= col_off + vis_cols) col_off = cur_col - vis_cols + 1;

    // clear eval state
    clear_eval_flags();

    // hide cursor and home
    write(STDOUT_FILENO, "\x1b[?25l\x1b[H", 9);

    // Header row
    {
        // left gutter
        char buf[64];
        memset(buf, ' ', sizeof(buf));
        int n = left_w;
        if (n > (int)sizeof(buf)) n = (int)sizeof(buf);
        write(STDOUT_FILENO, buf, n);
        // column headers
        for (int c = 0; c < vis_cols; ++c) {
            char title[16];
            snprintf(title, sizeof(title), " %c", col_name(col_off + c));
            char cellbuf[64];
            snprintf(cellbuf, sizeof(cellbuf), "%-*s", cell_w, title);
            write(STDOUT_FILENO, cellbuf, strlen(cellbuf));
        }
        write(STDOUT_FILENO, "\r\n", 2);
    }

    // Grid
    for (int r = 0; r < vis_rows; ++r) {
        int rr = row_off + r;
        // row label (right-aligned)
        char rl[32];
        snprintf(rl, sizeof(rl), "%*d ", left_w - 1 > 0 ? left_w - 1 : 0, rr + 1);
        write(STDOUT_FILENO, rl, strlen(rl));
        for (int c = 0; c < vis_cols; ++c) {
            int cc = col_off + c;
            Cell* cell = &sheet[rr][cc];
            char out[128] = "";
            if (cell->text[0] == '=') {
                double v = eval_cell(rr, cc);
                if (cell->err != ERR_NONE) {
                    snprintf(out, sizeof(out), "#%s", err_name(cell->err));
                } else {
                    snprintf(out, sizeof(out), "%.10g", v);
                }
            } else if (cell->text[0] == '\0') {
                out[0] = '\0';
            } else {
                // show literal text
                snprintf(out, sizeof(out), "%s", cell->text);
            }
            // truncate display to cell_w
            char disp[128];
            int len = (int)strlen(out);
            if (len > cell_w - 1) {
                memcpy(disp, out, cell_w - 2);
                disp[cell_w - 2] = '…';
                disp[cell_w - 1] = '\0';
            } else {
                snprintf(disp, sizeof(disp), "%s", out);
            }

            if (rr == cur_row && cc == cur_col) write(STDOUT_FILENO, "\x1b[7m", 4);
            char cellbuf[256];
            snprintf(cellbuf, sizeof(cellbuf), "%-*s", cell_w, disp);
            write(STDOUT_FILENO, cellbuf, strlen(cellbuf));
            if (rr == cur_row && cc == cur_col) write(STDOUT_FILENO, "\x1b[0m", 4);
        }
        write(STDOUT_FILENO, "\r\n", 2);
    }

    // Fill remainder if terminal larger than sheet
    for (int r = vis_rows; r < grid_h; ++r) {
        for (int i = 0; i < screen_cols; ++i) write(STDOUT_FILENO, " ", 1);
        write(STDOUT_FILENO, "\r\n", 2);
    }

    // Status bar (inverse)
    {
        char label[8]; cell_label(cur_row, cur_col, label);
        Cell* cur = &sheet[cur_row][cur_col];
        char valbuf[64] = "";
        double v = 0.0;
        int err = 0;
        if (cur->text[0] == '=') {
            clear_eval_flags();
            v = eval_cell(cur_row, cur_col);
            err = cur->err;
            if (err == ERR_NONE) snprintf(valbuf, sizeof(valbuf), "val=%.10g", v);
            else snprintf(valbuf, sizeof(valbuf), "err=%s", err_name(err));
        } else if (cur->text[0] == '\0') {
            snprintf(valbuf, sizeof(valbuf), "empty");
        } else {
            char* end = NULL;
            v = strtod(cur->text, &end);
            if (end && *end == '\0') snprintf(valbuf, sizeof(valbuf), "num=%.10g", v);
            else snprintf(valbuf, sizeof(valbuf), "text");
        }

        char msg[1024];
        snprintf(msg, sizeof(msg),
                 "\x1b[7m %-4s raw:%s  %s  | arrows:move  Enter:edit  '=':formula  c:clear  s:save  l:load  g:goto  q:quit %-*s\x1b[0m",
                 label,
                 cur->text[0] ? cur->text : "(empty)",
                 valbuf,
                 screen_cols - 90 > 0 ? screen_cols - 90 : 1, "");

        // Overwrite with transient status message if present
        if (status_msg[0]) {
            char smsg[1024];
            snprintf(smsg, sizeof(smsg), "\x1b[7m %s %-*s\x1b[0m",
                     status_msg,
                     screen_cols - (int)strlen(status_msg) - 3 > 0 ? screen_cols - (int)strlen(status_msg) - 3 : 1,
                     "");
            write(STDOUT_FILENO, smsg, strlen(smsg));
            status_msg[0] = '\0'; // show once
        } else {
            write(STDOUT_FILENO, msg, strlen(msg));
        }
    }

    // place cursor at bottom-right-ish and show it
    write(STDOUT_FILENO, "\x1b[?25h", 6);
}

static int prompt_line(const char* prompt, const char* initial, char* out, int outsz) {
    // Minimal line editor on status line; ESC or Ctrl-G cancels, Enter accepts
    int len = 0;
    out[0] = '\0';
    if (initial && *initial) {
        strncpy(out, initial, outsz - 1);
        out[outsz - 1] = '\0';
        len = (int)strlen(out);
    }
    for (;;) {
        // draw prompt on last line
        get_window_size();
        char buf[1024];
        snprintf(buf, sizeof(buf), "\x1b[?25l\x1b[%d;1H\x1b[0m\x1b[2K%s%s\x1b[?25h",
                 screen_rows, prompt, out);
        write(STDOUT_FILENO, buf, strlen(buf));
        int k = read_key();
        if (k == KEY_ENTER) {
            // clear prompt line
            snprintf(buf, sizeof(buf), "\x1b[%d;1H\x1b[2K", screen_rows);
            write(STDOUT_FILENO, buf, strlen(buf));
            return 1;
        }
        if (k == 0x1b || k == KEY_CTRL_G) { // cancel
            snprintf(buf, sizeof(buf), "\x1b[%d;1H\x1b[2K", screen_rows);
            write(STDOUT_FILENO, buf, strlen(buf));
            return 0;
        }
        if (k == KEY_BACKSPACE || k == 8) {
            if (len > 0) { out[--len] = '\0'; }
        } else if (k >= 32 && k < 127) {
            if (len < outsz - 1) { out[len++] = (char)k; out[len] = '\0'; }
        }
    }
}

static void escape_write(FILE* f, const char* s) {
    for (; *s; ++s) {
        if (*s == '\\') fputs("\\\\", f);
        else if (*s == '\n') fputs("\\n", f);
        else if (*s == '\t') fputs("\\t", f);
        else fputc(*s, f);
    }
}

static void unescape_into(const char* s, char* out, int outsz) {
    int i = 0;
    while (*s && i < outsz - 1) {
        if (*s == '\\') {
            s++;
            if (*s == 'n') { out[i++] = '\n'; s++; }
            else if (*s == 't') { out[i++] = '\t'; s++; }
            else if (*s == '\\') { out[i++] = '\\'; s++; }
            else { out[i++] = '\\'; } // unknown, keep slash
        } else {
            out[i++] = *s++;
        }
    }
    out[i] = '\0';
}

static int save_file(const char* path) {
    FILE* f = fopen(path, "w");
    if (!f) return -1;
    // simple lines: r c text
    for (int r = 0; r < MAX_ROWS; ++r) {
        for (int c = 0; c < MAX_COLS; ++c) {
            if (sheet[r][c].text[0]) {
                fprintf(f, "%d %d ", r, c);
                escape_write(f, sheet[r][c].text);
                fputc('\n', f);
            }
        }
    }
    fclose(f);
    return 0;
}

static int load_file(const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) return -1;
    // clear sheet
    for (int r = 0; r < MAX_ROWS; ++r)
        for (int c = 0; c < MAX_COLS; ++c)
            sheet[r][c].text[0] = '\0';

    char line[2048];
    int count = 0;
    while (fgets(line, sizeof(line), f)) {
        char* p = line;
        while (isspace((unsigned char)*p)) p++;
        if (*p == '#' || *p == '\0') continue;
        char* end;
        long r = strtol(p, &end, 10);
        if (end == p) continue;
        p = end;
        long c = strtol(p, &end, 10);
        if (end == p) continue;
        p = end;
        while (*p == ' ' || *p == '\t') p++;
        // strip trailing newline
        char* nl = strchr(p, '\n'); if (nl) *nl = '\0';
        if (r >= 0 && r < MAX_ROWS && c >= 0 && c < MAX_COLS) {
            unescape_into(p, sheet[r][c].text, CELL_TEXT_LEN);
            count++;
        }
    }
    fclose(f);
    return count;
}

static void edit_cell(bool start_with_equals) {
    char label[8]; cell_label(cur_row, cur_col, label);
    char prompt[64];
    snprintf(prompt, sizeof(prompt), "Edit %s: ", label);
    char buf[CELL_TEXT_LEN];
    if (start_with_equals) {
        snprintf(buf, sizeof(buf), "%s", "=");
    } else {
        snprintf(buf, sizeof(buf), "%s", sheet[cur_row][cur_col].text);
    }
    int ok = prompt_line(prompt, buf, buf, sizeof(buf));
    if (ok) {
        strncpy(sheet[cur_row][cur_col].text, buf, CELL_TEXT_LEN - 1);
        sheet[cur_row][cur_col].text[CELL_TEXT_LEN - 1] = '\0';
    }
}

static void clear_cell(void) {
    sheet[cur_row][cur_col].text[0] = '\0';
}

static void goto_cell(void) {
    char buf[32] = "";
    if (prompt_line("Go to (e.g., B7): ", "", buf, sizeof(buf))) {
        int r, c;
        if (parse_cell_label(buf, &r, &c)) {
            cur_row = r; cur_col = c;
            statusf("Moved to %s", buf);
        } else {
            statusf("Invalid cell label");
        }
    }
}

int main(void) {
    // Line-buffering off for smoother redraws
    setvbuf(stdout, NULL, _IOFBF, 0);
    enable_raw_mode();
    signal(SIGWINCH, handle_sigwinch);
    get_window_size();

    // Main loop
    for (;;) {
        draw_screen();
        int k = read_key();
        if (k == 'q') break;
        else if (k == KEY_ARROW_UP)    cur_row = clampi(cur_row - 1, 0, MAX_ROWS - 1);
        else if (k == KEY_ARROW_DOWN)  cur_row = clampi(cur_row + 1, 0, MAX_ROWS - 1);
        else if (k == KEY_ARROW_LEFT)  cur_col = clampi(cur_col - 1, 0, MAX_COLS - 1);
        else if (k == KEY_ARROW_RIGHT) cur_col = clampi(cur_col + 1, 0, MAX_COLS - 1);
        else if (k == KEY_HOME)        cur_col = 0;
        else if (k == KEY_END)         cur_col = MAX_COLS - 1;
        else if (k == KEY_PAGE_UP)     cur_row = clampi(cur_row - 10, 0, MAX_ROWS - 1);
        else if (k == KEY_PAGE_DOWN)   cur_row = clampi(cur_row + 10, 0, MAX_ROWS - 1);
        else if (k == '\t')            cur_col = clampi(cur_col + 1, 0, MAX_COLS - 1);
        else if (k == KEY_SHIFT_TAB)   cur_col = clampi(cur_col - 1, 0, MAX_COLS - 1);
        else if (k == KEY_ENTER || k == 'e') { edit_cell(false); }
        else if (k == '=') { edit_cell(true); }
        else if (k == 'c') { clear_cell(); }
        else if (k == 'g') { goto_cell(); }
        else if (k == 's') {
            char buf[256];
            if (last_file[0]) snprintf(buf, sizeof(buf), "%s", last_file);
            else buf[0] = '\0';
            if (prompt_line("Save file: ", buf, buf, sizeof(buf))) {
                if (save_file(buf) == 0) {
                    snprintf(last_file, sizeof(last_file), "%s", buf);
                    statusf("Saved: %s", buf);
                } else {
                    statusf("Save failed: %s", strerror(errno));
                }
            }
        }
        else if (k == 'l') {
            char buf[256];
            if (last_file[0]) snprintf(buf, sizeof(buf), "%s", last_file);
            else buf[0] = '\0';
            if (prompt_line("Load file: ", buf, buf, sizeof(buf))) {
                int n = load_file(buf);
                if (n >= 0) {
                    snprintf(last_file, sizeof(last_file), "%s", buf);
                    statusf("Loaded %d cells from %s", n, buf);
                } else {
                    statusf("Load failed: %s", strerror(errno));
                }
            }
        }
        else if (k == KEY_CTRL_C) {
            break;
        }
    }

    return 0;
}
