#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <unistd.h>
#include <stdarg.h>
#include <libpq-fe.h>

#define MAX_ITEMS 1000
#define MAX_NAME_LEN 64
#define MAX_DESC_LEN 256
#define MAX_UNIT_LEN 32
#define MAX_LOCATION_LEN 64
#define MAX_REQUESTS 500
#define DATA_FILE "equipment.dat"
#define REQUEST_FILE "requests.dat"
#define LOG_FILE "equipment.log"
#define DB_CONFIG_FILE "db_config.conf"
#define HASH_SIZE 1009
#define MAX_QUERY_LEN 2048

// ANSI Color codes for military theming
#define RESET   "\033[0m"
#define BOLD    "\033[1m"
#define RED     "\033[1;31m"
#define GREEN   "\033[1;32m"
#define YELLOW  "\033[1;33m"
#define BLUE    "\033[1;34m"
#define MAGENTA "\033[1;35m"
#define CYAN    "\033[1;36m"
#define WHITE   "\033[1;37m"
#define BG_RED  "\033[41m"
#define BG_GREEN "\033[42m"
#define BG_BLUE "\033[44m"

// Database connection structure
typedef struct {
    char host[64];
    char port[8];
    char dbname[64];
    char user[64];
    char password[128];
} DBConfig;

// Equipment item structure
typedef struct {
    int id;
    char name[MAX_NAME_LEN];
    char description[MAX_DESC_LEN];
    int quantity;
    int min_threshold;
    char unit[MAX_UNIT_LEN];
    char location[MAX_LOCATION_LEN];
    time_t last_updated;
    int classification;
    char checksum[16];
} Equipment;

// Supply request structure
typedef struct {
    int req_id;
    int equipment_id;
    int requested_qty;
    char requesting_unit[MAX_UNIT_LEN];
    time_t request_time;
    int status;
    int priority;
} SupplyRequest;

// Hash table node for fast lookups
typedef struct HashNode {
    Equipment* equipment;
    struct HashNode* next;
} HashNode;

// Enums for better code readability
typedef enum {
    STATUS_OK = 0,
    STATUS_WATCH = 1,
    STATUS_LOW = 2
} StockStatus;

typedef enum {
    CLASS_UNCLASS = 0,
    CLASS_RESTRICTED = 1,
    CLASS_CONFIDENTIAL = 2,
    CLASS_SECRET = 3
} Classification;

typedef enum {
    REQ_PENDING = 0,
    REQ_APPROVED = 1,
    REQ_FULFILLED = 2,
    REQ_DENIED = 3
} RequestStatus;

typedef enum {
    PRIORITY_LOW = 1,
    PRIORITY_NORMAL = 2,
    PRIORITY_HIGH = 3,
    PRIORITY_CRITICAL = 4
} Priority;

// Global data structures
Equipment inventory[MAX_ITEMS];
SupplyRequest requests[MAX_REQUESTS];
HashNode* hash_table[HASH_SIZE];
int item_count = 0;
int request_count = 0;
int next_item_id = 1;
int next_request_id = 1;

// Database globals
PGconn* db_conn = NULL;
DBConfig db_config;
int use_database = 0;

// Lookup tables
const char* CLASS_NAMES[] = {"UNCLASSIFIED", "RESTRICTED", "CONFIDENTIAL", "SECRET"};
const char* STATUS_NAMES[] = {"PENDING", "APPROVED", "FULFILLED", "DENIED"};
const char* PRIORITY_NAMES[] = {"", "LOW", "NORMAL", "HIGH", "CRITICAL"};
const char* STOCK_STATUS_NAMES[] = {"OK", "WATCH", "LOW"};

// Function prototypes
void hash_insert(Equipment* equipment);
Equipment* hash_find(const char* name);
Equipment* find_by_id(int id);
void log_action(const char* action);
void clear_screen(void);
void display_banner(void);
void wait_for_enter(void);

// ============================================================================
// ENHANCED TERMINAL INTERFACE FUNCTIONS
// ============================================================================

void clear_screen(void) {
    printf("\033[2J\033[H"); // Clear screen and move cursor to top
}

