#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#ifdef _WIN32
    #include <conio.h>
    #include <windows.h>
    #define CLEAR_SCREEN "cls"
#else
    #include <termios.h>
    #include <unistd.h>
    #include <sys/ioctl.h>
    #define CLEAR_SCREEN "clear"
#endif

#define MAX_ROWS 100
#define MAX_COLS 26
#define MAX_CELL_LEN 256
#define MAX_FORMULA_LEN 512
#define MAX_STACK 64

// ANSI color codes
#define RESET   "\033[0m"
#define BOLD    "\033[1m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN    "\033[36m"
#define WHITE   "\033[37m"
#define BG_BLUE "\033[44m"
#define BG_WHITE "\033[47m"
#define BG_CYAN "\033[46m"

typedef enum { CELL_EMPTY, CELL_NUMBER, CELL_STRING, CELL_FORMULA, CELL_ERROR } CellType;

typedef struct {
    char raw[MAX_CELL_LEN];      // Raw input
    char display[MAX_CELL_LEN];  // Computed/display value
    CellType type;
    double numValue;
    int width;
} Cell;

typedef struct {
    Cell cells[MAX_ROWS][MAX_COLS];
    int curRow, curCol;
    int topRow, leftCol;
    char filename[256];
    int modified;
    char statusMsg[256];
    int editMode;
    char editBuffer[MAX_CELL_LEN];
    int editPos;
    int screenRows, screenCols;
} Spreadsheet;

// Function prototypes
void initSpreadsheet(Spreadsheet *s);
void drawSpreadsheet(Spreadsheet *s);
void handleInput(Spreadsheet *s);
void evaluateCell(Spreadsheet *s, int row, int col);
void evaluateAllCells(Spreadsheet *s);
double evaluateFormula(Spreadsheet *s, const char *formula, int *error);
void saveSpreadsheet(Spreadsheet *s, const char *filename);
void loadSpreadsheet(Spreadsheet *s, const char *filename);
void exportCSV(Spreadsheet *s, const char *filename);
void setCellValue(Spreadsheet *s, int row, int col, const char *value);
void showHelp();
char getch_custom();
void getTerminalSize(int *rows, int *cols);
void enableRawMode();
void disableRawMode();
void copyCell(Spreadsheet *s);
void pasteCell(Spreadsheet *s);
void deleteCell(Spreadsheet *s, int row, int col);
int parseCell(const char *ref, int *row, int *col);
double getCellNumber(Spreadsheet *s, int row, int col);

// Global variables
static struct termios orig_termios;
static int rawModeEnabled = 0;
static char clipboard[MAX_CELL_LEN] = "";

// Terminal handling
void enableRawMode() {
#ifndef _WIN32
    if (rawModeEnabled) return;
    tcgetattr(STDIN_FILENO, &orig_termios);
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    rawModeEnabled = 1;
#endif
}

void disableRawMode() {
#ifndef _WIN32
    if (rawModeEnabled) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
        rawModeEnabled = 0;
    }
#endif
}

char getch_custom() {
#ifdef _WIN32
    return _getch();
#else
    char c;
    read(STDIN_FILENO, &c, 1);
    return c;
#endif
}

void getTerminalSize(int *rows, int *cols) {
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
    *cols = csbi.srWindow.Right - csbi.srWindow.Left + 1;
    *rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
#else
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    *rows = w.ws_row;
    *cols = w.ws_col;
#endif
}

void clearScreen() {
    printf("\033[2J\033[H");
    fflush(stdout);
}

void moveCursor(int row, int col) {
    printf("\033[%d;%dH", row, col);
}

// Initialize spreadsheet
void initSpreadsheet(Spreadsheet *s) {
    memset(s, 0, sizeof(Spreadsheet));
    for (int i = 0; i < MAX_ROWS; i++) {
        for (int j = 0; j < MAX_COLS; j++) {
            s->cells[i][j].type = CELL_EMPTY;
            s->cells[i][j].width = 10;
            s->cells[i][j].raw[0] = '\0';
            s->cells[i][j].display[0] = '\0';
        }
    }
    s->curRow = 0;
    s->curCol = 0;
    s->topRow = 0;
    s->leftCol = 0;
    s->modified = 0;
    s->editMode = 0;
    s->editPos = 0;
    strcpy(s->statusMsg, "Ready. Press F1 for help, Ctrl+Q to quit");
    getTerminalSize(&s->screenRows, &s->screenCols);
}

