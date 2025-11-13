#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#define ROWS 100
#define COLS 26
#define CELL_WIDTH 12
#define MAX_FORMULA 256

typedef enum { EMPTY, NUMBER, TEXT, FORMULA } CellType;

typedef struct {
    CellType type;
    double number;
    char text[MAX_FORMULA];
    char formula[MAX_FORMULA];
} Cell;

Cell sheet[ROWS][COLS];
int cursor_row = 0, cursor_col = 0;

void init_sheet() {
    for (int i = 0; i < ROWS; i++)
        for (int j = 0; j < COLS; j++)
            sheet[i][j].type = EMPTY;
}

void clear_screen() {
    printf("\033[2J\033[H");
}

double get_cell_value(int row, int col) {
    if (row < 0 || row >= ROWS || col < 0 || col >= COLS)
        return 0;
    Cell *c = &sheet[row][col];
    return (c->type == NUMBER || c->type == FORMULA) ? c->number : 0;
}

int parse_cell_ref(const char *ref, int *row, int *col) {
    if (!isalpha(ref[0])) return 0;
    *col = toupper(ref[0]) - 'A';
    *row = atoi(ref + 1) - 1;
    return (*row >= 0 && *row < ROWS && *col >= 0 && *col < COLS);
}

double eval_formula(const char *formula);

double eval_function(const char *fname, const char *args) {
    char arg_copy[MAX_FORMULA];
    strncpy(arg_copy, args, MAX_FORMULA - 1);
    
    if (strcmp(fname, "SUM") == 0) {
        double sum = 0;
        char *token = strtok(arg_copy, ",:");
        int r1, c1, r2, c2;
        
        if (strchr(args, ':')) {
            // Range like A1:A10
            char range[MAX_FORMULA];
            strncpy(range, args, MAX_FORMULA - 1);
            char *colon = strchr(range, ':');
            *colon = '\0';
            if (parse_cell_ref(range, &r1, &c1) && parse_cell_ref(colon + 1, &r2, &c2)) {
                for (int r = r1; r <= r2; r++)
                    for (int c = c1; c <= c2; c++)
                        sum += get_cell_value(r, c);
            }
        } else {
            // Comma-separated like A1,A2,A3
            while (token) {
                if (parse_cell_ref(token, &r1, &c1))
                    sum += get_cell_value(r1, c1);
                token = strtok(NULL, ",:");
            }
        }
        return sum;
    }
    
    if (strcmp(fname, "AVERAGE") == 0 || strcmp(fname, "AVG") == 0) {
        double sum = 0;
        int count = 0;
        int r1, c1, r2, c2;
        
        if (strchr(args, ':')) {
            char range[MAX_FORMULA];
            strncpy(range, args, MAX_FORMULA - 1);
            char *colon = strchr(range, ':');
            *colon = '\0';
            if (parse_cell_ref(range, &r1, &c1) && parse_cell_ref(colon + 1, &r2, &c2)) {
                for (int r = r1; r <= r2; r++)
                    for (int c = c1; c <= c2; c++) {
                        sum += get_cell_value(r, c);
                        count++;
                    }
            }
        }
        return count > 0 ? sum / count : 0;
    }
    
    if (strcmp(fname, "MIN") == 0 || strcmp(fname, "MAX") == 0) {
        double result = (strcmp(fname, "MIN") == 0) ? INFINITY : -INFINITY;
        int r1, c1, r2, c2;
        
        if (strchr(args, ':')) {
            char range[MAX_FORMULA];
            strncpy(range, args, MAX_FORMULA - 1);
            char *colon = strchr(range, ':');
            *colon = '\0';
            if (parse_cell_ref(range, &r1, &c1) && parse_cell_ref(colon + 1, &r2, &c2)) {
                for (int r = r1; r <= r2; r++)
                    for (int c = c1; c <= c2; c++) {
                        double v = get_cell_value(r, c);
                        if (strcmp(fname, "MIN") == 0)
                            result = (v < result) ? v : result;
                        else
                            result = (v > result) ? v : result;
                    }
            }
        }
        return (result == INFINITY || result == -INFINITY) ? 0 : result;
    }
    
    return 0;
}

