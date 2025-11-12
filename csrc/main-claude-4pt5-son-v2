#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_ROWS 100
#define MAX_COLS 26
#define CELL_WIDTH 12
#define MAX_FORMULA_LEN 256

typedef struct {
    char formula[MAX_FORMULA_LEN];
    double value;
    int is_formula;
} Cell;

typedef struct {
    Cell cells[MAX_ROWS][MAX_COLS];
    int cursor_row;
    int cursor_col;
    int view_row;
    int view_col;
} Spreadsheet;

Spreadsheet sheet;

// Function prototypes
void init_spreadsheet();
void display_spreadsheet();
void edit_cell();
double evaluate_cell(int row, int col);
double evaluate_formula(const char *formula);
void parse_cell_ref(const char *ref, int *row, int *col);
double get_cell_value(int row, int col);
void save_spreadsheet(const char *filename);
void load_spreadsheet(const char *filename);
void clear_screen();
void show_help();
double sum_range(const char *range);

// Clear screen (cross-platform)
void clear_screen() {
    #ifdef _WIN32
        system("cls");
    #else
        system("clear");
    #endif
}

// Initialize spreadsheet
void init_spreadsheet() {
    memset(&sheet, 0, sizeof(Spreadsheet));
    sheet.cursor_row = 0;
    sheet.cursor_col = 0;
    sheet.view_row = 0;
    sheet.view_col = 0;
}

// Parse cell reference like "A1" to row=0, col=0
void parse_cell_ref(const char *ref, int *row, int *col) {
    *col = toupper(ref[0]) - 'A';
    *row = atoi(ref + 1) - 1;
}

// Get cell value (evaluating if needed)
double get_cell_value(int row, int col) {
    if (row < 0 || row >= MAX_ROWS || col < 0 || col >= MAX_COLS) {
        return 0.0;
    }
    
    Cell *cell = &sheet.cells[row][col];
    
    if (cell->is_formula) {
        return evaluate_formula(cell->formula);
    } else if (strlen(cell->formula) > 0) {
        return atof(cell->formula);
    }
    
    return 0.0;
}

// Evaluate SUM(A1:A10) type range
double sum_range(const char *range) {
    char start[10], end[10];
    int start_row, start_col, end_row, end_col;
    double sum = 0.0;
    
    // Parse range like "A1:A10"
    sscanf(range, "%[^:]:%s", start, end);
    parse_cell_ref(start, &start_row, &start_col);
    parse_cell_ref(end, &end_row, &end_col);
    
    for (int r = start_row; r <= end_row; r++) {
        for (int c = start_col; c <= end_col; c++) {
            sum += get_cell_value(r, c);
        }
    }
    
    return sum;
}

// Evaluate formula
double evaluate_formula(const char *formula) {
    char buffer[MAX_FORMULA_LEN];
    strncpy(buffer, formula, MAX_FORMULA_LEN - 1);
    buffer[MAX_FORMULA_LEN - 1] = '\0';
    
    // Handle SUM function
    if (strncmp(buffer, "SUM(", 4) == 0) {
        char *end = strchr(buffer, ')');
        if (end) {
            *end = '\0';
            return sum_range(buffer + 4);
        }
    }
    
    // Handle AVG function
    if (strncmp(buffer, "AVG(", 4) == 0) {
        char *end = strchr(buffer, ')');
        if (end) {
            *end = '\0';
            char start[10], end_ref[10];
            int start_row, start_col, end_row, end_col;
            sscanf(buffer + 4, "%[^:]:%s", start, end_ref);
            parse_cell_ref(start, &start_row, &start_col);
            parse_cell_ref(end_ref, &end_row, &end_col);
            
            int count = (end_row - start_row + 1) * (end_col - start_col + 1);
            return count > 0 ? sum_range(buffer + 4) / count : 0.0;
        }
    }
    
    // Handle simple arithmetic with cell references
    char *ptr = buffer;
    double result = 0.0;
    char op = '+';
    
    while (*ptr) {
        while (*ptr == ' ') ptr++;
        
        double value = 0.0;
        
        // Check if it's a cell reference
        if (isalpha(*ptr)) {
            char ref[10];
            int i = 0;
            while (isalnum(*ptr) && i < 9) {
                ref[i++] = *ptr++;
            }
            ref[i] = '\0';
            
            int row, col;
            parse_cell_ref(ref, &row, &col);
            value = get_cell_value(row, col);
        } else if (isdigit(*ptr) || *ptr == '.') {
            value = atof(ptr);
            while (isdigit(*ptr) || *ptr == '.') ptr++;
        }
        
        // Apply operation
        switch (op) {
            case '+': result += value; break;
            case '-': result -= value; break;
            case '*': result *= value; break;
            case '/': result = (value != 0) ? result / value : 0; break;
        }
        
        // Get next operation
        while (*ptr == ' ') ptr++;
        if (*ptr == '+' || *ptr == '-' || *ptr == '*' || *ptr == '/') {
            op = *ptr++;
            if (op == '-' || op == '*' || op == '/') {
                // For first operation
                if (ptr == buffer + 1) {
                    result = 0;
                }
            }
        } else {
            break;
        }
    }
    
    return result;
}

