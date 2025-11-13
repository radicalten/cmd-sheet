#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#ifdef _WIN32
    #include <conio.h>
    #include <windows.h>
    #define CLEAR_SCREEN "cls"
#else
    #include <termios.h>
    #include <unistd.h>
    #define CLEAR_SCREEN "clear"
#endif

#define MAX_ROWS 100
#define MAX_COLS 26
#define MAX_CELL_LENGTH 256
#define MAX_DISPLAY_WIDTH 12

typedef struct {
    char formula[MAX_CELL_LENGTH];
    char display[MAX_CELL_LENGTH];
    double value;
    int is_numeric;
} Cell;

typedef struct {
    Cell cells[MAX_ROWS][MAX_COLS];
    int cursor_row;
    int cursor_col;
    int view_offset_row;
    int view_offset_col;
    char clipboard[MAX_CELL_LENGTH];
} Spreadsheet;

// Terminal handling
#ifndef _WIN32
static struct termios orig_termios;

void disable_raw_mode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void enable_raw_mode() {
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disable_raw_mode);
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

int getch() {
    int c = getchar();
    if (c == 27) {
        int next = getchar();
        if (next == '[') {
            int arrow = getchar();
            switch(arrow) {
                case 'A': return 'W'; // Up
                case 'B': return 'S'; // Down
                case 'C': return 'D'; // Right
                case 'D': return 'A'; // Left
            }
        }
        return 27;
    }
    return c;
}
#else
void enable_raw_mode() {
    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode;
    GetConsoleMode(hStdin, &mode);
    mode &= ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT);
    SetConsoleMode(hStdin, mode);
}

int getch() {
    int c = _getch();
    if (c == 224) {
        c = _getch();
        switch(c) {
            case 72: return 'W'; // Up
            case 80: return 'S'; // Down
            case 77: return 'D'; // Right
            case 75: return 'A'; // Left
        }
    }
    return c;
}
#endif

void init_spreadsheet(Spreadsheet *ss) {
    memset(ss, 0, sizeof(Spreadsheet));
    ss->cursor_row = 0;
    ss->cursor_col = 0;
    ss->view_offset_row = 0;
    ss->view_offset_col = 0;
}

void col_to_label(int col, char *label) {
    label[0] = 'A' + col;
    label[1] = '\0';
}

int label_to_col(char label) {
    return toupper(label) - 'A';
}

int parse_cell_ref(const char *ref, int *row, int *col) {
    if (!ref || !isalpha(ref[0])) return 0;
    *col = label_to_col(ref[0]);
    *row = atoi(ref + 1) - 1;
    return (*row >= 0 && *row < MAX_ROWS && *col >= 0 && *col < MAX_COLS);
}

double get_cell_value(Spreadsheet *ss, int row, int col) {
    if (row < 0 || row >= MAX_ROWS || col < 0 || col >= MAX_COLS) return 0;
    return ss->cells[row][col].value;
}

double eval_range_function(Spreadsheet *ss, const char *func, const char *range) {
    char start_ref[10], end_ref[10];
    int start_row, start_col, end_row, end_col;
    
    if (sscanf(range, "%[^:]:%s", start_ref, end_ref) != 2) return 0;
    if (!parse_cell_ref(start_ref, &start_row, &start_col)) return 0;
    if (!parse_cell_ref(end_ref, &end_row, &end_col)) return 0;
    
    double result = 0, count = 0;
    int initialized = 0;
    
    for (int r = start_row; r <= end_row; r++) {
        for (int c = start_col; c <= end_col; c++) {
            if (r >= 0 && r < MAX_ROWS && c >= 0 && c < MAX_COLS) {
                if (ss->cells[r][c].is_numeric) {
                    double val = ss->cells[r][c].value;
                    if (strcmp(func, "SUM") == 0) {
                        result += val;
                    } else if (strcmp(func, "AVERAGE") == 0 || strcmp(func, "AVG") == 0) {
                        result += val;
                        count++;
                    } else if (strcmp(func, "MIN") == 0) {
                        if (!initialized || val < result) result = val;
                        initialized = 1;
                    } else if (strcmp(func, "MAX") == 0) {
                        if (!initialized || val > result) result = val;
                        initialized = 1;
                    } else if (strcmp(func, "COUNT") == 0) {
                        count++;
                    }
                }
            }
        }
    }
    
    if (strcmp(func, "AVERAGE") == 0 || strcmp(func, "AVG") == 0) {
        return count > 0 ? result / count : 0;
    } else if (strcmp(func, "COUNT") == 0) {
        return count;
    }
    return result;
}

