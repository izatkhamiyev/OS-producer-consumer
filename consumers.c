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
#include <fcntl.h>
#include "prodcon.h"
#include <math.h>

char *host;
char *service;
int port;
int numberOfClients;
double rate;
int bad;

double poissonRandomInterarrivalDelay( double r ){
    return (log((double) 1.0 - ((double) rand())/((double) RAND_MAX)))/-r;
}

int create_response_file(){
    long long thread_id = pthread_self();

    char path[100];
    sprintf(path, "%lld", thread_id);
    int len = strlen(path);
    strcat(path, ".txt");
    int fid = open(path, O_WRONLY | O_CREAT, 0644);

    return fid;
}

void *create_consumer(void *tid){
    int csock = (int*)tid;
    
    int probability = random() % 100 + 1;
    if(probability <= bad)
        sleep(SLOW_CLIENT);


    char *initial_message = "CONSUME\r\n";
    if(write(csock, initial_message, strlen(initial_message)) < 0){
        fprintf(stderr, "client write: %s\n", strerror(errno) );
        int fid = create_response_file();
        if(write(fid, REJECT, strlen(REJECT)) < 0){
            fprintf(stderr, "Consumer client: Cannot create or write file\n");
        }
        close(fid);
		close(csock);
        pthread_exit(NULL);
    }

    int size;    
    int cc = read(csock, &size, sizeof(size));
    size = ntohl(size);
    if(cc <= 0){
        printf( "Consumer client: The server has gone unexpectedly.\n" );
        int fid = create_response_file();
        if(write(fid, REJECT, strlen(REJECT)) < 0){
            fprintf(stderr, "Consumer client: Cannot create or write file\n");
        }
        close(fid);
        close(csock);
        pthread_exit(NULL);
    }

    int dev_null_fd = open("/dev/null", O_WRONLY);
    cc = 0;
	int cur = 0;
    while(cur < size){
		int step = BUFSIZE;
		char* buff = malloc(sizeof(char) * step);
        if((cc = read(csock, buff, step)) <= 0){
			fprintf(stderr, "Consumer client: The server has gone unexpectedly.\n");
			close(csock);
            pthread_exit(NULL);
		}
		cur += cc;
    
        if(write(dev_null_fd, buff, cc) < 0){
            fprintf(stderr, "Consumer client: Cannot stream to file\n");
        }
        free(buff);
    }


    int fid = create_response_file();

    if(size != cur){
        if(write(fid, BYTE_ERROR, strlen(BYTE_ERROR)) < 0){
            fprintf(stderr, "Consumer client: Cannot create or write file\n");
        }
    } else if(size == cur) {
        if(write(fid, SUCCESS, strlen(SUCCESS)) < 0){
            fprintf(stderr, "Consumer client: Cannot create or write file\n");
        }
    }
    printf("Consumer client: The file is written with size %d\n", size);
 
    close(csock);
    close(dev_null_fd);
    close(fid);
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
        
        int status = pthread_create(&threads[i], NULL, create_consumer,  (void*) csock);
        if(status != 0){
            fprintf(stderr, "pthread_create returned error %d.\n", status);
			exit(-1);
        }
    }
    for(i = 0; i < numberOfClients; i++){
        pthread_join(threads[i], NULL);
    }
}
