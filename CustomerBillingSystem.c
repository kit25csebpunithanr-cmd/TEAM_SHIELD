#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#define CSV_PATH "C:/Users/ajoli/OneDrive/Documents/CP_Mine_project/file.csv"
#define MAX_ITEMS 50
#define LINE_BUF 512

struct Item {
    char name[50];
    float quantity;        
    char unit[8];          
    float price;
    float total;
};

struct Customer {
    char name[50];
    char mobile[15];
};


static float parse_number(const char *str) {
    if (!str || !*str) return 0.0f;
    if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) {
        return (float)strtol(str, NULL, 16);
    }
    return (float)atof(str);
}


static int parse_int(const char *str) {
    if (!str || !*str) return 0;
    if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) {
        return (int)strtol(str, NULL, 16);
    }
    return atoi(str);
}

/* Prototypes */
static void ensure_header(void);
static float compute_customer_total(const char *name_or_mobile);
static void read_line(const char *prompt, char *buf, size_t sz);
static int contains_substr(const char *full, const char *partial);
static int matches_customer_record(const char *cname, const char *cmobile, const char *search);
static int customer_exists(const char *name_or_mobile);
static int mobile_exists_exact(const char *mobile);
static int get_customer_info(const char *name_or_mobile, char *name, size_t ns, char *mobile, size_t ms);
static int valid_mobile(const char *m);
static int find_customers_by_mobile(const char *mobile, char names[][50], int max);
static void add_new_bill(void);
static void update_customer_bill(void);
static void view_customer_bills(void);
static int clear_customer_data_by_name(const char *name_or_mobile);
static void clear_customer_by_name(void);
static void view_all_records(void);

/* SAFE input: read a line from stdin and strip newline */
static void read_line(const char *prompt, char *buf, size_t sz) {
    if (prompt) printf("%s", prompt);
    if (!fgets(buf, (int)sz, stdin)) { buf[0] = '\0'; return; }
    size_t len = strlen(buf);
    if (len && buf[len - 1] == '\n') buf[len - 1] = '\0';
}

/* case-insensitive substring check */
static int contains_substr(const char *full, const char *partial) {
    if (!full || !partial) return 0;
    char a[LINE_BUF], b[LINE_BUF];
    size_t i;
    strncpy(a, full, sizeof(a) - 1); a[sizeof(a)-1] = '\0';
    strncpy(b, partial, sizeof(b) - 1); b[sizeof(b)-1] = '\0';
    for (i = 0; a[i]; ++i) a[i] = (char)tolower((unsigned char)a[i]);
    for (i = 0; b[i]; ++i) b[i] = (char)tolower((unsigned char)b[i]);
    return strstr(a, b) != NULL;
}

/* match either name or mobile (substring) */
static int matches_customer_record(const char *cname, const char *cmobile, const char *search) {
    if (!search) return 0;
    if (cname && contains_substr(cname, search)) return 1;
    if (cmobile && contains_substr(cmobile, search)) return 1;
    return 0;
}

/* ensure header line exists in CSV - use new schema with Date/Time and Unit */
static void ensure_header(void) {
    FILE *f = fopen(CSV_PATH, "r");
    if (f) { fclose(f); return; }
    f = fopen(CSV_PATH, "w");
    if (!f) return;
    fprintf(f, "Date,Time,Customer,Mobile,Item,Qty,Unit,Price,Total\n");
    fclose(f);
}

/* compute customer's total (supports both new 9-col and old 7-col formats) */
static float compute_customer_total(const char *name_or_mobile) {
    FILE *f = fopen(CSV_PATH, "r");
    if (!f) return 0.0f;

    char line[LINE_BUF];
    if (!fgets(line, sizeof(line), f)) { fclose(f); return 0.0f; } /* skip header */

    float sum_items = 0.0f;
    int found_any = 0;

    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '\n' || line[0] == '\0') continue;
        char copy[LINE_BUF];
        strncpy(copy, line, sizeof(copy)-1); copy[sizeof(copy)-1] = '\0';

        /* tokenise up to 10 columns to detect schema */
        char *cols[12] = {0};
        int c = 0;
        char *tok = strtok(copy, ",");
        while (tok && c < 12) {
            size_t len = strlen(tok);
            while (len > 0 && (tok[len-1] == '\n' || tok[len-1] == '\r')) tok[--len] = '\0';
            cols[c++] = tok;
            tok = strtok(NULL, ",");
        }

        const char cname = (c >= 3) ? cols[2] : (c >= 1 ? cols[0] : NULL); / new schema -> cols[2], old -> cols[0] */
        const char *cmobile = (c >= 4) ? cols[3] : (c >= 2 ? cols[1] : NULL);

        if (!matches_customer_record(cname, cmobile, name_or_mobile)) continue;
        found_any = 1;

        /* new schema: total is cols[8] when c >= 9 */
        if (c >= 9 && cols[8] && cols[8][0] != '\0') {
            sum_items += parse_number(cols[8]);
            continue;
        }

        /* old schema: item total at cols[5] (index 5) when c >= 6 */
        if (c >= 6 && cols[5] && cols[5][0] != '\0') {
            sum_items += parse_number(cols[5]);
            continue;
        }
    }

    fclose(f);
    if (!found_any) return 0.0f;
    return sum_items;
}