double eval_expression(Spreadsheet *ss, const char *expr, int depth);

double eval_function(Spreadsheet *ss, const char *formula, int depth) {
    char func[32], args[MAX_CELL_LENGTH];
    
    if (sscanf(formula, "%[A-Z](%[^)])", func, args) != 2) return 0;
    
    if (strchr(args, ':')) {
        return eval_range_function(ss, func, args);
    }
    
    // Single argument functions
    double arg_val = eval_expression(ss, args, depth + 1);
    
    if (strcmp(func, "ABS") == 0) return fabs(arg_val);
    if (strcmp(func, "SQRT") == 0) return sqrt(arg_val);
    if (strcmp(func, "ROUND") == 0) return round(arg_val);
    
    return 0;
}

double eval_expression(Spreadsheet *ss, const char *expr, int depth) {
    if (depth > 100) return 0; // Prevent infinite recursion
    
    char trimmed[MAX_CELL_LENGTH];
    int i = 0, j = 0;
    
    // Trim whitespace
    while (expr[i] && isspace(expr[i])) i++;
    while (expr[i]) trimmed[j++] = expr[i++];
    trimmed[j] = '\0';
    
    // Check if it's a number
    char *endptr;
    double val = strtod(trimmed, &endptr);
    if (*endptr == '\0') return val;
    
    // Check if it's a cell reference
    int ref_row, ref_col;
    if (parse_cell_ref(trimmed, &ref_row, &ref_col)) {
        return get_cell_value(ss, ref_row, ref_col);
    }
    
    // Check if it's a function
    if (strchr(trimmed, '(')) {
        return eval_function(ss, trimmed, depth);
    }
    
    // Simple expression parsing (handle +, -, *, /)
    char *ops = "+-*/";
    for (int op_idx = 0; op_idx < 2; op_idx++) { // Do +- first, then */
        for (i = strlen(trimmed) - 1; i >= 0; i--) {
            if ((op_idx == 0 && (trimmed[i] == '+' || trimmed[i] == '-')) ||
                (op_idx == 1 && (trimmed[i] == '*' || trimmed[i] == '/'))) {
                if (i == 0) continue; // Unary operator
                char left[MAX_CELL_LENGTH], right[MAX_CELL_LENGTH];
                strncpy(left, trimmed, i);
                left[i] = '\0';
                strcpy(right, trimmed + i + 1);
                
                double left_val = eval_expression(ss, left, depth + 1);
                double right_val = eval_expression(ss, right, depth + 1);
                
                switch(trimmed[i]) {
                    case '+': return left_val + right_val;
                    case '-': return left_val - right_val;
                    case '*': return left_val * right_val;
                    case '/': return right_val != 0 ? left_val / right_val : 0;
                }
            }
        }
    }
    
    return 0;
}

void update_cell(Spreadsheet *ss, int row, int col) {
    Cell *cell = &ss->cells[row][col];
    
    if (cell->formula[0] == '\0') {
        cell->display[0] = '\0';
        cell->value = 0;
        cell->is_numeric = 0;
        return;
    }
    
    if (cell->formula[0] == '=') {
        // Formula
        cell->value = eval_expression(ss, cell->formula + 1, 0);
        cell->is_numeric = 1;
        snprintf(cell->display, MAX_CELL_LENGTH, "%.2f", cell->value);
    } else {
        // Try to parse as number
        char *endptr;
        double val = strtod(cell->formula, &endptr);
        if (*endptr == '\0' && cell->formula[0] != '\0') {
            cell->value = val;
            cell->is_numeric = 1;
            snprintf(cell->display, MAX_CELL_LENGTH, "%.2f", val);
        } else {
            cell->is_numeric = 0;
            cell->value = 0;
            strncpy(cell->display, cell->formula, MAX_CELL_LENGTH - 1);
            cell->display[MAX_CELL_LENGTH - 1] = '\0';
        }
    }
}

