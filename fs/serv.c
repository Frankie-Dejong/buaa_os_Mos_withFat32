/*
 * File system server main loop -
 * serves IPC requests from other environments.
 */

#include "serv.h"
#include <fd.h>
#include <fsreq.h>
#include <lib.h>
#include <mmu.h>

struct Open {
	struct File *o_file; // mapped descriptor for open file
	u_int o_fileid;	     // file id
	int o_mode;	     // open mode
	struct Filefd *o_ff; // va of filefd page
};

struct Fat32_Open {
	struct Fat32_Dir *o_file;
	u_int o_fileid;
	int o_mode;
	struct Fat32_Filefd *o_ff;
};

// Max number of open files in the file system at once
#define MAXOPEN 1024
#define FILEVA 0x60000000

// initialize to force into data section
struct Open opentab[MAXOPEN] = {{0, 0, 1}};
struct Fat32_Open fat_opentab[MAXOPEN] = {{0, 0, 1}};



/* Fat32 buf, to fit the mos fs, copy linked clusters to one page
   set the va of it to FATDISKMAP + FATDISKMAX*/


// Virtual address at which to receive page mappings containing client requests.
#define REQVA 0x0ffff000

// Overview:
//  Initialize file system server process.
void serve_init(void) {
	int i;
	u_int va;

	// Set virtual address to map.
	va = FILEVA;
	// Initial array opentab.
	for (i = 0; i < MAXOPEN; i++) {
		opentab[i].o_fileid = i;
		opentab[i].o_ff = (struct Filefd *)va;
		fat_opentab[i].o_fileid = i;
		fat_opentab[i].o_ff = (struct Fat32_Filefd *)va;
		va += BY2PG;
	}
}

// Overview:
//  Allocate an open file.
int open_alloc(struct Open **o) {
	int i, r;

	// Find an available open-file table entry
	for (i = 0; i < MAXOPEN; i++) {
		switch (pageref(opentab[i].o_ff)) {
		case 0:
			if ((r = syscall_mem_alloc(0, opentab[i].o_ff, PTE_D | PTE_LIBRARY)) < 0) {
				return r;
			}
		case 1:
			opentab[i].o_fileid += MAXOPEN;
			*o = &opentab[i];
			memset((void *)opentab[i].o_ff, 0, BY2PG);
			return (*o)->o_fileid;
		}
	}

	return -E_MAX_OPEN;
}

int fat_open_alloc(struct Fat32_Open **o) {
	int i, r;
	for(i = 0;i < MAXOPEN;i ++) {
		switch (pageref(fat_opentab[i].o_ff))
		{
		case 0:
			if ((r = syscall_mem_alloc(0, fat_opentab[i].o_ff, PTE_D | PTE_LIBRARY)) < 0) {
				return r;
			}
		case 1:
			fat_opentab[i].o_fileid += MAXOPEN;
			*o = &fat_opentab[i];
			memset((void *)fat_opentab[i].o_ff, 0, BY2PG);
			return (*o)->o_fileid;
		}
	}
	return -E_MAX_OPEN;
}


// Overview:
//  Look up an open file for envid.
int open_lookup(u_int envid, u_int fileid, struct Open **po) {
	struct Open *o;

	o = &opentab[fileid % MAXOPEN];

	if (pageref(o->o_ff) == 1 || o->o_fileid != fileid) {
		return -E_INVAL;
	}

	*po = o;
	return 0;
}

int fat_open_lookup(u_int envid, u_int fileid, struct Fat32_Open **po) {
	struct Fat32_Open *o;
	o = &fat_opentab[fileid % MAXOPEN];
	if (pageref(o->o_ff) == 1 || o->o_fileid != fileid) {
		return -E_INVAL;
	}

	*po = o;
	return 0;
}

// Serve requests, sending responses back to envid.
// To send a result back, ipc_send(envid, r, 0, 0).
// To include a page, ipc_send(envid, r, srcva, perm).

