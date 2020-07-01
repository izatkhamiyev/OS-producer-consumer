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
#include <semaphore.h>
#include <sys/select.h>
#include <sys/time.h>
#include "prodcon.h"


#define PRODUCER 0
#define CONSUMER 1
#define UNKNOWN 2

ITEM **buffer;
int count = 0;
int producer_count = 0;
int consumer_count = 0;
int client_count = 0;

int producer_served_count = 0;
int consumer_served_count = 0;
int client_reject_slow_count = 0;
int client_reject_max_count = 0;
int producer_reject_count = 0;
int consumer_reject_count = 0;
int tot_cons = 0;

fd_set afds;
fd_set rfds;
int nfds;


pthread_mutex_t mutex, count_mutex;
sem_t empty, full;


void close_socket(int sock, int type){
	pthread_mutex_lock(&count_mutex);
	if(type == PRODUCER){
		producer_count--;
	}
	if(type == CONSUMER){
		consumer_count--;
	}
	client_count--;
	pthread_mutex_unlock(&count_mutex);
	close(sock);
}


void *producer(void* tid){
	int ssock = (int*)tid;
	if (write(ssock, "GO\r\n", 4) < 0 ){
		fprintf(stderr, "Cannot respond with go to producer\n");
		close_socket(ssock, PRODUCER);
		pthread_exit(NULL);
	}

	int size;
	int cc;
	char buf[4096];
	if((cc = read(ssock, &size, sizeof(size))) <= 0){
		fprintf(stderr,"The client has gone.\n" );
		close_socket(ssock, PRODUCER);
		pthread_exit(NULL);
	}
	size = ntohl(size);
	printf("Received len is %d \n", size);
	

	ITEM *item = malloc(sizeof(ITEM));
	item->size = size;
	item->psd = ssock;

	sem_wait(&empty);
	pthread_mutex_lock(&mutex);
	buffer[count] = item;
	count++;
	pthread_mutex_unlock(&mutex);
	sem_post(&full);

	pthread_mutex_lock(&count_mutex);
	producer_served_count++;
	pthread_mutex_unlock(&count_mutex);

	pthread_exit(NULL);
}

void *consumer(void* tid){
	int ssock = (int*)tid;

	sem_wait(&full);
	pthread_mutex_lock(&mutex);
	ITEM *item = buffer[count - 1];
	buffer[count - 1] = NULL;
	count--;
	pthread_mutex_unlock(&mutex);
	sem_post(&empty);

	pthread_mutex_lock(&count_mutex);
	consumer_served_count++;
	pthread_mutex_unlock(&count_mutex);

	int con_size = htonl(item->size);
	if (write(ssock, &con_size, sizeof(con_size)) < 0 ){
		fprintf(stderr, "Cannot respond to consumer\n");
		close_socket(ssock, CONSUMER);
		close_socket(item->psd, PRODUCER);
		pthread_exit(NULL);
	}

	if (write(item->psd, "GO\r\n", 4) < 0 ){
		fprintf(stderr, "Cannot respond with go to producer");
		close_socket(item->psd, PRODUCER);
		close_socket(ssock, CONSUMER);
		pthread_exit(NULL);
	}
	
	int cc = 0;
	int cur = 0;
    while(cur < item->size){
		int step = BUFSIZE;
		char* buff = malloc(sizeof(char) * step);
        if((cc = read(item->psd, buff, step)) <= 0){
			fprintf(stderr, "The client has gone\n" );
			close_socket(item->psd, PRODUCER);
			close_socket(ssock, CONSUMER);
			pthread_exit(NULL);
		}
		cur += cc;
		if(write(ssock, buff, cc) < 0){
			fprintf(stderr, "Cannot respond to consumer\n");
			close_socket(item->psd, PRODUCER);
			close_socket(ssock, CONSUMER);
			pthread_exit(NULL);
		}
		free(buff);
    }

	printf("Sent item with len: %d \n", item->size);

	if (write(item->psd, "DONE\r\n", 6) < 0 ){
		fprintf(stderr, "Cannot respond with done to producer");
	}

	close_socket(ssock, CONSUMER);
	close_socket(item->psd, PRODUCER);

	free(item);
	pthread_exit(NULL);
}