void recalculate_all(Spreadsheet *ss) {
    for (int r = 0; r < MAX_ROWS; r++) {
        for (int c = 0; c < MAX_COLS; c++) {
            update_cell(ss, r, c);
        }
    }
}

void display_spreadsheet(Spreadsheet *ss) {
    system(CLEAR_SCREEN);
    
    int visible_rows = 20;
    int visible_cols = 8;
    
    printf("\n  === SPREADSHEET ===\n");
    printf("  Arrows: Navigate | E: Edit | C: Copy | V: Paste | X: Delete | S: Save | L: Load | Q: Quit\n\n");
    
    // Column headers
    printf("    ");
    for (int c = ss->view_offset_col; c < ss->view_offset_col + visible_cols && c < MAX_COLS; c++) {
        char label[3];
        col_to_label(c, label);
        printf("%-*s ", MAX_DISPLAY_WIDTH, label);
    }
    printf("\n");
    
    // Rows
    for (int r = ss->view_offset_row; r < ss->view_offset_row + visible_rows && r < MAX_ROWS; r++) {
        printf(" %2d ", r + 1);
        for (int c = ss->view_offset_col; c < ss->view_offset_col + visible_cols && c < MAX_COLS; c++) {
            char display[MAX_DISPLAY_WIDTH + 1];
            strncpy(display, ss->cells[r][c].display, MAX_DISPLAY_WIDTH);
            display[MAX_DISPLAY_WIDTH] = '\0';
            
            if (r == ss->cursor_row && c == ss->cursor_col) {
                printf("[%-*s] ", MAX_DISPLAY_WIDTH - 2, display);
            } else {
                printf("%-*s ", MAX_DISPLAY_WIDTH, display);
            }
        }
        printf("\n");
    }
    
    // Status line
    char col_label[3];
    col_to_label(ss->cursor_col, col_label);
    printf("\n  Cell: %s%d | Formula: %s\n", col_label, ss->cursor_row + 1, 
           ss->cells[ss->cursor_row][ss->cursor_col].formula);
}

void edit_cell(Spreadsheet *ss) {
    char input[MAX_CELL_LENGTH];
    
    printf("\n  Enter value (current: %s): ", ss->cells[ss->cursor_row][ss->cursor_col].formula);
    
    #ifndef _WIN32
    disable_raw_mode();
    #endif
    
    if (fgets(input, MAX_CELL_LENGTH, stdin)) {
        input[strcspn(input, "\n")] = 0;
        strncpy(ss->cells[ss->cursor_row][ss->cursor_col].formula, input, MAX_CELL_LENGTH - 1);
        recalculate_all(ss);
    }
    
    #ifndef _WIN32
    enable_raw_mode();
    #endif
}

void save_spreadsheet(Spreadsheet *ss, const char *filename) {
    FILE *f = fopen(filename, "w");
    if (!f) {
        printf("\n  Error: Could not save file!\n");
        return;
    }
    
    for (int r = 0; r < MAX_ROWS; r++) {
        int has_data = 0;
        for (int c = 0; c < MAX_COLS; c++) {
            if (ss->cells[r][c].formula[0] != '\0') has_data = 1;
        }
        if (!has_data) continue;
        
        for (int c = 0; c < MAX_COLS; c++) {
            fprintf(f, "\"%s\"", ss->cells[r][c].formula);
            if (c < MAX_COLS - 1) fprintf(f, ",");
        }
        fprintf(f, "\n");
    }
    
    fclose(f);
    printf("\n  Saved to %s. Press any key...", filename);
    getch();
}

