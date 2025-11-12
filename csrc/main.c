#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <termios.h>
#include <unistd.h>
#include <math.h>

#define ROWS 99
#define COLS 26
#define MAX_FORMULA 256
#define MAX_DISPLAY 10

// Cell structure
typedef struct {
    char formula[MAX_FORMULA];
    double value;
    int is_numeric;
    int error;
} Cell;

// Spreadsheet state
typedef struct {
    Cell cells[ROWS][COLS];
    int cursor_row;
    int cursor_col;
    int offset_row;
    int offset_col;
    char input_buffer[MAX_FORMULA];
    int input_mode;
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
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

// ANSI escape codes
void clear_screen() { printf("\033[2J\033[H"); }
void move_cursor(int row, int col) { printf("\033[%d;%dH", row, col); }
void hide_cursor() { printf("\033[?25l"); }
void show_cursor() { printf("\033[?25h"); }

// Forward declarations
double evaluate_formula(Sheet *sheet, const char *formula, int row, int col);
void calculate_all(Sheet *sheet);

// Initialize spreadsheet
void init_sheet(Sheet *sheet) {
    memset(sheet, 0, sizeof(Sheet));
    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            sheet->cells[r][c].value = 0.0;
            sheet->cells[r][c].is_numeric = 0;
            sheet->cells[r][c].error = 0;
            sheet->cells[r][c].formula[0] = '\0';
        }
    }
}

// Convert column letter to index
int col_to_index(char c) {
    return toupper(c) - 'A';
}

// Parse cell reference like "A1" or "Z99"
int parse_cell_ref(const char *ref, int *row, int *col) {
    if (!isalpha(ref[0])) return 0;
    *col = col_to_index(ref[0]);
    if (*col < 0 || *col >= COLS) return 0;
    
    int r = atoi(ref + 1);
    if (r < 1 || r > ROWS) return 0;
    *row = r - 1;
    return 1;
}

// Get cell value
double get_cell_value(Sheet *sheet, int row, int col) {
    if (row < 0 || row >= ROWS || col < 0 || col >= COLS) return 0.0;
    if (sheet->cells[row][col].error) return 0.0;
    return sheet->cells[row][col].value;
}

// Evaluate SUM function
double eval_sum(Sheet *sheet, const char *args, int current_row, int current_col) {
    int r1, c1, r2, c2;
    char range[MAX_FORMULA];
    strncpy(range, args, MAX_FORMULA - 1);
    
    char *colon = strchr(range, ':');
    if (!colon) return 0.0;
    
    *colon = '\0';
    char *start = range;
    char *end = colon + 1;
    
    // Trim spaces
    while (*start == ' ') start++;
    while (*end == ' ') end++;
    
    if (!parse_cell_ref(start, &r1, &c1)) return 0.0;
    if (!parse_cell_ref(end, &r2, &c2)) return 0.0;
    
    double sum = 0.0;
    for (int r = r1; r <= r2; r++) {
        for (int c = c1; c <= c2; c++) {
            sum += get_cell_value(sheet, r, c);
        }
    }
    return sum;
}

// Evaluate AVG function
double eval_avg(Sheet *sheet, const char *args, int current_row, int current_col) {
    int r1, c1, r2, c2;
    char range[MAX_FORMULA];
    strncpy(range, args, MAX_FORMULA - 1);
    
    char *colon = strchr(range, ':');
    if (!colon) return 0.0;
    
    *colon = '\0';
    char *start = range;
    char *end = colon + 1;
    
    while (*start == ' ') start++;
    while (*end == ' ') end++;
    
    if (!parse_cell_ref(start, &r1, &c1)) return 0.0;
    if (!parse_cell_ref(end, &r2, &c2)) return 0.0;
    
    double sum = 0.0;
    int count = 0;
    for (int r = r1; r <= r2; r++) {
        for (int c = c1; c <= c2; c++) {
            sum += get_cell_value(sheet, r, c);
            count++;
        }
    }
    return count > 0 ? sum / count : 0.0;
}

