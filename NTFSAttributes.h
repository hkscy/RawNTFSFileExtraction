#include "Debug.h"

/*NTFS Attribute types */
#define STANDARD_INFORMATION 0x10	/*MFT Record attribute constants */
#define ATTRIBUTE_LIST 0x20
#define FILE_NAME 0x30
#define OBJECT_ID 0x40
#define SECURITY_DESCRIPTOR 0x50
#define VOLUME_NAME 0x60
#define VOLUME_INFORMATION 0x70
#define DATA 0x80
#define INDEX_ROOT 0x90 		/*This is the root node of the B+ tree that implements an index (e.g. a directory).*/
#define INDEX_ALLOCATION 0xA0 	/*A sequence of all index buffers that belong to the index. */
#define BITMAP 0xB0
#define REPARSE_POINT 0xC0
#define EA_INFORMATION 0xD0
#define EA 0xE0
#define LOGGED_UTILITY_STREAM 0x100

/*File permissions */
#define RDONLY		0x0001
#define HIDDEN 		0x0002
#define SYSTEM 		0x0004
#define	ARCHIVE		0x0020
#define DEVICE 		0X0040
#define NORMAL		0x0080
#define TEMP		0x0100
#define SPARSE		0x0200
#define REPARSE_PT	0x0400
#define COMPRESSED	0x0800
#define OFFLINE		0x1000
#define	NOTINDEXED 	0x2000
#define ENCRYPTED	0X4000

#pragma pack(push, 1) /*Pack structures to a one byte alignment */

	typedef struct _STD_INFORMATION {
		uint64_t	fileCreateTime;		//C Time
		uint64_t	fileAltTime;		//A Time
		uint64_t	mftChangeTime;		//M Time
		uint64_t	fileReadTime;		//R Time
		uint32_t	filePermissions;	//DOS File Permissions
		uint32_t	maxNumVersions;
		uint32_t	versionNum;
		uint32_t	classID;
		uint32_t	ownerID2KONLY;		//Unused beyond win2k
		uint32_t	securityID2KONLY;	//Unused beyond win2k
		uint64_t	quota2KONLY;		//Unused beyond win2k
		uint64_t	updSeqNum2KONLY;	//Unused beyond win2k
	} STD_INFORMATION;

	/* http://0cch.net/ntfsdoc/attributes/file_name.html */
	typedef struct _FILE_NAME_ATTR {
		int64_t		n64ParentDirReference; 		/*File reference to the parent directory. */
		int64_t 	n64FileCreationTime;		/*C Time - File Creation */
		int64_t 	n64FileAlterationTime; 		/*A Time - File Altered */
		int64_t 	n64MFTChangedTime;	 		/*M Time - MFT Changed */
		int64_t 	n64ReadTime; 				/*R Time - File Read */
		int64_t 	n64AllocatedFileSize; 		/*Allocated size of the file */
		int64_t 	n64RealFileSize; 			/*Real size of the file */
		uint32_t	dwFlags;					/*Flags, e.g. Directory, compressed, hidden */
		uint32_t	dwUnused;			 		/*Used by EAs and Reparse */
		BYTE 	 	bFileNameLength;			/*Filename length in characters (L) */
		BYTE 	 	bFilenameNamespace;			/*Filename namespace */
		uint16_t	arrUnicodeFileName[255]; 	/*File name in Unicode (not null terminated). Maximum filename length of 255 Unicode characters.  */
	} FILE_NAME_ATTR;

	typedef struct _VOLUME_NAME_ATTR {
		char	arrUnicodeVolumeName[255];
	} VOLUME_NAME_ATTR;

#pragma pack(pop)

/**
 * 	Given an NTFS_ATTRIBUTE of type FILE_NAME (0x30)
 * 	and the mftBuffer record (1024 bytes)
 * 	and the offs into the record which the attribute is located at.
 *
 * 	returns the fileName in 8-bit character width.
 *
 * 	WARNING: Memory is allocated for asciiFileName, need to free the returned pointer.
 */
char * getFileName(NTFS_ATTRIBUTE * mftRecAttr, char * mftBuffer, uint16_t offs ) {
	char * asciiFileName;

	if(!mftRecAttr->dwType == FILE_NAME) { /*Make sure this is a FILE_NAME attribute */
		return NULL;
	}

	FILE_NAME_ATTR *fileNameAttr = malloc( mftRecAttr->Attr.Resident.dwLength + 1 );
	memcpy(fileNameAttr, mftBuffer+offs+(mftRecAttr->Attr).Resident.wAttrOffset,
										(mftRecAttr->Attr).Resident.dwLength);

	uint16_t* wFileName = fileNameAttr->arrUnicodeFileName;

	asciiFileName = malloc( fileNameAttr->bFileNameLength + 1 );
	*asciiFileName = '\0';

	int  k;
	/* Convert UNICODE filename to ASCII filename */
	for(k = 0; k<fileNameAttr->bFileNameLength; k++) {
		if((wFileName[k] & 0xFF) > 20 ) { /*Bottom 20 are control characters */
			sprintf(asciiFileName + strlen(asciiFileName), "%c", wFileName[k]&0xFF);
		}
	}

	if(false) {
		printf("FILE_NAME attribute:\n");
		printf("\tFile name length: %u\t", fileNameAttr->bFileNameLength);
		printf("\tNamespace: %u\t", fileNameAttr->bFilenameNamespace);

		/* Print out the file name of the record*/
		printf("\tFile name: ");
		int  k = 0;
		for(; k<fileNameAttr->bFileNameLength; k++) {
			printf("%c", wFileName[k]&0xFF); /*Mask just the final 8 bits */
		}
		printf("\n");
	}

	free(fileNameAttr);
	return asciiFileName;
}

