#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#define MAX_ROWS 100
#define MAX_COLS 26
#define MAX_CELL_LEN 256
#define MAX_DISPLAY_WIDTH 12

typedef struct {
    char formula[MAX_CELL_LEN];
    double value;
    int is_formula;
    int error;
} Cell;

typedef struct {
    Cell cells[MAX_ROWS][MAX_COLS];
    int cursor_row;
    int cursor_col;
    int view_row;
    int view_col;
} Spreadsheet;

void init_spreadsheet(Spreadsheet *s);
void display_spreadsheet(Spreadsheet *s);
void handle_input(Spreadsheet *s);
void edit_cell(Spreadsheet *s, int row, int col, const char *input);
double evaluate_formula(Spreadsheet *s, const char *formula, int row, int col);
double parse_expression(Spreadsheet *s, const char **str, int row, int col);
double parse_term(Spreadsheet *s, const char **str, int row, int col);
double parse_factor(Spreadsheet *s, const char **str, int row, int col);
double get_cell_value(Spreadsheet *s, int row, int col);
void parse_cell_ref(const char **str, int *row, int *col);
void recalculate_all(Spreadsheet *s);
void save_spreadsheet(Spreadsheet *s, const char *filename);
void load_spreadsheet(Spreadsheet *s, const char *filename);
double eval_function(Spreadsheet *s, const char *func, const char *args, int row, int col);
void parse_range(const char *range, int *r1, int *c1, int *r2, int *c2);

int main() {
    Spreadsheet s;
    init_spreadsheet(&s);
    
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║         C SPREADSHEET - Multiplan Style v1.0             ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n\n");
    printf("Navigation: h=left, j=down, k=up, l=right\n");
    printf("Commands:   e=edit cell, s=save, o=open, q=quit, ?=help\n");
    printf("\nPress Enter to continue...");
    getchar();
    
    while (1) {
        display_spreadsheet(&s);
        handle_input(&s);
    }
    
    return 0;
}

void init_spreadsheet(Spreadsheet *s) {
    for (int i = 0; i < MAX_ROWS; i++) {
        for (int j = 0; j < MAX_COLS; j++) {
            s->cells[i][j].formula[0] = '\0';
            s->cells[i][j].value = 0.0;
            s->cells[i][j].is_formula = 0;
            s->cells[i][j].error = 0;
        }
    }
    s->cursor_row = 0;
    s->cursor_col = 0;
    s->view_row = 0;
    s->view_col = 0;
}

void display_spreadsheet(Spreadsheet *s) {
    printf("\033[2J\033[H"); // Clear screen
    
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║  C SPREADSHEET                     Cell: %c%-2d            ║\n", 
           'A' + s->cursor_col, s->cursor_row + 1);
    printf("╚═══════════════════════════════════════════════════════════╝\n");
    
    Cell *current = &s->cells[s->cursor_row][s->cursor_col];
    if (strlen(current->formula) > 0) {
        printf("Formula: %-50s\n", current->formula);
        if (current->error) {
            printf("Value:   ERROR\n");
        } else {
            printf("Value:   %.4f\n", current->value);
        }
    } else {
        printf("Formula: (empty)\n");
        printf("Value:   \n");
    }
    
    printf("\n     ");
    for (int j = s->view_col; j < s->view_col + 6 && j < MAX_COLS; j++) {
        printf("%-12c ", 'A' + j);
    }
    printf("\n   ┌");
    for (int j = s->view_col; j < s->view_col + 6 && j < MAX_COLS; j++) {
        printf("────────────┬");
    }
    printf("\n");
    
    for (int i = s->view_row; i < s->view_row + 18 && i < MAX_ROWS; i++) {
        printf("%2d │", i + 1);
        for (int j = s->view_col; j < s->view_col + 6 && j < MAX_COLS; j++) {
            Cell *cell = &s->cells[i][j];
            
            if (i == s->cursor_row && j == s->cursor_col) {
                printf("\033[7m"); // Reverse video
            }
            
            if (cell->error) {
                printf(" %-11s", "ERROR");
            } else if (cell->is_formula || strlen(cell->formula) > 0) {
                char display[13];
                if (fabs(cell->value) < 0.01 && cell->value != 0) {
                    snprintf(display, 12, "%.2e", cell->value);
                } else {
                    snprintf(display, 12, "%.2f", cell->value);
                }
                printf(" %-11s", display);
            } else {
                printf(" %-11s", "");
            }
            
            if (i == s->cursor_row && j == s->cursor_col) {
                printf("\033[0m"); // Reset
            }
            printf("│");
        }
        printf("\n");
    }
    
    printf("\n[h/j/k/l]:Move  [e]:Edit  [s]:Save  [o]:Open  [?]:Help  [q]:Quit\n> ");
}

