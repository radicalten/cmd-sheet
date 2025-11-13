#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#ifdef _WIN32
#include <conio.h>
#include <windows.h>
#else
#include <termios.h>
#include <unistd.h>
#include <sys/select.h>
#endif

/* Configuration */
#define MAX_ROWS 100
#define MAX_COLS 26
#define MAX_CELL_LEN 256
#define MAX_DISPLAY_LEN 12
#define SCREEN_ROWS 20
#define SCREEN_COLS 10

/* Cell structure */
typedef struct {
    char formula[MAX_CELL_LEN];
    double value;
    int is_numeric;
    int error;
} Cell;

/* Global state */
Cell sheet[MAX_ROWS][MAX_COLS];
int cursor_row = 0;
int cursor_col = 0;
int scroll_row = 0;
int scroll_col = 0;
char input_buffer[MAX_CELL_LEN];
int input_mode = 0;
int input_pos = 0;
char status_msg[256] = "Ready";

/* Terminal handling */
#ifndef _WIN32
struct termios orig_termios;

void disable_raw_mode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void enable_raw_mode() {
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disable_raw_mode);
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

int getch() {
    int c;
    if (read(STDIN_FILENO, &c, 1) == 1) return c;
    return -1;
}
#else
void enable_raw_mode() {
    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode;
    GetConsoleMode(hStdin, &mode);
    SetConsoleMode(hStdin, mode & ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT));
}
#endif

/* Clear screen and position cursor */
void clear_screen() {
    printf("\033[2J\033[H");
    fflush(stdout);
}

void move_cursor(int row, int col) {
    printf("\033[%d;%dH", row + 1, col + 1);
}

/* Cell reference parsing */
int parse_cell_ref(const char *ref, int *row, int *col) {
    if (!ref || !isalpha(ref[0])) return 0;
    *col = toupper(ref[0]) - 'A';
    *row = atoi(ref + 1) - 1;
    return (*col >= 0 && *col < MAX_COLS && *row >= 0 && *row < MAX_ROWS);
}

void get_cell_name(int row, int col, char *name) {
    sprintf(name, "%c%d", 'A' + col, row + 1);
}

/* Expression evaluator */
double eval_expr(const char *expr, int *error);

double eval_function(const char *name, const char *args, int *error) {
    char arg_buf[MAX_CELL_LEN];
    strncpy(arg_buf, args, MAX_CELL_LEN - 1);
    
    if (strcmp(name, "SUM") == 0) {
        double sum = 0;
        char *token = strtok(arg_buf, ",:");
        int start_row, start_col, end_row, end_col;
        
        /* Check for range (A1:B5) */
        const char *colon = strchr(args, ':');
        if (colon) {
            char start[32], end[32];
            int len = colon - args;
            strncpy(start, args, len);
            start[len] = 0;
            strcpy(end, colon + 1);
            
            if (parse_cell_ref(start, &start_row, &start_col) &&
                parse_cell_ref(end, &end_row, &end_col)) {
                for (int r = start_row; r <= end_row; r++) {
                    for (int c = start_col; c <= end_col; c++) {
                        if (sheet[r][c].is_numeric && !sheet[r][c].error)
                            sum += sheet[r][c].value;
                    }
                }
                return sum;
            }
        }
        
        /* Individual cells */
        token = strtok(arg_buf, ",");
        while (token) {
            int row, col;
            if (parse_cell_ref(token, &row, &col)) {
                if (sheet[row][col].is_numeric && !sheet[row][col].error)
                    sum += sheet[row][col].value;
            } else {
                sum += eval_expr(token, error);
            }
            token = strtok(NULL, ",");
        }
        return sum;
    }
    
    if (strcmp(name, "AVG") == 0 || strcmp(name, "AVERAGE") == 0) {
        double sum = 0;
        int count = 0;
        const char *colon = strchr(args, ':');
        
        if (colon) {
            char start[32], end[32];
            int len = colon - args;
            strncpy(start, args, len);
            start[len] = 0;
            strcpy(end, colon + 1);
            
            int start_row, start_col, end_row, end_col;
            if (parse_cell_ref(start, &start_row, &start_col) &&
                parse_cell_ref(end, &end_row, &end_col)) {
                for (int r = start_row; r <= end_row; r++) {
                    for (int c = start_col; c <= end_col; c++) {
                        if (sheet[r][c].is_numeric && !sheet[r][c].error) {
                            sum += sheet[r][c].value;
                            count++;
                        }
                    }
                }
                return count > 0 ? sum / count : 0;
            }
        }
        
        token = strtok(arg_buf, ",");
        while (token) {
            int row, col;
            if (parse_cell_ref(token, &row, &col)) {
                if (sheet[row][col].is_numeric && !sheet[row][col].error) {
                    sum += sheet[row][col].value;
                    count++;
                }
            }
            token = strtok(NULL, ",");
        }
        return count > 0 ? sum / count : 0;
    }
    
    if (strcmp(name, "MIN") == 0) {
        double min = INFINITY;
        const char *colon = strchr(args, ':');
        
        if (colon) {
            char start[32], end[32];
            int len = colon - args;
            strncpy(start, args, len);
            start[len] = 0;
            strcpy(end, colon + 1);
            
            int start_row, start_col, end_row, end_col;
            if (parse_cell_ref(start, &start_row, &start_col) &&
                parse_cell_ref(end, &end_row, &end_col)) {
                for (int r = start_row; r <= end_row; r++) {
                    for (int c = start_col; c <= end_col; c++) {
                        if (sheet[r][c].is_numeric && !sheet[r][c].error) {
                            if (sheet[r][c].value < min) min = sheet[r][c].value;
                        }
                    }
                }
            }
        }
        return min == INFINITY ? 0 : min;
    }
    
    if (strcmp(name, "MAX") == 0) {
        double max = -INFINITY;
        const char *colon = strchr(args, ':');
        
        if (colon) {
            char start[32], end[32];
            int len = colon - args;
            strncpy(start, args, len);
            start[len] = 0;
            strcpy(end, colon + 1);
            
            int start_row, start_col, end_row, end_col;
            if (parse_cell_ref(start, &start_row, &start_col) &&
                parse_cell_ref(end, &end_row, &end_col)) {
                for (int r = start_row; r <= end_row; r++) {
                    for (int c = start_col; c <= end_col; c++) {
                        if (sheet[r][c].is_numeric && !sheet[r][c].error) {
                            if (sheet[r][c].value > max) max = sheet[r][c].value;
                        }
                    }
                }
            }
        }
        return max == -INFINITY ? 0 : max;
    }
    
    if (strcmp(name, "SQRT") == 0) {
        return sqrt(eval_expr(args, error));
    }
    
    if (strcmp(name, "ABS") == 0) {
        return fabs(eval_expr(args, error));
    }
    
    *error = 1;
    return 0;
}

