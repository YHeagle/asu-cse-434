/* Primary source file for the server. */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "server.h"
#include "request.h"
#include "list.h"

char recv_buffer[sizeof(request_t) + 16];
list_t client_list;
list_t file_list;

#ifndef TEST
int main(int argc, char** argv)
{
    int sock;
    unsigned short server_port;
    struct sockaddr_in server_address;

    /* Check number of arguments */
    if (argc != 2) {
        fprintf(stderr, "usage: %s PORT\n", argv[0]);
        exit(1);
    }

    /* Convert port from string to int. */
    server_port = (unsigned short)atoi(argv[1]);

    /* Create socket for sending and receiving UDP datagrams. */
    printf("Creating UDP socket\n");
    if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
        fail_with_error("socket() failed");

    /* Build local address. */
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_address.sin_port = htons(server_port);

    /* Bind to the local address. */
    printf("Binding UDP socket to port %s\n", argv[1]);
    if (bind(sock, (struct sockaddr*) &server_address, sizeof(server_address)) < 0)
        fail_with_error("bind() failed");

    struct sockaddr_in client_address;
    unsigned int client_addr_len = (unsigned int)sizeof(client_address);

    /* Initialize data structures */
    list_init(&client_list);
    list_init(&file_list);

    printf("Listening for requests on port %s\n", argv[1]);
    for (;;) { /* Loop forever */
        ssize_t message_size;

        /* Block until a message is received. */
        if ((message_size = recvfrom(sock, recv_buffer, sizeof(recv_buffer), 0,
            (struct sockaddr*) &client_address, &client_addr_len)) < 0)
            fail_with_error("recvfrom() failed");

        /* Located in a statically allocated buffer, so no need to free. */
        char* client_ip_str = inet_ntoa(client_address.sin_addr);

        /* Print log messages. */
        if (message_size != sizeof(request_t)) {
            /* Received message is invalid. Print a message then ignore and return to listening. */
            printf("Invalid request from %s (invalid size)\n", client_ip_str);
        } else {
            printf("Handling request from %s\n", client_ip_str);

            /* Handle request */
            response_t *response = handle_request((request_t*) &recv_buffer);
            if (response) {
                if (sendto(sock, response, sizeof(response_t), 0, 
                    (struct sockaddr *) &client_address, sizeof(client_address)) != sizeof(response_t))
                    fail_with_error("sendto() sent a different number of bytes than expected");
                printf("Sent response to %s\n", client_ip_str);
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

/* Builds the response to a client request. */
response_t *handle_request(request_t *request)
{
    client_t *client = retrieve_client(request);
    if (!client)
        return (response_t*)0;

    if ((int)request->incarnation != client->last_incarn) {
        printf("Client incarnation number has changed\n");
        clear_locks(client);
        client->last_incarn = (int)request->incarnation;
    }

    if (request->request < client->last_request) {
        /* Request has already been completed. */
        printf("Request has already been completed\nRequest ignored\n");
        return (response_t*)0;
    
    } else if (request->request == client->last_request) {
        /* Request has already been completed but send stored response. */
        printf("Request has already been completed.\nSending stored response\n");
        return client->last_response;

    } else {
        /* Request number is higher than previous. This is a new request. */

        client->last_request = request->request;
    }
    
    /* FIXME */
    return (response_t*)0;
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
            printf("Found record for machine=\"%s\" and client=%d\n", req_machine, req_id);
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
        client->last_request = request->request;
        client->last_incarn = request->incarnation;
        client->last_response = (response_t*)0;
        list_append(&client_list, client);
        printf("Created new record for machine=\"%s\" and client=%d\n", req_machine, req_id);
    }

    return client;
}

/* Removes all locks held by the specified client. */
/* Called when the incarnation number for client has incremented. */
void clear_locks(client_t *client)
{
    printf("Clearing locks held by machine=\"%s\" and client=%d\n", client->machine, client->id);

    /* Iterate through all files on the system. */
    for (int i = 0, end = file_list.size; i < end; ++i) {
        file_lock_t *file = list_at(&file_list, i);

        if (file->lock == LOCK_WRITE) {
            /* Check if this client is the one holding the write lock on this file. */
            if (client == file->writeholder) {
                file->lock = LOCK_UN;
                file->writeholder = (void*)0;
                printf("Cleared write lock on file %s\n", file->filename);
            }

        } else if (file->lock == LOCK_READ) {
            /* Iterate through all clients that have readlocks on the file. */
            for (int j = 0, endj = file->readholders.size; j < endj; ++j) {
                client_t *readclient = (client_t*)list_at(&(file->readholders), j);

                /* Check if the client is the same as the client we are clearing. */
                if (client == readclient) {
                    /* Remove the readlock that this client holds. */
                    list_remove(&(file->readholders), j);
                    printf("Cleared read lock on file %s\n", file->filename);
                    break;
                }
            }

            /* Check if there are no more read locks remaining. */
            if (file->readholders.size == 0) {
                file->lock = LOCK_UN;
            }

        }
    }
}
