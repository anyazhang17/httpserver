#include "asgn2_helper_funcs.h"
#include "queue.h"
#include "rwlock.h"
#include "hashmap.h"
#include <regex.h>
#include <fcntl.h>
#include <unistd.h>
#include <bits/getopt_core.h>
#include <pthread.h>
#include <errno.h>
#include <math.h>
#include <assert.h>

#define MAX_SIZE        100
#define ARRAY_SIZE(arr) (sizeof((arr)) / sizeof((arr)[0]))
//global hashmap with key uri and value rwlock, global queue containing connection_fd
queue_t *q;
hashmap *hm;
pthread_mutex_t *mutex;
// pthread_mutex_t *buf_mutex;
// char buf[4096] = { 0 };
int connections = 0;
int connections_closed = 0;

void *worker_thread(void *);
void GET(int connection_fd, char *uri, int request_id);
void PUT(int connection_fd, char *uri, int request_id, int length, int bytes_read, regoff_t so,
    char *buffer);
void response(int fd, int status_code);
void audit_log(char *method, char *uri, int status_code, int request_id);

void response(int fd, int status_code) {
    int res = 0;
    if (status_code == 200) {
        res = write_n_bytes(fd, "HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\nOK\n", 41);
    } else if (status_code == 201) {
        res = write_n_bytes(fd, "HTTP/1.1 201 Created\r\nContent-Length: 8\r\n\r\nCreated\n", 51);
    } else if (status_code == 400) {
        res = write_n_bytes(
            fd, "HTTP/1.1 400 Bad Request\r\nContent-Length: 12\r\n\r\nBad Request\n", 60);
    } else if (status_code == 403) {
        res = write_n_bytes(
            fd, "HTTP/1.1 403 Forbidden\r\nContent-Length: 10\r\n\r\nForbidden\n", 56);
    } else if (status_code == 404) {
        res = write_n_bytes(
            fd, "HTTP/1.1 404 Not Found\r\nContent-Length: 10\r\n\r\nNot Found\n", 56);
    } else if (status_code == 500) {
        res = write_n_bytes(fd,
            "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 22\r\n\r\nInternal "
            "Server Error\n",
            80);
    } else if (status_code == 501) {
        res = write_n_bytes(
            fd, "HTTP/1.1 501 Not Implemented\r\nContent-Length: 16\r\n\r\nNot Implemented\n", 68);
    } else if (status_code == 505) {
        res = write_n_bytes(fd,
            "HTTP/1.1 505 Version Not Supported\r\nContent-Length: 22\r\n\r\nVersion Not "
            "Supported\n",
            80);
    }
    if (res == -1) {
        write_n_bytes(fd,
            "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 22\r\n\r\nInternal "
            "Server Error\n",
            80);
    }
}

void audit_log(char *method, char *uri, int status_code, int request_id) {
    fprintf(stderr, "%s,%s,%d,%d\n", method, uri, status_code, request_id);
}

void GET(int connection_fd, char *uri, int request_id) {
    //int status_code = 200;
    //int res;
    //pthread_mutex_lock(mutex);
    rwlock_t *rw = (rwlock_t *) hashmap_get(hm, uri);
    if (rw == NULL) {
        //printf("GET didnt get rw in GET for id: %d, adding rw to hashmap\n", request_id);
        //add uri, rwlock to hashmap
        rw = rwlock_new(N_WAY, 1);
        hashmap_add(hm, uri, rw);
    }
    reader_lock(rw);
    //printf("in GET\n");
    int fd = open(uri, O_RDWR, 0666);
    if (fd == -1) {
        //UNLOCK BEFORE RETURNING
        //pthread_mutex_unlock(mutex);
        reader_unlock(rw);
        if (errno == ENOENT) {
            response(connection_fd, 404);
            audit_log("GET", uri, 404, request_id);
            return;
        } else if (errno == EACCES || errno == EISDIR) {
            response(connection_fd, 403);
            return;
        } else {
            response(connection_fd, 500);
            return;
        }
    }

    // rwlock_t *rw = (rwlock_t*) hashmap_get(hm, uri);
    // if (rw == NULL) {
    //     //printf("GET didnt get rw in GET for id: %d, adding rw to hashmap\n", request_id);
    //     //add uri, rwlock to hashmap
    //     rw = rwlock_new(N_WAY, 1);
    //     hashmap_add(hm, uri, rw);
    // }
    //printf("GET rwlock address: %p\n", (void *)rw);

    //reader_lock(rw);
    //printf("reader locked\n");
    //printf("fd = %d\n", fd);
    //pthread_mutex_unlock(mutex);

    //send response
    //get length of file
    int length = lseek(fd, 0, SEEK_END);
    //printf("length is %d", length);
    lseek(fd, 0, SEEK_SET);
    //char r[100] = "HTTP/1.1 200 OK\r\nContent-Length: ";
    char *r = "HTTP/1.1 200 OK\r\nContent-Length: ";
    write_n_bytes(connection_fd, r, 33);
    //printf("res is %d\n", res);
    int length_bytes = (int) ((floor(log10(length)) + 1) * sizeof(char));
    if (length_bytes < 0) {
        length_bytes = 0;
    }
    char length_str[length_bytes];
    sprintf(length_str, "%d", length);
    //append content length to response r
    //strcat(r, length_str);
    write_n_bytes(connection_fd, length_str, length_bytes);
    //printf("res is %d\n", res);
    r = "\r\n\r\n";
    //char space[5] = "\r\n\r\n\0";
    //strcat(r, space);
    write_n_bytes(connection_fd, r, strlen(r));
    //printf("res is %d\n", res);

    //send file
    //pass bytes from file to socket
    int res = pass_n_bytes(fd, connection_fd, length);
    if (length != res) {
        printf("length is %d and res is %d\n", length, res);
    }
    lseek(fd, 0, SEEK_SET);
    close(fd);

    audit_log("GET", uri, 200, request_id);
    //printf("reader unlocking\n");
    reader_unlock(rw);
}

