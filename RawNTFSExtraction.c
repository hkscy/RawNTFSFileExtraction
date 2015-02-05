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
#include <inttypes.h>  /*for (u)int64_t format specifiers PRId64 and PRIu64 */
#include <wchar.h>	   /*for formatted output of wide characters. */
#include "NTFSStruct.h"
#include "NTFSAttributes.h"

#define FALSE 0
#define TRUE 1
#define BUFFSIZE 1024
#define P_PARTITIONS 4
#define SECTOR_SIZE 512
#define P_OFFEST 0x1BE	/*Partition information begins at offest 0x1BE */
#define NTFS_TYPE 0x07	/*NTFS partitions are represented by 0x07 in the partition table */
#define MFT_RECORD_LENGTH 1024 /*MFT entries are 1024 bytes long */
#define MFT_META_HEADERS 6 /*The first 16 MFT entries are reserved for metadata files */

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
				  "Media descriptor: %s\n"
				  "Physical sectors per track: %u\n"
				  "Number of heads: %u\n"
				  "Hidden sectors: %d\n"
				  "Sectors in volume: %" PRId64 "\n"
				  "Size of volume sectors: %0.2f mb\n"
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
				  (bootSec->bpb).uchMediaDescriptor == 0xF8?"Hard Disk":"Other",
				  (bootSec->bpb).wSecPerTrack,
				  (bootSec->bpb).wNumberOfHeads,
				  (bootSec->bpb).dwHiddenSec,
				  (bootSec->bpb).n64TotalSec,
				  (bootSec->bpb).wBytesPerSec*(bootSec->bpb).n64TotalSec/(1024.0*1024.0),
				  (bootSec->bpb).n64MFTLogicalClustNum,	/*Cluster number for MFT! */
				  (bootSec->bpb).n64MFTMirrLoficalClustNum,
				  (bootSec->bpb).nClustPerMFTRecord,
				  (bootSec->bpb).nClustPerIndexRecord,
				  (bootSec->bpb).n64VolumeSerialNum,
				  (bootSec->bpb).dwCheckSum,
				  bootSec->wSecMark );
	return 0;
}

