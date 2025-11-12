/* 
 * TermSheet - A Full-Featured Terminal Spreadsheet
 * Single file, no dependencies, compile with: gcc -o termsheet termsheet.c -lm
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#define MAX_ROWS 1000
#define MAX_COLS 26
#define MAX_CELL_LEN 256
#define MAX_FORMULA_LEN 512
#define CELL_WIDTH 12

/* Cell types */
typedef enum {
    CELL_EMPTY,
    CELL_NUMBER,
    CELL_TEXT,
    CELL_FORMULA,
    CELL_ERROR
} CellType;

/* Cell structure */
typedef struct {
    CellType type;
    char raw[MAX_CELL_LEN];      /* Raw input */
    char display[MAX_CELL_LEN];  /* Displayed value */
    double numValue;              /* Numeric value */
    int dirty;                    /* Needs recalculation */
} Cell;

/* Spreadsheet structure */
typedef struct {
    Cell cells[MAX_ROWS][MAX_COLS];
    int curRow;
    int curCol;
    int topRow;
    int leftCol;
    int rows;
    int cols;
    char filename[256];
    int modified;
} Sheet;

/* Clipboard */
typedef struct {
    char content[MAX_CELL_LEN];
    int hasContent;
} Clipboard;

/* Function prototypes */
void initSheet(Sheet *sheet);
void drawSheet(Sheet *sheet);
void setCellValue(Sheet *sheet, int row, int col, const char *value);
void evaluateCell(Sheet *sheet, int row, int col);
double evaluateFormula(Sheet *sheet, const char *formula, int curRow, int curCol, int *error);
void recalculateAll(Sheet *sheet);
void handleInput(Sheet *sheet, Clipboard *clip);
void saveSheet(Sheet *sheet);
void loadSheet(Sheet *sheet);
void clearScreen();
void moveCursor(int row, int col);
char* columnName(int col);
int parseCell(const char *ref, int *row, int *col);
double getNumericValue(Sheet *sheet, int row, int col);

/* Built-in functions */
double func_sum(Sheet *sheet, const char *range, int *error);
double func_avg(Sheet *sheet, const char *range, int *error);
double func_min(Sheet *sheet, const char *range, int *error);
double func_max(Sheet *sheet, const char *range, int *error);
double func_count(Sheet *sheet, const char *range, int *error);

/* Utility functions */
void clearScreen() {
    printf("\033[2J");
    printf("\033[H");
}

void moveCursor(int row, int col) {
    printf("\033[%d;%dH", row, col);
}

char* columnName(int col) {
    static char name[3];
    if (col < 26) {
        name[0] = 'A' + col;
        name[1] = '\0';
    } else {
        name[0] = 'A' + (col / 26) - 1;
        name[1] = 'A' + (col % 26);
        name[2] = '\0';
    }
    return name;
}

int parseCell(const char *ref, int *row, int *col) {
    if (!ref || !*ref) return 0;
    
    /* Parse column (A, B, ..., Z, AA, AB, ...) */
    int c = 0;
    const char *p = ref;
    
    if (!isalpha(*p)) return 0;
    
    if (isalpha(p[0]) && isalpha(p[1])) {
        c = (toupper(p[0]) - 'A' + 1) * 26 + (toupper(p[1]) - 'A');
        p += 2;
    } else {
        c = toupper(p[0]) - 'A';
        p++;
    }
    
    /* Parse row */
    if (!isdigit(*p)) return 0;
    int r = atoi(p) - 1;
    
    if (r < 0 || r >= MAX_ROWS || c < 0 || c >= MAX_COLS) return 0;
    
    *row = r;
    *col = c;
    return 1;
}

void initSheet(Sheet *sheet) {
    int i, j;
    for (i = 0; i < MAX_ROWS; i++) {
        for (j = 0; j < MAX_COLS; j++) {
            sheet->cells[i][j].type = CELL_EMPTY;
            sheet->cells[i][j].raw[0] = '\0';
            sheet->cells[i][j].display[0] = '\0';
            sheet->cells[i][j].numValue = 0.0;
            sheet->cells[i][j].dirty = 0;
        }
    }
    sheet->curRow = 0;
    sheet->curCol = 0;
    sheet->topRow = 0;
    sheet->leftCol = 0;
    sheet->rows = MAX_ROWS;
    sheet->cols = MAX_COLS;
    sheet->filename[0] = '\0';
    sheet->modified = 0;
}

