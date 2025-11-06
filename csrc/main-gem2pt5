/******************************************************************************
 * T-CALC: A Simple Terminal Spreadsheet
 *
 * A single-file, dependency-free C program for a basic spreadsheet in the
 * terminal.
 *
 * Author: GPT-4 and human collaborator
 * License: Public Domain
 *
 * How to Compile:
 *   gcc tcalc.c -o tcalc
 *
 * How to Run:
 *   ./tcalc
 *
 * Controls:
 *   - Arrow Keys: Move the cursor.
 *   - Enter: Start editing the current cell.
 *   - While editing:
 *     - Type to enter content (numbers, text, or formulas).
 *     - Formulas must start with '=' (e.g., =A1+B2).
 *     - Backspace/Delete: Erase characters.
 *     - Enter: Confirm changes and move down.
 *     - Escape: Cancel changes.
 *   - Ctrl-Q: Quit the program.
 *
 * Limitations:
 *   - Formulas support only one operator (+, -, *, /) and two operands.
 *   - No support for parentheses or operator precedence.
 *   - Cell references are case-sensitive (A1, not a1).
 *   - Display width is fixed.
 ******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <ctype.h>

// --- Configuration ---
#define ROWS 20
#define COLS 10
#define CELL_WIDTH 10
#define INPUT_BUFFER_SIZE 256
#define CELL_EXPR_SIZE 100

// --- Data Structures ---
typedef struct {
    char expr[CELL_EXPR_SIZE]; // The raw expression string (e.g., "123", "hello", "=A1+B2")
    float value;               // The calculated numeric value
    char display[CELL_WIDTH + 1]; // The string to display on the grid
} Cell;

Cell sheet[ROWS][COLS];
int current_row = 0;
int current_col = 0;

// --- Terminal Handling ---
struct termios orig_termios;

void disableRawMode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void enableRawMode() {
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disableRawMode);

    struct termios raw = orig_termios;
    raw.c_iflag &= ~(IXON); // Disable software flow control (Ctrl-S, Ctrl-Q)
    raw.c_lflag &= ~(ECHO | ICANON | ISIG); // Disable echo, canonical mode, and signals (Ctrl-C, etc.)
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

// --- Forward Declarations ---
void recalculate_all();
float evaluate_expression(const char *expr, int *error, int r, int c);

// --- Core Logic ---
void get_cell_name(int r, int c, char *name) {
    if (c >= 0 && c < 26) {
        sprintf(name, "%c%d", 'A' + c, r + 1);
    } else {
        strcpy(name, "??");
    }
}

void init_sheet() {
    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            strcpy(sheet[r][c].expr, "");
            sheet[r][c].value = 0.0f;
            strcpy(sheet[r][c].display, "");
        }
    }
}

// Parses a cell reference like "A1" into row and column indices.
int parse_cell_ref(const char *ref, int *row, int *col) {
    if (strlen(ref) < 2 || !isupper(ref[0])) return 0;
    *col = ref[0] - 'A';
    *row = atoi(&ref[1]) - 1;
    return (*row >= 0 && *row < ROWS && *col >= 0 && *col < COLS);
}

// Evaluates a single term, which can be a number or a cell reference.
float evaluate_term(const char *term, int *error, int r, int c) {
    if (isdigit(term[0]) || (term[0] == '-' && isdigit(term[1]))) {
        return atof(term);
    }

    int dep_r, dep_c;
    if (parse_cell_ref(term, &dep_r, &dep_c)) {
        // Recursive call to get dependent cell's value
        return evaluate_expression(sheet[dep_r][dep_c].expr, error, dep_r, dep_c);
    }

    *error = 1; // Invalid term
    return 0.0f;
}

// A simple recursive evaluator for expressions like "=A1+B2"
// `r` and `c` are the coordinates of the cell being evaluated to detect circular refs.
float evaluate_expression(const char *expr, int *error, int r, int c) {
    // Keep track of cells currently being evaluated in this recursion stack.
    static int evaluating[ROWS][COLS] = {0};

    if (expr[0] == '\0') return 0.0f;
    if (expr[0] != '=') { // It's a literal value, not a formula
        char* end;
        float val = strtof(expr, &end);
        if (*end == '\0' || *end == '\n') return val; // It's a valid number
        return 0.0f; // Treat non-numeric literals as 0 in calculations
    }

    // Circular dependency check
    if (evaluating[r][c]) {
        *error = 1;
        return 0.0f;
    }
    evaluating[r][c] = 1;

    // Simple parser: find operator, split into left/right terms
    const char *p = expr + 1; // Skip '='
    char left_term[CELL_EXPR_SIZE] = {0};
    char right_term[CELL_EXPR_SIZE] = {0};
    char op = 0;

    const char *op_pos = strpbrk(p, "+-*/");

    if (op_pos) {
        op = *op_pos;
        strncpy(left_term, p, op_pos - p);
        strcpy(right_term, op_pos + 1);
    } else {
        strcpy(left_term, p); // Only one term, e.g., "=A1" or "=123"
    }

    float left_val = evaluate_term(left_term, error, r, c);
    if (*error) {
        evaluating[r][c] = 0;
        return 0.0f;
    }

    if (op == 0) { // No operator, just a single term
        evaluating[r][c] = 0;
        return left_val;
    }
    
    float right_val = evaluate_term(right_term, error, r, c);
    if (*error) {
        evaluating[r][c] = 0;
        return 0.0f;
    }

    evaluating[r][c] = 0; // Unmark before returning

    switch (op) {
        case '+': return left_val + right_val;
        case '-': return left_val - right_val;
        case '*': return left_val * right_val;
        case '/':
            if (right_val == 0) {
                *error = 1; // Division by zero
                return 0.0f;
            }
            return left_val / right_val;
    }

    *error = 1; // Should not be reached
    return 0.0f;
}