int getNTFSAttrib(char* buff, NTFS_MFT_FILE_ENTRY_HEADER *mftFileEntry) {
	sprintf(buff, "File signature: %s\n"
				  "Offset to the update sequence: %u\n"
				  "Number of entries in fixup array: %u\n" /*Number times entry written */
				  "$LogFile Sequence Number (LSN): %" PRId64 "\n"
				  "Sequence number %u\n"
				  "Hard link count: %u\n"
				  "Offset to first attribute: %u\n"
				  "Flags: %u\n"
				  "Used size of MFT entry: %d\n"
				  "Allocated size of MFT entry: %d\n"
				  "File reference to the base FILE record: %" PRId64 "\n" /*Non zero if attributes not contained in MFT record */
				  "Next attribute ID: %u\n"
				  "wFixUpPattern: %u\n" /*Recognise partially written clusters */
				  "Number of this MFT record: %d\n",
				  mftFileEntry->fileSignature,
				  mftFileEntry->wFixupOffset,
				  mftFileEntry->wFixupSize,
				  mftFileEntry->n64LogSeqNumber,
				  mftFileEntry->wSequence,
				  mftFileEntry->wHardLinks,
				  mftFileEntry->wAttribOffset,
				  mftFileEntry->wFlags,
				  mftFileEntry->dwRecLength,
				  mftFileEntry->dwAllLength,
				  mftFileEntry->n64BaseMftRec,
				  mftFileEntry->wNextAttrID,
				  mftFileEntry->wFixUpPattern,
				  mftFileEntry->dwMFTRecNumber);
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

	/*Find the root directory metafile entry in the MFT, and extract its index allocation attributes */
	/*Find and extract the first 16 MFT entries which are reserved for metadata */
	NTFS_MFT_FILE_ENTRY_HEADER **mftMetaHeaders;
	mftMetaHeaders = malloc(MFT_META_HEADERS*sizeof(NTFS_MFT_FILE_ENTRY_HEADER));
	/*Move file pointer to the absolute MFT location */
	if(u64bytesAbsoluteMFT > 0) {
		if((blk_offset = lseek(fileDescriptor, u64bytesAbsoluteMFT, SEEK_SET)) == -1) {
			int errsv = errno;
			printf("Failed to position file pointer to NTFS MFT with error: %s.\n", strerror(errsv));
			return EXIT_FAILURE;
		}
	}
	for(i = 0; i < MFT_META_HEADERS; i++) { /*For each of the 16 headers */
		mftMetaHeaders[i] = malloc(sizeof(NTFS_MFT_FILE_ENTRY_HEADER)); /*Allocate for the file header */
		/* Read the MFT entry */
		if((readStatus = read( fileDescriptor, mftBuffer, MFT_RECORD_LENGTH)) == -1) { /*Read the next record */
			int errsv = errno;
			printf("Failed to read MFT at offset: %" PRIu64 ", with error %s.\n", u64bytesAbsoluteMFT, strerror(errsv));
			return EXIT_FAILURE;
		} else {
			memcpy(mftMetaHeaders[i], mftBuffer, sizeof(NTFS_MFT_FILE_ENTRY_HEADER)); /* Copy header from record*/
			printf("\nRead MFT record %d into buffer.\n", i);
			/*--------------------- Get MFT Record file entry headers ---------------------*/
			getNTFSAttrib(buff, mftMetaHeaders[i]);
			printf("%s\n", buff);

			NTFS_ATTRIBUTE *mftRecAttrib, *mftRecAttribTemp = malloc( sizeof(NTFS_ATTRIBUTE) );
			uint16_t attribOffset = mftMetaHeaders[i]->wAttribOffset; /*Offset of the first attribute */
			/*---------------------- Follow attribute(s) offset position(s) ---------------------*/
			do {
				memcpy(mftRecAttribTemp, mftBuffer+attribOffset, sizeof(NTFS_ATTRIBUTE));
				/*Determine actual attribute length and use to copy full attribute */
				mftRecAttrib = malloc(mftRecAttribTemp->dwFullLength);
				memcpy(mftRecAttrib, mftBuffer+attribOffset, mftRecAttribTemp->dwFullLength);
				//getMFTAttribMembers(buff, mftRecAttrib);
				//printf("%s\n", buff);

				if(mftRecAttrib->dwType == STANDARD_INFORMATION) { /*if is STANDATRD_INFORMATION attribute */
					printf("STANDARD_INFORMATION attribute\n");
				}
				else if(mftRecAttrib->dwType == ATTRIBUTE_LIST) {
					printf("ATTRIBUTE_LIST attribute\n");
				}
				else if(mftRecAttrib->dwType == FILE_NAME) { /*If is FILE_NAME attribute */
					printf("FILE_NAME attribute:\n");
					FILE_NAME_ATTR *fileNameAttr = malloc( mftRecAttrib->Attr.Resident.dwLength + 1 );
					memcpy(fileNameAttr, mftBuffer+attribOffset+(mftRecAttrib->Attr).Resident.wAttrOffset,
										 	 	 	 	 	 	(mftRecAttrib->Attr).Resident.dwLength);
					printf("\tFile name length: %u\n", fileNameAttr->bFileNameLength);
					uint16_t* fileName = fileNameAttr->arrUnicodeFileName;
					//fileName[fileNameAttr->bFileNameLength] = 0;
					printf("\tFilename Namespace: %u\n", fileNameAttr->bFilenameNamespace);
					printf("\tFile name: ");
					int  k = 0;
					for(; k<fileNameAttr->bFileNameLength; k++) {
						printf("%c", fileName[k]&0xFF);
					}
					//printf(fileNameAttr->arrUnicodeFileName);
					printf("\n");
					free(fileNameAttr);
				} else if(mftRecAttrib->dwType == OBJECT_ID ) {
					printf("OBJECT_ID attribute\n");
				} else if(mftRecAttrib->dwType == SECURITY_DESCRIPTOR) {
					printf("SECURITY_DESCRIPTOR attribute\n");
				} else if(mftRecAttrib->dwType == VOLUME_NAME) {
					/*This attribute simply contains the name of the volume. (In UNICODE!) */
					printf("VOLUME_NAME attribute\n");
					VOLUME_NAME_ATTR *vNameAttr = malloc( mftRecAttrib->Attr.Resident.dwLength + 1);
					memcpy(vNameAttr, mftBuffer+attribOffset+(mftRecAttrib->Attr).Resident.wAttrOffset,
															 (mftRecAttrib->Attr).Resident.dwLength);
					printf("\tVolume Name: %s\n", vNameAttr->arrUnicodeVolumeName);
					free(vNameAttr);
				} else if(mftRecAttrib->dwType == VOLUME_INFORMATION) {
					printf("VOLUME_INFORMATION attribute\n");
				} else if(mftRecAttrib->dwType == DATA) {
					printf("DATA attribute\n");
				} else if(mftRecAttrib->dwType == INDEX_ROOT) {
					/*This is the root node of the B+ tree that implements an index (e.g. a directory).
					  This file attribute is always resident. */
					printf("INDEX_ROOT attribute\n");
				} else if(mftRecAttrib->dwType == INDEX_ALLOCATION) {
					printf("INDEX_ALLOCATION attribute\n");
				} else if(mftRecAttrib->dwType == BITMAP) {
					/*This file attribute is a sequence of bits, each of which represents the status of an entity. */
					printf("BITMAP attribute\n");
				} else if(mftRecAttrib->dwType == REPARSE_POINT) {
					printf("REPARSE_POINT attribute\n");
				} else if(mftRecAttrib->dwType == EA_INFORMATION) {
					printf("EA_INFORMATION attribute\n");
				} else if(mftRecAttrib->dwType == EA) {
					printf("EA attribute\n");
				} else if(mftRecAttrib->dwType == LOGGED_UTILITY_STREAM) {
					printf("LOGGED_UTILITY_STREAM attribute\n");
				} else {
					printf("Unknown attribute of type: %d.\n", mftRecAttrib->dwType);
				}
				printf("\t%s\n", mftRecAttrib->uchNonResFlag==TRUE?"Non-Resident.":"Resident.");
				/*--- If the attribute data is non-resident then... */
				if(mftRecAttrib->uchNonResFlag==TRUE) {
					uint64_t realSize = (mftRecAttrib->Attr).NonResident.n64RealSize;
					printf("\tReal size: %" PRId64 " bytes.\n", realSize);

					uint16_t dataRunOffset = (mftRecAttrib->Attr).NonResident.wDatarunOffset; /*Starting cluster for data */
					BYTE lenOffsetSize = 0xFF&dataRunOffset; /*First byte from read position, */
					/*Top four bits represent a length, and the last four bits represent an offset. */
					printf("\tdataRunOffset: %u, lenOffsetSize = %u\n", dataRunOffset, lenOffsetSize);
					LEN_OFFS_BITFIELD *len_offset_bitField = malloc( sizeof(LEN_OFFS_BITFIELD) );
					memcpy(len_offset_bitField, &lenOffsetSize, sizeof(LEN_OFFS_BITFIELD) );
					dataRunOffset++; /*Next read position */
					/*The four bit length field in our bit field union contains
					the length in bytes from which to copy the full length of the data run.
					This length will never exceed eight bytes in length, but can often be
					much shorter */
					printf("\tlength: %u ", len_offset_bitField->bitfield.length);
					printf("offset: %u\n", len_offset_bitField->bitfield.offset);
					uint64_t *dataRunLength = malloc( sizeof(uint64_t) );
					memcpy(dataRunLength, mftBuffer+attribOffset+dataRunOffset, len_offset_bitField->bitfield.length);
					/*To continue, create an eight byte value which will hold a copied record
					length for our first data run.
					Copy in the data from the current record position at the
					specific length, and advance the current read position by that same length to
					move to the next read offset.*/
					printf("\t%" PRIu64 "\n", *dataRunLength);

					free(len_offset_bitField);
					free(dataRunLength);
				}
				attribOffset += mftRecAttrib->dwFullLength; /*Increment the offset by the length of this attribute */
			} while(attribOffset+8 < mftMetaHeaders[i]->dwRecLength); /*While there are attributes left to inspect */
			free(mftRecAttrib);
			free(mftRecAttribTemp);
		}
	}

	/*---------------------------------- Tidy up ----------------------------------*/
	for(i = 0; i<P_PARTITIONS; i++) {
		free(priParts[i]);	/*Free the memory allocated for primary partition structs */
	} free(priParts);
	for(i=0; i<nNTFS; i++) {
		free(nTFSParts[i]); /*Free the memory allocated for NTFS partition structs */
	} free(nTFSParts);
	for(i = 0; i<MFT_META_HEADERS; i++) {
		free(mftMetaHeaders[i]);
	}free(mftMetaHeaders);	/*Free the memory allocated for the NTFS metadata files*/

	free(nTFS_Boot);		/*Free Boot sector memory */
	//free(firstMFTHeader);	/*48b First MFT entry header*/
	free(buff); 			/*Used for buffering various texts */
	free(mftBuffer);		/*Used for buffering one MFT record, 1kb*/

	if((close(fileDescriptor)) == -1) { /*close block device and check if failed */
		int errsv = errno;
		printf("Failed to close block device %s with error: %s.\n", BLOCK_DEVICE, strerror(errsv));
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