/* check if customer exists: if argument is exactly 10 digits, treat as mobile exact match;
   otherwise check name/mobile substring as before */
static int customer_exists(const char *name_or_mobile) {
    if (!name_or_mobile || !*name_or_mobile) return 0;
    /* exact mobile check */
    size_t len = strlen(name_or_mobile);
    int is_digits = 1;
    if (len == 10) {
        for (size_t i = 0; i < len; ++i) if (!isdigit((unsigned char)name_or_mobile[i])) { is_digits = 0; break; }
    } else is_digits = 0;

    FILE *f = fopen(CSV_PATH, "r");
    if (!f) return 0;
    char line[LINE_BUF];
    if (!fgets(line, sizeof(line), f)) { fclose(f); return 0; } /* skip header */

    while (fgets(line, sizeof(line), f)) {
        char copy[LINE_BUF];
        strncpy(copy, line, sizeof(copy)-1); copy[sizeof(copy)-1] = '\0';

        char *cols[12] = {0};
        int c = 0;
        char *tok = strtok(copy, ",");
        while (tok && c < 12) {
            size_t l = strlen(tok);
            while (l > 0 && (tok[l-1] == '\n' || tok[l-1] == '\r')) tok[--l] = '\0';
            cols[c++] = tok;
            tok = strtok(NULL, ",");
        }

        const char *cname = (c >= 3) ? cols[2] : (c >= 1 ? cols[0] : NULL);
        const char *cmobile = (c >= 4) ? cols[3] : (c >= 2 ? cols[1] : NULL);

        if (is_digits) {
            if (cmobile && strcmp(cmobile, name_or_mobile) == 0) { fclose(f); return 1; }
        } else {
            if (matches_customer_record(cname, cmobile, name_or_mobile)) { fclose(f); return 1; }
        }
    }

    fclose(f);
    return 0;
}

/* exact mobile existence helper */
static int mobile_exists_exact(const char *mobile) {
    if (!mobile || !*mobile) return 0;
    FILE *f = fopen(CSV_PATH, "r");
    if (!f) return 0;
    char line[LINE_BUF];
    if (!fgets(line, sizeof(line), f)) { fclose(f); return 0; } /* skip header */

    while (fgets(line, sizeof(line), f)) {
        char copy[LINE_BUF];
        strncpy(copy, line, sizeof(copy)-1); copy[sizeof(copy)-1] = '\0';
        char *cols[12] = {0};
        int c = 0;
        char *tok = strtok(copy, ",");
        while (tok && c < 12) { cols[c++] = tok; tok = strtok(NULL, ","); }
        const char *cmobile = (c >= 4) ? cols[3] : (c >= 2 ? cols[1] : NULL);
        if (cmobile && strcmp(cmobile, mobile) == 0) { fclose(f); return 1; }
    }
    fclose(f);
    return 0;
}

/* get first matching customer name & mobile */
static int get_customer_info(const char *name_or_mobile, char *name, size_t ns, char *mobile, size_t ms) {
    FILE *f = fopen(CSV_PATH, "r");
    if (!f) return 0;
    char line[LINE_BUF];
    if (!fgets(line, sizeof(line), f)) { fclose(f); return 0; }

    while (fgets(line, sizeof(line), f)) {
        char copy[LINE_BUF];
        strncpy(copy, line, sizeof(copy)-1); copy[sizeof(copy)-1] = '\0';
        char *cols[12] = {0};
        int c = 0;
        char *tok = strtok(copy, ",");
        while (tok && c < 12) {
            size_t l = strlen(tok);
            while (l > 0 && (tok[l-1] == '\n' || tok[l-1] == '\r')) tok[--l] = '\0';
            cols[c++] = tok;
            tok = strtok(NULL, ",");
        }
        const char *cname = (c >= 3) ? cols[2] : (c >= 1 ? cols[0] : NULL);
        const char *cmobile = (c >= 4) ? cols[3] : (c >= 2 ? cols[1] : NULL);
        if (!matches_customer_record(cname, cmobile, name_or_mobile)) continue;
        if (name && ns > 0 && cname) { strncpy(name, cname, ns-1); name[ns-1] = '\0'; }
        if (mobile && ms > 0 && cmobile) { strncpy(mobile, cmobile, ms-1); mobile[ms-1] = '\0'; }
        fclose(f);
        return 1;
    }
    fclose(f);
    return 0;
}

