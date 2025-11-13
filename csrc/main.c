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
    int is_numeric;
} Cell;

typedef struct {
    Cell cells[MAX_ROWS][MAX_COLS];
    int current_row;
    int current_col;
    int view_row;
    int view_col;
} Spreadsheet;

Spreadsheet sheet;
char clipboard[MAX_FORMULA_LEN];

// Function prototypes
void init_spreadsheet();
void display();
void evaluate_cell(int row, int col);
double parse_expression(const char* expr, int current_row, int current_col);
void get_cell_ref(const char* ref, int* row, int* col);
void handle_input();
void save_file(const char* filename);
void load_file(const char* filename);
void clear_screen();
void move_cursor(int row, int col);

// Initialize spreadsheet
void init_spreadsheet() {
    for (int i = 0; i < MAX_ROWS; i++) {
        for (int j = 0; j < MAX_COLS; j++) {
            sheet.cells[i][j].formula[0] = '\0';
            sheet.cells[i][j].value = 0.0;
            sheet.cells[i][j].is_numeric = 0;
        }
    }
    sheet.current_row = 0;
    sheet.current_col = 0;
    sheet.view_row = 0;
    sheet.view_col = 0;
    clipboard[0] = '\0';
}

// Clear screen (cross-platform approach)
void clear_screen() {
    #ifdef _WIN32
        system("cls");
    #else
        system("clear");
    #endif
}

// Convert column number to letter (0->A, 1->B, etc.)
void col_to_letter(int col, char* letter) {
    letter[0] = 'A' + col;
    letter[1] = '\0';
}

// Convert cell reference (like "A1") to row and column
void get_cell_ref(const char* ref, int* row, int* col) {
    *col = toupper(ref[0]) - 'A';
    *row = atoi(ref + 1) - 1;
}

// Check if string is a cell reference
int is_cell_ref(const char* str) {
    if (!isalpha(str[0])) return 0;
    for (int i = 1; str[i]; i++) {
        if (!isdigit(str[i])) return 0;
    }
    return 1;
}

// Get value from cell reference
double get_cell_value(const char* ref) {
    int row, col;
    get_cell_ref(ref, &row, &col);
    if (row >= 0 && row < MAX_ROWS && col >= 0 && col < MAX_COLS) {
        evaluate_cell(row, col);
        return sheet.cells[row][col].value;
    }
    return 0.0;
}

// Parse SUM function: SUM(A1:A5)
double parse_sum(const char* args) {
    char start[10], end[10];
    sscanf(args, "%[^:]:%s", start, end);
    
    int r1, c1, r2, c2;
    get_cell_ref(start, &r1, &c1);
    get_cell_ref(end, &r2, &c2);
    
    double sum = 0.0;
    for (int i = r1; i <= r2; i++) {
        for (int j = c1; j <= c2; j++) {
            if (i >= 0 && i < MAX_ROWS && j >= 0 && j < MAX_COLS) {
                evaluate_cell(i, j);
                sum += sheet.cells[i][j].value;
            }
        }
    }
    return sum;
}

// Parse AVG function: AVG(A1:A5)
double parse_avg(const char* args) {
    char start[10], end[10];
    sscanf(args, "%[^:]:%s", start, end);
    
    int r1, c1, r2, c2;
    get_cell_ref(start, &r1, &c1);
    get_cell_ref(end, &r2, &c2);
    
    double sum = 0.0;
    int count = 0;
    for (int i = r1; i <= r2; i++) {
        for (int j = c1; j <= c2; j++) {
            if (i >= 0 && i < MAX_ROWS && j >= 0 && j < MAX_COLS) {
                evaluate_cell(i, j);
                sum += sheet.cells[i][j].value;
                count++;
            }
        }
    }
    return count > 0 ? sum / count : 0.0;
}

// Parse MIN function
double parse_min(const char* args) {
    char start[10], end[10];
    sscanf(args, "%[^:]:%s", start, end);
    
    int r1, c1, r2, c2;
    get_cell_ref(start, &r1, &c1);
    get_cell_ref(end, &r2, &c2);
    
    double min_val = INFINITY;
    for (int i = r1; i <= r2; i++) {
        for (int j = c1; j <= c2; j++) {
            if (i >= 0 && i < MAX_ROWS && j >= 0 && j < MAX_COLS) {
                evaluate_cell(i, j);
                if (sheet.cells[i][j].value < min_val) {
                    min_val = sheet.cells[i][j].value;
                }
            }
        }
    }
    return min_val == INFINITY ? 0.0 : min_val;
}

// Parse MAX function
double parse_max(const char* args) {
    char start[10], end[10];
    sscanf(args, "%[^:]:%s", start, end);
    
    int r1, c1, r2, c2;
    get_cell_ref(start, &r1, &c1);
    get_cell_ref(end, &r2, &c2);
    
    double max_val = -INFINITY;
    for (int i = r1; i <= r2; i++) {
        for (int j = c1; j <= c2; j++) {
            if (i >= 0 && i < MAX_ROWS && j >= 0 && j < MAX_COLS) {
                evaluate_cell(i, j);
                if (sheet.cells[i][j].value > max_val) {
                    max_val = sheet.cells[i][j].value;
                }
            }
        }
    }
    return max_val == -INFINITY ? 0.0 : max_val;
}

