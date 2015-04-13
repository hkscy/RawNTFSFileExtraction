/* Christopher Hicks */
#ifndef NTFSSTRUCT_H_
#define NTFSSTRUCT_H_

#include <inttypes.h>

#define FRAG_PADDING 1008
#define BUFFSIZE 1024
#define NTFS_TYPE 0x07			/*NTFS partitions are represented by 0x07 in the partition table */

typedef unsigned char BYTE; /*Define byte abbreviation */

#pragma pack(push, 1) /*Pack structures to a one byte alignment */
	typedef struct _PARTITION { 	/*NTFS partition table struct */
		BYTE chBootInd; 			/*Boot indicator bit flag: 0 = no, 0x80 = bootable (or "active") */
		BYTE chHead;				/*Starting head */
		BYTE chSector;				/*Starting sector (Bits 6-7 are the upper two bits for the Starting Cylinder field.) */
		BYTE chCylinder;			/*Starting Cylinder */
		BYTE chType; 				/*Partition type identifier */
		BYTE chLastHead;			/*Ending Head */
		BYTE chLastSector; 			/*Ending Sector (Bits 6-7 are the upper two bits for the ending cylinder field) */
		BYTE chLastCylinder;		/*Ending Cylinder */
		uint32_t dwRelativeSector; 	/*Relative Sector (to start of partition), also the partition's starting LBA value) */
		uint32_t dwNumberSector; 	/* Total Sectors in partition  */
	} PARTITION, *P_PARTITION;

	typedef struct _NTFS_BOOT_SECTOR {
		char	chJumpInstruction[3];	/*Skips the next several non-executable bytes */
		char	chOemID[4]; 			/*name and version number of the OS that formatted the volume*/
		char	chDummy[4];

		struct NTFS_BPB	{ /*BIOS Parameter block, provides information that enables the executable boot code to locate Ntldr */
			uint16_t	wBytesPerSec;		/*Bytes per logical sector */
			BYTE		uchSecPerClust;		/*Logical sectors per cluster */
			uint16_t	wReservedSec;		/*Reserved logical sectors */
			BYTE		uchReserved[3];		/*Number of FATs[0], Root directory entries[1,2] */
			uint16_t	wUnused1;
			BYTE		uchMediaDescriptor;	/*Media descriptor */
			uint16_t	wUnused2;
			uint16_t	wSecPerTrack;		/*Physical sectors per track */
			uint16_t	wNumberOfHeads;		/*Number of heads */
			uint32_t	dwHiddenSec;		/*Hidden sectors  */
			uint32_t	dwUnused3;
			uint32_t	dwUnused4;
			int64_t		n64TotalSec;				/*Sectors in volume */
			int64_t		n64MFTLogicalClustNum;		/*Cluster number for MFT! */
			int64_t		n64MFTMirrLoficalClustNum;	/*MFT mirror first cluster number */
			int			nClustPerMFTRecord;			/*MFT record size */
			int			nClustPerIndexRecord;		/*Index block size */
			int64_t		n64VolumeSerialNum;			/*Volume serial number */
			uint32_t	dwCheckSum;
		} bpb;

		char		chBootStrapCode[426];
		uint16_t	wSecMark;				/*Always 0x55AA, marks end of boot sector */
	} NTFS_BOOT_SECTOR, *P_NTFS_BOOT_SECTOR;

	/*
	 * This is not a standard NTFS structure
	 * I have created it to take up the same space as an MFT record,
	 * but carry information about the offset to the MFT record which the following
	 * MFT records are located at.
	 */
	typedef struct _FRAG {
		char 		fileSignature[4];
		uint64_t	u64fragOffset;
		BYTE 		unusedPadding[FRAG_PADDING];
		uint32_t	dwEOF;
	} FRAG;

	/*http://www.cse.scu.edu/~tschwarz/coen252_07Fall/Lectures/NTFS.html */
	typedef struct _NTFS_MFT_FILE_ENTRY_HEADER {
		char		fileSignature[4];		/*Magic Number: "FILE" */
		uint16_t	wFixupOffset;			/*Offset to the update sequence. */
		uint16_t	wFixupSize;				/*Number of entries in fixup array */
		int64_t		n64LogSeqNumber;		/*$LogFile Sequence Number (LSN) */
		uint16_t	wSequence;				/*Sequence number */
		uint16_t	wHardLinks;				/*Hard link count (number of directory entries that reference this record.) */
		uint16_t	wAttribOffset;			/*Offset to first attribute */
		uint16_t	wFlags;					/*Flags: 0x01: record in use, 0x02 directory. */
		uint32_t	dwRecLength;			/*Used size of MFT entry */
		uint32_t	dwAllLength;			/*Allocated size of MFT entry. */
		int64_t		n64BaseMftRec;			/*File reference to the base FILE record */
		uint16_t	wNextAttrID;			/*Next attribute ID */
		uint16_t	wFixUpPattern;			/*Align to 4B boundary */
		uint32_t	dwMFTRecNumber;			/*Number of this MFT record */
		//BYTE		uchMFTData[976];		/*Remainder of MFT record */
	} NTFS_MFT_FILE_ENTRY_HEADER, *P_NTFS_MFT_FILE_ENTRY_HEADER;

	/*
	 * The attribute header is there to help find the relevant data for an attribute,
	 * not to be the relevant data header itself.
	 */
	typedef struct	_NTFS_ATTRIBUTE {
		uint32_t	dwType;			/*Attribute Type Identifier */
		uint32_t	dwFullLength;	/*Length of Attribute (includes header) */
		BYTE		uchNonResFlag;	/*Non-Resident Flag */
		BYTE		uchNameLength;	/*Length of Name (only for ADS) */
		uint16_t	wNameOffset;	/*Offset to Name (only for ADS) */
		uint16_t	wFlags;			/*Flags(Compressed, Encrypted, Sparse) */
		uint16_t	wID;			/*Attribute Identifier */

		union ATTR	{
			/*MFT records contain various attributes preceded by an attribute header */
			struct RESIDENT	{
				uint32_t	dwLength;		/*Length of Attribute Content */
				uint16_t 	wAttrOffset;	/*Offset to Attribute Content */
				BYTE 		uchIndexedTag;	/*Indexed */
				BYTE 		uchPadding;		/*Padding */
				/*Attribute itself is read from this point onwards */
			} Resident;
			/*
			 * non-resident attributes need to describe an arbitrary number of cluster runs,
			 * consecutive clusters that they occupy.
			 */
			struct NONRESIDENT {
				int64_t		n64StartVCN;		/*Start cluster number */
				int64_t		n64EndVCN;			/*End cluster number */
				uint16_t	wDatarunOffset;
				uint16_t	wCompressionSize;	/*Compression unit size, 0: Uncompressed */
				BYTE		uchPadding[4];
				int64_t		n64AllocSize;		/*Attr size rounded up to cluster size */
				int64_t		n64RealSize;		/*Actual size of the file, probably have to confine this  */
				int64_t		n64StreamSize; 		/*Always equal to allocated size? */
			} NonResident;
		} Attr;

	} NTFS_ATTRIBUTE, *P_NTFS_ATTRIBUTE;

	typedef struct _OFFS_LEN_BITFIELD {
		union {
			BYTE val;
			struct {
				unsigned char lengthSize:  4;	/*Nibble contains the size of the length field */
				unsigned char offsetSize: 4;	/*Nibble contains the size of the offset field */
			} bitfield;
		};
	} OFFS_LEN_BITFIELD, *P_OFFS_LEN_BITFIELD;

	/*Found at the beginning of each 4096 INDX record. */
	typedef struct _NTATTR_STANDARD_INDX_HEADER {
		char magicNumber[4]; /*Contains the string literal 'INDX' */

		unsigned short updateSeqOffs;
		unsigned short sizeOfUpdateSequenceNumberInWords;

		uint64_t logFileSeqNum;
		uint64_t vcnOfINDX;

		uint32_t indexEntryOffs;
		uint32_t sizeOfEntries;	/*Use for loop condition. read offset<sizeOfEntries */
		uint32_t sizeOfEntryAlloc;

		uint16_t flags;
		uint16_t padding[3];

		unsigned short updateSeq;

	} NTATTR_STANDARD_INDX_HEADER;

	/*Actual index record structure */
	typedef struct _NTATTR_INDEX_RECORD_ENTRY {
		/*Multiply by MFT_RECORD_SIZE (1024 bytes) and use as offset from
		  start of the MFT to find the record */
		uint64_t mftReference;
		/*Next INDX record can be located by adding sizeofIndexEntry to the
		 *current offset */
		unsigned short sizeofIndexEntry;
		unsigned short filenameOffset;

		unsigned short flags;
		char padding[2];

		uint64_t mftFileReferenceOfParent;
		uint64_t creationTime;
		uint64_t lastModified;
		uint64_t lastModifiedFilerecord;
		uint64_t lastAccessTime;
		uint64_t allocatedSizeOfFile;
		uint64_t realFileSize;
		uint64_t fileFlags;

		uint16_t fileNameLength;
		uint16_t fileNameNamespace;
	} NTATTR_INDEX_RECORD_ENTRY;
	/*Unicode file name string is found directly after the end of the index record entry structure */
