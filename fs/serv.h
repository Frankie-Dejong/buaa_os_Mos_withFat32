#include <fs.h>
#include <lib.h>
#include <mmu.h>

#define PTE_DIRTY 0x0002 // file system block cache is dirty

/* IDE disk number to look on for our file system */
#define DISKNO 1



#define SECT2BLK (BY2BLK / BY2SECT) /* sectors to a block */

/* Disk block n, when in memory, is mapped into the file system
 * server's address space at DISKMAP+(n*BY2BLK). */
#define DISKMAP 0x10000000

/* Maximum disk size we can handle (1GB) */
#define DISKMAX 0x40000000

/* FAT.img in memory, 0x50000000 - 0x58000000 
 * which means the maximum disk size we can hadle is 128MB. 
 * to fit the mos, we will copy linked cluster to one page, which va begin at FATPAGEMAP*/
#define FATDISKMAP 0x50000000
#define FATDISKMAX 0x8000000
#define FATPAGEMAP (FATDISKMAP + FATDISKMAX)


/* ide.c */
void ide_read(u_int diskno, u_int secno, void *dst, u_int nsecs);
void ide_write(u_int diskno, u_int secno, void *src, u_int nsecs);

/* fs.c */
int file_open(char *path, struct File **pfile);
int file_get_block(struct File *f, u_int blockno, void **pblk);
int file_set_size(struct File *f, u_int newsize);
void file_close(struct File *f);
int file_remove(char *path);
int file_dirty(struct File *f, u_int offset);
void file_flush(struct File *);

void fs_init(void);
void fs_sync(void);
extern uint32_t *bitmap;
int map_block(u_int);
int alloc_block(void);

/*fatfs.c*/
void fat32_init();
int fat_file_get_cluster(struct Fat32_Dir *f, u_int fileClusNo, void **Fat32_buf);
int fat_file_open(char *path, struct Fat32_Dir **pfile, int omode);
int fat_file_dirty(struct Fat32_Dir *f, u_int offset);
void fat_file_close(struct Fat32_Dir *f);
int fat_file_set_size(struct Fat32_Dir *f, u_int newsize);
int fat_file_writeBack(struct Fat32_Dir *f, u_int offset, u_int n, char *src);
int fat_file_remove(char *path);
int fat_file_openat(struct Fat32_Dir *dir, char *path, struct Fat32_Dir **pfile, int omode);
int fat_file_removeat(struct Fat32_Dir *par_dir, char *path);
void fat_fs_sync(void);