void serve_open(u_int envid, struct Fsreq_open *rq) {
	struct File *f;
	struct Filefd *ff;
	int r;
	struct Open *o;

	// Find a file id.
	if ((r = open_alloc(&o)) < 0) {
		ipc_send(envid, r, 0, 0);
	}

	// Open the file.
	if ((r = file_open(rq->req_path, &f)) < 0) {
		ipc_send(envid, r, 0, 0);
		return;
	}

	// Save the file pointer.
	o->o_file = f;

	// Fill out the Filefd structure
	ff = (struct Filefd *)o->o_ff;
	ff->f_file = *f;
	ff->f_fileid = o->o_fileid;
	o->o_mode = rq->req_omode;
	ff->f_fd.fd_omode = o->o_mode;
	ff->f_fd.fd_dev_id = devfile.dev_id;
	ff->f_fd.fd_fat = 0;

	ipc_send(envid, 0, o->o_ff, PTE_D | PTE_LIBRARY);
}

void serve_open_Fat32(u_int envid, struct Fsreq_open * rq) {
	struct Fat32_Dir *f;
	struct Fat32_Filefd *ff;
	int r;
	struct Fat32_Open *o;



	if((r = fat_open_alloc(&o)) < 0) {
		ipc_send(envid, r, 0, 0);
	}

	if((r = fat_file_open(rq->req_path, &f, rq->req_omode)) < 0) {
		ipc_send(envid, r, 0, 0);
		return;
	}

	o->o_file = f;
	ff = (struct Fat32_Filefd *)o->o_ff;
	ff->f_file = *f;
	ff->f_fileid = o->o_fileid;
	o->o_mode = rq->req_omode;
	ff->f_fd.fd_omode = o->o_mode;
	ff->f_fd.fd_dev_id = devfile.dev_id;
	ff->f_fd.fd_fat = 1;

	ipc_send(envid, 0, o->o_ff, PTE_D | PTE_LIBRARY);
}

void serve_map(u_int envid, struct Fsreq_map *rq) {
	struct Open *pOpen;
	u_int filebno;
	void *blk;
	int r;

	if ((r = open_lookup(envid, rq->req_fileid, &pOpen)) < 0) {
		ipc_send(envid, r, 0, 0);
		return;
	}

	filebno = rq->req_offset / BY2BLK;

	if ((r = file_get_block(pOpen->o_file, filebno, &blk)) < 0) {
		ipc_send(envid, r, 0, 0);
		return;
	}

	ipc_send(envid, 0, blk, PTE_D | PTE_LIBRARY);
}


void serve_map_Fat32(u_int envid, struct Fsreq_map *rq) {
	struct Fat32_Open *pOpen;
	u_int fileClusNo;
	void *Fat32_buf;
	int r;
	if ((r = fat_open_lookup(envid, rq->req_fileid, &pOpen)) < 0) {
		ipc_send(envid, r, 0, 0);
		return;
	}

	u_int SECT2CLUS = fat_get_clus() / BY2SECT;

	fileClusNo = rq->req_offset / BY2CLUS;

	if((r = fat_file_get_cluster(pOpen->o_file, fileClusNo, &Fat32_buf)) < 0) {
		ipc_send(envid, r, 0, 0);
		return;
	}

	ipc_send(envid, 0, Fat32_buf, PTE_D | PTE_LIBRARY);
}


void serve_set_size(u_int envid, struct Fsreq_set_size *rq) {
	struct Open *pOpen;
	int r;
	if ((r = open_lookup(envid, rq->req_fileid, &pOpen)) < 0) {
		ipc_send(envid, r, 0, 0);
		return;
	}

	if ((r = file_set_size(pOpen->o_file, rq->req_size)) < 0) {
		ipc_send(envid, r, 0, 0);
		return;
	}

	ipc_send(envid, 0, 0, 0);
}


void serve_set_size_Fat32(u_int envid, struct Fsreq_set_size *rq) {
	struct Fat32_Open *pOpen;
	int r;
	if((r = fat_open_lookup(envid, rq->req_fileid, &pOpen)) < 0) {
		ipc_send(envid, r, 0, 0);
		return;
	}

	if((r = fat_file_set_size(pOpen->o_file, rq->req_size)) < 0) {
		ipc_send(envid, r, 0, 0);
		return;
	}

	ipc_send(envid, 0, 0, 0);
}

