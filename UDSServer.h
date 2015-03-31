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
#define Q_ELEMENTS 1000
#define Q_SIZE (Q_ELEMENTS + 1)
#define QUEUE_FULL -1

void *udsServerThreadFn( void *socket_path );

/* Producer-Consumer methods */
void QInit(void);
int  QPut(char *qItem);
int  QGet(char **qItem);

/*FIFO buffer for QEMU writes */
char *writeQueue[Q_SIZE];
int writeQueueIn = 0, writeQueueOut = 0;

char *socket_path = "\0diskTap";
int udsFD;

/*
 * Creates a UDS socket at the path specified and prints received messages to stdout.
 * Producer thread
 */
void *udsServerThreadFn(void *socket_path) {

	struct sockaddr_un addr;
	char buf[SOCKET_BUFF];
	int socketDescriptor, udsReadStatus;
	QInit();

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
		while ( (udsReadStatus=read(socketDescriptor, buf, sizeof(buf))) > 0 ) {
			//printf("read %u bytes: %.*s\n", udsReadStatus, udsReadStatus, buf);
			buf[(strcspn(buf, "\r\n"))] = 0; /*Replaces LF with \0 */
			if(DEBUG) printf("read %u bytes: %s\n", udsReadStatus, buf);
			char * recvd = malloc( sizeof(buf) );
			strcpy(recvd, buf);
			QPut(recvd); /*Put received data in the FIFO */
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

/**
 * Initialise the FIFO buffer for use.
 */
void QInit()
{
    writeQueueIn = writeQueueOut = 0;
}

/**
 * Put a new item in the queue
 */
int QPut(char *qItem)
{
	if(writeQueueIn == (( writeQueueOut - 1 + Q_SIZE) % Q_SIZE)) {
		return -1;	/* Queue Full*/
	} else {
		writeQueue[writeQueueIn] = qItem;
		writeQueueIn = (writeQueueIn + 1) % Q_SIZE;
		return 0;
	}
}

/**
 * Remove the earliest item from the queue (FIFO)
 */
int QGet(char **qItem)
{
    if(writeQueueIn == writeQueueOut) {
        return -1;	/* Queue Empty */
    } else {
    	*qItem = writeQueue[writeQueueOut];
    	writeQueueOut = (writeQueueOut + 1) % Q_SIZE; /*Wrap round to zero */
    	return 0;
    }
}

#endif