void handle_input(Spreadsheet *s) {
    char cmd[MAX_CELL_LEN];
    if (fgets(cmd, sizeof(cmd), stdin) == NULL) return;
    cmd[strcspn(cmd, "\n")] = 0;
    if (strlen(cmd) == 0) return;
    
    switch (cmd[0]) {
        case 'h': case 'H':
            if (s->cursor_col > 0) {
                s->cursor_col--;
                if (s->cursor_col < s->view_col) s->view_col = s->cursor_col;
            }
            break;
        case 'l': case 'L':
            if (s->cursor_col < MAX_COLS - 1) {
                s->cursor_col++;
                if (s->cursor_col >= s->view_col + 6) s->view_col = s->cursor_col - 5;
            }
            break;
        case 'k': case 'K':
            if (s->cursor_row > 0) {
                s->cursor_row--;
                if (s->cursor_row < s->view_row) s->view_row = s->cursor_row;
            }
            break;
        case 'j': case 'J':
            if (s->cursor_row < MAX_ROWS - 1) {
                s->cursor_row++;
                if (s->cursor_row >= s->view_row + 18) s->view_row = s->cursor_row - 17;
            }
            break;
        case 'e': case 'E':
            printf("Enter formula/value (prefix with = for formula, empty to clear):\n> ");
            if (fgets(cmd, sizeof(cmd), stdin) != NULL) {
                cmd[strcspn(cmd, "\n")] = 0;
                edit_cell(s, s->cursor_row, s->cursor_col, cmd);
                recalculate_all(s);
            }
            break;
        case 's': case 'S':
            printf("Filename to save: ");
            if (fgets(cmd, sizeof(cmd), stdin) != NULL) {
                cmd[strcspn(cmd, "\n")] = 0;
                save_spreadsheet(s, cmd);
                printf("Saved to %s! Press Enter...", cmd);
                getchar();
            }
            break;
        case 'o': case 'O':
            printf("Filename to open: ");
            if (fgets(cmd, sizeof(cmd), stdin) != NULL) {
                cmd[strcspn(cmd, "\n")] = 0;
                load_spreadsheet(s, cmd);
                recalculate_all(s);
                printf("Loaded %s! Press Enter...", cmd);
                getchar();
            }
            break;
        case '?':
            printf("\n=== HELP ===\n");
            printf("Formulas start with =, e.g., =A1+B2*3\n");
            printf("Functions: SUM(A1:A10), AVERAGE(B1:B5), MIN, MAX, COUNT\n");
            printf("Operators: +, -, *, /, ()\n");
            printf("Cell refs: A1, B2, etc.\n");
            printf("Ranges: A1:A10\n");
            printf("\nPress Enter...");
            getchar();
            break;
        case 'q': case 'Q':
            printf("Quit without saving? (y/n): ");
            if (fgets(cmd, sizeof(cmd), stdin) != NULL && (cmd[0] == 'y' || cmd[0] == 'Y')) {
                exit(0);
            }
            break;
    }
}

void edit_cell(Spreadsheet *s, int row, int col, const char *input) {
    Cell *cell = &s->cells[row][col];
    
    if (strlen(input) == 0) {
        cell->formula[0] = '\0';
        cell->value = 0.0;
        cell->is_formula = 0;
        cell->error = 0;
        return;
    }
    
    strncpy(cell->formula, input, MAX_CELL_LEN - 1);
    cell->formula[MAX_CELL_LEN - 1] = '\0';
    cell->error = 0;
    
    if (input[0] == '=') {
        cell->is_formula = 1;
        cell->value = evaluate_formula(s, input + 1, row, col);
    } else {
        cell->is_formula = 0;
        char *endptr;
        double val = strtod(input, &endptr);
        if (endptr != input && (*endptr == '\0' || isspace(*endptr))) {
            cell->value = val;
        } else {
            cell->value = 0.0;
        }
    }
}

