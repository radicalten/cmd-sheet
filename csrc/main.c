#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <termios.h>
#include <time.h>

#define MAX_ROWS 20
#define MAX_COLS 10
#define MAX_CELL_LEN 256
#define MAX_FORMULA_LEN 512
#define MAX_DISPLAY_WIDTH 12

typedef struct {
    char value[MAX_CELL_LEN];
    char display[MAX_CELL_LEN];
    double numeric;
    int is_formula;
} Cell;

typedef struct {
    Cell cells[MAX_ROWS][MAX_COLS];
    int current_row;
    int current_col;
    int edit_mode;
    char edit_buffer[MAX_CELL_LEN];
    int status_message_time;
    char status_message[100];
} Spreadsheet;

// Terminal control functions
void enable_raw_mode() {
    struct termios term;
    tcgetattr(STDIN_FILENO, &term);
    term.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &term);
}

void disable_raw_mode() {
    struct termios term;
    tcgetattr(STDIN_FILENO, &term);
    term.c_lflag |= (ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &term);
}

void clear_screen() {
    printf("\033[2J\033[H");
}

void set_cursor(int row, int col) {
    printf("\033[%d;%dH", row, col);
}

void set_color(int color) {
    printf("\033[%dm", color);
}

void reset_color() {
    printf("\033[0m");
}

// Spreadsheet functions
void init_spreadsheet(Spreadsheet *ss) {
    for (int i = 0; i < MAX_ROWS; i++) {
        for (int j = 0; j < MAX_COLS; j++) {
            strcpy(ss->cells[i][j].value, "");
            strcpy(ss->cells[i][j].display, "");
            ss->cells[i][j].numeric = 0;
            ss->cells[i][j].is_formula = 0;
        }
    }
    ss->current_row = 0;
    ss->current_col = 0;
    ss->edit_mode = 0;
    strcpy(ss->edit_buffer, "");
    ss->status_message_time = 0;
}

void get_cell_name(int row, int col, char *name) {
    sprintf(name, "%c%d", 'A' + col, row + 1);
}

int parse_cell_reference(const char *ref, int *row, int *col) {
    if (strlen(ref) < 2 || !isalpha(ref[0])) return 0;
    *col = toupper(ref[0]) - 'A';
    *row = atoi(ref + 1) - 1;
    return (*row >= 0 && *row < MAX_ROWS && *col >= 0 && *col < MAX_COLS);
}

double evaluate_expression(Spreadsheet *ss, const char *expr, int current_row, int current_col);

double get_cell_value(Spreadsheet *ss, int row, int col) {
    if (row < 0 || row >= MAX_ROWS || col < 0 || col >= MAX_COLS) return 0;
    if (ss->cells[row][col].is_formula) {
        return evaluate_expression(ss, ss->cells[row][col].value, row, col);
    }
    return ss->cells[row][col].numeric;
}

double evaluate_expression(Spreadsheet *ss, const char *expr, int current_row, int current_col) {
    char temp[MAX_FORMULA_LEN];
    strcpy(temp, expr + 1); // Skip '='
    
    // Replace cell references with values
    char *ptr = temp;
    while (*ptr) {
        if (isalpha(*ptr) && *(ptr+1) && isdigit(*(ptr+1))) {
            char ref[10];
            int i = 0;
            while (*ptr && (isalpha(*ptr) || isdigit(*ptr))) {
                ref[i++] = *ptr++;
            }
            ref[i] = '\0';
            
            int row, col;
            if (parse_cell_reference(ref, &row, &col)) {
                double val = get_cell_value(ss, row, col);
                char val_str[50];
                sprintf(val_str, "%.6f", val);
                
                int len = strlen(ref);
                int val_len = strlen(val_str);
                if (val_len <= len) {
                    memcpy(ptr - len, val_str, val_len);
                    memmove(ptr - len + val_len, ptr, strlen(ptr) + 1);
                    ptr = ptr - len + val_len;
                } else {
                    char new_temp[MAX_FORMULA_LEN];
                    int offset = ptr - temp;
                    strncpy(new_temp, temp, offset - len);
                    new_temp[offset - len] = '\0';
                    strcat(new_temp, val_str);
                    strcat(new_temp, ptr);
                    strcpy(temp, new_temp);
                    ptr = temp + offset + val_len - len;
                }
            }
        } else {
            ptr++;
        }
    }
    
    // Simple expression evaluation (supports +, -, *, /)
    double result = 0;
    double current = 0;
    char op = '+';
    char *token = strtok(temp, "+-*/");
    
    while (token != NULL) {
        current = atof(token);
        switch (op) {
            case '+': result += current; break;
            case '-': result -= current; break;
            case '*': result *= current; break;
            case '/': result /= current; break;
        }
        
        char *next_op = strtok(NULL, "+-*/");
        if (next_op && strlen(next_op) == 1 && strchr("+-*/", *next_op)) {
            op = *next_op;
            token = strtok(NULL, "+-*/");
        } else {
            token = next_op;
        }
    }
    
    return result;
}

