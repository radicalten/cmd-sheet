/******************************************************************************
 * Simple-Sheet: A bare-bones, single-file spreadsheet program in C.
 *
 * Features:
 * - 10x10 grid (A-J, 0-9)
 * - Cells hold integers or formulas.
 * - Formulas support: +, -, *, /
 * - Formula support: SUM(range), e.g., SUM(A0:A9)
 * - Automatic recalculation on every edit.
 * - No external dependencies.
 *
 * Author: A helpful AI
 * Compile: gcc -o spreadsheet spreadsheet.c
 * Run: ./spreadsheet
 ******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// --- Configuration ---
#define ROWS 10
#define COLS 10
#define MAX_CELL_TEXT 256
#define MAX_INPUT_BUFFER 512
#define RECALC_PASSES 10 // How many times to iterate to resolve dependencies

// --- Data Structures ---

// Represents a single cell in the spreadsheet
typedef struct {
    char text[MAX_CELL_TEXT]; // The raw text entered by the user (e.g., "123", "=A0+B0")
    int value;                // The calculated numeric value of the cell
    int is_formula;           // Flag: 1 if the cell contains a formula, 0 otherwise
    int error;                // Flag for errors like #DIV/0! or #REF!
} Cell;

// The global grid for our spreadsheet
Cell grid[ROWS][COLS];

// --- Function Prototypes ---

// Core Logic
void init_grid();
void cleanup();
void recalculate_all();
int evaluate_expression(const char* expr);

// User Interaction & Display
void display_grid();
void process_input(char* input);
void print_help();

// Helper/Utility Functions
int parse_cell_ref(const char* ref, int* row, int* col);
int get_cell_value_by_ref(const char* ref);
void trim_whitespace(char* str);

// --- Main Function ---

int main() {
    char input_buffer[MAX_INPUT_BUFFER];

    init_grid();
    print_help();

    while (1) {
        display_grid();
        printf("> ");
        
        if (fgets(input_buffer, sizeof(input_buffer), stdin) == NULL) {
            break; // End on EOF
        }

        trim_whitespace(input_buffer);

        if (strcmp(input_buffer, "quit") == 0 || strcmp(input_buffer, "exit") == 0) {
            break;
        } else if (strcmp(input_buffer, "help") == 0) {
            print_help();
        } else if (strlen(input_buffer) > 0) {
            process_input(input_buffer);
            recalculate_all();
        }
    }
    
    cleanup();
    printf("Exiting Simple-Sheet. Goodbye!\n");
    return 0;
}


// --- Core Logic Implementation ---

/**
 * @brief Initializes the grid, setting all cells to a default empty state.
 */
void init_grid() {
    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            strcpy(grid[r][c].text, "");
            grid[r][c].value = 0;
            grid[r][c].is_formula = 0;
            grid[r][c].error = 0;
        }
    }
}

/**
 * @brief Frees any resources allocated during runtime. (Currently none, but good practice).
 */
void cleanup() {
    // In a version with dynamic memory (e.g., malloc'd strings),
    // this function would free that memory.
}

/**
 * @brief Recalculates the values of all formula cells in the grid.
 * It iterates multiple times to allow for dependency chains (e.g., C1=B1, B1=A1).
 */
void recalculate_all() {
    for (int pass = 0; pass < RECALC_PASSES; pass++) {
        for (int r = 0; r < ROWS; r++) {
            for (int c = 0; c < COLS; c++) {
                if (grid[r][c].is_formula) {
                    grid[r][c].value = evaluate_expression(grid[r][c].text);
                }
            }
        }
    }
}

/**
 * @brief Parses and evaluates a formula expression from a cell's text.
 * @param expr The expression string, starting with '='.
 * @return The calculated integer value.
 */
int evaluate_expression(const char* expr) {
    // Skip the leading '='
    const char* p = expr + 1;
    char term1_str[MAX_CELL_TEXT], term2_str[MAX_CELL_TEXT];
    char op = 0;
    int val1, val2;

    // --- Check for SUM(A0:B9) function ---
    if (strncmp(p, "SUM(", 4) == 0) {
        char start_ref_str[16], end_ref_str[16];
        if (sscanf(p, "SUM(%[^:]:%[^)])", start_ref_str, end_ref_str) == 2) {
            int start_r, start_c, end_r, end_c;
            if (parse_cell_ref(start_ref_str, &start_r, &start_c) && parse_cell_ref(end_ref_str, &end_r, &end_c)) {
                int sum = 0;
                // Ensure the loop range is valid
                int r_min = (start_r < end_r) ? start_r : end_r;
                int r_max = (start_r > end_r) ? start_r : end_r;
                int c_min = (start_c < end_c) ? start_c : end_c;
                int c_max = (start_c > end_c) ? start_c : end_c;

                for (int r = r_min; r <= r_max; r++) {
                    for (int c = c_min; c <= c_max; c++) {
                        sum += grid[r][c].value;
                    }
                }
                return sum;
            }
        }
        return 0; // #REF! error if parsing fails
    }

    // --- Check for simple arithmetic: operand operator operand ---
    const char* op_ptr = strpbrk(p, "+-*/");

    if (op_ptr) {
        op = *op_ptr;
        
        // Copy term1
        strncpy(term1_str, p, op_ptr - p);
        term1_str[op_ptr - p] = '\0';
        trim_whitespace(term1_str);
        
        // Copy term2
        strcpy(term2_str, op_ptr + 1);
        trim_whitespace(term2_str);

        val1 = get_cell_value_by_ref(term1_str);
        val2 = get_cell_value_by_ref(term2_str);
        
        switch (op) {
            case '+': return val1 + val2;
            case '-': return val1 - val2;
            case '*': return val1 * val2;
            case '/': 
                if (val2 == 0) return 0; // #DIV/0! error state
                return val1 / val2;
        }
    }
    
    // --- If no operator, it's a single reference or a constant ---
    return get_cell_value_by_ref(p);
}


