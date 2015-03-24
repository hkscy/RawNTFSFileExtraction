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
#include <math.h>
#include <stdlib.h>
#include <sys/types.h> 	/*Various data types used elsewhere */
#include <sys/stat.h>  	/*File information (stat et al) */
#include <fcntl.h>     	/*File opening, locking and other operations */
#include <unistd.h>
#include <errno.h>     	/*For errno */
#include <stdint.h>    	/*unitN_t */
#include <string.h>
#include <inttypes.h>  	/*for (u)int64_t format specifiers PRId64 and PRIu64 */
#include <wchar.h>	   	/*for formatted output of wide characters. */
#include <stdbool.h>	/*C99 boolean type */

#include "NTFSStruct.h"
#include "NTFSAttributes.h"
#include "RunList.h"
#include "Debug.h"
#include "Utility.h"
#include "FileLUT.h"
#include "UserInterface.h"

#define BUFFSIZE 1024			/*Generic data buffer size */
#define P_PARTITIONS 4			/*Number of primary partitions */
#define SECTOR_SIZE 512			/*Size of one sector */
#define P_OFFSET 0x1BE			/*Partition information begins at offset 0x1BE */
#define NTFS_TYPE 0x07			/*NTFS partitions are represented by 0x07 in the partition table */
#define MFT_RECORD_LENGTH 1024 	/*MFT entries are 1024 bytes long */
#define MFT_FILE_ATTR_PAD 8

#define IN_USE		0x01		/*MFT FILE0 record flags */
#define DIRECTORY	0x02

static const char BLOCK_DEVICE[] = "/dev/mechastriessand/windows7";

/*Information methods which print to the buffer pointer given */
int getPartitionInfo(char *buff, PARTITION *part);
int getBootSectInfo(char* buff, NTFS_BOOT_SECTOR *bootSec);
int getFILE0Attrib(char* buff, NTFS_MFT_FILE_ENTRY_HEADER *mftFileEntry);
int getMFTAttribMembers(char * buff, NTFS_ATTRIBUTE* attrib);

/*Seek methods for block device file descriptor */
int lseekAbs(int fileDescriptor, off_t offset);
int lseekRel(int fileDescriptor, off_t offset);

/*Utilities */
int64_t roundToNearestCluster(int64_t sec_offs, int32_t secPerClus);
int extractResFile(char * fileName, void * dataAttr, uint32_t len);

uint16_t blkDevDescriptor = 0;		/*File descriptor for block device */
off_t blk_offset = 0;
uint32_t dwBytesPerCluster = -1;  	/*Bytes per cluster on the disk */
uint64_t relativePartSector = -1; 	/*Relative offset in bytes of the NTFS partition table */

FILE * MFT_offline_copy;