// Parse cell reference like "A1" or "Z99"
int parseCell(const char *ref, int *row, int *col) {
    if (!ref || !isalpha(ref[0])) return 0;
    
    *col = toupper(ref[0]) - 'A';
    *row = atoi(ref + 1) - 1;
    
    if (*row < 0 || *row >= MAX_ROWS || *col < 0 || *col >= MAX_COLS)
        return 0;
    
    return 1;
}

// Get numeric value from cell
double getCellNumber(Spreadsheet *s, int row, int col) {
    if (row < 0 || row >= MAX_ROWS || col < 0 || col >= MAX_COLS)
        return 0.0;
    
    Cell *cell = &s->cells[row][col];
    
    if (cell->type == CELL_NUMBER || cell->type == CELL_FORMULA)
        return cell->numValue;
    
    if (cell->type == CELL_STRING) {
        double val = atof(cell->display);
        return val;
    }
    
    return 0.0;
}

// Tokenizer for formula evaluation
typedef enum { TOK_NUMBER, TOK_CELL, TOK_RANGE, TOK_FUNC, TOK_OP, TOK_LPAREN, TOK_RPAREN, TOK_COMMA, TOK_END } TokenType;

typedef struct {
    TokenType type;
    double numValue;
    char strValue[64];
    char op;
} Token;

const char *skipWhitespace(const char *p) {
    while (*p && isspace(*p)) p++;
    return p;
}

const char *getToken(const char *p, Token *tok) {
    p = skipWhitespace(p);
    
    if (!*p) {
        tok->type = TOK_END;
        return p;
    }
    
    // Operators
    if (strchr("+-*/^", *p)) {
        tok->type = TOK_OP;
        tok->op = *p;
        return p + 1;
    }
    
    // Parentheses
    if (*p == '(') {
        tok->type = TOK_LPAREN;
        return p + 1;
    }
    if (*p == ')') {
        tok->type = TOK_RPAREN;
        return p + 1;
    }
    if (*p == ',') {
        tok->type = TOK_COMMA;
        return p + 1;
    }
    
    // Numbers
    if (isdigit(*p) || (*p == '.' && isdigit(p[1]))) {
        char *end;
        tok->type = TOK_NUMBER;
        tok->numValue = strtod(p, &end);
        return end;
    }
    
    // Functions and cell references
    if (isalpha(*p)) {
        int i = 0;
        while (isalnum(*p) && i < 63) {
            tok->strValue[i++] = toupper(*p++);
        }
        tok->strValue[i] = '\0';
        
        // Check if it's a function
        if (strcmp(tok->strValue, "SUM") == 0 || strcmp(tok->strValue, "AVG") == 0 ||
            strcmp(tok->strValue, "AVERAGE") == 0 || strcmp(tok->strValue, "MIN") == 0 ||
            strcmp(tok->strValue, "MAX") == 0 || strcmp(tok->strValue, "COUNT") == 0 ||
            strcmp(tok->strValue, "ABS") == 0 || strcmp(tok->strValue, "SQRT") == 0 ||
            strcmp(tok->strValue, "POW") == 0 || strcmp(tok->strValue, "IF") == 0) {
            tok->type = TOK_FUNC;
        } else if (strlen(tok->strValue) >= 2 && isalpha(tok->strValue[0]) && isdigit(tok->strValue[1])) {
            // Check for range (A1:A10)
            const char *colon = p;
            if (*colon == ':' && isalpha(colon[1])) {
                tok->type = TOK_RANGE;
                colon++;
                while (isalnum(*colon)) {
                    if (i < 63) tok->strValue[i++] = toupper(*colon++);
                    else colon++;
                }
                tok->strValue[i] = '\0';
                p = colon;
            } else {
                tok->type = TOK_CELL;
            }
        } else {
            tok->type = TOK_CELL;
        }
        return p;
    }
    
    tok->type = TOK_END;
    return p;
}