void drawSheet(Sheet *sheet) {
    clearScreen();
    
    /* Title bar */
    printf("=== TermSheet v1.0 ===");
    if (sheet->filename[0]) printf(" [%s]", sheet->filename);
    if (sheet->modified) printf(" *");
    printf("\n");
    
    /* Instructions */
    printf("Arrow keys: Move | Enter: Edit | Ctrl+S: Save | Ctrl+L: Load | Ctrl+Q: Quit | Ctrl+C: Copy | Ctrl+V: Paste\n");
    
    /* Column headers */
    printf("    ");
    int visibleCols = 6;  /* Number of visible columns */
    for (int c = sheet->leftCol; c < sheet->leftCol + visibleCols && c < MAX_COLS; c++) {
        printf("%-*s ", CELL_WIDTH, columnName(c));
    }
    printf("\n");
    
    /* Separator */
    printf("----");
    for (int c = 0; c < visibleCols; c++) {
        for (int i = 0; i < CELL_WIDTH + 1; i++) printf("-");
    }
    printf("\n");
    
    /* Rows */
    int visibleRows = 20;
    for (int r = sheet->topRow; r < sheet->topRow + visibleRows && r < MAX_ROWS; r++) {
        printf("%-3d ", r + 1);
        for (int c = sheet->leftCol; c < sheet->leftCol + visibleCols && c < MAX_COLS; c++) {
            Cell *cell = &sheet->cells[r][c];
            
            /* Highlight current cell */
            if (r == sheet->curRow && c == sheet->curCol) {
                printf("\033[7m"); /* Reverse video */
            }
            
            /* Display cell content */
            if (cell->type == CELL_EMPTY) {
                printf("%-*s", CELL_WIDTH, "");
            } else {
                char truncated[CELL_WIDTH + 1];
                strncpy(truncated, cell->display, CELL_WIDTH);
                truncated[CELL_WIDTH] = '\0';
                printf("%-*s", CELL_WIDTH, truncated);
            }
            
            if (r == sheet->curRow && c == sheet->curCol) {
                printf("\033[0m"); /* Reset */
            }
            printf(" ");
        }
        printf("\n");
    }
    
    /* Status bar */
    printf("\n");
    printf("Current: %s%d | ", columnName(sheet->curCol), sheet->curRow + 1);
    Cell *curCell = &sheet->cells[sheet->curRow][sheet->curCol];
    if (curCell->type != CELL_EMPTY) {
        printf("Value: %s", curCell->raw);
    }
    printf("\n");
}

void setCellValue(Sheet *sheet, int row, int col, const char *value) {
    if (row < 0 || row >= MAX_ROWS || col < 0 || col >= MAX_COLS) return;
    
    Cell *cell = &sheet->cells[row][col];
    strncpy(cell->raw, value, MAX_CELL_LEN - 1);
    cell->raw[MAX_CELL_LEN - 1] = '\0';
    
    /* Determine cell type */
    if (value[0] == '\0') {
        cell->type = CELL_EMPTY;
        cell->display[0] = '\0';
        cell->numValue = 0.0;
    } else if (value[0] == '=') {
        cell->type = CELL_FORMULA;
        cell->dirty = 1;
        evaluateCell(sheet, row, col);
    } else {
        /* Try to parse as number */
        char *endptr;
        double num = strtod(value, &endptr);
        if (*endptr == '\0' && endptr != value) {
            cell->type = CELL_NUMBER;
            cell->numValue = num;
            snprintf(cell->display, MAX_CELL_LEN, "%.2f", num);
        } else {
            cell->type = CELL_TEXT;
            strncpy(cell->display, value, MAX_CELL_LEN - 1);
            cell->display[MAX_CELL_LEN - 1] = '\0';
        }
    }
    
    sheet->modified = 1;
    recalculateAll(sheet);
}

double getNumericValue(Sheet *sheet, int row, int col) {
    if (row < 0 || row >= MAX_ROWS || col < 0 || col >= MAX_COLS) return 0.0;
    
    Cell *cell = &sheet->cells[row][col];
    if (cell->type == CELL_NUMBER || cell->type == CELL_FORMULA) {
        return cell->numValue;
    }
    return 0.0;
}

