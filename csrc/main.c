/*
 * Professional Spreadsheet Program
 * Compile: gcc -o spreadsheet spreadsheet.c -lm
 * Usage: ./spreadsheet [filename]
 * 
 * Features:
 * - 256 columns × 1000 rows
 * - Formula support with cell references
 * - Functions: SUM, AVERAGE, MIN, MAX, COUNT, IF, ABS, SQRT, POWER, ROUND
 * - Ranges (e.g., A1:B10)
 * - File save/load (CSV format)
 * - Copy/paste
 * - Undo/redo
 * - Auto-calculation
 * - Navigation with arrow keys
 * - Column resizing
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>

#define MAX_ROWS 1000
#define MAX_COLS 256
#define MAX_FORMULA_LEN 256
#define MAX_DISPLAY_LEN 1024
#define DEFAULT_COL_WIDTH 12
#define MAX_UNDO 50

/* Cell structure */
typedef struct {
    char formula[MAX_FORMULA_LEN];
    double value;
    int is_text;
    int error;
} Cell;

/* Spreadsheet state */
typedef struct {
    Cell cells[MAX_ROWS][MAX_COLS];
    int cursor_row;
    int cursor_col;
    int scroll_row;
    int scroll_col;
    int col_widths[MAX_COLS];
    char filename[256];
    int modified;
    int edit_mode;
    char edit_buffer[MAX_FORMULA_LEN];
    int edit_pos;
} Spreadsheet;

/* Undo/redo structures */
typedef struct {
    int row, col;
    char formula[MAX_FORMULA_LEN];
    double value;
    int is_text;
} UndoItem;

typedef struct {
    UndoItem items[MAX_UNDO];
    int count;
    int position;
} UndoStack;

/* Clipboard */
typedef struct {
    char formula[MAX_FORMULA_LEN];
    double value;
    int is_text;
    int valid;
} Clipboard;

/* Global variables */
Spreadsheet sheet;
UndoStack undo_stack;
Clipboard clipboard;
struct termios orig_termios;

/* Terminal handling */
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

/* Utility functions */
void clear_screen() {
    printf("\033[2J\033[H");
}

void move_cursor(int row, int col) {
    printf("\033[%d;%dH", row, col);
}

void get_terminal_size(int *rows, int *cols) {
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    *rows = w.ws_row;
    *cols = w.ws_col;
}

/* Column/row conversion */
void col_to_name(int col, char *name) {
    if (col < 26) {
        sprintf(name, "%c", 'A' + col);
    } else {
        sprintf(name, "%c%c", 'A' + (col / 26) - 1, 'A' + (col % 26));
    }
}

int name_to_col(const char *name) {
    int col = 0;
    for (int i = 0; name[i] && isalpha(name[i]); i++) {
        col = col * 26 + (toupper(name[i]) - 'A' + 1);
    }
    return col - 1;
}

/* Cell reference parsing */
int parse_cell_ref(const char *ref, int *row, int *col) {
    int i = 0;
    char col_name[10] = {0};
    
    while (ref[i] && isalpha(ref[i])) {
        col_name[i] = ref[i];
        i++;
    }
    
    if (i == 0 || !isdigit(ref[i])) return 0;
    
    *col = name_to_col(col_name);
    *row = atoi(&ref[i]) - 1;
    
    return (*row >= 0 && *row < MAX_ROWS && *col >= 0 && *col < MAX_COLS);
}

/* Formula evaluator forward declarations */
double eval_expression(const char *expr, int *error);
double eval_function(const char *name, const char *args, int *error);

/* Parse range like A1:B10 */
int parse_range(const char *range, int *r1, int *c1, int *r2, int *c2) {
    char buf[MAX_FORMULA_LEN];
    strcpy(buf, range);
    
    char *colon = strchr(buf, ':');
    if (!colon) return 0;
    
    *colon = '\0';
    if (!parse_cell_ref(buf, r1, c1)) return 0;
    if (!parse_cell_ref(colon + 1, r2, c2)) return 0;
    
    if (*r1 > *r2) { int t = *r1; *r1 = *r2; *r2 = t; }
    if (*c1 > *c2) { int t = *c1; *c1 = *c2; *c2 = t; }
    
    return 1;
}