// Evaluate range functions
double evalRange(Spreadsheet *s, const char *range, const char *func, int *error) {
    char start[32], end[32];
    int i = 0, j = 0;
    
    // Parse range "A1:A10"
    while (range[i] && range[i] != ':' && j < 31) {
        start[j++] = range[i++];
    }
    start[j] = '\0';
    
    if (range[i] != ':') {
        *error = 1;
        return 0.0;
    }
    i++; j = 0;
    
    while (range[i] && j < 31) {
        end[j++] = range[i++];
    }
    end[j] = '\0';
    
    int r1, c1, r2, c2;
    if (!parseCell(start, &r1, &c1) || !parseCell(end, &r2, &c2)) {
        *error = 1;
        return 0.0;
    }
    
    // Ensure r1 <= r2 and c1 <= c2
    if (r1 > r2) { int t = r1; r1 = r2; r2 = t; }
    if (c1 > c2) { int t = c1; c1 = c2; c2 = t; }
    
    double result = 0.0;
    int count = 0;
    
    if (strcmp(func, "SUM") == 0) {
        for (int r = r1; r <= r2; r++) {
            for (int c = c1; c <= c2; c++) {
                result += getCellNumber(s, r, c);
            }
        }
    } else if (strcmp(func, "AVG") == 0 || strcmp(func, "AVERAGE") == 0) {
        for (int r = r1; r <= r2; r++) {
            for (int c = c1; c <= c2; c++) {
                result += getCellNumber(s, r, c);
                count++;
            }
        }
        result = count > 0 ? result / count : 0.0;
    } else if (strcmp(func, "MIN") == 0) {
        result = INFINITY;
        for (int r = r1; r <= r2; r++) {
            for (int c = c1; c <= c2; c++) {
                double val = getCellNumber(s, r, c);
                if (val < result) result = val;
            }
        }
        if (result == INFINITY) result = 0.0;
    } else if (strcmp(func, "MAX") == 0) {
        result = -INFINITY;
        for (int r = r1; r <= r2; r++) {
            for (int c = c1; c <= c2; c++) {
                double val = getCellNumber(s, r, c);
                if (val > result) result = val;
            }
        }
        if (result == -INFINITY) result = 0.0;
    } else if (strcmp(func, "COUNT") == 0) {
        for (int r = r1; r <= r2; r++) {
            for (int c = c1; c <= c2; c++) {
                if (s->cells[r][c].type != CELL_EMPTY) count++;
            }
        }
        result = count;
    }
    
    return result;
}

// Expression evaluator (recursive descent parser)
double parseExpression(Spreadsheet *s, const char **p, int *error);
double parseTerm(Spreadsheet *s, const char **p, int *error);
double parseFactor(Spreadsheet *s, const char **p, int *error);

double parseFactor(Spreadsheet *s, const char **p, int *error) {
    Token tok;
    *p = getToken(*p, &tok);
    
    if (tok.type == TOK_NUMBER) {
        return tok.numValue;
    }
    
    if (tok.type == TOK_CELL) {
        int row, col;
        if (parseCell(tok.strValue, &row, &col)) {
            return getCellNumber(s, row, col);
        }
        *error = 1;
        return 0.0;
    }
    
    if (tok.type == TOK_FUNC) {
        Token next;
        const char *peek = getToken(*p, &next);
        
        if (next.type != TOK_LPAREN) {
            *error = 1;
            return 0.0;
        }
        *p = peek;
        
        // Handle range functions
        *p = getToken(*p, &next);
        if (next.type == TOK_RANGE) {
            double result = evalRange(s, next.strValue, tok.strValue, error);
            *p = getToken(*p, &next);
            if (next.type != TOK_RPAREN) *error = 1;
            return result;
        }
        
        // Handle regular functions
        if (strcmp(tok.strValue, "ABS") == 0) {
            if (next.type == TOK_RPAREN) { *error = 1; return 0.0; }
            *p = getToken(*p, &next);
            *p = skipWhitespace(*p - 1);
            double arg = parseExpression(s, p, error);
            *p = getToken(*p, &next);
            if (next.type != TOK_RPAREN) *error = 1;
            return fabs(arg);
        }
        
        if (strcmp(tok.strValue, "SQRT") == 0) {
            if (next.type == TOK_RPAREN) { *error = 1; return 0.0; }
            *p = getToken(*p, &next);
            *p = skipWhitespace(*p - 1);
            double arg = parseExpression(s, p, error);
            *p = getToken(*p, &next);
            if (next.type != TOK_RPAREN) *error = 1;
            return sqrt(arg);
        }
        
        if (strcmp(tok.strValue, "POW") == 0) {
            if (next.type == TOK_RPAREN) { *error = 1; return 0.0; }
            *p = getToken(*p, &next);
            *p = skipWhitespace(*p - 1);
            double arg1 = parseExpression(s, p, error);
            *p = getToken(*p, &next);
            if (next.type != TOK_COMMA) { *error = 1; return 0.0; }
            double arg2 = parseExpression(s, p, error);
            *p = getToken(*p, &next);
            if (next.type != TOK_RPAREN) *error = 1;
            return pow(arg1, arg2);
        }
        
        if (strcmp(tok.strValue, "SUM") == 0 || strcmp(tok.strValue, "AVG") == 0 ||
            strcmp(tok.strValue, "AVERAGE") == 0 || strcmp(tok.strValue, "MIN") == 0 ||
            strcmp(tok.strValue, "MAX") == 0) {
            // Handle multiple arguments
            double result = 0.0;
            int count = 0;
            double minVal = INFINITY, maxVal = -INFINITY;
            
            if (next.type != TOK_RPAREN) {
                *p = skipWhitespace(*p - 1);
                do {
                    double val = parseExpression(s, p, error);
                    if (strcmp(tok.strValue, "SUM") == 0) {
                        result += val;
                    } else if (strcmp(tok.strValue, "MIN") == 0) {
                        if (val < minVal) minVal = val;
                    } else if (strcmp(tok.strValue, "MAX") == 0) {
                        if (val > maxVal) maxVal = val;
                    } else {
                        result += val;
                    }
                    count++;
                    
                    *p = getToken(*p, &next);
                } while (next.type == TOK_COMMA);
            }
            
            if (next.type != TOK_RPAREN) { *error = 1; return 0.0; }
            
            if (strcmp(tok.strValue, "AVG") == 0 || strcmp(tok.strValue, "AVERAGE") == 0)
                return count > 0 ? result / count : 0.0;
            if (strcmp(tok.strValue, "MIN") == 0)
                return minVal == INFINITY ? 0.0 : minVal;
            if (strcmp(tok.strValue, "MAX") == 0)
                return maxVal == -INFINITY ? 0.0 : maxVal;
            
            return result;
        }
        
        *error = 1;
        return 0.0;
    }
    
    if (tok.type == TOK_LPAREN) {
        double result = parseExpression(s, p, error);
        *p = getToken(*p, &tok);
        if (tok.type != TOK_RPAREN) *error = 1;
        return result;
    }
    
    if (tok.type == TOK_OP && tok.op == '-') {
        return -parseFactor(s, p, error);
    }
    
    if (tok.type == TOK_OP && tok.op == '+') {
        return parseFactor(s, p, error);
    }
    
    *error = 1;
    return 0.0;
}