void PUT(int connection_fd, char *uri, int request_id, int length, int bytes_read, regoff_t so,
    char *buffer) {
    //printf("in PUT\n");

    rwlock_t *rw = (rwlock_t *) hashmap_get(hm, uri);
    if (rw == NULL) {
        //printf("PUT didnt get rw in GET for id: %d, adding rw to hashmap\n", request_id);
        //add uri, rwlock to hashmap
        rw = rwlock_new(N_WAY, 1);
        hashmap_add(hm, uri, rw);
    }

    writer_lock(rw);
    int status_code = 200;
    //pthread_mutex_lock(mutex);
    int fd;
    if ((fd = open(uri, O_WRONLY | O_TRUNC, 0666)) == -1) {
        fd = creat(uri, 0666);
        //failed to create
        if (fd == -1) {
            status_code = 500;
            response(connection_fd, status_code);
            //UNLOCK BEFORE RETURNING
            //pthread_mutex_unlock(mutex);
            writer_unlock(rw);
            return;
        } else {
            status_code = 201;
        }
    }

    // rwlock_t *rw = (rwlock_t*) hashmap_get(hm, uri);
    // if (rw == NULL) {
    //     //printf("PUT didnt get rw in GET for id: %d, adding rw to hashmap\n", request_id);
    //     //add uri, rwlock to hashmap
    //     rw = rwlock_new(N_WAY, 1);
    //     hashmap_add(hm, uri, rw);
    // }

    //printf("PUT rwlock address: %p\n", (void *)rw);

    //writer_lock(rw);
    //printf("writer locked\n");
    //printf("fd = %d\n", fd);
    //pthread_mutex_unlock(mutex);

    response(connection_fd, status_code);

    int res;
    int bytes_written = 0;
    //write message body content
    int to_write = bytes_read - so;
    lseek(fd, 0, SEEK_SET);
    if (so != -1 && to_write > 0) {
        res = write_n_bytes(fd, buffer + so, to_write);
        if (res == -1) {
            status_code = 500;
            writer_unlock(rw);
            return;
        } else {
            bytes_written += res;
        }
    }
    //write remaining content
    pass_n_bytes(connection_fd, fd, length - bytes_written);
    lseek(fd, 0, SEEK_SET);
    close(fd);

    audit_log("PUT", uri, status_code, request_id);
    //printf("writer unlocking\n");
    writer_unlock(rw);
}