/* Evaluate a formula */
double evaluateFormula(Sheet *sheet, const char *formula, int curRow, int curCol, int *error) {
    *error = 0;
    char f[MAX_FORMULA_LEN];
    strncpy(f, formula, MAX_FORMULA_LEN - 1);
    f[MAX_FORMULA_LEN - 1] = '\0';
    
    /* Skip the '=' sign */
    char *expr = f;
    if (*expr == '=') expr++;
    
    /* Trim whitespace */
    while (isspace(*expr)) expr++;
    
    /* Check for functions */
    if (strncasecmp(expr, "SUM(", 4) == 0) {
        char *args = expr + 4;
        char *end = strchr(args, ')');
        if (end) {
            *end = '\0';
            return func_sum(sheet, args, error);
        }
    } else if (strncasecmp(expr, "AVG(", 4) == 0 || strncasecmp(expr, "AVERAGE(", 8) == 0) {
        char *args = strchr(expr, '(') + 1;
        char *end = strchr(args, ')');
        if (end) {
            *end = '\0';
            return func_avg(sheet, args, error);
        }
    } else if (strncasecmp(expr, "MIN(", 4) == 0) {
        char *args = expr + 4;
        char *end = strchr(args, ')');
        if (end) {
            *end = '\0';
            return func_min(sheet, args, error);
        }
    } else if (strncasecmp(expr, "MAX(", 4) == 0) {
        char *args = expr + 4;
        char *end = strchr(args, ')');
        if (end) {
            *end = '\0';
            return func_max(sheet, args, error);
        }
    } else if (strncasecmp(expr, "COUNT(", 6) == 0) {
        char *args = expr + 6;
        char *end = strchr(args, ')');
        if (end) {
            *end = '\0';
            return func_count(sheet, args, error);
        }
    } else if (strncasecmp(expr, "SQRT(", 5) == 0) {
        char *args = expr + 5;
        char *end = strchr(args, ')');
        if (end) {
            *end = '\0';
            double val = evaluateFormula(sheet, args, curRow, curCol, error);
            if (*error) return 0.0;
            return sqrt(val);
        }
    } else if (strncasecmp(expr, "POW(", 4) == 0) {
        char *args = expr + 4;
        char *end = strchr(args, ')');
        if (end) {
            *end = '\0';
            char *comma = strchr(args, ',');
            if (comma) {
                *comma = '\0';
                double base = evaluateFormula(sheet, args, curRow, curCol, error);
                if (*error) return 0.0;
                double exp = evaluateFormula(sheet, comma + 1, curRow, curCol, error);
                if (*error) return 0.0;
                return pow(base, exp);
            }
        }
    }
    
    /* Simple expression evaluation */
    /* Handle cell references */
    int refRow, refCol;
    if (parseCell(expr, &refRow, &refCol)) {
        /* Detect circular reference */
        if (refRow == curRow && refCol == curCol) {
            *error = 1;
            return 0.0;
        }
        return getNumericValue(sheet, refRow, refCol);
    }
    
    /* Simple arithmetic parser */
    double result = 0.0;
    double current = 0.0;
    char op = '+';
    char *p = expr;
    
    while (*p) {
        while (isspace(*p)) p++;
        
        double value = 0.0;
        
        /* Check for cell reference */
        if (isalpha(*p)) {
            char ref[10];
            int i = 0;
            while (*p && (isalnum(*p)) && i < 9) {
                ref[i++] = *p++;
            }
            ref[i] = '\0';
            
            if (parseCell(ref, &refRow, &refCol)) {
                if (refRow == curRow && refCol == curCol) {
                    *error = 1;
                    return 0.0;
                }
                value = getNumericValue(sheet, refRow, refCol);
            } else {
                *error = 1;
                return 0.0;
            }
        } else if (isdigit(*p) || *p == '.') {
            value = strtod(p, &p);
        } else if (*p == '-' || *p == '+') {
            if (op == '+' || op == '-' || op == '*' || op == '/') {
                /* Unary minus/plus */
                char sign = *p++;
                while (isspace(*p)) p++;
                
                if (isdigit(*p) || *p == '.') {
                    value = strtod(p, &p);
                } else if (isalpha(*p)) {
                    char ref[10];
                    int i = 0;
                    while (*p && isalnum(*p) && i < 9) {
                        ref[i++] = *p++;
                    }
                    ref[i] = '\0';
                    if (parseCell(ref, &refRow, &refCol)) {
                        value = getNumericValue(sheet, refRow, refCol);
                    }
                }
                
                if (sign == '-') value = -value;
            }
        }
        
        /* Apply operation */
        switch (op) {
            case '+': result += value; break;
            case '-': result -= value; break;
            case '*': result *= value; break;
            case '/': 
                if (value != 0.0) result /= value;
                else { *error = 1; return 0.0; }
                break;
        }
        
        /* Get next operator */
        while (isspace(*p)) p++;
        if (*p == '+' || *p == '-' || *p == '*' || *p == '/') {
            op = *p++;
        } else {
            break;
        }
    }
    
    return result;
}