void recalculate_all() {
    // Multiple passes to propagate dependencies (A1=B1, B1=C1, etc.)
    for (int pass = 0; pass < 5; ++pass) {
        for (int r = 0; r < ROWS; r++) {
            for (int c = 0; c < COLS; c++) {
                if (sheet[r][c].expr[0] == '=') {
                    int error = 0;
                    sheet[r][c].value = evaluate_expression(sheet[r][c].expr, &error, r, c);
                    if (error) {
                        snprintf(sheet[r][c].display, CELL_WIDTH + 1, "#ERROR");
                    } else {
                        snprintf(sheet[r][c].display, CELL_WIDTH + 1, "%.2f", sheet[r][c].value);
                    }
                } else { // It's a literal string or number
                    char* end;
                    strtof(sheet[r][c].expr, &end);
                    // If the whole string is a number, format it, otherwise display as text.
                    if (*end == '\0' || *end == '\n') {
                         snprintf(sheet[r][c].display, CELL_WIDTH + 1, "%.2f", atof(sheet[r][c].expr));
                    } else {
                        snprintf(sheet[r][c].display, CELL_WIDTH + 1, "%s", sheet[r][c].expr);
                    }
                }
            }
        }
    }
}

// --- UI Rendering ---
void draw_sheet() {
    char buffer[4096] = {0}; // Use a buffer to reduce flicker
    char *buf_ptr = buffer;

    // ANSI escape codes: clear screen, move to home
    buf_ptr += sprintf(buf_ptr, "\x1b[2J\x1b[H");

    // Draw column headers
    buf_ptr += sprintf(buf_ptr, "%4s|", "");
    for (int c = 0; c < COLS; c++) {
        buf_ptr += sprintf(buf_ptr, "%*c |", CELL_WIDTH - 1, 'A' + c);
    }
    buf_ptr += sprintf(buf_ptr, "\n");

    // Draw separator line
    buf_ptr += sprintf(buf_ptr, "----|");
    for (int c = 0; c < COLS; c++) {
        for (int i = 0; i < CELL_WIDTH + 1; i++) buf_ptr += sprintf(buf_ptr, "-");
    }
    buf_ptr += sprintf(buf_ptr, "\n");

    // Draw cells
    for (int r = 0; r < ROWS; r++) {
        buf_ptr += sprintf(buf_ptr, "%3d |", r + 1);
        for (int c = 0; c < COLS; c++) {
            int is_current = (r == current_row && c == current_col);
            if (is_current) buf_ptr += sprintf(buf_ptr, "\x1b[7m"); // Reverse video
            
            buf_ptr += sprintf(buf_ptr, " %-*.*s ", CELL_WIDTH - 2, CELL_WIDTH - 2, sheet[r][c].display);

            if (is_current) buf_ptr += sprintf(buf_ptr, "\x1b[0m"); // Reset video
            buf_ptr += sprintf(buf_ptr, "|");
        }
        buf_ptr += sprintf(buf_ptr, "\n");
    }
    write(STDOUT_FILENO, buffer, strlen(buffer));
}

