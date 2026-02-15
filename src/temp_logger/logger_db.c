#include "logger_interface.h"

#include <assert.h>
#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/cdefs.h>

#include "sqlite3.h"

#include "cross_time.h"
#include "my_types.h"
#include "temp_logger.h"
#include "utils.h"

#define SELECT_BETWEEN_DATE_FQUERY "select date, temp from %s where DATETIME(date) between '%s' and '%s';"
#define COUNT_BETWEEN_DATE_FQUERY "select count(1) from %s where DATETIME(date) between '%s' and '%s';"
#define SELECT_BY_ID_FQUERY "select id, date from %s order by id;"
#define DELETE_BY_ID_FQUERY "delete from %s where id = %d;"
#define INSERT_FQUERY "insert into %s (date, temp) values ('%s', %lf);"

#define CREATE_TABLE_FQUERY                                                                                  \
    "create table if not exists %s"                                                                          \
    "(id integer primary key,                                                                                \
    date datetime not null,                                                                                  \
    temp float not null);"

#define PRAGMA_WAL_QUERY "pragma journal_mode=WAL;"

#define BUSY_TIMEOUT_MS 1000
#define MAX_QUERY_LEN 1024

struct Log {
    sqlite3 *db;
    const char *table_name;
};

static int check_db_exist(const char *path);
static void xprint_fquery(char *query, const char *format, ...);
static int prepare_stmt(sqlite3 *db, const char *query, sqlite3_stmt **stmt);
static void xexec_query(sqlite3 *db, char *query);
static sqlite3_stmt *prepare_select_between_dates_stmt(Log *log, const DateTime *date_start,
                                                       const DateTime *date_end);
static i64 count_between_dates(Log *log, const DateTime *date_start, const DateTime *date_end);

// TODO: it would be better to accept one string in format like path/to/database.db:table_name
Log *init_log(const char db_path[], const char table_name[])
{
    int res;

    int exists = check_db_exist(db_path);
    if (exists == -1) {
        fprintf(stderr, "Failed to access database file: %s\n", db_path);
        exit(1);
    }

    Log *log = xmalloc(sizeof(Log));
    res = sqlite3_open(db_path, &log->db);
    if (res != SQLITE_OK) {
        fprintf(stderr, "Failed to open database: %s (%d)\n", sqlite3_errmsg(log->db), res);
        exit(1);
    }

    log->table_name = table_name;

    char query_create[MAX_QUERY_LEN + 1];
    xprint_fquery(query_create, CREATE_TABLE_FQUERY, log->table_name);
    xexec_query(log->db, query_create);
    if (!exists)
        fprintf(stderr, "Created and initialized new table %s in database %s.\n", table_name, db_path);

    xexec_query(log->db, PRAGMA_WAL_QUERY);

    sqlite3_busy_timeout(log->db, BUSY_TIMEOUT_MS);

    return log;
}

int deinit_log(Log *log)
{
    int res = 0;
    if (sqlite3_close(log->db) != SQLITE_OK)
        res = -1;
    free(log);
    return res;
}

int write_log(Log *log, f64 value, DateTime *date, usize max_period)
{
    int res;
    char date_str[DATE_LEN + 1];
    print_date(date_str, date);

    if (delete_old_entries(log, date, max_period) == -1)
        fprintf(stderr, "Failed to delete old entries\n");

    char query[MAX_QUERY_LEN + 1];
    xprint_fquery(query, INSERT_FQUERY, log->table_name, date_str, value);

    sqlite3_stmt *stmt;
    if (prepare_stmt(log->db, query, &stmt) == -1)
        return -1;

    res = sqlite3_step(stmt);
    if (res != SQLITE_DONE) {
        fprintf(stderr, "Failed to insert into database: %s (%d)\n", sqlite3_errstr(res), res);
        return -1;
    }

    sqlite3_finalize(stmt);
    return 0;
}

