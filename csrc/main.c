/******************************************************************************
 *                                                                            *
 *                                  sc.c                                      *
 *             A Simple, Single-File, Dependency-Free Spreadsheet             *
 *                                in C                                        *
 *                                                                            *
 *      This program is a proof-of-concept text-based spreadsheet that        *
 *      demonstrates the core principles of a calculation grid. It is         *
 *      not a replacement for modern spreadsheet software but serves as an     *
 *      educational example of what's possible within extreme constraints.    *
 *                                                                            *
 *      Features:                                                             *
 *      - Text-based interface running in the terminal.                       *
 *      - Grid of 26 columns (A-Z) and 99 rows (1-99).                        *
 *      - Cells can contain numbers, text, or simple formulas.                *
 *      - Simple formula support: =<cell1>+<cell2> (e.g., =A1+B2).            *
 *      - Automatic recalculation of the entire sheet on any change.          *
 *      - Basic circular reference detection to prevent infinite loops.       *
 *      - Save to and load from a CSV-like file.                              *
 *                                                                            *
 *      How to Compile and Run:                                               *
 *      $ gcc -o sc sc.c -lm                                                  *
 *      $ ./sc                                                                *
 *                                                                            *
 *      Usage within the program:                                             *
 *      - To set a value: `A1=123` or `B2=hello`                                *
 *      - To set a formula: `C3=A1+B1`                                          *
 *      - To see the sheet: `show`                                            *
 *      - To save: `save filename.csv`                                        *
 *      - To load: `load filename.csv`                                        *
 *      - To exit: `quit`                                                     *
 *                                                                            *
 ******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

// --- Configuration ---
#define COLS 26
#define ROWS 99
#define CELL_WIDTH 10
#define MAX_INPUT_LEN 256
#define MAX_FORMULA_DEPTH 10 // For circular reference detection

// --- Data Structures ---

// Enum to represent the type of data stored in a cell
typedef enum {
    EMPTY,
    NUMBER,
    TEXT,
    FORMULA
} CellType;

// The core structure for a single cell
typedef struct Cell {
    CellType type;
    char* raw_content;      // The user's input (e.g., "123", "hello", "=A1+B2")
    double value;           // The calculated numeric value if it's a number or formula
    char display_text[CELL_WIDTH + 1]; // Truncated text for display
} Cell;

// The global spreadsheet grid
Cell sheet[ROWS][COLS];

// --- Forward Declarations ---
void init_sheet();
void display_sheet();
void process_input(const char* input);
void evaluate_sheet();
void evaluate_cell(int row, int col, int depth);
int parse_cell_ref(const char* ref, int* row, int* col);
void set_cell_from_string(int row, int col, const char* input_str);
void save_sheet(const char* filename);
void load_sheet(const char* filename);
void free_sheet();

// --- Main Program Loop ---

int main() {
    char input_buffer[MAX_INPUT_LEN];
    int running = 1;

    init_sheet();
    printf("Welcome to sc - The Simple C Spreadsheet!\n");
    printf("Commands: <cell>=<value> (e.g., A1=100), <cell>=<formula> (e.g., C1=A1+B2)\n");
    printf("          'show', 'save <file>', 'load <file>', 'quit'\n\n");

    display_sheet();

    while (running) {
        printf("> ");
        if (fgets(input_buffer, MAX_INPUT_LEN, stdin) == NULL) {
            break; // End of input
        }

        // Remove trailing newline
        input_buffer[strcspn(input_buffer, "\n")] = 0;

        if (strcmp(input_buffer, "quit") == 0) {
            running = 0;
        } else if (strcmp(input_buffer, "show") == 0) {
            display_sheet();
        } else if (strncmp(input_buffer, "save ", 5) == 0) {
            save_sheet(input_buffer + 5);
        } else if (strncmp(input_buffer, "load ", 5) == 0) {
            load_sheet(input_buffer + 5);
            evaluate_sheet();
            display_sheet();
        } else {
            process_input(input_buffer);
            evaluate_sheet();
            display_sheet();
        }
    }

    free_sheet();
    printf("Goodbye!\n");
    return 0;
}

// --- Core Functions ---

// Initialize all cells to an empty state
void init_sheet() {
    for (int r = 0; r < ROWS; ++r) {
        for (int c = 0; c < COLS; ++c) {
            sheet[r][c].type = EMPTY;
            sheet[r][c].raw_content = NULL;
            sheet[r][c].value = 0.0;
            strcpy(sheet[r][c].display_text, "");
        }
    }
}

// Free all dynamically allocated memory
void free_sheet() {
    for (int r = 0; r < ROWS; ++r) {
        for (int c = 0; c < COLS; ++c) {
            if (sheet[r][c].raw_content != NULL) {
                free(sheet[r][c].raw_content);
                sheet[r][c].raw_content = NULL;
            }
        }
    }
}

// Display the entire sheet in a grid format
void display_sheet() {
    printf("\n");
    // Print column headers (A, B, C, ...)
    printf("%4s", "");
    for (int c = 0; c < COLS; ++c) {
        printf("| %-*c ", CELL_WIDTH - 2, 'A' + c);
    }
    printf("|\n");

    // Print separator line
    printf("%4s", "");
    for (int c = 0; c < COLS; ++c) {
        printf("+");
        for(int i = 0; i < CELL_WIDTH; ++i) printf("-");
    }
    printf("+\n");

    // Print rows
    for (int r = 0; r < ROWS; ++r) {
        printf("%3d ", r + 1);
        for (int c = 0; c < COLS; ++c) {
            printf("| %-*s ", CELL_WIDTH - 2, sheet[r][c].display_text);
        }
        printf("|\n");
    }
    printf("\n");
}


// Parse user input like "A1=123"
void process_input(const char* input) {
    char cell_ref_str[4]; // e.g., "A1", "Z99"
    int row, col;

    // Find the '=' separator
    const char* equals_pos = strchr(input, '=');
    if (equals_pos == NULL) {
        printf("Error: Invalid input format. Use 'Cell=Value'.\n");
        return;
    }

    int ref_len = equals_pos - input;
    if (ref_len > 3 || ref_len < 2) {
        printf("Error: Invalid cell reference.\n");
        return;
    }

    strncpy(cell_ref_str, input, ref_len);
    cell_ref_str[ref_len] = '\0';
    
    // Trim whitespace from reference
    // (A simple version could just assume no whitespace)

    if (!parse_cell_ref(cell_ref_str, &row, &col)) {
        printf("Error: Invalid cell reference '%s'.\n", cell_ref_str);
        return;
    }

    // The rest of the string is the value
    const char* value_str = equals_pos + 1;
    set_cell_from_string(row, col, value_str);
}


// Sets a cell's type and content from a string
void set_cell_from_string(int row, int col, const char* value_str) {
    if (row < 0 || row >= ROWS || col < 0 || col >= COLS) return;

    Cell* cell = &sheet[row][col];
    
    // Free old content if it exists
    if(cell->raw_content) {
        free(cell->raw_content);
    }
    cell->raw_content = strdup(value_str);
    if (cell->raw_content == NULL) {
        printf("Error: Out of memory!\n");
        exit(1);
    }

    // Determine cell type
    if (value_str[0] == '\0') {
        cell->type = EMPTY;
    } else if (value_str[0] == '=') {
        cell->type = FORMULA;
    } else {
        // Check if it's a number
        char* endptr;
        strtod(value_str, &endptr);
        if (*endptr == '\0') { // The whole string was a valid double
            cell->type = NUMBER;
        } else {
            cell->type = TEXT;
        }
    }
}

// --- Calculation Engine ---

// Iterate through all cells and evaluate them
void evaluate_sheet() {
    for (int r = 0; r < ROWS; ++r) {
        for (int c = 0; c < COLS; ++c) {
            evaluate_cell(r, c, 0); // Start evaluation with depth 0
        }
    }
}

// Evaluate a single cell (recursively for formulas)
void evaluate_cell(int row, int col, int depth) {
    if (row < 0 || row >= ROWS || col < 0 || col >= COLS) return;

    if (depth > MAX_FORMULA_DEPTH) {
        // Circular reference detected
        strcpy(sheet[row][col].display_text, "#CIRC!");
        return;
    }

    Cell* cell = &sheet[row][col];

    switch (cell->type) {
        case EMPTY:
            strcpy(cell->display_text, "");
            cell->value = 0.0;
            break;

        case NUMBER:
            cell->value = atof(cell->raw_content);
            snprintf(cell->display_text, CELL_WIDTH + 1, "%.2f", cell->value);
            break;

        case TEXT:
            strncpy(cell->display_text, cell->raw_content, CELL_WIDTH);
            cell->display_text[CELL_WIDTH] = '\0';
            cell->value = 0.0; // Text cells have a numeric value of 0
            break;

        case FORMULA: {
            // VERY simple parser: =<cell1>+<cell2>
            char ref1_str[4], ref2_str[4];
            int r1, c1, r2, c2;

            if (sscanf(cell->raw_content, "=%3[A-Z0-9]+%3[A-Z0-9]", ref1_str, ref2_str) == 2) {
                if (parse_cell_ref(ref1_str, &r1, &c1) && parse_cell_ref(ref2_str, &r2, &c2)) {
                    // Recursively evaluate dependencies
                    evaluate_cell(r1, c1, depth + 1);
                    evaluate_cell(r2, c2, depth + 1);

                    // Check if dependencies had errors
                    if(strcmp(sheet[r1][c1].display_text, "#CIRC!") == 0 || strcmp(sheet[r2][c2].display_text, "#CIRC!") == 0) {
                        strcpy(cell->display_text, "#REF!");
                        break;
                    }

                    cell->value = sheet[r1][c1].value + sheet[r2][c2].value;
                    snprintf(cell->display_text, CELL_WIDTH + 1, "%.2f", cell->value);
                } else {
                    strcpy(cell->display_text, "#NAME?");
                }
            } else {
                strcpy(cell->display_text, "#FORMULA!");
            }
            break;
        }
    }
}

// --- Utility Functions ---

// Parse a cell reference string like "A1" or "Z99" into row/col indices
int parse_cell_ref(const char* ref, int* row, int* col) {
    if (ref == NULL || strlen(ref) < 2) return 0;
    
    char col_char = toupper(ref[0]);
    if (col_char < 'A' || col_char > 'Z') return 0;
    *col = col_char - 'A';
    
    int row_num = atoi(&ref[1]);
    if (row_num < 1 || row_num > ROWS) return 0;
    *row = row_num - 1;
    
    return 1;
}

// --- File I/O ---

// Save sheet content to a file. We save the raw content.
void save_sheet(const char* filename) {
    FILE* fp = fopen(filename, "w");
    if (!fp) {
        printf("Error: Cannot open file '%s' for writing.\n", filename);
        return;
    }

    for (int r = 0; r < ROWS; ++r) {
        for (int c = 0; c < COLS; ++c) {
            if (sheet[r][c].type != EMPTY) {
                // Format: row,col,"raw_content"
                fprintf(fp, "%d,%d,\"%s\"\n", r, c, sheet[r][c].raw_content);
            }
        }
    }

    fclose(fp);
    printf("Sheet saved to '%s'.\n", filename);
}

// Load sheet from a file.
void load_sheet(const char* filename) {
    FILE* fp = fopen(filename, "r");
    if (!fp) {
        printf("Error: Cannot open file '%s' for reading.\n", filename);
        return;
    }

    // Clear the current sheet before loading
    free_sheet();
    init_sheet();

    char line[MAX_INPUT_LEN * 2];
    int r, c;

    while (fgets(line, sizeof(line), fp)) {
        char* content_start = strchr(line, '"');
        if (!content_start) continue; // Malformed line

        // Null-terminate the coordinates part
        *content_start = '\0';
        if (sscanf(line, "%d,%d,", &r, &c) != 2) continue; // Malformed line

        // Extract content
        content_start++; // Move past the opening quote
        char* content_end = strrchr(content_start, '"');
        if (content_end) {
            *content_end = '\0'; // Null-terminate the content
        }
        // Also remove trailing newline if any
        content_start[strcspn(content_start, "\n\r")] = 0;


        if (r >= 0 && r < ROWS && c >= 0 && c < COLS) {
            set_cell_from_string(r, c, content_start);
        }
    }

    fclose(fp);
    printf("Sheet loaded from '%s'.\n", filename);
}
