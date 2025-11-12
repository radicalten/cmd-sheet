/*
 * MinPlan - A Multiplan-inspired spreadsheet
 * Compile: gcc -o minplan minplan.c -lm
 * (only depends on standard C library and math library)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#define MAX_ROWS 100
#define MAX_COLS 26
#define MAX_CELL_LEN 256
#define COL_WIDTH 12

typedef struct {
    char formula[MAX_CELL_LEN];
    double value;
    int is_numeric;
    int needs_recalc;
} Cell;

typedef struct {
    Cell cells[MAX_ROWS][MAX_COLS];
    int cur_row;
    int cur_col;
    int top_row;
    char status[256];
} Sheet;

Sheet sheet;

/* Function prototypes */
void init_sheet(void);
void display_sheet(void);
void clear_screen(void);
void move_cursor(int row, int col);
void process_command(char *cmd);
void set_cell(int row, int col, const char *content);
double eval_formula(const char *formula, int cur_row, int cur_col);
double eval_expression(const char *expr, int cur_row, int cur_col);
void recalculate_all(void);
void save_sheet(const char *filename);
void load_sheet(const char *filename);
int parse_cell_ref(const char *ref, int *row, int *col);
void get_cell_display(int row, int col, char *buf);
double cell_value(int row, int col);

/* Utility functions */
void clear_screen(void) {
    printf("\033[2J\033[H");
}

void move_cursor(int row, int col) {
    printf("\033[%d;%dH", row, col);
}

void init_sheet(void) {
    for (int i = 0; i < MAX_ROWS; i++) {
        for (int j = 0; j < MAX_COLS; j++) {
            sheet.cells[i][j].formula[0] = '\0';
            sheet.cells[i][j].value = 0.0;
            sheet.cells[i][j].is_numeric = 0;
            sheet.cells[i][j].needs_recalc = 0;
        }
    }
    sheet.cur_row = 0;
    sheet.cur_col = 0;
    sheet.top_row = 0;
    strcpy(sheet.status, "MinPlan - Type /h for help");
}

void display_sheet(void) {
    clear_screen();
    
    /* Header */
    printf("MinPlan Spreadsheet - Cell: %c%d\n", 'A' + sheet.cur_col, sheet.cur_row + 1);
    
    /* Show current cell content */
    Cell *cur = &sheet.cells[sheet.cur_row][sheet.cur_col];
    printf("Content: %s\n", cur->formula[0] ? cur->formula : "(empty)");
    printf("─────────────────────────────────────────────────────────────────────────\n");
    
    /* Column headers */
    printf("    ");
    for (int j = 0; j < 7 && j < MAX_COLS; j++) {
        printf("%-*c ", COL_WIDTH, 'A' + j);
    }
    printf("\n");
    
    /* Rows */
    for (int i = 0; i < 15 && (sheet.top_row + i) < MAX_ROWS; i++) {
        int row = sheet.top_row + i;
        printf("%-3d ", row + 1);
        
        for (int j = 0; j < 7 && j < MAX_COLS; j++) {
            char buf[COL_WIDTH + 1];
            get_cell_display(row, j, buf);
            
            if (row == sheet.cur_row && j == sheet.cur_col) {
                printf("[%-*s]", COL_WIDTH - 2, buf);
            } else {
                printf("%-*s ", COL_WIDTH, buf);
            }
        }
        printf("\n");
    }
    
    printf("─────────────────────────────────────────────────────────────────────────\n");
    printf("%s\n", sheet.status);
    printf("Commands: /h=help /s=save /l=load /q=quit | Arrows: w/a/s/d | Enter to edit\n");
    printf("> ");
}

void get_cell_display(int row, int col, char *buf) {
    Cell *cell = &sheet.cells[row][col];
    
    if (cell->formula[0] == '\0') {
        buf[0] = '\0';
        return;
    }
    
    if (cell->formula[0] == '=') {
        if (cell->is_numeric) {
            snprintf(buf, COL_WIDTH, "%.2f", cell->value);
        } else {
            snprintf(buf, COL_WIDTH, "ERROR");
        }
    } else if (cell->is_numeric) {
        snprintf(buf, COL_WIDTH, "%.2f", cell->value);
    } else {
        snprintf(buf, COL_WIDTH, "%s", cell->formula);
    }
}

int parse_cell_ref(const char *ref, int *row, int *col) {
    if (!ref || !isalpha(ref[0])) return 0;
    
    *col = toupper(ref[0]) - 'A';
    *row = atoi(ref + 1) - 1;
    
    if (*row < 0 || *row >= MAX_ROWS || *col < 0 || *col >= MAX_COLS) {
        return 0;
    }
    return 1;
}

double cell_value(int row, int col) {
    if (row < 0 || row >= MAX_ROWS || col < 0 || col >= MAX_COLS) {
        return 0.0;
    }
    
    Cell *cell = &sheet.cells[row][col];
    
    if (cell->needs_recalc && cell->formula[0] == '=') {
        cell->value = eval_formula(cell->formula + 1, row, col);
        cell->is_numeric = 1;
        cell->needs_recalc = 0;
    }
    
    return cell->value;
}

