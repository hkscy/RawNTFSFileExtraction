/*
 ============================================================================
 Name        : RawNTFSExtraction.c
 Author      : Christopher Hicks
 Version     :
 Copyright   : GPL
 Description : Hello World in C, Ansi-style
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>

#include <sys/types.h> /*For POSIX open() */
#include <sys/stat.h>  /*For POSIX open() */
#include <fcntl.h>     /*For POSIX open() */

#include <errno.h>     /*For errno access */

static const char BLOCK_DEVICE[] = "/dev/mechastriessand/windows7";

int main(void) {
	puts("Hello NTFS!"); /* prints Hello NTFS! */
	/*HANDLE WINAPI CreateFile(
 		_In_      LPCTSTR lpFileName,	“\\\\.\\PhysicalDrive0”
  	  	_In_      DWORD dwDesiredAccess,		  GENERIC_READ
  	  	_In_      DWORD dwShareMode,		   FILE_SHARE_READ
  	    _In_opt_  LPSECURITY_ATTRIBUTES lpSecurityAttributes,0
  	  	_In_      DWORD dwCreationDisposition,	 OPEN_EXISTING
  	  	_In_      DWORD dwFlagsAndAttributes,				 0
  	  	_In_opt_  HANDLE hTemplateFile );				   NULL
	 */
	int fileDescriptor;
	fileDescriptor = open(BLOCK_DEVICE, O_RDONLY);
	if(fileDescriptor == -1) { //open failed.
		int errsv = errno;
		if(errsv == EACCES) {
			printf("Failed to open block device %s with permission denied.", BLOCK_DEVICE);
		} else {
			printf("Failed to open block device %s with error number %d.", BLOCK_DEVICE, errsv);
		}
	}


	return EXIT_SUCCESS;
}