void evaluateCell(Sheet *sheet, int row, int col) {
    Cell *cell = &sheet->cells[row][col];
    
    if (cell->type != CELL_FORMULA) return;
    
    int error = 0;
    double result = evaluateFormula(sheet, cell->raw, row, col, &error);
    
    if (error) {
        cell->type = CELL_ERROR;
        strcpy(cell->display, "#ERROR");
        cell->numValue = 0.0;
    } else {
        cell->numValue = result;
        snprintf(cell->display, MAX_CELL_LEN, "%.2f", result);
    }
    
    cell->dirty = 0;
}

void recalculateAll(Sheet *sheet) {
    for (int i = 0; i < MAX_ROWS; i++) {
        for (int j = 0; j < MAX_COLS; j++) {
            if (sheet->cells[i][j].type == CELL_FORMULA) {
                evaluateCell(sheet, i, j);
            }
        }
    }
}

/* Built-in spreadsheet functions */
double func_sum(Sheet *sheet, const char *range, int *error) {
    int r1, c1, r2, c2;
    char rangeCopy[64];
    strncpy(rangeCopy, range, 63);
    rangeCopy[63] = '\0';
    
    /* Trim whitespace */
    char *p = rangeCopy;
    while (isspace(*p)) p++;
    
    /* Parse range A1:B10 */
    char *colon = strchr(p, ':');
    if (colon) {
        *colon = '\0';
        if (!parseCell(p, &r1, &c1) || !parseCell(colon + 1, &r2, &c2)) {
            *error = 1;
            return 0.0;
        }
        
        /* Ensure proper order */
        if (r1 > r2) { int t = r1; r1 = r2; r2 = t; }
        if (c1 > c2) { int t = c1; c1 = c2; c2 = t; }
        
        double sum = 0.0;
        for (int r = r1; r <= r2; r++) {
            for (int c = c1; c <= c2; c++) {
                sum += getNumericValue(sheet, r, c);
            }
        }
        return sum;
    } else {
        /* Single cell */
        if (!parseCell(p, &r1, &c1)) {
            *error = 1;
            return 0.0;
        }
        return getNumericValue(sheet, r1, c1);
    }
}

double func_avg(Sheet *sheet, const char *range, int *error) {
    int r1, c1, r2, c2;
    char rangeCopy[64];
    strncpy(rangeCopy, range, 63);
    rangeCopy[63] = '\0';
    
    char *p = rangeCopy;
    while (isspace(*p)) p++;
    
    char *colon = strchr(p, ':');
    if (colon) {
        *colon = '\0';
        if (!parseCell(p, &r1, &c1) || !parseCell(colon + 1, &r2, &c2)) {
            *error = 1;
            return 0.0;
        }
        
        if (r1 > r2) { int t = r1; r1 = r2; r2 = t; }
        if (c1 > c2) { int t = c1; c1 = c2; c2 = t; }
        
        double sum = 0.0;
        int count = 0;
        for (int r = r1; r <= r2; r++) {
            for (int c = c1; c <= c2; c++) {
                if (sheet->cells[r][c].type != CELL_EMPTY) {
                    sum += getNumericValue(sheet, r, c);
                    count++;
                }
            }
        }
        return count > 0 ? sum / count : 0.0;
    } else {
        if (!parseCell(p, &r1, &c1)) {
            *error = 1;
            return 0.0;
        }
        return getNumericValue(sheet, r1, c1);
    }
}