double eval_expression(const char *expr, int cur_row, int cur_col) {
    char clean[MAX_CELL_LEN];
    int k = 0;
    
    /* Remove spaces */
    for (int i = 0; expr[i] && k < MAX_CELL_LEN - 1; i++) {
        if (!isspace(expr[i])) {
            clean[k++] = expr[i];
        }
    }
    clean[k] = '\0';
    
    /* Check if it's a cell reference */
    if (isalpha(clean[0])) {
        int row, col;
        if (parse_cell_ref(clean, &row, &col)) {
            return cell_value(row, col);
        }
    }
    
    /* Simple expression evaluator */
    double result = 0;
    double current_num = 0;
    char op = '+';
    int i = 0;
    
    while (clean[i]) {
        if (isdigit(clean[i]) || clean[i] == '.') {
            char num_buf[64];
            int j = 0;
            while ((isdigit(clean[i]) || clean[i] == '.') && j < 63) {
                num_buf[j++] = clean[i++];
            }
            num_buf[j] = '\0';
            current_num = atof(num_buf);
            
            if (op == '+') result += current_num;
            else if (op == '-') result -= current_num;
            else if (op == '*') result *= current_num;
            else if (op == '/') result = (current_num != 0) ? result / current_num : 0;
        } else if (isalpha(clean[i])) {
            /* Cell reference */
            char ref[10];
            int j = 0;
            while ((isalnum(clean[i])) && j < 9) {
                ref[j++] = clean[i++];
            }
            ref[j] = '\0';
            
            int row, col;
            if (parse_cell_ref(ref, &row, &col)) {
                current_num = cell_value(row, col);
                if (op == '+') result += current_num;
                else if (op == '-') result -= current_num;
                else if (op == '*') result *= current_num;
                else if (op == '/') result = (current_num != 0) ? result / current_num : 0;
            }
        } else if (strchr("+-*/", clean[i])) {
            op = clean[i++];
            if (op == '*' || op == '/') {
                /* For proper precedence, would need more complex parser */
                /* This is simplified */
            }
        } else {
            i++;
        }
    }
    
    return result;
}

double eval_formula(const char *formula, int cur_row, int cur_col) {
    char func[MAX_CELL_LEN];
    strncpy(func, formula, MAX_CELL_LEN - 1);
    func[MAX_CELL_LEN - 1] = '\0';
    
    /* Check for functions */
    if (strncmp(func, "SUM(", 4) == 0) {
        char *args = func + 4;
        char *end = strchr(args, ')');
        if (!end) return 0;
        *end = '\0';
        
        /* Parse range like A1:A10 */
        char *colon = strchr(args, ':');
        if (colon) {
            *colon = '\0';
            int r1, c1, r2, c2;
            if (parse_cell_ref(args, &r1, &c1) && parse_cell_ref(colon + 1, &r2, &c2)) {
                double sum = 0;
                for (int r = r1; r <= r2; r++) {
                    for (int c = c1; c <= c2; c++) {
                        sum += cell_value(r, c);
                    }
                }
                return sum;
            }
        }
        return 0;
    } else if (strncmp(func, "AVG(", 4) == 0 || strncmp(func, "AVERAGE(", 8) == 0) {
        char *args = func + (func[1] == 'V' ? 4 : 8);
        char *end = strchr(args, ')');
        if (!end) return 0;
        *end = '\0';
        
        char *colon = strchr(args, ':');
        if (colon) {
            *colon = '\0';
            int r1, c1, r2, c2;
            if (parse_cell_ref(args, &r1, &c1) && parse_cell_ref(colon + 1, &r2, &c2)) {
                double sum = 0;
                int count = 0;
                for (int r = r1; r <= r2; r++) {
                    for (int c = c1; c <= c2; c++) {
                        sum += cell_value(r, c);
                        count++;
                    }
                }
                return count > 0 ? sum / count : 0;
            }
        }
        return 0;
    } else if (strncmp(func, "COUNT(", 6) == 0) {
        char *args = func + 6;
        char *end = strchr(args, ')');
        if (!end) return 0;
        *end = '\0';
        
        char *colon = strchr(args, ':');
        if (colon) {
            *colon = '\0';
            int r1, c1, r2, c2;
            if (parse_cell_ref(args, &r1, &c1) && parse_cell_ref(colon + 1, &r2, &c2)) {
                int count = 0;
                for (int r = r1; r <= r2; r++) {
                    for (int c = c1; c <= c2; c++) {
                        if (sheet.cells[r][c].formula[0] != '\0') {
                            count++;
                        }
                    }
                }
                return count;
            }
        }
        return 0;
    }
    
    /* Otherwise, evaluate as expression */
    return eval_expression(formula, cur_row, cur_col);
}

void set_cell(int row, int col, const char *content) {
    if (row < 0 || row >= MAX_ROWS || col < 0 || col >= MAX_COLS) {
        return;
    }
    
    Cell *cell = &sheet.cells[row][col];
    strncpy(cell->formula, content, MAX_CELL_LEN - 1);
    cell->formula[MAX_CELL_LEN - 1] = '\0';
    
    if (content[0] == '=') {
        cell->needs_recalc = 1;
        cell->value = eval_formula(content + 1, row, col);
        cell->is_numeric = 1;
    } else {
        char *endptr;
        double val = strtod(content, &endptr);
        if (*endptr == '\0' && *content != '\0') {
            cell->value = val;
            cell->is_numeric = 1;
        } else {
            cell->is_numeric = 0;
        }
    }
}

