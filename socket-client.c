/*
 * socket-client.c
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

int main(int argc, char *argv[])
{
	int sd, port, ret, i;
	ssize_t n;
	char buf[100], ch;
	char *hostname;
	struct hostent *hp;
	struct sockaddr_in sa;

	//set of socket descriptors
	fd_set fds;

	if (argc != 3) {
		fprintf(stderr, "Usage: %s hostname port\n", argv[0]);
		exit(1);
	}

	hostname = argv[1];
	port = atoi(argv[2]); /* Needs better error checking */

	/* Create TCP/IP socket, used as main chat channel */
	if ((sd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket");
		exit(1);
	}
	fprintf(stderr, "Created TCP socket\n");
	
	/* Look up remote hostname on DNS */
	if ( !(hp = gethostbyname(hostname))) {
		printf("DNS lookup failed for host %s\n", hostname);
		exit(1);
	}

	/* Connect to remote TCP port */
	sa.sin_family = AF_INET;
	sa.sin_port = htons(port);
	memcpy(&sa.sin_addr.s_addr, hp->h_addr, sizeof(struct in_addr));
	fprintf(stderr, "Connecting to remote host... "); fflush(stderr);
	if (connect(sd, (struct sockaddr *) &sa, sizeof(sa)) < 0) {
		perror("connect");
		exit(1);
	}
	fprintf(stderr, "Connected.\n");

	/* 
		the parameters of the terms structure:
		tcflag_t c_iflag; input modes
		tcflag_t c_oflag; output modes
		tcflag_t c_cflag; control modes
		tcflag_t c_lflag; local modes
		cc_t cc_c[NCCS]; special characters
	*/

	////////////////////////////////////////////////////////
	/* Be careful with buffer overruns, ensure NUL-termination */
	strncpy(buf, HELLO_THERE, sizeof(buf));
	buf[sizeof(buf) - 1] = '\0';

	/* Say something... */
	if (insist_write(sd, buf, strlen(buf)) != strlen(buf)) {
		perror("write");
		exit(1);
	}
	fprintf(stdout, "I said:\n%s\nRemote says:\n", buf);
	fflush(stdout);
	/////////////////////////////////////////////////////////


	/* Read answer and write it to standard output */
	for (;;) {

		//clear the socket set
		FD_ZERO(&fds);

		//now add the socket and stdin to the set
		FD_SET(sd, &fds);
		FD_SET(0, &fds);
		
		/* select allows the program to monitor more than 1 fds, 
		waiting until 1 or 1+ of them become ready for some I/O operation. */
		ret = select(sd+1, &fds, NULL, NULL, NULL);
		if ((ret<0) && (errno!=EINTR)) {
			printf("error in select");
			exit(1);
		}

		if (FD_ISSET(sd, &fds)) {
			
			n = read(sd, buf, sizeof(buf));

			if (n < 0) {
				perror("read");
				exit(1);
			}

			if (n <= 0)
				break;

			if (insist_write(1, buf, n) != n) {
				perror("write");
				exit(1);
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

	/*
	* Let the remote know we're not going to write anything else.
	* Try removing the shutdown() call and see what happens.
	*/
	if (shutdown(sd, SHUT_WR) < 0) {
		perror("shutdown");
		exit(1);
	}
	fprintf(stderr, "\nDone.\n");
	return 0;
}
