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
#pragma pack(pop)
