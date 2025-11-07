#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define ROWS 10
#define COLS 10
#define MAX_CELL_LEN 256
#define MAX_FORMULA_LEN 256

typedef struct {
    char content[MAX_CELL_LEN];
    int is_formula;  // 1 if formula, 0 otherwise
    double value;    // Cached value if numeric
} Cell;

Cell sheet[ROWS][COLS];

// Function prototypes
void init_sheet();
void print_sheet();
void set_cell(int row, int col, const char* value);
double evaluate_cell(int row, int col);
double parse_expression(const char* expr, int* pos);
double parse_term(const char* expr, int* pos);
double parse_factor(const char* expr, int* pos);
int get_row(char label);
int get_col(const char* label);

// Main function
int main() {
    char command[512];
    init_sheet();

    printf("Simple Terminal Spreadsheet\n");
    printf("Commands: set <cell> <value> (e.g., set A1 42 or set B2 =A1+3)\n");
    printf("          view\n");
    printf("          quit\n");

    while (1) {
        printf("> ");
        if (fgets(command, sizeof(command), stdin) == NULL) break;
        command[strcspn(command, "\n")] = 0;  // Remove newline

        if (strcmp(command, "quit") == 0) break;
        if (strcmp(command, "view") == 0) {
            print_sheet();
            continue;
        }

        // Parse 'set' command
        char* token = strtok(command, " ");
        if (token && strcmp(token, "set") == 0) {
            char* cell = strtok(NULL, " ");
            char* value = strtok(NULL, "");
            if (!cell || !value || strlen(cell) < 2) {
                printf("Invalid command. Use: set <cell> <value>\n");
                continue;
            }
            int row = get_row(cell[0]);
            char* col_str = cell + 1;
            int col = atoi(col_str) - 1;
            if (row < 0 || row >= ROWS || col < 0 || col >= COLS) {
                printf("Invalid cell: %s\n", cell);
                continue;
            }
            set_cell(row, col, value);
            printf("Set %s to '%s'\n", cell, value);
        } else {
            printf("Unknown command\n");
        }
    }

    return 0;
}

void init_sheet() {
    for (int i = 0; i < ROWS; i++) {
        for (int j = 0; j < COLS; j++) {
            strcpy(sheet[i][j].content, "");
            sheet[i][j].is_formula = 0;
            sheet[i][j].value = 0.0;
        }
    }
}

void print_sheet() {
    printf("   ");
    for (int j = 1; j <= COLS; j++) printf("%-8d", j);
    printf("\n");

    for (int i = 0; i < ROWS; i++) {
        printf("%c  ", 'A' + i);
        for (int j = 0; j < COLS; j++) {
            if (sheet[i][j].content[0] == '\0') {
                printf("%-8s", "");
            } else if (sheet[i][j].is_formula) {
                double val = evaluate_cell(i, j);
                printf("%-8.2f", val);
            } else if (isdigit(sheet[i][j].content[0]) || sheet[i][j].content[0] == '-' || sheet[i][j].content[0] == '.') {
                printf("%-8.2f", atof(sheet[i][j].content));
            } else {
                printf("%-8s", sheet[i][j].content);
            }
        }
        printf("\n");
    }
}

void set_cell(int row, int col, const char* value) {
    strncpy(sheet[row][col].content, value, MAX_CELL_LEN - 1);
    sheet[row][col].is_formula = (value[0] == '=');
    if (!sheet[row][col].is_formula) {
        sheet[row][col].value = atof(value);
    }
}

double evaluate_cell(int row, int col) {
    if (!sheet[row][col].is_formula) return sheet[row][col].value;

    const char* expr = sheet[row][col].content + 1;  // Skip '='
    int pos = 0;
    return parse_expression(expr, &pos);
}

double parse_expression(const char* expr, int* pos) {
    double result = parse_term(expr, pos);
    while (expr[*pos] == '+' || expr[*pos] == '-') {
        char op = expr[(*pos)++];
        double term = parse_term(expr, pos);
        if (op == '+') result += term;
        else result -= term;
    }
    return result;
}

double parse_term(const char* expr, int* pos) {
    double result = parse_factor(expr, pos);
    while (expr[*pos] == '*' || expr[*pos] == '/') {
        char op = expr[(*pos)++];
        double factor = parse_factor(expr, pos);
        if (op == '*') result *= factor;
        else if (factor != 0) result /= factor;
        else return 0;  // Division by zero
    }
    return result;
}

double parse_factor(const char* expr, int* pos) {
    while (isspace(expr[*pos])) (*pos)++;

    if (isdigit(expr[*pos]) || expr[*pos] == '-' || expr[*pos] == '.') {
        // Parse number
        char* end;
        double num = strtod(expr + *pos, &end);
        *pos = end - expr;
        return num;
    } else if (isalpha(expr[*pos])) {
        // Parse cell reference (e.g., A1)
        char row_label = expr[(*pos)++];
        char col_str[10];
        int col_idx = 0;
        while (isdigit(expr[*pos]) && col_idx < 9) {
            col_str[col_idx++] = expr[(*pos)++];
        }
        col_str[col_idx] = '\0';
        int r = get_row(row_label);
        int c = atoi(col_str) - 1;
        if (r >= 0 && r < ROWS && c >= 0 && c < COLS) {
            return evaluate_cell(r, c);
        } else {
            return 0;  // Invalid reference
        }
    } else if (expr[*pos] == '(') {
        (*pos)++;  // Skip '('
        double sub = parse_expression(expr, pos);
        if (expr[*pos] == ')') (*pos)++;
        return sub;
    }
    return 0;  // Error
}

int get_row(char label) {
    if (label >= 'A' && label <= 'J') return label - 'A';
    return -1;
}
