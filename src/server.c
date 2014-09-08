/* Primary source file for the server. */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "server.h"
#include "request.h"
#include "list.h"

char recv_buffer[sizeof(request_t) + 16];
list_t client_list;
list_t file_list;
response_t invalid_req_resp;

#ifndef TEST
int main(int argc, char** argv)
{
    int sock;
    unsigned short server_port;
    struct sockaddr_in server_address;

    /* Check number of arguments */
    if (argc != 2) {
        fprintf(stderr, "Usage: %s PORT\n", argv[0]);
        exit(1);
    }

    init();

    /* Convert port from string to int. */
    server_port = (unsigned short)atoi(argv[1]);

    /* Create socket for sending and receiving UDP datagrams. */
    printf("INFO: Creating UDP socket.\n");
    if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
        fail_with_error("FATAL: socket() failed");

    /* Build local address. */
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_address.sin_port = htons(server_port);

    /* Bind to the local address. */
    printf("INFO: Binding UDP socket to port %s.\n", argv[1]);
    if (bind(sock, (struct sockaddr*) &server_address, sizeof(server_address)) < 0)
        fail_with_error("FATAL: bind() failed");

    struct sockaddr_in client_address;
    unsigned int client_addr_len = (unsigned int)sizeof(client_address);

    printf("INFO: Listening for requests on port %s.\n", argv[1]);
    for (;;) { /* Loop forever */
        ssize_t message_size;

        /* Block until a message is received. */
        if ((message_size = recvfrom(sock, recv_buffer, sizeof(recv_buffer), 0,
            (struct sockaddr*) &client_address, &client_addr_len)) < 0)
            fail_with_error("FATAL: recvfrom() failed");

        /* Located in a statically allocated buffer, so no need to free. */
        char* client_ip_str = inet_ntoa(client_address.sin_addr);

        /* Print log messages. */
        if (message_size != sizeof(request_t)) {
            /* Received message is invalid. Print a message then ignore and return to listening. */
            printf("ERROR: Invalid request from %s (invalid size).\n", client_ip_str);
        } else {
            printf("INFO: Handling request from %s.\n", client_ip_str);

            /* Handle request */
            response_t *response = handle_request((request_t*) &recv_buffer);
            if (response) {
                if (sendto(sock, response, sizeof(response_t), 0,
                    (struct sockaddr *) &client_address, sizeof(client_address)) != sizeof(response_t))
                    fail_with_error("FATAL: sendto() sent a different number of bytes than expected");
                printf("    INFO: Sent response to %s.\n", client_ip_str);
            }
        }
    }

    /* Never reached */
    return 0;
}
#endif

/* Displays an error message and exits the process. */
void fail_with_error(const char* msg)
{
    perror(msg);
    exit(1);
}

/* Performs application-logic specific initialization. */
void init()
{
    /* Initialize data structures */
    list_init(&client_list);
    list_init(&file_list);

    /* Initialize generic response to invalid requests. */
    memset(&invalid_req_resp, 0, sizeof(response_t));
    invalid_req_resp.status = EINVAL;
}

/* Builds the response to a client request. */
response_t *handle_request(request_t *request)
{
    client_t *client = retrieve_client(request);
    if (!client)
        return (response_t*)0;

    if ((int)request->incarnation != client->last_incarn) {
        printf("    WARNING: Client incarnation number has changed.\n");
        clear_locks(client);
        client->last_incarn = (int)request->incarnation;
    }

    response_t *response;

    if (request->request < client->last_request) {
        /* Request has already been completed. */
        printf("    WARNING: Request has already been completed.\nRequest ignored.\n");
        response = (response_t*)0;

    } else if (request->request == client->last_request) {
        /* Request has already been completed but send stored response. */
        printf("    WARNING: Request has already been completed. Sending stored response.\n");
        response = client->last_response;

    } else {
        /* Request number is higher than previous. This is a new request. */
        printf("    INFO: Request is new.\n");

        /* Generate a random number to decide between the 3 options. */
        /* FIXME */
        //int rnd = rand() % 3;
        int rnd = 2;

        if (rnd == 0) {
            /* Drop the request. */
            printf("    INFO: Dropping the request.\n");
            response = (response_t*)0;

        } else {
            if (rnd == 1) {
                /* Perform the request but drop the response. */
                printf("    INFO: Performing the request but dropping the reply.\n");
            } else {
                /* Perform the request. */
                printf("    INFO: Performing the request.\n");
            }

            /* Perform the request. */
            response = dispatch_request(request, client);
            if (client->last_response)
                free(client->last_response);
            client->last_response = response;

            if (rnd == 1) {
                /* Drop reply. */
                response = (response_t*)0;
            }

        }

        /* Set the last request number. */
        client->last_request = request->request;
    }

    return response;
}

