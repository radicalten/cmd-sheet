/******************************************************************************
 *
 *                             -- MiniCalc --
 *
 * A single-file, dependency-free C spreadsheet program inspired by VisiCalc.
 *
 *                          Features:
 *                          - A 2D grid of cells.
 *                          - Keyboard navigation (Arrow Keys).
 *                          - Cell Editing (Enter to start, Enter to commit, Esc to cancel).
 *                          - Data Types: Numbers, Text (Labels), and Formulas.
 *                          - Formulas start with '='.
 *                          - Formula Engine: Supports +, -, *, / and cell references (e.g., A1, Z50).
 *                          - Automatic, full-sheet recalculation on change.
 *                          - Error Display: #DIV/0!, #REF!, #CIRC!, #SYNTAX!
 *                          - Status bar showing current cell, content, and mode.
 *
 *                          How to Compile:
 *                          - Linux/macOS: gcc spreadsheet.c -o spreadsheet -lm
 *                          - Windows (MinGW): gcc spreadsheet.c -o spreadsheet.exe
 *                          - Windows (MSVC): cl spreadsheet.c
 *
 *                          How to Run:
 *                          - Linux/macOS: ./spreadsheet
 *                          - Windows: spreadsheet.exe
 *
 *                          Author: An AI Assistant
 *                          Date: 2023
 *
 ******************************************************************************/

// =============================================================================
// Includes and Platform-specific Setup
// =============================================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

// Platform-specific includes for terminal control and unbuffered input
#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#else
#include <unistd.h>
#include <termios.h>
#endif

// =============================================================================
// Constants and Definitions
// =============================================================================

#define ROWS 50
#define COLS 26 // A-Z
#define CELL_WIDTH 10
#define INPUT_BUFFER_SIZE 256

// ANSI Escape Codes for terminal graphics
#define CLEAR_SCREEN "\x1b[2J"
#define CURSOR_HOME "\x1b[H"
#define CURSOR_TO(r, c) "\x1b[" #r ";" #c "H"
#define INVERSE_VIDEO "\x1b[7m"
#define RESET_VIDEO "\x1b[0m"

// Forward Declarations of Core Types and Functions
typedef enum { CELL_EMPTY, CELL_NUMBER, CELL_TEXT, CELL_FORMULA } CellType;
typedef struct Cell Cell;
struct Cell {
    CellType type;
    char* raw_content;    // The literal string the user typed
    double value;         // Numeric value, if applicable
    char* display_str;    // Cached string for rendering
    int needs_recalc;     // Flag for the calculation loop
};

// Forward Declarations for the parser
double eval_expression(const char** expression_ptr, int r, int c, int depth);

// ===================================
// Global State
// ===================================

Cell sheet[ROWS][COLS];
int cursor_row = 0;
int cursor_col = 0;
char edit_mode = 0; // 0 for navigate, 1 for edit
char edit_buffer[INPUT_BUFFER_SIZE];
int program_running = 1;

// =============================================================================
// Terminal Control (Platform-specific)
// =============================================================================

#ifndef _WIN32
struct termios orig_termios;

void disable_raw_mode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void enable_raw_mode() {
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disable_raw_mode);
    struct termios raw = orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;
    tcsetattr(STDIN_fileno, TCSAFLUSH, &raw);
}

int get_key() {
    int nread;
    char c;
    while ((nread = read(STDIN_fileno, &c, 1)) != 1);

    if (c == '\x1b') { // Escape sequence
        char seq[3];
        if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';
        if (seq[0] == '[') {
            switch (seq[1]) {
                case 'A': return 1000; // Up Arrow
                case 'B': return 1001; // Down Arrow
                case 'C': return 1002; // Right Arrow
                case 'D': return 1003; // Left Arrow
            }
        }
        return '\x1b';
    } else {
        return c;
    }
}

#else
HANDLE hStdin, hStdout;
DWORD old_console_mode;

void disable_raw_mode() {
    SetConsoleMode(hStdin, old_console_mode);
}

void enable_raw_mode() {
    hStdin = GetStdHandle(STD_INPUT_HANDLE);
    hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
    GetConsoleMode(hStdin, &old_console_mode);
    atexit(disable_raw_mode);

    DWORD new_mode = old_console_mode & ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT);
    SetConsoleMode(hStdin, new_mode);

    // Enable ANSI support
    DWORD out_mode = 0;
    GetConsoleMode(hStdout, &out_mode);
    out_mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hStdout, out_mode);
}

