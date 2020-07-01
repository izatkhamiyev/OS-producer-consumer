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
#include <math.h>

double poissonRandomInterarrivalDelay( double r ){
    return (log((double) 1.0 - ((double) rand())/((double) RAND_MAX)))/-r;
}

int main(){
	int i = 0;
	for(; i < 30; i++){
		double wait = poissonRandomInterarrivalDelay(i) * 1000000;
        	printf("%d: %lf \n", i, wait); 
	}
	return 0;
}