#pragma pack(pop)

/* Verbose debug methods */
FRAG *createFragRecord(uint64_t fragOffset);
int getPartitionInfo(char *buff, PARTITION *part);
int getBootSectInfo(char* buff, NTFS_BOOT_SECTOR *bootSec);
int getFILE0Attrib(char* buff, NTFS_MFT_FILE_ENTRY_HEADER *mftFileEntry);
int getFileAttribMembers(char * buff, NTFS_ATTRIBUTE* attrib);

/**
 *	Create a Fragment record which contains the offset from which the MFT records
 *	which follow were copied. This is necessary to determine the absolute offset
 *	to resident attributes (i.e. files < ~900B
 *
 *	WARNING: Memory is allocated but not free()'d.
 */
FRAG *createFragRecord(uint64_t fragOffset) {
	FRAG *fragRec = malloc( sizeof(FRAG) );
	if(fragRec != NULL) {
		fragRec->fileSignature[0] = 'F';
		fragRec->fileSignature[1] = 'R';
		fragRec->fileSignature[2] = 'A';
		fragRec->fileSignature[3] = 'G';
		fragRec->u64fragOffset = fragOffset;
		char unused[FRAG_PADDING] = { 0x00 };
		memset(fragRec->unusedPadding, *unused, sizeof(unused));
		fragRec->dwEOF = 0xFFFFFFFF;
	}
	return fragRec;
}

/*
 * Prints the members of an MFT FILE record attribute into buff.
 */
int getFileAttribMembers(char * buff, NTFS_ATTRIBUTE* attrib) {
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

		sprintf(buff, "%s"
					  "n64StartVCN: %" PRId64 "\n"
					  "n64EndVCN: %" PRId64 "\n"
					  "wDatarunOffset: %u\n"
					  "wCompressionSize: %u\n"
					  "n64AllocSize: %" PRId64 "\n"
					  "n64RealSize: %" PRId64 "\n"
					  "n64StreamSize: %" PRId64 "\n"
					  ,tempBuff,
					  (attrib->Attr).NonResident.n64StartVCN,
					  (attrib->Attr).NonResident.n64EndVCN,
					  (attrib->Attr).NonResident.wDatarunOffset,
					  (attrib->Attr).NonResident.wCompressionSize,
					  (attrib->Attr).NonResident.n64AllocSize,
					  (attrib->Attr).NonResident.n64RealSize,
					  (attrib->Attr).NonResident.n64StreamSize);


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

#endif