/* validate mobile: require 10 digits */
static int valid_mobile(const char *m) {
    if (!m) return 0;
    size_t len = strlen(m);
    if (len != 10) return 0;
    for (size_t i = 0; i < len; ++i) if (!isdigit((unsigned char)m[i])) return 0;
    return 1;
}

/* find unique customer names that use the given mobile number
 * returns number of unique names found (0..max)
 */
static int find_customers_by_mobile(const char *mobile, char names[][50], int max)
{
    if (!mobile || !*mobile || max <= 0) return 0;
    FILE *f = fopen(CSV_PATH, "r");
    if (!f) return 0;

    char line[LINE_BUF];
    /* skip header */
    if (!fgets(line, sizeof(line), f)) { fclose(f); return 0; }

    int found = 0;
    while (fgets(line, sizeof(line), f)) {
        char copy[LINE_BUF];
        strncpy(copy, line, sizeof(copy) - 1);
        copy[sizeof(copy)-1] = '\0';

        char *cols[12] = {0};
        int c = 0;
        char *tok = strtok(copy, ",");
        while (tok && c < 12) {
            size_t l = strlen(tok);
            while (l > 0 && (tok[l-1] == '\n' || tok[l-1] == '\r')) tok[--l] = '\0';
            cols[c++] = tok;
            tok = strtok(NULL, ",");
        }

        char *cname = (c >= 3) ? cols[2] : (c >= 1 ? cols[0] : NULL);
        char *cmobile = (c >= 4) ? cols[3] : (c >= 2 ? cols[1] : NULL);
        if (!cname || !cmobile) continue;

        if (strcmp(cmobile, mobile) == 0) {
            int dup = 0;
            for (int i = 0; i < found; ++i) {
                if (strcmp(names[i], cname) == 0) { dup = 1; break; }
            }
            if (!dup) {
                strncpy(names[found], cname, 49);
                names[found][49] = '\0';
                ++found;
                if (found >= max) break;
            }
        }
    }

    fclose(f);
    return found;
}

/* Add new customer bill: now supports unit selection and float qty,
   and writes new CSV schema (Date,Time,Customer,Mobile,Item,Qty,Unit,Price,Total) */
