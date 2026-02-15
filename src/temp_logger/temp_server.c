#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cross_socket.h"
#include "cross_time.h"
#include "my_types.h"
#include "utils.h"

#include "logger_interface.h"
#include "temp_logger.h"

#define LISTEN_PORT 8080
#define MAX_PENDING 10
#define ACCEPT_TIMEOUT_MS 500
#define HTTP_GET_MAX_LEN 1024

#define RESPONSE_HEADER_FSTRING                                                                              \
    "HTTP/1.1 200 OK\r\nContent-Length: %zu\r\nContent-Type: application/json; charset=utf-8\r\nAccess-Control-Allow-Origin: *\r\n\r\n"
#define RESPONSE_HEADER_MAX_LEN 256

#define _TO_TEXT(S) #S
#define TO_TEXT(S) _TO_TEXT(S)

#define ERROR_RESPONSE_BUF_LEN 1024
#define TEMP_SERIALIZE_LEN (MSG_LEN - 1)

static bool is_working = true;
void sigint_handler(int sig)
{
    (void)sig;
    is_working = false;
}

int respond_client(Socket client, char *message)
{
    int res = send(client, message, strlen(message), 0);
    if (res == -1)
        perror("Failed to respond to client");
    return res;
}

int respond_error(Socket client, char *code, const char *message)
{
    char response[ERROR_RESPONSE_BUF_LEN];
    int n = snprintf(response, ERROR_RESPONSE_BUF_LEN, "HTTP/1.1 %s %s\r\n\r\n", code, message);
    assert(n < ERROR_RESPONSE_BUF_LEN);

    return respond_client(client, response);
}

int respond_server_error(Socket client)
{
    return respond_error(client, "500", "Internal Server Error");
}

bool is_entry_null(TempEntry *entry)
{
    return memcmp(entry, &(TempEntry){0}, sizeof(TempEntry)) == 0;
}

usize count_not_null(TempArray *array)
{
    usize ctr = 0;
    for (usize i = 0; i < array->size; i++) {
        if (!is_entry_null(&array->items[i]))
            ctr++;
    }
    return ctr;
}

// {"data":[{"date":"YYYY-MM-DD hh:mm:ss.sss","temp":12.0000},]}
char *print_json(TempArray *array, char *dest)
{
    char *pos = dest;

    pos += sprintf(pos, "{\"data\":[");
    for (usize i = 0; i < array->size; i++) {
        if (is_entry_null(&array->items[i]))
            continue;

        if (pos != dest + 9) { // Insert comma between each entry
            sprintf(pos, ",");
            pos++;
        }

        // TODO: send unix-epoch timestamp instead of this 
        char date_str[DATE_LEN + 1];
        print_date(date_str, &array->items[i].date);

        char temp_str[TEMP_SERIALIZE_LEN + 1];
        snprintf(temp_str, TEMP_SERIALIZE_LEN + 1, "%lf", array->items[i].temp);

        pos += sprintf(pos, "{\"date\":\"%s\",\"temp\":%s}", date_str, temp_str);
    }
    sprintf(pos, "]}");
    pos += 2;
    return pos;
}

char *create_response(TempArray *array)
{
    assert(array != NULL);

    usize json_size = 11 + 50 * count_not_null(array) - 1;

    // HTTP/1.1 200 OK  Content-Length:   Content-Type: application/json; charset=utf-8
    usize total_size = RESPONSE_HEADER_MAX_LEN + json_size;

    char *response = malloc(sizeof(char) * (total_size + 1));
    if (response == NULL) {
        fprintf(stderr, "Failed to malloc for response string of size %zu", total_size);
        return NULL;
    }

    // Print header
    char *print_pos = response;
    print_pos += sprintf(response, RESPONSE_HEADER_FSTRING, json_size);

    print_pos = print_json(array, print_pos);

    *print_pos = '\0';

    return response;
}

