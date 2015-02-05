#define STANDARD_INFORMATION 0x10	/*MFT Record attribute constants */
#define ATTRIBUTE_LIST 0x20
#define FILE_NAME 0x30
#define OBJECT_ID 0x40
#define SECURITY_DESCRIPTOR 0x50
#define VOLUME_NAME 0x60
#define VOLUME_INFORMATION 0x70
#define DATA 0x80
#define INDEX_ROOT 0x90 /*This is the root node of the B+ tree that implements an index (e.g. a directory).*/
#define INDEX_ALLOCATION 0xA0 /*A sequence of all index buffers that belong to the index. */
#define BITMAP 0xB0
#define REPARSE_POINT 0xC0
#define EA_INFORMATION 0xD0
#define EA 0xE0
#define LOGGED_UTILITY_STREAM (0x100)

#pragma pack(push, 1) /*Pack structures to a one byte alignment */
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

/*
 * Returns the attribute name for a given attribute ID.
 */
char * getAttributeName(char * buff, BYTE attribute) {

	return buff;
}
