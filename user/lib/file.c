#include <fs.h>
#include <lib.h>

#define debug 0
int User_BY2CLUS = 0;

static int file_close(struct Fd *fd);
static int file_read(struct Fd *fd, void *buf, u_int n, u_int offset);
static int file_write(struct Fd *fd, const void *buf, u_int n, u_int offset);
static int file_stat(struct Fd *fd, struct Stat *stat);

// Dot represents choosing the member within the struct declaration
// to initialize, with no need to consider the order of members.
struct Dev devfile = {
    .dev_id = 'f',
    .dev_name = "file",
    .dev_read = file_read,
    .dev_write = file_write,
    .dev_close = file_close,
    .dev_stat = file_stat,
};

char Fat_buf[BY2PG]__attribute__((aligned(BY2PG)));

int isFat32(const char *path) {
	char *p = (char *)path;
	int i;
	for(i = 0;p[i] != 0;i++)
	{
		if(p[i] == '/') {
			break;
		}
	}
	if(i > 0 && p[i - 1] == ':') {
		return 1;
	}
	if(i > 3 && p[i - 3] == 'F' && p[i - 2] == 'A' && p[i-1] == 'T') {
		return 1;
	}
	return 0;
}

void getBY2CLUS() {
	User_BY2CLUS = fsipc_getBY2CLUS();
	user_assert(User_BY2CLUS > 0);
}



// Overview:
//  Open a file (or directory).
//
// Returns:
//  the file descriptor on success,
//  the underlying error on failure.

int open_Fat32(const char *path, int mode, struct Fd *fd) {
	int r;
	r = fsipc_open_Fat32(path, mode, fd);
	if(r < 0) {
		return r;
	}
	char *va;
	struct Fat32_Filefd *ffd;
	u_int size, fileid;
	va = fd2data(fd);
	ffd = (struct Fat32_Filefd *) fd;
	size = ffd->f_file.FileSize;
	fileid = ffd->f_fileid;

	if(User_BY2CLUS == 0) {
		getBY2CLUS();
	}

	for(int i = 0;i < size;i += User_BY2CLUS) {
		r = fsipc_map_Fat32(fileid, i, Fat_buf);
		if(r < 0) {
			return r;
		}
		memcpy(va + i, Fat_buf, User_BY2CLUS);
	}
	return fd2num(fd);
}


int openat(int dirfd, const char *path, int mode) {
	int r;
	struct Fd *dir, *fd;
	fd_alloc(&fd);
	fd_lookup(dirfd, &dir); 
	struct Fat32_Filefd *fdir = (struct Fat32_Filefd *)dir;
	u_int dir_fileid = fdir->f_fileid;
	r = fsipc_openat_Fat32(dir_fileid, path, mode, fd);
	if(r < 0) {
		return r;
	}
	char *va;
	struct Fat32_Filefd *ffd;
	u_int size, fileid;
	va = fd2data(fd);
	ffd = (struct Fat32_Filefd *)fd;
	size = ffd->f_file.FileSize;
	fileid = ffd->f_fileid;

	if(User_BY2CLUS == 0) {
		getBY2CLUS();
	}


	for(int i = 0;i < size;i += User_BY2CLUS) {
		r = fsipc_map_Fat32(fileid, i, Fat_buf);
		if(r < 0) {
			return r;
		}
		memcpy(va + i, Fat_buf, User_BY2CLUS);
	}
	return fd2num(fd);
}








int open(const char *path, int mode) {
	int r;

	// Step 1: Alloc a new 'Fd' using 'fd_alloc' in fd.c.
	// Hint: return the error code if failed.
	struct Fd *fd;
	/* Exercise 5.9: Your code here. (1/5) */
	r = fd_alloc(&fd);
	if(r < 0) {
		return r;
	}

	// check whether it is a fat32 file
	if(isFat32(path)) {
		return open_Fat32(path, mode, fd);
	}
	// Step 2: Prepare the 'fd' using 'fsipc_open' in fsipc.c.
	/* Exercise 5.9: Your code here. (2/5) */
	r = fsipc_open(path, mode, fd);
	if(r < 0) {
		return r;
	}
	// Step 3: Set 'va' to the address of the page where the 'fd''s data is cached, using
	// 'fd2data'. Set 'size' and 'fileid' correctly with the value in 'fd' as a 'Filefd'.
	char *va;
	struct Filefd *ffd;
	u_int size, fileid;
	/* Exercise 5.9: Your code here. (3/5) */
	va = fd2data(fd);
	ffd = (struct Filefd *)fd;
	size = ffd->f_file.f_size;
	fileid = ffd->f_fileid;
	// Step 4: Alloc pages and map the file content using 'fsipc_map'.
	for (int i = 0; i < size; i += BY2PG) {
		/* Exercise 5.9: Your code here. (4/5) */
		r = fsipc_map(fileid, i, va + i);
		if(r < 0) {
			return r;
		}
	}

	// Step 5: Return the number of file descriptor using 'fd2num'.
	/* Exercise 5.9: Your code here. (5/5) */
	return fd2num(fd);
}



