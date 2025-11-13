#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#define MAX_ROWS 100
#define MAX_COLS 26
#define CELL_WIDTH 12
#define MAX_FORMULA_LEN 256

typedef struct {
    char formula[MAX_FORMULA_LEN];
    double value;
    int is_formula;
} Cell;

Cell sheet[MAX_ROWS][MAX_COLS];
int cur_row = 0, cur_col = 0;
int view_row = 0, view_col = 0;

// Function prototypes
void init_sheet();
void display_sheet();
void edit_cell();
double eval_formula(const char *formula, int row, int col);
double eval_expr(const char *expr, int row, int col);
int parse_cell_ref(const char *ref, int *row, int *col);
void recalculate_all();
void save_sheet(const char *filename);
void load_sheet(const char *filename);
void print_help();
char *get_input(char *buffer, int size);
void clear_screen();
void move_cursor(int row, int col);

// Initialize all cells
void init_sheet() {
    for (int i = 0; i < MAX_ROWS; i++) {
        for (int j = 0; j < MAX_COLS; j++) {
            sheet[i][j].formula[0] = '\0';
            sheet[i][j].value = 0.0;
            sheet[i][j].is_formula = 0;
        }
    }
}

// Clear screen using ANSI codes
void clear_screen() {
    printf("\033[2J\033[H");
}

// Display the spreadsheet
void display_sheet() {
    clear_screen();
    
    printf("=== SIMPLE SPREADSHEET ===\n");
    printf("Cell: %c%d | Commands: (e)dit (s)ave (l)oad (q)uit (h)elp | Arrow keys/hjkl to move\n\n",
           'A' + cur_col, cur_row + 1);
    
    // Column headers
    printf("    ");
    for (int j = view_col; j < view_col + 7 && j < MAX_COLS; j++) {
        printf("%-*c ", CELL_WIDTH, 'A' + j);
    }
    printf("\n");
    
    // Rows
    for (int i = view_row; i < view_row + 20 && i < MAX_ROWS; i++) {
        printf("%-3d ", i + 1);
        for (int j = view_col; j < view_col + 7 && j < MAX_COLS; j++) {
            char display[32];
            if (sheet[i][j].is_formula) {
                snprintf(display, sizeof(display), "%.2f", sheet[i][j].value);
            } else if (sheet[i][j].formula[0] != '\0') {
                snprintf(display, sizeof(display), "%s", sheet[i][j].formula);
            } else {
                display[0] = '\0';
            }
            
            // Highlight current cell
            if (i == cur_row && j == cur_col) {
                printf("\033[7m%-*.*s\033[0m ", CELL_WIDTH, CELL_WIDTH, display);
            } else {
                printf("%-*.*s ", CELL_WIDTH, CELL_WIDTH, display);
            }
        }
        printf("\n");
    }
    
    // Show current cell content
    printf("\nCurrent cell content: ");
    if (sheet[cur_row][cur_col].formula[0] != '\0') {
        printf("%s", sheet[cur_row][cur_col].formula);
        if (sheet[cur_row][cur_col].is_formula) {
            printf(" = %.2f", sheet[cur_row][cur_col].value);
        }
    }
    printf("\n");
}

// Parse cell reference like "A1" into row and column
int parse_cell_ref(const char *ref, int *row, int *col) {
    if (!ref || !isalpha(ref[0])) return 0;
    
    *col = toupper(ref[0]) - 'A';
    *row = atoi(ref + 1) - 1;
    
    if (*col < 0 || *col >= MAX_COLS || *row < 0 || *row >= MAX_ROWS) {
        return 0;
    }
    return 1;
}