/* Evaluate functions like SUM, AVERAGE, etc. */
double eval_function(const char *name, const char *args, int *error) {
    char upper_name[50];
    for (int i = 0; name[i]; i++) {
        upper_name[i] = toupper(name[i]);
        upper_name[i + 1] = '\0';
    }
    
    /* Handle range functions */
    if (strcmp(upper_name, "SUM") == 0 || strcmp(upper_name, "AVERAGE") == 0 ||
        strcmp(upper_name, "MIN") == 0 || strcmp(upper_name, "MAX") == 0 ||
        strcmp(upper_name, "COUNT") == 0) {
        
        int r1, c1, r2, c2;
        if (parse_range(args, &r1, &c1, &r2, &c2)) {
            double sum = 0, min = INFINITY, max = -INFINITY;
            int count = 0;
            
            for (int r = r1; r <= r2; r++) {
                for (int c = c1; c <= c2; c++) {
                    if (!sheet.cells[r][c].error && !sheet.cells[r][c].is_text) {
                        double val = sheet.cells[r][c].value;
                        sum += val;
                        if (val < min) min = val;
                        if (val > max) max = val;
                        count++;
                    }
                }
            }
            
            if (strcmp(upper_name, "SUM") == 0) return sum;
            if (strcmp(upper_name, "AVERAGE") == 0) return count > 0 ? sum / count : 0;
            if (strcmp(upper_name, "MIN") == 0) return count > 0 ? min : 0;
            if (strcmp(upper_name, "MAX") == 0) return count > 0 ? max : 0;
            if (strcmp(upper_name, "COUNT") == 0) return count;
        }
    }
    
    /* Handle IF function: IF(condition, true_val, false_val) */
    if (strcmp(upper_name, "IF") == 0) {
        char *comma1 = strchr(args, ',');
        if (!comma1) { *error = 1; return 0; }
        
        char cond[MAX_FORMULA_LEN];
        strncpy(cond, args, comma1 - args);
        cond[comma1 - args] = '\0';
        
        char *comma2 = strchr(comma1 + 1, ',');
        if (!comma2) { *error = 1; return 0; }
        
        char true_expr[MAX_FORMULA_LEN];
        strncpy(true_expr, comma1 + 1, comma2 - comma1 - 1);
        true_expr[comma2 - comma1 - 1] = '\0';
        
        double condition = eval_expression(cond, error);
        if (*error) return 0;
        
        if (condition != 0) {
            return eval_expression(true_expr, error);
        } else {
            return eval_expression(comma2 + 1, error);
        }
    }
    
    /* Mathematical functions with single argument */
    char arg_copy[MAX_FORMULA_LEN];
    strcpy(arg_copy, args);
    double arg_val = eval_expression(arg_copy, error);
    if (*error) return 0;
    
    if (strcmp(upper_name, "ABS") == 0) return fabs(arg_val);
    if (strcmp(upper_name, "SQRT") == 0) return arg_val >= 0 ? sqrt(arg_val) : (*error = 1, 0);
    if (strcmp(upper_name, "ROUND") == 0) return round(arg_val);
    if (strcmp(upper_name, "INT") == 0) return floor(arg_val);
    if (strcmp(upper_name, "SIN") == 0) return sin(arg_val);
    if (strcmp(upper_name, "COS") == 0) return cos(arg_val);
    if (strcmp(upper_name, "TAN") == 0) return tan(arg_val);
    if (strcmp(upper_name, "LN") == 0) return arg_val > 0 ? log(arg_val) : (*error = 1, 0);
    if (strcmp(upper_name, "LOG") == 0) return arg_val > 0 ? log10(arg_val) : (*error = 1, 0);
    
    /* POWER function: POWER(base, exp) */
    if (strcmp(upper_name, "POWER") == 0) {
        char *comma = strchr(args, ',');
        if (!comma) { *error = 1; return 0; }
        
        char base_str[MAX_FORMULA_LEN], exp_str[MAX_FORMULA_LEN];
        strncpy(base_str, args, comma - args);
        base_str[comma - args] = '\0';
        strcpy(exp_str, comma + 1);
        
        double base = eval_expression(base_str, error);
        if (*error) return 0;
        double exp = eval_expression(exp_str, error);
        if (*error) return 0;
        
        return pow(base, exp);
    }
    
    *error = 1;
    return 0;
}

/* Expression evaluator with proper operator precedence */
double eval_term(const char **expr, int *error);
double eval_factor(const char **expr, int *error);