double parsePower(Spreadsheet *s, const char **p, int *error) {
    double left = parseFactor(s, p, error);
    
    Token tok;
    const char *peek = getToken(*p, &tok);
    
    if (tok.type == TOK_OP && tok.op == '^') {
        *p = peek;
        double right = parsePower(s, p, error);
        return pow(left, right);
    }
    
    return left;
}

double parseTerm(Spreadsheet *s, const char **p, int *error) {
    double left = parsePower(s, p, error);
    
    while (1) {
        Token tok;
        const char *peek = getToken(*p, &tok);
        
        if (tok.type == TOK_OP && (tok.op == '*' || tok.op == '/')) {
            *p = peek;
            double right = parsePower(s, p, error);
            if (tok.op == '*')
                left *= right;
            else {
                if (right == 0.0) {
                    *error = 1;
                    return 0.0;
                }
                left /= right;
            }
        } else {
            break;
        }
    }
    
    return left;
}

double parseExpression(Spreadsheet *s, const char **p, int *error) {
    double left = parseTerm(s, p, error);
    
    while (1) {
        Token tok;
        const char *peek = getToken(*p, &tok);
        
        if (tok.type == TOK_OP && (tok.op == '+' || tok.op == '-')) {
            *p = peek;
            double right = parseTerm(s, p, error);
            if (tok.op == '+')
                left += right;
            else
                left -= right;
        } else {
            break;
        }
    }
    
    return left;
}

double evaluateFormula(Spreadsheet *s, const char *formula, int *error) {
    *error = 0;
    const char *p = formula;
    
    // Skip leading '='
    if (*p == '=') p++;
    
    double result = parseExpression(s, &p, error);
    
    Token tok;
    getToken(p, &tok);
    if (tok.type != TOK_END) *error = 1;
    
    return result;
}

void evaluateCell(Spreadsheet *s, int row, int col) {
    Cell *cell = &s->cells[row][col];
    
    if (cell->type == CELL_EMPTY) {
        cell->display[0] = '\0';
        return;
    }
    
    if (cell->type == CELL_STRING || cell->type == CELL_NUMBER) {
        strncpy(cell->display, cell->raw, MAX_CELL_LEN - 1);
        return;
    }
    
    if (cell->type == CELL_FORMULA) {
        int error = 0;
        double result = evaluateFormula(s, cell->raw, &error);
        
        if (error) {
            strcpy(cell->display, "#ERROR");
            cell->type = CELL_ERROR;
        } else {
            cell->numValue = result;
            snprintf(cell->display, MAX_CELL_LEN, "%.6g", result);
        }
    }
}

