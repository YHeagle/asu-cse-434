/* Primary source file for the client. */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>

#include "client.h"
#include "request.h"

int main(int argc, char **argv)
{
	/* Check number of arguments. */
	if (argc != 7) {
		fprintf(stderr, "usage: %s <SERVER IP> <MACHINE> <CLIENT ID> <SERVER PORT> <OPERATION> <REQUEST>\n", argv[0]);
		exit(1);
	}

	int sock;
	if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
		fail_with_error("socket() failed");

	/* Construct the server address */
	struct sockaddr_in server_address;
	memset(&server_address, 0, sizeof(server_address));
	server_address.sin_family = AF_INET;
	server_address.sin_addr.s_addr = inet_addr(argv[1]);
	server_address.sin_port = htons(atoi(argv[4]));

	request_t request;
	strcpy(request.machine, argv[2]);
	strcpy(request.operation, argv[5]);
	request.client = atoi(argv[3]);
	request.request = atoi(argv[6]);

	if (sendto(sock, &request, sizeof(request_t), 0, (struct sockaddr *)
               &server_address, sizeof(server_address)) != sizeof(request_t))
        fail_with_error("sendto() sent a different number of bytes than expected");
}

void fail_with_error(const char* msg)
{
    perror(msg);
    exit(1);
}