void load_spreadsheet(Spreadsheet *ss, const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        printf("\n  Error: Could not open file!\n");
        return;
    }
    
    init_spreadsheet(ss);
    char line[MAX_COLS * MAX_CELL_LENGTH];
    int row = 0;
    
    while (fgets(line, sizeof(line), f) && row < MAX_ROWS) {
        int col = 0;
        char *ptr = line;
        
        while (*ptr && col < MAX_COLS) {
            while (*ptr && isspace(*ptr)) ptr++;
            if (*ptr == '"') {
                ptr++;
                char *start = ptr;
                while (*ptr && *ptr != '"') ptr++;
                int len = ptr - start;
                if (len > 0) {
                    strncpy(ss->cells[row][col].formula, start, len < MAX_CELL_LENGTH ? len : MAX_CELL_LENGTH - 1);
                    ss->cells[row][col].formula[len < MAX_CELL_LENGTH ? len : MAX_CELL_LENGTH - 1] = '\0';
                }
                if (*ptr == '"') ptr++;
            }
            while (*ptr && *ptr != ',' && *ptr != '\n') ptr++;
            if (*ptr == ',') ptr++;
            col++;
        }
        row++;
    }
    
    fclose(f);
    recalculate_all(ss);
    printf("\n  Loaded from %s. Press any key...", filename);
    getch();
}

int main() {
    Spreadsheet ss;
    init_spreadsheet(&ss);
    
    enable_raw_mode();
    
    int running = 1;
    while (running) {
        display_spreadsheet(&ss);
        
        int ch = getch();
        ch = toupper(ch);
        
        switch(ch) {
            case 'W': // Up
                if (ss.cursor_row > 0) ss.cursor_row--;
                if (ss.cursor_row < ss.view_offset_row) ss.view_offset_row = ss.cursor_row;
                break;
            case 'S': // Down
                if (ss.cursor_row < MAX_ROWS - 1) ss.cursor_row++;
                if (ss.cursor_row >= ss.view_offset_row + 20) ss.view_offset_row = ss.cursor_row - 19;
                break;
            case 'A': // Left
                if (ss.cursor_col > 0) ss.cursor_col--;
                if (ss.cursor_col < ss.view_offset_col) ss.view_offset_col = ss.cursor_col;
                break;
            case 'D': // Right
                if (ss.cursor_col < MAX_COLS - 1) ss.cursor_col++;
                if (ss.cursor_col >= ss.view_offset_col + 8) ss.view_offset_col = ss.cursor_col - 7;
                break;
            case 'E': // Edit
                edit_cell(&ss);
                break;
            case 'C': // Copy
                strncpy(ss.clipboard, ss.cells[ss.cursor_row][ss.cursor_col].formula, MAX_CELL_LENGTH);
                break;
            case 'V': // Paste
                strncpy(ss.cells[ss.cursor_row][ss.cursor_col].formula, ss.clipboard, MAX_CELL_LENGTH);
                recalculate_all(&ss);
                break;
            case 'X': // Delete
                ss.cells[ss.cursor_row][ss.cursor_col].formula[0] = '\0';
                recalculate_all(&ss);
                break;
            case 'P': // Save
                {
                    char filename[256];
                    printf("\n  Enter filename: ");
                    #ifndef _WIN32
                    disable_raw_mode();
                    #endif
                    if (fgets(filename, sizeof(filename), stdin)) {
                        filename[strcspn(filename, "\n")] = 0;
                        save_spreadsheet(&ss, filename);
                    }
                    #ifndef _WIN32
                    enable_raw_mode();
                    #endif
                }
                break;
            case 'L': // Load
                {
                    char filename[256];
                    printf("\n  Enter filename: ");
                    #ifndef _WIN32
                    disable_raw_mode();
                    #endif
                    if (fgets(filename, sizeof(filename), stdin)) {
                        filename[strcspn(filename, "\n")] = 0;
                        load_spreadsheet(&ss, filename);
                    }
                    #ifndef _WIN32
                    enable_raw_mode();
                    #endif
                }
                break;
            case 'Q': // Quit
                running = 0;
                break;
        }
    }
    
    #ifndef _WIN32
    disable_raw_mode();
    #endif
    
    system(CLEAR_SCREEN);
    printf("Goodbye!\n");
    
    return 0;
}
