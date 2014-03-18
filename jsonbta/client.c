#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include "bta_json.h"

int main(int argc, char *argv[]){
	int sockfd = 0, n = 0;
	char recvBuff[1024];
	memset(recvBuff, '0',sizeof(recvBuff));
	struct addrinfo h, *r, *p;
	memset(&h, 0, sizeof(h));
	h.ai_family = AF_INET;
	h.ai_socktype = SOCK_STREAM;
	h.ai_flags = AI_CANONNAME;
	char *host = "localhost";
	if(argc > 1) host = argv[1];
	char *port = PORT;
	if(argc > 2) port = argv[2];
	if(getaddrinfo(host, port, &h, &r)){perror("getaddrinfo"); return -1;}
	struct sockaddr_in *ia = (struct sockaddr_in*)r->ai_addr;
	char str[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &(ia->sin_addr), str, INET_ADDRSTRLEN);
	printf("canonname: %s, port: %u, addr: %s\n", r->ai_canonname, ntohs(ia->sin_port), str);
	for(p = r; p; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("socket");
			continue;
		}
		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("connect");
			continue;
		}
		break; // if we get here, we must have connected successfully
	}
	if (p == NULL) {
		// looped off the end of the list with no connection
		fprintf(stderr, "failed to connect\n");
		return -1;
	}
	freeaddrinfo(r);
	char *msg = malloc(2048);
	char *res = RESOURCE;
	if(argc == 4) res = argv[3];
	snprintf(msg, 2047, "GET %s HTTP/1.1\r\n", res);
	if(send(sockfd, msg, strlen(msg), 0) != strlen(msg)){perror("send");}
	while((n = read(sockfd, recvBuff, sizeof(recvBuff)-1)) > 0){
		recvBuff[n] = 0;
		if(fputs(recvBuff, stdout) == EOF) 	printf("\n Error : Fputs error\n");
	}
	if(n < 0) printf("\n Read error \n");
	return 0;
}