void recalculate_all(void) {
    /* Mark all formula cells for recalculation */
    for (int i = 0; i < MAX_ROWS; i++) {
        for (int j = 0; j < MAX_COLS; j++) {
            if (sheet.cells[i][j].formula[0] == '=') {
                sheet.cells[i][j].needs_recalc = 1;
            }
        }
    }
    
    /* Recalculate all (simple approach - may need multiple passes for dependencies) */
    for (int pass = 0; pass < 3; pass++) {
        for (int i = 0; i < MAX_ROWS; i++) {
            for (int j = 0; j < MAX_COLS; j++) {
                cell_value(i, j); /* This will recalc if needed */
            }
        }
    }
}

void save_sheet(const char *filename) {
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        strcpy(sheet.status, "Error: Could not save file");
        return;
    }
    
    for (int i = 0; i < MAX_ROWS; i++) {
        for (int j = 0; j < MAX_COLS; j++) {
            if (sheet.cells[i][j].formula[0] != '\0') {
                fprintf(fp, "%d,%d,%s\n", i, j, sheet.cells[i][j].formula);
            }
        }
    }
    
    fclose(fp);
    snprintf(sheet.status, sizeof(sheet.status), "Saved to %s", filename);
}

void load_sheet(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        strcpy(sheet.status, "Error: Could not load file");
        return;
    }
    
    init_sheet();
    
    char line[MAX_CELL_LEN + 20];
    while (fgets(line, sizeof(line), fp)) {
        int row, col;
        char content[MAX_CELL_LEN];
        
        char *comma1 = strchr(line, ',');
        if (!comma1) continue;
        *comma1 = '\0';
        row = atoi(line);
        
        char *comma2 = strchr(comma1 + 1, ',');
        if (!comma2) continue;
        *comma2 = '\0';
        col = atoi(comma1 + 1);
        
        strncpy(content, comma2 + 1, MAX_CELL_LEN - 1);
        content[MAX_CELL_LEN - 1] = '\0';
        
        /* Remove newline */
        char *nl = strchr(content, '\n');
        if (nl) *nl = '\0';
        
        set_cell(row, col, content);
    }
    
    fclose(fp);
    recalculate_all();
    snprintf(sheet.status, sizeof(sheet.status), "Loaded from %s", filename);
}

void show_help(void) {
    strcpy(sheet.status, "w/s=up/down a/d=left/right Enter=edit /s=save /l=load /q=quit");
}

void process_command(char *cmd) {
    /* Remove trailing newline */
    char *nl = strchr(cmd, '\n');
    if (nl) *nl = '\0';
    
    if (cmd[0] == '\0') {
        return;
    }
    
    if (cmd[0] == '/') {
        if (strcmp(cmd, "/q") == 0 || strcmp(cmd, "/quit") == 0) {
            exit(0);
        } else if (strcmp(cmd, "/h") == 0 || strcmp(cmd, "/help") == 0) {
            show_help();
        } else if (strncmp(cmd, "/s ", 3) == 0) {
            save_sheet(cmd + 3);
        } else if (strcmp(cmd, "/s") == 0) {
            save_sheet("sheet.txt");
        } else if (strncmp(cmd, "/l ", 3) == 0) {
            load_sheet(cmd + 3);
        } else if (strcmp(cmd, "/l") == 0) {
            load_sheet("sheet.txt");
        } else {
            strcpy(sheet.status, "Unknown command");
        }
    } else if (cmd[0] == 'w') {
        if (sheet.cur_row > 0) {
            sheet.cur_row--;
            if (sheet.cur_row < sheet.top_row) {
                sheet.top_row = sheet.cur_row;
            }
        }
    } else if (cmd[0] == 's') {
        if (sheet.cur_row < MAX_ROWS - 1) {
            sheet.cur_row++;
            if (sheet.cur_row >= sheet.top_row + 15) {
                sheet.top_row = sheet.cur_row - 14;
            }
        }
    } else if (cmd[0] == 'a') {
        if (sheet.cur_col > 0) {
            sheet.cur_col--;
        }
    } else if (cmd[0] == 'd') {
        if (sheet.cur_col < MAX_COLS - 1) {
            sheet.cur_col++;
        }
    } else {
        /* Treat as cell content */
        set_cell(sheet.cur_row, sheet.cur_col, cmd);
        recalculate_all();
        snprintf(sheet.status, sizeof(sheet.status), 
                 "Set %c%d to: %s", 'A' + sheet.cur_col, sheet.cur_row + 1, cmd);
    }
}

int main(void) {
    init_sheet();
    char input[MAX_CELL_LEN];
    
    while (1) {
        display_sheet();
        
        if (fgets(input, sizeof(input), stdin)) {
            process_command(input);
        }
    }
    
    return 0;
}