// Display spreadsheet
void display_spreadsheet() {
    clear_screen();
    
    printf("Simple Spreadsheet - Cell: %c%d\n", 
           'A' + sheet.cursor_col, sheet.cursor_row + 1);
    printf("Commands: [E]dit [S]ave [L]oad [Q]uit [H]elp | Arrow keys: WASD\n");
    
    // Show current cell formula
    Cell *current = &sheet.cells[sheet.cursor_row][sheet.cursor_col];
    if (strlen(current->formula) > 0) {
        printf("Current: %s\n", current->formula);
    } else {
        printf("Current: (empty)\n");
    }
    printf("─────────────────────────────────────────────────────────────────\n");
    
    // Column headers
    printf("    ");
    for (int c = sheet.view_col; c < sheet.view_col + 6 && c < MAX_COLS; c++) {
        printf("|%c%-*s", 'A' + c, CELL_WIDTH - 2, "");
    }
    printf("|\n");
    
    // Separator
    printf("────");
    for (int c = sheet.view_col; c < sheet.view_col + 6 && c < MAX_COLS; c++) {
        for (int i = 0; i < CELL_WIDTH; i++) printf("─");
    }
    printf("\n");
    
    // Rows
    for (int r = sheet.view_row; r < sheet.view_row + 15 && r < MAX_ROWS; r++) {
        printf("%-3d ", r + 1);
        
        for (int c = sheet.view_col; c < sheet.view_col + 6 && c < MAX_COLS; c++) {
            Cell *cell = &sheet.cells[r][c];
            char display[CELL_WIDTH + 1];
            
            if (strlen(cell->formula) > 0) {
                if (cell->is_formula) {
                    snprintf(display, CELL_WIDTH, "%.2f", get_cell_value(r, c));
                } else {
                    snprintf(display, CELL_WIDTH, "%s", cell->formula);
                }
            } else {
                display[0] = '\0';
            }
            
            // Highlight cursor
            if (r == sheet.cursor_row && c == sheet.cursor_col) {
                printf("|>%-*.*s", CELL_WIDTH - 2, CELL_WIDTH - 2, display);
            } else {
                printf("| %-*.*s", CELL_WIDTH - 2, CELL_WIDTH - 2, display);
            }
        }
        printf("|\n");
    }
}

// Edit cell
void edit_cell() {
    Cell *cell = &sheet.cells[sheet.cursor_row][sheet.cursor_col];
    char input[MAX_FORMULA_LEN];
    
    printf("\nEnter value or formula (start with = for formula): ");
    fgets(input, MAX_FORMULA_LEN, stdin);
    
    // Remove newline
    input[strcspn(input, "\n")] = 0;
    
    if (strlen(input) == 0) {
        // Clear cell
        cell->formula[0] = '\0';
        cell->value = 0.0;
        cell->is_formula = 0;
    } else if (input[0] == '=') {
        // Formula
        strncpy(cell->formula, input + 1, MAX_FORMULA_LEN - 1);
        cell->is_formula = 1;
        cell->value = evaluate_formula(cell->formula);
    } else {
        // Plain value
        strncpy(cell->formula, input, MAX_FORMULA_LEN - 1);
        cell->is_formula = 0;
        cell->value = atof(input);
    }
}

// Save spreadsheet
void save_spreadsheet(const char *filename) {
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        printf("Error: Could not save file!\n");
        return;
    }
    
    for (int r = 0; r < MAX_ROWS; r++) {
        for (int c = 0; c < MAX_COLS; c++) {
            Cell *cell = &sheet.cells[r][c];
            if (strlen(cell->formula) > 0) {
                fprintf(fp, "%d,%d,%d,%s\n", r, c, cell->is_formula, cell->formula);
            }
        }
    }
    
    fclose(fp);
    printf("Saved to %s. Press Enter to continue...", filename);
    getchar();
}