/* Retrieves the client structure associated with a client or constructs a new one. */
client_t *retrieve_client(request_t *request)
{
    char *req_machine = request->machine;
    int req_id = (int)request->client;
    client_t *client;

    char found = 0;
    /* Iterate through all clients in the client table. */
    for (int i = 0, end = client_list.size; i < end; ++i) {
        client = (client_t*)list_at(&client_list, i);

        /* Match on machine name and client number. */
        if (strcmp(req_machine, client->machine) == 0 && req_id == client->id) {
            found = 1;
            printf("    INFO: Found record for machine=\"%s\" and client=%d.\n", req_machine, req_id);
            break;
        }
    }

    if (!found) {
        client = (client_t*)malloc(sizeof(client_t));
        /* Check for a null pointer */
        if (!client)
            return 0;
        memset(client, 0, sizeof(client_t));
        strcpy(client->machine, req_machine);
        client->id = req_id;
        client->last_request = request->request - 1;
        client->last_incarn = request->incarnation;
        client->last_response = (response_t*)0;
        list_append(&client_list, client);
        printf("    INFO: Created new record for machine=\"%s\" and client=%d.\n", req_machine, req_id);
    }

    return client;
}

/* Removes all locks held by the specified client. */
/* Called when the incarnation number for client has incremented. */
void clear_locks(client_t *client)
{
    printf("    INFO: Clearing locks held by machine=\"%s\" and client=%d.\n", client->machine, client->id);

    /* Iterate through all files on the system. */
    for (int i = 0, end = file_list.size; i < end; ++i) {
        file_entry_t *file = list_at(&file_list, i);

        if (file->lock == LOCK_WRITE) {
            /* Check if this client is the one holding the write lock on this file. */
            if (client == file->writeholder) {
                file->lock = LOCK_UNLOCKED;
                file->writeholder = (void*)0;
                printf("    INFO: Cleared write lock on file %s.\n", file->filename);
            }

        } else if (file->lock == LOCK_READ) {
            /* Iterate through all clients that have readlocks on the file. */
            for (int j = 0, endj = file->readholders.size; j < endj; ++j) {
                client_t *readclient = (client_t*)list_at(&(file->readholders), j);

                /* Check if the client is the same as the client we are clearing. */
                if (client == readclient) {
                    /* Remove the readlock that this client holds. */
                    list_remove(&(file->readholders), j);
                    printf("    INFO: Cleared read lock on file %s.\n", file->filename);
                    break;
                }
            }

            /* Check if there are no more read locks remaining. */
            if (file->readholders.size == 0) {
                file->lock = LOCK_UNLOCKED;
            }

        }
    }
}

/* Reads the command contained in a request, then calls the appropriate
function to perform the command. */
response_t *dispatch_request(request_t *request, client_t *client)
{
    /* Scan the command from the operation string. */
    char command[20];
    sscanf(request->operation, "%s", command);
    printf("    INFO: Requested operation => %s\n", request->operation);

    response_t *response;

    if (strcmp(command, "open") == 0) {
        response = perform_open(request, client);
    } else if (strcmp(command, "close") == 0) {
        response = perform_close(request, client);
    } else if (strcmp(command, "read") == 0) {
        response = perform_read(request, client);
    } else if (strcmp(command, "write") == 0) {
        response = perform_write(request, client);
    } else if (strcmp(command, "lseek") == 0) {
        response = perform_lseek(request, client);
    } else {
        /* Received an invalid request. */
        printf("    ERROR: The requested operation is invalid.\n");
        response = resp_from_status(EINVAL);
    }

    return response;
}