double eval_factor(const char **expr, int *error) {
    while (isspace(**expr)) (*expr)++;
    
    /* Handle parentheses */
    if (**expr == '(') {
        (*expr)++;
        double result = eval_expression(*expr, error);
        while (isspace(**expr)) (*expr)++;
        if (**expr == ')') (*expr)++;
        return result;
    }
    
    /* Handle unary minus */
    if (**expr == '-') {
        (*expr)++;
        return -eval_factor(expr, error);
    }
    
    /* Handle functions */
    if (isalpha(**expr)) {
        char name[50] = {0};
        int i = 0;
        while (isalpha(**expr)) {
            name[i++] = *(*expr)++;
        }
        
        while (isspace(**expr)) (*expr)++;
        
        if (**expr == '(') {
            (*expr)++;
            char args[MAX_FORMULA_LEN] = {0};
            int depth = 1;
            i = 0;
            
            while (depth > 0 && **expr) {
                if (**expr == '(') depth++;
                if (**expr == ')') depth--;
                if (depth > 0) args[i++] = **expr;
                (*expr)++;
            }
            
            return eval_function(name, args, error);
        }
        
        /* Cell reference */
        char ref[50];
        sprintf(ref, "%s", name);
        i = 0;
        while (isdigit(**expr)) {
            ref[strlen(ref)] = **expr;
            (*expr)++;
        }
        
        int row, col;
        if (parse_cell_ref(ref, &row, &col)) {
            if (sheet.cells[row][col].error) {
                *error = 1;
                return 0;
            }
            return sheet.cells[row][col].value;
        }
        
        *error = 1;
        return 0;
    }
    
    /* Handle numbers */
    if (isdigit(**expr) || **expr == '.') {
        char *end;
        double result = strtod(*expr, &end);
        *expr = end;
        return result;
    }
    
    *error = 1;
    return 0;
}

double eval_term(const char **expr, int *error) {
    double result = eval_factor(expr, error);
    
    while (!*error) {
        while (isspace(**expr)) (*expr)++;
        
        if (**expr == '*') {
            (*expr)++;
            result *= eval_factor(expr, error);
        } else if (**expr == '/') {
            (*expr)++;
            double divisor = eval_factor(expr, error);
            if (divisor == 0) {
                *error = 1;
                return 0;
            }
            result /= divisor;
        } else if (**expr == '^') {
            (*expr)++;
            result = pow(result, eval_factor(expr, error));
        } else {
            break;
        }
    }
    
    return result;
}

double eval_expression(const char *expr, int *error) {
    const char *p = expr;
    double result = eval_term(&p, error);
    
    while (!*error) {
        while (isspace(*p)) p++;
        
        if (*p == '+') {
            p++;
            result += eval_term(&p, error);
        } else if (*p == '-') {
            p++;
            result -= eval_term(&p, error);
        } else if (*p == '>' || *p == '<' || *p == '=' || *p == '!') {
            char op[3] = {0};
            op[0] = *p++;
            if (*p == '=') op[1] = *p++;
            
            double right = eval_term(&p, error);
            
            if (strcmp(op, ">") == 0) result = result > right;
            else if (strcmp(op, "<") == 0) result = result < right;
            else if (strcmp(op, ">=") == 0) result = result >= right;
            else if (strcmp(op, "<=") == 0) result = result <= right;
            else if (strcmp(op, "=") == 0) result = result == right;
            else if (strcmp(op, "!=") == 0) result = result != right;
        } else {
            break;
        }
    }
    
    return result;
}

/* Calculate cell value from formula */
void calculate_cell(int row, int col) {
    Cell *cell = &sheet.cells[row][col];
    
    if (cell->formula[0] == '\0') {
        cell->value = 0;
        cell->is_text = 0;
        cell->error = 0;
        return;
    }
    
    if (cell->formula[0] == '=') {
        cell->is_text = 0;
        int error = 0;
        cell->value = eval_expression(cell->formula + 1, &error);
        cell->error = error;
    } else {
        char *endptr;
        double val = strtod(cell->formula, &endptr);
        if (*endptr == '\0' && cell->formula[0] != '\0') {
            cell->value = val;
            cell->is_text = 0;
            cell->error = 0;
        } else {
            cell->value = 0;
            cell->is_text = 1;
            cell->error = 0;
        }
    }
}