int get_key() {
    int ch = _getch();
    if (ch == 224) { // Arrow key prefix
        switch (_getch()) {
            case 72: return 1000; // Up
            case 80: return 1001; // Down
            case 77: return 1002; // Right
            case 75: return 1003; // Left
        }
    }
    return ch;
}
#endif


// =============================================================================
// Cell and Sheet Management
// =============================================================================

void free_cell_memory(Cell* cell) {
    if (cell->raw_content) free(cell->raw_content);
    if (cell->display_str) free(cell->display_str);
}

void initialize_sheet() {
    for (int r = 0; r < ROWS; ++r) {
        for (int c = 0; c < COLS; ++c) {
            sheet[r][c].type = CELL_EMPTY;
            sheet[r][c].raw_content = NULL;
            sheet[r][c].value = 0.0;
            sheet[r][c].display_str = NULL;
            sheet[r][c].needs_recalc = 0;
        }
    }
}

void set_cell_display_string(Cell* cell, const char* str) {
    if (cell->display_str) free(cell->display_str);
    cell->display_str = strdup(str);
}

// =============================================================================
// Formula Parser and Evaluator
// =============================================================================

// Forward declaration
void evaluate_cell(int r, int c, int depth);

// Parses a cell reference like "A1", "Z50" etc.
// Returns 1 on success, 0 on failure. Updates r_out, c_out.
int parse_cell_ref(const char** expression, int* r_out, int* c_out) {
    const char* start = *expression;
    if (!isalpha(*start)) return 0;
    *c_out = toupper(*start) - 'A';
    start++;
    if (!isdigit(*start)) return 0;
    *r_out = 0;
    while(isdigit(*start)) {
        *r_out = *r_out * 10 + (*start - '0');
        start++;
    }
    (*r_out)--; // Adjust to 0-based index

    if (*r_out < 0 || *r_out >= ROWS || *c_out < 0 || *c_out >= COLS) return 0;
    
    *expression = start;
    return 1;
}

double parse_primary(const char** expression, int r, int c, int depth, int* error) {
    while (isspace(**expression)) (*expression)++;

    if (**expression == '(') {
        (*expression)++;
        double result = eval_expression(expression, r, c, depth);
        if (**expression == ')') {
            (*expression)++;
            return result;
        } else {
            *error = 1; // Mismatched parentheses
            return NAN;
        }
    }

    if (isalpha(**expression)) {
        int ref_r, ref_c;
        if(parse_cell_ref(expression, &ref_r, &ref_c)) {
            evaluate_cell(ref_r, ref_c, depth + 1);
            if (sheet[ref_r][ref_c].type == CELL_EMPTY) return 0.0;
            if (isnan(sheet[ref_r][ref_c].value)) {
                *error = 2; // Propagate error
                return NAN;
            }
            return sheet[ref_r][ref_c].value;
        } else {
            *error = 1; // Invalid reference format
            return NAN;
        }
    }

    char* end;
    double val = strtod(*expression, &end);
    if (*expression == end) {
        *error = 1; // Syntax error
        return NAN;
    }
    *expression = end;
    return val;
}

double parse_factor(const char** expression, int r, int c, int depth, int* error) {
    double left = parse_primary(expression, r, c, depth, error);
    if (*error) return NAN;

    while (1) {
        while (isspace(**expression)) (*expression)++;
        char op = **expression;
        if (op != '*' && op != '/') return left;
        (*expression)++;
        double right = parse_primary(expression, r, c, depth, error);
        if (*error) return NAN;
        if (op == '*') {
            left *= right;
        } else {
            if (right == 0) {
                *error = 3; // Division by zero
                return NAN;
            }
            left /= right;
        }
    }
}

double eval_expression(const char** expression_ptr, int r, int c, int depth, int *error) {
    if (depth > ROWS * COLS) {
        *error = 4; // Circular reference
        return NAN;
    }

    double left = parse_factor(expression_ptr, r, c, depth, error);
    if (*error) return NAN;

    while(1) {
        while (isspace(**expression_ptr)) (*expression_ptr)++;
        char op = **expression_ptr;
        if (op != '+' && op != '-') return left;
        (*expression_ptr)++;
        double right = parse_factor(expression_ptr, r, c, depth, error);
        if (*error) return NAN;
        if (op == '+') left += right; else left -= right;
    }
}