//void worker_thread(args): like single threaded server from asgn2
//pop requests out of the queue and process them, responds to client
void *worker_thread(void *arg) {
    if (0) {
        printf("%s\n", (char *) arg);
    }
    //regex to get request, header fields, and beginning of content body
    const char *request_re = "^([a-zA-Z]{1,8}) /([a-zA-Z0-9.-]{1,63}) "
                             "(HTTP/[0-9]\\.[0-9]+)\r\n([a-zA-Z0-9.-]{1,128}: "
                             "[\x20-\x7E]{1,128}\r\n)*\r\n(.*)$";
    regex_t request_regex;
    if (regcomp(&request_regex, request_re, REG_NEWLINE | REG_EXTENDED)) {
        printf("request_regex error\n");
    }
    //regex to get Request-Id header field
    const char *id_re = "^([a-zA-Z0-9.-]{1,128}: "
                        "[\x20-\x7E]{1,128}\r\n)*(Request-Id: [0-9]+)\r\n";
    regex_t id_regex;
    if (regcomp(&id_regex, id_re, REG_NEWLINE | REG_EXTENDED))
        printf("id_regex error\n");
    //regex to get Content-Length header field
    const char *length_re = "^([a-zA-Z0-9.-]{1,128}: "
                            "[\x20-\x7E]{1,128}\r\n)*(Content-Length: [0-9]+)\r\n";
    regex_t length_regex;
    if (regcomp(&length_regex, length_re, REG_NEWLINE | REG_EXTENDED))
        printf("length_regex error\n");

    while (1) {
        //printf("in loop of worker thread\n");
        //pop requests out of the queue and process them, responds to client
        void *temp;
        queue_pop(q, &temp);
        int connection_fd = (int) (intptr_t) temp;
        //printf("popped fd %d\n", connection_fd);
        char *method = NULL;
        char *uri = NULL;
        char *version = NULL;
        char *header_fields = NULL;
        char *message_body = NULL;
        int length = 0;
        int id = 0;
        regmatch_t request_pmatch[6] = { 0 };
        regmatch_t id_pmatch[3] = { 0 };
        regmatch_t length_pmatch[3] = { 0 };
        char buf[4096] = { 0 };

        //parse request from client
        // memset(buf, '\0', sizeof(buf));
        int bytes_read = read_until(connection_fd, buf, sizeof(buf), "\r\n\r\n");
        if (bytes_read > 0 && strstr(buf, "\r\n\r\n") != NULL) {
            //printf("read bytes, %d\n", bytes_read);
            //printf("buf =  %s\n", buf);
            //convert buf char array into a string for regexec
            char s[bytes_read + 1];
            strncpy(s, buf, bytes_read);
            s[bytes_read] = '\0';
            //printf("s =  %s\n", s);
            //s doesn't match regex, request bad format
            if (regexec(&request_regex, s, ARRAY_SIZE(request_pmatch), request_pmatch, 0) != 0) {
                response(connection_fd, 400);
                //CLOSE CONNECTION
                close(connection_fd);
                connections_closed++;
                //printf("connections = %d, connections closed = %d\n", connections, connections_closed);
                //pthread_mutex_unlock(buf_mutex);
                continue;
            }

            //save matches in string variables, get 5 substrings from buf
            for (size_t i = 1; i < ARRAY_SIZE(request_pmatch); i++) {
                regoff_t start = request_pmatch[i].rm_so;
                regoff_t len = request_pmatch[i].rm_eo - request_pmatch[i].rm_so;
                //printf("substring should be = %.*s\n", len, buf + start);
                if (i == 1) {
                    method = calloc(len + 1, sizeof(char));
                    strncpy(method, buf + start, len);
                    method[len] = '\0';
                    //printf("method is = %s\n", method);
                } else if (i == 2) {
                    uri = calloc(len + 1, sizeof(char));
                    strncpy(uri, buf + start, len);
                    uri[len] = '\0';
                    //printf("uri is = %s\n", uri);
                } else if (i == 3) {
                    version = calloc(len + 1, sizeof(char));
                    strncpy(version, buf + start, len);
                    version[len] = '\0';
                    //printf("version is = %s\n", version);
                } else if (i == 4) {
                    //check if there are headerfield matches first
                    if (len != 0) {
                        start = request_pmatch[i - 1].rm_eo + 2;
                        len = request_pmatch[i].rm_eo - start;
                        header_fields = calloc(len + 1, sizeof(char));
                        strncpy(header_fields, buf + start, len);
                        header_fields[len] = '\0';
                        //printf("headers are: %s\n", header_fields);
                        //extract the request id using regex, len("Request-Id: ") = 12
                        if (regexec(&id_regex, header_fields, ARRAY_SIZE(id_pmatch), id_pmatch, 0)
                            == 0) {
                            int l = id_pmatch[2].rm_eo - id_pmatch[2].rm_so - 12;
                            char str_id[l + 1];
                            str_id[l] = '\0';
                            strncpy(str_id, header_fields + id_pmatch[2].rm_so + 12, l);
                            id = strtol(str_id, NULL, 10);
                            //printf("id is: %d\n", id);
                        }
                        //extract the content length using regex, len("Content-Length: ") = 16
                        if (regexec(&length_regex, header_fields, ARRAY_SIZE(length_pmatch),
                                length_pmatch, 0)
                            == 0) {
                            int l = length_pmatch[2].rm_eo - length_pmatch[2].rm_so - 16;
                            char str_length[l + 1];
                            str_length[l] = '\0';
                            strncpy(str_length, header_fields + length_pmatch[2].rm_so + 16, l);
                            length = strtol(str_length, NULL, 10);
                            //printf("length is: %d\n", length);
                        }
                    }
                } else if (i == 5) {
                    //check if there is content
                    if (len != 0) {
                        message_body = calloc(len + 1, sizeof(char));
                        strncpy(message_body, buf + start, len);
                        message_body[len] = '\0';
                        //printf("message is = %s\n", message_body);
                    }
                }
            }
        } else {
            response(connection_fd, 400);
            //printf("BRUH bytes read: %d\n", bytes_read);
            //CLOSE CONNECTION
            close(connection_fd);
            connections_closed++;
            //printf("connections = %d, connections closed = %d\n", connections, connections_closed);
            //pthread_mutex_unlock(buf_mutex);
            continue;
        }

        if (strcmp(method, "GET") == 0) {
            GET(connection_fd, uri, id);
        } else if (strcmp(method, "PUT") == 0) {
            PUT(connection_fd, uri, id, length, bytes_read, request_pmatch[5].rm_so, buf);
        } else {
            response(connection_fd, 501);
        }

        //UNLOCK!!!
        //pthread_mutex_unlock(buf_mutex);

        if (method != NULL) {
            free(method);
        }
        if (uri != NULL) {
            free(uri);
        }
        if (version != NULL) {
            free(version);
        }
        if (header_fields != NULL) {
            free(header_fields);
        }
        if (message_body != NULL) {
            free(message_body);
        }

        //CLOSE CONNECTION
        close(connection_fd);
        connections_closed++;
        //printf("connections = %d, connections closed = %d\n", connections, connections_closed);
        //printf("end of while loop, accept another\n\n");
    }

    regfree(&request_regex);
    regfree(&id_regex);
    regfree(&length_regex);
}