// Simple expression parser (handles +, -, *, /, cell references)
double parse_expression(const char* expr, int current_row, int current_col) {
    char temp[MAX_FORMULA_LEN];
    strncpy(temp, expr, MAX_FORMULA_LEN - 1);
    temp[MAX_FORMULA_LEN - 1] = '\0';
    
    // Remove spaces
    char clean[MAX_FORMULA_LEN];
    int k = 0;
    for (int i = 0; temp[i]; i++) {
        if (temp[i] != ' ') clean[k++] = temp[i];
    }
    clean[k] = '\0';
    
    // Check for functions
    if (strncmp(clean, "SUM(", 4) == 0) {
        char args[MAX_FORMULA_LEN];
        sscanf(clean + 4, "%[^)]", args);
        return parse_sum(args);
    }
    if (strncmp(clean, "AVG(", 4) == 0) {
        char args[MAX_FORMULA_LEN];
        sscanf(clean + 4, "%[^)]", args);
        return parse_avg(args);
    }
    if (strncmp(clean, "MIN(", 4) == 0) {
        char args[MAX_FORMULA_LEN];
        sscanf(clean + 4, "%[^)]", args);
        return parse_min(args);
    }
    if (strncmp(clean, "MAX(", 4) == 0) {
        char args[MAX_FORMULA_LEN];
        sscanf(clean + 4, "%[^)]", args);
        return parse_max(args);
    }
    
    // Simple arithmetic parser
    double result = 0.0;
    char* token;
    char* rest = clean;
    char op = '+';
    
    while (*rest) {
        char term[MAX_FORMULA_LEN];
        int i = 0;
        
        // Get next term
        while (*rest && *rest != '+' && *rest != '-') {
            term[i++] = *rest++;
        }
        term[i] = '\0';
        
        // Parse term (handle * and /)
        double term_val = 0.0;
        char* term_rest = term;
        char term_op = '*';
        double factor = 1.0;
        
        while (*term_rest) {
            char factor_str[MAX_FORMULA_LEN];
            int j = 0;
            
            while (*term_rest && *term_rest != '*' && *term_rest != '/') {
                factor_str[j++] = *term_rest++;
            }
            factor_str[j] = '\0';
            
            // Check if it's a cell reference
            if (is_cell_ref(factor_str)) {
                factor = get_cell_value(factor_str);
            } else {
                factor = atof(factor_str);
            }
            
            if (term_op == '*') {
                term_val = (term_rest == term + j) ? factor : term_val * factor;
            } else if (term_op == '/') {
                term_val = factor != 0 ? term_val / factor : 0;
            }
            
            if (*term_rest) {
                term_op = *term_rest++;
            }
        }
        
        if (op == '+') {
            result += term_val;
        } else if (op == '-') {
            result -= term_val;
        }
        
        if (*rest) {
            op = *rest++;
        }
    }
    
    return result;
}

// Evaluate a cell's formula
void evaluate_cell(int row, int col) {
    Cell* cell = &sheet.cells[row][col];
    
    if (cell->formula[0] == '\0') {
        cell->value = 0.0;
        cell->is_numeric = 0;
        return;
    }
    
    if (cell->formula[0] == '=') {
        // It's a formula
        cell->value = parse_expression(cell->formula + 1, row, col);
        cell->is_numeric = 1;
    } else {
        // Try to parse as number
        char* endptr;
        double val = strtod(cell->formula, &endptr);
        if (*endptr == '\0' && endptr != cell->formula) {
            cell->value = val;
            cell->is_numeric = 1;
        } else {
            cell->is_numeric = 0;
        }
    }
}

// Display the spreadsheet
void display() {
    clear_screen();
    
    printf("Simple Spreadsheet - Current: %c%d\n", 
           'A' + sheet.current_col, sheet.current_row + 1);
    printf("Commands: [E]dit [S]ave [L]oad [C]opy [P]aste [Q]uit | Arrow keys to navigate\n");
    printf("Formula: %s\n", sheet.cells[sheet.current_row][sheet.current_col].formula);
    printf("\n");
    
    // Header
    printf("    ");
    for (int j = sheet.view_col; j < sheet.view_col + 7 && j < MAX_COLS; j++) {
        printf("%-*c", CELL_WIDTH, 'A' + j);
    }
    printf("\n");
    
    // Rows
    for (int i = sheet.view_row; i < sheet.view_row + 20 && i < MAX_ROWS; i++) {
        printf("%-3d ", i + 1);
        for (int j = sheet.view_col; j < sheet.view_col + 7 && j < MAX_COLS; j++) {
            char marker = (i == sheet.current_row && j == sheet.current_col) ? '>' : ' ';
            
            evaluate_cell(i, j);
            Cell* cell = &sheet.cells[i][j];
            
            if (cell->formula[0] == '\0') {
                printf("%c%-*s", marker, CELL_WIDTH - 1, "");
            } else if (cell->is_numeric) {
                printf("%c%-*.2f", marker, CELL_WIDTH - 1, cell->value);
            } else {
                char display[CELL_WIDTH];
                strncpy(display, cell->formula, CELL_WIDTH - 1);
                display[CELL_WIDTH - 1] = '\0';
                printf("%c%-*s", marker, CELL_WIDTH - 1, display);
            }
        }
        printf("\n");
    }
}

