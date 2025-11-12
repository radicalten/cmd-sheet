#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#define MAX_ROWS 50
#define MAX_COLS 10
#define MAX_CELL_LEN 128
#define MAX_INPUT 256

typedef struct {
    char raw[MAX_CELL_LEN];
    double value;
    char display[MAX_CELL_LEN];
    int is_formula;
    int dirty;
} Cell;

Cell grid[MAX_ROWS][MAX_COLS];
int cursor_row = 0, cursor_col = 0;
char status_msg[MAX_INPUT];
int recalc_depth = 0;

void init_grid() {
    for (int r = 0; r < MAX_ROWS; r++) {
        for (int c = 0; c < MAX_COLS; c++) {
            grid[r][c].raw[0] = '\0';
            grid[r][c].display[0] = '\0';
            grid[r][c].value = 0;
            grid[r][c].is_formula = 0;
            grid[r][c].dirty = 0;
        }
    }
}

void coords_to_cell(int row, int col, char* buf) {
    sprintf(buf, "%c%d", 'A' + col, row + 1);
}

int cell_to_coords(const char* cell, int* row, int* col) {
    if (!isalpha(cell[0]) || !isdigit(cell[1])) return 0;
    *col = toupper(cell[0]) - 'A';
    char* end;
    *row = (int)strtol(cell + 1, &end, 10) - 1;
    return *row >= 0 && *row < MAX_ROWS && *col >= 0 && *col < MAX_COLS && *end == '\0';
}

double eval_expression(const char** expr);

double eval_primary(const char** expr) {
    while (isspace(**expr)) (*expr)++;
    
    if (**expr == '(') {
        (*expr)++;
        double result = eval_expression(expr);
        if (**expr == ')') (*expr)++;
        return result;
    }
    
    if (isalpha(**expr)) {
        char cell_ref[16];
        int i = 0;
        while (isalnum(**expr) && i < 15) {
            cell_ref[i++] = **expr;
            (*expr)++;
        }
        cell_ref[i] = '\0';
        
        if (strncmp(cell_ref, "SUM", 3) == 0) {
            while (isspace(**expr)) (*expr)++;
            if (**expr == '(') (*expr)++;
            double sum = 0;
            while (**expr && **expr != ')') {
                char range[16];
                int j = 0;
                while (isalnum(**expr) && j < 15) {
                    range[j++] = **expr;
                    (*expr)++;
                }
                range[j] = '\0';
                if (isalpha(range[0])) {
                    int r, c;
                    if (cell_to_coords(range, &r, &c)) {
                        sum += grid[r][c].value;
                    }
                }
                if (**expr == ',') (*expr)++;
            }
            if (**expr == ')') (*expr)++;
            return sum;
        }
        
        int r, c;
        if (cell_to_coords(cell_ref, &r, &c)) {
            return grid[r][c].value;
        }
        return 0;
    }
    
    char* end;
    double val = strtod(*expr, &end);
    *expr = end;
    return val;
}

double eval_power(const char** expr) {
    double left = eval_primary(expr);
    while (isspace(**expr)) (*expr)++;
    if (**expr == '^') {
        (*expr)++;
        double right = eval_power(expr);
        return pow(left, right);
    }
    return left;
}

double eval_term(const char** expr) {
    double result = eval_power(expr);
    while (isspace(**expr)) (*expr)++;
    while (**expr == '*' || **expr == '/') {
        char op = **expr;
        (*expr)++;
        double right = eval_power(expr);
        if (op == '*') result *= right;
        else result /= (right != 0 ? right : 1);
        while (isspace(**expr)) (*expr)++;
    }
    return result;
}

double eval_expression(const char** expr) {
    double result = eval_term(expr);
    while (isspace(**expr)) (*expr)++;
    while (**expr == '+' || **expr == '-') {
        char op = **expr;
        (*expr)++;
        double right = eval_term(expr);
        if (op == '+') result += right;
        else result -= right;
        while (isspace(**expr)) (*expr)++;
    }
    return result;
}

void recalculate_cell(int row, int col) {
    if (recalc_depth > 100) return;
    if (!grid[row][col].is_formula) return;
    
    recalc_depth++;
    const char* expr = grid[row][col].raw + 1;
    grid[row][col].value = eval_expression(&expr);
    
    if (fabs(grid[row][col].value - floor(grid[row][col].value)) < 0.0001) {
        sprintf(grid[row][col].display, "%.0f", grid[row][col].value);
    } else {
        sprintf(grid[row][col].display, "%.2f", grid[row][col].value);
    }
    recalc_depth--;
}