double func_min(Sheet *sheet, const char *range, int *error) {
    int r1, c1, r2, c2;
    char rangeCopy[64];
    strncpy(rangeCopy, range, 63);
    rangeCopy[63] = '\0';
    
    char *p = rangeCopy;
    while (isspace(*p)) p++;
    
    char *colon = strchr(p, ':');
    if (colon) {
        *colon = '\0';
        if (!parseCell(p, &r1, &c1) || !parseCell(colon + 1, &r2, &c2)) {
            *error = 1;
            return 0.0;
        }
        
        if (r1 > r2) { int t = r1; r1 = r2; r2 = t; }
        if (c1 > c2) { int t = c1; c1 = c2; c2 = t; }
        
        double min = INFINITY;
        for (int r = r1; r <= r2; r++) {
            for (int c = c1; c <= c2; c++) {
                if (sheet->cells[r][c].type != CELL_EMPTY) {
                    double val = getNumericValue(sheet, r, c);
                    if (val < min) min = val;
                }
            }
        }
        return min == INFINITY ? 0.0 : min;
    }
    return 0.0;
}

double func_max(Sheet *sheet, const char *range, int *error) {
    int r1, c1, r2, c2;
    char rangeCopy[64];
    strncpy(rangeCopy, range, 63);
    rangeCopy[63] = '\0';
    
    char *p = rangeCopy;
    while (isspace(*p)) p++;
    
    char *colon = strchr(p, ':');
    if (colon) {
        *colon = '\0';
        if (!parseCell(p, &r1, &c1) || !parseCell(colon + 1, &r2, &c2)) {
            *error = 1;
            return 0.0;
        }
        
        if (r1 > r2) { int t = r1; r1 = r2; r2 = t; }
        if (c1 > c2) { int t = c1; c1 = c2; c2 = t; }
        
        double max = -INFINITY;
        for (int r = r1; r <= r2; r++) {
            for (int c = c1; c <= c2; c++) {
                if (sheet->cells[r][c].type != CELL_EMPTY) {
                    double val = getNumericValue(sheet, r, c);
                    if (val > max) max = val;
                }
            }
        }
        return max == -INFINITY ? 0.0 : max;
    }
    return 0.0;
}

double func_count(Sheet *sheet, const char *range, int *error) {
    int r1, c1, r2, c2;
    char rangeCopy[64];
    strncpy(rangeCopy, range, 63);
    rangeCopy[63] = '\0';
    
    char *p = rangeCopy;
    while (isspace(*p)) p++;
    
    char *colon = strchr(p, ':');
    if (colon) {
        *colon = '\0';
        if (!parseCell(p, &r1, &c1) || !parseCell(colon + 1, &r2, &c2)) {
            *error = 1;
            return 0.0;
        }
        
        if (r1 > r2) { int t = r1; r1 = r2; r2 = t; }
        if (c1 > c2) { int t = c1; c1 = c2; c2 = t; }
        
        int count = 0;
        for (int r = r1; r <= r2; r++) {
            for (int c = c1; c <= c2; c++) {
                if (sheet->cells[r][c].type != CELL_EMPTY) {
                    count++;
                }
            }
        }
        return (double)count;
    }
    return 0.0;
}

void saveSheet(Sheet *sheet) {
    if (sheet->filename[0] == '\0') {
        printf("\nEnter filename: ");
        if (scanf("%255s", sheet->filename) != 1) {
            printf("Invalid filename.\n");
            return;
        }
    }
    
    FILE *fp = fopen(sheet->filename, "w");
    if (!fp) {
        printf("Error: Cannot open file for writing.\n");
        getchar();
        getchar();
        return;
    }
    
    /* Save as CSV with formulas preserved */
    for (int r = 0; r < MAX_ROWS; r++) {
        int hasData = 0;
        for (int c = 0; c < MAX_COLS; c++) {
            if (sheet->cells[r][c].type != CELL_EMPTY) {
                hasData = 1;
                break;
            }
        }
        
        if (!hasData) continue;
        
        for (int c = 0; c < MAX_COLS; c++) {
            Cell *cell = &sheet->cells[r][c];
            if (cell->type != CELL_EMPTY) {
                /* Quote if contains comma or quote */
                if (strchr(cell->raw, ',') || strchr(cell->raw, '"')) {
                    fprintf(fp, "\"%s\"", cell->raw);
                } else {
                    fprintf(fp, "%s", cell->raw);
                }
            }
            if (c < MAX_COLS - 1) fprintf(fp, ",");
        }
        fprintf(fp, "\n");
    }
    
    fclose(fp);
    sheet->modified = 0;
    printf("Saved to %s. Press Enter to continue...", sheet->filename);
    getchar();
    getchar();
}