double eval_term(const char **expr, int *error);

double eval_factor(const char **expr, int *error) {
    while (isspace(**expr)) (*expr)++;
    
    if (**expr == '(') {
        (*expr)++;
        double val = eval_expr(*expr, error);
        while (isspace(**expr)) (*expr)++;
        if (**expr == ')') (*expr)++;
        return val;
    }
    
    if (**expr == '-') {
        (*expr)++;
        return -eval_factor(expr, error);
    }
    
    if (**expr == '+') {
        (*expr)++;
        return eval_factor(expr, error);
    }
    
    /* Function call */
    if (isalpha(**expr)) {
        char name[32] = {0};
        char args[MAX_CELL_LEN] = {0};
        int i = 0;
        
        while (isalpha(**expr) || isdigit(**expr)) {
            if (i < 31) name[i++] = **expr;
            (*expr)++;
        }
        
        while (isspace(**expr)) (*expr)++;
        
        if (**expr == '(') {
            (*expr)++;
            int paren = 1;
            i = 0;
            while (paren > 0 && **expr) {
                if (**expr == '(') paren++;
                if (**expr == ')') paren--;
                if (paren > 0 && i < MAX_CELL_LEN - 1) args[i++] = **expr;
                (*expr)++;
            }
            return eval_function(name, args, error);
        } else {
            /* Cell reference */
            int row, col;
            if (parse_cell_ref(name, &row, &col)) {
                if (sheet[row][col].error) {
                    *error = 1;
                    return 0;
                }
                return sheet[row][col].value;
            }
            *error = 1;
            return 0;
        }
    }
    
    /* Number */
    char *endptr;
    double val = strtod(*expr, &endptr);
    if (endptr > *expr) {
        *expr = endptr;
        return val;
    }
    
    *error = 1;
    return 0;
}

double eval_term(const char **expr, int *error) {
    double val = eval_factor(expr, error);
    
    while (1) {
        while (isspace(**expr)) (*expr)++;
        
        if (**expr == '*') {
            (*expr)++;
            val *= eval_factor(expr, error);
        } else if (**expr == '/') {
            (*expr)++;
            double divisor = eval_factor(expr, error);
            if (divisor == 0) {
                *error = 1;
                return 0;
            }
            val /= divisor;
        } else if (**expr == '^') {
            (*expr)++;
            val = pow(val, eval_factor(expr, error));
        } else {
            break;
        }
    }
    
    return val;
}

double eval_expr(const char *expr, int *error) {
    const char *p = expr;
    double val = eval_term(&p, error);
    
    while (1) {
        while (isspace(*p)) p++;
        
        if (*p == '+') {
            p++;
            val += eval_term(&p, error);
        } else if (*p == '-') {
            p++;
            val -= eval_term(&p, error);
        } else {
            break;
        }
    }
    
    return val;
}

