#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#define MAX_ROWS 100
#define MAX_COLS 26  // A-Z
#define MAX_FORMULA_LEN 256
#define MAX_STR_LEN 256

typedef enum {
    CELL_EMPTY,
    CELL_NUMBER,
    CELL_STRING,
    CELL_FORMULA
} CellType;

typedef struct {
    CellType type;
    double number_value;
    char string_value[MAX_STR_LEN];
    char formula[MAX_FORMULA_LEN];
    double computed_value;
    int needs_recalc;
} Cell;

typedef struct {
    Cell cells[MAX_ROWS][MAX_COLS];
    int active_row;
    int active_col;
} Spreadsheet;

// Global spreadsheet instance
Spreadsheet sheet;

// Function prototypes
void init_spreadsheet();
void display_spreadsheet();
void clear_screen();
int parse_cell_reference(const char* ref, int* row, int* col);
double evaluate_formula(const char* formula);
double evaluate_expression(const char* expr, int* pos);
double evaluate_term(const char* expr, int* pos);
double evaluate_factor(const char* expr, int* pos);
double evaluate_function(const char* func_name, const char* args);
void set_cell_value(int row, int col, const char* value);
void recalculate_all();
void save_to_csv(const char* filename);
void load_from_csv(const char* filename);
void process_command(const char* cmd);
double get_cell_numeric_value(int row, int col);
void skip_whitespace(const char* expr, int* pos);

// Initialize spreadsheet
void init_spreadsheet() {
    for (int i = 0; i < MAX_ROWS; i++) {
        for (int j = 0; j < MAX_COLS; j++) {
            sheet.cells[i][j].type = CELL_EMPTY;
            sheet.cells[i][j].number_value = 0;
            sheet.cells[i][j].string_value[0] = '\0';
            sheet.cells[i][j].formula[0] = '\0';
            sheet.cells[i][j].computed_value = 0;
            sheet.cells[i][j].needs_recalc = 0;
        }
    }
    sheet.active_row = 0;
    sheet.active_col = 0;
}

// Clear screen (platform-independent way)
void clear_screen() {
    printf("\033[2J\033[H");  // ANSI escape codes
}

// Display the spreadsheet
void display_spreadsheet() {
    printf("\n     ");
    for (int j = 0; j < 10 && j < MAX_COLS; j++) {
        printf("      %c      ", 'A' + j);
    }
    printf("\n");
    
    printf("    +");
    for (int j = 0; j < 10 && j < MAX_COLS; j++) {
        printf("-------------+");
    }
    printf("\n");
    
    for (int i = 0; i < 20 && i < MAX_ROWS; i++) {
        printf("%3d |", i + 1);
        for (int j = 0; j < 10 && j < MAX_COLS; j++) {
            Cell* cell = &sheet.cells[i][j];
            char display[14];
            display[0] = '\0';
            
            if (cell->type == CELL_NUMBER) {
                snprintf(display, sizeof(display), "%.2f", cell->number_value);
            } else if (cell->type == CELL_STRING) {
                snprintf(display, sizeof(display), "%s", cell->string_value);
            } else if (cell->type == CELL_FORMULA) {
                snprintf(display, sizeof(display), "%.2f", cell->computed_value);
            }
            
            // Truncate if too long
            if (strlen(display) > 12) {
                display[12] = '\0';
            }
            
            printf(" %-12s|", display);
        }
        printf("\n");
    }
    
    printf("    +");
    for (int j = 0; j < 10 && j < MAX_COLS; j++) {
        printf("-------------+");
    }
    printf("\n");
}

// Parse cell reference (e.g., "A1" -> row=0, col=0)
int parse_cell_reference(const char* ref, int* row, int* col) {
    if (!ref || strlen(ref) < 2) return 0;
    
    char col_char = toupper(ref[0]);
    if (col_char < 'A' || col_char > 'Z') return 0;
    
    *col = col_char - 'A';
    *row = atoi(ref + 1) - 1;
    
    if (*row < 0 || *row >= MAX_ROWS || *col < 0 || *col >= MAX_COLS) {
        return 0;
    }
    
    return 1;
}

