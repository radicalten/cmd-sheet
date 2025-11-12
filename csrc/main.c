/*
 * MicroSheet - A Terminal-Based Spreadsheet
 * Compile: gcc -o microsheet microsheet.c -lm
 * Features: Formulas, cell references, functions (SUM, AVG, MIN, MAX, COUNT),
 *           save/load, copy/paste, navigation, and more
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>

#define MAX_ROWS 100
#define MAX_COLS 26
#define CELL_WIDTH 12
#define MAX_FORMULA_LEN 256
#define MAX_DISPLAY_LEN 11

// Cell structure
typedef struct {
    char formula[MAX_FORMULA_LEN];
    double value;
    int is_string;
    char display[MAX_DISPLAY_LEN + 1];
    int error;
} Cell;

// Spreadsheet state
typedef struct {
    Cell cells[MAX_ROWS][MAX_COLS];
    int cursor_row;
    int cursor_col;
    int view_row;
    int view_col;
    char input_buffer[MAX_FORMULA_LEN];
    int input_pos;
    int edit_mode;
    int needs_recalc;
    char status_msg[256];
    char clipboard[MAX_FORMULA_LEN];
    int clipboard_has_data;
} Sheet;

// Terminal handling
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

void clear_screen() {
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
}

void set_cursor(int row, int col) {
    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", row, col);
    write(STDOUT_FILENO, buf, strlen(buf));
}

void set_color(int fg, int bg) {
    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dm", fg, bg);
    write(STDOUT_FILENO, buf, strlen(buf));
}

void reset_color() {
    write(STDOUT_FILENO, "\x1b[0m", 4);
}

// Utility functions
void col_to_name(int col, char *name) {
    name[0] = 'A' + col;
    name[1] = '\0';
}

int name_to_col(const char *name) {
    if (name[0] >= 'A' && name[0] <= 'Z')
        return name[0] - 'A';
    if (name[0] >= 'a' && name[0] <= 'z')
        return name[0] - 'a';
    return -1;
}

void set_status(Sheet *sheet, const char *msg) {
    strncpy(sheet->status_msg, msg, sizeof(sheet->status_msg) - 1);
}

// Formula parser and evaluator
typedef struct {
    const char *pos;
    const char *start;
    Sheet *sheet;
    int error;
} Parser;

double parse_expr(Parser *p);

void skip_whitespace(Parser *p) {
    while (*p->pos == ' ' || *p->pos == '\t') p->pos++;
}

double parse_number(Parser *p) {
    char *end;
    double val = strtod(p->pos, &end);
    if (end == p->pos) {
        p->error = 1;
        return 0;
    }
    p->pos = end;
    return val;
}

double parse_cell_ref(Parser *p) {
    char col_name[3] = {0};
    int row, col;
    
    if (!isalpha(*p->pos)) {
        p->error = 1;
        return 0;
    }
    
    col_name[0] = toupper(*p->pos++);
    col = name_to_col(col_name);
    
    if (!isdigit(*p->pos)) {
        p->error = 1;
        return 0;
    }
    
    row = atoi(p->pos) - 1;
    while (isdigit(*p->pos)) p->pos++;
    
    if (col < 0 || col >= MAX_COLS || row < 0 || row >= MAX_ROWS) {
        p->error = 1;
        return 0;
    }
    
    if (p->sheet->cells[row][col].error) {
        p->error = 1;
        return 0;
    }
    
    return p->sheet->cells[row][col].value;
}

double parse_range_function(Parser *p, const char *func_name) {
    int func_type = 0; // 0=SUM, 1=AVG, 2=MIN, 3=MAX, 4=COUNT
    
    if (strcmp(func_name, "AVG") == 0) func_type = 1;
    else if (strcmp(func_name, "MIN") == 0) func_type = 2;
    else if (strcmp(func_name, "MAX") == 0) func_type = 3;
    else if (strcmp(func_name, "COUNT") == 0) func_type = 4;
    
    skip_whitespace(p);
    if (*p->pos != '(') {
        p->error = 1;
        return 0;
    }
    p->pos++;
    
    skip_whitespace(p);
    
    // Parse start cell (e.g., A1)
    if (!isalpha(*p->pos)) {
        p->error = 1;
        return 0;
    }
    int start_col = toupper(*p->pos++) - 'A';
    int start_row = atoi(p->pos) - 1;
    while (isdigit(*p->pos)) p->pos++;
    
    skip_whitespace(p);
    if (*p->pos != ':') {
        p->error = 1;
        return 0;
    }
    p->pos++;
    skip_whitespace(p);
    
    // Parse end cell (e.g., A5)
    if (!isalpha(*p->pos)) {
        p->error = 1;
        return 0;
    }
    int end_col = toupper(*p->pos++) - 'A';
    int end_row = atoi(p->pos) - 1;
    while (isdigit(*p->pos)) p->pos++;
    
    skip_whitespace(p);
    if (*p->pos != ')') {
        p->error = 1;
        return 0;
    }
    p->pos++;
    
    // Calculate function
    double result = 0;
    int count = 0;
    int first = 1;
    
    for (int r = start_row; r <= end_row && r < MAX_ROWS; r++) {
        for (int c = start_col; c <= end_col && c < MAX_COLS; c++) {
            if (p->sheet->cells[r][c].error || p->sheet->cells[r][c].is_string)
                continue;
            
            double val = p->sheet->cells[r][c].value;
            count++;
            
            switch (func_type) {
                case 0: result += val; break; // SUM
                case 1: result += val; break; // AVG
                case 2: if (first || val < result) result = val; break; // MIN
                case 3: if (first || val > result) result = val; break; // MAX
                case 4: break; // COUNT
            }
            first = 0;
        }
    }
    
    if (func_type == 1 && count > 0) result /= count; // AVG
    if (func_type == 4) result = count; // COUNT
    
    return result;
}

double parse_function(Parser *p) {
    char func_name[32] = {0};
    int i = 0;
    
    while (isalpha(*p->pos) && i < 31) {
        func_name[i++] = toupper(*p->pos++);
    }
    
    // Range functions
    if (strcmp(func_name, "SUM") == 0 || strcmp(func_name, "AVG") == 0 ||
        strcmp(func_name, "MIN") == 0 || strcmp(func_name, "MAX") == 0 ||
        strcmp(func_name, "COUNT") == 0) {
        return parse_range_function(p, func_name);
    }
    
    // Single-argument functions
    skip_whitespace(p);
    if (*p->pos != '(') {
        p->error = 1;
        return 0;
    }
    p->pos++;
    
    double arg = parse_expr(p);
    if (p->error) return 0;
    
    skip_whitespace(p);
    if (*p->pos != ')') {
        p->error = 1;
        return 0;
    }
    p->pos++;
    
    if (strcmp(func_name, "ABS") == 0) return fabs(arg);
    if (strcmp(func_name, "SQRT") == 0) return sqrt(arg);
    if (strcmp(func_name, "INT") == 0) return floor(arg);
    if (strcmp(func_name, "ROUND") == 0) return round(arg);
    
    p->error = 1;
    return 0;
}

double parse_primary(Parser *p) {
    skip_whitespace(p);
    
    if (*p->pos == '(') {
        p->pos++;
        double val = parse_expr(p);
        skip_whitespace(p);
        if (*p->pos != ')') {
            p->error = 1;
            return 0;
        }
        p->pos++;
        return val;
    }
    
    if (isalpha(*p->pos)) {
        const char *start = p->pos;
        p->pos++;
        
        // Check if it's a function or cell reference
        if (*p->pos == '(' || isalpha(*p->pos)) {
            p->pos = start;
            return parse_function(p);
        } else if (isdigit(*p->pos)) {
            p->pos = start;
            return parse_cell_ref(p);
        } else {
            p->error = 1;
            return 0;
        }
    }
    
    if (isdigit(*p->pos) || *p->pos == '.') {
        return parse_number(p);
    }
    
    if (*p->pos == '-') {
        p->pos++;
        return -parse_primary(p);
    }
    
    if (*p->pos == '+') {
        p->pos++;
        return parse_primary(p);
    }
    
    p->error = 1;
    return 0;
}

double parse_term(Parser *p) {
    double val = parse_primary(p);
    if (p->error) return 0;
    
    while (1) {
        skip_whitespace(p);
        if (*p->pos == '*') {
            p->pos++;
            val *= parse_primary(p);
        } else if (*p->pos == '/') {
            p->pos++;
            double divisor = parse_primary(p);
            if (divisor == 0) {
                p->error = 1;
                return 0;
            }
            val /= divisor;
        } else if (*p->pos == '^') {
            p->pos++;
            val = pow(val, parse_primary(p));
        } else {
            break;
        }
        if (p->error) return 0;
    }
    
    return val;
}

double parse_expr(Parser *p) {
    double val = parse_term(p);
    if (p->error) return 0;
    
    while (1) {
        skip_whitespace(p);
        if (*p->pos == '+') {
            p->pos++;
            val += parse_term(p);
        } else if (*p->pos == '-') {
            p->pos++;
            val -= parse_term(p);
        } else {
            break;
        }
        if (p->error) return 0;
    }
    
    return val;
}

void evaluate_cell(Sheet *sheet, int row, int col) {
    Cell *cell = &sheet->cells[row][col];
    cell->error = 0;
    cell->is_string = 0;
    cell->value = 0;
    
    if (cell->formula[0] == '\0') {
        strcpy(cell->display, "");
        return;
    }
    
    if (cell->formula[0] == '=') {
        // Formula
        Parser p = {cell->formula + 1, cell->formula + 1, sheet, 0};
        cell->value = parse_expr(&p);
        
        skip_whitespace(&p);
        if (p.error || *p.pos != '\0') {
            cell->error = 1;
            strcpy(cell->display, "ERROR");
        } else {
            snprintf(cell->display, sizeof(cell->display), "%.6g", cell->value);
        }
    } else if (isdigit(cell->formula[0]) || 
               (cell->formula[0] == '-' && isdigit(cell->formula[1])) ||
               cell->formula[0] == '.') {
        // Number
        cell->value = atof(cell->formula);
        snprintf(cell->display, sizeof(cell->display), "%.6g", cell->value);
    } else {
        // String
        cell->is_string = 1;
        strncpy(cell->display, cell->formula, MAX_DISPLAY_LEN);
        cell->display[MAX_DISPLAY_LEN] = '\0';
    }
}

void recalculate_all(Sheet *sheet) {
    // Simple recalculation (multiple passes for dependencies)
    for (int pass = 0; pass < 5; pass++) {
        for (int r = 0; r < MAX_ROWS; r++) {
            for (int c = 0; c < MAX_COLS; c++) {
                evaluate_cell(sheet, r, c);
            }
        }
    }
    sheet->needs_recalc = 0;
}

void init_sheet(Sheet *sheet) {
    memset(sheet, 0, sizeof(Sheet));
    strcpy(sheet->status_msg, "MicroSheet Ready | F1:Help F2:Save F3:Load F10:Quit");
}

void draw_sheet(Sheet *sheet) {
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    int screen_rows = w.ws_row;
    int screen_cols = w.ws_col;
    
    int max_display_rows = screen_rows - 4;
    int max_display_cols = screen_cols / CELL_WIDTH;
    
    clear_screen();
    
    // Column headers
    set_cursor(1, 1);
    printf("     ");
    for (int c = 0; c < max_display_cols && (sheet->view_col + c) < MAX_COLS; c++) {
        char col_name[3];
        col_to_name(sheet->view_col + c, col_name);
        printf("%-*s", CELL_WIDTH, col_name);
    }
    
    // Rows
    for (int r = 0; r < max_display_rows && (sheet->view_row + r) < MAX_ROWS; r++) {
        set_cursor(r + 2, 1);
        printf("%-4d ", sheet->view_row + r + 1);
        
        for (int c = 0; c < max_display_cols && (sheet->view_col + c) < MAX_COLS; c++) {
            int actual_row = sheet->view_row + r;
            int actual_col = sheet->view_col + c;
            
            if (actual_row == sheet->cursor_row && actual_col == sheet->cursor_col) {
                set_color(37, 44); // White on blue
            }
            
            Cell *cell = &sheet->cells[actual_row][actual_col];
            
            if (cell->is_string) {
                printf("%-*s", CELL_WIDTH, cell->display);
            } else {
                printf("%*s", CELL_WIDTH, cell->display);
            }
            
            if (actual_row == sheet->cursor_row && actual_col == sheet->cursor_col) {
                reset_color();
            }
        }
    }
    
    // Current cell info
    set_cursor(screen_rows - 2, 1);
    printf("\x1b[K"); // Clear line
    char col_name[3];
    col_to_name(sheet->cursor_col, col_name);
    printf("Cell: %s%d", col_name, sheet->cursor_row + 1);
    
    set_cursor(screen_rows - 1, 1);
    printf("\x1b[K");
    if (sheet->edit_mode) {
        printf("Edit: %s", sheet->input_buffer);
    } else {
        Cell *cell = &sheet->cells[sheet->cursor_row][sheet->cursor_col];
        printf("Formula: %s", cell->formula);
    }
    
    // Status line
    set_cursor(screen_rows, 1);
    set_color(30, 47); // Black on white
    printf("%-*s", screen_cols, sheet->status_msg);
    reset_color();
    
    fflush(stdout);
}

void save_sheet(Sheet *sheet, const char *filename) {
    FILE *f = fopen(filename, "w");
    if (!f) {
        set_status(sheet, "Error: Cannot save file");
        return;
    }
    
    for (int r = 0; r < MAX_ROWS; r++) {
        for (int c = 0; c < MAX_COLS; c++) {
            if (sheet->cells[r][c].formula[0] != '\0') {
                fprintf(f, "%d,%d,%s\n", r, c, sheet->cells[r][c].formula);
            }
        }
    }
    
    fclose(f);
    set_status(sheet, "File saved successfully");
}

void load_sheet(Sheet *sheet, const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        set_status(sheet, "Error: Cannot open file");
        return;
    }
    
    // Clear sheet
    for (int r = 0; r < MAX_ROWS; r++) {
        for (int c = 0; c < MAX_COLS; c++) {
            sheet->cells[r][c].formula[0] = '\0';
        }
    }
    
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        int r, c;
        char formula[MAX_FORMULA_LEN];
        if (sscanf(line, "%d,%d,%[^\n]", &r, &c, formula) == 3) {
            if (r >= 0 && r < MAX_ROWS && c >= 0 && c < MAX_COLS) {
                strncpy(sheet->cells[r][c].formula, formula, MAX_FORMULA_LEN - 1);
            }
        }
    }
    
    fclose(f);
    sheet->needs_recalc = 1;
    set_status(sheet, "File loaded successfully");
}

void enter_edit_mode(Sheet *sheet) {
    Cell *cell = &sheet->cells[sheet->cursor_row][sheet->cursor_col];
    strncpy(sheet->input_buffer, cell->formula, MAX_FORMULA_LEN - 1);
    sheet->input_pos = strlen(sheet->input_buffer);
    sheet->edit_mode = 1;
}

void exit_edit_mode(Sheet *sheet, int save) {
    if (save) {
        Cell *cell = &sheet->cells[sheet->cursor_row][sheet->cursor_col];
        strncpy(cell->formula, sheet->input_buffer, MAX_FORMULA_LEN - 1);
        sheet->needs_recalc = 1;
    }
    sheet->edit_mode = 0;
    sheet->input_buffer[0] = '\0';
    sheet->input_pos = 0;
}

void show_help(Sheet *sheet) {
    clear_screen();
    printf("\n=== MicroSheet Help ===\n\n");
    printf("Navigation:\n");
    printf("  Arrow Keys    - Move cursor\n");
    printf("  Home/End      - First/Last column\n");
    printf("  PgUp/PgDn     - Scroll up/down\n\n");
    printf("Editing:\n");
    printf("  Enter/F2      - Edit cell\n");
    printf("  Esc           - Cancel edit\n");
    printf("  Delete        - Clear cell\n");
    printf("  Ctrl+C        - Copy cell\n");
    printf("  Ctrl+V        - Paste cell\n\n");
    printf("Formulas:\n");
    printf("  Start with =  - Formula (e.g., =A1+B2)\n");
    printf("  Operators     - + - * / ^\n");
    printf("  Functions     - SUM(A1:A5), AVG(A1:A5)\n");
    printf("                  MIN(A1:A5), MAX(A1:A5)\n");
    printf("                  COUNT(A1:A5)\n");
    printf("                  ABS(x), SQRT(x), INT(x)\n\n");
    printf("File:\n");
    printf("  F2            - Save (enter filename)\n");
    printf("  F3            - Load (enter filename)\n");
    printf("  F10           - Quit\n\n");
    printf("Press any key to continue...");
    fflush(stdout);
    
    char c;
    while (read(STDIN_FILENO, &c, 1) == 0);
}

int main() {
    Sheet sheet;
    init_sheet(&sheet);
    enable_raw_mode();
    
    char c, seq[3];
    int running = 1;
    
    while (running) {
        if (sheet.needs_recalc) {
            recalculate_all(&sheet);
        }
        
        draw_sheet(&sheet);
        
        if (read(STDIN_FILENO, &c, 1) == 0) continue;
        
        if (sheet.edit_mode) {
            if (c == '\x1b') { // ESC
                exit_edit_mode(&sheet, 0);
            } else if (c == '\r' || c == '\n') {
                exit_edit_mode(&sheet, 1);
            } else if (c == 127 || c == '\b') { // Backspace
                if (sheet.input_pos > 0) {
                    sheet.input_buffer[--sheet.input_pos] = '\0';
                }
            } else if (c >= 32 && c < 127) {
                if (sheet.input_pos < MAX_FORMULA_LEN - 1) {
                    sheet.input_buffer[sheet.input_pos++] = c;
                    sheet.input_buffer[sheet.input_pos] = '\0';
                }
            }
        } else {
            if (c == '\x1b') {
                if (read(STDIN_FILENO, &seq[0], 1) == 0) continue;
                if (read(STDIN_FILENO, &seq[1], 1) == 0) continue;
                
                if (seq[0] == '[') {
                    switch (seq[1]) {
                        case 'A': // Up
                            if (sheet.cursor_row > 0) sheet.cursor_row--;
                            if (sheet.cursor_row < sheet.view_row) sheet.view_row = sheet.cursor_row;
                            break;
                        case 'B': // Down
                            if (sheet.cursor_row < MAX_ROWS - 1) sheet.cursor_row++;
                            if (sheet.cursor_row >= sheet.view_row + 20) sheet.view_row++;
                            break;
                        case 'C': // Right
                            if (sheet.cursor_col < MAX_COLS - 1) sheet.cursor_col++;
                            if (sheet.cursor_col >= sheet.view_col + 6) sheet.view_col++;
                            break;
                        case 'D': // Left
                            if (sheet.cursor_col > 0) sheet.cursor_col--;
                            if (sheet.cursor_col < sheet.view_col) sheet.view_col = sheet.cursor_col;
                            break;
                        case 'H': // Home
                            sheet.cursor_col = 0;
                            sheet.view_col = 0;
                            break;
                        case 'F': // End
                            sheet.cursor_col = MAX_COLS - 1;
                            break;
                        case '5': // PgUp
                            read(STDIN_FILENO, &c, 1); // consume ~
                            if (sheet.cursor_row > 10) sheet.cursor_row -= 10;
                            else sheet.cursor_row = 0;
                            sheet.view_row = sheet.cursor_row;
                            break;
                        case '6': // PgDn
                            read(STDIN_FILENO, &c, 1); // consume ~
                            if (sheet.cursor_row < MAX_ROWS - 10) sheet.cursor_row += 10;
                            else sheet.cursor_row = MAX_ROWS - 1;
                            break;
                        case '3': // Delete
                            read(STDIN_FILENO, &c, 1); // consume ~
                            sheet.cells[sheet.cursor_row][sheet.cursor_col].formula[0] = '\0';
                            sheet.needs_recalc = 1;
                            break;
                    }
                } else if (seq[0] == 'O') {
                    switch (seq[1]) {
                        case 'P': show_help(&sheet); break; // F1
                        case 'Q': // F2 - Save
                            printf("\nEnter filename: ");
                            fflush(stdout);
                            disable_raw_mode();
                            char filename[256];
                            if (fgets(filename, sizeof(filename), stdin)) {
                                filename[strcspn(filename, "\n")] = 0;
                                save_sheet(&sheet, filename);
                            }
                            enable_raw_mode();
                            break;
                        case 'R': // F3 - Load
                            printf("\nEnter filename: ");
                            fflush(stdout);
                            disable_raw_mode();
                            if (fgets(filename, sizeof(filename), stdin)) {
                                filename[strcspn(filename, "\n")] = 0;
                                load_sheet(&sheet, filename);
                            }
                            enable_raw_mode();
                            break;
                    }
                }
            } else if (c == '\r' || c == '\n') {
                enter_edit_mode(&sheet);
            } else if (c == 3) { // Ctrl+C - Copy
                Cell *cell = &sheet.cells[sheet.cursor_row][sheet.cursor_col];
                strncpy(sheet.clipboard, cell->formula, MAX_FORMULA_LEN - 1);
                sheet.clipboard_has_data = 1;
                set_status(&sheet, "Cell copied");
            } else if (c == 22) { // Ctrl+V - Paste
                if (sheet.clipboard_has_data) {
                    Cell *cell = &sheet.cells[sheet.cursor_row][sheet.cursor_col];
                    strncpy(cell->formula, sheet.clipboard, MAX_FORMULA_LEN - 1);
                    sheet.needs_recalc = 1;
                    set_status(&sheet, "Cell pasted");
                }
            } else if (c == 127 || c == '\b') { // Backspace/Delete
                sheet.cells[sheet.cursor_row][sheet.cursor_col].formula[0] = '\0';
                sheet.needs_recalc = 1;
            } else if (c >= 32 && c < 127) {
                // Start editing with character
                sheet.input_buffer[0] = c;
                sheet.input_buffer[1] = '\0';
                sheet.input_pos = 1;
                sheet.edit_mode = 1;
            } else if (c == 21) { // F10 (Ctrl+U sometimes, or handle differently)
                running = 0;
            }
        }
        
        // Special quit sequence
        if (c == 17) running = 0; // Ctrl+Q
    }
    
    clear_screen();
    set_cursor(1, 1);
    printf("MicroSheet closed. Goodbye!\n");
    
    return 0;
}