void loadSheet(Sheet *sheet) {
    char filename[256];
    printf("\nEnter filename to load: ");
    if (scanf("%255s", filename) != 1) {
        printf("Invalid filename.\n");
        getchar();
        return;
    }
    
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        printf("Error: Cannot open file for reading.\n");
        getchar();
        getchar();
        return;
    }
    
    initSheet(sheet);
    strcpy(sheet->filename, filename);
    
    char line[4096];
    int row = 0;
    
    while (fgets(line, sizeof(line), fp) && row < MAX_ROWS) {
        int col = 0;
        char *p = line;
        char cell[MAX_CELL_LEN];
        int cellIdx = 0;
        int inQuote = 0;
        
        while (*p && col < MAX_COLS) {
            if (*p == '"') {
                inQuote = !inQuote;
                p++;
            } else if (*p == ',' && !inQuote) {
                cell[cellIdx] = '\0';
                if (cellIdx > 0) {
                    setCellValue(sheet, row, col, cell);
                }
                col++;
                cellIdx = 0;
                p++;
            } else if (*p == '\n' || *p == '\r') {
                break;
            } else {
                if (cellIdx < MAX_CELL_LEN - 1) {
                    cell[cellIdx++] = *p;
                }
                p++;
            }
        }
        
        /* Last cell in row */
        cell[cellIdx] = '\0';
        if (cellIdx > 0) {
            setCellValue(sheet, row, col, cell);
        }
        
        row++;
    }
    
    fclose(fp);
    recalculateAll(sheet);
    sheet->modified = 0;
    printf("Loaded from %s. Press Enter to continue...", filename);
    getchar();
    getchar();
}

void handleInput(Sheet *sheet, Clipboard *clip) {
    drawSheet(sheet);
    
    printf("\nCommand: ");
    fflush(stdout);
    
    /* Simple input handling */
    char cmd[512];
    if (!fgets(cmd, sizeof(cmd), stdin)) return;
    
    /* Remove newline */
    cmd[strcspn(cmd, "\n")] = 0;
    
    if (strlen(cmd) == 0) return;
    
    /* Parse command */
    char first = tolower(cmd[0]);
    
    switch (first) {
        case 'q': /* Quit */
            if (sheet->modified) {
                printf("Save before quitting? (y/n): ");
                char resp[10];
                if (fgets(resp, sizeof(resp), stdin) && tolower(resp[0]) == 'y') {
                    saveSheet(sheet);
                }
            }
            exit(0);
            break;
            
        case 's': /* Save */
            saveSheet(sheet);
            break;
            
        case 'l': /* Load */
            loadSheet(sheet);
            break;
            
        case 'u': /* Up */
            if (sheet->curRow > 0) {
                sheet->curRow--;
                if (sheet->curRow < sheet->topRow) {
                    sheet->topRow = sheet->curRow;
                }
            }
            break;
            
        case 'd': /* Down */
            if (sheet->curRow < MAX_ROWS - 1) {
                sheet->curRow++;
                if (sheet->curRow >= sheet->topRow + 20) {
                    sheet->topRow = sheet->curRow - 19;
                }
            }
            break;
            
        case 'r': /* Right */
            if (sheet->curCol < MAX_COLS - 1) {
                sheet->curCol++;
                if (sheet->curCol >= sheet->leftCol + 6) {
                    sheet->leftCol = sheet->curCol - 5;
                }
            }
            break;
            
        case 'f': /* Left (f for "former") */
            if (sheet->curCol > 0) {
                sheet->curCol--;
                if (sheet->curCol < sheet->leftCol) {
                    sheet->leftCol = sheet->curCol;
                }
            }
            break;
            
        case 'e': /* Edit current cell */
            printf("Enter value for %s%d: ", 
                   columnName(sheet->curCol), sheet->curRow + 1);
            char value[MAX_CELL_LEN];
            if (fgets(value, sizeof(value), stdin)) {
                value[strcspn(value, "\n")] = 0;
                setCellValue(sheet, sheet->curRow, sheet->curCol, value);
            }
            break;
            
        case 'c': /* Copy */
            strcpy(clip->content, sheet->cells[sheet->curRow][sheet->curCol].raw);
            clip->hasContent = 1;
            printf("Copied. Press Enter...");
            getchar();
            break;
            
        case 'v': /* Paste */
            if (clip->hasContent) {
                setCellValue(sheet, sheet->curRow, sheet->curCol, clip->content);
                printf("Pasted. Press Enter...");
                getchar();
            }
            break;
            
        case 'x': /* Clear cell */
            setCellValue(sheet, sheet->curRow, sheet->curCol, "");
            break;
            
        case 'g': /* Go to cell */
            printf("Go to cell (e.g., B5): ");
            char cellRef[10];
            if (fgets(cellRef, sizeof(cellRef), stdin)) {
                cellRef[strcspn(cellRef, "\n")] = 0;
                int r, c;
                if (parseCell(cellRef, &r, &c)) {
                    sheet->curRow = r;
                    sheet->curCol = c;
                    
                    /* Adjust view */
                    if (sheet->curRow < sheet->topRow) {
                        sheet->topRow = sheet->curRow;
                    } else if (sheet->curRow >= sheet->topRow + 20) {
                        sheet->topRow = sheet->curRow - 19;
                    }
                    
                    if (sheet->curCol < sheet->leftCol) {
                        sheet->leftCol = sheet->curCol;
                    } else if (sheet->curCol >= sheet->leftCol + 6) {
                        sheet->leftCol = sheet->curCol - 5;
                    }
                }
            }
            break;
            
        case 'h': /* Help */
            printf("\n=== Help ===\n");
            printf("Commands (enter letter):\n");
            printf("  u/d/f/r - Move up/down/left(former)/right\n");
            printf("  e - Edit current cell\n");
            printf("  c - Copy current cell\n");
            printf("  v - Paste to current cell\n");
            printf("  x - Clear current cell\n");
            printf("  g - Go to cell\n");
            printf("  s - Save spreadsheet\n");
            printf("  l - Load spreadsheet\n");
            printf("  q - Quit\n");
            printf("  h - This help\n\n");
            printf("Formulas:\n");
            printf("  Start with = (e.g., =A1+B2)\n");
            printf("  Functions: SUM, AVG, MIN, MAX, COUNT, SQRT, POW\n");
            printf("  Example: =SUM(A1:A10)\n");
            printf("  Example: =AVG(B1:B5)*2\n");
            printf("  Example: =SQRT(C1)\n");
            printf("\nPress Enter to continue...");
            getchar();
            break;
    }
}