void evaluateAllCells(Spreadsheet *s) {
    // Multiple passes to handle dependencies
    for (int pass = 0; pass < 3; pass++) {
        for (int i = 0; i < MAX_ROWS; i++) {
            for (int j = 0; j < MAX_COLS; j++) {
                if (s->cells[i][j].type == CELL_FORMULA) {
                    evaluateCell(s, i, j);
                }
            }
        }
    }
}

void setCellValue(Spreadsheet *s, int row, int col, const char *value) {
    Cell *cell = &s->cells[row][col];
    
    strncpy(cell->raw, value, MAX_CELL_LEN - 1);
    cell->raw[MAX_CELL_LEN - 1] = '\0';
    
    if (value[0] == '\0') {
        cell->type = CELL_EMPTY;
        cell->display[0] = '\0';
    } else if (value[0] == '=') {
        cell->type = CELL_FORMULA;
        evaluateCell(s, row, col);
        evaluateAllCells(s);
    } else {
        char *endptr;
        double num = strtod(value, &endptr);
        
        if (*endptr == '\0' && *value != '\0') {
            cell->type = CELL_NUMBER;
            cell->numValue = num;
            snprintf(cell->display, MAX_CELL_LEN, "%.6g", num);
        } else {
            cell->type = CELL_STRING;
            strncpy(cell->display, value, MAX_CELL_LEN - 1);
        }
    }
    
    s->modified = 1;
}

void deleteCell(Spreadsheet *s, int row, int col) {
    setCellValue(s, row, col, "");
}

void copyCell(Spreadsheet *s) {
    Cell *cell = &s->cells[s->curRow][s->curCol];
    strncpy(clipboard, cell->raw, MAX_CELL_LEN - 1);
    strcpy(s->statusMsg, "Cell copied");
}

void pasteCell(Spreadsheet *s) {
    setCellValue(s, s->curRow, s->curCol, clipboard);
    strcpy(s->statusMsg, "Cell pasted");
}

void drawSpreadsheet(Spreadsheet *s) {
    clearScreen();
    
    int visibleRows = s->screenRows - 4;
    int visibleCols = (s->screenCols - 5) / 12;
    
    // Title bar
    printf(BG_BLUE BOLD WHITE "  SpreadSheet Pro v1.0");
    for (int i = 22; i < s->screenCols; i++) printf(" ");
    printf(RESET "\n");
    
    // Column headers
    printf("     ");
    for (int c = s->leftCol; c < s->leftCol + visibleCols && c < MAX_COLS; c++) {
        printf(BOLD CYAN "%-12c" RESET, 'A' + c);
    }
    printf("\n");
    
    // Rows
    for (int r = s->topRow; r < s->topRow + visibleRows && r < MAX_ROWS; r++) {
        printf(BOLD CYAN "%3d " RESET, r + 1);
        
        for (int c = s->leftCol; c < s->leftCol + visibleCols && c < MAX_COLS; c++) {
            Cell *cell = &s->cells[r][c];
            
            if (r == s->curRow && c == s->curCol) {
                printf(BG_CYAN);
            }
            
            char formatted[13];
            if (strlen(cell->display) > 11) {
                strncpy(formatted, cell->display, 9);
                formatted[9] = '.';
                formatted[10] = '.';
                formatted[11] = '\0';
            } else {
                strcpy(formatted, cell->display);
            }
            
            if (cell->type == CELL_ERROR) {
                printf(RED "%-12s" RESET, formatted);
            } else if (cell->type == CELL_NUMBER || cell->type == CELL_FORMULA) {
                printf(GREEN "%-12s" RESET, formatted);
            } else {
                printf("%-12s", formatted);
            }
            
            if (r == s->curRow && c == s->curCol) {
                printf(RESET);
            }
        }
        printf("\n");
    }
    
    // Status bar
    printf("\n" BG_WHITE "  ");
    char cellRef[8];
    snprintf(cellRef, sizeof(cellRef), "%c%d", 'A' + s->curCol, s->curRow + 1);
    printf(BOLD "%s: " RESET, cellRef);
    
    if (s->editMode) {
        printf(YELLOW "[EDIT] %s" RESET, s->editBuffer);
    } else {
        Cell *cell = &s->cells[s->curRow][s->curCol];
        if (cell->type != CELL_EMPTY) {
            printf("%s", cell->raw);
        }
    }
    
    for (int i = strlen(cellRef) + strlen(s->editMode ? s->editBuffer : s->cells[s->curRow][s->curCol].raw) + 10;
         i < s->screenCols; i++) {
        printf(" ");
    }
    printf(RESET "\n");
    
    printf(BG_WHITE "  " RESET);
    printf(MAGENTA "%s" RESET, s->statusMsg);
    for (int i = strlen(s->statusMsg) + 2; i < s->screenCols; i++) printf(" ");
    
    fflush(stdout);
}