static void add_new_bill(void) {
    ensure_header();

    struct Customer cust;
    char buf[128];
    printf("\n--- Add New Customer Bill ---\n");

    do {
        read_line("Enter Customer Name: ", cust.name, sizeof(cust.name));
        if (cust.name[0] == '\0') printf("Customer name cannot be empty.\n");
    } while (cust.name[0] == '\0');

    /* prevent duplicate mobile: require unique mobile for a customer */
    do {
        read_line("Enter Mobile Number (10 digits): ", cust.mobile, sizeof(cust.mobile));
        if (!valid_mobile(cust.mobile)) {
            printf("Mobile must be exactly 10 digits.\n");
            continue;
        }
        if (mobile_exists_exact(cust.mobile) ) {
            printf("Mobile %s already exists in records. Use Update option to add items.\n", cust.mobile);
            cust.mobile[0] = '\0';
            /* allow user to abort by entering blank? We'll force them to choose another or cancel */
            read_line("Enter another mobile or press Enter to abort: ", buf, sizeof(buf));
            if (buf[0] == '\0') return;
            strncpy(cust.mobile, buf, sizeof(cust.mobile)-1); cust.mobile[sizeof(cust.mobile)-1] = '\0';
            if (!valid_mobile(cust.mobile)) { cust.mobile[0] = '\0'; }
        }
    } while (!valid_mobile(cust.mobile));

    struct Item items[MAX_ITEMS];
    float grandTotal = 0.0f;
    int i = 0;

    printf("\n--- Add Items (type 'n' to finish) ---\n");
    //printf("(Numeric inputs accept decimal or hexadecimal with 0x prefix)\n");

    while (i < MAX_ITEMS) {
        read_line("\nItem Name: ", items[i].name, sizeof(items[i].name));
        if (strlen(items[i].name) == 1 && (items[i].name[0] == 'n' || items[i].name[0] == 'N')) break;
        if (items[i].name[0] == '\0') { printf("Item name cannot be empty.\n"); continue; }

        /* unit selection */
        printf("Select Unit:\n");
        printf("1. Grams (g)\n");
        printf("2. Kilograms (kg)\n");
        printf("3. Milliliters (ml)\n");
        printf("4. Liters (l)\n");
        printf("5. Pieces (pcs)\n");
        read_line("Enter choice (1-5): ", buf, sizeof(buf));
        int unit_choice = parse_int(buf);
        switch (unit_choice) {
            case 1: strncpy(items[i].unit, "g", sizeof(items[i].unit)); break;
            case 2: strncpy(items[i].unit, "kg", sizeof(items[i].unit)); break;
            case 3: strncpy(items[i].unit, "ml", sizeof(items[i].unit)); break;
            case 4: strncpy(items[i].unit, "l", sizeof(items[i].unit)); break;
            case 5: strncpy(items[i].unit, "pcs", sizeof(items[i].unit)); break;
            default: strncpy(items[i].unit, "pcs", sizeof(items[i].unit)); break;
        }
        items[i].unit[sizeof(items[i].unit)-1] = '\0';

        /* quantity as float */
        do {
            read_line("Quantity : ", buf, sizeof(buf));
            items[i].quantity = parse_number(buf);
            if (items[i].quantity <= 0.0f) printf("Quantity must be > 0.\n");
        } while (items[i].quantity <= 0.0f);

        do {
            read_line("Price per unit : ", buf, sizeof(buf));
            items[i].price = parse_number(buf);
            if (items[i].price <= 0.0f) printf("Price must be > 0.\n");
        } while (items[i].price <= 0.0f);

        items[i].total = items[i].quantity * items[i].price;
        grandTotal += items[i].total;
        printf("Item Total: %.2f\n", items[i].total);
        i++;
    }

    if (i == 0) {
        printf("No items added. Aborting.\n");
        return;
    }

    /* capture timestamp */
    char datestr[32] = "", timestr[32] = "";
    time_t now = time(NULL);
    struct tm *lt = localtime(&now);
    if (lt) {
        strftime(datestr, sizeof(datestr), "%Y-%m-%d", lt);
        strftime(timestr, sizeof(timestr), "%H:%M:%S", lt);
    }

    FILE *f = fopen(CSV_PATH, "a");
    if (!f) {
        fprintf(stderr, "Error opening file.\n");
        return;
    }


    for (int j = 0; j < i; ++j) {
        fprintf(f, "%s,%s,%s,%s,%s,%.3f,%s,%.2f,%.2f\n",
                datestr, timestr, cust.name, cust.mobile,
                items[j].name, items[j].quantity, items[j].unit,
                items[j].price, items[j].total);
    }

    fclose(f);

    printf("\n\t--- Summary ---\n");
    printf("Customer: %s | Mobile: %s\n", cust.name, cust.mobile);
    if (datestr[0] && timestr[0]) printf("Created On: %s %s\n", datestr, timestr);
    printf("Total Items: %d | Grand Total: %.2f\n", i, grandTotal);
}