f64 get_avg_log(Log *log, f64 period, DateTime *date)
{
    DateTime date_start;
    get_datetime_from_secs(&date_start, to_secs(date) - period);

    sqlite3_stmt *stmt = prepare_select_between_dates_stmt(log, &date_start, date);
    if (stmt == NULL)
        return INFINITY;

    f64 sum = 0;
    usize ctr = 0;
    for (int res = sqlite3_step(stmt); res != SQLITE_DONE; res = sqlite3_step(stmt)) {
        if (res != SQLITE_ROW) {
            fprintf(stderr, "Failed to obtain database entry: %s (%d)\n", sqlite3_errstr(res), res);
            return INFINITY;
        }
        sum += sqlite3_column_double(stmt, 1);
        ctr++;
    }
    sqlite3_finalize(stmt);
    return sum / ctr;
}

int delete_old_entries(Log *log, DateTime *date, usize max_period)
{
    char query_select[MAX_QUERY_LEN + 1];
    xprint_fquery(query_select, SELECT_BY_ID_FQUERY, log->table_name);

    sqlite3_stmt *stmt_select;
    if (prepare_stmt(log->db, query_select, &stmt_select) == -1)
        return -1;

    // This thing evaluates lazily and we exit after first "new" entry,
    // so it's amortized O(1) actually.
    for (int res = sqlite3_step(stmt_select); res != SQLITE_DONE; res = sqlite3_step(stmt_select)) {
        if (res != SQLITE_ROW) {
            fprintf(stderr, "Failed to obtain database entry: %s (%d)\n", sqlite3_errstr(res), res);
            return -1;
        }
        const u8 *date_str = sqlite3_column_text(stmt_select, 1);
        bool is_ascii = !is_valid_ascii(date_str);
        bool is_old = false;
        if (!is_ascii) {
            fprintf(stderr, "Non ASCII characters in date column! Deleting entry...\n");
        } else {
            DateTime date_entry;
            scan_date((char *)date_str, &date_entry);
            is_old = to_secs(date) - to_secs(&date_entry) > max_period;
        }

        if (!is_ascii || is_old) {
            int id = sqlite3_column_int(stmt_select, 0);
            char query_del[MAX_QUERY_LEN + 1];
            xprint_fquery(query_del, DELETE_BY_ID_FQUERY, log->table_name, id);

            char *errmsg;
            sqlite3_exec(log->db, query_del, NULL, NULL, &errmsg);
            if (errmsg != NULL) {
                fprintf(stderr, "Failed to delete entry from database with query '%s': %s", query_del,
                        errmsg);
                sqlite3_free(errmsg);
            }
        } else {
            // If we encountered at least one entry which is neither old nor invalid, we can stop here.
            break;
        }
    }
    sqlite3_finalize(stmt_select);
    return 0;
}

TempArray *get_array_entries(Log *log, const DateTime *date_start, const DateTime *date_end)
{
    if (date_start == NULL)
        date_start = &FIRST_DATE;

    if (date_end == NULL)
        date_end = &LAST_DATE;

    i64 n = count_between_dates(log, date_start, date_end);
    if (n == -1)
        return NULL;

    TempArray *array = xmalloc(sizeof(TempArray));
    array->items = malloc(sizeof(TempEntry) * n);
    if (array->items == NULL) {
        fprintf(stderr, "Failed to allocate memory for TempArray of size %zd: %s (%d)\n", n, strerror(errno),
                errno);
        free(array);
        return NULL;
    }
    array->size = n;

    sqlite3_stmt *stmt = prepare_select_between_dates_stmt(log, date_start, date_end);
    if (stmt == NULL)
        goto error;

    usize i = 0;
    for (int res = sqlite3_step(stmt); res != SQLITE_DONE; res = sqlite3_step(stmt), i++) {
        if (res != SQLITE_ROW) {
            fprintf(stderr, "Failed to obtain database entry: %s (%d)\n", sqlite3_errstr(res), res);
            goto error;
        }

        const u8 *date_str = sqlite3_column_text(stmt, 0);
        if (!is_valid_ascii(date_str) || scan_date((const char *)date_str, &array->items[i].date) == -1) {
            fprintf(stderr, "WARN: Invalid entry found in date column! %s\n", date_str);
            array->items[i] = (TempEntry){0};
            continue;
        }

        array->items[i].temp = sqlite3_column_double(stmt, 1);
    }

end:
    sqlite3_finalize(stmt);
    return array;
error:
    free(array->items);
    free(array);
    array = NULL;
    goto end;
}