double eval_formula(const char *formula) {
    char f[MAX_FORMULA];
    strncpy(f, formula, MAX_FORMULA - 1);
    
    // Skip leading =
    char *expr = f;
    if (expr[0] == '=') expr++;
    
    // Trim whitespace
    while (isspace(*expr)) expr++;
    
    // Check for function call
    if (isalpha(expr[0])) {
        char fname[32] = {0};
        int i = 0;
        while (isalpha(expr[i]) && i < 31) {
            fname[i] = toupper(expr[i]);
            i++;
        }
        fname[i] = '\0';
        
        if (expr[i] == '(') {
            char *args_start = expr + i + 1;
            char *args_end = strchr(args_start, ')');
            if (args_end) {
                *args_end = '\0';
                return eval_function(fname, args_start);
            }
        }
        
        // Cell reference
        int r, c;
        if (parse_cell_ref(expr, &r, &c))
            return get_cell_value(r, c);
    }
    
    // Simple arithmetic parser
    double result = 0;
    char op = '+';
    char *p = expr;
    
    while (*p) {
        while (isspace(*p)) p++;
        
        double value = 0;
        if (isdigit(*p) || *p == '-' || *p == '.') {
            value = strtod(p, &p);
        } else if (isalpha(*p)) {
            int r, c;
            char ref[16];
            int i = 0;
            while ((isalnum(*p) || *p == '$') && i < 15) {
                ref[i++] = *p++;
            }
            ref[i] = '\0';
            if (parse_cell_ref(ref, &r, &c))
                value = get_cell_value(r, c);
        }
        
        switch (op) {
            case '+': result += value; break;
            case '-': result -= value; break;
            case '*': result *= value; break;
            case '/': result = (value != 0) ? result / value : 0; break;
        }
        
        while (isspace(*p)) p++;
        if (*p && strchr("+-*/", *p))
            op = *p++;
        else
            break;
    }
    
    return result;
}

void set_cell(int row, int col, const char *input) {
    Cell *c = &sheet[row][col];
    
    if (strlen(input) == 0) {
        c->type = EMPTY;
        return;
    }
    
    if (input[0] == '=') {
        c->type = FORMULA;
        strncpy(c->formula, input, MAX_FORMULA - 1);
        c->number = eval_formula(input);
    } else if (isdigit(input[0]) || input[0] == '-' || input[0] == '.') {
        c->type = NUMBER;
        c->number = atof(input);
    } else {
        c->type = TEXT;
        strncpy(c->text, input, MAX_FORMULA - 1);
    }
}

void recalculate() {
    for (int i = 0; i < ROWS; i++)
        for (int j = 0; j < COLS; j++)
            if (sheet[i][j].type == FORMULA)
                sheet[i][j].number = eval_formula(sheet[i][j].formula);
}

void display_sheet(int start_row, int start_col) {
    clear_screen();
    printf("Mini Spreadsheet | Cell: %c%d | [q]uit [e]dit [s]ave [l]oad [arrows] move\n\n",
           'A' + cursor_col, cursor_row + 1);
    
    // Column headers
    printf("     ");
    for (int j = start_col; j < start_col + 8 && j < COLS; j++)
        printf("%-*c ", CELL_WIDTH, 'A' + j);
    printf("\n");
    
    // Rows
    for (int i = start_row; i < start_row + 20 && i < ROWS; i++) {
        printf("%3d  ", i + 1);
        for (int j = start_col; j < start_col + 8 && j < COLS; j++) {
            Cell *c = &sheet[i][j];
            
            if (i == cursor_row && j == cursor_col) printf("[");
            else printf(" ");
            
            if (c->type == EMPTY)
                printf("%-*s", CELL_WIDTH - 1, "");
            else if (c->type == NUMBER || c->type == FORMULA)
                printf("%-*.2f", CELL_WIDTH - 1, c->number);
            else
                printf("%-*.*s", CELL_WIDTH - 1, CELL_WIDTH - 1, c->text);
            
            if (i == cursor_row && j == cursor_col) printf("]");
            else printf(" ");
        }
        printf("\n");
    }
    
    // Show current cell formula if applicable
    Cell *curr = &sheet[cursor_row][cursor_col];
    if (curr->type == FORMULA)
        printf("\nFormula: %s", curr->formula);
}