int file_close_Fat(struct Fd *fd) {
	int r;
	struct Fat32_Filefd *ffd;
	void *va;
	u_int size, fileid;
	u_int i;

	ffd = (struct Fat32_Filefd *)fd;
	fileid = ffd->f_fileid;
	size = ffd->f_file.FileSize;
	va = fd2data(fd);


	for(i = 0;i < size;i += BY2PG) {
		fsipc_dirty_Fat32(fileid, i);
	}


	if ((r = fsipc_close_Fat32(fileid)) < 0) {
		debugf("cannot close the Fat32 file\n");
		return r;
	}


	if(size == 0) {
		return 0;
	}
	for(i = 0;i < size;i += BY2PG) {
		if((r = syscall_mem_unmap(0, (void *)(va + i))) < 0) {
			debugf("cannot unmap the Fat32 file\n");
			return r;
		}
	}
	return 0;
}



// Overview:
//  Close a file descriptor
int file_close(struct Fd *fd) {
	int r;
	struct Filefd *ffd;
	void *va;
	u_int size, fileid;
	u_int i;

	if(fd->fd_fat) {
		return file_close_Fat(fd);
	}

	ffd = (struct Filefd *)fd;
	fileid = ffd->f_fileid;
	size = ffd->f_file.f_size;

	// Set the start address storing the file's content.
	va = fd2data(fd);
	
	// Tell the file server the dirty page.
	for (i = 0; i < size; i += BY2PG) {
		fsipc_dirty(fileid, i);
	}

	// Request the file server to close the file with fsipc.
	if ((r = fsipc_close(fileid)) < 0) {
		debugf("cannot close the file\n");
		return r;
	}

	// Unmap the content of file, release memory.
	if (size == 0) {
		return 0;
	}
	for (i = 0; i < size; i += BY2PG) {
		if ((r = syscall_mem_unmap(0, (void *)(va + i))) < 0) {
			debugf("cannont unmap the file.\n");
			return r;
		}
	}
	return 0;
}


static int file_read_Fat(struct Fd *fd, void *buf, u_int n, u_int offset) {
	user_assert(fd->fd_fat);
	u_int size;
	struct Fat32_Filefd *f;
	f = (struct Fat32_Filefd *)fd;
	size = f->f_file.FileSize;

	if(offset > size) {
		return 0;
	}

	if(offset + n > size) {
		n = size - offset;
	}
	memcpy(buf, (char *)fd2data(fd) + offset, n);
	return n;
}


// Overview:
//  Read 'n' bytes from 'fd' at the current seek position into 'buf'. Since files
//  are memory-mapped, this amounts to a memcpy() surrounded by a little red
//  tape to handle the file size and seek pointer.
static int file_read(struct Fd *fd, void *buf, u_int n, u_int offset) {
	if(fd->fd_fat) {
		return file_read_Fat(fd, buf, n, offset);
	}
	u_int size;
	struct Filefd *f;
	f = (struct Filefd *)fd;

	// Avoid reading past the end of file.
	size = f->f_file.f_size;

	if (offset > size) {
		return 0;
	}

	if (offset + n > size) {
		n = size - offset;
	}

	memcpy(buf, (char *)fd2data(fd) + offset, n);
	return n;
}



// Overview:
//  Find the virtual address of the page that maps the file block
//  starting at 'offset'.
int read_map(int fdnum, u_int offset, void **blk) {
	int r;
	void *va;
	struct Fd *fd;

	if ((r = fd_lookup(fdnum, &fd)) < 0) {
		return r;
	}

	if (fd->fd_dev_id != devfile.dev_id) {
		return -E_INVAL;
	}

	va = fd2data(fd) + offset;

	if (offset >= MAXFILESIZE) {
		return -E_NO_DISK;
	}

	if (!(vpd[PDX(va)] & PTE_V) || !(vpt[VPN(va)] & PTE_V)) {
		return -E_NO_DISK;
	}

	*blk = (void *)va;
	return 0;
}


static int file_write_Fat(struct Fd *fd, const void *buf, u_int n, u_int offset) {
	int r;
	u_int tot;
	struct Fat32_Filefd *f;
	f = (struct Fat32_Filefd *)fd;

	tot = offset + n;
	if (tot > MAXFILESIZE) {
		return -E_NO_DISK;
	}

	if(tot > f->f_file.FileSize) {
		if((r = ftruncate_Fat(fd2num(fd), tot)) < 0) {
			return r;
		}
	}
	
	for(int i = 0;i < n;i += MAXWB) {
		u_int undone = (n - i * MAXWB)  > MAXWB ? MAXWB : (n - i * MAXWB);
		user_assert(fsipc_flush_Fat32(f->f_fileid, offset + i * MAXWB, undone, (char *)buf + i) == 0);
	}
	memcpy((char *)fd2data(fd) + offset, buf, n);
	return n;
}




