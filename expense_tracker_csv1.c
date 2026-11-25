#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#define MAX 1000
#define EXPENSE_FILE "tracker_expenses.csv"
#define SETTINGS_FILE "financial_settings.csv"

typedef struct {
    int id;
    char date[11];
    char category[30];
    char description[100];
    double amount;
} Expense;

static Expense expenses[MAX];
static int count = 0, nextId = 1;
static double monthlyBudget = 0;
static double monthlySalary = 0;

void readLine(const char *prompt, char *out, size_t size) {
    int ch;
    for (;;) {
        printf("%s", prompt);
        if (!fgets(out, (int)size, stdin)) { puts("Input ended."); exit(0); }
        size_t len = strlen(out);
        if (len && out[len - 1] == '\n') out[len - 1] = '\0';
        else { while ((ch = getchar()) != '\n' && ch != EOF) {} }
        if (out[0]) return;
        puts("Input cannot be empty.");
    }
}

int readInt(const char *prompt, int min, int max) {
    char buf[80], extra;
    int n;
    for (;;) {
        readLine(prompt, buf, sizeof(buf));
        if (sscanf(buf, " %d %c", &n, &extra) == 1 && n >= min && n <= max) return n;
        printf("Enter an integer from %d to %d.\n", min, max);
    }
}

double readAmount(const char *prompt, int allowZero) {
    char buf[80], extra;
    double n;
    for (;;) {
        readLine(prompt, buf, sizeof(buf));
        if (sscanf(buf, " %lf %c", &n, &extra) == 1 &&
            n <= 1000000000000.0 && (allowZero ? n >= 0 : n > 0)) return n;
        puts("Enter a valid positive amount (zero allowed for budget).");
    }
}

int validDate(const char *s) {
    int d, m, y, days;
    if (strlen(s) != 10 || s[2] != '-' || s[5] != '-') return 0;
    for (int i = 0; i < 10; i++) if (i != 2 && i != 5 && !isdigit((unsigned char)s[i])) return 0;
    if (sscanf(s, "%2d-%2d-%4d", &d, &m, &y) != 3 || y < 1900 || y > 9999 || m < 1 || m > 12) return 0;
    int monthDays[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    days = monthDays[m - 1];
    if (m == 2 && (y % 400 == 0 || (y % 4 == 0 && y % 100 != 0))) days++;
    return d >= 1 && d <= days;
}

void readDate(char *date) {
    for (;;) {
        readLine("Date (DD-MM-YYYY): ", date, 11);
        if (validDate(date)) return;
        puts("Invalid date. Example: 24-11-2026.");
    }
}

int sameMonth(const char *date, int month, int year) {
    int d, m, y;
    return sscanf(date, "%2d-%2d-%4d", &d, &m, &y) == 3 && m == month && y == year;
}

int equalsIgnoreCase(const char *a, const char *b) {
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
        a++; b++;
    }
    return *a == '\0' && *b == '\0';
}

double monthlyTotal(int month, int year);

void writeCSVField(FILE *fp, const char *text) {
    fputc('"', fp);
    for (const char *p = text; *p; p++) {
        if (*p == '"') fputc('"', fp);
        fputc(*p, fp);
    }
    fputc('"', fp);
}

int readCSVField(const char **p, char *out, size_t size) {
    size_t n = 0;
    if (**p != '"') return 0;
    (*p)++;
    while (**p) {
        if (**p == '"') {
            if ((*p)[1] == '"') {
                if (n + 1 < size) out[n++] = '"';
                *p += 2;
            } else {
                (*p)++;
                out[n] = '\0';
                return 1;
            }
        } else {
            if (n + 1 < size) out[n++] = **p;
            (*p)++;
        }
    }
    return 0;
}

void saveData(void) {
    FILE *fp = fopen(EXPENSE_FILE, "w");
    if (!fp) { perror("Cannot save expenses"); return; }
    fputs("ID,Date,Category,Description,Amount\n", fp);
    for (int i = 0; i < count; i++) {
        Expense *e = &expenses[i];
        fprintf(fp, "%d,%s,", e->id, e->date);
        writeCSVField(fp, e->category);
        fputc(',', fp);
        writeCSVField(fp, e->description);
        fprintf(fp, ",%.2f\n", e->amount);
    }
    if (fclose(fp) != 0) puts("Warning: expense CSV may be incomplete.");

    fp = fopen(SETTINGS_FILE, "w");
    if (!fp) { perror("Cannot save financial settings"); return; }
    fputs("MonthlySalary,MonthlyBudget\n", fp);
    fprintf(fp, "%.2f,%.2f\n", monthlySalary, monthlyBudget);
    if (fclose(fp) != 0) puts("Warning: settings CSV may be incomplete.");
}

