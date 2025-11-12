#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#define MAX_ROWS 100
#define MAX_COLS 26
#define MAX_FORMULA_LEN 256
#define MAX_DISPLAY_WIDTH 12

// Cell structure
typedef struct {
    char formula[MAX_FORMULA_LEN];
    double value;
    int is_formula;
    int error;
} Cell;

// Spreadsheet structure
typedef struct {
    Cell cells[MAX_ROWS][MAX_COLS];
    int cursor_row;
    int cursor_col;
    int view_row;
    int view_col;
} Spreadsheet;

Spreadsheet sheet;

// Function prototypes
void init_spreadsheet();
void display_spreadsheet();
void move_cursor(int dr, int dc);
void edit_cell();
double evaluate_expression(const char* expr, int* error);
double parse_formula(const char* formula, int* error);
void recalculate_all();
void save_spreadsheet(const char* filename);
void load_spreadsheet(const char* filename);
void print_help();
char* get_cell_display(int row, int col);
int col_letter_to_num(char c);
char col_num_to_letter(int n);
double get_cell_value(int row, int col);

// Expression parsing
double parse_sum(const char* str, int* pos, int* error);
double parse_product(const char* str, int* pos, int* error);
double parse_factor(const char* str, int* pos, int* error);
double parse_function(const char* str, int* pos, int* error);
void skip_whitespace(const char* str, int* pos);

// Terminal control
void clear_screen() {
    printf("\033[2J\033[H");
}

void init_spreadsheet() {
    for (int i = 0; i < MAX_ROWS; i++) {
        for (int j = 0; j < MAX_COLS; j++) {
            sheet.cells[i][j].formula[0] = '\0';
            sheet.cells[i][j].value = 0.0;
            sheet.cells[i][j].is_formula = 0;
            sheet.cells[i][j].error = 0;
        }
    }
    sheet.cursor_row = 0;
    sheet.cursor_col = 0;
    sheet.view_row = 0;
    sheet.view_col = 0;
}

char col_num_to_letter(int n) {
    return 'A' + n;
}

int col_letter_to_num(char c) {
    return toupper(c) - 'A';
}

double get_cell_value(int row, int col) {
    if (row < 0 || row >= MAX_ROWS || col < 0 || col >= MAX_COLS) {
        return 0.0;
    }
    return sheet.cells[row][col].value;
}

void skip_whitespace(const char* str, int* pos) {
    while (str[*pos] == ' ' || str[*pos] == '\t') {
        (*pos)++;
    }
}

double parse_factor(const char* str, int* pos, int* error) {
    skip_whitespace(str, pos);
    
    // Handle parentheses
    if (str[*pos] == '(') {
        (*pos)++;
        double result = parse_sum(str, pos, error);
        skip_whitespace(str, pos);
        if (str[*pos] == ')') {
            (*pos)++;
        } else {
            *error = 1;
        }
        return result;
    }
    
    // Handle functions
    if (isalpha(str[*pos]) && isupper(str[*pos])) {
        // Check if it's a function or cell reference
        if (str[*pos + 1] && isalpha(str[*pos + 1])) {
            // It's a function
            return parse_function(str, pos, error);
        } else if (str[*pos + 1] && isdigit(str[*pos + 1])) {
            // It's a cell reference
            int col = col_letter_to_num(str[*pos]);
            (*pos)++;
            int row = 0;
            while (isdigit(str[*pos])) {
                row = row * 10 + (str[*pos] - '0');
                (*pos)++;
            }
            row--; // Convert to 0-indexed
            return get_cell_value(row, col);
        }
    }
    
    // Handle numbers
    if (isdigit(str[*pos]) || str[*pos] == '.' || str[*pos] == '-') {
        char* endptr;
        double result = strtod(str + *pos, &endptr);
        *pos += (endptr - (str + *pos));
        return result;
    }
    
    *error = 1;
    return 0.0;
}