void draw_input_bar(const char *input_buffer) {
    char cell_name[10];
    get_cell_name(current_row, current_col, cell_name);

    // Move cursor to the line after the grid
    printf("\x1b[%d;1H", ROWS + 3);
    // Clear the line and print status
    printf("\x1b[K"); // Clear line from cursor to end
    printf("%s: %s", cell_name, input_buffer);
    fflush(stdout);
}

// --- Input Handling ---
void move_cursor(int dr, int dc) {
    current_row += dr;
    current_col += dc;
    if (current_row < 0) current_row = 0;
    if (current_row >= ROWS) current_row = ROWS - 1;
    if (current_col < 0) current_col = 0;
    if (current_col >= COLS) current_col = COLS - 1;
}

void edit_current_cell() {
    char input_buffer[INPUT_BUFFER_SIZE];
    strcpy(input_buffer, sheet[current_row][current_col].expr);
    int pos = strlen(input_buffer);

    while (1) {
        draw_input_bar(input_buffer);
        printf("\x1b[%d;%ldH", ROWS + 3, (long)(strlen(input_buffer) + 5 + pos)); // Move cursor to end of input
        fflush(stdout);

        char c = '\0';
        read(STDIN_FILENO, &c, 1);

        switch (c) {
            case '\r': // Enter key
            case '\n':
                strcpy(sheet[current_row][current_col].expr, input_buffer);
                recalculate_all();
                move_cursor(1, 0); // Move down after editing
                return;

            case 27: // Escape sequence
                {
                    char seq[3];
                    if (read(STDIN_FILENO, &seq[0], 1) == 0) { // Just escape key
                        return; // Cancel edit
                    }
                    if (read(STDIN_FILENO, &seq[1], 1) == 0) return;
                    // Could handle more complex sequences here
                }
                break;

            case 127: // Backspace
            case 8:
                if (pos > 0) {
                    input_buffer[--pos] = '\0';
                }
                break;

            default:
                if (isprint(c) && pos < INPUT_BUFFER_SIZE - 1) {
                    input_buffer[pos++] = c;
                    input_buffer[pos] = '\0';
                }
                break;
        }
    }
}

void handle_input() {
    char c = '\0';
    if (read(STDIN_FILENO, &c, 1) == -1) return;

    if (c == '\x04' || c == 17) { // Ctrl-D or Ctrl-Q
        printf("\x1b[2J\x1b[H"); // Clear screen and go home on exit
        exit(0);
    }
    
    switch (c) {
        case '\r': // Enter
        case '\n':
            edit_current_cell();
            break;

        case 27: // Escape sequence (likely arrows)
            {
                char seq[3];
                if (read(STDIN_FILENO, &seq[0], 1) == 0) return;
                if (read(STDIN_FILENO, &seq[1], 1) == 0) return;

                if (seq[0] == '[') {
                    switch (seq[1]) {
                        case 'A': move_cursor(-1, 0); break; // Up
                        case 'B': move_cursor(1, 0); break;  // Down
                        case 'C': move_cursor(0, 1); break;  // Right
                        case 'D': move_cursor(0, -1); break; // Left
                    }
                }
            }
            break;
        
        default:
             // Any other key starts editing
             if (isprint(c)) {
                 edit_current_cell();
             }
             break;
    }
}

// --- Main Loop ---
int main() {
    enableRawMode();
    init_sheet();

    // Example default values
    strcpy(sheet[0][0].expr, "10");
    strcpy(sheet[0][1].expr, "20");
    strcpy(sheet[0][2].expr, "=A1+B1");
    strcpy(sheet[1][0].expr, "Value:");
    strcpy(sheet[2][2].expr, "=A1*A1");
    recalculate_all();

    while (1) {
        draw_sheet();
        char cell_name[10], expr_preview[50];
        get_cell_name(current_row, current_col, cell_name);
        snprintf(expr_preview, 50, "%s", sheet[current_row][current_col].expr);
        draw_input_bar(expr_preview);
        handle_input();
    }

    return 0;
}
