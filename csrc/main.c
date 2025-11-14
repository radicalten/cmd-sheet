#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <termios.h>
#include <unistd.h>

#define MAX_ROWS 100
#define MAX_COLS 26
#define MAX_CELL_WIDTH 10
#define MAX_FORMULA_LEN 256

typedef enum {
    CELL_EMPTY,
    CELL_NUMBER,
    CELL_TEXT,
    CELL_FORMULA
} CellType;

typedef struct {
    CellType type;
    double number;
    char text[MAX_FORMULA_LEN];
    char formula[MAX_FORMULA_LEN];
    int calculated;
    double cached_value;
} Cell;

typedef struct {
    Cell cells[MAX_ROWS][MAX_COLS];
    int cursor_row;
    int cursor_col;
    int view_top;
    int view_left;
    char filename[256];
} Spreadsheet;

// Terminal control
struct termios orig_termios;

void enable_raw_mode() {
    tcgetattr(STDIN_FILENO, &orig_termios);
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void disable_raw_mode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void clear_screen() {
    printf("\033[2J\033[H");
}

void move_cursor(int row, int col) {
    printf("\033[%d;%dH", row + 1, col + 1);
}

// Spreadsheet functions
void init_spreadsheet(Spreadsheet *sheet) {
    memset(sheet, 0, sizeof(Spreadsheet));
}

char col_to_letter(int col) {
    return 'A' + col;
}

int letter_to_col(char c) {
    return toupper(c) - 'A';
}

void parse_cell_ref(const char *ref, int *row, int *col) {
    *col = letter_to_col(ref[0]);
    *row = atoi(ref + 1) - 1;
}

double evaluate_expression(Spreadsheet *sheet, const char *expr);

double get_cell_value(Spreadsheet *sheet, int row, int col) {
    if (row < 0 || row >= MAX_ROWS || col < 0 || col >= MAX_COLS) {
        return 0;
    }
    
    Cell *cell = &sheet->cells[row][col];
    
    switch (cell->type) {
        case CELL_NUMBER:
            return cell->number;
        case CELL_FORMULA:
            if (!cell->calculated) {
                cell->cached_value = evaluate_expression(sheet, cell->formula);
                cell->calculated = 1;
            }
            return cell->cached_value;
        default:
            return 0;
    }
}

// Helper function to trim whitespace
char* trim_whitespace(char *str) {
    char *end;
    
    // Trim leading space
    while(isspace((unsigned char)*str)) str++;
    
    if(*str == 0) return str;
    
    // Trim trailing space
    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;
    
    end[1] = '\0';
    
    return str;
}

double evaluate_function(Spreadsheet *sheet, const char *func, const char *args) {
    if (strcasecmp(func, "SUM") == 0) {
        double sum = 0;
        char *args_copy = strdup(args);
        char *token = strtok(args_copy, ",");
        
        while (token) {
            token = trim_whitespace(token);
            
            // Check if it's a range (contains ':')
            char *colon = strchr(token, ':');
            if (colon) {
                // It's a range like A1:A5
                *colon = '\0';
                char *start_ref = trim_whitespace(token);
                char *end_ref = trim_whitespace(colon + 1);
                
                int r1, c1, r2, c2;
                parse_cell_ref(start_ref, &r1, &c1);
                parse_cell_ref(end_ref, &r2, &c2);
                
                // Ensure r1 <= r2 and c1 <= c2
                if (r1 > r2) { int tmp = r1; r1 = r2; r2 = tmp; }
                if (c1 > c2) { int tmp = c1; c1 = c2; c2 = tmp; }
                
                for (int r = r1; r <= r2; r++) {
                    for (int c = c1; c <= c2; c++) {
                        sum += get_cell_value(sheet, r, c);
                    }
                }
            } else {
                // It's a single cell reference
                int row, col;
                parse_cell_ref(token, &row, &col);
                sum += get_cell_value(sheet, row, col);
            }
            
            token = strtok(NULL, ",");
        }
        free(args_copy);
        return sum;
    } else if (strcasecmp(func, "AVG") == 0 || strcasecmp(func, "AVERAGE") == 0) {
        double sum = 0;
        int count = 0;
        char *args_copy = strdup(args);
        char *token = strtok(args_copy, ",");
        
        while (token) {
            token = trim_whitespace(token);
            
            char *colon = strchr(token, ':');
            if (colon) {
                *colon = '\0';
                char *start_ref = trim_whitespace(token);
                char *end_ref = trim_whitespace(colon + 1);
                
                int r1, c1, r2, c2;
                parse_cell_ref(start_ref, &r1, &c1);
                parse_cell_ref(end_ref, &r2, &c2);
                
                if (r1 > r2) { int tmp = r1; r1 = r2; r2 = tmp; }
                if (c1 > c2) { int tmp = c1; c1 = c2; c2 = tmp; }
                
                for (int r = r1; r <= r2; r++) {
                    for (int c = c1; c <= c2; c++) {
                        sum += get_cell_value(sheet, r, c);
                        count++;
                    }
                }
            } else {
                int row, col;
                parse_cell_ref(token, &row, &col);
                sum += get_cell_value(sheet, row, col);
                count++;
            }
            
            token = strtok(NULL, ",");
        }
        free(args_copy);
        return count > 0 ? sum / count : 0;
    } else if (strcasecmp(func, "MIN") == 0) {
        double min_val = INFINITY;
        char *args_copy = strdup(args);
        char *token = strtok(args_copy, ",");
        
        while (token) {
            token = trim_whitespace(token);
            
            char *colon = strchr(token, ':');
            if (colon) {
                *colon = '\0';
                char *start_ref = trim_whitespace(token);
                char *end_ref = trim_whitespace(colon + 1);
                
                int r1, c1, r2, c2;
                parse_cell_ref(start_ref, &r1, &c1);
                parse_cell_ref(end_ref, &r2, &c2);
                
                if (r1 > r2) { int tmp = r1; r1 = r2; r2 = tmp; }
                if (c1 > c2) { int tmp = c1; c1 = c2; c2 = tmp; }
                
                for (int r = r1; r <= r2; r++) {
                    for (int c = c1; c <= c2; c++) {
                        double val = get_cell_value(sheet, r, c);
                        if (val < min_val) min_val = val;
                    }
                }
            } else {
                int row, col;
                parse_cell_ref(token, &row, &col);
                double val = get_cell_value(sheet, row, col);
                if (val < min_val) min_val = val;
            }
            
            token = strtok(NULL, ",");
        }
        free(args_copy);
        return min_val == INFINITY ? 0 : min_val;
    } else if (strcasecmp(func, "MAX") == 0) {
        double max_val = -INFINITY;
        char *args_copy = strdup(args);
        char *token = strtok(args_copy, ",");
        
        while (token) {
            token = trim_whitespace(token);
            
            char *colon = strchr(token, ':');
            if (colon) {
                *colon = '\0';
                char *start_ref = trim_whitespace(token);
                char *end_ref = trim_whitespace(colon + 1);
                
                int r1, c1, r2, c2;
                parse_cell_ref(start_ref, &r1, &c1);
                parse_cell_ref(end_ref, &r2, &c2);
                
                if (r1 > r2) { int tmp = r1; r1 = r2; r2 = tmp; }
                if (c1 > c2) { int tmp = c1; c1 = c2; c2 = tmp; }
                
                for (int r = r1; r <= r2; r++) {
                    for (int c = c1; c <= c2; c++) {
                        double val = get_cell_value(sheet, r, c);
                        if (val > max_val) max_val = val;
                    }
                }
            } else {
                int row, col;
                parse_cell_ref(token, &row, &col);
                double val = get_cell_value(sheet, row, col);
                if (val > max_val) max_val = val;
            }
            
            token = strtok(NULL, ",");
        }
        free(args_copy);
        return max_val == -INFINITY ? 0 : max_val;
    } else if (strcasecmp(func, "COUNT") == 0) {
        int count = 0;
        char *args_copy = strdup(args);
        char *token = strtok(args_copy, ",");
        
        while (token) {
            token = trim_whitespace(token);
            
            char *colon = strchr(token, ':');
            if (colon) {
                *colon = '\0';
                char *start_ref = trim_whitespace(token);
                char *end_ref = trim_whitespace(colon + 1);
                
                int r1, c1, r2, c2;
                parse_cell_ref(start_ref, &r1, &c1);
                parse_cell_ref(end_ref, &r2, &c2);
                
                if (r1 > r2) { int tmp = r1; r1 = r2; r2 = tmp; }
                if (c1 > c2) { int tmp = c1; c1 = c2; c2 = tmp; }
                
                for (int r = r1; r <= r2; r++) {
                    for (int c = c1; c <= c2; c++) {
                        if (sheet->cells[r][c].type != CELL_EMPTY) count++;
                    }
                }
            } else {
                int row, col;
                parse_cell_ref(token, &row, &col);
                if (sheet->cells[row][col].type != CELL_EMPTY) count++;
            }
            
            token = strtok(NULL, ",");
        }
        free(args_copy);
        return count;
    }
    
    return 0;
}

// Forward declarations for recursive descent parser
double parse_expression(Spreadsheet *sheet, const char **expr_ptr);
double parse_term(Spreadsheet *sheet, const char **expr_ptr);
double parse_factor(Spreadsheet *sheet, const char **expr_ptr);

void skip_whitespace(const char **expr_ptr) {
    while (**expr_ptr == ' ' || **expr_ptr == '\t') {
        (*expr_ptr)++;
    }
}

// Parse a factor: number, cell reference, or parenthesized expression
double parse_factor(Spreadsheet *sheet, const char **expr_ptr) {
    skip_whitespace(expr_ptr);
    
    const char *e = *expr_ptr;
    
    // Handle parentheses
    if (*e == '(') {
        e++;
        *expr_ptr = e;
        double result = parse_expression(sheet, expr_ptr);
        skip_whitespace(expr_ptr);
        if (**expr_ptr == ')') {
            (*expr_ptr)++;
        }
        return result;
    }
    
    // Handle unary minus
    if (*e == '-') {
        e++;
        *expr_ptr = e;
        return -parse_factor(sheet, expr_ptr);
    }
    
    // Handle unary plus
    if (*e == '+') {
        e++;
        *expr_ptr = e;
        return parse_factor(sheet, expr_ptr);
    }
    
    // Handle cell references (letter followed by digit)
    if (isalpha(*e)) {
        char ref[10];
        int i = 0;
        while ((isalpha(*e) || isdigit(*e)) && i < 9) {
            ref[i++] = *e++;
        }
        ref[i] = '\0';
        *expr_ptr = e;
        
        int row, col;
        parse_cell_ref(ref, &row, &col);
        return get_cell_value(sheet, row, col);
    }
    
    // Handle numbers
    if (isdigit(*e) || *e == '.') {
        char *end;
        double value = strtod(e, (char **)&end);
        *expr_ptr = end;
        return value;
    }
    
    return 0;
}

// Parse a term: handles * and /
double parse_term(Spreadsheet *sheet, const char **expr_ptr) {
    double result = parse_factor(sheet, expr_ptr);
    
    skip_whitespace(expr_ptr);
    
    while (**expr_ptr == '*' || **expr_ptr == '/') {
        char op = **expr_ptr;
        (*expr_ptr)++;
        double right = parse_factor(sheet, expr_ptr);
        
        if (op == '*') {
            result *= right;
        } else {
            if (right != 0) {
                result /= right;
            } else {
                result = 0; // Division by zero returns 0
            }
        }
        
        skip_whitespace(expr_ptr);
    }
    
    return result;
}

// Parse an expression: handles + and -
double parse_expression(Spreadsheet *sheet, const char **expr_ptr) {
    double result = parse_term(sheet, expr_ptr);
    
    skip_whitespace(expr_ptr);
    
    while (**expr_ptr == '+' || **expr_ptr == '-') {
        char op = **expr_ptr;
        (*expr_ptr)++;
        double right = parse_term(sheet, expr_ptr);
        
        if (op == '+') {
            result += right;
        } else {
            result -= right;
        }
        
        skip_whitespace(expr_ptr);
    }
    
    return result;
}

double evaluate_expression(Spreadsheet *sheet, const char *expr) {
    char expr_copy[MAX_FORMULA_LEN];
    strcpy(expr_copy, expr);
    
    // Remove leading '='
    const char *e = expr_copy;
    if (*e == '=') e++;
    
    // Skip whitespace
    while (*e == ' ' || *e == '\t') e++;
    
    // Check for functions (SUM, AVG, etc.)
    if (isalpha(*e)) {
        // Look ahead to see if it's a function call
        const char *lookahead = e;
        while (isalpha(*lookahead)) lookahead++;
        while (*lookahead == ' ' || *lookahead == '\t') lookahead++;
        
        if (*lookahead == '(') {
            // It's a function
            char func_name[50];
            int i = 0;
            while (isalpha(*e) && i < 49) {
                func_name[i++] = *e++;
            }
            func_name[i] = '\0';
            
            while (*e == ' ' || *e == '\t') e++;
            
            if (*e == '(') {
                e++;
                char args[MAX_FORMULA_LEN];
                int paren_count = 1;
                i = 0;
                while (*e && paren_count > 0 && i < MAX_FORMULA_LEN - 1) {
                    if (*e == '(') paren_count++;
                    else if (*e == ')') paren_count--;
                    if (paren_count > 0) args[i++] = *e;
                    e++;
                }
                args[i] = '\0';
                return evaluate_function(sheet, func_name, args);
            }
        }
    }
    
    // Use recursive descent parser for arithmetic expressions
    return parse_expression(sheet, &e);
}

void invalidate_formulas(Spreadsheet *sheet) {
    for (int r = 0; r < MAX_ROWS; r++) {
        for (int c = 0; c < MAX_COLS; c++) {
            if (sheet->cells[r][c].type == CELL_FORMULA) {
                sheet->cells[r][c].calculated = 0;
            }
        }
    }
}

void recalculate_all(Spreadsheet *sheet) {
    // Recalculate all formulas (multi-pass for dependencies)
    int max_iterations = 10;
    for (int iter = 0; iter < max_iterations; iter++) {
        int changes = 0;
        for (int r = 0; r < MAX_ROWS; r++) {
            for (int c = 0; c < MAX_COLS; c++) {
                if (sheet->cells[r][c].type == CELL_FORMULA && !sheet->cells[r][c].calculated) {
                    double new_value = evaluate_expression(sheet, sheet->cells[r][c].formula);
                    if (sheet->cells[r][c].cached_value != new_value) {
                        changes++;
                    }
                    sheet->cells[r][c].cached_value = new_value;
                    sheet->cells[r][c].calculated = 1;
                }
            }
        }
        if (changes == 0) break;
    }
}

void set_cell_value(Spreadsheet *sheet, int row, int col, const char *value) {
    if (row < 0 || row >= MAX_ROWS || col < 0 || col >= MAX_COLS) return;
    
    Cell *cell = &sheet->cells[row][col];
    
    if (value[0] == '=') {
        cell->type = CELL_FORMULA;
        strcpy(cell->formula, value);
        cell->calculated = 0;
    } else if (isdigit(value[0]) || value[0] == '-' || value[0] == '.') {
        cell->type = CELL_NUMBER;
        cell->number = atof(value);
    } else if (strlen(value) > 0) {
        cell->type = CELL_TEXT;
        strcpy(cell->text, value);
    } else {
        cell->type = CELL_EMPTY;
    }
    
    // Invalidate and recalculate all formulas for Excel-like automatic updates
    invalidate_formulas(sheet);
    recalculate_all(sheet);
}

void display_sheet(Spreadsheet *sheet) {
    clear_screen();
    
    // Display title
    printf("MiniCalc - [%s] | Commands: Q)uit S)ave L)oad D)elete | Arrow keys to navigate\n", 
           sheet->filename[0] ? sheet->filename : "Untitled");
    printf("================================================================================\n");
    
    // Display column headers
    printf("     ");
    for (int c = 0; c < 10 && c < MAX_COLS; c++) {
        printf("%-*c", MAX_CELL_WIDTH, col_to_letter(c));
    }
    printf("\n");
    
    // Display rows
    for (int r = sheet->view_top; r < sheet->view_top + 20 && r < MAX_ROWS; r++) {
        printf("%3d: ", r + 1);
        for (int c = sheet->view_left; c < sheet->view_left + 10 && c < MAX_COLS; c++) {
            Cell *cell = &sheet->cells[r][c];
            char display[MAX_CELL_WIDTH + 1];
            
            if (r == sheet->cursor_row && c == sheet->cursor_col) {
                printf("\033[7m"); // Reverse video for cursor
            }
            
            switch (cell->type) {
                case CELL_NUMBER:
                    snprintf(display, MAX_CELL_WIDTH, "%.2f", cell->number);
                    break;
                case CELL_FORMULA:
                    snprintf(display, MAX_CELL_WIDTH, "%.2f", get_cell_value(sheet, r, c));
                    break;
                case CELL_TEXT:
                    snprintf(display, MAX_CELL_WIDTH, "%s", cell->text);
                    break;
                default:
                    strcpy(display, "");
            }
            
            printf("%-*.*s", MAX_CELL_WIDTH, MAX_CELL_WIDTH, display);
            
            if (r == sheet->cursor_row && c == sheet->cursor_col) {
                printf("\033[0m"); // Reset formatting
            }
        }
        printf("\n");
    }
    
    // Display current cell info
    printf("================================================================================\n");
    printf("Cell %c%d: ", col_to_letter(sheet->cursor_col), sheet->cursor_row + 1);
    Cell *current = &sheet->cells[sheet->cursor_row][sheet->cursor_col];
    
    switch (current->type) {
        case CELL_FORMULA:
            printf("%s = %.2f", current->formula, get_cell_value(sheet, sheet->cursor_row, sheet->cursor_col));
            break;
        case CELL_NUMBER:
            printf("%.2f", current->number);
            break;
        case CELL_TEXT:
            printf("%s", current->text);
            break;
        default:
            printf("(empty)");
    }
    printf("\n");
}

void save_spreadsheet(Spreadsheet *sheet, const char *filename) {
    FILE *f = fopen(filename, "w");
    if (!f) {
        printf("Error saving file!\n");
        return;
    }
    
    for (int r = 0; r < MAX_ROWS; r++) {
        for (int c = 0; c < MAX_COLS; c++) {
            Cell *cell = &sheet->cells[r][c];
            if (cell->type != CELL_EMPTY) {
                fprintf(f, "%d,%d,%d,", r, c, cell->type);
                switch (cell->type) {
                    case CELL_NUMBER:
                        fprintf(f, "%f\n", cell->number);
                        break;
                    case CELL_FORMULA:
                        fprintf(f, "%s\n", cell->formula);
                        break;
                    case CELL_TEXT:
                        fprintf(f, "%s\n", cell->text);
                        break;
                    default:
                        fprintf(f, "\n");
                }
            }
        }
    }
    
    fclose(f);
    strcpy(sheet->filename, filename);
}

void load_spreadsheet(Spreadsheet *sheet, const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        printf("Error loading file!\n");
        return;
    }
    
    init_spreadsheet(sheet);
    
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        int r, c, type;
        char data[MAX_FORMULA_LEN];
        
        if (sscanf(line, "%d,%d,%d,%[^\n]", &r, &c, &type, data) >= 3) {
            Cell *cell = &sheet->cells[r][c];
            cell->type = type;
            
            switch (type) {
                case CELL_NUMBER:
                    cell->number = atof(data);
                    break;
                case CELL_FORMULA:
                    strcpy(cell->formula, data);
                    cell->calculated = 0;
                    break;
                case CELL_TEXT:
                    strcpy(cell->text, data);
                    break;
            }
        }
    }
    
    fclose(f);
    strcpy(sheet->filename, filename);
    recalculate_all(sheet);
}