// Get numeric value of a cell
double get_cell_numeric_value(int row, int col) {
    if (row < 0 || row >= MAX_ROWS || col < 0 || col >= MAX_COLS) {
        return 0;
    }
    
    Cell* cell = &sheet.cells[row][col];
    if (cell->type == CELL_NUMBER) {
        return cell->number_value;
    } else if (cell->type == CELL_FORMULA) {
        return cell->computed_value;
    }
    return 0;
}

// Skip whitespace in expression
void skip_whitespace(const char* expr, int* pos) {
    while (expr[*pos] == ' ' || expr[*pos] == '\t') {
        (*pos)++;
    }
}

// Evaluate a factor (number, cell reference, or parenthesized expression)
double evaluate_factor(const char* expr, int* pos) {
    skip_whitespace(expr, pos);
    
    // Check for parentheses
    if (expr[*pos] == '(') {
        (*pos)++;
        double result = evaluate_expression(expr, pos);
        skip_whitespace(expr, pos);
        if (expr[*pos] == ')') {
            (*pos)++;
        }
        return result;
    }
    
    // Check for cell reference
    if (isalpha(expr[*pos])) {
        char ref[10];
        int ref_pos = 0;
        ref[ref_pos++] = expr[(*pos)++];
        while (isdigit(expr[*pos]) && ref_pos < 9) {
            ref[ref_pos++] = expr[(*pos)++];
        }
        ref[ref_pos] = '\0';
        
        int row, col;
        if (parse_cell_reference(ref, &row, &col)) {
            return get_cell_numeric_value(row, col);
        }
        return 0;
    }
    
    // Parse number
    double num = 0;
    int decimal = 0;
    double decimal_place = 0.1;
    
    while (isdigit(expr[*pos]) || expr[*pos] == '.') {
        if (expr[*pos] == '.') {
            decimal = 1;
            (*pos)++;
        } else {
            if (decimal) {
                num += (expr[*pos] - '0') * decimal_place;
                decimal_place *= 0.1;
            } else {
                num = num * 10 + (expr[*pos] - '0');
            }
            (*pos)++;
        }
    }
    
    return num;
}

// Evaluate a term (handles multiplication and division)
double evaluate_term(const char* expr, int* pos) {
    double left = evaluate_factor(expr, pos);
    
    while (1) {
        skip_whitespace(expr, pos);
        char op = expr[*pos];
        if (op != '*' && op != '/') break;
        
        (*pos)++;
        double right = evaluate_factor(expr, pos);
        
        if (op == '*') {
            left *= right;
        } else {
            if (right != 0) {
                left /= right;
            }
        }
    }
    
    return left;
}

// Evaluate an expression (handles addition and subtraction)
double evaluate_expression(const char* expr, int* pos) {
    double left = evaluate_term(expr, pos);
    
    while (1) {
        skip_whitespace(expr, pos);
        char op = expr[*pos];
        if (op != '+' && op != '-') break;
        
        (*pos)++;
        double right = evaluate_term(expr, pos);
        
        if (op == '+') {
            left += right;
        } else {
            left -= right;
        }
    }
    
    return left;
}