double parse_product(const char* str, int* pos, int* error) {
    double result = parse_factor(str, pos, error);
    
    while (1) {
        skip_whitespace(str, pos);
        char op = str[*pos];
        
        if (op == '*' || op == '/') {
            (*pos)++;
            double right = parse_factor(str, pos, error);
            if (op == '*') {
                result *= right;
            } else {
                if (right == 0.0) {
                    *error = 1;
                    return 0.0;
                }
                result /= right;
            }
        } else {
            break;
        }
    }
    
    return result;
}

double parse_sum(const char* str, int* pos, int* error) {
    double result = parse_product(str, pos, error);
    
    while (1) {
        skip_whitespace(str, pos);
        char op = str[*pos];
        
        if (op == '+' || op == '-') {
            (*pos)++;
            double right = parse_product(str, pos, error);
            if (op == '+') {
                result += right;
            } else {
                result -= right;
            }
        } else {
            break;
        }
    }
    
    return result;
}

double parse_function(const char* str, int* pos, int* error) {
    char func_name[20];
    int fn_idx = 0;
    
    while (isalpha(str[*pos])) {
        func_name[fn_idx++] = str[*pos];
        (*pos)++;
    }
    func_name[fn_idx] = '\0';
    
    skip_whitespace(str, pos);
    if (str[*pos] != '(') {
        *error = 1;
        return 0.0;
    }
    (*pos)++;
    
    double values[100];
    int val_count = 0;
    
    while (1) {
        skip_whitespace(str, pos);
        if (str[*pos] == ')') {
            (*pos)++;
            break;
        }
        
        // Check for range (e.g., A1:A10)
        if (isalpha(str[*pos]) && isdigit(str[*pos + 1])) {
            int col1 = col_letter_to_num(str[*pos]);
            (*pos)++;
            int row1 = 0;
            while (isdigit(str[*pos])) {
                row1 = row1 * 10 + (str[*pos] - '0');
                (*pos)++;
            }
            row1--;
            
            skip_whitespace(str, pos);
            if (str[*pos] == ':') {
                (*pos)++;
                skip_whitespace(str, pos);
                int col2 = col_letter_to_num(str[*pos]);
                (*pos)++;
                int row2 = 0;
                while (isdigit(str[*pos])) {
                    row2 = row2 * 10 + (str[*pos] - '0');
                    (*pos)++;
                }
                row2--;
                
                // Add all cells in range
                for (int r = row1; r <= row2; r++) {
                    for (int c = col1; c <= col2; c++) {
                        if (val_count < 100) {
                            values[val_count++] = get_cell_value(r, c);
                        }
                    }
                }
            } else {
                values[val_count++] = get_cell_value(row1, col1);
            }
        } else {
            values[val_count++] = parse_sum(str, pos, error);
        }
        
        skip_whitespace(str, pos);
        if (str[*pos] == ',') {
            (*pos)++;
        }
    }
    
    // Execute function
    if (strcmp(func_name, "SUM") == 0) {
        double sum = 0.0;
        for (int i = 0; i < val_count; i++) sum += values[i];
        return sum;
    } else if (strcmp(func_name, "AVG") == 0 || strcmp(func_name, "AVERAGE") == 0) {
        if (val_count == 0) return 0.0;
        double sum = 0.0;
        for (int i = 0; i < val_count; i++) sum += values[i];
        return sum / val_count;
    } else if (strcmp(func_name, "MIN") == 0) {
        if (val_count == 0) return 0.0;
        double min = values[0];
        for (int i = 1; i < val_count; i++) {
            if (values[i] < min) min = values[i];
        }
        return min;
    } else if (strcmp(func_name, "MAX") == 0) {
        if (val_count == 0) return 0.0;
        double max = values[0];
        for (int i = 1; i < val_count; i++) {
            if (values[i] > max) max = values[i];
        }
        return max;
    } else if (strcmp(func_name, "COUNT") == 0) {
        return (double)val_count;
    } else if (strcmp(func_name, "SQRT") == 0) {
        if (val_count > 0) return sqrt(values[0]);
        return 0.0;
    } else if (strcmp(func_name, "ABS") == 0) {
        if (val_count > 0) return fabs(values[0]);
        return 0.0;
    } else if (strcmp(func_name, "POW") == 0) {
        if (val_count >= 2) return pow(values[0], values[1]);
        return 0.0;
    } else if (strcmp(func_name, "IF") == 0) {
        if (val_count >= 3) {
            return values[0] != 0.0 ? values[1] : values[2];
        }
        return 0.0;
    }
    
    *error = 1;
    return 0.0;
}