// Simple expression evaluator
double eval_expression(Sheet *sheet, char *expr, int row, int col) {
    // Remove spaces
    char clean[MAX_FORMULA];
    int j = 0;
    for (int i = 0; expr[i]; i++) {
        if (expr[i] != ' ') clean[j++] = expr[i];
    }
    clean[j] = '\0';
    
    // Handle functions
    if (strncmp(clean, "SUM(", 4) == 0) {
        char *end = strchr(clean, ')');
        if (end) {
            *end = '\0';
            return eval_sum(sheet, clean + 4, row, col);
        }
    }
    
    if (strncmp(clean, "AVG(", 4) == 0) {
        char *end = strchr(clean, ')');
        if (end) {
            *end = '\0';
            return eval_avg(sheet, clean + 4, row, col);
        }
    }
    
    // Simple arithmetic parser (handles +, -, *, /)
    // Parse terms separated by + or -
    double result = 0.0;
    char *ptr = clean;
    int add = 1;
    
    while (*ptr) {
        double term = 1.0;
        int multiply = 1;
        
        // Parse factors separated by * or /
        while (*ptr && *ptr != '+' && *ptr != '-') {
            double factor = 0.0;
            
            // Check for cell reference
            if (isalpha(*ptr)) {
                char ref[10];
                int k = 0;
                while (isalnum(*ptr) && k < 9) {
                    ref[k++] = *ptr++;
                }
                ref[k] = '\0';
                
                int r, c;
                if (parse_cell_ref(ref, &r, &c)) {
                    factor = get_cell_value(sheet, r, c);
                }
            }
            // Check for number
            else if (isdigit(*ptr) || *ptr == '.') {
                factor = strtod(ptr, &ptr);
            }
            else {
                ptr++;
                continue;
            }
            
            if (multiply) {
                term *= factor;
            } else {
                term /= factor;
            }
            
            if (*ptr == '*') {
                multiply = 1;
                ptr++;
            } else if (*ptr == '/') {
                multiply = 0;
                ptr++;
            }
        }
        
        if (add) {
            result += term;
        } else {
            result -= term;
        }
        
        if (*ptr == '+') {
            add = 1;
            ptr++;
        } else if (*ptr == '-') {
            add = 0;
            ptr++;
        }
    }
    
    return result;
}

// Evaluate formula
double evaluate_formula(Sheet *sheet, const char *formula, int row, int col) {
    if (!formula || formula[0] == '\0') return 0.0;
    
    // If it starts with =, it's a formula
    if (formula[0] == '=') {
        char expr[MAX_FORMULA];
        strncpy(expr, formula + 1, MAX_FORMULA - 1);
        return eval_expression(sheet, expr, row, col);
    }
    
    // Otherwise try to parse as number
    return strtod(formula, NULL);
}

// Set cell formula and calculate
void set_cell(Sheet *sheet, int row, int col, const char *formula) {
    if (row < 0 || row >= ROWS || col < 0 || col >= COLS) return;
    
    strncpy(sheet->cells[row][col].formula, formula, MAX_FORMULA - 1);
    sheet->cells[row][col].formula[MAX_FORMULA - 1] = '\0';
    sheet->cells[row][col].error = 0;
    
    if (formula[0] == '\0') {
        sheet->cells[row][col].is_numeric = 0;
        sheet->cells[row][col].value = 0.0;
    } else if (formula[0] == '=') {
        sheet->cells[row][col].is_numeric = 1;
        sheet->cells[row][col].value = evaluate_formula(sheet, formula, row, col);
    } else {
        char *endptr;
        double val = strtod(formula, &endptr);
        if (*endptr == '\0' && endptr != formula) {
            sheet->cells[row][col].is_numeric = 1;
            sheet->cells[row][col].value = val;
        } else {
            sheet->cells[row][col].is_numeric = 0;
        }
    }
}

// Recalculate all cells
void calculate_all(Sheet *sheet) {
    for (int pass = 0; pass < 3; pass++) {  // Multiple passes for dependencies
        for (int r = 0; r < ROWS; r++) {
            for (int c = 0; c < COLS; c++) {
                Cell *cell = &sheet->cells[r][c];
                if (cell->formula[0] == '=') {
                    cell->value = evaluate_formula(sheet, cell->formula, r, c);
                }
            }
        }
    }
}

// Display spreadsheet
void display_sheet(Sheet *sheet) {
    clear_screen();
    
    // Header
    move_cursor(1, 1);
    printf("   ");
    for (int c = 0; c < 8 && (c + sheet->offset_col) < COLS; c++) {
        printf("    %c     ", 'A' + c + sheet->offset_col);
    }
    
    // Grid
    for (int r = 0; r < 20 && (r + sheet->offset_row) < ROWS; r++) {
        move_cursor(r + 2, 1);
        printf("%2d ", r + 1 + sheet->offset_row);
        
        for (int c = 0; c < 8 && (c + sheet->offset_col) < COLS; c++) {
            int actual_r = r + sheet->offset_row;
            int actual_c = c + sheet->offset_col;
            Cell *cell = &sheet->cells[actual_r][actual_c];
            
            if (cell->formula[0] != '\0') {
                if (cell->is_numeric) {
                    printf("%9.2f ", cell->value);
                } else {
                    printf("%-9.9s ", cell->formula);
                }
            } else {
                printf("          ");
            }
        }
    }
    
    // Status line
    move_cursor(23, 1);
    printf("Cell: %c%d", 'A' + sheet->cursor_col, sheet->cursor_row + 1);
    
    move_cursor(24, 1);
    Cell *current = &sheet->cells[sheet->cursor_row][sheet->cursor_col];
    if (sheet->input_mode) {
        printf("Edit: %s", sheet->input_buffer);
    } else if (current->formula[0] != '\0') {
        printf("Formula: %s", current->formula);
    } else {
        printf("Arrow keys:move | Enter:edit | S:save | L:load | Q:quit");
    }
    
    // Position cursor
    int screen_r = sheet->cursor_row - sheet->offset_row + 2;
    int screen_c = (sheet->cursor_col - sheet->offset_col) * 10 + 4;
    move_cursor(screen_r, screen_c);
    
    fflush(stdout);
}

