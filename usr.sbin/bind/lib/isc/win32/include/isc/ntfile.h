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

/* $ISC: ntfile.h,v 1.5 2001/07/16 03:52:14 mayer Exp $ */

#ifndef ISC_NTFILE_H
#define ISC_NTFILE_H 1

/*
 * This file has been necessitated by the fact that the iov array is local
 * to the module, so passing the FILE ptr to a file I/O function in a
 * different module or DLL will cause the application to fail to find the
 * I/O channel and the application will terminate. The standard file I/O
 * functions are redefined to call these routines instead and there will
 * be just the one iov to deal with.
 */

/*
 * Outside of lib isc we need to redefine these functions
 * This is due to the way _iob is set up.
 * liblwres should not use this.
 */

#if !defined(LIBISC_EXPORTS) && !defined(LIBLWRES_EXPORTS)

#undef fdopen
#undef getc

#define fopen    isc_ntfile_fopen
#define fclose   isc_ntfile_fclose
#define fwrite   isc_ntfile_fwrite
#define fread    isc_ntfile_fread
#define fseek    isc_ntfile_fseek
#define fflush   isc_ntfile_flush
#define fsync    isc_ntfile_sync
#define printf   isc_ntfile_printf
#define fprintf  isc_ntfile_fprintf
#define vfprintf isc_ntfile_vfprintf
#define getc     isc_ntfile_getc
#define fgetc    isc_ntfile_fgetc
#define fgets    isc_ntfile_fgets
#define fputc    isc_ntfile_fputc
#define fputs    isc_ntfile_fputs
#define fgetpos  isc_ntfile_fgetpos
#define freopen  isc_ntfile_freopen
#define fdopen   isc_ntfile_fdopen
#define open	 isc_ntfile_open
#define close	 isc_ntfile_close
#define read	 isc_ntfile_read
#define write	 isc_ntfile_write

#undef stdin
#undef stdout
#undef stderr

#define stdin  isc_ntfile_getaddress(0)
#define stdout isc_ntfile_getaddress(1)
#define stderr isc_ntfile_getaddress(2)

#endif

FILE*
isc_ntfile_fopen(const char *filename, const char *mode);

int
isc_ntfile_fclose(FILE *f);

int
isc_ntfile_fseek(FILE *f, long offset, int whence);

size_t
isc_ntfile_fread(void *ptr, size_t size, size_t nmemb, FILE *f);

size_t
isc_ntfile_fwrite(const void *ptr, size_t size, size_t nmemb, FILE *f);

int
isc_ntfile_flush(FILE *f);

int
isc_ntfile_sync(FILE *f);

FILE*
isc_ntfile_getaddress(int r);

int
isc_ntfile_printf(const char *format, ...);

int
isc_ntfile_fprintf(FILE *fp, const char *format, ...);

int
isc_ntfile_vfprintf(FILE *, const char *, va_list);

int
isc_ntfile_fputc(int iv, FILE *fp);

int
isc_ntfile_fputs(const char *bf, FILE *fp);

int
isc_ntfile_fgetc(FILE *fp);

int
isc_ntfile_fgetpos(FILE *, fpos_t *pos);

char * 
isc_ntfile_fgets(char *ch, int r, FILE *fp);

int
isc_ntfile_getc(FILE *fp);

FILE *
isc_ntfile_freopen(const char *path, const char *mode, FILE *fp);

FILE *
isc_ntfile_fdopen(int handle, const char *mode);

int 
isc_ntfile_open(const char *fn, int flags, ...);

int
isc_ntfile_close(int fd);

int 
isc_ntfile_read(int fd, char *buf, int len);

int
isc_ntfile_write(int fd, char *buf, int len);

#endif /* ISC_NTFILE_H */