// Evaluate built-in functions
double evaluate_function(const char* func_name, const char* args) {
    if (strcmp(func_name, "SUM") == 0) {
        // Parse range (e.g., "A1:A5")
        char start_ref[10], end_ref[10];
        if (sscanf(args, "%[^:]:%s", start_ref, end_ref) == 2) {
            int start_row, start_col, end_row, end_col;
            if (parse_cell_reference(start_ref, &start_row, &start_col) &&
                parse_cell_reference(end_ref, &end_row, &end_col)) {
                
                double sum = 0;
                for (int i = start_row; i <= end_row; i++) {
                    for (int j = start_col; j <= end_col; j++) {
                        sum += get_cell_numeric_value(i, j);
                    }
                }
                return sum;
            }
        }
    } else if (strcmp(func_name, "AVG") == 0 || strcmp(func_name, "AVERAGE") == 0) {
        char start_ref[10], end_ref[10];
        if (sscanf(args, "%[^:]:%s", start_ref, end_ref) == 2) {
            int start_row, start_col, end_row, end_col;
            if (parse_cell_reference(start_ref, &start_row, &start_col) &&
                parse_cell_reference(end_ref, &end_row, &end_col)) {
                
                double sum = 0;
                int count = 0;
                for (int i = start_row; i <= end_row; i++) {
                    for (int j = start_col; j <= end_col; j++) {
                        sum += get_cell_numeric_value(i, j);
                        count++;
                    }
                }
                return count > 0 ? sum / count : 0;
            }
        }
    } else if (strcmp(func_name, "MIN") == 0) {
        char start_ref[10], end_ref[10];
        if (sscanf(args, "%[^:]:%s", start_ref, end_ref) == 2) {
            int start_row, start_col, end_row, end_col;
            if (parse_cell_reference(start_ref, &start_row, &start_col) &&
                parse_cell_reference(end_ref, &end_row, &end_col)) {
                
                double min_val = get_cell_numeric_value(start_row, start_col);
                for (int i = start_row; i <= end_row; i++) {
                    for (int j = start_col; j <= end_col; j++) {
                        double val = get_cell_numeric_value(i, j);
                        if (val < min_val) min_val = val;
                    }
                }
                return min_val;
            }
        }
    } else if (strcmp(func_name, "MAX") == 0) {
        char start_ref[10], end_ref[10];
        if (sscanf(args, "%[^:]:%s", start_ref, end_ref) == 2) {
            int start_row, start_col, end_row, end_col;
            if (parse_cell_reference(start_ref, &start_row, &start_col) &&
                parse_cell_reference(end_ref, &end_row, &end_col)) {
                
                double max_val = get_cell_numeric_value(start_row, start_col);
                for (int i = start_row; i <= end_row; i++) {
                    for (int j = start_col; j <= end_col; j++) {
                        double val = get_cell_numeric_value(i, j);
                        if (val > max_val) max_val = val;
                    }
                }
                return max_val;
            }
        }
    }
    
    return 0;
}

// Evaluate a formula
double evaluate_formula(const char* formula) {
    if (!formula || formula[0] != '=') return 0;
    
    // Check for functions
    char func_name[50];
    char func_args[200];
    if (sscanf(formula + 1, "%[A-Z](%[^)])", func_name, func_args) == 2) {
        return evaluate_function(func_name, func_args);
    }
    
    // Regular expression evaluation
    int pos = 1;  // Skip '='
    return evaluate_expression(formula, &pos);
}

// Set cell value
void set_cell_value(int row, int col, const char* value) {
    if (row < 0 || row >= MAX_ROWS || col < 0 || col >= MAX_COLS) return;
    
    Cell* cell = &sheet.cells[row][col];
    
    if (value[0] == '=') {
        // Formula
        cell->type = CELL_FORMULA;
        strncpy(cell->formula, value, MAX_FORMULA_LEN - 1);
        cell->computed_value = evaluate_formula(value);
    } else if (isdigit(value[0]) || value[0] == '-' || value[0] == '.') {
        // Number
        cell->type = CELL_NUMBER;
        cell->number_value = atof(value);
    } else {
        // String
        cell->type = CELL_STRING;
        strncpy(cell->string_value, value, MAX_STR_LEN - 1);
    }
}

// Recalculate all formulas
void recalculate_all() {
    for (int i = 0; i < MAX_ROWS; i++) {
        for (int j = 0; j < MAX_COLS; j++) {
            if (sheet.cells[i][j].type == CELL_FORMULA) {
                sheet.cells[i][j].computed_value = evaluate_formula(sheet.cells[i][j].formula);
            }
        }
    }
}

// Save to CSV file
void save_to_csv(const char* filename) {
    FILE* file = fopen(filename, "w");
    if (!file) {
        printf("Error: Cannot open file for writing\n");
        return;
    }
    
    for (int i = 0; i < MAX_ROWS; i++) {
        int row_has_data = 0;
        for (int j = 0; j < MAX_COLS; j++) {
            if (sheet.cells[i][j].type != CELL_EMPTY) {
                row_has_data = 1;
                break;
            }
        }
        
        if (row_has_data) {
            for (int j = 0; j < MAX_COLS; j++) {
                Cell* cell = &sheet.cells[i][j];
                if (j > 0) fprintf(file, ",");
                
                if (cell->type == CELL_NUMBER) {
                    fprintf(file, "%.2f", cell->number_value);
                } else if (cell->type == CELL_STRING) {
                    fprintf(file, "\"%s\"", cell->string_value);
                } else if (cell->type == CELL_FORMULA) {
                    fprintf(file, "%s", cell->formula);
                }
            }
            fprintf(file, "\n");
        }
    }
    
    fclose(file);
    printf("Saved to %s\n", filename);
}

