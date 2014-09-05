/* Request and response structures common to both the client and server code. */

#ifndef REQUEST_H
#define REQUEST_H

#include <stdint.h>

typedef struct {
    char client_ip[16]; /* Holds client IP address in dotted decimal */
    char machine[24]; /* Name of machine on which client is running */
    int32_t client; /* Client number */
    int32_t request; /* Request number of client */
    int32_t incarnation; /* Incarnation number of client’s machine */
    char operation[80]; /* File operation client sends to server */
} request_t;

typedef struct {
	int32_t status;
	int32_t size;
	char result[80];
} response_t;

#endif /* REQUEST_H */
