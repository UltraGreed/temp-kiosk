#include "logger_interface.h"

#include <assert.h>
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

#define SELECT_BETWEEN_DATE_FQUERY "select temp from %s where DATETIME(date) BETWEEN '%s' AND '%s';"
#define SELECT_BY_ID_FQUERY "select id, date from %s order by id;"
#define DELETE_BY_ID_FQUERY "delete from %s where id = %d;"
#define INSERT_FQUERY "insert into %s (date, temp) values ('%s', %lf);"
#define CREATE_TABLE_FQUERY                                                                                  \
    "create table if not exists %s"                                                                          \
    "(id integer primary key, \
                            date datetime not null, \
                            temp float not null);"

#define MAX_QUERY_LEN 1024

struct Log {
    sqlite3 *db;
    const char *table_name;
};

static int check_db_exist(const char *path);
static void print_fquery(char *query, const char *format, ...);
static int prepare_stmt(sqlite3 *db, const char *query, sqlite3_stmt **stmt);

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

    char query[MAX_QUERY_LEN + 1];
    print_fquery(query, CREATE_TABLE_FQUERY, log->table_name);

    char *errmsg;
    sqlite3_exec(log->db, query, NULL, NULL, &errmsg);
    if (errmsg != NULL) {
        fprintf(stderr, "Failed to create and initialize database with query '%s': %s\n", query, errmsg);
        exit(1);
    } else if (!exists) {
        fprintf(stderr, "Created and initialized new table %s in database %s.\n", table_name, db_path);
    }

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
    print_fquery(query, INSERT_FQUERY, log->table_name, date_str, value);

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
    char date_start_str[DATE_LEN + 1], date_now_str[DATE_LEN + 1];
    get_datetime_from_secs(&date_start, to_secs(date) - period);

    print_date(date_start_str, &date_start);
    print_date(date_now_str, date);

    char query[MAX_QUERY_LEN + 1];
    print_fquery(query, SELECT_BETWEEN_DATE_FQUERY, log->table_name, date_start_str, date_now_str);

    sqlite3_stmt *stmt;
    if (prepare_stmt(log->db, query, &stmt) == -1)
        return INFINITY;

    f64 sum = 0;
    usize ctr = 0;
    for (int res = sqlite3_step(stmt); res != SQLITE_DONE; res = sqlite3_step(stmt)) {
        if (res != SQLITE_ROW) {
            fprintf(stderr, "Failed to obtain database entry: %s (%d)\n", sqlite3_errstr(res), res);
            return INFINITY;
        }
        sum += sqlite3_column_double(stmt, 0);
        ctr++;
    }
    sqlite3_finalize(stmt);
    return sum / ctr;
}

int delete_old_entries(Log *log, DateTime *date, usize max_period)
{
    char query_select[MAX_QUERY_LEN + 1];
    print_fquery(query_select, SELECT_BY_ID_FQUERY, log->table_name);

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
            print_fquery(query_del, DELETE_BY_ID_FQUERY, log->table_name, id);

            char *errmsg;
            sqlite3_exec(log->db, query_del, NULL, NULL, &errmsg);
            if (errmsg != NULL) {
                fprintf(stderr, "Failed to delete entry from database with query '%s': %s", query_del, errmsg);
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
__attribute__((format(printf, 2, 3)))
static void print_fquery(char *query, const char *format, ...)
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
        fprintf(stderr, "Failed to prepare statement for query '%s': %s (%d)", query, sqlite3_errstr(res), res);
        return -1;
    }
    return 0;
}

