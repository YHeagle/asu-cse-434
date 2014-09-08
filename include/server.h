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
	list_t fstates;
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
} file_entry_t;

/* Contains the information about a file that a client currently has open,
such as the mode in which the file was opened and the current position
of the client's "cursor" within the file. */
typedef struct {
	file_entry_t *file;
	lock_t mode;
	size_t position;
} file_state_t;

/* The entrypoint to the program. Performs network-related functions. */
int main(int argc, char **argv);

/* Displays an error message and exits the process. */
void fail_with_error(const char *msg);

/* Performs application-logic specific initialization. */
void init();

/* Builds the response to a request, or possibly returns a null pointer
if no reponse should be sent. */
response_t *handle_request(request_t *request);

/* Retrieves the client structure associated with a client or constructs
a new one. */
client_t *retrieve_client(request_t *request);

/* Removes all locks held by the specified client. */
void clear_locks(client_t *client);

/* Reads the command contained in a request, then calls the appropriate
function to perform the command. */
response_t *dispatch_request(request_t *request, client_t *client);

/* Performs the open operation. */
response_t *perform_open(request_t *request, client_t *client);

/* Adds an opened file record with the given information to the client. */
void add_fstate(client_t *client, file_entry_t *file, lock_t mode, size_t position);

/* Sets the lock on the given file to the specified client. */
void set_lock(file_entry_t *file, client_t *client, lock_t mode);

/* Performs the close operation. */
response_t *perform_close(request_t *request, client_t *client);

/* Performs the read operation. */
response_t *perform_read(request_t *request, client_t *client);

/* Performs the write operation. */
response_t *perform_write(request_t *request, client_t *client);

/* Performs the lseek operation. */
response_t *perform_lseek(request_t *request, client_t *client);

/* Generates a response with the given status code, and 0 for the
response and response size. Caller's responsibility to deallocate. */
response_t *resp_from_status(int status);

/* Finds the file entry with the given filename and machine name. */
file_entry_t *find_file(char *filename, char*machinename);

/* Checks whether the given client has the given file open with the specified mode. */
char check_open(client_t* client, file_entry_t* file, lock_t mode);

/* Allocates a new file_entry object and copies the filename and
machine provided into the object. */
file_entry_t *new_file(char *filename, char *machine);

/* Computes the filename of the specified file on the local disk,
and opens that file. */
int open_file(file_entry_t *file, int flags, mode_t mode);

#endif /* SERVER_H */