/* Cell evaluation */
void eval_cell(int row, int col) {
    Cell *cell = &sheet[row][col];
    cell->error = 0;
    
    if (cell->formula[0] == 0) {
        cell->is_numeric = 0;
        cell->value = 0;
        return;
    }
    
    if (cell->formula[0] == '=') {
        int error = 0;
        cell->value = eval_expr(cell->formula + 1, &error);
        cell->is_numeric = !error;
        cell->error = error;
    } else {
        char *endptr;
        double val = strtod(cell->formula, &endptr);
        if (*endptr == 0 && endptr > cell->formula) {
            cell->is_numeric = 1;
            cell->value = val;
        } else {
            cell->is_numeric = 0;
        }
    }
}

void recalc_all() {
    for (int i = 0; i < MAX_ROWS; i++) {
        for (int j = 0; j < MAX_COLS; j++) {
            eval_cell(i, j);
        }
    }
    /* Second pass for dependencies */
    for (int i = 0; i < MAX_ROWS; i++) {
        for (int j = 0; j < MAX_COLS; j++) {
            eval_cell(i, j);
        }
    }
}

/* Display */
void display_cell(int row, int col) {
    Cell *cell = &sheet[row][col];
    char display[MAX_DISPLAY_LEN + 1];
    
    if (cell->error) {
        snprintf(display, MAX_DISPLAY_LEN + 1, "ERROR");
    } else if (cell->formula[0] == 0) {
        snprintf(display, MAX_DISPLAY_LEN + 1, "");
    } else if (cell->is_numeric) {
        if (fabs(cell->value) >= 1e10 || (fabs(cell->value) < 0.01 && cell->value != 0)) {
            snprintf(display, MAX_DISPLAY_LEN + 1, "%.3e", cell->value);
        } else {
            snprintf(display, MAX_DISPLAY_LEN + 1, "%.2f", cell->value);
        }
    } else {
        snprintf(display, MAX_DISPLAY_LEN + 1, "%s", cell->formula);
    }
    
    printf("%-*.*s", MAX_DISPLAY_LEN, MAX_DISPLAY_LEN, display);
}

void draw_screen() {
    clear_screen();
    
    /* Header */
    printf("\033[7m");  /* Reverse video */
    printf("   ");
    for (int c = scroll_col; c < scroll_col + SCREEN_COLS && c < MAX_COLS; c++) {
        printf(" %c%-*s", 'A' + c, MAX_DISPLAY_LEN - 1, "");
    }
    printf("\033[0m\n");
    
    /* Rows */
    for (int r = scroll_row; r < scroll_row + SCREEN_ROWS && r < MAX_ROWS; r++) {
        printf("\033[7m%-3d\033[0m", r + 1);
        for (int c = scroll_col; c < scroll_col + SCREEN_COLS && c < MAX_COLS; c++) {
            if (r == cursor_row && c == cursor_col && !input_mode) {
                printf("\033[7m");  /* Highlight current cell */
            }
            printf(" ");
            display_cell(r, c);
            if (r == cursor_row && c == cursor_col && !input_mode) {
                printf("\033[0m");
            }
        }
        printf("\n");
    }
    
    /* Status bar */
    printf("\033[%d;1H\033[7m", SCREEN_ROWS + 3);
    char cell_name[8];
    get_cell_name(cursor_row, cursor_col, cell_name);
    printf("%-10s", cell_name);
    
    if (input_mode) {
        printf(" Edit: %-60s", input_buffer);
    } else {
        printf(" %-70s", sheet[cursor_row][cursor_col].formula);
    }
    printf("\033[0m\n");
    
    /* Command bar */
    printf("\033[K%s\n", status_msg);
    printf("\033[K[Arrows]Move [Enter]Edit [S]ave [L]oad [Q]uit [C]opy [V]Paste [D]elete [R]ecalc");
    
    fflush(stdout);
}

/* File operations */
void save_file(const char *filename) {
    FILE *f = fopen(filename, "w");
    if (!f) {
        snprintf(status_msg, sizeof(status_msg), "Error: Cannot save to %s", filename);
        return;
    }
    
    fprintf(f, "SPREADSHEET_V1\n");
    for (int r = 0; r < MAX_ROWS; r++) {
        for (int c = 0; c < MAX_COLS; c++) {
            if (sheet[r][c].formula[0]) {
                fprintf(f, "%d,%d,%s\n", r, c, sheet[r][c].formula);
            }
        }
    }
    
    fclose(f);
    snprintf(status_msg, sizeof(status_msg), "Saved to %s", filename);
}

