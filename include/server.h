/* Header file for the primary server file. Contains declarations for all functions defined
   in server.c */

#ifndef SERVER_H
#define SERVER_H

#include <arpa/inet.h>

#include "request.h"
#include "list.h"

typedef enum {
	LOCK_UNLOCKED = 0,
	LOCK_READ = 1,
	LOCK_WRITE = 2
} lock_t;

/* Contains information about a client, such as it's machine, client
number, last request number, last incarnation number, the last response
sent to it, and a list of the statuses of all the files it has open 
(mode and position in the file). */
typedef struct {
	char machine[24];
	int id;
	int last_request;
	int last_incarn;
	response_t *last_response;
	list_t file_states;
} client_t;

/* Contains information about a file, such as the machine name, file
name, whether it is locked, and who holds the locks. If the lock is
a write lock, writeholder will point to the client structure that
holds the lock and readholders will be empty, and if a read lock is
held, writeholder will be null and readholders will contains a list of
all clients holding a read lock (multiple read locks can be held at the
same time). */
typedef struct {
	char machine[24];
	char filename[24];
	lock_t lock;
	client_t *writeholder;
	list_t readholders;
} file_lock_t;

/* Contains the information about a file that a client currently has open,
such as the mode in which the file was opened and the current position
of the client's "cursor" within the file. */
typedef struct {
	file_lock_t *file;
	short mode;
	size_t position;
} file_state_t;

#define LOCK_UN 0
#define LOCK_READ 1
#define LOCK_WRITE 2

int main(int argc, char **argv);

/* Displays an error message and exits the process. */
void fail_with_error(const char *msg);

/* Builds the response to a client request. */
response_t *handle_request(request_t *request);

/* Retrieves the client structure associated with a client or constructs a new one. */
client_t *retrieve_client(request_t *request);

/* Removes all locks held by the specified client. */
void clear_locks(client_t *client);

#endif /* SERVER_H */