double get_cell_value(Spreadsheet *s, int row, int col) {
    if (row < 0 || row >= MAX_ROWS || col < 0 || col >= MAX_COLS) return 0.0;
    return s->cells[row][col].value;
}

void parse_cell_ref(const char **str, int *row, int *col) {
    *col = -1;
    *row = -1;
    
    while (isspace(**str)) (*str)++;
    if (!isalpha(**str)) return;
    
    *col = toupper(**str) - 'A';
    (*str)++;
    
    if (!isdigit(**str)) {
        *col = -1;
        return;
    }
    
    *row = 0;
    while (isdigit(**str)) {
        *row = *row * 10 + (**str - '0');
        (*str)++;
    }
    *row -= 1;
}

void parse_range(const char *range, int *r1, int *c1, int *r2, int *c2) {
    const char *ptr = range;
    parse_cell_ref(&ptr, r1, c1);
    
    while (isspace(*ptr)) ptr++;
    
    if (*ptr == ':') {
        ptr++;
        parse_cell_ref(&ptr, r2, c2);
    } else {
        *r2 = *r1;
        *c2 = *c1;
    }
}

double eval_function(Spreadsheet *s, const char *func, const char *args, int row, int col) {
    int r1, c1, r2, c2;
    parse_range(args, &r1, &c1, &r2, &c2);
    
    if (r1 < 0 || c1 < 0) return 0.0;
    if (r2 < 0) r2 = r1;
    if (c2 < 0) c2 = c1;
    
    if (strcmp(func, "SUM") == 0) {
        double sum = 0.0;
        for (int i = r1; i <= r2 && i < MAX_ROWS; i++)
            for (int j = c1; j <= c2 && j < MAX_COLS; j++)
                sum += get_cell_value(s, i, j);
        return sum;
    } else if (strcmp(func, "AVERAGE") == 0 || strcmp(func, "AVG") == 0) {
        double sum = 0.0;
        int count = 0;
        for (int i = r1; i <= r2 && i < MAX_ROWS; i++)
            for (int j = c1; j <= c2 && j < MAX_COLS; j++) {
                sum += get_cell_value(s, i, j);
                count++;
            }
        return count > 0 ? sum / count : 0.0;
    } else if (strcmp(func, "MIN") == 0) {
        double min_val = INFINITY;
        for (int i = r1; i <= r2 && i < MAX_ROWS; i++)
            for (int j = c1; j <= c2 && j < MAX_COLS; j++) {
                double v = get_cell_value(s, i, j);
                if (v < min_val) min_val = v;
            }
        return min_val == INFINITY ? 0.0 : min_val;
    } else if (strcmp(func, "MAX") == 0) {
        double max_val = -INFINITY;
        for (int i = r1; i <= r2 && i < MAX_ROWS; i++)
            for (int j = c1; j <= c2 && j < MAX_COLS; j++) {
                double v = get_cell_value(s, i, j);
                if (v > max_val) max_val = v;
            }
        return max_val == -INFINITY ? 0.0 : max_val;
    } else if (strcmp(func, "COUNT") == 0) {
        return (double)((r2 - r1 + 1) * (c2 - c1 + 1));
    }
    return 0.0;
}