// Evaluate a simple expression with cell references
double eval_expr(const char *expr, int calling_row, int calling_col) {
    char buf[MAX_FORMULA_LEN];
    strncpy(buf, expr, MAX_FORMULA_LEN - 1);
    buf[MAX_FORMULA_LEN - 1] = '\0';
    
    // Remove spaces
    char clean[MAX_FORMULA_LEN];
    int k = 0;
    for (int i = 0; buf[i]; i++) {
        if (buf[i] != ' ') clean[k++] = buf[i];
    }
    clean[k] = '\0';
    
    // Simple operator precedence parser
    // For this simple version, we'll evaluate left to right with basic precedence
    // Handle cell references first
    char processed[MAX_FORMULA_LEN] = "";
    int i = 0, p = 0;
    
    while (clean[i]) {
        if (isalpha(clean[i])) {
            // Possible cell reference
            char ref[10];
            int r = 0;
            while (isalnum(clean[i]) && r < 9) {
                ref[r++] = clean[i++];
            }
            ref[r] = '\0';
            
            int row, col;
            if (parse_cell_ref(ref, &row, &col)) {
                char num[32];
                snprintf(num, sizeof(num), "%.10f", sheet[row][col].value);
                strcpy(processed + p, num);
                p += strlen(num);
            } else {
                strcpy(processed + p, ref);
                p += strlen(ref);
            }
        } else {
            processed[p++] = clean[i++];
        }
    }
    processed[p] = '\0';
    
    // Simple arithmetic evaluation (left to right, * and / before + and -)
    double result = 0;
    char op = '+';
    i = 0;
    
    while (processed[i]) {
        double num = 0;
        int negative = 0;
        
        if (processed[i] == '-' && (i == 0 || processed[i-1] == '(' || 
            processed[i-1] == '+' || processed[i-1] == '-' || 
            processed[i-1] == '*' || processed[i-1] == '/')) {
            negative = 1;
            i++;
        }
        
        if (processed[i] == '(') {
            // Find matching closing parenthesis
            int depth = 1, start = i + 1, end;
            i++;
            while (processed[i] && depth > 0) {
                if (processed[i] == '(') depth++;
                if (processed[i] == ')') depth--;
                i++;
            }
            end = i - 1;
            char sub[MAX_FORMULA_LEN];
            strncpy(sub, processed + start, end - start);
            sub[end - start] = '\0';
            num = eval_expr(sub, calling_row, calling_col);
        } else {
            num = atof(processed + i);
            while (processed[i] && (isdigit(processed[i]) || processed[i] == '.')) {
                i++;
            }
        }
        
        if (negative) num = -num;
        
        // Apply pending operation
        switch (op) {
            case '+': result += num; break;
            case '-': result -= num; break;
            case '*': result *= num; break;
            case '/': result = (num != 0) ? result / num : 0; break;
        }
        
        if (processed[i] && strchr("+-*/", processed[i])) {
            // Simple precedence: evaluate * and / immediately
            if (processed[i] == '*' || processed[i] == '/') {
                op = processed[i++];
                continue;
            }
            op = processed[i++];
        }
    }
    
    return result;
}

// Evaluate formulas like =SUM(A1:A10), =A1+B2, etc.
double eval_formula(const char *formula, int row, int col) {
    if (formula[0] != '=') {
        return atof(formula);
    }
    
    const char *expr = formula + 1;
    
    // Handle functions
    if (strncmp(expr, "SUM(", 4) == 0) {
        const char *args = expr + 4;
        char range[64];
        int i = 0;
        while (args[i] && args[i] != ')' && i < 63) {
            range[i] = args[i];
            i++;
        }
        range[i] = '\0';
        
        // Parse range like A1:A10
        char *colon = strchr(range, ':');
        if (colon) {
            *colon = '\0';
            int r1, c1, r2, c2;
            if (parse_cell_ref(range, &r1, &c1) && parse_cell_ref(colon + 1, &r2, &c2)) {
                double sum = 0;
                for (int ri = (r1 < r2 ? r1 : r2); ri <= (r1 > r2 ? r1 : r2); ri++) {
                    for (int ci = (c1 < c2 ? c1 : c2); ci <= (c1 > c2 ? c1 : c2); ci++) {
                        sum += sheet[ri][ci].value;
                    }
                }
                return sum;
            }
        }
        return 0;
    }
    
    if (strncmp(expr, "AVG(", 4) == 0 || strncmp(expr, "AVERAGE(", 8) == 0) {
        const char *args = strchr(expr, '(') + 1;
        char range[64];
        int i = 0;
        while (args[i] && args[i] != ')' && i < 63) {
            range[i] = args[i];
            i++;
        }
        range[i] = '\0';
        
        char *colon = strchr(range, ':');
        if (colon) {
            *colon = '\0';
            int r1, c1, r2, c2;
            if (parse_cell_ref(range, &r1, &c1) && parse_cell_ref(colon + 1, &r2, &c2)) {
                double sum = 0;
                int count = 0;
                for (int ri = (r1 < r2 ? r1 : r2); ri <= (r1 > r2 ? r1 : r2); ri++) {
                    for (int ci = (c1 < c2 ? c1 : c2); ci <= (c1 > c2 ? c1 : c2); ci++) {
                        sum += sheet[ri][ci].value;
                        count++;
                    }
                }
                return count > 0 ? sum / count : 0;
            }
        }
        return 0;
    }
    
    if (strncmp(expr, "MIN(", 4) == 0) {
        const char *args = expr + 4;
        char range[64];
        int i = 0;
        while (args[i] && args[i] != ')' && i < 63) {
            range[i] = args[i];
            i++;
        }
        range[i] = '\0';
        
        char *colon = strchr(range, ':');
        if (colon) {
            *colon = '\0';
            int r1, c1, r2, c2;
            if (parse_cell_ref(range, &r1, &c1) && parse_cell_ref(colon + 1, &r2, &c2)) {
                double min_val = INFINITY;
                for (int ri = (r1 < r2 ? r1 : r2); ri <= (r1 > r2 ? r1 : r2); ri++) {
                    for (int ci = (c1 < c2 ? c1 : c2); ci <= (c1 > c2 ? c1 : c2); ci++) {
                        if (sheet[ri][ci].value < min_val) min_val = sheet[ri][ci].value;
                    }
                }
                return min_val == INFINITY ? 0 : min_val;
            }
        }
        return 0;
    }
    
    if (strncmp(expr, "MAX(", 4) == 0) {
        const char *args = expr + 4;
        char range[64];
        int i = 0;
        while (args[i] && args[i] != ')' && i < 63) {
            range[i] = args[i];
            i++;
        }
        range[i] = '\0';
        
        char *colon = strchr(range, ':');
        if (colon) {
            *colon = '\0';
            int r1, c1, r2, c2;
            if (parse_cell_ref(range, &r1, &c1) && parse_cell_ref(colon + 1, &r2, &c2)) {
                double max_val = -INFINITY;
                for (int ri = (r1 < r2 ? r1 : r2); ri <= (r1 > r2 ? r1 : r2); ri++) {
                    for (int ci = (c1 < c2 ? c1 : c2); ci <= (c1 > c2 ? c1 : c2); ci++) {
                        if (sheet[ri][ci].value > max_val) max_val = sheet[ri][ci].value;
                    }
                }
                return max_val == -INFINITY ? 0 : max_val;
            }
        }
        return 0;
    }
    
    // Arithmetic expression
    return eval_expr(expr, row, col);
}