void save_sheet(const char *filename) {
    FILE *f = fopen(filename, "w");
    if (!f) return;
    
    for (int i = 0; i < ROWS; i++) {
        for (int j = 0; j < COLS; j++) {
            Cell *c = &sheet[i][j];
            if (c->type != EMPTY) {
                fprintf(f, "%d,%d,%d,", i, j, c->type);
                if (c->type == FORMULA)
                    fprintf(f, "%s\n", c->formula);
                else if (c->type == NUMBER)
                    fprintf(f, "%.10g\n", c->number);
                else
                    fprintf(f, "%s\n", c->text);
            }
        }
    }
    fclose(f);
}

void load_sheet(const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) return;
    
    init_sheet();
    char line[MAX_FORMULA + 32];
    while (fgets(line, sizeof(line), f)) {
        int row, col, type;
        if (sscanf(line, "%d,%d,%d,", &row, &col, &type) == 3) {
            char *data = strchr(strchr(strchr(line, ',') + 1, ',') + 1, ',') + 1;
            data[strcspn(data, "\n")] = 0;
            
            if (row >= 0 && row < ROWS && col >= 0 && col < COLS) {
                sheet[row][col].type = type;
                if (type == FORMULA) {
                    strncpy(sheet[row][col].formula, data, MAX_FORMULA - 1);
                } else if (type == NUMBER) {
                    sheet[row][col].number = atof(data);
                } else {
                    strncpy(sheet[row][col].text, data, MAX_FORMULA - 1);
                }
            }
        }
    }
    fclose(f);
    recalculate();
}

int main() {
    init_sheet();
    int start_row = 0, start_col = 0;
    
    // Sample data
    set_cell(0, 0, "Sales");
    set_cell(1, 0, "100");
    set_cell(2, 0, "200");
    set_cell(3, 0, "300");
    set_cell(4, 0, "=SUM(A2:A4)");
    
    while (1) {
        recalculate();
        display_sheet(start_row, start_col);
        
        char cmd = getchar();
        while (getchar() != '\n'); // Clear input buffer
        
        switch (cmd) {
            case 'q': return 0;
            case 'w': if (cursor_row > 0) cursor_row--; break;
            case 's': if (cursor_row < ROWS - 1) cursor_row++; break;
            case 'a': if (cursor_col > 0) cursor_col--; break;
            case 'd': if (cursor_col < COLS - 1) cursor_col++; break;
            case 'e': {
                printf("Enter value: ");
                char input[MAX_FORMULA];
                fgets(input, MAX_FORMULA, stdin);
                input[strcspn(input, "\n")] = 0;
                set_cell(cursor_row, cursor_col, input);
                break;
            }
            case 'S': {
                printf("Filename to save: ");
                char fn[256];
                fgets(fn, 256, stdin);
                fn[strcspn(fn, "\n")] = 0;
                save_sheet(fn);
                break;
            }
            case 'L': {
                printf("Filename to load: ");
                char fn[256];
                fgets(fn, 256, stdin);
                fn[strcspn(fn, "\n")] = 0;
                load_sheet(fn);
                break;
            }
        }
        
        // Auto-scroll
        if (cursor_row < start_row) start_row = cursor_row;
        if (cursor_row >= start_row + 20) start_row = cursor_row - 19;
        if (cursor_col < start_col) start_col = cursor_col;
        if (cursor_col >= start_col + 8) start_col = cursor_col - 7;
    }
    
    return 0;
}