void loadData(void) {
    count = 0; nextId = 1; monthlyBudget = 0; monthlySalary = 0;

    FILE *fp = fopen(EXPENSE_FILE, "r");
    if (fp) {
        char line[512];
        fgets(line, sizeof(line), fp); /* header */
        while (count < MAX && fgets(line, sizeof(line), fp)) {
            Expense e = {0};
            char *p = line, *end;
            long id = strtol(p, &end, 10);
            if (end == p || *end != ',') continue;
            p = end + 1;
            char *comma = strchr(p, ',');
            if (!comma || (size_t)(comma - p) >= sizeof(e.date)) continue;
            memcpy(e.date, p, (size_t)(comma - p));
            e.date[comma - p] = '\0';
            p = comma + 1;
            const char *cp = p;
            if (!readCSVField(&cp, e.category, sizeof(e.category)) || *cp != ',') continue;
            cp++;
            if (!readCSVField(&cp, e.description, sizeof(e.description)) || *cp != ',') continue;
            cp++;
            char *amountEnd;
            double amount = strtod(cp, &amountEnd);
            if (amountEnd == cp || id <= 0 || id > 2147483647L || !validDate(e.date) || amount <= 0) continue;
            e.id = (int)id; e.amount = amount;
            expenses[count++] = e;
            if (e.id >= nextId && e.id < 2147483647) nextId = e.id + 1;
        }
        fclose(fp);
    }

    fp = fopen(SETTINGS_FILE, "r");
    if (fp) {
        char line[200];
        fgets(line, sizeof(line), fp); /* header */
        if (fgets(line, sizeof(line), fp)) {
            double salary, budget;
            if (sscanf(line, "%lf,%lf", &salary, &budget) == 2 &&
                salary >= 0 && salary <= 1000000000000.0 &&
                budget >= 0 && budget <= 1000000000000.0 &&
                (salary == 0 || budget <= salary)) {
                monthlySalary = salary;
                monthlyBudget = budget;
            }
        }
        fclose(fp);
    }
}

void startupBudgetWarning(void) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    if (!t) return;
    int month = t->tm_mon + 1, year = t->tm_year + 1900;
    double spent = monthlyTotal(month, year);

    if (monthlySalary > 0 && spent >= monthlySalary) {
        printf("\nCRITICAL WARNING: Spending this month is Rs %.2f and has reached/exceeded your salary of Rs %.2f!\n",
               spent, monthlySalary);
    }
    if (monthlyBudget > 0 && spent >= monthlyBudget) {
        printf("\nBUDGET EXCEEDED: Spending this month is Rs %.2f against a budget of Rs %.2f!\n",
               spent, monthlyBudget);
    } else if (monthlyBudget > 0 && spent >= monthlyBudget * 0.80) {
        printf("\nBUDGET WARNING: You have already used %.1f%% of this month's budget.\n",
               100.0 * spent / monthlyBudget);
    }
}

void printHeader(void) {
    printf("\n%-5s %-12s %-18s %-30s %12s\n", "ID", "Date", "Category", "Description", "Amount (Rs)");
    puts("------------------------------------------------------------------------------------");
}

void printExpense(const Expense *e) {
    printf("%-5d %-12s %-18.18s %-30.30s %12.2f\n", e->id, e->date, e->category, e->description, e->amount);
}

int findExpenseIndex(int id) {
    for (int i = 0; i < count; i++) if (expenses[i].id == id) return i;
    return -1;
}

void addExpense(void) {
    if (count >= MAX) { puts("Storage full."); return; }
    if (nextId == 2147483647) { puts("ID limit reached."); return; }
    Expense e = {0};
    e.id = nextId;
    readDate(e.date);
    readLine("Category: ", e.category, sizeof(e.category));
    readLine("Description: ", e.description, sizeof(e.description));
    e.amount = readAmount("Amount (Rs): ", 0);

    int d, month, year;
    sscanf(e.date, "%2d-%2d-%4d", &d, &month, &year);
    double newMonthlyTotal = monthlyTotal(month, year) + e.amount;

    if (monthlySalary > 0 && newMonthlyTotal > monthlySalary) {
        printf("CRITICAL ALERT: This expense would make monthly spending Rs %.2f, above your salary of Rs %.2f.\n",
               newMonthlyTotal, monthlySalary);
        puts("Expense not added because spending cannot exceed monthly salary.");
        return;
    }

    if (monthlyBudget > 0 && newMonthlyTotal > monthlyBudget) {
        char answer[10];
        printf("WARNING: This expense would make monthly spending Rs %.2f, above your budget of Rs %.2f.\n",
               newMonthlyTotal, monthlyBudget);
        printf("You would exceed the budget by Rs %.2f.\n", newMonthlyTotal - monthlyBudget);
        readLine("Add this expense anyway? (Y/N): ", answer, sizeof(answer));
        if (tolower((unsigned char)answer[0]) != 'y') {
            puts("Expense cancelled and was NOT added.");
            return;
        }
        puts("WARNING acknowledged. Adding expense above the monthly budget.");
    } else if (monthlyBudget > 0 && newMonthlyTotal >= monthlyBudget * 0.80) {
        printf("WARNING: After this expense, you will have used at least 80%% of your monthly budget.\n");
    }

    expenses[count++] = e;
    nextId++;
    saveData();
    printf("Added expense #%d.\n", e.id);
}