// Recalculate all formulas
void recalculate_all() {
    // First pass: update all non-formula values
    for (int i = 0; i < MAX_ROWS; i++) {
        for (int j = 0; j < MAX_COLS; j++) {
            if (!sheet[i][j].is_formula && sheet[i][j].formula[0] != '\0') {
                sheet[i][j].value = atof(sheet[i][j].formula);
            }
        }
    }
    
    // Second pass: evaluate formulas (simple approach, doesn't handle circular refs)
    for (int pass = 0; pass < 3; pass++) {  // Multiple passes for dependencies
        for (int i = 0; i < MAX_ROWS; i++) {
            for (int j = 0; j < MAX_COLS; j++) {
                if (sheet[i][j].is_formula) {
                    sheet[i][j].value = eval_formula(sheet[i][j].formula, i, j);
                }
            }
        }
    }
}

// Edit current cell
void edit_cell() {
    char input[MAX_FORMULA_LEN];
    printf("\nEnter cell content (or empty to clear): ");
    fflush(stdout);
    
    if (fgets(input, sizeof(input), stdin)) {
        // Remove newline
        input[strcspn(input, "\n")] = 0;
        
        strncpy(sheet[cur_row][cur_col].formula, input, MAX_FORMULA_LEN - 1);
        sheet[cur_row][cur_col].formula[MAX_FORMULA_LEN - 1] = '\0';
        
        if (input[0] == '=') {
            sheet[cur_row][cur_col].is_formula = 1;
        } else {
            sheet[cur_row][cur_col].is_formula = 0;
            sheet[cur_row][cur_col].value = atof(input);
        }
        
        recalculate_all();
    }
}

// Save spreadsheet to file
void save_sheet(const char *filename) {
    FILE *f = fopen(filename, "w");
    if (!f) {
        printf("Error: Cannot open file for writing.\n");
        return;
    }
    
    for (int i = 0; i < MAX_ROWS; i++) {
        for (int j = 0; j < MAX_COLS; j++) {
            if (sheet[i][j].formula[0] != '\0') {
                fprintf(f, "%d,%d,%s\n", i, j, sheet[i][j].formula);
            }
        }
    }
    
    fclose(f);
    printf("Saved to %s. Press Enter to continue...", filename);
    getchar();
}

