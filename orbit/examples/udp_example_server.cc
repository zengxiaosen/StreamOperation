#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <strings.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

int main (int argc, char * const argv[]) {
		
	int port = 0;
	if (argc == 2)
		port = atoi(argv[1]);
	
	int s = socket(AF_INET, SOCK_DGRAM, 0);
	assert(s != -1);
	
	struct sockaddr_in saddr;
	bzero(&saddr,sizeof(saddr));
	//saddr.sin_len = sizeof(saddr);
	saddr.sin_family = AF_INET;
	unsigned long addr_any = htonl(INADDR_ANY);
	bcopy(&addr_any,&(saddr.sin_addr),sizeof(saddr.sin_addr));
	saddr.sin_port = htons(port);
	int ret = bind(s,(const struct sockaddr *) &saddr,sizeof(saddr));
	if (ret == -1) { perror("bind"); exit(-1); };
	
	socklen_t saddr_len = sizeof(saddr);
	getsockname(s, (struct sockaddr *) &saddr, &saddr_len);
	printf("Listening at port %d\n",ntohs(saddr.sin_port));
	
	const int max_message_size = 65536;
	char *message = (char *) malloc(max_message_size);
	assert(message != NULL);
	
	int n_messages = 0;
	while (true) {
		int n = recvfrom(s,message,max_message_size,0,NULL,NULL);
		if (n == -1) { perror("recvfrom"); exit(-1); };
		int m_nr;
		bcopy(message,&m_nr,sizeof(m_nr));
		n_messages++;
		printf("%d: Message %d with %d bytes\n",n_messages,m_nr,n);
	}
	
    return 0;
}