void statusClient(int ssock, char* command){
	int res;
	if(strstr(command, "CURRCLI") != NULL){
		pthread_mutex_lock(&count_mutex);
		res = client_count;
		pthread_mutex_unlock(&count_mutex);
	} else if(strstr(command, "CURRPROD") != NULL){
		pthread_mutex_lock(&count_mutex);
		res = producer_count;
		pthread_mutex_unlock(&count_mutex);
	} else if(strstr(command, "CURRCONS") != NULL){
		pthread_mutex_lock(&count_mutex);
		res = consumer_count;
		pthread_mutex_unlock(&count_mutex);
	} else if(strstr(command, "TOTPROD") != NULL){
		pthread_mutex_lock(&count_mutex);
		res = producer_served_count;
		pthread_mutex_unlock(&count_mutex);
	} else if(strstr(command, "TOTCONS") != NULL){
		pthread_mutex_lock(&count_mutex);
		res = consumer_served_count;
		pthread_mutex_unlock(&count_mutex);
	} else if(strstr(command, "REJMAX") != NULL){
		pthread_mutex_lock(&count_mutex);
		res = client_reject_max_count;
		pthread_mutex_unlock(&count_mutex);
	} else if(strstr(command, "REJSLOW") != NULL){
		pthread_mutex_lock(&count_mutex);
		res = client_reject_slow_count;
		pthread_mutex_unlock(&count_mutex);
	} else if(strstr(command, "REJPROD") != NULL){
		pthread_mutex_lock(&count_mutex);
		res = producer_reject_count;
		pthread_mutex_unlock(&count_mutex);
	} else if(strstr(command, "REJCONS") != NULL){
		pthread_mutex_lock(&count_mutex);
		res = consumer_reject_count;
		pthread_mutex_unlock(&count_mutex);
	} else {
		printf("Incorrect command \n");
		fflush(stdout);
		return;
	}
	char response[100];
    sprintf(response, "%d\r\n", res);
    int len = strlen(response);
	write(ssock, response, len);
}