void update_cell(Spreadsheet *ss, int row, int col) {
    Cell *cell = &ss->cells[row][col];
    
    if (strlen(cell->value) == 0) {
        strcpy(cell->display, "");
        cell->numeric = 0;
        cell->is_formula = 0;
        return;
    }
    
    if (cell->value[0] == '=') {
        cell->is_formula = 1;
        cell->numeric = evaluate_expression(ss, cell->value, row, col);
        if (cell->numeric == (int)cell->numeric) {
            sprintf(cell->display, "%.0f", cell->numeric);
        } else {
            sprintf(cell->display, "%.2f", cell->numeric);
        }
    } else {
        cell->is_formula = 0;
        cell->numeric = atof(cell->value);
        strcpy(cell->display, cell->value);
    }
}

void update_all_formulas(Spreadsheet *ss) {
    for (int i = 0; i < MAX_ROWS; i++) {
        for (int j = 0; j < MAX_COLS; j++) {
            if (ss->cells[i][j].is_formula) {
                update_cell(ss, i, j);
            }
        }
    }
}

void draw_spreadsheet(Spreadsheet *ss) {
    clear_screen();
    set_color(37); // White
    printf("Terminal Spreadsheet - Use arrow keys to move, Enter to edit, 'q' to quit\n");
    printf("Commands: s=save, l=load, c=clear cell\n");
    reset_color();
    
    // Column headers
    printf("    ");
    for (int j = 0; j < MAX_COLS; j++) {
        printf("%*c ", MAX_DISPLAY_WIDTH, 'A' + j);
    }
    printf("\n");
    
    // Draw separator line
    printf("   +");
    for (int j = 0; j < MAX_COLS; j++) {
        for (int k = 0; k < MAX_DISPLAY_WIDTH + 1; k++) printf("-");
        printf("+");
    }
    printf("\n");
    
    // Draw cells
    for (int i = 0; i < MAX_ROWS; i++) {
        printf("%3d|", i + 1);
        for (int j = 0; j < MAX_COLS; j++) {
            if (i == ss->current_row && j == ss->current_col && !ss->edit_mode) {
                set_color(47); // White background
                set_color(30); // Black text
            }
            
            char *content = ss->cells[i][j].display;
            if (ss->cells[i][j].is_formula && strlen(content) == 0) {
                content = "0";
            }
            
            printf("%-*s ", MAX_DISPLAY_WIDTH, content);
            
            if (i == ss->current_row && j == ss->current_col && !ss->edit_mode) {
                reset_color();
            }
            printf("|");
        }
        printf("\n");
        
        // Draw separator line
        printf("   +");
        for (int j = 0; j < MAX_COLS; j++) {
            for (int k = 0; k < MAX_DISPLAY_WIDTH + 1; k++) printf("-");
            printf("+");
        }
        printf("\n");
    }
    
    // Status bar
    set_cursor(MAX_ROWS * 2 + 5, 1);
    set_color(36); // Cyan
    printf("Cell: ");
    char cell_name[10];
    get_cell_name(ss->current_row, ss->current_col, cell_name);
    printf("%s", cell_name);
    
    printf(" | Value: ");
    if (ss->edit_mode) {
        printf("%s_", ss->edit_buffer);
    } else {
        printf("%s", ss->cells[ss->current_row][ss->current_col].value);
        if (ss->cells[ss->current_row][ss->current_col].is_formula) {
            printf(" (formula)");
        }
    }
    
    if (ss->status_message_time > 0) {
        printf(" | %s", ss->status_message);
        ss->status_message_time--;
    }
    
    reset_color();
    fflush(stdout);
}

void save_spreadsheet(Spreadsheet *ss, const char *filename) {
    FILE *file = fopen(filename, "w");
    if (!file) {
        strcpy(ss->status_message, "Error: Cannot save file");
        ss->status_message_time = 3;
        return;
    }
    
    for (int i = 0; i < MAX_ROWS; i++) {
        for (int j = 0; j < MAX_COLS; j++) {
            fprintf(file, "%s\n", ss->cells[i][j].value);
        }
    }
    
    fclose(file);
    strcpy(ss->status_message, "File saved successfully");
    ss->status_message_time = 3;
}