int main(int argc, char *argv[]) {
    // ./httpserver [-t threads] <port>, -t optional but port required
    if (argc < 2 || argc > 4) {
        fprintf(stdout, "Invalid Port\n");
        return 1;
    }

    int opt;
    int num_threads = 4;
    int port = 0;
    while ((opt = getopt(argc, argv, "t:")) != -1) {
        switch (opt) {
        case 't': num_threads = atoi(optarg); break;
        default: fprintf(stdout, "Invalid Argument\n"); break;
        }
    }

    if (num_threads < 1) {
        fprintf(stdout, "Invalid Number of Threads\n");
        return 1;
    }

    char *end;
    port = strtol(argv[optind], &end, 10);
    //check port is an int
    if (*end != 0) {
        fprintf(stdout, "Invalid Port\n");
        return 1;
    }

    //check port is between 1 and 65535
    if (port < 1 || port > 65535) {
        fprintf(stdout, "Invalid Port\n");
        return 1;
    }

    //initialize global data structures
    q = queue_new(num_threads);
    hm = hashmap_new(num_threads);
    mutex = calloc(1, sizeof(pthread_mutex_t));
    int rc = pthread_mutex_init(mutex, NULL);
    assert(!rc);
    //buf_mutex = calloc(1, sizeof(pthread_mutex_t));
    //rc = pthread_mutex_init(buf_mutex, NULL);
    //assert(!rc);

    //bind to socket to listen
    Listener_Socket socket;
    int listener = listener_init(&socket, port);
    if (listener == -1) {
        fprintf(stdout, "Invalid Port\n");
        return 1;
    }

    //start num_threads number of threads
    for (int i = 0; i < num_threads; i++) {
        pthread_t thread;
        pthread_create(&thread, NULL, worker_thread, NULL);
    }

    //accept connections and add connection's fd to queue
    while (1) {
        uintptr_t connection_fd = listener_accept(&socket);
        //printf("listener accepted fd: %lu\n", connection_fd);
        // if (connection_fd == -1) {
        //     continue;
        // }
        //printf("connection fd is %d\n", connection_fd);
        queue_push(q, (void *) connection_fd);
        connections++;
    }
    return 0;
}
