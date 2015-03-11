/*
 * FileLUT.h
 *
 *  Created on: 9 Mar 2015
 *      Author: Christopher Hicks
 *
 * A linked list which links file name, offset on disk to the file location, and the
 * number of the corresponding MFT record.
 */
#include "UserInterface.h"

#ifndef FILELUT_H_
#define FILELUT_H_

/* Represents the information necessary to link file writes with file names on disk */
typedef struct _File {
	char *fileName;			/* File name defined in $FILE_NAME */
	uint64_t offset;		/* Offset in bytes to the file */
	uint32_t length;		/* Length of the file in bytes */
	uint32_t recordNumber;	/* MFT record number from which this originates*/
	struct _File *p_next;	/* Pointer to next file record */
} File;

/*
 * Adds a new file to the start of the list and returns it.
 */
File* addFile(File *p_head, char *fileName, uint64_t offset, uint32_t length, uint32_t recordNumber) {

	File *p_new_run = malloc( sizeof(File) );

	p_new_run->p_next = p_head;		/*This item is now the head. */
	p_new_run->fileName = fileName;
	p_new_run->offset = offset;
	p_new_run->length = length;
	p_new_run->recordNumber = recordNumber;

	return p_new_run;	/*Return the new head of the list */
}

/**
 * Prints the members of the file record to stdout.
 */
void printFile(File *fileP) {
	if(fileP != NULL && fileP->fileName != NULL) {
		printf("%8d | %12" PRIu64 " | %10" PRIu32 " | %s\n", fileP->recordNumber,
															 fileP->offset ,
															 fileP->length,
															 fileP->fileName);
	}
}

/*
 * Traverses the Files list and Prints each file name and MFT record number
 *
 * Returns the number of named files in the list.
 */
uint32_t printAllFiles(File *p_head) {

	uint32_t countFiles = 0;
	File *p_current_item = p_head;
	while (p_current_item) {    // Loop while the current pointer is not NULL.
		if (p_current_item->fileName != NULL) {
			printFile(p_current_item);
			countFiles++;
		}
		// Advance the current pointer to the next item in the list.
		p_current_item = p_current_item->p_next;
	}
	printf("%" PRIu32 " files on record.\n", countFiles);
	return countFiles;
}

/**
 * Search for the specified parameter, in the specified list
 *
 * Returns the number of hits.
 */
int8_t searchFiles(File *p_head, uint8_t srchType, char * searchTerm) {
	int intSTerm = atoi(searchTerm);
	uint32_t countHits = 0;
	File *p_current_item = p_head;
	while (p_current_item) {    // Loop while the current pointer is not NULL.
		if (p_current_item->fileName != NULL) {
			if(SRCH_NUM == srchType) { /*Search for the record number given in searchTerm */
				if(p_current_item->recordNumber == intSTerm) {
					printFile(p_current_item);
					countHits++;
				}
			} else if (SRCH_OFFS == srchType) { /*Search for records using disk offset to content */
				if(p_current_item->recordNumber == intSTerm) {
					printFile(p_current_item);
					countHits++;
				}
			} else if (SRCH_NAME == srchType) {
				if( strcmp(searchTerm, p_current_item->fileName) == 0 ) {
					printFile(p_current_item);
					countHits++;
				}
			}
		}
		// Advance the current pointer to the next item in the list.
		p_current_item = p_current_item->p_next;
	}
	return countHits;
}

#endif /* FILELUT_H_ */