void load_spreadsheet(Spreadsheet *ss, const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        strcpy(ss->status_message, "Error: Cannot load file");
        ss->status_message_time = 3;
        return;
    }
    
    for (int i = 0; i < MAX_ROWS; i++) {
        for (int j = 0; j < MAX_COLS; j++) {
            if (fgets(ss->cells[i][j].value, MAX_CELL_LEN, file)) {
                ss->cells[i][j].value[strcspn(ss->cells[i][j].value, "\n")] = '\0';
                update_cell(ss, i, j);
            }
        }
    }
    
    fclose(file);
    update_all_formulas(ss);
    strcpy(ss->status_message, "File loaded successfully");
    ss->status_message_time = 3;
}

void handle_input(Spreadsheet *ss) {
    char c;
    if (read(STDIN_FILENO, &c, 1) <= 0) return;
    
    if (ss->edit_mode) {
        if (c == 27) { // ESC
            ss->edit_mode = 0;
            strcpy(ss->edit_buffer, "");
        } else if (c == '\r' || c == '\n') { // Enter
            strcpy(ss->cells[ss->current_row][ss->current_col].value, ss->edit_buffer);
            update_cell(ss, ss->current_row, ss->current_col);
            update_all_formulas(ss);
            ss->edit_mode = 0;
            strcpy(ss->edit_buffer, "");
        } else if (c == 127 || c == 8) { // Backspace
            int len = strlen(ss->edit_buffer);
            if (len > 0) {
                ss->edit_buffer[len - 1] = '\0';
            }
        } else if (isprint(c) && strlen(ss->edit_buffer) < MAX_CELL_LEN - 1) {
            char temp[2] = {c, '\0'};
            strcat(ss->edit_buffer, temp);
        }
    } else {
        switch (c) {
            case 'q':
                exit(0);
            case 's': {
                char filename[100];
                printf("\nEnter filename to save: ");
                scanf("%99s", filename);
                save_spreadsheet(ss, filename);
                break;
            }
            case 'l': {
                char filename[100];
                printf("\nEnter filename to load: ");
                scanf("%99s", filename);
                load_spreadsheet(ss, filename);
                break;
            }
            case 'c':
                strcpy(ss->cells[ss->current_row][ss->current_col].value, "");
                update_cell(ss, ss->current_row, ss->current_col);
                update_all_formulas(ss);
                break;
            case '\r':
            case '\n':
                ss->edit_mode = 1;
                strcpy(ss->edit_buffer, ss->cells[ss->current_row][ss->current_col].value);
                break;
            case 'A': // Up arrow (ANSI sequence)
                if (read(STDIN_FILENO, &c, 1) == 1 && c == '[') {
                    if (read(STDIN_FILENO, &c, 1) == 1) {
                        if (c == 'A' && ss->current_row > 0) ss->current_row--;
                        else if (c == 'B' && ss->current_row < MAX_ROWS - 1) ss->current_row++;
                        else if (c == 'C' && ss->current_col < MAX_COLS - 1) ss->current_col++;
                        else if (c == 'D' && ss->current_col > 0) ss->current_col--;
                    }
                }
                break;
        }
    }
}

int main() {
    Spreadsheet ss;
    init_spreadsheet(&ss);
    
    enable_raw_mode();
    clear_screen();
    
    // Add some sample data
    strcpy(ss.cells[0][0].value, "Item");
    strcpy(ss.cells[0][1].value, "Quantity");
    strcpy(ss.cells[0][2].value, "Price");
    strcpy(ss.cells[0][3].value, "Total");
    strcpy(ss.cells[0][4].value, "=C1*D1");
    
    strcpy(ss.cells[1][0].value, "Apples");
    strcpy(ss.cells[1][1].value, "10");
    strcpy(ss.cells[1][2].value, "1.50");
    strcpy(ss.cells[1][3].value, "=B1*C1");
    
    strcpy(ss.cells[2][0].value, "Oranges");
    strcpy(ss.cells[2][1].value = "5");
    strcpy(ss.cells[2][2].value = "2.00");
    strcpy(ss.cells[2][3].value, "=B2*C2");
    
    strcpy(ss.cells[3][0].value = "TOTAL");
    strcpy(ss.cells[3][3].value = "=SUM(D1:D2)");
    
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            update_cell(&ss, i, j);
        }
    }
    
    while (1) {
        draw_spreadsheet(&ss);
        handle_input(&ss);
    }
    
    disable_raw_mode();
    return 0;
}