void display_banner(void) {
    clear_screen();
    printf(BOLD GREEN);
    printf("╔══════════════════════════════════════════════════════════════════════════════╗\n");
    printf("║                      🛡️  TACTICAL SUPPLY MANAGEMENT SYSTEM 🛡️               ║\n");
    printf("║                           MILITARY EQUIPMENT TRACKER                        ║\n");
    printf("╠══════════════════════════════════════════════════════════════════════════════╣\n");
    if (use_database) {
        printf("║  " CYAN "🗄️  DATABASE MODE" GREEN " - Real-time PostgreSQL Operations                    ║\n");
    } else {
        printf("║  " YELLOW "📁 OFFLINE MODE" GREEN " - Local File Storage                                 ║\n");
    }
    printf("║  System Status:  " GREEN "✅ OPERATIONAL" GREEN "                                              ║\n");
    printf("║  Access Level:   " YELLOW "🔒 AUTHORIZED PERSONNEL ONLY" GREEN "                             ║\n");
    printf("║  Equipment Count: " WHITE "%d items" GREEN " | Supply Requests: " WHITE "%d pending" GREEN "                   ║\n", 
           item_count, request_count);
    printf("╚══════════════════════════════════════════════════════════════════════════════╝" RESET "\n\n");
}

void display_classification_banner(Classification level) {
    const char* colors[] = {WHITE, YELLOW, CYAN, RED};
    const char* bg_colors[] = {"", BG_GREEN, BG_BLUE, BG_RED};
    
    printf("\n%s%s", colors[level], bg_colors[level] && level > 0 ? bg_colors[level] : "");
    printf("╔══════════════════════════════════════════════════════════════════════════════╗\n");
    printf("║                            %s%-25s                            ║\n", CLASS_NAMES[level], "");
    printf("╚══════════════════════════════════════════════════════════════════════════════╝" RESET "\n");
}

void wait_for_enter(void) {
    printf(BOLD CYAN "\n[Press ENTER to continue...]" RESET);
    while (getchar() != '\n');
}

void display_command_prompt(void) {
    printf(BOLD YELLOW "TACTICAL-SUPPLY" RESET WHITE "$ " RESET);
}

// ============================================================================
// DATABASE FUNCTIONS (Same as before)
// ============================================================================

int load_db_config(void) {
    FILE* config = fopen(DB_CONFIG_FILE, "r");
    if (!config) {
        printf(YELLOW "⚠️  Warning: Database config file not found. Using offline mode.\n" RESET);
        return 0;
    }
    
    char line[256];
    while (fgets(line, sizeof(line), config)) {
        line[strcspn(line, "\n")] = 0;
        
        if (strncmp(line, "host=", 5) == 0) {
            strncpy(db_config.host, line + 5, sizeof(db_config.host) - 1);
        } else if (strncmp(line, "port=", 5) == 0) {
            strncpy(db_config.port, line + 5, sizeof(db_config.port) - 1);
        } else if (strncmp(line, "dbname=", 7) == 0) {
            strncpy(db_config.dbname, line + 7, sizeof(db_config.dbname) - 1);
        } else if (strncmp(line, "user=", 5) == 0) {
            strncpy(db_config.user, line + 5, sizeof(db_config.user) - 1);
        } else if (strncmp(line, "password=", 9) == 0) {
            strncpy(db_config.password, line + 9, sizeof(db_config.password) - 1);
        }
    }
    
    fclose(config);
    return 1;
}

int connect_database(void) {
    if (!load_db_config()) {
        return 0;
    }
    
    char conninfo[512];
    snprintf(conninfo, sizeof(conninfo),
             "host=%s port=%s dbname=%s user=%s password=%s",
             db_config.host, db_config.port, db_config.dbname,
             db_config.user, db_config.password);
    
    db_conn = PQconnectdb(conninfo);
    
    if (PQstatus(db_conn) != CONNECTION_OK) {
        printf(YELLOW "⚠️  Warning: Database connection failed: %s\n" RESET, PQerrorMessage(db_conn));
        printf(CYAN "🔄 Falling back to file-based storage for offline operation.\n" RESET);
        PQfinish(db_conn);
        db_conn = NULL;
        return 0;
    }
    
    printf(GREEN "✅ Connected to PostgreSQL database successfully.\n" RESET);
    return 1;
}

PGresult* execute_query(const char* query, int expected_result) {
    if (!db_conn) return NULL;
    
    PGresult* res = PQexec(db_conn, query);
    ExecStatusType status = PQresultStatus(res);
    
    if (status != expected_result) {
        printf(RED "❌ Database query failed: %s\n" RESET, PQerrorMessage(db_conn));
        PQclear(res);
        return NULL;
    }
    
    return res;
}

