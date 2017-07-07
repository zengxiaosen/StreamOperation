
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <strings.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>

int main (int argc, char * const argv[]) {
	// Usage: udp_sender <receiver host> <receiver port> <message size in bytes> <number of messages> <delay between messages in usec>
	
	if (argc != 6) {
		fprintf(stderr,"Usage: %s <receiver host> <receiver port> <message size in bytes> <number of messages> <delay between messages in usec>\n",argv[0]);
		exit(-1);
	}
	
	char *receiver_host = argv[1];
	int receiver_port = atoi(argv[2]);
	int message_length = atoi(argv[3]);
	int n_messages = atoi(argv[4]);
	int delay = atoi(argv[5]);
	
	struct sockaddr_in receiver;
	bzero(&receiver,sizeof(receiver));
	//receiver.sin_len = sizeof(struct sockaddr_in);
	receiver.sin_family = AF_INET;
	struct hostent *rh = gethostbyname(receiver_host);
	assert(rh != 0);
	bcopy(rh->h_addr,&(receiver.sin_addr),rh->h_length);
	receiver.sin_port = htons(receiver_port);		  
	
	int s = socket(AF_INET, SOCK_DGRAM, 0);
	assert(s != -1);
	
	char *message = (char *) malloc(message_length);
	assert(message != NULL);
	
	for (int i=0; i<message_length; i++)
		message[i] = (char) (i % 127 + 1);
	
	for (int m_nr = 0; m_nr < n_messages; m_nr++) {
		bcopy(&m_nr,message,sizeof(m_nr));
		int ret = sendto(s,message,message_length,0,(const struct sockaddr *) &receiver,sizeof(receiver));
		if (ret == -1) { perror("sendto"); exit(-1); }
		if (delay > 0)
			usleep(delay);
	}

    return 0;
}
