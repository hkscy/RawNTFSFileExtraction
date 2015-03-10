/*
 * FileLUT.h
 *
 *  Created on: 9 Mar 2015
 *      Author: Christopher Hicks
 *
 * A linked list which links file name, offset on disk to the file location, and the
 * number of the corresponding MFT record.
 */

#ifndef FILELUT_H_
#define FILELUT_H_

/* Represents the information necessary to link file writes with file names on disk */
typedef struct _File {
	char *fileName;			/* File name defined in $FILE_NAME */
	uint64_t offset;		/* Offset in clusters to the file */
	uint32_t recordNumber;	/* MFT record number from which this originates*/
	struct _File *p_next;
} File;

/*
 * Adds a new file to the start of the list and returns it.
 */
File* addFile(File *p_head, char *fileName, uint64_t offset, uint32_t recordNumber) {

	File *p_new_run = malloc( sizeof(File) );
	p_new_run->p_next = p_head;	// This item is now the head.
	p_new_run->fileName = fileName;
	p_new_run->offset = offset;	// Set data pointers
	p_new_run->recordNumber = recordNumber;

	return (p_head = p_new_run);	// Sets the head of the list to this element.
}

/*
 * Traverses the Files list and Prints each file name and MFT record number
 * Diagnostic use only.
 */
void printAllFiles(File *p_head) {

	File *p_current_item = p_head;
	while (p_current_item) {    // Loop while the current pointer is not NULL.
		if (p_current_item->fileName != NULL) {
			printf("%d | %" PRIu64 "| %s\n", p_current_item->recordNumber, p_current_item->offset ,p_current_item->fileName);
		}
		// Advance the current pointer to the next item in the list.
		p_current_item = p_current_item->p_next;
	}
}

#endif /* FILELUT_H_ */
