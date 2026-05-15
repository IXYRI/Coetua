#include "io.h"
#include "config.h"
#include "err.h"
#include "fmt.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#if !defined(_WIN32)
  #error "Coetua raw I/O currently has only the Windows backend; add a direct OS backend for this platform."
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static HANDLE *fd_table;
static int     fdcap;

static int     fd_seed(void) { return COETUA_FD_TABLE_SEED < 4 ? 4 : COETUA_FD_TABLE_SEED; }

static bool    fd_table_init(void) {
	if (fd_table) return true;
	fdcap    = fd_seed();
	fd_table = ( HANDLE * ) calloc(( size_t ) fdcap, sizeof(HANDLE));
	if (!fd_table) {
		errmsg("io: out of memory");
		fdcap = 0;
		return false;
	}
	return true;
}

static bool fd_table_grow(void) {
	int newcap = fdcap ? fdcap * 2 : fd_seed();
	if (newcap <= fdcap) {
		errmsg("io: descriptor overflow");
		return false;
	}
	HANDLE *p = ( HANDLE * ) realloc(fd_table, ( size_t ) newcap * sizeof(HANDLE));
	if (!p) {
		errmsg("io: out of memory");
		return false;
	}
	memset(p + fdcap, 0, ( size_t ) (newcap - fdcap) * sizeof(HANDLE));
	fd_table = p;
	fdcap    = newcap;
	return true;
}

static bool fd_slot_free(int fd) { return !fd_table [fd] || fd_table [fd] == INVALID_HANDLE_VALUE; }

static int  fd_alloc(HANDLE h) {
	if (!fd_table_init()) return -1;
	for (;;) {
		for (int i = 3; i < fdcap; i++)
			if (fd_slot_free(i)) {
				fd_table [i] = h;
				return i;
			}
		if (!fd_table_grow()) return -1;
	}
}

static HANDLE fd_get(int fd) {
	if (fd < 0) return INVALID_HANDLE_VALUE;
	if (fd == 0) return GetStdHandle(STD_INPUT_HANDLE);
	if (fd == 1) return GetStdHandle(STD_OUTPUT_HANDLE);
	if (fd == 2) return GetStdHandle(STD_ERROR_HANDLE);
	if (!fd_table_init() || fd >= fdcap) return INVALID_HANDLE_VALUE;
	return fd_table [fd] ? fd_table [fd] : INVALID_HANDLE_VALUE;
}

static DWORD access_of(omode mod) {
	DWORD access = 0;
	if (mod.r) access |= GENERIC_READ;
	if (mod.a) access |= FILE_APPEND_DATA;
	else if (mod.w || mod.t) access |= GENERIC_WRITE;
	if (mod.d) access |= DELETE;
	return access ? access : GENERIC_READ;
}

static DWORD creation_of_open(omode mod) {
	if (mod.x) return OPEN_EXISTING;
	if (mod.t) return TRUNCATE_EXISTING;
	return OPEN_EXISTING;
}

static DWORD creation_of_create(omode mod) {
	if (mod.x) return CREATE_NEW;
	if (mod.t) return CREATE_ALWAYS;
	return OPEN_ALWAYS;
}

static DWORD flags_of(omode mod) {
	DWORD flags = FILE_ATTRIBUTE_NORMAL;
	if (mod.d) flags |= FILE_FLAG_DELETE_ON_CLOSE;
	return flags;
}

static int fd_from_handle(HANDLE h) {
	if (h == INVALID_HANDLE_VALUE) {
		errmsg("io: open failed");
		return -1;
	}
	int fd = fd_alloc(h);
	if (fd < 0) CloseHandle(h);
	return fd;
}

int permtomode(perm p) { return p.bits & 0777; }

int dopen(char *file, omode mod) {
	if (!file) {
		errmsg("dopen: bad path");
		return -1;
	}
	if (mod.x) {
		errmsg("dopen: exclusive mode requires create");
		return -1;
	}
	HANDLE h  = CreateFileA(file, access_of(mod), FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, null,
	                        creation_of_open(mod), flags_of(mod), null);
	int    fd = fd_from_handle(h);
	if (fd >= 0 && mod.a) dseek(fd, 0, 2);
	return fd;
}

int dcreate(char *file, omode mod, perm pm) {
	( void ) pm;
	if (!file) {
		errmsg("dcreate: bad path");
		return -1;
	}
	HANDLE h  = CreateFileA(file, access_of(mod), FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, null,
	                        creation_of_create(mod), flags_of(mod), null);
	int    fd = fd_from_handle(h);
	if (fd >= 0 && mod.a) dseek(fd, 0, 2);
	return fd;
}

