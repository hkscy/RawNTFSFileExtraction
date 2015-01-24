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
#include "NTFSStruct.h"

#define P_PARTITIONS 4
#define SECTOR_SIZE 512
#define P_OFFEST 0x1BE	/*Partition information begins at offest 0x1BE */
#define NTFS_TYPE 0x07	/*NTFS partitions are represented by 0x07 in the partition table */

static const char BLOCK_DEVICE[] = "/dev/mechastriessand/windows7";

/**
 * Prints the members of *part into *buff, plus some extra derived info.
 * Returns the relative sector offest for the partition if it is NTFS,
 * otherwise returns -1.
 */
int getPartitionInfo(char *buff, PARTITION *part) {
	if(part->dwNumberSector == 0) { /*Primary partition entry is empty */
		sprintf(buff, "Primary partition table entry empty.");
		return -1;
	}
	sprintf(buff, "Is bootable: %s\n"
			      "Partition type: %s\n"
				  "Start CHS address: %d/%d/%d\n"
				  "End CHS address: %d/%d/%d\n"
				  "Relative sector: %d\n"
				  "Total sectors: %d\n"
				  "Partition size: %0.2f GB",
				  part->chBootInd == 0x80 ? "Yes" : "No",
				  part->chType == NTFS_TYPE ? "NTFS" : "Other",
				  part->chCylinder,
				  part->chHead,
				  part->chSector,
				  part->chLastCylinder,
				  part->chLastHead,
				  part->chLastSector,
				  part->dwRelativeSector,
				  part->dwNumberSector,
				  part->dwNumberSector/2097152.0);
	if(part->chType == 0x07) {
		return part->dwRelativeSector;

	}
	return -1;
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
	char* buff = malloc(256); /*Used for getPartitionInfo(...) */

	/*--------------------- Read in primary partitions from MBR ---------------------*/
	PARTITION **priParts = malloc( P_PARTITIONS*sizeof(PARTITION) );
	PARTITION **nTFSParts = malloc( P_PARTITIONS*sizeof(PARTITION) );
	int i, nNTFS = 0;
	for(i = 0; i<P_PARTITIONS; i++) { /*Iterate the primary partitions in MBR to look for NTFS partitions */
		priParts[i] = malloc( sizeof(PARTITION) );
		if((readStatus = read( fileDescriptor, priParts[i], sizeof(PARTITION))) == -1){
				int errsv = errno;
				printf("Failed to open partition table with error: %s.\n", strerror(errsv));
			} else {
				if(priParts[i]->chType == NTFS_TYPE) {	/*If the part table contains an NTFS entry */
					nTFSParts[nNTFS] = malloc( sizeof(PARTITION) );
					nTFSParts[nNTFS++] = priParts[i]; /* Increment the NTFS parts counter */
					getPartitionInfo(buff, priParts[i]);
					printf("\nPartition%d:\n%s\n",i, buff);
				}
		}
	}

	/*--- Follow relative sector offset of NTFS partitions to find boot sector ----*/
	NTFS_BOOT_SECTOR *nTFS_Boot = malloc( sizeof(NTFS_BOOT_SECTOR) );	/*Allocate memory for holding the boot sector code*/
	for(i = 0; i<nNTFS; i++) {
		/*Set offest pointer to partition table */
		if((blk_offset = lseek(fileDescriptor, nTFSParts[i]->dwRelativeSector*SECTOR_SIZE, SEEK_SET)) == -1) {
			int errsv = errno;
			printf("Failed to position file pointer to NTFS boot sector with error: %s.\n", strerror(errsv));
			return EXIT_FAILURE;
		}
		if((readStatus = read( fileDescriptor, nTFS_Boot, sizeof(NTFS_BOOT_SECTOR))) == -1){
			int errsv = errno;
			printf("Failed to open NTFS Boot sector for partition %d with error: %s.\n",i , strerror(errsv));
		} else {
			/* Do something with the boot sector code */
		}
	}

	/*---------------------------------- Tidy up ----------------------------------*/
	for(i = 0; i<P_PARTITIONS; i++) {
		free(priParts[i]);	/*Free the memory allocated for primary partition structs */
	} free(priParts);
	for(i=0; i<nNTFS; i++) {
		free(nTFSParts[i]); /*Free the memory allocated for NTFS partition structs */
	} free(nTFSParts);

	//free(nTFS_Boot);	/*Free Boot sector memory */
	free(buff); /*Used for buffering various texts */

	if((close(fileDescriptor)) == -1) { /*close block device and check if failed */
		int errsv = errno;
		printf("Failed to close block device %s with error: %s.\n", BLOCK_DEVICE, strerror(errsv));
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
