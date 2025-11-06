#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define ROWS 20
#define COLS 10
#define CELL_WIDTH 12
#define MAX_INPUT 100

typedef struct {
    char display[CELL_WIDTH];
    char formula[MAX_INPUT];
    double value;
    int is_formula;
} Cell;

Cell spreadsheet[ROWS][COLS];
int cursor_row = 0;
int cursor_col = 0;

void initialize_spreadsheet() {
    for (int i = 0; i < ROWS; i++) {
        for (int j = 0; j < COLS; j++) {
            strcpy(spreadsheet[i][j].display, "");
            strcpy(spreadsheet[i][j].formula, "");
            spreadsheet[i][j].value = 0;
            spreadsheet[i][j].is_formula = 0;
        }
    }
}

void clear_screen() {
    printf("\033[2J\033[H"); // ANSI escape codes to clear screen
}

void display_header() {
    printf("    ");
    for (int j = 0; j < COLS; j++) {
        printf("|%*c ", CELL_WIDTH - 1, 'A' + j);
    }
    printf("|\n");
    
    printf("----");
    for (int j = 0; j < COLS; j++) {
        printf("+");
        for (int k = 0; k < CELL_WIDTH; k++) printf("-");
    }
    printf("+\n");
}

void display_spreadsheet() {
    clear_screen();
    printf("Simple Spreadsheet - Use WASD to move, E to edit, Q to quit\n\n");
    
    display_header();
    
    for (int i = 0; i < ROWS; i++) {
        printf("%2d  ", i + 1);
        for (int j = 0; j < COLS; j++) {
            printf("|");
            if (i == cursor_row && j == cursor_col) {
                printf("\033[7m"); // Reverse video for cursor
            }
            
            printf("%*s", CELL_WIDTH, spreadsheet[i][j].display);
            
            if (i == cursor_row && j == cursor_col) {
                printf("\033[0m"); // Reset formatting
            }
        }
        printf("|\n");
    }
    
    printf("\nCursor: %c%d | ", 'A' + cursor_col, cursor_row + 1);
    if (spreadsheet[cursor_row][cursor_col].is_formula) {
        printf("Formula: =%s", spreadsheet[cursor_row][cursor_col].formula);
    } else {
        printf("Value: %s", spreadsheet[cursor_row][cursor_col].display);
    }
    printf("\n> ");
    fflush(stdout);
}

int evaluate_formula(const char* formula, double* result) {
    // Simple formula evaluator - only handles basic arithmetic
    char* endptr;
    *result = strtod(formula, &endptr);
    
    if (*endptr == '\0') {
        return 1; // It's just a number
    }
    
    // Very basic formula parsing - only handles cell references and +-*/
    double total = 0;
    char op = '+';
    const char* ptr = formula;
    
    while (*ptr) {
        if (isspace(*ptr)) {
            ptr++;
            continue;
        }
        
        if (isalpha(*ptr) && isdigit(*(ptr+1))) {
            // Cell reference like A1, B2, etc.
            int col = toupper(*ptr) - 'A';
            int row = atoi(ptr + 1) - 1;
            
            if (row >= 0 && row < ROWS && col >= 0 && col < COLS) {
                double cell_value = spreadsheet[row][col].value;
                
                switch (op) {
                    case '+': total += cell_value; break;
                    case '-': total -= cell_value; break;
                    case '*': total *= cell_value; break;
                    case '/': 
                        if (cell_value != 0) total /= cell_value;
                        else return 0; // Division by zero
                        break;
                }
            }
            
            ptr += 2; // Skip cell reference
            while (isdigit(*ptr)) ptr++; // Skip remaining digits
        }
        else if (isdigit(*ptr) || *ptr == '.') {
            double num = strtod(ptr, (char**)&ptr);
            
            switch (op) {
                case '+': total += num; break;
                case '-': total -= num; break;
                case '*': total *= num; break;
                case '/': 
                    if (num != 0) total /= num;
                    else return 0; // Division by zero
                    break;
            }
        }
        else if (strchr("+-*/", *ptr)) {
            op = *ptr;
            ptr++;
        }
        else {
            ptr++;
        }
    }
    
    *result = total;
    return 1;
}

void update_cell_display(int row, int col) {
    Cell* cell = &spreadsheet[row][col];
    
    if (cell->is_formula) {
        double result;
        if (evaluate_formula(cell->formula, &result)) {
            cell->value = result;
            snprintf(cell->display, CELL_WIDTH, "%.2f", result);
        } else {
            strcpy(cell->display, "#ERROR");
            cell->value = 0;
        }
    } else {
        // Try to parse as number
        char* endptr;
        double num = strtod(cell->formula, &endptr);
        
        if (*endptr == '\0' && cell->formula[0] != '\0') {
            cell->value = num;
            snprintf(cell->display, CELL_WIDTH, "%.2f", num);
        } else {
            cell->value = 0;
            // Truncate if too long
            strncpy(cell->display, cell->formula, CELL_WIDTH - 1);
            cell->display[CELL_WIDTH - 1] = '\0';
        }
    }
}

void edit_cell() {
    printf("\nEdit cell %c%d: ", 'A' + cursor_col, cursor_row + 1);
    fflush(stdout);
    
    char input[MAX_INPUT];
    if (fgets(input, MAX_INPUT, stdin)) {
        // Remove newline
        input[strcspn(input, "\n")] = 0;
        
        if (input[0] == '=') {
            // Formula
            spreadsheet[cursor_row][cursor_col].is_formula = 1;
            strncpy(spreadsheet[cursor_row][cursor_col].formula, 
                   input + 1, MAX_INPUT - 1);
        } else {
            // Direct value
            spreadsheet[cursor_row][cursor_col].is_formula = 0;
            strncpy(spreadsheet[cursor_row][cursor_col].formula, 
                   input, MAX_INPUT - 1);
        }
        
        update_cell_display(cursor_row, cursor_col);
    }
}

int main() {
    initialize_spreadsheet();
    
    printf("Simple Spreadsheet\n");
    printf("Commands: WASD-move, E-edit, Q-quit\n");
    printf("Formulas start with = (e.g., =A1+B2)\n");
    printf("Press Enter to start...");
    getchar();
    
    int running = 1;
    while (running) {
        display_spreadsheet();
        
        char cmd = getchar();
        getchar(); // Consume newline
        
        switch (tolower(cmd)) {
            case 'w': if (cursor_row > 0) cursor_row--; break;
            case 's': if (cursor_row < ROWS - 1) cursor_row++; break;
            case 'a': if (cursor_col > 0) cursor_col--; break;
            case 'd': if (cursor_col < COLS - 1) cursor_col++; break;
            case 'e': edit_cell(); break;
            case 'q': running = 0; break;
        }
    }
    
    clear_screen();
    printf("Goodbye!\n");
    return 0;
}
