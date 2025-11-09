/******************************************************************************
 * tcalc.c - A Simple Terminal-Based Spreadsheet Program
 *
 * Author: A helpful AI assistant
 * Date: 2023
 *
 * A single-file, dependency-free C program that mimics the basic
 * functionality of early spreadsheet software like VisiCalc in the terminal.
 *
 * Features:
 * - A grid of cells with addresses (A1, B2, etc.).
 * - Navigation with 'w', 'a', 's', 'd'.
 * - Data entry for text, numbers, and formulas.
 * - Formulas start with '=' and support one binary operator (+, -, *, /).
 *   Examples: =A1+B2, =C1*10, =50/2
 * - Automatic recalculation of all cells.
 * - Circular dependency detection.
 * - Error reporting for syntax, value, division by zero, and circular refs.
 * - Quit with 'q' or 'quit'.
 *
 * Compilation:
 *   gcc -o tcalc tcalc.c -lm
 *
 * Usage:
 *   ./tcalc
 *
 *****************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

// --- Configuration ---
#define COLS 8
#define ROWS 15
#define CELL_WIDTH 10
#define INPUT_BUFFER_SIZE 256
#define MAX_EXPR_LEN (INPUT_BUFFER_SIZE - 1)

// --- Error Types ---
#define NO_ERROR 0
#define ERROR_SYNTAX 1
#define ERROR_CIRCULAR_REFERENCE 2
#_define ERROR_BAD_REFERENCE 3 // Currently folded into SYNTAX
#define ERROR_DIV_BY_ZERO 4
#define ERROR_VALUE 5 // e.g., trying to do math on text

// --- Cell Structure ---
typedef struct Cell {
    char expression[MAX_EXPR_LEN + 1]; // What the user typed, e.g., "=A1+B2"
    char display[CELL_WIDTH + 1];      // What is shown in the grid
    double value;                      // The numeric value if it's a number/formula
    int error;                         // Error state of the cell
    int needs_recalc;                  // Flag to trigger recalculation
} Cell;

// --- Global State ---
Cell grid[ROWS][COLS];
int currentRow = 0;
int currentCol = 0;
// For circular reference detection during a single evaluation pass
int evaluation_path[ROWS][COLS];

// --- Forward Declarations ---
void initializeGrid();
void clearScreen();
void drawGrid();
void handleInput();
void updateAllCells();
void evaluateCell(int r, int c);
double parseExpression(const char* expr, int* error_code);
double getCellValue(int r, int c, int* error_code);

// --- Main Program Loop ---
int main() {
    initializeGrid();
    int running = 1;

    while (running) {
        // Core loop: update, draw, get input
        updateAllCells();
        clearScreen();
        drawGrid();
        
        // Get user input and check if we should quit
        printf("Enter command or expression for %c%d: %s\n> ", 
               'A' + currentCol, currentRow + 1, grid[currentRow][currentCol].expression);
        
        char input[INPUT_BUFFER_SIZE];
        if (fgets(input, sizeof(input), stdin) == NULL) {
            // EOF or error, quit gracefully
            break;
        }

        // Remove trailing newline
        input[strcspn(input, "\n")] = 0;

        if (strcmp(input, "q") == 0 || strcmp(input, "quit") == 0) {
            running = 0;
        } else {
            handleInput(input);
        }
    }

    clearScreen();
    printf("Exiting tcalc. Goodbye!\n");
    return 0;
}

// --- Initialization ---
void initializeGrid() {
    for (int r = 0; r < ROWS; ++r) {
        for (int c = 0; c < COLS; ++c) {
            strcpy(grid[r][c].expression, "");
            strcpy(grid[r][c].display, "");
            grid[r][c].value = 0.0;
            grid[r][c].error = NO_ERROR;
            grid[r][c].needs_recalc = 1; // Mark all for initial calculation
        }
    }
}

// --- Screen and Drawing ---
void clearScreen() {
    // Basic ANSI escape codes to clear screen and move cursor to top-left
    printf("\033[H\033[J");
}

void drawGrid() {
    // Print header
    printf("tcalc - A Simple Terminal Spreadsheet\n");
    printf("Use w/a/s/d to navigate, q to quit.\n\n");
    
    // Print column letters
    printf("%*s", CELL_WIDTH, "");
    for (int c = 0; c < COLS; ++c) {
        printf("|%*c%*s", CELL_WIDTH/2, 'A' + c, (CELL_WIDTH-1)/2, "");
    }
    printf("|\n");

    // Print separator line
    printf("%*s", CELL_WIDTH, "");
    for (int c = 0; c < COLS; ++c) {
        printf("+");
        for(int i=0; i<CELL_WIDTH; ++i) printf("-");
    }
    printf("+\n");

    // Print rows
    for (int r = 0; r < ROWS; ++r) {
        // Print row number
        printf("%*d ", CELL_WIDTH - 2, r + 1);

        // Print cell contents
        for (int c = 0; c < COLS; ++c) {
            char cell_content[CELL_WIDTH + 1];

            // Truncate display string if it's too long
            strncpy(cell_content, grid[r][c].display, CELL_WIDTH);
            cell_content[CELL_WIDTH] = '\0';
            
            // Highlight current cell
            if (r == currentRow && c == currentCol) {
                printf("|[%-*s]", CELL_WIDTH - 2, cell_content);
            } else {
                printf("| %-*s ", CELL_WIDTH - 2, cell_content);
            }
        }
        printf("|\n");
    }
}

// --- Input Handling ---
void handleInput(const char* input) {
    if (strlen(input) == 1) { // Check for navigation commands
        switch (input[0]) {
            case 'w': if (currentRow > 0) currentRow--; return;
            case 's': if (currentRow < ROWS - 1) currentRow++; return;
            case 'a': if (currentCol > 0) currentCol--; return;
            case 'd': if (currentCol < COLS - 1) currentCol++; return;
        }
    }

    // If not a navigation command, it's an expression for the current cell
    strncpy(grid[currentRow][currentCol].expression, input, MAX_EXPR_LEN);
    grid[currentRow][currentCol].expression[MAX_EXPR_LEN] = '\0';

    // Mark all cells for recalculation. A more optimized approach would use
    // a dependency graph, but this is simpler and sufficient for this scale.
    for (int r = 0; r < ROWS; ++r) {
        for (int c = 0; c < COLS; ++c) {
            grid[r][c].needs_recalc = 1;
        }
    }
}


// --- Calculation Engine ---

// Recalculates all cells that are marked as needing it
void updateAllCells() {
    for (int r = 0; r < ROWS; ++r) {
        for (int c = 0; c < COLS; ++c) {
            if (grid[r][c].needs_recalc) {
                // Clear the evaluation path for this top-level calculation
                memset(evaluation_path, 0, sizeof(evaluation_path));
                evaluateCell(r, c);
            }
        }
    }
}

// Evaluates a single cell and updates its display and value
void evaluateCell(int r, int c) {
    Cell* cell = &grid[r][c];
    cell->error = NO_ERROR;
    
    if (cell->expression[0] == '\0') {
        // Empty cell
        strcpy(cell->display, "");
        cell->value = 0.0;
    } else if (cell->expression[0] == '=') {
        // Formula
        int error_code = NO_ERROR;
        // Mark this cell as being part of the current evaluation path
        evaluation_path[r][c] = 1;
        cell->value = parseExpression(cell->expression + 1, &error_code);
        // Unmark it after evaluation
        evaluation_path[r][c] = 0;
        
        cell->error = error_code;
        if (error_code == NO_ERROR) {
            snprintf(cell->display, CELL_WIDTH + 1, "%.2f", cell->value);
        } else {
            // Set display to error message
            switch(error_code) {
                case ERROR_SYNTAX: strncpy(cell->display, "#SYNTAX!", CELL_WIDTH); break;
                case ERROR_CIRCULAR_REFERENCE: strncpy(cell->display, "#REF!", CELL_WIDTH); break;
                case ERROR_DIV_BY_ZERO: strncpy(cell->display, "#DIV/0!", CELL_WIDTH); break;
                case ERROR_VALUE: strncpy(cell->display, "#VALUE!", CELL_WIDTH); break;
                default: strncpy(cell->display, "#ERROR!", CELL_WIDTH); break;
            }
        }

    } else {
        // Literal (text or number)
        char* endptr;
        double val = strtod(cell->expression, &endptr);
        if (*endptr == '\0' || isspace(*endptr)) {
            // It's a number
            cell->value = val;
            snprintf(cell->display, CELL_WIDTH + 1, "%.2f", cell->value);
        } else {
            // It's text
            cell->value = 0.0; // Text cells have a numeric value of 0
            cell->error = ERROR_VALUE; // Not an error to display, but an error if used in math
            strncpy(cell->display, cell->expression, CELL_WIDTH);
        }
    }
    cell->display[CELL_WIDTH] = '\0'; // Ensure null-termination
    cell->needs_recalc = 0; // This cell is now up-to-date
}

// A very simple parser for "operand operator operand" format
double parseExpression(const char* expr, int* error_code) {
    char operand1_str[MAX_EXPR_LEN];
    char operand2_str[MAX_EXPR_LEN];
    char op;
    double operand1_val, operand2_val;

    const char* p = expr;
    int i = 0;
    
    // Skip leading whitespace
    while (*p && isspace(*p)) p++;

    // Parse operand 1
    while (*p && !strchr("+-*/", *p)) {
        if (!isspace(*p)) operand1_str[i++] = *p;
        p++;
    }
    operand1_str[i] = '\0';
    if (strlen(operand1_str) == 0) { *error_code = ERROR_SYNTAX; return 0; }

    // Parse operator
    while (*p && isspace(*p)) p++;
    if (!*p) { // Only one operand, treat it as a value
        op = 0; 
    } else {
        op = *p;
        p++;
    }

    if(op == 0) { // Single operand case (e.g., "=A1" or "=123")
        i = 0;
        operand2_str[0] = '\0';
    } else { // Two operands case
        // Parse operand 2
        while (*p && isspace(*p)) p++;
        i = 0;
        while (*p) {
            if (!isspace(*p)) operand2_str[i++] = *p;
            p++;
        }
        operand2_str[i] = '\0';
        if (strlen(operand2_str) == 0) { *error_code = ERROR_SYNTAX; return 0; }
    }


    // Evaluate operand 1
    if (isalpha(operand1_str[0])) { // Cell reference
        int c = toupper(operand1_str[0]) - 'A';
        int r = atoi(operand1_str + 1) - 1;
        operand1_val = getCellValue(r, c, error_code);
        if (*error_code != NO_ERROR) return 0;
    } else { // Number literal
        char* endptr;
        operand1_val = strtod(operand1_str, &endptr);
        if (*endptr != '\0') { *error_code = ERROR_SYNTAX; return 0; }
    }
    
    // If there's no second operand, just return the first
    if(op == 0) return operand1_val;

    // Evaluate operand 2
    if (isalpha(operand2_str[0])) { // Cell reference
        int c = toupper(operand2_str[0]) - 'A';
        int r = atoi(operand2_str + 1) - 1;
        operand2_val = getCellValue(r, c, error_code);
        if (*error_code != NO_ERROR) return 0;
    } else { // Number literal
        char* endptr;
        operand2_val = strtod(operand2_str, &endptr);
        if (*endptr != '\0') { *error_code = ERROR_SYNTAX; return 0; }
    }

    // Perform operation
    switch (op) {
        case '+': return operand1_val + operand2_val;
        case '-': return operand1_val - operand2_val;
        case '*': return operand1_val * operand2_val;
        case '/':
            if (fabs(operand2_val) < 1e-9) {
                *error_code = ERROR_DIV_BY_ZERO;
                return 0;
            }
            return operand1_val / operand2_val;
        default:
            *error_code = ERROR_SYNTAX;
            return 0;
    }
}

// Safely gets the value of another cell, handling errors and recursion
double getCellValue(int r, int c, int* error_code) {
    // Bounds check
    if (r < 0 || r >= ROWS || c < 0 || c >= COLS) {
        *error_code = ERROR_SYNTAX; // Bad reference
        return 0;
    }

    // Circular reference check
    if (evaluation_path[r][c]) {
        *error_code = ERROR_CIRCULAR_REFERENCE;
        return 0;
    }

    // If the referenced cell has not been calculated in this pass, calculate it now
    if (grid[r][c].needs_recalc) {
        evaluateCell(r, c);
    }
    
    // Propagate errors from the referenced cell
    if (grid[r][c].error != NO_ERROR && grid[r][c].error != ERROR_VALUE) {
        *error_code = grid[r][c].error;
        return 0;
    }
    if (grid[r][c].error == ERROR_VALUE) { // Trying to use a text cell in math
        *error_code = ERROR_VALUE;
        return 0;
    }


    return grid[r][c].value;
}
