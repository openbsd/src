/*
 * Copyright (C) 2001  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
 * INTERNET SOFTWARE CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
 * FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* $ISC: ntfile.c,v 1.5 2001/07/16 04:09:45 gson Exp $ */

/*
 * This file has been necessitated by the fact that the iov array is local
 * to the module, so passing the FILE ptr to a file I/O function in a
 * different module or DLL will cause the application to fail to find the
 * I/O channel and the application will terminate. The standard file I/O
 * functions are redefined to call these routines instead and there will
 * be just the one iov to deal with.
 */

#include <config.h>

#include <io.h>

#include <isc/ntfile.h>

FILE *
isc_ntfile_fopen(const char *filename, const char *mode) {
	return (fopen(filename, mode));
}

int 
isc_ntfile_fclose(FILE *f) {
	return (fclose(f));
}

int 
isc_ntfile_fseek(FILE *f, long offset, int whence) {
	return (fseek(f, offset, whence));
}

size_t 
isc_ntfile_fread(void *ptr, size_t size, size_t nmemb, FILE *f) {
	return (fread(ptr, size, nmemb, f));
}

size_t 
isc_ntfile_fwrite(const void *ptr, size_t size, size_t nmemb, FILE *f) {
	int r;
	r = fwrite(ptr, size, nmemb, f);
	fflush(f);
	return (r);
}

int 
isc_ntfile_flush(FILE *f) {
	return (fflush(f));
}

int 
isc_ntfile_sync(FILE *f) {
	return (_commit(_fileno(f)));
}

FILE * 
isc_ntfile_getaddress(int r) {
	return (&_iob[r]);
}

int 
isc_ntfile_printf(const char *format, ...) {
	int r;
	FILE *fp = stdout;
	va_list ap;
	va_start(ap, format);
	r = vfprintf(fp, format, ap);
	va_end(ap);
	fflush(fp);
	return (r);
}

int 
isc_ntfile_fprintf(FILE *fp, const char *format, ...) {
	int r;
	va_list ap;
	va_start(ap, format);
	r = vfprintf(fp, format, ap);
	va_end(ap);
	fflush(fp);
	return (r);
}

int 
isc_ntfile_vfprintf(FILE *fp, const char *format, va_list alist) {
	int r;
	r = vfprintf(fp, format, alist);
	fflush(fp);
	return (r);
}

int
isc_ntfile_fputc(int iv, FILE *fp) {
	int r;
	r = fputc(iv, fp);
	fflush(fp);
	return (r);
}

int
isc_ntfile_fputs(const char *bf, FILE *fp) {
	int r;
	r = fputs(bf, fp);
	fflush(fp);
	return (r);
}

int
isc_ntfile_fgetc(FILE *fp) {
	return (fgetc(fp));
}

int
isc_ntfile_fgetpos(FILE *fp, fpos_t *pos) {
	return (fgetpos(fp, pos));
}

char * 
isc_ntfile_fgets(char *ch, int r, FILE *fp) {
	return (fgets(ch,r, fp));
}

int
isc_ntfile_getc(FILE *fp) {
	return (getc(fp));
}

FILE *
isc_ntfile_freopen(const char *path, const char *mode, FILE *fp) {
	return (freopen(path, mode,fp));
}

FILE *
isc_ntfile_fdopen(int handle, const char *mode) {
	return (fdopen(handle, mode));
}

/*
 * open(), close(), read(), write(), fsync()
 * sockets are file descriptors in UNIX.  This is not so in NT
 * We keep track of what is a socket and what is an FD to
 * make everything flow right.
 */
int 
isc_ntfile_open(const char *fn, int flags, ...){
	va_list args;
	int pmode;
	int fd;

 	/* Extract the cmd parameter */
	va_start(args, flags);
	pmode = va_arg(args, int);
	fd = _open(fn, flags, pmode);
	return fd;
}

int
isc_ntfile_close(int fd){
	return (_close(fd));
}

int 
isc_ntfile_read(int fd, char *buf, int len) {
	return (_read(fd, buf, len));
}

int
isc_ntfile_write(int fd, char *buf, int len){
	int r;
	r = _write(fd, buf, len);
	_commit(fd);
	return (r);
}
