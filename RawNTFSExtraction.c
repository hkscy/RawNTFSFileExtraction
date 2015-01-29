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
#include <errno.h>     /*For errno */
#include <stdint.h>    /*unitN_t */
#include <string.h>
#include <inttypes.h> /*for (u)int64_t format specifiers PRId64 and PRIu64 */
#include "NTFSStruct.h"

#define FALSE 0
#define TRUE 1
#define BUFFSIZE 512
#define P_PARTITIONS 4
#define SECTOR_SIZE 512
#define P_OFFEST 0x1BE	/*Partition information begins at offest 0x1BE */
#define NTFS_TYPE 0x07	/*NTFS partitions are represented by 0x07 in the partition table */
#define MFT_RECORD_LENGTH 1024 /*MFT entries are 1024 bytes long */

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
	if(part->chType == 0x07)
		return part->dwRelativeSector;
	return -1;
}

/**
 *	Prints the members of *bootSec into *buff
 *	Returns -1 if error.
 */
int getBootSectInfo(char* buff, NTFS_BOOT_SECTOR *bootSec) {
	sprintf(buff, "jumpInstruction: %s\n"
				  "OEM ID: %s\n"
				  "BIOS Parameter Block(BPB) data: \n"
				  "Bytes per logical sector: %u\n"
				  "Logical sectors per cluster: %u\n"
				  "Reserved logical sectors: %u\n"
				  "Media descriptor: %d\n"
				  "Physical sectors per track: %u\n"
				  "Number of heads: %u\n"
				  "Hidden sectors: %d\n"
				  "Sectors in volume: %" PRId64 "\n"
				  "\n*Cluster number for MFT: %" PRId64 "\n"
				  "Mirror of cluster number for MFT: %" PRId64 "\n"
				  "MFT record size: %d\n"
				  "Index block size: %d\n"
				  "\nVolume serial number: %" PRId64 "\n"
				  "Volume checksum: %d\n"
				  "End of sector marker: %u\n",
				  bootSec->chJumpInstruction,
				  bootSec->chOemID,
				  (bootSec->bpb).wBytesPerSec,
				  (bootSec->bpb).uchSecPerClust,
				  (bootSec->bpb).wReservedSec,
				  (bootSec->bpb).uchMediaDescriptor,
				  (bootSec->bpb).wSecPerTrack,
				  (bootSec->bpb).wNumberOfHeads,
				  (bootSec->bpb).dwHiddenSec,
				  (bootSec->bpb).n64TotalSec,
				  (bootSec->bpb).n64MFTLogicalClustNum,	/*Cluster number for MFT! */
				  (bootSec->bpb).n64MFTMirrLoficalClustNum,
				  (bootSec->bpb).nClustPerMFTRecord,
				  (bootSec->bpb).nClustPerIndexRecord,
				  (bootSec->bpb).n64VolumeSerialNum,
				  (bootSec->bpb).dwCheckSum,
				  bootSec->wSecMark );
	return 0;
}

int getMFTAttribMembers(char * buff, NTFS_ATTRIBUTE* attrib) {
	char* tempBuff = malloc(BUFFSIZE);
	sprintf(tempBuff, "Attribute type: %d\n"
				  	  "Length of attribute: %d\n"
				  	  "Non-resident flag: %s\n"
				  	  "Length of name: %u\n"
				  	  "Offset to name: %u\n"
				  	  "Flags: %u\n"
				  	  "Attribute identifier: %u\n",
					  attrib->dwType,
					  attrib->dwFullLength,
					  attrib->uchNonResFlag == TRUE ? "Non-Resident" : "Resident",
					  attrib->uchNameLength,
					  attrib->wNameOffset,
					  attrib->wFlags,
					  attrib->wID);
	if(attrib->uchNonResFlag) { /*If attribute is Non-Resident*/
		sprintf(buff, "%s", tempBuff);
		//(attrib->Attr).NonResident;

	} else{ /*Attribute is resident */
		sprintf(buff, "%s"
				"Length of attribute content: %d\n"
				"Offset to attribute content: %u\n"
				"Indexed: %u\n"
				"Padding: %u\n",
				tempBuff,
				(attrib->Attr).Resident.dwLength,
				(attrib->Attr).Resident.wAttrOffset,
				(attrib->Attr).Resident.uchIndexedTag,
				(attrib->Attr).Resident.uchPadding);
	}
	return 0;
}