/* Add items to existing customer: supports unit and float qty and writes new CSV schema */
static void update_customer_bill(void) {
    ensure_header();

    char search[64];
    read_line("\nEnter Customer Name or Mobile: ", search, sizeof(search));
    if (search[0] == '\0') { printf("Empty search. Aborted.\n"); return; }

    if (!customer_exists(search)) {
        printf("Customer not found.\n");
        return;
    }

    char cname[50] = "", mob[32] = "";
    if (!get_customer_info(search, cname, sizeof(cname), mob, sizeof(mob))) {
        printf("Failed to retrieve customer info.\n");
        return;
    }

    float existing = compute_customer_total(search);
    printf("Previous total: %.2f\n", existing);

    struct Item items[MAX_ITEMS];
    float added = 0;
    int i = 0;
    char buf[64];

    //printf("(Numeric inputs accept decimal or hexadecimal with 0x prefix)\n");

    while (i < MAX_ITEMS) {
        read_line("\nItem Name (or 'n' to finish): ", items[i].name, sizeof(items[i].name));
        if (strlen(items[i].name) == 1 && (items[i].name[0] == 'n' || items[i].name[0] == 'N')) break;
        if (items[i].name[0] == '\0') { printf("Item name cannot be empty.\n"); continue; }

        printf("Select Unit:\n");
        printf("1. Grams (g)\n");
        printf("2. Kilograms (kg)\n");
        printf("3. Milliliters (ml)\n");
        printf("4. Liters (l)\n");
        printf("5. Pieces (pcs)\n");
        read_line("Enter choice (1-5): ", buf, sizeof(buf));
        int unit_choice = parse_int(buf);
        switch (unit_choice) {
            case 1: strncpy(items[i].unit, "g", sizeof(items[i].unit)); break;
            case 2: strncpy(items[i].unit, "kg", sizeof(items[i].unit)); break;
            case 3: strncpy(items[i].unit, "ml", sizeof(items[i].unit)); break;
            case 4: strncpy(items[i].unit, "l", sizeof(items[i].unit)); break;
            case 5: strncpy(items[i].unit, "pcs", sizeof(items[i].unit)); break;
            default: strncpy(items[i].unit, "pcs", sizeof(items[i].unit)); break;
        }
        items[i].unit[sizeof(items[i].unit)-1] = '\0';

        do {
            read_line("Quantity : ", buf, sizeof(buf));
            items[i].quantity = parse_number(buf);
            if (items[i].quantity <= 0.0f) printf("Quantity must be > 0.\n");
        } while (items[i].quantity <= 0.0f);

        
        do {
            read_line("Price : ", buf, sizeof(buf));
            items[i].price = parse_number(buf);
            if (items[i].price <= 0.0f) printf("Price must be > 0.\n");
        } while (items[i].price <= 0.0f);

        items[i].total = items[i].quantity * items[i].price;
        added += items[i].total;
        i++;
    }

    if (i == 0) {
        printf("No items added. Aborting.\n");
        return;
    }

    /* timestamp */
    char datestr[32] = "", timestr[32] = "";
    time_t now = time(NULL);
    struct tm *lt = localtime(&now);
    if (lt) {
        strftime(datestr, sizeof(datestr), "%Y-%m-%d", lt);
        strftime(timestr, sizeof(timestr), "%H:%M:%S", lt);
    }

    FILE *f = fopen(CSV_PATH, "a");
    if (!f) { fprintf(stderr, "Error opening file.\n"); return; }

    for (int j = 0; j < i; ++j) {
        fprintf(f, "%s,%s,%s,%s,%s,%.3f,%s,%.2f,%.2f\n",
                datestr, timestr, cname, mob,
                items[j].name, items[j].quantity, items[j].unit,
                items[j].price, items[j].total);
    }

    fclose(f);

    printf("Added: %.2f | New Total: %.2f\n", added, existing + added);
}

/* View customer bills - updated parser to accept new schema and float qty with unit */
static void view_customer_bills(void) {
    ensure_header();
    char search[64];
    read_line("\nEnter Customer Name or Mobile: ", search, sizeof(search));
    if (search[0] == '\0') { puts("Empty search. Aborted."); return; }

    FILE *f = fopen(CSV_PATH, "r");
    if (!f) { perror("fopen"); return; }

    char line[LINE_BUF];
    if (!fgets(line, sizeof(line), f)) { fclose(f); puts("No records."); return; } /* skip header */

    struct RecItem { char name[64]; float qty; char unit[8]; float price; float total; char date[16]; char time[16]; };
    struct RecItem items[MAX_ITEMS];
    int item_count = 0;
    char cust_name[64] = "", cust_mobile[32] = "";
    float summed_total = 0.0f;
    int found = 0;

    while (fgets(line, sizeof(line), f)) {
        char copy[LINE_BUF];
        strncpy(copy, line, sizeof(copy)-1); copy[sizeof(copy)-1] = '\0';

        char *cols[12] = {0};
        int c = 0;
        char *tok = strtok(copy, ",");
        while (tok && c < 12) {
            size_t l = strlen(tok);
            while (l > 0 && (tok[l-1] == '\n' || tok[l-1] == '\r')) tok[--l] = '\0';
            cols[c++] = tok;
            tok = strtok(NULL, ",");
        }

        /* two possible schemas:
           new: cols[0]=Date,1=Time,2=Customer,3=Mobile,4=Item,5=Qty,6=Unit,7=Price,8=Total
           old: cols[0]=Customer,1=Mobi
