#ifndef _FSREQ_H_
#define _FSREQ_H_

#include <fs.h>
#include <types.h>

// Definitions for requests from clients to file system

#define FSREQ_OPEN 1
#define FSREQ_MAP 2
#define FSREQ_SET_SIZE 3
#define FSREQ_CLOSE 4
#define FSREQ_DIRTY 5
#define FSREQ_REMOVE 6
#define FSREQ_SYNC 7
#define FSREQ_FLUSH_FAT 8
#define FSREQ_OPENAT 9
#define FSREQ_REMOVEAT 10
#define FSREQ_GET 11
#define FSREQ_FAT 0x10000

struct Fsreq_open {
	char req_path[MAXPATHLEN];
	u_int req_omode;
};

struct Fsreq_map {
	int req_fileid;
	u_int req_offset;
};

struct Fsreq_set_size {
	int req_fileid;
	u_int req_size;
};

struct Fsreq_close {
	int req_fileid;
};

struct Fsreq_dirty {
	int req_fileid;
	u_int req_offset;
};

struct Fsreq_remove {
	char req_path[MAXPATHLEN];
};

struct Fsreq_flush_Fat32 {
	u_int req_fileid;
	u_int req_offset;
	u_int req_len;
	char content[BY2PG - 3 * sizeof(u_int)];
};

struct Fsreq_openat {
	u_int dir_fileid;
	char req_path[MAXPATHLEN];
	u_int req_omode;
 };


struct Fsreq_removeat {
	u_int dir_fileid;
	char req_path[MAXPATHLEN];
 };


#endif