void load_file(const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        snprintf(status_msg, sizeof(status_msg), "Error: Cannot open %s", filename);
        return;
    }
    
    char line[MAX_CELL_LEN + 32];
    if (!fgets(line, sizeof(line), f) || strncmp(line, "SPREADSHEET_V1", 14) != 0) {
        fclose(f);
        snprintf(status_msg, sizeof(status_msg), "Error: Invalid file format");
        return;
    }
    
    /* Clear sheet */
    memset(sheet, 0, sizeof(sheet));
    
    while (fgets(line, sizeof(line), f)) {
        int r, c;
        char formula[MAX_CELL_LEN];
        if (sscanf(line, "%d,%d,%[^\n]", &r, &c, formula) == 3) {
            if (r >= 0 && r < MAX_ROWS && c >= 0 && c < MAX_COLS) {
                strncpy(sheet[r][c].formula, formula, MAX_CELL_LEN - 1);
            }
        }
    }
    
    fclose(f);
    recalc_all();
    snprintf(status_msg, sizeof(status_msg), "Loaded from %s", filename);
}

/* Copy/paste */
Cell clipboard;

void copy_cell() {
    clipboard = sheet[cursor_row][cursor_col];
    snprintf(status_msg, sizeof(status_msg), "Copied");
}

void paste_cell() {
    sheet[cursor_row][cursor_col] = clipboard;
    eval_cell(cursor_row, cursor_col);
    recalc_all();
    snprintf(status_msg, sizeof(status_msg), "Pasted");
}

void delete_cell() {
    memset(&sheet[cursor_row][cursor_col], 0, sizeof(Cell));
    recalc_all();
    snprintf(status_msg, sizeof(status_msg), "Deleted");
}

/* Input handling */
void handle_input() {
    int ch = getch();
    if (ch == -1) return;
    
    if (input_mode) {
        if (ch == 27) {  /* ESC */
            input_mode = 0;
            strcpy(status_msg, "Cancelled");
        } else if (ch == '\n' || ch == '\r') {
            strncpy(sheet[cursor_row][cursor_col].formula, input_buffer, MAX_CELL_LEN - 1);
            eval_cell(cursor_row, cursor_col);
            recalc_all();
            input_mode = 0;
            strcpy(status_msg, "Ready");
        } else if (ch == 127 || ch == 8) {  /* Backspace */
            if (input_pos > 0) {
                input_buffer[--input_pos] = 0;
            }
        } else if (input_pos < MAX_CELL_LEN - 1 && ch >= 32 && ch < 127) {
            input_buffer[input_pos++] = ch;
            input_buffer[input_pos] = 0;
        }
    } else {
        /* Handle escape sequences for arrow keys */
        if (ch == 27) {
            int ch2 = getch();
            if (ch2 == '[') {
                int ch3 = getch();
                if (ch3 == 'A' && cursor_row > 0) cursor_row--;  /* Up */
                else if (ch3 == 'B' && cursor_row < MAX_ROWS - 1) cursor_row++;  /* Down */
                else if (ch3 == 'C' && cursor_col < MAX_COLS - 1) cursor_col++;  /* Right */
                else if (ch3 == 'D' && cursor_col > 0) cursor_col--;  /* Left */
            }
        } else if (ch == '\n' || ch == '\r') {
            input_mode = 1;
            input_pos = 0;
            strncpy(input_buffer, sheet[cursor_row][cursor_col].formula, MAX_CELL_LEN - 1);
            input_pos = strlen(input_buffer);
            strcpy(status_msg, "Editing...");
        } else if (ch == 's' || ch == 'S') {
            save_file("sheet.txt");
        } else if (ch == 'l' || ch == 'L') {
            load_file("sheet.txt");
        } else if (ch == 'c' || ch == 'C') {
            copy_cell();
        } else if (ch == 'v' || ch == 'V') {
            paste_cell();
        } else if (ch == 'd' || ch == 'D') {
            delete_cell();
        } else if (ch == 'r' || ch == 'R') {
            recalc_all();
            strcpy(status_msg, "Recalculated");
        } else if (ch == 'q' || ch == 'Q') {
            clear_screen();
            exit(0);
        }
        
        /* Adjust scroll */
        if (cursor_row < scroll_row) scroll_row = cursor_row;
        if (cursor_row >= scroll_row + SCREEN_ROWS) scroll_row = cursor_row - SCREEN_ROWS + 1;
        if (cursor_col < scroll_col) scroll_col = cursor_col;
        if (cursor_col >= scroll_col + SCREEN_COLS) scroll_col = cursor_col - SCREEN_COLS + 1;
    }
}

/* Main */
int main() {
    memset(sheet, 0, sizeof(sheet));
    enable_raw_mode();
    
    while (1) {
        draw_screen();
        handle_input();
    }
    
    return 0;
}