int main(int argc, char* argv[]) {
	int fileDescriptor = 0;
	off_t blk_offset = 0;
	ssize_t readStatus, bsFound = -1;
	uint64_t u64bytesAbsoluteMFT = -1;
	char* buff = malloc(BUFFSIZE);	/*Used for getPartitionInfo(...), getBootSectinfo(...) et al*/
	char* mftBuffer = malloc(MFT_RECORD_LENGTH); /*Buffer an entire MFT Record here*/
	printf("Launching raw NTFS extraction engine for %s\n", BLOCK_DEVICE);

	fileDescriptor = open(BLOCK_DEVICE, O_RDONLY); /*Open block device and check if failed */
	if(fileDescriptor == -1) { /* open failed. */
		int errsv = errno;
		printf("Failed to open block device %s with error: %s.\n", BLOCK_DEVICE, strerror(errsv));
		return EXIT_FAILURE;
	}

	if((blk_offset = lseek(fileDescriptor, P_OFFEST, SEEK_SET)) == -1) { /*Set offest pointer to partition table */
		int errsv = errno;
		printf("Failed to position file pointer to partition table with error: %s.\n", strerror(errsv));
		return EXIT_FAILURE;
	}

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
		if(nTFSParts[i]->chBootInd == 0x80) { /*If this is a Bootable NTFS partition*/
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
				bsFound = i;/* Set BS found flag and which partition it's in */
			}
		}
	}

	/*------ If NTFS boot sector found then use it to find Master File Table -----*/
	if(bsFound > -1) {
		getBootSectInfo(buff, nTFS_Boot);
		printf("\nNTFS boot sector data\n%s\n", buff);
		/*Calculate the number of bytes per sector = sectors per cluster * bytes per sector */
		uint32_t dwBytesPerCluster = (nTFS_Boot->bpb.uchSecPerClust) * (nTFS_Boot->bpb.wBytesPerSec);
		printf("Filesystem Bytes Per Sector: %d\n", dwBytesPerCluster);
		/*Calculate the number of bytes by which the boot sector is offset on disk */
		uint64_t u64bytesAbsoluteSector = (nTFS_Boot->bpb.wBytesPerSec) * (nTFSParts[bsFound]->dwRelativeSector);
		printf("Bootsector offset in bytes: %" PRIu64 "\n", u64bytesAbsoluteSector );
		/*Calculate the relative bytes location of the MFT on the partition */
		uint64_t u64bytesRelativeMFT = dwBytesPerCluster * (nTFS_Boot->bpb.n64MFTLogicalClustNum);
		printf("Relative bytes location of MFT: %" PRIu64 "\n", u64bytesRelativeMFT);
		/*Absolute MFT offset in bytes*/
		u64bytesAbsoluteMFT = u64bytesAbsoluteSector + u64bytesRelativeMFT;
		printf("Absolute MFT location in bytes: %" PRIu64 "\n", u64bytesAbsoluteMFT);
	}

	/* Find the root directory metafile entry in the MFT, and extract its index allocation attributes */
	NTFS_MFT_FILE_ENTRY_HEADER *firstMFTHeader;// = malloc( MFT_RECORD_LENGTH );
	if(u64bytesAbsoluteMFT > 0) {
		/* Move file pointer to the absolute MFT location */
		if((blk_offset = lseek(fileDescriptor, u64bytesAbsoluteMFT, SEEK_SET)) == -1) {
			int errsv = errno;
			printf("Failed to position file pointer to NTFS boot sector with error: %s.\n", strerror(errsv));
			return EXIT_FAILURE;
		}
		firstMFTHeader = malloc(sizeof(NTFS_MFT_FILE_ENTRY_HEADER));
		/* Read the first MFT entry */
		if((readStatus = read( fileDescriptor, mftBuffer, MFT_RECORD_LENGTH)) == -1){
			int errsv = errno;
			printf("Failed to read MFT at offset: %" PRIu64 ", with error %s.\n", u64bytesAbsoluteMFT, strerror(errsv));
			return EXIT_FAILURE;
		} else {
			printf("\nRead first MFT record into buffer.\n");
			memcpy(firstMFTHeader, mftBuffer, sizeof(NTFS_MFT_FILE_ENTRY_HEADER)); /* Copy header from record*/
		}
	}

	/*---------------------- Follow attribute offset position ---------------------*/
	NTFS_ATTRIBUTE *mftRecAttrib = malloc( sizeof(NTFS_ATTRIBUTE) );
	uint16_t attribOffset = firstMFTHeader->wAttribOffset;
	printf("Attribute offset into the MFT header: %u\n", attribOffset);
	memcpy(mftRecAttrib, mftBuffer+attribOffset, sizeof(NTFS_ATTRIBUTE));
	getMFTAttribMembers(buff, mftRecAttrib);
	printf("%s\n", buff);
	if(mftRecAttrib->uchNonResFlag == FALSE) {
		/*Data is resident to the attribute */
		uint32_t dataLength = (mftRecAttrib->Attr).Resident.dwLength;
		uint16_t dataOffset = (mftRecAttrib->Attr).Resident.wAttrOffset;
		char* data = malloc( dataLength );
		memcpy(data, mftBuffer+attribOffset+dataOffset, dataLength);
		printf("Data: %s\n", data);
		free(data);
	} else if(mftRecAttrib->uchNonResFlag == TRUE) {
		/*Data is non-resident to the attribute */

	}


	/*---------------------------------- Tidy up ----------------------------------*/
	for(i = 0; i<P_PARTITIONS; i++) {
		free(priParts[i]);	/*Free the memory allocated for primary partition structs */
	} free(priParts);
	for(i=0; i<nNTFS; i++) {
		free(nTFSParts[i]); /*Free the memory allocated for NTFS partition structs */
	} free(nTFSParts);

	free(nTFS_Boot);		/*Free Boot sector memory */
	free(firstMFTHeader);	/*48b First MFT entry header*/
	free(buff); 			/*Used for buffering various texts */
	free(mftBuffer);		/*Used for buffering one MFT record, 1kb*/
	free(mftRecAttrib);

	if((close(fileDescriptor)) == -1) { /*close block device and check if failed */
		int errsv = errno;
		printf("Failed to close block device %s with error: %s.\n", BLOCK_DEVICE, strerror(errsv));
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