int handle_client(Log **logs, Socket client)
{
    TempArray *array = NULL, *array1 = NULL, *array2 = NULL, *array3 = NULL;
    char *response = NULL;

    int retval = 0;

    char request[HTTP_GET_MAX_LEN + 1];
    i64 n_read = recv(client, request, HTTP_GET_MAX_LEN + 1, 0);
    if (n_read == -1) {
        perror("Failed reading from connected client");
        goto error;
    }
    if (n_read == HTTP_GET_MAX_LEN + 1) {
        fprintf(stderr, "Received invalid request: request too long!\n");
        respond_error(client, "413", "Content Too Large");
        goto error;
    }
    request[n_read] = '\0';

    fprintf(stderr, "Received request:\n\"\"\"%s\"\"\"\n", request);

    // TODO: this is not a great way to do things, for sure
    char get_query[HTTP_GET_MAX_LEN + 1];
    f32 http_ver;
    int n_match = sscanf(request, "GET %" TO_TEXT(HTTP_GET_MAX_LEN) "s HTTP/%f", get_query, &http_ver);
    if (n_match != 2) {
        fprintf(stderr, "Failed to parse request: invalid format!\n");
        respond_error(client, "400", "Bad Request");
        goto error;
    }

    DateTime *date_start_ptr = NULL, *date_end_ptr = NULL;
    char *unix_start_str = strstr(get_query, "date_start=");
    if (unix_start_str != NULL) {
        usize unix_ms;
        int ms_read = sscanf(unix_start_str + 11, "%zd", &unix_ms);
        if (ms_read != 1) {
            fprintf(stderr, "Failed to parse request: invalid date_start!\n");
            respond_error(client, "400", "Bad Request");
            goto error;
        }
        DateTime date_start;
        get_datetime_from_secs(&date_start, (f64) unix_ms / 1000);
        date_start_ptr = &date_start;
    }

    char *unix_end_str = strstr(get_query, "date_end=");
    if (unix_end_str != NULL) {
        usize unix_ms;
        int ms_read = sscanf(unix_end_str + 9, "%zd", &unix_ms);
        if (ms_read != 1) {
            fprintf(stderr, "Failed to parse request: invalid date_end format!\n");
            respond_error(client, "400", "Bad Request");
            goto error;
        }
        DateTime date_start;
        get_datetime_from_secs(&date_start, (f64) unix_ms / 1000);
        date_end_ptr = &date_start;
    }

    array1 = get_array_entries(logs[0], date_start_ptr, date_end_ptr);
    array2 = get_array_entries(logs[1], date_start_ptr, date_end_ptr);
    array3 = get_array_entries(logs[2], date_start_ptr, date_end_ptr);
    if (array1 == NULL || array2 == NULL || array3 == NULL) {
        respond_server_error(client);
        goto error;
    }

    array = xmalloc(sizeof(TempArray));

    usize sum_size = array1->size + array2->size + array3->size;
    array->items = malloc(sizeof(TempEntry) * sum_size);
    if (array->items == NULL) {
        fprintf(stderr, "Failed to malloc for %zu entries: %s (%d)\n", sum_size, strerror(errno), errno);
        respond_server_error(client);
        goto end;
    }

    usize n1 = array1->size * sizeof(TempEntry);
    memcpy(array->items, array1->items, n1);
    usize n2 = array2->size * sizeof(TempEntry);
    memcpy(&array->items[array1->size], array2->items, n2);
    usize n3 = array3->size * sizeof(TempEntry);
    memcpy(&array->items[array1->size + array2->size], array3->items, n3);

    array->size = sum_size;

    response = create_response(array);
    if (response == NULL) {
        fprintf(stderr, "Failed to create response with array of size %zu!\n", sum_size);
        respond_server_error(client);
        goto end;
    }

    if (respond_client(client, response) != -1)
        fprintf(stderr, "Request parsed successfully, response send:\n\"\"\"%s\"\"\"\n", response);

end:
    if (array != NULL) {
        if (array->items != NULL)
            free(array->items);
        free(array);
    }

    if (response != NULL)
        free(response);

    if (array1 != NULL)
        free(array1);
    if (array2 != NULL)
        free(array2);
    if (array3 != NULL)
        free(array3);

    close_socket(client);
    return retval;
error:
    retval = -1;
    goto end;
}

int main(int argc, char **argv)
{
#ifndef USEDB
    fprintf(stderr, "HTTP server without database is not supported.\n");
    exit(2);
#endif
    if (argc != 2) {
        fprintf(stderr, "Usage: temp_server LOG_PATH\n");
        exit(2);
    }

#ifndef WIN32
    signal(SIGPIPE, SIG_IGN);
#endif
    signal(SIGINT, sigint_handler);

    char *db_path = argv[1];
    Log *logs[3];
    for (int i = 0; i < 3; i++)
        logs[i] = init_log(db_path, LOG_ARGS[i]);

    Socket server_socket = open_socket_tcp();
    if (server_socket == (Socket)-1) {
        perror("Failed to open socket");
        exit(1);
    }

    SocketAddress server_addr = init_ipv4_addr(LISTEN_PORT);

    int res = bind(server_socket, (const struct sockaddr *)&server_addr, sizeof(server_addr));
    if (res == -1) {
        perror("Failed to bind socket");
        exit(1);
    }

    res = listen(server_socket, MAX_PENDING);
    if (res == -1) {
        perror("Failed to listen on socket");
        exit(1);
    }

    fprintf(stderr, "Server successfully started!\n");

    while (is_working) {
        // TODO: this it kind of legacy, but all the other solutions are platform-specific,
        // so this will do for now.
        // poll for POSIX, IOCP for Windows are better choices.
        // Also possibly epoll for Linux and kqueue for OS X/FreeBSD.
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(server_socket, &read_fds);
        struct timeval accept_timeout = {
            .tv_sec = 0,
            .tv_usec = ACCEPT_TIMEOUT_MS * 1000,
        };
        int n_ready = select(server_socket + 1, &read_fds, NULL, NULL, &accept_timeout);
        if (n_ready == -1) {
            if (errno == EINTR)
                break;
            perror("Failed to call select() with server socket");
            continue;
        }
        if (n_ready == 0) // Timed out
            continue;

        Socket client_socket = accept(server_socket, NULL, NULL);
        if (client_socket == (Socket)-1) {
            perror("Failed to accept client");
            continue;
        }

        handle_client(logs, client_socket);
    }

    if (close_socket(server_socket) == -1)
        fprintf(stderr, "Failed to close socket!\n");

    res = 0;
    for (int i = 0; i < 3; i++)
        res |= deinit_log(logs[i]);

    if (res != 0)
        fprintf(stderr, "Failed to deinit logs!\n");

    fprintf(stderr, "Server finished!\n");
}
