#ifndef _FS_H_
#define _FS_H_ 1

#include <stdint.h>

// File nodes (both in-memory and on-disk)

#define BY2SECT 512		    /* Bytes per disk sector */

// Bytes per file system block - same as page size
#define BY2BLK BY2PG
#define BIT2BLK (BY2BLK * 8)

// Maximum size of a filename (a single path component), including null
#define MAXNAMELEN 128

// Maximum size of a complete pathname, including null
#define MAXPATHLEN 1024

// Number of (direct) block pointers in a File descriptor
#define NDIRECT 10
#define NINDIRECT (BY2BLK / 4)

#define MAXFILESIZE (NINDIRECT * BY2BLK)

#define BY2FILE 256

struct File {
	char f_name[MAXNAMELEN]; // filename
	uint32_t f_size;	 // file size in bytes
	uint32_t f_type;	 // file type
	uint32_t f_direct[NDIRECT];
	uint32_t f_indirect;

	struct File *f_dir; // the pointer to the dir where this file is in, valid only in memory.
	char f_pad[BY2FILE - MAXNAMELEN - (3 + NDIRECT) * 4 - sizeof(void *)];
} __attribute__((aligned(4), packed));

#define FILE2BLK (BY2BLK / sizeof(struct File))

// File types
#define FTYPE_REG 0 // Regular file
#define FTYPE_DIR 1 // Directory
#define FTYPE_LINK 2
// File system super-block (both in-memory and on-disk)

#define FS_MAGIC 0x68286097 // Everyone's favorite OS class

struct Super {
	uint32_t s_magic;   // Magic number: FS_MAGIC
	uint32_t s_nblocks; // Total number of blocks on disk
	struct File s_root; // Root directory node
};


/*for FAT32*/
#define FAT_BPB_END_NUMBER 0x55AA
struct FAT_Super {
	int FAT_BY2SEC;
	int FAT_SECPClus;
	char FSType[9];
	int FAT_endNumber;
	int RootClus;
	int SECPFAT;
	int TotClus;
	int numOfFats;
	int numOfReserved;
};


struct FSInfo {
	int FSI_LeadSig;
	char reserved1[480];
	int FSI_StructSig;
	int FSI_FreeCount;
	int FSI_NxtFree;
	char reserved2[12];
	int FSI_TrailSig;
};

struct Fat32_Dir {
	char Name[11];
	char Attr;
	char NTRes;
	char CrtTimeTenth;
	short CrtTime;
	short CrtDate;
	char LstAccDate[2];
	char ClusHI[2];
	short WrtTime;
	short WrtDate;
	char ClusLO[2];
	int FileSize;
}__attribute__((packed));

struct Fat32_LDIR {
	char LDIR_Ord;
	short LDIR_Name1[5];
	char LDIR_Attr;
	char LDIR_Type;
	char LDIR_Chksum;
	short LDIR_Name2[6];
	short LDIR_FstClusLO;
	short LDIR_Name3[2];
}__attribute__((packed));



// #define BY2CLUS (BY2SECT * SECT2CLUS)
#define BY2CLUS BY2SECT
#define CLUS2Page (BY2PG / BY2CLUS)
#define EOF_Fat32 0xfffffff 
#define ATTR_READ_ONLY 0x01 
#define ATTR_HIDDEN 0x02 
#define ATTR_SYSTEM 0x04 
#define ATTR_VOLUME_ID 0x08 
#define ATTR_DIRECTORY 0x10 
#define ATTR_ARCHIVE 0x20
#define ATTR_LONG_NAME (ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_VOLUME_ID)
#define ATTR_LN_MASK (ATTR_LONG_NAME | ATTR_ARCHIVE | ATTR_DIRECTORY)
#define LAST_LONG_ENTRY 0x40

#endif // _FS_H_