// Save to file
void save_file(const char* filename) {
    FILE* fp = fopen(filename, "w");
    if (!fp) {
        printf("Error: Cannot open file for writing.\n");
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
    printf("Saved to %s\n", filename);
}

// Load from file
void load_file(const char* filename) {
    FILE* fp = fopen(filename, "r");
    if (!fp) {
        printf("Error: Cannot open file for reading.\n");
        return;
    }
    
    init_spreadsheet();
    
    char line[MAX_FORMULA_LEN + 20];
    while (fgets(line, sizeof(line), fp)) {
        int row, col;
        char formula[MAX_FORMULA_LEN];
        
        char* comma1 = strchr(line, ',');
        char* comma2 = strchr(comma1 + 1, ',');
        
        if (comma1 && comma2) {
            *comma1 = '\0';
            *comma2 = '\0';
            
            row = atoi(line);
            col = atoi(comma1 + 1);
            strncpy(formula, comma2 + 1, MAX_FORMULA_LEN - 1);
            formula[MAX_FORMULA_LEN - 1] = '\0';
            
            // Remove newline
            formula[strcspn(formula, "\n")] = '\0';
            
            if (row >= 0 && row < MAX_ROWS && col >= 0 && col < MAX_COLS) {
                strncpy(sheet.cells[row][col].formula, formula, MAX_FORMULA_LEN - 1);
            }
        }
    }
    
    fclose(fp);
    printf("Loaded from %s\n", filename);
}

// Handle user input
void handle_input() {
    char cmd;
    printf("\nCommand: ");
    scanf(" %c", &cmd);
    
    switch (toupper(cmd)) {
        case 'E': { // Edit cell
            char input[MAX_FORMULA_LEN];
            printf("Enter value/formula: ");
            scanf(" %[^\n]", input);
            strncpy(sheet.cells[sheet.current_row][sheet.current_col].formula, 
                   input, MAX_FORMULA_LEN - 1);
            sheet.cells[sheet.current_row][sheet.current_col].formula[MAX_FORMULA_LEN - 1] = '\0';
            break;
        }
        case 'S': { // Save
            char filename[256];
            printf("Filename: ");
            scanf("%s", filename);
            save_file(filename);
            printf("Press Enter to continue...");
            getchar();
            getchar();
            break;
        }
        case 'L': { // Load
            char filename[256];
            printf("Filename: ");
            scanf("%s", filename);
            load_file(filename);
            printf("Press Enter to continue...");
            getchar();
            getchar();
            break;
        }
        case 'C': { // Copy
            strncpy(clipboard, sheet.cells[sheet.current_row][sheet.current_col].formula, 
                   MAX_FORMULA_LEN - 1);
            clipboard[MAX_FORMULA_LEN - 1] = '\0';
            printf("Copied!\n");
            printf("Press Enter to continue...");
            getchar();
            getchar();
            break;
        }
        case 'P': { // Paste
            strncpy(sheet.cells[sheet.current_row][sheet.current_col].formula, 
                   clipboard, MAX_FORMULA_LEN - 1);
            sheet.cells[sheet.current_row][sheet.current_col].formula[MAX_FORMULA_LEN - 1] = '\0';
            printf("Pasted!\n");
            printf("Press Enter to continue...");
            getchar();
            getchar();
            break;
        }
        case 'W': // Up
            if (sheet.current_row > 0) sheet.current_row--;
            if (sheet.current_row < sheet.view_row) sheet.view_row = sheet.current_row;
            break;
        case 'A': // Left
            if (sheet.current_col > 0) sheet.current_col--;
            if (sheet.current_col < sheet.view_col) sheet.view_col = sheet.current_col;
            break;
        case 'X': // Down
            if (sheet.current_row < MAX_ROWS - 1) sheet.current_row++;
            if (sheet.current_row >= sheet.view_row + 20) sheet.view_row = sheet.current_row - 19;
            break;
        case 'D': // Right
            if (sheet.current_col < MAX_COLS - 1) sheet.current_col++;
            if (sheet.current_col >= sheet.view_col + 7) sheet.view_col = sheet.current_col - 6;
            break;
        case 'Q': // Quit
            break;
        default:
            printf("Unknown command\n");
            printf("Press Enter to continue...");
            getchar();
            getchar();
    }
}

int main() {
    init_spreadsheet();
    
    char cmd;
    do {
        display();
        handle_input();
        cmd = getchar(); // consume newline
    } while (toupper(cmd) != 'Q');
    
    printf("Goodbye!\n");
    return 0;
}