// Load from CSV file
void load_from_csv(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        printf("Error: Cannot open file for reading\n");
        return;
    }
    
    init_spreadsheet();
    
    char line[1024];
    int row = 0;
    while (fgets(line, sizeof(line), file) && row < MAX_ROWS) {
        int col = 0;
        char* token = strtok(line, ",\n");
        while (token && col < MAX_COLS) {
            // Remove quotes if present
            if (token[0] == '"' && token[strlen(token)-1] == '"') {
                token[strlen(token)-1] = '\0';
                token++;
            }
            set_cell_value(row, col, token);
            token = strtok(NULL, ",\n");
            col++;
        }
        row++;
    }
    
    fclose(file);
    recalculate_all();
    printf("Loaded from %s\n", filename);
}

// Process user commands
void process_command(const char* cmd) {
    char command[50];
    char arg1[100];
    char arg2[500];
    
    int parsed = sscanf(cmd, "%s %s %[^\n]", command, arg1, arg2);
    
    if (strcmp(command, "SET") == 0 && parsed >= 3) {
        int row, col;
        if (parse_cell_reference(arg1, &row, &col)) {
            set_cell_value(row, col, arg2);
            recalculate_all();
        } else {
            printf("Invalid cell reference\n");
        }
    } else if (strcmp(command, "SAVE") == 0 && parsed >= 2) {
        save_to_csv(arg1);
    } else if (strcmp(command, "LOAD") == 0 && parsed >= 2) {
        load_from_csv(arg1);
    } else if (strcmp(command, "CLEAR") == 0) {
        init_spreadsheet();
    } else if (strcmp(command, "HELP") == 0) {
        printf("\nCommands:\n");
        printf("  SET <cell> <value>  - Set cell value (e.g., SET A1 42)\n");
        printf("  SET <cell> =<formula> - Set formula (e.g., SET A3 =A1+A2)\n");
        printf("  SAVE <filename>     - Save to CSV file\n");
        printf("  LOAD <filename>     - Load from CSV file\n");
        printf("  CLEAR               - Clear all cells\n");
        printf("  QUIT                - Exit program\n");
        printf("\nFunctions: SUM, AVG/AVERAGE, MIN, MAX\n");
        printf("Example: SET A5 =SUM(A1:A4)\n\n");
    } else if (strcmp(command, "QUIT") == 0 || strcmp(command, "EXIT") == 0) {
        printf("Goodbye!\n");
        exit(0);
    } else {
        printf("Unknown command. Type HELP for commands.\n");
    }
}

int main() {
    init_spreadsheet();
    
    printf("=== Simple Spreadsheet (Excel-like) ===\n");
    printf("Type HELP for commands\n\n");
    
    // Example data
    set_cell_value(0, 0, "10");    // A1
    set_cell_value(1, 0, "20");    // A2
    set_cell_value(2, 0, "30");    // A3
    set_cell_value(3, 0, "=A1+A2+A3");  // A4
    set_cell_value(4, 0, "=SUM(A1:A3)"); // A5
    set_cell_value(0, 1, "Product");     // B1
    set_cell_value(1, 1, "=A1*2");       // B2
    recalculate_all();
    
    while (1) {
        display_spreadsheet();
        
        printf("\nCommand: ");
        char input[600];
        if (fgets(input, sizeof(input), stdin)) {
            // Remove newline
            input[strcspn(input, "\n")] = 0;
            
            // Convert to uppercase for command parsing
            char upper_input[600];
            strcpy(upper_input, input);
            for (int i = 0; upper_input[i] && i < 50; i++) {
                if (upper_input[i] == ' ') break;
                upper_input[i] = toupper(upper_input[i]);
            }
            
            process_command(upper_input);
        }
    }
    
    return 0;
}
