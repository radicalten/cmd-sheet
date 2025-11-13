#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#define MAX_ROWS 100
#define MAX_COLS 26
#define MAX_CELL_SIZE 256
#define MAX_FORMULA_SIZE 256

typedef enum {
    CELL_EMPTY,
    CELL_NUMBER,
    CELL_STRING,
    CELL_FORMULA
} CellType;

typedef struct {
    CellType type;
    char raw_content[MAX_CELL_SIZE];
    double numeric_value;
    char string_value[MAX_CELL_SIZE];
    char formula[MAX_FORMULA_SIZE];
} Cell;

typedef struct {
    Cell cells[MAX_ROWS][MAX_COLS];
    int cursor_row;
    int cursor_col;
} Spreadsheet;

// Function prototypes
void init_spreadsheet(Spreadsheet *sheet);
void display_spreadsheet(Spreadsheet *sheet);
void clear_screen();
void move_cursor(Spreadsheet *sheet, char direction);
void edit_cell(Spreadsheet *sheet);
double evaluate_formula(Spreadsheet *sheet, const char *formula);
double evaluate_cell_reference(Spreadsheet *sheet, const char *ref);
double evaluate_expression(Spreadsheet *sheet, const char *expr);
void save_spreadsheet(Spreadsheet *sheet, const char *filename);
void load_spreadsheet(Spreadsheet *sheet, const char *filename);
void recalculate_formulas(Spreadsheet *sheet);
int parse_cell_reference(const char *ref, int *row, int *col);

// Clear screen (works on most terminals)
void clear_screen() {
    #ifdef _WIN32
        system("cls");
    #else
        system("clear");
    #endif
}

// Initialize empty spreadsheet
void init_spreadsheet(Spreadsheet *sheet) {
    for (int i = 0; i < MAX_ROWS; i++) {
        for (int j = 0; j < MAX_COLS; j++) {
            sheet->cells[i][j].type = CELL_EMPTY;
            sheet->cells[i][j].raw_content[0] = '\0';
            sheet->cells[i][j].string_value[0] = '\0';
            sheet->cells[i][j].formula[0] = '\0';
            sheet->cells[i][j].numeric_value = 0.0;
        }
    }
    sheet->cursor_row = 0;
    sheet->cursor_col = 0;
}

// Display the spreadsheet grid
void display_spreadsheet(Spreadsheet *sheet) {
    clear_screen();
    
    printf("\n  SIMPLE SPREADSHEET v1.0\n");
    printf("  Commands: Arrow keys (move), E (edit), S (save), L (load), Q (quit)\n\n");
    
    // Column headers
    printf("     ");
    for (int col = 0; col < 10; col++) {
        printf("     %c      ", 'A' + col);
    }
    printf("\n");
    
    printf("   +");
    for (int col = 0; col < 10; col++) {
        printf("-----------+");
    }
    printf("\n");
    
    // Rows with data
    for (int row = 0; row < 20; row++) {
        printf("%3d|", row + 1);
        
        for (int col = 0; col < 10; col++) {
            char display[12];
            display[0] = '\0';
            
            Cell *cell = &sheet->cells[row][col];
            
            // Highlight cursor position
            if (row == sheet->cursor_row && col == sheet->cursor_col) {
                printf("\033[7m"); // Reverse video
            }
            
            switch (cell->type) {
                case CELL_EMPTY:
                    sprintf(display, "           ");
                    break;
                case CELL_NUMBER:
                    sprintf(display, "%10.2f ", cell->numeric_value);
                    break;
                case CELL_STRING:
                    snprintf(display, 12, "%-11s", cell->string_value);
                    break;
                case CELL_FORMULA:
                    sprintf(display, "%10.2f ", cell->numeric_value);
                    break;
            }
            
            printf("%s", display);
            
            if (row == sheet->cursor_row && col == sheet->cursor_col) {
                printf("\033[0m"); // Reset
            }
            printf("|");
        }
        printf("\n");
        
        printf("   +");
        for (int col = 0; col < 10; col++) {
            printf("-----------+");
        }
        printf("\n");
    }
    
    // Display current cell info
    Cell *current = &sheet->cells[sheet->cursor_row][sheet->cursor_col];
    printf("\n  Cell %c%d: ", 'A' + sheet->cursor_col, sheet->cursor_row + 1);
    if (current->type == CELL_FORMULA) {
        printf("%s (=%g)", current->formula, current->numeric_value);
    } else if (current->type != CELL_EMPTY) {
        printf("%s", current->raw_content);
    }
    printf("\n");
}

