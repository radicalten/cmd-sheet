#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <termios.h>
#include <unistd.h>

#define MAX_ROWS 20
#define MAX_COLS 10
#define CELL_WIDTH 12
#define MAX_CELL_LEN 256

typedef struct {
    char formula[MAX_CELL_LEN];  // What user typed (e.g., "=A1+B2")
    char display[MAX_CELL_LEN];  // What to display (calculated value or text)
    double value;                 // Numeric value (if applicable)
    int is_numeric;              // Flag for numeric cells
} Cell;

typedef struct {
    Cell cells[MAX_ROWS][MAX_COLS];
    int cursor_row;
    int cursor_col;
    struct termios orig_termios;
} Spreadsheet;

// Terminal handling
void disable_raw_mode(Spreadsheet *sheet) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &sheet->orig_termios);
}

void enable_raw_mode(Spreadsheet *sheet) {
    tcgetattr(STDIN_FILENO, &sheet->orig_termios);
    struct termios raw = sheet->orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

// Clear screen and move cursor to top
void clear_screen() {
    printf("\033[2J\033[H");
}

// Parse cell reference like "A1" to row, col
int parse_cell_ref(const char *ref, int *row, int *col) {
    if (!ref || !isalpha(ref[0])) return 0;
    *col = toupper(ref[0]) - 'A';
    *row = atoi(ref + 1) - 1;
    return (*row >= 0 && *row < MAX_ROWS && *col >= 0 && *col < MAX_COLS);
}

// Convert column number to letter (0=A, 1=B, etc.)
char col_to_letter(int col) {
    return 'A' + col;
}

// Evaluate a simple expression (number, cell ref, or basic arithmetic)
double evaluate_expr(Spreadsheet *sheet, const char *expr);

// Get numeric value from a cell (handle references)
double get_cell_value(Spreadsheet *sheet, int row, int col) {
    if (row < 0 || row >= MAX_ROWS || col < 0 || col >= MAX_COLS)
        return 0.0;
    return sheet->cells[row][col].value;
}

// Parse and evaluate SUM(A1:A5) or similar range function
double eval_sum(Spreadsheet *sheet, const char *expr) {
    char range[MAX_CELL_LEN];
    int start_row, start_col, end_row, end_col;
    
    // Extract range from SUM(...)
    const char *p = strchr(expr, '(');
    if (!p) return 0.0;
    p++;
    
    const char *colon = strchr(p, ':');
    if (!colon) return 0.0;
    
    char start_ref[10], end_ref[10];
    int len = colon - p;
    strncpy(start_ref, p, len);
    start_ref[len] = '\0';
    
    const char *end_paren = strchr(colon, ')');
    if (!end_paren) return 0.0;
    len = end_paren - colon - 1;
    strncpy(end_ref, colon + 1, len);
    end_ref[len] = '\0';
    
    if (!parse_cell_ref(start_ref, &start_row, &start_col) ||
        !parse_cell_ref(end_ref, &end_row, &end_col))
        return 0.0;
    
    double sum = 0.0;
    for (int r = start_row; r <= end_row && r < MAX_ROWS; r++) {
        for (int c = start_col; c <= end_col && c < MAX_COLS; c++) {
            sum += get_cell_value(sheet, r, c);
        }
    }
    return sum;
}

// Evaluate AVG function
double eval_avg(Spreadsheet *sheet, const char *expr) {
    char range[MAX_CELL_LEN];
    int start_row, start_col, end_row, end_col;
    
    const char *p = strchr(expr, '(');
    if (!p) return 0.0;
    p++;
    
    const char *colon = strchr(p, ':');
    if (!colon) return 0.0;
    
    char start_ref[10], end_ref[10];
    int len = colon - p;
    strncpy(start_ref, p, len);
    start_ref[len] = '\0';
    
    const char *end_paren = strchr(colon, ')');
    if (!end_paren) return 0.0;
    len = end_paren - colon - 1;
    strncpy(end_ref, colon + 1, len);
    end_ref[len] = '\0';
    
    if (!parse_cell_ref(start_ref, &start_row, &start_col) ||
        !parse_cell_ref(end_ref, &end_row, &end_col))
        return 0.0;
    
    double sum = 0.0;
    int count = 0;
    for (int r = start_row; r <= end_row && r < MAX_ROWS; r++) {
        for (int c = start_col; c <= end_col && c < MAX_COLS; c++) {
            sum += get_cell_value(sheet, r, c);
            count++;
        }
    }
    return count > 0 ? sum / count : 0.0;
}

// Simple expression evaluator (supports +, -, *, /, cell refs)
double evaluate_expr(Spreadsheet *sheet, const char *expr) {
    char buf[MAX_CELL_LEN];
    strncpy(buf, expr, MAX_CELL_LEN - 1);
    buf[MAX_CELL_LEN - 1] = '\0';
    
    // Trim whitespace
    char *p = buf;
    while (isspace(*p)) p++;
    
    // Check for functions
    if (strncasecmp(p, "SUM(", 4) == 0) {
        return eval_sum(sheet, p);
    }
    if (strncasecmp(p, "AVG(", 4) == 0) {
        return eval_avg(sheet, p);
    }
    
    // Check if it's a cell reference
    if (isalpha(p[0]) && isdigit(p[1])) {
        int row, col;
        if (parse_cell_ref(p, &row, &col)) {
            return get_cell_value(sheet, row, col);
        }
    }
    
    // Try to parse as number
    char *endptr;
    double val = strtod(p, &endptr);
    if (endptr != p && *endptr == '\0') {
        return val;
    }
    
    // Simple arithmetic parser (very basic: handles A1+B2 style)
    char operators[] = "+-*/";
    for (int i = 0; operators[i]; i++) {
        char *op = strchr(p, operators[i]);
        if (op && op != p) {  // Not at start
            char left[MAX_CELL_LEN], right[MAX_CELL_LEN];
            int len = op - p;
            strncpy(left, p, len);
            left[len] = '\0';
            strcpy(right, op + 1);
            
            double left_val = evaluate_expr(sheet, left);
            double right_val = evaluate_expr(sheet, right);
            
            switch (operators[i]) {
                case '+': return left_val + right_val;
                case '-': return left_val - right_val;
                case '*': return left_val * right_val;
                case '/': return right_val != 0 ? left_val / right_val : 0.0;
            }
        }
    }
    
    return 0.0;
}

// Recalculate a cell
void recalculate_cell(Spreadsheet *sheet, int row, int col) {
    Cell *cell = &sheet->cells[row][col];
    
    if (cell->formula[0] == '\0') {
        // Empty cell
        cell->display[0] = '\0';
        cell->value = 0.0;
        cell->is_numeric = 0;
        return;
    }
    
    if (cell->formula[0] == '=') {
        // Formula
        cell->value = evaluate_expr(sheet, cell->formula + 1);
        snprintf(cell->display, MAX_CELL_LEN, "%.2f", cell->value);
        cell->is_numeric = 1;
    } else {
        // Try to parse as number
        char *endptr;
        double val = strtod(cell->formula, &endptr);
        if (endptr != cell->formula && (*endptr == '\0' || isspace(*endptr))) {
            cell->value = val;
            snprintf(cell->display, MAX_CELL_LEN, "%.2f", val);
            cell->is_numeric = 1;
        } else {
            // Text
            strncpy(cell->display, cell->formula, MAX_CELL_LEN - 1);
            cell->display[MAX_CELL_LEN - 1] = '\0';
            cell->value = 0.0;
            cell->is_numeric = 0;
        }
    }
}

// Recalculate all cells (simple approach - doesn't handle dependencies)
void recalculate_all(Spreadsheet *sheet) {
    // Multiple passes to handle dependencies (simple approach)
    for (int pass = 0; pass < 3; pass++) {
        for (int r = 0; r < MAX_ROWS; r++) {
            for (int c = 0; c < MAX_COLS; c++) {
                recalculate_cell(sheet, r, c);
            }
        }
    }
}

// Display the spreadsheet
void display_spreadsheet(Spreadsheet *sheet) {
    clear_screen();
    
    printf("Terminal Spreadsheet (Arrow keys: move, Enter: edit, Ctrl+Q: quit, Ctrl+S: save)\n");
    printf("Current cell: %c%d  Formula: %s\n\n", 
           col_to_letter(sheet->cursor_col), 
           sheet->cursor_row + 1,
           sheet->cells[sheet->cursor_row][sheet->cursor_col].formula);
    
    // Column headers
    printf("    ");
    for (int c = 0; c < MAX_COLS; c++) {
        printf("%-*c ", CELL_WIDTH, col_to_letter(c));
    }
    printf("\n");
    
    // Separator
    printf("    ");
    for (int c = 0; c < MAX_COLS; c++) {
        for (int i = 0; i < CELL_WIDTH + 1; i++) printf("-");
    }
    printf("\n");
    
    // Rows
    for (int r = 0; r < MAX_ROWS; r++) {
        printf("%-3d|", r + 1);
        for (int c = 0; c < MAX_COLS; c++) {
            if (r == sheet->cursor_row && c == sheet->cursor_col) {
                printf("\033[7m"); // Reverse video
            }
            printf("%-*.*s\033[0m ", CELL_WIDTH, CELL_WIDTH, 
                   sheet->cells[r][c].display);
        }
        printf("\n");
    }
    
    printf("\n[Commands: Arrows=Move, Enter=Edit, Delete=Clear, Ctrl+Q=Quit, Ctrl+S=Save, Ctrl+L=Load]\n");
}

// Edit current cell
void edit_cell(Spreadsheet *sheet) {
    Cell *cell = &sheet->cells[sheet->cursor_row][sheet->cursor_col];
    
    printf("\n\nEdit %c%d (current: %s)\n", 
           col_to_letter(sheet->cursor_col), 
           sheet->cursor_row + 1,
           cell->formula);
    printf("Enter value: ");
    
    // Temporarily restore normal terminal mode
    disable_raw_mode(sheet);
    
    char input[MAX_CELL_LEN];
    if (fgets(input, MAX_CELL_LEN, stdin)) {
        // Remove newline
        input[strcspn(input, "\n")] = '\0';
        strncpy(cell->formula, input, MAX_CELL_LEN - 1);
        cell->formula[MAX_CELL_LEN - 1] = '\0';
        recalculate_all(sheet);
    }
    
    enable_raw_mode(sheet);
}

// Clear current cell
void clear_cell(Spreadsheet *sheet) {
    Cell *cell = &sheet->cells[sheet->cursor_row][sheet->cursor_col];
    cell->formula[0] = '\0';
    recalculate_all(sheet);
}

// Save spreadsheet to file
void save_spreadsheet(Spreadsheet *sheet, const char *filename) {
    FILE *f = fopen(filename, "w");
    if (!f) {
        printf("\nError: Could not save file!\n");
        sleep(1);
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
    printf("\nSaved to %s\n", filename);
    sleep(1);
}

// Load spreadsheet from file
void load_spreadsheet(Spreadsheet *sheet, const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        printf("\nError: Could not load file!\n");
        sleep(1);
        return;
    }
    
    // Clear existing data
    for (int r = 0; r < MAX_ROWS; r++) {
        for (int c = 0; c < MAX_COLS; c++) {
            sheet->cells[r][c].formula[0] = '\0';
        }
    }
    
    char line[MAX_CELL_LEN + 20];
    while (fgets(line, sizeof(line), f)) {
        int r, c;
        char formula[MAX_CELL_LEN];
        if (sscanf(line, "%d,%d,%[^\n]", &r, &c, formula) == 3) {
            if (r >= 0 && r < MAX_ROWS && c >= 0 && c < MAX_COLS) {
                strncpy(sheet->cells[r][c].formula, formula, MAX_CELL_LEN - 1);
                sheet->cells[r][c].formula[MAX_CELL_LEN - 1] = '\0';
            }
        }
    }
    
    fclose(f);
    recalculate_all(sheet);
    printf("\nLoaded from %s\n", filename);
    sleep(1);
}

// Initialize spreadsheet
void init_spreadsheet(Spreadsheet *sheet) {
    memset(sheet, 0, sizeof(Spreadsheet));
    sheet->cursor_row = 0;
    sheet->cursor_col = 0;
}

// Main program loop
int main() {
    Spreadsheet sheet;
    init_spreadsheet(&sheet);
    
    enable_raw_mode(&sheet);
    
    int running = 1;
    while (running) {
        display_spreadsheet(&sheet);
        
        // Read key
        char c;
        read(STDIN_FILENO, &c, 1);
        
        // Check for escape sequence (arrow keys)
        if (c == 27) {
            char seq[2];
            if (read(STDIN_FILENO, &seq[0], 1) == 1 &&
                read(STDIN_FILENO, &seq[1], 1) == 1) {
                if (seq[0] == '[') {
                    switch (seq[1]) {
                        case 'A': // Up
                            if (sheet.cursor_row > 0) sheet.cursor_row--;
                            break;
                        case 'B': // Down
                            if (sheet.cursor_row < MAX_ROWS - 1) sheet.cursor_row++;
                            break;
                        case 'C': // Right
                            if (sheet.cursor_col < MAX_COLS - 1) sheet.cursor_col++;
                            break;
                        case 'D': // Left
                            if (sheet.cursor_col > 0) sheet.cursor_col--;
                            break;
                        case '3': // Delete key
                            read(STDIN_FILENO, &c, 1); // consume '~'
                            clear_cell(&sheet);
                            break;
                    }
                }
            }
        } else if (c == 17) { // Ctrl+Q
            running = 0;
        } else if (c == 19) { // Ctrl+S
            disable_raw_mode(&sheet);
            printf("\n\nSave to file: ");
            char filename[256];
            if (fgets(filename, sizeof(filename), stdin)) {
                filename[strcspn(filename, "\n")] = '\0';
                if (filename[0] != '\0') {
                    save_spreadsheet(&sheet, filename);
                }
            }
            enable_raw_mode(&sheet);
        } else if (c == 12) { // Ctrl+L
            disable_raw_mode(&sheet);
            printf("\n\nLoad from file: ");
            char filename[256];
            if (fgets(filename, sizeof(filename), stdin)) {
                filename[strcspn(filename, "\n")] = '\0';
                if (filename[0] != '\0') {
                    load_spreadsheet(&sheet, filename);
                }
            }
            enable_raw_mode(&sheet);
        } else if (c == '\n' || c == '\r') { // Enter
            edit_cell(&sheet);
        } else if (c == 127 || c == '\b') { // Backspace/Delete
            clear_cell(&sheet);
        }
    }
    
    disable_raw_mode(&sheet);
    clear_screen();
    printf("Goodbye!\n");
    
    return 0;
}