void showHelp() {
    clearScreen();
    printf(BOLD CYAN "\n╔══════════════════════════════════════════════════════════════╗\n");
    printf("║              SPREADSHEET PRO - HELP GUIDE                   ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n\n" RESET);
    
    printf(BOLD "NAVIGATION:\n" RESET);
    printf("  Arrow Keys    - Move cursor\n");
    printf("  Home/End      - First/Last column\n");
    printf("  PgUp/PgDn     - Scroll up/down\n\n");
    
    printf(BOLD "EDITING:\n" RESET);
    printf("  Enter         - Edit current cell\n");
    printf("  Esc           - Cancel edit\n");
    printf("  Delete        - Clear cell\n");
    printf("  Ctrl+C        - Copy cell\n");
    printf("  Ctrl+V        - Paste cell\n\n");
    
    printf(BOLD "FILE OPERATIONS:\n" RESET);
    printf("  Ctrl+S        - Save spreadsheet\n");
    printf("  Ctrl+L        - Load spreadsheet\n");
    printf("  Ctrl+E        - Export to CSV\n");
    printf("  Ctrl+Q        - Quit\n\n");
    
    printf(BOLD "FORMULAS:\n" RESET);
    printf("  Start with '=' sign\n");
    printf("  Operators: +, -, *, /, ^\n");
    printf("  Cell refs: A1, B2, etc.\n\n");
    
    printf(BOLD "FUNCTIONS:\n" RESET);
    printf("  SUM(A1:A10)   - Sum range\n");
    printf("  AVG(A1:A10)   - Average range\n");
    printf("  MIN(A1:A10)   - Minimum value\n");
    printf("  MAX(A1:A10)   - Maximum value\n");
    printf("  COUNT(A1:A10) - Count cells\n");
    printf("  ABS(value)    - Absolute value\n");
    printf("  SQRT(value)   - Square root\n");
    printf("  POW(x,y)      - Power x^y\n\n");
    
    printf(BOLD "EXAMPLES:\n" RESET);
    printf("  =A1+B1        - Add two cells\n");
    printf("  =SUM(A1:A10)  - Sum range A1 to A10\n");
    printf("  =A1*2+B2      - Expression\n");
    printf("  =SQRT(A1)     - Square root of A1\n\n");
    
    printf(YELLOW "Press any key to return..." RESET);
    getch_custom();
}

void saveSpreadsheet(Spreadsheet *s, const char *filename) {
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        strcpy(s->statusMsg, "Error: Cannot save file");
        return;
    }
    
    fprintf(fp, "SPREADSHEET_V1\n");
    
    for (int i = 0; i < MAX_ROWS; i++) {
        for (int j = 0; j < MAX_COLS; j++) {
            Cell *cell = &s->cells[i][j];
            if (cell->type != CELL_EMPTY) {
                fprintf(fp, "%d,%d,%d,%s\n", i, j, cell->type, cell->raw);
            }
        }
    }
    
    fclose(fp);
    s->modified = 0;
    strcpy(s->filename, filename);
    snprintf(s->statusMsg, sizeof(s->statusMsg), "Saved to %s", filename);
}

void loadSpreadsheet(Spreadsheet *s, const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        strcpy(s->statusMsg, "Error: Cannot open file");
        return;
    }
    
    char header[32];
    if (!fgets(header, sizeof(header), fp) || strncmp(header, "SPREADSHEET_V1", 14) != 0) {
        fclose(fp);
        strcpy(s->statusMsg, "Error: Invalid file format");
        return;
    }
    
    initSpreadsheet(s);
    
    int row, col, type;
    char raw[MAX_CELL_LEN];
    
    while (fscanf(fp, "%d,%d,%d,", &row, &col, &type) == 3) {
        if (fgets(raw, sizeof(raw), fp)) {
            raw[strcspn(raw, "\n")] = '\0';
            
            if (row >= 0 && row < MAX_ROWS && col >= 0 && col < MAX_COLS) {
                setCellValue(s, row, col, raw);
            }
        }
    }
    
    evaluateAllCells(s);
    fclose(fp);
    s->modified = 0;
    strcpy(s->filename, filename);
    snprintf(s->statusMsg, sizeof(s->statusMsg), "Loaded from %s", filename);
}