// Parse cell reference (e.g., "A1" -> row=0, col=0)
int parse_cell_reference(const char *ref, int *row, int *col) {
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

// Evaluate cell reference
double evaluate_cell_reference(Spreadsheet *sheet, const char *ref) {
    int row, col;
    if (parse_cell_reference(ref, &row, &col)) {
        Cell *cell = &sheet->cells[row][col];
        if (cell->type == CELL_NUMBER || cell->type == CELL_FORMULA) {
            return cell->numeric_value;
        }
    }
    return 0.0;
}

// Simple expression evaluator
double evaluate_expression(Spreadsheet *sheet, const char *expr) {
    char expr_copy[MAX_FORMULA_SIZE];
    strcpy(expr_copy, expr);
    
    // Handle SUM function
    if (strncmp(expr_copy, "SUM(", 4) == 0) {
        char *start = expr_copy + 4;
        char *colon = strchr(start, ':');
        if (colon) {
            *colon = '\0';
            char *end = colon + 1;
            char *paren = strchr(end, ')');
            if (paren) *paren = '\0';
            
            int start_row, start_col, end_row, end_col;
            if (parse_cell_reference(start, &start_row, &start_col) &&
                parse_cell_reference(end, &end_row, &end_col)) {
                
                double sum = 0.0;
                for (int r = start_row; r <= end_row; r++) {
                    for (int c = start_col; c <= end_col; c++) {
                        Cell *cell = &sheet->cells[r][c];
                        if (cell->type == CELL_NUMBER || cell->type == CELL_FORMULA) {
                            sum += cell->numeric_value;
                        }
                    }
                }
                return sum;
            }
        }
    }
    
    // Handle basic arithmetic
    double result = 0.0;
    char *token = strtok(expr_copy, "+-*/");
    if (token) {
        // Check if it's a cell reference or number
        if (isalpha(token[0])) {
            result = evaluate_cell_reference(sheet, token);
        } else {
            result = atof(token);
        }
    }
    
    // Find operator and second operand
    char *op_ptr = strpbrk(expr, "+-*/");
    if (op_ptr) {
        char op = *op_ptr;
        char *second = op_ptr + 1;
        double second_val;
        
        if (isalpha(second[0])) {
            second_val = evaluate_cell_reference(sheet, second);
        } else {
            second_val = atof(second);
        }
        
        switch (op) {
            case '+': result += second_val; break;
            case '-': result -= second_val; break;
            case '*': result *= second_val; break;
            case '/': 
                if (second_val != 0) result /= second_val;
                break;
        }
    }
    
    return result;
}

// Evaluate formula
double evaluate_formula(Spreadsheet *sheet, const char *formula) {
    if (formula[0] != '=') return 0.0;
    return evaluate_expression(sheet, formula + 1);
}

// Recalculate all formulas
void recalculate_formulas(Spreadsheet *sheet) {
    for (int i = 0; i < MAX_ROWS; i++) {
        for (int j = 0; j < MAX_COLS; j++) {
            if (sheet->cells[i][j].type == CELL_FORMULA) {
                sheet->cells[i][j].numeric_value = 
                    evaluate_formula(sheet, sheet->cells[i][j].formula);
            }
        }
    }
}

// Edit cell
void edit_cell(Spreadsheet *sheet) {
    char input[MAX_CELL_SIZE];
    printf("\n  Enter value (or formula starting with =): ");
    
    if (fgets(input, MAX_CELL_SIZE, stdin)) {
        // Remove newline
        input[strcspn(input, "\n")] = '\0';
        
        Cell *cell = &sheet->cells[sheet->cursor_row][sheet->cursor_col];
        
        if (strlen(input) == 0) {
            cell->type = CELL_EMPTY;
            cell->raw_content[0] = '\0';
        } else if (input[0] == '=') {
            // Formula
            cell->type = CELL_FORMULA;
            strcpy(cell->formula, input);
            strcpy(cell->raw_content, input);
            cell->numeric_value = evaluate_formula(sheet, input);
        } else if (isdigit(input[0]) || input[0] == '-' || input[0] == '.') {
            // Number
            cell->type = CELL_NUMBER;
            strcpy(cell->raw_content, input);
            cell->numeric_value = atof(input);
        } else {
            // String
            cell->type = CELL_STRING;
            strcpy(cell->raw_content, input);
            strcpy(cell->string_value, input);
        }
        
        recalculate_formulas(sheet);
    }
}

// Move cursor
void move_cursor(Spreadsheet *sheet, char direction) {
    switch (direction) {
        case 'w': // Up
            if (sheet->cursor_row > 0) sheet->cursor_row--;
            break;
        case 's': // Down
            if (sheet->cursor_row < MAX_ROWS - 1) sheet->cursor_row++;
            break;
        case 'a': // Left
            if (sheet->cursor_col > 0) sheet->cursor_col--;
            break;
        case 'd': // Right
            if (sheet->cursor_col < MAX_COLS - 1) sheet->cursor_col++;
            break;
    }
}

// Save spreadsheet
void save_spreadsheet(Spreadsheet *sheet, const char *filename) {
    FILE *file = fopen(filename, "w");
    if (!file) {
        printf("Error: Cannot save file\n");
        return;
    }
    
    for (int i = 0; i < MAX_ROWS; i++) {
        for (int j = 0; j < MAX_COLS; j++) {
            Cell *cell = &sheet->cells[i][j];
            if (cell->type != CELL_EMPTY) {
                fprintf(file, "%d,%d,%d,%s\n", i, j, cell->type, cell->raw_content);
            }
        }
    }
    
    fclose(file);
    printf("Spreadsheet saved to %s\n", filename);
}

// Load spreadsheet
void load_spreadsheet(Spreadsheet *sheet, const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        printf("Error: Cannot open file\n");
        return;
    }
    
    init_spreadsheet(sheet);
    
    char line[MAX_CELL_SIZE + 50];
    while (fgets(line, sizeof(line), file)) {
        int row, col, type;
        char content[MAX_CELL_SIZE];
        
        if (sscanf(line, "%d,%d,%d,%[^\n]", &row, &col, &type, content) == 4) {
            Cell *cell = &sheet->cells[row][col];
            cell->type = type;
            strcpy(cell->raw_content, content);
            
            switch (type) {
                case CELL_NUMBER:
                    cell->numeric_value = atof(content);
                    break;
                case CELL_STRING:
                    strcpy(cell->string_value, content);
                    break;
                case CELL_FORMULA:
                    strcpy(cell->formula, content);
                    break;
            }
        }
    }
    
    fclose(file);
    recalculate_formulas(sheet);
    printf("Spreadsheet loaded from %s\n", filename);
}

int main() {
    Spreadsheet sheet;
    init_spreadsheet(&sheet);
    
    char command;
    char filename[256];
    
    while (1) {
        display_spreadsheet(&sheet);
        
        printf("\n  Command: ");
        command = getchar();
        while (getchar() != '\n'); // Clear buffer
        
        switch (tolower(command)) {
            case 'w':
            case 'a':
            case 's':
            case 'd':
                move_cursor(&sheet, tolower(command));
                break;
            case 'e':
                edit_cell(&sheet);
                break;
            case 'v': // Save
                printf("  Filename: ");
                if (fgets(filename, sizeof(filename), stdin)) {
                    filename[strcspn(filename, "\n")] = '\0';
                    save_spreadsheet(&sheet, filename);
                    printf("  Press Enter to continue...");
                    getchar();
                }
                break;
            case 'l': // Load
                printf("  Filename: ");
                if (fgets(filename, sizeof(filename), stdin)) {
                    filename[strcspn(filename, "\n")] = '\0';
                    load_spreadsheet(&sheet, filename);
                    printf("  Press Enter to continue...");
                    getchar();
                }
                break;
            case 'q':
                printf("  Goodbye!\n");
                return 0;
            default:
                break;
        }
    }
    
    return 0;
}