double parse_formula(const char* formula, int* error) {
    *error = 0;
    int pos = 0;
    
    // Skip leading '='
    if (formula[pos] == '=') {
        pos++;
    }
    
    double result = parse_sum(formula, &pos, error);
    
    skip_whitespace(formula, &pos);
    if (formula[pos] != '\0') {
        *error = 1;
    }
    
    return result;
}

void recalculate_all() {
    // Multiple passes for dependencies
    for (int pass = 0; pass < 3; pass++) {
        for (int i = 0; i < MAX_ROWS; i++) {
            for (int j = 0; j < MAX_COLS; j++) {
                Cell* cell = &sheet.cells[i][j];
                if (cell->formula[0] != '\0') {
                    if (cell->is_formula) {
                        int error = 0;
                        cell->value = parse_formula(cell->formula, &error);
                        cell->error = error;
                    } else {
                        // Plain number
                        cell->value = atof(cell->formula);
                        cell->error = 0;
                    }
                }
            }
        }
    }
}

char* get_cell_display(int row, int col) {
    static char display[MAX_DISPLAY_WIDTH + 1];
    Cell* cell = &sheet.cells[row][col];
    
    if (cell->formula[0] == '\0') {
        strcpy(display, "");
    } else if (cell->error) {
        strcpy(display, "#ERROR");
    } else if (cell->is_formula) {
        snprintf(display, MAX_DISPLAY_WIDTH + 1, "%.2f", cell->value);
    } else {
        snprintf(display, MAX_DISPLAY_WIDTH + 1, "%s", cell->formula);
    }
    
    return display;
}

void display_spreadsheet() {
    clear_screen();
    
    printf("=== SPREADSHEET === [%c%d] ===\n", 
           col_num_to_letter(sheet.cursor_col), 
           sheet.cursor_row + 1);
    
    // Display current cell formula
    Cell* current = &sheet.cells[sheet.cursor_row][sheet.cursor_col];
    printf("Formula: %s\n", current->formula[0] ? current->formula : "(empty)");
    printf("Value: ");
    if (current->error) {
        printf("#ERROR\n");
    } else if (current->formula[0]) {
        printf("%.4f\n", current->value);
    } else {
        printf("(empty)\n");
    }
    printf("\n");
    
    // Column headers
    printf("     ");
    for (int j = sheet.view_col; j < sheet.view_col + 6 && j < MAX_COLS; j++) {
        printf("     %c      ", col_num_to_letter(j));
    }
    printf("\n");
    
    printf("   +");
    for (int j = sheet.view_col; j < sheet.view_col + 6 && j < MAX_COLS; j++) {
        printf("------------+");
    }
    printf("\n");
    
    // Rows
    for (int i = sheet.view_row; i < sheet.view_row + 15 && i < MAX_ROWS; i++) {
        printf("%2d |", i + 1);
        for (int j = sheet.view_col; j < sheet.view_col + 6 && j < MAX_COLS; j++) {
            char* display = get_cell_display(i, j);
            
            if (i == sheet.cursor_row && j == sheet.cursor_col) {
                printf("\033[7m"); // Reverse video
            }
            
            printf(" %-11s", display);
            
            if (i == sheet.cursor_row && j == sheet.cursor_col) {
                printf("\033[0m"); // Reset
            }
            
            printf("|");
        }
        printf("\n");
    }
    
    printf("   +");
    for (int j = sheet.view_col; j < sheet.view_col + 6 && j < MAX_COLS; j++) {
        printf("------------+");
    }
    printf("\n\n");
    
    printf("Commands: [Arrows]=Move [E]=Edit [S]=Save [L]=Load [Q]=Quit [H]=Help\n");
}