int main(int argc, char *argv[]) {
    Sheet sheet;
    Clipboard clip = {.hasContent = 0};
    
    initSheet(&sheet);
    
    /* Load file if specified */
    if (argc > 1) {
        FILE *fp = fopen(argv[1], "r");
        if (fp) {
            fclose(fp);
            strcpy(sheet.filename, argv[1]);
            fp = fopen(sheet.filename, "r");
            if (fp) {
                char line[4096];
                int row = 0;
                while (fgets(line, sizeof(line), fp) && row < MAX_ROWS) {
                    int col = 0;
                    char *p = line;
                    char cell[MAX_CELL_LEN];
                    int cellIdx = 0;
                    int inQuote = 0;
                    
                    while (*p && col < MAX_COLS) {
                        if (*p == '"') {
                            inQuote = !inQuote;
                            p++;
                        } else if (*p == ',' && !inQuote) {
                            cell[cellIdx] = '\0';
                            if (cellIdx > 0) {
                                setCellValue(&sheet, row, col, cell);
                            }
                            col++;
                            cellIdx = 0;
                            p++;
                        } else if (*p == '\n' || *p == '\r') {
                            break;
                        } else {
                            if (cellIdx < MAX_CELL_LEN - 1) {
                                cell[cellIdx++] = *p;
                            }
                            p++;
                        }
                    }
                    cell[cellIdx] = '\0';
                    if (cellIdx > 0) {
                        setCellValue(&sheet, row, col, cell);
                    }
                    row++;
                }
                fclose(fp);
                recalculateAll(&sheet);
                sheet.modified = 0;
            }
        }
    }
    
    printf("Welcome to TermSheet!\n");
    printf("Press Enter to start, or type 'h' and Enter for help...\n");
    getchar();
    
    /* Main loop */
    while (1) {
        handleInput(&sheet, &clip);
    }
    
    return 0;
}