// Load spreadsheet
void load_spreadsheet(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        printf("Error: Could not load file!\n");
        getchar();
        return;
    }
    
    init_spreadsheet();
    
    char line[MAX_FORMULA_LEN + 20];
    while (fgets(line, sizeof(line), fp)) {
        int r, c, is_formula;
        char formula[MAX_FORMULA_LEN];
        
        char *token = strtok(line, ",");
        if (!token) continue;
        r = atoi(token);
        
        token = strtok(NULL, ",");
        if (!token) continue;
        c = atoi(token);
        
        token = strtok(NULL, ",");
        if (!token) continue;
        is_formula = atoi(token);
        
        token = strtok(NULL, "\n");
        if (!token) continue;
        strncpy(formula, token, MAX_FORMULA_LEN - 1);
        
        if (r >= 0 && r < MAX_ROWS && c >= 0 && c < MAX_COLS) {
            Cell *cell = &sheet.cells[r][c];
            strncpy(cell->formula, formula, MAX_FORMULA_LEN - 1);
            cell->is_formula = is_formula;
            if (is_formula) {
                cell->value = evaluate_formula(cell->formula);
            } else {
                cell->value = atof(cell->formula);
            }
        }
    }
    
    fclose(fp);
    printf("Loaded from %s. Press Enter to continue...", filename);
    getchar();
}

// Show help
void show_help() {
    clear_screen();
    printf("=== SIMPLE SPREADSHEET HELP ===\n\n");
    printf("Navigation:\n");
    printf("  W - Move up\n");
    printf("  S - Move down\n");
    printf("  A - Move left\n");
    printf("  D - Move right\n\n");
    printf("Commands:\n");
    printf("  E - Edit current cell\n");
    printf("  S - Save spreadsheet\n");
    printf("  L - Load spreadsheet\n");
    printf("  Q - Quit\n");
    printf("  H - This help\n\n");
    printf("Formulas:\n");
    printf("  Start with = sign\n");
    printf("  Examples:\n");
    printf("    =A1+B1\n");
    printf("    =A1*2.5\n");
    printf("    =SUM(A1:A10)\n");
    printf("    =AVG(B1:B5)\n");
    printf("    =A1+B2-C3\n\n");
    printf("Cell References:\n");
    printf("  Column A-Z, Row 1-100\n");
    printf("  Example: A1, B5, Z100\n\n");
    printf("Press Enter to continue...");
    getchar();
}

int main() {
    char command;
    
    init_spreadsheet();
    
    while (1) {
        display_spreadsheet();
        
        printf("\nCommand: ");
        command = getchar();
        while (getchar() != '\n'); // Clear input buffer
        
        command = toupper(command);
        
        switch (command) {
            case 'W': // Up
                if (sheet.cursor_row > 0) {
                    sheet.cursor_row--;
                    if (sheet.cursor_row < sheet.view_row) {
                        sheet.view_row = sheet.cursor_row;
                    }
                }
                break;
                
            case 'S': // Down
                if (sheet.cursor_row < MAX_ROWS - 1) {
                    sheet.cursor_row++;
                    if (sheet.cursor_row >= sheet.view_row + 15) {
                        sheet.view_row = sheet.cursor_row - 14;
                    }
                }
                break;
                
            case 'A': // Left
                if (sheet.cursor_col > 0) {
                    sheet.cursor_col--;
                    if (sheet.cursor_col < sheet.view_col) {
                        sheet.view_col = sheet.cursor_col;
                    }
                }
                break;
                
            case 'D': // Right
                if (sheet.cursor_col < MAX_COLS - 1) {
                    sheet.cursor_col++;
                    if (sheet.cursor_col >= sheet.view_col + 6) {
                        sheet.view_col = sheet.cursor_col - 5;
                    }
                }
                break;
                
            case 'E': // Edit
                edit_cell();
                break;
                
            case 'Q': // Quit
                printf("Are you sure you want to quit? (Y/N): ");
                char confirm = getchar();
                while (getchar() != '\n');
                if (toupper(confirm) == 'Y') {
                    printf("Goodbye!\n");
                    return 0;
                }
                break;
                
            case 'H': // Help
                show_help();
                break;
                
            default:
                // Check if it's save command
                if (command == 'S' && strchr("SAVE", 'S')) {
                    char filename[256];
                    printf("Enter filename: ");
                    fgets(filename, sizeof(filename), stdin);
                    filename[strcspn(filename, "\n")] = 0;
                    if (strlen(filename) > 0) {
                        save_spreadsheet(filename);
                    }
                } else if (command == 'L') {
                    char filename[256];
                    printf("Enter filename: ");
                    fgets(filename, sizeof(filename), stdin);
                    filename[strcspn(filename, "\n")] = 0;
                    if (strlen(filename) > 0) {
                        load_spreadsheet(filename);
                    }
                }
                break;
        }
    }
    
    return 0;
}