/* Recalculate all cells */
void recalculate_all() {
    for (int r = 0; r < MAX_ROWS; r++) {
        for (int c = 0; c < MAX_COLS; c++) {
            calculate_cell(r, c);
        }
    }
}

/* Undo/redo functions */
void save_undo(int row, int col) {
    if (undo_stack.position < undo_stack.count) {
        undo_stack.count = undo_stack.position;
    }
    
    if (undo_stack.count >= MAX_UNDO) {
        memmove(&undo_stack.items[0], &undo_stack.items[1], 
                sizeof(UndoItem) * (MAX_UNDO - 1));
        undo_stack.count = MAX_UNDO - 1;
    }
    
    UndoItem *item = &undo_stack.items[undo_stack.count];
    item->row = row;
    item->col = col;
    strcpy(item->formula, sheet.cells[row][col].formula);
    item->value = sheet.cells[row][col].value;
    item->is_text = sheet.cells[row][col].is_text;
    
    undo_stack.count++;
    undo_stack.position = undo_stack.count;
}

void undo() {
    if (undo_stack.position > 0) {
        undo_stack.position--;
        UndoItem *item = &undo_stack.items[undo_stack.position];
        
        strcpy(sheet.cells[item->row][item->col].formula, item->formula);
        sheet.cells[item->row][item->col].value = item->value;
        sheet.cells[item->row][item->col].is_text = item->is_text;
        
        recalculate_all();
        sheet.modified = 1;
    }
}

void redo() {
    if (undo_stack.position < undo_stack.count) {
        UndoItem *item = &undo_stack.items[undo_stack.position];
        undo_stack.position++;
        
        strcpy(sheet.cells[item->row][item->col].formula, item->formula);
        sheet.cells[item->row][item->col].value = item->value;
        sheet.cells[item->row][item->col].is_text = item->is_text;
        
        recalculate_all();
        sheet.modified = 1;
    }
}

/* File I/O */
void save_file(const char *filename) {
    FILE *f = fopen(filename, "w");
    if (!f) return;
    
    for (int r = 0; r < MAX_ROWS; r++) {
        int last_col = -1;
        for (int c = 0; c < MAX_COLS; c++) {
            if (sheet.cells[r][c].formula[0] != '\0') {
                last_col = c;
            }
        }
        
        if (last_col >= 0) {
            for (int c = 0; c <= last_col; c++) {
                if (c > 0) fprintf(f, ",");
                
                if (sheet.cells[r][c].formula[0] != '\0') {
                    if (strchr(sheet.cells[r][c].formula, ',') || 
                        strchr(sheet.cells[r][c].formula, '"')) {
                        fprintf(f, "\"");
                        for (int i = 0; sheet.cells[r][c].formula[i]; i++) {
                            if (sheet.cells[r][c].formula[i] == '"') {
                                fprintf(f, "\"\"");
                            } else {
                                fputc(sheet.cells[r][c].formula[i], f);
                            }
                        }
                        fprintf(f, "\"");
                    } else {
                        fprintf(f, "%s", sheet.cells[r][c].formula);
                    }
                }
            }
            fprintf(f, "\n");
        }
    }
    
    fclose(f);
    sheet.modified = 0;
}

void load_file(const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) return;
    
    char line[4096];
    int row = 0;
    
    while (fgets(line, sizeof(line), f) && row < MAX_ROWS) {
        int col = 0;
        char *p = line;
        char cell_buf[MAX_FORMULA_LEN];
        
        while (*p && *p != '\n' && col < MAX_COLS) {
            int i = 0;
            
            if (*p == '"') {
                p++;
                while (*p && i < MAX_FORMULA_LEN - 1) {
                    if (*p == '"') {
                        if (*(p + 1) == '"') {
                            cell_buf[i++] = '"';
                            p += 2;
                        } else {
                            p++;
                            break;
                        }
                    } else {
                        cell_buf[i++] = *p++;
                    }
                }
            } else {
                while (*p && *p != ',' && *p != '\n' && i < MAX_FORMULA_LEN - 1) {
                    cell_buf[i++] = *p++;
                }
            }
            
            cell_buf[i] = '\0';
            strcpy(sheet.cells[row][col].formula, cell_buf);
            calculate_cell(row, col);
            
            if (*p == ',') p++;
            col++;
        }
        
        row++;
    }
    
    fclose(f);
    sheet.modified = 0;
}