int main() {
    Spreadsheet sheet;
    init_spreadsheet(&sheet);
    
    enable_raw_mode();
    
    while (1) {
        display_sheet(&sheet);
        
        char c = getchar();
        
        if (c == 'q' || c == 'Q') {
            break;
        } else if (c == 's' || c == 'S') {
            printf("Save as: ");
            disable_raw_mode();
            char filename[256];
            scanf("%255s", filename);
            save_spreadsheet(&sheet, filename);
            enable_raw_mode();
        } else if (c == 'l' || c == 'L') {
            printf("Load file: ");
            disable_raw_mode();
            char filename[256];
            scanf("%255s", filename);
            load_spreadsheet(&sheet, filename);
            enable_raw_mode();
        } else if (c == 'd' || c == 'D') {
            sheet.cells[sheet.cursor_row][sheet.cursor_col].type = CELL_EMPTY;
            invalidate_formulas(&sheet);
            recalculate_all(&sheet);
        } else if (c == '\033') {
            getchar(); // Skip [
            c = getchar();
            switch (c) {
                case 'A': // Up arrow
                    if (sheet.cursor_row > 0) sheet.cursor_row--;
                    if (sheet.cursor_row < sheet.view_top) sheet.view_top = sheet.cursor_row;
                    break;
                case 'B': // Down arrow
                    if (sheet.cursor_row < MAX_ROWS - 1) sheet.cursor_row++;
                    if (sheet.cursor_row >= sheet.view_top + 20) sheet.view_top++;
                    break;
                case 'C': // Right arrow
                    if (sheet.cursor_col < MAX_COLS - 1) sheet.cursor_col++;
                    if (sheet.cursor_col >= sheet.view_left + 10) sheet.view_left++;
                    break;
                case 'D': // Left arrow
                    if (sheet.cursor_col > 0) sheet.cursor_col--;
                    if (sheet.cursor_col < sheet.view_left) sheet.view_left = sheet.cursor_col;
                    break;
            }
        } else if (c == '\n' || c == '\r') {
            printf("Enter value for %c%d: ", col_to_letter(sheet.cursor_col), sheet.cursor_row + 1);
            disable_raw_mode();
            char input[MAX_FORMULA_LEN];
            fgets(input, sizeof(input), stdin);
            input[strcspn(input, "\n")] = 0; // Remove newline
            set_cell_value(&sheet, sheet.cursor_row, sheet.cursor_col, input);
            enable_raw_mode();
        }
    }
    
    disable_raw_mode();
    clear_screen();
    printf("Goodbye!\n");
    
    return 0;
}