void viewExpenses(void) {
    if (!count) { puts("No expenses found."); return; }
    printHeader();
    for (int i = 0; i < count; i++) printExpense(&expenses[i]);
}

double calculateTotal(void) {
    double total = 0;
    for (int i = 0; i < count; i++) total += expenses[i].amount;
    return total;
}

void showTotal(void) { printf("Total recorded spending: Rs %.2f\n", calculateTotal()); }

void searchCategory(void) {
    char category[30]; int found = 0; double total = 0;
    readLine("Category to search: ", category, sizeof(category));
    for (int i = 0; i < count; i++) if (equalsIgnoreCase(category, expenses[i].category)) {
        if (!found) printHeader();
        printExpense(&expenses[i]); total += expenses[i].amount; found++;
    }
    if (!found) puts("No matching expenses.");
    else printf("%d record(s); total Rs %.2f\n", found, total);
}

void setBudget(void) {
    monthlySalary = readAmount("Enter monthly salary/income (Rs): ", 0);
    for (;;) {
        monthlyBudget = readAmount("Set monthly spending budget (Rs; 0 disables budget alerts): ", 1);
        if (monthlyBudget <= monthlySalary) break;
        printf("Budget cannot exceed your salary of Rs %.2f.\n", monthlySalary);
    }
    saveData();
    puts("Salary and monthly budget saved.");
    printf("Salary: Rs %.2f | Budget: Rs %.2f | Planned savings: Rs %.2f\n",
           monthlySalary, monthlyBudget, monthlySalary - monthlyBudget);
}

void readMonthYear(int *month, int *year) {
    *month = readInt("Month (1-12): ", 1, 12);
    *year = readInt("Year (1900-9999): ", 1900, 9999);
}

double monthlyTotal(int month, int year) {
    double total = 0;
    for (int i = 0; i < count; i++) if (sameMonth(expenses[i].date, month, year)) total += expenses[i].amount;
    return total;
}

void checkBudget(void) {
    int month, year; readMonthYear(&month, &year);
    double spent = monthlyTotal(month, year);
    printf("\nBudget report for %02d/%04d\nSpent: Rs %.2f\n", month, year, spent);
    if (monthlyBudget == 0) { puts("No monthly budget set."); return; }
    printf("Budget: Rs %.2f\nRemaining: Rs %.2f\n", monthlyBudget, monthlyBudget - spent);
    double percent = 100.0 * spent / monthlyBudget;
    printf("Used: %.1f%%\n", percent);
    if (percent >= 100) puts("ALERT: Monthly budget reached or exceeded!");
    else if (percent >= 80) puts("WARNING: At least 80% of budget used.");
    else puts("Spending is within the budget alert threshold.");
}

void deleteExpense(void) {
    int id = readInt("Expense ID to delete: ", 1, 2147483647);
    int index = findExpenseIndex(id);
    if (index < 0) { puts("Expense not found."); return; }
    for (int i = index; i < count - 1; i++) expenses[i] = expenses[i + 1];
    count--; saveData(); puts("Expense deleted.");
}

void editExpense(void) {
    int id = readInt("Expense ID to edit: ", 1, 2147483647);
    int index = findExpenseIndex(id);
    if (index < 0) { puts("Expense not found."); return; }
    Expense updated = expenses[index];
    puts("Enter replacement details:");
    readDate(updated.date);
    readLine("Category: ", updated.category, sizeof(updated.category));
    readLine("Description: ", updated.description, sizeof(updated.description));
    updated.amount = readAmount("Amount (Rs): ", 0);
    expenses[index] = updated;
    saveData(); puts("Expense updated.");
}

void searchDate(void) {
    char date[11]; int found = 0;
    readDate(date);
    for (int i = 0; i < count; i++) if (strcmp(date, expenses[i].date) == 0) {
        if (!found) printHeader();
        printExpense(&expenses[i]); found++;
    }
    if (!found) puts("No expenses on that date.");
}