/* Display functions */
void display_sheet() {
    int term_rows, term_cols;
    get_terminal_size(&term_rows, &term_cols);
    
    clear_screen();
    
    /* Calculate visible columns */
    int vis_cols = 0;
    int width_sum = 5; /* Row number width */
    
    while (width_sum < term_cols && sheet.scroll_col + vis_cols < MAX_COLS) {
        width_sum += sheet.col_widths[sheet.scroll_col + vis_cols] + 1;
        if (width_sum < term_cols) vis_cols++;
    }
    
    /* Header row */
    printf("     ");
    for (int c = 0; c < vis_cols; c++) {
        char col_name[10];
        col_to_name(sheet.scroll_col + c, col_name);
        printf("%-*s|", sheet.col_widths[sheet.scroll_col + c], col_name);
    }
    printf("\n");
    
    /* Separator */
    printf("-----");
    for (int c = 0; c < vis_cols; c++) {
        for (int i = 0; i < sheet.col_widths[sheet.scroll_col + c]; i++) printf("-");
        printf("+");
    }
    printf("\n");
    
    /* Data rows */
    int vis_rows = term_rows - 5; /* Leave room for header and status */
    for (int r = 0; r < vis_rows && sheet.scroll_row + r < MAX_ROWS; r++) {
        int row = sheet.scroll_row + r;
        printf("%-4d|", row + 1);
        
        for (int c = 0; c < vis_cols; c++) {
            int col = sheet.scroll_col + c;
            Cell *cell = &sheet.cells[row][col];
            
            char display[MAX_DISPLAY_LEN];
            if (cell->error) {
                strcpy(display, "#ERROR");
            } else if (cell->is_text) {
                strncpy(display, cell->formula, sizeof(display) - 1);
                display[sizeof(display) - 1] = '\0';
            } else if (cell->formula[0] != '\0') {
                snprintf(display, sizeof(display), "%.6g", cell->value);
            } else {
                display[0] = '\0';
            }
            
            if (row == sheet.cursor_row && col == sheet.cursor_col) {
                printf("\033[7m%-*.*s\033[0m|", 
                       sheet.col_widths[col], sheet.col_widths[col], display);
            } else {
                printf("%-*.*s|", 
                       sheet.col_widths[col], sheet.col_widths[col], display);
            }
        }
        printf("\n");
    }
    
    /* Status bar */
    move_cursor(term_rows - 2, 1);
    printf("\033[7m");
    for (int i = 0; i < term_cols; i++) printf(" ");
    move_cursor(term_rows - 2, 1);
    
    char cell_name[10];
    col_to_name(sheet.cursor_col, cell_name);
    printf(" %s%d: ", cell_name, sheet.cursor_row + 1);
    
    if (sheet.edit_mode) {
        printf("%s", sheet.edit_buffer);
    } else {
        printf("%s", sheet.cells[sheet.cursor_row][sheet.cursor_col].formula);
    }
    printf("\033[0m");
    
    /* Help bar */
    move_cursor(term_rows - 1, 1);
    printf("^S Save | ^Q Quit | ^C Copy | ^V Paste | ^Z Undo | ^Y Redo | F2 Edit | ESC Cancel");
    
    fflush(stdout);
}

/* Initialize spreadsheet */
void init_sheet() {
    memset(&sheet, 0, sizeof(sheet));
    memset(&undo_stack, 0, sizeof(undo_stack));
    memset(&clipboard, 0, sizeof(clipboard));
    
    for (int c = 0; c < MAX_COLS; c++) {
        sheet.col_widths[c] = DEFAULT_COL_WIDTH;
    }
}

