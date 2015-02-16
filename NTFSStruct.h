typedef unsigned char BYTE; /*Define byte symbolic abbreviation */

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