// =============================================================================
// Cell Calculation and Update Logic
// =============================================================================

void evaluate_cell(int r, int c, int depth) {
    Cell* cell = &sheet[r][c];

    // Don't re-evaluate if not needed in this pass
    if (!cell->needs_recalc && cell->type != CELL_EMPTY) return; 

    cell->needs_recalc = 0;

    switch (cell->type) {
        case CELL_NUMBER: {
            cell->value = strtod(cell->raw_content, NULL);
            char buf[32];
            snprintf(buf, 32, "%g", cell->value);
            set_cell_display_string(cell, buf);
            break;
        }
        case CELL_TEXT: {
            cell->value = NAN; // Not a number
            set_cell_display_string(cell, cell->raw_content);
            break;
        }
        case CELL_FORMULA: {
            const char* formula = cell->raw_content + 1; // Skip '='
            int error = 0;
            double result = eval_expression(&formula, r, c, depth, &error);

            if (error) {
                cell->value = NAN;
                if (error == 1) set_cell_display_string(cell, "#SYNTAX!");
                else if (error == 2) set_cell_display_string(cell, "#REF!");
                else if (error == 3) set_cell_display_string(cell, "#DIV/0!");
                else if (error == 4) set_cell_display_string(cell, "#CIRC!");
                else set_cell_display_string(cell, "#ERROR!");
            } else {
                cell->value = result;
                char buf[32];
                snprintf(buf, 32, "%g", cell->value);
                set_cell_display_string(cell, buf);
            }
            break;
        }
        case CELL_EMPTY:
            cell->value = 0.0;
            set_cell_display_string(cell, "");
            break;
    }
}

void recalculate_all() {
    // Mark all formula cells as needing recalculation
    for (int r = 0; r < ROWS; ++r) {
        for (int c = 0; c < COLS; ++c) {
            if (sheet[r][c].type == CELL_FORMULA) {
                sheet[r][c].needs_recalc = 1;
            }
        }
    }

    // Iterate to resolve dependencies. 10 passes is usually enough.
    for (int i = 0; i < 10; ++i) {
        for (int r = 0; r < ROWS; ++r) {
            for (int c = 0; c < COLS; ++c) {
                if (sheet[r][c].needs_recalc) {
                    evaluate_cell(r, c, 0);
                }
            }
        }
    }
}


void update_cell(int r, int c, const char* input) {
    Cell* cell = &sheet[r][c];
    free_cell_memory(cell);

    // If input is empty, reset the cell
    if (input == NULL || strlen(input) == 0) {
        cell->type = CELL_EMPTY;
        cell->raw_content = strdup("");
        cell->needs_recalc = 1;
        evaluate_cell(r, c, 0);
        recalculate_all();
        return;
    }

    cell->raw_content = strdup(input);

    if (input[0] == '=') {
        cell->type = CELL_FORMULA;
    } else {
        char *endptr;
        strtod(input, &endptr);
        if (*endptr == '\0') { // It's a valid number
            cell->type = CELL_NUMBER;
        } else {
            cell->type = CELL_TEXT;
        }
    }
    
    cell->needs_recalc = 1;
    evaluate_cell(r, c, 0);

    // A change might affect other cells, so do a full recalculation
    recalculate_all();
}


// =============================================================================
// Display and Rendering
// =============================================================================

