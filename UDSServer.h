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

#define SOCKET_BUFF	64
#define Q_ELEMENTS 1000
#define Q_SIZE (Q_ELEMENTS + 1)
#define QUEUE_FULL -1

typedef struct _qemu_offs_len {
	int64_t sectorN;
	int nSectors;
} QEMU_OFFS_LEN;

/* Producer-Consumer methods */
void QInit(void);
int  QPut(QEMU_OFFS_LEN qItem);
int  QGet(QEMU_OFFS_LEN *qItem);

/* Producer worker thread for UDS server */
void *udsServerThreadFn( void *socket_path );

/*FIFO buffer for QEMU writes */
QEMU_OFFS_LEN writeQueue[Q_SIZE];
int writeQueueIn = 0, writeQueueOut = 0;
pthread_mutex_t mutexqueue;

char *socket_path = "\0diskTap";
int udsFD;

/*
 * Creates a UDS socket at the path specified and prints received messages to stdout.
 * Producer thread
 */
void *udsServerThreadFn(void *socket_path) {

	struct sockaddr_un addr;
	BYTE buffer[SOCKET_BUFF];
	QEMU_OFFS_LEN buff;
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
			int errsv = errno;
			printf("Accept error: %s\n", strerror(errsv));
			continue;
		}

		/* Read the received data */
		while ( (udsReadStatus=read(socketDescriptor, &buff, sizeof(QEMU_OFFS_LEN))) > 0 ) {
			if(DEBUG) printf("read %u bytes: %s\n", udsReadStatus, buffer);
			QPut(buff); /*Put received data in the FIFO */
		}

		if (udsReadStatus == -1) { /*Error reading socket */
			int errsv = errno;
			printf("Read from UDS socket error: %s\n", strerror(errsv));
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

	writeQueueIn = 0;
	writeQueueOut = 0;
}

/**
 * Put a new item in the queue
 */
int QPut(QEMU_OFFS_LEN qItem)
{
	pthread_mutex_lock (&mutexqueue);
	if(writeQueueIn == (( writeQueueOut - 1 + Q_SIZE) % Q_SIZE)) {
		pthread_mutex_unlock (&mutexqueue);
		return -1;	/* Queue Full*/
	} else {
		writeQueue[writeQueueIn] = qItem;
		writeQueueIn = (writeQueueIn + 1) % Q_SIZE;
		pthread_mutex_unlock (&mutexqueue);
		return 0;
	}
}

/**
 * Remove the earliest item from the queue (FIFO)
 */
int QGet(QEMU_OFFS_LEN *qItem)
{
	pthread_mutex_lock (&mutexqueue);
    if(writeQueueIn == writeQueueOut) {
    	pthread_mutex_unlock (&mutexqueue);
        return -1;	/* Queue Empty */
    } else {
    	*qItem = writeQueue[writeQueueOut];
    	writeQueueOut = (writeQueueOut + 1) % Q_SIZE; /*Wrap round to zero */
    	pthread_mutex_unlock (&mutexqueue);
    	return 0;
    }
}

#endif
