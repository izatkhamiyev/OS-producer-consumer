#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <pthread.h>
#include "prodcon.h"
#include <math.h>


int main(int argc, char* argv[]){
    char *port;
    char *host = "localhost";
    char *value;

    switch (argc){
		case 3:
			port = argv[1];
			value = argv[2];
            break;
		case 4:
			host = argv[1];
			port = argv[2];
            value = argv[3];
			break;
		default:
			fprintf( stderr, "usage: server [port]\n" );
			exit(-1);
	}

    int csock;
    if (( csock = connectsock( host, port, "tcp" )) == 0 )	{
        fprintf( stderr, "Cannot connect to server.\n" );
        exit(-1);
    }
    char req[100];
    sprintf(req, "STATUS/%s\r\n", value);
    if(write(csock, req, strlen(req)) < 0){
        fprintf(stderr, "Server has gone\n");
        close(csock);
        exit(-1);
    }
    char response[BUFSIZE];
    int cc = read(csock, response, BUFSIZE);
    if(cc <= 0){
        fprintf(stderr, "The server has gone unexpectedly or Incorrect argument.\n");
        close(csock);
        exit(-1);
    }
    response[cc] = '\0';
    printf("%s: %s\n", value, response);
    
    return 0;
}