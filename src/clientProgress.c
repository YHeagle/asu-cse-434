// William Skylor Haselwood
// Nathaniel Flick
// Arizona State University - CSE 434 Computer Networks — Fall 2014
// Instructor: Dr. Violet R. Syrotiuk
// Two-Person Team Socket Programming Project

#include <stdio.h>                  /* for printf() and fprintf() */
#include <sys/socket.h>             /* for socket(), connect(), sendto(), and recvfrom() */
#include <arpa/inet.h>              /* for sockaddr_in and inet_addr() */
#include <stdlib.h>                 /* for atoi() and exit() */
#include <string.h>                 /* for memset() */
#include <unistd.h>                 /* for close() */
#include </home/Skylor/client.h>    /* for client structures */
#include </home/Skylor/request.h>   /* for request structures */
#include <sys/time.h>               /* for timeout */
#include <fcntl.h>                  /* for locking and unlocking files */

#define FileLockMAX 255             /* Longest string to FileLock */


int main(int argc, char **argv) 
{

    int respStringLen;                      /* Length of received response */
    char FileLockBuffer[FileLockMAX+1];     /* Buffer for receiving FileLocked string */



    // Tests for correct number of arguments
    if (argc != 6) 
    {
        fprintf(stderr, "usage: %s <SERVER IP> <MACHINE NAME> <CLIENT ID> <SERVER PORT> <SCRIPT FILE>\n", argv[0]);
        exit(1);
    }



    // Initializes socket
    int sock;
    if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
        fail_with_error("socket() failed");



    // Constructs the server address
    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = inet_addr(argv[1]);
    server_address.sin_port = htons(atoi(argv[4]));

/*
    // Request initializer
    request_t request;
    strcpy(request.machine, argv[2]);
    strcpy(request.operation, argv[5]);
    request.client = atoi(argv[3]);
    request.request = atoi(argv[6]);        // Program fails here
*/

    // Timeout structure for server response
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 100000;



    // Initialize script and incarnation file
    FILE *Script;


    // Open script
    Script = fopen(argv[5],"r");
    // Open incarnation file
    char *incarnationFile = "/home/Skylor/incarnationFile.txt";


    // Initialize variables to be stored from script
    char    command[100];       // Script command
    char    filename[80];       // Filename associated with script command
    char    mode[40];           // Mode of the open operation
    char    bytes[10];          // Number of bytes associated with certain script commands
    char    string[100];        // String associated with certain script command
    char    operation[200];     // Full operation code
    int32_t requestNumber;      // Keeps track of the number of processed requests
    unsigned int fromSize;      // In-out of address size for recvfrom()
    int FileLockStringLen;      // Length of string to FileLock
    int incarnationNum = 0;      // Incarnation Number holder
   

    // Create lock/unlocking structure for files
    int fd;                     // file descriptor
    struct flock lock;
    char line[100];             // read line from script file

  
    // Split string into tokens
    int stringWord = 0;     // Keeps track of which word is being read in the string 
    char * parse;           // Holds the next word in the string


    // Reads script file to send commands to the server
    for (requestNumber = 0; !feof(Script); requestNumber++)
    {
            fgets(line,sizeof(line),Script);
            printf("Command Read: %s\n", line);

            // Split string into tokens
            stringWord = 0;     // Resets stringWord

            parse = strtok (line, " ,-");
            while (parse != NULL)
            {

                if (stringWord == 0)
                {
                    printf("%s\n",parse);
                    strcpy(command,parse);
                    parse = strtok (NULL, " ,-");
                }

                if (stringWord == 1)
                {
                    printf("%s\n",parse);
                    strcpy(filename,parse);
                    parse = strtok (NULL, " ,-");
                }

                if (stringWord == 2)
                {
                    printf("%s\n",parse);

                    if (strcmp(command,"open") == 0)
                    {
                        strcpy(mode,parse);
                    }
                    if (strcmp(command,"read") == 0)
                    {
                        strcpy(bytes,parse);
                    }
                    if (strcmp(command,"write") == 0)
                    {
                        strcpy(string,parse);
                    }
                    if (strcmp(command,"lseek") == 0)
                    {
                        strcpy(bytes,parse);
                    }

                    parse = strtok (NULL, " ,-");

                }

                stringWord ++;
            }

            printf("command= %s, file= %s, mode= %s\n",command,filename,mode);

            // Creates a request
            request_t request;
            strcpy(request.machine, argv[2]);
            strcpy(request.operation, command);
            request.client = atoi(argv[3]);
            request.request = requestNumber;


            if (strcmp(command,"fail") == 0)
            {
                // Lock the file
                fd = open(incarnationFile, O_WRONLY);
                printf("Locking File... \n");

                // Initialize the flock structure
                memset(&lock,0,sizeof(lock));
                lock.l_type = F_WRLCK;

                // Place a write lock on the file
                fcntl(fd,F_SETLKW,&lock);
                printf("File Locked. \n");

                // Write to file to simulate client failure
                fscan(fd,"%d",&incarnationNum);
                incarnationNum ++;
                fprint(fd,"%d",&incarnationNum);

                //unlock the file
                printf("File Unlocking... \n");
                lock.l_type = F_UNLCK;
                cntl(incarnationFile, F_SETLKW,&lock);
                printf("File Unlocked. \n");
            }


            // Sends the request and tuple to the server
            if (strcmp(command,"fail") == 1)
            {
               
                if (sendto(sock, &request, sizeof(request_t), 0, (struct sockaddr *)
                 &server_address, sizeof(server_address)) != sizeof(request_t))
                fail_with_error("sendto() sent a different number of bytes than expected");

                // Recieves a response from the server
                fromSize = sizeof(server_address);
                if ((respStringLen = recvfrom(sock, FileLockBuffer, FileLockMAX, 0, 
                        (struct sockaddr *) &server_address, &fromSize)) != FileLockStringLen)
                    fail_with_error("recvfrom() failed");

                // Null-terminates the received data
                FileLockBuffer[respStringLen] = '\0';
                printf("Received: %s\n", FileLockBuffer);    /* Print the FileLocked arg */


                // Time out for server response
                if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0)
                { 
                    perror("Error: Server response timed Out..");
                }
            }

        }


    close(sock);
    fclose(Script);
    close(fd);
    exit(0);

} // End of program




// External error handeling function
void fail_with_error(const char* msg)
{
    perror(msg);
    exit(1);
}