void draw_sheet() {
    printf(CURSOR_HOME);

    // Print column headers
    printf("%4s", "");
    for (int c = 0; c < COLS; ++c) {
        printf("|%*s%c%*s", (CELL_WIDTH - 1) / 2, "", 'A' + c, CELL_WIDTH - 1 - (CELL_WIDTH - 1) / 2, "");
    }
    printf("|\n");

    // Print separator
    printf("%4s", "");
    for (int c = 0; c < COLS * (CELL_WIDTH + 1) + 1; ++c) {
        printf("-");
    }
    printf("\n");

    // Print rows
    for (int r = 0; r < ROWS; ++r) {
        printf("%3d ", r + 1);
        for (int c = 0; c < COLS; ++c) {
            printf("|");
            int is_cursor_cell = (r == cursor_row && c == cursor_col);
            if (is_cursor_cell) printf(INVERSE_VIDEO);
            
            char display_buf[CELL_WIDTH + 1] = {0};
            if (sheet[r][c].display_str) {
                strncpy(display_buf, sheet[r][c].display_str, CELL_WIDTH);
            }
            
            if (sheet[r][c].type == CELL_NUMBER || sheet[r][c].type == CELL_FORMULA) {
                 printf("%*s", CELL_WIDTH, display_buf); // Right-align numbers
            } else {
                 printf("%-*s", CELL_WIDTH, display_buf); // Left-align text
            }

            if (is_cursor_cell) printf(RESET_VIDEO);
        }
        printf("|\n");
    }
}

void draw_status_bar() {
    // Position cursor after the grid
    printf(CURSOR_TO(%d, 1), ROWS + 3);

    // Clear the status bar area
    for (int i=0; i<3; ++i) {
        printf("\x1b[K\n");
    }
    printf(CURSOR_TO(%d, 1), ROWS + 3);

    char cell_name[5];
    snprintf(cell_name, 5, "%c%d", 'A' + cursor_col, cursor_row + 1);

    char* cell_content = sheet[cursor_row][cursor_col].raw_content ? sheet[cursor_row][cursor_col].raw_content : "";

    printf("Cell: %-5s | Content: %s\n", cell_name, cell_content);
    printf("------------------------------------------------------------------\n");

    if (edit_mode) {
        printf("EDIT> %s", edit_buffer);
    } else {
        printf("q:quit | Arrows:move | Enter:edit");
    }
    fflush(stdout);
}

// =============================================================================
// Input Processing and Main Loop
// =============================================================================

void process_input() {
    int c = get_key();

    if (edit_mode) {
        switch (c) {
            case 13: // Enter
                edit_mode = 0;
                update_cell(cursor_row, cursor_col, edit_buffer);
                break;
            case 27: // Escape
                edit_mode = 0;
                break;
            case 127: // Backspace (for Linux/macOS)
            case 8:   // Backspace (for Windows)
                {
                    int len = strlen(edit_buffer);
                    if (len > 0) edit_buffer[len - 1] = '\0';
                }
                break;
            default:
                if (isprint(c)) {
                    int len = strlen(edit_buffer);
                    if (len < INPUT_BUFFER_SIZE - 1) {
                        edit_buffer[len] = c;
                        edit_buffer[len + 1] = '\0';
                    }
                }
                break;
        }
    } else { // Navigate mode
        switch (c) {
            case 'q':
                program_running = 0;
                break;
            case 1000: // Up
                if (cursor_row > 0) cursor_row--;
                break;
            case 1001: // Down
                if (cursor_row < ROWS - 1) cursor_row++;
                break;
            case 1003: // Left
                if (cursor_col > 0) cursor_col--;
                break;
            case 1002: // Right
                if (cursor_col < COLS - 1) cursor_col++;
                break;
            case 13: // Enter: start editing
                edit_mode = 1;
                char* current_content = sheet[cursor_row][cursor_col].raw_content;
                if (current_content) {
                    strncpy(edit_buffer, current_content, INPUT_BUFFER_SIZE-1);
                } else {
                    edit_buffer[0] = '\0';
                }
                break;
        }
    }
}


int main() {
    enable_raw_mode();
    initialize_sheet();

    // Some sample data to demonstrate features
    update_cell(0, 0, "10");
    update_cell(1, 0, "20");
    updatecell(2, 0, "=A1+A2");
    update_cell(0, 1, "Price");
    update_cell(1, 1, "Tax");
    update_cell(2, 1, "Total");
    update_cell(0, 2, "1.99");
    update_cell(1, 2, "=C1*0.08");
    update_cell(2, 2, "=C1+C2");

    while (program_running) {
        draw_sheet();
        draw_status_bar();
        process_input();
    }
    
    // Cleanup
    printf(CLEAR_SCREEN CURSOR_HOME);
    for (int r = 0; r < ROWS; ++r) {
        for (int c = 0; c < COLS; ++c) {
            free_cell_memory(&sheet[r][c]);
        }
    }

    return 0;
}