// --- User Interaction & Display Implementation ---

/**
 * @brief Displays the current state of the grid to the console.
 */
void display_grid() {
    printf("\n");
    // Print column headers
    printf("      ");
    for (int c = 0; c < COLS; c++) {
        printf("%-8c", 'A' + c);
    }
    printf("\n------");
    for (int c = 0; c < COLS; c++) {
        printf("---------");
    }
    printf("\n");

    // Print rows
    for (int r = 0; r < ROWS; r++) {
        printf("%-5d|", r);
        for (int c = 0; c < COLS; c++) {
            // If cell contains text that isn't a simple number or formula, show "TEXT"
            if (!grid[r][c].is_formula && isalpha(grid[r][c].text[0])) {
                 printf("%-8s", "TEXT");
            } else {
                 printf("%-8d", grid[r][c].value);
            }
        }
        printf("\n");
    }
    printf("\n");
}

/**
 * @brief Processes a line of user input, e.g., "A1 = 123" or "B2 = A0 + A1".
 * @param input The raw input string from the user.
 */
void process_input(char* input) {
    char cell_ref_str[16];
    char* expression_str;
    
    char* equals_ptr = strchr(input, '=');
    if (equals_ptr == NULL) {
        printf("ERROR: Invalid format. Use 'CELL = value/formula'. Example: A1 = 100\n");
        return;
    }

    // Isolate the cell reference part
    *equals_ptr = '\0'; // Temporarily terminate the string at '='
    strcpy(cell_ref_str, input);
    trim_whitespace(cell_ref_str);
    
    // The rest of the string is the expression
    expression_str = equals_ptr + 1;
    trim_whitespace(expression_str);

    int r, c;
    if (!parse_cell_ref(cell_ref_str, &r, &c)) {
        printf("ERROR: Invalid cell reference '%s'. Use A-J and 0-9.\n", cell_ref_str);
        return;
    }

    // Store the raw text in the cell
    strncpy(grid[r][c].text, expression_str, MAX_CELL_TEXT - 1);
    grid[r][c].text[MAX_CELL_TEXT-1] = '\0';

    // Check if it's a formula or a direct value
    if (expression_str[0] == '=') {
        grid[r][c].is_formula = 1;
        // The value will be calculated by recalculate_all()
    } else {
        grid[r][c].is_formula = 0;
        // If not a formula, its value is just its integer conversion
        grid[r][c].value = atoi(expression_str);
    }
}

/**
 * @brief Prints the help message with instructions.
 */
void print_help() {
    printf("\n--- Simple-Sheet Help ---\n");
    printf("Commands:\n");
    printf("  CELL = value       Set a numeric value (e.g., A1 = 123)\n");
    printf("  CELL = \"text\"    Set a text value (e.g., B2 = \"hello\")\n");
    printf("  CELL = formula     Set a formula (e.g., C3 = A1+B1)\n");
    printf("  Formulas start with '='.\n");
    printf("  Supported operators: +, -, *, /\n");
    printf("  Supported functions: SUM(range), e.g., D4 = SUM(A0:C0)\n");
    printf("\n");
    printf("  help               Show this help message\n");
    printf("  quit or exit       Exit the program\n");
    printf("-------------------------\n");
}


// --- Helper/Utility Function Implementation ---

/**
 * @brief Parses a cell reference string like "A1" or "J9" into row/col indices.
 * @param ref The string to parse.
 * @param row Pointer to store the resulting row index.
 * @param col Pointer to store the resulting column index.
 * @return 1 on success, 0 on failure (invalid format or out of bounds).
 */
int parse_cell_ref(const char* ref, int* row, int* col) {
    if (ref == NULL || strlen(ref) < 2) return 0;
    
    char c_ref = toupper(ref[0]);
    int r_ref = atoi(&ref[1]);

    if (c_ref < 'A' || c_ref > ('A' + COLS - 1)) return 0;
    if (r_ref < 0 || r_ref >= ROWS) return 0;
    
    *col = c_ref - 'A';
    *row = r_ref;
    
    return 1;
}

/**
 * @brief Gets the value of an operand, which could be a cell reference or a number.
 * @param ref The string representing the operand (e.g., "A1", "50").
 * @return The integer value.
 */
int get_cell_value_by_ref(const char* ref) {
    int r, c;
    // If it's a valid cell reference, return that cell's value
    if (parse_cell_ref(ref, &r, &c)) {
        return grid[r][c].value;
    }
    // Otherwise, assume it's a literal number and convert it
    return atoi(ref);
}

/**
 * @brief Removes leading and trailing whitespace from a string, in-place.
 * @param str The string to trim.
 */
void trim_whitespace(char* str) {
    if (str == NULL) return;
    char* end;
    
    // Trim leading space
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return; // All spaces

    // Trim trailing space
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    
    // Write new null terminator
    *(end + 1) = 0;
}