/* Performs the open operation. */
response_t *perform_open(request_t *request, client_t* client)
{
    char filename[24];
    char strmode[10];
    response_t *response;

    sscanf(request->operation, "%*s %s %s", filename, strmode);

    file_entry_t *file = find_file(filename, request->machine);

    /* Parse the string mode to the lock_t value. */
    lock_t mode;
    if (strcmp(strmode, "read") == 0) {
        mode = LOCK_READ;
    } else if (strcmp(strmode, "write") == 0) {
        mode = LOCK_WRITE;
    } else if (strcmp(strmode, "readwrite") == 0) {
        mode = LOCK_READ | LOCK_WRITE;
    } else {
        /* Received an invalid value for the mode argument. */
        printf("    ERROR: Received invalid value for the mode argument.\n");
        return resp_from_status(EINVAL);
    }

    if (file) {
        /* Verify that this client does not already have this file open. */
        if (check_open(client, file, LOCK_READ | LOCK_WRITE)) {
            /* Can't open the same file again. */
            printf("    ERROR: Client already has %s open.\n", filename);
            return resp_from_status(EINVAL);
        }

        lock_t lock = file->lock;
        if (lock == LOCK_UNLOCKED) {
            /* File is not opened. Open it and add the appropriate lock. */
            if (mode & LOCK_WRITE) {
                set_lock(file, client, LOCK_WRITE);
                add_fstate(client, file, mode, 0);
            } else {
                set_lock(file, client, LOCK_READ);
                add_fstate(client, file, mode, 0);
            }
            response = resp_from_status(0);

        } else if (lock == LOCK_READ) {
            /* File is already open in read mode. If this request is exclusively for
            read, this request can be performed. */
            if (mode == LOCK_READ) {
                /* Add this client to the list of clients holding a read lock on
                this file. */
                set_lock(file, client, LOCK_READ);
                add_fstate(client, file, LOCK_READ, 0);
                response = resp_from_status(0);

            } else {
                /* File cannot be opened in the requested mode (write or readwrite). */
                response = resp_from_status(EPERM);
            }

        } else {
            /* File has a write lock on it. It cannot be opened in read or write by
            any other client. */
            response = resp_from_status(EPERM);
        }

        if (response->status == 0) {
            /* Request was successful. */
            printf("    INFO: Opened %s in %s mode.\n", filename, strmode);
        } else {
            printf("    ERROR: Existing locks prevent opening %s in %s mode.\n", filename, strmode);
        }

    } else {
        /* File could not be found. If write mode was request create new file,
        otherwise return error. */
        if (mode & LOCK_WRITE) {
            file = new_file(filename, client->machine);
            set_lock(file, client, LOCK_WRITE);
            add_fstate(client, file, mode, 0);

            /* Create file on disk by opening then closing the file. */
            int fd = open_disk_file(file, O_WRONLY | O_CREAT, (mode_t)00644);
            if (close(fd) < 0)
                fail_with_error("FATAL: close() failed");

            response = resp_from_status(0);
            printf("    INFO: Created new file %s in mode %s.\n", filename, strmode);

        } else {
            response = resp_from_status(ENOENT);
            printf("    ERROR: File does not exist and read mode requested.\n");
        }
    }

    return response;
}

/* Adds an opened file record with the given information to the client. */
void add_fstate(client_t *client, file_entry_t *file, lock_t mode, size_t position)
{
    file_state_t *fstate = (file_state_t*)malloc(sizeof(file_state_t));
    fstate->file = file;
    fstate->mode = mode;
    fstate->position = position;
    list_append(&client->fstates, fstate);
}

/* Sets the lock on the given file to the specified client. */
void set_lock(file_entry_t *file, client_t *client, lock_t mode)
{
    if (mode & LOCK_WRITE) {
        file->lock = LOCK_WRITE;
        file->writeholder = client;
    } else {
        file->lock = LOCK_READ;
        list_append(&file->readholders, client);
    }
}

/* Allocates a new file_entry object and copies the filename and
machine provided into the object. */
file_entry_t *new_file(char *filename, char *machine)
{
    file_entry_t *file = (file_entry_t*)malloc(sizeof(file_entry_t));
    strcpy(file->filename, filename);
    strcpy(file->machine, machine);
    list_append(&file_list, file);
    return file;
}