void monthlyReport(void) {
    int month, year, found = 0; readMonthYear(&month, &year);
    for (int i = 0; i < count; i++) if (sameMonth(expenses[i].date, month, year)) {
        if (!found) printHeader();
        printExpense(&expenses[i]); found++;
    }
    printf("%d expenses; monthly total Rs %.2f\n", found, monthlyTotal(month, year));
}

void categoryReport(void) {
    if (!count) { puts("No expenses found."); return; }
    int visited[MAX] = {0};
    puts("\nCategory                  Total (Rs)");
    puts("---------------------------------------");
    for (int i = 0; i < count; i++) {
        if (visited[i]) continue;
        double total = expenses[i].amount;
        for (int j = i + 1; j < count; j++) if (equalsIgnoreCase(expenses[i].category, expenses[j].category)) {
            total += expenses[j].amount; visited[j] = 1;
        }
        printf("%-24.24s %12.2f\n", expenses[i].category, total);
    }
}

void highestExpense(void) {
    if (!count) { puts("No expenses found."); return; }
    int best = 0;
    for (int i = 1; i < count; i++) if (expenses[i].amount > expenses[best].amount) best = i;
    printHeader(); printExpense(&expenses[best]);
}

int compareAmount(const void *a, const void *b) {
    const Expense *x = a, *y = b;
    return (x->amount > y->amount) - (x->amount < y->amount);
}

void sortExpenses(void) {
    if (!count) { puts("No expenses found."); return; }
    Expense sorted[MAX];
    memcpy(sorted, expenses, (size_t)count * sizeof(Expense));
    qsort(sorted, (size_t)count, sizeof(Expense), compareAmount);
    puts("Sorted by amount (ascending; original order unchanged):");
    printHeader();
    for (int i = 0; i < count; i++) printExpense(&sorted[i]);
}

void statistics(void) {
    if (!count) { puts("No expenses to analyze."); return; }
    double total = calculateTotal(), min = expenses[0].amount, max = min;
    for (int i = 1; i < count; i++) {
        if (expenses[i].amount < min) min = expenses[i].amount;
        if (expenses[i].amount > max) max = expenses[i].amount;
    }
    printf("\nRecords: %d\nTotal: Rs %.2f\nAverage: Rs %.2f\nMinimum: Rs %.2f\nMaximum: Rs %.2f\n",
           count, total, total / count, min, max);
}

void exportCSV(void) {
    FILE *fp = fopen("expenses_export.csv", "w");
    if (!fp) { perror("Cannot export CSV"); return; }
    fputs("ID,Date,Category,Description,Amount\n", fp);
    for (int i = 0; i < count; i++) {
        Expense *e = &expenses[i];
        fprintf(fp, "%d,%s,\"", e->id, e->date);
        for (const char *p = e->category; *p; p++) { if (*p == '"') fputc('"', fp); fputc(*p, fp); }
        fputs("\",\"", fp);
        for (const char *p = e->description; *p; p++) { if (*p == '"') fputc('"', fp); fputc(*p, fp); }
        fprintf(fp, "\",%.2f\n", e->amount);
    }
    if (fclose(fp) != 0) puts("CSV export may be incomplete.");
    else puts("Exported to expenses_export.csv");
}

void showMenu(void) {
    puts("\n========== ADVANCED EXPENSE TRACKER ==========");
    puts(" 1. Add expense             2. View all expenses");
    puts(" 3. Total expenses          4. Search by category");
    puts(" 5. Set salary & budget     6. Monthly budget status");
    puts(" 7. Delete expense          8. Edit expense");
    puts(" 9. Search by date        10. Monthly report");
    puts("11. Category analysis     12. Highest expense");
    puts("13. Sort by amount        14. Statistics");
    puts("15. Export to CSV          0. Exit");
}

int main(void) {
    loadData();
    startupBudgetWarning();
    for (;;) {
        showMenu();
        int choice = readInt("Choose option: ", 0, 15);
        switch (choice) {
            case 1: addExpense(); break;
            case 2: viewExpenses(); break;
            case 3: showTotal(); break;
            case 4: searchCategory(); break;
            case 5: setBudget(); break;
            case 6: checkBudget(); break;
            case 7: deleteExpense(); break;
            case 8: editExpense(); break;
            case 9: searchDate(); break;
            case 10: monthlyReport(); break;
            case 11: categoryReport(); break;
            case 12: highestExpense(); break;
            case 13: sortExpenses(); break;
            case 14: statistics(); break;
            case 15: exportCSV(); break;
            case 0: saveData(); puts("Goodbye!"); return 0;
        }
    }
}