int main(int argc, char* argv[]) {
	ssize_t readStatus;
	int workingPartition = -1;
	uint64_t u64bytesAbsoluteMFT = -1;
	char* buff = malloc( BUFFSIZE );	/*Used for getPartitionInfo(...), getBootSectinfo(...) et al*/
	char* mftBuffer = malloc( MFT_RECORD_LENGTH ); /*Buffer an entire MFT Record here*/

	system("clear"); /*Clear terminal window */
	printf("Launching raw NTFS extraction engine for %s\n", BLOCK_DEVICE);

	/*Open block device in read-only mode */
	if((blkDevDescriptor = open(BLOCK_DEVICE, O_RDONLY)) == -1 ) {
		int errsv = errno;
		printf("Failed to open block device %s with error: %s.\n", BLOCK_DEVICE, strerror(errsv));
		return EXIT_FAILURE;
	}

	/*Seek partition table  */
	lseekAbs(blkDevDescriptor, P_OFFSET);

	/*--------------------- Read in primary partitions from MBR ---------------------*/
	printf("Reading primary partition data: ");
	PARTITION **priParts = malloc( P_PARTITIONS*sizeof(PARTITION) );
	PARTITION **nTFSParts = malloc( P_PARTITIONS*sizeof(PARTITION) );
	int i, nNTFS = 0;
	/*Iterate the primary partitions in MBR to look for NTFS partitions */
	for(i = 0; i < P_PARTITIONS; i++) {
		priParts[i] = malloc( sizeof(PARTITION) );
		if((readStatus = read( blkDevDescriptor, priParts[i], sizeof(PARTITION))) == -1){
			int errsv = errno;
			printf("Failed to open partition table with error: %s.\n", strerror(errsv));
		} else {
			if(priParts[i]->chType == NTFS_TYPE) {	/*If this partition is an NTFS entity */
				nTFSParts[nNTFS] = malloc( sizeof(PARTITION) );
				nTFSParts[nNTFS++] = priParts[i]; 	/*Increment the NTFS parts counter */
				if(DEBUG) {
					getPartitionInfo(buff, priParts[i]);
					printf("\nPartition %d:\n%s\n",i, buff);
				}
			}
		}
	}
	if(nNTFS < 1) { /*Can't continue if there's no NTFS partitions */
		printf("No NTFS partitions found, please check user privileges.\n");
		printf("Can't continue\n");
		return EXIT_FAILURE;
	} else {
		printf("%u NTFS partitions located.\n", nNTFS);
	}


	/*-------------- Follow relative sector offset of NTFS partitions ---------------*/
	for(workingPartition = 0; workingPartition < nNTFS; workingPartition++) {
		NTFS_BOOT_SECTOR *nTFS_Boot = malloc( sizeof(NTFS_BOOT_SECTOR) );
		relativePartSector = nTFSParts[workingPartition]->dwRelativeSector*SECTOR_SIZE;

		/*Set offset pointer to partition table */
		lseekAbs(blkDevDescriptor, relativePartSector);

		if((readStatus = read(blkDevDescriptor, nTFS_Boot, sizeof(NTFS_BOOT_SECTOR))) == -1) {
			int errsv = errno;
			printf("Failed to open NTFS Boot sector for partition %d with error: %s.\n",i , strerror(errsv));
		} else {
			printf("\nExtracting MFT from partition %d\n", workingPartition);
		}
		if(nTFSParts[workingPartition]->chBootInd == 0x08) { /*If this is a Bootable NTFS partition -0x80*/
			printf("\tThis is the boot partition.\n");
		}

		/*------ If NTFS boot sector found then use it to find Master File Table -----*/
		if (DEBUG) {
			getBootSectInfo(buff, nTFS_Boot);
			printf("\nNTFS boot sector data\n%s\n", buff);
		}
		/*Calculate the number of bytes per sector = sectors per cluster * bytes per sector */
		dwBytesPerCluster = (nTFS_Boot->bpb.uchSecPerClust) * (nTFS_Boot->bpb.wBytesPerSec);
		if (DEBUG) printf("Filesystem Bytes Per Cluster: %d\n", dwBytesPerCluster);
		/*Calculate the number of bytes by which the boot sector is offset on disk */
		uint64_t u64bytesAbsoluteSector = (nTFS_Boot->bpb.wBytesPerSec) * (nTFSParts[workingPartition]->dwRelativeSector);
		if (DEBUG) printf("Bootsector offset in bytes: %" PRIu64 "\n", u64bytesAbsoluteSector );
		/*Calculate the relative bytes location of the MFT on the partition */
		uint64_t u64bytesRelativeMFT = dwBytesPerCluster * (nTFS_Boot->bpb.n64MFTLogicalClustNum);
		if (DEBUG) printf("Relative bytes location of MFT: %" PRIu64 "\n", u64bytesRelativeMFT);
		/*Absolute MFT offset in bytes*/
		u64bytesAbsoluteMFT = u64bytesAbsoluteSector + u64bytesRelativeMFT;
		if (DEBUG) printf("Absolute MFT location in bytes: %" PRIu64 "\n", u64bytesAbsoluteMFT);

		/*Find the root directory metafile entry in the MFT, and extract its index allocation attributes */
		/*$MFT is always the first MFT record, and it's mirror the second */
		NTFS_MFT_FILE_ENTRY_HEADER *mftMetaMFT;
		//mftMetaMFT = malloc( MFT_META_HEADERS*sizeof(NTFS_MFT_FILE_ENTRY_HEADER) );

		/*Move file pointer to the absolute MFT location */
		if(u64bytesAbsoluteMFT > 0) {
			lseekAbs(blkDevDescriptor, u64bytesAbsoluteMFT);
		}

		//for(i = 0; i < MFT_META_HEADERS; i++) { /*For each of the MFT entries */
		bool isMFTFile = false;	/*Set true only for the MFT entry */
		char * ascFileName = NULL;
		mftMetaMFT = malloc( sizeof(NTFS_MFT_FILE_ENTRY_HEADER) ); /*Allocate for the file header */
		/* Read the MFT entry */
		if((readStatus = read( blkDevDescriptor, mftBuffer, MFT_RECORD_LENGTH)) == -1) { /*Read the next record */
			int errsv = errno;
			printf("Failed to read MFT at offset: %" PRIu64 ", with error %s.\n",
													 u64bytesAbsoluteMFT, strerror(errsv));
			return EXIT_FAILURE;
		}
		/* Copy MFT record header*/
		memcpy(mftMetaMFT, mftBuffer, sizeof(NTFS_MFT_FILE_ENTRY_HEADER));
		if(DEBUG) printf("\nRead MFT record %d into buffer.\n", i);

		/*------------------------- Get MFT Record attributes ------------------------*/
		if(DEBUG) {
			getFILE0Attrib(buff, mftMetaMFT);
			printf("%s\n", buff);
		}

		NTFS_ATTRIBUTE *mftRecAttrib, *mftRecAttribTemp = malloc( sizeof(NTFS_ATTRIBUTE) );
		uint16_t attribOffset = mftMetaMFT->wAttribOffset; /*Offset to attributes */

		/*---------------------- Follow attribute(s) offset position(s) ---------------------*/
		do {
			memcpy(mftRecAttribTemp, mftBuffer+attribOffset, sizeof(NTFS_ATTRIBUTE));
			/*Determine actual attribute length and use to copy full attribute */
			mftRecAttrib = malloc(mftRecAttribTemp->dwFullLength);
			memcpy(mftRecAttrib, mftBuffer+attribOffset, mftRecAttribTemp->dwFullLength);
			if (VERBOSE & DEBUG) {
				getMFTAttribMembers(buff, mftRecAttrib);
				printf("%s\n", buff);
			}

			if(DEBUG && mftRecAttrib->dwType == STANDARD_INFORMATION) { /*if is STANDATRD_INFORMATION attribute */
				printf("STANDARD_INFORMATION attribute\n");
			}
			else if(DEBUG && mftRecAttrib->dwType == ATTRIBUTE_LIST) {
				printf("ATTRIBUTE_LIST attribute\n");
			}
			else if(mftRecAttrib->dwType == FILE_NAME) { /*If is FILE_NAME attribute */
				ascFileName = getFileName(mftRecAttrib, mftBuffer, attribOffset);
				/* Check if fileName == $MFT, set toggle */
				if( strcmp(ascFileName, "$MFT" ) == 0 ) {
					isMFTFile = true;
				} else {
					isMFTFile = false;
				}
			} else if(DEBUG && mftRecAttrib->dwType == OBJECT_ID ) {
				printf("OBJECT_ID attribute\n");
			} else if(DEBUG && mftRecAttrib->dwType == SECURITY_DESCRIPTOR) {
				printf("SECURITY_DESCRIPTOR attribute\n");
			} else if(DEBUG && mftRecAttrib->dwType == VOLUME_NAME) {
				/*This attribute simply contains the name of the volume. (In UNICODE!) */
				printf("VOLUME_NAME attribute\n");
				VOLUME_NAME_ATTR *vNameAttr = malloc( mftRecAttrib->Attr.Resident.dwLength + 1);
				memcpy(vNameAttr, mftBuffer + attribOffset + (mftRecAttrib->Attr).Resident.wAttrOffset,
														 	 (mftRecAttrib->Attr).Resident.dwLength);
				printf("\tVolume Name: %s\n", vNameAttr->arrUnicodeVolumeName);
				free(vNameAttr);
			} else if(DEBUG && mftRecAttrib->dwType == VOLUME_INFORMATION) {
				printf("VOLUME_INFORMATION attribute\n");
			} else if(DEBUG && mftRecAttrib->dwType == DATA) {
				printf("DATA attribute\n");
			} else if(DEBUG && mftRecAttrib->dwType == INDEX_ROOT) {
				/*This is the root node of the B+ tree that implements an index (e.g. a directory).
			  This file attribute is always resident. */
				printf("INDEX_ROOT attribute\n");
			} else if(DEBUG && mftRecAttrib->dwType == INDEX_ALLOCATION) {
				printf("INDEX_ALLOCATION attribute\n");
			} else if(DEBUG && mftRecAttrib->dwType == BITMAP) {
				/*This file attribute is a sequence of bits, each of which represents the status of an entity. */
				printf("BITMAP attribute\n");
			} else if(DEBUG && mftRecAttrib->dwType == REPARSE_POINT) {
				printf("REPARSE_POINT attribute\n");
			} else if(DEBUG && mftRecAttrib->dwType == EA_INFORMATION) {
				printf("EA_INFORMATION attribute\n");
			} else if(DEBUG && mftRecAttrib->dwType == EA) {
				printf("EA attribute\n");
			} else if(DEBUG && mftRecAttrib->dwType == LOGGED_UTILITY_STREAM) {
				printf("LOGGED_UTILITY_STREAM attribute\n");
			} else {
				if(DEBUG) printf("Unknown attribute of type: %d.\n", mftRecAttrib->dwType);
			}
			/* Is attribute resident? */
			if(DEBUG) printf("\t%s ", mftRecAttrib->uchNonResFlag==true?"Non-Resident.":"Resident.");

			if(mftRecAttrib->uchNonResFlag==false) { /*Is resident */
				uint32_t attribDataSize = (mftRecAttrib->Attr.Resident).dwLength;
				if(DEBUG && VERBOSE) {
					printf("\tData size: %d Bytes.\n", attribDataSize);
					size_t attribDataOffset = (mftRecAttrib->Attr.Resident).wAttrOffset;
					uint16_t * residentData = malloc( attribDataSize );
					/* Dump raw resident-attribute data for debugging purposes. */
					memcpy(residentData, mftBuffer+attribOffset+attribDataOffset, attribDataSize);
					int  k = 0;
					printf("\tSize of data: %lu Raw attribute data: ", attribDataSize/sizeof(uint16_t));
					for(; k<attribDataSize/sizeof(uint16_t); k++) {
						printf("%04x: ", residentData[k]);
						printf("%c ", residentData[k]&0xFF);
					}
					printf("\n");
					free(residentData);
				}
			}
			/*--------- If the attribute data is non-resident then... ---------*/
			else if(mftRecAttrib->uchNonResFlag==true) {
				uint8_t countRuns = 0;
				OFFS_LEN_BITFIELD *offs_len_bitField = malloc( sizeof(OFFS_LEN_BITFIELD) );
				uint64_t realSize = (mftRecAttrib->Attr).NonResident.n64RealSize;

				/*Offset to data runs */
				uint16_t dataRunOffset = (mftRecAttrib->Attr).NonResident.wDatarunOffset;
				if(DEBUG) {
					printf("\tReal file size: %" PRId64 " bytes.\n", realSize);
					printf("\tData run offset in attribute header: %u out of %u\n", dataRunOffset, mftRecAttrib->dwFullLength);
					printf("\tProcessing run list...\n");
				}

				DataRun *p_head = NULL; /*Allocate for runlist */
				do {
					uint64_t *length = malloc( sizeof(uint64_t) ); /*The length and offset data run fields are always 8 or less bytes */
					int64_t *offset = malloc( sizeof(int64_t) ); /* Offset is signed */
					/*Follow offset to data runs, read first data run. */
					/*First read it's offset and length nibbles using the bitfield */
					/*Top four bits represent a length, and the last four bits represent an offset. */
					if(countRuns == 0) {
						memcpy(offs_len_bitField, mftBuffer+attribOffset+dataRunOffset, sizeof(OFFS_LEN_BITFIELD));
					}
					if(DEBUG && VERBOSE) {
						printf("\tlength of length field of datarun: %u ", offs_len_bitField->bitfield.lengthSize);
						printf("\tlength of offset field of datarun: %u\n", offs_len_bitField->bitfield.offsetSize);
					}

					*length = 0; /*Initialise to zero since values may be less than 8 bytes long */
					*offset = 0;
					dataRunOffset++; /*Move offset past offset_length_union */

					/*Copy length field from run list */
					memcpy(length, mftBuffer+attribOffset+dataRunOffset, offs_len_bitField->bitfield.lengthSize);
					dataRunOffset+=offs_len_bitField->bitfield.lengthSize; /*Move offset past length field */

					/*Copy offset field from run list */
					memcpy(offset, mftBuffer+attribOffset+dataRunOffset, offs_len_bitField->bitfield.offsetSize);
					dataRunOffset+=offs_len_bitField->bitfield.offsetSize; /*Move offset past offset field */

					p_head = addRun(p_head, length, offset);
					if(DEBUG && VERBOSE) {
						printf("\tLength of datarun: %" PRIu64 " clusters\t", *length);
						printf("\tVCN offset to datarun: %" PRId64 " clusters\n", *offset);
					}
					countRuns++;

					/*Copy next bitfield header, check if == 0 for loop termination */
					memcpy(offs_len_bitField, mftBuffer+attribOffset+dataRunOffset, sizeof(OFFS_LEN_BITFIELD));
				} while(offs_len_bitField->val != 0);

				p_head = reverseList(p_head);
				if(DEBUG) {
					printRuns(buff, p_head);
					printf("%s", buff);
					printf("\tFinished processing %u data runs from runlist\n", countRuns);
				}

				/*Now.. I need the DATA attribute from the MFT, so check */
				/*If this is it, then extract it to a local file*/
				if(isMFTFile && (mftRecAttrib->dwType == DATA)) {
					printf("\t$MFT meta file found.\n");
					uint8_t fileNameLen = snprintf(NULL, 0, "%s%d", ascFileName , workingPartition) + 1; // \0 terminated
					char * fileName = malloc( fileNameLen );

					snprintf(fileName, fileNameLen, "%s%d.data", ascFileName, workingPartition);
					if((MFT_offline_copy = fopen(fileName, "w+")) == NULL) {/*Open/create file, r/w pointer at start */
						int errsv = errno;
						printf("Failed to create local file for storing %s: %s.\n", ascFileName, strerror(errsv));
						return EXIT_FAILURE;
					}
					if(countRuns > 1) {
						printf("\t%s is fragmented on disk, located %u fragments.\n", ascFileName, countRuns);
					}
					printf("\tWriting DATA attribute to local %s file\n", fileName);
					free(fileName);

					off_t offset_restore = blk_offset; /*Backup the current read offset */

					/* Move file pointer to start of partition */
					if((blk_offset = lseek( blkDevDescriptor, relativePartSector, SEEK_SET )) == -1) {
						int errsv = errno;
						printf("Failed to set file pointer with error: %s.\n", strerror(errsv));
						return EXIT_FAILURE;
					}

					/*Iterate through the runlist and extract data*/
					DataRun *p_current_item = p_head;
					uint64_t sizeofMFT = 0;
					while (p_current_item) {
						if (p_current_item->offset && p_current_item->length) {
							off_t nonResReadFrom =  dwBytesPerCluster*(*p_current_item->offset);
							if(DEBUG) {
								printf("\t%" PRIu64 "\t%" PRId64 "\n", *p_current_item->offset, *p_current_item->length);
								printf("\tnonResReadFrom: %" PRId64 "\n", nonResReadFrom);
							}

							/* Move file pointer to the cluster */
							lseekRel(blkDevDescriptor, nonResReadFrom);
							if(DEBUG && VERBOSE) printf("\tblk_offset = %" PRId64 "\n", blk_offset);

							size_t readLength = dwBytesPerCluster*(*p_current_item->length);
							char * dataRun = malloc( readLength );

							/*Read for length specified in dataRun */
							if((readStatus = read(blkDevDescriptor, dataRun, readLength)) == -1 ){
								int errsv = errno;
								printf("Failed to open read MFT from disk with error: %s.\n", strerror(errsv));
							} else {
								/*Create special fragment header and write to file before records in fragment */
								FRAG *frag = createFragRecord(blk_offset);
								if( fwrite( frag, sizeof(FRAG), 1, MFT_offline_copy ) != 1) {
									int errsv = errno;
									printf("Failed to write MFT to local file with error: %s.\n", strerror(errsv));
									return EXIT_FAILURE;
								}
								/*Copy the memory to the local file */
								if( fwrite( dataRun, readLength, 1, MFT_offline_copy ) != 1) {
									int errsv = errno;
									printf("Failed to write MFT to local file with error: %s.\n", strerror(errsv));
									return EXIT_FAILURE;
								}
								sizeofMFT += readLength;
								free(frag);
							}
							free(dataRun);
						} else {
							if(DEBUG) printf("\tNo data.\n");
						}
						/*Rewind position (-1) by length of the data run */
						lseekRel(blkDevDescriptor, (-1)*(*p_current_item->length)*dwBytesPerCluster);
						p_current_item = p_current_item->p_next; /*Advance position in list */
					} // while (p_current_item)
					printf("\tSize of MFT extracted from partition %u: %" PRId64 " bytes\n", workingPartition, sizeofMFT);

					/*Restore read position on block device */
					lseekAbs(blkDevDescriptor, offset_restore);

				}// end of if(isMFTFile && (mftRecAttrib->dwType == DATA))

				freeList(p_head);
				free(offs_len_bitField);
				//free(p_head); Can't free this yet.
			}
			attribOffset += mftRecAttrib->dwFullLength; /*Increment the offset by the length of this attribute */
		} while( attribOffset+MFT_FILE_ATTR_PAD < mftMetaMFT->dwRecLength ); /*While there are attributes left to inspect */
		free(mftRecAttrib);
		free(mftRecAttribTemp);
		if(ascFileName != NULL) {
			free(ascFileName);
		}
		free(nTFS_Boot);		/*Free Boot sector memory */

	} //for(workingPartition = 0; workingPartition < nNTFS; workingPartition++) {

	/*Close local file copy of MFT if open */
	if(MFT_offline_copy!=NULL) {
		fclose(MFT_offline_copy);
	}

	/*------------------- Process FILE records from extracted MFT  ------------------*/
	printf("\nProcessing MFT...\n");
	int countRecords = 0;
	int32_t secPerClus = dwBytesPerCluster/SECTOR_SIZE;

	/*Open file, r pointer at start */
	if((MFT_offline_copy = fopen("$MFT1", "r+")) == NULL) {
		int errsv = errno;
		printf("Failed to open file: %s.\n", strerror(errsv));
		return EXIT_FAILURE;
	}

	/*Set fp to beginning of file */
	if((fseek(MFT_offline_copy, 0, SEEK_SET)) != 0) {
		int errsv = errno;
		printf("Error handing local MFT file copy: %s.\n", strerror(errsv));
		return EXIT_FAILURE;
	}

	NTFS_MFT_FILE_ENTRY_HEADER *mftFileH = malloc( sizeof(NTFS_MFT_FILE_ENTRY_HEADER) ); /*Allocate for the FILE0 header */
	NTFS_ATTRIBUTE *mftRecAttrTmp = malloc( sizeof(NTFS_ATTRIBUTE) ); /*Attribute header size */
	NTFS_ATTRIBUTE *mftRecAttr = malloc( MFT_RECORD_LENGTH ); /*Attribute size << record size */

	int countFiles = 0, countDelEntity = 0, countDir = 0, countOther = 0;
	int countBadAttr = 0;
	int countFileNames = 0;
	int countFrags = 0;
	int relRecN = 0; /*Relative record number, needed for calculating offset to record on disk */

	File *offl_files = NULL; /* Init the list of files to be constructed */
	uint64_t u64bytesAbsMFTOffset = 0;
	int64_t d64segAbsMFTOffset = 0;

	while((readStatus = fread(mftBuffer, MFT_RECORD_LENGTH, 1, MFT_offline_copy)) != 0) {
		/*Read in one whole MFT record to mftBuffer, each time loop iterates */
		/*Extract MFT header */
		memcpy(mftFileH, mftBuffer, sizeof(NTFS_MFT_FILE_ENTRY_HEADER));
		if(VERBOSE && DEBUG) {
			getFILE0Attrib(buff, mftFileH);
			printf("%s\n", buff);
		}
		/*Fragment records keep track of the MFT fragment from which records originated */
		/*Each fragment record starts with signature 'FRAG', check this */
		if(mftFileH->fileSignature[0] == 'F' &&
		   mftFileH->fileSignature[1] == 'R' &&
		   mftFileH->fileSignature[2] == 'A' &&
		   mftFileH->fileSignature[3] == 'G') {
				printf("MFT Fragment record found\n");
				FRAG *frag = malloc( sizeof(FRAG) );
				memcpy(frag, mftBuffer, sizeof(FRAG));
				u64bytesAbsMFTOffset = frag->u64fragOffset;
				d64segAbsMFTOffset = u64bytesAbsMFTOffset/SECTOR_SIZE;
				printf("\tOffset for the records that follow: %" PRIu64 "\n", d64segAbsMFTOffset);
				free(frag);
				countFrags++;
				relRecN = 0; /*Reset for each fragment's records */
		}

		/*Each subsequent file record should  start with signature 'FILE0', check this. */
		else if(strcmp(mftFileH->fileSignature, "FILE0") == 0) {

			char * aFileName = NULL;	/*Set for files which have this attribute */
			bool hasDataAttr = false;	/*Set for files which have $DATA */
			BYTE uchNonResFlag;			/*If hasDataAttr then set */
			// size_t resDataOffset = 0;	/*Offset to non-resident data attribute in record */
			uint32_t resDataSize = 0;
			DataRun *runList = NULL; 	/*Allocate for non-resident $DATA runlist */

			/*---------------------------- Get MFT Record attributes ---------------------------*/
			uint16_t attrOffset = mftFileH->wAttribOffset; 	 	    /*Offset to first attribute */
			do {
				/*------- Attribute size if unknown, so get header first which contains size -------*/
				memcpy(mftRecAttrTmp, mftBuffer+attrOffset, sizeof(NTFS_ATTRIBUTE));

				/*- NOTE: Some attributes have impossible record lengths > 1024, this breaks things -*/
				if(mftRecAttrTmp->dwFullLength > MFT_RECORD_LENGTH-attrOffset) {
					if(DEBUG) {
						printf("Bad record attribute:\n");
						getMFTAttribMembers(buff,mftRecAttrTmp);
						printf("%s\n", buff);
					}
					countBadAttr++;
					break;
				}
				/*-------- Determine actual attribute length and use to copy full attribute --------*/
				memcpy(mftRecAttr, mftBuffer+attrOffset, mftRecAttrTmp->dwFullLength);

				if(mftRecAttr->dwType == STANDARD_INFORMATION) {
					if(DEBUG) {
						STD_INFORMATION *stdInfo = malloc( sizeof(STANDARD_INFORMATION) );
						uint32_t fileP = getFilePermissions(stdInfo);
						printf("%" PRIu32 " ", fileP);
						free(stdInfo);
					}
				}

				/*---------------------------- Get file name from record ---------------------------*/
				/*------------------ Generally have more than one per actual file ------------------*/
				else if(mftRecAttr->dwType == FILE_NAME) { 					/*If is FILE_NAME attribute */
					aFileName = getFileName(mftRecAttr, mftBuffer, attrOffset);
					//printf("%s\n", aFileName);
					//free(aFileName);
					countFileNames++;
				}

				/*Get Directory information, resident.  */
				else if(mftRecAttr->dwType == INDEX_ROOT) {

				}

				/* Get Directory information, always non-resident (INDEX_ROOT is resident) */
				else if(mftRecAttr->dwType == INDEX_ALLOCATION) {

				}

				else if(mftRecAttr->dwType == DATA) {
					hasDataAttr = true;
					uchNonResFlag = mftRecAttr->uchNonResFlag;
					if(uchNonResFlag==true) { /*non-resident $DATA attribute  */

						uint8_t countRuns = 0;
						OFFS_LEN_BITFIELD *offs_len_bitField = malloc( sizeof(OFFS_LEN_BITFIELD) );
						uint16_t dataRunOffset = (mftRecAttr->Attr).NonResident.wDatarunOffset;

						do {
							uint64_t *length = malloc( sizeof(uint64_t) ); /*The length and offset data run fields are always 8 or less bytes */
							int64_t *offset = malloc( sizeof(int64_t) ); /* Offset is signed */
							/*Follow offset to data runs, read first data run. */
							/*First read it's offset and length nibbles using the bitfield */
							/*Top four bits represent a length, and the last four bits represent an offset. */
							if(countRuns == 0) {
								memcpy(offs_len_bitField, mftBuffer+attrOffset+dataRunOffset, sizeof(OFFS_LEN_BITFIELD));
							}

							*length = 0; *offset = 0;
							dataRunOffset++; /*Move offset past offset_length_union */

							/*Copy length field from run list */
							memcpy(length, mftBuffer+attrOffset+dataRunOffset, offs_len_bitField->bitfield.lengthSize);
							dataRunOffset+=offs_len_bitField->bitfield.lengthSize; /*Move offset past length field */
							/*Copy offset field from run list */
							memcpy(offset, mftBuffer+attrOffset+dataRunOffset, offs_len_bitField->bitfield.offsetSize);
							dataRunOffset+=offs_len_bitField->bitfield.offsetSize; /*Move offset past offset field */

							runList = addRun(runList, length, offset); /*Add extracted run to runlist */
							countRuns++;

							/*Copy next bitfield header, check if == 0 for loop termination */
							memcpy(offs_len_bitField, mftBuffer+attrOffset+dataRunOffset, sizeof(OFFS_LEN_BITFIELD));
						} while(offs_len_bitField->val != 0);
						runList = reverseList(runList);	/*Put the data runs in disk order */

					} else if(uchNonResFlag == false) { /* Non-resident file Data */
						resDataSize = (mftRecAttr->Attr.Resident).dwLength;
					}
				}

				attrOffset += mftRecAttr->dwFullLength;    /*Increment the offset by the length of this attribute */
			} while(attrOffset+MFT_FILE_ATTR_PAD < mftFileH->dwRecLength); /*While there are attributes left to inspect */

			countRecords++;
			//if(countRecords > 48) break; /*Debug break out */

			/* At this point we have all of the attributes and need to do something with them */
			/*Check file flags on record, determine record type */
			uint16_t mftFlags = mftFileH->wFlags;
			if(mftFlags==IN_USE) {	/*This is a file record */

				if(hasDataAttr) {	/*And it has $DATA */
					countFiles++;
					int64_t d64DataOffset = d64segAbsMFTOffset;
					int64_t relSecN = relRecN*(MFT_RECORD_LENGTH/SECTOR_SIZE);
					/* Need to round this value to the cluster which contains it */

					if(uchNonResFlag == false) {/*$DATA is resident */
						//u64DataOffset += (mftFileH->dwMFTRecNumber*MFT_RECORD_LENGTH) + resDataOffset;
						/* The Data Offset is only useful if you wanted to extract the data,
						 * In terms of locating the file which has been written to/read from
						 * the MFT record offset is probably better
						 * resDataOffset will stop things dividing by 512.0 byte segments - BAD.
						 */

						offl_files = addFile(offl_files, aFileName,
											   d64DataOffset+relSecN, /*Sector offset, specifies the actual record */
											   roundToNearestCluster(d64DataOffset+relSecN, secPerClus),
											   resDataSize,
											   mftFileH->dwMFTRecNumber);

					} else if (uchNonResFlag) {/*DATA is non-resident,  Iterate through the runlist and extract data*/
						uint32_t totalNonResSize = 0;
						DataRun *p_current_item = runList;
						while (p_current_item) {
							if (p_current_item->offset && p_current_item->length) {
								//off_t nonResDataOffs =  dwBytesPerCluster*(*p_current_item->offset);
								size_t nonResDataLen = dwBytesPerCluster*(*p_current_item->length);
								totalNonResSize += nonResDataLen;
							}
							p_current_item = p_current_item->p_next; /*Advance position in list */
						}
						offl_files = addFile(offl_files, aFileName,	/*Add file record for the MFT record */
											   //nonResDataOffs,
											   d64DataOffset+relSecN,
											   roundToNearestCluster(d64DataOffset+relSecN, secPerClus),
											   totalNonResSize,
											   mftFileH->dwMFTRecNumber);
					}
				}

			} else if (mftFlags==!IN_USE) {
				countDelEntity++;
			} else if (mftFlags==(IN_USE|DIRECTORY)) { /*This is a directory */
				countDir++;
			} else {
				countOther++;
				if(DEBUG)printf("%u\t", mftFlags);
			}
			relRecN++; /* Increment for each FILE record */

		} else {
			printf("MFT file corrupted.\n");
			return EXIT_FAILURE;
		}

	} //while((readStatus = fread(mftBuffer, MFT_RECORD_LENGTH, 1, MFT_file_copy)) != 0) {


	printf("\n%d MFT fragments\n", countFrags);
	printf("files: %d\tdirectories: %d\n"
			"deleted entities: %d\tOther entities: %d\n",
			countFiles, countDir,
			countDelEntity, countOther);
	printf("Bad record attributes: %d\n", countBadAttr);
	printf("File names: %d\n", countFileNames);
	printf("%d FILE records processed and stored offline.\n\n", countRecords);

	/*------------------------------ User interface to the program ------------------------------*/
	char cmd[CMD_BUFF];
	int8_t pRet = -1;
	char *searchTerm;
	do {
		printf("What do you want to do? \n");
		fgets(cmd, CMD_BUFF-1, stdin);
		switch(pRet = parseUserInput(cmd)) {
			case PRINT_HELP : ;
				printf(HELP);
				break;
			case PRINT_FILES : ;	/* Print list of files stored in offline MFT copy */
				printAllFiles(offl_files);
				break;
			case SRCH_FOR_MFTN : ;	/* Search offline MFT records using record number */
				while( strcmp((searchTerm = getSearchTerm()), EXIT_CMD) != 0 ) {
					searchFiles(offl_files, SRCH_NUM, searchTerm);
					free(searchTerm);
				}
				break;
			case SRCH_FOR_MFTC : ;	/* Search offline MFT records using record file name */
				while( strcmp((searchTerm = getSearchTerm()), EXIT_CMD) != 0 ) {
					searchFiles(offl_files, SRCH_NAME, searchTerm);
					free(searchTerm);
				}
				break;
			case SRCH_FOR_MFTO : ;	/* Search offline MFT records using record sector offset */
				while( strcmp((searchTerm = getSearchTerm()), EXIT_CMD) != 0 ) {
					searchFiles(offl_files, SRCH_OFFS, searchTerm);
					free(searchTerm);
				}
				break;
			case EXT_MFTN: ; /* Extract file using MFT record number( offline directory ) */
				while( strcmp((searchTerm = getSearchTerm()), EXIT_CMD) != 0 ) {

					File *found = searchFiles(offl_files, SRCH_NUM, searchTerm);
					while(found) {
						int64_t sOffsBytes = found->sec_offset*SECTOR_SIZE;
						off_t offs_restore = blk_offset; 			/*Backup current read position */
						lseekAbs(blkDevDescriptor, sOffsBytes);		/*Move to file record sector offset */

						/* Read the MFT entry */
						if((readStatus = read(blkDevDescriptor, mftBuffer, MFT_RECORD_LENGTH)) == -1) { /*Read the next record */
							int errsv = errno;
							printf("Failed to read MFT at offset: %" PRIu64 ", with error %s.\n",
																	 u64bytesAbsoluteMFT, strerror(errsv));
							return EXIT_FAILURE;
						}

						/* Copy MFT record header*/
						NTFS_MFT_FILE_ENTRY_HEADER *mftRecHeader = malloc( sizeof(NTFS_MFT_FILE_ENTRY_HEADER) );
						memcpy(mftRecHeader, mftBuffer, sizeof(NTFS_MFT_FILE_ENTRY_HEADER));

						/* Determine offset to record attributes from header */
						NTFS_ATTRIBUTE *mftRecAttr, *mftRecAttrTmp = malloc( sizeof(NTFS_ATTRIBUTE) );
						uint16_t attrOffset = mftRecHeader->wAttribOffset; /*Offset to attributes */

						do {
							/*----- Attribute size is unknown, so get header first which contains full size -----*/
							memcpy(mftRecAttrTmp, mftBuffer+attrOffset, sizeof(NTFS_ATTRIBUTE));

							/*- NOTE: Some attributes have impossible record lengths > 1024, this breaks things -*/
							if(mftRecAttrTmp->dwFullLength > MFT_RECORD_LENGTH-attrOffset) {
								printf("Bad record attribute:\n");
								getMFTAttribMembers(buff,mftRecAttrTmp);
								printf("%s\n", buff);
								break;
							}

							/*-------- Determine actual attribute length and use to copy full attribute --------*/
							mftRecAttr = malloc(mftRecAttrTmp->dwFullLength);
							memcpy(mftRecAttr, mftBuffer+attrOffset, mftRecAttrTmp->dwFullLength);

							if(mftRecAttr->dwType == DATA) {
								if(mftRecAttr->uchNonResFlag==false) { /*Is resident $DATA */
									uint32_t attrDataSize = (mftRecAttr->Attr.Resident).dwLength;
									size_t attbDataOffs = (mftRecAttr->Attr.Resident).wAttrOffset;
									if(DEBUG) printf("\tData size: %d Bytes.\n", attrDataSize);

									/* Extract the file to disk */
									extractResFile(found->fileName, mftBuffer+attrOffset+attbDataOffs, attrDataSize);

								} else if(mftRecAttr->uchNonResFlag==true) { /*non-resident $DATA attribute */
									printf("This record contains non-resident data.\n");
									printf("Operation not currently supported.\n");
								}
							}

							attrOffset += mftRecAttr->dwFullLength;    /*Increment the offset by the length of this attribute */
							free(mftRecAttr);
						} while(attrOffset+MFT_FILE_ATTR_PAD < mftRecHeader->dwRecLength); /*While there are attributes left to inspect */

						lseekAbs(blkDevDescriptor, offs_restore);		   /*Restore read position */
						freeFilesList(found);
						free(mftRecHeader);
						found = found->p_next; /*If there is more than one file found, go through them */
					}
					free(searchTerm);
				}
				break;
			case EXT_MFTCO: ; /* Extract file using QEMU write offset */

				/*Sector offsets rounded to the nearest cluster may contain up to 4 MFT records */
				/*Retrieve any records at the offset - do this online*/
				while( strcmp((searchTerm = getSearchTerm()), EXIT_CMD) != 0 ) {

					int64_t d64SearchTerm = strtoull(searchTerm, NULL, 0);
					if(d64SearchTerm >= 0) {
						int64_t sOffsBytes = d64SearchTerm*SECTOR_SIZE;
						off_t offs_restore = blk_offset; 			/*Backup current read position */
						lseekAbs(blkDevDescriptor, sOffsBytes);		/*Move to first file record sector offset */
						char *cBuff = malloc( dwBytesPerCluster );

						/* Read the cluster */
						if((readStatus = read(blkDevDescriptor, cBuff, dwBytesPerCluster)) == -1) {
							int errsv = errno;
							printf("Failed to read cluster at offset: %" PRIu64 ", with error %s.\n",
																	 u64bytesAbsoluteMFT, strerror(errsv));
							break; //return EXIT_FAILURE;
						}

						/* Try to read MFT records from the cluster memory*/
						//FILE *found = NULL;
						uint8_t recN = 0;
						char *mftBuff = malloc( MFT_RECORD_LENGTH );
						NTFS_MFT_FILE_ENTRY_HEADER *mftRecHeader = malloc( sizeof(NTFS_MFT_FILE_ENTRY_HEADER) );
						NTFS_ATTRIBUTE *mftRecAttrTmp = malloc( sizeof(NTFS_ATTRIBUTE) ); /*Attribute header size */
						NTFS_ATTRIBUTE *mftRecAttr = malloc( MFT_RECORD_LENGTH ); /*Attribute size << record size */

						for(; recN < dwBytesPerCluster/MFT_RECORD_LENGTH; recN++) {
							memcpy(mftBuff, cBuff+(recN*MFT_RECORD_LENGTH), MFT_RECORD_LENGTH);
							/* Copy MFT record header*/
							memcpy(mftRecHeader, mftBuff, sizeof(NTFS_MFT_FILE_ENTRY_HEADER));
							/* Check if this memory contains an MFT record, they all start 'FILE0' */
							if(strcmp("FILE0", mftRecHeader->fileSignature) == 0) {
								uint16_t attrOffs = mftRecHeader->wAttribOffset; 	 	    /*Offset to first attribute */
								char * fName = malloc( BUFFSIZE );
								do {
									/*------- Attribute size is unknown, so get header first which contains size -------*/
									memcpy(mftRecAttrTmp, mftBuff+attrOffs, sizeof(NTFS_ATTRIBUTE));

									/*- NOTE: Some attributes have impossible record lengths > 1024, this breaks things -*/
									if(mftRecAttrTmp->dwFullLength > MFT_RECORD_LENGTH-attrOffs) {
										if(DEBUG) {
											printf("Bad record attribute:\n");
											getMFTAttribMembers(buff,mftRecAttrTmp);
											printf("%s\n", buff);
										}
										countBadAttr++;
										break;
									}

									/*-------- Determine actual attribute length and use to copy full attribute --------*/
									memcpy(mftRecAttr, mftBuff+attrOffs, mftRecAttrTmp->dwFullLength);

									if(mftRecAttr->dwType == STANDARD_INFORMATION) { /*Contains create/modify stamps */
										STD_INFORMATION *stdInfo = malloc( sizeof(STANDARD_INFORMATION) );
										uint64_t altTime = stdInfo->fileAltTime;
										printf("File alt time: %" PRIu64 "\n", altTime);
										free(stdInfo);
									}

									else if(mftRecAttr->dwType == FILE_NAME) { /* Get file name */
										fName = getFileName(mftRecAttr, mftBuff, attrOffs);
										printf("\t%s\n", fName);
									}

									else if(mftRecAttr->dwType == DATA) {
										printf("Has data\n");
										if(mftRecAttr->uchNonResFlag==false) { /*$DATA is resident */
											uint32_t attrDataSize = (mftRecAttr->Attr.Resident).dwLength;
											size_t attrDataOffs = (mftRecAttr->Attr.Resident).wAttrOffset;
											printf("\tData size: %d Bytes.\n", attrDataSize);

											/* Extract the file to disk */
											extractResFile(fName, mftBuffer+attrOffs+attrDataOffs, attrDataSize);
										}
									}

									attrOffs += mftRecAttr->dwFullLength;    /*Increment the offset by the length of this attribute */
								} while(attrOffs+8 < mftRecHeader->dwRecLength); /*While there are attributes left to inspect */
								free(fName);
							}

						}

						lseekAbs(blkDevDescriptor, offs_restore);		   /*Restore read position */

						/*Check the modified time to help eliminate some records*/
						free(mftBuff);
						free(mftRecHeader);
						free(mftRecAttrTmp);
						free(mftRecAttr);
						free(cBuff);
					}
					free(searchTerm);
				}
				break;
			case UNKNOWN :
				printf("Command not recognised, try \'help\'\n");
				break;
		}
	} while( pRet != EXIT );

	/*--------------------------------------- Tidy up ---------------------------------------*/
	for(i = 0; i < P_PARTITIONS; i++) {
		free(priParts[i]);	/*Free the memory allocated for primary partition structs */
	} free(priParts);

	for(i=0; i < nNTFS; i++) {
		free(nTFSParts[i]); /*Free the memory allocated for NTFS partition structs */
	} free(nTFSParts);

	free(buff); 			/*Used for buffering various texts */
	free(mftBuffer);		/*Used for buffering one MFT record, 1kb*/
	free(mftFileH);
	free(mftRecAttr);
	free(mftRecAttrTmp);

	if((close(blkDevDescriptor)) == -1) { /*close block device and check if failed */
		int errsv = errno;
		printf("Failed to close block device %s with error: %s.\n", BLOCK_DEVICE, strerror(errsv));
		return EXIT_FAILURE;
	}

	if(MFT_offline_copy != NULL) {		/*Make sure the offline MFT copy is closed */
		fclose(MFT_offline_copy);
	}

	return EXIT_SUCCESS;
} //end of main method.

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

