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


char *host;
char *service;
int port;
int numberOfClients;
int sum;
double rate;
int bad;

double poissonRandomInterarrivalDelay( double r ){
    return (log((double) 1.0 - ((double) rand())/((double) RAND_MAX)))/-r;
}

void *create_producer(void *tid){
    int csock = (int*)tid;

    int probability = random() % 100 + 1;
    if(probability <= bad)
        sleep(SLOW_CLIENT);

    char *initial_message = "PRODUCE\r\n";
    if(write(csock, initial_message, strlen(initial_message)) < 0){
        fprintf(stderr, "Producer client: %s\n", strerror(errno));
		close(csock);
        pthread_exit(NULL);
    }
    
    char response[BUFSIZE];
    int cc = read(csock, response, BUFSIZE);
    if(cc <= 0){
        fprintf(stderr, "Producer client: the server has gone unexpectedly.\n");
        close(csock);
        pthread_exit(NULL);
    }
    response[cc] = '\0';
    if(strcmp(response, "GO\r\n") != 0){
        fprintf(stderr, "Producer client: rejected\n");
        close(csock);
        pthread_exit(NULL);
    }
  
    int size = random() % MAX_LETTERS + 1;
    int con_size = htonl(size);
    if(write(csock, &con_size, sizeof(con_size)) < 0){
        fprintf(stderr, "Producer client: client write: %s\n", strerror(errno) );
        close(csock);
        pthread_exit(NULL);
    }

    printf("Producer client: len is %d\n", size);

    cc = read(csock, response, BUFSIZE);
    if(cc <= 0){
        fprintf(stderr, "Producer client: the server has gone unexpectedly.\n");
        close(csock);
        pthread_exit(NULL);
    }
    response[cc] = '\0';
    if(strcmp(response, "GO\r\n") != 0){
        fprintf(stderr, "Producer client: rejected\n");
        close(csock);
        pthread_exit(NULL);
    }
    char* stream = malloc(sizeof(char) * BUFSIZE);
    int i = 0;
    for(; i < BUFSIZE; i++){	
        stream[i] = random() % 26 + 'a';	
    }

	int cur = 0;
    while(cur < size){
		int step = cur + BUFSIZE <= size ? BUFSIZE : size - cur;    
        cur += step;
        if(write(csock, stream, step) < 0){
			fprintf(stderr, "Producer client: Cannot send data\n");
            free(stream);
			close(csock);
			pthread_exit(NULL);
		}
    }
    
    free(stream); 

    cc = read(csock, response, BUFSIZE);
    if(cc <= 0){
        fprintf(stderr, "Producer client: the server has gone unexpectedly.\n" );
        close(csock);
        pthread_exit(NULL);
    }
    response[cc] = '\0';
    if(strcmp(response, "DONE\r\n") != 0){
        fprintf(stderr, "Producer client: did not allocate an Item\n");
        close(csock);
        pthread_exit(NULL);
    }
    printf("Producer client: sent the item wiht size %d and ended\n", size);

    close(csock);
    pthread_exit(NULL);
}

int main( int argc, char *argv[] ){
	// initialization of argv
    host = "localhost";

    switch (argc){
		case 5:
            service = argv[1];
            numberOfClients = atoi(argv[2]);
            rate = atof(argv[3]);
            bad = atoi(argv[4]);
            break;
		case 6:
			// User provides a port? then use it
			host = argv[1];
            service = argv[2];
            numberOfClients = atoi(argv[3]);
            rate = atof(argv[4]);
            bad = atoi(argv[5]);
			break;
		default:
		    fprintf( stderr, "Invalid arguments \n" );
			exit(-1);
	}

    if(rate <= 0 ){
        fprintf(stderr, "rate is incorrect %lf.\n", rate);
		exit(-1);
    } 

    if(!(bad >= 0 && bad <= 100)){
        fprintf(stderr, "bad is incorrect %d.\n", bad);
		exit(-1);
    } 

    if(numberOfClients > 2000){
        fprintf(stderr, "too many consumers %d.\n", numberOfClients);
		exit(-1);
    }
    

    pthread_t threads[numberOfClients];
  
    int i = 0;
    for(; i < numberOfClients; i++){   
        double wait = poissonRandomInterarrivalDelay(rate) * 1000000;
        usleep(wait);     
        
        int	csock;
        if (( csock = connectsock( host, service, "tcp" )) == 0 )	{
            fprintf( stderr, "Cannot connect to server.\n" );
            exit(-1);
        }
        
        int status = pthread_create(&threads[i], NULL, create_producer, (void*)csock);
        if(status != 0){
            fprintf(stderr, "pthread_create returned error %d.\n", status);
			exit(-1);
        }
    }

    for(i = 0; i < numberOfClients; i++){
        pthread_join(threads[i], NULL);
    }
}