int main(int argc, char *argv[] ){
	char *service;
	struct sockaddr_in fsin;
	int	msock;
	int	rport = 0;
	int buffersize = 0;
	
	pthread_t thread;
	
	switch (argc){
		case 2:
			// No args? let the OS choose a port and tell the user
			rport = 1;
			buffersize = atoi(argv[1]);
			break;
		case 3:
			// User provides a port? then use it
			service = argv[1];
			buffersize = atoi(argv[2]);
			break;
		default:
			fprintf( stderr, "usage: server [port]\n" );
			exit(-1);
	}

	buffer = malloc(sizeof(ITEM*) * buffersize);
	

	pthread_mutex_init(&mutex, NULL);
	pthread_mutex_init(&count_mutex, NULL);
	
	sem_init(&full, 0, 0);
	sem_init(&empty, 0, buffersize);

	msock = passivesock( service, "tcp", QLEN, &rport );
	if (rport){
		printf( "server: port %d\n", rport );	
		fflush( stdout );
	}

	nfds = msock + 1;
	FD_ZERO(&afds);
	FD_SET(msock, &afds);

	// wake up every 0.5 seconds
	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 500000;

	struct timeval socket_accept_time[4096];
	
	while(1){
		memcpy((char*)&rfds, (char*)&afds, sizeof(rfds));
		
		if(select(nfds, &rfds, NULL, NULL, &tv) < 0){
			fprintf( stderr, "server select: %s\n", strerror(errno));
			exit(-1);
		}

		if(FD_ISSET(msock, &rfds)){
			int alen = sizeof(fsin);
			int	ssock = accept( msock, (struct sockaddr *)&fsin, &alen );
			if (ssock < 0){
				fprintf( stderr, "accept: %s\n", strerror(errno) );
				exit(-1);
			}
			pthread_mutex_lock(&count_mutex);
			int accept_client = client_count < MAX_CLIENTS ? 1 : 0;
			pthread_mutex_unlock(&count_mutex);
				
			if(accept_client){
				pthread_mutex_lock(&count_mutex);
				client_count++;
				pthread_mutex_unlock(&count_mutex);
				
				FD_SET(ssock, &afds);
				if(ssock + 1 > nfds){
					nfds = ssock + 1;
				}
				gettimeofday(&socket_accept_time[ssock], NULL);
			}else{
				printf("Reject client\n");
				pthread_mutex_lock(&count_mutex);
				client_reject_max_count++;
				pthread_mutex_unlock(&count_mutex);
				close(ssock);
			}		
		}

		int fd;
		for(fd = nfds - 1; fd >= 0; fd--){
			if(fd != msock && !FD_ISSET(fd, &rfds) && FD_ISSET(fd, &afds)){
				struct timeval now;
				gettimeofday(&now, NULL);
				long micros = (now.tv_sec * 1000000 + now.tv_usec) 
						- (socket_accept_time[fd].tv_sec * 1000000 + socket_accept_time[fd].tv_usec);
				if(micros > 1000000 * REJECT_TIME){
					printf("Reject slow client\n");
					fflush(stdout);
					
					pthread_mutex_lock(&count_mutex);
					client_reject_slow_count++;
					pthread_mutex_unlock(&count_mutex);
					
					close_socket(fd, UNKNOWN);
					FD_CLR(fd, &afds);
					if(nfds == fd + 1){
						nfds--;
					}	
				}
			}
			else if(fd != msock && FD_ISSET(fd, &rfds)){
				int	cc;
				char buf[BUFSIZE];
				if ((cc = read( fd, buf, BUFSIZE )) <= 0 ){
					printf("The client has gone. \n" );
					close_socket(fd, UNKNOWN);
				} else {
					buf[cc] = '\0';
					if(strcmp(buf, "CONSUME\r\n") == 0){
						pthread_mutex_lock(&count_mutex);
						tot_cons++;
						pthread_mutex_unlock(&count_mutex);
						pthread_mutex_lock(&count_mutex);
						int can_consume = consumer_count < MAX_CON ? 1 : 0;
						consumer_count++;
						pthread_mutex_unlock(&count_mutex);
						if(can_consume){
							int ssock = fd;
							int status = pthread_create(&thread, NULL, consumer, (void*) ssock);
							if (status != 0 ){
								fprintf(stderr, "pthread_create returned error %d.\n", status );
								exit(-1);
							}
						}else{
							printf("Reject consumer \n");
							pthread_mutex_lock(&count_mutex);
							consumer_reject_count++;
							pthread_mutex_unlock(&count_mutex);
							close_socket(fd, CONSUMER);
						}

					}else if(strcmp(buf, "PRODUCE\r\n") == 0){
						pthread_mutex_lock(&count_mutex);
						int can_produce = producer_count < MAX_PROD ? 1 : 0;
						producer_count++;
						pthread_mutex_unlock(&count_mutex);
						if(can_produce){							
							int ssock = fd;
							int status = pthread_create(&thread, NULL, producer, (void*) ssock);
							if (status != 0 ){
								fprintf(stderr, "pthread_create returned error %d.\n", status );
								exit( -1 );
							}
						}else{
							printf("Reject producer \n");
							pthread_mutex_lock(&count_mutex);
							producer_reject_count++;
							pthread_mutex_unlock(&count_mutex);
							close_socket(fd, PRODUCER);
						}
					} else if(strstr(buf, "STATUS") != NULL){
						printf("Status requested\n");
						statusClient(fd, buf);
						close_socket(fd, UNKNOWN);
					} else {
						printf("Unknown client: close the connection \n");
						fflush(stdout);
						close_socket(fd, UNKNOWN);
					}	
				}
				FD_CLR(fd, &afds);
				if(nfds == fd + 1){
					nfds--;
				}	
			}
		}
		// pthread_mutex_lock(&count_mutex);
		// printf("Current client count %d\n", client_count);
		// printf("Current producer count %d\n", producer_count);
		// printf("Current consumer count %d\n", consumer_count);
		// printf("Total prodecers served %d\n", producer_served_count);
		// printf("Total consumers served %d\n", consumer_served_count);
		// printf("Client rejects - slow %d\n", client_reject_slow_count);
		// printf("Client rejects - max %d\n", client_reject_max_count);
		// printf("Producer rejects %d\n", producer_reject_count);
		// printf("Consumer rejects %d\n\n", consumer_reject_count);
		// pthread_mutex_unlock(&count_mutex);
	}
}