void dclose(int fd) {
	if (fd < 3 || !fd_table || fd >= fdcap) return;
	HANDLE h = fd_get(fd);
	if (h != INVALID_HANDLE_VALUE) {
		CloseHandle(h);
		fd_table [fd] = INVALID_HANDLE_VALUE;
	}
}

vlong dread(int fd, void *buf, uvlong len) {
	HANDLE h = fd_get(fd);
	if (h == INVALID_HANDLE_VALUE) {
		errmsg("dread: bad fd");
		return -1;
	}
	if (!buf && len) {
		errmsg("dread: bad buffer");
		return -1;
	}
	DWORD want = len > 0xffffffffull ? 0xfffffffful : ( DWORD ) len;
	DWORD got  = 0;
	if (!ReadFile(h, buf, want, &got, null)) {
		errmsg("dread: failed");
		return -1;
	}
	return ( vlong ) got;
}

uvlong dreadn(int fd, void *buf, uvlong len) {
	uvlong total = 0;
	uchar *p     = ( uchar * ) buf;
	while (total < len) {
		vlong n = dread(fd, p + total, len - total);
		if (n <= 0) break;
		total += ( uvlong ) n;
	}
	return total;
}

vlong dwrite(int fd, void *buf, uvlong len) {
	HANDLE h = fd_get(fd);
	if (h == INVALID_HANDLE_VALUE) {
		errmsg("dwrite: bad fd");
		return -1;
	}
	if (!buf && len) {
		errmsg("dwrite: bad buffer");
		return -1;
	}
	DWORD want = len > 0xffffffffull ? 0xfffffffful : ( DWORD ) len;
	DWORD put  = 0;
	if (!WriteFile(h, buf, want, &put, null)) {
		errmsg("dwrite: failed");
		return -1;
	}
	return ( vlong ) put;
}

vlong dseek(int fd, vlong amount, int whence) {
	HANDLE h = fd_get(fd);
	if (h == INVALID_HANDLE_VALUE) {
		errmsg("dseek: bad fd");
		return -1;
	}
	if (whence < 0 || whence > 2) {
		errmsg("dseek: bad whence");
		return -1;
	}
	DWORD         method = whence == 0 ? FILE_BEGIN : whence == 1 ? FILE_CURRENT : FILE_END;
	LARGE_INTEGER move;
	LARGE_INTEGER pos;
	move.QuadPart = amount;
	if (!SetFilePointerEx(h, move, &pos, method)) {
		errmsg("dseek: failed");
		return -1;
	}
	return ( vlong ) pos.QuadPart;
}

static bool offset_fits_vlong(uvlong offset) {
	uvlong max = ((( uvlong ) 1 << (sizeof(vlong) * CHAR_BIT - 1)) - 1);
	return offset <= max;
}

static bool seek_to_offset(int fd, uvlong offset, vlong *saved, char *who) {
	if (!offset_fits_vlong(offset)) {
		errmsg(who);
		return false;
	}
	*saved = dseek(fd, 0, 1);
	if (*saved < 0) return false;
	if (dseek(fd, ( vlong ) offset, 0) < 0) return false;
	return true;
}

vlong pread(int fd, void *buf, uvlong len, uvlong offset) {
	vlong saved;
	if (!seek_to_offset(fd, offset, &saved, "pread: offset out of range")) return -1;
	vlong n = dread(fd, buf, len);
	dseek(fd, saved, 0);
	return n;
}

vlong pwrite(int fd, void *buf, uvlong len, uvlong offset) {
	vlong saved;
	if (!seek_to_offset(fd, offset, &saved, "pwrite: offset out of range")) return -1;
	vlong n = dwrite(fd, buf, len);
	dseek(fd, saved, 0);
	return n;
}

int dvprint(int fd, char *fm, va_list args) {
	int sd = vfmts(0, fm, args);
	if (sd < 0) return -1;
	slitr s = obslitr(sd);
	vlong n = 0;
	if (s.len > 0) n = dwrite(fd, s.s, s.len);
	rmstrand(sd);
	return n < 0 ? -1 : ( int ) n;
}

int dprint(int fd, char *fm, ...) {
	va_list args;
	va_start(args, fm);
	int n = dvprint(fd, fm, args);
	va_end(args);
	return n;
}
