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
char* commands[10];
char* reply_messages[10];

void prepare_data(){
    int i = 1; 
    for(; i < 10; i++){
        commands[i] = malloc(sizeof(char) * 10);
        reply_messages[i] = malloc(sizeof(char) * 100);
    }
    strcpy(commands[1], "CURRCLI");
    strcpy(reply_messages[1], "Current clients");
    strcpy(commands[2], "CURRPROD");
    strcpy(reply_messages[2], "Current producers");
    strcpy(commands[3], "CURRCONS");
    strcpy(reply_messages[3], "Current consumers");
    strcpy(commands[4], "TOTPROD");
    strcpy(reply_messages[4], "Total producers served");
    strcpy(commands[5], "TOTCONS");
    strcpy(reply_messages[5], "Total consumers served");
    strcpy(commands[6], "REJMAX"); 
    strcpy(reply_messages[6], "Total clients rejected for MAX_CLIENTS");
    strcpy(commands[7], "REJSLOW");
    strcpy(reply_messages[7], "Total slow clients rejected");
    strcpy(commands[8], "REJPROD"); 
    strcpy(reply_messages[8], "Total producers rejected for MAX_PROD");
    strcpy(commands[9], "REJCONS");  
    strcpy(reply_messages[9], "Total consumers rejected for MAX_CONS");
}

void clear_data(){
    int i = 1; 
    for(; i < 10; i++){
        free(commands[i]);
        free(reply_messages[i]);
    }
}

void welcome_user(){
    printf("\n##############################################\n");
    printf("\nMAIN MENU\n");
    int i = 1;
    for(; i < 10; i++){
        printf("%d = %s\t", i, commands[i]);
        if(i == 5)
            printf("\n");
    }
    printf("q = quit\t\n");
}

int main(int argc, char* argv[]){
    char *port;
    char *host = "localhost";

    switch (argc){
		case 2:
			port = argv[1];
			break;
		case 3:
            host = argv[1];
			port = argv[2];
			break;
		default:
			fprintf( stderr, "usage: server [port]\n" );
			exit(-1);
	}

    prepare_data();
    while(1){
        welcome_user();
        
        char respond[100];
        printf("\nSelect the value to check: ");
        scanf("%s", respond);

        if(strcmp(respond, "q") == 0)
            break;
        int value = atoi(respond);
        if(value == 0 || value >= 10){
            printf("Incorrect value: %s\n", respond);
            continue;
        }
        int csock;
        if (( csock = connectsock( host, port, "tcp" )) == 0 )	{
		    fprintf( stderr, "Cannot connect to server.\n" );
            break;
        }
        char req[100];
        sprintf(req, "STATUS/%s\r\n", commands[value]);
        if(write(csock, req, strlen(req)) < 0){
            fprintf(stderr, "Server has gone\n");
		    close(csock);
            break;
        }
        char response[BUFSIZE];
        int cc = read(csock, response, BUFSIZE);
        if(cc <= 0){
            fprintf(stderr, "The server has gone unexpectedly.\n");
            close(csock);
            break;
        }
        response[cc] = '\0';
        printf("%s: %s\n", reply_messages[value], response);
    }

    clear_data();

    return 0;
}