void load_equipment_from_db(void) {
    const char* query = "SELECT id, name, description, quantity, min_threshold, "
                       "unit, location, classification, checksum, "
                       "EXTRACT(EPOCH FROM last_updated) FROM equipment ORDER BY id";
    
    PGresult* res = execute_query(query, PGRES_TUPLES_OK);
    if (!res) return;
    
    item_count = PQntuples(res);
    if (item_count > MAX_ITEMS) {
        printf(YELLOW "⚠️  Warning: Database contains more items than maximum. Truncating to %d.\n" RESET, MAX_ITEMS);
        item_count = MAX_ITEMS;
    }
    
    for (int i = 0; i < item_count; i++) {
        Equipment* item = &inventory[i];
        
        item->id = atoi(PQgetvalue(res, i, 0));
        strncpy(item->name, PQgetvalue(res, i, 1), MAX_NAME_LEN - 1);
        strncpy(item->description, PQgetvalue(res, i, 2), MAX_DESC_LEN - 1);
        item->quantity = atoi(PQgetvalue(res, i, 3));
        item->min_threshold = atoi(PQgetvalue(res, i, 4));
        strncpy(item->unit, PQgetvalue(res, i, 5), MAX_UNIT_LEN - 1);
        strncpy(item->location, PQgetvalue(res, i, 6), MAX_LOCATION_LEN - 1);
        item->classification = atoi(PQgetvalue(res, i, 7));
        strncpy(item->checksum, PQgetvalue(res, i, 8), 15);
        item->last_updated = (time_t)atol(PQgetvalue(res, i, 9));
        
        hash_insert(item);
        
        if (item->id >= next_item_id) {
            next_item_id = item->id + 1;
        }
    }
    
    PQclear(res);
    printf(GREEN "📊 Loaded %d equipment items from database.\n" RESET, item_count);
}

void load_requests_from_db(void) {
    const char* query = "SELECT req_id, equipment_id, requested_qty, requesting_unit, "
                       "EXTRACT(EPOCH FROM request_time), status, priority "
                       "FROM supply_requests ORDER BY req_id";
    
    PGresult* res = execute_query(query, PGRES_TUPLES_OK);
    if (!res) return;
    
    request_count = PQntuples(res);
    if (request_count > MAX_REQUESTS) {
        printf(YELLOW "⚠️  Warning: Database contains more requests than maximum. Truncating to %d.\n" RESET, MAX_REQUESTS);
        request_count = MAX_REQUESTS;
    }
    
    for (int i = 0; i < request_count; i++) {
        SupplyRequest* req = &requests[i];
        
        req->req_id = atoi(PQgetvalue(res, i, 0));
        req->equipment_id = atoi(PQgetvalue(res, i, 1));
        req->requested_qty = atoi(PQgetvalue(res, i, 2));
        strncpy(req->requesting_unit, PQgetvalue(res, i, 3), MAX_UNIT_LEN - 1);
        req->request_time = (time_t)atol(PQgetvalue(res, i, 4));
        req->status = atoi(PQgetvalue(res, i, 5));
        req->priority = atoi(PQgetvalue(res, i, 6));
        
        if (req->req_id >= next_request_id) {
            next_request_id = req->req_id + 1;
        }
    }
    
    PQclear(res);
    printf(GREEN "📋 Loaded %d supply requests from database.\n" RESET, request_count);
}

int add_equipment_to_db(const Equipment* item) {
    if (!db_conn) return 0;
    
    char query[MAX_QUERY_LEN];
    snprintf(query, sizeof(query),
             "INSERT INTO equipment (name, description, quantity, min_threshold, "
             "unit, location, classification, checksum) VALUES "
             "('%s', '%s', %d, %d, '%s', '%s', %d, '%s') RETURNING id",
             item->name, item->description, item->quantity, item->min_threshold,
             item->unit, item->location, item->classification, item->checksum);
    
    PGresult* res = execute_query(query, PGRES_TUPLES_OK);
    if (!res) return 0;
    
    int new_id = atoi(PQgetvalue(res, 0, 0));
    PQclear(res);
    
    return new_id;
}

int update_equipment_in_db(const Equipment* item) {
    if (!db_conn) return 0;
    
    char query[MAX_QUERY_LEN];
    snprintf(query, sizeof(query),
             "UPDATE equipment SET quantity=%d, checksum='%s', "
             "last_updated=CURRENT_TIMESTAMP WHERE id=%d",
             item->quantity, item->checksum, item->id);
    
    PGresult* res = execute_query(query, PGRES_COMMAND_OK);
    if (!res) return 0;
    
    PQclear(res);
    return 1;
}

