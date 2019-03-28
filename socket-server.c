/*
 * socket-server.c
 * Simple TCP/IP communication using sockets
 *
 */

#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "socket-common.h"

/* Convert a buffer to upercase */
void toupper_buf(char *buf, size_t n)
{
	size_t i;

	for (i = 0; i < n; i++)
		buf[i] = toupper(buf[i]);
}

/* Insist until all of the data has been written */
ssize_t insist_write(int fd, const void *buf, size_t cnt)
{
	ssize_t ret;
	size_t orig_cnt = cnt;
	
	while (cnt > 0) {
	        ret = write(fd, buf, cnt);
	        if (ret < 0)
	                return ret;
	        buf += ret;
	        cnt -= ret;
	}

	return orig_cnt;
}

int main(void)
{
	char buf[100], ch;
	char addrstr[INET_ADDRSTRLEN];
	int sd, newsd, ret, i=0;
	ssize_t n;
	socklen_t len;
	struct sockaddr_in sa;
	fd_set fds;	


	/* Make sure a broken connection doesn't kill us */
	signal(SIGPIPE, SIG_IGN);

	/* Create TCP/IP socket, used as main chat channel */
	if ((sd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket");
		exit(1);
	}
	fprintf(stderr, "Created TCP socket\n");

	/* Bind to a well-known port */
	memset(&sa, 0, sizeof(sa));
	sa.sin_family = AF_INET;
	sa.sin_port = htons(TCP_PORT);
	sa.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(sd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
		perror("bind");
		exit(1);
	}
	fprintf(stderr, "Bound TCP socket to port %d\n", TCP_PORT);

	/* Listen for incoming connections */
	if (listen(sd, TCP_BACKLOG) < 0) {
		perror("listen");
		exit(1);
	}

	/* Loop forever, accept()ing connections */
	for (;;) {
		fprintf(stderr, "Waiting for an incoming connection...\n");

		/* Accept an incoming connection */
		len = sizeof(struct sockaddr_in);
		if ((newsd = accept(sd, (struct sockaddr *)&sa, &len)) < 0) {
			perror("accept");
			exit(1);
		}
		if (!inet_ntop(AF_INET, &sa.sin_addr, addrstr, sizeof(addrstr))) {
			perror("could not format IP address");
			exit(1);
		}
		fprintf(stderr, "Incoming connection from %s:%d\n",
			addrstr, ntohs(sa.sin_port));


		/* We break out of the loop when the remote peer goes away */
		for (;;) {

			//clear the socket set
			FD_ZERO(&fds);

			//add socket and stdin to socket set
			FD_SET(newsd, &fds);
			FD_SET(0, &fds);

			ret = select(newsd+1, &fds, NULL, NULL, NULL);
			if ((ret<0) && (errno!=EINTR)) {
				printf("error in select");
				exit(1);
			}

			if (FD_ISSET(newsd, &fds)) {
			
				//read from socket and write to stdout
				n = read(newsd, buf, sizeof(buf));
				if (n <= 0) {
					if (n < 0)
						perror("read from remote peer failed");
					else
						fprintf(stderr, "Peer went away\n");
					break;
				}
				toupper_buf(buf, n); //lowercase to uppercase
				if (insist_write(1, buf, n) != n) {
					perror("write to remote peer failed");
					break;
				}
			}
			//if the stdin is part of the fds set then
			else if (FD_ISSET(0, &fds)) {
				//read from input, write to socket
				i=0;
				while( read(STDIN_FILENO, &ch, 1) > 0 && i<100)
					buf[i] = ch;
					i++;
				}
				i--;
				if (insist_write(sd, buf, i) != i) { //and copy the buffer to the socket
					perror("write");
					exit(1);
				}		
	
				//clear the buffer
				memset(&buf[0], 0, sizeof(buf));
			}
		}
		/* Make sure we don't leak open files */
		if (close(newsd) < 0) {
			perror("close");
		}

	/* This will never happen */
	return 1;
}

