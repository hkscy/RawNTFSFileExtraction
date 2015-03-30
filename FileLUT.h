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
	int64_t sec_offset;		/* Offset in sectors to the file record */
	int64_t cl_offset;		/* Offset to the cluster which contains this record(amongst others) */
	uint32_t length;		/* Length of the file in bytes */
	uint32_t recordNumber;	/* MFT record number from which this originates*/
	struct _File *p_next;	/* Pointer to next file record */
} File;

/*
 * Adds a new file to the start of the list and returns it.
 */
File* addFile(File *p_head, char *fileName,
		 	  int64_t sec_offs, int64_t cl_offs,
			  uint32_t length, uint32_t recordNumber) {

	File *p_new_run = malloc( sizeof(File) );

	p_new_run->p_next = p_head;		/*This item is now the head. */
	p_new_run->fileName = fileName;
	p_new_run->sec_offset = sec_offs;
	p_new_run->cl_offset = cl_offs;
	p_new_run->length = length;
	p_new_run->recordNumber = recordNumber;

	return p_new_run;	/*Return the new head of the list */
}

/**
 * Adds a copy of an existing file (source) to the head of the list at *dest
 */
File* addFileCopy(File *source, File *dest) {
	return addFile(dest, source->fileName,
						 source->sec_offset,
						 source->cl_offset ,
						 source->length,
						 source->recordNumber);
}

/**
 * Prints the members of the file record to stdout.
 */
void printFile(File *fileP) {
	if(fileP != NULL && fileP->fileName != NULL) {
		printf("%8d | %12" PRId64 " | %12" PRId64 " | %10" PRIu32 " | %s\n",
														fileP->recordNumber,
														  fileP->sec_offset,
														   fileP->cl_offset,
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
File *searchFiles(File *p_head, uint8_t srchType, char * searchTerm) {

	int64_t d64SearchTerm = strtoull(searchTerm, NULL, 10);
	File *foundFiles = NULL;

	File *p_current_item = p_head;
	while (p_current_item) {    // Loop while the current pointer is not NULL.
		if (p_current_item->fileName != NULL) {

			if(SRCH_NUM == srchType) { /*Search for the record number given in searchTerm */
				if( p_current_item->recordNumber == d64SearchTerm ) {
					if(d64SearchTerm != 0) {
						printFile(p_current_item);
						foundFiles = addFileCopy(p_current_item, foundFiles);
					} else {
						printf("Please enter a valid search query.\n");
					}
				}
			} else if (SRCH_OFFS == srchType) { /*Search for records using disk offset to content */
				if( p_current_item->cl_offset == d64SearchTerm ) { /*use cluster offset not sector */
					if(d64SearchTerm != 0) {
						printFile(p_current_item);
						foundFiles = addFileCopy(p_current_item, foundFiles);
					} else {
						printf("Please enter a valid search query.\n");
					}
				}
			} else if (SRCH_NAME == srchType) { /*Search for records using file name */
				if( strcmp(searchTerm, p_current_item->fileName) == 0 ) {
					printFile(p_current_item);
					foundFiles = addFileCopy(p_current_item, foundFiles);
				}
			}
		}
		// Advance the current pointer to the next item in the list.
		p_current_item = p_current_item->p_next;
	}

	return foundFiles;
}

/* Not working yet - why not?*/
int freeFilesList(File *p_head)	{

	File *p_current_item = p_head;
	int items_freed = 0;
	while (p_current_item) {
		File *p_next = p_current_item->p_next; /*Backup pointer to next list element.*/

	    if (p_current_item->fileName) {		   /*Free fileName */
	    	free(p_current_item->fileName);
	    }

	    free(p_current_item);		//	Free data run structure
	    p_current_item = p_next;	// Move to the next item
	    items_freed++;
	}
	//free(p_head); 					// Free the head data run structure, corrupts on one item lists.
	return items_freed;
}

#endif /* FILELUT_H_ */
