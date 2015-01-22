/*
 ============================================================================
 Name        : RawNTFSExtraction.c
 Author      : Christopher Hicks
 Version     :
 Copyright   : GPL
 Description : A raw NTFS extraction engine.
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h> /*Various data types used elsewhere */
#include <sys/stat.h>  /*File information (stat et al) */
#include <fcntl.h>     /*File opening, locking and other operations */
#include <unistd.h>
#include <errno.h>     /*For errno access */
#include <stdint.h>	   /*unitN_t macros */
#include <string.h>

#define SECTOR_SIZE 512
#define P_OFFEST 0x1BE	/*Partition information begins at offest 0x1BE */

typedef unsigned char BYTE; /*Define byte symbolic abbreviation */

#pragma pack(push, 1) /*Pack structures to a one byte alignment */
	typedef struct _PARTITION { /*NTFS partition table struct */
		BYTE chBootInd; 	/*Boot indicator bit flag: 0 = no, 0x80 = bootable (or "active") */
		BYTE chHead;		/*Starting head */
		BYTE chSector;		/*Starting sector (Bits 6-7 are the upper two bits for the Starting Cylinder field.) */
		BYTE chCylinder;	/*Starting Cylinder */
		BYTE chType; 		/*Partition type identifier */
		BYTE chLastHead;	/*Ending Head */
		BYTE chLastSector; 	/* Ending Sector (Bits 6-7 are the upper two bits for the ending cylinder field) */
		BYTE chLastCylinder;/* Ending Cylinder */
		uint32_t dwRelativeSector; 	/*Relative Sector (to start of partition -- also equals the partition's starting LBA value) */
		uint32_t dwNumberSector; 	/* Total Sectors in partition  */
	} PARTITION, *P_PARTITION;
#pragma pack(pop)

static const char BLOCK_DEVICE[] = "/dev/mechastriessand/windows7";

int getPartitionInfo(char *buff, int len, PARTITION *part) {
	sprintf(buff, "\nIs bootable: %d\n"
				  "Starting head: %d\n"
				  "Start sector: %d\n"
			      "Start cylinder: %d\n"
			      "Partition type: %d\n"
				  "Ending head: %d\n"
				  "Ending sector: %d\n"
				  "Ending cylinder: %d\n"
				  "Relative sector: %d\n"
				  "Total sectors: %d\n",
				  part->chBootInd,
				  part->chHead,
				  part->chSector,
				  part->chCylinder,
				  part->chType,
				  part->chLastHead,
				  part->chLastSector,
				  part->chLastCylinder,
				  part->dwRelativeSector,
				  part->dwNumberSector);
	return 1;
}

int main(int argc, char* argv[]) {
	printf("Launching raw NTFS extraction engine for %s\n", BLOCK_DEVICE);
	int fileDescriptor;
	fileDescriptor = open(BLOCK_DEVICE, O_RDONLY); /*Open block device and check if failed */
	if(fileDescriptor == -1) { /* open failed. */
		int errsv = errno;
		printf("Failed to open block device %s with error: %s.\n", BLOCK_DEVICE, strerror(errsv));
		return EXIT_FAILURE;
	}

	off_t blk_offset = 0;
	if((blk_offset = lseek(fileDescriptor, P_OFFEST, SEEK_SET)) == -1) { /*Set offest pointer to partition table */
		int errsv = errno;
		printf("Failed to position file pointer to partition table with error: %s.\n", strerror(errsv));
		return EXIT_FAILURE;
	}

	ssize_t readStatus;
	PARTITION *part1 = malloc( sizeof(PARTITION) );
	readStatus = read( fileDescriptor, part1, sizeof(PARTITION)  );
	char* buff = malloc(256);
	getPartitionInfo(buff, 256, part1);
	printf("%s\n",buff);


	PARTITION *part2 = malloc( sizeof(PARTITION) );
	readStatus = read( fileDescriptor, part2, sizeof(PARTITION)  );
	getPartitionInfo(buff, 256, part2);
	printf("%s\n",buff);

	PARTITION *part3 = malloc( sizeof(PARTITION) );
	readStatus = read( fileDescriptor, part3, sizeof(PARTITION)  );
	getPartitionInfo(buff, 256, part3);
	printf("%s\n",buff);

	PARTITION *part4 = malloc( sizeof(PARTITION) );
	readStatus = read( fileDescriptor, part4, sizeof(PARTITION)  );
	getPartitionInfo(buff, 256, part4);
	printf("%s\n",buff);

	free(part1);
	free(part2);
	free(part3);
	free(part4);
	free(buff);



	if((close(fileDescriptor)) == -1) { /*close block device and check if failed */
		int errsv = errno;
		printf("Failed to close block device %s with error: %s.\n", BLOCK_DEVICE, strerror(errsv));
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
