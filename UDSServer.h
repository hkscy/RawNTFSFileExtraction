#ifndef UDS_H_
#define UDS_H_

#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>

#define SOCKET_BUFF	256

void *udsServerThreadFn( void *socket_path );

char *socket_path = "\0diskTap";
int udsFD;

/*
 * Creates a UDS socket at the path specified and prints received messages to stdout.
 */
void *udsServerThreadFn(void *socket_path) {

	struct sockaddr_un addr;
	char buf[SOCKET_BUFF];
	int socketDescriptor, udsReadStatus;

	if ( (udsFD = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		perror("socket error");
		pthread_exit(NULL);
	}

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, ((char *)socket_path), sizeof(addr.sun_path)-1);
	unlink(((char *)socket_path));

	if (bind(udsFD, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
		perror("bind error");
		pthread_exit(NULL);
	}

	if (listen(udsFD, 5) == -1) {
		perror("listen error");
		pthread_exit(NULL);
	}

	while (true) {
		if ( (socketDescriptor = accept(udsFD, NULL, NULL)) == -1) {
			perror("accept error");
			continue;
		}

		/* Read the received data */
		while ( (udsReadStatus=read(socketDescriptor, buf, sizeof(buf))) > 0) {
			printf("read %u bytes: %.*s\n", udsReadStatus, udsReadStatus, buf);
		}

		if (udsReadStatus == -1) { /*Error reading socket */
			perror("Read UDS socket error");
			pthread_exit(NULL);
		}

		else if (udsReadStatus == 0) {
			printf("Client disconnected.\n");
			close(socketDescriptor);
		}
	}

    pthread_exit(0);
}

#endif UDS_H_