/* Performs the close operation. */
response_t *perform_close(request_t *request, client_t *client)
{
    char filename[24];
    response_t *response;

    sscanf(request->operation, "%*s %s", filename);
    file_entry_t *file = find_file(filename, request->machine);

    if (file) {
        if (check_open(client, file, LOCK_READ | LOCK_WRITE)) {
            /* File exists and client has file open. Operation can be performed.
            Need to remove fstate from client struct and client from holding a
            lock on the file. */
            char found = 0;
            for (int i = 0, end = client->fstates.size; i < end; ++i) {
                file_state_t *fstate = (file_state_t*)list_at(&client->fstates, i);
                if (fstate->file == file) {
                    list_remove(&client->fstates, i);
                    found = 1;
                    break;
                }
            }

            if (!found) {
                printf("FATAL: Internal data structure inconsistency (%s:%d).\n", __FILE__, __LINE__);
                exit(1);
            }

            /* File must either have a read or write lock since this client
            has it open. */
            if (file->lock == LOCK_WRITE) {
                file->writeholder = (client_t*)0;
                file->lock = LOCK_UNLOCKED;

            } else {
                /* File must have a read lock. */
                found = 0;
                for (int i = 0, end = file->readholders.size; i < end; ++i) {
                    client_t *c = (client_t*)list_at(&file->readholders, i);
                    if (c == client) {
                        list_remove(&file->readholders, i);
                        found = 1;
                        break;
                    }
                }

                if (!found) {
                    printf("FATAL: Internal data structure inconsistency (%s:%d).\n", __FILE__, __LINE__);
                    exit(1);
                }

                if (file->readholders.size == 0) {
                    /* We just removed the last client holding a read lock on the file.
                    The file is now unlocked. */
                    file->lock = LOCK_UNLOCKED;
                }
            }

            response = resp_from_status(0);
            printf("Closed %s.\n", file->filename);

        } else {
            /* File exists but client does not have file open. */
            printf("    ERROR: File exists but not opened by client.\n");
            response = resp_from_status(EINVAL);
        }
    } else {
        /* File does not exist. */
        printf("    ERROR: File does not exist.\n");
        response = resp_from_status(ENOENT);
    }

    return response;
}

/* Performs the read operation. */
response_t *perform_read(request_t *request, client_t *client)
{
    char filename[24];
    int numbytes;
    response_t *response;

    sscanf(request->operation, "%*s %s %d", filename, &numbytes);
    file_entry_t *file = find_file(filename, request->machine);

    if (!file) {
        /* File does not exist. */
        printf("    ERROR: File does not exist.\n");
        return resp_from_status(EINVAL);
    }

    /* Check that the client has the file open in the correct
    mode. */
    if (!check_open(client, file, LOCK_READ)) {
        /* File exists but not opened by client (or not in right mode). */
        printf("    ERROR: Client does not have file open in correct mode.\n");
        return resp_from_status(EINVAL);
    }

    /* Check if number of bytes to be read is valid. */
    if (numbytes <= 0 || numbytes > 80) {
        printf("    ERROR: Invalid number of bytes to read (%d).\n", numbytes);
        return resp_from_status(EINVAL);
    }
    /* Everything is correct, we can perform the read.
    mode argument is not needed. */
    int fd = open_disk_file(file, O_RDONLY, 0);
    
    file_state_t *fstate = find_fstate(client, file);
    if (fstate->position != 0)
        lseek(fd, fstate->position, SEEK_SET);

    response = resp_from_status(0);
    size_t size = read(fd, &response->result, numbytes);
    if (size < 0) {
        response->status = errno;
        response->size = 0;
    } else {
        response->size = size;
    }

    size_t position = lseek(fd, 0, SEEK_CUR);
    if (position < 0)
        fail_with_error("FATAL: lseek() failed");

    fstate->position = position;

    if (close(fd) < 0)
        fail_with_error("FATAL: close() failed");

    printf("    INFO: Performed read.\n");
    return response;
}