void serve_close(u_int envid, struct Fsreq_close *rq) {
	struct Open *pOpen;

	int r;

	if ((r = open_lookup(envid, rq->req_fileid, &pOpen)) < 0) {
		ipc_send(envid, r, 0, 0);
		return;
	}

	file_close(pOpen->o_file);
	ipc_send(envid, 0, 0, 0);
}

void serve_close_Fat32(u_int envid, struct Fsreq_close *rq) {
	struct Fat32_Open *pOpen;
	int r;

	if((r = fat_open_lookup(envid, rq->req_fileid, &pOpen)) < 0) {
		ipc_send(envid, r, 0, 0);
	}

	fat_file_close(pOpen->o_file);
	ipc_send(envid, 0, 0, 0);
}

// Overview:
//  Serve to remove a file specified by the path in `req`.
void serve_remove(u_int envid, struct Fsreq_remove *rq) {
	// Step 1: Remove the file specified in 'rq' using 'file_remove' and store its return value.
	int r;
	/* Exercise 5.11: Your code here. (1/2) */
	r = file_remove(rq->req_path);
	// Step 2: Respond the return value to the requester 'envid' using 'ipc_send'.
	/* Exercise 5.11: Your code here. (2/2) */
	ipc_send(envid, r, 0, 0);
}

void serve_remove_Fat32(u_int envid, struct Fsreq_remove *rq) {
	int r;
	r = fat_file_remove(rq->req_path);
	ipc_send(envid, r, 0, 0);
}

void serve_removeat_Fat32(u_int envid, struct Fsreq_removeat *rq) {
	int r;
	struct Fat32_Open *pOpen;
	if ((r = fat_open_lookup(envid, rq->dir_fileid + MAXOPEN, &pOpen)) < 0) {
		ipc_send(envid, r, 0, 0);
		return;
	}
	struct Fat32_Dir *dir = pOpen->o_file;
	r = fat_file_removeat(dir, rq->req_path);
	ipc_send(envid, r, 0, 0);
}

void serve_dirty(u_int envid, struct Fsreq_dirty *rq) {
	struct Open *pOpen;
	int r;

	if ((r = open_lookup(envid, rq->req_fileid, &pOpen)) < 0) {
		ipc_send(envid, r, 0, 0);
		return;
	}

	if ((r = file_dirty(pOpen->o_file, rq->req_offset)) < 0) {
		ipc_send(envid, r, 0, 0);
		return;
	}

	ipc_send(envid, 0, 0, 0);
}

void serve_dirty_Fat32(u_int envid, struct Fsreq_dirty *rq) {
	struct Open *pOpen;
	int r;

	if((r = fat_open_lookup(envid, rq->req_fileid, &pOpen)) < 0) {
		ipc_send(envid, r, 0, 0);
		return;
	}

	if((r = fat_file_dirty(pOpen->o_file,rq->req_offset)) < 0) {
		ipc_send(envid, r, 0, 0);
	}

	ipc_send(envid, 0, 0, 0);
}

void serve_flush_Fat32(u_int envid, struct Fsreq_flush_Fat32 *rq) {
	struct Open *pOpen;
	int r;
	if((r = fat_open_lookup(envid, rq->req_fileid, &pOpen)) < 0) {
		ipc_send(envid, r, 0, 0);
		return;
	}

	if((r = fat_file_writeBack(pOpen->o_file, rq->req_offset, rq->req_len, rq->content)) < 0) {
		ipc_send(envid, r, 0, 0);
		return;
	}

	ipc_send(envid, 0, 0, 0);
}

void serve_sync(u_int envid) {
	fs_sync();
	fat_fs_sync();
	ipc_send(envid, 0, 0, 0);
}