void recalculate_all() {
    for (int r = 0; r < MAX_ROWS; r++) {
        for (int c = 0; c < MAX_COLS; c++) {
            if (grid[r][c].is_formula) {
                recalculate_cell(r, c);
            }
        }
    }
}

void set_cell(int row, int col, const char* value) {
    strncpy(grid[row][col].raw, value, MAX_CELL_LEN - 1);
    grid[row][col].is_formula = (value[0] == '=');
    
    if (grid[row][col].is_formula) {
        recalculate_cell(row, col);
    } else {
        strncpy(grid[row][col].display, value, MAX_CELL_LEN - 1);
        grid[row][col].value = strtod(value, NULL);
    }
}

void render() {
    printf("\033[2J\033[H");
    printf("Simple Spreadsheet (Ctrl+S=Save Ctrl+O=Open Ctrl+Q=Quit Enter=Edit)\n");
    printf("Status: %s\n\n", status_msg);
    
    printf("     ");
    for (int c = 0; c < MAX_COLS; c++) {
        printf(" %-10c", 'A' + c);
    }
    printf("\n");
    
    for (int r = 0; r < MAX_ROWS; r++) {
        printf("%3d  ", r + 1);
        for (int c = 0; c < MAX_COLS; c++) {
            char cell_str[MAX_CELL_LEN];
            if (r == cursor_row && c == cursor_col) {
                sprintf(cell_str, ">%9s<", grid[r][c].display);
            } else {
                sprintf(cell_str, " %-9s ", grid[r][c].display);
            }
            if (strlen(cell_str) > 11) {
                memcpy(cell_str + 9, "...", 3);
            }
            printf("%.11s", cell_str);
        }
        printf("\n");
    }
    
    char cell_ref[16];
    coords_to_cell(cursor_row, cursor_col, cell_ref);
    printf("\nCell %s: %s\n", cell_ref, grid[cursor_row][cursor_col].raw);
}

void handle_input() {
    status_msg[0] = '\0';
    char input[MAX_INPUT];
    
    printf("Command: ");
    fflush(stdout);
    
    if (fgets(input, MAX_INPUT, stdin) == NULL) {
        strcpy(status_msg, "Error reading input");
        return;
    }
    
    input[strcspn(input, "\n")] = 0;
    
    if (strcmp(input, ":q") == 0 || strcmp(input, "\x11") == 0) {
        printf("Exiting...\n");
        exit(0);
    }
    
    if (strcmp(input, ":s") == 0 || strcmp(input, "\x13") == 0) {
        FILE* fp = fopen("spreadsheet.csv", "w");
        if (!fp) {
            strcpy(status_msg, "Save failed");
            return;
        }
        for (int r = 0; r < MAX_ROWS; r++) {
            for (int c = 0; c < MAX_COLS; c++) {
                if (grid[r][c].raw[0]) {
                    char cell_ref[16];
                    coords_to_cell(r, c, cell_ref);
                    fprintf(fp, "%s,%s\n", cell_ref, grid[r][c].raw);
                }
            }
        }
        fclose(fp);
        strcpy(status_msg, "Saved to spreadsheet.csv");
        return;
    }
    
    if (strcmp(input, ":o") == 0 || strcmp(input, "\x0f") == 0) {
        FILE* fp = fopen("spreadsheet.csv", "r");
        if (!fp) {
            strcpy(status_msg, "Load failed");
            return;
        }
        char line[MAX_INPUT];
        while (fgets(line, MAX_INPUT, fp)) {
            char* cell_ref = strtok(line, ",");
            char* value = strtok(NULL, "\n");
            if (cell_ref && value) {
                int r, c;
                if (cell_to_coords(cell_ref, &r, &c)) {
                    set_cell(r, c, value);
                }
            }
        }
        fclose(fp);
        recalculate_all();
        strcpy(status_msg, "Loaded from spreadsheet.csv");
        return;
    }
    
    if (input[0] == 'w' && cursor_row > 0) cursor_row--;
    else if (input[0] == 's' && cursor_row < MAX_ROWS - 1) cursor_row++;
    else if (input[0] == 'a' && cursor_col > 0) cursor_col--;
    else if (input[0] == 'd' && cursor_col < MAX_COLS - 1) cursor_col++;
    else if (input[0] != '\0') {
        set_cell(cursor_row, cursor_col, input);
        recalculate_all();
    }
}

int main() {
    init_grid();
    strcpy(status_msg, "Ready");
    
    system("stty raw -echo");
    atexit([](){ system("stty cooked echo"); printf("\n"); });
    
    while (1) {
        render();
        handle_input();
    }
    
    return 0;
}