/* Performs the write operation. */
response_t *perform_write(request_t *request, client_t *client)
{
    char filename[24];
    char data[80];
    memset(data, 0, sizeof(data));
    response_t *response;

    sscanf(request->operation, "%*s %s %80c", filename, data);
    file_entry_t *file = find_file(filename, request->machine);

    if (!file) {
        /* File does not exist. */
        printf("    ERROR: File does not exist.\n");
        return resp_from_status(EINVAL);
    }

    /* Check that the client has the file open in the correct
    mode. */
    if (!check_open(client, file, LOCK_WRITE)) {
        /* File exists but not opened by client (or not in right mode). */
        printf("    ERROR: Client does not have file open in correct mode.\n");
        return resp_from_status(EINVAL);
    }

    /* Everything is correct, we can perform the write.
    mode argument is not needed. */
    int fd = open_disk_file(file, O_WRONLY, 0);
    
    file_state_t *fstate = find_fstate(client, file);
    if (fstate->position != 0)
        lseek(fd, fstate->position, SEEK_SET);

    response = resp_from_status(0);
    size_t size = write(fd, data, strlen(data));
    if (size < 0) {
        response->status = errno;
        response->size = 0;
    } else {
        response->size = size;
    }

    size_t position = lseek(fd, 0, SEEK_CUR);
    if (position < 0)
        fail_with_error("FATAL: lseek() failed");

    fstate->position = position;

    if (close(fd) < 0)
        fail_with_error("FATAL: close() failed");

    printf("    INFO: Performed write.\n");
    return response;
}

/* Performs the lseek operation. */
response_t *perform_lseek(request_t *request, client_t *client)
{
    char filename[24];
    int position;

    sscanf(request->operation, "%*s %s %d", filename, &position);
    file_entry_t *file = find_file(filename, request->machine);

    if (!file) {
        /* File does not exist. */
        printf("    ERROR: File does not exist.\n");
        return resp_from_status(EINVAL);
    }

    /* Check that the client has the file open in the correct
    mode. */
    if (!check_open(client, file, LOCK_WRITE)) {
        /* File exists but not opened by client (or not in right mode). */
        printf("    ERROR: Client does not have file open in correct mode.\n");
        return resp_from_status(EINVAL);
    }

    /* Don't actually open file, just change the position recorded
    in the client's fstates table. */

    file_state_t *fstate = find_fstate(client, file);
    fstate->position = position;

    printf("    INFO: Performed lseek.\n");
    return resp_from_status(0);
}

/* Generates a response with the given status code, and 0 for the
response and response size. Caller's responsibility to deallocate. */
response_t *resp_from_status(int status)
{
        response_t *response = (response_t*)calloc(1, sizeof(response_t));
        response->status = (int32_t)status;
        return response;
}

/* Finds the file entry with the given filename and machine name. */
file_entry_t *find_file(char *filename, char *machine)
{
    char found = 0;
    file_entry_t *file;
    /* Iterate through all file entries. */
    for (int i = 0, end = file_list.size; i < end; ++i) {
        file = list_at(&file_list, i);

        if (strcmp(machine, file->machine) == 0 &&
            strcmp(filename, file->filename) == 0) {
            found = 1;
            break;
        }
    }

    if (found)
        return file;
    else
        return (file_entry_t*)0;
}

/* Checks whether the given client has the given file open with the specified mode. */
char check_open(client_t *client, file_entry_t *file, lock_t mode)
{
    file_state_t *fstate = find_fstate(client, file);
    if (fstate && (fstate->mode & mode))
        return 1;
    else
        return 0;
}

/* Computes the filename of the specified file on the local disk,
and opens that file. */
int open_disk_file(file_entry_t *file, int flags, mode_t mode)
{
    char disk_filename[50];
    strcpy(disk_filename, file->machine);
    strcat(disk_filename, ":");
    strcat(disk_filename, file->filename);

    int fd = open(disk_filename, flags, mode);
    if (!fd)
        fail_with_error("FATAL: open() failed");

    return fd;
}

/* Finds the record for the client's file state for the given
file. */
file_state_t *find_fstate(client_t *client, file_entry_t *file)
{
    for (int i = 0, end = client->fstates.size; i < end; ++i) {
        file_state_t *fstate = (file_state_t*)list_at(&client->fstates, i);
        if (file == fstate->file) {
            return fstate;
        }
    }

    return (file_state_t*)0;
}