/**
 * Prints the members of a MFT File entry to buff.
 */
int getFILE0Attrib(char* buff, NTFS_MFT_FILE_ENTRY_HEADER *mftFileEntry) {
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

/*
 * Prints the members of an MFT FILE record attribute into buff.
 */
int getMFTAttribMembers(char * buff, NTFS_ATTRIBUTE* attrib) {
	char* tempBuff = malloc(BUFFSIZE);
	sprintf(tempBuff, "Attribute type: %d\n"// PRIu32 "\n"
				  	  "Length of attribute: %d\n"//%" PRIu32 "\n",
				  	  "Non-resident flag: %s\n"
				  	  "Length of name: %u\n"
				  	  "Offset to name: %u\n"
				  	  "Flags: %u\n"
				  	  "Attribute identifier: %u\n",
					  attrib->dwType,
					  attrib->dwFullLength,
					  attrib->uchNonResFlag == true ? "Non-Resident" : "Resident",
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

/**
 * Calls lseek(.. with error checking, absolute position mode.
 */
int lseekAbs(int fileDescriptor, off_t offset) {
	if((blk_offset = lseek(fileDescriptor, offset, SEEK_SET)) == -1) {
		int errsv = errno;
		printf("Failed to position file pointer with error: %s.\n", strerror(errsv));
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}

/**
 * Calls lseek(.. with error checking, relative position mode.
 */
int lseekRel(int fileDescriptor, off_t offset) {
	if((blk_offset = lseek(fileDescriptor, offset, SEEK_CUR)) == -1) {
		int errsv = errno;
		printf("Failed to position file pointer with error: %s.\n", strerror(errsv));
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
/**
 * Given the offset in sectors to an MFT record,
 * Returns the cluster which that sector is contained within.
 *
 * Created because experimentally file writes were seen targeting
 * whole clusters rather than individual sectors on Windows7.
 */
int64_t roundToNearestCluster(int64_t sec_offs, int32_t secPerClus) {
	if(sec_offs%secPerClus == 0) {
		return sec_offs;
	} else {
		return (sec_offs/secPerClus)*secPerClus; /*round down to nearest cluster */
	}
}

/**
 * Given a pointer to the memory containing data, the length of the data
 * and the name of the file to be written, writes the data to disk.
 */
int extractResFile(char * fileName, void * dataAttr, uint32_t len) {

	/* Copy resident file data from disk to memory. */
	char *residentData = malloc( len );
	memcpy(residentData, dataAttr, len);

	if(DEBUG) { /* Dump file data as hex/text to stdout */
		int  k = 0;
		printf("\t");
		for(; k<len; k++) {
			//printf("%04x", residentData[k]);
			printf("%c", residentData[k]);
		}
		printf("\n");
	}

	/*Create local file to which the NTFS file data is extracted */
	FILE *fileExtracted;
	if((fileExtracted = fopen(fileName, "w+")) == NULL) {
		int errsv = errno;
		printf("Failed to create local file for storing %s: %s.\n", fileName, strerror(errsv));
		return EXIT_FAILURE;
	}

	/*Write the file data */
	if( fwrite( residentData, len, 1, fileExtracted ) != len) {
		int errsv = errno;
		printf("Error extracting file: %s.\n", strerror(errsv));
		return EXIT_FAILURE;
	}

	/*Close the file */
	if(fileExtracted != NULL) {
		fclose(fileExtracted);
	}

	free(residentData);

	return EXIT_SUCCESS;
}