/// Return -1 on error, boolean otherwise
static int check_db_exist(const char *path)
{
    sqlite3 *db;
    int err = sqlite3_open_v2(path, &db, SQLITE_OPEN_READONLY, NULL);
    int res;
    if (err == SQLITE_CANTOPEN)
        res = 0;
    else if (err == SQLITE_OK)
        res = 1;
    else
        res = -1;

    if (db != NULL)
        sqlite3_close(db);
    return res;
}

/// Try to print formatted query, exit if query is too long.
__attribute__((format(printf, 2, 3))) //
static void xprint_fquery(char *query, const char *format, ...)
{
    va_list args;
    va_start(args, format);

    int res = vsnprintf(query, MAX_QUERY_LEN + 1, format, args);
    if (res >= MAX_QUERY_LEN + 1) {
        fprintf(stderr, "Data string representation is too long, can't fit in query! Query: '%s'\n", query);
        exit(1);
    }

    va_end(args);
}

static int prepare_stmt(sqlite3 *db, const char *query, sqlite3_stmt **stmt)
{
    int res = sqlite3_prepare_v2(db, query, -1, stmt, NULL);
    if (res != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement for query '%s': %s (%d)", query, sqlite3_errstr(res),
                res);
        return -1;
    }
    return 0;
}

/// Execute provided statement or exit on fail
static void xexec_query(sqlite3 *db, char *query)
{
    char *errmsg;
    sqlite3_exec(db, query, NULL, NULL, &errmsg);
    if (errmsg != NULL) {
        fprintf(stderr, "Failed to execute query '%s': %s\n", query, errmsg);
        exit(1);
    }
}

/// Prepare statement, selecting all the entries in range of the given dates.
/// Caller is responsible for memory freeing.
/// Return NULL on error.
static sqlite3_stmt *prepare_select_between_dates_stmt(Log *log, const DateTime *date_start,
                                                       const DateTime *date_end)
{
    char date_start_str[DATE_LEN + 1], date_end_str[DATE_LEN + 1];
    print_date(date_start_str, date_start);
    print_date(date_end_str, date_end);

    char query[MAX_QUERY_LEN + 1];
    xprint_fquery(query, SELECT_BETWEEN_DATE_FQUERY, log->table_name, date_start_str, date_end_str);

    sqlite3_stmt *stmt;
    int res = prepare_stmt(log->db, query, &stmt);
    if (res == -1) {
        fprintf(stderr, "Failed to prepare database statement with query \"%s\": %s (%d)\n", query,
                sqlite3_errstr(res), res);
        return NULL;
    }
    return stmt;
}

static int count_callback(void *count, int n_cols, char **entries, char **col_names)
{
    (void)col_names;
    assert(n_cols == 1);
    *(i64 *)count = atoll(entries[0]);
    return 0;
}

/// Count entries between provided dates.
/// Return -1 on error, amount of entries otherwise.
static i64 count_between_dates(Log *log, const DateTime *date_start, const DateTime *date_end)
{
    char date_start_str[DATE_LEN + 1], date_end_str[DATE_LEN + 1];
    print_date(date_start_str, date_start);
    print_date(date_end_str, date_end);

    char count_query[MAX_QUERY_LEN + 1];
    xprint_fquery(count_query, COUNT_BETWEEN_DATE_FQUERY, log->table_name, date_start_str, date_end_str);

    char *errmsg;
    i64 count;
    sqlite3_exec(log->db, count_query, &count_callback, &count, &errmsg);
    if (errmsg != NULL) {
        fprintf(stderr, "Failed to count amount of entries with query \"%s\": %s\n", count_query, errmsg);
        sqlite3_free(errmsg);
        return -1;
    }

    return count;
}