// Overview:
//  Write 'n' bytes from 'buf' to 'fd' at the current seek position.
static int file_write(struct Fd *fd, const void *buf, u_int n, u_int offset) {
	if(fd->fd_fat) {
		return file_write_Fat(fd, buf, n, offset);
	}
	int r;
	u_int tot;
	struct Filefd *f;

	f = (struct Filefd *)fd;

	// Don't write more than the maximum file size.
	tot = offset + n;

	if (tot > MAXFILESIZE) {
		return -E_NO_DISK;
	}

	// Increase the file's size if necessary
	if (tot > f->f_file.f_size) {
		if ((r = ftruncate(fd2num(fd), tot)) < 0) {
			return r;
		}
	}

	// Write the data
	memcpy((char *)fd2data(fd) + offset, buf, n);
	return n;
}

static int file_stat(struct Fd *fd, struct Stat *st) {
	struct Filefd *f;

	f = (struct Filefd *)fd;

	strcpy(st->st_name, f->f_file.f_name);
	st->st_size = f->f_file.f_size;
	st->st_isdir = f->f_file.f_type == FTYPE_DIR;
	return 0;
}


int ftruncate_Fat(int fdnum, u_int size) {
	int i, r;
	struct Fd *fd;
	struct Fat32_Filefd *f;
	u_int oldsize, fileid;

	if(size > MAXFILESIZE) {
		return -E_NO_DISK;
	}

	if((r = fd_lookup(fdnum, &fd)) < 0) {
		return r;
	}

	if(fd->fd_dev_id != devfile.dev_id) {
		return -E_INVAL;
	}

	f = (struct Fat32_Filefd *)fd;
	fileid = f->f_fileid;
	oldsize = f->f_file.FileSize;
	f->f_file.FileSize = size;

	if((r = fsipc_set_size_Fat32(fileid, size)) < 0) {
		return r;
	}

	void *va = fd2data(fd);

	if(User_BY2CLUS == 0) {
		getBY2CLUS();
	}

	for (i = ROUND(oldsize, User_BY2CLUS); i < ROUND(size, User_BY2CLUS); i += User_BY2CLUS) {
		if ((r = fsipc_map_Fat32(fileid, i, Fat_buf)) < 0) {
			fsipc_set_size_Fat32(fileid, oldsize);
			return r;
		}
		memcpy(va + i, Fat_buf, User_BY2CLUS);
	}

	for (i = ROUND(size, BY2PG); i < ROUND(oldsize, BY2PG); i += BY2PG) {
		if ((r = syscall_mem_unmap(0, (void *)(va + i))) < 0) {
			user_panic("ftruncate: syscall_mem_unmap %08x: %e", va + i, r);
		}
	}

	return 0;
}


// Overview:
//  Truncate or extend an open file to 'size' bytes
int ftruncate(int fdnum, u_int size) {
	int i, r;
	struct Fd *fd;
	struct Filefd *f;
	u_int oldsize, fileid;

	if (size > MAXFILESIZE) {
		return -E_NO_DISK;
	}

	if ((r = fd_lookup(fdnum, &fd)) < 0) {
		return r;
	}

	if (fd->fd_dev_id != devfile.dev_id) {
		return -E_INVAL;
	}

	f = (struct Filefd *)fd;
	fileid = f->f_fileid;
	oldsize = f->f_file.f_size;
	f->f_file.f_size = size;

	if ((r = fsipc_set_size(fileid, size)) < 0) {
		return r;
	}

	void *va = fd2data(fd);

	// Map any new pages needed if extending the file
	for (i = ROUND(oldsize, BY2PG); i < ROUND(size, BY2PG); i += BY2PG) {
		if ((r = fsipc_map(fileid, i, va + i)) < 0) {
			fsipc_set_size(fileid, oldsize);
			return r;
		}
	}

	// Unmap pages if truncating the file
	for (i = ROUND(size, BY2PG); i < ROUND(oldsize, BY2PG); i += BY2PG) {
		if ((r = syscall_mem_unmap(0, (void *)(va + i))) < 0) {
			user_panic("ftruncate: syscall_mem_unmap %08x: %e", va + i, r);
		}
	}

	return 0;
}

// Overview:
//  Delete a file or directory.
int remove(const char *path) {
	// Your code here.
	// Call fsipc_remove.
	if(isFat32(path)) {
		return fsipc_remove_Fat32(path);
	}
	/* Exercise 5.13: Your code here. */
	return fsipc_remove(path);
}


int removeAt(int fd, const char *path) {
	return fsipc_removeat_Fat32(fd, path);
}

// Overview:
//  Synchronize disk with buffer cache
int sync(void) {
	return fsipc_sync();
}