void exportCSV(Spreadsheet *s, const char *filename) {
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        strcpy(s->statusMsg, "Error: Cannot export file");
        return;
    }
    
    for (int i = 0; i < MAX_ROWS; i++) {
        int hasData = 0;
        for (int j = 0; j < MAX_COLS; j++) {
            if (s->cells[i][j].type != CELL_EMPTY) {
                hasData = 1;
                break;
            }
        }
        
        if (!hasData) continue;
        
        for (int j = 0; j < MAX_COLS; j++) {
            Cell *cell = &s->cells[i][j];
            
            if (cell->type == CELL_STRING && strchr(cell->display, ',')) {
                fprintf(fp, "\"%s\"", cell->display);
            } else if (cell->type != CELL_EMPTY) {
                fprintf(fp, "%s", cell->display);
            }
            
            if (j < MAX_COLS - 1) fprintf(fp, ",");
        }
        fprintf(fp, "\n");
    }
    
    fclose(fp);
    snprintf(s->statusMsg, sizeof(s->statusMsg), "Exported to %s", filename);
}

void handleInput(Spreadsheet *s) {
    char c = getch_custom();
    
    if (s->editMode) {
        if (c == 27) { // ESC
            s->editMode = 0;
            s->editBuffer[0] = '\0';
            s->editPos = 0;
            strcpy(s->statusMsg, "Edit cancelled");
        } else if (c == '\n' || c == '\r') {
            setCellValue(s, s->curRow, s->curCol, s->editBuffer);
            evaluateAllCells(s);
            s->editMode = 0;
            s->editBuffer[0] = '\0';
            s->editPos = 0;
            strcpy(s->statusMsg, "Cell updated");
        } else if (c == 127 || c == 8) { // Backspace
            if (s->editPos > 0) {
                s->editBuffer[--s->editPos] = '\0';
            }
        } else if (c >= 32 && c < 127 && s->editPos < MAX_CELL_LEN - 1) {
            s->editBuffer[s->editPos++] = c;
            s->editBuffer[s->editPos] = '\0';
        }
        return;
    }
    
    // Arrow keys and navigation
    if (c == 27) { // Escape sequence
        char seq[2];
        seq[0] = getch_custom();
        seq[1] = getch_custom();
        
        if (seq[0] == '[') {
            switch (seq[1]) {
                case 'A': // Up
                    if (s->curRow > 0) {
                        s->curRow--;
                        if (s->curRow < s->topRow) s->topRow = s->curRow;
                    }
                    break;
                case 'B': // Down
                    if (s->curRow < MAX_ROWS - 1) {
                        s->curRow++;
                        int visibleRows = s->screenRows - 4;
                        if (s->curRow >= s->topRow + visibleRows) s->topRow = s->curRow - visibleRows + 1;
                    }
                    break;
                case 'C': // Right
                    if (s->curCol < MAX_COLS - 1) {
                        s->curCol++;
                        int visibleCols = (s->screenCols - 5) / 12;
                        if (s->curCol >= s->leftCol + visibleCols) s->leftCol = s->curCol - visibleCols + 1;
                    }
                    break;
                case 'D': // Left
                    if (s->curCol > 0) {
                        s->curCol--;
                        if (s->curCol < s->leftCol) s->leftCol = s->curCol;
                    }
                    break;
                case '5': // Page Up
                    getch_custom(); // consume '~'
                    s->curRow = (s->curRow > 10) ? s->curRow - 10 : 0;
                    s->topRow = (s->topRow > 10) ? s->topRow - 10 : 0;
                    break;
                case '6': // Page Down
                    getch_custom(); // consume '~'
                    s->curRow = (s->curRow < MAX_ROWS - 10) ? s->curRow + 10 : MAX_ROWS - 1;
                    int visibleRows = s->screenRows - 4;
                    if (s->curRow >= s->topRow + visibleRows) s->topRow = s->curRow - visibleRows + 1;
                    break;
                case 'H': // Home
                    s->curCol = 0;
                    s->leftCol = 0;
                    break;
                case 'F': // End
                    s->curCol = MAX_COLS - 1;
                    break;
            }
        }
    } else if (c == '\n' || c == '\r') { // Enter - edit mode
        s->editMode = 1;
        strcpy(s->editBuffer, s->cells[s->curRow][s->curCol].raw);
        s->editPos = strlen(s->editBuffer);
        strcpy(s->statusMsg, "Editing cell (ESC to cancel, Enter to confirm)");
    } else if (c == 127 || c == 'd') { // Delete
        deleteCell(s, s->curRow, s->curCol);
        evaluateAllCells(s);
        strcpy(s->statusMsg, "Cell deleted");
    } else if (c == 3) { // Ctrl+C
        copyCell(s);
    } else if (c == 22) { // Ctrl+V
        pasteCell(s);
        evaluateAllCells(s);
    } else if (c == 19) { // Ctrl+S
        char filename[256];
        printf("\n\nEnter filename to save: ");
        disableRawMode();
        fgets(filename, sizeof(filename), stdin);
        filename[strcspn(filename, "\n")] = '\0';
        enableRawMode();
        
        if (strlen(filename) > 0) {
            saveSpreadsheet(s, filename);
        } else {
            strcpy(s->statusMsg, "Save cancelled");
        }
    } else if (c == 12) { // Ctrl+L
        char filename[256];
        printf("\n\nEnter filename to load: ");
        disableRawMode();
        fgets(filename, sizeof(filename), stdin);
        filename[strcspn(filename, "\n")] = '\0';
        enableRawMode();
        
        if (strlen(filename) > 0) {
            loadSpreadsheet(s, filename);
        } else {
            strcpy(s->statusMsg, "Load cancelled");
        }
    } else if (c == 5) { // Ctrl+E
        char filename[256];
        printf("\n\nEnter CSV filename to export: ");
        disableRawMode();
        fgets(filename, sizeof(filename), stdin);
        filename[strcspn(filename, "\n")] = '\0';
        enableRawMode();
        
        if (strlen(filename) > 0) {
            exportCSV(s, filename);
        } else {
            strcpy(s->statusMsg, "Export cancelled");
        }
    } else if (c == 17) { // Ctrl+Q
        if (s->modified) {
            printf("\n\nFile modified. Save before quitting? (y/n): ");
            disableRawMode();
            char response = getchar();
            enableRawMode();
            
            if (response == 'y' || response == 'Y') {
                char filename[256];
                printf("Enter filename: ");
                disableRawMode();
                fgets(filename, sizeof(filename), stdin);
                filename[strcspn(filename, "\n")] = '\0';
                enableRawMode();
                
                if (strlen(filename) > 0) {
                    saveSpreadsheet(s, filename);
                }
            }
        }
        
        disableRawMode();
        clearScreen();
        printf("Thank you for using SpreadSheet Pro!\n");
        exit(0);
    } else if (c == 'h' || c == '?') { // Help
        showHelp();
    }
}