int add_request_to_db(const SupplyRequest* req) {
    if (!db_conn) return 0;
    
    char query[MAX_QUERY_LEN];
    snprintf(query, sizeof(query),
             "INSERT INTO supply_requests (equipment_id, requested_qty, "
             "requesting_unit, status, priority) VALUES "
             "(%d, %d, '%s', %d, %d) RETURNING req_id",
             req->equipment_id, req->requested_qty, req->requesting_unit,
             req->status, req->priority);
    
    PGresult* res = execute_query(query, PGRES_TUPLES_OK);
    if (!res) return 0;
    
    int new_id = atoi(PQgetvalue(res, 0, 0));
    PQclear(res);
    
    return new_id;
}

void log_to_database(const char* action) {
    if (!db_conn) return;
    
    char query[MAX_QUERY_LEN];
    snprintf(query, sizeof(query),
             "INSERT INTO audit_log (action, user_info) VALUES ('%s', 'system')",
             action);
    
    PGresult* res = execute_query(query, PGRES_COMMAND_OK);
    if (res) PQclear(res);
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

unsigned int hash_function(const char* str) {
    unsigned int hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash % HASH_SIZE;
}

void hash_insert(Equipment* equipment) {
    unsigned int index = hash_function(equipment->name);
    HashNode* new_node = malloc(sizeof(HashNode));
    new_node->equipment = equipment;
    new_node->next = hash_table[index];
    hash_table[index] = new_node;
}

Equipment* hash_find(const char* name) {
    unsigned int index = hash_function(name);
    HashNode* current = hash_table[index];
    
    while (current) {
        if (strcasestr(current->equipment->name, name)) {
            return current->equipment;
        }
        current = current->next;
    }
    return NULL;
}

Equipment* find_by_id(int id) {
    for (int i = 0; i < item_count; i++) {
        if (inventory[i].id == id) {
            return &inventory[i];
        }
    }
    return NULL;
}

StockStatus get_stock_status(const Equipment* item) {
    if (item->quantity <= item->min_threshold) {
        return STATUS_LOW;
    } else if (item->quantity <= (int)(item->min_threshold * 1.5)) {
        return STATUS_WATCH;
    }
    return STATUS_OK;
}

void get_string_input(const char* prompt, char* buffer, size_t buffer_size) {
    printf(CYAN "%s" RESET, prompt);
    if (fgets(buffer, buffer_size, stdin)) {
        buffer[strcspn(buffer, "\n")] = 0;
    }
}

int get_int_input(const char* prompt, int min_val, int max_val) {
    int value;
    do {
        printf(CYAN "%s" RESET, prompt);
        if (scanf("%d", &value) != 1) {
            printf(RED "❌ Invalid input. Please enter a number.\n" RESET);
            while (getchar() != '\n');
            continue;
        }
        while (getchar() != '\n');
        
        if (value < min_val || value > max_val) {
            printf(YELLOW "⚠️  Value must be between %d and %d.\n" RESET, min_val, max_val);
        }
    } while (value < min_val || value > max_val);
    
    return value;
}

void log_action(const char* action) {
    FILE* log = fopen(LOG_FILE, "a");
    if (log) {
        time_t now = time(NULL);
        char* time_str = ctime(&now);
        time_str[strlen(time_str) - 1] = 0;
        fprintf(log, "[%s] %s\n", time_str, action);
        fclose(log);
    }
    
    if (use_database) {
        log_to_database(action);
    }
}

int calculate_checksum(const Equipment* item) {
    int sum = item->id + item->quantity + item->min_threshold;
    for (int i = 0; item->name[i]; i++) {
        sum += item->name[i];
    }
    return sum % 10000;
}

void display_equipment_details(const Equipment* item) {
    StockStatus status = get_stock_status(item);
    
    display_classification_banner(item->classification);
    
    printf(BOLD WHITE "📦 EQUIPMENT DETAILS\n" RESET);
    printf("════════════════════════════════════════════════════════════════════════════════\n");
    printf(CYAN "ID: " WHITE "%d\n" RESET, item->id);
    printf(CYAN "Name: " WHITE "%s\n" RESET, item->name);
    printf(CYAN "Description: " WHITE "%s\n" RESET, item->description);
    printf(CYAN "Quantity: " WHITE "%d %s\n" RESET, item->quantity, item->unit);
    printf(CYAN "Location: " WHITE "%s\n" RESET, item->location);
    printf(CYAN "Min Threshold: " WHITE "%d\n" RESET, item->min_threshold);
    
    switch (status) {
        case STATUS_LOW:
            printf(BOLD RED "🚨 STATUS: *** LOW STOCK - RESUPPLY REQUIRED ***\n" RESET);
            break;
        case STATUS_WATCH:
            printf(BOLD YELLOW "⚠️  STATUS: CAUTION - Monitor stock levels\n" RESET);
            break;
        case STATUS_OK:
            printf(BOLD GREEN "✅ STATUS: ADEQUATE\n" RESET);
            break;
    }
    
    printf(CYAN "Last Updated: " WHITE "%s" RESET, ctime(&item->last_updated));
    printf(CYAN "Checksum: " WHITE "%s\n" RESET, item->checksum);
    printf("════════════════════════════════════════════════════════════════════════════════\n");
}

void display_equipment_table_header(void) {
    printf(BOLD WHITE);
    printf("┌──────┬──────────────────────┬──────────┬────────┬─────────────────┬────────────┐\n");
    printf("│ %-4s │ %-20s │ %-8s │ %-6s │ %-15s │ %-10s │\n", 
           "ID", "NAME", "QTY", "UNIT", "LOCATION", "STATUS");
    printf("├──────┼──────────────────────┼──────────┼────────┼─────────────────┼────────────┤\n");
    printf(RESET);
}

void display_equipment_row(const Equipment* item) {
    StockStatus status = get_stock_status(item);
    const char* status_color = (status == STATUS_LOW) ? RED : 
                              (status == STATUS_WATCH) ? YELLOW : GREEN;
    
    printf("│ %-4d │ %-20s │ %s%-8d%s │ %-6s │ %-15s │ %s%-10s%s │\n",
           item->id, item->name, WHITE, item->quantity, RESET,
           item->unit, item->location, status_color, STOCK_STATUS_NAMES[status], RESET);
}

void display_equipment_table_footer(int count) {
    printf("└──────┴──────────────────────┴──────────┴────────┴─────────────────┴────────────┘\n");
    printf(BOLD CYAN "Total Equipment Items: %d\n" RESET, count);
}

// ============================================================================
// DATA PERSISTENCE
// ============================================================================

void load_data(void) {
    if (use_database) {
        load_equipment_from_db();
        load_requests_from_db();
    } else {
        FILE* file = fopen(DATA_FILE, "rb");
        if (file) {
            fread(&item_count, sizeof(int), 1, file);
            fread(&next_item_id, sizeof(int), 1, file);
            fread(inventory, sizeof(Equipment), item_count, file);
            fclose(file);
            
            for (int i = 0; i < item_count; i++) {
                hash_insert(&inventory[i]);
            }
            printf(GREEN "📁 Loaded %d equipment items from local files.\n" RESET, item_count);
        }
        
        file = fopen(REQUEST_FILE, "rb");
        if (file) {
            fread(&request_count, sizeof(int), 1, file);
            fread(&next_request_id, sizeof(int), 1, file);
            fread(requests, sizeof(SupplyRequest), request_count, file);
            fclose(file);
            printf(GREEN "📋 Loaded %d supply requests from local files.\n" RESET, request_count);
        }
    }
}

void save_data(void) {
    if (!use_database) {
        FILE* file = fopen(DATA_FILE, "wb");
        if (file) {
            fwrite(&item_count, sizeof(int), 1, file);
            fwrite(&next_item_id, sizeof(int), 1, file);
            fwrite(inventory, sizeof(Equipment), item_count, file);
            fclose(file);
        }
        
        file = fopen(REQUEST_FILE, "wb");
        if (file) {
            fwrite(&request_count, sizeof(int), 1, file);
            fwrite(&next_request_id, sizeof(int), 1, file);
            fwrite(requests, sizeof(SupplyRequest), request_count, file);
            fclose(file);
        }
        printf(GREEN "💾 Data saved to local files.\n" RESET);
    }
}

// ============================================================================
// ENHANCED CORE FUNCTIONALITY
// ============================================================================

void display_menu(void) {
    display_banner();
    
    printf(BOLD WHITE "🎯 MISSION COMMANDS:\n" RESET);
    printf("════════════════════════════════════════════════════════════════════════════════\n");
    printf(GREEN "  [1]" WHITE " 📦 Add Equipment          " GREEN "[2]" WHITE " 🔍 Check Inventory\n");
    printf(GREEN "  [3]" WHITE " 📋 List All Equipment     " GREEN "[4]" WHITE " 📊 Update Quantity\n");
    printf(GREEN "  [5]" WHITE " 📝 Request Supply         " GREEN "[6]" WHITE " 📑 Check Requests\n");
    printf(GREEN "  [7]" WHITE " 🚨 Low Stock Alert        " GREEN "[8]" WHITE " 📄 Export Report\n");
    printf(GREEN "  [9]" WHITE " 🚪 Exit System\n" RESET);
    printf("════════════════════════════════════════════════════════════════════════════════\n");
    display_command_prompt();
}

void add_equipment(void) {
    display_banner();
    printf(BOLD YELLOW "📦 ADD NEW EQUIPMENT\n" RESET);
    printf("════════════════════════════════════════════════════════════════════════════════\n");
    
    if (item_count >= MAX_ITEMS) {
        printf(RED "❌ ERROR: Maximum equipment limit reached.\n" RESET);
        wait_for_enter();
        return;
    }
    
    Equipment* item = &inventory[item_count];
    item->id = next_item_id++;
    
    get_string_input("Equipment Name: ", item->name, MAX_NAME_LEN);
    get_string_input("Description: ", item->description, MAX_DESC_LEN);
    
    item->quantity = get_int_input("Initial Quantity: ", 0, 999999);
    item->min_threshold = get_int_input("Minimum Threshold: ", 0, 999999);
    
    get_string_input("Unit (ea, box, case, etc.): ", item->unit, MAX_UNIT_LEN);
    get_string_input("Location: ", item->location, MAX_LOCATION_LEN);
    
    item->classification = get_int_input(
        "Classification (0=Unclass, 1=Restricted, 2=Confidential, 3=Secret): ", 
        0, 3);
    
    item->last_updated = time(NULL);
    sprintf(item->checksum, "%04d", calculate_checksum(item));
    
    if (use_database) {
        int db_id = add_equipment_to_db(item);
        if (db_id > 0) {
            item->id = db_id;
            if (db_id >= next_item_id) {
                next_item_id = db_id + 1;
            }
        }
    }
    
    hash_insert(item);
    item_count++;
    
    char log_msg[256];
    sprintf(log_msg, "Added equipment: %s (ID: %d)", item->name, item->id);
    log_action(log_msg);
    
    printf(GREEN "\n✅ Equipment added successfully. ID: %d\n" RESET, item->id);
    wait_for_enter();
}

void check_inventory(const char* item_name) {
    display_banner();
    printf(BOLD YELLOW "🔍 INVENTORY SEARCH RESULTS\n" RESET);
    printf("════════════════════════════════════════════════════════════════════════════════\n");
    
    Equipment* item = hash_find(item_name);
    if (item) {
        display_equipment_details(item);
    } else {
        int found = 0;
        for (int i = 0; i < item_count; i++) {
            if (strcasestr(inventory[i].name, item_name)) {
                display_equipment_details(&inventory[i]);
                found = 1;
                printf("\n");
            }
        }
        if (!found) {
            printf(RED "❌ No equipment found matching '%s'\n" RESET, item_name);
        }
    }
    wait_for_enter();
}

void list_all_equipment(void) {
    display_banner();
    printf(BOLD YELLOW "📋 COMPLETE INVENTORY LISTING\n" RESET);
    printf("════════════════════════════════════════════════════════════════════════════════\n");
    
    if (item_count == 0) {
        printf(YELLOW "⚠️  No equipment in inventory.\n" RESET);
        wait_for_enter();
        return;
    }
    
    display_equipment_table_header();
    
    for (int i = 0; i < item_count; i++) {
        display_equipment_row(&inventory[i]);
    }
    
    display_equipment_table_footer(item_count);
    wait_for_enter();
}

void update_quantity(void) {
    display_banner();
    printf(BOLD YELLOW "📊 UPDATE EQUIPMENT QUANTITY\n" RESET);
    printf("════════════════════════════════════════════════════════════════════════════════\n");
    
    int id = get_int_input("Equipment ID: ", 1, 999999);
    Equipment* item = find_by_id(id);
    
    if (!item) {
        printf(RED "❌ Equipment ID not found.\n" RESET);
        wait_for_enter();
        return;
    }
    
    printf(CYAN "Current quantity: " WHITE "%d %s\n" RESET, item->quantity, item->unit);
    int old_qty = item->quantity;
    item->quantity = get_int_input("New quantity: ", 0, 999999);
    
    item->last_updated = time(NULL);
    sprintf(item->checksum, "%04d", calculate_checksum(item));
    
    if (use_database) {
        update_equipment_in_db(item);
    }
    
    char log_msg[256];
    sprintf(log_msg, "Updated %s quantity: %d -> %d", item->name, old_qty, item->quantity);
    log_action(log_msg);
    
    printf(GREEN "\n✅ Quantity updated successfully.\n" RESET);
    wait_for_enter();
}

void request_supply(void) {
    display_banner();
    printf(BOLD YELLOW "📝 CREATE SUPPLY REQUEST\n" RESET);
    printf("════════════════════════════════════════════════════════════════════════════════\n");
    
    if (request_count >= MAX_REQUESTS) {
        printf(RED "❌ ERROR: Maximum request limit reached.\n" RESET);
        wait_for_enter();
        return;
    }
    
    SupplyRequest* req = &requests[request_count];
    req->req_id = next_request_id++;
    
    req->equipment_id = get_int_input("Equipment ID: ", 1, 999999);
    
    Equipment* item = find_by_id(req->equipment_id);
    if (!item) {
        printf(RED "❌ Equipment ID not found.\n" RESET);
        wait_for_enter();
        return;
    }
    
    printf(GREEN "Requesting: " WHITE "%s\n" RESET, item->name);
    req->requested_qty = get_int_input("Quantity needed: ", 1, 999999);
    
    get_string_input("Requesting unit: ", req->requesting_unit, MAX_UNIT_LEN);
    
    req->priority = get_int_input(
        "Priority (1=Low, 2=Normal, 3=High, 4=Critical): ", 1, 4);
    
    req->request_time = time(NULL);
    req->status = REQ_PENDING;
    
    if (use_database) {
        int db_id = add_request_to_db(req);
        if (db_id > 0) {
            req->req_id = db_id;
            if (db_id >= next_request_id) {
                next_request_id = db_id + 1;
            }
        }
    }
    
    request_count++;
    
    char log_msg[256];
    sprintf(log_msg, "Supply request created: REQ-%d for equipment ID %d", 
            req->req_id, req->equipment_id);
    log_action(log_msg);
    
    printf(GREEN "\n✅ Supply request submitted. Request ID: REQ-%d\n" RESET, req->req_id);
    wait_for_enter();
}

void check_requests(void) {
    display_banner();
    printf(BOLD YELLOW "📑 SUPPLY REQUEST STATUS\n" RESET);
    printf("════════════════════════════════════════════════════════════════════════════════\n");
    
    if (request_count == 0) {
        printf(YELLOW "⚠️  No supply requests on file.\n" RESET);
        wait_for_enter();
        return;
    }
    
    printf(BOLD WHITE);
    printf("┌──────────┬──────────┬──────────────┬─────────┬──────────────┬────────────┐\n");
    printf("│ %-8s │ %-8s │ %-12s │ %-7s │ %-12s │ %-10s │\n",
           "REQ-ID", "EQUIP-ID", "UNIT", "QTY", "PRIORITY", "STATUS");
    printf("├──────────┼──────────┼──────────────┼─────────┼──────────────┼────────────┤\n");
    printf(RESET);
    
    for (int i = 0; i < request_count; i++) {
        SupplyRequest* req = &requests[i];
        const char* status_color = (req->status == REQ_PENDING) ? YELLOW :
                                  (req->status == REQ_APPROVED) ? GREEN :
                                  (req->status == REQ_FULFILLED) ? BLUE : RED;
        
        printf("│ %-8d │ %-8d │ %-12s │ %-7d │ %-12s │ %s%-10s%s │\n",
               req->req_id, req->equipment_id, req->requesting_unit,
               req->requested_qty, PRIORITY_NAMES[req->priority],
               status_color, STATUS_NAMES[req->status], RESET);
    }
    
    printf("└──────────┴──────────┴──────────────┴─────────┴──────────────┴────────────┘\n");
    printf(BOLD CYAN "Total Supply Requests: %d\n" RESET, request_count);
    wait_for_enter();
}

void low_stock_alert(void) {
    display_banner();
    printf(BOLD RED "🚨 LOW STOCK ALERT\n" RESET);
    printf("════════════════════════════════════════════════════════════════════════════════\n");
    printf(BOLD WHITE "Equipment requiring immediate attention:\n\n" RESET);
    
    int alerts = 0;
    
    for (int i = 0; i < item_count; i++) {
        if (get_stock_status(&inventory[i]) == STATUS_LOW) {
            printf(BOLD RED "🚨 CRITICAL: " WHITE "%s (ID: %d)\n" RESET, 
                   inventory[i].name, inventory[i].id);
            printf(CYAN "    Current: " WHITE "%d" CYAN ", Minimum: " WHITE "%d\n" RESET, 
                   inventory[i].quantity, inventory[i].min_threshold);
            printf(CYAN "    Location: " WHITE "%s\n\n" RESET, inventory[i].location);
            alerts++;
        }
    }
    
    if (alerts == 0) {
        printf(GREEN "✅ All equipment levels are adequate.\n" RESET);
    } else {
        printf(BOLD RED "⚠️  Total items requiring resupply: %d\n" RESET, alerts);
    }
    
    wait_for_enter();
}

void export_report(void) {
    display_banner();
    printf(BOLD YELLOW "📄 EXPORT INVENTORY REPORT\n" RESET);
    printf("════════════════════════════════════════════════════════════════════════════════\n");
    
    FILE* report = fopen("inventory_report.txt", "w");
    if (!report) {
        printf(RED "❌ Error creating report file.\n" RESET);
        wait_for_enter();
        return;
    }
    
    time_t now = time(NULL);
    fprintf(report, "TACTICAL SUPPLY INVENTORY REPORT\n");
    fprintf(report, "Generated: %s", ctime(&now));
    fprintf(report, "Data Source: %s\n", use_database ? "PostgreSQL Database" : "Local Files");
    fprintf(report, "================================\n\n");
    
    fprintf(report, "INVENTORY SUMMARY:\n");
    fprintf(report, "Total Items: %d\n", item_count);
    
    int low_stock = 0;
    for (int i = 0; i < item_count; i++) {
        if (get_stock_status(&inventory[i]) == STATUS_LOW) {
            low_stock++;
        }
    }
    fprintf(report, "Items requiring resupply: %d\n\n", low_stock);
    
    fprintf(report, "DETAILED INVENTORY:\n");
    for (int i = 0; i < item_count; i++) {
        Equipment* item = &inventory[i];
        fprintf(report, "ID: %d | %s | Qty: %d %s | Location: %s | Status: %s | Class: %s\n",
                item->id, item->name, item->quantity, item->unit, 
                item->location, STOCK_STATUS_NAMES[get_stock_status(item)],
                CLASS_NAMES[item->classification]);
    }
    
    fclose(report);
    printf(GREEN "✅ Report exported to 'inventory_report.txt'\n" RESET);
    log_action("Inventory report exported");
    wait_for_enter();
}

int main(void) {
    printf(GREEN "🔄 Initializing Tactical Supply Management System...\n" RESET);
    
    memset(hash_table, 0, sizeof(hash_table));
    
    use_database = connect_database();
    load_data();
    
    printf(GREEN "🎯 System ready. Loaded %d equipment items and %d requests.\n" RESET, 
           item_count, request_count);
    sleep(2);
    
    int choice;
    char search_term[MAX_NAME_LEN];
    
    while (1) {
        display_menu();
        choice = get_int_input("", 1, 9);
        
        switch (choice) {
            case 1: add_equipment(); break;
            case 2:
                display_banner();
                get_string_input("🔍 Search term: ", search_term, MAX_NAME_LEN);
                check_inventory(search_term);
                break;
            case 3: list_all_equipment(); break;
            case 4: update_quantity(); break;
            case 5: request_supply(); break;
            case 6: check_requests(); break;
            case 7: low_stock_alert(); break;
            case 8: export_report(); break;
            case 9:
                display_banner();
                printf(BOLD YELLOW "🔄 Shutting down system...\n" RESET);
                save_data();
                printf(GREEN "💾 Data saved successfully.\n" RESET);
                log_action("System shutdown");
                
                if (db_conn) {
                    PQfinish(db_conn);
                }
                
                for (int i = 0; i < HASH_SIZE; i++) {
                    HashNode* current = hash_table[i];
                    while (current) {
                        HashNode* temp = current;
                        current = current->next;
                        free(temp);
                    }
                }
                
                printf(BOLD GREEN "🛡️  Tactical Supply Management System offline.\n" RESET);
                printf(BOLD WHITE "✅ All systems secured. Mission complete.\n" RESET);
                exit(0);
        }
    }
    
    return 0;
}