void serve_openat_Fat32(u_int envid, struct Fsreq_openat *rq) {
	struct Fat32_Dir *f;
	struct Fat32_Filefd *ff;
	int r;
	struct Fat32_Open *o;

	// Find a file id.
	if ((r = fat_open_alloc(&o)) < 0) {
		ipc_send(envid, r, 0, 0);
	}

	struct Fat32_Open *pOpen;
	if ((r = fat_open_lookup(envid, rq->dir_fileid, &pOpen)) < 0) {
		ipc_send(envid, r, 0, 0);
		return;
	}
	struct Fat32_Dir *dir = pOpen->o_file;

	// Open the file.
	if ((r = fat_file_openat(dir, rq->req_path, &f, rq->req_omode)) < 0) {
		ipc_send(envid, r, 0, 0);
		return;
	}

	// Save the file pointer.
	o->o_file = f;

	// Fill out the Filefd structure
	ff = (struct Fat32_Filefd *)o->o_ff;
	ff->f_file = *f;
	ff->f_fileid = o->o_fileid;
	o->o_mode = rq->req_omode;
	ff->f_fd.fd_omode = o->o_mode;
	ff->f_fd.fd_dev_id = devfile.dev_id;
	ff->f_fd.fd_fat = 1;

	ipc_send(envid, 0, o->o_ff, PTE_D | PTE_LIBRARY);

}


void serve_BPB(u_int envid) {
	int r = fat_get_clus();
	ipc_send(envid, r, 0, 0);
}


void serve(void) {
	u_int req, whom, perm;

	for (;;) {
		perm = 0;

		req = ipc_recv(&whom, (void *)REQVA, &perm);

		// All requests must contain an argument page
		if (!(perm & PTE_V)) {
			debugf("Invalid request from %08x: no argument page\n", whom);
			continue; // just leave it hanging, waiting for the next request.
		}

		switch (req) {
		case FSREQ_FAT | FSREQ_OPEN:
			serve_open_Fat32(whom, (struct Fsreq_open *)REQVA);
			break;

		case FSREQ_FAT | FSREQ_MAP:
			serve_map_Fat32(whom, (struct Fsreq_map *)REQVA);
			break;

		case FSREQ_FAT | FSREQ_DIRTY:
			serve_dirty_Fat32(whom, (struct Fsreq_dirty *)REQVA);
			break;

		case FSREQ_FAT | FSREQ_CLOSE:
			serve_close_Fat32(whom, (struct Fsreq_close *)REQVA);
			break;

		case FSREQ_FAT | FSREQ_SET_SIZE:
			serve_set_size_Fat32(whom, (struct Fsreq_set_size *)REQVA);
			break;
		
		case FSREQ_FLUSH_FAT:
			serve_flush_Fat32(whom, (struct Fsreq_flush_Fat32 *)REQVA);
			break;
		
		case FSREQ_FAT | FSREQ_REMOVE:
			serve_remove_Fat32(whom, (struct Fsreq_remove *)REQVA);
			break;
		
		case FSREQ_OPENAT:
			serve_openat_Fat32(whom, (struct Fsreq_openat *)REQVA);
			break;

		case FSREQ_REMOVEAT:
			serve_removeat_Fat32(whom, (struct Fsreq_removeat *)REQVA);
			break;
		
		case FSREQ_GET:
			serve_BPB(whom);
			break;
		
		case FSREQ_OPEN:
			serve_open(whom, (struct Fsreq_open *)REQVA);
			break;

		case FSREQ_MAP:
			serve_map(whom, (struct Fsreq_map *)REQVA);
			break;

		case FSREQ_SET_SIZE:
			serve_set_size(whom, (struct Fsreq_set_size *)REQVA);
			break;

		case FSREQ_CLOSE:
			serve_close(whom, (struct Fsreq_close *)REQVA);
			break;

		case FSREQ_DIRTY:
			serve_dirty(whom, (struct Fsreq_dirty *)REQVA);
			break;

		case FSREQ_REMOVE:
			serve_remove(whom, (struct Fsreq_remove *)REQVA);
			break;

		case FSREQ_SYNC:
			serve_sync(whom);
			break;

		default:
			debugf("Invalid request code %d from %08x\n", whom, req);
			break;
		}

		syscall_mem_unmap(0, (void *)REQVA);
	}
}

int main() {
	user_assert(sizeof(struct File) == BY2FILE);

	debugf("FS is running\n");

	serve_init();
	fs_init();
	fat32_init();

	serve();
	return 0;
}