int main() {
    Spreadsheet sheet;
    initSpreadsheet(&sheet);
    
    enableRawMode();
    
    // Demo data
    setCellValue(&sheet, 0, 0, "Product");
    setCellValue(&sheet, 0, 1, "Price");
    setCellValue(&sheet, 0, 2, "Quantity");
    setCellValue(&sheet, 0, 3, "Total");
    
    setCellValue(&sheet, 1, 0, "Apples");
    setCellValue(&sheet, 1, 1, "1.50");
    setCellValue(&sheet, 1, 2, "10");
    setCellValue(&sheet, 1, 3, "=B2*C2");
    
    setCellValue(&sheet, 2, 0, "Oranges");
    setCellValue(&sheet, 2, 1, "2.00");
    setCellValue(&sheet, 2, 2, "5");
    setCellValue(&sheet, 2, 3, "=B3*C3");
    
    setCellValue(&sheet, 3, 0, "Bananas");
    setCellValue(&sheet, 3, 1, "0.75");
    setCellValue(&sheet, 3, 2, "20");
    setCellValue(&sheet, 3, 3, "=B4*C4");
    
    setCellValue(&sheet, 5, 0, "Total:");
    setCellValue(&sheet, 5, 3, "=SUM(D2:D4)");
    
    evaluateAllCells(&sheet);
    sheet.modified = 0;
    
    while (1) {
        getTerminalSize(&sheet.screenRows, &sheet.screenCols);
        drawSpreadsheet(&sheet);
        handleInput(&sheet);
    }
    
    disableRawMode();
    return 0;
}