// Save to file
void save_sheet(Sheet *sheet, const char *filename) {
    FILE *f = fopen(filename, "w");
    if (!f) return;
    
    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            if (sheet->cells[r][c].formula[0] != '\0') {
                fprintf(f, "%c%d=%s\n", 'A' + c, r + 1, sheet->cells[r][c].formula);
            }
        }
    }
    fclose(f);
}

// Load from file
void load_sheet(Sheet *sheet, const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) return;
    
    init_sheet(sheet);
    
    char line[MAX_FORMULA + 10];
    while (fgets(line, sizeof(line), f)) {
        if (strlen(line) < 3) continue;
        
        char *eq = strchr(line, '=');
        if (!eq) continue;
        
        *eq = '\0';
        char *formula = eq + 1;
        
        // Remove newline
        char *nl = strchr(formula, '\n');
        if (nl) *nl = '\0';
        
        int row, col;
        if (parse_cell_ref(line, &row, &col)) {
            set_cell(sheet, row, col, formula);
        }
    }
    
    fclose(f);
    calculate_all(sheet);
}

// Main program
int main() {
    Sheet sheet;
    init_sheet(&sheet);
    
    enable_raw_mode();
    hide_cursor();
    
    int running = 1;
    
    while (running) {
        display_sheet(&sheet);
        
        char c = getchar();
        
        if (sheet.input_mode) {
            if (c == '\n' || c == '\r') {
                // Finish editing
                set_cell(&sheet, sheet.cursor_row, sheet.cursor_col, sheet.input_buffer);
                calculate_all(&sheet);
                sheet.input_mode = 0;
                sheet.input_buffer[0] = '\0';
            } else if (c == 27) {  // ESC
                sheet.input_mode = 0;
                sheet.input_buffer[0] = '\0';
            } else if (c == 127 || c == 8) {  // Backspace
                int len = strlen(sheet.input_buffer);
                if (len > 0) sheet.input_buffer[len - 1] = '\0';
            } else if (strlen(sheet.input_buffer) < MAX_FORMULA - 1 && c >= 32 && c < 127) {
                int len = strlen(sheet.input_buffer);
                sheet.input_buffer[len] = c;
                sheet.input_buffer[len + 1] = '\0';
            }
        } else {
            // Navigation mode
            if (c == 'q' || c == 'Q') {
                running = 0;
            } else if (c == 's' || c == 'S') {
                save_sheet(&sheet, "sheet.txt");
            } else if (c == 'l' || c == 'L') {
                load_sheet(&sheet, "sheet.txt");
            } else if (c == '\n' || c == '\r' || c == 'e' || c == 'E') {
                // Enter edit mode
                sheet.input_mode = 1;
                strncpy(sheet.input_buffer, sheet.cells[sheet.cursor_row][sheet.cursor_col].formula, MAX_FORMULA - 1);
            } else if (c == 27) {  // Arrow keys
                getchar();  // Skip [
                c = getchar();
                if (c == 'A' && sheet.cursor_row > 0) {  // Up
                    sheet.cursor_row--;
                    if (sheet.cursor_row < sheet.offset_row) sheet.offset_row = sheet.cursor_row;
                } else if (c == 'B' && sheet.cursor_row < ROWS - 1) {  // Down
                    sheet.cursor_row++;
                    if (sheet.cursor_row >= sheet.offset_row + 20) sheet.offset_row = sheet.cursor_row - 19;
                } else if (c == 'C' && sheet.cursor_col < COLS - 1) {  // Right
                    sheet.cursor_col++;
                    if (sheet.cursor_col >= sheet.offset_col + 8) sheet.offset_col = sheet.cursor_col - 7;
                } else if (c == 'D' && sheet.cursor_col > 0) {  // Left
                    sheet.cursor_col--;
                    if (sheet.cursor_col < sheet.offset_col) sheet.offset_col = sheet.cursor_col;
                }
            } else if (c == 'd' || c == 'D') {  // Delete cell
                set_cell(&sheet, sheet.cursor_row, sheet.cursor_col, "");
                calculate_all(&sheet);
            }
        }
    }
    
    show_cursor();
    clear_screen();
    move_cursor(1, 1);
    
    return 0;
}