// Load spreadsheet from file
void load_sheet(const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        printf("Error: Cannot open file for reading.\n");
        return;
    }
    
    init_sheet();
    
    char line[MAX_FORMULA_LEN + 32];
    while (fgets(line, sizeof(line), f)) {
        int row, col;
        char formula[MAX_FORMULA_LEN];
        
        if (sscanf(line, "%d,%d,", &row, &col) == 2) {
            char *comma = strchr(line, ',');
            if (comma) {
                comma = strchr(comma + 1, ',');
                if (comma) {
                    strncpy(formula, comma + 1, MAX_FORMULA_LEN - 1);
                    formula[MAX_FORMULA_LEN - 1] = '\0';
                    formula[strcspn(formula, "\n")] = 0;
                    
                    if (row >= 0 && row < MAX_ROWS && col >= 0 && col < MAX_COLS) {
                        strncpy(sheet[row][col].formula, formula, MAX_FORMULA_LEN - 1);
                        sheet[row][col].is_formula = (formula[0] == '=');
                    }
                }
            }
        }
    }
    
    fclose(f);
    recalculate_all();
    printf("Loaded from %s. Press Enter to continue...", filename);
    getchar();
}

// Print help
void print_help() {
    clear_screen();
    printf("=== SPREADSHEET HELP ===\n\n");
    printf("NAVIGATION:\n");
    printf("  Arrow keys or h/j/k/l  - Move cursor\n");
    printf("  e                      - Edit current cell\n");
    printf("  q                      - Quit\n\n");
    printf("COMMANDS:\n");
    printf("  s                      - Save to file\n");
    printf("  l                      - Load from file\n");
    printf("  h                      - Show this help\n\n");
    printf("FORMULAS:\n");
    printf("  =A1+B2                 - Add cells\n");
    printf("  =A1-B2                 - Subtract cells\n");
    printf("  =A1*B2                 - Multiply cells\n");
    printf("  =A1/B2                 - Divide cells\n");
    printf("  =SUM(A1:A10)           - Sum range\n");
    printf("  =AVG(A1:A10)           - Average range\n");
    printf("  =MIN(A1:A10)           - Minimum in range\n");
    printf("  =MAX(A1:A10)           - Maximum in range\n");
    printf("  =(A1+B2)*C3            - Use parentheses\n\n");
    printf("Press Enter to continue...");
    getchar();
}

int main() {
    init_sheet();
    
    // Set up some example data
    strcpy(sheet[0][0].formula, "100");
    sheet[0][0].value = 100;
    strcpy(sheet[1][0].formula, "200");
    sheet[1][0].value = 200;
    strcpy(sheet[2][0].formula, "300");
    sheet[2][0].value = 300;
    strcpy(sheet[3][0].formula, "=SUM(A1:A3)");
    sheet[3][0].is_formula = 1;
    
    recalculate_all();
    
    char cmd;
    while (1) {
        display_sheet();
        
        printf("\nCommand: ");
        cmd = getchar();
        
        // Clear input buffer
        int c;
        while ((c = getchar()) != '\n' && c != EOF);
        
        switch (cmd) {
            case 'q': case 'Q':
                return 0;
                
            case 'e': case 'E':
                edit_cell();
                break;
                
            case 'h':
                cur_col = (cur_col > 0) ? cur_col - 1 : 0;
                if (cur_col < view_col) view_col = cur_col;
                break;
                
            case 'l':
                cur_col = (cur_col < MAX_COLS - 1) ? cur_col + 1 : MAX_COLS - 1;
                if (cur_col >= view_col + 7) view_col = cur_col - 6;
                break;
                
            case 'k':
                cur_row = (cur_row > 0) ? cur_row - 1 : 0;
                if (cur_row < view_row) view_row = cur_row;
                break;
                
            case 'j':
                cur_row = (cur_row < MAX_ROWS - 1) ? cur_row + 1 : MAX_ROWS - 1;
                if (cur_row >= view_row + 20) view_row = cur_row - 19;
                break;
                
            case 's': case 'S': {
                char filename[256];
                printf("Enter filename to save: ");
                if (fgets(filename, sizeof(filename), stdin)) {
                    filename[strcspn(filename, "\n")] = 0;
                    save_sheet(filename);
                }
                break;
            }
                
            case 'L': {
                char filename[256];
                printf("Enter filename to load: ");
                if (fgets(filename, sizeof(filename), stdin)) {
                    filename[strcspn(filename, "\n")] = 0;
                    load_sheet(filename);
                }
                break;
            }
                
            case 'H':
            case '?':
                print_help();
                break;
        }
    }
    
    return 0;
}