void move_cursor(int dr, int dc) {
    sheet.cursor_row += dr;
    sheet.cursor_col += dc;
    
    if (sheet.cursor_row < 0) sheet.cursor_row = 0;
    if (sheet.cursor_row >= MAX_ROWS) sheet.cursor_row = MAX_ROWS - 1;
    if (sheet.cursor_col < 0) sheet.cursor_col = 0;
    if (sheet.cursor_col >= MAX_COLS) sheet.cursor_col = MAX_COLS - 1;
    
    // Adjust view if cursor is out of visible area
    if (sheet.cursor_row < sheet.view_row) {
        sheet.view_row = sheet.cursor_row;
    }
    if (sheet.cursor_row >= sheet.view_row + 15) {
        sheet.view_row = sheet.cursor_row - 14;
    }
    if (sheet.cursor_col < sheet.view_col) {
        sheet.view_col = sheet.cursor_col;
    }
    if (sheet.cursor_col >= sheet.view_col + 6) {
        sheet.view_col = sheet.cursor_col - 5;
    }
}

void edit_cell() {
    Cell* cell = &sheet.cells[sheet.cursor_row][sheet.cursor_col];
    
    printf("\nEdit cell %c%d\n", 
           col_num_to_letter(sheet.cursor_col), 
           sheet.cursor_row + 1);
    printf("Current: %s\n", cell->formula);
    printf("Enter formula (or value, or press ENTER to clear): ");
    
    char input[MAX_FORMULA_LEN];
    if (fgets(input, MAX_FORMULA_LEN, stdin)) {
        // Remove newline
        input[strcspn(input, "\n")] = 0;
        
        if (strlen(input) == 0) {
            // Clear cell
            cell->formula[0] = '\0';
            cell->value = 0.0;
            cell->is_formula = 0;
            cell->error = 0;
        } else {
            strncpy(cell->formula, input, MAX_FORMULA_LEN - 1);
            cell->formula[MAX_FORMULA_LEN - 1] = '\0';
            
            if (input[0] == '=') {
                cell->is_formula = 1;
            } else {
                cell->is_formula = 0;
            }
            
            recalculate_all();
        }
    }
}

void save_spreadsheet(const char* filename) {
    FILE* fp = fopen(filename, "w");
    if (!fp) {
        printf("Error: Could not open file for writing.\n");
        printf("Press ENTER to continue...");
        getchar();
        return;
    }
    
    // Save as CSV with formulas
    for (int i = 0; i < MAX_ROWS; i++) {
        int row_has_data = 0;
        for (int j = 0; j < MAX_COLS; j++) {
            if (sheet.cells[i][j].formula[0] != '\0') {
                row_has_data = 1;
                break;
            }
        }
        
        if (row_has_data) {
            for (int j = 0; j < MAX_COLS; j++) {
                fprintf(fp, "\"%s\"", sheet.cells[i][j].formula);
                if (j < MAX_COLS - 1) fprintf(fp, ",");
            }
            fprintf(fp, "\n");
        }
    }
    
    fclose(fp);
    printf("Saved to %s\n", filename);
    printf("Press ENTER to continue...");
    getchar();
}