double parse_factor(Spreadsheet *s, const char **str, int row, int col) {
    while (isspace(**str)) (*str)++;
    
    if (**str == '(') {
        (*str)++;
        double result = parse_expression(s, str, row, col);
        while (isspace(**str)) (*str)++;
        if (**str == ')') (*str)++;
        return result;
    }
    
    if (isalpha(**str)) {
        char func[32];
        int i = 0;
        const char *start = *str;
        while (isalpha(**str) && i < 31) {
            func[i++] = toupper(**str);
            (*str)++;
        }
        func[i] = '\0';
        
        while (isspace(**str)) (*str)++;
        
        if (**str == '(') {
            (*str)++;
            char args[MAX_CELL_LEN];
            i = 0;
            int paren = 1;
            while (paren > 0 && **str && i < MAX_CELL_LEN - 1) {
                if (**str == '(') paren++;
                else if (**str == ')') paren--;
                if (paren > 0) args[i++] = **str;
                (*str)++;
            }
            args[i] = '\0';
            return eval_function(s, func, args, row, col);
        } else {
            *str = start;
            int r, c;
            parse_cell_ref(str, &r, &c);
            if (r >= 0 && c >= 0) return get_cell_value(s, r, c);
            return 0.0;
        }
    }
    
    if (**str == '-') {
        (*str)++;
        return -parse_factor(s, str, row, col);
    }
    
    if (**str == '+') {
        (*str)++;
        return parse_factor(s, str, row, col);
    }
    
    if (isdigit(**str) || **str == '.') {
        char *end;
        double val = strtod(*str, &end);
        *str = end;
        return val;
    }
    
    return 0.0;
}

double parse_term(Spreadsheet *s, const char **str, int row, int col) {
    double result = parse_factor(s, str, row, col);
    
    while (1) {
        while (isspace(**str)) (*str)++;
        if (**str == '*') {
            (*str)++;
            result *= parse_factor(s, str, row, col);
        } else if (**str == '/') {
            (*str)++;
            double divisor = parse_factor(s, str, row, col);
            result = (divisor != 0.0) ? result / divisor : 0.0;
        } else {
            break;
        }
    }
    return result;
}

double parse_expression(Spreadsheet *s, const char **str, int row, int col) {
    double result = parse_term(s, str, row, col);
    
    while (1) {
        while (isspace(**str)) (*str)++;
        if (**str == '+') {
            (*str)++;
            result += parse_term(s, str, row, col);
        } else if (**str == '-') {
            (*str)++;
            result -= parse_term(s, str, row, col);
        } else {
            break;
        }
    }
    return result;
}

double evaluate_formula(Spreadsheet *s, const char *formula, int row, int col) {
    const char *ptr = formula;
    return parse_expression(s, &ptr, row, col);
}

void recalculate_all(Spreadsheet *s) {
    for (int pass = 0; pass < 5; pass++) {
        for (int i = 0; i < MAX_ROWS; i++) {
            for (int j = 0; j < MAX_COLS; j++) {
                Cell *cell = &s->cells[i][j];
                if (cell->is_formula && cell->formula[0] == '=') {
                    cell->value = evaluate_formula(s, cell->formula + 1, i, j);
                }
            }
        }
    }
}

void save_spreadsheet(Spreadsheet *s, const char *filename) {
    FILE *f = fopen(filename, "w");
    if (!f) {
        printf("Error: Cannot write to file!\n");
        return;
    }
    
    fprintf(f, "# C Spreadsheet File\n");
    for (int i = 0; i < MAX_ROWS; i++) {
        for (int j = 0; j < MAX_COLS; j++) {
            if (strlen(s->cells[i][j].formula) > 0) {
                fprintf(f, "%d,%d,%s\n", i, j, s->cells[i][j].formula);
            }
        }
    }
    fclose(f);
}

void load_spreadsheet(Spreadsheet *s, const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        printf("Error: Cannot open file!\n");
        return;
    }
    
    init_spreadsheet(s);
    char line[MAX_CELL_LEN + 32];
    
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#') continue;
        
        int row, col;
        char formula[MAX_CELL_LEN];
        if (sscanf(line, "%d,%d,", &row, &col) == 2) {
            char *f_start = strchr(line, ',');
            if (f_start && (f_start = strchr(f_start + 1, ','))) {
                strncpy(formula, f_start + 1, MAX_CELL_LEN - 1);
                formula[strcspn(formula, "\n")] = 0;
                if (row >= 0 && row < MAX_ROWS && col >= 0 && col < MAX_COLS) {
                    edit_cell(s, row, col, formula);
                }
            }
        }
    }
    fclose(f);
}