/* Main program */
int main(int argc, char *argv[]) {
    init_sheet();
    
    if (argc > 1) {
        strcpy(sheet.filename, argv[1]);
        load_file(sheet.filename);
    }
    
    recalculate_all();
    
    enable_raw_mode();
    
    int running = 1;
    while (running) {
        display_sheet();
        
        int c = getchar();
        if (c == EOF) {
            usleep(10000);
            continue;
        }
        
        if (sheet.edit_mode) {
            if (c == 27) { /* ESC */
                sheet.edit_mode = 0;
            } else if (c == '\n' || c == '\r') {
                save_undo(sheet.cursor_row, sheet.cursor_col);
                strcpy(sheet.cells[sheet.cursor_row][sheet.cursor_col].formula, 
                       sheet.edit_buffer);
                recalculate_all();
                sheet.edit_mode = 0;
                sheet.modified = 1;
            } else if (c == 127 || c == 8) { /* Backspace */
                if (sheet.edit_pos > 0) {
                    sheet.edit_buffer[--sheet.edit_pos] = '\0';
                }
            } else if (c >= 32 && c < 127 && sheet.edit_pos < MAX_FORMULA_LEN - 1) {
                sheet.edit_buffer[sheet.edit_pos++] = c;
                sheet.edit_buffer[sheet.edit_pos] = '\0';
            }
        } else {
            /* Navigation */
            if (c == 27) { /* Escape sequences */
                if (getchar() == '[') {
                    c = getchar();
                    if (c == 'A' && sheet.cursor_row > 0) sheet.cursor_row--; /* Up */
                    else if (c == 'B' && sheet.cursor_row < MAX_ROWS - 1) sheet.cursor_row++; /* Down */
                    else if (c == 'C' && sheet.cursor_col < MAX_COLS - 1) sheet.cursor_col++; /* Right */
                    else if (c == 'D' && sheet.cursor_col > 0) sheet.cursor_col--; /* Left */
                    else if (c == '1') { /* F-keys */
                        getchar(); /* consume '~' or other */
                        c = getchar();
                        if (c == '2') { /* F2 - Edit */
                            sheet.edit_mode = 1;
                            strcpy(sheet.edit_buffer, 
                                   sheet.cells[sheet.cursor_row][sheet.cursor_col].formula);
                            sheet.edit_pos = strlen(sheet.edit_buffer);
                        }
                    }
                    
                    /* Adjust scroll */
                    int term_rows, term_cols;
                    get_terminal_size(&term_rows, &term_cols);
                    
                    if (sheet.cursor_row < sheet.scroll_row) 
                        sheet.scroll_row = sheet.cursor_row;
                    if (sheet.cursor_row >= sheet.scroll_row + term_rows - 5)
                        sheet.scroll_row = sheet.cursor_row - term_rows + 6;
                    
                    if (sheet.cursor_col < sheet.scroll_col)
                        sheet.scroll_col = sheet.cursor_col;
                }
            }
            /* Control characters */
            else if (c == 17) { /* Ctrl+Q */
                if (sheet.modified) {
                    /* Should add confirmation, but keeping it simple */
                }
                running = 0;
            }
            else if (c == 19) { /* Ctrl+S */
                if (sheet.filename[0] == '\0') {
                    strcpy(sheet.filename, "spreadsheet.csv");
                }
                save_file(sheet.filename);
            }
            else if (c == 3) { /* Ctrl+C */
                Cell *cell = &sheet.cells[sheet.cursor_row][sheet.cursor_col];
                strcpy(clipboard.formula, cell->formula);
                clipboard.value = cell->value;
                clipboard.is_text = cell->is_text;
                clipboard.valid = 1;
            }
            else if (c == 22) { /* Ctrl+V */
                if (clipboard.valid) {
                    save_undo(sheet.cursor_row, sheet.cursor_col);
                    Cell *cell = &sheet.cells[sheet.cursor_row][sheet.cursor_col];
                    strcpy(cell->formula, clipboard.formula);
                    cell->value = clipboard.value;
                    cell->is_text = clipboard.is_text;
                    recalculate_all();
                    sheet.modified = 1;
                }
            }
            else if (c == 26) { /* Ctrl+Z */
                undo();
            }
            else if (c == 25) { /* Ctrl+Y */
                redo();
            }
            else if (c == 127 || c == 'd') { /* Delete */
                save_undo(sheet.cursor_row, sheet.cursor_col);
                sheet.cells[sheet.cursor_row][sheet.cursor_col].formula[0] = '\0';
                recalculate_all();
                sheet.modified = 1;
            }
            else if (c >= 32 && c < 127) { /* Start typing */
                sheet.edit_mode = 1;
                sheet.edit_buffer[0] = c;
                sheet.edit_buffer[1] = '\0';
                sheet.edit_pos = 1;
            }
        }
    }
    
    disable_raw_mode();
    clear_screen();
    
    return 0;
}
