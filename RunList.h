#include <inttypes.h>
#include "Debug.h"

/*
 * Non-resident attributes are stored in intervals of clusters called runs.
 * Each data run consists of a cluster offset number and a run length (in clusters).
 * The cluster offset is defined relative to the previous run. The first run is 0+cluster_number.
 *
 * This header maintains an NTFS run list as a singly linked list of cluster number and lengths.
 * Cluster offset and length are always less than 8 bytes in length. Offset is signed.
 */
typedef struct _DataRun {
  uint64_t *length;
  int64_t  *offset;
  struct _DataRun *p_next;
} DataRun;

DataRun* addRun(DataRun *p_head, uint64_t *length, int64_t *offset);
void printRuns(char * buff, DataRun *p_head);
DataRun* reverseList(DataRun *p_head);
int freeList(DataRun *p_head);

/*
 * Adds a new data_run to the start of the list and returns it.
 */
DataRun* addRun(DataRun *p_head, uint64_t *length, int64_t *offset) {

	DataRun *p_new_run = malloc( sizeof(DataRun) );
	p_new_run->p_next = p_head;	// This item is now the head.
	p_new_run->length = length;	// Set data pointers
	p_new_run->offset = offset;

	return (p_head = p_new_run);	// Sets the head of the list to this element.
}

/*
 * Traverses the RunList with head p_head. Prints each Data Run into buff.
 */
void printRuns(char * buff, DataRun *p_head) {

	DataRun *p_current_item = p_head;
	sprintf(buff, "\tVCN\tLength\n");
	while (p_current_item) {    // Loop while the current pointer is not NULL.
		if (p_current_item->offset && p_current_item->length) {
			sprintf(buff + strlen(buff), "\t%" PRIu64 "\t%" PRId64 "\n", *p_current_item->offset, *p_current_item->length);
		} else {
			sprintf(buff + strlen(buff), "\tNo data.\n");
		}
		// Advance the current pointer to the next item in the list.
		p_current_item = p_current_item->p_next;
	}
}

/*
 * Free all of the DataRun elements, and the structure itself.
 */
int freeList(DataRun *p_head)	{

	if(DEBUG && VERBOSE) printf("\tFreeing RunList: ");
	DataRun *p_current_item = p_head;
	int items_freed = 0;
	while (p_current_item) {
		DataRun *p_next = p_current_item->p_next; // Backup pointer to next list element.

	    if (p_current_item->offset) {	// Free offset member.
	    	free(p_current_item->offset);
	    }

	    if (p_current_item->length) {	// Free length member.
	    	free(p_current_item->length);
	    }

	    free(p_current_item);		//	Free data run structure
	    p_current_item = p_next;	// Move to the next item
	    items_freed++;
	}
	if(DEBUG && VERBOSE) printf("Freed %d data runs in total\n", items_freed);
	//free(p_head); 					// Free the head data run structure.
	return items_freed;
}

/*
 * Reverses the order of the elements in the list with head at *p_head
 */
DataRun* reverseList(DataRun *p_head) {
  DataRun *p_new_head = NULL;
  while (p_head) {
    DataRun *p_next = p_head->p_next;
    p_head->p_next = p_new_head;
    p_new_head = p_head;
    p_head = p_next;
  }
  return p_new_head;
}
