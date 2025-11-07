#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#define MAX_ROWS 100
#define MAX_COLS 26
#define CELL_WIDTH 12
#define MAX_EXPR_LEN 256
#define MAX_CELL_LEN 1024

typedef enum { EMPTY, NUMBER, TEXT, FORMULA } CellType;

typedef struct {
    CellType type;
    double number;
    char text[MAX_CELL_LEN];
    char formula[MAX_EXPR_LEN];
} Cell;

Cell sheet[MAX_ROWS][MAX_COLS];
int cursor_row = 0, cursor_col = 0;

// Function prototypes
void display_sheet();
void handle_input();
int evaluate_formula(char *expr, double *result);
double get_cell_value(int row, int col);
void set_cell_value(int row, int col, char *value);
void clear_sheet();
char *trim(char *str);
int parse_cell_ref(char *ref, int *row, int *col);

int main() {
    printf("Terminal Spreadsheet - Commands:\n");
    printf("Arrow keys: Move cursor\n");
    printf("Enter: Edit cell\n");
    printf("c: Clear sheet\n");
    printf("q: Quit\n\n");
    
    clear_sheet();
    display_sheet();
    handle_input();
    
    return 0;
}

void clear_sheet() {
    for (int i = 0; i < MAX_ROWS; i++) {
        for (int j = 0; j < MAX_COLS; j++) {
            sheet[i][j].type = EMPTY;
            sheet[i][j].number = 0;
            sheet[i][j].text[0] = '\0';
            sheet[i][j].formula[0] = '\0';
        }
    }
}

void display_sheet() {
    // Clear screen (ANSI escape code)
    printf("\033[2J\033[H");
    
    // Print column headers
    printf("   ");
    for (int j = 0; j < MAX_COLS; j++) {
        printf("%*c", CELL_WIDTH, 'A' + j);
    }
    printf("\n");
    
    // Print rows
    for (int i = 0; i < MAX_ROWS; i++) {
        printf("%2d ", i + 1);
        for (int j = 0; j < MAX_COLS; j++) {
            char display[CELL_WIDTH + 1];
            switch (sheet[i][j].type) {
                case NUMBER:
                    snprintf(display, CELL_WIDTH, "%.2f", sheet[i][j].number);
                    break;
                case TEXT:
                    snprintf(display, CELL_WIDTH, "%s", sheet[i][j].text);
                    break;
                case FORMULA:
                    snprintf(display, CELL_WIDTH, "=%s", sheet[i][j].formula);
                    break;
                default:
                    snprintf(display, CELL_WIDTH, "");
                    break;
            }
            
            // Highlight cursor position
            if (i == cursor_row && j == cursor_col) {
                printf("\033[7m%*.*s\033[0m", CELL_WIDTH, CELL_WIDTH - 1, display);
            } else {
                printf("%*.*s", CELL_WIDTH, CELL_WIDTH - 1, display);
            }
        }
        printf("\n");
    }
    
    printf("\nCurrent cell: %c%d\n", 'A' + cursor_col, cursor_row + 1);
    printf("Enter command or cell content: ");
    fflush(stdout);
}

void handle_input() {
    char input[MAX_CELL_LEN];
    
    while (1) {
        if (fgets(input, sizeof(input), stdin)) {
            // Remove newline
            input[strcspn(input, "\n")] = 0;
            
            // Handle commands
            if (strcmp(input, "q") == 0) {
                break;
            } else if (strcmp(input, "c") == 0) {
                clear_sheet();
            } else if (strlen(input) > 0) {
                // Set cell content
                set_cell_value(cursor_row, cursor_col, input);
            }
            
            display_sheet();
        }
    }
}

void set_cell_value(int row, int col, char *value) {
    if (row < 0 || row >= MAX_ROWS || col < 0 || col >= MAX_COLS) return;
    
    Cell *cell = &sheet[row][col];
    strcpy(cell->text, value);
    
    // Check if it's a formula
    if (value[0] == '=') {
        cell->type = FORMULA;
        strcpy(cell->formula, value + 1);
        double result;
        if (evaluate_formula(cell->formula, &result)) {
            cell->type = NUMBER;
            cell->number = result;
            snprintf(cell->text, MAX_CELL_LEN, "%.2f", result);
        }
    } 
    // Check if it's a number
    else if (sscanf(value, "%lf", &cell->number) == 1) {
        cell->type = NUMBER;
    } 
    // Otherwise treat as text
    else {
        cell->type = TEXT;
    }
}

int evaluate_formula(char *expr, double *result) {
    // Simple formula evaluator supporting:
    // - Basic arithmetic: +, -, *, /
    // - Cell references: A1, B2, etc.
    // - Parentheses
    
    char *p = trim(expr);
    double stack[100];
    char ops[100];
    int stack_top = -1, ops_top = -1;
    
    // Very basic implementation - handles only simple expressions
    // For a full implementation, you'd need a proper expression parser
    
    // Try to parse as a single number
    if (sscanf(p, "%lf", result) == 1) {
        return 1;
    }
    
    // Try to parse as a single cell reference
    int ref_row, ref_col;
    if (parse_cell_ref(p, &ref_row, &ref_col)) {
        *result = get_cell_value(ref_row, ref_col);
        return 1;
    }
    
    // For this simple version, we'll just return 0 (error)
    // A real implementation would parse the expression properly
    return 0;
}

double get_cell_value(int row, int col) {
    if (row < 0 || row >= MAX_ROWS || col < 0 || col >= MAX_COLS) return 0;
    
    Cell *cell = &sheet[row][col];
    switch (cell->type) {
        case NUMBER:
            return cell->number;
        case FORMULA: {
            double result;
            if (evaluate_formula(cell->formula, &result)) {
                return result;
            }
            return 0;
        }
        default:
            return 0;
    }
}

int parse_cell_ref(char *ref, int *row, int *col) {
    if (!ref || strlen(ref) < 2) return 0;
    
    // Parse column (letter)
    if (ref[0] < 'A' || ref[0] > 'Z') return 0;
    *col = ref[0] - 'A';
    
    // Parse row (number)
    char *end;
    *row = strtol(ref + 1, &end, 10) - 1;
    
    return (*row >= 0 && *row < MAX_ROWS && end != ref + 1);
}

char *trim(char *str) {
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;
    
    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    
    return str;
}