void load_spreadsheet(const char* filename) {
    FILE* fp = fopen(filename, "r");
    if (!fp) {
        printf("Error: Could not open file for reading.\n");
        printf("Press ENTER to continue...");
        getchar();
        return;
    }
    
    init_spreadsheet();
    
    char line[MAX_COLS * MAX_FORMULA_LEN];
    int row = 0;
    
    while (fgets(line, sizeof(line), fp) && row < MAX_ROWS) {
        int col = 0;
        char* ptr = line;
        
        while (*ptr && col < MAX_COLS) {
            // Skip whitespace
            while (*ptr == ' ' || *ptr == '\t') ptr++;
            
            if (*ptr == '"') {
                ptr++;
                char* start = ptr;
                while (*ptr && *ptr != '"') ptr++;
                int len = ptr - start;
                if (len > 0 && len < MAX_FORMULA_LEN) {
                    strncpy(sheet.cells[row][col].formula, start, len);
                    sheet.cells[row][col].formula[len] = '\0';
                    
                    if (sheet.cells[row][col].formula[0] == '=') {
                        sheet.cells[row][col].is_formula = 1;
                    }
                }
                if (*ptr == '"') ptr++;
            }
            
            while (*ptr == ',' || *ptr == ' ' || *ptr == '\t') ptr++;
            col++;
        }
        row++;
    }
    
    fclose(fp);
    recalculate_all();
    
    printf("Loaded from %s\n", filename);
    printf("Press ENTER to continue...");
    getchar();
}

void print_help() {
    clear_screen();
    printf("=== SPREADSHEET HELP ===\n\n");
    printf("NAVIGATION:\n");
    printf("  Arrow keys: w/a/s/d or i/j/k/l\n");
    printf("  e - Edit current cell\n");
    printf("  q - Quit program\n\n");
    
    printf("CELL FORMULAS:\n");
    printf("  Plain values: 123 or 45.67\n");
    printf("  Formulas start with '=': =A1+B2\n");
    printf("  Cell references: A1, B2, Z99\n");
    printf("  Operators: + - * /\n");
    printf("  Parentheses: =(A1+B2)*C3\n\n");
    
    printf("FUNCTIONS:\n");
    printf("  SUM(A1:A10) - Sum of range\n");
    printf("  AVG(A1:A10) - Average\n");
    printf("  MIN(A1:A10) - Minimum\n");
    printf("  MAX(A1:A10) - Maximum\n");
    printf("  COUNT(A1:A10) - Count cells\n");
    printf("  SQRT(A1) - Square root\n");
    printf("  ABS(A1) - Absolute value\n");
    printf("  POW(A1,2) - Power\n");
    printf("  IF(A1>10,100,200) - Conditional\n\n");
    
    printf("FILE OPERATIONS:\n");
    printf("  s - Save spreadsheet\n");
    printf("  l - Load spreadsheet\n\n");
    
    printf("Press ENTER to continue...");
    getchar();
}

int main() {
    init_spreadsheet();
    
    // Sample data
    strcpy(sheet.cells[0][0].formula, "Revenue");
    strcpy(sheet.cells[1][0].formula, "100");
    strcpy(sheet.cells[2][0].formula, "200");
    strcpy(sheet.cells[3][0].formula, "150");
    strcpy(sheet.cells[4][0].formula, "=SUM(A2:A4)");
    sheet.cells[4][0].is_formula = 1;
    
    recalculate_all();
    
    char cmd;
    while (1) {
        display_spreadsheet();
        
        cmd = getchar();
        if (cmd == '\n') continue;
        
        // Consume rest of line
        while (getchar() != '\n');
        
        switch (cmd) {
            case 'w': case 'i': move_cursor(-1, 0); break;
            case 's': case 'k': move_cursor(1, 0); break;
            case 'a': case 'j': move_cursor(0, -1); break;
            case 'd': case 'l': move_cursor(0, 1); break;
            case 'e': case 'E': edit_cell(); break;
            case 'q': case 'Q': 
                printf("Quit? (y/n): ");
                if (getchar() == 'y') return 0;
                while (getchar() != '\n');
                break;
            case 'h': case 'H': print_help(); break;
            case 'S':
                printf("Filename: ");
                char savefile[256];
                if (fgets(savefile, 256, stdin)) {
                    savefile[strcspn(savefile, "\n")] = 0;
                    save_spreadsheet(savefile);
                }
                break;
            case 'L':
                printf("Filename: ");
                char loadfile[256];
                if (fgets(loadfile, 256, stdin)) {
                    loadfile[strcspn(loadfile, "\n")] = 0;
                    load_spreadsheet(loadfile);
                }
                break;
        }
    }
    
    return 0;
}
